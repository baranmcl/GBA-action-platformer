#include "test_framework.h"
#include "logic/world_state.h"
using namespace logic;
TEST(fresh_world_defaults){ World w; CHECK(w.spronk1_freed==false); CHECK(w.featherleap==false); }
TEST(save_roundtrip){ World w; w.spronk1_freed=true; w.featherleap=true; w.gate1_open=true;
  SaveData s = make_save(w); World w2; CHECK(load_save(s,w2)==true);
  CHECK(w2.spronk1_freed); CHECK(w2.featherleap); CHECK(w2.gate1_open); }
TEST(empty_sram_rejected){ SaveData s{}; World w2; CHECK(load_save(s,w2)==false); }
TEST(bad_checksum_rejected){ World w; SaveData s=make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s,w2)==false); }
