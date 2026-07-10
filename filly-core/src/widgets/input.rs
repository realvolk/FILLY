use crate::event::{Event, KeyCode, KeyModifiers};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use regex_lite::Regex;

pub struct InputWidget {
    pub title: String,
    pub message: String,
    pub text: String,
    pub cursor: usize,
    pub placeholder: String,
    pub validation: Option<Regex>,
    dirty: bool,
}

impl InputWidget {
    pub fn new(
        title: String,
        message: String,
        default: Option<String>,
        placeholder: Option<String>,
        validation: Option<String>,
    ) -> Self {
        let text = default.unwrap_or_default();
        let cursor = text.len();
        let re = validation.and_then(|v| Regex::new(&v).ok());
        Self {
            title,
            message,
            text,
            cursor,
            placeholder: placeholder.unwrap_or_default(),
            validation: re,
            dirty: true,
        }
    }
}

impl Widget for InputWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 60u16.min(area.width.saturating_sub(2));
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
        if !self.message.is_empty() {
            children.push(RenderNode::Text {
                rect: Rect::new(1, 1, box_w - 2, 2),
                content: self.message.clone(),
                alignment: Alignment::Left,
                style: TextStyle::normal(),
                role: None,
                label: None,
                description: None,
            });
        }

        let input_y = if self.message.is_empty() { 2 } else { 4 };
        children.push(RenderNode::Input {
            rect: Rect::new(1, input_y, box_w - 2, 1),
            text: self.text.clone(),
            cursor: self.cursor,
            placeholder: self.placeholder.clone(),
            masked: false,
            role: None,
            label: None,
            description: None,
        });

        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Type + Enter:confirm  Ctrl+V:paste  Esc:cancel  Ctrl+C:quit".into(),
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
            Event::Key(code, mods) => {
                if *mods == KeyModifiers::ctrl() && *code == KeyCode::Char('c') {
                    return EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: None,
                    });
                }
                if *mods == KeyModifiers::ctrl() && *code == KeyCode::Char('v') {
                    if let Some(clip) =
                        crate::clipboard::paste_from_clipboard(&mut std::io::stdout())
                    {
                        self.text.insert_str(self.cursor, &clip);
                        self.cursor += clip.len();
                        self.dirty = true;
                    }
                    return EventResult::Handled;
                }
                match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse {
                        result: None,
                        cancelled: true,
                        error: None,
                    }),
                    KeyCode::Enter => {
                        if let Some(ref re) = self.validation {
                            if !re.is_match(&self.text) {
                                return EventResult::Handled;
                            }
                        }
                        EventResult::Response(WidgetResponse {
                            result: Some(serde_json::Value::String(self.text.clone())),
                            cancelled: false,
                            error: None,
                        })
                    }
                    KeyCode::Char(c) => {
                        self.text.insert(self.cursor, *c);
                        self.cursor += 1;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Backspace => {
                        if self.cursor > 0 {
                            self.text.remove(self.cursor - 1);
                            self.cursor -= 1;
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Delete => {
                        if self.cursor < self.text.len() {
                            self.text.remove(self.cursor);
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Left => {
                        if self.cursor > 0 {
                            self.cursor -= 1;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Right => {
                        if self.cursor < self.text.len() {
                            self.cursor += 1;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Home => {
                        self.cursor = 0;
                        EventResult::Handled
                    }
                    KeyCode::End => {
                        self.cursor = self.text.len();
                        EventResult::Handled
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