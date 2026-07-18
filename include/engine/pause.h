#pragma once
namespace engine {
// If START was just pressed, freeze the frame and show "GAME PAUSED" until START is pressed again.
// Call once per gameplay loop (boss / dungeon / hub). No-op when START wasn't pressed this frame.
void check_pause();

// Debug-only CPU meter toggle. OFF by default -- normal players never see the "CPU nn% / MAX nn%"
// line on the pause screen, only "GAME PAUSED". main.cpp flips this on for the duration of a
// debug-launched session (alongside set_save_enabled(false)) and back off afterward.
void set_cpu_meter_enabled(bool enabled);
}
