pub mod checklist;
pub mod disk;
pub mod file_picker;
pub mod filter;
pub mod hub;
pub mod input;
pub mod menu;
pub mod msg;
pub mod multiselect;
pub mod password;
pub mod progress;
pub mod separator;
pub mod spinner;
pub mod summary;
pub mod text;
pub mod toggle;
pub mod yesno;
pub mod table;
pub mod tree;
pub mod gauge;
pub mod calendar;
pub mod form;
pub mod tabs;
pub mod split_panes;
pub mod context_menu;
pub mod notification;
pub mod radio_group;
pub mod range_slider;
pub mod color_picker;
pub mod badge;
pub mod rich_text;
pub mod tooltip;

use crate::plugin::PluginRegistry;
use crate::widget::Widget;
use crate::store::Store;
use crate::theme::Theme;
use filly_protocol::WidgetRequest;
use serde_json::Value;
use crate::render::{TreeNode, FormField, Orientation};
use std::sync::Mutex;

lazy_static::lazy_static! {
    pub static ref REGISTRY: Mutex<PluginRegistry> = Mutex::new({
        let mut reg = PluginRegistry::new();
        reg.register("menu", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let default = p.get("default").and_then(|v| v.as_str().map(String::from));
            let default_idx = default.and_then(|d| choices.iter().position(|c| c == &d));
            Box::new(menu::MenuWidget::new(title, message, choices, default_idx))
        });
        reg.register("yesno", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let default = p.get("default").and_then(|v| v.as_bool());
            Box::new(yesno::YesNoWidget::new(title, message, default))
        });
        reg.register("input", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let default = p.get("default").and_then(|v| v.as_str().map(String::from));
            let placeholder = p.get("placeholder").and_then(|v| v.as_str().map(String::from));
            let validation = p.get("validation").and_then(|v| v.as_str().map(String::from));
            Box::new(input::InputWidget::new(title, message, default, placeholder, validation))
        });
        reg.register("password", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let placeholder = p.get("placeholder").and_then(|v| v.as_str().map(String::from));
            Box::new(password::PasswordWidget::new(title, message, placeholder))
        });
        reg.register("checklist", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let min = p.get("min").and_then(|v| v.as_u64()).map(|v| v as usize);
            let max = p.get("max").and_then(|v| v.as_u64()).map(|v| v as usize);
            let default: Vec<String> = p.get("default").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            Box::new(checklist::ChecklistWidget::new(title, message, choices, min, max, default))
        });
        reg.register("msg", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(msg::MsgWidget::new(title, message))
        });
        reg.register("progress", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let command: Vec<String> = p.get("command").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let logfile = p.get("logfile").and_then(|v| v.as_str().map(String::from));
            Box::new(progress::ProgressWidget::new(title, command, logfile, None))
        });
        reg.register("file_picker", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let start_dir = p.get("start_dir").and_then(|v| v.as_str().map(String::from));
            let filter = p.get("filter").and_then(|v| v.as_str().map(String::from));
            Box::new(file_picker::FilePickerWidget::new(title, start_dir, filter))
        });
        reg.register("filter", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let placeholder = p.get("placeholder").and_then(|v| v.as_str().map(String::from));
            Box::new(filter::FilterWidget::new(title, message, choices, placeholder))
        });
        reg.register("multiselect", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let placeholder = p.get("placeholder").and_then(|v| v.as_str().map(String::from));
            let min = p.get("min").and_then(|v| v.as_u64()).map(|v| v as usize);
            let max = p.get("max").and_then(|v| v.as_u64()).map(|v| v as usize);
            Box::new(multiselect::MultiselectWidget::new(title, message, choices, placeholder, min, max))
        });
        reg.register("summary", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str().map(String::from));
            let file = p.get("file").and_then(|v| v.as_str().map(String::from));
            Box::new(summary::SummaryWidget::new(title, message, file))
        });
        reg.register("text", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let file = p.get("file").and_then(|v| v.as_str().map(String::from));
            let content = p.get("content").and_then(|v| v.as_str().map(String::from));
            Box::new(text::TextEditorWidget::new(title, file, content))
        });
        reg.register("hub", |req, store, _| {
            Box::new(hub::HubComposite::from_params(&req.params, store))
        });
        reg.register("toggle", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let label = p.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let default = p.get("default").and_then(|v| v.as_bool());
            Box::new(toggle::ToggleWidget::new(title, label, default))
        });
        reg.register("spinner", |req, _, _| {
            let p = &req.params;
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(spinner::SpinnerWidget::new(message))
        });
        reg.register("separator", |req, _, _| {
            let p = &req.params;
            let orientation = p.get("orientation").and_then(|v| v.as_str()).unwrap_or("horizontal");
            let o = if orientation == "vertical" { Orientation::Vertical } else { Orientation::Horizontal };
            Box::new(separator::SeparatorWidget::new(o))
        });
        reg.register("disk", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let disk = p.get("disk").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let partitions = p.get("partitions").cloned().unwrap_or(Value::Null);
            let free_space = p.get("free_space").cloned();
            let readonly = p.get("readonly").and_then(|v| v.as_bool());
            Box::new(disk::DiskWidget::new(title, disk, partitions, free_space, readonly))
        });
        reg.register("table", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let headers: Vec<String> = p.get("headers").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let rows: Vec<Vec<String>> = p.get("rows").and_then(|v| v.as_array())
                .map(|a| a.iter().map(|row| row.as_array()
                    .map(|cells| cells.iter().filter_map(|c| c.as_str().map(String::from)).collect())
                    .unwrap_or_default()).collect())
                .unwrap_or_default();
            Box::new(table::TableWidget::new(title, headers, rows))
        });
        reg.register("tree", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let nodes: Vec<TreeNode> = p.get("nodes").and_then(|v| serde_json::from_value(v.clone()).ok()).unwrap_or_default();
            Box::new(tree::TreeWidget::new(title, nodes))
        });
        reg.register("gauge", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let percent = p.get("percent").and_then(|v| v.as_u64()).unwrap_or(0) as u16;
            let label = p.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(gauge::GaugeWidget::new(title, percent, label))
        });
        reg.register("calendar", |req, _, _| {
            let title = req.params.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(calendar::CalendarWidget::new(title))
        });
        reg.register("form", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let fields: Vec<FormField> = p.get("fields").and_then(|v| serde_json::from_value(v.clone()).ok()).unwrap_or_default();
            let submit_label = p.get("submit_label").and_then(|v| v.as_str()).unwrap_or("Submit").to_string();
            Box::new(form::FormWidget::new(title, fields, submit_label))
        });
        reg.register("tabs", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("Tabs").to_string();
            let tab_names: Vec<String> = p.get("tabs").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            Box::new(tabs::TabsWidget::new(title, tab_names, vec![]))
        });
        reg.register("split_panes", |req, _, _| {
            let p = &req.params;
            let orientation = p.get("orientation").and_then(|v| v.as_str()).unwrap_or("horizontal");
            let o = if orientation == "vertical" { Orientation::Vertical } else { Orientation::Horizontal };
            Box::new(split_panes::SplitPanesWidget::new(o,
                Box::new(text::TextEditorWidget::new("Left".into(), None, Some("Left pane".into()))),
                Box::new(text::TextEditorWidget::new("Right".into(), None, Some("Right pane".into())))))
        });
        reg.register("context_menu", |req, _, _| {
            let p = &req.params;
            let items: Vec<String> = p.get("items").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            Box::new(context_menu::ContextMenuWidget::new(items))
        });
        reg.register("notification", |req, _, _| {
            let p = &req.params;
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let duration = p.get("duration").and_then(|v| v.as_u64()).unwrap_or(3);
            Box::new(notification::NotificationWidget::new(message, duration))
        });
        reg.register("radio_group", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let message = p.get("message").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let choices: Vec<String> = p.get("choices").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            let default = p.get("default").and_then(|v| v.as_u64()).map(|v| v as usize);
            Box::new(radio_group::RadioGroupWidget::new(title, message, choices, default))
        });
        reg.register("range_slider", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let min = p.get("min").and_then(|v| v.as_i64()).unwrap_or(0) as i32;
            let max = p.get("max").and_then(|v| v.as_i64()).unwrap_or(100) as i32;
            let value = p.get("value").and_then(|v| v.as_i64()).unwrap_or(50) as i32;
            let label = p.get("label").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(range_slider::RangeSliderWidget::new(title, min, max, value, label))
        });
        reg.register("color_picker", |req, _, _| {
            let p = &req.params;
            let title = p.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
            let colors: Vec<String> = p.get("colors").and_then(|v| v.as_array())
                .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default();
            Box::new(color_picker::ColorPickerWidget::new(title, colors))
        });
        reg.register("badge", |req, _, _| {
            let p = &req.params;
            let text = p.get("text").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(badge::BadgeWidget::new(text))
        });
        reg.register("rich_text", |req, _, _| {
            let p = &req.params;
            let content = p.get("content").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(rich_text::RichTextWidget::new(content))
        });
        reg.register("tooltip", |req, _, _| {
            let p = &req.params;
            let text = p.get("text").and_then(|v| v.as_str()).unwrap_or("").to_string();
            Box::new(tooltip::TooltipWidget::new(text))
        });
        reg
    });
}

pub fn create_widget(req: &WidgetRequest, store: &Store, theme: &Theme) -> Option<Box<dyn Widget>> {
    REGISTRY.lock().unwrap().create(req, store, theme)
}