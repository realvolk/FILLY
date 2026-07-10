use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct PasswordWidget {
    pub title: String,
    pub message: String,
    pub text: String,
    pub placeholder: String,
    dirty: bool,
}

impl PasswordWidget {
    pub fn new(title: String, message: String, placeholder: Option<String>) -> Self {
        Self {
            title,
            message,
            text: String::new(),
            placeholder: placeholder.unwrap_or_default(),
            dirty: true,
        }
    }
}

impl Widget for PasswordWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 60u16.min(area.width.saturating_sub(2));
        let box_h = 10u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        if !self.title.is_empty() {
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
        }
        if !self.message.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, 2),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::normal(),
                role: None,
                label: None,
                description: None,
            });
        }

        let input_y = if self.message.is_empty() { 2 } else { 4 };
        children.push(RenderNode::Input {
            rect: Rect::new(1, input_y, box_w - 2, 1),
            text: self.text.clone(),
            cursor: 0,
            placeholder: self.placeholder.clone(),
            masked: true,
            role: None,
            label: None,
            description: None,
        });

        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Type + Enter:confirm  Esc:cancel".into(),
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
                KeyCode::Enter => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::String(self.text.clone())),
                    cancelled: false,
                    error: None,
                }),
                KeyCode::Char(c) => {
                    self.text.push(*c);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Backspace => {
                    self.text.pop();
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