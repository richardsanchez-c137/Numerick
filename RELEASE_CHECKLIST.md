# NUMERICK v1.0.0 — Release Readiness Checklist

This checklist defines the evidence required before calling v1.0.0 release-ready. Do not mark a checkbox complete until the corresponding result has been observed.

## 1. Source and repository hygiene

- [ ] `git status --short` has no unintended files.
- [ ] No secrets, user memory files, logs, binaries, build directories, local backups, or temporary patch scripts are staged.
- [ ] `git diff --check` and `git diff --cached --check` report no whitespace errors.
- [ ] `CMakeLists.txt`, the application welcome screen, README, and documentation show the same version.

## 2. Clean local build

Run from the repository root using the configured Windows/MSYS2 toolchain:

```powershell
.\scripts\verify-release.ps1 -Clean
```

If CMake needs an explicit generator, specify it, for example:

```powershell
.\scripts\verify-release.ps1 -Clean -Generator "MinGW Makefiles"
```

- [ ] CMake configure succeeds in a dedicated `build-release/` directory.
- [ ] All targets compile successfully.
- [ ] CTest reports passing tests; `--no-tests=error` prevents an empty test run from counting as success.
- [ ] Every CTest test has the configured 30-second timeout.

## 3. Continuous integration

- [ ] The `Windows UCRT64 CI` GitHub Actions workflow succeeds on the release commit.
- [ ] CI configures a fresh build using MSYS2 UCRT64, builds all targets, and runs CTest.
- [ ] The CI test run does not require Ollama or the user's local memory data.

## 4. Manual runtime smoke test

Run:

```powershell
.\build-release\numerick.exe
```

- [ ] The welcome screen reports version `1.0.0`.
- [ ] `help`, `status`, and `time` respond.
- [ ] A simple `ask` prompt works when Ollama and the configured model are available.
- [ ] A safe `agent` prompt can use a registered tool.
- [ ] Workspace access remains read-only and does not read blocked private paths.
- [ ] Agent diagnostics distinguish successful, failed, and rejected tool calls.

## 5. Known limitations to disclose

- Persistent memory is local plaintext and is not encrypted.
- Workspace filters reduce exposure but do not provide a complete OS sandbox.
- C++ component and member-relationship inspection is bounded textual/heuristic analysis, not compiler-grade semantic analysis.
- Voice, camera understanding, arbitrary command execution, and a GUI are not implemented.
- Automated tests do not prove the live Ollama service or model behaves correctly; that requires a manual smoke test.

## 6. Release actions

- [ ] Review staged files and staged diff.
- [ ] Commit the verified release-readiness changes.
- [ ] Push to `main`.
- [ ] Confirm the CI run for the pushed commit is green.
- [ ] Create a Git tag/release only after those checks pass.

**Release status:** pending local verification and a green CI run. The presence of this checklist is not itself evidence that a release passed.
