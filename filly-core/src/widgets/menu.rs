use crate::event::{Event, KeyCode, KeyModifiers};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct MenuWidget {
    pub title: String,
    pub message: String,
    pub choices: Vec<String>,
    pub selected: usize,
    dirty: bool,
}

impl MenuWidget {
    pub fn new(
        title: String,
        message: String,
        choices: Vec<String>,
        default: Option<usize>,
    ) -> Self {
        let selected = default.unwrap_or(0).min(choices.len().saturating_sub(1));
        Self {
            title,
            message,
            choices,
            selected,
            dirty: true,
        }
    }
}

impl Widget for MenuWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.6) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;


        let mut children = Vec::new();
        if !self.title.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 0, box_w - 2, 1),
                content: self.title.clone(),
                alignment: Alignment::Center,
                role: None, label: None, description: None, style: TextStyle {
                    bold: true,
                    fg: Some("green".into()),
                    ..TextStyle::normal()
                },
            });
        }
        if !self.message.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, 2),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::normal(), role: None, label: None, description: None,
            });
        }

        let list_y = if self.message.is_empty() { 1 } else { 3 };
        let list_h = box_h.saturating_sub(list_y).saturating_sub(2);
        let items: Vec<ListItem> = self
            .choices
            .iter()
            .map(|c| ListItem {
                label: c.clone(),
                meta: None,
            })
            .collect();

        children.push(RenderNode::List {
            rect: Rect::new(1, list_y, box_w - 2, list_h),
            items,
            selected: Some(self.selected),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        let help_y = box_h.saturating_sub(1);
        children.push(RenderNode::Text {
            rect: Rect::new(1, help_y, box_w - 2, 1),
            content: "Up/Down:move  Enter:select  Esc:cancel  Ctrl+C:quit".into(),
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
        match event {
            Event::Key(code, mods) => {
                if *mods == KeyModifiers::ctrl() && *code == KeyCode::Char('c') {
                    return EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: None,
                    });
                }
                match code {
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
                        if self.selected + 1 < self.choices.len() {
                            self.selected += 1;
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        let result = self.choices.get(self.selected).cloned();
                        EventResult::Response(WidgetResponse {
                            result: result.map(serde_json::Value::String),
                            cancelled: false,
                            error: None,
                        })
                    }
                    _ => EventResult::Unhandled,
                }
            }
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