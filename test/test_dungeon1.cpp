#include "test_framework.h"
#include "logic/dungeon1.h"
using namespace logic;
static Body mk(int x, int y, int hw, int hh){
    Body b{}; b.half_w=Fixed::from_int(hw); b.half_h=Fixed::from_int(hh);
    b.pos={Fixed::from_int(x), Fixed::from_int(y)}; return b;
}
TEST(spronk_freed_on_overlap){
    World w; Body p = mk(10,10,4,4); Body cage = mk(12,12,4,4);
    CHECK(try_free_spronk(p, cage, w, 1) == true);
    CHECK(w.spronk_freed(1)); CHECK(!w.spronk_freed(2));
}
TEST(no_free_when_apart){
    World w; Body p = mk(0,0,4,4); Body cage = mk(40,0,4,4);
    CHECK(try_free_spronk(p, cage, w, 1) == false);
    CHECK(!w.spronk_freed(1));
}
TEST(idempotent_after_freed){
    World w; w.free_spronk(1);
    Body p = mk(0,0,4,4); Body cage = mk(40,0,4,4); // not overlapping
    CHECK(try_free_spronk(p, cage, w, 1) == true); // stays freed
    CHECK(w.spronk_freed(1));
}
