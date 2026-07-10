use std::collections::HashSet;
use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct MultiselectWidget {
    pub title: String,
    pub message: String,
    pub choices: Vec<String>,
    pub selected: HashSet<usize>,
    pub cursor: usize,
    pub query: String,
    pub placeholder: String,
    pub min: usize,
    pub max: usize,
    dirty: bool,
}

impl MultiselectWidget {
    pub fn new(
        title: String,
        message: String,
        choices: Vec<String>,
        placeholder: Option<String>,
        min: Option<usize>,
        max: Option<usize>,
    ) -> Self {
        Self {
            title,
            message,
            choices,
            selected: HashSet::new(),
            cursor: 0,
            query: String::new(),
            placeholder: placeholder.unwrap_or("Type to filter...".into()),
            min: min.unwrap_or(0),
            max: max.unwrap_or(usize::MAX),
            dirty: true,
        }
    }

    fn filtered(&self) -> Vec<usize> {
        if self.query.is_empty() {
            (0..self.choices.len()).collect()
        } else {
            let q = self.query.to_lowercase();
            self.choices
                .iter()
                .enumerate()
                .filter(|(_, c)| c.to_lowercase().contains(&q))
                .map(|(i, _)| i)
                .collect()
        }
    }
}

impl Widget for MultiselectWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.7) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let inner_h = box_h.saturating_sub(2);
        let filtered = self.filtered();
        let absolute_selected = filtered.get(self.cursor).copied();
        let mut children = Vec::new();

        let mut y = 1;

        if !self.title.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, y, box_w - 2, 1),
                content: self.title.clone(),
                alignment: Alignment::Center,
                role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
            });
            y += 1;
        }
        if !self.message.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, y, box_w - 2, 2),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            });
            y += 2;
        }

        children.push(RenderNode::Input {
            rect: Rect::new(1, y, box_w - 2, 1),
            text: if self.query.is_empty() { self.placeholder.clone() } else { format!("> {}", self.query) },
            cursor: if self.query.is_empty() { 0 } else { 2 + self.query.len() },
            placeholder: self.placeholder.clone(),
            masked: false, role: None, label: None, description: None,
        });
        y += 1;

        let list_h = inner_h.saturating_sub(y).saturating_sub(2);

        let items: Vec<ListItem> = self.choices.iter().enumerate().map(|(i, c)| {
            let mark = if self.selected.contains(&i) { "[x]" } else { "[ ]" };
            ListItem { label: format!(" {} {}", mark, c), meta: None }
        }).collect();

        children.push(RenderNode::List {
            rect: Rect::new(1, y, box_w - 2, list_h),
            items,
            selected: absolute_selected,
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        children.push(RenderNode::Text {
            rect: Rect::new(1, inner_h, box_w - 2, 1),
            content: format!(" Selected: {}/{}   Space=toggle  Enter=confirm  Esc=cancel",
                self.selected.len(), self.choices.len()),
            alignment: Alignment::Left,
            style: TextStyle::muted(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, inner_h + 1, box_w - 2, 1),
            content: "Type:filter  Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel".into(),
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
                    if self.cursor > 0 {
                        self.cursor -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Down => {
                    let filtered = self.filtered();
                    if self.cursor + 1 < filtered.len() {
                        self.cursor += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Char(' ') => {
                    let filtered = self.filtered();
                    if let Some(&idx) = filtered.get(self.cursor) {
                        if self.selected.contains(&idx) {
                            if self.selected.len() > self.min {
                                self.selected.remove(&idx);
                                self.dirty = true;
                            }
                        } else if self.selected.len() < self.max {
                            self.selected.insert(idx);
                            self.dirty = true;
                        }
                    }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    if self.selected.len() >= self.min && self.selected.len() <= self.max {
                        let selected: Vec<String> = self
                            .selected
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
                KeyCode::Char(c) => {
                    self.query.push(*c);
                    self.cursor = 0;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Backspace => {
                    self.query.pop();
                    self.cursor = 0;
                    self.dirty = true;
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