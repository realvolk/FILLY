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
use std::collections::HashSet;

fn title_node(text: &str, box_w: u16) -> RenderNode {
    RenderNode::Text {
        rect: Rect::new(1, 0, box_w.saturating_sub(2), 1),
        content: text.to_string(),
        alignment: Alignment::Center,
        style: TextStyle { bold: true, fg: Some("green".into()), ..TextStyle::normal() },
        role: None, label: None, description: None,
    }
}

fn message_node(text: &str, box_w: u16) -> RenderNode {
    RenderNode::Text {
        rect: Rect::new(1, 1, box_w.saturating_sub(2), 2),
        content: text.to_string(),
        alignment: Alignment::Left,
        style: TextStyle::normal(),
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

struct Stage3PickerWidget {
    title: String,
    variants: Vec<String>,
    selected: usize,
    dirty: bool,
}

impl Widget for Stage3PickerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.7);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        let items: Vec<ListItem> = self.variants.iter().map(|v| ListItem { label: v.clone(), meta: None }).collect();
        children.push(list_node(items, self.selected, Rect::new(1, 1, box_w - 2, box_h - 3)));
        children.push(footer_node("Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.selected + 1 < self.variants.len() { self.selected += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Enter => {
                    let result = self.variants.get(self.selected).cloned();
                    EventResult::Response(WidgetResponse { result: result.map(Value::String), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct ProfilePickerWidget {
    title: String,
    profiles: Vec<String>,
    selected: usize,
    dirty: bool,
}

impl Widget for ProfilePickerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.7);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        let items: Vec<ListItem> = self.profiles.iter().map(|p| ListItem { label: p.clone(), meta: None }).collect();
        children.push(list_node(items, self.selected, Rect::new(1, 1, box_w - 2, box_h - 3)));
        children.push(footer_node("Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.selected + 1 < self.profiles.len() { self.selected += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Enter => {
                    let result = self.profiles.get(self.selected).cloned();
                    EventResult::Response(WidgetResponse { result: result.map(Value::String), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct KernelPickerWidget {
    title: String,
    kernels: Vec<String>,
    selected: usize,
    dirty: bool,
}

impl Widget for KernelPickerWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.7);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        let items: Vec<ListItem> = self.kernels.iter().map(|k| ListItem { label: k.clone(), meta: None }).collect();
        children.push(list_node(items, self.selected, Rect::new(1, 1, box_w - 2, box_h - 3)));
        children.push(footer_node("Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.selected + 1 < self.kernels.len() { self.selected += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Enter => {
                    let result = self.kernels.get(self.selected).cloned();
                    EventResult::Response(WidgetResponse { result: result.map(Value::String), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct UseFlagsWidget {
    title: String,
    flags: Vec<String>,
    selected: HashSet<usize>,
    cursor: usize,
    min: usize,
    max: usize,
    dirty: bool,
}

impl Widget for UseFlagsWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.7, 0.8);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        let items: Vec<ListItem> = self.flags.iter().enumerate().map(|(i, f)| {
            let mark = if self.selected.contains(&i) { "[x]" } else { "[ ]" };
            ListItem { label: format!(" {} {}", mark, f), meta: None }
        }).collect();
        children.push(list_node(items, self.cursor, Rect::new(1, 1, box_w - 2, box_h - 3)));
        children.push(footer_node("Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.cursor > 0 { self.cursor -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.cursor + 1 < self.flags.len() { self.cursor += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Char(' ') => {
                    if self.selected.contains(&self.cursor) {
                        if self.selected.len() > self.min { self.selected.remove(&self.cursor); self.dirty = true; }
                    } else if self.selected.len() < self.max { self.selected.insert(self.cursor); self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let result: Vec<String> = self.selected.iter().map(|&i| self.flags[i].clone()).collect();
                    EventResult::Response(WidgetResponse { result: Some(Value::String(result.join(" "))), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct CflagsWidget {
    title: String,
    cflags: String,
    cxxflags: String,
    makeopts: String,
    rustflags: String,
    field: usize,
    dirty: bool,
}

impl Widget for CflagsWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.5);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        let fields = [
            ("CFLAGS", &self.cflags),
            ("CXXFLAGS", &self.cxxflags),
            ("MAKEOPTS", &self.makeopts),
            ("RUSTFLAGS", &self.rustflags),
        ];
        for (i, (label, val)) in fields.iter().enumerate() {
            let style = if i == self.field { TextStyle::selected() } else { TextStyle::normal() };
            children.push(text_node(Rect::new(1, 1 + i as u16, box_w - 2, 1), format!("{}: {}", label, val), style));
        }
        children.push(footer_node("Up/Down:field  Enter:edit  F1:save  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.field > 0 { self.field -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.field < 3 { self.field += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::F(1) => {
                    let result = serde_json::json!({
                        "CFLAGS": self.cflags, "CXXFLAGS": self.cxxflags,
                        "MAKEOPTS": self.makeopts, "RUSTFLAGS": self.rustflags,
                    });
                    EventResult::Response(WidgetResponse { result: Some(result), cancelled: false, error: None })
                }
                KeyCode::Enter => {
                    let field_names = ["CFLAGS", "CXXFLAGS", "MAKEOPTS", "RUSTFLAGS"];
                    EventResult::Response(WidgetResponse {
                        result: Some(Value::String(field_names[self.field].into())),
                        cancelled: false, error: None,
                    })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct CheckListWidget {
    title: String,
    message: String,
    choices: Vec<String>,
    selected: HashSet<usize>,
    cursor: usize,
    min: usize,
    max: usize,
    dirty: bool,
}

impl Widget for CheckListWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.7, 0.8);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        if !self.message.is_empty() { children.push(message_node(&self.message, box_w)); }
        let list_y = if self.message.is_empty() { 1 } else { 3 };
        let items: Vec<ListItem> = self.choices.iter().enumerate().map(|(i, c)| {
            let mark = if self.selected.contains(&i) { "[x]" } else { "[ ]" };
            ListItem { label: format!(" {} {}", mark, c), meta: None }
        }).collect();
        children.push(list_node(items, self.cursor, Rect::new(1, list_y, box_w - 2, box_h - list_y - 2)));
        children.push(footer_node("Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.cursor > 0 { self.cursor -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.cursor + 1 < self.choices.len() { self.cursor += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Char(' ') => {
                    if self.selected.contains(&self.cursor) {
                        if self.selected.len() > self.min { self.selected.remove(&self.cursor); self.dirty = true; }
                    } else if self.selected.len() < self.max { self.selected.insert(self.cursor); self.dirty = true; }
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let result: Vec<String> = self.selected.iter().map(|&i| self.choices[i].clone()).collect();
                    EventResult::Response(WidgetResponse { result: Some(Value::String(result.join(" "))), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct VideoCardsWidget {
    title: String,
    message: String,
    choices: Vec<String>,
    selected: usize,
    dirty: bool,
}

impl Widget for VideoCardsWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.6);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        if !self.message.is_empty() { children.push(message_node(&self.message, box_w)); }
        let list_y = if self.message.is_empty() { 1 } else { 3 };
        let items: Vec<ListItem> = self.choices.iter().map(|c| ListItem { label: c.clone(), meta: None }).collect();
        children.push(list_node(items, self.selected, Rect::new(1, list_y, box_w - 2, box_h - list_y - 2)));
        children.push(footer_node("Up/Down:move  Enter:select  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Up => { if self.selected > 0 { self.selected -= 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Down => { if self.selected + 1 < self.choices.len() { self.selected += 1; self.dirty = true; } EventResult::Handled }
                KeyCode::Enter => {
                    let result = self.choices.get(self.selected).cloned();
                    EventResult::Response(WidgetResponse { result: result.map(Value::String), cancelled: false, error: None })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool { self.dirty }
    fn clear_dirty(&mut self) { self.dirty = false; }
}

struct BinhostWidget {
    title: String,
    message: String,
    current: String,
    input: String,
    dirty: bool,
}

impl Widget for BinhostWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let (box_rect, box_w, box_h) = centered_box(area, 0.6, 0.35);
        let mut children = Vec::new();
        children.push(title_node(&self.title, box_w));
        children.push(text_node(Rect::new(1, 1, box_w - 2, 1), self.message.clone(), TextStyle::normal()));
        children.push(input_node(Rect::new(1, 2, box_w - 2, 1), self.input.clone(), self.input.len(), "https://...".into(), false));
        children.push(footer_node("Type + Enter:confirm  Esc:cancel", box_w, box_h));
        container_node(box_rect, children)
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse { result: None, cancelled: true, error: None }),
                KeyCode::Enter => EventResult::Response(WidgetResponse { result: Some(Value::String(self.input.clone())), cancelled: false, error: None }),
                KeyCode::Char(c) => { self.input.push(*c); self.dirty = true; EventResult::Handled }
                KeyCode::Backspace => { self.input.pop(); self.dirty = true; EventResult::Handled }
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
    registry.register("stage3_picker", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Stage3").to_string();
        let variants: Vec<String> = p.get("choices").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();
        let default = p.get("default").and_then(|v| v.as_str().map(String::from));
        let selected = default.and_then(|d| variants.iter().position(|c| c == &d)).unwrap_or(0);
        Box::new(Stage3PickerWidget { title, variants, selected, dirty: true })
    });

    registry.register("profile_picker", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Profile").to_string();
        let profiles: Vec<String> = p.get("choices").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();
        let default = p.get("default").and_then(|v| v.as_str().map(String::from));
        let selected = default.and_then(|d| profiles.iter().position(|c| c == &d)).unwrap_or(0);
        Box::new(ProfilePickerWidget { title, profiles, selected, dirty: true })
    });

    registry.register("kernel_picker", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Kernel").to_string();
        let kernels: Vec<String> = p.get("choices").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();
        let default = p.get("default").and_then(|v| v.as_str().map(String::from));
        let selected = default.and_then(|d| kernels.iter().position(|c| c == &d)).unwrap_or(0);
        Box::new(KernelPickerWidget { title, kernels, selected, dirty: true })
    });

    registry.register("use_flags", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("USE Flags").to_string();
        let flags: Vec<String> = p.get("choices").and_then(|v| v.as_array())
            .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect()).unwrap_or_default();
        let min = p.get("min").and_then(|v| v.as_u64()).unwrap_or(0) as usize;
        let max = p.get("max").and_then(|v| v.as_u64()).unwrap_or(usize::MAX as u64) as usize;
        Box::new(UseFlagsWidget { title, flags, selected: HashSet::new(), cursor: 0, min, max, dirty: true })
    });

    registry.register("cflags", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("CFLAGS").to_string();
        let cflags = p.get("CFLAGS").and_then(|v| v.as_str()).unwrap_or("").to_string();
        let cxxflags = p.get("CXXFLAGS").and_then(|v| v.as_str()).unwrap_or("").to_string();
        let makeopts = p.get("MAKEOPTS").and_then(|v| v.as_str()).unwrap_or("").to_string();
        let rustflags = p.get("RUSTFLAGS").and_then(|v| v.as_str()).unwrap_or("").to_string();
        Box::new(CflagsWidget { title, cflags, cxxflags, makeopts, rustflags, field: 0, dirty: true })
    });

    let check_register = |name: &str, default_choices: Vec<&str>| {
        registry.register(name, move |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or(name).to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_else(|| default_choices.iter().map(|s| s.to_string()).collect());
            Box::new(CheckListWidget { title, message, choices, selected: HashSet::new(), cursor: 0, min: 0, max: usize::MAX, dirty: true })
        });
    };

    check_register("portage_licenses", vec!["@FREE", "@BINARY-REDISTRIBUTABLE", "@EULA", "GPL-2", "GPL-3", "LGPL-2.1", "BSD", "MIT", "Apache-2.0"]);
    check_register("portage_features", vec!["ccache", "buildpkg", "parallel-install", "keep-going", "userpriv", "quiet-build", "getbinpkg"]);
    check_register("portage_overlays", vec!["gentoo", "guru", "pentoo", "science"]);

    registry.register("portage_mirrors", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Mirrors").to_string();
        let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
        let current = p.get("default").and_then(|v| v.as_str()).unwrap_or("").to_string();
        Box::new(BinhostWidget { title, message, current: current.clone(), input: current, dirty: true })
    });

    registry.register("portage_accept_keywords", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("ACCEPT_KEYWORDS").to_string();
        let choices = vec!["amd64".into(), "~amd64".into()];
        let default = p.get("default").and_then(|v| v.as_str().map(String::from));
        let selected = default.and_then(|d| choices.iter().position(|c| c == &d)).unwrap_or(0);
        Box::new(VideoCardsWidget { title, message: String::new(), choices, selected, dirty: true })
    });

    registry.register("portage_video_cards", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("VIDEO_CARDS").to_string();
        let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
        let choices = vec!["intel".into(), "nvidia".into(), "amdgpu radeonsi".into(), "vesa".into()];
        let default = p.get("default").and_then(|v| v.as_str().map(String::from));
        let selected = default.and_then(|d| choices.iter().position(|c| c == &d)).unwrap_or(0);
        Box::new(VideoCardsWidget { title, message, choices, selected, dirty: true })
    });

    registry.register("portage_binhost", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Binhost").to_string();
        let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("Enter binhost URL:").to_string();
        let current = p.get("default").and_then(|v| v.as_str()).unwrap_or("").to_string();
        Box::new(BinhostWidget { title, message, current: current.clone(), input: current, dirty: true })
    });

    registry.register("portage_desktop_use", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Desktop USE").to_string();
        let suggestions = p.get("suggestions").and_then(|v| v.as_str()).unwrap_or("").to_string();
        Box::new(filly_core::widgets::msg::MsgWidget::new(title, suggestions))
    });

    registry.register("portage_telemetry", |req: &WidgetRequest, _store: &Store, _theme: &Theme| {
        let p = &req.params;
        let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Telemetry").to_string();
        let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("Mask telemetry?").to_string();
        let default = p.get("default").and_then(|v| v.as_bool());
        Box::new(filly_core::widgets::yesno::YesNoWidget::new(title, message, default))
    });

    check_register("portage_desktop_extras", vec!["Vulkan drivers", "Printer support", "Bluetooth", "Power management", "SSD TRIM", "NetworkManager applet", "Fonts", "Input method"]);
    check_register("portage_tool_groups", vec!["Virtualization", "Containers", "Development", "Gaming"]);
}