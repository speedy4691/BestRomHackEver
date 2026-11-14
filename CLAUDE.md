# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Pokémon ROM hack (GBA, C) built on **RHH's `pokeemerald-expansion` 1.17.2**, which itself is built on pret's `pokeemerald` decompilation. Output is `pokeemerald.gba`. This is NOT a web project — the global Next.js/Supabase/Vercel/ESLint/Vitest defaults do not apply here.

- `origin` = `speedy4691/BestRomHackEver` (this hack). Working branch: `main`.
- `RHH` = upstream `rh-hideout/pokeemerald-expansion`. `master` tracks upstream; update via `git pull RHH expansion/X.Y.Z` (step through versions; see INSTALL.md). Upgrade-time data migrations live in `migration_scripts/<version>/`.
- Upstream docs: https://rh-hideout.github.io/pokeemerald-expansion/ (local copies in `docs/`, tutorials in `docs/tutorials/`).

## Environment

The repo lives inside WSL (`\\wsl.localhost\Ubuntu\home\speedy4691\...`). **All build commands must run in WSL/Linux**, not PowerShell, e.g. from PowerShell: `wsl -e bash -lc "cd ~/decomps/BestRomHackEver && make -j$(nproc)"`. Running `git` from Windows against this path fails with "dubious ownership" and is very slow — run git inside WSL too (or pass `-c safe.directory='*'`). Windows git also shows many files as modified that are likely not real changes (probably file-mode or line-ending differences on scripts and `.png`/`.pal`/`.bin` assets); trust `git status` from inside WSL only.

## Commands (run in WSL)

```sh
make -j$(nproc)              # build pokeemerald.gba (modern GCC toolchain)
make debug                   # -Og -g build for debugging the .elf
make release                 # optimized build, NDEBUG + LTO (disable LTO in config.mk)
make firered / leafgreen     # build the FRLG variant instead of Emerald
make check -j$(nproc)        # run the full headless test suite
make check TESTS="Spikes"    # run tests whose name starts with/contains a filter
make pokeemerald-test.elf TESTS="Spikes"   # build test ROM to watch a test in mGBA
make clean-generated         # wipe auto-generated headers (see below) if they get stale
make tidy                    # remove build outputs, keep tools
```

Useful make vars: `UNUSED_ERROR=1 DEPRECATED_ERROR=1` (what CI uses), `KEEP_TEMPS=1` (keep `.i`/`.s` intermediates), `ANALYZE=1`. CI (`.github/workflows/build.yml`) builds Emerald + FRLG and runs `make check`.

## Architecture

- **Configuration lives in `include/config/*.h`** (`battle.h`, `pokemon.h`, `overworld.h`, `general.h`, `debug.h`, `test.h`, etc.). Most expansion features and generation-specific behaviors are toggled by `B_*`, `P_*`, `OW_*`, `I_*` defines there — check config before changing code.
- **Data-driven tables** in `src/data/`: `moves_info.h`, `items.h`, `abilities.h`, `pokemon/` (species info, learnsets, evolutions), `wild_encounters.json`, etc. Adding moves/species/trainers mostly means editing these; see `docs/tutorials/how_to_new_move.md`, `how_to_new_pokemon.md`.
- **Generated files — edit the source, not the output:**
  - `src/data/*.party` (Showdown-style trainer format, documented at top of `trainers.party`) → `trainers.h`, `battle_partners.h`, `debug_trainers.h` via `tools/trainerproc`.
  - `data/maps/*/map.json`, `data/layouts/` → map headers/events and `include/constants/{map_groups,layouts,map_event_ids}.h` via `tools/mapjson`. Edit maps with Porymap (`porymap.project.cfg` present).
  - `src/data/wild_encounters.json` → `wild_encounters.h`; region map JSON → `region_map_entries.h`; teachable learnsets → `src/data/pokemon/teachable_learnsets.h`.
- **Scripts**: overworld event scripts are assembly macros in `data/scripts/`, `data/maps/*/scripts.inc`, `data/event_scripts.s`; battle scripts in `data/battle_scripts_*.s`; battle animations in `data/battle_anim_scripts.s`.
- **Battle engine**: `src/battle_*.c` (core in `battle_script_commands.c`, `battle_util.c`, `battle_main.c`), AI in `src/battle_ai_*.c`, move effects enumerated in `src/data/battle_move_effects.h`.
- **Text** uses a custom charset: string literals go through `tools/preproc` with `charmap.txt` (`_("...")` / `COMPOUND_STRING("...")`).
- **Tests** (`test/`, battle tests under `test/battle/`) run inside an emulated GBA via the in-tree test runner. Pattern: `ASSUMPTIONS` → `GIVEN` (parties) → `WHEN` (turns) → `SCENE` (expected messages/anims) → `THEN` (state asserts). See `docs/tutorials/how_to_testing_system.md`. Trainer test fixtures use `test/battle/trainer_control.party`.
- `graphics/`, `sound/` assets are converted at build time by `tools/gbagfx`, `wav2agb`, `mid2agb` (rules in `graphics_file_rules.mk`, `audio_rules.mk`).

## Conventions (from docs/STYLEGUIDE.md)

- Functions/structs `PascalCase`; variables/fields `camelCase`; globals `g` prefix, statics `s` prefix; macros/constants `CAPS_WITH_UNDERSCORES`.
- Comments explain *why*, not what. Prefer config toggles over hard-coded behavior changes when touching expansion systems, to keep future `RHH` merges clean.
- Memory is tight (EWRAM/IWRAM ~86% used); the link step prints usage — watch it when adding globals or save data.

## Project overview

A Pokémon Emerald ROM hack built on pokeemerald-expansion. Solo project (Chase). Full feature list, decisions and roadmap live in the Google Sheet "Emerald Hack Feature Plan" (tabs: Roadmap, Core features, Level caps, Species by area, Photography, Side quests and plot). Roadmap step numbers (e.g. 2.1) and Core numbers (e.g. Core #6) refer to that sheet.

## Project environment

- Ubuntu on WSL2. Project at `~/decomps/BestRomHackEver`.
- Build: `make -j$(nproc)`. Output ROM: `pokeemerald.gba`.
- I test in mGBA on Windows. You can't run the emulator, so after each build tell me exactly what to check in-game.
- Remotes: `origin` = my fork (speedy4691/BestRomHackEver), `RHH` = upstream rh-hideout/pokeemerald-expansion.
- `main` was created from the latest stable expansion release tag.

## Workflow for every feature

In this repo, these rules override ~/.claude/CLAUDE.md: use feature/ branch names, and write PR test steps for mGBA, not a Vercel preview.

1. Start from an up-to-date main: `git checkout main && git pull`.
2. Create one branch per feature: `feature/<short-name>` (e.g. `feature/running-shoes`).
3. Before writing new code, check expansion's config files (`include/config/*.h`) for an existing option. Prefer a config change over new code.
4. Keep changes small and focused on the one feature. Don't reformat or touch unrelated files.
5. Build with `make -j$(nproc)`. Fix all errors before handing over. Report any new warnings.
6. Give me a short test checklist for mGBA (what to do in-game and what should happen). Remind me to start a new game if save data could be affected.
7. Only after I confirm the playtest passed: commit with a clear message, push the branch, and open a pull request with `gh pr create` against `main`. Never push directly to `main`, and never merge without my go-ahead.

## Explaining changes

- Before editing, tell me in a few lines which files you'll change and why.
- After editing, summarise what changed per file.
- I'm learning this codebase: when you touch an unfamiliar system (scripts, map headers, battle code), give a one-line explanation of how it works.

## Settled design decisions (don't re-ask)

- **Level caps:** hard cap (no EXP at the cap). Roxanne cap Lv 15; leader's strongest Pokémon = cap. Rare Candy and the Daycare respect the cap (the Daycare levels by steps, so it needs its own check). No on-screen cap display; the new cap is told in dialogue after the badge. The first cap (Lv 15) also needs an early dialogue line.
- **Pokédex gates:** only caught Pokémon count; "seen" doesn't.
- **No items for the player in trainer battles;** wild battles and held items unaffected; opposing trainers keep their items.
- **No switching after a KO** (Battle Style locked to Set).
- **PC anywhere:** {PKMN} Box Link key item opens PC storage anywhere except the Elite Four, Battle Frontier and Trainer Hill. No portable healing (2.7 was de-scoped).
- **Return to last Pokémon Center** from anywhere, blocked in gyms, Elite Four and scripted events.
- **HMs** can be forgotten AND field moves work through badges, but a compatible Pokémon (one that can learn the move) must be in the party.
- **Key items given by an NPC in the early game** (who and when not decided yet), who explains each one: Teleport, {PKMN} Box Link, Mach and Acro Bike, and a toggleable **Infinite Repel**. Until then they're only obtainable via the debug menu (custom key items go first in the debug "Fill Pocket Key Items" list in `src/debug.c`).
- **Modern mechanics on:** reusable TMs, physical/special split, Fairy type, modern learnsets, ability pop-ups.
- **FireRed and Gold/Silver species** added in the same cadence as their original games (by badge count).

## Roadmap order (next steps)

Done: 2.1–2.3, 2.6–2.8. Deferred: 2.4 (waits on 4.3), 2.5 (to review with Will and Chase). In progress: 2.9 Bikes and Infinite Repel. Next: the vertical slice (3.x). The Google Sheet's Roadmap tab is the source of truth; update its Status and Notes after each merged PR.
