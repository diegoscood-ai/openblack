/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellWater.h"

#include <cmath>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "ECS/WaterRings.h"
#include "Game.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
water::SpellWaterData& MutableDataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<water::SpellWaterData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<water::SpellWaterData>(spell);
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// GameThing::GetRadius vt 0x60: Object 0x638110 jumps to Get2DRadius (vt 0x64): Field 0x528E80 = 5 m, Object 0x638180
/// = the mesh's half extent x the scale (ecs::object::GetRadius). No class of the water's targets overrides vt 0x60.
float ObjectRadius(entt::entity object)
{
	return ecs::object::GetRadius(object);
}

/// The ring of a drop (0x725243..0x7252B9): the first free of the 1024 slots at 0xEAB7C8; +0x0C flags |= 1, +0x10 age 0,
/// +0x18 growth, +0x20 angle, +0x24 1.0, +0x28 aspect 1.0, +0x2C rate 1.0, +0x30 cell 0x30, +0x34 colour. The colour
/// is one of the constants k_RippleColours (0x7251C5), not the landscape light table: it is written as is (no seaLight)
/// and kept for the ring's life (ecs::AddWaterRing). Nothing when the 1024 slots are full (0x725236).
bool AddDropRing(const glm::vec3& position, float growth, float angle, uint32_t argb)
{
	ecs::WaterRing ring;
	ring.position = position;
	ring.age = 0;
	ring.growth = growth;
	ring.angle = angle;
	ring.aspect = 1.0f;
	ring.rate = 1.0f;
	ring.cell = 0x30;
	ring.argb = argb;
	return ecs::AddWaterRing(ring);
}

/// OPENBLACK_TEST_WATER_SHOT="<turns>,<path>[;<turns>,<path>...]" (test hook, not in the original): a screenshot that
/// many game turns after a water spell's first drop (docs/bw1-notes/openblack-internals.md)
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

std::vector<Shot>& Shots()
{
	static std::vector<Shot> shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_WATER_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			const auto comma = item.find(',');
			if (comma != std::string::npos)
			{
				list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
			}
		}
		return list;
	}();
	return shots;
}

void ShotHook(entt::entity spell)
{
	static std::unordered_map<entt::entity, unsigned int> firstTurn;
	if (Shots().empty() || Game::Instance() == nullptr)
	{
		return;
	}
	const unsigned int turn = CurrentTurn();
	const auto first = firstTurn.try_emplace(spell, turn).first->second;
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= first + shot.turns)
		{
			shot.done = true;
			Game::Instance()->RequestScreenshot(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Water test: screenshot {} turns after the first drop (turn {}) -> {}",
			                   shot.turns, turn, shot.path);
			return; // one request per frame
		}
	}
}

/// SpellWater::Process 0x724ED0 (vt 0x528): one drop per game turn
int Process(entt::entity entity)
{
	// Spell::Process 0x720710 first (maintain, the PSys step); its result is returned on every path
	const int result = base::Process(entity);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return result;
	}
	auto& data = MutableDataOf(entity);
	// +0xF0 && !IsAvailable (vt 0x2C) -> +0xF0 = 0
	if (data.puttingOutFireReaction != 0 &&
	    !ecs::effects::reactions::IsAvailable(ecs::effects::reactions::Find(data.puttingOutFireReaction)))
	{
		data.puttingOutFireReaction = 0;
	}
	ShotHook(entity); // test hook (OPENBLACK_TEST_WATER_SHOT), also while the spell closes
	const auto& spell = registry.Get<const Spell>(entity);
	if (spell.closedDown)
	{
		return result;
	}
	// C = (castPos(+0xCC).x, GetAltitude(castPos) + castPos.y(+0xD4), castPos.z): only x and z are used below
	const glm::vec3 centre(spell.castPos.x, LandAt(spell.castPos.x, spell.castPos.z) + spell.castPos.y, spell.castPos.z);
	// r = GameFloatRand(R) x 0.7 + 0.3, a = GameFloatRand(2 pi) (0x40C90FDB)
	// (0x724F65, then 0x724F89)
	const float radius = water::RainRadius(spell.magicType);
	const float r = water::DropDistance(game_random::GameFloatRand(radius));
	const float a = game_random::GameFloatRand(glm::two_pi<float>());
	glm::vec3 drop(centre.x + r * std::cos(a), 0.0f, centre.z + r * std::sin(a));
	// P.y = GetAltitude(MapCoords(ftol(x x 65536 x 0.1), ftol(z x 65536 x 0.1), 0)) + 0.2 (0x8AB244)
	drop.y = LandAt(drop.x, drop.z) + 0.2f;
	// SpellEvent{2, P, movement 0, strength 1, 0, target 0} through vt 0x52C (Spell::SpellEvent 0x720F40 ->
	// ApplyDefaultSpellEffect): burn -4000 x strength x tribal power within 1 m cools fires, costPerEvent 10, reaction 21.
	// Its result is ignored: the drop reaches the objects and makes its ring even without chants.
	psys::SpellEventInfo event;
	event.type = psys::SpellEventInfo::Point;
	event.position = drop;
	event.velocity = glm::vec3(0.0f);
	event.strength = 1.0f;
	event.checkShields = false;
	event.target = entt::null;
	OpsOf(spell.spellClass).spellEvent(entity, event);
	if (!registry.Valid(entity))
	{
		return result;
	}
	// the 3 x 3 cells from P's (GUtils::Spiral, 9 steps): every object whose edge is within 2.5 x GetPower of P gets
	// ApplyWaterSpell (vt 0x67C). GetDistanceInMetres 0x74CD70 is 2D (hypotenuse 0x74F680 of the MapCoords x, z).
	// 0x7250CC..0x725179 keep no "done" set and no own-cell test: a multi-cell object (a field) in several of the 9 cells
	// gets ApplyWaterSpell once per cell, as in the original
	// the spiral walks the drop's MapCoords (0x7250A2..0x7250BB): GetFirstIterator / GetMapChild on it, Spiral 0x74D7E0
	// (0x725166) and operator+= 0x605470 (0x725173), which adds the step to the high words only (the fraction stays and
	// the 16-bit add wraps at the map's edge)
	const auto dropCoords = ecs::map_coords::FromMetres(glm::vec2(drop.x, drop.z));
	auto coords = dropCoords;
	ecs::map_coords::Spiral spiral; // GUtils::Spiral 0x74D7E0, direction 1 and count 1 (0x7250BF..0x7250C3)
	std::string watered;            // the trace's list
	for (int i = 0; i < 9; ++i)
	{
		const auto cell = ecs::map_coords::Cell(coords);
		// GetFirstIterator / GetMapChild (0x7250C3..0x725166): the fixed list, then the mobile one, from the heads
		for (const auto object : ecs::map_cells::ObjectsInCell(glm::ivec2(cell)))
		{
			if (!registry.Valid(object) || object == entity)
			{
				continue;
			}
			const auto* transform = registry.TryGet<const Transform>(object);
			if (transform == nullptr)
			{
				continue;
			}
			// GUtils::GetDistanceInMetres 0x74CD70: the table hypotenuse 0x74F680 on the 16.16 map coordinates
			// (ECS/GUtilsDistance)
			const float distance = gutils::GetDistanceInMetres(
			    dropCoords, ecs::map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z)));
			if (water::InReach(distance, ObjectRadius(object)))
			{
				water::ApplyWaterSpell(object, entity);
				if (TraceEnabled())
				{
					watered += fmt::format(" {}", static_cast<uint32_t>(object));
				}
			}
		}
		ecs::map_coords::AddCells(coords, spiral.Next());
	}
	// a ring when GetRippleEvery < age - lastRipple
	const auto& after = registry.Get<const Spell>(entity);
	if (water::RippleDue(after.age, data.lastRipple))
	{
		data.lastRipple = after.age;
		const auto colour = water::k_RippleColours[game_random::GameRand(5)]; // GameRand(5) 0x7251ED
		const float angle = game_random::GameFloatRand(glm::two_pi<float>()); // GameFloatRand(2 pi) 0x725205
		AddDropRing(drop, water::RippleGrowth(after.magicType), angle, colour);
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: spell {} SpellWater drop at ({:.2f}, {:.2f}, {:.2f}) r {:.2f} of R {:.0f}: watered [{} ], "
		                   "age {:.2f}, last ring {:.2f}, chants {:.1f}",
		                   static_cast<uint32_t>(entity), drop.x, drop.y, drop.z, r, radius, watered, after.age,
		                   data.lastRipple, after.chants);
	}
	return result;
}

int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// GMagicWaterInfo::AllocSpell 0x5FAC70 -> fn_00724EC0: +0xEC = 0, +0xF0 = 0
	MutableDataOf(spell) = water::SpellWaterData {};
	return base::InitWithPos(spell, position, castData, info);
}

/// Object::ApplyWaterSpell 0x63A8E0: a burning object makes the spell start REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34),
/// CreateReaction(spell, 0x22, spell->GetPlayer(), 1), once (while +0xF0 holds it). Returns 0.
float ObjectApplyWaterSpell(entt::entity object, entt::entity entity)
{
	auto& data = MutableDataOf(entity);
	if (ecs::fire::IsOnFire(object) && data.puttingOutFireReaction == 0)
	{
		const auto& spell = Locator::entitiesRegistry::value().Get<const Spell>(entity);
		// (inferido) a spell without a player (+0xA4 NULL) passes the neutral player, as the other CreateReaction callers
		data.puttingOutFireReaction =
		    ecs::effects::reactions::CreateReaction(entity, openblack::Reaction::ReactToMagicWaterPuttingOutFire, spell.player, true);
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} water on burning entity {} -> reaction {} (34)",
			                   static_cast<uint32_t>(entity), static_cast<uint32_t>(object), data.puttingOutFireReaction);
		}
	}
	return 0.0f;
}
} // namespace

float water::RainRadius(MagicType type)
{
	// mov eax, [info + 0x10]; sub eax, 0x16: 22 -> 6.0 (0x92BFA8), 23 -> 12.0 (0x92BFAC), else 1.0
	switch (type)
	{
	case MagicType::Water:
		return 6.0f;
	case MagicType::WaterPowerUpOne:
		return 12.0f;
	default:
		return 1.0f;
	}
}

float water::RippleGrowth(MagicType type)
{
	// 22 -> 2.0 (0x92BFA0), 23 -> 4.0 (0x92BFA4), else 1.0
	switch (type)
	{
	case MagicType::Water:
		return 2.0f;
	case MagicType::WaterPowerUpOne:
		return 4.0f;
	default:
		return 1.0f;
	}
}

bool water::RippleDue(float age, float lastRipple)
{
	// fld [+0xB8]; fsub [+0xEC]; fstp dword (single); fld 0.1; fcomp; test ah, 1: C0 = 0.1 < diff
	const volatile float difference = age - lastRipple;
	return k_RippleEvery < difference;
}

float water::ApplyWaterSpell(entt::entity object, entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.Valid(entity))
	{
		return 0.0f;
	}
	if (registry.AllOf<Tree>(object))
	{
		// Tree::ApplyWaterSpell 0x74C390: the Object part first (its result dropped, fstp st(0))
		ObjectApplyWaterSpell(object, entity);
		const auto& spell = registry.Get<const Spell>(entity);
		// pu = (spell +0xB4 == 0x17, WATER_PU1): grows full grown trees past their size and never seeds a sapling
		const auto sapling = ecs::ApplyWaterSpell(object, spell.magicType == MagicType::WaterPowerUpOne);
		if (sapling != entt::null && spell.hasPlayer)
		{
			// GPlayer::FUN_0064DA80(0xE, 1): a statistic kept only in a multiplayer game (IsMultiplayerGame 0x552F80),
			// nothing here. GAlignment::Update(player, newTree, 1) 0x4145A0: good.
			ecs::effects::alignment::UpdateForTree(spell.player, true);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} water on tree {}: sapling {} (good alignment)",
				                   static_cast<uint32_t>(entity), static_cast<uint32_t>(object),
				                   static_cast<uint32_t>(sapling));
			}
		}
		return 1.0f;
	}
	if (registry.AllOf<Field>(object))
	{
		// Field::ApplyWaterSpell 0x528F30: the Object part, then (with a player) GPlayer::ConsiderMakingCreatureMimicPlayer
		// 0x4EA900 (action 0x21, magic 0x16; TODO(M8): the creature's learning), then, if it is not on fire, sow or grow
		const float result = ObjectApplyWaterSpell(object, entity);
		if (!ecs::fire::IsOnFire(object))
		{
			const auto* field = registry.TryGet<const Field>(object);
			const auto crops = field->crops;
			const float growth = field->growth;
			ecs::ApplyWaterSpellToField(object);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Spell trace: spell {} water on field {}: crops {} -> {}, growth {:.1f} -> {:.1f}, food {:.2f}",
				                   static_cast<uint32_t>(entity), static_cast<uint32_t>(object), crops, field->crops, growth,
				                   field->growth, field->food);
			}
		}
		return result;
	}
	return ObjectApplyWaterSpell(object, entity);
}

const water::SpellWaterData* water::DataOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().TryGet<const SpellWaterData>(spell);
}

void openblack::magic::RegisterWaterSpell()
{
	// SpellWater's vtable 0x8F553C overrides only Process (vt 0x528); the rest is Spell's
	SpellOps ops;
	ops.initWithPos = InitWithPos;
	ops.initWithObject = base::InitWithObject;
	ops.process = Process;
	ops.spellEvent = spell_event::SpellEvent;
	ops.costToMaintain = base::CalculateCostToMaintain;
	ops.closeDown = base::CloseDown;
	ops.toBeDeleted = nullptr;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;
	RegisterOps(SpellClass::Water, ops);
}
