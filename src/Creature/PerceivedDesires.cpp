/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PerceivedDesires.h"

#include <algorithm>

#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"

using namespace openblack;
using namespace openblack::creature_perceived_desires;

namespace
{
/// The weight added, then held between 0 and 1: anything not at least 0 (a NaN too) is 0
void Add(float& value, float weight)
{
	value = weight + value;
	if (!(value >= 0.0f))
	{
		value = 0.0f;
	}
	else if (value > 1.0f)
	{
		value = 1.0f;
	}
}
} // namespace

void creature_perceived_desires::Increase(PerceivedDesires& desires, int32_t desire, float weight)
{
	if (desire >= 0 && static_cast<size_t>(desire) < desires.player.size())
	{
		Add(desires.player.at(static_cast<size_t>(desire)), weight);
	}
}

void creature_perceived_desires::IncreaseTown(PerceivedDesires& desires, int32_t desire, float weight)
{
	if (desire >= 0 && static_cast<size_t>(desire) < desires.town.size())
	{
		Add(desires.town.at(static_cast<size_t>(desire)), weight);
	}
}

void creature_perceived_desires::Fade(PerceivedDesires& desires)
{
	std::ranges::for_each(desires.player, [](float& value) { value *= k_TurnFade; });
	std::ranges::for_each(desires.town, [](float& value) { value *= k_TurnFade; });
}

std::optional<size_t> creature_perceived_desires::TakeDominant(PerceivedDesires& desires,
                                                               const std::function<bool(size_t)>& activated)
{
	std::optional<size_t> dominant;
	for (size_t desire = 0; desire < desires.player.size(); ++desire)
	{
		if (activated(desire) && desires.player.at(desire) > 0.0f)
		{
			desires.player.at(desire) = 0.0f;
			dominant = desire;
		}
	}
	return dominant;
}

bool creature_perceived_desires::CanSeePos(uint16_t lookAngle, uint16_t angleToPoint, bool sameCell)
{
	return gutils::GetAngleDifference(lookAngle, angleToPoint) <= k_SeeHalfAngle || sameCell;
}

bool creature_perceived_desires::CanSeePos(float lookYaw, const map_coords::MapCoords& from, const map_coords::MapCoords& point)
{
	const auto look = static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(lookYaw));
	const auto toPoint = gutils::GetAngleFromXZ(from, point);
	return CanSeePos(look, toPoint, map_coords::Cell(from) == map_coords::Cell(point));
}

float creature_perceived_desires::LookYaw(const std::optional<glm::vec3>& lookAt, const glm::vec3& position, float bodyYaw)
{
	if (!lookAt.has_value() || *lookAt == glm::vec3(0.0f) || *lookAt == position)
	{
		return bodyYaw;
	}
	return static_cast<float>(affine::GetYAngle(*lookAt - position));
}
