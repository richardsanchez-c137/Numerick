# NUMERICK — Architecture and Product Decisions

**Decision log reconciled:** 8 November 2026<br>
**Rule:** This file records decisions that are currently visible in the implementation or explicitly adopted in the roadmap. It does not imply that every decision has been fully implemented or validated on all machines.

## DEC-001 — Keep a modular C++17 core

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** Keep the main application, tool orchestration and Windows integration in a modular C++17 codebase built with CMake.
- **Rationale:** This is the established repository architecture and provides a direct path to Windows-native APIs.
- **Consequences:** New system integrations should prefer C++ unless a concrete technical boundary justifies another language. Cross-language boundaries must remain narrow and documented.

## DEC-002 — Prefer local inference, retain explicit fallbacks

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** Use the configured local Ollama model for assistant reasoning, local Whisper for speech recognition, the native NVIDIA NVIGI worker as the preferred custom TTS path, Rust voice-service fallback where configured, and Windows SAPI as the speech-output fallback.
- **Rationale:** Keep common interaction local and usable while preserving fallback paths for optional runtime dependencies.
- **Consequences:** The launcher and diagnostics must report which engine is selected; a configured path is not proof that a worker is healthy until it starts and responds.

## DEC-003 — Keep private speech assets outside Git

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** Store reference recordings/transcripts, speaker profiles, NVIDIA model weights, generated WAVs and voice diagnostics outside the repository.
- **Rationale:** These are private user data and large machine-specific runtime assets.
- **Consequences:** Installation/build documentation must describe expected locations without committing the assets. Logs and diagnostic bundles must avoid exposing private content or secrets.

## DEC-004 — Keep persistent memory explicit and separate from chat history

- **Date recorded:** 2026-11-08
- **Status:** Adopted; broader memory architecture remains planned
- **Decision:** Only explicitly saved facts enter the current local persistent-memory store. Temporary conversation history remains a separate session concern.
- **Rationale:** Do not turn ordinary conversation or model-generated assumptions into permanent user facts.
- **Consequences:** The present JSON memory store is a basic foundation, not completion of Phase 4. Future memory needs source, timestamp, confidence, scope and user inspection/correction/deletion.

## DEC-005 — Bound native Qwen speech generation

- **Date recorded:** 2026-11-08
- **Status:** Provisional safeguard in production code
- **Decision:** Set native `runtime.maxNewTokens` to `384` rather than `2048`.
- **Rationale:** A short joke response previously reached 2,048 generated frames, with NVIDIA reporting about 163.84 seconds of audio. Synchronous playback could exceed the C++ client's 185-second response timeout. The cap prevented the reproduced timeout in two manual regression tests.
- **Consequences:** The cap is a safety bound, not a tuned quality optimum. Test longer natural responses and watch for truncation before treating the value as final. Do not increase the IPC timeout as a substitute for correcting runaway generation.

## DEC-006 — Keep application launch bounded; defer general desktop control

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** Expose a constrained `open_application` tool for application targets resolved through the Windows packaged-app target, URI, Start Menu shortcuts or executable lookup. Do not provide arbitrary shell-command execution through this tool.
- **Rationale:** App launch is a narrow, testable first Windows action; arbitrary shell execution would create a much larger safety boundary.
- **Consequences:** Launching Camera, Spotify, Notepad and Blender is not equivalent to general desktop operation. Window enumeration, focus/close, UIA controls and dialog handling remain future work.

## DEC-007 — Finish safety foundations before broad UI Automation

- **Date recorded:** 2026-11-08
- **Status:** Roadmap constraint
- **Decision:** Complete Phase 1 reliability/permission foundations and Phase 3 task verification before broad multi-step desktop workflows or consequential autonomous actions.
- **Rationale:** Desktop controls can change application state and may trigger irreversible operations; actions need explicit risk classification, cancellation, observable outcomes and recovery.
- **Consequences:** Phase 1A is the next implementation task. Windows UI Automation is not the immediate next milestone.

## DEC-008 — Treat source and test evidence as authoritative

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** Distinguish implemented, built, automated-test passed, manually demonstrated, committed, pushed, PR-created and merged states.
- **Rationale:** These are separate facts and must not be inferred from one another.
- **Consequences:** `CURRENT_STATE.md` is a verified snapshot; `SESSION_LOG.md` is evidence of work sessions; the master checklist records intended scope. Update all records when new evidence changes the state.

## DEC-009 — Choose implementation languages per component

- **Date recorded:** 2026-11-08
- **Status:** Adopted
- **Decision:** NUMERICK may use any suitable programming language when it materially improves development speed, reliability, maintainability or access to a strong ecosystem. C++ remains the existing application core, not a mandatory language for every future component. Python is an option for AI/ML, prototyping, automation and orchestration. Rust remains appropriate for isolated services and workers where its safety and concurrency properties justify the boundary. Other languages may be considered case by case.
- **Rationale:** A language restriction should not slow development or force a poor technical fit. Different components have different performance, ecosystem, integration and maintenance needs.
- **Consequences:** Choose languages per component and record the reasoning. Evaluate packaging, dependency management, runtime overhead, security, debugging, resource use and interoperability. Keep cross-language boundaries narrow and documented through IPC, service protocols, FFI or library interfaces. Do not rewrite stable components solely to change languages; new components must remain testable, observable and maintainable.
## Pending decisions

- Tool result/error schema and execution metadata contract (Phase 1A).
- Tool permission levels and confirmation policy (Phase 1B).
- Logging format, retention and safe diagnostic export (Phase 1C).
- Barge-in and push-to-talk versus hands-free voice interaction design (Phase 2).
- Durable multi-category memory schema and privacy controls (Phase 4).
