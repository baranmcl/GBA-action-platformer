#!/usr/bin/env bash
# Build the Spronk Quest GBA ROM through devkitPro's bundled MSYS2 environment.
#
# Why this script: the ROM build must run inside devkitPro's MSYS2 shell, where
# C:\devkitPro is mounted at /opt/devkitpro and DEVKITPRO/DEVKITARM are set. That
# MSYS2 has no `python`, which Butano's build pipeline (grit/codegen) requires, so
# we prepend the Windows Python 3.12 (which has Pillow) to PATH. Run from anywhere:
#   bash tools/build_rom.sh            # build  -> build/SpronkQuest.gba
#   bash tools/build_rom.sh clean      # clean
#   bash tools/build_rom.sh -j8        # pass make args
set -euo pipefail

DKP_BASH="/c/devkitPro/msys2/usr/bin/bash.exe"
REPO="/c/Users/baranmcl/Code/GBA-action-platformer"
WINPY="/c/Users/baranmcl/AppData/Local/Programs/Python/Python312"

MAKE_ARGS="${*:-"-j8"}"

exec "$DKP_BASH" -lc "cd '$REPO' && export PATH=\"$WINPY:\$PATH\" && make $MAKE_ARGS"
