import json
import os
import tempfile
import unittest

import validate_dungeons as vd


def write_room(levels_dir, name, txt, meta=None):
    with open(os.path.join(levels_dir, name + '.txt'), 'w', encoding='utf-8') as f:
        f.write(txt)
    with open(os.path.join(levels_dir, name + '.json'), 'w', encoding='utf-8') as f:
        json.dump(meta or {}, f)


def write_manifest(levels_dir, manifest):
    with open(os.path.join(levels_dir, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f)


# A minimal two-way two-room dungeon: room0 <-> room1, both entrance id 0.
ROOM_TXT = "######\n#@ND.#\n######\n"


def make_two_room_dungeon(levels_dir, room1_extra_meta=None):
    write_room(levels_dir, 'room0', ROOM_TXT, {
        "entrances": [{"id": 0, "facing": 1}],
        "room_doors": [{"target_room": 1, "target_entrance": 0}],
    })
    meta1 = {
        "entrances": [{"id": 0, "facing": 1}],
        "room_doors": [{"target_room": 0, "target_entrance": 0}],
    }
    if room1_extra_meta:
        meta1.update(room1_extra_meta)
    write_room(levels_dir, 'room1', ROOM_TXT, meta1)
    write_manifest(levels_dir, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})


class TestRoomGraph(unittest.TestCase):
    def test_happy_path_no_errors(self):
        with tempfile.TemporaryDirectory() as d:
            make_two_room_dungeon(d)
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])
            self.assertEqual(warnings, [])

    def test_oob_target_room_errors(self):
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 5, "target_entrance": 0}],  # OOB: only 2 rooms
            })
            write_room(d, 'room1', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('target_room' in e and '5' in e for e in errors), errors)

    def test_unknown_target_entrance_errors(self):
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 99}],  # room1 has no entrance 99
            })
            write_room(d, 'room1', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('target_entrance' in e and '99' in e for e in errors), errors)

    def test_one_way_door_is_warning_not_error(self):
        # room0 -> room1 exists, but room1 has NO door back to room0: a one-way edge.
        # Per the brief this must be a WARNING (stderr), never an error.
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', "######\n#@N..#\n######\n", {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [],  # no return door -> one-way
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])
            self.assertTrue(any('one-way' in w or 'return' in w for w in warnings), warnings)


class TestLatchRegistry(unittest.TestCase):
    def test_cross_room_duplicate_latch_id_errors(self):
        with tempfile.TemporaryDirectory() as d:
            room_txt = "######\n#@k..#\n######\n"
            make_two_room_dungeon(d)
            # Overwrite room0/room1 with cracked-floor gates sharing latch_id=3 across rooms.
            write_room(d, 'room0', room_txt, {
                "cracked_floors": [{"latch_id": 3}],
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', room_txt, {
                "cracked_floors": [{"latch_id": 3}],
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('latch_id 3' in e for e in errors), errors)

    def test_same_room_duplicate_latch_id_allowed(self):
        # Two 'k' symbols in the SAME room sharing a latch_id (D7's pattern) must NOT error.
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', "########\n#@Nk.k.#\n########\n", {
                "cracked_floors": [{"latch_id": 3}, {"latch_id": 3}],
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])

    def test_latch_id_out_of_range_errors(self):
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', "######\n#@k..#\n######\n", {
                "cracked_floors": [{"latch_id": 24}],  # out of [0,23]
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('latch_id 24' in e for e in errors), errors)


class TestHeartRegistry(unittest.TestCase):
    def test_duplicate_heart_id_across_rooms_errors(self):
        with tempfile.TemporaryDirectory() as d:
            room_txt = "######\n#@.H.#\n######\n"
            write_room(d, 'room0', room_txt, {
                "heart_containers": [{"id": 2}],
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', room_txt, {
                "heart_containers": [{"id": 2}],  # duplicate of room0's id 2
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('heart id 2' in e for e in errors), errors)

    def test_heart_id_out_of_range_errors(self):
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', "######\n#@.H.#\n######\n", {
                "heart_containers": [{"id": 8}],  # out of [0,7]
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 1, "target_entrance": 0}],
            })
            write_room(d, 'room1', ROOM_TXT, {
                "entrances": [{"id": 0, "facing": 1}],
                "room_doors": [{"target_room": 0, "target_entrance": 0}],
            })
            write_manifest(d, {"dungeons": {"1": ["room0", "room1"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('heart id 8' in e for e in errors), errors)


# A boss-arena room: 8 wide x 5 tall, solid floor rows at h-2 (=3) and h-1 (=4, border).
# w=8 -> w//2=4, so columns 3,4,5 must be solid at row 3.
BOSS_ROOM_TXT = (
    "########\n"
    "#......#\n"
    "#..@...#\n"
    "########\n"
    "########\n"
)


class TestArenaConstraints(unittest.TestCase):
    def test_boss_room_without_crystal_ok(self):
        # Zero crystals in a boss arena is LEGAL (e.g. D1's TiredWindow needs no magic;
        # D3's Coldforge Twins recharge via block-to-charge) — must not error.
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', BOSS_ROOM_TXT, {"boss": "d1"})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])

    def test_boss_room_with_one_crystal_ok(self):
        txt = (
            "########\n"
            "#......#\n"
            "#..@.$.#\n"
            "########\n"
            "########\n"
        )
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', txt, {"boss": "d1"})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])

    def test_boss_room_with_two_crystals_errors(self):
        # The boss fight loop only ever reads magic_crystals[0]; a second crystal in a
        # boss room is dead content and must be flagged.
        txt = (
            "##########\n"
            "#........#\n"
            "#..@.$.$.#\n"
            "##########\n"
            "##########\n"
        )
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', txt, {"boss": "d1"})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('magic crystal' in e for e in errors), errors)

    def test_non_boss_room_ignored_by_arena_check(self):
        # No "boss" key -> arena constraints don't apply, even with no crystal.
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', BOSS_ROOM_TXT, {})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])

    def test_boss_room_floor_gap_errors(self):
        # Boss arena floor must be solid at (w/2 - 1, h-2), (w/2, h-2), and (w/2 + 1, h-2).
        # Replace one of the solid floor tiles with '.' to trigger the error.
        # BOSS_ROOM_TXT has w=8, h=5, so w//2=4 and floor_y=3.
        # Checked positions: x in {3, 4, 5} at row 3. Replace x=3 with '.'.
        txt = (
            "########\n"
            "#......#\n"
            "#..@...#\n"
            "###.####\n"
            "########\n"
        )
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', txt, {"boss": "d1"})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('arena floor not solid' in e for e in errors), errors)


class TestSpriteBudget(unittest.TestCase):
    def test_sprite_budget_exceeded_errors(self):
        # 8 loose platforms (cap 8) x len 8 (cap 8) = 64, + 8 hidden platforms x len 8 = 64:
        # 64 + 64 + 45 = 173 > 110, while respecting every per-room cap in build_level.py.
        with tempfile.TemporaryDirectory() as d:
            txt = "#" * 20 + "\n" + "#@" + (":" * 8) + ("h" * 8) + ".#" + "\n" + "#" * 20 + "\n"
            write_room(d, 'room0', txt, {
                "loose_platforms": [{"len": 8}] * 8,
                "hidden_platforms": [{"len": 8}] * 8,
            })
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertTrue(any('budget' in e for e in errors), errors)

    def test_sprite_budget_within_cap_ok(self):
        with tempfile.TemporaryDirectory() as d:
            write_room(d, 'room0', "######\n#@...#\n######\n", {})
            write_manifest(d, {"dungeons": {"1": ["room0"]}, "standalone": []})
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d)
            self.assertEqual(errors, [])


class TestDungeonsHDrift(unittest.TestCase):
    def test_matching_dungeons_h_ok(self):
        with tempfile.TemporaryDirectory() as d:
            make_two_room_dungeon(d)
            dh_path = os.path.join(d, 'dungeons.h')
            with open(dh_path, 'w', encoding='utf-8') as f:
                f.write(
                    "inline constexpr const logic::LevelData* DUNGEON1_ROOMS[] = {\n"
                    "    &ROOM0_DATA, &ROOM1_DATA };\n"
                )
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d, dh_path)
            self.assertEqual(errors, [])

    def test_mismatched_dungeons_h_errors(self):
        with tempfile.TemporaryDirectory() as d:
            make_two_room_dungeon(d)
            dh_path = os.path.join(d, 'dungeons.h')
            with open(dh_path, 'w', encoding='utf-8') as f:
                f.write(
                    "inline constexpr const logic::LevelData* DUNGEON1_ROOMS[] = {\n"
                    "    &ROOM1_DATA, &ROOM0_DATA };\n"  # order swapped -> drift
                )
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d, dh_path)
            self.assertTrue(any('drift' in e for e in errors), errors)

    def test_missing_array_in_dungeons_h_errors(self):
        with tempfile.TemporaryDirectory() as d:
            make_two_room_dungeon(d)
            dh_path = os.path.join(d, 'dungeons.h')
            with open(dh_path, 'w', encoding='utf-8') as f:
                f.write("// no DUNGEON1_ROOMS array here\n")
            errors, warnings = vd.validate_all(os.path.join(d, 'manifest.json'), d, dh_path)
            self.assertTrue(any('DUNGEON1_ROOMS' in e for e in errors), errors)


if __name__ == '__main__':
    unittest.main()
