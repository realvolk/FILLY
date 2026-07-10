use crate::event::Event;
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};

pub struct TooltipWidget {
    pub text: String,
    dirty: bool,
}

impl TooltipWidget {
    pub fn new(text: String) -> Self { Self { text, dirty: true } }
}

impl Widget for TooltipWidget {
    fn render(&self, area: Rect) -> RenderTree {
        RenderNode::Text { rect: area, content: self.text.clone(), alignment: Alignment::Left, role: None, label: None, description: None, style: TextStyle { fg: Some("yellow".into()), ..TextStyle::normal() } }
    }
    fn handle_event(&mut self, _event: &Event) -> EventResult {
        EventResult::Unhandled
    }
    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}