/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/LandMorph.h"
#include "Enums.h"

// The temple's three leash posts, one for each leash: aggression, learning and compassion, in that order. Each hangs
// at one of the temple's special points, shows its leash's collar slowly turning with a puff of smoke behind it, and is
// tapped by its player to pick that leash. The temple keeps which one is picked. Pure rules, with no game state: the
// caller passes in the temple's matrix, its special points, the land, the random draws and the frame time.
// (wiki: creature.md, "The temple's leash posts")

namespace openblack::worship::leash_posts
{
/// The posts of a temple
constexpr size_t k_Count = 3;

/// The leash of post `post` (0, 1, 2): aggression, learning, compassion
[[nodiscard]] constexpr LeashType TypeOf(size_t post) noexcept
{
	return static_cast<LeashType>(static_cast<int>(post) + 1);
}

// ---- Where a post stands --------------------------------------------------------------------------------------------

/// Where post `post` stands: the translation of the temple mesh's special point `post` through the temple's matrix
/// (`temple` in openblack's layout: column k is the original's row k, column 3 the position), every product and sum a
/// float one in the original's order. With no such point, the temple's own position
[[nodiscard]] glm::vec3 Point(const glm::mat4& temple, std::span<const glm::mat4> specialPoints, size_t post);

/// The same for a temple that follows the land, as the temple always does: a point that exists is lifted by the land's
/// rise from the temple's position to it, (ground(point) - ground(temple)) + y. The temple's own position, used when
/// the point is missing, is not lifted
[[nodiscard]] glm::vec3 PointOnLand(const glm::mat4& temple, std::span<const glm::mat4> specialPoints, size_t post,
                                    const land_morph::Ground& ground);

// ---- What the frame keeps of a post ---------------------------------------------------------------------------------

/// The ranges of the four random draws a post takes when it is made, in the order they are drawn: the collar's scroll,
/// its two turns, the smoke's frame clock. A temple draws them post by post, 12 in all
struct DrawRange
{
	float low;
	float high;
};
inline constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu); ///< 2 pi as a float
inline constexpr std::array<DrawRange, 4> k_Draws = {{{0.0f, 1.0f}, {0.0f, k_TwoPi}, {0.0f, k_TwoPi}, {0.0f, 15.0f}}};

/// A post's moving parts, kept from frame to frame: the scroll of the collar's texture, the collar's turns about its own
/// X and Z axes and the smoke's frame clock
struct Spin
{
	float scroll {0.0f}; ///< along the leash texture, 0..1
	float xAngle {0.0f}; ///< radians, 0..2 pi
	float zAngle {0.0f}; ///< radians, 0..2 pi
	float frame {0.0f};  ///< 0..15
};

/// A new post's spin from its four draws, given in the order of k_Draws
[[nodiscard]] constexpr Spin SeedSpin(float scroll, float xAngle, float zAngle, float frame) noexcept
{
	return {.scroll = scroll, .xAngle = xAngle, .zAngle = zAngle, .frame = frame};
}

/// How fast each part moves, a second
inline constexpr float k_ScrollRate = 0.5f;
inline constexpr float k_XTurnRate = 0.1f;
inline constexpr float k_ZTurnRate = 1.0f;
inline constexpr float k_FrameRate = 10.0f;
/// The smoke's frames
inline constexpr float k_Frames = 15.0f;

/// The frame time in seconds, from the frame's game milliseconds
[[nodiscard]] float FrameSeconds(uint32_t gameTimeIncMs) noexcept;

/// One frame: every part moves on by its rate times `seconds` and is wrapped back into its range by taking off whole
/// turns. The wrap counts the turns by multiplying with the reciprocal, so a value a hair under a whole turn wraps to a
/// hair under 0
void Step(Spin& spin, float seconds) noexcept;

/// The smoke sprite's frame cell, 0..14
[[nodiscard]] uint8_t SpriteCell(const Spin& spin) noexcept;

/// Which band of the leash texture the collar of post `post` shows (the v offset of its texture): the same band as the
/// worn rope of that leash
[[nodiscard]] constexpr float CollarBand(size_t post) noexcept
{
	return post == 0 ? 0.375f : static_cast<float>(post) * 0.125f;
}

/// How bright the smoke is drawn: a sixth of the sum of the red, green and blue of the land light table's last entry
/// (`landLight` as 0xAARRGGBB), truncated, 0..127
[[nodiscard]] uint32_t SmokeBrightness(uint32_t landLight) noexcept;
/// The smoke's colour, 0xAARRGGBB, with that brightness as its alpha: white, or orange for the picked post (drawn
/// additive)
inline constexpr uint32_t k_SmokeColour = 0xFFFFFFu;
inline constexpr uint32_t k_PickedSmokeColour = 0xC18119u;
[[nodiscard]] constexpr uint32_t SmokeColour(uint32_t brightness, bool picked) noexcept
{
	return (brightness << 24u) | (picked ? k_PickedSmokeColour : k_SmokeColour);
}
/// The smoke sprite's half width, in metres
inline constexpr float k_SmokeHalfWidth = 2.0f;

// ---- What the frame shows of a post ---------------------------------------------------------------------------------

/// Whether a post shows this frame (drawn, and felt by the hand): only on a fully built temple (its drawn percent
/// exactly 1) whose player has a creature, and only once that creature knows the post's leash
[[nodiscard]] constexpr bool Shown(float templeBuilt, bool hasCreature, bool knowsLeash) noexcept
{
	return templeBuilt == 1.0f && hasCreature && knowsLeash;
}

/// The collar's matrix: the post's own (`post`, fixed when it was made), turned in its own frame by the spin's X and Z
/// turns (affine::RotationYXZ(0, x, z), no translation, put before the post's: affine::MultiplyReversed)
[[nodiscard]] glm::mat4 CollarMatrix(const glm::mat4& post, const Spin& spin);

/// The hand object's scale to the collar's size on the hand
inline constexpr float k_HandCollarScale = std::bit_cast<float>(0x432D878Cu); ///< 173.52948
/// The picked post's collar as it hangs on the hand: at the translation of the hand's root bone `handRoot` (in the
/// world), with that bone's turn alone (its YXZ angles, affine::DecomposeYXZ then RotationYXZ: the scale dropped), then
/// put through diag(0.5 s) with the offset (0.05 s, 0.25 s, 0) in its own frame, s = `handScale` x k_HandCollarScale,
/// every product a float one
[[nodiscard]] glm::mat4 CollarOnHand(const glm::mat4& handRoot, float handScale);

// ---- The temple's pick ----------------------------------------------------------------------------------------------

/// The temple's pick is the picked post (0, 1, 2), or none when the temple is made
inline constexpr int32_t k_NoPick = -1;

/// The pick after a leash is picked: its post. Any other value leaves the pick as it was
[[nodiscard]] constexpr int32_t PickAfterSet(int32_t pick, LeashType type) noexcept
{
	switch (type)
	{
	case LeashType::Evil:
		return 0;
	case LeashType::Rope:
		return 1;
	case LeashType::Good:
		return 2;
	default:
		return pick;
	}
}

/// The leash picked at the temple: none with no pick, else the leash of post 0 or 1, and compassion for any other pick
[[nodiscard]] constexpr LeashType PickedType(int32_t pick) noexcept
{
	if (pick == k_NoPick)
	{
		return LeashType::None;
	}
	if (pick == 0)
	{
		return LeashType::Evil;
	}
	return pick == 1 ? LeashType::Rope : LeashType::Good;
}

/// The leash the scripts read as picked at a player's temple, numbered as the scripts number it: 0 when the player has
/// no temple heart (no pick given), else -1 with nothing picked and 1, 2, 3 for the picked post's leash
[[nodiscard]] constexpr int32_t ScriptLeashType(std::optional<int32_t> templePick) noexcept
{
	return templePick.has_value() ? static_cast<int32_t>(PickedType(*templePick)) : 0;
}

// ---- The hand -------------------------------------------------------------------------------------------------------

/// The hand feels a shown post as an invisible sphere of this radius at its point, in metres
inline constexpr float k_HandCollisionRadius = 1.0f;

/// Whether the hand of the local player may tap a post: only their own temple's posts
[[nodiscard]] constexpr bool ValidToTap(PlayerNames owner, PlayerNames local) noexcept
{
	return owner == local;
}

/// What the local player's tap on one of their posts does: the temple's pick after it, whether the click sounds and the
/// leash it sends to the next turn. The picked post is unpicked, silently and with nothing sent; any other post is
/// picked at once, with the click for the local interface (`myInterface`) and its leash sent
struct TapOutcome
{
	int32_t pick;
	bool click;
	std::optional<LeashType> sent;
};
[[nodiscard]] constexpr TapOutcome Tap(int32_t pick, size_t post, bool myInterface) noexcept
{
	if (pick == static_cast<int32_t>(post))
	{
		return {.pick = k_NoPick, .click = false, .sent = std::nullopt};
	}
	return {.pick = static_cast<int32_t>(post), .click = myInterface, .sent = TypeOf(post)};
}
/// The click of a tap: the in-game bank's sample 42, the click on a spell
inline constexpr uint16_t k_TapSample = 42;

/// Whether the turn refuses a leash picked at the temple: only when the player has a creature, while it is under the
/// compassionate or the angry spell
[[nodiscard]] constexpr bool LeashRefused(bool hasCreature, bool compassionate, bool angry) noexcept
{
	return hasCreature && (compassionate || angry);
}

/// The post's own text for the hand's "interact" tooltip, one for each leash. The hand shows it only over an object it
/// can lock onto, which a post is not, so over a post it shows the plain "Tap" of every object it can tap
inline constexpr uint32_t k_EvilPostToolTip = 0xEC8;
inline constexpr uint32_t k_RopePostToolTip = 0xECA;
inline constexpr uint32_t k_GoodPostToolTip = 0xEC9;
[[nodiscard]] constexpr uint32_t ToolTipOf(size_t post) noexcept
{
	if (post == 0)
	{
		return k_EvilPostToolTip;
	}
	return post == 1 ? k_RopePostToolTip : k_GoodPostToolTip;
}

} // namespace openblack::worship::leash_posts
