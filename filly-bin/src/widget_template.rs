use crate::event::{Event, KeyCode};
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct {{NAME}}Widget {
    pub title: String,
}

impl {{NAME}}Widget {
    pub fn new(title: String) -> Self {
        Self { title }
    }
}

impl Widget for {{NAME}}Widget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = (area.width as f32 * 0.6) as u16;
        let box_h = (area.height as f32 * 0.4) as u16;
        let box_x = (area.width - box_w) / 2;
        let box_y = (area.height - box_h) / 2;

        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, 2, box_w - 2, 1),
            content: "Widget content here".into(),
            alignment: Alignment::Center,
            style: TextStyle::normal(),
        });

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::zero(),
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
                KeyCode::Enter => EventResult::Response(WidgetResponse {
                    result: Some(serde_json::Value::String("done".into())),
                    cancelled: false,
                    error: None,
                }),
                _ => EventResult::Unhandled,
            }
        } else {
            EventResult::Unhandled
        }
    }
}