use anyhow::Result;
use crate::backend::Backend;
use crate::widget::{EventResult, Widget};

pub fn run(
    mut widget: impl Widget,
    backend: &mut impl Backend,
    tx: Option<std::sync::mpsc::Sender<serde_json::Value>>,
) -> Result<filly_protocol::WidgetResponse> {
    loop {
        if widget.is_dirty() {
            let area = backend.size();
            let tree = widget.render(area);
            backend.draw(&tree)?;
            widget.clear_dirty();
        }
        let event = backend.next_event()?;
        match widget.handle_event(&event) {
            EventResult::Response(resp) => return Ok(resp),
            EventResult::Yield(v) => {
                if let Some(ref tx) = tx { tx.send(v).ok(); }
            }
            EventResult::Handled => {}
            EventResult::Unhandled => {}
        }
    }
}