use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, FormField, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct FormWidget {
    pub title: String,
    pub fields: Vec<FormField>,
    pub focused: usize,
    pub submit_label: String,
    dirty: bool,
}

impl FormWidget {
    pub fn new(title: String, fields: Vec<FormField>, submit_label: String) -> Self {
        Self {
            title,
            fields,
            focused: 0,
            submit_label,
            dirty: true,
        }
    }
}

impl Widget for FormWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 60u16.min(area.width.saturating_sub(2));
        let box_h = (4 + self.fields.len() as u16 * 2 + 2).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            style: TextStyle {
                bold: true,
                fg: Some("green".into()),
                ..TextStyle::normal()
            },
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Form {
            rect: Rect::new(1, 1, box_w - 2, box_h - 3),
            fields: self.fields.clone(),
            focused: self.focused,
            submit_label: self.submit_label.clone(),
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: "Tab:next field  Enter:submit  Esc:cancel".into(),
            alignment: Alignment::Center,
            style: TextStyle::muted(),
            role: None,
            label: None,
            description: None,
        });

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::zero(),
            children,
            role: None,
            label: None,
            description: None,
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
                KeyCode::Tab => {
                    self.focused = (self.focused + 1) % (self.fields.len() + 1);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::BackTab => {
                    if self.focused > 0 {
                        self.focused -= 1;
                    } else {
                        self.focused = self.fields.len();
                    }
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    if self.focused == self.fields.len() {
                        let result: Vec<_> = self
                            .fields
                            .iter()
                            .map(|f| (f.label.clone(), f.value.clone()))
                            .collect();
                        EventResult::Response(WidgetResponse {
                            result: Some(serde_json::json!(result)),
                            cancelled: false,
                            error: None,
                        })
                    } else {
                        EventResult::Handled
                    }
                }
                KeyCode::Char(c) if self.focused < self.fields.len() => {
                    self.fields[self.focused].value.push(*c);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Backspace if self.focused < self.fields.len() => {
                    self.fields[self.focused].value.pop();
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