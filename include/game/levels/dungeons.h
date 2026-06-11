#pragma once
#include "logic/level_data.h"
#include "game/levels/dungeon1.h"
#include "game/levels/dungeon2.h"
#include "game/levels/dungeon3.h"
#include "game/levels/dungeon4.h"
#include "game/levels/dungeon5.h"
#include "game/levels/d6_room0.h"
#include "game/levels/d6_room1.h"

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
inline constexpr const logic::LevelData* DUNGEON6_ROOMS[] = { &D6_ROOM0_DATA, &D6_ROOM1_DATA };
inline constexpr logic::DungeonData DUNGEON6_DUNGEON{ DUNGEON6_ROOMS, 2, 0 };
