# Matching Decompilation Ruleset (Byte-for-Byte C) — v2

## How to use this

This is a standing ruleset for an AI agent (Claude Code) doing a **matching decompilation**: C/C++ source that, built with the original toolchain, reproduces the original shipped files byte for byte. Put this file in the project root, reference it from CLAUDE.md, and fill in the Phase 1 table. Everything below the line is written to the agent.

---

## Prime directive

The only proof of progress is a machine comparison against the original. A function, file or build is "matching" only when the diff tool reports zero differences. Readability, plausibility and "functionally equivalent" never count as matching. Your own description of a result is never evidence; tool output is.

### Definitions (use these exact meanings)

| Term | Meaning |
| --- | --- |
| Matching | Compiled code and data (instructions, relocations resolving to the same symbol + addend, data, section placement) are identical to the original. |
| Equivalent | Believed to behave the same but differs in bytes. Lives under `NON_MATCHING`. Never reported as matching. |
| Non-matching | Has a C attempt that does not yet match. The default build still uses the original asm via the asm-include macro. |
| Unstarted | Still included as disassembly. |
| Handwritten asm | Proven (with evidence and user approval) to be originally written in assembly. Kept as asm permanently. |
| Library-linked | Satisfied by linking an original SDK/CRT `.lib`/`.a` object, not by our source. Counted separately from matched. |
| Unmatchable | A function the user has signed off as not reachable after documented effort. Listed in `docs/UNMATCHABLE.md`. |
| Baseline | Every in-scope original file with its SHA-1, plus checksums of each extracted segment. |

### Core rules (override everything else)

1. **Never change the target or the judge.** The baseline, the hashes, the diff tools, the verify script, and `expected/` are read-only. Never edit the Makefile, split config or linker script so that a C file is not actually compiled or checked.
2. **Never claim progress without tool output.** Status comes from the generated progress report, never from what you typed.
3. **`verify` passes on every commit.** The verify mode is chosen in Phase 1 (see there). A commit that fails verify is reverted, not "fixed later".
4. **No hidden bytes.** No `incbin`, `.word`/`.byte` dump or copied blob may cover any range the split config labels code, rodata, data or headers, except entries on `docs/ALLOWED_BLOBS.md`, which the user has approved. Verify fails if any other exists.
5. **No forcing.** Never write new asm, or asm statements inside otherwise-C functions, to force a match. Reproduce original inline asm only where the evidence shows the original used it (Phase 6).
6. **No publishing without approval.** Do not upload code, binaries or asm to decomp.me or any public service unless the user approves it.

---

## Phase 0 — Scope, baseline and project setup

Goal: a reproducible workspace that checks every in-scope file automatically. No decompiling yet.

- [ ] **Define scope.** List every shipped file (each EXE, DLL, overlay, ROM, disc image, SYSTEM.CNF, data archives, setup/editor tools). Mark each in or out of scope with its SHA-1. Verify checks every in-scope output, not just one.
- [ ] **Validate the dump.** Check against Redump / No-Intro / retail hashes. Reject cracked, No-CD, trained or patched executables. If DRM or a packer is present (SafeDisc, SecuROM, PKLITE, UPX, custom), record whether the target is the wrapped or unwrapped file and why.
- [ ] **Record the baseline:** name, size, SHA-1 (and the CRC/MD5 the community uses), region, revision, and dump source.
- [ ] **Versions:** list all regions, revisions, demos and debug builds; pick ONE primary target. If others may be matched later, use `#if VERSION == ...` from the start. Builds that ship with debug symbols are the most valuable source of names and types.
- [ ] **Prior work:** search for existing decomps, symbol maps, shipped or leaked symbols (.map, .pdb, .sym, DWARF/stabs), SDK headers and docs, sibling projects with the same engine or SDK. Record what was found and its license.
- [ ] **Repo layout:** `baserom/` (git-ignored), `asm/`, `src/`, `include/`, `tools/`, `build/`, `docs/`, `config/` (split config, symbol file), `expected/` (generated, git-ignored). Never commit original binaries, extracted assets, expected objects or original SDK libraries.
- [ ] **Pinned tools:** every external tool in `tools/` pinned to an exact version or commit, with one setup script. A fresh machine reaches a passing verify with one command.
- [ ] **`verify` target:** rebuilds and compares every in-scope output. On FAIL it prints the first differing offset and maps it to section and symbol. It also runs the blob check (core rule 4).
- [ ] **CI:** run verify and the progress report on every commit. A CI log is the evidence of record.
- [ ] Create `docs/NOTES.md`, `docs/QUIRKS.md`, `docs/files/`, `docs/claims.md`, `docs/UNMATCHABLE.md`, `docs/ALLOWED_BLOBS.md`.

## Phase 1 — Identify the target

Goal: know exactly what produced each file, proven with evidence. A wrong compiler guess makes matching impossible.

Fill this in before going further, with evidence for each row in NOTES.md:

| Item | Value | Evidence |
| --- | --- | --- |
| Platform / CPU | {N64 MIPS, PS1 R3000, x86 DOS, Win32, GBA ARM7, GameCube PPC…} | |
| Executable format | {ELF, COFF, PE, MZ/LE/LX/NE, a.out, raw ROM, custom overlays} | |
| Endianness, word size | | |
| Compiler and exact version | {IDO 5.3, GCC 2.7.2 KMC, agbcc, MWCC x.y, MSVC 6.0 SP5, Watcom 10.6…} | |
| Flags (per file if they differ) | | |
| Assembler and linker (+ versions) | | |
| Runtime / SDK / libraries (+ versions) | | |
| Languages present | {C, C++, handwritten asm, inline asm} | |
| Debug info present? | {none, DWARF, stabs, CodeView, .mdebug} | |
| Volatile fields | {PE timestamp, checksum, PDB path/GUID, build dates in strings} | |
| Compression / encryption / DRM | | |
| **Verify mode** | {A or B, below} | |

**Verify mode (pick one and record it):**

- **Mode A — whole-image hash.** The default wherever the toolchain allows it (most ROM/console projects). Every commit must rebuild every in-scope file to the baseline SHA-1, using asm for anything not yet matched.
- **Mode B — per-unit match + whole-image diff.** Use this for PC (MSVC/Watcom) targets, where relinking to an identical image only becomes possible late. Functions and data are scored per unit (objdiff, reccmp or similar). The whole image is also diffed, and only the fields listed under "Volatile fields" may differ, each documented. Mode B is a working mode only: the definition of done still requires whole-image byte identity.

**Identifying the compiler (fastest first):**

1. Strings: compiler banners, library version strings, build paths, dates.
2. Headers: the MSVC Rich header (exact cl/link build numbers), the PE linker version field, import-library versions, ELF `.comment`, and SDK version strings.
3. Fingerprints: prologue/epilogue shape, frame layout, register allocation habits, how switch tables, divides, 64-bit math and float conversions are emitted, and which runtime helpers appear.
4. Sweep: test 3–5 small, non-trivial functions against every candidate compiler × flag set (`tools/try-compilers`, or decomp.me presets if the user approves). An exact match on a non-trivial function is strong evidence; no match at all means keep looking.
5. Expect several compilers or flag sets per binary (libraries, SDK, different files).

**Set expectations:** MIPS/IDO, agbcc and MWCC projects match routinely. x86 MSVC/Watcom matching is much harder (register allocation instability, COMDAT folding, link order). Tell the user which difficulty class this target is in.

## Phase 2 — Reproduce the original toolchain

Goal: run the exact compiler, assembler and linker the developers used. A newer version of the "same" compiler will not match. Do not substitute a modern compiler and "tune" it.

- [ ] Get the original binaries (SDKs, archived dev kits, preserved copies). Record the version and hash of each.
- [ ] Make them run on a modern host (native, Wine, DOSBox, static recompilation such as for IDO, or an emulated host OS). Wrap each in a script.
- [ ] **Determinism:** compile the same file twice and diff. Control anything that leaks into output.
- [ ] **Build-environment strings:** where the binary contains `__FILE__` paths (asserts), `__DATE__`/`__TIME__`, PDB paths or similar, reproduce them through directory layout, defines or a fake clock. Do not strip them.
- [ ] Reproduce every original build step: custom preprocessors, asm-processor (IDO), charset conversion (Shift-JIS, EUC-JP), code generators, packers.
- [ ] Match assembler behaviour (macros, delay slots, alignment, pseudo-instruction expansion) and linker behaviour (section order, alignment, fill bytes, symbol order, linker script or response file).
- [ ] **`tools/try <func>`:** compiles the function's file with that file's flags, diffs the function against the expected object, and prints a score plus at most 40 differing lines. `tools/try --full <func>` prints the full diff. Every Phase 5 iteration uses this.
- [ ] **`tools/ctx <file>`:** writes a preprocessed context header (types, prototypes, externs) for m2c, Ghidra and decomp.me.

## Phase 3 — Split, rebuild from asm, and scaffold

Goal: rebuild every in-scope file byte for byte from disassembly and generated metadata before writing any C.

- [ ] **Map every byte** into a named region in the split config: headers, segments, overlays, code, rodata, data, BSS, relocation tables, assets, padding, and any data after the image end.
- [ ] **Generate container bytes with tools**, never copy them as blobs:
    - PE: DOS stub, Rich header, COFF/optional header fields, data directories, imports (DLL order, hint/name, IAT, bound imports), exports, `.reloc`, TLS, load config, `.rsrc` (rebuilt with the original rc/cvtres), debug directory.
    - MZ/LE/NE: relocation/fixup order, DOS extender stub, overlays.
    - ROM/disc: header fields and checksums (for example the N64 CIC CRC or GBA header complement), computed by a tool.
    - Volatile fields (timestamps, PE checksum, PDB GUID) are set by documented linker options or a documented post-link step that writes the recorded original values.
- [ ] Unpack anything packed, and reproduce the packer so the build re-packs identically.
- [ ] Disassemble with a re-assemblable, relocation-aware tool: splat + spimdisasm (N64/PS1/PS2/PSP), decomp-toolkit/dtk (GameCube/Wii), pret-style tooling (GB/GBA/DS), Ghidra/IDA export + custom cleanup or reccmp-style tooling (x86).
- [ ] **TU boundaries:** split at best-evidence boundaries (alignment padding, rodata/data grouping, static clusters, string order, debug info, style changes). Record evidence and confidence per boundary in `docs/files/`. Where the linker destroyed boundaries (COMDAT/ICF, `/Gy`, .lib extraction order), document the chosen grouping.
- [ ] Separate data from code: jump tables, literal pools and embedded constants are data.
- [ ] **Asm-include scaffolding:** every function sits in its `.c` file through the project's asm-include macro (`INCLUDE_ASM`, `GLOBAL_ASM` via asm-processor, MWCC `asm` functions, or equivalent). This keeps TU layout and rodata order intact while functions are converted one by one.
- [ ] Write the linker script that reproduces the original layout.
- [ ] **Generate `expected/`** from baserom + split config (git-ignored, never hand-edited). Regenerate only after a user-approved split change, and verify still checks against the baseline hashes.
- [ ] **Checkpoint (gates Phase 5):** the asm-only build passes verify. Shiftability is not required yet. Log every known hard-coded pointer in NOTES.md as you find it.

## Phase 4 — Symbols, types, data and libraries (continuous)

Goal: the foundation C stands on. This runs alongside Phase 5, not before it. Most match failures come from wrong types and data layout, not wrong logic.

**Symbols.** One symbol file is the source of truth: address, name, kind, size, confidence (debug info / SDK / guessed). Use official names wherever debug info, SDK headers or strings give them; otherwise use placeholders (`func_80012345`, `D_80098760`).

**Libraries.** Identify statically linked library code early (libc, math, SDK, middleware, sound drivers) and match it against known versions or sibling projects first. These are the largest wins per hour. A library function that does not match usually means a different library version, not wrong C. Linking original library objects counts as library-linked, never as matched.

**Types.** Define a struct when a function needs it, with only the fields proven so far, in a shared header; grow it as more functions are matched. Offsets, sizes, alignment and padding must equal the original. Respect the original ABI: sizes of `int`/`long`/`enum`/`bool`/pointers, packing, bitfield order, float/double promotion, `char` signedness. Use original SDK headers and macros where available.

**Data placement.** Decompiling a file includes its data. Every object ends up in the same section, order and alignment. Watch for:

- source-order effects: string literal pools, float constants, jump tables, static locals, late_rodata;
- linker-determined order (not source order): COMMON/tentative definitions (use `-fno-common` only if the original did), small-data sections (.sdata/.sbss/.sdata2 and the -G threshold), cross-TU string merging (`/GF`, SHF_MERGE);
- `const`, `static` and initializer style changing the section. Check placement in the map file, not just the bytes.

**Debug sections.** If the original contains debug info, it must match, or the user signs off on declaring it out of scope.

## Phase 5 — The per-function matching loop

Goal: replace asm with C one function at a time, keeping verify passing.

**Tools this loop relies on:**

- `tools/next` ranks unstarted functions in this order: (a) library/SDK code matching a known version or a sibling project; (b) duplicate or near-duplicate functions (byte hash with relocations masked), where one match serves all copies; (c) small functions whose callees are already matched; (d) functions that finish a file that is already ≥70% matched.
- `tools/draft --all` stores an m2c/Ghidra first draft for every unstarted function in `build/drafts/`. Regenerate it after major type changes. Drafting may be batched; matching may not.

**The loop:**

1. **Pick** the top entry from `tools/next`, unless you log a reason not to. Claim its file first (see parallel work).
2. **Read** the asm fully: arguments, return value, frame size, saved registers, every load/store offset and width, every branch, every call and its arguments.
3. **Search QUIRKS.md** for patterns that apply.
4. **Draft C** from the stored draft. Write it the way a programmer of that era would: plain loops, original-style types, no modern idioms.
5. **`tools/try`** until the score is 100%. Typical levers: types and signedness; declaration order; adding/removing temporaries; `for`/`while`/`do`; `if/else` order and inversion; `switch` vs `if` chains; pointer arithmetic vs indexing; splitting or combining expressions; casts; `register`/`volatile`; SDK macros; operand order of commutative operations.
6. **A match requires relocations to resolve to the same symbol and addend**, not just the same address (for example `&struct.field`, not a lookalike `D_` symbol).
7. **Time-box.** Stop iterating after 30 `try` runs, or 10 runs with no score improvement, or about 45 minutes. Then:
    - start decomp-permuter in the background with a 1–2 hour limit. If it finds a shape, rewrite that shape as plausible C; never commit raw permuter output;
    - mark the function non-matching (pattern below), log what differs in `docs/files/<file>.md`, and take the next queue entry;
    - return to it only after the types it uses change, or once the queue is empty.
8. **Non-matching pattern:** `#ifdef NON_MATCHING` (your C) `#else` asm-include `#endif`. The C under `NON_MATCHING` must be believed equivalent. A `nonmatching` build target compiles all such C and must boot. C known to be wrong goes under `#if 0` and is logged.
9. **Commit cadence:** run full verify before every commit; never commit a failing verify. Commit at least once per completed file and at the end of every session.

**Matching rules:**

- Do not change behaviour to get a match. Dead code, redundant loads and original bugs are reproduced.
- Do not hard-code addresses or offsets that should be symbols or struct fields.
- When an inline function or macro produces a match, add it to `include/` and record it in QUIRKS.md.

## Phase 6 — Hard cases

| Case | How to recognise it | What to do |
| --- | --- | --- |
| Handwritten assembly | No compiler prologue, impossible scheduling, special instructions, or a matching `.s` in an SDK | Classify only with evidence in NOTES.md and user approval. Keep as clean, symbolized asm. |
| Original inline asm in C | Compiler-generated frame around a block no compiler would emit (common in MSVC, Watcom, DOS, GC SDK) | Reproduce it as inline asm inside the C function, in the original form. |
| Inlined functions | Repeated code across callers | Use the form the original compiler supported: macros for IDO, `inline` where honoured. Confirm no stray out-of-line copy is emitted. |
| Per-file flag differences | A file matches only with other flags | You may set per-file flags without asking when a non-trivial function proves them. Log the change. |
| C++ | Mangled names, vtables, ctors/dtors, RTTI | Recover class layouts and vtable order. Mangling depends on the exact compiler version. Watch template and weak-inline emission order, vtable thunks, scalar/vector deleting destructors, RTTI type-name strings. |
| MSVC link artifacts | One address with many names, 0xCC fill, jump thunks | Handle COMDAT folding (`/OPT:ICF`), incremental-link thunks and padding, `/Gy` and order-file function ordering. |
| Exception and init tables | EH FuncInfo, unwind maps, SEH scope tables, `.CRT$XC*` | They must match. Static-initializer order follows link and source order. |
| Floating point | Constant pools, conversion helpers | Check float vs double literal suffixes, FPU mode, soft-float helpers. |
| Self-modifying / relocated code | Code copied or patched at runtime | Match the stored bytes, not the runtime state. |
| Unused / dead code | Nothing calls it | Still decompile and match it. |
| Data inside code sections | Literal pools, padding, fill | Emit as data with the exact bytes, generated by the toolchain where possible. |

A function that truly cannot be matched after documented effort can go on `docs/UNMATCHABLE.md` only with diff evidence and the user's sign-off. Never hide it.

## Phase 7 — Assets

- [ ] Reverse-engineer each asset and archive format.
- [ ] Write extract and rebuild tools. The build extracts from the user's original and rebuilds from extracted files.
- [ ] Rebuilt assets must byte-match and feed the hashed build. Archive order, compression parameters, alignment and padding are all part of the match.

## Phase 8 — Final verification and definition of done

- [ ] Fresh clone + setup script + the user's original → verify passes in **whole-image mode (Mode A)** for every in-scope file, regardless of the working mode.
- [ ] Two clean builds produce identical output.
- [ ] No asm remains except approved handwritten asm and approved original inline asm.
- [ ] No `NON_MATCHING` or `#if 0` blocks remain.
- [ ] Blob check passes; `ALLOWED_BLOBS.md` contains only items the user approved.
- [ ] Shiftability: every hard-coded pointer is symbolized. A build with added padding (and, for PE/MZ, regenerated relocations and a rebased load) links and runs.
- [ ] Scripted smoke test (input replay, savestate or demo playback) passes on both the normal and the shifted builds.
- [ ] Every TU boundary has its evidence recorded.
- [ ] Debug sections match or are signed off as out of scope.
- [ ] Assets rebuild from tools; nothing copyrighted is committed.
- [ ] Secondary versions match or are listed out of scope.
- [ ] Report the percentage of placeholder symbol names (tracked, not required).

**Status levels:**

- **Done:** every item above checked, the generated report shows 100% of code and data bytes matched, plus only approved handwritten asm and library-linked objects, and `UNMATCHABLE.md` is empty.
- **Substantially complete:** the same, but `UNMATCHABLE.md` lists user-approved functions. Never call this "done".

## Agent conduct, parallel work and session handoff

**Conduct rules:**

1. Match one function at a time. Drafting may be batched.
2. Never report a match from reading code; quote `tools/try` or verify output, and rely on the CI log as the record.
3. If a Phase 0–3 checkpoint fails, stop and fix it. Phase 4 is continuous and never gates.
4. Record guesses (types, names, boundaries) with confidence; never present them as fact.
5. Prefer evidence from the binary over assumptions about what the developers "probably" wrote.
6. Ask the user before changing the toolchain or the linker script, and before changing the split config once the Phase 3 checkpoint has passed. Per-file flags follow Phase 6.
7. Ask before classifying anything as handwritten asm, adding to `ALLOWED_BLOBS.md` or `UNMATCHABLE.md`, or publishing anything.

**Progress reporting:** PROGRESS.md is generated by `tools/progress` from the build (objdiff/reccmp/splat report plus a scan for remaining asm-include and `.s` files). It reports functions and bytes per status (matched, non-matching, unstarted, handwritten, library-linked, unmatchable), per file and overall. Never edit it by hand. Per-file notes go in `docs/files/<file>.md`.

**Parallel agents:** each agent works in its own git worktree and claims a whole file in `docs/claims.md` before starting. Never edit another agent's claimed file. Changes to shared headers or the symbol file go in small separate commits, rebased first. Before merging: rebase, run full verify, regenerate progress.

**Session start:** read the PROGRESS.md totals, QUIRKS.md (keep it under about 150 lines of compiler-quirk → C-pattern recipes), and the notes for your claimed file. Read the full NOTES.md only when stuck. Run verify and confirm PASS before touching anything.

**Session end:** run verify, regenerate progress, add new quirks to QUIRKS.md and file details to `docs/files/<file>.md`, release or keep claims, and commit.
