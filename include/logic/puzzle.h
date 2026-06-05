#pragma once
#include <cstdint>
namespace logic {
enum class TriggerKind : uint8_t { Braziers, Plate, Button };
struct Trigger {
    TriggerKind kind;
    int total = 0;     // braziers: how many to light
    int lit = 0;       // braziers: how many lit
    bool pressed = false; // plate/button
    int target_tx = 0, target_ty = 0; // tile the scene opens when active()
    static Trigger braziers(int total){ Trigger t; t.kind=TriggerKind::Braziers; t.total=total; return t; }
    static Trigger plate(){ Trigger t; t.kind=TriggerKind::Plate; return t; }
    static Trigger button(){ Trigger t; t.kind=TriggerKind::Button; return t; }
    bool active() const {
        switch(kind){
            case TriggerKind::Braziers: return total>0 && lit>=total;
            case TriggerKind::Plate:    return pressed;
            case TriggerKind::Button:   return pressed;
        }
        return false;
    }
};
}
