/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellForest.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Map.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Weather.h"
#include "InfoConstants.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MagicTree.h"
#include "ForestDebugHooks.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
/// The trees this SpellEvent made so far: they are the newest objects of their cells (Tree::CallVirtualFunctionsForCreation
/// puts each in its cell at once), while openblack's map grid is only rebuilt at the next turn
std::vector<entt::entity> g_NewTrees;

const GMagicForestInfo& ForestInfoOf(entt::entity spell)
{
	// SpellForest::GetMagicInfo 0x725A20
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicForestInfo>(Locator::infoConstants::value(), type);
}

SpellForestData& DataFor(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellForestData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<SpellForestData>(spell);
}

/// The forest the spell points at (+0xEC), when it still exists (ECS/Trees drops an empty forest after 2000 turns)
bool HasForest(const SpellForestData& data)
{
	return data.forestId != 0 && !data.forestDeleted && ecs::IsInForest(data.forestId);
}

/// Forest +0x4C + +0x54: its trees (ECS/Trees ForestTreeCount, fn_0053AD00)
uint32_t TreeCountOf(const SpellForestData& data)
{
	return HasForest(data) ? static_cast<uint32_t>(ecs::ForestTreeCount(data.forestId)) : 0;
}

/// MagicTree::ToBeDeleted 0x5FD070's last part: a magic tree of the forest went (ECS/Trees' DeleteTree told
/// MagicTree.cpp) and the forest has no tree left -> the forest is deleted (Forest::ToBeDeleted 0x539C60, DeleteForest)
bool ForestWentWithItsLastTree(const SpellForestData& data)
{
	if (data.forestId == 0 || !magic_tree::ForestLostAMagicTree(data.forestId) ||
	    ecs::ForestTreeCount(data.forestId) != 0)
	{
		return false;
	}
	ecs::DeleteForest(data.forestId);
	return true;
}

/// fn_00725790 on a spell
int TreesWanted(entt::entity spell)
{
	return spell_forest::TreesWanted(GetSpellStrength(spell), DataFor(spell).maxTrees, ForestInfoOf(spell).finalNoTrees);
}

glm::u16vec2 CellOf(const glm::vec3& position)
{
	return ecs::MapInterface::GetGridCell(position);
}

/// Is the object one of the MultiMapFixed classes? Its ctor 0x52E1F0 is the only place that sets the flag +0x24 bit 1
/// (0x52E207 `or byte [esi+0x24], 2`), which is what MapCell::IsFixed reads. The list of classes that derive from it
/// comes from bw1-decomp (src/Black/*.h): Abode (so Field, Footpath, CreaturePen, BuildingSite and StoragePit too),
/// BigForest, CitadelPart, Feature, FishFarm, MobileStatic (so MagicTeleport and the street lanterns), PFootball,
/// PrayerSite, SpellIcon and TotemStatue. SingleMapFixed (Tree, MapShield, ScriptHighlight, PrayerIcon) does not set it.
bool IsMultiMapFixed(const ecs::Registry& registry, entt::entity object)
{
	return registry.AnyOf<Abode, Field, Footpath, BigForest, Feature, FishFarm, MobileStatic, TotemStatue, SpellIcon,
	                      MagicTeleport>(object);
}

/// MapCoords::IsFixed 0x603790 -> MapCell::IsFixed 0x601EA0: the cell's first fixed object (MapCell +4, where
/// Fixed::InsertMapObjectToCell 0x52DEA0 puts the newest) has the flag +0x24 bit 1, so IsFixed is "the newest fixed
/// object of the cell is a MultiMapFixed". A tree (SingleMapFixed) is in its own cell only.
/// (aproximado) The newest is taken by the creation index: openblack's map grid is an unordered_set rebuilt from
/// scratch (ECS/Map.h), so it has no insertion order. The two differ only for an object that was taken out of the map
/// and put back without being created again (picked up and dropped): the original makes it the newest again, here it
/// keeps its old index.
bool IsFixedCell(const glm::vec3& position)
{
	const auto cell = CellOf(position);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto tree : g_NewTrees)
	{
		if (registry.Valid(tree) && CellOf(registry.Get<const Transform>(tree).position) == cell)
		{
			return false; // this event's tree is the cell's newest object
		}
	}
	if (!Locator::entitiesMap::has_value())
	{
		return false;
	}
	entt::entity newest = entt::null;
	int64_t newestIndex = -1;
	for (const auto object : Locator::entitiesMap::value().GetFixedInGridCell(cell))
	{
		if (!registry.Valid(object))
		{
			continue;
		}
		// openblack's grid puts a fixed object in every cell its circle touches; a tree only counts in its own
		if (registry.AllOf<Tree>(object) && CellOf(registry.Get<const Transform>(object).position) != cell)
		{
			continue;
		}
		const auto index = ecs::object_index::Of(object);
		if (index >= newestIndex)
		{
			newestIndex = index;
			newest = object;
		}
	}
	return newest != entt::null && IsMultiMapFixed(registry, newest);
}

/// fn_005FADF0: no Abode (FindType ABODE in the position's cell) has Get2DRadius > its distance to the point
bool NoAbodeCovers(const glm::vec3& position)
{
	if (!Locator::entitiesMap::has_value())
	{
		return true;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : Locator::entitiesMap::value().GetFixedInGridCell(CellOf(position)))
	{
		// OBJECT_TYPE 0: the abodes, the fields among them (Field : Abode)
		if (!registry.Valid(object) || !registry.AnyOf<Abode, Field>(object))
		{
			continue;
		}
		const auto centre = ecs::fire::traits::FireCentre(object); // vt 0x5F0
		const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(centre.x, centre.z));
		// vt 0x64: Field::Get2DRadius 0x528E80 is the constant 5 m ([0x8AB6E4]); every other class here keeps
		// Object::Get2DRadius 0x638180 (only Field overrides the slot: checked on the Object, Abode, Field, Tree and
		// Pot vtables)
		const float radius = registry.AllOf<Field>(object) ? 5.0f : ecs::effects::Object2DRadius(object);
		// fcomp; test ah, 0x41; je -> radius > distance: no
		if (radius > distance)
		{
			return false;
		}
	}
	return true;
}

/// Terrain::GetTerrainMaterial 0x735380 (cellX, cellZ): 27 under deep snow (GClimate::GetSnow >= 27 at the cell's
/// point), else the land's material (fn_00804CF0: the cell's country, the second material of its altitude, that
/// material's type; 0 off the map or without a block) with 0 -> 1
uint32_t TerrainMaterial(glm::u16vec2 cell)
{
	const glm::vec3 corner(static_cast<float>(cell.x) * 10.0f, 0.0f, static_cast<float>(cell.y) * 10.0f);
	if (weather::GetSnowAt(ToWorld(corner)) >= 27.0f)
	{
		return 27;
	}
	uint32_t material = 0;
	if (Locator::terrainSystem::has_value() && cell.x <= 0x1FF && cell.y <= 0x1FF)
	{
		const auto& island = Locator::terrainSystem::value();
		if (cell.x < island.GetCellsPerSide() && cell.y < island.GetCellsPerSide())
		{
			const auto& landCell = island.GetCell(cell);
			lnd::LNDCell empty {};
			empty.properties.fullWater = true;
			const auto& countries = island.GetCountries();
			const auto& materials = island.GetMaterialInfo();
			if (std::memcmp(&landCell, &empty, sizeof(empty)) != 0 && landCell.properties.country < countries.size())
			{
				const auto& country = countries[landCell.properties.country];
				const auto altitude = std::min<uint16_t>(island.GetCellAltitude(landCell), 255);
				const auto index = country.materials[altitude].indices[1];
				if (index < materials.size())
				{
					material = materials[index].type;
				}
			}
		}
	}
	return material != 0 ? material : 1;
}

/// fn_00725600 (pos, treeInfo): the Forest on the first tree, then a MagicTree at a random angle with its target scale
entt::entity CreateTree(entt::entity entity, const glm::vec3& position, TreeInfo type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = DataFor(entity);
	if (data.forestId == 0)
	{
		// new Forest (0x58) -> fn_005399E0(pos, creator): a new forest for every cast (ECS/Trees). The creator's
		// player (0x5399F4 GetPlayer -> the GameThing ctor fn_0046B8A0) is not kept here and is not needed: nothing in
		// Forest::Process 0x539DA0 reads it. The statistic the forest's new trees raise, GPlayer::FUN_0064da80(14, 1)
		// at 0x539FB7, is for the player CalculateMostInfluentialPlayer 0x603830 gives at the new tree (0x539F9B), not
		// for the forest's own player, and FUN_0064da80 returns at once outside a multiplayer game (0x64DA90
		// IsMultiplayerGame). (inferido) no other reader of a Forest's player was found.
		data.forestId = ecs::CreateForest(0, glm::vec3(position.x, 0.0f, position.z));
		data.forestCreated = true;
	}
	const float angle = Locator::rng::value().NextValue(0.0f, glm::two_pi<float>()); // GameFloatRand(2 pi)
	const float woodMultiplier = ForestInfoOf(entity).woodValueMultiplier * GetTribalPower(entity);
	// fn_00725600 -> fn_005FD000(pos, spell, info, forest, angle, scale 0, woodMul) (resources.md)
	const auto tree = magic_tree::Create(position, entity, type, data.forestId, angle, 0.0f, woodMultiplier);
	if (tree != entt::null)
	{
		const auto& castPos = registry.Get<const Spell>(entity).originalCastPos;
		const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(castPos.x, castPos.z));
		registry.Get<Tree>(tree).maxSize = spell_forest::TargetScale(distance); // +0x64
	}
	return tree;
}

int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// SpellForest::InitWithPos 0x725540 = Spell::InitWithPos; AllocSpell 0x5FAD90 cleared +0xEC and +0xF0
	DataFor(spell) = {};
	const int result = base::InitWithPos(spell, position, castData, info);
	// Spell::InitWithPos's SetMaxObjectsToCreate (vt 0x54C, 0x7256C0): -1 -> finalNoTrees
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(spell) && registry.AllOf<Spell>(spell))
	{
		const int requested = registry.Get<const Spell>(spell).maxObjectsToCreate;
		DataFor(spell).maxTrees = requested == -1 ? static_cast<int>(ForestInfoOf(spell).finalNoTrees) : requested;
	}
	return result;
}

/// SpellForest::SpellEvent 0x725830: the seed has landed (type 3): the whole forest at once
int SpellEvent(entt::entity entity, const psys::SpellEventInfo& event)
{
	if (event.type == psys::SpellEventInfo::Started || DataFor(entity).forestId != 0)
	{
		return 1;
	}
	// pays costPerEvent (1); the NATURE EffectValues (alignment 1) around the spell, the reaction
	if (spell_event::ApplyDefaultSpellEffect(entity, event) != 1)
	{
		return 1;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const int n = TreesWanted(entity);
	const auto castPos = registry.Get<const Spell>(entity).originalCastPos; // +0xC0
	g_NewTrees.clear();
	int made = 0;
	for (int i = 0; i < n; ++i)
	{
		const auto point = spell_forest::ToMapCoords(glm::vec2(castPos.x, castPos.z) + spell_forest::SpiralOffset(i, n));
		const glm::vec3 position(point.x, 0.0f, point.y);
		// fn_00725800: the forest has fewer trees than wanted
		if (!spell_forest::ValidPlaceForTree(position) || !(TreeCountOf(DataFor(entity)) < static_cast<uint32_t>(n)))
		{
			continue;
		}
		// GetRandomTreeInfo(castPos): the material under the cast point, a new GameRand(4) for each tree
		const auto tree = CreateTree(entity, position, spell_forest::RandomTreeType(castPos));
		if (tree != entt::null)
		{
			g_NewTrees.push_back(tree);
			++made;
		}
	}
	g_NewTrees.clear();
	forest_debug::OnLanded(entity, CurrentTurn()); // OPENBLACK_TEST_FOREST_SHOT
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: spell {} SpellForest event {} at ({:.1f}, {:.1f}): {} trees wanted, {} made around "
		                   "({:.1f}, {:.1f}), material {}",
		                   static_cast<uint32_t>(entity), event.type, event.position.x, event.position.z, n, made, castPos.x,
		                   castPos.z, TerrainMaterial(CellOf(castPos)));
	}
	return 1;
}

/// SpellForest::Process 0x7259C0: Spell::CoreProcess, then ProcessTrees 0x725A30
int Process(entt::entity entity)
{
	base::CoreProcess(entity);
	forest_debug::OnTurn(entity, CurrentTurn()); // OPENBLACK_TEST_FOREST_SHOT
	auto& data = DataFor(entity);
	auto& registry = Locator::entitiesRegistry::value();
	// the forest was deleted (+0xA bit 0): +0xEC = 0 and CloseDown (vt 0x530). It is deleted with its last magic tree
	// (MagicTree::ToBeDeleted 0x5FD070, ForestWentWithItsLastTree) or, empty, by ECS/Trees' forest turn.
	if (data.forestId != 0 && (data.forestDeleted || ForestWentWithItsLastTree(data) || !ecs::IsInForest(data.forestId)))
	{
		data.forestId = 0;
		data.forestDeleted = false;
		magic::CloseDown(entity);
	}
	if (data.forestId == 0)
	{
		return registry.Get<const Spell>(entity).psys != 0 ? 1 : 5;
	}
	const auto& info = ForestInfoOf(entity);
	const int wanted = TreesWanted(entity);
	const uint32_t count = TreeCountOf(data);
	float change = 0.0f;
	// cmp wanted, count; jae (unsigned)
	const bool decay = static_cast<uint32_t>(wanted) < count;
	if (decay)
	{
		// fn_00725B00 -> fn_0053A490 (ECS/Trees): a tree reaching 0 is ToBeDeleted (DeleteTree)
		change = ecs::ShrinkAllTrees(data.forestId, info.decaySpeed);
		// the last tree took the forest with it: CloseDown on the next turn, as the original
		data.forestDeleted = ForestWentWithItsLastTree(data);
	}
	else
	{
		change = ecs::GrowAllTrees(data.forestId, info.growSpeed); // fn_00725AD0 -> fn_0053A520
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} SpellForest trees {} wanted {}: {} {:.3f}",
		                   static_cast<uint32_t>(entity), count, wanted, decay ? "decay" : "grow", change);
	}
	return 1;
}

float CalculateCostToMaintain(entt::entity entity)
{
	const auto& effect = EffectInfoOf(entity);
	return spell_forest::CostToMaintain(base::CalculateCostToMaintain(entity), effect.costPerEvent, TreeCountOf(DataFor(entity)));
}

/// (not named CloseDown: inside magic::RegisterForestSpell that name is magic::CloseDown, the vt 0x530 dispatch)
void ForestCloseDown(entt::entity entity)
{
	// SpellForest::CloseDown 0x55D1E0: CoreCloseDown, creator = NULL
	base::CloseDown(entity);
	Locator::entitiesRegistry::value().Get<Spell>(entity).creator = {};
}

void ToBeDeleted(entt::entity entity)
{
	// SpellForest::ToBeDeleted 0x725500: the forest (if not already going) goes with its trees (Forest::ToBeDeleted
	// 0x539C60: ECS/Trees' DeleteForest, each tree's Tree::ToBeDeleted), then Spell::ToBeDeleted
	auto& data = DataFor(entity);
	if (HasForest(data))
	{
		ecs::DeleteForest(data.forestId);
	}
	data.forestId = 0;
}

bool HasEnoughChantsAndLifeForRecast(entt::entity spell)
{
	return spell_forest::GetMaxObjectsToCreate(spell) > 0; // 0x725730: vt 0x550 > 0
}
} // namespace

int spell_forest::TreesWanted(float strength, int maxTrees, uint32_t finalNoTrees)
{
	const float alive = strength > 0.0f ? 1.0f : 0.0f;
	const int trees = maxTrees == -1 ? static_cast<int>(finalNoTrees) : maxTrees;
	return static_cast<int>(std::nearbyint(static_cast<float>(trees) * alive));
}

glm::vec2 spell_forest::SpiralOffset(int i, int n)
{
	const float step = n > 1 ? 1.0f / (static_cast<float>(n) - 1.0f) : 1.0f;
	// fild n; fmul 0x9819FC; fmul 0x8AB210 (the float 2 pi); fstp: a float
	const float turns = static_cast<float>(static_cast<double>(n) * k_TurnsPerTree * static_cast<double>(glm::two_pi<float>()));
	const float f = static_cast<float>(i) * step;                          // fstp: a float
	const double g = 1.0 - static_cast<double>(f);
	const double r = k_InnerRadius + (k_ForestRadius - k_InnerRadius) * std::sqrt(1.0 - g * g);
	const double angle = static_cast<double>(f) * static_cast<double>(turns);
	return {static_cast<float>(r * std::cos(angle)), static_cast<float>(r * std::sin(angle))};
}

glm::vec2 spell_forest::ToMapCoords(glm::vec2 point)
{
	// fmul by the float 6553.6 in the FPU, __ftol truncates; GetLHPoint: fild x the float 10 / 65536 (0x8AA3A4)
	const auto x = static_cast<int32_t>(static_cast<double>(point.x) * static_cast<double>(6553.6f));
	const auto z = static_cast<int32_t>(static_cast<double>(point.y) * static_cast<double>(6553.6f));
	return {static_cast<float>(x * (10.0 / 65536.0)), static_cast<float>(z * (10.0 / 65536.0))};
}

float spell_forest::TargetScale(float distance)
{
	return 1.0f - distance * 0.5f / k_ForestRadius;
}

int spell_forest::MaxObjectsToCreate(int maxTrees, uint32_t finalNoTrees, bool hasForest, uint32_t trees, bool created)
{
	int count = static_cast<int>(finalNoTrees);
	if (hasForest)
	{
		count = static_cast<int>(trees);
	}
	else if (created)
	{
		count = 0;
	}
	return count < maxTrees ? count : maxTrees; // jl
}

float spell_forest::CostToMaintain(float costPerGameTurn, float costPerEvent, uint32_t trees)
{
	return costPerGameTurn + static_cast<float>(trees) * costPerEvent; // fild qword
}

float spell_forest::AdjustSpellSeedAltitude(bool hasForest, float tallestTree, float altitude)
{
	if (!hasForest)
	{
		return -5.0f; // 0xC0A00000
	}
	return altitude > tallestTree ? altitude : tallestTree;
}

float spell_forest::AdjustSpellSeedPos(entt::entity spell, float altitude)
{
	// 0x725750: +0xEC (the Forest) == 0 -> -5; else fcomp against fn_0053A740 (the tallest tree, ECS/Trees)
	const auto& data = DataFor(spell);
	const bool hasForest = HasForest(data);
	return AdjustSpellSeedAltitude(hasForest, hasForest ? ecs::TallestTreeHeight(data.forestId) : 0.0f, altitude);
}

bool spell_forest::CanCastAt(const glm::vec3& position)
{
	return cast_rules::InBounds(position) && cast_rules::IsLand(position) && NoAbodeCovers(position) &&
	       ValidPlaceForTree(position);
}

bool spell_forest::ValidPlaceForTree(const glm::vec3& position)
{
	return cast_rules::InBounds(position) && cast_rules::IsLand(position) && !IsFixedCell(position);
}

TreeInfo spell_forest::RandomTreeType(const glm::vec3& position)
{
	const auto& materials = Locator::infoConstants::value().terrainMaterial;
	uint32_t material = TerrainMaterial(CellOf(position));
	// Terrain::GetMaterialInfo 0x735330: > 0x2B -> 0
	if (material > 0x2B || material >= materials.size())
	{
		material = 0;
	}
	const auto pick = Locator::rng::value().NextValue<uint32_t>(0, 3); // GameRand(4)
	return materials[material].magicTreeTypes[pick];
}

const SpellForestData* spell_forest::DataOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().TryGet<const SpellForestData>(spell);
}

int spell_forest::GetMaxObjectsToCreate(entt::entity spell)
{
	const auto& data = DataFor(spell);
	return MaxObjectsToCreate(data.maxTrees, ForestInfoOf(spell).finalNoTrees, HasForest(data), TreeCountOf(data),
	                          data.forestCreated);
}

void openblack::magic::RegisterForestSpell()
{
	SpellOps ops;
	ops.initWithPos = InitWithPos;
	ops.initWithObject = base::InitWithObject;
	ops.process = Process;
	ops.spellEvent = SpellEvent;
	ops.costToMaintain = CalculateCostToMaintain;
	ops.closeDown = ForestCloseDown;
	ops.toBeDeleted = ToBeDeleted;
	ops.hasEnoughChantsForRecast = HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;
	ops.maxObjectsToCreate = spell_forest::GetMaxObjectsToCreate;
	RegisterOps(SpellClass::Forest, ops);
}
