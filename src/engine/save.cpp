#include "engine/save.h"
#include "bn_sram.h"
namespace engine {
namespace {
    constexpr int SLOT_B_OFFSET = 32; // SaveDataV6 is 24 bytes; 8 bytes slack before slot B
    uint16_t s_seq = 0;
    int s_next_slot = 0;
    bool s_loaded = false;
}
bool read_world(logic::World& out){
    logic::SaveDataV6 a{}, b{};
    logic::SaveData legacy{};
    bn::sram::read_offset(a, 0);
    bn::sram::read_offset(b, SLOT_B_OFFSET);
    bn::sram::read_offset(legacy, 0); // legacy v1-v5 layout shares offset 0 with slot A
    logic::SaveDecision d = logic::decide_load(a, b, legacy);
    s_seq = d.seq; s_next_slot = d.next_slot; s_loaded = true;
    if(d.valid) out = d.world;
    return d.valid;
}
void write_world(const logic::World& w){
    if(!s_loaded){ logic::World tmp; read_world(tmp); } // arbitration state before first write
    ++s_seq;
    logic::SaveDataV6 s = logic::make_save_v6(w, s_seq);
    bn::sram::write_offset(s, s_next_slot == 0 ? 0 : SLOT_B_OFFSET);
    s_next_slot ^= 1;
}
}
