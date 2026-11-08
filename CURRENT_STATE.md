# NUMERICK — Current State

**Verified snapshot:** 8 November 2026<br>
**Repository:** https://github.com/richardsanchez-c137/Numerick<br>
**Project version:** `1.2.0`<br>
**Feature milestone:** Voice Engine 2.1 and Windows application launching<br>
**Primary platform:** Windows<br>
**Core language/build:** C++17 / CMake<br>
**Status authority:** This file records observed repository, build, test and manual-run evidence. Planned capabilities are not described as implemented.

## 1. Source-control baseline

| Item | Verified state |
|---|---|
| Local branch at Phase 0 closeout synchronization | `main` |
| Remote branch | `origin/main` |
| Main commit at Phase 0 documentation closeout | `862761d514650a04c7a1f2b537fb23dbebf293ea` |
| Merge request | [PR #3 — Voice engine 2.1 and Windows application control](https://github.com/richardsanchez-c137/Numerick/pull/3) |
| PR state | Merged on 8 November 2026 |
| Phase 0 documentation PR | [PR #4 — docs: close out phase 0 project baseline](https://github.com/richardsanchez-c137/Numerick/pull/4) |
| PR #4 state | Merged on 8 November 2026 |
| PR #4 merge commit | `862761d514650a04c7a1f2b537fb23dbebf293ea` |
| PR head | `1bd57ae576b0cf4a3e26215f378515e9f3900e60` |
| Merge method/result | Merge commit `0f2d64ae5bcde2b82e52c11f524d3b3f0f3bb8d7` |
| Local synchronization | `git pull --ff-only origin main` completed successfully |
| Working tree | `git status -sb` reported `## main...origin/main` with no changes |

PR #3 merged three commits and 31 changed files, establishing the code baseline for Phase 0. PR #4 subsequently integrated the documentation closeout: eight files merged into `main` with merge commit `862761d514650a04c7a1f2b537fb23dbebf293ea`.

## 2. Implemented capabilities with current evidence

### Core and agent

- C++17 console application built with CMake.
- Local Ollama integration; the observed runtime configuration was `qwen3.5:4b` at `http://127.0.0.1:11434`.
- Tool registry with tool definitions, supported JSON-schema-style argument validation, handlers, success/failure results and exception containment.
- Agent can request registered tools and enforces configured tool-round and tool-call limits.
- Read-only workspace/project inspection tools remain available.
- Explicit local persistent facts are separate from temporary conversation history. Memory is opt-in and stored locally; it is not the complete memory/continuity system in the long-term roadmap.

### Voice pipeline

- Local Whisper speech-to-text and Silero VAD are used in continuous voice mode.
- Voice flow: microphone capture → speech endpoint detection → transcription → agent/model and registered tools → speech output → return to listening. `ESC` ends the session.
- The launcher prefers the native NVIDIA NVIGI Qwen3-TTS worker when it is built and configured.
- The Rust voice service path is retained as a fallback; Windows SAPI is the speech-output fallback.
- The native worker uses CUDA/NVIGI and local NVIDIA Qwen3-TTS model assets plus a local speaker profile.
- Native generation is currently capped at `384` new tokens (`runtime.maxNewTokens`). This is a protective, provisional setting introduced after an observed runaway generation reached 2,048 frames and produced about 163.84 seconds of audio for a short response.
- Manual regression checks after the cap passed: a short knock-knock joke and a three-sentence factual explanation both completed, returned to `Listening...`, and did not report the persistent-worker timeout or SAPI fallback.

### Windows application launch

- `open_application` is registered as an agent tool.
- Launch logic uses constrained Windows targets such as the packaged Camera target, the Spotify URI, Start Menu shortcuts and executable lookup; it is not an arbitrary shell-command interface.
- Camera, Spotify, Notepad and Blender were manually tested in the development environment.
- Matching logic has dedicated automated tests.

## 3. Build and test evidence

### Local release verification

Command run from the repository root on feature head `1bd57ae` immediately before PR merge:

```powershell
.\scripts\verify-release.ps1 -Clean
```

Observed results:

- Version consistency check passed for `1.2.0`.
- Fresh CMake configure succeeded using Ninja and MSYS2 UCRT64 G++ 16.2.0.
- All 54/54 build targets completed.
- All 11/11 registered CTest suites passed.
- The script reported: `Release verification passed: configure, build, and CTest all succeeded.`

The branch was then merged without code changes after that verified head; the local `main` was fast-forwarded to the merge commit.

### Native NVIDIA worker

The native worker was rebuilt successfully with Visual Studio 2026 MSVC x64 after changing the generation cap from 2,048 to 384. The build emitted NVIDIA-header warning `C4324` about structure padding, but the worker linked and the build completed successfully.

### GitHub CI

The Windows UCRT64 CI workflow completed successfully for PR #3. The PR page showed two successful checks (push and pull request) and no merge conflicts.

### Manual smoke tests

- Text/agent/tool smoke test: a current-time request was transcribed and answered using a tool call in a prior voice-session check.
- Continuous voice: the knock-knock joke test passed after the generation cap change.
- Continuous voice: the three-sentence explanation test passed after the generation cap change.
- Application launch: Camera, Spotify, Notepad and Blender were each previously demonstrated in the development environment.

These are targeted manual checks, not a claim of exhaustive long-session, device-loss or all-application testing.

## 4. Runtime configuration and machine assumptions

Observed development environment:

- Windows PowerShell.
- MSYS2 UCRT64, G++ 16.2.0, CMake/Ninja for the main application and tests.
- Visual Studio 2026 MSVC x64 for the native NVIDIA worker.
- Ryzen 7 7445HS, NVIDIA RTX 4050 Laptop GPU with 6 GB VRAM, 16 GB system RAM.
- Local Ollama endpoint and configured `qwen3.5:4b` model.
- Local Whisper and Silero VAD model assets.
- NVIDIA NVIGI package, Qwen3-TTS model assets and speaker profile are installed outside the repository under the user's `NumerickVoiceLab` directory. Private recordings, speaker profile, generated WAV files, model weights and diagnostic logs must remain outside Git.

The launcher sets the runtime environment, including `NUMERICK_VOICE_SERVICE_EXE`. In the verified setup it selected:

```text
build-native-voice-msvc/numerick-nvigi-voice-worker.exe
```

PowerShell initially blocked the unsigned build script under the machine's execution policy. The development session proceeded using a process-scoped bypass only:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
```

This did not change the machine/user execution policy permanently. The native build also emitted the NVIDIA header alignment warning described above.

## 5. Known limitations and open risks

- The `384` speech-generation cap is a provisional safeguard. The two manual utterances passed, but substantially longer responses need additional testing to ensure natural completion and avoid unwanted truncation.
- Voice latency, CPU/GPU/RAM utilization, repeated-turn stability and long-session reliability have not been comprehensively measured.
- Wake-word activation, reliable barge-in/interruption, configurable push-to-talk, and robust microphone-loss/noise recovery are not complete.
- `open_application` launches a tested subset of applications. General window enumeration/focus/close operations, Windows UI Automation, semantic click/type/scroll workflows and robust dialog handling are not implemented.
- Tool execution still lacks a common structured error code/result metadata contract, per-tool timeout/cancellation and a permission/confirmation model.
- Agent planning is bounded tool-call orchestration, not a general plan/act/verify task engine.
- Persistent memory is basic explicit local fact storage, not the full episodic/project/preference/task memory system. It is local plaintext, not encrypted.
- No general screen understanding, on-demand camera/document perception, source-grounded web research or proactive companion behavior is claimed as implemented.

## 6. Next work item

**Phase 1A — Standardize the tool execution contract.** Start by inspecting and documenting the existing `ToolRegistry`, tool definitions, handlers and `ToolAgent` execution path. Record current validation, result/error, exception, limits and diagnostics behavior before changing interfaces. Do not start broad Windows UI Automation until the Phase 1 permission, cancellation, reliability and Phase 3 task-verification foundations are in place.

## 7. Verification commands

```powershell
git status -sb
git log -1 --oneline --decorate
git fetch origin
git status -sb
.\scripts\verify-release.ps1 -Clean
```

Record the date, commit and complete command output whenever these checks are repeated. A successful historical run does not replace a fresh verification after future source changes.
