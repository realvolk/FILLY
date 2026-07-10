use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::store::Store;
use crate::widget::{EventResult, Widget};
use crate::widgets;
use filly_protocol::{WidgetRequest, WidgetResponse};
use serde_json::Value;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct HubCategory {
    pub label: String,
    pub summary_template: String,
    pub items: Vec<HubItem>,
}

#[derive(Debug, Clone)]
pub struct HubItem {
    pub id: String,
    pub label: String,
    pub value: String,
    pub widget: String,
    pub choices: Vec<String>,
    pub placeholder: String,
    pub visible_if: HashMap<String, String>,
    pub display: String,
}

#[derive(Debug, Clone, PartialEq)]
pub enum HubMode {
    Browsing,
    EditingItem,
    ConfirmQuit,
}

pub struct HubComposite {
    pub categories: Vec<HubCategory>,
    pub actions: Vec<String>,
    pub cat_idx: usize,
    pub item_idx: usize,
    pub mode: HubMode,
    pub store: Store,
    pub sub_editor: Option<Box<dyn Widget>>,
    dirty: bool,
}

impl HubComposite {
    pub fn from_params(params: &Value, store: &Store) -> Self {
        let mut categories = Vec::new();
        if let Some(arr) = params.get("categories").and_then(|v| v.as_array()) {
            for cat_val in arr {
                let label = cat_val
                    .get("label")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .to_string();
                let summary_template = cat_val
                    .get("summary_template")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .to_string();
                let mut items = Vec::new();
                if let Some(items_arr) = cat_val.get("items").and_then(|v| v.as_array()) {
                    for item_val in items_arr {
                        let id = item_val
                            .get("id")
                            .and_then(|v| v.as_str())
                            .unwrap_or("")
                            .to_string();
                        if id.is_empty() {
                            continue;
                        }
                        let label = item_val
                            .get("label")
                            .and_then(|v| v.as_str())
                            .unwrap_or("")
                            .to_string();
                        let value = item_val
                            .get("value")
                            .and_then(|v| v.as_str())
                            .unwrap_or("")
                            .to_string();
                        let widget = item_val
                            .get("widget")
                            .and_then(|v| v.as_str())
                            .unwrap_or("menu")
                            .to_string();
                        let choices: Vec<String> = item_val
                            .get("choices")
                            .and_then(|v| v.as_array())
                            .map(|a| {
                                a.iter()
                                    .filter_map(|v| v.as_str().map(String::from))
                                    .collect()
                            })
                            .unwrap_or_default();
                        let placeholder = item_val
                            .get("placeholder")
                            .and_then(|v| v.as_str())
                            .unwrap_or("")
                            .to_string();
                        let visible_if: HashMap<String, String> = item_val
                            .get("visible_if")
                            .and_then(|v| v.as_object())
                            .map(|o| {
                                o.iter()
                                    .map(|(k, v)| {
                                        (k.clone(), v.as_str().unwrap_or("").to_string())
                                    })
                                    .collect()
                            })
                            .unwrap_or_default();
                        let display = item_val
                            .get("display")
                            .and_then(|v| v.as_str())
                            .unwrap_or("")
                            .to_string();
                        items.push(HubItem {
                            id,
                            label,
                            value,
                            widget,
                            choices,
                            placeholder,
                            visible_if,
                            display,
                        });
                    }
                }
                categories.push(HubCategory {
                    label,
                    summary_template,
                    items,
                });
            }
        }
        let actions: Vec<String> = params
            .get("actions")
            .and_then(|v| v.as_array())
            .map(|a| {
                a.iter()
                    .filter_map(|v| v.as_str().map(String::from))
                    .collect()
            })
            .unwrap_or_default();
        let mut store = store.clone();
        for cat in &categories {
            for item in &cat.items {
                store.set_str(item.id.clone(), &item.value);
            }
        }
        Self {
            categories,
            actions,
            cat_idx: 0,
            item_idx: 0,
            mode: HubMode::Browsing,
            store,
            sub_editor: None,
            dirty: true,
        }
    }

    fn visible_items(&self) -> Vec<&HubItem> {
        if self.categories.is_empty() {
            return vec![];
        }
        let cat = &self.categories[self.cat_idx];
        cat.items
            .iter()
            .filter(|item| {
                if item.visible_if.is_empty() {
                    return true;
                }
                item.visible_if
                    .iter()
                    .all(|(k, v)| self.store.get_string(k) == *v)
            })
            .collect()
    }

    fn start_edit_item(&mut self) {
        let items: Vec<&HubItem> = self.visible_items();
        if let Some(item) = items.get(self.item_idx) {
            let current_val = self.store.get_string(&item.id);
            let req = WidgetRequest {
                widget: item.widget.clone(),
                step: None,
                total: None,
                params: serde_json::json!({
                    "title": item.label,
                    "message": "",
                    "choices": item.choices,
                    "default": current_val,
                    "placeholder": item.placeholder,
                }),
                session_id: None,
            };
            let editor =
                widgets::create_widget(&req, &self.store, &crate::theme::Theme::default());
            if let Some(widget) = editor {
                self.sub_editor = Some(widget);
                self.mode = HubMode::EditingItem;
                self.dirty = true;
            }
        }
    }
}

impl Widget for HubComposite {
    fn render(&self, area: Rect) -> RenderTree {
        if self.mode == HubMode::EditingItem {
            if let Some(ref editor) = self.sub_editor {
                return editor.render(area);
            }
        }

        let box_w = ((area.width as f32 * 0.95) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.95) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();

        let cat_items: Vec<ListItem> = self
            .categories
            .iter()
            .enumerate()
            .map(|(i, cat)| {
                let label = if i == self.cat_idx {
                    format!("> {}", cat.label)
                } else {
                    format!("  {}", cat.label)
                };
                ListItem { label, meta: None }
            })
            .collect();
        let left_w = box_w * 30 / 100;
        children.push(RenderNode::List {
            rect: Rect::new(1, 0, left_w, box_h - 2),
            items: cat_items,
            selected: Some(self.cat_idx),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        let right_x = left_w + 2;
        let right_w = box_w - right_x - 1;
        let visible = self.visible_items();
        let item_lines: Vec<ListItem> = visible
            .iter()
            .enumerate()
            .map(|(_i, item)| {
                let val = self.store.get_string(&item.id);
                let display_val = if item.display == "set/not set" {
                    if val.is_empty() {
                        "not set".to_string()
                    } else {
                        "set".to_string()
                    }
                } else if val.is_empty() {
                    "(none)".to_string()
                } else {
                    val.clone()
                };
                let label = format!(" {}: {}", item.label, display_val);
                ListItem { label, meta: None }
            })
            .collect();
        children.push(RenderNode::List {
            rect: Rect::new(right_x, 0, right_w, box_h - 2),
            items: item_lines,
            selected: Some(self.item_idx),
            highlight_style: TextStyle::selected(), role: None, label: None, description: None,
        });

        let footer_text = if self.actions.is_empty() {
            "Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit".to_string()
        } else {
            let acts = self
                .actions
                .iter()
                .enumerate()
                .map(|(i, a)| format!("F{}:{}", i + 1, a))
                .collect::<Vec<_>>()
                .join("  ");
            format!(
                "{}   Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit",
                acts
            )
        };
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: footer_text,
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
        match &self.mode {
            HubMode::EditingItem => {
                if let Some(editor) = &mut self.sub_editor {
                    match editor.handle_event(event) {
                        EventResult::Response(resp) => {
                            if !resp.cancelled {
                                let visible = self.visible_items();
                                if let Some(item) = visible.get(self.item_idx) {
                                    if let Some(ref value) = resp.result {
                                        self.store.set(item.id.clone(), value.clone());
                                    }
                                }
                            }
                            self.sub_editor = None;
                            self.mode = HubMode::Browsing;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        other => other,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            HubMode::ConfirmQuit => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            let mut map = serde_json::Map::new();
                            for (key, val) in &self.store.data {
                                map.insert(key.clone(), val.clone());
                            }
                            EventResult::Response(WidgetResponse {
                                result: Some(Value::Object(map)),
                                cancelled: false,
                                error: None,
                            })
                        }
                        _ => {
                            self.mode = HubMode::Browsing;
                            self.dirty = true;
                            EventResult::Handled
                        }
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            HubMode::Browsing => match event {
                Event::Key(code, _mods) => match code {
                    KeyCode::Esc => {
                        self.mode = HubMode::ConfirmQuit;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Up | KeyCode::Char('k') => {
                        self.item_idx = self.item_idx.saturating_sub(1);
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Down | KeyCode::Char('j') => {
                        let vis = self.visible_items();
                        if self.item_idx + 1 < vis.len() {
                            self.item_idx += 1;
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Left | KeyCode::Char('h') => {
                        self.cat_idx = self.cat_idx.saturating_sub(1);
                        self.item_idx = 0;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Right | KeyCode::Char('l') | KeyCode::Tab => {
                        if self.cat_idx + 1 < self.categories.len() {
                            self.cat_idx += 1;
                            self.item_idx = 0;
                        }
                        self.dirty = true;
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        self.start_edit_item();
                        EventResult::Handled
                    }
                    KeyCode::F(f) if *f >= 1 && (*f as usize) <= self.actions.len() => {
                        let action = &self.actions[*f as usize - 1];
                        if action == "Proceed" {
                            let mut map = serde_json::Map::new();
                            for (key, val) in &self.store.data {
                                map.insert(key.clone(), val.clone());
                            }
                            map.insert(
                                "_action".to_string(),
                                Value::String("Proceed".to_string()),
                            );
                            return EventResult::Response(WidgetResponse {
                                result: Some(Value::Object(map)),
                                cancelled: false,
                                error: None,
                            });
                        } else {
                            return EventResult::Response(WidgetResponse {
                                result: Some(Value::String(action.clone())),
                                cancelled: false,
                                error: None,
                            });
                        }
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
        }
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}