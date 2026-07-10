#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Event {
    Key(KeyCode, KeyModifiers),
    Mouse(MouseEvent),
    Resize(u16, u16),
    Tick,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum KeyCode {
    Backspace,
    Enter,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Tab,
    BackTab,
    Delete,
    Insert,
    F(u8),
    Char(char),
    Esc,
    Null,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KeyModifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
    pub super_key: bool,
}

impl KeyModifiers {
    pub const NONE: Self = Self { shift: false, ctrl: false, alt: false, super_key: false };
    pub fn ctrl() -> Self { Self { ctrl: true, ..Self::NONE } }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseEvent {
    Press(MouseButton, u16, u16),
    Release(u16, u16),
    ScrollDown(u16, u16),
    ScrollUp(u16, u16),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseButton {
    Left,
    Right,
    Middle,
}