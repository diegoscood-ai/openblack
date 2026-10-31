/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageTotem.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/vec3.hpp>

#include "3D/CreatureBody.h"
#include "Common/GUtilsAngle.h"
#include "InfoConstants.h"

namespace openblack::ecs::village_totem
{

float RiseOf(float share)
{
	return share * k_RiseAtFullShare;
}

float HandSlide(float upPixels, float screenHeight, float iconHeight, float handAboveLand)
{
	if (screenHeight <= 0.0f)
	{
		return 0.0f; // (openblack guard) no screen to measure the move against
	}
	float slide = upPixels * iconHeight * k_SlidePerScreen / screenHeight;
	// going down it stops at the land: never further down than the hand is above it. Only a slide down is held to the
	// land; a slide up is the mouse's own, even with the hand under the land
	if (slide < 0.0f && -slide > handAboveLand)
	{
		slide = -handAboveLand;
	}
	return slide;
}

float SlideShare(float held, float slide)
{
	return std::clamp(slide * k_SharePerSlide + held, 0.0f, 1.0f);
}

float GripY(float iconY, float iconHeight)
{
	return iconHeight * k_GripHeight + iconY;
}

float HandTiltAt(float handY)
{
	if (!(handY < k_LowHand))
	{
		return k_HandTilt;
	}
	// the drop below the low height over the hand's size, both rounded to floats; its arctangent is taken wider, the
	// sum rounded once
	const float drop = k_LowHand - handY;
	const float ratio = drop / k_HandSize;
	return static_cast<float>(std::atan(static_cast<double>(ratio)) + static_cast<double>(k_HandTilt));
}

float GripClosure(float holdRadius)
{
	const float closure = holdRadius / (k_HandSize * 0.5f);
	return closure > 1.0f ? 1.0f : closure;
}

int32_t GripTimeMs(float closure, uint32_t clipLength)
{
	// half the clip as a signed whole number, times half the closure, truncated
	const auto half = static_cast<float>(static_cast<int32_t>(clipLength) >> 1);
	return static_cast<int32_t>(half * (closure * 0.5f));
}

glm::ivec2 CursorPin(glm::vec2 handOnScreen, glm::ivec2 screenSize)
{
	const auto keep = [](float value, int32_t size) {
		const auto last = static_cast<float>(size - 1);
		if (value < 0.0f)
		{
			return 0.0f;
		}
		return value > last ? last : value;
	};
	// to the nearest whole pixel, a half to the even one
	return {static_cast<int32_t>(std::nearbyint(keep(handOnScreen.x, screenSize.x))),
	        static_cast<int32_t>(std::nearbyint(keep(handOnScreen.y, screenSize.y)))};
}

glm::mat3 HoldRotation(float tilt, float heading)
{
	// the sine and cosine taken wider and rounded once; each product rounded to a float
	const auto sinTilt = static_cast<float>(std::sin(static_cast<double>(tilt)));
	const auto cosTilt = static_cast<float>(std::cos(static_cast<double>(tilt)));
	// half a turn on from the heading, a float sum; its sine is also kept wider, unrounded, for the tipped rows
	const float turn = heading + std::numbers::pi_v<float>;
	const auto cosTurn = static_cast<float>(std::cos(static_cast<double>(turn)));
	const double sinTurnWide = std::sin(static_cast<double>(turn));
	const auto sinTurn = static_cast<float>(sinTurnWide);
	// a float times the wider sine, rounded once
	const auto timesSinTurn = [sinTurnWide](float value) {
		return static_cast<float>(static_cast<double>(value) * sinTurnWide);
	};
	// the model's x, y and z in the world, each sum with its zero kept as the original works it
	const glm::vec3 x {cosTurn - 0.0f, 0.0f, 0.0f + sinTurn};
	const glm::vec3 y {0.0f - timesSinTurn(sinTilt), cosTilt, sinTilt * cosTurn + 0.0f};
	const glm::vec3 z {0.0f - timesSinTurn(cosTilt), -sinTilt, cosTilt * cosTurn + 0.0f};
	return {x, y, z};
}

bool ValidForLockedSelect(const GripFacts& facts)
{
	return facts.samePlayer && facts.functional && facts.heartBuilt && facts.siteBuilt;
}

Shown ShownShares(float current, float held, bool gripped)
{
	const float solid = gripped ? held : current;
	const float other = gripped ? current : held;
	// they are told apart by how high each stands
	if (RiseOf(other) == RiseOf(solid))
	{
		return {.solid = solid, .ghost = std::nullopt};
	}
	return {.solid = solid, .ghost = other};
}

bool Replaces(float solidRise, float lastRise, bool ghostShown)
{
	return solidRise != lastRise || ghostShown;
}

bool ShowsShareToolTip(bool replaces, bool quietOnce, bool localPlayers)
{
	return replaces && !quietOnce && localPlayers;
}

float PercentBuiltSetTo(float value)
{
	// anything not at or above none (a negative value, or no number) is none
	const float percent = value >= 0.0f ? value : 0.0f;
	// from the whole up it is built, at exactly the whole
	return percent >= 1.0f ? 1.0f : percent;
}

bool IsBuilt(float percentBuilt)
{
	return percentBuilt >= 1.0f;
}

bool IsDrawn(bool built, float life)
{
	return built && life > 0.0f;
}

float AsPercentage(float share)
{
	return share * 100.0f;
}

uint32_t IconMesh(std::optional<int32_t> creatureIcon, uint32_t meshesInPack)
{
	const int32_t icon = creatureIcon.value_or(k_HandIcon);
	return icon >= 0 && static_cast<uint32_t>(icon) < meshesInPack ? static_cast<uint32_t>(icon) : 0;
}

std::optional<int32_t> CreatureIcon(std::optional<CreatureType> species, std::span<const GCreatureInfo> infos)
{
	if (!species.has_value())
	{
		return std::nullopt;
	}
	const auto row = creature::InfoRow(*species);
	return row < infos.size() ? std::optional(static_cast<int32_t>(infos[row].totemIcon)) : std::nullopt;
}

float FacingSite(const map_coords::MapCoords& totem, const map_coords::MapCoords& site)
{
	// the way to the site as the game's angle turned to radians, then a quarter turn, summed as floats
	return gutils::Get3DAngleFromXZ(totem, site) + gutils::k_ScawenOffset;
}

} // namespace openblack::ecs::village_totem
