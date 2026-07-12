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

fn input_node(rect: Rect, text: String, cursor: usize, placeholder: String, masked: bool) -> RenderNode {
    RenderNode::Input { rect, text, cursor, placeholder, masked, role: None, label: None, description: None }
}

struct PasswordConfirmWidget {
    title: String,
    message: String,
    pass1: String,
    pass2: String,
    field: usize,
    dirty: bool,
}

impl PasswordConfirmWidget {
    fn new(title: String, message: String) -> Self {
        Self { title, message, pass1: String::new(), pass2: String::new(), field: 0, dirty: true }
    }
}

impl Widget for PasswordConfirmWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.50);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        if !self.message.is_empty() {
            children.push(text_node(Rect::new(1, 1, box_w - 2, 2), self.message.clone(), TextStyle::normal()));
        }
        let fields: [(&str, &String, &str); 2] = [
            ("Password:", &self.pass1, "Enter password"),
            ("Confirm:",  &self.pass2, "Confirm password"),
        ];
        for (i, (label, val, placeholder)) in fields.iter().enumerate() {
            let y = 4 + i as u16 * 2;
            let style = if i == self.field { TextStyle::selected() } else { TextStyle::normal() };
            children.push(text_node(Rect::new(1, y, box_w - 2, 1), label.to_string(), style));
            children.push(input_node(
                Rect::new(1, y + 1, box_w - 2, 1),
                val.to_string(), val.len(), placeholder.to_string(), true,
            ));
        }
        if !self.pass1.is_empty() && !self.pass2.is_empty() && self.pass1 != self.pass2 {
            children.push(text_node(
                Rect::new(1, box_h - 3, box_w - 2, 1),
                "Passwords do not match!".into(),
                TextStyle { fg: Some("red".into()), bold: true, ..TextStyle::normal() },
            ));
        }
        children.push(footer_node("Tab:next field  Enter:submit  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Tab => { self.field = (self.field + 1) % 2; self.dirty = true; EventResult::Handled }
                KeyCode::Enter => {
                    if !self.pass1.is_empty() && self.pass1 == self.pass2 {
                        let hash = std::process::Command::new("openssl")
                            .args(&["passwd", "-6", &self.pass1])
                            .output()
                            .ok()
                            .and_then(|o| String::from_utf8(o.stdout).ok())
                            .map(|s| s.trim().to_string())
                            .unwrap_or_default();
                        EventResult::Response(WidgetResponse {
                            result: Some(Value::String(hash)),
                            cancelled: false, error: None,
                        })
                    } else {
                        EventResult::Handled
                    }
                }
                KeyCode::Char(c) => {
                    if self.field == 0 { self.pass1.push(*c); } else { self.pass2.push(*c); }
                    self.dirty = true; EventResult::Handled
                }
                KeyCode::Backspace => {
                    if self.field == 0 { self.pass1.pop(); } else { self.pass2.pop(); }
                    self.dirty = true; EventResult::Handled
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

#[derive(Clone)]
struct UserEntry {
    name: String,
    pass: String,
    shell: String,
    groups: Vec<String>,
    sudo: bool,
}

struct UserManagerWidget {
    users: Vec<UserEntry>,
    mode: UserManagerMode,
    selected: usize,
    dirty: bool,
}

#[derive(PartialEq)]
enum UserManagerMode {
    Browsing,
    Adding(AddUserState),
    ConfirmDelete(usize),
}

#[derive(Clone, PartialEq)]
struct AddUserState {
    name: String,
    pass: String,
    confirm_pass: String,
    shell_idx: usize,
    groups: Vec<String>,
    sudo: bool,
    field: usize,
}

const SHELLS: &[&str] = &["/bin/bash", "/bin/zsh", "/usr/bin/fish"];

impl UserManagerWidget {
    fn new() -> Self {
        Self { users: Vec::new(), mode: UserManagerMode::Browsing, selected: 0, dirty: true }
    }

    fn user_list_items(&self) -> Vec<ListItem> {
        self.users.iter().map(|u| {
            let sudo_str = if u.sudo { "sudo" } else { "nosudo" };
            ListItem {
                label: format!("{} ({}) [{}]", u.name, u.shell.split('/').last().unwrap_or("bash"), sudo_str),
                meta: None,
            }
        }).collect()
    }
}

impl Widget for UserManagerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        match &self.mode {
            UserManagerMode::Browsing => {
                let (box_rect, box_w, box_h) = centered_box(area, 0.70, 0.80);
                let mut children = Vec::new();
                children.push(title_node("Manage Users", box_w));
                if self.users.is_empty() {
                    children.push(text_node(Rect::new(1, 2, box_w - 2, 1), "No users configured. Press A to add one.".into(), TextStyle::muted()));
                } else {
                    let items = self.user_list_items();
                    children.push(list_node(items, self.selected, Rect::new(1, 1, box_w - 2, box_h - 4)));
                }
                children.push(footer_node("A:add  D:delete  Enter:edit  Esc:done", box_w, box_h));
                container_node(box_rect, children)
            }
            UserManagerMode::Adding(state) => {
                let (box_rect, box_w, box_h) = centered_box(area, 0.60, 0.70);
                let mut children = Vec::new();
                children.push(title_node("Add / Edit User", box_w));
                let field_labels = [
                    ("Username:", state.name.clone()),
                    ("Password:", "•".repeat(state.pass.len())),
                    ("Confirm:",  "•".repeat(state.confirm_pass.len())),
                    ("Shell:",    SHELLS[state.shell_idx].to_string()),
                    ("Groups:",   state.groups.join(", ")),
                    ("Sudo:",     if state.sudo { "yes".to_string() } else { "no".to_string() }),
                ];
                for (i, (label, val)) in field_labels.iter().enumerate() {
                    let y = 1 + i as u16;
                    let style = if i == state.field { TextStyle::selected() } else { TextStyle::normal() };
                    let display = if val.is_empty() && i == 0 { "(required)".to_string() } else { val.clone() };
                    children.push(text_node(Rect::new(1, y, box_w - 2, 1), format!("{} {}", label, display), style));
                }
                if !state.pass.is_empty() && !state.confirm_pass.is_empty() && state.pass != state.confirm_pass {
                    children.push(text_node(
                        Rect::new(1, 8, box_w - 2, 1),
                        "Passwords do not match!".into(),
                        TextStyle { fg: Some("red".into()), bold: true, ..TextStyle::normal() },
                    ));
                }
                children.push(footer_node("Tab/Down:next  Up:prev  Left/Right:change  Enter:save  Esc:cancel", box_w, box_h));
                container_node(box_rect, children)
            }
            UserManagerMode::ConfirmDelete(idx) => {
                let (box_rect, box_w, box_h) = centered_box(area, 0.40, 0.25);
                let mut children = Vec::new();
                children.push(title_node("Delete User", box_w));
                children.push(text_node(Rect::new(1, 1, box_w - 2, 1), format!("Delete user '{}'?", self.users[*idx].name), TextStyle::normal()));
                children.push(footer_node("Y:yes  N:no", box_w, box_h));
                container_node(box_rect, children)
            }
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            UserManagerMode::ConfirmDelete(idx) => {
                let idx = *idx;
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            self.users.remove(idx);
                            if self.selected >= self.users.len() && self.selected > 0 {
                                self.selected -= 1;
                            }
                            self.mode = UserManagerMode::Browsing;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        _ => { self.mode = UserManagerMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            UserManagerMode::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => {
                        let result: Vec<Value> = self.users.iter().map(|u| serde_json::json!({
                            "name": u.name, "pass": u.pass, "shell": u.shell,
                            "groups": u.groups.join(","), "sudo": u.sudo,
                        })).collect();
                        EventResult::Response(WidgetResponse { result: Some(Value::Array(result)), cancelled: false, error: None })
                    }
                    KeyCode::Up | KeyCode::Char('k') => {
                        if self.selected > 0 { self.selected -= 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if self.selected + 1 < self.users.len() { self.selected += 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Char('a') | KeyCode::Char('A') => {
                        self.mode = UserManagerMode::Adding(AddUserState {
                            name: String::new(), pass: String::new(), confirm_pass: String::new(),
                            shell_idx: 0, groups: vec!["wheel".into(), "audio".into(), "video".into(), "storage".into()],
                            sudo: true, field: 0,
                        });
                        self.dirty = true; EventResult::Handled
                    }
                    KeyCode::Char('d') | KeyCode::Char('D') => {
                        if self.selected < self.users.len() {
                            self.mode = UserManagerMode::ConfirmDelete(self.selected);
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        if self.selected < self.users.len() {
                            let u = &self.users[self.selected];
                            self.mode = UserManagerMode::Adding(AddUserState {
                                name: u.name.clone(), pass: String::new(), confirm_pass: String::new(),
                                shell_idx: SHELLS.iter().position(|s| *s == u.shell).unwrap_or(0),
                                groups: u.groups.clone(), sudo: u.sudo, field: 0,
                            });
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    _ => EventResult::Unhandled,
                },
                _ => EventResult::Unhandled,
            },
            UserManagerMode::Adding(state) => {
                let mut state = state.clone();
                let result = match event {
                    Event::Key(code, _) => match code {
                        KeyCode::Esc => { self.dirty = true; EventResult::Handled }
                        KeyCode::Tab | KeyCode::Down => { state.field = (state.field + 1) % 6; self.dirty = true; EventResult::Handled }
                        KeyCode::Up => { state.field = (state.field + 5) % 6; self.dirty = true; EventResult::Handled }
                        KeyCode::Enter => {
                            if !state.name.is_empty() && !state.pass.is_empty() && state.pass == state.confirm_pass {
                                let hash = std::process::Command::new("openssl")
                                    .args(&["passwd", "-6", &state.pass])
                                    .output().ok()
                                    .and_then(|o| String::from_utf8(o.stdout).ok())
                                    .map(|s| s.trim().to_string())
                                    .unwrap_or_default();
                                let entry = UserEntry {
                                    name: state.name.clone(), pass: hash,
                                    shell: SHELLS[state.shell_idx].to_string(),
                                    groups: state.groups.clone(), sudo: state.sudo,
                                };
                                if self.selected < self.users.len() {
                                    self.users[self.selected] = entry;
                                } else {
                                    self.users.push(entry);
                                }
                                self.dirty = true;
                                self.mode = UserManagerMode::Browsing;
                                EventResult::Handled
                            } else { EventResult::Handled }
                        }
                        KeyCode::Left => {
                            match state.field {
                                3 => { state.shell_idx = (state.shell_idx + SHELLS.len() - 1) % SHELLS.len(); self.dirty = true; }
                                5 => { state.sudo = !state.sudo; self.dirty = true; }
                                _ => {}
                            }
                            EventResult::Handled
                        }
                        KeyCode::Right => {
                            match state.field {
                                3 => { state.shell_idx = (state.shell_idx + 1) % SHELLS.len(); self.dirty = true; }
                                5 => { state.sudo = !state.sudo; self.dirty = true; }
                                _ => {}
                            }
                            EventResult::Handled
                        }
                        KeyCode::Char(c) => {
                            match state.field {
                                0 => { state.name.push(*c); self.dirty = true; }
                                1 => { state.pass.push(*c); self.dirty = true; }
                                2 => { state.confirm_pass.push(*c); self.dirty = true; }
                                _ => {}
                            }
                            EventResult::Handled
                        }
                        KeyCode::Backspace => {
                            match state.field {
                                0 => { state.name.pop(); self.dirty = true; }
                                1 => { state.pass.pop(); self.dirty = true; }
                                2 => { state.confirm_pass.pop(); self.dirty = true; }
                                _ => {}
                            }
                            EventResult::Handled
                        }
                        _ => EventResult::Unhandled,
                    },
                    _ => EventResult::Unhandled,
                };
                if matches!(result, EventResult::Handled) {
                    self.mode = UserManagerMode::Adding(state);
                } else if self.mode != UserManagerMode::Browsing {
                    self.mode = UserManagerMode::Adding(state);
                }
                result
            }
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
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
            RecoveryMode::Browsing => { children.push(footer_node(&footer, box_w, box_h)); }
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

    registry.register("password_confirm", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Password").to_string();
        let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
        Box::new(PasswordConfirmWidget::new(title, message))
    });

    registry.register("user_manager", |_req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        Box::new(UserManagerWidget::new())
    });
}