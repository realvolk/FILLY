use crate::backend::Backend;
use crate::event::Event;
use crate::render::{Rect, RenderTree};
use anyhow::Result;
use std::collections::VecDeque;

pub struct HeadlessBackend {
    pub events: VecDeque<Event>,
    pub output: String,
    pub size: (u16, u16),
}

impl HeadlessBackend {
    pub fn new() -> Self {
        Self {
            events: VecDeque::new(),
            output: String::new(),
            size: (80, 24),
        }
    }

    pub fn push_event(&mut self, event: Event) {
        self.events.push_back(event);
    }

    pub fn push_key(&mut self, code: crate::KeyCode) {
        self.events.push_back(Event::Key(code, crate::KeyModifiers::NONE));
    }

    pub fn push_char(&mut self, c: char) {
        self.events.push_back(Event::Key(crate::KeyCode::Char(c), crate::KeyModifiers::NONE));
    }

    pub fn push_enter(&mut self) {
        self.events.push_back(Event::Key(crate::KeyCode::Enter, crate::KeyModifiers::NONE));
    }

    pub fn push_esc(&mut self) {
        self.events.push_back(Event::Key(crate::KeyCode::Esc, crate::KeyModifiers::NONE));
    }

    pub fn push_up(&mut self) {
        self.events.push_back(Event::Key(crate::KeyCode::Up, crate::KeyModifiers::NONE));
    }

    pub fn push_down(&mut self) {
        self.events.push_back(Event::Key(crate::KeyCode::Down, crate::KeyModifiers::NONE));
    }
}

impl Backend for HeadlessBackend {
    fn setup(&mut self) -> Result<()> { Ok(()) }
    fn draw(&mut self, tree: &RenderTree) -> Result<()> {
        self.output = format!("{:#?}", tree);
        Ok(())
    }
    fn next_event(&mut self) -> Result<Event> {
        self.events.pop_front()
            .ok_or_else(|| anyhow::anyhow!("No more events"))
    }
    fn teardown(&mut self) -> Result<()> { Ok(()) }
    fn size(&self) -> Rect {
        Rect::new(0, 0, self.size.0, self.size.1)
    }
}