/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCells.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace map_cells = openblack::ecs::map_cells;

static_assert(map_cells::CountsAsFixed(ObjectType::Abode) && !map_cells::CountsAsFixed(ObjectType::Villager) &&
              map_cells::CountsAsFixed(ObjectType::Pot) && map_cells::CountsAsFixed(ObjectType::MobileStatic) &&
              !map_cells::CountsAsFixed(ObjectType::MobileObject) && !map_cells::CountsAsFixed(ObjectType::Firefly) &&
              map_cells::CountsAsFixed(ObjectType::MapShield) && !map_cells::CountsAsFixed(ObjectType::Any) &&
              !map_cells::CountsAsFixed(ObjectType::Invalid));

namespace
{
/// NewCollideDescriptor::Init 0x46AB10: the circle of a map cell (push 0x40E33333 at 0x46AC23)
constexpr float k_CellCircleRadius = 7.1f;
/// [0x8AA390] = 1.0, added to scale x mesh+0x30 (0x46AB6E)
constexpr float k_DescriptorReachAdd = 1.0f;
/// [0x8AC404] = 0.1, metres -> cells (0x46AB7A)
constexpr float k_CellsPerMetre = 0.1f;
/// FindNearestInSpiral's stop, best x 1.5 [0x8AB24C] + 10 [0x930050] (0x604CAC / 0x604CB2)
constexpr float k_SpiralStopFactor = 1.5f;
constexpr float k_SpiralStopAdd = 10.0f;
/// GetNearestTown (cells) 0x601F90: the best starts at 10 000 000 (0x989680)
constexpr uint32_t k_TownCellsStart = 10000000u;
/// Tree::CreateCollideData 0x74C5F0: fn_00829590(the world matrix's position, 0.3) (push 0x3E99999A at 0x74C622)
constexpr float k_TreeCollideRadius = 0.3f;
/// MapCell::CollideWithFixe 0x601D10: NewCollide::Obj(0.5, point) (push 0x3F000000 at 0x601D4C, ctor 0x82AD90)
constexpr float k_FixedTestRadius = 0.5f;

/// MapCell (bw1-decomp src/Black/Map.h): +0 the first mobile object, +4 the first fixed one
struct Cell
{
	entt::entity mobile {entt::null};
	entt::entity fixed {entt::null};
};

/// MultiMapFixed +0x68: {Object* next; word x; word z} (8 bytes) per cell
struct MultiChild
{
	entt::entity next {entt::null};
	int16_t x {0};
	int16_t z {0};
};

/// An object's map fields: Object +0x20 (next), +0x24 bit 15 (the fixed list), +0x38 (the mobile list's previous),
/// MultiMapFixed +0x68 (one next per cell)
struct Link
{
	map_cells::InsertKind kind {map_cells::InsertKind::None};
	ObjectType type {ObjectType::Invalid};
	bool fixedList {false};
	entt::entity next {entt::null};
	entt::entity prev {entt::null};
	glm::ivec2 cell {0};
	std::vector<MultiChild> children;
	// what the cells were worked out from (openblack: Sync's move test)
	int32_t x {0};
	int32_t z {0};
	glm::mat3 rotation {1.0f};
	glm::vec3 scale {1.0f};
	entt::id_type mesh {0};
	/// GetCollideData vt +0x858 (SingleMapFixed +0x58 0x52EB30, MultiMapFixed +0x78 0x401630; Object 0x419B30 is 0),
	/// built by CreateCollideData on insert (SingleMapFixed::InsertMapObject 0x52E633 vt +0x864, MultiMapFixed's
	/// 0x52E669 vt +0x908), so kept here from the same insert
	std::optional<map_collide::Shape> collide;
};

std::vector<Cell> g_Cells;
std::unordered_map<entt::entity, Link> g_Links;
std::unordered_set<entt::entity> g_HeldOut;
const void* g_Registry = nullptr;
map_cells::detail::ShapeProvider g_ShapeProvider = nullptr;

bool CheckEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("OPENBLACK_MAPCELLS_CHECK");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return enabled;
}

Registry* RegistryOrNull()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// A new registry (the tests make one per test) drops every link: an entity of another registry is not this one's
void CheckRegistry()
{
	const void* current = RegistryOrNull();
	if (current != g_Registry)
	{
		map_cells::Clear();
		g_Registry = current;
	}
}

/// ToMap 0x603430 on a cell: NULL off the map (InBounds 0x6042C0, the 512 of [0x59C8] / [0x59C4])
Cell* CellAt(glm::ivec2 cell)
{
	if (!map_coords::InBounds(cell))
	{
		return nullptr;
	}
	if (g_Cells.empty())
	{
		g_Cells.resize(static_cast<size_t>(map_coords::k_MapCells) * map_coords::k_MapCells); // GMap::Init 0x6014C0
	}
	return &g_Cells[static_cast<size_t>(cell.x) * map_coords::k_MapCells + static_cast<size_t>(cell.y)];
}

const Cell* CellIfAny(glm::ivec2 cell)
{
	if (!map_coords::InBounds(cell) || g_Cells.empty())
	{
		return nullptr;
	}
	return &g_Cells[static_cast<size_t>(cell.x) * map_coords::k_MapCells + static_cast<size_t>(cell.y)];
}

Link* LinkOf(entt::entity object)
{
	const auto found = g_Links.find(object);
	return found != g_Links.end() ? &found->second : nullptr;
}

bool IsOneCell(map_cells::InsertKind kind)
{
	return kind == map_cells::InsertKind::Object || kind == map_cells::InsertKind::SingleMapFixed ||
	       kind == map_cells::InsertKind::FishFarm;
}

bool IsFixedClass(map_cells::InsertKind kind)
{
	return kind == map_cells::InsertKind::SingleMapFixed || kind == map_cells::InsertKind::MultiMapFixed ||
	       kind == map_cells::InsertKind::FishFarm;
}

/// CompareMultiChilds 0x52DBC0: by x, then z
bool ChildLess(const MultiChild& a, const MultiChild& b)
{
	return a.x != b.x ? a.x < b.x : a.z < b.z;
}

/// GetMapChild vt +0x53C: Object 0x418C90 (+0x20); MultiMapFixed 0x52E400 (bsearch 0x52DC30 by x, z); FishFarm 0x52CAD0
/// (its one position)
entt::entity ChildOf(entt::entity object, glm::ivec2 cell)
{
	const auto* link = LinkOf(object);
	if (link == nullptr)
	{
		return entt::null;
	}
	if (link->kind != map_cells::InsertKind::MultiMapFixed)
	{
		return link->next;
	}
	const MultiChild key {entt::null, static_cast<int16_t>(cell.x), static_cast<int16_t>(cell.y)};
	const auto found = std::lower_bound(link->children.begin(), link->children.end(), key, ChildLess);
	if (found == link->children.end() || found->x != key.x || found->z != key.z)
	{
		return entt::null;
	}
	return found->next;
}

/// SetMapChild vt +0x540: Object 0x418CC0; MultiMapFixed 0x52E420 (a linear search: the children may not be sorted
/// yet while InsertMapObject runs)
void SetChildOf(entt::entity object, glm::ivec2 cell, entt::entity next)
{
	auto* link = LinkOf(object);
	if (link == nullptr)
	{
		return;
	}
	if (link->kind != map_cells::InsertKind::MultiMapFixed)
	{
		link->next = next;
		return;
	}
	for (auto& child : link->children)
	{
		if (child.x == static_cast<int16_t>(cell.x) && child.z == static_cast<int16_t>(cell.y))
		{
			child.next = next;
			return;
		}
	}
}

ObjectType LinkType(entt::entity object)
{
	const auto* link = LinkOf(object);
	return link != nullptr ? link->type : ObjectType::Invalid;
}

/// Fixed::InsertMapObjectToCell 0x52DEA0 (= InsertMapObjectToCellAssumeFixed 0x52DEE0): the old head becomes the
/// child (SetMapChild vt +0x540; with no old head the link is not touched), then SetFirstObjectFixed 0x601B70
void InsertFixedHead(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	if (cell->fixed != entt::null)
	{
		SetChildOf(object, cellXZ, cell->fixed);
	}
	cell->fixed = object;
}

/// Object::InsertMapObjectToCell 0x636830: with +0x25 & 0x80 (the fixed list) at the TAIL of the fixed list (the head
/// when empty, else the last's SetMapChild); else at the HEAD of the mobile list ((cell+0)->+0x38 = this,
/// SetMapChild(cell+0), SetFirstObjectMobile 0x601B60)
void InsertObject(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	auto* link = LinkOf(object);
	if (cell == nullptr || link == nullptr)
	{
		return;
	}
	if (link->fixedList)
	{
		if (cell->fixed == entt::null)
		{
			cell->fixed = object;
			return;
		}
		auto last = cell->fixed;
		for (auto next = ChildOf(last, cellXZ); next != entt::null; next = ChildOf(last, cellXZ))
		{
			last = next;
		}
		SetChildOf(last, cellXZ, object);
		return;
	}
	if (cell->mobile != entt::null)
	{
		if (auto* head = LinkOf(cell->mobile))
		{
			head->prev = object;
		}
		SetChildOf(object, cellXZ, cell->mobile);
	}
	cell->mobile = object;
}

/// Object::RemoveMapObjectFromCell 0x6368D0: the list of bit 15. Fixed: the head takes the child, else the previous one
/// found by walking does. Mobile: no previous (+0x38) is the head (cell+0 = child, child->+0x38 = 0), else
/// prev->SetMapChild(child), child->+0x38 = prev, this->+0x38 = 0. Then always SetMapChild(0).
/// (openblack guard) a Fixed class leaves the fixed list it entered at the head even if its type's bit says mobile
void RemoveFromCell(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	auto* link = LinkOf(object);
	if (cell == nullptr || link == nullptr)
	{
		return;
	}
	const auto child = ChildOf(object, cellXZ);
	if (link->fixedList || IsFixedClass(link->kind))
	{
		if (cell->fixed == object)
		{
			cell->fixed = child;
		}
		else
		{
			for (auto previous = cell->fixed; previous != entt::null;)
			{
				const auto next = ChildOf(previous, cellXZ);
				if (next == object)
				{
					SetChildOf(previous, cellXZ, child);
					break;
				}
				previous = next;
			}
		}
	}
	else
	{
		const auto previous = link->prev;
		if (previous == entt::null || LinkOf(previous) == nullptr)
		{
			if (cell->mobile == object)
			{
				cell->mobile = child;
			}
			if (auto* next = LinkOf(child))
			{
				next->prev = entt::null;
			}
		}
		else
		{
			SetChildOf(previous, cellXZ, child);
			if (auto* next = LinkOf(child))
			{
				next->prev = previous;
			}
			link->prev = entt::null;
		}
	}
	SetChildOf(object, cellXZ, entt::null);
}

/// The y angle of an openblack Transform (the archetypes' eulerAngleY(-a)): the sign map_collide::FromMesh turns by
float YAngleOf(const glm::mat3& rotation)
{
	return std::atan2(rotation[0][2], rotation[0][0]);
}

/// What the readers skip: what the original has already taken out of the map (Object::RemoveMapObject from
/// InitialisePhysics 0x637480+0x3A, InitialisePhysicsFromHand +0x63, the hand's pick-up, CleanupWhenDeleted 0x6377F0)
struct ReadFilter
{
	const Registry* registry {RegistryOrNull()};
	std::optional<entt::entity> held;
	std::vector<entt::entity> thrown;
	std::vector<entt::entity> flying;

	ReadFilter()
	{
		if (Locator::handSystem::has_value())
		{
			const auto& hand = Locator::handSystem::value();
			held = hand.GetHeldObject();
			thrown = hand.GetThrownObjects();
		}
		// physics::PhysicsObjects::IsFlying of each, once: a body that is not a resting proxy
		physics::PhysicsObjects::ForEach([this](const physics::PhysicsObject& po) {
			if (!po.body.resting)
			{
				flying.push_back(po.entity);
			}
		});
	}

	[[nodiscard]] bool Out(entt::entity object) const
	{
		if (registry == nullptr || !registry->Valid(object) || g_HeldOut.contains(object))
		{
			return true;
		}
		if (held && *held == object)
		{
			return true;
		}
		if (std::find(thrown.begin(), thrown.end(), object) != thrown.end())
		{
			return true;
		}
		return std::find(flying.begin(), flying.end(), object) != flying.end();
	}
};

/// The snapshot of the live map_cells::ReadBatch, if any, and how deep they nest
std::optional<ReadFilter> g_BatchFilter;
int g_BatchDepth = 0;

/// The filter of one read: the batch's when one lives, else its own
class FilterRef
{
public:
	FilterRef()
	{
		if (!g_BatchFilter)
		{
			_own.emplace();
		}
	}
	[[nodiscard]] bool Out(entt::entity object) const { return _own ? _own->Out(object) : g_BatchFilter->Out(object); }

private:
	std::optional<ReadFilter> _own;
};

/// The square of FindNearType 0x6045F0 / FindNearForScript 0x604370 / 0x604870: x * 10 * (1 / 65536) -/+ r, then
/// * 65536 / 10 and __ftol, the signed high words
int32_t Corner(int32_t fixed, float offset)
{
	return map_coords::SignedCellOf(map_coords::ToFixedGUtils(map_coords::ToMetres(fixed) + offset));
}

/// The walk of FindNearForScript 0x604370 and 0x604870: x from the low corner to the high one (signed words, `jg`);
/// the z count is (high & 0xFFFF) - low + 1 (0x6044A7..0x6044AF, movsx low, `and esi, 0xFFFF` high) and z goes up as
/// a 32-bit counter whose low word is the cell (fn_00601F40 stores words)
template <typename Fn>
void ScriptSquare(const map_coords::MapCoords& coords, float radius, Fn&& fn)
{
	const int32_t lowX = Corner(coords.x, -radius);
	const int32_t lowZ = Corner(coords.z, -radius);
	const int32_t highX = Corner(coords.x, radius);
	const int32_t highZ = Corner(coords.z, radius);
	if (lowX > highX || lowZ > highZ)
	{
		return; // cmp ax, cx; jg (0x604471, 0x60449E)
	}
	const int32_t countZ = static_cast<int32_t>(static_cast<uint16_t>(highZ)) - lowZ + 1;
	for (int32_t x = lowX; x <= highX; ++x)
	{
		for (int32_t i = 0, z = lowZ; i < countZ; ++i, ++z)
		{
			// fn_00601F40: the words (x, z) as a MapCoords, then InBounds 0x6042C0
			const glm::ivec2 cell(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			if (map_coords::InBounds(cell))
			{
				fn(cell);
			}
		}
	}
}

/// GetTotemPos 0x77CF30 for a worship site (vt +0x304 IsWorshipSite), else the object's MapCoords +0x14. (inferido)
/// the totem is openblack's WorshipSite::totem entity
map_coords::MapCoords DistancePointOf(entt::entity object)
{
	if (const auto* site = Locator::entitiesRegistry::value().TryGet<const WorshipSite>(object))
	{
		if (site->totem != entt::null && Locator::entitiesRegistry::value().Valid(site->totem))
		{
			return object::MapCoordsOf(site->totem);
		}
	}
	return object::MapCoordsOf(object);
}

/// The x, z MapCoords of a world point (the altitude is not needed for the cells)
map_coords::MapCoords XZ(const glm::vec3& position)
{
	return map_coords::FromMetres(glm::vec2(position.x, position.z));
}

void Store(Link& link, const Transform& transform, entt::entity object)
{
	const auto coords = XZ(transform.position);
	link.x = coords.x;
	link.z = coords.z;
	link.rotation = transform.rotation;
	link.scale = transform.scale;
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	link.mesh = mesh != nullptr ? mesh->id : 0;
}

/// Sync's move test. Object / SingleMapFixed / FishFarm: Object::MoveMapObject 0x636A40 re-enters only when the cell
/// (the words +0x16 / +0x1A) changes. (aproximado) FishFarm's vt +0x55C is MultiMapFixed's 0x52E4F0 (vtable 0x8DADC0),
/// which re-enters (at the head) on any MapCoords change even in the same cell; a fish farm does not move, so the cell
/// test is kept for it. MultiMapFixed: 0x52E4F0 on any MapCoords change and SetXYZAnglesAndScale on a
/// turn or a scale; (aproximado) the altitude is not compared (the ground openblack settles while drawing must not
/// send the object to the head every turn) and a new mesh counts as a change of shape
bool Moved(const Link& link, const Transform& transform, entt::entity object)
{
	const auto coords = XZ(transform.position);
	if (IsOneCell(link.kind))
	{
		return map_coords::Cell(coords) != link.cell;
	}
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	return coords.x != link.x || coords.z != link.z || transform.rotation != link.rotation || transform.scale != link.scale ||
	       (mesh != nullptr ? mesh->id : 0) != link.mesh;
}
} // namespace

// ---- Classification -------------------------------------------------------------------------------------------------

map_cells::InsertKind map_cells::KindOf(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object) || !registry->AllOf<Transform>(object))
	{
		return InsertKind::None;
	}
	// SpellSeed 0x728F30 and MagicFireBall 0x682D10: InsertMapObject is a bare ret
	if (registry->AnyOf<MagicFireBall, SpellSeed>(object))
	{
		return InsertKind::None;
	}
	// FishFarm::InsertMapObject 0x52CA10 (FishFarm : MultiMapFixed, its own override)
	if (registry->AllOf<FishFarm>(object))
	{
		return InsertKind::FishFarm;
	}
	// MultiMapFixed::InsertMapObject 0x52E650 (Abode 0x403EE0 = jmp 0x52E650): the abodes (fields, storage pits, town
	// centres), features, animated statics, mobile statics (rocks, bonfires), dead trees, big forests, totems, worship
	// sites, the citadel heart (openblack's Temple), spell icons, magic teleports (: MobileStatic) and fragments
	// (: Rock); the vtable sweep of map_cell_queries_original.md §3
	if (registry->AnyOf<Abode, Field, Feature, AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite,
	                    Temple, SpellIcon, MagicTeleport, Fragment>(object))
	{
		return InsertKind::MultiMapFixed;
	}
	// SingleMapFixed::InsertMapObject 0x52E620: Tree, MagicTree, MapShield
	if (registry->AnyOf<Tree, MagicTree, MapShield>(object))
	{
		return InsertKind::SingleMapFixed;
	}
	// Object::InsertMapObject 0x636740 (MobileObject 0x607250 -> 0x636830 too): the living, the creature, the street
	// lanterns (GStreetLantern : Object), pots and piles, the one-off orbs, mobile objects, the whale (openblack's shark)
	if (registry->AnyOf<Villager, Animal, Creature, StreetLantern, Pot, OneOffSpellSeed, MobileObject, Shark>(object))
	{
		return InsertKind::Object;
	}
	return InsertKind::None;
}

bool map_cells::IsMultiMapFixedClass(entt::entity object)
{
	const auto kind = KindOf(object);
	return kind == InsertKind::MultiMapFixed || kind == InsertKind::FishFarm;
}

ObjectType map_cells::TypeOf(entt::entity object)
{
	if (KindOf(object) == InsertKind::None)
	{
		return ObjectType::Invalid;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const InfoConstants* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	// the info row's type (info +0x10); without info.dat (the unit tests) the type every row of that table has
	const auto row = [info](auto table, size_t index, ObjectType fallback) {
		if (info == nullptr || index >= (info->*table).size())
		{
			return fallback;
		}
		return (info->*table)[index].type;
	};
	if (const auto* c = registry.TryGet<const AnimatedStatic>(object))
	{
		return row(&InfoConstants::animatedStatic, static_cast<size_t>(c->type),
		           ObjectType::Feature);
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return info != nullptr ? info->fishFarm.type : ObjectType::FishFarm;
	}
	if (const auto* c = registry.TryGet<const BigForest>(object))
	{
		return row(&InfoConstants::bigForest, static_cast<size_t>(c->type),
		           ObjectType::BigForest);
	}
	if (registry.AllOf<TotemStatue>(object))
	{
		// (inferido) the first GTotemStatueInfo: openblack's statue keeps no row
		return row(&InfoConstants::totemStatue, 0, ObjectType::TotemStatue);
	}
	if (const auto* c = registry.TryGet<const WorshipSite>(object))
	{
		return row(&InfoConstants::worshipSite, c->infoIndex, ObjectType::Citadel);
	}
	if (registry.AllOf<Temple>(object))
	{
		return info != nullptr ? info->citadelHeart.type : ObjectType::Citadel;
	}
	if (const auto* c = registry.TryGet<const SpellIcon>(object))
	{
		return row(&InfoConstants::spellIcon, c->infoIndex, ObjectType::Citadel);
	}
	// (inferido) a GMobileStaticInfo row: MagicTeleport 0x5FC130 (GMobileStaticInfo 0xD3B614), Fragment 0x76E9D0
	// (0xD3A930) and DeadTree (: Rock, whose ctor 0x6E6F70 takes a GMobileStaticInfo) pass one, GStreetLantern::Create
	// 0x7346E0 takes one; openblack keeps no row for them, so the first row's type
	if (registry.AnyOf<MagicTeleport, Fragment, DeadTree, StreetLantern>(object))
	{
		return row(&InfoConstants::mobileStatic, 0, ObjectType::MobileStatic);
	}
	if (registry.AllOf<OneOffSpellSeed>(object))
	{
		// OneOffSpellSeed ctor 0x72A3A0: MobileObject(pos, GMobileObjectInfo 0xD39F3C = row 25, ...); its type in
		// info.dat is 20 MOBILE_OBJECT (test_map_cells), so the orb is at the head of the mobile list
		return row(&InfoConstants::mobileObject, static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed),
		           ObjectType::MobileObject);
	}
	if (registry.AllOf<Shark>(object))
	{
		// the whale's GMobileObjectInfo (MobileObjectInfo::Whale)
		return row(&InfoConstants::mobileObject, static_cast<size_t>(MobileObjectInfo::Whale), ObjectType::MobileObject);
	}
	if (registry.AllOf<Creature>(object))
	{
		return row(&InfoConstants::creature, 0, ObjectType::Creature); // (inferido)
	}
	if (info != nullptr)
	{
		// the rows openblack keeps: physics::PhysicsObjects::ObjectInfo (mobile statics, pots, mobile objects, orbs,
		// trees, dead trees, shields, animals, villagers), fields (fieldType), abodes, features
		if (const auto* objectInfo = fire::traits::InfoOf(object))
		{
			return objectInfo->type;
		}
	}
	// no row: the type of the class's table (inferido)
	if (registry.AllOf<Field>(object))
	{
		return ObjectType::Field;
	}
	if (registry.AllOf<Abode>(object))
	{
		return ObjectType::Abode;
	}
	if (registry.AllOf<Feature>(object))
	{
		return ObjectType::Feature;
	}
	if (registry.AllOf<MobileStatic>(object))
	{
		return ObjectType::MobileStatic;
	}
	if (registry.AnyOf<Tree, MagicTree>(object))
	{
		return ObjectType::ForestTree;
	}
	if (registry.AllOf<MapShield>(object))
	{
		return ObjectType::MapShield;
	}
	if (registry.AllOf<Pot>(object))
	{
		return ObjectType::Pot;
	}
	if (registry.AllOf<Villager>(object))
	{
		return ObjectType::Villager;
	}
	if (registry.AllOf<Animal>(object))
	{
		return ObjectType::Animal;
	}
	return ObjectType::MobileObject;
}

// ---- The cells of an object ----------------------------------------------------------------------------------------

std::vector<glm::ivec2> map_cells::DescriptorCells(const map_collide::Shape& shape, float reach)
{
	// 0x46AB74..0x46ABC2: fld centre; fsub reach; fmul 0.1; __ftol (and fadd for the high corner). (aproximado) the
	// original keeps reach (0x46AB68..0x46AB6E) and each product on the x87 stack at extended precision; here floats
	int32_t x0 = map_coords::FtoL((shape.centre.x - reach) * k_CellsPerMetre);
	int32_t x1 = map_coords::FtoL((shape.centre.x + reach) * k_CellsPerMetre);
	int32_t z0 = map_coords::FtoL((shape.centre.y - reach) * k_CellsPerMetre);
	int32_t z1 = map_coords::FtoL((shape.centre.y + reach) * k_CellsPerMetre);
	// 0x46ABC9..0x46ABE7: a negative low corner is 0, and only then a negative high one is 0 too
	if (x0 < 0)
	{
		x0 = 0;
		if (x1 < 0)
		{
			x1 = 0;
		}
	}
	if (z0 < 0)
	{
		z0 = 0;
		if (z1 < 0)
		{
			z1 = 0;
		}
	}
	const int32_t width = x1 - x0 + 1;
	const int32_t depth = z1 - z0 + 1;
	if (width <= 0 || depth <= 0)
	{
		return {};
	}
	std::vector<uint8_t> marked(static_cast<size_t>(width) * static_cast<size_t>(depth), 0);
	int32_t count = 0;
	// 0x46AC7D..0x46AD04: x outer, z inner, the mask index always moves on; a cell is tested only when it is on the map
	// (x < [0x59C8], z < [0x59C4], unsigned) against a 7.1 m circle at (10 x + 5, 10 z + 5) (fild of the integers)
	size_t index = 0;
	for (int32_t x = x0; x <= x1; ++x)
	{
		for (int32_t z = z0; z <= z1; ++z, ++index)
		{
			if (static_cast<uint32_t>(x) >= map_coords::k_MapCells || static_cast<uint32_t>(z) >= map_coords::k_MapCells)
			{
				continue;
			}
			const glm::vec2 point(static_cast<float>(x * 10 + 5), static_cast<float>(z * 10 + 5));
			if (map_collide::Collide(point, k_CellCircleRadius, shape))
			{
				marked[index] = 0xFF;
				++count;
			}
		}
	}
	// 0x46AD06..0x46AD3B: none hit -> the middle one, ((w / 2) x d) + d / 2 (cdq; sub; sar 1: towards 0), not checked
	// against the map
	if (count == 0)
	{
		marked[static_cast<size_t>((width / 2) * depth + depth / 2)] = 0xFF;
	}
	// GetNext 0x46AD80: the marked cells in that order; a marked cell off the map gives NULL, and InsertMapObject
	// 0x52E70D stops there
	std::vector<glm::ivec2> cells;
	index = 0;
	for (int32_t x = x0; x <= x1; ++x)
	{
		for (int32_t z = z0; z <= z1; ++z, ++index)
		{
			if (marked[index] == 0)
			{
				continue;
			}
			if (static_cast<uint32_t>(x) >= map_coords::k_MapCells || static_cast<uint32_t>(z) >= map_coords::k_MapCells)
			{
				return cells;
			}
			cells.emplace_back(x, z);
		}
	}
	return cells;
}

namespace
{
/// NewCollide(LH3DObject) 0x829390 of the object's mesh (map_collide::FromMesh) at its position, turned and scaled, and
/// the descriptor's reach = [obj3d +0x44] (the scale) x mesh +0x30 + 1 (0x46AB68..0x46AB6E). The tests' provider first.
/// False without a mesh (openblack: no NewCollide to build)
bool MeshShape(entt::entity object, const Transform& transform, map_collide::Shape& shape, float& reach)
{
	if (g_ShapeProvider != nullptr && g_ShapeProvider(object, shape, reach))
	{
		return true;
	}
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	const float scale = object::GetScale(object);
	if (mesh == nullptr || !map_collide::FromMesh(mesh->id, glm::vec2(transform.position.x, transform.position.z),
	                                              YAngleOf(transform.rotation), scale, shape))
	{
		return false;
	}
	reach = scale * object::MeshHalfDiagonal(mesh->id) + k_DescriptorReachAdd;
	return true;
}

/// The cells InsertMapObject puts the object in and, when asked, the collide data its insert builds (GetCollideData
/// vt +0x858 afterwards):
/// - Object (0x636740): none, Object::GetCollideData 0x419B30 is `xor eax, eax` (villagers, animals, pots and piles,
///   street lanterns, mobile objects: the vtables of Object, MobileObject, Pot, GStreetLantern, Villager read).
/// - SingleMapFixed (0x52E620 -> vt +0x864 at 0x52E633): Tree / MagicTree a 0.3 circle at the position
///   (Tree::CreateCollideData 0x74C5F0, both vtables); MapShield and the rest SingleMapFixed::CreateCollideData 0x52F510,
///   NewCollide(LH3DObject +0x40) 0x829390: the mesh's shape.
/// - MultiMapFixed (0x52E650 -> vt +0x908 at 0x52E669): MultiMapFixed::CreateCollideData 0x52F550, the mesh's shape;
///   BigForest's 0x439580 is a jmp to ReleaseCollideData 0x52F6D0: none. (aproximado) WorshipSite 0x77E490 and
///   CitadelHeart 0x468FB0 build their own shapes (not ported): the mesh's.
/// - FishFarm (0x52CA10 never calls vt +0x908; the MultiMapFixed ctor zeroes +0x78 at 0x52E26F): none.
/// (aproximado) a fixed object without a mesh in openblack has no shape (and only the cell of its position)
std::vector<glm::ivec2> CellsAndCollide(entt::entity object, map_cells::InsertKind kind,
                                        std::optional<map_collide::Shape>* collide)
{
	using map_cells::InsertKind;
	if (kind == InsertKind::None)
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(object);
	// Object::InsertMapObject 0x636740 (and SingleMapFixed, FishFarm::GetNextPos 0x52C940: one position, its MapCoords
	// +0x14): ToMap of the position, nothing off the map
	const auto own = map_coords::CellOf(transform.position);
	const auto ownCell = [&own]() -> std::vector<glm::ivec2> {
		if (!map_coords::InBounds(own))
		{
			return {};
		}
		return {own};
	};
	if (kind == InsertKind::SingleMapFixed)
	{
		if (collide != nullptr)
		{
			if (registry.AnyOf<Tree, MagicTree>(object))
			{
				*collide = map_collide::Shape {glm::vec2(transform.position.x, transform.position.z), k_TreeCollideRadius,
				                               {}, 0.0f, "tree"};
			}
			else
			{
				map_collide::Shape shape;
				float reach = 0.0f;
				if (MeshShape(object, transform, shape, reach))
				{
					*collide = std::move(shape);
				}
			}
		}
		return ownCell();
	}
	if (kind != InsertKind::MultiMapFixed)
	{
		return ownCell();
	}
	// NewCollideDescriptor(this) 0x46A860: the same NewCollide(LH3DObject) as CreateCollideData 0x52F550
	map_collide::Shape shape;
	float reach = 0.0f;
	if (!MeshShape(object, transform, shape, reach))
	{
		return ownCell(); // (aproximado) no mesh: the cell of its position
	}
	auto cells = map_cells::DescriptorCells(shape, reach);
	if (collide != nullptr && !registry.AllOf<BigForest>(object))
	{
		*collide = std::move(shape);
	}
	return cells;
}
} // namespace

std::vector<glm::ivec2> map_cells::CellsOf(entt::entity object)
{
	return CellsAndCollide(object, KindOf(object), nullptr);
}

// ---- The hooks ------------------------------------------------------------------------------------------------------

void map_cells::InsertMapObject(entt::entity object)
{
	CheckRegistry();
	auto* registry = RegistryOrNull();
	if (registry == nullptr || LinkOf(object) != nullptr || g_HeldOut.contains(object))
	{
		return;
	}
	const auto kind = KindOf(object);
	if (kind == InsertKind::None)
	{
		return;
	}
	Link link;
	const auto cells = CellsAndCollide(object, kind, &link.collide);
	if (cells.empty() && kind != InsertKind::MultiMapFixed)
	{
		return; // ToMap NULL: not in the map, +0x24 bit 0 stays clear
	}
	link.kind = kind;
	link.type = TypeOf(object);
	link.fixedList = CountsAsFixed(link.type); // InitialiseIsFixedForMapList 0x63A640
	Store(link, registry->Get<const Transform>(object), object);
	if (kind == InsertKind::MultiMapFixed)
	{
		// AllocateMultiChild 0x52EA50, JustMapXZ::Init 0x5E1920, then AssumeFixed 0x52DEE0 per cell
		link.children.reserve(cells.size());
		auto& stored = g_Links.emplace(object, std::move(link)).first->second;
		for (const auto cell : cells)
		{
			stored.children.push_back({entt::null, static_cast<int16_t>(cell.x), static_cast<int16_t>(cell.y)});
			InsertFixedHead(object, cell);
		}
		// SortChildren 0x52DC10 (qsort with CompareMultiChilds 0x52DBC0)
		auto* sorted = LinkOf(object);
		std::sort(sorted->children.begin(), sorted->children.end(), ChildLess);
		return;
	}
	link.cell = cells.front();
	g_Links.emplace(object, std::move(link));
	if (kind == InsertKind::Object)
	{
		InsertObject(object, cells.front()); // vt +0x54C = 0x636830 (MobileObject 0x607250)
	}
	else
	{
		InsertFixedHead(object, cells.front()); // SingleMapFixed 0x52F440 -> 0x52DEA0; FishFarm AssumeFixed 0x52DEE0
	}
}

void map_cells::RemoveMapObject(entt::entity object)
{
	auto* link = LinkOf(object);
	if (link == nullptr)
	{
		return;
	}
	// (aproximado) the cells it was put in: 0x52E7B0 builds the descriptor again from the current state, which is the
	// same because the original removes before it moves or turns the object (ActualMoveMapObject, SetXYZAnglesAndScale)
	if (link->kind == InsertKind::MultiMapFixed)
	{
		const auto children = link->children;
		for (const auto& child : children)
		{
			RemoveFromCell(object, glm::ivec2(child.x, child.z));
		}
	}
	else
	{
		RemoveFromCell(object, link->cell);
	}
	g_Links.erase(object); // +0x24 &= ~1
}

void map_cells::MoveMapObject(entt::entity object, const glm::vec3& position)
{
	auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return;
	}
	auto* transform = registry->TryGet<Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const auto* link = LinkOf(object);
	bool reinsert = false;
	if (link != nullptr)
	{
		if (IsOneCell(link->kind))
		{
			// 0x636A40: the words +0x16 / +0x1A of the new MapCoords against the old ones. (aproximado) a FishFarm has
			// MultiMapFixed's 0x52E4F0 (any MapCoords change); it does not move
			reinsert = map_coords::CellOf(position) != link->cell;
		}
		else
		{
			// 0x52E4F0: MapCoords::operator== 0x605660 (x, z and the altitude)
			reinsert = !(map_coords::FromWorld(position) == map_coords::FromWorld(transform->position));
		}
	}
	if (!reinsert)
	{
		transform->position = position;
		return;
	}
	// ActualMoveMapObject 0x638040: Remove, SetPos, Insert
	RemoveMapObject(object);
	transform->position = position;
	InsertMapObject(object);
}

void map_cells::OnAnglesOrScaleChanged(entt::entity object)
{
	if (LinkOf(object) == nullptr)
	{
		return;
	}
	RemoveMapObject(object);
	InsertMapObject(object);
}

bool map_cells::IsObjectInMap(entt::entity object)
{
	return LinkOf(object) != nullptr;
}

void map_cells::SetHeldOutOfMap(entt::entity object, bool held)
{
	CheckRegistry();
	if (held)
	{
		RemoveMapObject(object);
		g_HeldOut.insert(object);
	}
	else
	{
		g_HeldOut.erase(object);
	}
}

void map_cells::Clear()
{
	g_Cells.clear();
	g_Links.clear();
	g_HeldOut.clear();
}

size_t map_cells::ObjectCount()
{
	return g_Links.size();
}

void map_cells::Sync()
{
	CheckRegistry();
	auto* registry = RegistryOrNull();
	if (registry == nullptr)
	{
		return;
	}
	const ReadFilter filter;
	// the held-out entities that went away
	std::erase_if(g_HeldOut, [registry](entt::entity e) { return !registry->Valid(e); });
	// 1. out of the map: deleted (CleanupWhenDeleted 0x6377F0), now of another class (a felled tree), in the hand or in
	// physics, or a one-cell object off the map. Taking an object out of a list does not change the others' order
	std::vector<entt::entity> out;
	for (const auto& [entity, link] : g_Links)
	{
		if (!registry->Valid(entity) || filter.Out(entity) || KindOf(entity) != link.kind)
		{
			out.push_back(entity);
		}
	}
	for (const auto entity : out)
	{
		RemoveMapObject(entity);
	}
	// 2. the moves and the new ones, by creation index (inferido: the turn's processing order; CallVirtualFunctions-
	// ForCreation 0x636BE0+0xD8 / 0x607150+0xA9 / 0x52E890+0x184 inserts each object as it is made)
	struct Action
	{
		int64_t index;
		entt::entity entity;
		bool move;
	};
	std::vector<Action> actions;
	registry->Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		const auto kind = KindOf(entity);
		if (kind == InsertKind::None || filter.Out(entity))
		{
			return;
		}
		const auto* link = LinkOf(entity);
		if (link == nullptr)
		{
			actions.push_back({object_index::Of(entity), entity, false});
		}
		else if (Moved(*link, transform, entity))
		{
			actions.push_back({object_index::Of(entity), entity, true});
		}
	});
	std::sort(actions.begin(), actions.end(), [](const Action& a, const Action& b) {
		const auto ia = a.index < 0 ? std::numeric_limits<int64_t>::max() : a.index;
		const auto ib = b.index < 0 ? std::numeric_limits<int64_t>::max() : b.index;
		return ia != ib ? ia < ib : a.entity < b.entity;
	});
	for (const auto& action : actions)
	{
		if (action.move)
		{
			RemoveMapObject(action.entity); // ActualMoveMapObject 0x638040 / SetXYZAnglesAndScale: to the head
		}
		InsertMapObject(action.entity);
	}
	if (CheckEnabled())
	{
		const auto errors = CheckConsistency();
		size_t used = 0;
		for (const auto& cell : g_Cells)
		{
			used += (cell.mobile != entt::null || cell.fixed != entt::null) ? 1 : 0;
		}
		if (auto logger = spdlog::get("game")) // (openblack) the tests have no "game" logger
		{
			SPDLOG_LOGGER_INFO(logger, "map_cells: {} objects, {} cells, {} errors", g_Links.size(), used, errors);
		}
	}
}

size_t map_cells::CheckConsistency()
{
	size_t errors = 0;
	std::unordered_map<entt::entity, size_t> seen;
	const size_t limit = g_Links.size() + 1;
	for (size_t i = 0; i < g_Cells.size(); ++i)
	{
		const glm::ivec2 cellXZ(static_cast<int32_t>(i / map_coords::k_MapCells), static_cast<int32_t>(i % map_coords::k_MapCells));
		const auto& cell = g_Cells[i];
		for (int list = 0; list < 2; ++list)
		{
			size_t steps = 0;
			entt::entity previous = entt::null;
			for (auto e = list == 0 ? cell.fixed : cell.mobile; e != entt::null; e = ChildOf(e, cellXZ))
			{
				const auto* link = LinkOf(e);
				if (link == nullptr || ++steps > limit)
				{
					++errors; // an unlinked entity in a list, or a cycle
					break;
				}
				const bool inFixed = link->fixedList || IsFixedClass(link->kind);
				if (inFixed != (list == 0) || (list == 1 && link->prev != previous))
				{
					++errors;
				}
				++seen[e];
				previous = e;
			}
		}
	}
	for (const auto& [entity, link] : g_Links)
	{
		const size_t expected = link.kind == InsertKind::MultiMapFixed ? link.children.size() : 1;
		const auto found = seen.find(entity);
		if ((found == seen.end() ? 0 : found->second) != expected)
		{
			++errors;
		}
	}
	return errors;
}

// ---- One cell -------------------------------------------------------------------------------------------------------

entt::entity map_cells::FirstMobile(glm::ivec2 cell)
{
	const auto* c = CellIfAny(cell);
	return c != nullptr ? c->mobile : entt::null;
}

entt::entity map_cells::FirstFixed(glm::ivec2 cell)
{
	const auto* c = CellIfAny(cell);
	return c != nullptr ? c->fixed : entt::null;
}

entt::entity map_cells::GetMapChild(entt::entity object, glm::ivec2 cell)
{
	return ChildOf(object, cell);
}

bool map_cells::IsReadable(entt::entity object)
{
	return !FilterRef().Out(object);
}

map_cells::ReadBatch::ReadBatch()
{
	if (g_BatchDepth++ == 0)
	{
		g_BatchFilter.emplace();
	}
}

map_cells::ReadBatch::~ReadBatch()
{
	if (--g_BatchDepth == 0)
	{
		g_BatchFilter.reset();
	}
}

void map_cells::ForEachInCell(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	// GetFirstIterator 0x6034D0: {cell+4, IsFixed 1}, or {cell+0, 0} when the fixed list is empty
	auto object = cell->fixed;
	bool fixed = true;
	if (object == entt::null)
	{
		object = cell->mobile;
		fixed = false;
	}
	while (object != entt::null)
	{
		// the next first (fn_00603500), MoveToMobileObsIfNeededAndPoss fn_006827E0 at the end of the fixed list
		auto next = ChildOf(object, cellXZ);
		if (next == entt::null && fixed)
		{
			next = cell->mobile;
			fixed = false;
		}
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

std::vector<entt::entity> map_cells::ObjectsInCell(glm::ivec2 cell)
{
	std::vector<entt::entity> objects;
	ForEachInCell(cell, [&objects](entt::entity object) {
		objects.push_back(object);
		return true;
	});
	return objects;
}

void map_cells::ForEachMobile(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	for (auto object = cell->mobile; object != entt::null;)
	{
		const auto next = ChildOf(object, cellXZ);
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

std::vector<entt::entity> map_cells::MobileInCell(glm::ivec2 cell)
{
	std::vector<entt::entity> objects;
	ForEachMobile(cell, [&objects](entt::entity object) {
		objects.push_back(object);
		return true;
	});
	return objects;
}

entt::entity map_cells::FindType(glm::ivec2 cellXZ, ObjectType type, entt::entity after)
{
	const auto* cell = CellIfAny(cellXZ); // MapCoords::FindType 0x6045C0: ToMap, 0 off the map
	if (cell == nullptr)
	{
		return entt::null;
	}
	const FilterRef filter;
	if (type == ObjectType::Any)
	{
		// 0x6015E0: after's child; at the end of the fixed list (after's type counts as fixed) the mobile head
		const auto next = [cell, cellXZ](entt::entity object) {
			const auto child = ChildOf(object, cellXZ);
			if (child == entt::null && CountsAsFixed(LinkType(object)))
			{
				return cell->mobile;
			}
			return child;
		};
		auto object = after == entt::null ? (cell->fixed != entt::null ? cell->fixed : cell->mobile) : next(after);
		while (object != entt::null && filter.Out(object))
		{
			object = next(object);
		}
		return object;
	}
	// 0x601646: only the list of the type, from after's child or the head; the first with info +0x10 == type
	auto object = after != entt::null ? ChildOf(after, cellXZ) : (CountsAsFixed(type) ? cell->fixed : cell->mobile);
	while (object != entt::null && (LinkType(object) != type || filter.Out(object)))
	{
		object = ChildOf(object, cellXZ);
	}
	return object;
}

entt::entity map_cells::FindFixedOnMap(glm::ivec2 cellXZ, entt::entity after)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return entt::null;
	}
	const FilterRef filter;
	auto object = after != entt::null ? ChildOf(after, cellXZ) : cell->fixed;
	while (object != entt::null)
	{
		const auto* link = LinkOf(object);
		// +0x24 & 2: a MultiMapFixed (FishFarm : MultiMapFixed)
		if (link != nullptr &&
		    (link->kind == InsertKind::MultiMapFixed || link->kind == InsertKind::FishFarm) && !filter.Out(object))
		{
			return object;
		}
		object = ChildOf(object, cellXZ);
	}
	return entt::null;
}

bool map_cells::IsFixed(glm::ivec2 cellXZ)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return false;
	}
	// 0x601EA0: cell+4 && (cell+4)->+0x24 & 2. The head the original has (an object out of the map is not in it)
	const FilterRef filter;
	auto head = cell->fixed;
	while (head != entt::null && filter.Out(head))
	{
		head = ChildOf(head, cellXZ);
	}
	const auto* link = head != entt::null ? LinkOf(head) : nullptr;
	return link != nullptr && (link->kind == InsertKind::MultiMapFixed || link->kind == InsertKind::FishFarm);
}

void map_cells::ForEachFixed(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	for (auto object = cell->fixed; object != entt::null;)
	{
		const auto next = ChildOf(object, cellXZ); // GetMapChild vt +0x53C
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

const map_collide::Shape* map_cells::CollideDataOf(entt::entity object)
{
	const auto* link = LinkOf(object);
	return link != nullptr && link->collide ? &*link->collide : nullptr;
}

namespace
{
/// MapCell::Collide 0x601BD0 on a cell that ToMap gave
uint32_t CellCollide(glm::ivec2 cellXZ)
{
	// 0x601BEE..0x601C62: the landscape cell's water bit (1 when no block), fn_00601E00 0x10 off the game map, then
	// "sete dl; inc edx" 0x601C7D: 1 water, 2 land (ecs::sea_cells::CollideLandscape). No island (the unit tests): no
	// block, so water (0x601C5D)
	uint32_t bits = Locator::terrainSystem::has_value()
	                    ? sea_cells::CollideLandscape(Locator::terrainSystem::value(), cellXZ)
	                    : static_cast<uint32_t>(sea_cells::k_CollideWater);
	if (bits == sea_cells::k_CollideEdge)
	{
		return bits; // 0x601C6E: no object bits
	}
	// 0x601C78..0x601CAE: the fixed list from the head ([edi + 4], next vt +0x53C at 0x601CA6), the type of each
	// ([obj + 0x28] + 0x10): 6 FOREST_TREE |= 0x20 (0x601C9E), 0x12 FIELD |= 4 (0x601C99); the mobile list is not
	// read. openblack's filter stands in for the objects the original has already taken out of the list
	map_cells::ForEachFixed(cellXZ, [&bits](entt::entity object) {
		const auto type = LinkType(object);
		if (type == ObjectType::ForestTree)
		{
			bits |= sea_cells::k_CollideTree;
		}
		else if (type == ObjectType::Field)
		{
			bits |= sea_cells::k_CollideField;
		}
		return true;
	});
	return bits;
}
} // namespace

uint32_t map_cells::Collide(const map_coords::MapCoords& coords)
{
	// MapCoords::Collide 0x6033C0: ToMap NULL -> "or eax, 0xFFFFFFFF" (0x6033CC)
	if (!map_coords::InBounds(coords))
	{
		return 0xFFFFFFFFu;
	}
	// MapCell::Collide(MapCoords) 0x601CE0: 0x601BD0, then CollideWithFixe only when its result has bit 8
	// ("test bl, 8" 0x601CEB). 0x601BD0 only ever gives 0x10, or 1 / 2 with 4 and 0x20: that branch is dead
	return CellCollide(map_coords::Cell(coords));
}

uint32_t map_cells::CollideWithFixed(const map_coords::MapCoords& coords)
{
	// MapCoords::CollideCollideWithFixe 0x604FE0: ToMap NULL -> 0xFFFFFFFF (0x604FEC)
	if (!map_coords::InBounds(coords))
	{
		return 0xFFFFFFFFu;
	}
	// MapCell::CollideWithFixe 0x601D10: 0x601BD0 (0x601D18), then a 0.5 circle at (x, z) = fild x fmul [0x8AA3A4]
	// (10 / 65536, 0x601D23..0x601D3C; the altitude is the point's y, the collision is in x, z) against the
	// GetCollideData (vt +0x858, 0x601D61) of every object of the fixed list (0x601D56..0x601D8A, none skipped but
	// those without data); the first hit (NewCollide::Obj::Collide 0x829140) gives | 8 (0x601DAD)
	const auto cellXZ = map_coords::Cell(coords);
	uint32_t bits = CellCollide(cellXZ);
	const glm::vec2 point = map_coords::ToMetres(coords);
	bool hit = false;
	ForEachFixed(cellXZ, [&](entt::entity object) {
		const auto* shape = CollideDataOf(object);
		hit = shape != nullptr && map_collide::Collide(point, k_FixedTestRadius, *shape);
		return !hit;
	});
	if (hit)
	{
		bits |= sea_cells::k_CollideFixed;
	}
	return bits;
}

// ---- Searches -------------------------------------------------------------------------------------------------------

entt::entity map_cells::FindNearType(const map_coords::MapCoords& coords, ObjectType type, float radius)
{
	// the corners (0x6045F8..0x6046E6), x outer and z inner as signed words (movsx, `jg`), InBounds per cell
	const int32_t lowX = Corner(coords.x, -radius);
	const int32_t lowZ = Corner(coords.z, -radius);
	const int32_t highX = Corner(coords.x, radius);
	const int32_t highZ = Corner(coords.z, radius);
	const FilterRef filter;
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max(); // [0x93004C]
	const bool fixedList = CountsAsFixed(type);              // 0x60475C: ANY is the mobile list
	for (int32_t x = lowX; x <= highX; ++x)
	{
		for (int32_t z = lowZ; z <= highZ; ++z)
		{
			const glm::ivec2 cellXZ(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			const auto* cell = CellIfAny(cellXZ);
			if (cell == nullptr)
			{
				continue;
			}
			for (auto object = fixedList ? cell->fixed : cell->mobile; object != entt::null; object = ChildOf(object, cellXZ))
			{
				if ((type != ObjectType::Any && LinkType(object) != type) || filter.Out(object))
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object)); // fn_00605CD0
				if (distance < bestDistance) // fcom; test ah, 1
				{
					bestDistance = distance;
					best = object;
				}
			}
		}
	}
	return best;
}

entt::entity map_cells::FindNearForScript(const map_coords::MapCoords& coords, const std::function<bool(entt::entity)>& pred,
                                          float radius)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max(); // [0x93004C]
	ScriptSquare(coords, radius, [&](glm::ivec2 cell) {
		ForEachInCell(cell, [&](entt::entity object) {
			if (!pred(object))
			{
				return true;
			}
			// fn_00605CD0 from this to the totem (vt +0x304 IsWorshipSite) or the object's MapCoords
			const float distance = gutils::GetDistanceInMetres(coords, DistancePointOf(object));
			if (distance < bestDistance) // fcom; test ah, 1
			{
				bestDistance = distance;
				best = object;
			}
			return true;
		});
	});
	return best;
}

entt::entity map_cells::FindNearestInSpiral(const map_coords::MapCoords& coords,
                                            const std::function<bool(entt::entity)>& pred, float radius,
                                            entt::entity excluded)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	// fld r; fadd st0, st0; fdiv 10; _ceil (double); __ftol; "cmp eax, 3; jg" (0x604C33..0x604C77). (aproximado) 2r / 10
	// in float here, at x87 extended precision there
	int32_t side = map_coords::FtoL(static_cast<float>(std::ceil(static_cast<double>((radius + radius) / 10.0f))));
	if (side <= 3)
	{
		side = 3;
	}
	int32_t cells = side * side;
	auto walk = coords;
	map_coords::Spiral spiral; // dir = count = 1 (0x604C8A / 0x604C8E)
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	while (cells != 0)
	{
		// 0x604C98..0x604CBF: with a best, stop when best x 1.5 + 10 < the distance to this cell's walk point
		if (best != entt::null &&
		    bestDistance * k_SpiralStopFactor + k_SpiralStopAdd < gutils::GetDistanceInMetres(coords, walk))
		{
			break;
		}
		if (map_coords::InBounds(walk))
		{
			const auto cell = map_coords::Cell(walk);
			for (auto object = FindType(cell, ObjectType::Any); object != entt::null;
			     object = FindType(cell, ObjectType::Any, object))
			{
				if (!pred(object) || object == excluded)
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object));
				// d < r (0x604D08), then d < best or no best yet (0x604D13..0x604D1A)
				if (distance < radius && (distance < bestDistance || best == entt::null))
				{
					bestDistance = distance;
					best = object;
				}
			}
		}
		--cells;
		map_coords::AddCells(walk, spiral.Next()); // Spiral 0x74D7E0 + operator+= 0x605470
	}
	return best;
}

entt::entity map_cells::FindNearInfluenced(const map_coords::MapCoords& coords, PlayerNames player,
                                           const std::function<float(entt::entity)>& score, float radius)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	entt::entity best = entt::null;
	float bestScore = 0.0f; // the first argument's slot, zeroed at 0x6048D0
	ScriptSquare(coords, radius, [&](glm::ivec2 cell) {
		// CalculatePlayerInfluence 0x5CD170 (the cell's MapCoords, player, 0, 0, 1) > 0 (fcomp 0; test ah, 0x41).
		// (inferido) fn_00601F40's low words ([0x59E0] >> 1) x [0x59E4] / [0x59E8] are taken as the cell's middle
		const glm::vec3 middle(static_cast<float>(cell.x) * map_coords::k_CellSize + map_coords::k_CellSize * 0.5f, 0.0f,
		                       static_cast<float>(cell.y) * map_coords::k_CellSize + map_coords::k_CellSize * 0.5f);
		if (!(influence::CalculatePlayerInfluence(player, middle) > 0.0f))
		{
			return;
		}
		ForEachInCell(cell, [&](entt::entity object) {
			const float s = score(object);
			if (s < bestScore) // fcomp best; test ah, 1; jne
			{
				return true;
			}
			const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object));
			const float value = gutils::GetDistanceModifier(distance, radius) * s; // 0x74F290 (d, r), fmul
			if (value > bestScore) // fcom; test ah, 0x41; jne
			{
				bestScore = value;
				best = object;
			}
			return true;
		});
	});
	return best;
}

float map_cells::TallestOverlapping(const map_coords::MapCoords& coords, entt::entity self,
                                    const map_coords::MapCoords& selfCoords, float selfRadius, bool skipLiving)
{
	if (!map_coords::InBounds(coords)) // 0x60231E
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the low words of self's MapCoords x 1/65536 x 10 (0x6022E0..0x60231A)
	const auto offset = [](const map_coords::MapCoords& c) {
		return glm::vec2(static_cast<float>(static_cast<uint32_t>(c.x) & 0xFFFFu) * (1.0f / 65536.0f) * 10.0f,
		                 static_cast<float>(static_cast<uint32_t>(c.z) & 0xFFFFu) * (1.0f / 65536.0f) * 10.0f);
	};
	const glm::vec2 selfOffset = offset(selfCoords);
	float best = 0.0f;
	ForEachInCell(map_coords::Cell(coords), [&](entt::entity object) {
		if (object == self)
		{
			return true;
		}
		// IsLiving vt +0x3C4; IsMoving vt +0x174 (Object 0x402710). (aproximado) openblack keeps no last position for a
		// fixed object: one with a physics body is the moving one
		if (skipLiving && (registry.AnyOf<Villager, Animal, Creature>(object) ||
		                   physics::PhysicsObjects::Find(object) != nullptr))
		{
			return true;
		}
		// GetTopPos vt +0x630 (0x602388); fcomp best; test ah, 0x41; jne: only above the best
		const float top = object::GetTopPos(object);
		if (!(top > best))
		{
			return true;
		}
		const glm::vec2 d = selfOffset - offset(object::MapCoordsOf(object));
		const float radius = object::Get2DRadius(object); // vt +0x64 twice (0x6023E9, 0x6023F4)
		// (r_obj r_obj + r_self r_self) against (dz dz + dx dx): fcompp; test ah, 1 (0x602417)
		if (d.y * d.y + d.x * d.x < radius * radius + selfRadius * selfRadius)
		{
			best = top;
		}
		return true;
	});
	return best;
}

// ---- Towns ----------------------------------------------------------------------------------------------------------

std::vector<entt::entity> map_cells::TownsOf(PlayerNames player)
{
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	if (auto* registry = RegistryOrNull())
	{
		registry->Each<const Town>([&](entt::entity entity, const Town& town) {
			if (town.owner == player)
			{
				towns.emplace_back(town.id, entity);
			}
		});
	}
	// fn_0064C090 adds at the tail: the oldest first (inferido: by Town::id)
	std::sort(towns.begin(), towns.end());
	std::vector<entt::entity> result;
	result.reserve(towns.size());
	for (const auto& [id, entity] : towns)
	{
		result.push_back(entity);
	}
	return result;
}

void map_cells::ForEachTown(const std::function<bool(entt::entity)>& fn)
{
	// GetNextPlayerAndNeutral 0x550980: the slots 0..7 in order, the neutral one (7) last
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		for (const auto town : TownsOf(static_cast<PlayerNames>(p)))
		{
			if (!fn(town))
			{
				return;
			}
		}
	}
}

namespace
{
template <typename Accept>
entt::entity NearestTown(const map_coords::MapCoords& coords, float radius, Accept&& accept)
{
	entt::entity best = entt::null;
	float bestDistance = radius;
	map_cells::ForEachTown([&](entt::entity town) {
		if (!accept(town))
		{
			return true;
		}
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town)); // fn_00605CD0 / 0x74CD50
		if (distance < bestDistance) // fcom; test ah, 1
		{
			bestDistance = distance;
			best = town;
		}
		return true;
	});
	return best;
}
} // namespace

entt::entity map_cells::GetNearestTown(const map_coords::MapCoords& coords, float radius)
{
	return NearestTown(coords, radius, [](entt::entity) { return true; });
}

bool map_cells::TownHasCentre(entt::entity town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& data = registry.Get<const Town>(town);
	// fn_00741020 0x741024..0x741044: the town's abodes (+0x754, next +0x9C), one whose IsTownCentre (vt+0x1E0) is 1:
	// only TownCentre's (0x55DB70 `mov eax, 1`; GameThingWithPos 0x401AF0 gives 0). A TownCentre is the abode of an info
	// of type 0x404 (CREATE_TOWN_CENTRE / CREATE_ABODE -> AbodeArchetype), and in info.dat those are exactly the infos of
	// number ABODE_NUMBER_TOWN_CENTRE (12; test_map_cells TownCentreInfosAreNumber12), the only thing the Abode keeps
	bool found = false;
	registry.Each<const Abode>([&](const Abode& abode) {
		found = found || (abode.townId == data.id && abode.type == AbodeNumber::TownCentre);
	});
	if (found)
	{
		return true; // 0x74103A jne 0x741069: 1
	}
	// 0x741046..0x741062: the planned list (+0x9A8, next +0x44), one whose info (+0x40) GetAbodeNumber (vt+0x44) is 0xC:
	// GAbodeInfo::GetAbodeNumber 0x401260 reads info +0x124 (the other planned things' infos, GFeatureInfo 0x421E90,
	// give -1; openblack's plannedAbodes are CREATE_PLANNED_ABODE's only)
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	return std::any_of(data.plannedAbodes.begin(), data.plannedAbodes.end(), [&infos](const PlannedAbode& planned) {
		const auto i = static_cast<size_t>(planned.info);
		return i < infos.size() && infos[i].abodeNumber == AbodeNumber::TownCentre;
	});
}

entt::entity map_cells::GetNearestTownWithCentre(const map_coords::MapCoords& coords, float radius)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return NearestTown(coords, radius, [&registry](entt::entity town) {
		// 0x6021A7..0x6021BA: +0x9A4 != 0, or else fn_00741020 (0x6021B3) != 0
		return registry.Get<const Town>(town).centre != entt::null || TownHasCentre(town);
	});
}

map_cells::TownInCells map_cells::GetNearestTownCells(const map_coords::MapCoords& coords, entt::entity excluded,
                                                      std::optional<Tribe> tribe)
{
	const auto& registry = Locator::entitiesRegistry::value();
	TownInCells result;
	uint32_t best = k_TownCellsStart;
	ForEachTown([&](entt::entity town) {
		if (town == excluded)
		{
			return true;
		}
		if (const auto* t = registry.TryGet<const Tribe>(town); tribe && t != nullptr && *t == *tribe)
		{
			return true; // +0x5B8 == tribe
		}
		const auto at = object::MapCoordsOf(town);
		// the high words' differences, |dx| and |dz|, then max + (min >> 1) (0x602003 / 0x60200B)
		const int32_t dx = std::abs(static_cast<int32_t>(map_coords::SignedCellOf(at.x)) - map_coords::SignedCellOf(coords.x));
		const int32_t dz = std::abs(static_cast<int32_t>(map_coords::SignedCellOf(at.z)) - map_coords::SignedCellOf(coords.z));
		const auto distance = static_cast<uint32_t>(std::max(dx, dz) + (std::min(dx, dz) >> 1));
		if (distance < best)
		{
			best = distance;
			result.town = town;
			result.distance = distance;
		}
		return true;
	});
	if (result.town != entt::null)
	{
		result.code = 2; // (aproximado) no town rectangle (+0x72A..+0x73A) in openblack: never "within 4 cells"
	}
	return result;
}

entt::entity map_cells::GetNearestCitadel(const map_coords::MapCoords& coords, float radius)
{
	entt::entity best = entt::null;
	float bestDistance = radius;
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto citadel = worship::citadel::Of(static_cast<PlayerNames>(p)); // GPlayer +0xA48
		if (citadel == entt::null)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(citadel));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = citadel;
		}
	}
	return best;
}

entt::entity map_cells::GetNearestTownToPos(const map_coords::MapCoords& coords, std::optional<Tribe> tribe,
                                            int32_t abodeType, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	return NearestTown(coords, radius, [&](entt::entity town) {
		if (tribe)
		{
			const auto* t = registry.TryGet<const Tribe>(town); // GetTribe() +0x10
			if (t == nullptr || *t != *tribe)
			{
				return false;
			}
		}
		if (abodeType == k_AnyAbodeType)
		{
			return true;
		}
		// IsAbodeTypeInTown 0x73D6B0: the town's abodes (+0x754, next +0x9C) with info (+0x28) +0x120 == abode; a town
		// that has one is skipped. The fields are in that list: Field::Field 0x527DD0 calls Abode::Abode (0x527DFA) with
		// its town, which calls Town::AddStructureToTown 0x7399A0 (0x4013BB), whose dynamic_cast to Abode (0x7399B7)
		// links it at +0x754 (0x7399C3..0x7399CF); their info +0x28 is the GAbodeInfo given to Abode::Abode
		const auto id = registry.Get<const Town>(town).id;
		bool found = false;
		registry.Each<const Abode>([&](entt::entity abode, const Abode& data) {
			if (found || data.townId != id)
			{
				return;
			}
			const auto* info = fire::traits::AbodeInfo(abode);
			found = info != nullptr && static_cast<int32_t>(info->abodeType) == abodeType;
		});
		return !found;
	});
}

entt::entity map_cells::FindPlayerTownAtPos(const map_coords::MapCoords& coords, float radius, PlayerNames player)
{
	// GScript::FindPlayerTownAtPos 0x6F72E0: only that player's list (GPlayer +0xA50, next +0x75C), best = r;
	// GUtils::GetDistanceInMetres 0x74CD70 to the town's MapCoords (+0x14); "fcom; test ah, 0x41; je" (0x6F7312) keeps
	// it unless it is farther: <=, so a tie goes to the later town (and a NaN distance is taken)
	entt::entity best = entt::null;
	float bestDistance = radius;
	for (const auto town : TownsOf(player))
	{
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town));
		if (!(distance > bestDistance))
		{
			bestDistance = distance;
			best = town;
		}
	}
	return best;
}

entt::entity map_cells::FindNearestTownInList(const map_coords::MapCoords& coords)
{
	auto* registry = RegistryOrNull();
	if (registry == nullptr)
	{
		return entt::null;
	}
	// g_game+0x205C84: every town (inferido: in the order they were made, Town::id)
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry->Each<const Town>([&](entt::entity entity, const Town& town) { towns.emplace_back(town.id, entity); });
	std::sort(towns.begin(), towns.end());
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	for (const auto& [id, town] : towns)
	{
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town));
		if (best == entt::null || distance < bestDistance) // the first always (0x55300E), then fcomp; test ah, 1
		{
			bestDistance = distance;
			best = town;
		}
	}
	return best;
}

void map_cells::detail::SetShapeProviderForTests(ShapeProvider provider)
{
	g_ShapeProvider = provider;
}
