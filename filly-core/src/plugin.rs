use crate::widget::Widget;
use crate::store::Store;
use crate::theme::Theme;
use filly_protocol::WidgetRequest;
use std::collections::HashMap;
use std::path::Path;
use libloading::{Library, Symbol};

pub type WidgetFactory = fn(&WidgetRequest, &Store, &Theme) -> Box<dyn Widget>;

pub struct PluginRegistry {
    factories: HashMap<String, WidgetFactory>,
    libraries: Vec<Library>,
}

impl PluginRegistry {
    pub fn new() -> Self {
        Self {
            factories: HashMap::new(),
            libraries: Vec::new(),
        }
    }

    pub fn register(&mut self, widget_type: &str, factory: WidgetFactory) {
        self.factories.insert(widget_type.to_string(), factory);
    }

    pub fn load_plugin(&mut self, path: &Path) -> Result<(), Box<dyn std::error::Error>> {
        unsafe {
            let lib = Library::new(path)?;
            let register_func: Symbol<fn(&mut PluginRegistry)> = lib.get(b"register")?;
            register_func(self);
            self.libraries.push(lib);
        }
        Ok(())
    }

    pub fn create(
        &self,
        req: &WidgetRequest,
        store: &Store,
        theme: &Theme,
    ) -> Option<Box<dyn Widget>> {
        self.factories
            .get(&req.widget)
            .map(|f| f(req, store, theme))
    }
}