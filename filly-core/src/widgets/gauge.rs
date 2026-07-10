use crate::event::Event;
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct GaugeWidget {
    pub title: String,
    pub percent: u16,
    pub label: String,
    dirty: bool,
}

impl GaugeWidget {
    pub fn new(title: String, percent: u16, label: String) -> Self {
        Self {
            title,
            percent,
            label,
            dirty: true,
        }
    }
}

impl Widget for GaugeWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 50u16.min(area.width.saturating_sub(2));
        let box_h = 6u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
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
        children.push(RenderNode::Gauge {
            rect: Rect::new(1, 2, box_w - 2, 2),
            percent: self.percent,
            label: self.label.clone(),
            style: TextStyle::accent(),
            role: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Any key to dismiss".into(),
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
        if let Event::Key(_, _) = event {
            EventResult::Response(WidgetResponse {
                result: None,
                cancelled: false,
                error: None,
            })
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