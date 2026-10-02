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

/// ?Random@@YAMMM@Z 0x81D180: ((rand() x k) x (b - a)) + a, k [0x9A3700] (about 1 / 32767), so b itself can come out.
/// Forwards to game_random::crt::Random: the one CRT seed of the game thread, 1 at the start (__initptd 0x7D2323)
float Random(float a, float b) noexcept;

} // namespace openblack::graphics::lh3d

/// GRand's local stream (the unsynced one, LHRand on g_game +0x205A3C): forwards to game_random::LocalRand /
/// LocalFloatRand (Common/GameRandom.h), kept for the callers not migrated yet
namespace openblack::grand_local
{

/// GRand::LocalRand 0x6DE570: 0 for 0 (0x6DE574), else 0 .. count - 1 (LHRand(count) 0x6DE587)
int LocalRand(int count) noexcept;
/// GRand::LocalFloatRand 0x6DE590: 0 for 0 (0x6DE597..0x6DE5AD), else (LHRand(0xFFFF) x max) x [0x8D6050]
/// (0x6DE5B9..0x6DE5DA)
float LocalFloatRand(float max) noexcept;

} // namespace openblack::grand_local
