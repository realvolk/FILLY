use crate::event::{Event, KeyCode};
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct ColorPickerWidget {
    pub title: String,
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub channel: usize, // 0=R, 1=G, 2=B
    dirty: bool,
}

impl ColorPickerWidget {
    pub fn new(title: String, _colors: Vec<String>) -> Self {
        Self { title, r: 128, g: 128, b: 128, channel: 0, dirty: true }
    }
}

impl Widget for ColorPickerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 50u16.min(area.width.saturating_sub(2));
        let box_h = 8u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();

        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        });

        // Preview swatch
        let hex = format!("#{:02x}{:02x}{:02x}", self.r, self.g, self.b);
        children.push(RenderNode::Text {
            rect: Rect::new(1, 1, box_w - 2, 1),
            content: hex.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle { fg: Some(hex.clone()), ..TextStyle::normal() },
        });

        // RGB sliders
        let channels = [
            ("R", self.r, "red"),
            ("G", self.g, "green"),
            ("B", self.b, "blue"),
        ];
        for (i, (label, value, color)) in channels.iter().enumerate() {
            let bar_w = box_w - 8;
            let filled = (*value as usize * bar_w as usize) / 255;
            let bar = format!("{}{}", "█".repeat(filled), "░".repeat(bar_w as usize - filled));
            let marker = if i == self.channel { ">" } else { " " };
            children.push(RenderNode::Text {
                rect: Rect::new(1, 2 + i as u16 * 2, 2, 1),
                content: format!("{}{}", marker, label),
                alignment: Alignment::Left,
                role: None, label: None, description: None, style: TextStyle { fg: Some(color.to_string()), ..TextStyle::normal() },
            });
            children.push(RenderNode::Text {
                rect: Rect::new(4, 2 + i as u16 * 2, bar_w, 1),
                content: bar,
                alignment: Alignment::Left,
                role: None, label: None, description: None, style: TextStyle { fg: Some(color.to_string()), ..TextStyle::normal() },
            });
            children.push(RenderNode::Text {
                rect: Rect::new(4 + bar_w, 2 + i as u16 * 2, 4, 1),
                content: format!("{:3}", value),
                alignment: Alignment::Left,
                style: TextStyle::muted(), role: None, label: None, description: None,
            });
        }

        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 1, box_w - 2, 1),
            content: "Up/Down:channel  Left/Right:adjust  Enter:confirm  Esc:cancel".into(),
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
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { self.channel = self.channel.saturating_sub(1); self.dirty = true; EventResult::Handled }
                KeyCode::Down => { if self.channel < 2 { self.channel += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Left => {
                    match self.channel {
                        0 => self.r = self.r.saturating_sub(1),
                        1 => self.g = self.g.saturating_sub(1),
                        2 => self.b = self.b.saturating_sub(1),
                        _ => {}
                    }
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Right => {
                    match self.channel {
                        0 => self.r = self.r.saturating_add(1),
                        1 => self.g = self.g.saturating_add(1),
                        2 => self.b = self.b.saturating_add(1),
                        _ => {}
                    }
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let hex = format!("#{:02x}{:02x}{:02x}", self.r, self.g, self.b);
                    EventResult::Response(WidgetResponse { result: Some(serde_json::Value::String(hex)), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}