use crate::event::Event;
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};

pub struct BadgeWidget {
    pub text: String,
    pub color: String,
    dirty: bool,
}

impl BadgeWidget {
    pub fn new(text: String) -> Self {
        Self { text, color: "green".into(), dirty: true }
    }
}

impl Widget for BadgeWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let w = (self.text.len() as u16 + 4).min(area.width);
        let h = 1u16;
        let x = area.x;
        let y = area.y;

        RenderTree::Container {
            rect: Rect::new(x, y, w, h),
            background: Some(self.color.clone()),
            border: BorderStyle::Rounded,
            padding: EdgeInsets::uniform(1), role: None, label: None, description: None,
            children: vec![
                RenderNode::Text {
                    rect: Rect::new(1, 0, w.saturating_sub(2), 1),
                    content: self.text.clone(),
                    alignment: Alignment::Center,
                    role: None, label: None, description: None, style: TextStyle { fg: Some("white".into()), bold: true, ..TextStyle::normal() },
                },
            ],
        }
    }

    fn handle_event(&mut self, _event: &Event) -> EventResult {
        EventResult::Unhandled
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}