#pragma once
namespace engine {
// Spell controls: R = cast the selected spell, L = cycle the selected spell.
// (A = jump, B = bolt stay in engine/input.)
struct SpellIntent { bool cast; bool cycle; };
SpellIntent read_spell_intent();
}
