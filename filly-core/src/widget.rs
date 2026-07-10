use crate::event::Event;
use crate::render::{Rect, RenderTree};
use filly_protocol::WidgetResponse;

pub enum EventResult {
    Handled,
    Response(WidgetResponse),
    Yield(serde_json::Value),
    Unhandled,
}

pub trait Widget {
    fn render(&self, area: Rect) -> RenderTree;
    fn handle_event(&mut self, event: &Event) -> EventResult;
    fn size_hint(&self) -> (u16, u16) { (0, 0) }
    fn is_dirty(&self) -> bool { true }
    fn clear_dirty(&mut self) {}
}

pub struct CompositeWidget {
    pub children: Vec<Box<dyn Widget>>,
    pub focused: usize,
}

impl CompositeWidget {
    pub fn new(children: Vec<Box<dyn Widget>>) -> Self {
        Self { children, focused: 0 }
    }

    pub fn focus_next(&mut self) {
        if !self.children.is_empty() {
            self.focused = (self.focused + 1) % self.children.len();
        }
    }

    pub fn focus_prev(&mut self) {
        if !self.children.is_empty() {
            self.focused = (self.focused + self.children.len() - 1) % self.children.len();
        }
    }
}

impl Widget for CompositeWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let count = self.children.len().max(1) as u16;
        let height_per_child = area.height / count;
        let mut children_nodes = Vec::new();
        for (i, child) in self.children.iter().enumerate() {
            let child_area = Rect::new(
                area.x,
                area.y + i as u16 * height_per_child,
                area.width,
                height_per_child,
            );
            children_nodes.push(child.render(child_area));
        }
        RenderTree::Container {
            rect: area,
            background: None,
            border: crate::render::BorderStyle::None,
            padding: crate::render::EdgeInsets::zero(),
            children: children_nodes,
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        if let Some(child) = self.children.get_mut(self.focused) {
            child.handle_event(event)
        } else {
            EventResult::Unhandled
        }
    }
}

impl Widget for Box<dyn Widget> {
    fn render(&self, area: Rect) -> RenderTree {
        self.as_ref().render(area)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        self.as_mut().handle_event(event)
    }
}