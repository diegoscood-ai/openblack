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

#include <functional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

// The town's queries the idle villagers use (Town.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V2_spec.md §4.2,
// disassembly dev\_scratch\mapa\v2_b.txt, v2_c.txt, v2_d.txt): the emergency, the congregation point and the two
// clear-area searches, plus the GUtils helpers they are made of. Positions are MapCoords x / z (6553.6 per metre, as
// the original keeps them: Object +0x14), converted from openblack's metres by truncation.

namespace openblack::ecs::components
{
struct Town;
}

namespace openblack::ecs::town_queries
{
/// metres -> MapCoords (ecs::map_coords::ToFixed, MapCoords(LHPoint) 0x603160: fmul [0x8AC400], truncated towards 0).
/// (aproximado) openblack keeps the positions in float metres
[[nodiscard]] glm::ivec2 ToMapCoords(glm::vec2 metres);
[[nodiscard]] glm::vec2 ToMetres(glm::ivec2 mapCoords);
/// The x / z of an object's Transform as MapCoords (Object +0x14); (0, 0) without one
[[nodiscard]] glm::ivec2 PosOf(entt::entity object);

/// GUtils::GetDistanceInMetres 0x74CD70: GetDistance 0x74CCB0 (hypotenuse 0x74F680 of dx, dz) x 10 / 65536
/// (ConvertWholeDistanceToMeters 0x74DCC0), through ECS/GUtilsDistance (gutils::GetDistanceInMetres)
[[nodiscard]] float GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b);
/// GUtils::GetAngleFromXZ 0x74D240 = GetAngleFromDXDZ 0x74D200(b - a) = LHArcTan 0x74D0C0: 2048ths, from a towards b
/// (gutils::GetAngleFromXZ)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::ivec2 a, glm::ivec2 b);
/// GUtils::Get3DAngleFromXZ 0x74D270 = ConvertGameAngleTo3D 0x74DC50(GetAngleFromDXDZ(b - a)): (angle & 0x7FF) x
/// 0.0030679617 (0x99A1CC) radians
[[nodiscard]] float Get3DAngleFromXZ(glm::ivec2 a, glm::ivec2 b);
/// GUtils::GetPosFromAngle 0x74D580: (ftol(cos(angle) x d x 65536 / 10), ftol(sin(angle) x d x 65536 / 10))
/// (gutils::GetPosFromAngle, the x and z)
[[nodiscard]] glm::ivec2 GetPosFromAngle(float angle, float metres);

/// GUtils::GetMapCellSpiralSizeFromRadius 0x74F520: max(ftol(0.2 r), 1)^2 map cells
[[nodiscard]] uint32_t GetMapCellSpiralSizeFromRadius(float radius);
/// GUtils::GetIncrementSpiralSizeFromRadius 0x74F540: (1 - ftol(-2 a / b))^2 points
[[nodiscard]] uint32_t GetIncrementSpiralSizeFromRadius(float a, float b);
/// GUtils::SpiralIncrement 0x74D810: --count == 0 -> ++dir, count = dir / 2; then pos = ftol((pos x 10 / 65536 +
/// step x table[dir & 3]) x 65536 / 10) on x and on z (the table of GUtils::Spiral: (1, 0) (0, 1) (-1, 0) (0, -1))
void SpiralIncrement(glm::ivec2& pos, int32_t& dir, int32_t& count, float step);

/// Town::IsInStateOfEmergency 0x747970: start (+0xF1C) != 0 && turn - start < GTownInfo +0x110
/// gameTurnsAfterEmergencyVillagersReact (unsigned, jae)
[[nodiscard]] bool IsInStateOfEmergency(const components::Town& town);

/// The filters of the clear-area searches (member function pointers of Object in the original)
using ClearAreaFilter = std::function<bool(entt::entity)>;
/// Object vt +0x534 (the thunk 0x743690): Object 1 (houses, Fixed, Feature, Field, stores, town centre...), Mobile 0
/// (villagers, animals, piles, pots), MobileStatic 0 (fallen tree, rock, bonfire...), Tree 0
[[nodiscard]] bool BlocksTownClearArea(entt::entity object);
/// Object vt +0x460 (through Villager FUN_00761BB0): 1 for everything derived from Object (also villagers and trees).
/// (aproximado) every entity of openblack's map cells is taken as an Object
[[nodiscard]] bool IsObject(entt::entity object);

/// Town::CheckForClearArea 0x7413D0: in the max(ftol(0.2 r), 1)^2 map cells of GUtils::Spiral 0x74D7E0 from pos's
/// cell (those inside the map, MapCoords::InBounds 0x6042C0), no object with GetDistanceInMetres(object, pos) -
/// Get2DRadius < r (fcomp, test ah, 1) that is not `excluded` and passes `filter`. Its two lists (0x741444 and 0x741496):
/// (aproximado) effects::ObjectsInMapCell (openblack's fixed and mobile grid, rebuilt each turn)
/// `blocker` (openblack, for the trace): the object that made it not clear
[[nodiscard]] bool CheckForClearArea(glm::ivec2 pos, float radius, const ClearAreaFilter& filter, entt::entity excluded,
                                     entt::entity* blocker = nullptr);
/// Get2DRadius (vt +0x64, Object 0x638180 and the class overrides): ecs::object::Get2DRadius, or the tests'
[[nodiscard]] float Get2DRadius(entt::entity object);
/// Town::FindClearArea 0x7412F0: GetIncrementSpiralSizeFromRadius(a, b) points from `start` (SpiralIncrement, steps of
/// b metres, dir = count = 1), the first one with CheckForClearArea(p, r) goes to `result` (true). None: `result` is
/// left as it was (0x741390 only resets the local) and false
bool FindClearArea(glm::ivec2& result, glm::ivec2 start, float a, float b, float radius, const ClearAreaFilter& filter,
                   entt::entity excluded);

/// Town::GetCongregationPos 0x7408B0 (with its cache Town +0xF10): the average of the town's abodes that are not fields
/// (and of its planned ones when there are fewer than 3) cleared with FindClearArea(130, 3, 10, BlocksTownClearArea),
/// else a point 10..20 m from a base (the single abode, or the town). `town` is the town entity
[[nodiscard]] glm::ivec2 GetCongregationPos(entt::entity town);

/// The tests: the objects of a map cell and their 2D radius (empty functions: effects::ObjectsInMapCell and
/// ecs::object::Get2DRadius, which need the map and the meshes)
void SetCellObjectsForTests(std::function<std::vector<entt::entity>(int cellX, int cellZ)> objects,
                            std::function<float(entt::entity)> radius);
} // namespace openblack::ecs::town_queries
