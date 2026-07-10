use crate::render::RenderTree;
use crate::event::Event;
use crate::render::Rect;
use anyhow::Result;

pub trait Backend {
    fn setup(&mut self) -> Result<()>;
    fn draw(&mut self, tree: &RenderTree) -> Result<()>;
    fn next_event(&mut self) -> Result<Event>;
    fn teardown(&mut self) -> Result<()>;
    fn size(&self) -> Rect;
}