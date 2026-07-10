use std::collections::HashMap;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::{UnixListener, UnixStream};
use std::sync::{Arc, Mutex, mpsc};
use std::thread;
use anyhow::Result;
use serde_json::Value;
use filly_protocol::{Message, WidgetResponse};
use filly_core::{
    backend::Backend,
    session,
    store::Store,
    theme::Theme,
    widgets,
    widgets::progress,
};
use filly_terminal::TerminalBackend;
use std::path::Path;

struct SessionState {
    store: Store,
    theme: Theme,
}

pub fn run(listener: UnixListener, default_theme: Theme) -> Result<()> {
    // Auto-load plugins
    if let Some(home) = dirs::home_dir() {
        let plugin_dir = home.join(".config/filly/plugins");
        if plugin_dir.exists() {
            if let Ok(entries) = std::fs::read_dir(&plugin_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().map_or(false, |ext| ext == "so") {
                        eprintln!("Loading plugin: {:?}", path);
                        if let Err(e) = widgets::REGISTRY.lock().unwrap().load_plugin(&path) {
                            eprintln!("Failed to load plugin {:?}: {}", path, e);
                        }
                    }
                }
            }
        }
    }

    let sessions = Arc::new(Mutex::new(HashMap::<String, SessionState>::new()));

    for stream in listener.incoming() {
        let stream = stream?;
        let sessions = Arc::clone(&sessions);
        let theme = default_theme.clone();
        thread::spawn(move || {
            if let Err(e) = handle_client(stream, theme, sessions) {
                eprintln!("Client error: {}", e);
            }
        });
    }
    Ok(())
}

fn handle_client(
    stream: UnixStream,
    default_theme: Theme,
    sessions: Arc<Mutex<HashMap<String, SessionState>>>,
) -> Result<()> {
    let mut backend = TerminalBackend::new()?;
    backend.setup()?;
    let mut reader = BufReader::new(stream.try_clone()?);
    let mut line = String::new();
    let mut use_msgpack = false;

    while reader.read_line(&mut line).is_ok() {
        if line.trim().is_empty() { line.clear(); continue; }

        if line.as_bytes().first().map_or(false, |&b| b != b'{' && b != b'[') {
            use_msgpack = true;
        }

        let msg: Message = if use_msgpack {
            rmp_serde::from_slice(line.as_bytes())?
        } else {
            serde_json::from_str(&line)?
        };

        let send_response = |resp: WidgetResponse| -> Result<()> {
            let mut s = stream.try_clone()?;
            if use_msgpack {
                let bytes = rmp_serde::to_vec(&resp)?;
                s.write_all(&bytes)?;
            } else {
                let json = serde_json::to_string(&resp)?;
                s.write_all(json.as_bytes())?;
                s.write_all(b"\n")?;
            }
            s.flush()?;
            Ok(())
        };

        match msg {
            Message::Request(req) => {
                if req.widget == "quit" {
                    send_response(WidgetResponse { result: None, cancelled: false, error: None })?;
                    break;
                }

                let session_id = req.session_id.clone().unwrap_or_else(|| "default".to_string());
                let (mut store, theme) = {
                    let mut sessions = sessions.lock().unwrap();
                    let session = sessions.entry(session_id.clone()).or_insert_with(|| SessionState {
                        store: Store::new(),
                        theme: default_theme.clone(),
                    });
                    (session.store.clone(), session.theme.clone())
                };

                if req.widget == "create_session" {
                    let new_id = uuid::Uuid::new_v4().to_string();
                    sessions.lock().unwrap().insert(new_id.clone(), SessionState {
                        store: Store::new(),
                        theme: default_theme.clone(),
                    });
                    send_response(WidgetResponse { result: Some(Value::String(new_id)), cancelled: false, error: None })?;
                    line.clear();
                    continue;
                }
                if req.widget == "destroy_session" {
                    if let Some(sid) = req.params.get("session_id").and_then(|v| v.as_str()) {
                        sessions.lock().unwrap().remove(sid);
                    }
                    send_response(WidgetResponse { result: None, cancelled: false, error: None })?;
                    line.clear();
                    continue;
                }
                if req.widget == "reload_theme" {
                    if let Some(theme_name) = req.params.get("theme").and_then(|v| v.as_str()) {
                        let path = format!("themes/{}.json", theme_name);
                        if let Ok(new_theme) = Theme::load(Path::new(&path)) {
                            sessions.lock().unwrap().get_mut(&session_id).unwrap().theme = new_theme.clone();
                            send_response(WidgetResponse { result: Some(Value::String("ok".into())), cancelled: false, error: None })?;
                        } else {
                            send_response(WidgetResponse { result: None, cancelled: false, error: Some("Failed to load theme".into()) })?;
                        }
                    }
                    line.clear();
                    continue;
                }

                let (state_tx, state_rx) = mpsc::channel::<serde_json::Value>();
                {
                    let mut sessions = sessions.lock().unwrap();
                    if let Some(session) = sessions.get_mut(&session_id) {
                        let tx = state_tx.clone();
                        session.store.subscribe("*", move |key, value| {
                            let update = serde_json::json!({"key": key, "value": value});
                            let _ = tx.send(update);
                        });
                    }
                }
                let mut stream_clone = stream.try_clone()?;
                thread::spawn(move || {
                    for update in state_rx {
                        let msg = format!("state: {}\n", serde_json::to_string(&update).unwrap());
                        let _ = stream_clone.write_all(msg.as_bytes());
                        let _ = stream_clone.flush();
                    }
                });

                if req.widget == "progress" {
                    let (tx, rx) = mpsc::channel::<serde_json::Value>();
                    let title = req.params.get("title").and_then(|v| v.as_str()).unwrap_or("").to_string();
                    let command: Vec<String> = req.params.get("command").and_then(|v| v.as_array())
                        .map(|a| a.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                        .unwrap_or_default();
                    let logfile = req.params.get("logfile").and_then(|v| v.as_str().map(String::from));
                    let mut widget = progress::ProgressWidget::new(title, command, logfile, Some(tx));

                    let mut stream_clone2 = stream.try_clone()?;
                    thread::spawn(move || {
                        for value in rx {
                            let yield_msg = format!("yield: {}\n", value);
                            let _ = stream_clone2.write_all(yield_msg.as_bytes());
                            let _ = stream_clone2.flush();
                        }
                    });

                    let resp = session::run(widget, &mut backend, None)?;
                    send_response(resp)?;
                    line.clear();
                    continue;
                }

                if let Some(widget) = widgets::create_widget(&req, &store, &theme) {
                    let resp = session::run(widget, &mut backend, None).unwrap_or_else(|e| WidgetResponse {
                        result: None, cancelled: true, error: Some(format!("{}", e)),
                    });
                    {
                        let mut sessions = sessions.lock().unwrap();
                        if let Some(session) = sessions.get_mut(&session_id) {
                            session.store = store;
                        }
                    }
                    send_response(resp)?;
                } else {
                    send_response(WidgetResponse { result: None, cancelled: true, error: Some("Unknown widget".into()) })?;
                }
            }
            Message::Batch(requests) => {
                for req in requests {
                    let store = Store::new();
                    if let Some(widget) = widgets::create_widget(&req, &store, &default_theme) {
                        let resp = session::run(widget, &mut backend, None).unwrap_or_else(|e| WidgetResponse {
                            result: None, cancelled: true, error: Some(format!("{}", e)),
                        });
                        let _ = send_response(resp);
                    }
                }
            }
        }
        line.clear();
    }
    backend.teardown()?;
    Ok(())
}