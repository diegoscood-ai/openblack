/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellFlock.h"

#include <cmath>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/MagicTables.h"
#include "PSys/PSys.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "PSys/Rules/Shield.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
namespace animal_ai = openblack::ecs::animal_ai;

namespace
{
/// Flock::Flock 0x52F780's id argument of SpellFlock::InitWithPos 0x7232D0 (push 0xABA52)
constexpr int32_t k_FlockId = 0xABA52;
/// Flock::Flock 0x52F780: domain radius 0x50 (and 0x1E flock distance, which the spell sets to the radius)
constexpr uint16_t k_FlockDomainRadius = 0x50;

Spell& SpellOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().Get<Spell>(spell);
}

/// OPENBLACK_TEST_FLOCK_SHOT="<turns>,<path.png>[;...]": a screenshot that many game turns after a flock spell's cast
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
		const char* value = std::getenv("OPENBLACK_TEST_FLOCK_SHOT");
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

void TestShots(unsigned int castTurn)
{
	if (Game::Instance() == nullptr)
	{
		return;
	}
	for (auto& shot : Shots())
	{
		if (!shot.done && CurrentTurn() >= castTurn + shot.turns)
		{
			shot.done = true;
			Game::Instance()->RequestScreenshot(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Flock test: screenshot {} turns after the cast (turn {}) -> {}",
			                   shot.turns, CurrentTurn(), shot.path);
			return; // one request per frame
		}
	}
}

SpellFlockData& DataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellFlockData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<SpellFlockData>(spell);
}

/// Spell::GetMagicInfo 0x7201D0 of the two classes: numberToCreate +0x58, alignmentSwitch +0x5C, distanceToTravel
/// +0x60 (GetDistanceToTravel 0x723530 / 0x723540), huntingRadius +0x64 (fn_00723170, ground only)
struct FlockInfo
{
	uint32_t numberToCreate {0};
	float alignmentSwitch {0.0f};
	float distanceToTravel {0.0f};
	float huntingRadius {0.0f};
};

FlockInfo InfoOf(entt::entity spell)
{
	const auto& tables = Locator::infoConstants::value();
	const auto type = SpellOf(spell).magicType;
	FlockInfo out;
	if (const auto* flying = GetMagicInfoAs<GMagicFlockFlyingInfo>(tables, type); flying != nullptr)
	{
		out = {flying->numberToCreate, flying->alignmentSwitch, flying->distanceToTravel, 0.0f};
	}
	else if (const auto* ground = GetMagicInfoAs<GMagicFlockGroundInfo>(tables, type); ground != nullptr)
	{
		out = {ground->numberToCreate, ground->alignmentSwitch, ground->distanceToTravel, ground->huntingRadius};
	}
	return out;
}

/// vt 0x55C GetNumberToCreate
int NumberToCreate(entt::entity spell)
{
	return spell_flock::NumberToCreate(InfoOf(spell).numberToCreate, GetTribalPower(spell));
}

/// GetPlayer()->+0x60 (GAlignment) +8
float PlayerAlignment(entt::entity spell)
{
	return ecs::effects::alignment::Get(SpellOf(spell).player);
}

float LandHeight(glm::vec2 metres)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(metres) : 0.0f;
}

/// MapCoords::InBounds 0x6042C0: the high words (10 m cells) unsigned below g_game +0x59C8 / +0x59C4 (the land's cells
/// per side; 512 without a land, the fallback of Magic/CastRules)
bool InBoundsMapCoords(glm::ivec2 p)
{
	const uint32_t side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	return ecs::map_coords::InBounds(ecs::map_coords::MapCoords {p.x, p.y, 0.0f}, side);
}

/// MapCoords (x, z with a height above the land) -> the world point (MapCoords::GetLHPoint 0x605C40)
glm::vec3 WorldOf(glm::ivec2 mapCoords, float height)
{
	const auto xz = spell_flock::ToMetres(mapCoords);
	return {xz.x, LandHeight(xz) + height, xz.y};
}

/// GameFloatRand(max) (GRand 0x6DE530)
using game_random::GameFloatRand;

/// The spawn loop's jitter (0x723DB6..0x723E49): ftol((metres + GameFloatRand(0.2) - 0.1) x 65536 / 10), x then z
int32_t Jitter(int32_t mapCoord)
{
	const double offset = static_cast<double>(GameFloatRand(spell_flock::k_SpawnJitter + spell_flock::k_SpawnJitter)) -
	                      static_cast<double>(spell_flock::k_SpawnJitter);
	const double metres = static_cast<double>(mapCoord) * 10.0 * static_cast<double>(1.52587890625e-05f);
	return static_cast<int32_t>((metres + offset) * 65536.0 / 10.0);
}

/// The leader of a flock: its first member (flock +0x40 -> +8)
entt::entity LeaderOf(entt::entity flockEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (flockEntity == entt::null || !registry.Valid(flockEntity) || !registry.AllOf<Flock>(flockEntity))
	{
		return entt::null;
	}
	const auto& members = registry.Get<const Flock>(flockEntity).members;
	return members.empty() ? entt::null : members.front();
}

/// Object::SetScale (vt 0x124), MobileWallHug::SetGameAngle 0x60DA90 (+0x5C and the Y angle) and SetPos with the
/// height above the land, before the animal's AI state exists (it takes the altitude from the drawn position)
void PlaceAnimal(entt::entity animal, const glm::vec3& world, float scale, uint16_t angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(animal);
	transform.position = world;
	transform.scale = glm::vec3(scale);
	animal_ai::detail::FaceAngle(transform, angle);
	if (auto* brain = animal_ai::detail::BrainOf(animal); brain != nullptr)
	{
		brain->angle = angle;
	}
}

/// SpellEvent{2 (point), the leader's position, its movement (MobileWallHug::GetMovementDirection 0x60C040: the step
/// +0x64 / +0x6C, zero before its first move), 1.0, no shield test}: the reaction (37 for the flying flock)
void LeaderEvent(entt::entity spell, entt::entity leader)
{
	psys::SpellEventInfo event;
	event.type = psys::SpellEventInfo::Point;
	event.position = Locator::entitiesRegistry::value().Get<const Transform>(leader).position;
	event.velocity = glm::vec3(0.0f);
	event.strength = 1.0f;
	event.checkShields = false;
	OpsOf(SpellOf(spell).spellClass).spellEvent(spell, event);
}

/// the per-animal part of the SpellDove / SpellBat / SpellWolf ctors (0x41EAC0 / 0x41EF60 / 0x4207F0)
SpellFlockAnimal& AddAnimal(entt::entity spell, entt::entity animal, bool wolf, const glm::vec3& world)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = registry.AssignOrReplace<SpellFlockAnimal>(animal);
	data.spell = spell;
	data.wolf = wolf;
	data.fade.SetPosition(spell_flock::k_FullAlpha);
	data.previous = world;
	return data;
}

/// The part of SpellFlockFlying::Process 0x723BC0 / SpellFlockGround::Process 0x7242A0 before their loops: the spell
/// is where the leader is; true while animals are still to be made (created < GetNumberToCreate), with the emit
/// accumulator advanced and the spawn segment (old -> new hand position) in `from` / `fromHeight`
bool StartSpawnTurn(entt::entity spell, float& previousEmitted, glm::ivec2& from, float& fromHeight)
{
	auto& data = DataOf(spell);
	auto& component = SpellOf(spell);
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto leader = LeaderOf(data.flock); leader != entt::null && registry.AllOf<Transform>(leader))
	{
		component.position = ToMap(registry.Get<const Transform>(leader).position); // +0x14 = leader +0x14
	}
	if (data.created >= NumberToCreate(spell))
	{
		return false;
	}
	previousEmitted = data.emitted;
	// emit += GetNumToEmitPerSecond (12, a float) x *(u32*)0xD01A38 (fimul) x 0.001 (0x8AA3B0), stored as a float
	data.emitted = static_cast<float>(static_cast<double>(data.emitted) +
	                                  static_cast<double>(spell_flock::k_EmitPerSecond) * static_cast<double>(k_TurnMs) *
	                                      static_cast<double>(0.001f));
	from = data.lastSpawn;
	fromHeight = data.lastSpawnHeight;
	// the new end of the segment: the hand (PSysProcessInfo +0xC = spell +0x70), its height above the land
	const auto& hand = component.processInfo.handPos;
	data.lastSpawn = spell_flock::ToMapCoords(glm::vec2(hand.x, hand.z));
	data.lastSpawnHeight = hand.y - LandHeight(glm::vec2(hand.x, hand.z));
	return true;
}

/// One animal of the loop: its spawn point S (unjittered, for the destination) and the created point (jittered)
struct Spawn
{
	glm::ivec2 point;    ///< ebp-0x20: S
	float height;        ///< ebp-0x18
	glm::ivec2 created;  ///< ebp-0x14: S + the jitter
	glm::ivec2 target;   ///< ebp-0x34: T
};

/// The loop's common checks (0x723E4C..0x723EAF): the jittered point on the map, a human caster's influence at the
/// spell (its +0x14, the leader's position) and fn_00723570's destination
bool SpawnAllowed(entt::entity spell, Spawn& spawn)
{
	if (!InBoundsMapCoords(spawn.created))
	{
		return false;
	}
	const auto& component = SpellOf(spell);
	if (component.isHumanPlayerCasting &&
	    influence::CalculatePlayerInfluence(component.player, ToWorld(component.position),
	                                        influence::CalcType::Default, true) <= 0.0f)
	{
		return false; // Influence::CalculatePlayerInfluence 0x5CD170 (pos, player, 0, 0, 1) <= 0
	}
	// fn_00723570
	const auto& data = DataOf(spell);
	const bool human = component.isHumanPlayerCasting;
	const auto direction = spell_flock::Direction(human, component.processInfo.cameraForward, ToWorld(component.castPos),
	                                              component.processInfo.handPos);
	const auto spawnMetres = spell_flock::ToMetres(spawn.point);
	const float side = spell_flock::Side(human, direction, spawnMetres, glm::vec2(component.castPos.x, component.castPos.z),
	                                     data.created);
	const float angle = spell_flock::Angle(data.created, side, NumberToCreate(spell));
	const auto rotated = spell_flock::Rotate(direction, angle);
	return spell_flock::Destination(spawn.point, rotated, InfoOf(spell).distanceToTravel, InBoundsMapCoords, spawn.target);
}

/// The interpolation (0x723D0A..0x723DA7) and the jitter of the `created`-th animal
Spawn MakeSpawn(float created, float previousEmitted, float emitted, glm::ivec2 from, float fromHeight, glm::ivec2 to,
                float toHeight)
{
	// fild created; fsub prev; fld emit; fsub prev; fdivp; fstp (a float)
	const auto f = static_cast<float>((static_cast<double>(created) - static_cast<double>(previousEmitted)) /
	                                  (static_cast<double>(emitted) - static_cast<double>(previousEmitted)));
	Spawn spawn {};
	spawn.point = spell_flock::SpawnPoint(from, to, f);
	spawn.height = static_cast<float>(static_cast<double>(fromHeight) +
	                                  (static_cast<double>(toHeight) - static_cast<double>(fromHeight)) * f);
	// `created` != 0 always holds after the increment: every animal is jittered, x first
	spawn.created.x = Jitter(spawn.point.x);
	spawn.created.y = Jitter(spawn.point.y);
	return spawn;
}

void TraceAnimal(entt::entity spell, const char* kind, entt::entity animal, const Spawn& spawn, float height, bool leader)
{
	if (!TraceEnabled())
	{
		return;
	}
	const auto s = spell_flock::ToMetres(spawn.created);
	const auto t = spell_flock::ToMetres(spawn.target);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Spell trace: spell {} {} {} #{} entity {} at ({:.1f}, {:.1f}) +{:.1f} m -> ({:.1f}, {:.1f}){}",
	                   static_cast<uint32_t>(spell), kind, DataOf(spell).created, NumberToCreate(spell),
	                   animal == entt::null ? -1 : static_cast<int>(static_cast<uint32_t>(animal)), s.x, s.y, height,
	                   t.x, t.y, leader ? " (leader)" : "");
}

// ---- SpellFlock ----

/// SpellFlock::InitWithPos 0x7232D0
int FlockInitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = DataOf(spell);
	data = {}; // the ctor 0x7231C0 / fn_00723210 / SetToZero 0x7239E0
	// the last spawn point: the hand (PSysProcessInfo +0xC) as MapCoords, its height above the land
	data.lastSpawn = spell_flock::ToMapCoords(glm::vec2(info.handPos.x, info.handPos.z));
	data.lastSpawnHeight = info.handPos.y - LandHeight(glm::vec2(info.handPos.x, info.handPos.z));
	// new Flock(pos, GFlockInfo 0xC5E624, GetPlayer(), 0xABA52) (Flock::Flock 0x52F780), flock +0x52 = flock +0x50
	const auto flockEntity = registry.Create();
	auto& flock = registry.Assign<Flock>(flockEntity);
	flock.id = k_FlockId;
	flock.domainCentre = ToWorld(position); // SetDomainCentrePos 0x52FC20
	flock.savedDomainCentre = flock.domainCentre;
	flock.domainRadius = k_FlockDomainRadius;
	flock.flockDistance = flock.domainRadius;
	data.flock = flockEntity;
	data.castTurn = CurrentTurn();
	return base::InitWithPos(spell, position, castData, info);
}

/// SpellFlock::Process 0x7233D0 (the end of both Process)
int FlockProcess(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& component = SpellOf(spell);
	TestShots(DataOf(spell).castTurn); // OPENBLACK_TEST_FLOCK_SHOT
	if (!component.closedDown)
	{
		for (const auto member : std::vector<entt::entity>(spell_objects::Objects(spell)))
		{
			if (!registry.Valid(member) || !registry.AllOf<Transform>(member))
			{
				continue;
			}
			auto* animal = registry.TryGet<SpellFlockAnimal>(member);
			const auto position = registry.Get<const Transform>(member).position;
			const auto previous = animal != nullptr ? animal->previous : position;
			// fn_006D0C20(its +0x2C, its +0x14, GetRadius vt 0x60 = Get2DRadius): the first shield it crossed into
			const auto* hit = psys::shields::FindShieldCrossedInto(previous, position, ecs::object::GetRadius(member));
			if (hit != nullptr)
			{
				// SpellEvent{4, its position, no movement, 1.0, target = the shield's spell (fn_006D0B10)}; costPerShieldCollide
				psys::SpellEventInfo event;
				event.type = psys::SpellEventInfo::HitSpell;
				event.position = position;
				event.velocity = glm::vec3(0.0f);
				event.strength = 1.0f;
				event.checkShields = false;
				event.target = psys::shields::SpellOf(*hit);
				if (OpsOf(component.spellClass).spellEvent(spell, event) == 0 && animal != nullptr)
				{
					// R15: vt+0x6A4 = SpellDove::SetDying 0x41F5C0 / SpellWolf::SetDying 0x420CF0, the fade (the shield
					// held); when the shield broke (the event answered 1) the animal goes on
					spell_flock::StartFade(*animal);
				}
				psys::shields::AddImpactTarget(*hit, position); // fn_006D0AF0: a spark on the shield there
			}
		}
	}
	// SpellWithObjects::Process 0x721290: CoreProcess, then ProcessSpellObjectsAndRemoveToBeDeleted 0x721040, whose
	// Animal::ProcessBySpell 0x417700 is ProcessFadeOut (vt 0xBD8)
	base::CoreProcess(spell);
	bool any = false;
	for (const auto member : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		if (!registry.Valid(member))
		{
			spell_objects::Remove(spell, member); // IsAvailable (vt 0x2C) != 1: off the list
			continue;
		}
		any = true;
		if (auto* animal = registry.TryGet<SpellFlockAnimal>(member); animal != nullptr)
		{
			animal->previous = registry.AllOf<Transform>(member) ? registry.Get<const Transform>(member).position
			                                                   : animal->previous;
			// (the wolves' hunt and their end within 30 m are their own MOVE_TO_POS state, SpellWolf::MoveToPos
			// 0x421300: ECS/AnimalPredators.cpp SpellWolfMoveToPos, in the animals' turn)
			if (spell_flock::ProcessFade(*animal))
			{
				animal_ai::Remove(member); // ToBeDeleted(0) at alpha 0
				continue;
			}
			// SpellDove::SetColor 0x41F630 / SpellWolf::Draw 0x51C6EC: colour (fistp alpha) << 24 | 0xFFFFFF, and the
			// mesh translucent while that is not 255 (done per game turn: the alpha only changes in the turn)
			const float alpha = std::nearbyint(animal->fade.value);
			animal_ai::SetAlpha(member, alpha / spell_flock::k_FullAlpha);
		}
	}
	int result = any ? 1 : (component.psys != 0 ? 1 : 5);
	if (TraceEnabled() && (CurrentTurn() - DataOf(spell).castTurn) % 10 == 0)
	{
		for (const auto member : spell_objects::Objects(spell))
		{
			if (registry.Valid(member) && registry.AllOf<Transform, SpellFlockAnimal>(member))
			{
				const auto& p = registry.Get<const Transform>(member).position;
				const auto* brain = registry.TryGet<const AnimalBrain>(member);
				const auto target = brain != nullptr ? brain->target : entt::null;
				const bool hunting = target != entt::null && registry.Valid(target) && registry.AllOf<Transform>(target);
				const auto at = hunting ? registry.Get<const Transform>(target).position : p;
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Spell trace: spell {} flock member {} at ({:.1f}, {:.1f}, {:.1f}) state {} alpha {:.0f} target {} "
				                   "at {:.1f} m speed {}",
				                   static_cast<uint32_t>(spell), static_cast<uint32_t>(member), p.x, p.y, p.z,
				                   static_cast<int>(animal_ai::TopState(member)), registry.Get<const SpellFlockAnimal>(member).fade.value,
				                   hunting ? static_cast<int>(static_cast<uint32_t>(target)) : -1,
				                   glm::distance(glm::vec2(p.x, p.z), glm::vec2(at.x, at.z)), brain != nullptr ? brain->speed : 0);
			}
		}
	}
	// the Flock gone (IsAvailable != 1): +0xF4 = 0; else the FLOCK is where its leader is (0x7234F2..0x723519: edi =
	// spell +0xF4, flock +0x14 = leader +0x14, the flock's domain centre in components::Flock; audit4: the port wrote
	// the spell's +0x14 here, which only the start of the class Process 0x723BE6 does)
	auto& data = DataOf(spell);
	if (data.flock != entt::null && (!registry.Valid(data.flock) || !registry.AllOf<Flock>(data.flock)))
	{
		data.flock = entt::null;
		return result;
	}
	if (const auto leader = LeaderOf(data.flock); leader != entt::null && registry.AllOf<Transform>(leader))
	{
		registry.Get<Flock>(data.flock).domainCentre = registry.Get<const Transform>(leader).position;
	}
	return result;
}

/// SpellFlock::CalculateCostToMaintain 0x723240: fild(+0xF0, the objects) x costPerEvent + costPerGameTurn
float FlockCostToMaintain(entt::entity spell)
{
	const auto& effect = EffectInfoOf(spell);
	const auto count = static_cast<double>(spell_objects::Objects(spell).size());
	return static_cast<float>(count * static_cast<double>(effect.costPerEvent) + static_cast<double>(effect.costPerGameTurn));
}

/// SpellFlock::CloseDown 0x723270 = SpellWithObjects::CloseDown 0x721300: CoreCloseDown, then (GetSetObjectsDyingOnCloseDown
/// 0x55CF50 = 1) SetDying (vt 0x6A4) on every object not already ToBeDeleted: the animals fade
void FlockCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto member : spell_objects::Objects(spell))
	{
		if (!registry.Valid(member))
		{
			continue;
		}
		if (auto* animal = registry.TryGet<SpellFlockAnimal>(member); animal != nullptr)
		{
			spell_flock::StartFade(*animal);
		}
	}
}

/// SpellWithObjects::ToBeDeleted 0x720FD0 (vt 0x530 CloseDown first, then the list goes) and the flying class's dtor
/// fn_007239B0 (the cast PSys +0x110 deleted)
void FlockToBeDeleted(entt::entity spell)
{
	FlockCloseDown(spell);
	auto& data = DataOf(spell);
	if (data.castPsys != 0)
	{
		psys::manager::Delete(data.castPsys);
		data.castPsys = 0;
	}
	for (const auto member : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		spell_objects::Remove(spell, member);
	}
}

// ---- SpellFlockFlying ----

/// SpellFlockFlying::GetParticleType 0x723A30: 0x7C + evil
ParticleType FlyingParticleType(entt::entity spell)
{
	return spell_flock::IsEvil(PlayerAlignment(spell), InfoOf(spell).alignmentSwitch) ? ParticleType::FlockFlyingRainEvil
	                                                                                   : ParticleType::FlockFlyingRainGood;
}

/// SpellFlockFlying::InitWithPos 0x723A80
int FlyingInitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	const int result = FlockInitWithPos(spell, position, castData, info);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(spell) || !registry.AllOf<Spell>(spell))
	{
		return result;
	}
	const bool evil = spell_flock::IsEvil(PlayerAlignment(spell), InfoOf(spell).alignmentSwitch);
	// this computer's interface casts it (NetUnsafeIsMyInterfaceCasting 0x7201E0): SF_FlockFlyingCast* (0x7A + evil) at
	// the cast point, GJPSysInterface::Create(NULL spell, type, point, (0, 0, 0), 1.0, 0), SetPlayer (vt 0x20). It
	// follows the local hand (UR_FollowLocalHand) and Draw 0x724100 steps it every frame.
	if (SpellOf(spell).isMyInterfaceCasting)
	{
		const auto type = evil ? ParticleType::FlockFlyingCastEvil : ParticleType::FlockFlyingCastGood;
		const auto file = psys::ParticleTypeFile(type);
		if (!file.empty())
		{
			auto& data = DataOf(spell);
			data.castPsys = psys::manager::Start(std::string(file), ToWorld(position), 1.0f);
			if (data.castPsys != 0)
			{
				psys::manager::SetPerFrame(data.castPsys);
				if (auto* effect = psys::manager::Find(data.castPsys); effect != nullptr)
				{
					effect->SetPlayer(static_cast<int>(SpellOf(spell).player));
				}
			}
		}
	}
	return result;
}

/// SpellFlockFlying::Process 0x723BC0
int FlyingProcess(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = DataOf(spell);
	float previousEmitted = 0.0f;
	glm::ivec2 from {0};
	float fromHeight = 0.0f;
	if (!StartSpawnTurn(spell, previousEmitted, from, fromHeight))
	{
		// every animal made: the cast effect closes (vt 0x118)
		if (data.castPsys != 0)
		{
			psys::manager::CloseDown(data.castPsys);
		}
		return FlockProcess(spell);
	}
	const auto info = InfoOf(spell);
	while (static_cast<float>(data.created) < data.emitted)
	{
		++data.created;
		const auto type = spell_flock::FlyingAnimal(PlayerAlignment(spell), info.alignmentSwitch);
		const auto& animalInfo = Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
		auto spawn = MakeSpawn(static_cast<float>(data.created), previousEmitted, data.emitted, from, fromHeight,
		                       data.lastSpawn, data.lastSpawnHeight);
		if (!SpawnAllowed(spell, spawn))
		{
			continue;
		}
		const float targetAltitude = animalInfo.altitudeNormal; // T's height = the GAnimalInfo +0x278
		// fn_00419D10(S, info, no town, the flock, age 0)
		const auto world = WorldOf(spawn.created, spawn.height);
		const auto animal = animal_ai::CreateAnimal(world, type, data.flock, 0, -1);
		if (animal != entt::null)
		{
			// scale = GetScale() (GameThingWithPos 0x4247E0: 1) x 2.8 + GameFloatRand(3 - 2.8), the random first
			const float random = GameFloatRand(spell_flock::k_FlyingScaleTop - spell_flock::k_FlyingScale);
			const float scale = 1.0f * spell_flock::k_FlyingScale + random;
			const auto angle = gutils::GetAngleFromXZ(spawn.created, spawn.target); // 0x74D240
			PlaceAnimal(animal, world, scale, angle);
			AddAnimal(spell, animal, false, world);
			spell_objects::Add(spell, animal);
			// every bird (fn_00723160 = 1: created % 1 == 0) is a target of the rain effect (vt 0x114)
			if (auto* effect = psys::manager::Find(SpellOf(spell).psys); effect != nullptr)
			{
				effect->AddTarget(animal);
			}
		}
		const auto leader = LeaderOf(data.flock);
		if (leader == entt::null || animal == entt::null)
		{
			TraceAnimal(spell, "SpellFlockFlying", animal, spawn, spawn.height, false);
			continue; // (the original would call through a null animal here; it only happens when no animal was made)
		}
		if (leader == animal)
		{
			LeaderEvent(spell, leader);
			// SetupMoveToPos(T, START_WANDER 0x1F), SetState(0, SPECIAL_MOVE_TO_POS 0x2C) (vt 0x938); the flock follows
			// in formation: +0x78 FOLLOW_FLOCK 0x2D, +0x80 3, +0x7C DECIDE_WHAT_TO_DO 0x2B
			animal_ai::MoveTo(animal, spell_flock::ToMetres(spawn.target), targetAltitude, animal_ai::AnimalState::StartWander);
			animal_ai::SetStateRaw(animal, animal_ai::AnimalState::SpecialMoveToPos);
			if (auto* flock = registry.TryGet<Flock>(data.flock); flock != nullptr)
			{
				flock->followState = static_cast<uint8_t>(animal_ai::AnimalState::FollowFlock);
				flock->followMode = 3;
				flock->afterMove = static_cast<uint8_t>(animal_ai::AnimalState::DecideWhatToDo);
			}
		}
		else
		{
			// SetupMoveToPos(the leader's GetDestPos vt 0x860 = its +0x80, START_WANDER), SetState(0, 0x2C)
			const auto destination = animal_ai::Destination(leader).value_or(glm::vec3(0.0f));
			animal_ai::MoveTo(animal, glm::vec2(destination.x, destination.z), destination.y,
			                  animal_ai::AnimalState::StartWander);
			animal_ai::SetStateRaw(animal, animal_ai::AnimalState::SpecialMoveToPos);
		}
		TraceAnimal(spell, "SpellFlockFlying", animal, spawn, spawn.height, leader == animal);
	}
	return FlockProcess(spell);
}

// ---- SpellFlockGround ----

ParticleType GroundParticleType(entt::entity /*spell*/)
{
	return ParticleType::FlockGroundDust; // SpellFlockGround::GetParticleType 0x724290: 0x7E
}

/// SpellFlockGround::InitWithPos 0x724220 = SpellFlock::InitWithPos
int GroundInitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	return FlockInitWithPos(spell, position, castData, info);
}

/// fn_00420F50 (SpellWolf, start, destination, halfWidth): the corridor (+0x154..+0x164), +0x80 (the move goal) and
/// +0x148 (the final destination) both = the destination (0x421054..0x421081), then SetRunToFinalDest 0x4209C0
/// (0x421084: speed = scale x info.speed4 x 1.1, SetupMoveToPos(+0x148, SET_DYING)), in the spell's turn: the wolf
/// runs from its first turn
void SetupWolf(entt::entity wolf, glm::vec2 start, glm::vec2 destination, float halfWidth)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* data = registry.TryGet<SpellFlockAnimal>(wolf);
	auto* brain = animal_ai::detail::BrainOf(wolf);
	if (data == nullptr || brain == nullptr)
	{
		return;
	}
	spell_flock::SetupCorridor(*data, start, destination, halfWidth);
	data->destination = destination;
	brain->finalDestination = destination; // +0x148
	brain->goal = destination;             // +0x80
	auto& animal = registry.Get<Animal>(wolf);
	animal_ai::detail::Context ctx {wolf, animal, *brain, registry.Get<Transform>(wolf), animal_ai::detail::InfoOf(animal)};
	animal_ai::detail::SetRunToFinalDest(ctx);
}

/// The leader's GetDestPos (vt 0x860 = MobileWallHug +0x80): its final destination since SetRunToFinalDest, or the
/// prey it chases while it hunts (the followers' corridor then points at it, as in the original)
glm::vec2 WolfDestination(entt::entity leader)
{
	const auto destination = animal_ai::Destination(leader).value_or(glm::vec3(0.0f));
	return {destination.x, destination.z};
}

/// SpellFlockGround::Process 0x7242A0
int GroundProcess(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = DataOf(spell);
	float previousEmitted = 0.0f;
	glm::ivec2 from {0};
	float fromHeight = 0.0f;
	if (!StartSpawnTurn(spell, previousEmitted, from, fromHeight))
	{
		return FlockProcess(spell);
	}
	const auto info = InfoOf(spell);
	while (static_cast<float>(data.created) < data.emitted)
	{
		++data.created;
		auto spawn = MakeSpawn(static_cast<float>(data.created), previousEmitted, data.emitted, from, fromHeight,
		                       data.lastSpawn, data.lastSpawnHeight);
		if (!SpawnAllowed(spell, spawn))
		{
			continue;
		}
		// on the ground: the created point's height 0 (0x72445C), T's 0 (0x72457A); fn_00419D10(S, GAnimalInfo 22
		// SpellWolf 0xC50DB8, no town, the flock, age 0)
		const auto world = WorldOf(spawn.created, 0.0f);
		const auto animal = animal_ai::CreateAnimal(world, AnimalInfo::SpellWolf, data.flock, 0,
		                                            static_cast<int32_t>(SpellOf(spell).player)); // SetPlayer (vt 0x20)
		if (animal == entt::null)
		{
			continue;
		}
		// the SpellWolf class (AllocSpell 0x4207F0) ignores fn_00419D10's age: Lion's ctor fn_0041FD30 gets
		// info.grownUpAge + 1 (0x420816..0x420822, an adult), and +0xE4 (the hunger) = info.hunger (0x420885..0x42088E):
		// it is hungry from birth. The age before the brain exists (its birth turn comes from it, Living::SetAge)
		const auto& wolfInfo = Locator::infoConstants::value().animal.at(static_cast<size_t>(AnimalInfo::SpellWolf));
		registry.Get<Animal>(animal).age = wolfInfo.grownUpAge + 1;
		if (auto* brain = animal_ai::detail::BrainOf(animal); brain != nullptr)
		{
			brain->hunger = static_cast<int16_t>(wolfInfo.hunger);
		}
		// scale = GetScale() x 1.5 + GameFloatRand(2 - 1.5), the random first; SetGameAngle 0x60DA90 (+0x5C and the Y
		// angle) of GetAngleFromXZ 0x74D240 (the jittered point ebp-0x14, T ebp-0x34; 0x7245E8..0x7245FB) = LHArcTan
		// 0x74D0C0 of T - S
		const float random = GameFloatRand(spell_flock::k_GroundScaleTop - spell_flock::k_GroundScale);
		const float scale = 1.0f * spell_flock::k_GroundScale + random;
		const auto angle = gutils::GetAngleFromXZ(spawn.created, spawn.target);
		PlaceAnimal(animal, world, scale, angle);
		AddAnimal(spell, animal, true, world);
		spell_objects::Add(spell, animal);
		// GParticleContainer::CreateSpotVisual(S, MAGIC_OBJECT_CREATED 9, 1.0, no owner): the entry's own life
		psys::manager::CreateSpotVisual(spell_flock::k_MagicObjectCreated, world, 0.0f, entt::null);
		if (auto* effect = psys::manager::Find(SpellOf(spell).psys); effect != nullptr)
		{
			effect->AddTarget(animal); // every wolf (vt 0x114)
		}
		const auto leader = LeaderOf(data.flock);
		if (leader != entt::null)
		{
			const auto start = spell_flock::ToMetres(spawn.point); // the unjittered S
			if (leader == animal)
			{
				LeaderEvent(spell, leader);
				SetupWolf(animal, start, spell_flock::ToMetres(spawn.target), info.huntingRadius);
			}
			else
			{
				SetupWolf(animal, start, WolfDestination(leader), info.huntingRadius);
			}
		}
		TraceAnimal(spell, "SpellFlockGround", animal, spawn, 0.0f, leader == animal);
	}
	return FlockProcess(spell);
}

/// SetDying (vt 0x6A4) of the flock miracles' animals, instead of Living::SetDying (animal_ai::SetSpeciesDying):
/// SpellDove::SetDying 0x41F5C0 (also SpellBat's slot) / SpellWolf::SetDying 0x420CF0 only start the fade (no dying
/// states, no corpse) and return 1
void SpellAnimalSetDying(entt::entity animal)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellFlockAnimal>(animal); data != nullptr)
	{
		if (TraceEnabled() && data->fade.destination != 0.0f)
		{
			const auto* brain = registry.TryGet<const AnimalBrain>(animal);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: flock member {} SetDying (state {}, final {})",
			                   static_cast<uint32_t>(animal), brain != nullptr ? brain->topState : 0,
			                   brain != nullptr ? brain->finalState : 0);
		}
		spell_flock::StartFade(*data);
	}
}

SpellOps CommonOps()
{
	SpellOps ops;
	ops.initWithObject = base::InitWithObject;
	ops.spellEvent = spell_event::SpellEvent;
	ops.costToMaintain = FlockCostToMaintain;
	ops.closeDown = FlockCloseDown; // (not "CloseDown": inside magic:: that is the vt 0x530 dispatch)
	ops.toBeDeleted = FlockToBeDeleted;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast; // vt 0x518 = Spell 0x55CE00
	return ops;
}
} // namespace

// ---- the formulas ----

int spell_flock::NumberToCreate(uint32_t numberToCreate, float tribalPower)
{
	// fild +0x58; fstp; GetTribalPower; fmul; fstp (a float); fistp
	const auto product = static_cast<float>(static_cast<double>(static_cast<float>(numberToCreate)) * static_cast<double>(tribalPower));
	return static_cast<int>(std::nearbyint(product));
}

bool spell_flock::IsEvil(float alignment, float alignmentSwitch)
{
	return alignment < alignmentSwitch;
}

AnimalInfo spell_flock::FlyingAnimal(float alignment, float alignmentSwitch)
{
	return IsEvil(alignment, alignmentSwitch) ? AnimalInfo::SpellBat : AnimalInfo::SpellDove;
}

glm::vec2 spell_flock::Direction(bool human, glm::vec3 cameraForward, glm::vec3 castPos, glm::vec3 handPos)
{
	glm::vec2 d = human ? glm::vec2(cameraForward.x, cameraForward.z) : glm::vec2(castPos.x - handPos.x, castPos.z - handPos.z);
	if (d.x * d.x + d.y * d.y < 0.0001f)
	{
		d = glm::vec2(1.0f, 0.0f);
	}
	return d;
}

float spell_flock::Side(bool human, glm::vec2 direction, glm::vec2 spawn, glm::vec2 castPos, int created)
{
	if (human)
	{
		glm::vec2 v = spawn - castPos;
		if (v.x * v.x + v.y * v.y < 0.0001f)
		{
			v = glm::vec2(1.0f, 0.0f);
		}
		// InverseSquareRoot 0x841170 on both
		v *= 1.0f / std::sqrt(v.x * v.x + v.y * v.y);
		const glm::vec2 d = direction * (1.0f / std::sqrt(direction.x * direction.x + direction.y * direction.y));
		const float cross = v.y * d.x - v.x * d.y;
		if (cross > 0.1f)
		{
			return 1.0f;
		}
		if (cross < -0.1f)
		{
			return -1.0f;
		}
	}
	return created % 2 == 0 ? 1.0f : -1.0f;
}

float spell_flock::Angle(int created, float side, int numberToCreate)
{
	// GetAngleVariation; fimul created; fmul side; fstp; fild N; fdivr
	const auto top = static_cast<float>(static_cast<double>(k_AngleVariation) * created * static_cast<double>(side));
	return numberToCreate != 0 ? static_cast<float>(static_cast<double>(top) / numberToCreate) : 0.0f;
}

glm::vec2 spell_flock::Rotate(glm::vec2 d, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {d.x * c - d.y * s, d.x * s + d.y * c};
}

glm::ivec2 spell_flock::DestinationAt(glm::ivec2 spawn, glm::vec2 direction, float distance)
{
	// fn_006805F0: d x (distance / |d|); the zero vector stays 0
	glm::vec2 d = direction;
	const float length = std::sqrt(d.x * d.x + d.y * d.y);
	if (length != 0.0f)
	{
		d *= distance / length;
	}
	const auto move = [](int32_t coordinate, float delta) {
		// 0x7238E2..0x723905: "xor edx, edx; mov dx, [esi+2]" (the high word, unsigned = map_coords::CellOf), fild,
		// fmul 10 [0x9819A4], fadd d, fdiv 10, __ftol, and the word stored back. All of it in float (FPU at 24 bits)
		const auto cell = ecs::map_coords::CellOf(coordinate);
		const auto moved = static_cast<int32_t>((static_cast<float>(cell) * 10.0f + delta) / 10.0f);
		const uint32_t low = static_cast<uint32_t>(coordinate) & 0xFFFFu;
		return static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(moved)) << 16) | low);
	};
	return {move(spawn.x, d.x), move(spawn.y, d.y)};
}

glm::ivec2 spell_flock::SpawnPoint(glm::ivec2 from, glm::ivec2 to, float f)
{
	// fild (new - old); fmul f; fstp (a float); fld; fistp (to nearest)
	const auto x = static_cast<float>(static_cast<double>(to.x - from.x) * static_cast<double>(f));
	const auto z = static_cast<float>(static_cast<double>(to.y - from.y) * static_cast<double>(f));
	return {from.x + static_cast<int32_t>(std::nearbyint(x)), from.y + static_cast<int32_t>(std::nearbyint(z))};
}

glm::ivec2 spell_flock::ToMapCoords(glm::vec2 metres)
{
	// fld x; fmul 6553.6 (0x8AC400, a float); __ftol (truncation)
	return {ecs::map_coords::ToFixed(metres.x), ecs::map_coords::ToFixed(metres.y)};
}

glm::vec2 spell_flock::ToMetres(glm::ivec2 mapCoords)
{
	// fild; fmul 10 / 65536 (0x8AA3A4)
	return {ecs::map_coords::ToMetres(mapCoords.x), ecs::map_coords::ToMetres(mapCoords.y)};
}

void spell_flock::SetupCorridor(SpellFlockAnimal& wolf, glm::vec2 start, glm::vec2 destination, float halfWidth)
{
	// (a, b) = (-(dest.x - start.x), dest.z - start.z); +0x154 = b, +0x15C = a, normalised when not zero
	float a = -(destination.x - start.x);
	float b = destination.y - start.y;
	if (a * a + b * b < 0.0001f)
	{
		a = 0.0f;
		b = 1.0f;
	}
	else if (b != 0.0f || a != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(a * a + b * b);
		a *= inverse;
		b *= inverse;
	}
	wolf.normal = glm::vec2(b, a);
	wolf.offset = -(start.x * wolf.normal.x + start.y * wolf.normal.y);
	wolf.halfWidth = halfWidth;
}

bool spell_flock::WolfArrived(const SpellFlockAnimal& wolf, glm::vec2 position)
{
	// GUtils::GetDistanceInMetres 0x74CD70 (the table hypotenuse 0x74F680 on the two MapCoords), then fcomp 30
	// (0x8BF51C); test ah, 1: below
	return gutils::GetDistanceInMetres(position, wolf.destination) < k_WolfArrive;
}

bool spell_flock::IsPosOnCorridor(const SpellFlockAnimal& wolf, glm::vec2 wolfPosition, glm::vec2 point)
{
	// the distance to the line
	const float across = std::abs(point.y * wolf.normal.y + point.x * wolf.normal.x + wolf.offset);
	if (across > wolf.halfWidth)
	{
		return false;
	}
	// along the corridor (u = (-normal.z, normal.x)) from the wolf's cell corner: SpellWolf::IsPosOnCorridor 0x420E67
	// reads the high words unsigned ("xor eax, eax; mov ax, [ecx+0x16]" = map_coords::CellOf) and multiplies by 10
	// [0x8BE8D4]
	const auto coords = ToMapCoords(wolfPosition);
	const glm::vec2 corner(static_cast<float>(ecs::map_coords::CellOf(coords.x)) * 10.0f,
	                       static_cast<float>(ecs::map_coords::CellOf(coords.y)) * 10.0f);
	const float ux = -wolf.normal.y;
	const float uz = wolf.normal.x;
	const float along = (ux * point.x + uz * point.y) - (ux * corner.x + uz * corner.y);
	return -wolf.halfWidth <= along;
}

void spell_flock::StartFade(SpellFlockAnimal& animal)
{
	// SpellDove::SetDying 0x41F5C0 / SpellWolf 0x420CF0: only while the fade's destination (+0x14C / +0x16C; the
	// Zoomer's value is +0x148 / +0x168, ctors 0x41F280 / 0x420930) is not 0, the fade (vt+0xBD4) to 0 over
	// GetNumTurnsToDieOver (0x14, 0x41F620 / 0x420D50) x [0xD01A38] (ms per turn) x 0.001 s: once started it is never
	// restarted (a second SetDying, the shield or the CloseDown, does nothing)
	if (animal.fade.destination == 0.0f)
	{
		return;
	}
	const float seconds = static_cast<float>(k_TurnsToDieOver * static_cast<int>(k_TurnMs)) * 0.001f;
	animal.fade.SetDestinationWithSpeedAndTime(0.0f, 0.0f, seconds);
}

bool spell_flock::ProcessFade(SpellFlockAnimal& animal)
{
	animal.fade.Update(static_cast<float>(k_TurnMs) * 0.001f);
	return animal.fade.value == 0.0f;
}

const SpellFlockAnimal* spell_flock::AnimalOf(entt::entity animal)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(animal) ? registry.TryGet<const SpellFlockAnimal>(animal) : nullptr;
}

bool spell_flock::IsOnCorridor(entt::entity wolf, glm::vec2 point)
{
	const auto* data = AnimalOf(wolf);
	if (data == nullptr || !data->wolf)
	{
		return false;
	}
	const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(wolf).position;
	return IsPosOnCorridor(*data, glm::vec2(position.x, position.z), point);
}

void openblack::magic::RegisterFlockSpells()
{
	auto flying = CommonOps();
	flying.initWithPos = FlyingInitWithPos;
	flying.process = FlyingProcess;
	flying.particleType = FlyingParticleType;
	RegisterOps(SpellClass::FlockFlying, flying);

	auto ground = CommonOps();
	ground.initWithPos = GroundInitWithPos;
	ground.process = GroundProcess;
	ground.particleType = GroundParticleType;
	RegisterOps(SpellClass::FlockGround, ground);

	// vt+0x6A4: SpellDove 0x41F5C0 (SpellBat's vtable 0x8BABC4 has the same entry), SpellWolf 0x420CF0
	animal_ai::SetSpeciesDying(AnimalInfo::SpellDove, SpellAnimalSetDying);
	animal_ai::SetSpeciesDying(AnimalInfo::SpellBat, SpellAnimalSetDying);
	animal_ai::SetSpeciesDying(AnimalInfo::SpellWolf, SpellAnimalSetDying);
}
