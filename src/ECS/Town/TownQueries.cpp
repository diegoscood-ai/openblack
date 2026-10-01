/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownQueries.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdlib>
#include <string>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Map.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::town_queries
{
using namespace components;

namespace
{
std::function<std::vector<entt::entity>(int, int)> g_CellObjectsForTests;
std::function<float(entt::entity)> g_RadiusForTests;

/// ConvertGameAngleTo3D 0x74DC50's factor (0x99A1CC, 0x3B490FDB)
constexpr float k_GameAngleTo3D = 0.0030679617f;
/// MapCoords per map cell (10 m: the high word of x / z, MapCoords::InBounds 0x6042C0 and operator+= JustMapXZ 0x605470)
constexpr int32_t k_MapCoordsPerCell = 65536;

/// The steps of GUtils::Spiral's table (0xDA59FC, words x / z; the same table as ecs::animal_ai::detail::Spiral)
constexpr std::array<glm::ivec2, 4> k_SpiralSteps = {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

std::vector<entt::entity> CellObjects(int cellX, int cellZ)
{
	if (g_CellObjectsForTests)
	{
		return g_CellObjectsForTests(cellX, cellZ);
	}
	return effects::ObjectsInMapCell(cellX, cellZ);
}

/// Object::Get2DRadius (vt +0x64, 0x638180)
float Radius2D(entt::entity object)
{
	if (g_RadiusForTests)
	{
		return g_RadiusForTests(object);
	}
	return effects::Object2DRadius(object);
}

/// ftol of a value the x87 keeps in extended precision (double here)
int32_t Ftol(double value)
{
	return static_cast<int32_t>(value);
}

/// GameThingWithPos::IsField (vt +0x210): a field. openblack's fields (components::Field) are not abodes, so they are
/// never in the town's abode list; an abode of number Field is taken as one too
bool IsField(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Field>(abode))
	{
		return true;
	}
	const auto* a = registry.TryGet<const Abode>(abode);
	return a != nullptr && a->type == AbodeNumber::Field;
}

/// The town's abode list (+0x754, next +0x9C), newest first: Town::AddStructureToTown inserts at the head
/// (0x7399C3 mov ecx,[esi+0x754]; 0x7399C9 new->next(+0x9C) = head; 0x7399CF head = new). openblack orders by the
/// object creation index (+0x3C), descending, as the order of AddStructureToTown (aproximado: an abode joins its town
/// when it is made). It decides GetCongregationPos's y (the last one read, 0x7409C1..0x7409DB: the oldest), the base of
/// its fallback when there is one abode, and which ones the ring of 100 keeps with more than 100 (0x74091F..0x740933)
std::vector<entt::entity> TownAbodes(const Town& town)
{
	std::vector<std::pair<int64_t, entt::entity>> found;
	Locator::entitiesRegistry::value().Each<const Abode, const Transform>(
	    [&](entt::entity entity, const Abode& abode, const Transform&) {
		    if (abode.townId == town.id)
		    {
			    found.emplace_back(object_index::Of(entity), entity);
		    }
	    });
	std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> result;
	result.reserve(found.size());
	for (const auto& [index, entity] : found)
	{
		result.push_back(entity);
	}
	return result;
}

/// Town::GetCongregationPos's "town <id>" trace, once per town
void TraceCongregation(const Town& town, const std::string& line)
{
	static std::bitset<256> traced;
	if (const char* trace = std::getenv("OPENBLACK_VILLAGER_TRACE"); trace == nullptr || *trace == '\0')
	{
		return;
	}
	const auto bit = static_cast<size_t>(town.id & 0xFF);
	if (traced.test(bit))
	{
		return;
	}
	traced.set(bit);
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		const auto& info = Locator::infoConstants::value().town;
		SPDLOG_LOGGER_INFO(logger,
		                   "Villager trace: congregation town {} (turn {}): {} (GTownInfo +0x110 {} +0x140 {:.2f} +0x144 {:.2f})",
		                   town.id, villager::CurrentTurn(), line, info.gameTurnsAfterEmergencyVillagersReact,
		                   info.maxDistanceFromCongreationPosThatPeopleChillOut, info.maxDistanceFromHouseThatPeopleChillOut);
	}
}
} // namespace

glm::ivec2 ToMapCoords(glm::vec2 metres)
{
	return {Ftol(static_cast<double>(metres.x) * k_MapCoordsPerMetre), Ftol(static_cast<double>(metres.y) * k_MapCoordsPerMetre)};
}

glm::vec2 ToMetres(glm::ivec2 mapCoords)
{
	return {static_cast<float>(mapCoords.x / k_MapCoordsPerMetre), static_cast<float>(mapCoords.y / k_MapCoordsPerMetre)};
}

glm::ivec2 PosOf(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	return transform != nullptr ? ToMapCoords({transform->position.x, transform->position.z}) : glm::ivec2(0);
}

float GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b)
{
	// 0x74CCB0: hypotenuse(b.x - a.x, b.z - a.z) (whole MapCoords); 0x74DCC0: 10 x 1/65536 x that
	const auto d = static_cast<double>(std::hypot(static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y)));
	const auto whole = static_cast<int32_t>(d);
	return 10.0f * (1.0f / 65536.0f) * static_cast<float>(whole);
}

uint16_t GetAngleFromXZ(glm::ivec2 a, glm::ivec2 b)
{
	// 0x74D200: LHArcTan(dx, dz) & 0xFFFF
	return animal_ai::detail::AngleOfMapCoords(b.x - a.x, b.y - a.y);
}

float Get3DAngleFromXZ(glm::ivec2 a, glm::ivec2 b)
{
	// 0x74D270 -> 0x74DC50: (angle & 0x7FF), fild qword, fmul 0x99A1CC
	return static_cast<float>(GetAngleFromXZ(a, b) & 0x7FF) * k_GameAngleTo3D;
}

glm::ivec2 GetPosFromAngle(float angle, float metres)
{
	// 0x74D58F..0x74D5C0: fcos / fsin, x d, x 65536, / 10, ftol (y = ftol(0 / 10) = 0)
	const double a = angle;
	const double d = metres;
	return {Ftol(std::cos(a) * d * 65536.0 / 10.0), Ftol(std::sin(a) * d * 65536.0 / 10.0)};
}

uint32_t GetMapCellSpiralSizeFromRadius(float radius)
{
	// 0x74F520: ftol(r x 0.2) (0x8AA3AC); below 1 (unsigned, jae) -> 1; squared
	auto n = static_cast<uint32_t>(Ftol(static_cast<double>(radius) * static_cast<double>(0.2f)));
	if (n < 1)
	{
		n = 1;
	}
	return n * n;
}

uint32_t GetIncrementSpiralSizeFromRadius(float a, float b)
{
	// 0x74F540: ftol(a x -2 / b) (0x8C7CE0 = -2); (1 - that)^2
	const int32_t t = Ftol(static_cast<double>(a) * -2.0 / static_cast<double>(b));
	const int32_t s = 1 - t;
	return static_cast<uint32_t>(s * s);
}

void SpiralIncrement(glm::ivec2& pos, int32_t& dir, int32_t& count, float step)
{
	// 0x74D81B..0x74D82B: --count == 0 -> ++dir, count = dir / 2 (cdq, sub, sar: towards 0)
	if (--count == 0)
	{
		++dir;
		count = dir / 2;
	}
	const auto& s = k_SpiralSteps.at(static_cast<size_t>(dir & 3));
	// 0x74D836..0x74D8A8: x = ftol((x x 10 x 1/65536 + table.x x step) x 65536 / 10), the same for z
	const auto move = [step](int32_t value, int32_t table) {
		const double metres = static_cast<double>(value) * 10.0 * static_cast<double>(1.0f / 65536.0f) +
		                      static_cast<double>(table) * static_cast<double>(step);
		return Ftol(metres * 65536.0 / 10.0);
	};
	pos.x = move(pos.x, s.x);
	pos.y = move(pos.y, s.y);
}

bool IsInStateOfEmergency(const Town& town)
{
	// 0x747970: +0xF1C != 0 && (unsigned) turn - +0xF1C < GTownInfo +0x110 (jae)
	const uint32_t start = town.emergencyStartTurn;
	if (start == 0)
	{
		return false;
	}
	return villager::CurrentTurn() - start < Locator::infoConstants::value().town.gameTurnsAfterEmergencyVillagersReact;
}

bool BlocksTownClearArea(entt::entity object)
{
	// vt +0x534: Object 1; Mobile, MobileStatic and Tree override it with 0
	const auto& registry = Locator::entitiesRegistry::value();
	return !registry.AnyOf<Mobile, MobileStatic, Tree>(object);
}

bool IsObject([[maybe_unused]] entt::entity object)
{
	// vt +0x460: 1 for Object and everything below it
	return true;
}

float Get2DRadius(entt::entity object)
{
	return Radius2D(object);
}

bool CheckForClearArea(glm::ivec2 pos, float radius, const ClearAreaFilter& filter, entt::entity excluded,
                       entt::entity* blocker)
{
	auto& registry = Locator::entitiesRegistry::value();
	// 0x7413F4: the number of cells; 0x7413FB: dir = count = 1
	uint32_t cells = GetMapCellSpiralSizeFromRadius(radius);
	int32_t dir = 1;
	int32_t count = 1;
	glm::ivec2 cell = pos; // the walk's MapCoords: pos plus whole cells
	while (cells != 0)
	{
		// 0x741421 MapCoords::InBounds: the high words (unsigned) inside the map
		const auto cx = static_cast<uint32_t>(cell.x) >> 16;
		const auto cz = static_cast<uint32_t>(cell.y) >> 16;
		if (cx < MapInterface::k_GridSize.x && cz < MapInterface::k_GridSize.y)
		{
			for (const auto object : CellObjects(static_cast<int>(cx), static_cast<int>(cz)))
			{
				if (!registry.Valid(object) || !registry.AllOf<Transform>(object))
				{
					continue;
				}
				// 0x74144A..0x741467: GetDistanceInMetres(object, pos) - Get2DRadius < r (fsubr; fcomp; test ah, 1)
				const float distance = GetDistanceInMetres(PosOf(object), pos);
				if (distance - Radius2D(object) < radius)
				{
					// 0x74146D: the excluded object is skipped; 0x741475: the filter; 1 -> not clear (0x7414E8)
					if (object != excluded && filter && filter(object))
					{
						if (blocker != nullptr)
						{
							*blocker = object;
						}
						return false;
					}
				}
			}
		}
		// 0x7414B2..0x7414D3: --cells; Spiral (the cell step); MapCoords += JustMapXZ (whole cells)
		--cells;
		if (--count == 0)
		{
			++dir;
			count = dir / 2;
		}
		const auto& s = k_SpiralSteps.at(static_cast<size_t>(dir & 3));
		cell.x += s.x * k_MapCoordsPerCell;
		cell.y += s.y * k_MapCoordsPerCell;
	}
	return true;
}

bool FindClearArea(glm::ivec2& result, glm::ivec2 start, float a, float b, float radius, const ClearAreaFilter& filter,
                   entt::entity excluded)
{
	// 0x741319: the number of points; 0x741320..0x74132E: dir = count = 1
	uint32_t points = GetIncrementSpiralSizeFromRadius(a, b);
	int32_t dir = 1;
	int32_t count = 1;
	glm::ivec2 pos = start;
	while (points != 0)
	{
		// 0x741363
		if (CheckForClearArea(pos, radius, filter, excluded))
		{
			// 0x7413A8: result.Set(pos)
			result = pos;
			return true;
		}
		// 0x741383..0x741384
		--points;
		SpiralIncrement(pos, dir, count, b);
	}
	// 0x741390: only the local is reset to start; result untouched
	return false;
}

glm::ivec2 GetCongregationPos(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* town = registry.TryGet<Town>(townEntity);
	if (town == nullptr)
	{
		return PosOf(townEntity);
	}
	// 0x7408B6..0x7408EF: the cache, unless x == 0 && z == 0 && y == 0.0
	if (town->congregationPos != glm::ivec2(0) || town->congregationPosY != 0.0f)
	{
		return town->congregationPos;
	}
	// 0x7408F5..0x740983: a ring of 100; the abodes that are not fields, then (fewer than 3) the planned ones
	struct Item
	{
		glm::ivec2 xz;
		float y;
	};
	std::array<Item, 100> ring {};
	uint32_t write = 0;
	uint32_t n = 0;
	for (const auto abode : TownAbodes(*town))
	{
		if (IsField(abode))
		{
			continue;
		}
		const auto& transform = registry.Get<const Transform>(abode);
		ring.at(write) = {PosOf(abode), transform.position.y};
		write = (write + 1) % 100;
		++n;
	}
	if (n < 3)
	{
		// 0x740953: the planned list (+0x9A8, next +0x44), oldest first: Town::AddPlanned appends at the tail
		// (0x73D08A..0x73D09E walks to the last +0x44 and links there; 0x73D0AD new->next = 0), as push_back does
		for (const auto& planned : town->plannedAbodes)
		{
			ring.at(write) = {ToMapCoords({planned.position.x, planned.position.z}), planned.position.y};
			write = (write + 1) % 100;
			++n;
		}
	}
	uint32_t read = 0;
	glm::ivec2 pos(0);
	float y = 0.0f;
	if (n > 1)
	{
		// 0x74099C..0x7409E3: the sums of x and z (32-bit, read as unsigned qwords), y the last one's
		uint32_t sumX = 0;
		uint32_t sumZ = 0;
		for (uint32_t i = 0; i < n; ++i)
		{
			const auto& item = ring.at(read);
			read = (read + 1) % 100;
			sumX += static_cast<uint32_t>(item.xz.x);
			sumZ += static_cast<uint32_t>(item.xz.y);
			y = item.y;
		}
		// 0x7409F3..0x740A1E: fild qword / fild qword n, ftol (truncated)
		pos.x = Ftol(static_cast<double>(sumX) / static_cast<double>(n));
		pos.y = Ftol(static_cast<double>(sumZ) / static_cast<double>(n));
		// 0x740A25..0x740A5D: FindClearArea(pos, pos, 130, 3, 10, BlocksTownClearArea (0x743690), none)
		if (FindClearArea(pos, pos, 130.0f, 3.0f, 10.0f, &BlocksTownClearArea, entt::null))
		{
			town->congregationPos = pos;
			town->congregationPosY = y;
			TraceCongregation(*town, fmt::format("n={} avg -> ({:.1f}, {:.1f})", n, ToMetres(pos).x, ToMetres(pos).y));
			return pos;
		}
	}
	// 0x740A71..0x740A98: the ring's first unread item if any (n == 1), else the town (+0x14)
	const uint32_t remaining = read > write ? write - read + 100 : write - read;
	// (the base's three dwords are copied, 0x740A9D..0x740ABC: y is the base's)
	glm::ivec2 base = PosOf(townEntity);
	const auto* townTransform = registry.TryGet<const Transform>(townEntity);
	y = townTransform != nullptr ? townTransform->position.y : 0.0f;
	const char* from = "town";
	if (remaining > 0)
	{
		base = ring.at(read).xz;
		y = ring.at(read).y;
		from = "first";
	}
	// 0x740AA6..0x740AFD: d = GameFloatRand(10) + 10 (first), angle = GameFloatRand(2 pi) (0x40C90FDB, second), Town.cpp
	// 0x11EC; base += GetPosFromAngle(angle, d)
	const float distance = villager::GameFloatRand(10.0f) + 10.0f;
	const float angle = villager::GameFloatRand(glm::two_pi<float>());
	pos = base + GetPosFromAngle(angle, distance);
	// 0x740B02: the cache
	town->congregationPos = pos;
	town->congregationPosY = y;
	TraceCongregation(*town, fmt::format("n={} {} -> ({:.1f}, {:.1f})", n, from, ToMetres(pos).x, ToMetres(pos).y));
	return pos;
}

void SetCellObjectsForTests(std::function<std::vector<entt::entity>(int, int)> objects, std::function<float(entt::entity)> radius)
{
	g_CellObjectsForTests = std::move(objects);
	g_RadiusForTests = std::move(radius);
}
} // namespace openblack::ecs::town_queries
