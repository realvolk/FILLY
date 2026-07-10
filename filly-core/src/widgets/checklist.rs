use std::collections::HashSet;
use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct ChecklistWidget {
    pub title: String,
    pub message: String,
    pub choices: Vec<String>,
    pub checked: HashSet<usize>,
    pub selected: usize,
    pub min: usize,
    pub max: usize,
    dirty: bool,
}

impl ChecklistWidget {
    pub fn new(
        title: String,
        message: String,
        choices: Vec<String>,
        min: Option<usize>,
        max: Option<usize>,
        default: Vec<String>,
    ) -> Self {
        let checked: HashSet<usize> = choices
            .iter()
            .enumerate()
            .filter(|(_, c)| default.contains(c))
            .map(|(i, _)| i)
            .collect();
        Self {
            title,
            message,
            choices,
            checked,
            selected: 0,
            min: min.unwrap_or(0),
            max: max.unwrap_or(usize::MAX),
            dirty: true,
        }
    }
}

impl Widget for ChecklistWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.7) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        if !self.title.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 0, box_w - 2, 1),
                content: self.title.clone(),
                alignment: Alignment::Center,
                role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
            });
        }
        if !self.message.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, 2),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            });
        }

        let list_y = if self.message.is_empty() { 1 } else { 3 };
        // List takes everything from list_y down to 2 rows above the bottom
        let list_h = box_h.saturating_sub(list_y).saturating_sub(2);

        let items: Vec<ListItem> = self.choices.iter().enumerate().map(|(i, c)| {
            let mark = if self.checked.contains(&i) { "[x]" } else { "[ ]" };
            ListItem { label: format!(" {} {}", mark, c), meta: None }
        }).collect();

        let selected = self.selected.min(self.choices.len().saturating_sub(1));
        children.push(RenderNode::List {
            rect: Rect::new(1, list_y, box_w - 2, list_h),
            items,
            selected: Some(selected),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        // Footer lines pinned to bottom
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: format!(" Selected: {}/{}   Space=toggle  Enter=confirm  Esc=cancel", self.checked.len(), self.choices.len()),
            alignment: Alignment::Left,
            style: TextStyle::muted(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel".into(),
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
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                }),
                KeyCode::Up => {
                    if self.selected > 0 {
                        self.selected -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Down => {
                    if self.selected + 1 < self.choices.len() {
                        self.selected += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Char(' ') => {
                    if self.checked.contains(&self.selected) {
                        if self.checked.len() > self.min {
                            self.checked.remove(&self.selected);
                        }
                    } else if self.checked.len() < self.max {
                        self.checked.insert(self.selected);
                    }
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    if self.checked.len() >= self.min && self.checked.len() <= self.max {
                        let selected: Vec<String> = self
                            .checked
                            .iter()
                            .map(|&i| self.choices[i].clone())
                            .collect();
                        EventResult::Response(WidgetResponse {
                            result: Some(serde_json::Value::Array(
                                selected
                                    .into_iter()
                                    .map(serde_json::Value::String)
                                    .collect(),
                            )),
                            cancelled: false,
                            error: None,
                        })
                    } else {
                        EventResult::Handled
                    }
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