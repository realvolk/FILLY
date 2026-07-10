use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, ListItem, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;
use serde_json::Value;

#[derive(Debug, Clone)]
pub struct Partition {
    pub number: u32,
    pub start: String,
    pub end: String,
    pub size: String,
    pub ptype: String,
    pub flags: Vec<String>,
    pub fs_signature: Option<String>,
}

#[derive(Debug, Clone)]
pub struct FreeSpace {
    pub start: String,
    pub end: String,
    pub size: String,
}

#[derive(Debug, Clone, PartialEq)]
pub enum DiskMode {
    Main,
    TypePicker,
    FlagPicker,
    ResizeInput,
    NewPartition,
    Confirm(ConfirmDialog),
}

#[derive(Debug, Clone, PartialEq)]
pub struct ConfirmDialog {
    pub title: String,
    pub message: String,
    pub action: ConfirmAction,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ConfirmAction {
    DeletePartition,
    WriteChanges,
}

fn human_to_bytes(s: &str) -> u64 {
    let s = s.trim().to_uppercase();
    if s.is_empty() {
        return 0;
    }
    let (num_str, suffix) = s.split_at(
        s.find(|c: char| !c.is_ascii_digit() && c != '.' && c != '-')
            .unwrap_or(s.len()),
    );
    let num: f64 = num_str.parse().unwrap_or(0.0);
    match suffix {
        "B" => num as u64,
        "K" | "KB" | "KIB" => (num * 1024.0) as u64,
        "M" | "MB" | "MIB" => (num * 1024.0 * 1024.0) as u64,
        "G" | "GB" | "GIB" => (num * 1024.0 * 1024.0 * 1024.0) as u64,
        "T" | "TB" | "TIB" => (num * 1024.0 * 1024.0 * 1024.0 * 1024.0) as u64,
        _ => num as u64,
    }
}

fn bytes_to_human(bytes: u64) -> String {
    if bytes >= 1024 * 1024 * 1024 * 1024 {
        format!("{:.1}TiB", bytes as f64 / (1024.0 * 1024.0 * 1024.0 * 1024.0))
    } else if bytes >= 1024 * 1024 * 1024 {
        format!("{:.1}GiB", bytes as f64 / (1024.0 * 1024.0 * 1024.0))
    } else if bytes >= 1024 * 1024 {
        format!("{:.1}MiB", bytes as f64 / (1024.0 * 1024.0))
    } else if bytes >= 1024 {
        format!("{:.1}KiB", bytes as f64 / 1024.0)
    } else {
        format!("{}B", bytes)
    }
}

fn partition_type_choices() -> Vec<&'static str> {
    vec![
        "EFI System", "BIOS boot", "Linux filesystem", "Linux swap",
        "Linux LVM", "Linux LUKS", "Linux RAID", "Linux /boot",
        "Linux /home", "Linux /var", "Linux /tmp", "Windows data",
        "Windows recovery", "FreeBSD", "FreeBSD swap", "FreeBSD ZFS",
        "FreeBSD UFS", "macOS APFS", "macOS HFS+", "Solaris", "Custom",
    ]
}

fn flag_choices() -> Vec<&'static str> {
    vec!["boot", "esp", "bios_grub", "lvm", "raid"]
}

pub struct DiskWidget {
    pub title: String,
    pub disk: String,
    pub partitions: Vec<Partition>,
    pub free_space: Vec<FreeSpace>,
    pub selected: usize,
    pub mode: DiskMode,
    pub readonly: bool,
    pub type_selected: usize,
    pub flag_selected: usize,
    pub input: String,
    pub editing_idx: usize,
    dirty: bool,
}

impl DiskWidget {
    pub fn new(
        title: String,
        disk: String,
        partitions_json: Value,
        free_space_json: Option<Value>,
        readonly: Option<bool>,
    ) -> Self {
        let partitions = parse_partitions(&partitions_json);
        let free_space = parse_free_space(&free_space_json.unwrap_or(Value::Null));
        Self {
            title,
            disk,
            partitions,
            free_space,
            selected: 0,
            mode: DiskMode::Main,
            readonly: readonly.unwrap_or(false),
            type_selected: 0,
            flag_selected: 0,
            input: String::new(),
            editing_idx: 0,
            dirty: true,
        }
    }

    fn total_entries(&self) -> usize {
        self.partitions.len() + self.free_space.len()
    }
}

fn parse_partitions(json: &Value) -> Vec<Partition> {
    let mut parts = Vec::new();
    if let Some(arr) = json.as_array() {
        for v in arr {
            let flags: Vec<String> = v
                .get("flags")
                .and_then(|f| f.as_array())
                .map(|a| a.iter().filter_map(|s| s.as_str().map(String::from)).collect())
                .unwrap_or_default();
            parts.push(Partition {
                number: v.get("number").and_then(|n| n.as_u64()).unwrap_or(0) as u32,
                start: v.get("start").and_then(|s| s.as_str()).unwrap_or("0").to_string(),
                end: v.get("end").and_then(|s| s.as_str()).unwrap_or("0").to_string(),
                size: v.get("size").and_then(|s| s.as_str()).unwrap_or("0").to_string(),
                ptype: v.get("type").and_then(|s| s.as_str()).unwrap_or("").to_string(),
                flags,
                fs_signature: v.get("fs_signature").and_then(|s| s.as_str()).map(String::from),
            });
        }
    }
    parts.sort_by_key(|p| p.number);
    parts
}

fn parse_free_space(json: &Value) -> Vec<FreeSpace> {
    let mut free = Vec::new();
    if let Some(arr) = json.as_array() {
        for v in arr {
            let start = v.get("start").and_then(|s| s.as_str()).unwrap_or("0").to_string();
            let end = v.get("end").and_then(|s| s.as_str()).unwrap_or("0").to_string();
            let size = v.get("size").and_then(|s| s.as_str()).unwrap_or("0").to_string();
            free.push(FreeSpace { start, end, size });
        }
    }
    free
}

impl Widget for DiskWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = ((area.width as f32 * 0.95) as u16).min(area.width.saturating_sub(2));
        let box_h = ((area.height as f32 * 0.95) as u16).min(area.height.saturating_sub(2));
        let box_x = (area.width - box_w) / 2;
        let box_y = (area.height - box_h) / 2;

        let mut children = Vec::new();

        match &self.mode {
            DiskMode::Main => {
                children.push(RenderNode::Text {
                    rect: Rect::new(1, 1, box_w - 2, 1),
                    content: build_partition_bar(&self.partitions, &self.free_space, box_w - 2),
                    alignment: Alignment::Left,
                    style: TextStyle::normal(), role: None, label: None, description: None,
                });

                let entries = build_partition_list(&self.partitions, &self.free_space, self.selected);
                children.push(RenderNode::List {
                    rect: Rect::new(1, 2, box_w - 2, box_h - 5),
                    items: entries,
                    selected: Some(self.selected),
                    highlight_style: TextStyle::selected(), role: None, label: None, description: None,
                });

                let detail = build_detail(&self.partitions, &self.free_space, self.selected);
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 3, box_w - 2, 1),
                    content: detail,
                    alignment: Alignment::Left,
                    style: TextStyle::normal(), role: None, label: None, description: None,
                });

                let footer = if self.readonly {
                    "Up/Down:move  Q:quit  Esc:cancel".to_string()
                } else {
                    "Up/Down:move  N:new  D:delete  R:resize  T:type  F:flags  W:write  Q:quit  Esc:cancel".to_string()
                };
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: footer,
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                });
            }
            DiskMode::TypePicker => {
                let types = partition_type_choices();
                let current = &self.partitions[self.editing_idx].ptype;
                let items: Vec<ListItem> = types.iter().map(|t| {
                    let label = if *t == current.as_str() {
                        format!("> {}", t)
                    } else {
                        format!("  {}", t)
                    };
                    ListItem { label, meta: None }
                }).collect();
                children.push(RenderNode::List {
                    rect: Rect::new(1, 1, box_w - 2, box_h - 3),
                    items,
                    selected: Some(self.type_selected),
                    highlight_style: TextStyle::selected(), role: None, label: None, description: None,
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: "Up/Down:select  Enter:confirm  Esc:cancel".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                });
            }
            DiskMode::FlagPicker => {
                let flags = flag_choices();
                let current = &self.partitions[self.editing_idx].flags;
                let items: Vec<ListItem> = flags.iter().enumerate().map(|(_i, f)| {
                    let mark = if current.contains(&f.to_string()) { "[x]" } else { "[ ]" };
                    ListItem { label: format!(" {} {}", mark, f), meta: None }
                }).collect();
                children.push(RenderNode::List {
                    rect: Rect::new(1, 1, box_w - 2, box_h - 3),
                    items,
                    selected: Some(self.flag_selected),
                    highlight_style: TextStyle::selected(), role: None, label: None, description: None,
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: "Up/Down:move  Space:toggle  Enter:done  Esc:cancel".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                });
            }
            DiskMode::ResizeInput => {
                let p = &self.partitions[self.editing_idx];
                children.push(RenderNode::Text {
                    rect: Rect::new(1, 2, box_w - 2, 1),
                    content: format!("New size for partition {} (current: {}): {}", p.number, p.size, self.input),
                    alignment: Alignment::Left,
                    style: TextStyle::accent(), role: None, label: None, description: None,
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: "Enter:confirm  Esc:cancel".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                });
            }
            DiskMode::NewPartition => {
                let fs = &self.free_space[self.editing_idx];
                children.push(RenderNode::Text {
                    rect: Rect::new(1, 2, box_w - 2, 1),
                    content: format!("New partition size (free: {}): {}", fs.size, self.input),
                    alignment: Alignment::Left,
                    style: TextStyle::accent(), role: None, label: None, description: None,
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: "Enter:confirm  Esc:cancel".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::muted(), role: None, label: None, description: None,
                });
            }
            DiskMode::Confirm(dialog) => {
                children.push(RenderNode::Text {
                    rect: Rect::new(1, 1, box_w - 2, 1),
                    content: dialog.title.clone(),
                    alignment: Alignment::Center,
                    role: None, label: None, description: None, style: TextStyle { bold: true, fg: Some("yellow".into()), ..TextStyle::normal() },
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, 3, box_w - 2, box_h - 5),
                    content: dialog.message.clone(),
                    alignment: Alignment::Left,
                    style: TextStyle::normal(), role: None, label: None, description: None,
                });
                children.push(RenderNode::Text {
                    rect: Rect::new(1, box_h - 2, box_w - 2, 1),
                    content: "[Y]es  [N]o".into(),
                    alignment: Alignment::Center,
                    style: TextStyle::accent(), role: None, label: None, description: None,
                });
            }
        }

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::uniform(1), role: None, label: None, description: None,
            children,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match &self.mode {
            DiskMode::Confirm(dialog) => {
                let action = dialog.action.clone();
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Char('y') | KeyCode::Char('Y') => {
                            match action {
                                ConfirmAction::DeletePartition => {
                                    if self.selected < self.partitions.len() {
                                        let p = &self.partitions[self.selected];
                                        self.free_space.push(FreeSpace {
                                            start: p.start.clone(),
                                            end: p.end.clone(),
                                            size: p.size.clone(),
                                        });
                                        self.partitions.remove(self.selected);
                                        merge_adjacent_free_space(&mut self.free_space);
                                        self.dirty = true;
                                    }
                                }
                                ConfirmAction::WriteChanges => {
                                    let result = serde_json::json!({
                                        "partitions": self.partitions.iter().map(|p| serde_json::json!({
                                            "number": p.number,
                                            "start": p.start,
                                            "end": p.end,
                                            "size": p.size,
                                            "type": p.ptype,
                                            "flags": p.flags,
                                        })).collect::<Vec<_>>(),
                                        "free_space": self.free_space.iter().map(|fs| serde_json::json!({
                                            "start": fs.start,
                                            "end": fs.end,
                                            "size": fs.size,
                                        })).collect::<Vec<_>>(),
                                    });
                                    return EventResult::Response(WidgetResponse {
                                        result: Some(result),
                                        cancelled: false,
                                        error: None,
                                    });
                                }
                            }
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Char('n') | KeyCode::Char('N') | KeyCode::Esc => {
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        _ => EventResult::Unhandled,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            DiskMode::TypePicker => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Up => {
                            if self.type_selected > 0 {
                                self.type_selected -= 1;
                                self.dirty = true;
                            }
                            EventResult::Handled
                        }
                        KeyCode::Down => {
                            let types = partition_type_choices();
                            if self.type_selected + 1 < types.len() {
                                self.type_selected += 1;
                                self.dirty = true;
                            }
                            EventResult::Handled
                        }
                        KeyCode::Enter => {
                            let types = partition_type_choices();
                            self.partitions[self.editing_idx].ptype = types[self.type_selected].to_string();
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Esc => {
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        _ => EventResult::Unhandled,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            DiskMode::FlagPicker => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Up => {
                            if self.flag_selected > 0 {
                                self.flag_selected -= 1;
                                self.dirty = true;
                            }
                            EventResult::Handled
                        }
                        KeyCode::Down => {
                            let flags = flag_choices();
                            if self.flag_selected + 1 < flags.len() {
                                self.flag_selected += 1;
                                self.dirty = true;
                            }
                            EventResult::Handled
                        }
                        KeyCode::Char(' ') => {
                            let flag = flag_choices()[self.flag_selected].to_string();
                            let flags = &mut self.partitions[self.editing_idx].flags;
                            if flags.contains(&flag) {
                                flags.retain(|f| f != &flag);
                            } else {
                                flags.push(flag);
                            }
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Enter | KeyCode::Esc => {
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        _ => EventResult::Unhandled,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            DiskMode::ResizeInput => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Enter => {
                            if !self.input.is_empty() {
                                let idx = self.editing_idx;
                                apply_resize(&mut self.partitions, &mut self.free_space, idx, &self.input);
                                self.input.clear();
                            }
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Esc => {
                            self.input.clear();
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Backspace => { self.input.pop(); self.dirty = true; EventResult::Handled }
                        KeyCode::Char(c) => { self.input.push(*c); self.dirty = true; EventResult::Handled }
                        _ => EventResult::Unhandled,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            DiskMode::NewPartition => {
                if let Event::Key(code, _) = event {
                    match code {
                        KeyCode::Enter => {
                            if !self.input.is_empty() {
                                let idx = self.editing_idx;
                                create_partition_from_free(idx, &self.input, &mut self.partitions, &mut self.free_space);
                                self.input.clear();
                                self.selected = self.partitions.len().saturating_sub(1);
                            }
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Esc => {
                            self.input.clear();
                            self.mode = DiskMode::Main;
                            self.dirty = true;
                            EventResult::Handled
                        }
                        KeyCode::Backspace => { self.input.pop(); self.dirty = true; EventResult::Handled }
                        KeyCode::Char(c) => { self.input.push(*c); self.dirty = true; EventResult::Handled }
                        _ => EventResult::Unhandled,
                    }
                } else {
                    EventResult::Unhandled
                }
            }
            DiskMode::Main => match event {
                Event::Key(code, _) => match code {
                    KeyCode::Esc | KeyCode::Char('q') => EventResult::Response(WidgetResponse {
                        result: None, cancelled: true, error: None,
                    }),
                    KeyCode::Up => {
                        if self.selected > 0 { self.selected -= 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Down => {
                        if self.selected + 1 < self.total_entries() { self.selected += 1; self.dirty = true; }
                        EventResult::Handled
                    }
                    KeyCode::Char('n') if !self.readonly => {
                        if self.selected >= self.partitions.len() {
                            let fi = self.selected - self.partitions.len();
                            self.editing_idx = fi;
                            self.input.clear();
                            self.mode = DiskMode::NewPartition;
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Char('d') if !self.readonly => {
                        if self.selected < self.partitions.len() {
                            let p = &self.partitions[self.selected];
                            let msg = if let Some(ref sig) = p.fs_signature {
                                format!("Delete partition {} ({}, {} detected)?\n\nThis cannot be undone.", p.number, p.size, sig)
                            } else {
                                format!("Delete partition {} ({})?\n\nThis cannot be undone.", p.number, p.size)
                            };
                            self.mode = DiskMode::Confirm(ConfirmDialog {
                                title: "Delete Partition".into(),
                                message: msg,
                                action: ConfirmAction::DeletePartition,
                            });
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Char('t') if !self.readonly => {
                        if self.selected < self.partitions.len() {
                            self.editing_idx = self.selected;
                            self.type_selected = 0;
                            self.mode = DiskMode::TypePicker;
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Char('f') if !self.readonly => {
                        if self.selected < self.partitions.len() {
                            self.editing_idx = self.selected;
                            self.flag_selected = 0;
                            self.mode = DiskMode::FlagPicker;
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Char('r') if !self.readonly => {
                        if self.selected < self.partitions.len() {
                            self.editing_idx = self.selected;
                            self.input.clear();
                            self.mode = DiskMode::ResizeInput;
                            self.dirty = true;
                        }
                        EventResult::Handled
                    }
                    KeyCode::Char('w') if !self.readonly => {
                        let summary: String = self.partitions.iter()
                            .map(|p| format!("  {}  {}  {}", p.number, p.size, p.ptype))
                            .collect::<Vec<_>>().join("\n");
                        self.mode = DiskMode::Confirm(ConfirmDialog {
                            title: "Write Changes".into(),
                            message: format!("Apply to {}?\n\n{}", self.disk, summary),
                            action: ConfirmAction::WriteChanges,
                        });
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

fn build_partition_bar(parts: &[Partition], free: &[FreeSpace], width: u16) -> String {
    let tw = width.saturating_sub(2) as usize;
    if tw == 0 { return String::new(); }
    let mut max_end: u64 = 0;
    for p in parts { max_end = max_end.max(human_to_bytes(&p.end)); }
    for fs in free { max_end = max_end.max(human_to_bytes(&fs.end)); }
    if max_end == 0 { return String::new(); }
    let mut segs: Vec<(&str, u64, u64)> = Vec::new();
    for p in parts { segs.push((&p.ptype, human_to_bytes(&p.start), human_to_bytes(&p.end))); }
    for fs in free { segs.push(("Free", human_to_bytes(&fs.start), human_to_bytes(&fs.end))); }
    segs.sort_by_key(|s| s.1);
    let mut bar = String::with_capacity(tw);
    let mut cur: u64 = 0;
    for (label, s, e) in segs {
        if s > cur {
            let gap = ((s - cur) as f64 / max_end as f64 * tw as f64) as usize;
            if gap > 0 { bar.push_str(&" ".repeat(gap)); }
        }
        let w = ((e - s) as f64 / max_end as f64 * tw as f64) as usize;
        if w > 0 {
            let truncated: String = label.chars().take(w).collect();
            bar.push_str(&format!("{:^w$}", truncated, w = w));
        }
        cur = e;
    }
    bar
}

fn build_partition_list(parts: &[Partition], free: &[FreeSpace], sel: usize) -> Vec<ListItem> {
    let mut items = Vec::new();
    for (i, p) in parts.iter().enumerate() {
        let label = if i == sel {
            format!("> {:>3}  {:>8}  {:<22}", p.number, p.size, p.ptype)
        } else {
            format!("  {:>3}  {:>8}  {:<22}", p.number, p.size, p.ptype)
        };
        items.push(ListItem { label, meta: None });
    }
    for (i, fs) in free.iter().enumerate() {
        let idx = parts.len() + i;
        let label = if idx == sel {
            format!(">      {:>8}  Free space", fs.size)
        } else {
            format!("       {:>8}  Free space", fs.size)
        };
        items.push(ListItem { label, meta: None });
    }
    items
}

fn build_detail(parts: &[Partition], free: &[FreeSpace], idx: usize) -> String {
    if idx < parts.len() {
        let p = &parts[idx];
        let fs = p.fs_signature.as_deref().unwrap_or("none");
        let flag_str = if p.flags.is_empty() { "none".to_string() } else { p.flags.join(", ") };
        format!("Partition {}  Type: {}  Size: {}  FS: {}  Flags: {}", p.number, p.ptype, p.size, fs, flag_str)
    } else if !free.is_empty() {
        let fs = &free[idx - parts.len()];
        format!("Free space  Size: {}", fs.size)
    } else {
        String::new()
    }
}

fn merge_adjacent_free_space(free: &mut Vec<FreeSpace>) {
    free.sort_by(|a, b| human_to_bytes(&a.start).cmp(&human_to_bytes(&b.start)));
    let mut i = 0;
    while i + 1 < free.len() {
        let ae = human_to_bytes(&free[i].end);
        let bs = human_to_bytes(&free[i + 1].start);
        if ae >= bs {
            let a_start = human_to_bytes(&free[i].start);
            let b_end = human_to_bytes(&free[i + 1].end);
            free[i].end = bytes_to_human(b_end);
            free[i].size = bytes_to_human(b_end - a_start);
            free.remove(i + 1);
        } else {
            i += 1;
        }
    }
}

fn create_partition_from_free(fs_idx: usize, size_str: &str, parts: &mut Vec<Partition>, free: &mut Vec<FreeSpace>) {
    let size_bytes = human_to_bytes(size_str);
    if size_bytes == 0 { return; }
    let fs = &free[fs_idx];
    let fs_start = human_to_bytes(&fs.start);
    let fs_end = human_to_bytes(&fs.end);
    let clamped = size_bytes.min(fs_end - fs_start);
    if clamped == 0 { return; }
    let num = parts.iter().map(|p| p.number).max().unwrap_or(0) + 1;
    parts.push(Partition {
        number: num, start: fs.start.clone(), end: bytes_to_human(fs_start + clamped),
        size: bytes_to_human(clamped), ptype: "Linux filesystem".into(),
        flags: vec![], fs_signature: None,
    });
    let rem = fs_end - fs_start - clamped;
    if rem > 0 {
        free[fs_idx].start = bytes_to_human(fs_start + clamped);
        free[fs_idx].size = bytes_to_human(rem);
    } else { free.remove(fs_idx); }
    merge_adjacent_free_space(free);
}

fn apply_resize(parts: &mut Vec<Partition>, free: &mut Vec<FreeSpace>, idx: usize, new_size: &str) {
    let new_bytes = human_to_bytes(new_size);
    if new_bytes == 0 { return; }
    let p = &parts[idx];
    let old_start = human_to_bytes(&p.start);
    let old_end = human_to_bytes(&p.end);
    let old_size = old_end - old_start;
    if new_bytes < old_size {
        let new_end = old_start + new_bytes;
        parts[idx].end = bytes_to_human(new_end);
        parts[idx].size = bytes_to_human(new_bytes);
        free.push(FreeSpace {
            start: bytes_to_human(new_end), end: bytes_to_human(old_end),
            size: bytes_to_human(old_end - new_end),
        });
    }
    merge_adjacent_free_space(free);
}