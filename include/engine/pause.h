#pragma once
namespace engine {
// If START was just pressed, freeze the frame and show "GAME PAUSED" until START is pressed again.
// Call once per gameplay loop (boss / dungeon / hub). No-op when START wasn't pressed this frame.
void check_pause();
}
