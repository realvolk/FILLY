use std::io::Write;
use base64::{Engine as _, engine::general_purpose::STANDARD as BASE64};

pub fn paste_from_clipboard(_writer: &mut impl Write) -> Option<String> {
    if let Ok(output) = std::process::Command::new("wl-paste")
        .arg("-t").arg("text/plain").output()
    {
        if output.status.success() {
            return Some(String::from_utf8_lossy(&output.stdout).into_owned());
        }
    }
    if let Ok(output) = std::process::Command::new("xclip")
        .args(&["-selection", "clipboard", "-o"]).output()
    {
        if output.status.success() {
            return Some(String::from_utf8_lossy(&output.stdout).into_owned());
        }
    }
    if let Ok(output) = std::process::Command::new("pbpaste").output() {
        if output.status.success() {
            return Some(String::from_utf8_lossy(&output.stdout).into_owned());
        }
    }
    None
}

pub fn copy_to_clipboard<W: Write>(writer: &mut W, text: &str) -> bool {
    let encoded = BASE64.encode(text);
    write!(writer, "\x1b]52;c;{}\x1b\\", encoded).is_ok()
}

pub fn copy_external(text: &str) -> bool {
    use std::process::{Command, Stdio};
    let mut cmd = if cfg!(target_os = "linux") {
        let mut c = Command::new("wl-copy");
        c.stdin(Stdio::piped());
        c
    } else if cfg!(target_os = "macos") {
        let mut c = Command::new("pbcopy");
        c.stdin(Stdio::piped());
        c
    } else {
        return false;
    };
    if let Ok(mut child) = cmd.spawn() {
        if let Some(stdin) = child.stdin.as_mut() {
            let _ = stdin.write_all(text.as_bytes());
        }
        child.wait().is_ok()
    } else {
        false
    }
}