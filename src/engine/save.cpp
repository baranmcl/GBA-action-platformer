#include "engine/save.h"
#include "bn_sram.h"
namespace engine {
void write_world(const logic::World& w){
    logic::SaveData s = logic::make_save(w);
    bn::sram::write(s);
}
bool read_world(logic::World& out){
    logic::SaveData s;
    bn::sram::read(s);
    return logic::load_save(s, out); // rejects empty/corrupt SRAM via magic+checksum
}
}
