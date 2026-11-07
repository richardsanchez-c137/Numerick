use super::Config;

use std::{
    io::{self, BufRead, BufReader, Write},
    process::{Child, ChildStdin, Command, ExitStatus, Stdio},
    sync::mpsc::{self, Receiver, RecvTimeoutError},
    thread,
    time::{Duration, Instant},
};

const MAX_FRAME_BYTES: usize = 32_000;
const MAX_TEXT_UTF16_UNITS: usize = 8_000;

const STARTUP_TIMEOUT: Duration = Duration::from_secs(90);
const REQUEST_TIMEOUT: Duration = Duration::from_secs(180);
const SHUTDOWN_TIMEOUT: Duration = Duration::from_secs(15);

const STARTUP_MARKER: &str = "=== Interactive TTS Mode ===";
const COMPLETION_MARKER: &str = "Streaming playback complete.";

enum NvidiaEvent {
    Line(String),
    Closed,
}

// Protocol format:
// <UTF-8 byte length>\n
// <exactly that many UTF-8 bytes>\n
//
// Length-prefixing allows requests to contain arbitrary newlines.
fn read_frame<R: BufRead>(reader: &mut R) -> Result<Option<String>, String> {
    let mut header = String::new();

    let bytes_read = reader
        .read_line(&mut header)
        .map_err(|e| format!("Protocol header read failed: {e}"))?;

    if bytes_read == 0 {
        return Ok(None);
    }

    if !header.ends_with('\n') {
        return Err("Incomplete protocol header.".to_string());
    }

    let header = header.trim_end_matches(&['\r', '\n'][..]);

    if header.is_empty() || !header.bytes().all(|b| b.is_ascii_digit()) {
        return Err("Invalid protocol frame length.".to_string());
    }

    let length: usize = header
        .parse()
        .map_err(|_| "Protocol frame length is invalid.".to_string())?;

    if length > MAX_FRAME_BYTES {
        return Err("Protocol frame exceeds the 32 KB limit.".to_string());
    }

    let mut payload = vec![0u8; length];

    reader
        .read_exact(&mut payload)
        .map_err(|e| format!("Protocol payload read failed: {e}"))?;

    let mut terminator = [0u8; 1];

    reader
        .read_exact(&mut terminator)
        .map_err(|e| format!("Protocol terminator read failed: {e}"))?;

    if terminator[0] != b'\n' {
        return Err("Invalid protocol frame terminator.".to_string());
    }

    String::from_utf8(payload)
        .map(Some)
        .map_err(|e| format!("Protocol payload is not valid UTF-8: {e}"))
}

fn write_frame<W: Write>(writer: &mut W, payload: &str) -> io::Result<()> {
    let bytes = payload.as_bytes();

    write!(writer, "{}\n", bytes.len())?;
    writer.write_all(bytes)?;
    writer.write_all(b"\n")?;
    writer.flush()
}

fn validate_text(text: &str) -> Result<(), String> {
    if text.trim().is_empty() {
        return Err("Speech text cannot be empty.".to_string());
    }

    if text.encode_utf16().count() > MAX_TEXT_UTF16_UNITS {
        return Err("Speech text exceeds the 8,000 UTF-16 unit limit.".to_string());
    }

    Ok(())
}

fn normalize_interactive_text(text: &str) -> String {
    let mut result = String::with_capacity(text.len());
    let mut pending_space = false;

    for character in text.chars() {
        if character.is_whitespace() {
            pending_space = !result.is_empty();
        } else {
            if pending_space {
                result.push(' ');
                pending_space = false;
            }

            result.push(character);
        }
    }

    result
}

fn start_nvidia(config: &Config) -> Result<(Child, ChildStdin, Receiver<NvidiaEvent>), String> {
    let exe_dir = config
        .exe
        .parent()
        .ok_or("Cannot determine NVIDIA executable directory.")?;

    eprintln!("[voice-service] Starting persistent NVIDIA runtime...");
    eprintln!("[voice-service] Backend: {}", config.backend);

    let mut child = Command::new(&config.exe)
        .current_dir(exe_dir)
        .arg("--models")
        .arg(&config.models)
        .arg("--speaker")
        .arg(&config.speaker)
        .arg("--interactive")
        .arg("--backend")
        .arg(config.backend)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Could not launch NVIDIA runtime: {e}"))?;

    let nvidia_input = child
        .stdin
        .take()
        .ok_or("Could not access NVIDIA standard input.")?;

    let nvidia_stdout = child
        .stdout
        .take()
        .ok_or("Could not access NVIDIA standard output.")?;

    let nvidia_stderr = child
        .stderr
        .take()
        .ok_or("Could not access NVIDIA standard error.")?;

    let (sender, receiver) = mpsc::channel();

    // Continuously drain NVIDIA stdout and forward complete lines.
    let stdout_sender = sender.clone();

    thread::spawn(move || {
        let reader = BufReader::new(nvidia_stdout);

        for result in reader.lines() {
            match result {
                Ok(line) => {
                    if stdout_sender.send(NvidiaEvent::Line(line)).is_err() {
                        return;
                    }
                }
                Err(error) => {
                    eprintln!("[nvidia stdout] Read error: {error}");
                    break;
                }
            }
        }

        let _ = stdout_sender.send(NvidiaEvent::Closed);
    });

    // Drain stderr independently. Otherwise NVIDIA could block
    // if its diagnostic pipe becomes full.
    thread::spawn(move || {
        let reader = BufReader::new(nvidia_stderr);

        for result in reader.lines() {
            match result {
                Ok(line) => eprintln!("[nvidia] {line}"),
                Err(error) => {
                    eprintln!("[nvidia stderr] Read error: {error}");
                    break;
                }
            }
        }
    });

    if let Err(error) = wait_for_marker(&mut child, &receiver, STARTUP_MARKER, STARTUP_TIMEOUT) {
        terminate_nvidia(&mut child);
        return Err(error);
    }

    eprintln!("[voice-service] NVIDIA interactive runtime is ready.");

    Ok((child, nvidia_input, receiver))
}

fn wait_for_marker(
    child: &mut Child,
    receiver: &Receiver<NvidiaEvent>,
    marker: &str,
    timeout: Duration,
) -> Result<(), String> {
    let deadline = Instant::now() + timeout;

    loop {
        if let Some(status) = child
            .try_wait()
            .map_err(|e| format!("Could not inspect NVIDIA process: {e}"))?
        {
            return Err(format!(
                "NVIDIA exited before emitting '{marker}'. Exit status: {status}"
            ));
        }

        let remaining = deadline.saturating_duration_since(Instant::now());

        if remaining.is_zero() {
            return Err(format!("Timed out waiting for NVIDIA event: {marker}"));
        }

        let wait_slice = remaining.min(Duration::from_millis(250));

        match receiver.recv_timeout(wait_slice) {
            Ok(NvidiaEvent::Line(line)) => {
                if line.trim() == marker {
                    return Ok(());
                }

                eprintln!("[nvidia stdout] {line}");
            }

            Ok(NvidiaEvent::Closed) => {
                return Err("NVIDIA standard output closed unexpectedly.".to_string());
            }

            Err(RecvTimeoutError::Timeout) => continue,

            Err(RecvTimeoutError::Disconnected) => {
                return Err("NVIDIA output event channel disconnected.".to_string());
            }
        }
    }
}

fn wait_for_child(child: &mut Child, timeout: Duration) -> Result<ExitStatus, String> {
    let deadline = Instant::now() + timeout;

    loop {
        if let Some(status) = child
            .try_wait()
            .map_err(|e| format!("Could not inspect NVIDIA shutdown: {e}"))?
        {
            return Ok(status);
        }

        if Instant::now() >= deadline {
            let _ = child.kill();

            return child
                .wait()
                .map_err(|e| format!("Could not reap NVIDIA process: {e}"));
        }

        thread::sleep(Duration::from_millis(100));
    }
}

fn terminate_nvidia(child: &mut Child) {
    let _ = child.kill();
    let _ = child.wait();
}

fn shutdown_nvidia(child: &mut Child, mut input: ChildStdin) -> Result<(), String> {
    let _ = input.write_all(b"exit\n");
    let _ = input.flush();

    drop(input);

    let status = wait_for_child(child, SHUTDOWN_TIMEOUT)?;

    if !status.success() {
        return Err(format!(
            "NVIDIA exited unsuccessfully during shutdown: {status}"
        ));
    }

    eprintln!("[voice-service] NVIDIA runtime shut down cleanly.");

    Ok(())
}

pub fn run(config: &Config) -> Result<(), String> {
    let stdout = io::stdout();
    let stdin = io::stdin();

    let mut protocol_out = stdout.lock();
    let mut protocol_in = BufReader::new(stdin.lock());

    let (mut child, mut nvidia_input, receiver) = match start_nvidia(config) {
        Ok(runtime) => runtime,
        Err(error) => {
            write_frame(&mut protocol_out, &format!("ERR\n{error}"))
                .map_err(|e| format!("Could not report startup error: {e}"))?;

            return Err(error);
        }
    };

    // stdout of the Rust worker is reserved exclusively for protocol frames.
    write_frame(&mut protocol_out, "READY")
        .map_err(|e| format!("Could not send readiness response: {e}"))?;

    eprintln!("[voice-service] Persistent request loop started.");

    loop {
        let request = match read_frame(&mut protocol_in) {
            Ok(Some(request)) => request,

            Ok(None) => {
                eprintln!("[voice-service] Request pipe closed.");
                break;
            }

            Err(error) => {
                let _ = write_frame(&mut protocol_out, &format!("ERR\n{error}"));

                terminate_nvidia(&mut child);
                return Err(error);
            }
        };

        if request == "SHUTDOWN" {
            write_frame(&mut protocol_out, "OK")
                .map_err(|e| format!("Could not acknowledge shutdown: {e}"))?;

            return shutdown_nvidia(&mut child, nvidia_input);
        }

        let text = match request.strip_prefix("SPEAK\n") {
            Some(text) => text,
            None => {
                write_frame(&mut protocol_out, "ERR\nUnknown request type.")
                    .map_err(|e| format!("Protocol write failed: {e}"))?;

                continue;
            }
        };

        let spoken_text = normalize_interactive_text(text);

        if let Err(error) = validate_text(&spoken_text) {
            write_frame(&mut protocol_out, &format!("ERR\n{error}"))
                .map_err(|e| format!("Protocol write failed: {e}"))?;

            continue;
        }

        eprintln!(
            "[voice-service] Sending {} UTF-16 units to NVIDIA.",
            spoken_text.encode_utf16().count()
        );

        // Send the normalized request exactly once.
        nvidia_input
            .write_all(spoken_text.as_bytes())
            .and_then(|_| nvidia_input.write_all(b"\n"))
            .and_then(|_| nvidia_input.flush())
            .map_err(|e| format!("Could not send text to NVIDIA: {e}"))?;

        match wait_for_marker(&mut child, &receiver, COMPLETION_MARKER, REQUEST_TIMEOUT) {
            Ok(()) => {
                write_frame(&mut protocol_out, "OK")
                    .map_err(|e| format!("Protocol response failed: {e}"))?;
            }

            Err(error) => {
                let _ = write_frame(&mut protocol_out, &format!("ERR\n{error}"));

                terminate_nvidia(&mut child);

                return Err(error);
            }
        }
    }

    shutdown_nvidia(&mut child, nvidia_input)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn collapses_multiline_speech_text() {
        assert_eq!(
            normalize_interactive_text("Hello NUMERICK.\nHow are you?\r\nI am doing well."),
            "Hello NUMERICK. How are you? I am doing well."
        );
    }

    #[test]
    fn removes_excess_whitespace() {
        assert_eq!(
            normalize_interactive_text("   Hello    NUMERICK   "),
            "Hello NUMERICK"
        );
    }

    #[test]
    fn protocol_round_trips_unicode_and_newlines() {
        let original = "SPEAK\nHello, NUMERICK! 🌟\nSecond line.";

        let mut encoded = Vec::new();

        write_frame(&mut encoded, original).unwrap();

        let decoded = read_frame(&mut Cursor::new(encoded)).unwrap();

        assert_eq!(decoded, Some(original.to_string()));
    }

    #[test]
    fn protocol_accepts_empty_frame() {
        let decoded = read_frame(&mut Cursor::new(b"0\n\n")).unwrap();

        assert_eq!(decoded, Some(String::new()));
    }

    #[test]
    fn protocol_rejects_oversized_frame() {
        let header = format!("{}\n", MAX_FRAME_BYTES + 1);

        assert!(read_frame(&mut Cursor::new(header.into_bytes())).is_err());
    }

    #[test]
    fn protocol_rejects_non_numeric_length() {
        assert!(read_frame(&mut Cursor::new(b"hello\n".to_vec())).is_err());
    }

    #[test]
    fn protocol_rejects_invalid_terminator() {
        assert!(read_frame(&mut Cursor::new(b"3\nabc!".to_vec())).is_err());
    }

    #[test]
    fn accepts_valid_speech_text() {
        assert!(validate_text("Hello NUMERICK").is_ok());
    }

    #[test]
    fn rejects_empty_speech_text() {
        assert!(validate_text("   ").is_err());
    }
}
