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
use std::collections::{HashMap, HashSet};

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

fn hash_password(password: &str) -> String {
    if password.is_empty() {
        return String::new();
    }
    std::process::Command::new("openssl")
        .args(&["passwd", "-6", "--", password])
        .output()
        .ok()
        .and_then(|o| {
            let s = String::from_utf8_lossy(&o.stdout).trim().to_string();
            if s.is_empty() { None } else { Some(s) }
        })
        .unwrap_or_default()
}

#[derive(Debug, Clone)]
struct HubItem {
    id: String,
    label: String,
    value: String,
    widget: String,
    choices: Vec<String>,
    placeholder: String,
    visible_if: HashMap<String, String>,
    display: String,
    disk_picker: bool,
    message: String,
}

#[derive(Debug, Clone)]
struct HubCategory {
    id: String,
    label: String,
    summary_template: String,
    items: Vec<HubItem>,
}

#[derive(Debug, Clone, PartialEq)]
enum HubMode {
    Browsing,
    EditingItem(usize, EditState),
    ConfirmProceed,
    ConfirmQuit,
}

#[derive(Debug, Clone, PartialEq)]
enum EditState {
    Menu(String, Vec<String>, usize),
    Input(String, String, usize),
    Password(String, String),
    PasswordConfirm(String, String, String, bool),
    YesNo(String, String, bool),
    Filter(String, String, usize, Vec<String>),
    Multiselect(String, String, usize, HashSet<usize>, Vec<String>),
    KernelPicker(String, String),
    UserManager(UserManagerState),
    DiskPicker,
}

#[derive(Debug, Clone, PartialEq)]
enum UserManagerMode {
    List(usize),
    Form(usize, UserFormState),
    DeleteConfirm(usize),
}

#[derive(Debug, Clone, PartialEq)]
struct UserFormState {
    name: String,
    pass1: String,
    pass2: String,
    shell_idx: usize,
    groups: Vec<bool>,
    sudo: bool,
    field: usize,
}

#[derive(Debug, Clone, PartialEq)]
struct UserEntry {
    name: String,
    pass: String,
    shell: String,
    groups: Vec<String>,
    sudo: bool,
}

#[derive(Debug, Clone, PartialEq)]
struct UserManagerState {
    users: Vec<UserEntry>,
    mode: UserManagerMode,
}

const SHELLS: &[&str] = &["/bin/bash", "/bin/zsh", "/usr/bin/fish"];
const GROUP_OPTS: &[&str] = &["wheel", "audio", "video", "storage", "lp", "network", "optical", "scanner", "users"];

fn render_hub_background(
    title: &str,
    categories: &[HubCategory],
    actions: &[String],
    cat_idx: usize,
    item_idx: usize,
    values: &HashMap<String, String>,
    visible_items: &[HubItem],
    area: Rect,
) -> RenderTree {
    let (box_rect, box_w, box_h) = centered_box(area, 0.95, 0.95);
    let mut children = Vec::new();
    children.push(title_node(title, box_w));

    let left_w = box_w * 30 / 100;
    let right_x = left_w + 2;
    let right_w = box_w - right_x - 1;

    let cat_items: Vec<ListItem> = categories.iter().enumerate().map(|(i, cat)| {
        let label = if i == cat_idx { format!("> {}", cat.label) } else { format!("  {}", cat.label) };
        ListItem { label, meta: None }
    }).collect();
    children.push(list_node(cat_items, cat_idx, Rect::new(1, 1, left_w, box_h - 3)));

    let item_lines: Vec<ListItem> = visible_items.iter().enumerate().map(|(i, item)| {
        let val = values.get(&item.id).cloned().unwrap_or_default();
        let display_val = if item.display == "set/not set" {
            if val.is_empty() { "not set".to_string() } else { "set".to_string() }
        } else if val.is_empty() { "(none)".to_string() } else { val };
        let label = if i == item_idx { format!("> {}: {}", item.label, display_val) } else { format!("  {}: {}", item.label, display_val) };
        ListItem { label, meta: None }
    }).collect();
    children.push(list_node(item_lines, item_idx, Rect::new(right_x, 1, right_w, box_h - 3)));

    let action_text = actions.iter().enumerate().map(|(i, a)| format!("F{}:{}", i+1, a)).collect::<Vec<_>>().join("  ");
    let footer = format!("{}   Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit", action_text);
    children.push(footer_node(&footer, box_w, box_h));

    container_node(box_rect, children)
}

pub struct InstallHubWidget {
    title: String,
    categories: Vec<HubCategory>,
    actions: Vec<String>,
    cat_idx: usize,
    item_idx: usize,
    values: HashMap<String, String>,
    mode: HubMode,
    dirty: bool,
}

impl InstallHubWidget {
    pub fn from_params(params: &Value) -> Self {
        let title = params.get("title").and_then(|v| v.as_str()).unwrap_or("Installation").to_string();
        let mut categories = Vec::new();
        let mut values = HashMap::new();

        if let Some(arr) = params.get("categories").and_then(|v| v.as_array()) {
            for cat_val in arr {
                let id = cat_val.get("id").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let label = cat_val.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let summary_template = cat_val.get("summary_template").and_then(|v| v.as_str()).unwrap_or("").to_string();
                let mut items = Vec::new();
                if let Some(items_arr) = cat_val.get("items").and_then(|v| v.as_array()) {
                    for item_val in items_arr {
                        let item_id = item_val.get("id").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        if item_id.is_empty() { continue; }
                        let value = item_val.get("value").and_then(|v| v.as_str()).unwrap_or("").to_string();
                        let visible_if: HashMap<String, String> = item_val.get("visible_if").and_then(|v| v.as_object()).map(|o| {
                            o.iter().map(|(k, v)| (k.clone(), v.as_str().unwrap_or("").to_string())).collect()
                        }).unwrap_or_default();
                        values.insert(item_id.clone(), value.clone());
                        items.push(HubItem {
                            id: item_id,
                            label: item_val.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                            value,
                            widget: item_val.get("widget").and_then(|v| v.as_str()).unwrap_or("menu").to_string(),
                            choices: item_val.get("choices").and_then(|v| v.as_array()).map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default(),
                            placeholder: item_val.get("placeholder").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                            visible_if,
                            display: item_val.get("display").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                            disk_picker: item_val.get("disk_picker").and_then(|v| v.as_bool()).unwrap_or(false),
                            message: item_val.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                        });
                    }
                }
                categories.push(HubCategory { id, label, summary_template, items });
            }
        }

        let actions: Vec<String> = params.get("actions").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();

        Self { title, categories, actions, cat_idx: 0, item_idx: 0, values, mode: HubMode::Browsing, dirty: true }
    }

    fn visible_items(&self) -> Vec<HubItem> {
        if self.categories.is_empty() { return vec![]; }
        let cat = &self.categories[self.cat_idx];
        cat.items.iter().filter(|item| {
            if item.visible_if.is_empty() { return true; }
            item.visible_if.iter().all(|(k, v)| self.values.get(k).map(|s| s == v).unwrap_or(false))
        }).cloned().collect()
    }

    fn handle_edit_event(&mut self, event: &Event, state: EditState, item: Option<HubItem>) -> Option<Option<String>> {
        match state {
            EditState::Menu(title, choices, selected) => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Up => {
                            let new = if selected > 0 { selected - 1 } else { selected };
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Menu(title, choices, new));
                            None
                        }
                        KeyCode::Down => {
                            let new = if selected + 1 < choices.len() { selected + 1 } else { selected };
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Menu(title, choices, new));
                            None
                        }
                        KeyCode::Enter => Some(Some(choices[selected].clone())),
                        _ => None,
                    }
                } else { None }
            }
            EditState::Input(title, mut text, cursor) => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Enter => Some(Some(text)),
                        KeyCode::Char(c) => { text.push(*c); self.mode = HubMode::EditingItem(self.item_idx, EditState::Input(title, text, cursor+1)); None }
                        KeyCode::Backspace => { text.pop(); self.mode = HubMode::EditingItem(self.item_idx, EditState::Input(title, text, cursor.saturating_sub(1))); None }
                        _ => None,
                    }
                } else { None }
            }
            EditState::Password(title, mut text) => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Enter => Some(Some(text)),
                        KeyCode::Char(c) => { text.push(*c); self.mode = HubMode::EditingItem(self.item_idx, EditState::Password(title, text)); None }
                        KeyCode::Backspace => { text.pop(); self.mode = HubMode::EditingItem(self.item_idx, EditState::Password(title, text)); None }
                        _ => None,
                    }
                } else { None }
            }
            EditState::PasswordConfirm(title, mut pass1, mut pass2, mut field) => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Tab => {
                            field = !field;
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::PasswordConfirm(title, pass1, pass2, field));
                            None
                        }
                        KeyCode::Enter => {
                            if !pass1.is_empty() && pass1 == pass2 {
                                Some(Some(hash_password(&pass1)))
                            } else { None }
                        }
                        KeyCode::Char(c) => {
                            if field { pass2.push(*c); } else { pass1.push(*c); }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::PasswordConfirm(title, pass1, pass2, field));
                            None
                        }
                        KeyCode::Backspace => {
                            if field { pass2.pop(); } else { pass1.pop(); }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::PasswordConfirm(title, pass1, pass2, field));
                            None
                        }
                        _ => None,
                    }
                } else { None }
            }
            EditState::YesNo(title, message, mut yes) => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Left | KeyCode::Right | KeyCode::Char('h') | KeyCode::Char('l') | KeyCode::Tab => {
                            yes = !yes;
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::YesNo(title, message, yes));
                            None
                        }
                        KeyCode::Enter => Some(Some(if yes { "yes".into() } else { "no".into() })),
                        KeyCode::Char('y') => Some(Some("yes".into())),
                        KeyCode::Char('n') => Some(Some("no".into())),
                        _ => None,
                    }
                } else { None }
            }
            EditState::Filter(title, mut query, mut selected, choices) => {
                let filtered: Vec<String> = if query.is_empty() { choices.clone() } else {
                    let q = query.to_lowercase();
                    choices.iter().filter(|c| c.to_lowercase().contains(&q)).cloned().collect()
                };
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Enter => Some(Some(filtered.get(selected).cloned().unwrap_or_default())),
                        KeyCode::Up => {
                            if selected > 0 { selected -= 1; }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Filter(title, query, selected, choices));
                            None
                        }
                        KeyCode::Down => {
                            if selected + 1 < filtered.len() { selected += 1; }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Filter(title, query, selected, choices));
                            None
                        }
                        KeyCode::Char(c) => { query.push(*c); selected = 0; self.mode = HubMode::EditingItem(self.item_idx, EditState::Filter(title, query, selected, choices)); None }
                        KeyCode::Backspace => { query.pop(); selected = 0; self.mode = HubMode::EditingItem(self.item_idx, EditState::Filter(title, query, selected, choices)); None }
                        _ => None,
                    }
                } else { None }
            }
            EditState::Multiselect(title, mut query, mut cursor, mut selected_set, choices) => {
                let filtered: Vec<String> = if query.is_empty() { choices.clone() } else {
                    let q = query.to_lowercase();
                    choices.iter().filter(|c| c.to_lowercase().contains(&q)).cloned().collect()
                };
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Esc => Some(None),
                        KeyCode::Enter => {
                            let result: Vec<String> = selected_set.iter().map(|&i| choices[i].clone()).collect();
                            Some(Some(result.join(" ")))
                        }
                        KeyCode::Up => {
                            if cursor > 0 { cursor -= 1; }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Multiselect(title, query, cursor, selected_set, choices));
                            None
                        }
                        KeyCode::Down => {
                            if cursor + 1 < filtered.len() { cursor += 1; }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Multiselect(title, query, cursor, selected_set, choices));
                            None
                        }
                        KeyCode::Char(' ') => {
                            if let Some(fidx) = filtered.get(cursor) {
                                if let Some(oidx) = choices.iter().position(|c| c == fidx) {
                                    if selected_set.contains(&oidx) {
                                        selected_set.remove(&oidx);
                                    } else {
                                        selected_set.insert(oidx);
                                    }
                                }
                            }
                            self.mode = HubMode::EditingItem(self.item_idx, EditState::Multiselect(title, query, cursor, selected_set, choices));
                            None
                        }
                        KeyCode::Char(c) => { query.push(*c); cursor = 0; self.mode = HubMode::EditingItem(self.item_idx, EditState::Multiselect(title, query, cursor, selected_set, choices)); None }
                        KeyCode::Backspace => { query.pop(); cursor = 0; self.mode = HubMode::EditingItem(self.item_idx, EditState::Multiselect(title, query, cursor, selected_set, choices)); None }
                        _ => None,
                    }
                } else { None }
            }
            _ => {
                if let Event::Key(code, _) = event {
                    if *code == KeyCode::Esc { Some(None) } else { None }
                } else { None }
            }
        }
    }
}

impl Widget for InstallHubWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let visible = self.visible_items();
        if let HubMode::EditingItem(idx, ref edit_state) = &self.mode {
            let hub_bg = render_hub_background(&self.title, &self.categories, &self.actions, self.cat_idx, *idx, &self.values, &visible, area);
            let overlay = match edit_state {
                EditState::Menu(title, choices, selected) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.60);
                    let items: Vec<ListItem> = choices.iter().map(|c| ListItem { label: c.clone(), meta: None }).collect();
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    children.push(list_node(items, *selected, Rect::new(1, 1, box_w - 2, box_h - 3)));
                    children.push(footer_node("Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::Input(title, text, cursor) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.25);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    children.push(input_node(Rect::new(1, 1, box_w - 2, 1), text.clone(), *cursor, String::new(), false));
                    children.push(footer_node("Enter:confirm  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::Password(title, text) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.25);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    let masked = "*".repeat(text.len());
                    children.push(input_node(Rect::new(1, 1, box_w - 2, 1), masked.clone(), masked.len(), String::new(), false));
                    children.push(footer_node("Enter:confirm  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::PasswordConfirm(title, pass1, pass2, field) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.45);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    let masked1 = "*".repeat(pass1.len());
                    let masked2 = "*".repeat(pass2.len());
                    let style1 = if !*field { TextStyle::selected() } else { TextStyle::normal() };
                    let style2 = if *field { TextStyle::selected() } else { TextStyle::normal() };
                    children.push(text_node(Rect::new(1, 1, box_w - 2, 1), format!("Password: {}", masked1), style1));
                    children.push(text_node(Rect::new(1, 2, box_w - 2, 1), format!("Confirm:  {}", masked2), style2));
                    if !pass1.is_empty() && !pass2.is_empty() && pass1 != pass2 {
                        children.push(text_node(Rect::new(1, 3, box_w - 2, 1), "Passwords do not match!".into(), TextStyle { fg: Some("red".into()), bold: true, ..TextStyle::normal() }));
                    }
                    children.push(footer_node("Tab:next  Enter:submit  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::YesNo(title, message, yes) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.35);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    if !message.is_empty() {
                        children.push(text_node(Rect::new(1, 1, box_w - 2, 3), message.clone(), TextStyle::normal()));
                    }
                    let yes_style = if *yes { TextStyle::selected() } else { TextStyle::muted() };
                    let no_style = if !*yes { TextStyle::selected() } else { TextStyle::muted() };
                    children.push(text_node(Rect::new(1, 4, 10, 1), "[ Yes ]".into(), yes_style));
                    children.push(text_node(Rect::new(12, 4, 10, 1), "[ No ]".into(), no_style));
                    children.push(footer_node("Left/Right:choose  Enter:confirm  y/n:quick  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::Filter(title, query, selected, choices) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.60, 0.70);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    let display = if query.is_empty() { "Type to filter...".to_string() } else { format!("> {}", query) };
                    children.push(text_node(Rect::new(1, 1, box_w - 2, 1), display, TextStyle::accent()));
                    let items: Vec<ListItem> = choices.iter().map(|c| ListItem { label: c.clone(), meta: None }).collect();
                    children.push(list_node(items, *selected, Rect::new(1, 2, box_w - 2, box_h - 4)));
                    children.push(footer_node("Type:filter  Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                EditState::Multiselect(title, query, cursor, selected_set, choices) => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.65, 0.75);
                    let mut children = Vec::new();
                    children.push(title_node(title, box_w));
                    let display = if query.is_empty() { "Type to filter...".to_string() } else { format!("> {}", query) };
                    children.push(text_node(Rect::new(1, 1, box_w - 2, 1), display, TextStyle::accent()));
                    let items: Vec<ListItem> = choices.iter().enumerate().map(|(i, c)| {
                        let mark = if selected_set.contains(&i) { "[x]" } else { "[ ]" };
                        ListItem { label: format!(" {} {}", mark, c), meta: None }
                    }).collect();
                    children.push(list_node(items, *cursor, Rect::new(1, 2, box_w - 2, box_h - 4)));
                    children.push(footer_node("Type:filter  Space:toggle  Enter:confirm  Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
                _ => {
                    let (box_rect, box_w, box_h) = centered_box(area, 0.50, 0.30);
                    let mut children = Vec::new();
                    children.push(title_node("Editing...", box_w));
                    children.push(footer_node("Esc:cancel", box_w, box_h));
                    container_node(box_rect, children)
                }
            };
            RenderTree::Container {
                rect: area,
                background: None,
                border: BorderStyle::None,
                padding: EdgeInsets::zero(),
                children: vec![hub_bg, overlay],
                role: None, label: None, description: None,
            }
        } else {
            render_hub_background(&self.title, &self.categories, &self.actions, self.cat_idx, self.item_idx, &self.values, &visible, area)
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            HubMode::ConfirmQuit => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None })
                        }
                        _ => { self.mode = HubMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            HubMode::ConfirmProceed => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            let mut map = serde_json::Map::new();
                            for (k, v) in &self.values { map.insert(k.clone(), Value::String(v.clone())); }
                            EventResult::Response(WidgetResponse { result: Some(Value::Object(map)), cancelled: false, error: None })
                        }
                        _ => { self.mode = HubMode::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            HubMode::EditingItem(item_idx, ref edit_state) => {
                let item_idx = *item_idx;
                let visible = self.visible_items();
                let item = visible.get(item_idx).cloned();
                match self.handle_edit_event(event, edit_state.clone(), item) {
                    Some(result) => {
                        if let Some(ref new_val) = result {
                            if let Some(ref item) = visible.get(item_idx) {
                                self.values.insert(item.id.clone(), new_val.clone());
                            }
                        }
                        self.mode = HubMode::Browsing;
                        self.dirty = true;
                        EventResult::Handled
                    }
                    None => EventResult::Handled,
                }
            }
            HubMode::Browsing => {
                let visible = self.visible_items();
                match event {
                    Event::Key(code, _) => match code {
                        KeyCode::Esc => { self.mode = HubMode::ConfirmQuit; self.dirty = true; EventResult::Handled }
                        KeyCode::Up | KeyCode::Char('k') => { self.item_idx = self.item_idx.saturating_sub(1); self.dirty = true; EventResult::Handled }
                        KeyCode::Down | KeyCode::Char('j') => {
                            if self.item_idx + 1 < visible.len() { self.item_idx += 1; self.dirty = true; }
                            EventResult::Handled
                        }
                        KeyCode::Left | KeyCode::Char('h') => {
                            if self.cat_idx > 0 { self.cat_idx -= 1; } else { self.cat_idx = self.categories.len().saturating_sub(1); }
                            self.item_idx = 0; self.dirty = true; EventResult::Handled
                        }
                        KeyCode::Right | KeyCode::Char('l') | KeyCode::Tab => {
                            if self.cat_idx + 1 < self.categories.len() { self.cat_idx += 1; } else { self.cat_idx = 0; }
                            self.item_idx = 0; self.dirty = true; EventResult::Handled
                        }
                        KeyCode::Enter => {
                            if let Some(item) = visible.get(self.item_idx).cloned() {
                                let current = self.values.get(&item.id).cloned().unwrap_or_default();
                                let edit_state = match item.widget.as_str() {
                                    "menu" | "disk_picker" => {
                                        let choices = if item.disk_picker {
                                            get_disks()
                                        } else {
                                            item.choices.clone()
                                        };
                                        let selected = choices.iter().position(|c| c == &current).unwrap_or(0);
                                        EditState::Menu(item.label.clone(), choices, selected)
                                    }
                                    "input" => EditState::Input(item.label.clone(), current.clone(), current.len()),
                                    "password" => {
                                        if item.id == "ROOT_PASS" {
                                            EditState::PasswordConfirm("Root Password".into(), String::new(), String::new(), false)
                                        } else {
                                            EditState::Password(item.label.clone(), String::new())
                                        }
                                    }
                                    "password_confirm" => {
                                        EditState::PasswordConfirm(item.label.clone(), String::new(), String::new(), false)
                                    }
                                    "yesno" => {
                                        let def = current == "yes";
                                        let msg = item.message.clone();
                                        if msg.is_empty() { EditState::YesNo(item.label.clone(), format!("Enable {}?", item.label.to_lowercase()), def) }
                                        else { EditState::YesNo(item.label.clone(), msg, def) }
                                    }
                                    "filter" => {
                                        let choices = item.choices.clone();
                                        EditState::Filter(item.label.clone(), String::new(), 0, choices)
                                    }
                                    "multiselect" => {
                                        let choices = item.choices.clone();
                                        EditState::Multiselect(item.label.clone(), String::new(), 0, HashSet::new(), choices)
                                    }
                                    "kernel_picker" => EditState::KernelPicker(item.label.clone(), current),
                                    "user_manager" => {
                                        let users: Vec<UserEntry> = if !current.is_empty() && current != "0" {
                                            serde_json::from_str::<Vec<Value>>(&current)
                                                .map(|arr| {
                                                    arr.iter().map(|v| UserEntry {
                                                        name: v.get("name").and_then(|s| s.as_str()).unwrap_or("").to_string(),
                                                        pass: v.get("pass").and_then(|s| s.as_str()).unwrap_or("").to_string(),
                                                        shell: v.get("shell").and_then(|s| s.as_str()).unwrap_or("/bin/bash").to_string(),
                                                        groups: v.get("groups").and_then(|s| s.as_str()).unwrap_or("wheel,audio,video,storage").split(',').map(|s| s.trim().to_string()).collect(),
                                                        sudo: v.get("sudo").and_then(|s| s.as_bool()).unwrap_or(true),
                                                    }).collect()
                                                })
                                                .unwrap_or_default()
                                        } else { vec![] };
                                        EditState::UserManager(UserManagerState { users, mode: UserManagerMode::List(0) })
                                    }
                                    _ => return EventResult::Unhandled,
                                };
                                self.mode = HubMode::EditingItem(self.item_idx, edit_state);
                                self.dirty = true;
                            }
                            EventResult::Handled
                        }
                        KeyCode::F(f) if *f >= 1 && (*f as usize) <= self.actions.len() => {
                            let action = &self.actions[*f as usize - 1];
                            if action == "Proceed" {
                                if let Some(disk) = self.values.get("DISK") {
                                    let short = disk.split_whitespace().next().unwrap_or(disk).to_string();
                                    self.values.insert("DISK".to_string(), short);
                                }
                                self.mode = HubMode::ConfirmProceed;
                                self.dirty = true;
                                EventResult::Handled
                            } else {
                                EventResult::Response(WidgetResponse { result: Some(Value::String(action.clone())), cancelled: false, error: None })
                            }
                        }
                        _ => EventResult::Unhandled,
                    },
                    _ => EventResult::Unhandled,
                }
            }
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

fn get_disks() -> Vec<String> {
    std::process::Command::new("lsblk")
        .args(&["-dpno", "NAME,SIZE,MODEL", "-e", "7"])
        .output()
        .ok()
        .map(|o| {
            String::from_utf8_lossy(&o.stdout)
                .lines()
                .map(|l| {
                    let parts: Vec<&str> = l.splitn(3, ' ').collect();
                    format!("{} - {} ({})", parts.first().unwrap_or(&""), parts.get(1).unwrap_or(&""), parts.get(2).unwrap_or(&"Unknown"))
                })
                .collect()
        })
        .unwrap_or_default()
}

#[no_mangle]
pub extern "C" fn register(registry: &mut PluginRegistry) {
    registry.register("install_hub", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        Box::new(InstallHubWidget::from_params(&req.params))
    });

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
                } else { format!("  {}", desc) };
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
                } else { format!("  {}: {}", id, val) };
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
            } else { format!("  [{}] {}", icon, label) };
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
                } else { format!("  {}: {}", id, val) };
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
                        let hash = hash_password(&self.pass1);
                        EventResult::Response(WidgetResponse {
                            result: Some(Value::String(hash)),
                            cancelled: false, error: None,
                        })
                    } else { EventResult::Handled }
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
struct UserEntry2 {
    name: String,
    pass: String,
    shell: String,
    groups: Vec<String>,
    sudo: bool,
}

struct UserManagerWidget {
    users: Vec<UserEntry2>,
    mode: UserManagerMode2,
    selected: usize,
    dirty: bool,
}

#[derive(PartialEq)]
enum UserManagerMode2 {
    Browsing,
    Adding(AddUserState2),
    ConfirmDelete(usize),
}

#[derive(Clone, PartialEq)]
struct AddUserState2 {
    name: String,
    pass: String,
    confirm_pass: String,
    shell_idx: usize,
    groups: Vec<String>,
    sudo: bool,
    field: usize,
}

const SHELLS2: &[&str] = &["/bin/bash", "/bin/zsh", "/usr/bin/fish"];

impl UserManagerWidget {
    fn new() -> Self {
        Self { users: Vec::new(), mode: UserManagerMode2::Browsing, selected: 0, dirty: true }
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
            UserManagerMode2::Browsing => {
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
            UserManagerMode2::Adding(state) => {
                let (box_rect, box_w, box_h) = centered_box(area, 0.60, 0.70);
                let mut children = Vec::new();
                children.push(title_node("Add / Edit User", box_w));
                let field_labels = [
                    ("Username:", state.name.clone()),
                    ("Password:", "•".repeat(state.pass.len())),
                    ("Confirm:",  "•".repeat(state.confirm_pass.len())),
                    ("Shell:",    SHELLS2[state.shell_idx].to_string()),
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
            UserManagerMode2::ConfirmDelete(idx) => {
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
            UserManagerMode2::ConfirmDelete(idx) => {
                let idx = *idx;
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            self.users.remove(idx);
                            if self.selected >= self.users.len() && self.selected > 0 { self.selected -= 1; }
                            self.mode = UserManagerMode2::Browsing; self.dirty = true; EventResult::Handled
                        }
                        _ => { self.mode = UserManagerMode2::Browsing; self.dirty = true; EventResult::Handled }
                    }
                } else { EventResult::Unhandled }
            }
            UserManagerMode2::Browsing => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc => {
                        let result: Vec<Value> = self.users.iter().map(|u| serde_json::json!({
                            "name": u.name, "pass": u.pass, "shell": u.shell,
                            "groups": u.groups.join(","), "sudo": u.sudo,
                        })).collect();
                        EventResult::Response(WidgetResponse { result: Some(Value::Array(result)), cancelled: false, error: None })
                    }
                    KeyCode::Up | KeyCode::Char('k') => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                    KeyCode::Down | KeyCode::Char('j') => {
                        if self.selected + 1 < self.users.len() { self.selected += 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Char('a') | KeyCode::Char('A') => {
                        self.mode = UserManagerMode2::Adding(AddUserState2 {
                            name: String::new(), pass: String::new(), confirm_pass: String::new(),
                            shell_idx: 0, groups: vec!["wheel".into(), "audio".into(), "video".into(), "storage".into()],
                            sudo: true, field: 0,
                        });
                        self.dirty = true; EventResult::Handled
                    }
                    KeyCode::Char('d') | KeyCode::Char('D') => {
                        if self.selected < self.users.len() { self.mode = UserManagerMode2::ConfirmDelete(self.selected); self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Enter => {
                        if self.selected < self.users.len() {
                            let u = &self.users[self.selected];
                            self.mode = UserManagerMode2::Adding(AddUserState2 {
                                name: u.name.clone(), pass: String::new(), confirm_pass: String::new(),
                                shell_idx: SHELLS2.iter().position(|s| *s == u.shell).unwrap_or(0),
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
            UserManagerMode2::Adding(state) => {
                let mut state = state.clone();
                let result = match event {
                    Event::Key(code, _) => match code {
                        KeyCode::Esc => { self.mode = UserManagerMode2::Browsing; self.dirty = true; EventResult::Handled }
                        KeyCode::Tab | KeyCode::Down => { state.field = (state.field + 1) % 6; self.dirty = true; EventResult::Handled }
                        KeyCode::Up => { state.field = (state.field + 5) % 6; self.dirty = true; EventResult::Handled }
                        KeyCode::Enter => {
                            if !state.name.is_empty() && !state.pass.is_empty() && state.pass == state.confirm_pass {
                                let hash = hash_password(&state.pass);
                                let entry = UserEntry2 {
                                    name: state.name.clone(), pass: hash,
                                    shell: SHELLS2[state.shell_idx].to_string(),
                                    groups: state.groups.clone(), sudo: state.sudo,
                                };
                                if self.selected < self.users.len() { self.users[self.selected] = entry; }
                                else { self.users.push(entry); }
                                self.mode = UserManagerMode2::Browsing; self.dirty = true; EventResult::Handled
                            } else { EventResult::Handled }
                        }
                        KeyCode::Left => {
                            match state.field {
                                3 => { state.shell_idx = (state.shell_idx + SHELLS2.len() - 1) % SHELLS2.len(); self.dirty = true; }
                                5 => { state.sudo = !state.sudo; self.dirty = true; }
                                _ => {}
                            }
                            EventResult::Handled
                        }
                        KeyCode::Right => {
                            match state.field {
                                3 => { state.shell_idx = (state.shell_idx + 1) % SHELLS2.len(); self.dirty = true; }
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
                if matches!(result, EventResult::Handled) && self.mode != UserManagerMode2::Browsing {
                    self.mode = UserManagerMode2::Adding(state);
                }
                result
            }
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}