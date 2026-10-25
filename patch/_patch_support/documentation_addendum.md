## 2026-10-25 — Scoped Read-Only Project File Tools

**Version:** 0.6.0

### Changes
- Added `list_project_files` for enumerating safe entries under a configured workspace root.
- Added `read_project_file` for reading allowlisted text files.
- Defaulted the workspace root to the process working directory, with `NUMERICK_WORKSPACE_ROOT` as an explicit override.
- Rejected absolute paths and parent-directory traversal, resolved candidate paths and checked that they remain inside the workspace.
- Excluded private/runtime and build folders, common credential/key/database/binary files, and non-allowlisted extensions.
- Capped file reads at 256 KiB, returned tool content at 12 KiB, and limited directory listings to 80 entries.
- Added tests for safe listing/reading, traversal, secret filtering, binary/oversized files, invalid path kinds, and symlink escape where symlink creation is supported.
- Kept file access read-only; no write, delete, process-execution, or arbitrary shell tools were added.

### Verification
- CMake build: Record actual result.
- CTest: Record actual results, including the `project_file_tools_tests` suite.
- Live Ollama file inspection: Record only after testing it on the target machine.

### Limitations
- The allowlist and blocked-path rules are defense-in-depth, not a complete OS sandbox.
- File access remains limited to the selected workspace root and allowlisted text formats.
- File content is untrusted data; the agent is instructed not to follow embedded instructions.
