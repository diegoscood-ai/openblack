/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootballRules.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <iterator>

namespace rules = openblack::ecs::football_rules;
namespace map_coords = openblack::map_coords;

namespace
{
/// The kick's gravity, as the float the game keeps
constexpr float k_KickGravity = std::bit_cast<float>(0xC11CF5C3u);
/// The angle between two watchers' places: 9 degrees, as the float the game keeps
constexpr float k_WatcherStep = std::bit_cast<float>(0x3E20D97Cu);
} // namespace

uint32_t rules::TurnsFor(float seconds, uint32_t msPerTurn)
{
	const uint32_t perSecond = msPerTurn != 0 ? 1000 / msPerTurn : 0;
	return static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(perSecond) * seconds));
}

float rules::MatchBelief(uint32_t count, float durationSeconds, float beliefForMatch, uint32_t msPerTurn)
{
	const uint32_t perSecond = msPerTurn != 0 ? 1000 / msPerTurn : 0;
	const auto durationTurns = map_coords::FtoL(static_cast<float>(perSecond) * durationSeconds);
	const auto twice = map_coords::FtoL(static_cast<float>(durationTurns) * static_cast<float>(perSecond));
	const float ratio = static_cast<float>(count) / static_cast<float>(twice);
	// "less than 2" also holds when the ratio is not a number
	const float share = !(ratio >= 2.0f) ? ratio : 2.0f;
	return share * beliefForMatch;
}

std::vector<size_t> rules::RankOrder(std::span<const uint32_t> distances)
{
	std::vector<size_t> ranked;
	for (size_t i = 0; i < distances.size(); ++i)
	{
		const auto further = std::ranges::find_if(ranked, [&](size_t j) { return distances[i] < distances[j]; });
		ranked.insert(further, i);
	}
	return ranked;
}

glm::vec3 rules::KickVelocity(const glm::vec3& from, const glm::vec3& to, float speed)
{
	const float dx = to.x - from.x;
	const float dy = to.y - from.y;
	const float dz = to.z - from.z;
	const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
	const float t = distance / speed;
	const float half = (t * t) * 0.5f;
	const float fall = half * k_KickGravity;
	const float inverse = 1.0f / t;
	return {(dx - 0.0f) * inverse, (dy - fall) * inverse, (dz - 0.0f) * inverse};
}

namespace
{
/// The clamps of the priorities: a value below the floor is the floor, above the ceiling the ceiling
float Clamp(float v, float floor, float ceiling)
{
	// "below the floor" also holds when not a number, as the x87 compare says
	if (!(v >= floor))
	{
		return floor;
	}
	return v > ceiling ? ceiling : v;
}
} // namespace

float rules::ShootPriority(float toGoal)
{
	const float share = 1.0f - Clamp(toGoal, 0.0f, 40.0f) * 0.025f;
	// the turn towards the goal adds nothing: its share is always 1
	return 1.0f * Clamp(share, 0.1f, 1.0f);
}

float rules::DribblePriority(float toGoal)
{
	return Clamp(Clamp(toGoal, 0.0f, 40.0f) * 0.025f, 0.1f, 1.0f);
}

float rules::MoveToBePassedPriority(float toBall)
{
	return Clamp(Clamp(toBall, 0.0f, 20.0f) * 0.05f, 0.0f, 0.2f);
}

float rules::GoHomePriority(float toHome)
{
	return Clamp(Clamp(toHome, 0.0f, 20.0f) * 0.05f, 0.1f, 1.0f);
}

float rules::IdlePriority(float toBall, float toHome)
{
	const float far = (Clamp(toBall, 10.0f, 40.0f) - 10.0f) * 0.025f;
	const float near = 1.0f - Clamp(toHome, 0.0f, 10.0f) * 0.1f;
	return Clamp(near * far, 0.0f, 1.0f);
}

float rules::AcrossShare(uint32_t i, uint32_t k, float width)
{
	if (k == 1)
	{
		return 0.0f;
	}
	if (k == 2)
	{
		return (static_cast<float>(i) - 0.5f) * width;
	}
	// k - 1 taken unsigned and exact, as the original loads it, the quotient rounded once
	const float step = static_cast<float>(static_cast<double>(width) / static_cast<double>(k - 1));
	return step * static_cast<float>(static_cast<int32_t>(i)) - width * 0.5f;
}

rules::FormationShares rules::AttackerShares(uint32_t number, uint32_t attackers)
{
	const uint32_t rows = number / 5;
	const uint32_t place = number - rows * 5;
	return {.across = AcrossShare(place, attackers, 0.8f), .along = 0.33f - static_cast<float>(rows) * 0.1f};
}

rules::FormationShares rules::DefenderShares(uint32_t number, uint32_t defenders)
{
	const uint32_t rows = number / 3;
	const uint32_t place = number - rows * 3;
	return {.across = AcrossShare(place, defenders, 0.5f),
	        .along = std::bit_cast<float>(0xBF428F5Cu) - static_cast<float>(rows) * 0.1f};
}

glm::vec3 rules::FormationSpot(const glm::vec3& a, const glm::vec3& b, const FormationShares& shares)
{
	const float dx = b.x - a.x;
	const float dz = b.z - a.z;
	const float sideX = -dz * shares.across;
	const float sideZ = dx * shares.across;
	const float x = (dx * shares.along + a.x) + sideX;
	const float y = (0.0f + a.y) + 0.0f;
	const float z = (dz * shares.along + a.z) + sideZ;
	return {x, y, z};
}

map_coords::MapCoords rules::MoveToBePassedSpot(const glm::vec3& own, const glm::vec3& attacked, float drawAlong,
                                                float drawAcross)
{
	const float dx = attacked.x - own.x;
	const float dy = attacked.y - own.y;
	const float dz = attacked.z - own.z;
	// the way's normal (-dz, 0, dx), made unit unless it is nothing. (inferred) the middle component is a stack word
	// the original never sets; any small value it holds (a pointer or a small integer read as a float) is nothing
	glm::vec3 normal {-dz, 0.0f, dx};
	if (normal.x != 0.0f || normal.y != 0.0f || normal.z != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((normal.y * normal.y + normal.z * normal.z) + normal.x * normal.x);
		normal = {inverse * normal.x, normal.y * inverse, inverse * normal.z};
	}
	const float along = static_cast<float>(static_cast<double>(drawAlong) + 0.6);
	const float x = along * dx + own.x;
	const float z = along * dz + own.z;
	static_cast<void>(dy);
	const float across = drawAcross - 9.0f;
	return {map_coords::ToFixed(across * normal.x + x), map_coords::ToFixed(across * normal.z + z), 0.0f};
}

glm::vec3 rules::DribbleSpot(const glm::vec3& me, const glm::vec3& goal)
{
	float dx = goal.x - me.x;
	float dy = goal.y - me.y;
	float dz = goal.z - me.z;
	float distance = std::sqrt((dy * dy + dx * dx) + dz * dz);
	if (dx != 0.0f || dy != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((dy * dy + dx * dx) + dz * dz);
		dx *= inverse;
		dy *= inverse;
		dz *= inverse;
	}
	distance *= 0.2f;
	return {me.x + dx * distance, me.y + dy * distance, me.z + dz * distance};
}

uint8_t rules::ChooseSubstate(uint8_t count, const std::function<bool(uint8_t)>& allowed,
                              const std::function<float(uint8_t)>& priority)
{
	constexpr float k_NoneAbove = -0.0001f;
	uint8_t best = count;
	float bestPriority = k_NoneAbove;
	for (uint8_t row = 0; row < count; ++row)
	{
		if (!allowed(row))
		{
			continue;
		}
		if (const float p = priority(row); bestPriority < p)
		{
			bestPriority = p;
			best = row;
		}
	}
	return best;
}

map_coords::MapCoords rules::Scattered(const map_coords::MapCoords& point, float drawX, float drawZ, float half)
{
	// each product and sum rounded to a float, the draw's offset taken first
	const float offsetX = drawX - half;
	const float offsetZ = drawZ - half;
	return {map_coords::MetresToFixedExact(map_coords::ToMetres(point.x) + offsetX),
	        map_coords::MetresToFixedExact(map_coords::ToMetres(point.z) + offsetZ), point.altitude};
}

map_coords::MapCoords rules::PlayerSpot(const map_coords::MapCoords& middle, float drawX, float drawZ)
{
	return Scattered(middle, drawX, drawZ, k_PlayerSpread * 0.5f);
}

float rules::WatcherAngle(uint32_t placesTaken)
{
	return static_cast<float>(placesTaken) * k_WatcherStep;
}

map_coords::MapCoords rules::WatcherPlace(const map_coords::MapCoords& point, float drawX, float drawZ)
{
	return Scattered(point, drawX, drawZ, k_WatcherSpread * 0.5f);
}
