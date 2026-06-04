#include "test_framework.h"
#include "logic/dungeon1.h"
using namespace logic;
static Body mk(int x, int y, int hw, int hh){
    Body b{}; b.half_w=Fixed::from_int(hw); b.half_h=Fixed::from_int(hh);
    b.pos={Fixed::from_int(x), Fixed::from_int(y)}; return b;
}
TEST(spronk_freed_on_overlap){
    World w; Body p = mk(10,10,4,4); Body cage = mk(12,12,4,4);
    CHECK(try_free_spronk(p, cage, w) == true);
    CHECK(w.spronk1_freed); CHECK(w.featherleap); CHECK(w.gate1_open);
}
TEST(no_free_when_apart){
    World w; Body p = mk(0,0,4,4); Body cage = mk(40,0,4,4);
    CHECK(try_free_spronk(p, cage, w) == false);
    CHECK(!w.spronk1_freed); CHECK(!w.featherleap); CHECK(!w.gate1_open);
}
TEST(idempotent_after_freed){
    World w; w.spronk1_freed=true; w.featherleap=true; w.gate1_open=true;
    Body p = mk(0,0,4,4); Body cage = mk(40,0,4,4); // not overlapping
    CHECK(try_free_spronk(p, cage, w) == true); // stays freed
    CHECK(w.featherleap);
}
