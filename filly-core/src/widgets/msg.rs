use crate::event::Event;
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct MsgWidget {
    pub title: String,
    pub message: String,
    dirty: bool,
}

impl MsgWidget {
    pub fn new(title: String, message: String) -> Self {
        Self { title, message, dirty: true }
    }
}

impl Widget for MsgWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.6) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.4) as u16).min(area.height.saturating_sub(2)).max(5);
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;
        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, 2, box_w - 2, box_h - 4),
            content: self.message.clone(),
            alignment: Alignment::Left,
            style: TextStyle::normal(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: "Any key to continue".into(),
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
        if let Event::Key(_, _) = event {
            EventResult::Response(WidgetResponse { result: None, cancelled: false, error: None })
        } else { EventResult::Unhandled }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}