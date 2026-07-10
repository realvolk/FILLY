use crate::event::{Event, KeyCode};
use crate::render::{Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct ContextMenuWidget {
    pub items: Vec<String>,
    pub selected: usize,
    dirty: bool,
}

impl ContextMenuWidget {
    pub fn new(items: Vec<String>) -> Self {
        Self {
            items,
            selected: 0,
            dirty: true,
        }
    }
}

impl Widget for ContextMenuWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let w = 25u16.min(area.width.saturating_sub(4));
        let h = (self.items.len() as u16 + 2).min(area.height.saturating_sub(4));
        let x = (area.width.saturating_sub(w)) / 2;
        let y = (area.height.saturating_sub(h)) / 2;

        let item_list: Vec<ListItem> = self.items.iter().map(|i| ListItem {
            label: i.clone(),
            meta: None,
        }).collect();

        RenderTree::Container {
            rect: Rect::new(x, y, w, h),
            background: Some("darkgray".into()),
            border: BorderStyle::Single,
            padding: EdgeInsets::uniform(1), role: None, label: None, description: None,
            children: vec![
                RenderNode::List {
                    rect: Rect::new(0, 0, w.saturating_sub(2), h.saturating_sub(2)),
                    items: item_list,
                    selected: Some(self.selected),
                    highlight_style: TextStyle::selected(), role: None, label: None, description: None,
                },
                RenderNode::Text {
                    rect: Rect::new(0, h.saturating_sub(2), w.saturating_sub(2), 1),
                    content: "Up/Down:move  Enter:select  Esc:cancel".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                },
            ],
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
                KeyCode::Up => {
                    if self.selected > 0 {
                        self.selected -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Down => {
                    if self.selected + 1 < self.items.len() {
                        self.selected += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let choice = self.items.get(self.selected).cloned();
                    EventResult::Response(WidgetResponse {
                        result: choice.map(serde_json::Value::String),
                        cancelled: false,
                        error: None,
                    })
                }
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