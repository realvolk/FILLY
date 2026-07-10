use crate::event::{Event, KeyCode, KeyModifiers};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::fs;

pub struct TextEditorWidget {
    pub title: String,
    pub file: Option<String>,
    pub lines: Vec<String>,
    pub row: usize,
    pub col: usize,
    pub scroll_row: usize,
    pub visible_height: usize,
    pub show_help: bool,
    dirty: bool,
}

impl TextEditorWidget {
    pub fn new(title: String, file: Option<String>, content: Option<String>) -> Self {
        let text = if let Some(ref path) = file {
            fs::read_to_string(path).unwrap_or_default()
        } else {
            content.unwrap_or_default()
        };
        let mut lines: Vec<String> = text.lines().map(|l| l.to_string()).collect();
        if lines.is_empty() {
            lines.push(String::new());
        }
        Self {
            title,
            file,
            lines,
            row: 0,
            col: 0,
            scroll_row: 0,
            visible_height: 20,
            show_help: true,
            dirty: true,
        }
    }

    fn save(&self) {
        if let Some(ref path) = self.file {
            let _ = fs::write(path, self.lines.join("\n"));
        }
    }

    fn clamp_cursor(&mut self) {
        if self.lines.is_empty() {
            self.lines.push(String::new());
        }
        self.row = self.row.min(self.lines.len().saturating_sub(1));
        self.col = self.col.min(self.lines[self.row].len());
        // scroll adjustment
        if self.row < self.scroll_row {
            self.scroll_row = self.row;
        }
        let visible = self.visible_height.max(1);
        if self.row >= self.scroll_row + visible {
            self.scroll_row = self.row.saturating_sub(visible.saturating_sub(1));
        }
        self.scroll_row = self.scroll_row.min(
            self.lines.len().saturating_sub(visible).max(0),
        );
    }
}

impl Widget for TextEditorWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.9) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.9) as u16).min(area.height.saturating_sub(2));
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

        let visible = ((box_h as usize).saturating_sub(4)).max(1);
        let end = (self.scroll_row + visible).min(self.lines.len());
        let start = self.scroll_row.min(self.lines.len().saturating_sub(1));

        if visible > 0 && start < end {
            let display_lines: Vec<String> = (start..end)
                .map(|i| self.lines[i].clone())
                .collect();
            let display_text = display_lines.join("\n");

            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, visible as u16),
                content: display_text,
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            });
        }

        let cursor_y = ((self.row.saturating_sub(self.scroll_row)) as u16 + 1)
            .min(visible as u16);
        children.push(RenderNode::Cursor {
            rect: Rect::new(1, 1, 0, 0),
            x: 1 + self.col as u16,
            y: cursor_y,
            role: None, label: None, description: None,
        });

        if self.show_help {
            children.push(RenderNode::Text {
                rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                content: "Ctrl+S:save  Ctrl+C:copy line  Ctrl+V:paste  Esc:done  Arrows:move  Enter:newline  Backspace:del  Home/End  PgUp/PgDn  Ctrl+H:hide help"
                    .into(),
                alignment: Alignment::Center,
                style: TextStyle::muted(), role: None, label: None, description: None,
            });
        }

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::zero(), role: None, label: None, description: None,
            children,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {

        match event {
            Event::Key(code, mods) => {
                if *mods == KeyModifiers::ctrl() {
                    match code {
                        KeyCode::Char('s') => {
                            self.save();
                            return EventResult::Handled;
                        }
                        KeyCode::Char('h') => {
                            self.show_help = !self.show_help;
                            self.dirty = true;
                            return EventResult::Handled;
                        }
                        KeyCode::Char('c') => {
                            if !self.lines.is_empty() {
                                let text = self.lines[self.row.min(self.lines.len().saturating_sub(1))].clone();
                                crate::clipboard::copy_external(&text);
                            }
                            return EventResult::Handled;
                        }
                        KeyCode::Char('v') => {
                            if let Some(clip) =
                                crate::clipboard::paste_from_clipboard(&mut std::io::stdout())
                            {
                                let clip = clip.replace("\r\n", "\n").replace('\r', "\n");
                                let mut lines_iter = clip.split('\n').peekable();
                                let first_line = lines_iter.next().unwrap_or("");
                                self.lines[self.row].insert_str(self.col, first_line);
                                self.col += first_line.len();
                                if lines_iter.peek().is_some() {
                                    let rest = self.lines[self.row].split_off(self.col);
                                    let mut new_lines: Vec<String> = lines_iter
                                        .map(|s| s.to_string())
                                        .collect();
                                    if !rest.is_empty() || new_lines.last().map_or(false, |s| s.is_empty()) {
                                    }
                                    let row = self.row + 1;
                                    for line in new_lines.iter().rev() {
                                        self.lines.insert(row, line.clone());
                                    }
                                    self.lines.insert(row + new_lines.len(), rest);
                                    self.row = row + new_lines.len() - 1;
                                    self.col = self.lines[self.row].len();
                                }
                                self.clamp_cursor();
                                self.dirty = true;
                            }
                            return EventResult::Handled;
                        }
                        _ => {}
                    }
                }
                match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse {
                        result: Some(serde_json::Value::String(self.lines.join("\n"))),
                        cancelled: false,
                        error: None,
                    }),
                    KeyCode::Up => {
                        if self.row > 0 {
                            self.row -= 1;
                            self.col = self.col.min(self.lines[self.row].len());
                            self.clamp_cursor();
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Down => {
                        if self.row + 1 < self.lines.len() {
                            self.row += 1;
                            self.col = self.col.min(self.lines[self.row].len());
                            self.clamp_cursor();
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Left => {
                        if self.col > 0 {
                            self.col -= 1;
                        } else if self.row > 0 {
                            self.row -= 1;
                            self.col = self.lines[self.row].len();
                            self.clamp_cursor();
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Right => {
                        if self.col < self.lines[self.row].len() {
                            self.col += 1;
                        } else if self.row + 1 < self.lines.len() {
                            self.row += 1;
                            self.col = 0;
                            self.clamp_cursor();
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Home => {
                        if *mods == KeyModifiers::ctrl() {
                            self.row = 0;
                            self.col = 0;
                            self.scroll_row = 0;
                        } else {
                            self.col = 0;
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::End => {
                        if *mods == KeyModifiers::ctrl() {
                            self.row = self.lines.len().saturating_sub(1);
                            self.col = self.lines[self.row].len();
                            self.clamp_cursor();
                        } else {
                            self.col = self.lines[self.row].len();
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::PageUp => {
                        self.row = self.row.saturating_sub(self.visible_height);
                        self.col = self.col.min(self.lines[self.row].len());
                        self.scroll_row = self.scroll_row.saturating_sub(self.visible_height);
                        self.clamp_cursor();
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::PageDown => {
                        self.row = (self.row + self.visible_height)
                            .min(self.lines.len().saturating_sub(1));
                        self.col = self.col.min(self.lines[self.row].len());
                        self.scroll_row = (self.scroll_row + self.visible_height)
                            .min(self.lines.len().saturating_sub(1));
                        self.clamp_cursor();
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        let rest = self.lines[self.row].split_off(self.col);
                        self.lines.insert(self.row + 1, rest);
                        self.row += 1;
                        self.col = 0;
                        self.clamp_cursor();
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Backspace => {
                        if self.col > 0 {
                            self.lines[self.row].remove(self.col - 1);
                            self.col -= 1;
                        } else if self.row > 0 {
                            let cur = self.lines.remove(self.row);
                            self.row -= 1;
                            self.col = self.lines[self.row].len();
                            self.lines[self.row].push_str(&cur);
                            self.clamp_cursor();
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Delete => {
                        if self.col < self.lines[self.row].len() {
                            self.lines[self.row].remove(self.col);
                        } else if self.row + 1 < self.lines.len() {
                            let next = self.lines.remove(self.row + 1);
                            self.lines[self.row].push_str(&next);
                        }
                        self.clamp_cursor();
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Tab => {
                        self.lines[self.row].insert_str(self.col, "    ");
                        self.col += 4;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Char(c) => {
                        self.lines[self.row].insert(self.col, *c);
                        self.col += 1;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    _ => EventResult::Unhandled,
                }
            }
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}