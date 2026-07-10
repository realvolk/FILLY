use crate::event::Event;
use crate::render::{Orientation, Rect, RenderNode, RenderTree};
use crate::widget::{EventResult, Widget};

pub struct SeparatorWidget {
    pub orientation: Orientation,
    dirty: bool,
}

impl SeparatorWidget {
    pub fn new(orientation: Orientation) -> Self {
        Self {
            orientation,
            dirty: true,
        }
    }
}

impl Widget for SeparatorWidget {
    fn render(&self, area: Rect) -> RenderTree {
        RenderNode::Separator {
            rect: area,
            orientation: self.orientation,
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, _event: &Event) -> EventResult {
        EventResult::Unhandled
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}