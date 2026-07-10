use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct FilterWidget {
    pub title: String,
    pub message: String,
    pub choices: Vec<String>,
    pub query: String,
    pub selected: usize,
    pub placeholder: String,
    dirty: bool,
}

impl FilterWidget {
    pub fn new(
        title: String,
        message: String,
        choices: Vec<String>,
        placeholder: Option<String>,
    ) -> Self {
        Self {
            title,
            message,
            choices,
            query: String::new(),
            selected: 0,
            placeholder: placeholder.unwrap_or("Type to filter...".into()),
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

impl Widget for FilterWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.7) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let filtered = self.filtered();
        let absolute_selected = filtered.get(self.selected).copied();
        let mut children = Vec::new();

        if !self.title.is_empty() {
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
        }

        let search_display = if self.query.is_empty() {
            self.placeholder.clone()
        } else {
            format!("> {}", self.query)
        };
        children.push(RenderNode::Input {
            rect: Rect::new(1, 1, box_w - 2, 1),
            text: search_display,
            cursor: if self.query.is_empty() {
                0
            } else {
                2 + self.query.len()
            },
            placeholder: self.placeholder.clone(),
            masked: false, role: None, label: None, description: None,
        });

        let list_y = 2;
        let list_h = box_h.saturating_sub(list_y).saturating_sub(2);
        let items: Vec<ListItem> = self
            .choices
            .iter()
            .enumerate()
            .map(|(_i, c)| ListItem {
                label: c.clone(),
                meta: None,
            })
            .collect();

        children.push(RenderNode::List {
            rect: Rect::new(1, list_y, box_w - 2, list_h),
            items,
            selected: absolute_selected,
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        let help_y = box_h.saturating_sub(1);
        children.push(RenderNode::Text {
            rect: Rect::new(1, help_y, box_w - 2, 1),
            content: "Type:filter  Up/Down:move  Enter:select  Esc:cancel".into(),
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
                    let len = self.filtered().len();
                    if self.selected + 1 < len {
                        self.selected += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let filtered = self.filtered();
                    if let Some(&idx) = filtered.get(self.selected) {
                        return EventResult::Response(WidgetResponse {
                            result: Some(serde_json::Value::String(
                                self.choices[idx].clone(),
                            )),
                            cancelled: false,
                            error: None,
                        });
                    }
                    EventResult::Handled
                }
                KeyCode::Char(c) => {
                    self.query.push(*c);
                    self.selected = 0;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Backspace => {
                    self.query.pop();
                    self.selected = 0;
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