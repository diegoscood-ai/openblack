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

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::map_collide
{
struct Shape;
}

/// The original's object lists of the map cells (GMap g_game+0x59B8, MapCell of 8 bytes at g_game+0x59FC; research
/// dev\tmp_dis\unify2\map_cell_queries_original.md and map_cell_queries_PLAN_A.md): every one of the 512 x 512 cells has
/// two ORDERED lists, +0 the mobile one (SetFirstObjectMobile 0x601B60) and +4 the fixed one (SetFirstObjectFixed
/// 0x601B70), kept up to date on every insert, remove and move, and the queries built on them.
///
/// - Which list: the object's type (DoesObjectTypeCountAsFixed 0x601510, the bit 15 of Object +0x24 that
///   InitialiseIsFixedForMapList 0x63A640 sets), not its class.
/// - Which end: the Fixed classes (SingleMapFixed, MultiMapFixed, FishFarm) enter at the HEAD of the fixed list
///   (Fixed::InsertMapObjectToCell 0x52DEA0 / AssumeFixed 0x52DEE0); every other class through
///   Object::InsertMapObjectToCell 0x636830: at the TAIL of the fixed list when its type counts as fixed (pots and piles,
///   street lanterns), else at the HEAD of the mobile list (a doubly linked one, MapParent +0x38).
/// - Which cells: a MultiMapFixed every cell whose 7.1 m circle touches its shape (NewCollideDescriptor 0x46A860 /
///   0x46AB10 / 0x46AD80), every other class the cell of its position (0x636740).
/// - A walk of a cell (MapCellIterator, GetFirstIterator 0x6034D0 and every inline copy) is the fixed list, then the
///   mobile one, each from its head.
///
/// The lists are kept by hooks the owners call where the original calls its vtable (InsertMapObject vt +0x544,
/// RemoveMapObject vt +0x548, MoveMapObject vt +0x55C) and, for what has no hook yet, by Sync() once per turn
/// (MapProduction::Rebuild calls it): the creations, deletions, hand, physics and moves of the other owners. Every read
/// also skips what the original has taken out of the map at once (invalid, in the hand, flying, carried by a storm).
///
/// The old ecs::MapInterface (MapProduction's unordered sets) stays for the callers that are not migrated yet.
namespace openblack::ecs::map_cells
{

/// DoesObjectTypeCountAsFixed 0x601510: the jump table 0x60152C (45 entries, read from the exe); above 0x2C
/// ("cmp eax, 0x2C; ja", unsigned: ANY -1 and -2 too) it is 0
constexpr std::array<bool, 45> k_CountsAsFixed {
    true,  false, false, false, false, false, true,  true,  true,  true,  false, true,  true,  false, true,
    false, false, false, true,  true,  false, true,  true,  true,  true,  true,  true,  false, true,  true,
    false, true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  false, true,  true};

[[nodiscard]] constexpr bool CountsAsFixed(ObjectType type)
{
	const auto index = static_cast<uint32_t>(type);
	return index < k_CountsAsFixed.size() && k_CountsAsFixed[index];
}

/// How the object's class enters the cells (vtable +0x544 InsertMapObject / +0x54C InsertMapObjectToCell / +0x53C
/// GetMapChild, map_cell_queries_original.md §3)
enum class InsertKind : uint8_t
{
	None,           ///< not in the map: SpellSeed 0x728F30 and MagicFireBall 0x682D10 are a bare ret; towns, forests...
	Object,         ///< Object::InsertMapObject 0x636740 -> 0x636830 (MobileObject 0x607250 too): its own cell
	SingleMapFixed, ///< 0x52E620 -> Fixed 0x52E530 -> 0x636740 -> SingleMapFixed 0x52F440 -> 0x52DEA0: its cell, head
	MultiMapFixed,  ///< 0x52E650: every cell of its NewCollideDescriptor, the head of each (AssumeFixed 0x52DEE0)
	FishFarm,       ///< 0x52CA10: the cells of GetNextPos 0x52C940 (only its own), the head (AssumeFixed 0x52DEE0)
};

/// The class of an openblack entity (its components stand in for the vtable)
[[nodiscard]] InsertKind KindOf(entt::entity object);
/// Object +0x24 bit 1 (the MultiMapFixed ctor 0x52E1F0): a MultiMapFixed or a FishFarm (FishFarm : MultiMapFixed)
[[nodiscard]] bool IsMultiMapFixedClass(entt::entity object);
/// The OBJECT_TYPE of the object's info (info +0x10, Object +0x28), read from info.dat; a fixed (inferido) type for
/// the classes openblack keeps no info row for. ObjectType::Invalid for a class that is not in the map
[[nodiscard]] ObjectType TypeOf(entt::entity object);

// ---- The hooks (the original's vtable calls) ------------------------------------------------------------------------

/// InsertMapObject vt +0x544 (0x636740 / 0x52E620 / 0x52E650 / 0x52CA10) by the object's class. Nothing if it is
/// already in the map (openblack: an idempotent hook) or it is not a map class. (inferido) A creation hook takes the
/// object's +0x24 bit 2 and +0xA & 0x11 as clear: CallVirtualFunctionsForCreation skips the insertion when either is
/// set (MobileObject 0x6071E8 / 0x6071EF, MultiMapFixed 0x52EA04 / 0x52EA0A)
void InsertMapObject(entt::entity object);
/// RemoveMapObject vt +0x548 (0x6367A0 / 0x52E600 / 0x52E7B0 / 0x52CA70): out of every cell it was put in
void RemoveMapObject(entt::entity object);
/// MoveMapObject vt +0x55C: Object 0x636A40 only re-enters when the cell changes (else the position changes and the
/// lists don't), MultiMapFixed 0x52E4F0 when the MapCoords changes at all (operator== 0x605660); both through
/// ActualMoveMapObject 0x638040 (Remove, SetPos, Insert: at the head). The Transform takes the new position
void MoveMapObject(entt::entity object, const glm::vec3& position);
/// SetXYZAnglesAndScale (Object 0x638F80, MobileObject 0x6074E0, MobileStatic 0x608D60): Remove and Insert when in the
/// map, so it goes to the head even in the same cell. For the code that turns or scales an object
void OnAnglesOrScaleChanged(entt::entity object);
/// Object +0x24 bit 0 "in the map" (0x636798 / 0x6367B7)
[[nodiscard]] bool IsObjectInMap(entt::entity object);
/// Out of the map while something else holds it (Object::InitialisePhysics 0x637480+0x3A: a storm's tornado carries
/// it). held = true removes it and keeps Sync from putting it back; false lets Sync put it back (at the head)
void SetHeldOutOfMap(entt::entity object, bool held);

/// openblack: once per turn, the other owners' creations, deletions, hand, physics and moves (until they call the hooks
/// themselves), in creation index order (inferido). MapProduction::Rebuild calls it
void Sync();
/// MapCell::Clean 0x601380 on every cell: no objects, no links
void Clear();
/// With OPENBLACK_MAPCELLS_CHECK=1 Sync checks the lists after each run (every link in the lists of its cells, no
/// cycles, every listed entity linked) and logs "map_cells: N objects, M cells, E errors". The number of errors
[[nodiscard]] size_t CheckConsistency();
/// The number of objects in the map (the links)
[[nodiscard]] size_t ObjectCount();

// ---- The cells of an object ----------------------------------------------------------------------------------------

/// NewCollideDescriptor::Init 0x46AB10 + GetNext 0x46AD80 on a shape (NewCollide(LH3DObject) 0x829390): the box
/// ftol((centre -/+ reach) x 0.1) clipped at 0, each cell (x outer, z inner) marked when its 7.1 m circle
/// (0x40E33333) at (10 i + 5, 10 j + 5) touches the shape, the box's middle cell when none does; in the order
/// MultiMapFixed::InsertMapObject 0x52E650 inserts them, cut at the first marked cell off the map (0x52E70D)
[[nodiscard]] std::vector<glm::ivec2> DescriptorCells(const map_collide::Shape& shape, float reach);
/// The cells InsertMapObject would put the object in now, in its order
[[nodiscard]] std::vector<glm::ivec2> CellsOf(entt::entity object);

// ---- One cell -------------------------------------------------------------------------------------------------------

/// MapCoords::GetFirstObjectMobile 0x603490 / GetFirstObjectFixed 0x6034B0: the head (unfiltered); null off the map
[[nodiscard]] entt::entity FirstMobile(glm::ivec2 cell);
[[nodiscard]] entt::entity FirstFixed(glm::ivec2 cell);
/// GetMapChild vt +0x53C: the next in that cell's list (Object 0x418C90; MultiMapFixed 0x52E400 = bsearch 0x52DC30 by
/// the cell's x and z; FishFarm 0x52CAD0). Unfiltered
[[nodiscard]] entt::entity GetMapChild(entt::entity object, glm::ivec2 cell);
/// The readers' filter: what the original has out of the map (invalid, in the hand, flying, held out)
[[nodiscard]] bool IsReadable(entt::entity object);
/// While one lives, the readers share one snapshot of the hand (held, thrown) and of the flying physics bodies instead
/// of taking it on each call (held out / invalid are still read live). openblack's cost only: the original takes the
/// objects out of its lists at once. Take it only around reads whose callbacks do not pick up, throw or launch
/// objects. Nests
class ReadBatch
{
public:
	ReadBatch();
	~ReadBatch();
	ReadBatch(const ReadBatch&) = delete;
	ReadBatch& operator=(const ReadBatch&) = delete;
};

/// MapCellIterator (GetFirstIterator 0x6034D0 + MoveToMobileObsIfNeededAndPoss fn_006827E0): the fixed list, then the
/// mobile one, each from the head; the next is taken before fn runs (fn_00603500). fn returns false to stop. Off the
/// map: nothing (the callers check InBounds first)
void ForEachInCell(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn);
/// The same walk as a snapshot (an object fn deletes or moves cannot break it)
[[nodiscard]] std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell);
/// The mobile list only (+0, 0x603490 + GetMapChild), from the head
[[nodiscard]] std::vector<entt::entity> MobileInCell(glm::ivec2 cell);
void ForEachMobile(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn);
/// MapCoords::FindType 0x6045C0 -> MapCell::FindTypeOnMap 0x6015E0. type ANY (-1): the fixed list, then the mobile
/// one (after the last of the fixed list, when its type counts as fixed, the mobile head: 0x601621). Any other type:
/// only its own list (the fixed one when CountsAsFixed(type), 0x601646), the first after `after` of that type. Null off
/// the map
[[nodiscard]] entt::entity FindType(glm::ivec2 cell, ObjectType type, entt::entity after = entt::null);
/// MapCell::FindFixedOnMap 0x601690: the fixed list from after's child (or the head), the first MultiMapFixed
[[nodiscard]] entt::entity FindFixedOnMap(glm::ivec2 cell, entt::entity after = entt::null);
/// MapCoords::IsFixed 0x603790 -> MapCell::IsFixed 0x601EA0: only the head of the fixed list, a MultiMapFixed
[[nodiscard]] bool IsFixed(glm::ivec2 cell);
/// fn_00604F40: the MapCoords' cell is this one (the effects walk a multi-cell object only in its own cell)
[[nodiscard]] constexpr bool IsOwnCell(const map_coords::MapCoords& coords, glm::ivec2 cell)
{
	return map_coords::Cell(coords) == cell;
}

// ---- Searches over several cells -----------------------------------------------------------------------------------

/// MapCoords::FindNearType 0x6045F0 (`ret 8`): the cells of the square +-r (GUtils metres -> 16.16, the signed high
/// words), x outer, z inner, InBounds; ONE list (the fixed one when CountsAsFixed(type), else the mobile one, so ANY
/// is the mobile list only); the nearest by GetDistanceInMetres, from FLT_MAX: not cut at r
[[nodiscard]] entt::entity FindNearType(const map_coords::MapCoords& coords, ObjectType type, float radius);
/// MapCoords::FindNearForScript 0x604370 (`ret 0x10`): the same square (x outer; z counted as (high & 0xFFFF) - low +
/// 1, 0x6044A7..0x6044AF), fn_00601F40 + InBounds on each cell, the full walk; among those pred accepts, the nearest
/// (from the totem for a worship site, GetTotemPos 0x77CF30), strictly, from FLT_MAX: not cut at r
[[nodiscard]] entt::entity FindNearForScript(const map_coords::MapCoords& coords,
                                             const std::function<bool(entt::entity)>& pred, float radius);
/// fn_00604AF0 / fn_00604C30: the spiral of max(3, ftol(ceil(2r / 10)))^2 cells (0x604C38..0x604C80), FindType(ANY) in
/// each; pred, not excluded, d < r and (d < best or none yet); stops when best x 1.5 [0x8AB24C] + 10 [0x930050] < the
/// distance to the cell
[[nodiscard]] entt::entity FindNearestInSpiral(const map_coords::MapCoords& coords,
                                               const std::function<bool(entt::entity)>& pred, float radius,
                                               entt::entity excluded = entt::null);
/// 0x604870 (`ret 0x18`: the object, a 16-byte member pointer, r): the square +-r of the cells in the player's influence
/// (CalculatePlayerInfluence 0x5CD170 > 0), the full walk; score(obj) not below the best, then score x
/// GetDistanceModifier(GetDistanceInMetres, r) 0x74F290 above the best (from 0)
[[nodiscard]] entt::entity FindNearInfluenced(const map_coords::MapCoords& coords, PlayerNames player,
                                              const std::function<float(entt::entity)>& score, float radius);
/// fn_006022C0 (MapCoords this, Object* self, bool skipLiving): the highest GetTopPos (vt +0x630) of the objects of
/// this cell (fixed, then mobile) other than self (and, with skipLiving, not living nor moving) whose in-cell offset is
/// within dx^2 + dz^2 < r_obj^2 + r_self^2 of self's (0x6023A1..0x60241E); 0 if none or off the map
[[nodiscard]] float TallestOverlapping(const map_coords::MapCoords& coords, entt::entity self,
                                       const map_coords::MapCoords& selfCoords, float selfRadius, bool skipLiving);

// ---- Towns (no cells) -----------------------------------------------------------------------------------------------

/// GGame::GetNextPlayerAndNeutral 0x550980 (the 8 slots from g_game+0x18, 0xA60 apart, the neutral one last) x each
/// player's town list (GPlayer +0xA50, Town +0x75C), filled at the tail (fn_0064C090): the oldest first. openblack: the
/// towns whose owner is the player, by Town::id (inferido: a town that changes hands would go to the end of its new
/// owner's list)
void ForEachTown(const std::function<bool(entt::entity)>& fn);
[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player);
/// MapCoords::GetNearestTown(r) 0x6020E0: GetDistanceInMetres (fn_00605CD0) < best, best = r (fcom; test ah, 1)
[[nodiscard]] entt::entity GetNearestTown(const map_coords::MapCoords& coords, float radius);
/// fn_00602160: the same, only the towns with a centre (+0x9A4, 0x6021A7). (aproximado) fn_00741020's other test (an
/// IsTownCentre in the list +0x754, or +0x9A8) is not ported
[[nodiscard]] entt::entity GetNearestTownWithCentre(const map_coords::MapCoords& coords, float radius);
struct TownInCells
{
	entt::entity town {entt::null};
	uint32_t distance {0};
	uint32_t code {0}; ///< 0 none, 1 within 4 cells of the town's rectangle, 2 not
};
/// MapCoords::GetNearestTown(Town**, uint*, excl, tribe) 0x601F90 (`ret 0x10`): the whole-cell distance max(|dx|, |dz|)
/// + (min >> 1) (sar 1, 0x602003 / 0x60200B), best = 10 000 000 (0x989680), not `excluded` and not of `tribe`.
/// (aproximado) openblack keeps no town rectangle (+0x72A..+0x73A): a town found is code 2
[[nodiscard]] TownInCells GetNearestTownCells(const map_coords::MapCoords& coords, entt::entity excluded,
                                              std::optional<Tribe> tribe);
/// MapCoords::GetNearestCitadel(r) 0x602200: GPlayer +0xA48 of each player, < best, best = r
[[nodiscard]] entt::entity GetNearestCitadel(const map_coords::MapCoords& coords, float radius);
/// "any abode type" for GetNearestTownToPos
constexpr int32_t k_AnyAbodeType = 0x7FFF;
/// Town::GetNearestTownToPos 0x73B170 (pos, tribe, abode, r): the town walk, 0x74CD50 (= GetDistanceInMetres) < best,
/// best = r; the tribe (GetTribe() +0x10) when given; abode != 0x7FFF skips the towns IsAbodeTypeInTown 0x73D6B0 finds
/// it in (it accepts the towns WITHOUT that abode type)
[[nodiscard]] entt::entity GetNearestTownToPos(const map_coords::MapCoords& coords, std::optional<Tribe> tribe,
                                               int32_t abodeType, float radius);
/// fn_00552FF0 (`ret 4`, one MapCoords): the global town list g_game+0x205C84 (inferido: by Town::id), the first
/// always taken, then GetDistanceInMetres < best. It has no id branch
[[nodiscard]] entt::entity FindNearestTownInList(const map_coords::MapCoords& coords);

namespace detail
{
/// Test hook: when set and it gives a shape, a MultiMapFixed's cells are DescriptorCells(shape, reach) of it (the
/// tests have no meshes)
using ShapeProvider = bool (*)(entt::entity object, map_collide::Shape& shape, float& reach);
void SetShapeProviderForTests(ShapeProvider provider);
} // namespace detail

} // namespace openblack::ecs::map_cells
