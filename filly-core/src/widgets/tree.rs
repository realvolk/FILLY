use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle, TreeNode,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct TreeWidget {
    pub title: String,
    pub nodes: Vec<TreeNode>,      // original tree (expanded state lives here)
    pub cursor: usize,             // index into the flat visible list
    dirty: bool,
}

impl TreeWidget {
    pub fn new(title: String, nodes: Vec<TreeNode>) -> Self {
        Self { title, nodes, cursor: 0, dirty: true }
    }

    fn flatten(&self) -> Vec<(usize, String, bool)> {
        let mut out = Vec::new();
        Self::flatten_rec(&self.nodes, 0, &mut out);
        out
    }

    fn flatten_rec(nodes: &[TreeNode], depth: usize, out: &mut Vec<(usize, String, bool)>) {
        for node in nodes {
            let marker = if node.children.is_empty() {
                "  "
            } else if node.expanded {
                "▼ "
            } else {
                "▶ "
            };
            out.push((depth, format!("{}{}", marker, node.label), node.children.is_empty()));
            if node.expanded {
                Self::flatten_rec(&node.children, depth + 1, out);
            }
        }
    }

    fn toggle_expand(&mut self) {
        let flat = self.flatten();
        if let Some((_, _, is_leaf)) = flat.get(self.cursor) {
            if *is_leaf { return; }
            let mut remaining = self.cursor;
            if Self::toggle_at(&mut self.nodes, &mut remaining) {
                self.dirty = true;
            }
        }
    }

    fn toggle_at(nodes: &mut [TreeNode], idx: &mut usize) -> bool {
        for node in nodes.iter_mut() {
            if *idx == 0 {
                if !node.children.is_empty() {
                    node.expanded = !node.expanded;
                    return true;
                }
                return false;
            }
            *idx -= 1;
            if node.expanded {
                if Self::toggle_at(&mut node.children, idx) {
                    return true;
                }
            }
        }
        false
    }
}

impl Widget for TreeWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.7) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.8) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;
        let flat = self.flatten();
        let items: Vec<ListItem> = flat.iter().map(|(depth, label, _)| {
            ListItem {
                label: format!("{:indent$}{}", "", label, indent = depth * 2),
                meta: None,
            }
        }).collect();

        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: self.title.clone(),
            alignment: Alignment::Center,
            role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        });
        children.push(RenderNode::List {
            rect: Rect::new(1, 1, box_w - 2, box_h - 3),
            items,
            selected: Some(self.cursor.min(flat.len().saturating_sub(1))),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: "Up/Down:move  Enter/Space:expand  Esc:cancel".into(),
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
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None, cancelled: true, error: None,
                }),
                KeyCode::Up => {
                    if self.cursor > 0 { self.cursor -= 1; self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Down => {
                    let len = self.flatten().len();
                    if self.cursor + 1 < len { self.cursor += 1; self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Enter | KeyCode::Char(' ') => {
                    self.toggle_expand();
                    EventResult::Handled
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}