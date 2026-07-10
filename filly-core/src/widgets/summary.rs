use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use std::fs;

pub struct SummaryWidget {
    pub title: String,
    pub text: String,
    pub scroll: usize,
    dirty: bool,
}

impl SummaryWidget {
    pub fn new(title: String, message: Option<String>, file: Option<String>) -> Self {
        let text = if let Some(path) = file {
            fs::read_to_string(&path).unwrap_or_else(|_| format!("[Error reading {}]", path))
        } else {
            message.unwrap_or_default()
        };
        Self {
            title,
            text,
            scroll: 0,
            dirty: true,
        }
    }
}

impl Widget for SummaryWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.8) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;


        let mut children = Vec::new();
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
        children.push(RenderNode::Text {
            rect: Rect::new(1, 1, box_w - 2, box_h - 2),
            content: self.text.clone(),
            alignment: Alignment::Left,
            style: TextStyle::normal(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Any key to continue  Esc:cancel".into(),
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
        if let Event::Key(code, _) = event {
            match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                }),
                _ => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: false,
                    error: None,
                }),
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