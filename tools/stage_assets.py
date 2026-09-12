#!/usr/bin/env python3
"""Stage only the non-audio assets referenced from src/data.c.

Recovers asset paths from the load_asset/load_asm_asset calls in
src/data.c, maps them to disasm/ source files, and copies just those
into build/assets/. Music and SoundFX are never staged.
"""

import os
import re
import shutil
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_C = os.path.join(REPO_ROOT, "src", "data.c")
DISASM = os.path.join(REPO_ROOT, "disasm")
BUILD_ASSETS = os.path.join(REPO_ROOT, "build", "assets")

SRC_DIR_MAP = {
    "anim": "_anim",
    "artnem": "artnem",
    "artunc": "artunc",
    "collide": "collide",
    "map16": "map16",
    "map256": "map256",
    "maps": "_maps",
    "objpos": "objpos",
    "palette": "palette",
    "tilemaps": "tilemaps",
    "levels": "levels",
}

RENAME_MAP = {
    "anim/psbtm.asm": "Press Start and TM.asm",
    "anim/rings.asm": "Rings.asm",
    "anim/sonic.asm": "Sonic.asm",
    "anim/Sonic.asm": "Sonic.asm",
    "anim/titlesonic.asm": "Title Screen Sonic.asm",
    "artunc/Sonic.unc": "Sonic.unc",
    "maps/Sonic - Dynamic Gfx Script.asm": "Sonic - Dynamic Gfx Script.asm",
    "artnem/title_sonic.nem": "Title Screen Sonic.nem",
    "artnem/sega_logo.nem": "Sega Logo (REV00).nem",
    "artnem/boss.nem": "Boss - Main.nem",
    "artnem/boss_exhaust.nem": "Boss - Exhaust Flame.nem",
    "artnem/boss_weapons.nem": "Boss - Weapons.nem",
    "artnem/buzz.nem": "Enemy Buzz Bomber.nem",
    "artnem/signpost.nem": "Signpost.nem",
    "artnem/hidden_bonus.nem": "Hidden Bonuses.nem",
    "artnem/bigflash.nem": "Giant Ring Flash.nem",
    "artnem/chopper.nem": "Enemy Chopper.nem",
    "artnem/continue.nem": "Continue Screen Stuff.nem",
    "artnem/crabmeat.nem": "Enemy Crabmeat.nem",
    "artnem/credit_text.nem": "Ending - Credits.nem",
    "artnem/ghz1.nem": "8x8 - GHZ1.nem",
    "artnem/ghz2.nem": "8x8 - GHZ2.nem",
    "artnem/ghz_stalk.nem": "GHZ Flower Stalk.nem",
    "artnem/hspring.nem": "Spring Horizontal.nem",
    "artnem/hud_lives.nem": "HUD - Life Counter Icon.nem",
    "artnem/jap_credits.nem": "Hidden Japanese Credits.nem",
    "artnem/lz.nem": "8x8 - LZ.nem",
    "artnem/mz.nem": "8x8 - MZ.nem",
    "artnem/motobug.nem": "Enemy Motobug.nem",
    "artnem/newtron.nem": "Enemy Newtron.nem",
    "artnem/rings.nem": "Rings.nem",
    "artnem/sbz.nem": "8x8 - SBZ.nem",
    "artnem/shield.nem": "Shield.nem",
    "artnem/slz.nem": "8x8 - SLZ.nem",
    "artnem/spikes.nem": "Spikes.nem",
    "artnem/stars.nem": "Invincibility Stars.nem",
    "artnem/syz.nem": "8x8 - SYZ.nem",
    "artnem/title_card.nem": "Title Cards.nem",
    "artnem/title_fg.nem": "Title Screen Foreground.nem",
    "artnem/title_sonic.nem": "Title Screen Sonic.nem",
    "artnem/title_tm.nem": "Title Screen TM.nem",
    "artnem/vspring.nem": "Spring Vertical.nem",
    "collide/Angle_Map.bin": "Angle Map.bin",
    "collide/Collision_Array_Normal.bin": "Collision Array (Normal).bin",
    "collide/Collision_Array_Rotated.bin": "Collision Array (Rotated).bin",
    "maps/psbtm.asm": "Press Start and TM.asm",
    "maps/rings.asm": "Rings (REV00).asm",
    "maps/signpost.asm": "Signpost.asm",
    "anim/signpost.asm": "Signpost.asm",
    "maps/sonic.asm": "Sonic.asm",
    "maps/titlecard.asm": "Title Cards.asm",
    "maps/titlesonic.asm": "Title Screen Sonic.asm",
    "palette/cycle_water.bin": "Cycle - Title Screen Water.bin",
    "palette/cycle_ghz.bin": "Cycle - GHZ.bin",
    "palette/ghz.bin": "Green Hill Zone.bin",
    "palette/level_select.bin": "Level Select.bin",
    "palette/lz.bin": "Labyrinth Zone.bin",
    "palette/lz_underwater.bin": "Labyrinth Zone Underwater.bin",
    "palette/mz.bin": "Marble Zone.bin",
    "palette/sbz1.bin": "SBZ Act 1.bin",
    "palette/sbz2.bin": "SBZ Act 2.bin",
    "palette/sbz3.bin": "SBZ Act 3.bin",
    "palette/sbz3_underwater.bin": "SBZ Act 3 Underwater.bin",
    "palette/sega_bg.bin": "Sega Background.bin",
    "palette/sega1.bin": "Sega1.bin",
    "palette/sega2.bin": "Sega2.bin",
    "palette/slz.bin": "Star Light Zone.bin",
    "palette/sonic.bin": "Sonic.bin",
    "palette/sonic_lz_underwater.bin": "Sonic - LZ Underwater.bin",
    "palette/sonic_sbz3_underwater.bin": "Sonic - SBZ3 Underwater.bin",
    "palette/syz.bin": "Spring Yard Zone.bin",
    "palette/title.bin": "Title Screen.bin",
    "tilemaps/jap_credits.eni": "Hidden Japanese Credits.eni",
    "tilemaps/sega_logo.eni": "Sega Logo (REV00).eni",
    "tilemaps/title.eni": "Title Screen.eni",
}

ASSET_RE = re.compile(r'load_(?:asm_)?asset\("([^"]+)"')


def extract_needed_assets():
    if not os.path.isfile(DATA_C):
        print(f"[stage] Cannot find {DATA_C}", file=sys.stderr)
        sys.exit(1)
    text = open(DATA_C, "r", encoding="utf-8", errors="ignore").read()
    return sorted(set(ASSET_RE.findall(text)))


def stage() -> int:
    needed = extract_needed_assets()
    if not needed:
        print("[stage] No asset paths extracted from data.c", file=sys.stderr)
        return 1

    os.makedirs(BUILD_ASSETS, exist_ok=True)
    staged = []
    missing = []

    for dest_rel in needed:
        src_name = RENAME_MAP.get(dest_rel, dest_rel.rsplit("/", 1)[-1])
        dest_category = dest_rel.split("/", 1)[0]
        src_category = SRC_DIR_MAP.get(dest_category, dest_category)
        src_rel = dest_rel.replace(dest_category + "/", src_category + "/", 1)
        src_rel = src_rel.rsplit("/", 1)[0] + "/" + src_name

        src_path = os.path.join(DISASM, src_rel)
        if not os.path.isfile(src_path):
            src_dir = os.path.join(DISASM, src_category)
            if os.path.isdir(src_dir):
                base = dest_rel.rsplit("/", 1)[-1]
                for entry in os.listdir(src_dir):
                    if entry.lower() == src_name.lower():
                        src_rel = src_category + "/" + entry
                        src_path = os.path.join(DISASM, src_rel)
                        break

        if not os.path.isfile(src_path):
            missing.append(dest_rel)
            continue

        dest_path = os.path.join(BUILD_ASSETS, dest_rel)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copy2(src_path, dest_path)
        staged.append(dest_rel)

    if missing:
        print("[stage] Missing source files in disasm/ for destinations:", file=sys.stderr)
        for m in missing:
            print(f"  - {m}", file=sys.stderr)

    print(f"[stage] Staged {len(staged)}/{len(needed)} needed assets, missing {len(missing)}")
    return 0 if not missing else 2


if __name__ == "__main__":
    sys.exit(stage())
