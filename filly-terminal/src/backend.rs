use anyhow::{Context, Result};
use crossterm::{
    event::{self, Event as CEvent, KeyCode as CKeyCode, KeyModifiers as CKeyModifiers, MouseEventKind},
    execute,
    terminal::{disable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen, size},
};
use ratatui::{backend::CrosstermBackend, Terminal};
use std::fs::OpenOptions;
use std::io::{stdout, Write};
use std::time::Duration;
use filly_core::{
    backend::Backend,
    event::{Event, KeyCode, KeyModifiers, MouseButton, MouseEvent},
    render::Rect,
    RenderTree,
};

pub struct TerminalBackend {
    terminal: Terminal<CrosstermBackend<std::fs::File>>,
}

impl TerminalBackend {
    pub fn new() -> Result<Self> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .open("/dev/tty")
            .context("Cannot open /dev/tty")?;

        let mut terminal = Terminal::new(CrosstermBackend::new(file))?;
        terminal.clear()?;

        Ok(Self { terminal })
    }
}

impl Backend for TerminalBackend {
    fn setup(&mut self) -> Result<()> {
        crossterm::terminal::enable_raw_mode()?;
        execute!(stdout(), EnterAlternateScreen, crossterm::cursor::Hide)?;
        std::thread::sleep(Duration::from_millis(50));
        Ok(())
    }

    fn draw(&mut self, tree: &RenderTree) -> Result<()> {
        self.terminal.draw(|f| {
            let area = f.area();
            super::renderer::render_tree(f, tree, area);
        })?;
        Ok(())
    }

    fn next_event(&mut self) -> Result<Event> {
        loop {
            if event::poll(Duration::from_millis(10))? {
                let ce = event::read()?;
                match ce {
                    CEvent::Key(key) => {
                        let code = match key.code {
                            CKeyCode::Backspace => KeyCode::Backspace,
                            CKeyCode::Enter => KeyCode::Enter,
                            CKeyCode::Left => KeyCode::Left,
                            CKeyCode::Right => KeyCode::Right,
                            CKeyCode::Up => KeyCode::Up,
                            CKeyCode::Down => KeyCode::Down,
                            CKeyCode::Home => KeyCode::Home,
                            CKeyCode::End => KeyCode::End,
                            CKeyCode::PageUp => KeyCode::PageUp,
                            CKeyCode::PageDown => KeyCode::PageDown,
                            CKeyCode::Tab => KeyCode::Tab,
                            CKeyCode::BackTab => KeyCode::BackTab,
                            CKeyCode::Delete => KeyCode::Delete,
                            CKeyCode::Insert => KeyCode::Insert,
                            CKeyCode::F(n) => KeyCode::F(n),
                            CKeyCode::Char(c) => KeyCode::Char(c),
                            CKeyCode::Esc => KeyCode::Esc,
                            _ => continue,
                        };
                        let modifiers = KeyModifiers {
                            shift: key.modifiers.contains(CKeyModifiers::SHIFT),
                            ctrl: key.modifiers.contains(CKeyModifiers::CONTROL),
                            alt: key.modifiers.contains(CKeyModifiers::ALT),
                            super_key: key.modifiers.contains(CKeyModifiers::SUPER),
                        };
                        return Ok(Event::Key(code, modifiers));
                    }
                    CEvent::Mouse(mouse) => {
                        let event = match mouse.kind {
                            MouseEventKind::Down(btn) => {
                                let button = match btn {
                                    crossterm::event::MouseButton::Left => MouseButton::Left,
                                    crossterm::event::MouseButton::Right => MouseButton::Right,
                                    crossterm::event::MouseButton::Middle => MouseButton::Middle,
                                };
                                MouseEvent::Press(button, mouse.column, mouse.row)
                            }
                            MouseEventKind::Up(_) => MouseEvent::Release(mouse.column, mouse.row),
                            MouseEventKind::ScrollDown => MouseEvent::ScrollDown(mouse.column, mouse.row),
                            MouseEventKind::ScrollUp => MouseEvent::ScrollUp(mouse.column, mouse.row),
                            _ => continue,
                        };
                        return Ok(Event::Mouse(event));
                    }
                    CEvent::Resize(w, h) => return Ok(Event::Resize(w, h)),
                    _ => {}
                }
            }
        }
    }

    fn teardown(&mut self) -> Result<()> {
        execute!(stdout(), crossterm::cursor::Show, LeaveAlternateScreen)?;
        disable_raw_mode()?;
        stdout().flush()?;
        Ok(())
    }

    fn size(&self) -> Rect {
        let (w, h) = size().unwrap_or((80, 24));
        Rect::new(0, 0, w, h)
    }
}