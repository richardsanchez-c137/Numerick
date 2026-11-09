# NUMERICK — Master Development Checklist

**Document type:** Living product roadmap and implementation ledger<br>
**Roadmap baseline:** 8 November 2026<br>
**Project direction:** A personal, voice-first AI companion for the Windows PC, with perception, memory, reasoning, computer control, research and engineering capabilities.<br>
**Implementation policy:** Choose the language per component based on reliability, development speed, maintainability and ecosystem fit. Retain the existing C++17 core; use local-first inference where practical and hybrid cloud/remote inference for workloads that exceed the laptop's limits. See DEC-009.

> **Purpose:** This document is the single roadmap reference for NUMERICK. It separates what has already been reported working from what remains planned. A feature must not be marked complete merely because code was written or because an AI model claimed success.

---

## 0. Rules for using this checklist

### Status key

- `[x]` **Previously verified** — reported implemented and tested during the previous development sessions. This is not a claim that it was re-tested today.
- `[ ]` **Not complete** — not implemented, incomplete, or not yet demonstrated.
- `[~]` **Partially complete / needs verification** — some implementation exists, but acceptance criteria are not yet proven.
- `[!]` **Blocked** — cannot proceed until the stated dependency or decision is resolved.
- `[?]` **Decision required** — a product or architecture choice must be made before implementation.

### Non-negotiable project rules

- [ ] Update this document at the end of every meaningful implementation session.
- [ ] Keep exactly one clearly named **Current Work Item** active at a time.
- [ ] Every work item must have: objective, scope, files/modules affected, dependencies, test procedure, and acceptance criteria.
- [ ] Do not mark a feature `[x]` until its acceptance criteria pass and the evidence is recorded.
- [ ] Distinguish **implemented**, **built**, **automated-test passed**, **manually demonstrated**, **committed**, **pushed**, and **merged**. These are different states.
- [ ] Never assume a PR was merged, a branch was pushed, or a test passed without current Git/test evidence or the user's confirmation.
- [ ] Inspect the current code and repository state before proposing edits. Do not invent file names, functions, APIs, or existing capabilities.
- [ ] Make small, reviewable changes. Do not replace whole files when a targeted patch is safer.
- [ ] Preserve the working baseline. Run the relevant tests after each change and the full release verification at milestone boundaries.
- [ ] Keep architecture decisions in `DECISIONS.md`; do not silently change direction mid-feature.
- [ ] Choose implementation languages per component based on concrete technical trade-offs. Document service, IPC, FFI or library boundaries and avoid rewriting stable components solely to change languages (DEC-009).
- [ ] Design for the actual laptop: Ryzen 7 7445HS, RTX 4050 Laptop GPU with 6 GB VRAM, 16 GB RAM, 1 TB SSD. Do not design around unlimited GPU memory.
- [ ] Use local models for low-latency/private tasks where feasible. Route heavier reasoning or vision tasks to an explicitly configured remote provider when needed.
- [ ] External actions must have permission boundaries, an execution log, and a way to stop or recover.

### Required work-session template

At the beginning of a session:

1. Read this checklist's **Current Work Item**, the latest `CURRENT_STATE.md`, and relevant entries in `DECISIONS.md`.
2. Check `git status`, current branch, latest commit, and the state of the relevant tests.
3. Restate the one task being worked on and its acceptance criteria.

At the end of a session:

1. Record exactly what changed and which files were affected.
2. Record the exact commands/tests run and their results.
3. Record manual checks and any limitations or failures.
4. Update checklist statuses only where evidence supports the change.
5. Record commit, push, PR and merge status separately.
6. Set the next single work item. Do not silently jump to a different roadmap phase.

---

## 1. Current baseline — what we last had working

**Baseline note:** Voice Engine 2.1 was merged by PR #3. The Phase 0 documentation closeout was integrated into `main` through PR #4 and PR #5 on 8 November 2026; the final Phase 0 handoff merge commit is `a81fc47`. Current hashes, commands and observed limits are recorded in `CURRENT_STATE.md`. Historical `[x]` labels still mean previously verified; consult the current snapshot for the exact verification date and evidence.

### 1.1 Core application and agent

- [x] C++ NUMERICK application and main entry point.
- [x] Ollama-backed local model integration used by the agent.
- [x] Tool registry with tool definitions, handlers and JSON-schema-style argument definitions.
- [x] Agent can select and invoke registered tools.
- [x] `get_local_time` tool.
- [x] Read-only project/file inspection tools.
- [~] A documented, stable contract for every tool: timeout, errors, permissions, result shape and logging.
- [ ] Centralized task/session state shared across voice, text and tools.

### 1.2 Voice pipeline

- [x] Local Whisper speech-to-text integration.
- [x] Windows SAPI fallback path in the voice workflow.
- [x] NVIDIA Qwen3-TTS integrated through the persistent native NVIGI worker.
- [~] Native `runtime.maxNewTokens` is provisionally capped at 384 after runaway output reached 2,048 frames; short and three-sentence manual regressions passed, but longer-response tuning remains open.
- [x] Rust voice-service fallback retained in the launcher path.
- [x] Continuous voice session: listen, detect utterance end, transcribe, reason/use tools, speak, and return to listening.
- [~] Measure and document end-to-end voice latency, CPU/GPU/RAM use and long-session stability.
- [ ] Reliable interruption/barge-in while NUMERICK is speaking.
- [ ] Configurable push-to-talk and hands-free modes with a clear listening indicator.
- [ ] Robust handling of microphone loss, silence, noisy input, transcription failure and TTS failure.

### 1.3 Windows application launching

- [x] `open_application` tool exposed to the agent.
- [x] Application matching and launch support for Windows Camera, Spotify, Notepad and Blender in the tested environment.
- [x] Application matcher test target added.
- [ ] Close/quit applications safely.
- [ ] Detect whether an application is already running and avoid duplicate launches where appropriate.
- [ ] Find, focus, minimize, maximize and switch windows reliably.
- [ ] Verify that an application actually opened before reporting success.

### 1.4 Build, test and repository state

- [x] CMake-based C++17 build.
- [x] `scripts/verify-release.ps1` release verification script.
- [x] Clean release verification passed on feature head `1bd57ae` immediately before PR #3 merge: version 1.2.0 consistent, 54/54 build targets and 11/11 CTest suites passed.
- [x] Windows UCRT64 GitHub Actions CI passed for both push and pull-request checks on PR #3.
- [x] Native NVIDIA voice worker rebuilt successfully using Visual Studio 2026 MSVC x64 after the 384-token safety cap change.
- [x] Manual continuous-voice regression: short knock-knock joke completed and returned to listening without timeout/fallback.
- [x] Manual continuous-voice regression: three-sentence explanation completed and returned to listening without timeout/fallback.
- [x] Camera, Spotify, Notepad and Blender launch cases manually demonstrated in the development environment.
- [x] Feature commit `1bd57ae` — `fix(voice): cap native Qwen speech generation` — pushed to `feature/voice-engine-2.1`.
- [x] PR #3 (`Voice engine 2.1 and Windows application control`) merged into `main` on 8 November 2026.
- [x] Local `main` fast-forwarded to `0f2d64ae5bcde2b82e52c11f524d3b3f0f3bb8d7`; `git status -sb` reported `## main...origin/main`.
- [~] Voice latency, peak CPU/GPU/RAM use and long-session stability still need measurement.
- [x] Create and maintain root-level `CURRENT_STATE.md`, `DECISIONS.md`, `ROADMAP.md` and `SESSION_LOG.md` (integrated into `main` through PR #4 and PR #5).
### 1.5 Known boundary of the current version

- [x] NUMERICK can hold a continuous voice interaction and invoke its current registered tools.
- [x] NUMERICK can launch a small set of Windows applications through its application tool.
- [ ] NUMERICK is **not yet** a general Windows desktop operator.
- [ ] NUMERICK does **not yet** have verified general screen understanding, click/type/scroll workflows, or robust dialog handling.
- [ ] NUMERICK does **not yet** have the complete persistent personal memory system described in the final vision.
- [ ] NUMERICK does **not yet** have the complete proactive companion behavior described in the final vision.

---

# 2. Product roadmap — from current baseline to final companion

## Phase 0 — Reconcile and protect the current baseline

**Goal:** Know exactly which code is stable before adding more capabilities.

- [x] Inspect `git status`; resolve or record all uncommitted changes.
- [x] Confirm the current branch and latest local commit (`main`, merge commit `0f2d64a`).
- [x] Confirm the feature head `1bd57ae` exists on the remote.
- [x] Confirm PR #3 for `feature/voice-engine-2.1` is merged into `main`.
- [x] Review the PR diff and checks; GitHub reported two successful Windows UCRT64 checks and no merge conflicts.
- [x] Confirm a clean Release configure/build/test run: 54/54 targets and 11/11 CTest suites passed on the PR head immediately before merge.
- [x] Confirm the merge commit is present locally and `main` is synchronized with `origin/main`.
- [x] Run manual text/agent/tool and continuous-voice smoke tests; record the specific scenarios and limits in `CURRENT_STATE.md`.
- [x] Run manual voice regressions for a short joke and three-sentence explanation after the native TTS cap change.
- [x] Confirm the four known app-launch cases: Camera, Spotify, Notepad and Blender (manual checks from the integration work).
- [x] Record machine-specific runtime assets, PowerShell execution-policy assumption, native worker build warning and remaining test gaps.
- [x] Add/maintain `CURRENT_STATE.md` with verified facts, not aspirations (integrated into `main` through PR #4 and PR #5).
- [x] Add/maintain `DECISIONS.md` for architecture choices and trade-offs (integrated into `main` through PR #4 and PR #5).
- [x] Add/maintain `ROADMAP.md` and `SESSION_LOG.md` (integrated into `main` through PR #4 and PR #5).

**Exit criteria:** Technical baseline, remote/PR/merge state and tests are known. The Phase 0 documentation set and final handoff are integrated into `main` through PR #4 and PR #5. The current work item is now Phase 1A.
## Phase 1 — Reliability, safety and tool foundations

**Goal:** Make every current and future capability predictable, observable and safe to invoke.

### 1A. Tool execution contract

- [x] Standard tool result structure: structured status, output, stable error code, safe human-readable message, internal diagnostic details and execution metadata. Implemented in PR #8.
- [x] Validate tool arguments before execution. Registry schema validation is retained and structured rejections are returned.
- [ ] Per-tool timeout and cancellation support. Status types exist, but enforcement is not implemented.
- [~] Standard handling for unavailable apps, missing files, permission failures and external-service failures. Specific mappings exist for migrated tools; complete coverage remains open.
- [x] Avoid leaking raw exceptions or internal diagnostic details into model-facing tool responses.
- [ ] Record tool name, arguments with secrets redacted, start/end time, result and failure reason in structured execution logs.
- [~] Add tests for invalid arguments, repeated calls, timeouts and tool failures. Invalid arguments and tool failures are covered; timeout and repeated-call coverage remains open.

### 1B. Permission and confirmation model

- [ ] Define permission levels: read-only, reversible action, consequential action and restricted action.
- [ ] Define which tools can run without asking, which need confirmation, and which are never allowed.
- [ ] Require confirmation before deleting, overwriting, sending, purchasing, publishing, installing or changing security settings.
- [ ] Require confirmation before executing code or scripts that affect files or the system beyond an approved workspace.
- [ ] Add a global cancel/stop mechanism.
- [ ] Make active listening, screen capture, camera capture and remote/cloud processing visible to the user.
- [ ] Store permission preferences explicitly; do not infer consent from silence.

### 1C. Reliability and diagnostics

- [ ] Structured logs with levels and timestamps.
- [ ] Rotating log files and a safe diagnostic export.
- [ ] Clear startup health checks for model server, microphone, TTS worker and required assets.
- [ ] Graceful recovery after model/service crashes.
- [ ] Avoid duplicate worker processes and orphaned child processes.
- [ ] Config validation with actionable error messages.
- [ ] Separate user configuration, secrets, runtime state and logs.
- [ ] Add regression tests for startup, shutdown and recovery.

**Exit criteria:** A tool failure is reported accurately; risky actions are permission-gated; logs explain what happened; the process can be stopped and restarted cleanly.

## Phase 2 — Complete voice-first interaction

**Goal:** Make conversation natural and dependable before adding complex autonomy.

### 2A. Voice controls

- [ ] Push-to-talk mode.
- [ ] Hands-free continuous mode.
- [ ] Clear microphone/listening/thinking/speaking states.
- [ ] Configurable voice, speaking rate, volume and response length.
- [ ] Reliable silence and end-of-utterance detection.
- [ ] Handle background noise and short/ambiguous utterances.
- [ ] Detect and recover from microphone device changes.
- [ ] Graceful fallback when Whisper or NVIDIA TTS is unavailable.

### 2B. Natural turn-taking

- [ ] Interrupt NUMERICK while it is speaking.
- [ ] Stop TTS immediately on interruption.
- [ ] Avoid processing the assistant's own speaker output as a new user command.
- [ ] Support follow-up questions without requiring the user to repeat context.
- [ ] Clarify ambiguous commands rather than guessing.
- [ ] Confirm important actions in plain language.
- [ ] Distinguish commands, questions, dictation and casual conversation.
- [ ] Support text input as a first-class alternative to voice.

### 2C. Conversation quality

- [ ] Consistent assistant identity and tone configuration.
- [ ] Conversation history with configurable retention.
- [ ] Summarize long sessions without silently dropping important decisions.
- [ ] Explicitly identify uncertainty and missing information.
- [ ] Never claim a tool action succeeded unless its result confirms success.
- [ ] Provide concise status updates during longer multi-step actions.

**Exit criteria:** A long voice session remains usable, interruption works, failures are recoverable, and the assistant does not invent action outcomes.

## Phase 3 — Cognitive core: plan, act, verify

**Goal:** Move from one-prompt/one-tool behavior to controlled multi-step task completion.

### 3A. Intent and task representation

- [ ] Normalize a request into a structured task.
- [ ] Identify the requested outcome, constraints, target applications/files and risk level.
- [ ] Separate known facts, assumptions and unknowns.
- [ ] Ask one focused clarification when a required detail is missing.
- [ ] Identify when a request is outside available tools or permissions.

### 3B. Planning and execution

- [ ] Represent a plan as explicit ordered steps.
- [ ] Track pending, running, completed, failed, skipped and awaiting-confirmation states.
- [ ] Maintain task context across tool calls.
- [ ] Set step and overall task timeouts.
- [ ] Support cancellation and safe resume where practical.
- [ ] Re-plan only when new evidence justifies it; record why.
- [ ] Prevent infinite tool-call loops and repeated failed actions.

### 3C. Verification and truthfulness

- [ ] Define an observable success condition for every action.
- [ ] Verify results using application state, filesystem state, process state or tool output.
- [ ] Distinguish “command sent” from “action completed”.
- [ ] If verification fails, report the failure instead of claiming success.
- [ ] Keep a compact action trace that can be inspected by the user.
- [ ] Add tests for hallucinated tool names, fabricated results, invalid plans and failed verification.

**Exit criteria:** NUMERICK can complete a bounded multi-step task, verify each meaningful step and explain exactly what succeeded or failed.

## Phase 4 — Persistent memory and personal context

**Goal:** Give NUMERICK continuity between sessions without turning guesses into facts.

### 4A. Memory types

- [ ] Short-term working memory for the active conversation/task.
- [ ] Episodic memory for significant sessions and completed work.
- [ ] Project memory for repositories, build commands, architecture and known issues.
- [ ] Preference memory for user-approved interaction preferences.
- [ ] Task memory for open tasks, deadlines and next actions.
- [ ] Knowledge/reference memory for explicitly saved documents and notes.

### 4B. Storage and retrieval

- [ ] Choose a durable local storage format (for example SQLite) and document the schema.
- [ ] Store source, timestamp, confidence and scope with every memory item.
- [ ] Distinguish user-confirmed facts from model-generated summaries and inferred hypotheses.
- [ ] Retrieve only memories relevant to the current task.
- [ ] Show the source or origin of important recalled project facts.
- [ ] Detect stale or conflicting memories and ask before replacing authoritative facts.
- [ ] Support export, backup, restore and migration.

### 4C. User control and privacy

- [ ] Let the user inspect saved memories.
- [ ] Let the user correct, edit and delete individual memories.
- [ ] Support “do not remember this” and temporary/private sessions.
- [ ] Never store passwords, tokens or other secrets as ordinary memory.
- [ ] Set retention rules and document what is stored locally versus sent to a remote provider.

### 4D. Project continuity guard

- [ ] Maintain `CURRENT_STATE.md` with current branch, commit, working features, failures and next task.
- [ ] Maintain `ROADMAP.md` as the product-level plan.
- [ ] Maintain `DECISIONS.md` as the architecture decision record.
- [ ] Maintain a small `SESSION_LOG.md` with dated outcomes and test evidence.
- [ ] On a new development session, read these records before suggesting changes.
- [ ] Never treat an old assistant summary as stronger evidence than current source code, logs or test results.

**Exit criteria:** NUMERICK can recall relevant approved context, explain where it came from, handle corrections and avoid confusing assumptions with confirmed facts.

## Phase 5 — Windows desktop control

**Goal:** Turn application launching into reliable, bounded desktop operation.

### 5A. Application and window management

- [x] Launch known applications through `open_application` for the previously tested cases.
- [ ] Enumerate running processes and top-level windows.
- [ ] Identify windows by application, title and stable attributes where possible.
- [ ] Focus, minimize, maximize, restore and switch windows.
- [ ] Close an application gracefully; force termination only with explicit confirmation.
- [ ] Detect unsaved-work prompts and do not dismiss them blindly.
- [ ] Verify the expected process/window state after every action.

### 5B. Windows UI Automation

- [ ] Evaluate Windows UI Automation (UIA) as the primary semantic control mechanism.
- [ ] Read accessible window/control names, roles, values and enabled states.
- [ ] Find controls by semantic properties rather than brittle screen coordinates wherever possible.
- [ ] Click buttons, select menu items, set fields and interact with common controls.
- [ ] Read dialog contents and choose safe responses.
- [ ] Handle file-open and file-save dialogs.
- [ ] Support keyboard shortcuts, typing, scrolling and clipboard operations.
- [ ] Handle elevated/admin windows as a separate permission boundary; never bypass UAC.
- [ ] Detect unsupported/custom-rendered controls and fall back safely.

### 5C. Screenshot and visual fallback

- [ ] Capture a screenshot only when required and permitted.
- [ ] Identify screen dimensions, active window and display scaling.
- [ ] Use OCR/vision to locate text or controls when UIA cannot expose them.
- [ ] Confirm target coordinates immediately before clicking.
- [ ] Never click based only on a stale screenshot after the UI has changed.
- [ ] Provide a user-visible preview/description for uncertain or consequential actions.

### 5D. Workflow automation

- [ ] Build a small approved set of repeatable workflows (open a document, save a copy, search within an app, etc.).
- [ ] Add checkpoints between consequential steps.
- [ ] Detect unexpected UI changes and stop rather than improvising destructively.
- [ ] Keep an action history with before/after verification.
- [ ] Provide undo/recovery guidance where the target application supports it.

**Exit criteria:** NUMERICK can safely perform a tested set of multi-step Windows workflows and verify the actual resulting state.

## Phase 6 — Visual, camera and document perception

**Goal:** Let NUMERICK understand relevant visual information without continuously consuming unnecessary resources.

### 6A. Screen understanding

- [ ] Capture the active window or selected region on request.
- [ ] Extract text from screenshots and preserve its screen/context relationship.
- [ ] Identify common UI elements and application state.
- [ ] Summarize charts, diagrams and tables with uncertainty where appropriate.
- [ ] Compare before/after screenshots for supported workflows.

### 6B. Camera and real-world vision

- [ ] Camera permission and visible capture indicator.
- [ ] Enumerate and select cameras.
- [ ] Capture a single image on request.
- [ ] Detect common objects using an appropriately sized local model.
- [ ] Recognize text/labels where confidence is sufficient.
- [ ] Support optional live video sampling at a configurable low rate.
- [ ] Pause capture when not needed; do not assume always-on camera access.
- [ ] Clearly distinguish visual observations from identity claims or guesses.

### 6C. Documents and media

- [ ] Read PDF and common office document content.
- [ ] Extract tables and preserve page/section references.
- [ ] Summarize long documents with source-grounded references.
- [ ] Read common image formats and screenshots.
- [ ] Handle audio files through the speech pipeline.
- [ ] Avoid claiming a scan/image is readable when extraction failed.

### 6D. Multimodal context

- [ ] Combine user prompt, selected screen, current task and permitted memory.
- [ ] Keep timestamps and source labels for captured content.
- [ ] Let the user select which context NUMERICK may inspect.
- [ ] Avoid background image/video capture unless a specific feature is enabled.

**Exit criteria:** On-demand screen, camera and document understanding works on representative test samples, reports uncertainty and respects capture permissions.

## Phase 7 — Research and connected knowledge

**Goal:** Answer questions using current evidence rather than unsupported model recall.

### 7A. Research workflow

- [ ] Add an explicit web/research provider interface.
- [ ] Search multiple relevant sources for research tasks.
- [ ] Prefer primary documentation, official sources and current references when appropriate.
- [ ] Capture title, URL, publisher and publication/update date where available.
- [ ] Separate retrieved facts, model interpretation and recommendations.
- [ ] Cite claims in the final response when information came from external sources.
- [ ] Detect stale, contradictory or weak sources.
- [ ] State when a question could not be verified.

### 7B. Local knowledge base

- [ ] Index user-selected project documentation and reference files.
- [ ] Support incremental re-indexing when files change.
- [ ] Preserve source file and location references.
- [ ] Search before answering questions about the user's own code/project history.
- [ ] Never silently replace a source document with a model-generated summary.

### 7C. Knowledge boundaries

- [ ] Respect robots, provider terms, authentication and privacy restrictions.
- [ ] Do not expose private files or data to an external provider without permission.
- [ ] Clearly label offline-model answers that were not checked against current sources.

**Exit criteria:** NUMERICK can conduct a bounded research task, provide source-backed findings and distinguish verified facts from its own reasoning.

## Phase 8 — Coding and developer companion

**Goal:** Help build, debug and maintain software while keeping the user in control of repository changes.

### 8A. Codebase understanding

- [ ] Inspect repository tree, build system, README and project instructions.
- [ ] Search symbols and references before editing.
- [ ] Understand language, framework, compiler and dependency versions.
- [ ] Read relevant source ranges instead of guessing unseen code.
- [ ] Map tests to the code they cover.

### 8B. Safe code changes

- [ ] Propose a bounded implementation plan before multi-file changes.
- [ ] Edit only approved workspace paths.
- [ ] Show a diff before applying or committing consequential changes.
- [ ] Preserve existing style and architecture.
- [ ] Avoid overwriting uncommitted user work.
- [ ] Ask before adding dependencies or running untrusted scripts.

### 8C. Build, test and diagnose

- [ ] Run configured format/lint/build/test commands with explicit approval rules.
- [ ] Capture stdout/stderr and exit codes.
- [ ] Diagnose failures from actual output, not guessed causes.
- [ ] Re-run targeted tests after fixes.
- [ ] Run full regression tests at milestone boundaries.
- [ ] Summarize exactly what passed and what remains failing.

### 8D. Git and collaboration

- [ ] Read-only Git status, log, diff and branch inspection.
- [ ] Create a branch only when requested or covered by an approved workflow.
- [ ] Stage only intended files.
- [ ] Review staged diff before commit.
- [ ] Never force-push, rewrite history or delete branches without explicit approval.
- [ ] Push and create/update PR only after approval.
- [ ] Verify remote state after push; never infer success from a command being issued.
- [ ] Merge only with explicit approval and passing required checks.

**Exit criteria:** NUMERICK can explain a codebase, prepare a safe patch, run tests and report verifiable results without silently committing or publishing changes.

## Phase 9 — Productivity and everyday PC assistance

**Goal:** Support useful everyday workflows after the control and permission layers are reliable.

- [ ] File and folder search in user-approved locations.
- [ ] Create, rename, move and organize files with confirmation for overwrites/deletes.
- [ ] Search and summarize selected documents.
- [ ] Draft emails, messages, reports and notes for user review.
- [ ] Manage local to-do items and project tasks.
- [ ] Calendar integration, if the user connects and authorizes a provider.
- [ ] Reminder and notification system with user-defined quiet hours.
- [ ] Optional email integration with draft-first behavior.
- [ ] Clipboard assistance with visible permission and history controls.
- [ ] Reusable workflows for repetitive tasks.
- [ ] User-visible activity history and cancellation controls.

**Exit criteria:** A small set of daily workflows is dependable, reviewable and permission-gated.

## Phase 10 — Engineering, science and robotics companion

**Goal:** Make NUMERICK genuinely useful for the user's technical work, not just desktop chores.

### 10A. Engineering reasoning and study support

- [ ] Explain technical concepts with stepwise reasoning and worked examples.
- [ ] Help debug C/C++ code and explain compiler/runtime errors.
- [ ] Assist with mathematics, physics, electronics and computer science problems.
- [ ] Show formulas, units, assumptions and calculation steps.
- [ ] Use executable calculations or scripts to verify numerical results where suitable.
- [ ] Distinguish an estimate from a measured or sourced result.

### 10B. Technical tools

- [ ] Safe calculator and unit conversion tools.
- [ ] Python execution in a restricted workspace for data analysis and plotting.
- [ ] CSV/Excel inspection and transformation with backups.
- [ ] Image analysis for diagrams, components and project photos.
- [ ] Read datasheets and extract pinouts/specifications with source references.
- [ ] Generate and validate simple circuit calculations.
- [ ] Support Git-based software project workflows.

### 10C. Hardware and robotics integration

- [ ] Serial-port discovery and explicit device selection.
- [ ] Read telemetry from approved microcontrollers/sensors.
- [ ] Parse common sensor formats and timestamp measurements.
- [ ] Visualize live and historical measurements.
- [ ] Draft firmware and explain required wiring/pin assumptions.
- [ ] Flash firmware only after explicit confirmation and target verification.
- [ ] Control motors/actuators only with a safe, bounded command interface.
- [ ] Add emergency stop, physical interlock expectations and safe default states.
- [ ] Keep simulation/testing separate from commands sent to real hardware.

**Exit criteria:** NUMERICK can support a complete small engineering workflow from understanding documentation to analysis, testing and (with explicit authorization) interaction with selected hardware.

## Phase 11 — Proactive companion behavior

**Goal:** Move from reactive command execution to useful, non-intrusive assistance.

### 11A. Awareness based on permitted signals

- [ ] Track explicitly selected active projects and current tasks.
- [ ] Detect stale or unfinished tasks from saved task state, not speculation.
- [ ] Monitor only user-approved folders, repositories or events.
- [ ] Support user-defined schedules and quiet hours.
- [ ] Show what signal triggered a proactive suggestion.

### 11B. Proactive assistance

- [ ] Remind about a task at a user-approved time.
- [ ] Notify when an approved long-running task or build completes.
- [ ] Detect a watched repository change or CI result when configured.
- [ ] Suggest a next step based on recorded project state.
- [ ] Prepare a draft plan or draft file for review.
- [ ] Avoid repeated, low-value or interruptive notifications.
- [ ] Let the user snooze, dismiss, disable or tune each proactive behavior.

### 11C. Companion experience

- [ ] Consistent conversational personality without pretending to have human feelings or experiences.
- [ ] Natural conversation when no task is active.
- [ ] Remember user-approved preferences and ongoing goals.
- [ ] Recognize when the user is focused and avoid unnecessary interruption when possible.
- [ ] Maintain continuity across sessions with a clear explanation of remembered context.
- [ ] Never take consequential action merely because NUMERICK predicts the user would want it.

**Exit criteria:** Proactive behavior is opt-in, explainable, useful and easy to disable. The user remains in control.

## Phase 12 — Interface and user experience

**Goal:** Make the system understandable and controllable as its capabilities grow.

- [ ] Clean console/status interface for development and diagnostics.
- [ ] Tray/background mode with explicit active/inactive state.
- [ ] Push-to-talk control and visible microphone status.
- [ ] Visual state for listening, transcribing, reasoning, using a tool and speaking.
- [ ] Current task/plan view with step status.
- [ ] Tool/action history with result and verification state.
- [ ] Permission prompt UI with clear action details.
- [ ] Settings for models, providers, voice, privacy, memory and notifications.
- [ ] Memory inspection/edit/delete UI.
- [ ] Model/resource status (GPU/CPU/RAM) and fallback indicator.
- [ ] Easy pause, stop, mute and exit controls.
- [ ] Accessible keyboard navigation and readable error messages.

**Exit criteria:** A user can see what NUMERICK is doing, what it can access, and stop or correct it without editing configuration files.

## Phase 13 — Performance, deployment and maintenance

**Goal:** Make NUMERICK dependable on the target laptop and easy to install, update and recover.

### 13A. Resource management

- [ ] Measure model load time, first-token latency, tokens/second and peak RAM/VRAM.
- [ ] Measure end-to-end speech latency and long-session resource use.
- [ ] Avoid loading large speech, language and vision models simultaneously unless capacity is confirmed.
- [ ] Add model/resource scheduling and unload idle models where practical.
- [ ] Add CPU fallback for supported workloads.
- [ ] Support configurable local model sizes and quantization.
- [ ] Add remote inference fallback for selected heavy workloads.
- [ ] Show when data will leave the machine before using a remote provider.
- [ ] Define a minimum supported hardware profile and a recommended profile.

### 13B. Release engineering

- [ ] Reproducible clean build from documented steps.
- [ ] Automated test suite runs on every PR.
- [ ] Release verification checks version consistency, build and tests.
- [ ] Package binaries, runtime dependencies and model/asset setup instructions.
- [ ] First-run setup and health check.
- [ ] Configuration migration between versions.
- [ ] Backup and restore of configuration and memory.
- [ ] Safe update process with rollback plan.
- [ ] Crash logs and diagnostic bundle with secrets redacted.
- [ ] Document supported Windows versions and known limitations.

**Exit criteria:** A clean installation can be configured, run, updated and recovered without relying on undocumented manual steps.

## Phase 14 — Final integration, security review and acceptance

**Goal:** Demonstrate that NUMERICK behaves like a reliable personal AI companion across complete workflows.

### 14A. Security and privacy review

- [ ] Review every tool for command injection, path traversal and unsafe argument handling.
- [ ] Review file-system boundaries and workspace restrictions.
- [ ] Review secret storage and log redaction.
- [ ] Review camera, microphone, screen capture and remote-provider permissions.
- [ ] Review UI automation against accidental destructive clicks.
- [ ] Review downloaded content and prompt-injection risks from web pages/documents.
- [ ] Review privilege boundaries and ensure no UAC/security bypass.
- [ ] Review memory access, deletion and data export behavior.
- [ ] Test cancellation and emergency stop during active tasks.

### 14B. End-to-end acceptance scenarios

- [ ] **Voice conversation:** Start a session, converse naturally, interrupt speech, ask a follow-up and end the session cleanly.
- [ ] **App launch and control:** Open an app, identify its window, perform a harmless UI action, verify the result and close it safely.
- [ ] **Document understanding:** Open a selected document, answer questions with page/section references and disclose extraction failures.
- [ ] **Screen assistance:** Inspect a user-selected window and explain what is visibly present without inventing hidden context.
- [ ] **Research:** Research a current question, compare credible sources, cite claims and state unresolved uncertainty.
- [ ] **Coding task:** Inspect a repository, propose a patch, apply it in the approved scope, run tests and show the diff without publishing it automatically.
- [ ] **Engineering task:** Read a selected datasheet, explain a circuit or component, perform a verified calculation and state assumptions.
- [ ] **Memory continuity:** Remember an approved project decision, recall its source later and accept a correction.
- [ ] **Proactive reminder:** Deliver a user-configured reminder, respect quiet hours and allow dismissal/disable.
- [ ] **Failure recovery:** Simulate a missing model/service, tool timeout and application failure; provide truthful failure messages and recover cleanly.
- [ ] **Privacy boundary:** Confirm that camera/microphone/screen capture and cloud processing are not used outside their configured permission state.
- [ ] **Resource boundary:** Run a representative workload on the target laptop without uncontrolled memory growth or system instability.

### 14C. Final release gate

- [ ] All required acceptance scenarios pass.
- [ ] No known critical security or data-loss issue remains open.
- [ ] Full automated test suite passes from a clean checkout.
- [ ] Release verification passes and logs are retained.
- [ ] Documentation matches the shipped behavior.
- [ ] Known limitations and hardware requirements are documented honestly.
- [ ] Version, tag, release notes and rollback path are prepared.
- [ ] User explicitly approves the release.

**Final definition of done:** NUMERICK is a dependable, permission-aware personal AI companion that can converse, remember approved context, perceive selected inputs, research with sources, operate a tested set of desktop workflows, assist with coding and engineering, and proactively help within boundaries the user controls.

---

# 3. Recommended implementation order

This is the intended order. A dependency may justify a small adjustment, but changing the order must be recorded in `DECISIONS.md`.

1. **Reconcile baseline** — Phase 0.
2. **Reliability and permission foundations** — Phase 1.
3. **Finish voice interaction and truthful task execution** — Phases 2–3.
4. **Add memory and project continuity** — Phase 4.
5. **Build Windows UI Automation incrementally** — Phase 5.
6. **Add on-demand screen, camera and document perception** — Phase 6.
7. **Add source-grounded research and coding workflows** — Phases 7–8.
8. **Add everyday productivity and engineering integrations** — Phases 9–10.
9. **Add opt-in proactive companion behavior** — Phase 11.
10. **Polish interface, performance and deployment** — Phases 12–13.
11. **Run security review and end-to-end acceptance** — Phase 14.

Do not start continuous visual surveillance, unrestricted computer control or autonomous consequential actions before the permission, task-verification and emergency-stop foundations are in place.

---

# 4. Current work tracking panel

> Update this panel whenever the active task changes. There must be only one active work item.

- **Current phase:** Phase 1 — Reliability, safety and tool foundations.
- **Current work item:** Phase 1A — Step 6: Regression tests.
- **Status:** Steps 1–4 are merged into `main`. Step 5 is implemented, built, tested, committed (`8d3a206`) and pushed to the feature branch; it is not yet merged. Step 6 is the only active implementation item. Step 7 remains open.
- **Implementation branch:** `feat/phase-1a-diagnostics-compatibility`.
- **Base commit:** `20aeabc` — PR #9 merge into `main`.
- **Step 5 implementation commit:** `8d3a206` — `feat: add structured tool execution diagnostics` (pushed; not yet PR-merged).
- **Expected deliverable:** Focused regression coverage for all five statuses, counter equations, early returns, diagnostic metadata, conversation compatibility and successful output preservation.
- **Files/modules in scope:** `tests/tool_agent_tests.cpp` and `tests/tool_agent_conversation_tests.cpp`.
- **Dependencies:** Step 5 implementation commit `8d3a206` is pushed; the approved `PHASE1A_TOOLRESULT_CONTRACT.md` remains authoritative.
- **Test procedure:** Run focused ToolAgent and conversation tests after changes, followed by full CTest verification.
- **Acceptance evidence:** Tests verify diagnostic status, error code, optional duration, handler-invocation flag and safe error message; agent-level rejections retain unavailable duration and `handlerInvoked = false`; all five statuses, counter equations and early-return accounting remain correct; conversation compatibility and successful textual outputs are preserved.
- **Guardrails:** Do not claim timeout or cancellation enforcement. Do not introduce permission prompts or unrelated logging infrastructure. Keep `LegacyToolResult` and `executeLegacy()` until all remaining consumers are migrated.

## Phase 1A work sequence

- [x] Inspect and document the existing tool execution architecture in `PHASE1A_TOOL_EXECUTION_AUDIT.md` (merged through PR #6).
- [x] Design and approve the standard `ToolResult` contract in `PHASE1A_TOOLRESULT_CONTRACT.md`.
- [x] Define the incremental implementation sequence and test requirements in the approved contract.
- [x] **Step 1 – Core result type:** Implement statuses, error codes, private-state result class, factories, name conversion and focused tests. Commit `c510abe`.
- [x] **Step 2 – Tool registry:** Return structured outcomes, attach registry-owned duration and handler-invocation metadata, and distinguish validation, unknown-tool and execution failures. Commit `ccee489`.
- [x] **Step 3 – Existing tool migration:** Migrate built-in, application and all seven project-file handlers. Commits `94c1033` and `ce721a2`.
- [x] **Step 4 – ToolAgent integration:** Migrate deterministic and model-directed execution paths; classify all five statuses; implement derived processed/not-executed accounting and early-return handling. Merged through PR #9 (`29a9a0a`).
- [x] **Step 5 – Diagnostics and conversation compatibility:** Add typed per-tool diagnostic metadata, update console and activity-log reporting, and preserve conversation privacy and Ollama compatibility. Implemented and verified locally; commit `8d3a206` pushed to `feat/phase-1a-diagnostics-compatibility` (not yet PR-merged).
- [ ] **Step 6 – Regression tests (CURRENT ACTIVE ITEM):** Add explicit assertions for diagnostic metadata, all five statuses, counter equations, early returns, agent-level rejection metadata, conversation compatibility and successful output preservation.
- [ ] **Step 7 – Integration verification:** Build, run complete CTest, perform appropriate manual checks and update project records with verified evidence.

---
# 5. Supporting project records

Keep these files at the repository root (or update the equivalent existing documents if they already exist):

- `ROADMAP.md` — short product vision, phase order and milestone summary; link to this master checklist.
- `NUMERICK_MASTER_CHECKLIST.md` — this detailed feature and acceptance checklist.
- `CURRENT_STATE.md` — verified current implementation, current branch/commit, tests, known failures and next task.
- `DECISIONS.md` — dated architecture/product decisions with rationale and consequences.
- `SESSION_LOG.md` — short dated session records containing changed files, commands, test evidence and Git status.

**Authority rule:** Source code and reproducible test output establish what is implemented. `CURRENT_STATE.md` records the latest verified snapshot. This checklist records intended scope and progress. Conversation summaries are helpful context, but they do not override current repository evidence.
