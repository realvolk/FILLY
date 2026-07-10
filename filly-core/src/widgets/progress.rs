use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::io::BufRead;
use std::process::{Child, Command, Stdio};
use std::sync::mpsc;
use std::thread;

pub struct ProgressWidget {
    pub title: String,
    pub command: Vec<String>,
    pub logfile: Option<String>,
    pub output: String,
    pub progress: u16,
    pub stage: String,
    pub show_raw: bool,
    pub cancelled: bool,
    pub child: Option<Child>,
    pub rx: Option<mpsc::Receiver<String>>,
    pub tx: Option<mpsc::Sender<serde_json::Value>>,
}

impl ProgressWidget {
    pub fn new(
        title: String,
        command: Vec<String>,
        logfile: Option<String>,
        tx: Option<mpsc::Sender<serde_json::Value>>,
    ) -> Self {
        Self {
            title,
            command,
            logfile,
            output: String::new(),
            progress: 0,
            stage: "Starting...".into(),
            show_raw: false,
            cancelled: false,
            child: None,
            rx: None,
            tx,
        }
    }

    fn start_command(&mut self) {
        let (prog, args) = self
            .command
            .split_first()
            .map(|(p, a)| (p.clone(), a.to_vec()))
            .unwrap_or_default();
        match Command::new(&prog)
            .args(&args)
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
        {
            Ok(mut child) => {
                let stdout = child.stdout.take().unwrap();
                let stderr = child.stderr.take().unwrap();
                let (tx, rx) = mpsc::channel();
                let tx2 = tx.clone();
                thread::spawn(move || {
                    for line in std::io::BufReader::new(stdout).lines() {
                        if let Ok(l) = line {
                            if tx.send(l).is_err() {
                                break;
                            }
                        }
                    }
                });
                thread::spawn(move || {
                    for line in std::io::BufReader::new(stderr).lines() {
                        if let Ok(l) = line {
                            let _ = tx2.send(l);
                        }
                    }
                });
                self.child = Some(child);
                self.rx = Some(rx);
            }
            Err(_) => {
                self.output = "Failed to start command".into();
                self.cancelled = true;
            }
        }
    }

    fn update_output(&mut self) {
        if let Some(ref rx) = self.rx {
            while let Ok(line) = rx.try_recv() {
                self.output.push_str(&line);
                self.output.push('\n');
                // Stage detection
                if line.contains("Preflight dependencies installed.") {
                    self.progress = self.progress.max(5);
                    self.stage = "Preflight complete".into();
                } else if line.contains("Mount setup completed.") {
                    self.progress = self.progress.max(20);
                    self.stage = "Storage configured".into();
                } else if line.contains("Base system installation complete.") {
                    self.progress = self.progress.max(50);
                    self.stage = "Base system installed".into();
                } else if line.contains("All source packages built and installed.") {
                    self.progress = self.progress.max(65);
                    self.stage = "Packages built".into();
                } else if line.contains("Bootloader setup complete.") {
                    self.progress = self.progress.max(78);
                    self.stage = "Bootloader configured".into();
                } else if line.contains("Post-install configuration complete.") {
                    self.progress = self.progress.max(90);
                    self.stage = "Post-install done".into();
                } else if line.contains("Applying final system configuration") {
                    self.progress = self.progress.max(100);
                    self.stage = "Finalizing".into();
                }
                // Yield progress update
                if let Some(ref tx) = self.tx {
                    let _ = tx.send(serde_json::json!({
                        "progress": self.progress,
                        "stage": self.stage,
                        "line": line,
                    }));
                }
            }
        }
    }
}

impl Widget for ProgressWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.8) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;


        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle {
                bold: true,
                fg: Some("green".into()),
                ..TextStyle::normal()
            },
        });

        if self.show_raw {
            let lines: Vec<&str> = self.output.lines().rev().take(15).collect();
            let reversed: String =
                lines.into_iter().rev().collect::<Vec<&str>>().join("\n");
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, box_h - 2),
                content: reversed,
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            });
        } else {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, 1),
                content: format!("[{}%] {}", self.progress, self.stage),
                alignment: Alignment::Left,
                style: TextStyle::accent(), role: None, label: None, description: None,
            });
            let bar_len =
                ((box_w - 4) as f32 * self.progress as f32 / 100.0) as usize;
            let bar = "#".repeat(bar_len);
            children.push(RenderNode::Text {
                rect: Rect::new(2, 2, box_w - 4, 1),
                content: bar,
                alignment: Alignment::Left,
                role: None, label: None, description: None, style: TextStyle {
                    fg: Some("green".into()),
                    ..TextStyle::normal()
                },
            });
            let recent: String = self
                .output
                .lines()
                .rev()
                .take(10)
                .collect::<Vec<&str>>()
                .into_iter()
                .rev()
                .collect::<Vec<&str>>()
                .join("\n");
            children.push(RenderNode::Text {
                rect: Rect::new(1, 4, box_w - 2, box_h - 5),
                content: recent,
                alignment: Alignment::Left,
                style: TextStyle::muted(), role: None, label: None, description: None,
            });
        }

        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "[Tab] toggle raw  [Esc] cancel".into(),
            alignment: Alignment::Center,
            style: TextStyle::muted(), role: None, label: None, description: None,
        });

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::zero(), role: None, label: None, description: None,
            children,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        if self.child.is_none() {
            self.start_command();
        }
        self.update_output();
        if let Some(ref mut child) = self.child {
            if let Ok(Some(status)) = child.try_wait() {
                if status.success() {
                    self.progress = 100;
                    self.stage = "Complete".into();
                    if let Some(ref tx) = self.tx {
                        let _ = tx.send(serde_json::json!({
                            "progress": 100,
                            "stage": "Complete"
                        }));
                    }
                    return EventResult::Response(WidgetResponse {
                        result: Some(serde_json::Value::String(self.output.clone())),
                        cancelled: false,
                        error: None,
                    });
                } else {
                    return EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: Some("Command failed".into()),
                    });
                }
            }
        }
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => {
                    self.cancelled = true;
                    if let Some(ref mut child) = self.child {
                        let _ = child.kill();
                    }
                    EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: None,
                    })
                }
                KeyCode::Tab => {
                    self.show_raw = !self.show_raw;
                    EventResult::Handled
                }
                _ => EventResult::Handled,
            },
            _ => EventResult::Handled,
        }
    }
}