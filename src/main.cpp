#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"

// Milestone 1, Task 0.3: prove the Butano build pipeline by booting to a solid
// backdrop. Real scenes arrive in Phase 3.
int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24)); // deep night-blue

    while(true)
    {
        bn::core::update();
    }
}
