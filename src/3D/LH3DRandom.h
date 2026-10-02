/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The 3D engine's own random numbers (not the game's synced GRand, so the game's sequence is left alone)
namespace openblack::graphics::lh3d
{

/// ?Random@@YAMMM@Z 0x81D180: a + (b - a) x rand() x (1 / 32767) ([0x9A3700]), so b itself can come out; rand() is the
/// MSVC CRT one 0x7C8837 (s = s x 214013 + 2531011, (s >> 16) & 0x7FFF). (aproximado) one private stream from seed 1,
/// shared by the users that call this (the LH3DMist ctors of the map mists and of the PSys mists, the storm puffs); the
/// original's rand() is the whole program's CRT sequence, seeded by srand(time) (fn_005776E0 0x577721)
float Random(float a, float b) noexcept;

} // namespace openblack::graphics::lh3d

/// GRand's local stream (the unsynced one, LHRand on g_game +0x205A3C), a stand-in shared by its callers until the
/// game_random module exists (unify2 PLAN §5, L1 / L4): FireGraphic (Object::GetPSysFireLocalRndFlamePos) and the PSys
/// light maps' jitter (ParticleLightMap::DrawAt 0x67B264..0x67B2A3). (aproximado) another generator (one private
/// std::mt19937 stream, seed 0x5EED), so the sequences differ from the original's
namespace openblack::grand_local
{

/// GRand::LocalRand 0x6DE570: 0 for 0 (0x6DE574), else 0 .. count - 1 (LHRand(count) 0x6DE587)
int LocalRand(int count) noexcept;
/// GRand::LocalFloatRand 0x6DE590: 0 for 0 (0x6DE597..0x6DE5AD), else LHRand(0xFFFF) x max x (1 / 65535) ([0x8D6050],
/// 0x6DE5B9..0x6DE5DA): [0, max)
float LocalFloatRand(float max) noexcept;

} // namespace openblack::grand_local
