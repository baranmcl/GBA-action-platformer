#pragma once
namespace engine {
// Screen fade transitions (to/from black). Each runs its own bn::core::update() loop.
// CONVENTION: a scene calls fade_in() right after building its visuals, and fade_out()
// just before returning (while its sprites/bg are still alive). The caller does NOT fade.
void fade_out(int frames); // screen -> black
void fade_in(int frames);  // black -> screen
}
