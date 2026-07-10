use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct RangeSliderWidget {
    pub title: String,
    pub min: i32,
    pub max: i32,
    pub value: i32,
    pub label: String,
    dirty: bool,
}

impl RangeSliderWidget {
    pub fn new(title: String, min: i32, max: i32, value: i32, label: String) -> Self {
        Self {
            title,
            min,
            max,
            value,
            label,
            dirty: true,
        }
    }
}

impl Widget for RangeSliderWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 50u16.min(area.width.saturating_sub(2));
        let box_h = 5u16.min(area.height.saturating_sub(2));
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

        let range = self.max - self.min;
        let pos = if range == 0 {
            0
        } else {
            ((self.value - self.min) as f32 / range as f32 * (box_w - 4) as f32)
                as usize
        };
        let bar = format!(
            "{}{}{}",
            "─".repeat(pos),
            "●",
            "─".repeat((box_w - 4) as usize - pos)
        );
        children.push(RenderNode::Text {
            rect: Rect::new(2, 2, box_w - 4, 1),
            content: bar,
            alignment: Alignment::Left,
            style: TextStyle::accent(),
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, 3, box_w - 2, 1),
            content: format!("{}: {}", self.label, self.value),
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
                KeyCode::Left => {
                    self.value = (self.value - 1).max(self.min);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Right => {
                    self.value = (self.value + 1).min(self.max);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::Number(serde_json::Number::from(
                        self.value,
                    ))),
                    cancelled: false,
                    error: None,
                }),
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