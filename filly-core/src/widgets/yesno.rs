use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct YesNoWidget {
    pub title: String,
    pub message: String,
    pub selected_yes: bool,
    dirty: bool,
}

impl YesNoWidget {
    pub fn new(title: String, message: String, default: Option<bool>) -> Self {
        Self {
            title,
            message,
            selected_yes: default.unwrap_or(true),
            dirty: true,
        }
    }
}

impl Widget for YesNoWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 50u16.min(area.width.saturating_sub(2));
        let box_h = 10u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        if !self.title.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 0, box_w - 2, 1),
                content: self.title.clone(),
                alignment: Alignment::Center,
                style: TextStyle {
                    bold: true,
                    fg: Some("green".into()),
                    ..TextStyle::normal()
                },
                role: None,
                label: None,
                description: None,
            });
        }
        children.push(RenderNode::Text {
            rect: Rect::new(1, 2, box_w - 2, 2),
            content: self.message.clone(),
            alignment: Alignment::Center,
            style: TextStyle::normal(),
            role: None,
            label: None,
            description: None,
        });

        let yes_style = if self.selected_yes {
            TextStyle::selected()
        } else {
            TextStyle::muted()
        };
        let no_style = if !self.selected_yes {
            TextStyle::selected()
        } else {
            TextStyle::muted()
        };
        let left = (box_w - 20) / 2;
        children.push(RenderNode::Text {
            rect: Rect::new(left, 5, 8, 1),
            content: "[ Yes ]".into(),
            alignment: Alignment::Center,
            style: yes_style,
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(left + 10, 5, 8, 1),
            content: "[ No ]".into(),
            alignment: Alignment::Center,
            style: no_style,
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, 7, box_w - 2, 1),
            content: "h/l:choose  Enter:confirm  y/n:quick  Esc:cancel".into(),
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
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                }),
                KeyCode::Left | KeyCode::Char('h') | KeyCode::Tab => {
                    self.selected_yes = !self.selected_yes;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Right | KeyCode::Char('l') => {
                    self.selected_yes = !self.selected_yes;
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter | KeyCode::Char('\n') | KeyCode::Char('\r') => {
                    EventResult::Response(WidgetResponse {
                        result: Some(serde_json::Value::Bool(self.selected_yes)),
                        cancelled: false,
                        error: None,
                    })
                }
                KeyCode::Char('y') => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::Bool(true)),
                    cancelled: false,
                    error: None,
                }),
                KeyCode::Char('n') => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::Bool(false)),
                    cancelled: false,
                    error: None,
                }),
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