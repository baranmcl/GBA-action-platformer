// Current-writer (v3) coverage lives in test_world_state_v3.cpp; this file keeps v1->migration + corruption-rejection cases.
#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;
TEST(world_fresh_defaults){ World w; CHECK_EQ((int)w.spronks_freed,0); CHECK_EQ((int)w.abilities,0); CHECK(!w.has(Ability::Featherleap)); }
TEST(world_free_and_grant){ World w; w.free_spronk(1); w.grant(Ability::Featherleap);
  CHECK(w.spronk_freed(1)); CHECK(!w.spronk_freed(2)); CHECK(w.has(Ability::Featherleap)); CHECK(!w.has(Ability::Fire)); }
TEST(save_roundtrip_basic){ World w; w.free_spronk(1); w.grant(Ability::Featherleap); w.grant(Ability::Fire); w.current_dungeon=2;
  SaveData s = make_save(w); World w2; CHECK(load_save(s,w2)==true);
  CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Featherleap)); CHECK(w2.has(Ability::Fire)); CHECK_EQ((int)w2.current_dungeon,2); }
TEST(empty_sram_rejected){ SaveData s{}; World w2; CHECK(load_save(s,w2)==false); }
TEST(bad_checksum_rejected){ World w; SaveData s=make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s,w2)==false); }
TEST(v1_save_migrates){ // build a v1-layout 16-byte buffer (v1 was 12 bytes; rest zero)
  uint8_t buf[sizeof(SaveData)]; std::memset(buf,0,sizeof(buf));
  uint32_t magic=0x5350524B; std::memcpy(buf,&magic,4);
  uint16_t ver=1; std::memcpy(buf+4,&ver,2);
  uint16_t flags=0x0007; std::memcpy(buf+6,&flags,2);   // v1: spronk1|featherleap|gate1
  uint32_t sum=0; for(int i=0;i<8;++i) sum+=buf[i]; buf[8]=(uint8_t)(sum&0xFF); // v1 checksum at offset 8
  SaveData s; std::memcpy(&s,buf,sizeof(SaveData));
  World w2; CHECK(load_save(s,w2)==true);
  CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Featherleap)); }
