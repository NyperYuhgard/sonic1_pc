# Contributing & Porting Guidelines

To maintain code quality, historical accuracy, and codebase integrity, all contributions to this project must follow the principles below.

## Porting Philosophy & Architecture
- **Strict 1:1 Parity:** The C codebase must maintain exact logic and structural parity with the original 68000 assembly (`disasm/sonic.asm` + `disasm/_inc/*`). 
- **No Shortcuts or Reinterpretations:** Avoid hardcoding values or logic that the original assembly derives dynamically through tables, headers, or lookups. If the ASM fetches a PLC ID from `LevelHeaders`, the C port must implement the exact same lookup mechanism (e.g., do not hardcode `AddPLC(plcid_GHZ)`).
- **Data Structures First:** Port the underlying mechanisms and lookup tables completely, even if only Green Hill Zone (GHZ) is currently active. Unsupported zone assets can be safely stubbed or set to `NULL`, but the architecture must remain faithful.

## Code & PR Policy
- **Focused Changes:** Keep pull requests concise and scoped to single, modular changes or routines.
- **Clean Submissions:** Ensure the project builds cleanly via CMake before submitting changes. Avoid adding custom debug harnesses or temporary dump scripts to the repository.