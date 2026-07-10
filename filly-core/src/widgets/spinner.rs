use crate::event::Event;
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::time::Instant;

pub struct SpinnerWidget {
    pub message: String,
    pub frame: usize,
    pub start: Instant,
    dirty: bool,
}

impl SpinnerWidget {
    pub fn new(message: String) -> Self {
        Self {
            message,
            frame: 0,
            start: Instant::now(),
            dirty: true,
        }
    }
}

impl Widget for SpinnerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 40u16.min(area.width.saturating_sub(2));
        let box_h = 5u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        children.push(RenderNode::Spinner {
            rect: Rect::new(1, 1, box_w - 2, 1),
            message: self.message.clone(),
            frame: self.frame,
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Please wait...".into(),
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
        self.frame = self.frame.wrapping_add(1);
        self.dirty = true;
        if let Event::Key(code, _) = event {
            if *code == crate::KeyCode::Esc {
                return EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                });
            }
        }
        EventResult::Handled
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}