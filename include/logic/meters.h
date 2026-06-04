#pragma once
namespace logic {
// One continuous meter type (integer units) serves both health and magic.
struct Meter {
    int cur, max;
    void damage(int n){ cur -= n; if(cur < 0) cur = 0; }
    void heal(int n){ cur += n; if(cur > max) cur = max; }     // also used to refill magic
    bool spend(int n){ if(cur < n) return false; cur -= n; return true; } // no partial spend
    bool is_empty() const { return cur <= 0; }
};
}
