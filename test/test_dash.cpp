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
