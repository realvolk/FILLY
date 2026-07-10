use anyhow::Result;
use clap::{Parser, Subcommand};
use crossterm::{
    execute,
    terminal::{disable_raw_mode, LeaveAlternateScreen},
};
use filly_core::{Backend, Store, Theme};
use filly_core::render::{TreeNode, FormField, Orientation};
use filly_protocol::WidgetRequest;
use filly_terminal::TerminalBackend;
use std::fs;
use std::io::{self, stdout, IsTerminal, Read, Write};
use std::os::unix::net::UnixListener;
use std::panic;
use std::path::Path;

#[derive(Parser)]
struct Cli {
    #[command(subcommand)]
    command: Option<Command>,
}

#[derive(Subcommand)]
enum Command {
    Daemon {
        #[arg(short, long, default_value = "/tmp/filly.sock")]
        socket: String,
        #[arg(short, long)]
        theme: Option<String>,
    },
    Oneshot {
        #[arg(short, long)]
        input: Option<String>,
        #[arg(short, long)]
        output: Option<String>,
        #[arg(short, long)]
        theme: Option<String>,
    },
    Batch { #[arg(short, long)] input: Option<String> },
    Validate { #[arg(short, long)] input: String },
    Schema { #[arg(short, long)] widget: Option<String> },
    NewWidget { name: String },
    Demo { #[arg(short, long)] theme: Option<String> },
}

fn main() {
    let original_hook = panic::take_hook();
    panic::set_hook(Box::new(move |info| {
        let _ = execute!(stdout(), crossterm::cursor::Show, LeaveAlternateScreen);
        let _ = disable_raw_mode();
        let _ = io::stdout().flush();
        original_hook(info);
    }));
    let result = run_app();
    let _ = execute!(stdout(), crossterm::cursor::Show, LeaveAlternateScreen);
    let _ = disable_raw_mode();
    let _ = io::stdout().flush();
    if let Err(e) = result { eprintln!("Error: {}", e); std::process::exit(1); }
}

fn load_theme(name: &Option<String>) -> Theme {
    match name {
        Some(n) => {
            let path = format!("themes/{}.json", n);
            Theme::load(Path::new(&path)).unwrap_or_else(|_| Theme::default())
        }
        None => Theme::default(),
    }
}

fn run_app() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Some(Command::Daemon { socket, theme }) => {
            let _ = fs::remove_file(&socket);
            filly_daemon::run(UnixListener::bind(&socket)?, load_theme(&theme))?;
        }
        Some(Command::Oneshot { input, output, theme }) => {
            let theme = load_theme(&theme);
            let data = if let Some(path) = input { fs::read_to_string(&path)? }
            else { let stdin = io::stdin(); if stdin.is_terminal() { String::new() } else { let mut s = String::new(); stdin.lock().read_to_string(&mut s)?; s } };
            if data.trim().is_empty() { run_demo(&theme)?; return Ok(()); }
            let req: WidgetRequest = serde_json::from_str(&data)?;
            let store = Store::new();
            let widget = filly_core::widgets::create_widget(&req, &store, &theme)
                .ok_or_else(|| anyhow::anyhow!("Unknown widget: {}", req.widget))?;
            let mut backend = TerminalBackend::new()?;
            backend.setup()?;
            let resp = filly_core::session::run(widget, &mut backend, None)?;
            backend.teardown()?;
            let json = serde_json::to_string(&resp)?;
            if let Some(path) = output { fs::write(path, json + "\n")?; } else { println!("{}", json); }
        }
        Some(Command::Batch { input }) => {
            let path = input.unwrap_or_else(|| "/dev/stdin".into());
            let content = fs::read_to_string(&path)?;
            let mut out = io::stdout();
            let theme = Theme::default();
            for line in content.lines() {
                let line = line.trim();
                if line.is_empty() { continue; }
                let req: WidgetRequest = match serde_json::from_str(line) {
                    Ok(r) => r,
                    Err(e) => { writeln!(out, "{}", serde_json::to_string(&filly_protocol::WidgetResponse { result: None, cancelled: true, error: Some(format!("Invalid: {}", e)) })?)?; continue; }
                };
                let store = Store::new();
                if let Some(widget) = filly_core::widgets::create_widget(&req, &store, &theme) {
                    let mut backend = TerminalBackend::new()?;
                    backend.setup()?;
                    let resp = filly_core::session::run(widget, &mut backend, None)?;
                    backend.teardown()?;
                    writeln!(out, "{}", serde_json::to_string(&resp)?)?;
                }
            }
        }
        Some(Command::Validate { input }) => {
            let data = fs::read_to_string(&input)?;
            let req: WidgetRequest = serde_json::from_str(&data)?;
            if filly_core::widgets::create_widget(&req, &Store::new(), &Theme::default()).is_some() {
                println!("Valid: {}", req.widget);
            } else { eprintln!("Unknown: {}", req.widget); std::process::exit(1); }
        }
        Some(Command::Schema { widget }) => {
            let schema = if let Some(w) = widget { schema_for_widget(&w) } else { all_widget_schemas() };
            println!("{}", serde_json::to_string_pretty(&schema)?);
        }
        Some(Command::NewWidget { name }) => {
            let template = include_str!("widget_template.rs");
            let content = template.replace("{{NAME}}", &name);
            let path = format!("filly-core/src/widgets/{}.rs", name.to_lowercase());
            fs::write(&path, &content)?;
            println!("Created {}", path);
        }
        Some(Command::Demo { theme }) => run_demo(&load_theme(&theme))?,
        None => run_demo(&Theme::default())?,
    }
    Ok(())
}

fn schema_for_widget(w: &str) -> serde_json::Value { serde_json::json!({"widget": w}) }
fn all_widget_schemas() -> serde_json::Value { serde_json::json!({}) }

fn run_demo(theme: &Theme) -> Result<()> {
    let mut backend = TerminalBackend::new()?;
    backend.setup()?;

    // 1  msg
    filly_core::session::run(filly_core::widgets::msg::MsgWidget::new("FILLY Demo".into(), "Welcome! Press any key.".into()), &mut backend, None)?;
    // 2  yesno
    filly_core::session::run(filly_core::widgets::yesno::YesNoWidget::new("Yes/No".into(), "Is FILLY working?".into(), Some(true)), &mut backend, None)?;
    // 3  menu
    filly_core::session::run(filly_core::widgets::menu::MenuWidget::new("Menu".into(), "Choose desktop:".into(), vec!["KDE".into(),"XFCE".into(),"Sway".into(),"Hyprland".into(),"i3".into()], Some(0)), &mut backend, None)?;
    // 4  input
    filly_core::session::run(filly_core::widgets::input::InputWidget::new("Input".into(), "Username:".into(), Some("artix".into()), Some("username".into()), None), &mut backend, None)?;
    // 5  password
    filly_core::session::run(filly_core::widgets::password::PasswordWidget::new("Password".into(), "Secret:".into(), Some("password".into())), &mut backend, None)?;
    // 6  checklist
    filly_core::session::run(filly_core::widgets::checklist::ChecklistWidget::new("Checklist".into(), "Select packages:".into(), vec!["git".into(),"neovim".into(),"firefox".into(),"alacritty".into(),"tmux".into()], Some(0), Some(5), vec!["git".into(),"neovim".into()]), &mut backend, None)?;
    // 7  filter
    filly_core::session::run(filly_core::widgets::filter::FilterWidget::new("Filter".into(), "Search timezones:".into(), vec!["Europe/London".into(),"Europe/Belgrade".into(),"America/New_York".into(),"Asia/Tokyo".into(),"Australia/Sydney".into()], Some("Type to filter...".into())), &mut backend, None)?;
    // 8  multiselect
    filly_core::session::run(filly_core::widgets::multiselect::MultiselectWidget::new("Multiselect".into(), "Select packages:".into(), vec!["git".into(),"neovim".into(),"firefox".into(),"alacritty".into(),"tmux".into(),"docker".into(),"podman".into()], Some("Search...".into()), Some(0), Some(7)), &mut backend, None)?;
    // 9  summary
    filly_core::session::run(filly_core::widgets::summary::SummaryWidget::new("Summary".into(), Some("Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8\nLine 9\nLine 10".into()), None), &mut backend, None)?;
    // 10 file_picker
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    filly_core::session::run(filly_core::widgets::file_picker::FilePickerWidget::new("File Picker".into(), Some(home), None), &mut backend, None)?;
    // 11 text editor
    filly_core::session::run(filly_core::widgets::text::TextEditorWidget::new("Text Editor".into(), None, Some("Edit me.\nLine 2\nLine 3".into())), &mut backend, None)?;
    // 12 toggle
    filly_core::session::run(filly_core::widgets::toggle::ToggleWidget::new("Toggle".into(), "Enable feature?".into(), Some(false)), &mut backend, None)?;
    // 13 hub
    let categories = serde_json::json!([
        {"label":"System","summary_template":"Host: {HOSTNAME}","items":[
            {"id":"HOSTNAME","label":"Hostname","value":"artix","widget":"input","choices":[],"placeholder":"","visible_if":{},"display":""},
            {"id":"INIT","label":"Init","value":"openrc","widget":"menu","choices":["openrc","runit","dinit","s6"],"placeholder":"","visible_if":{},"display":""}
        ]},
        {"label":"Desktop","summary_template":"WM: {WM_DE}","items":[
            {"id":"WM_DE","label":"WM","value":"none","widget":"menu","choices":["kde","xfce","sway","hyprland","i3","none"],"placeholder":"","visible_if":{},"display":""}
        ]}
    ]);
    let hub_req = WidgetRequest { widget: "hub".into(), step: None, total: None, params: serde_json::json!({"title":"Hub","categories":categories,"actions":["Proceed"]}), session_id: None };
    let store = Store::new();
    let hub = filly_core::widgets::create_widget(&hub_req, &store, theme).unwrap();
    filly_core::session::run(hub, &mut backend, None)?;
    // 14 table
    filly_core::session::run(filly_core::widgets::table::TableWidget::new("Table".into(), vec!["Name".into(),"Size".into(),"Type".into()], vec![vec!["firefox".into(),"120MB".into(),"browser".into()], vec!["neovim".into(),"8MB".into(),"editor".into()], vec!["htop".into(),"2MB".into(),"monitor".into()]]), &mut backend, None)?;
    // 15 tree
    let tree_nodes = vec![
        TreeNode { label: "src".into(), expanded: true, children: vec![
            TreeNode { label: "main.rs".into(), expanded: false, children: vec![] },
            TreeNode { label: "lib.rs".into(), expanded: false, children: vec![] },
        ]},
        TreeNode { label: "Cargo.toml".into(), expanded: false, children: vec![] },
    ];
    filly_core::session::run(filly_core::widgets::tree::TreeWidget::new("Tree".into(), tree_nodes), &mut backend, None)?;
    // 16 gauge
    filly_core::session::run(filly_core::widgets::gauge::GaugeWidget::new("Gauge".into(), 73, "Progress".into()), &mut backend, None)?;
    // 17 calendar
    filly_core::session::run(filly_core::widgets::calendar::CalendarWidget::new("Calendar".into()), &mut backend, None)?;
    // 18 form
    let form_fields = vec![
        FormField { label: "Name".into(), widget_type: "input".into(), value: "".into(), choices: vec![], placeholder: "Enter name".into() },
        FormField { label: "OS".into(), widget_type: "menu".into(), value: "Artix".into(), choices: vec!["Artix".into(),"Arch".into(),"Debian".into()], placeholder: "".into() },
        FormField { label: "Enable".into(), widget_type: "toggle".into(), value: "yes".into(), choices: vec![], placeholder: "".into() },
    ];
    filly_core::session::run(filly_core::widgets::form::FormWidget::new("Form".into(), form_fields, "Submit".into()), &mut backend, None)?;
    // 19 tabs
    let tab_widgets: Vec<Box<dyn filly_core::Widget>> = vec![
        Box::new(filly_core::widgets::msg::MsgWidget::new("Tab 1".into(), "First tab content".into())),
        Box::new(filly_core::widgets::msg::MsgWidget::new("Tab 2".into(), "Second tab content".into())),
    ];
    filly_core::session::run(filly_core::widgets::tabs::TabsWidget::new("Tabs".into(), vec!["One".into(),"Two".into()], tab_widgets), &mut backend, None)?;
    // 20 split panes
    let left = filly_core::widgets::msg::MsgWidget::new("Left".into(), "LEFT PANE - press F2 to switch".into());
    let right = filly_core::widgets::msg::MsgWidget::new("Right".into(), "RIGHT PANE".into());
    filly_core::session::run(filly_core::widgets::split_panes::SplitPanesWidget::new(Orientation::Horizontal, Box::new(left), Box::new(right)), &mut backend, None)?;
    // 21 context menu
    filly_core::session::run(filly_core::widgets::context_menu::ContextMenuWidget::new(vec!["Copy".into(),"Paste".into(),"Delete".into()]), &mut backend, None)?;
    // 22 notification
    filly_core::session::run(filly_core::widgets::notification::NotificationWidget::new("Toast notification".into(), 2), &mut backend, None)?;
    // 23 radio group
    filly_core::session::run(filly_core::widgets::radio_group::RadioGroupWidget::new("Radio".into(), "Pick one:".into(), vec!["A".into(),"B".into(),"C".into()], Some(0)), &mut backend, None)?;
    // 24 range slider
    filly_core::session::run(filly_core::widgets::range_slider::RangeSliderWidget::new("Range Slider".into(), 0, 100, 50, "Volume".into()), &mut backend, None)?;
    // 25 color picker
    filly_core::session::run(filly_core::widgets::color_picker::ColorPickerWidget::new("Color Picker".into(), vec!["red".into(),"green".into(),"blue".into(),"yellow".into(),"magenta".into(),"cyan".into(),"white".into(),"black".into()]), &mut backend, None)?;
    // 26 done
    filly_core::session::run(filly_core::widgets::msg::MsgWidget::new("Done".into(), "All widgets demonstrated.\n\nFILLY is operational.".into()), &mut backend, None)?;

    backend.teardown()?;
    println!("{}", serde_json::to_string(&serde_json::json!({"result":"Demo complete","cancelled":false}))?);
    Ok(())
}