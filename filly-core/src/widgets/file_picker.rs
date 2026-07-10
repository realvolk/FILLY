use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::fs;
use std::path::{Path, PathBuf};

pub struct FilePickerWidget {
    pub title: String,
    pub current_dir: PathBuf,
    pub entries: Vec<PathBuf>,
    pub selected: usize,
    pub filter_ext: String,
    pub mode: PickerMode,
    dirty: bool,
}

#[derive(PartialEq)]
pub enum PickerMode {
    Browsing,
    ConfirmQuit,
}

impl FilePickerWidget {
    pub fn new(title: String, start_dir: Option<String>, filter: Option<String>) -> Self {
        let current_dir = start_dir
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("/"));
        let filter_ext = filter.unwrap_or_default();
        let entries = get_entries(&current_dir, &filter_ext);
        Self {
            title,
            current_dir,
            entries,
            selected: 0,
            filter_ext,
            mode: PickerMode::Browsing,
            dirty: true,
        }
    }

    fn navigate_to(&mut self, dir: PathBuf) {
        self.current_dir = dir;
        self.entries = get_entries(&self.current_dir, &self.filter_ext);
        self.selected = 0;
        self.dirty = true;
    }
}

impl Widget for FilePickerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.7) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.75) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: format!("{}: {}", self.title, self.current_dir.display()),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle {
                bold: true,
                fg: Some("green".into()),
                ..TextStyle::normal()
            },
        });

        let list_y = 1;
        let list_h = box_h.saturating_sub(list_y).saturating_sub(2);
        let items: Vec<ListItem> = self
            .entries
            .iter()
            .map(|entry| {
                let is_dir = entry.is_dir();
                let prefix = if is_dir { "[DIR] " } else { "      " };
                let name = entry
                    .file_name()
                    .unwrap_or_default()
                    .to_string_lossy()
                    .to_string();
                ListItem {
                    label: format!("{}{}", prefix, name),
                    meta: None,
                }
            })
            .collect();

        children.push(RenderNode::List {
            rect: Rect::new(1, list_y, box_w - 2, list_h),
            items,
            selected: Some(self.selected),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        let help_y = box_h.saturating_sub(1);
        children.push(RenderNode::Text {
            rect: Rect::new(1, help_y, box_w - 2, 1),
            content: "Up/Down:move  Enter:open/select  Backspace:parent  Esc:cancel".into(),
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
        match (&self.mode, event) {
            (PickerMode::ConfirmQuit, Event::Key(code, _)) => match code {
                KeyCode::Char('y') | KeyCode::Char('Y') => {
                    EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: None,
                    })
                }
                _ => {
                    self.mode = PickerMode::Browsing;
                    self.dirty = true;
                    EventResult::Handled
                }
            },
            (PickerMode::Browsing, Event::Key(code, _)) => match code {
                KeyCode::Esc => {
                    self.mode = PickerMode::ConfirmQuit;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Up => {
                    if self.selected > 0 {
                        self.selected -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Down => {
                    if self.selected + 1 < self.entries.len() {
                        self.selected += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    if let Some(entry) = self.entries.get(self.selected) {
                        if entry.is_dir() {
                            self.navigate_to(entry.clone());
                        } else {
                            return EventResult::Response(WidgetResponse {
                                result: Some(serde_json::Value::String(
                                    entry.to_string_lossy().to_string(),
                                )),
                                cancelled: false,
                                error: None,
                            });
                        }
                    }
                    EventResult::Handled
                }
                KeyCode::Backspace => {
                    if let Some(parent) = self.current_dir.parent() {
                        self.navigate_to(parent.to_path_buf());
                    }
                    EventResult::Handled
                }
                _ => EventResult::Unhandled,
            },
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

fn get_entries(dir: &Path, filter: &str) -> Vec<PathBuf> {
    let mut entries: Vec<PathBuf> = if let Ok(read_dir) = fs::read_dir(dir) {
        read_dir
            .filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| {
                if filter.is_empty() {
                    true
                } else if p.is_dir() {
                    true
                } else {
                    p.extension().map(|ext| ext == filter).unwrap_or(false)
                }
            })
            .collect()
    } else {
        vec![]
    };
    entries.sort_by(|a, b| {
        let a_dir = a.is_dir();
        let b_dir = b.is_dir();
        if a_dir && !b_dir {
            std::cmp::Ordering::Less
        } else if !a_dir && b_dir {
            std::cmp::Ordering::Greater
        } else {
            a.file_name().cmp(&b.file_name())
        }
    });
    entries
}