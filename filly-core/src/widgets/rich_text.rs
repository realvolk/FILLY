use crate::event::Event;
use crate::render::{Alignment, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};

pub struct RichTextWidget {
    pub content: String,
    dirty: bool,
}

impl RichTextWidget {
    pub fn new(content: String) -> Self {
        Self {
            content,
            dirty: true,
        }
    }

    fn parse_links(&self) -> Vec<(String, Option<String>)> {
        let mut segments = Vec::new();
        let mut remaining = self.content.as_str();
        while let Some(start) = remaining.find('[') {
            if start > 0 {
                segments.push((remaining[..start].to_string(), None));
            }
            remaining = &remaining[start + 1..];
            if let Some(end) = remaining.find("](") {
                let text = remaining[..end].to_string();
                remaining = &remaining[end + 2..];
                if let Some(close) = remaining.find(')') {
                    let url = remaining[..close].to_string();
                    segments.push((text, Some(url)));
                    remaining = &remaining[close + 1..];
                } else {
                    segments.push((format!("[{}", text), None));
                    break;
                }
            } else {
                segments.push((format!("[{}", remaining), None));
                break;
            }
        }
        if !remaining.is_empty() {
            segments.push((remaining.to_string(), None));
        }
        segments
    }
}

impl Widget for RichTextWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let segments = self.parse_links();
        let mut children = Vec::new();
        let mut y = area.y;
        let mut x = area.x;

        for (text, url) in &segments {
            let w = text.len() as u16;
            if x + w > area.x + area.width {
                y += 1;
                x = area.x;
            }
            if let Some(link) = url {
                let content =
                    format!("\x1b]8;;{}\x1b\\{}\x1b]8;;\x1b\\", link, text);
                children.push(RenderNode::Text {
                    rect: Rect::new(x, y, w, 1),
                    content,
                    alignment: Alignment::Left,
                    style: TextStyle {
                        fg: Some("blue".into()),
                        underline: true,
                        ..TextStyle::normal()
                    },
                    role: None,
                    label: None,
                    description: None,
                });
            } else {
                children.push(RenderNode::Text {
                    rect: Rect::new(x, y, w, 1),
                    content: text.clone(),
                    alignment: Alignment::Left,
                    style: TextStyle::normal(),
                    role: None,
                    label: None,
                    description: None,
                });
            }
            x += w;
        }

        RenderTree::Container {
            rect: area,
            background: None,
            border: crate::render::BorderStyle::None,
            padding: crate::render::EdgeInsets::zero(),
            children,
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, _event: &Event) -> EventResult {
        EventResult::Unhandled
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}