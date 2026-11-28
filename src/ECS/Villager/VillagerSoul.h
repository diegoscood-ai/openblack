/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack
{
enum class MeshId : uint32_t;
}

// The soul of a dead villager, as the original draws it: made when the villager dies, on its first DEAD turn out of
// the water: a translucent white copy of the villager
// playing its "goto heaven / hell" clip (the clip moves it), alpha 105, fading out in its last 500 ms. In openblack an
// entity with a Transform, Mesh, SkeletalAnimation (the clip), Alpha (the translucent pass), ObjectColour (white) and
// components::VillagerSoul; no Villager, so nothing else treats it as one.

namespace openblack::ecs::components
{
struct VillagerSoul
{
	uint32_t elapsedMs {0};  ///< Time since the soul was made
	uint32_t durationMs {0}; ///< The clip's duration
};
} // namespace openblack::ecs::components

namespace openblack::ecs::villager_soul
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// Random(0, 100) (the CRT stream, drawn always) < 50 -> heaven, else hell; the source's current clip name
/// "M_P_DEAD1" -> the pair (247 P_DEAD2_GOTO_HEAVEN, 248 P_DEAD2_GOTO_HELL), else (244 P_DEAD1_GOTO_HEAVEN,
/// 245 P_DEAD1_GOTO_HELL) (literal: the pair looks inverted); heavenForced (a child) -> the heaven clip after the roll
[[nodiscard]] int32_t SoulClip(bool sourceIsDead1Name, bool heavenForced, float roll);
/// 105; in the last 500 ms (elapsed > duration - 500, signed) (1 - (elapsed - (duration - 500)) x 0.002) x 105,
/// truncated toward zero
[[nodiscard]] uint8_t SoulAlpha(uint32_t elapsed, uint32_t duration);
/// elapsed + 110 > duration (signed) -> the soul is freed
[[nodiscard]] bool SoulExpired(uint32_t elapsed, uint32_t duration);

/// One soul's step by a drawn frame's game time
struct SoulStep
{
	uint32_t elapsedMs {0}; ///< The time since the soul was made, with the frame's added
	bool expired {false};   ///< The soul is freed this frame, before it is drawn
	uint8_t alpha {0};      ///< Its alpha this frame (SoulAlpha), when it is not freed
};
/// The frame's game time is added first, then the soul is freed (SoulExpired) or given the frame's alpha
[[nodiscard]] SoulStep StepSoul(uint32_t elapsed, uint32_t duration, uint32_t frameMilliseconds);

// ---- the souls ---------------------------------------------------------------------------------------------------

/// The soul at the source's position and orientation, mesh = MeshPack[mesh], its clip (SoulClip); returns the clip
/// (-1 when nothing was made)
int32_t Create(entt::entity source, MeshId mesh, bool heavenForced);
/// Every soul's step (StepSoul) by the frame's game time, as the land's objects are drawn: after the objects' own
/// draws and before the bookmarks, only on frames that draw the land (Game.cpp). A soul that is not freed is posed
/// at its time and given its alpha, so what is drawn is its own clock's, whatever the animations did in between
void Update(uint32_t frameMilliseconds);
/// The souls that are up, the newest first, as the frame's draw takes them
[[nodiscard]] std::vector<entt::entity> NewestFirst();
/// The list is emptied with the land's entities (a new registry gives their ids out again)
void Clear();
} // namespace openblack::ecs::villager_soul
