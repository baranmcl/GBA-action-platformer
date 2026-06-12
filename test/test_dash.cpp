#include "test_framework.h"
#include "logic/dash.h"
using namespace logic;
// helper: run the 3-frame double-tap (press, release, press) for `right`
static void dtap_right(DashState& d, bool on_ground, bool has){
  d.tick(false,true,on_ground,has);   // press
  d.tick(false,false,on_ground,has);  // release
  d.tick(false,true,on_ground,has);   // press again -> dash
}
TEST(dash_on_double_tap_when_owned){
  DashState d; dtap_right(d,true,true);
  CHECK(d.active()); CHECK_EQ(d.dir(),1); CHECK(d.invincible()); }
TEST(no_dash_without_ability){
  DashState d; dtap_right(d,true,false); CHECK(!d.active()); }
TEST(single_tap_does_not_dash){
  DashState d; d.tick(false,true,true,true); CHECK(!d.active()); }
TEST(too_slow_double_tap_does_not_dash){
  DashState d;                                  // second tap arrives after the WINDOW expires -> re-arm, no dash
  d.tick(false,true,true,true);                 // press (arm, timer=WINDOW)
  for(int i=0;i<DashState::WINDOW;++i) d.tick(false,false,true,true); // hold release past the window
  d.tick(false,true,true,true);                 // press again, but timer already 0
  CHECK(!d.active()); }
TEST(air_dash_consumes_charge_until_landing){
  DashState d;
  d.tick(false,false,true,true);   // grounded once -> charged (charged defaults FALSE, so this is required)
  // double-tap in the AIR -> dash, charge spent
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true);
  CHECK(d.active());
  for(int i=0;i<DashState::DASH_FRAMES;++i) d.tick(false,false,false,true);  // dash ends, still airborne
  CHECK(!d.active());
  // second air double-tap with NO landing -> no dash (charge not refreshed)
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true);
  CHECK(!d.active()); }
TEST(dash_recharges_on_ground){
  DashState d;
  d.tick(false,false,true,true);   // grounded once -> charged
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true); // air dash
  for(int i=0;i<DashState::DASH_FRAMES;++i) d.tick(false,false,true,true);   // land (on_ground) -> recharge
  d.tick(false,true,true,true); d.tick(false,false,true,true); d.tick(false,true,true,true);     // dash again
  CHECK(d.active()); }
TEST(dash_left_on_double_tap_left){
  DashState d;
  // grounded once to charge
  d.tick(false,false,true,true);
  // double-tap LEFT: press, release, press
  d.tick(true,false,true,true);   // press left
  d.tick(false,false,true,true);  // release
  d.tick(true,false,true,true);   // press left again -> should dash
  CHECK(d.active()); CHECK_EQ(d.dir(),-1); }
TEST(forgiving_window_allows_slower_double_tap){
  // A 15-frame gap between taps used to MISS the old 12-frame window; the widened WINDOW (20) catches it.
  DashState d; d.tick(false,false,true,true);    // grounded -> charged
  d.tick(false,true,true,true);                  // press right (arm, timer=WINDOW)
  for(int i=0;i<15;++i) d.tick(false,false,true,true); // release for 15 frames (>12, <20)
  d.tick(false,true,true,true);                  // press right again -> dash (still within the 20-frame window)
  CHECK(d.active()); CHECK_EQ(d.dir(),1);
  static_assert(DashState::WINDOW >= 16, "window must stay forgiving enough for this gap"); }
