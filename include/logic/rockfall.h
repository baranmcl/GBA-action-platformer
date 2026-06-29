#pragma once
namespace logic {
// Minimum rock-free contiguous interior columns the picker guarantees (a dodge lane).
inline constexpr int ROCKFALL_GAP_MIN = 4;

// Pick up to `count` distinct rock columns (tile x) in the interior [1, arena_w_tiles-2], written
// ASCENDING to out[0..n). One column biases to the player's column (clamped interior); the rest are
// spread by an even stride offset by `seed` so repeats vary. Deterministic, no RNG. GUARANTEES a
// rock-free run of >= ROCKFALL_GAP_MIN interior columns (never walls off the arena). Returns n =
// min(count, max_out, interior width). Caller passes seed = a rolling rockfall counter for variety.
inline int rockfall_columns(int player_tx, int arena_w_tiles, int count, int seed,
                            int* out, int max_out){
    const int lo = 1, hi = arena_w_tiles - 2;     // interior column range (inside the walls)
    const int interior = hi - lo + 1;
    if(interior <= 0 || count <= 0 || max_out <= 0) return 0;
    if(count > max_out) count = max_out;
    // Cap so a dodge lane always survives: never fill the interior past (width - GAP_MIN) columns.
    int cap = interior - ROCKFALL_GAP_MIN;
    if(cap < 1) cap = 1;
    if(count > cap) count = cap;

    int tmp[16]; int n = 0;
    auto add = [&](int x){
        if(x < lo) x = lo;
        if(x > hi) x = hi;
        for(int i = 0; i < n; ++i) if(tmp[i] == x) return;   // distinct
        if(n < 16) tmp[n++] = x;
    };
    // 1) the player-biased column.
    add(player_tx);
    // 2) spread the rest on an even stride, offset by seed, skipping collisions. The stride leaves
    //    big gaps for few rocks in a wide arena; the cap above is the hard fairness guarantee.
    int stride = interior / (count + 1); if(stride < 1) stride = 1;
    for(int k = 1; n < count && k <= interior * 2; ++k){
        int x = lo + ((k * stride + seed * 3) % interior);
        add(x);
    }
    // insertion sort ascending into out (n is tiny).
    for(int i = 0; i < n; ++i){
        int v = tmp[i], j = i - 1;
        while(j >= 0 && out[j] > v){ out[j+1] = out[j]; --j; }
        out[j+1] = v;
    }
    return n;
}
}
