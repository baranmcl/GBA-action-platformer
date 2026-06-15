#pragma once
#include "logic/level_data.h"
#include "game/levels/dungeon1.h"
#include "game/levels/dungeon2.h"
#include "game/levels/dungeon3.h"
#include "game/levels/dungeon4.h"
#include "game/levels/dungeon5.h"
#include "game/levels/dungeon6_room0.h"
#include "game/levels/dungeon6_room1.h"
#include "game/levels/dungeon6_room2.h"
#include "game/levels/dungeon7_room0.h"
#include "game/levels/dungeon7_room1.h"
#include "game/levels/dungeon7_room2.h"
#include "game/levels/dungeon8_room0.h"
#include "game/levels/dungeon8_room1.h"
#include "game/levels/dungeon8_room2.h"

inline constexpr const logic::LevelData* DUNGEON1_ROOMS[] = { &DUNGEON1_DATA };
inline constexpr logic::DungeonData DUNGEON1_DUNGEON{ DUNGEON1_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON2_ROOMS[] = { &DUNGEON2_DATA };
inline constexpr logic::DungeonData DUNGEON2_DUNGEON{ DUNGEON2_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON3_ROOMS[] = { &DUNGEON3_DATA };
inline constexpr logic::DungeonData DUNGEON3_DUNGEON{ DUNGEON3_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON4_ROOMS[] = { &DUNGEON4_DATA };
inline constexpr logic::DungeonData DUNGEON4_DUNGEON{ DUNGEON4_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON5_ROOMS[] = { &DUNGEON5_DATA };
inline constexpr logic::DungeonData DUNGEON5_DUNGEON{ DUNGEON5_ROOMS, 1, 0 };
// DUNGEON6 — Thornwild Marsh (M7): a branching multi-room grapple dungeon. Room 0 (entry) holds the
// Grapple shrine + a first grapple beat and branches to Branch A (room 1: pull-block puzzle + a
// brazier-group that latches open a shortcut door back to room 0) and Branch B (room 2: grapple-gated,
// holds the spronk + exit). Not yet reachable from the hub (Door 6 wired in M7 Task 4.2).
inline constexpr const logic::LevelData* DUNGEON6_ROOMS[] = {
    &DUNGEON6_ROOM0_DATA, &DUNGEON6_ROOM1_DATA, &DUNGEON6_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON6_DUNGEON{ DUNGEON6_ROOMS, 3, 0 };
// DUNGEON7 — Quaking Quarry (M8): a 3-room Stone ground-pound descent dungeon. Room 0 (entry) holds
// the Stone shrine + a tutorial cracked floor + a heavy switch, and branches to Room 1 (Cut & Fill:
// loose-platform bridge, a latched cracked-floor shortcut, a boulder-gated heart container) and
// Room 2 (the Deep Shaft: stacked cracked floors, a fire-immune crush, an Ice water-gap, the spronk).
inline constexpr const logic::LevelData* DUNGEON7_ROOMS[] = {
    &DUNGEON7_ROOM0_DATA, &DUNGEON7_ROOM1_DATA, &DUNGEON7_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON7_DUNGEON{ DUNGEON7_ROOMS, 3, 0 };
// DUNGEON8 — Gloom Spire (M10): a 3-room Light-spell ascent dungeon. Room 0 (Spire Base) holds the
// Light shrine (base-reachable, before the Light-gated beats), a tutorial timed-reveal hidden-platform
// climb with a magic crystal at its base, a DarkVeil gate cleared with Light, and branches to Room 1
// (an optional grapple/Light branch with a latched cracked-floor shortcut) and Room 2 (The Ascent: a
// vertical timed-reveal climb of hidden platforms, with magic crystals on each rest ledge, topped by
// the spronk + exit). Reached from the hub via Door 8 once D7's spronk is freed (M10 Task 4.2).
inline constexpr const logic::LevelData* DUNGEON8_ROOMS[] = {
    &DUNGEON8_ROOM0_DATA, &DUNGEON8_ROOM1_DATA, &DUNGEON8_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON8_DUNGEON{ DUNGEON8_ROOMS, 3, 0 };
