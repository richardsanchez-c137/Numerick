# NUMERICK — Product Roadmap

**Last reconciled:** 8 November 2026<br>
**Current project version:** `1.2.0`<br>
**Current integrated milestone:** Voice Engine 2.1 and Windows application launching (merged to `main`)<br>
**Detailed source of truth:** [NUMERICK_MASTER_CHECKLIST.md](./NUMERICK_MASTER_CHECKLIST.md)<br>
**Verified implementation snapshot:** [CURRENT_STATE.md](./CURRENT_STATE.md)

## Product direction

Build a dependable, permission-aware, voice-first Windows AI companion. NUMERICK should understand user intent, use bounded tools, verify observable outcomes, remember explicitly approved context, and operate selected desktop workflows while keeping the user in control.

The C++17 application remains the core. The architecture is local-first where practical, with explicitly configured external/remote capabilities only when justified. Private model assets, speaker recordings/profiles, generated audio and runtime diagnostics stay outside version control.

## Milestone status

| Milestone / phase | Scope | Status |
|---|---|---|
| Phase 0 | Reconcile baseline, verify build/tests/voice/app tests, document current state | Complete; documentation integrated by PR #4 |
| Voice Engine 2.1 | Continuous Whisper/VAD voice, persistent native NVIDIA Qwen3-TTS, speech fallback, Windows app launching | Merged in PR #3 |
| Phase 1A | Standard tool execution contract | Next implementation task |
| Phase 1B | Permission levels and confirmation rules | Planned; depends on Phase 1A design |
| Phase 1C | Structured diagnostics, health checks, recovery and worker lifecycle | Planned |

## Roadmap sequence

1. **Phase 0 — Baseline and continuity.** Keep Git state, build/test evidence, current implementation, known limitations, decisions and next work item accurate.
2. **Phase 1 — Reliability, safety and tools.** Standard result/error contract; argument validation; timeouts/cancellation; permissions/confirmations; structured logs; health checks; recovery.
3. **Phase 2 — Voice interaction.** Push-to-talk/hands-free controls, visible states, barge-in, natural turn-taking, microphone/TTS failure recovery and session quality.
4. **Phase 3 — Plan, act, verify.** Structured tasks, explicit step states, bounded plans, cancellation/resume and observable success verification.
5. **Phase 4 — Persistent memory.** Source/confidence-aware episodic, project, preference and task memory with inspection, correction, deletion, export and privacy controls.
6. **Phase 5 — Windows desktop control.** Window/process discovery, UI Automation, safe dialogs, keyboard/mouse actions and verified workflows.
7. **Phase 6 — Visual and document perception.** On-demand screen, camera, image, PDF and office-document understanding with explicit capture permissions.
8. **Phase 7 — Research and connected knowledge.** Current source-grounded research, citations, local project indexing and source provenance.
9. **Phase 8 — Coding companion.** Repository understanding, bounded patches, diff review, test/build workflows and permission-gated Git collaboration.
10. **Phase 9 — Productivity.** Files, drafts, tasks, reminders and authorized calendar/email/clipboard integrations.
11. **Phase 10 — Engineering and robotics.** Verified calculations, datasheets, data analysis, serial telemetry, simulation and safe hardware boundaries.
12. **Phase 11 — Proactive companion.** Opt-in monitoring, reminders and suggestions with quiet hours and easy disable controls.
13. **Phase 12 — Interface and UX.** Visible task/permission/action state, settings, pause/stop and memory controls.
14. **Phase 13 — Performance and deployment.** Resource measurement, model scheduling, reproducible setup, packaging, updates, backups and rollback.
15. **Phase 14 — Security and acceptance.** Threat review, privacy verification, failure recovery and end-to-end workflow acceptance.

## Sequencing and safety gates

- Phase 1 safety and permission foundations precede broad desktop control.
- Phase 3 task representation and verification precede multi-step autonomous desktop workflows.
- UI Automation begins incrementally and uses Windows UI Automation (UIA) as the primary semantic mechanism; screenshots/vision are a fallback, not a reason to click stale coordinates.
- No continuous visual surveillance, unrestricted command execution or autonomous consequential actions without explicit permissions, visible operation state, verification and emergency stop.
- Every milestone closes with targeted tests, full regression verification where applicable, updated records, reviewed diffs and explicit Git/PR/merge state.

## Current next step

Phase 0 is complete. Its documentation closeout was merged through PR #4 on 8 November 2026. Begin **Phase 1A — Standardize the tool execution contract** by inspecting the existing `ToolRegistry`, handlers and `ToolAgent` path before changing implementation.
