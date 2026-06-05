#pragma once
namespace engine {
// Screen fade transitions (to/from black). Each runs its own bn::core::update() loop.
// CONVENTION: a scene calls fade_in() right after building its visuals, and fade_out()
// just before returning (while its sprites/bg are still alive). The caller does NOT fade.
void fade_out(int frames); // screen -> black (blocking; runs its own update loop)
void fade_in(int frames);  // black -> screen (blocking)

// Set the fade level directly without running a loop (for a LIVE fade-in interleaved
// with a gameplay loop): 0 = fully clear, 16 = fully black.
void set_fade(int level16);
}
