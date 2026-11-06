
use std::{
    env,
    fs,
    path::PathBuf,
    process::{Command, ExitCode},
    time::{Instant, SystemTime, UNIX_EPOCH},
};

const MAX_TEXT_UTF16_UNITS: usize = 8_000;
const MAX_OUTPUT_NAME_LENGTH: usize = 64;

struct Config {
    exe: PathBuf,
    models: PathBuf,
    speaker: PathBuf,
    output_dir: PathBuf,
    backend: &'static str,
}

struct Cli {
    text: Option<String>,
    output_name: Option<String>,
    help: bool,
}

fn read_absolute_path(name: &str) -> Result<PathBuf, String> {
    let raw = env::var_os(name)
        .ok_or_else(|| format!("Missing environment variable: {name}"))?;

    let path = PathBuf::from(raw);

    if !path.is_absolute() {
        return Err(format!("{name} must be an absolute path."));
    }

    Ok(path)
}

fn validate_backend(value: &str) -> Result<&'static str, String> {
    match value.to_ascii_lowercase().as_str() {
        "cuda" => Ok("cuda"),
        "vulkan" | "vk" => Ok("vulkan"),
        "d3d12" => Ok("d3d12"),
        _ => Err(format!(
            "Unsupported backend '{value}'. Use cuda, vulkan, or d3d12."
        )),
    }
}

fn load_config() -> Result<Config, String> {
    let exe = read_absolute_path("NUMERICK_TTS_EXE")?;
    let models = read_absolute_path("NUMERICK_TTS_MODELS")?;
    let speaker = read_absolute_path("NUMERICK_TTS_SPEAKER")?;
    let output_dir = read_absolute_path("NUMERICK_TTS_OUTPUT_DIR")?;

    if !exe.is_file() {
        return Err(format!(
            "NVIDIA executable not found: {}",
            exe.display()
        ));
    }

    if !models.is_dir() {
        return Err(format!(
            "NVIDIA models directory not found: {}",
            models.display()
        ));
    }

    if !speaker.is_file() {
        return Err(format!(
            "Voice profile not found: {}",
            speaker.display()
        ));
    }

    if fs::metadata(&speaker)
        .map_err(|e| format!("Cannot inspect voice profile: {e}"))?
        .len() == 0
    {
        return Err("Voice profile is empty.".to_string());
    }

    fs::create_dir_all(&output_dir)
        .map_err(|e| format!("Cannot create output directory: {e}"))?;

    let output_dir = output_dir
        .canonicalize()
        .map_err(|e| format!("Cannot resolve output directory: {e}"))?;

    let backend = env::var("NUMERICK_TTS_BACKEND")
        .unwrap_or_else(|_| "cuda".to_string());

    let backend = validate_backend(&backend)?;

    Ok(Config {
        exe,
        models,
        speaker,
        output_dir,
        backend,
    })
}

fn validate_output_name(name: &str) -> Result<(), String> {
    if name.is_empty() || name.len() > MAX_OUTPUT_NAME_LENGTH {
        return Err("Output name must contain 1-64 characters.".to_string());
    }

    if !name
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
    {
        return Err(
            "Output name may contain only letters, numbers, '-' and '_'."
                .to_string(),
        );
    }

    Ok(())
}

fn parse_arguments() -> Result<Cli, String> {
    let mut args = env::args().skip(1);

    let mut cli = Cli {
        text: None,
        output_name: None,
        help: false,
    };

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--help" | "-h" => cli.help = true,

            "--text" => {
                let value = args
                    .next()
                    .ok_or("--text requires a value.")?;

                if cli.text.replace(value).is_some() {
                    return Err("--text must only be supplied once.".to_string());
                }
            }

            "--output-name" => {
                let value = args
                    .next()
                    .ok_or("--output-name requires a value.")?;

                if cli.output_name.replace(value).is_some() {
                    return Err(
                        "--output-name must only be supplied once.".to_string()
                    );
                }
            }

            unknown => {
                return Err(format!("Unknown argument: {unknown}"));
            }
        }
    }

    Ok(cli)
}

fn generated_output_name() -> String {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();

    format!("speech-{timestamp}")
}

fn generate_speech(
    config: &Config,
    text: &str,
    output_name: &str,
) -> Result<PathBuf, String> {
    if text.trim().is_empty() {
        return Err("Speech text cannot be empty.".to_string());
    }

    if text.encode_utf16().count() > MAX_TEXT_UTF16_UNITS {
        return Err("Speech text exceeds the 8,000 UTF-16 unit limit.".to_string());
    }

    validate_output_name(output_name)?;

    let output_path = config
        .output_dir
        .join(format!("{output_name}.wav"));

    if output_path.exists() {
        return Err(format!(
            "Output already exists: {}. Choose another output name.",
            output_path.display()
        ));
    }

    let exe_dir = config.exe
        .parent()
        .ok_or("Cannot determine NVIDIA executable directory.")?;

    eprintln!("[voice-service] Initializing NVIDIA Qwen3-TTS...");
    eprintln!("[voice-service] Backend: {}", config.backend);

    let started = Instant::now();

    // Pass every argument separately. No shell is involved.
    let result = Command::new(&config.exe)
        .current_dir(exe_dir)
        .arg("--models")
        .arg(&config.models)
        .arg("--speaker")
        .arg(&config.speaker)
        .arg("--text")
        .arg(text)
        .arg("--output")
        .arg(&output_path)
        .arg("--backend")
        .arg(config.backend)
        .output()
        .map_err(|e| format!("Could not launch NVIDIA TTS: {e}"))?;

    if !result.stdout.is_empty() {
        eprint!("{}", String::from_utf8_lossy(&result.stdout));
    }

    if !result.stderr.is_empty() {
        eprint!("{}", String::from_utf8_lossy(&result.stderr));
    }

    if !result.status.success() {
        return Err(format!(
            "NVIDIA TTS exited with status {}.",
            result.status
        ));
    }

    let metadata = fs::metadata(&output_path)
        .map_err(|e| format!("NVIDIA did not create the WAV file: {e}"))?;

    if metadata.len() <= 44 {
        return Err("Generated WAV file is empty or invalid.".to_string());
    }

    eprintln!(
        "[voice-service] Generation completed in {:.2} seconds.",
        started.elapsed().as_secs_f64()
    );

    // stdout contains only the generated WAV path.
    println!("{}", output_path.display());

    Ok(output_path)
}

fn print_help() {
    println!(
        "NUMERICK Voice Service\n\
         \n\
         Usage:\n\
         numerick-voice-service --text \"Text to speak\" [--output-name NAME]\n\
         \n\
         Required environment variables:\n\
         NUMERICK_TTS_EXE\n\
         NUMERICK_TTS_MODELS\n\
         NUMERICK_TTS_SPEAKER\n\
         NUMERICK_TTS_OUTPUT_DIR\n\
         \n\
         Optional:\n\
         NUMERICK_TTS_BACKEND (cuda, vulkan, d3d12; default cuda)\n"
    );
}

fn main() -> ExitCode {
    let cli = match parse_arguments() {
        Ok(cli) => cli,
        Err(error) => {
            eprintln!("[voice-service] Argument error: {error}");
            return ExitCode::FAILURE;
        }
    };

    if cli.help {
        print_help();
        return ExitCode::SUCCESS;
    }

    let text = match cli.text {
        Some(text) => text,
        None => {
            eprintln!("[voice-service] Missing required --text argument.");
            print_help();
            return ExitCode::FAILURE;
        }
    };

    let output_name = cli.output_name
        .unwrap_or_else(generated_output_name);

    let config = match load_config() {
        Ok(config) => config,
        Err(error) => {
            eprintln!("[voice-service] Configuration error: {error}");
            return ExitCode::FAILURE;
        }
    };

    match generate_speech(&config, &text, &output_name) {
        Ok(_) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("[voice-service] Error: {error}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepts_safe_output_names() {
        assert!(validate_output_name("first-rust-voice").is_ok());
        assert!(validate_output_name("speech_001").is_ok());
    }

    #[test]
    fn rejects_path_traversal_names() {
        assert!(validate_output_name("../outside").is_err());
        assert!(validate_output_name("folder/file").is_err());
        assert!(validate_output_name("bad name").is_err());
    }

    #[test]
    fn accepts_supported_backends() {
        assert_eq!(validate_backend("cuda").unwrap(), "cuda");
        assert_eq!(validate_backend("vk").unwrap(), "vulkan");
        assert_eq!(validate_backend("d3d12").unwrap(), "d3d12");
    }

    #[test]
    fn rejects_unknown_backends() {
        assert!(validate_backend("unknown").is_err());
    }
}
