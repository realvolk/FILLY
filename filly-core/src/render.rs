use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy)]
pub struct Rect {
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
}

impl Rect {
    pub const fn new(x: u16, y: u16, width: u16, height: u16) -> Self {
        Self { x, y, width, height }
    }
}

#[derive(Debug, Clone, Copy)]
pub enum BorderStyle {
    None,
    Single,
    Double,
    Rounded,
}

#[derive(Debug, Clone, Copy)]
pub enum Alignment {
    Left,
    Center,
    Right,
}

#[derive(Debug, Clone, Copy)]
pub enum Orientation {
    Horizontal,
    Vertical,
}

#[derive(Debug, Clone, Copy)]
pub struct EdgeInsets {
    pub top: u16,
    pub bottom: u16,
    pub left: u16,
    pub right: u16,
}

impl EdgeInsets {
    pub const fn zero() -> Self {
        Self { top: 0, bottom: 0, left: 0, right: 0 }
    }
    pub const fn uniform(v: u16) -> Self {
        Self { top: v, bottom: v, left: v, right: v }
    }
}

#[derive(Debug, Clone)]
pub struct TextStyle {
    pub fg: Option<String>,
    pub bg: Option<String>,
    pub bold: bool,
    pub italic: bool,
    pub underline: bool,
}

impl TextStyle {
    pub fn normal() -> Self {
        Self { fg: Some("white".into()), bg: None, bold: false, italic: false, underline: false }
    }
    pub fn accent() -> Self {
        Self { fg: Some("green".into()), ..Self::normal() }
    }
    pub fn muted() -> Self {
        Self { fg: Some("darkgray".into()), ..Self::normal() }
    }
    pub fn selected() -> Self {
        Self { fg: Some("green".into()), bg: Some("darkgray".into()), ..Self::normal() }
    }
}

#[derive(Debug, Clone)]
pub struct ListItem {
    pub label: String,
    pub meta: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TreeNode {
    pub label: String,
    pub expanded: bool,
    pub children: Vec<TreeNode>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FormField {
    pub label: String,
    pub widget_type: String,
    pub value: String,
    pub choices: Vec<String>,
    pub placeholder: String,
}

#[derive(Debug, Clone)]
pub enum RenderNode {
    Container {
        rect: Rect,
        background: Option<String>,
        border: BorderStyle,
        padding: EdgeInsets,
        children: Vec<RenderNode>,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Text {
        rect: Rect,
        content: String,
        alignment: Alignment,
        style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    List {
        rect: Rect,
        items: Vec<ListItem>,
        selected: Option<usize>,
        highlight_style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Input {
        rect: Rect,
        text: String,
        cursor: usize,
        placeholder: String,
        masked: bool,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Checkbox {
        rect: Rect,
        label: String,
        checked: bool,
        focused: bool,
        role: Option<String>,
        description: Option<String>,
    },
    Toggle {
        rect: Rect,
        label: String,
        value: bool,
        focused: bool,
        role: Option<String>,
        description: Option<String>,
    },
    Spinner {
        rect: Rect,
        message: String,
        frame: usize,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Separator {
        rect: Rect,
        orientation: Orientation,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Badge {
        rect: Rect,
        text: String,
        style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Cursor {
        rect: Rect,
        x: u16,
        y: u16,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Table {
        rect: Rect,
        headers: Vec<String>,
        rows: Vec<Vec<String>>,
        selected_row: Option<usize>,
        selected_col: Option<usize>,
        highlight_style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Tree {
        rect: Rect,
        nodes: Vec<TreeNode>,
        selected: Vec<usize>,
        highlight_style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Gauge {
        rect: Rect,
        percent: u16,
        label: String,
        style: TextStyle,
        role: Option<String>,
        description: Option<String>,
    },
    Calendar {
        rect: Rect,
        year: i32,
        month: u32,
        selected_day: Option<u32>,
        highlight_style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Form {
        rect: Rect,
        fields: Vec<FormField>,
        focused: usize,
        submit_label: String,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Tabs {
        rect: Rect,
        tabs: Vec<String>,
        active: usize,
        child: Box<RenderNode>,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    SplitPanes {
        rect: Rect,
        orientation: Orientation,
        split_position: u16,
        first: Box<RenderNode>,
        second: Box<RenderNode>,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    ContextMenu {
        rect: Rect,
        items: Vec<ListItem>,
        selected: Option<usize>,
        highlight_style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
    Toast {
        rect: Rect,
        message: String,
        style: TextStyle,
        role: Option<String>,
        label: Option<String>,
        description: Option<String>,
    },
}

pub type RenderTree = RenderNode;