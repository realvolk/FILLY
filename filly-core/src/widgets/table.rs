use crate::event::{Event, KeyCode};
use crate::render::{Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct TableWidget {
    pub title: String,
    pub headers: Vec<String>,
    pub rows: Vec<Vec<String>>,
    pub selected_row: usize,
    pub selected_col: usize,
    dirty: bool,
}

impl TableWidget {
    pub fn new(title: String, headers: Vec<String>, rows: Vec<Vec<String>>) -> Self {
        Self { title, headers, rows, selected_row: 0, selected_col: 0, dirty: true }
    }
}

impl Widget for TableWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.9) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;


        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        });
        children.push(RenderNode::Table {
            rect: Rect::new(1, 1, box_w - 2, box_h - 3),
            headers: self.headers.clone(),
            rows: self.rows.clone(),
            selected_row: Some(self.selected_row),
            selected_col: Some(self.selected_col),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: "Up/Down:row  Left/Right:col  Enter:select  Esc:cancel".into(),
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
                KeyCode::Up => { if self.selected_row > 0 { self.selected_row -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.selected_row + 1 < self.rows.len() { self.selected_row += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Left => { if self.selected_col > 0 { self.selected_col -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Right => { if self.selected_col + 1 < self.headers.len() { self.selected_col += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Enter => {
                    let cell = self.rows.get(self.selected_row).and_then(|r| r.get(self.selected_col)).cloned().unwrap_or_default();
                    EventResult::Response(WidgetResponse { result: Some(serde_json::Value::String(cell)), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}