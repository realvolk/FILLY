use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct ToggleWidget {
    pub title: String,
    pub label: String,
    pub value: bool,
    pub focused: bool,
    dirty: bool,
}

impl ToggleWidget {
    pub fn new(title: String, label: String, default: Option<bool>) -> Self {
        Self {
            title,
            label,
            value: default.unwrap_or(false),
            focused: true,
            dirty: true,
        }
    }
}

impl Widget for ToggleWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 40u16.min(area.width.saturating_sub(2));
        let box_h = 5u16.min(area.height.saturating_sub(2));
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
        children.push(RenderNode::Toggle {
            rect: Rect::new(1, 2, box_w - 2, 1),
            label: self.label.clone(),
            value: self.value,
            focused: self.focused,
            role: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Space:toggle  Enter:confirm  Esc:cancel".into(),
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
        if let Event::Key(code, _) = event {
            match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                }),
                KeyCode::Char(' ') => {
                    self.value = !self.value;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::Bool(self.value)),
                    cancelled: false,
                    error: None,
                }),
                _ => EventResult::Unhandled,
            }
        } else {
            EventResult::Unhandled
        }
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}