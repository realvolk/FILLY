use crate::event::{Event, KeyCode};
use crate::render::{Rect, RenderNode, RenderTree};
use crate::widget::{EventResult, Widget};

pub struct TabsWidget {
    pub title: String,
    pub tabs: Vec<String>,
    pub active: usize,
    pub children: Vec<Box<dyn Widget>>,
    dirty: bool,
}

impl TabsWidget {
    pub fn new(title: String, tabs: Vec<String>, children: Vec<Box<dyn Widget>>) -> Self {
        Self {
            title,
            tabs,
            active: 0,
            children,
            dirty: true,
        }
    }
}

impl Widget for TabsWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let child = if let Some(w) = self.children.get(self.active) {
            w.render(area)
        } else {
            RenderNode::Text {
                rect: Rect::new(0, 0, 1, 1),
                content: String::new(),
                alignment: crate::render::Alignment::Left,
                style: crate::render::TextStyle::normal(),
                role: None,
                label: None,
                description: None,
            }
        };
        RenderNode::Tabs {
            rect: Rect::new(0, 0, area.width, area.height),
            tabs: self.tabs.clone(),
            active: self.active,
            child: Box::new(child),
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        if let Event::Key(code, mods) = event {
            if *mods == crate::KeyModifiers::ctrl() {
                if *code == KeyCode::Tab {
                    self.active = (self.active + 1) % self.tabs.len();
                    self.dirty = true;
                    return EventResult::Handled;
                }
            }
            match code {
                KeyCode::Left => {
                    if self.active > 0 {
                        self.active -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Right => {
                    if self.active + 1 < self.tabs.len() {
                        self.active += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                _ => {
                    if let Some(child) = self.children.get_mut(self.active) {
                        let result = child.handle_event(event);
                        if matches!(result, EventResult::Handled) {
                            self.dirty = true;
                        }
                        result
                    } else {
                        EventResult::Unhandled
                    }
                }
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