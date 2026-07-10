use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use crate::render::TextStyle;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Theme {
    pub name: String,
    pub colors: HashMap<String, String>,
    pub styles: HashMap<String, StyleDef>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct StyleDef {
    pub fg: Option<String>,
    pub bg: Option<String>,
    pub bold: Option<bool>,
    pub italic: Option<bool>,
    pub underline: Option<bool>,
    pub border: Option<String>,
    pub padding: Option<EdgeInsetsDef>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeInsetsDef {
    pub top: u16,
    pub bottom: u16,
    pub left: u16,
    pub right: u16,
}

impl Theme {
    pub fn load(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let data = fs::read_to_string(path)?;
        let theme: Theme = serde_json::from_str(&data)?;
        Ok(theme)
    }

    pub fn default() -> Self {
        let mut styles = HashMap::new();
        styles.insert("title".into(), StyleDef { fg: Some("green".into()), bold: Some(true), ..Default::default() });
        styles.insert("body".into(), StyleDef { fg: Some("white".into()), ..Default::default() });
        styles.insert("highlight".into(), StyleDef { fg: Some("green".into()), bg: Some("darkgray".into()), ..Default::default() });
        styles.insert("muted".into(), StyleDef { fg: Some("darkgray".into()), ..Default::default() });
        Theme {
            name: "default".into(),
            colors: HashMap::new(),
            styles,
        }
    }

    pub fn style_for(&self, key: &str) -> TextStyle {
        if let Some(def) = self.styles.get(key) {
            TextStyle {
                fg: def.fg.clone(),
                bg: def.bg.clone(),
                bold: def.bold.unwrap_or(false),
                italic: def.italic.unwrap_or(false),
                underline: def.underline.unwrap_or(false),
            }
        } else {
            TextStyle::normal()
        }
    }

    pub fn reload(&mut self, path: &Path) -> Result<(), Box<dyn std::error::Error>> {
        *self = Self::load(path)?;
        Ok(())
    }
}