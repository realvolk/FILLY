use crate::event::Event;
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::time::Instant;

pub struct NotificationWidget {
    pub message: String,
    pub start: Instant,
    pub duration: std::time::Duration,
    dirty: bool,
}

impl NotificationWidget {
    pub fn new(message: String, seconds: u64) -> Self {
        Self {
            message,
            start: Instant::now(),
            duration: std::time::Duration::from_secs(seconds),
            dirty: true,
        }
    }
}

impl Widget for NotificationWidget {
    fn render(&self, area: Rect) -> RenderTree {
        if self.start.elapsed() > self.duration {
            return RenderNode::Text {
                rect: Rect::new(0, 0, 0, 0),
                content: String::new(),
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            };
        }
        let w = 40u16.min(area.width.saturating_sub(4));
        let h = 3u16;
        let x = area.x + area.width.saturating_sub(w).saturating_sub(2);
        let y = area.y + area.height.saturating_sub(h).saturating_sub(2);

        RenderTree::Container {
            rect: Rect::new(x, y, w, h),
            background: Some("darkgray".into()),
            border: BorderStyle::Single,
            padding: EdgeInsets::uniform(1), role: None, label: None, description: None,
            children: vec![RenderNode::Text {
                rect: Rect::new(1, 0, w.saturating_sub(2), 1),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::accent(), role: None, label: None, description: None,
            }],
        }
    }

    fn handle_event(&mut self, _event: &Event) -> EventResult {
        EventResult::Response(WidgetResponse {
            result: None,
            cancelled: false,
            error: None,
        })
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}