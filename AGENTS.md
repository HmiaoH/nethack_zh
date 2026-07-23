# NetHack Chinese Localization (汉化)

NetHack 5.0 — a roguelike dungeon crawler. This document is for agents working on a Chinese localization plugin/patch.

## Build & Dev Setup

```bash
# From repo root:
cd sys/unix && sh setup.sh hints/macos.500 && cd ../..
make fetch-Lua      # first time only (downloads Lua 5.4.8 into lib/)
make all            # builds nethack, recover, all data files
make clean          # remove .o files
make spotless       # return to near-distribution state
```

After setup, Makefiles are generated at the repo root level (not in `sys/unix/`).

Hints file for macOS: `sys/unix/hints/macos.500` (883 lines, handles Homebrew/MacPorts detection).

## Architecture

```
src/        — C99 core engine (~133 .c files), all hardcoded English strings
include/    — headers (~91 files)
dat/        — data files: help text, Lua level/quest scripts, encyclopedia, etc.
win/        — window port implementations (tty, curses, X11, Qt)
sys/        — OS-specific build config, hints files
util/       — build tools: makedefs, recover, dlb
test/       — Lua test scripts (run via #wizloadlua in wizard mode)
doc/        — documentation, Guidebook
```

**Entry point**: `sys/unix/unixmain.c` → `src/allmain.c`
**String output**: `src/pline.c` (`pline()`, `You()`, `verbalize()`, etc.) and `src/pager.c`

## Text Landscape for Translation

### Tier 1 — Core game strings (highest impact)
- **`include/objects.h`** (1659 lines): All item names and unknown descriptions. Uses macro-based multi-pass inclusion. String fields: `name` (known name), `desc` (unknown description).
- **`include/monsters.h`** (3927 lines): All monster names. Uses `NAM()` / `NAMS()` macros. String fields in `struct permonst`: `mname` (name), `pmname` (female name), `mpname` (neuter name).
- **`src/*.c`** (~130 files): Every user-visible message string is hardcoded. Key files for message volume: `src/objnam.c`, `src/invent.c`, `src/pager.c`, `src/cmd.c`, `src/end.c`, `src/hack.c`, `src/shk.c`.
- **`include/func_tab.h`** + **`src/cmd.c`**: Extended command names and descriptions (`#` commands like `#pray`, `#enhance`).

### Tier 2 — Data/help files (medium impact)
- `dat/help`, `dat/cmdhelp`, `dat/keyhelp`, `dat/history`, `dat/opthelp`, `dat/optmenu`, `dat/wizhelp`, `dat/usagehlp` — help screens
- `dat/quest.lua` (3087 lines) — quest dialog/text with `%` variable substitution (e.g. `%n` = nemesis name, `%l` = leader name)
- `dat/dungeon.lua` — dungeon branch/level names
- Lua level scripts in `dat/` (e.g. `Arc-strt.lua`, `castle.lua`) — level descriptions, NPC speech

### Tier 3 — Flavor text (lowest priority)
- `dat/data.base` (6528 lines) — encyclopedia entries with literary quotes
- `dat/engrave.txt`, `dat/epitaph.txt` — random floor engravings and headstone text
- `dat/rumors.tru`, `dat/rumors.fal`, `dat/oracles.txt`, `dat/bogusmon.txt`

## Critical Config Change

In `include/config.h:323`, `MAKEDEFS_FILTER_NONASCII` rejects non-ASCII (including Chinese) characters in data files processed by `makedefs`. **Must comment this out** for Chinese text:

```c
/* #define MAKEDEFS_FILTER_NONASCII */
```

## Encoding

The game already supports UTF-8 via `ENHANCED_SYMBOLS` (config.h:368). Chinese characters should work with TTY window port if terminal supports UTF-8. The `utf8map.c` module handles UTF-8 glyph display. The `objects.h` and `monsters.h` string fields are `const char *` so UTF-8 can be used directly.

## Localization Approach Options

1. **Preprocessor-based**: `#ifdef CHINESE` blocks in objects.h/monsters.h/source files. Simple but creates maintenance burden across 130+ C files and ~3000+ monsters.h lines.

2. **Runtime string tables**: Build a lookup table mapping English → Chinese at runtime. Requires intercepting every `pline()`, `You()`, `verbalize()`, `OBJ_NAME()`, `OBJ_DESCR()` call. More complex but less invasive.

3. **Data file replacement**: Replace `objects.h`/`monsters.h` with Chinese variants, and patch key C files. Most practical approach — concentrate 90% of visible strings in a small number of files (`objects.h`, `monsters.h`, quest Lua files, help data files), then patch only the highest-frequency message source files.

### Recommended approach: Hybrid
- Translate `objects.h` → `objects_zh.h`, `monsters.h` → `monsters_zh.h`, and have a Chinese hint file or build flag swap them
- Translate all `dat/` help and Lua text files (just replace English text in-place or create `dat_zh/`)
- Patch ~15-20 most message-heavy C files (`objnam.c`, `invent.c`, `end.c`, `hack.c`, `shk.c`, `cmd.c`, `pray.c`, `eat.c` etc.) with `#ifdef ZHLANG` guards
- Add a hints file include (e.g. `sys/unix/hints/include/chinese.500`) to set `CFLAGS += -DZHLANG`

## Key Files Reference

| File | Lines | Content |
|------|-------|---------|
| `include/objects.h` | 1659 | All item names + descriptions |
| `include/monsters.h` | 3927 | All monster names |
| `include/extern.h` | 4071 | Function prototypes for all modules |
| `include/config.h` | 744 | Core compile-time config |
| `src/objnam.c` | 5729 | Object naming/variations (very text-heavy) |
| `src/cmd.c` | 5732 | Extended command definitions and descriptions |
| `src/pager.c` | 2967 | Help system, `/` command, data.base lookups |
| `dat/data.base` | 6528 | Encyclopedia entries |
| `dat/quest.lua` | 3087 | Quest dialog with variable substitution |
| `dat/symbols` | 1102 | Display symbol sets (not translation, just display) |

## Testing

Tests are Lua scripts in `test/`. To run: build without DLB, install, copy test `.lua` files to the playground, start in wizard mode, use `#wizloadlua` to run each test file. There is no automated test runner. CI is on Azure Pipelines (`azure-pipelines.yml`).

## Commit Style

From `Contributing.md`: 50/72 rule — subject ≤50 chars, body wrapped at 72 chars, blank line between subject and body.
