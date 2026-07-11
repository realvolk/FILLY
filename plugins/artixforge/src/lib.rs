use filly_core::event::{Event, KeyCode};
use filly_core::plugin::PluginRegistry;
use filly_core::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use filly_core::store::Store;
use filly_core::theme::Theme;
use filly_core::widget::{EventResult, Widget};
use filly_protocol::{WidgetRequest, WidgetResponse};
use serde_json::Value;
use std::collections::HashMap;

fn title_node(text: &str, box_w: u16) -> RenderNode {
    RenderNode::Text {
        rect: Rect::new(1, 0, box_w.saturating_sub(2), 1),
        content: text.to_string(),
        alignment: Alignment::Center,
        style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        role: None, label: None, description: None,
    }
}

fn footer_node(text: &str, box_w: u16, box_h: u16) -> RenderNode {
    RenderNode::Text {
        rect: Rect::new(1, box_h.saturating_sub(1), box_w.saturating_sub(2), 1),
        content: text.to_string(),
        alignment: Alignment::Center,
        style: TextStyle::muted(),
        role: None, label: None, description: None,
    }
}

fn centered_box(area: Rect, w_pct: f32, h_pct: f32) -> (Rect, u16, u16) {
    let box_w = ((area.width as f32 * w_pct) as u16).min(area.width.saturating_sub(2));
    let box_h = ((area.height as f32 * h_pct) as u16).min(area.height.saturating_sub(2));
    let box_x = (area.width.saturating_sub(box_w)) / 2;
    let box_y = (area.height.saturating_sub(box_h)) / 2;
    (Rect::new(box_x, box_y, box_w, box_h), box_w, box_h)
}

fn list_node(items: Vec<ListItem>, selected: usize, rect: Rect) -> RenderNode {
    RenderNode::List { rect, items, selected: Some(selected), highlight_style: TextStyle::selected(), role: None, label: None, description: None }
}

fn text_node(rect: Rect, content: String, style: TextStyle) -> RenderNode {
    RenderNode::Text { rect, content, alignment: Alignment::Left, style, role: None, label: None, description: None }
}

fn container_node(rect: Rect, children: Vec<RenderNode>) -> RenderTree {
    RenderTree::Container { rect, background: None, border: BorderStyle::Single, padding: EdgeInsets::zero(), children, role: None, label: None, description: None }
}

struct AnvilWidget {
    title: String,
    categories: Vec<(String, Vec<(String, String)>)>,
    cat_idx: usize,
    action_idx: usize,
    mode: AnvilMode,
    dirty: bool,
}

#[derive(PartialEq)]
enum AnvilMode {
    Browsing,
    ConfirmAction(String),
}

impl Widget for AnvilWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.85, 0.90);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        if self.categories.is_empty() {
            children.push(text_node(Rect::new(1, 1, box_w - 2, 1), "No actions available.".into(), TextStyle::normal()));
        } else {
            let left_w = box_w * 30 / 100;
            let right_x = left_w + 2;
            let right_w = box_w - right_x - 1;

            let cat_items: Vec<ListItem> = self.categories.iter().enumerate().map(|(i, (name, _))| {
                let label = if i == self.cat_idx { format!("> {}", name) } else { format!("  {}", name) };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(cat_items, self.cat_idx, Rect::new(1, 1, left_w, box_h - 3)));

            let actions = &self.categories[self.cat_idx].1;
            let action_items: Vec<ListItem> = actions.iter().enumerate().map(|(i, (_, desc))| {
                let label = if i == self.action_idx && self.mode == AnvilMode::Browsing {
                    format!("> {}", desc)
                } else {
                    format!("  {}", desc)
                };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(action_items, self.action_idx, Rect::new(right_x, 1, right_w, box_h - 3)));
        }

        match &self.mode {
            AnvilMode::Browsing => {
                children.push(footer_node("Up/Down:actions  Left/Right:categories  Enter:execute  Esc:cancel", box_w, box_h));
            }
            AnvilMode::ConfirmAction(key) => {
                children.push(text_node(Rect::new(1, box_h - 3, box_w - 2, 1), format!("Execute '{}'?", key), TextStyle::accent()));
                children.push(text_node(Rect::new(1, box_h - 2, box_w - 2, 1), "[Y]es  [N]o".into(), TextStyle::accent()));
            }
        }

        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            AnvilMode::ConfirmAction(key) => {
                let key = key.clone();
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            EventResult::Response(WidgetResponse { result: Some(Value::String(key)), cancelled: false, error: None })
                        }
                        _ => { self.mode = AnvilMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            AnvilMode::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                    KeyCode::Up | KeyCode::Char('k') => { self.action_idx = self.action_idx.saturating_sub(1); self.dirty = true; EventResult::Handled }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if !self.categories.is_empty() {
                            let actions = &self.categories[self.cat_idx].1;
                            if self.action_idx + 1 < actions.len() { self.action_idx += 1; self.dirty = true; }
                        }
                        EventResult::Handled
                    }
                    KeyCode::Left | KeyCode::Char('h') => { self.cat_idx = self.cat_idx.saturating_sub(1); self.action_idx = 0; self.dirty = true; EventResult::Handled }
                    KeyCode::Right | KeyCode::Char('l') | KeyCode::Tab => {
                        if self.cat_idx + 1 < self.categories.len() { self.cat_idx += 1; self.action_idx = 0; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        if !self.categories.is_empty() {
                            let actions = &self.categories[self.cat_idx].1;
                            if self.action_idx < actions.len() {
                                self.mode = AnvilMode::ConfirmAction(actions[self.action_idx].0.clone());
                                self.dirty = true;
                            }
                        }
                        EventResult::Handled
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct InstallHubWidget {
    title: String,
    categories: Value,
    actions: Vec<String>,
    dirty: bool,
}

impl Widget for InstallHubWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.95, 0.95);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        children.push(text_node(Rect::new(1, 2, box_w - 2, 1), "Installation hub — delegates to built-in hub widget".into(), TextStyle::muted()));
        children.push(footer_node("Enter:open hub  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Enter => EventResult::Response(WidgetResponse { result: Some(Value::String("open_hub".into())), cancelled: false, error: None }),
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct PowerUserWidget {
    title: String,
    categories: Vec<(String, Vec<(String, String)>)>,
    cat_idx: usize,
    item_idx: usize,
    values: HashMap<String, String>,
    mode: PowerUserMode,
    dirty: bool,
}

#[derive(PartialEq)]
enum PowerUserMode {
    Browsing,
    ConfirmBuild,
}

impl Widget for PowerUserWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.85, 0.90);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        if self.categories.is_empty() {
            children.push(text_node(Rect::new(1, 1, box_w - 2, 1), "No categories configured.".into(), TextStyle::normal()));
        } else {
            let left_w = box_w * 30 / 100;
            let right_x = left_w + 2;
            let right_w = box_w - right_x - 1;

            let cat_items: Vec<ListItem> = self.categories.iter().enumerate().map(|(i, (name, _))| {
                let label = if i == self.cat_idx { format!("> {}", name) } else { format!("  {}", name) };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(cat_items, self.cat_idx, Rect::new(1, 1, left_w, box_h - 3)));

            let items = &self.categories[self.cat_idx].1;
            let item_list: Vec<ListItem> = items.iter().enumerate().map(|(i, (id, val))| {
                let label = if i == self.item_idx && self.mode == PowerUserMode::Browsing {
                    format!("> {}: {}", id, val)
                } else {
                    format!("  {}: {}", id, val)
                };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(item_list, self.item_idx, Rect::new(right_x, 1, right_w, box_h - 3)));
        }

        match &self.mode {
            PowerUserMode::Browsing => {
                children.push(footer_node("Up/Down:items  Left/Right:categories  Enter:edit  F1:Build  Esc:cancel", box_w, box_h));
            }
            PowerUserMode::ConfirmBuild => {
                children.push(text_node(Rect::new(1, box_h - 3, box_w - 2, 1), "Begin source build with these settings?".into(), TextStyle::accent()));
                children.push(text_node(Rect::new(1, box_h - 2, box_w - 2, 1), "[Y]es  [N]o".into(), TextStyle::accent()));
            }
        }

        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            PowerUserMode::ConfirmBuild => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            let mut map = serde_json::Map::new();
                            for (k, v) in &self.values { map.insert(k.clone(), Value::String(v.clone())); }
                            EventResult::Response(WidgetResponse { result: Some(Value::Object(map)), cancelled: false, error: None })
                        }
                        _ => { self.mode = PowerUserMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            PowerUserMode::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                    KeyCode::Up | KeyCode::Char('k') => { self.item_idx = self.item_idx.saturating_sub(1); self.dirty = true; EventResult::Handled }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if !self.categories.is_empty() {
                            let items = &self.categories[self.cat_idx].1;
                            if self.item_idx + 1 < items.len() { self.item_idx += 1; self.dirty = true; }
                        }
                        EventResult::Handled
                    }
                    KeyCode::Left | KeyCode::Char('h') => { self.cat_idx = self.cat_idx.saturating_sub(1); self.item_idx = 0; self.dirty = true; EventResult::Handled }
                    KeyCode::Right | KeyCode::Char('l') | KeyCode::Tab => {
                        if self.cat_idx + 1 < self.categories.len() { self.cat_idx += 1; self.item_idx = 0; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::F(1) => { self.mode = PowerUserMode::ConfirmBuild; self.dirty = true; EventResult::Handled }
                    KeyCode::Enter => {
                        if !self.categories.is_empty() {
                            let items = &self.categories[self.cat_idx].1;
                            if self.item_idx < items.len() {
                                let (id, _) = &items[self.item_idx];
                                EventResult::Response(WidgetResponse { result: Some(Value::String(id.clone())), cancelled: false, error: None })
                            } else { EventResult::Handled }
                        } else { EventResult::Handled }
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct RecoveryWidget {
    title: String,
    status_items: Vec<(String, String, String)>,
    repairs: Vec<(String, String)>,
    selected: usize,
    mode: RecoveryMode,
    dirty: bool,
}

#[derive(PartialEq)]
enum RecoveryMode {
    Browsing,
    ConfirmRepair(String),
}

impl Widget for RecoveryWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.85, 0.85);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        let items: Vec<ListItem> = self.status_items.iter().enumerate().map(|(i, (_, label, status))| {
            let icon = match status.as_str() { "ok" => "OK", "warn" => "~ ", "error" => "!!", _ => "  " };
            let label = if i == self.selected && self.mode == RecoveryMode::Browsing {
                format!("> [{}] {}", icon, label)
            } else {
                format!("  [{}] {}", icon, label)
            };
            ListItem { label, meta: None }
        }).collect();
        children.push(list_node(items, self.selected, Rect::new(1, 1, box_w - 2, box_h - 4)));

        let repair_text: Vec<String> = self.repairs.iter().enumerate()
            .map(|(i, (_, desc))| format!("F{}:{}", i + 1, desc)).collect();
        let footer = format!("{}   Up/Down:move  Esc:cancel", repair_text.join("  "));

        match &self.mode {
            RecoveryMode::Browsing => {
                children.push(footer_node(&footer, box_w, box_h));
            }
            RecoveryMode::ConfirmRepair(key) => {
                children.push(text_node(Rect::new(1, box_h - 3, box_w - 2, 1), format!("Run repair '{}'?", key), TextStyle::accent()));
                children.push(text_node(Rect::new(1, box_h - 2, box_w - 2, 1), "[Y]es  [N]o".into(), TextStyle::accent()));
            }
        }

        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            RecoveryMode::ConfirmRepair(key) => {
                let key = key.clone();
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            EventResult::Response(WidgetResponse { result: Some(Value::String(key)), cancelled: false, error: None })
                        }
                        _ => { self.mode = RecoveryMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            RecoveryMode::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                    KeyCode::Up | KeyCode::Char('k') => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if self.selected + 1 < self.status_items.len() { self.selected += 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::F(f) if *f >= 1 && (*f as usize) <= self.repairs.len() => {
                        let (key, _) = &self.repairs[*f as usize - 1];
                        self.mode = RecoveryMode::ConfirmRepair(key.clone());
                        self.dirty = true;
                        EventResult::Handled
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct IsoWidget {
    title: String,
    categories: Vec<(String, Vec<(String, String)>)>,
    cat_idx: usize,
    item_idx: usize,
    values: HashMap<String, String>,
    mode: IsoMode,
    dirty: bool,
}

#[derive(PartialEq)]
enum IsoMode {
    Browsing,
    ConfirmBuild,
}

impl Widget for IsoWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.85, 0.90);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        if self.categories.is_empty() {
            children.push(text_node(Rect::new(1, 1, box_w - 2, 1), "No categories configured.".into(), TextStyle::normal()));
        } else {
            let left_w = box_w * 35 / 100;
            let right_x = left_w + 2;
            let right_w = box_w - right_x - 1;

            let cat_items: Vec<ListItem> = self.categories.iter().enumerate().map(|(i, (name, _))| {
                let label = if i == self.cat_idx { format!("> {}", name) } else { format!("  {}", name) };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(cat_items, self.cat_idx, Rect::new(1, 1, left_w, box_h - 3)));

            let items = &self.categories[self.cat_idx].1;
            let item_list: Vec<ListItem> = items.iter().enumerate().map(|(i, (id, val))| {
                let label = if i == self.item_idx && self.mode == IsoMode::Browsing {
                    format!("> {}: {}", id, val)
                } else {
                    format!("  {}: {}", id, val)
                };
                ListItem { label, meta: None }
            }).collect();
            children.push(list_node(item_list, self.item_idx, Rect::new(right_x, 1, right_w, box_h - 3)));
        }

        match &self.mode {
            IsoMode::Browsing => {
                children.push(footer_node("Up/Down:items  Left/Right:categories  Enter:edit  F1:Build  Esc:cancel", box_w, box_h));
            }
            IsoMode::ConfirmBuild => {
                children.push(text_node(Rect::new(1, box_h - 3, box_w - 2, 1), "Build ISO with these settings?".into(), TextStyle::accent()));
                children.push(text_node(Rect::new(1, box_h - 2, box_w - 2, 1), "[Y]es  [N]o".into(), TextStyle::accent()));
            }
        }

        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            IsoMode::ConfirmBuild => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            let mut map = serde_json::Map::new();
                            for (k, v) in &self.values { map.insert(k.clone(), Value::String(v.clone())); }
                            EventResult::Response(WidgetResponse { result: Some(Value::Object(map)), cancelled: false, error: None })
                        }
                        _ => { self.mode = IsoMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            IsoMode::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                    KeyCode::Up | KeyCode::Char('k') => { self.item_idx = self.item_idx.saturating_sub(1); self.dirty = true; EventResult::Handled }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if !self.categories.is_empty() {
                            let items = &self.categories[self.cat_idx].1;
                            if self.item_idx + 1 < items.len() { self.item_idx += 1; self.dirty = true; }
                        }
                        EventResult::Handled
                    }
                    KeyCode::Left | KeyCode::Char('h') => { self.cat_idx = self.cat_idx.saturating_sub(1); self.item_idx = 0; self.dirty = true; EventResult::Handled }
                    KeyCode::Right | KeyCode::Char('l') | KeyCode::Tab => {
                        if self.cat_idx + 1 < self.categories.len() { self.cat_idx += 1; self.item_idx = 0; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::F(1) => { self.mode = IsoMode::ConfirmBuild; self.dirty = true; EventResult::Handled }
                    KeyCode::Enter => {
                        if !self.categories.is_empty() {
                            let items = &self.categories[self.cat_idx].1;
                            if self.item_idx < items.len() {
                                let (id, _) = &items[self.item_idx];
                                EventResult::Response(WidgetResponse { result: Some(Value::String(id.clone())), cancelled: false, error: None })
                            } else { EventResult::Handled }
                        } else { EventResult::Handled }
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct MigrationInitWidget {
    title: String,
    current_init: String,
    target_inits: Vec<String>,
    source_idx: usize,
    target_idx: usize,
    field: usize,
    dirty: bool,
}

impl Widget for MigrationInitWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.35);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        let source_style = if self.field == 0 { TextStyle::selected() } else { TextStyle::normal() };
        let target_style = if self.field == 1 { TextStyle::selected() } else { TextStyle::normal() };

        children.push(text_node(Rect::new(1, 1, box_w - 2, 1), format!("Source: {}", self.current_init), source_style));
        children.push(text_node(Rect::new(1, 2, box_w - 2, 1), format!("Target: {}", self.target_inits[self.target_idx]), target_style));
        children.push(footer_node("Up/Down:field  Left/Right:change  Enter:confirm  Esc:cancel", box_w, box_h));

        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up | KeyCode::Char('k') => { self.field = self.field.saturating_sub(1); self.dirty = true; EventResult::Handled }
                KeyCode::Down | KeyCode::Char('j') => { if self.field < 1 { self.field += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Left | KeyCode::Char('h') => {
                    if self.field == 0 { }
                    else if self.target_idx > 0 { self.target_idx -= 1; self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Right | KeyCode::Char('l') => {
                    if self.field == 1 && self.target_idx + 1 < self.target_inits.len() { self.target_idx += 1; self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let result = serde_json::json!({
                        "source": self.current_init,
                        "target": self.target_inits[self.target_idx],
                    });
                    EventResult::Response(WidgetResponse { result: Some(result), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct MigrationDesktopWidget {
    title: String,
    current_de: String,
    des: Vec<String>,
    dm_choices: Vec<String>,
    x_choices: Vec<String>,
    audio_choices: Vec<String>,
    net_choices: Vec<String>,
    target_idx: usize,
    dm_idx: usize,
    x_idx: usize,
    audio_idx: usize,
    net_idx: usize,
    field: usize,
    dirty: bool,
}

impl Widget for MigrationDesktopWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.55, 0.55);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));

        let fields = [
            ("Source DE", self.current_de.as_str()),
            ("Target DE", self.des[self.target_idx].as_str()),
            ("Display Manager", self.dm_choices[self.dm_idx].as_str()),
            ("Display Stack", self.x_choices[self.x_idx].as_str()),
            ("Audio", self.audio_choices[self.audio_idx].as_str()),
            ("Network", self.net_choices[self.net_idx].as_str()),
        ];

        for (i, (label, val)) in fields.iter().enumerate() {
            let style = if i == self.field { TextStyle::selected() } else { TextStyle::normal() };
            children.push(text_node(Rect::new(1, 1 + i as u16, box_w - 2, 1), format!("{}: {}", label, val), style));
        }

        children.push(footer_node("Up/Down:field  Left/Right:change  Enter:confirm  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up | KeyCode::Char('k') => { self.field = self.field.saturating_sub(1); self.dirty = true; EventResult::Handled }
                KeyCode::Down | KeyCode::Char('j') => { if self.field < 5 { self.field += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Left | KeyCode::Char('h') => {
                    match self.field {
                        0 => {}
                        1 if self.target_idx > 0 => { self.target_idx -= 1; self.dirty = true; }
                        2 if self.dm_idx > 0 => { self.dm_idx -= 1; self.dirty = true; }
                        3 if self.x_idx > 0 => { self.x_idx -= 1; self.dirty = true; }
                        4 if self.audio_idx > 0 => { self.audio_idx -= 1; self.dirty = true; }
                        5 if self.net_idx > 0 => { self.net_idx -= 1; self.dirty = true; }
                        _ => {}
                    }
                    EventResult::Handled
                }
                KeyCode::Right | KeyCode::Char('l') => {
                    match self.field {
                        0 => {}
                        1 if self.target_idx + 1 < self.des.len() => { self.target_idx += 1; self.dirty = true; }
                        2 if self.dm_idx + 1 < self.dm_choices.len() => { self.dm_idx += 1; self.dirty = true; }
                        3 if self.x_idx + 1 < self.x_choices.len() => { self.x_idx += 1; self.dirty = true; }
                        4 if self.audio_idx + 1 < self.audio_choices.len() => { self.audio_idx += 1; self.dirty = true; }
                        5 if self.net_idx + 1 < self.net_choices.len() => { self.net_idx += 1; self.dirty = true; }
                        _ => {}
                    }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let result = serde_json::json!({
                        "source": self.current_de,
                        "target": self.des[self.target_idx],
                        "dm": self.dm_choices[self.dm_idx],
                        "x_stack": self.x_choices[self.x_idx],
                        "audio": self.audio_choices[self.audio_idx],
                        "network": self.net_choices[self.net_idx],
                    });
                    EventResult::Response(WidgetResponse { result: Some(result), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

#[no_mangle]
pub extern "C" fn register(registry: &mut PluginRegistry) {
    registry.register("anvil", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Anvil").to_string();
        let mut categories: Vec<(String, Vec<(String, String)>)> = Vec::new();
        if let Some(arr) = p.get("categories").and_then(|v| v.as_array()) {
            for cat_val in arr {
                let cat_name = cat_val.get("category").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let mut actions = Vec::new();
                if let Some(items) = cat_val.get("actions").and_then(|v| v.as_array()) {
                    for item in items {
                        let key = item.get("key").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        let desc = item.get("description").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        actions.push((key, desc));
                    }
                }
                if !actions.is_empty() { categories.push((cat_name, actions)); }
            }
        }
        Box::new(AnvilWidget { title, categories, cat_idx: 0, action_idx: 0, mode: AnvilMode::Browsing, dirty: true })
    });

    registry.register("install_hub", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Installation").to_string();
        let categories = p.get("categories").cloned().unwrap_or(Value::Null);
        let actions: Vec<String> = p.get("actions").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();
        Box::new(InstallHubWidget { title, categories, actions, dirty: true })
    });

    registry.register("poweruser", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Power User").to_string();
        let mut categories: Vec<(String, Vec<(String, String)>)> = Vec::new();
        if let Some(arr) = p.get("categories").and_then(|v| v.as_array()) {
            for cat_val in arr {
                let cat_name = cat_val.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let mut items = Vec::new();
                if let Some(items_arr) = cat_val.get("items").and_then(|v| v.as_array()) {
                    for item in items_arr {
                        let id = item.get("id").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        let value = item.get("value").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        items.push((id, value));
                    }
                }
                categories.push((cat_name, items));
            }
        }
        Box::new(PowerUserWidget { title, categories, cat_idx: 0, item_idx: 0, values: HashMap::new(), mode: PowerUserMode::Browsing, dirty: true })
    });

    registry.register("recovery", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Recovery").to_string();
        let mut status_items = Vec::new();
        if let Some(arr) = p.get("status").and_then(|v| v.as_array()) {
            for item in arr {
                let key = item.get("key").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let label = item.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let status = item.get("status").and_then(|v| v.as_str()).unwrap_or("ok").to_string();
                status_items.push((key, label, status));
            }
        }
        let mut repairs = Vec::new();
        if let Some(arr) = p.get("repairs").and_then(|v| v.as_array()) {
            for item in arr {
                let key = item.get("key").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let desc = item.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
                repairs.push((key, desc));
            }
        }
        Box::new(RecoveryWidget { title, status_items, repairs, selected: 0, mode: RecoveryMode::Browsing, dirty: true })
    });

    registry.register("iso", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("ISO Builder").to_string();
        let mut categories: Vec<(String, Vec<(String, String)>)> = Vec::new();
        if let Some(arr) = p.get("categories").and_then(|v| v.as_array()) {
            for cat_val in arr {
                let cat_name = cat_val.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let mut items = Vec::new();
                if let Some(items_arr) = cat_val.get("items").and_then(|v| v.as_array()) {
                    for item in items_arr {
                        let id = item.get("id").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        let value = item.get("value").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        items.push((id, value));
                    }
                }
                categories.push((cat_name, items));
            }
        }
        Box::new(IsoWidget { title, categories, cat_idx: 0, item_idx: 0, values: HashMap::new(), mode: IsoMode::Browsing, dirty: true })
    });

    registry.register("migration_init", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Init Migration").to_string();
        let current_init = p.get("current_init").and_then(|v| v.as_str()).unwrap_or("openrc").to_string();
        let target_inits = vec!["openrc".into(), "runit".into(), "dinit".into(), "s6".into()];
        Box::new(MigrationInitWidget { title, current_init, target_inits, source_idx: 0, target_idx: 0, field: 1, dirty: true })
    });

    registry.register("migration_desktop", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Desktop Migration").to_string();
        let current_de = p.get("current_de").and_then(|v| v.as_str()).unwrap_or("none").to_string();
        let des = vec!["kde".into(), "sonicde".into(), "xfce".into(), "lxqt".into(), "lxde".into(), "hyprland".into(), "sway".into(), "niri".into(), "i3wm".into(), "dwm".into(), "vxwm".into(), "icewm".into(), "mango".into(), "cinnamon".into(), "budgie".into(), "moksha".into(), "cosmic".into(), "none".into()];
        let dm_choices = vec!["current".into(), "sddm".into(), "lightdm".into(), "soniclogin".into(), "none".into()];
        let x_choices = vec!["current".into(), "xlibre".into(), "xorg".into(), "wayland".into()];
        let audio_choices = vec!["current".into(), "pipewire".into(), "pulseaudio".into(), "none".into()];
        let net_choices = vec!["current".into(), "networkmanager".into(), "dhcpcd+iwd".into(), "connman".into(), "none".into()];
        Box::new(MigrationDesktopWidget {
            title, current_de, des, dm_choices, x_choices, audio_choices, net_choices,
            target_idx: 0, dm_idx: 0, x_idx: 0, audio_idx: 0, net_idx: 0,
            field: 0, dirty: true,
        })
    });
}