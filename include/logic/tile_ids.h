#pragma once
namespace logic { namespace tiles {
// graphics/tiles.bmp strip indices — THE registry (was a comment in gates.h).
// make_placeholder_art.py gen_tiles draws these slots; keep in sync when adding art.
inline constexpr int BLANK=0, GROUND=1, ONE_WAY=2, GATE_CLOSED=3, CAGE=4,
    DOOR_OPEN=5, DOOR_LOCKED=6, VINE=7, ICE_WALL=8, WATERFALL=9, FIREWALL=10,
    CRACKED_FLOOR=11, DARK_VEIL=12, LAVA=13, BRAZIER_UNLIT=14, BRAZIER_LIT=15,
    WATER=16, PLATE=17, BUTTON=18, ICE_PLATFORM=19, UPDRAFT=20, WIND_LEFT=21,
    WIND_RIGHT=22, CRACKED_WALL=23, SPIKES=24, GRAPPLE_ANCHOR=25, HUB_PORTAL=26;
// collision TileKind -> bg tile index. Identity for 0/1/2 (blank/ground/one-way);
// Lava(3)->13, Water(4)->16, IcePlatform(5)->19, Updraft(6)->20, WindLeft(7)->21,
// WindRight(8)->22, Spikes(9)->24, GrapplePoint(10)->25. Gates/doors/entities overlaid via set_level_tile.
inline constexpr int bg_for_kind(int kind){
    return kind==3?LAVA : kind==4?WATER : kind==5?ICE_PLATFORM : kind==6?UPDRAFT
         : kind==7?WIND_LEFT : kind==8?WIND_RIGHT : kind==9?SPIKES
         : kind==10?GRAPPLE_ANCHOR : kind;
}
}}
