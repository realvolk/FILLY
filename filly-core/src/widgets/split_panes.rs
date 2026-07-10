use crate::event::{Event, KeyCode};
use crate::render::{Orientation, Rect, RenderNode, RenderTree};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct SplitPanesWidget {
    pub orientation: Orientation,
    pub split_position: u16,
    pub first: Box<dyn Widget>,
    pub second: Box<dyn Widget>,
    pub active_pane: usize,
    dirty: bool,
}

impl SplitPanesWidget {
    pub fn new(orientation: Orientation, first: Box<dyn Widget>, second: Box<dyn Widget>) -> Self {
        Self {
            orientation,
            split_position: 50,
            first,
            second,
            active_pane: 0,
            dirty: true,
        }
    }
}

impl Widget for SplitPanesWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let half = area.width / 2;
        let first_rect = Rect::new(0, 0, half, area.height);
        let second_rect = Rect::new(half, 0, area.width - half, area.height);

        let first_tree = self.first.render(first_rect);
        let second_tree = self.second.render(second_rect);

        RenderNode::Container {
            rect: area,
            background: None,
            border: crate::render::BorderStyle::None,
            padding: crate::render::EdgeInsets::zero(),
            children: vec![
                RenderNode::Container {
                    rect: first_rect,
                    background: if self.active_pane == 0 {
                        Some("darkgray".into())
                    } else {
                        None
                    },
                    border: crate::render::BorderStyle::Single,
                    padding: crate::render::EdgeInsets::zero(),
                    children: vec![first_tree],
                    role: None,
                    label: None,
                    description: None,
                },
                RenderNode::Container {
                    rect: second_rect,
                    background: if self.active_pane == 1 {
                        Some("darkgray".into())
                    } else {
                        None
                    },
                    border: crate::render::BorderStyle::Single,
                    padding: crate::render::EdgeInsets::zero(),
                    children: vec![second_tree],
                    role: None,
                    label: None,
                    description: None,
                },
            ],
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        if let Event::Key(code, _) = event {
            match code {
                KeyCode::F(1) | KeyCode::Left => {
                    self.active_pane = 0;
                    self.dirty = true;
                    return EventResult::Handled;
                }
                KeyCode::F(2) | KeyCode::Right => {
                    self.active_pane = 1;
                    self.dirty = true;
                    return EventResult::Handled;
                }
                _ => {}
            }
        }

        let result = if self.active_pane == 0 {
            self.first.handle_event(event)
        } else {
            self.second.handle_event(event)
        };

        if matches!(result, EventResult::Handled) {
            self.dirty = true;
        }
        result
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}