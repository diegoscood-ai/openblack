/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The beam explosion (MAGIC_TYPE 7-9 EXPLOSION_ONE*, seed BEAM_EXPLOSION; SF_BeamExplosionSingle / Many / Loads) and the
// rules of its spot visual SF_BeamExplosionFX. The spell is a plain Spell (SpellGeneral.cpp): everything it does comes
// from UR_Explosion's events. Disassembly: dev\tmp_dis\miracles\impl\m6b\explosion_*.asm; wiki: docs/bw1-notes/magic.md,
// "Explosión de rayo (M6b)".

#include "Explosion.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GroundMarks.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
#include "PSys/PSysWaterRings.h"
#include "PSys/Rules/Shield.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
// ---- constants of PSysExplosion.cpp ----
constexpr float k_DefaultRadius = 5.0f;          ///< 0x67E21E: the shield margin without a spell (else GMagicEffectInfo +0x2C)
constexpr float k_ShieldRayHeight = 200.0f;      ///< [0x8C7B34]: the blast is tested from 200 m above its centre
constexpr float k_TargetShieldMargin = 2.0f;     ///< 0x67EA77: a target inside a shield by 2 m is shielded
constexpr float k_CellMetres = 10.0f;            ///< [0x9357C8]: a map cell
constexpr float k_SearchExtra = 20.0f;           ///< [0x8C7658]: ceil((r + 20) / 10)^2 spiral cells
constexpr float k_TribalPowerMin = 1.0f;         ///< [0x8AA390]
constexpr float k_TribalPowerMax = 5.0f;         ///< [0x8AB6E4]
constexpr int k_SpotVisualBeamFx = 36;           ///< SPOT_VISUAL BEAM_EXPLOSION_FX (0x67EE99)
constexpr int k_BeamFxTurns = 60;                ///< 0x67EE92: 60 turns
constexpr int k_SpotVisualSmoke = 23;            ///< SMOKE on dry land (0x67EEDE)
constexpr int k_SpotVisualSteam = 22;            ///< STEAM on water (0x67EEF3)
constexpr float k_SmokeScale = 8.0f;             ///< [0x9357E4]: the smoke's magnitude
constexpr float k_SmokeSeconds = 4.0f;           ///< [0x9357E8]: ftol(1000 / [0xD01A38] x 4) turns
constexpr float k_ExplodeSpread = 6.0f;          ///< 0x67EC6F: fn_00681260's fourth argument
constexpr bool k_DestroyByBeam = true;           ///< [0xC029EC] = 1: the objects are destroyed

/// UR_Explosion::CollectionData (0x58 bytes, ctor fn_0067E140)
struct CollectionData
{
	glm::vec3 centre {0.0f};   ///< +0x24: GetCurrentParentPos at the land's altitude, every step
	bool notStarted {true};    ///< +0x20: InitCollection not run yet
	bool anyTarget {false};    ///< +0x21: a target is still in the list
	float spread {0.0f};       ///< +0x30: the ring of the blast, += SpreadSpeed x dt
	struct Target
	{
		unsigned int turn {0};           ///< the game turn it was last seen available (g_game +0x205A40)
		entt::entity object {entt::null}; ///< NULL once handled
	};
	std::vector<Target> targets; ///< +0x34 GJArray (+0x3C count, grows by 10)
	int exploded {0};            ///< +0x48, up to MaxObjectsToExplode
	int deleted {0};             ///< +0x4C, up to MaxObjectsToDelete
	bool smokeDone {false};      ///< +0x50
	bool beamDone {false};       ///< +0x51
	bool finished {false};       ///< +0x52: a shield stopped the blast
	entt::entity beamFx {entt::null}; ///< +0x54: the SF_BeamExplosionFX container
};

float LandHeight(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// (port guard) the spot visuals need the game's files and the registry: not there in the unit tests
bool CanCreateSpotVisuals()
{
	return Locator::filesystem::has_value() && Locator::entitiesRegistry::has_value();
}

bool Trace()
{
	return magic::TraceEnabled();
}

/// OPENBLACK_TEST_EXPLOSION_SHOT="<turns>,<path.png>[;...]": screenshots that many game turns after the first blast's
/// first step (the beam FX starts then; InitCollection 0.4 s later), like OPENBLACK_TEST_SHIELD_SHOT
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};
bool g_BlastStarted = false;
unsigned int g_BlastTurn = 0;

std::vector<Shot>& Shots()
{
	static std::vector<Shot> shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_EXPLOSION_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			if (const auto comma = item.find(','); comma != std::string::npos)
			{
				list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
			}
		}
		return list;
	}();
	return shots;
}

void TestShots()
{
	if (!g_BlastStarted || Game::Instance() == nullptr)
	{
		return;
	}
	const unsigned int turn = magic::CurrentTurn();
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= g_BlastTurn + shot.turns)
		{
			shot.done = true;
			Game::Instance()->RequestScreenshot(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion test: screenshot {} turns after the blast (turn {}) -> {}", shot.turns,
			                   turn, shot.path);
			return;
		}
	}
}

bool IsAvailable(entt::entity object)
{
	return object != entt::null && ecs::fire::traits::IsAvailable(object);
}

/// GUtils::Spiral 0x74D7E0 (the same walk as ECS/Effects/Reactions.cpp): the next step of the square spiral
struct Spiral
{
	int dir {1};
	int count {1};
	glm::ivec2 Next()
	{
		if (--count == 0)
		{
			++dir;
			count = dir / 2;
		}
		static constexpr glm::ivec2 k_Steps[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
		return k_Steps[dir & 3];
	}
};

/// The world position of a target (MapCoords +0x14 x/z, GetAltitude + the height above the land +0x1C)
glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// UR_Explosion (DefineProperties 0x6B0B90, an AtomCreateRule): +0x2C MaxObjectsToDelete, +0x30 MaxObjectsToExplode,
/// +0x34 MaxDistance, +0x38 BlastSpeed, +0x3C SpreadSpeed, +0x40 TimeToDoEventsFor, +0x44 InitialDelay, +0x48
/// SmokeDelay, +0x4C BeamDelay. The DefineProperties ranges are editor limits; the defaults are the ctor's (0x67E090:
/// 20, 20, 100, 10, 10, 5, 3.5, 3 and 0; no file leaves one out but BeamDelay).
class Explosion final: public Modifier
{
public:
	explicit Explosion(const Object& object)
	    : maxObjectsToDelete(object.Int("MaxObjectsToDelete", 20))      // +0x2C = 0x14
	    , maxObjectsToExplode(object.Int("MaxObjectsToExplode", 20))    // +0x30 = 0x14
	    , maxDistance(object.Float("MaxDistance", 100.0f))              // +0x34 = 0x42C80000
	    , blastSpeed(object.Float("BlastSpeed", 10.0f))                 // +0x38 = 0x41200000
	    , spreadSpeed(object.Float("SpreadSpeed", 10.0f))               // +0x3C
	    , timeToDoEventsFor(object.Float("TimeToDoEventsFor", 5.0f))    // +0x40 = 0x40A00000
	    , initialDelay(object.Float("InitialDelay", 3.5f))              // +0x44 = 0x40600000
	    , smokeDelay(object.Float("SmokeDelay", 3.0f))                  // +0x48 = 0x40400000
	    , beamDelay(object.Float("BeamDelay", 0.0f))                    // +0x4C = 0
	{
	}

	/// ModifyAtomCollection 0x67ECE0
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = DataOf(this, collection);
		if (!g_BlastStarted)
		{
			g_BlastStarted = true; // OPENBLACK_TEST_EXPLOSION_SHOT counts from the first blast's first step
			g_BlastTurn = magic::CurrentTurn();
		}
		// GetCurrentParentPos 0x674AF0 (the parent atom's +0x80, or the origin), then y = the land's altitude there
		data.centre = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		data.centre.y = LandHeight(data.centre.x, data.centre.z);
		const float age = effect.CollectionAge(collection);
		if (!data.finished && !effect.Closing() && data.notStarted && age > initialDelay)
		{
			InitCollection(effect, data);
			data.notStarted = false;
		}
		if (!data.finished && !effect.Closing())
		{
			// the blast itself: a SpellEvent 2 at the centre every step for TimeToDoEventsFor after the delay (the spell's
			// default event: its burn / crush / hit in the effect radius, ApplyEffectToMapPos)
			if (!data.notStarted && age < initialDelay + timeToDoEventsFor)
			{
				SpellEventInfo event;
				event.type = SpellEventInfo::Point;
				event.position = data.centre;
				event.velocity = glm::vec3(0.0f);
				event.strength = 1.0f;
				event.checkShields = false;
				event.target = entt::null;
				effect.SendSpellEvent(event);
			}
			if (!data.beamDone && age > beamDelay && CanCreateSpotVisuals())
			{
				// CreateSpotVisualWithSpecifiedDuration(centre, BEAM_EXPLOSION_FX, 1.0, 60 turns, NULL): the column and cones
				data.beamFx = manager::CreateSpotVisual(k_SpotVisualBeamFx, data.centre, static_cast<float>(k_BeamFxTurns) * 0.1f,
				                                        entt::null, 1.0f);
				data.beamDone = true;
			}
			if (!data.smokeDone && age > smokeDelay && CanCreateSpotVisuals())
			{
				data.smokeDone = true;
				// MapCoords::IsDryLand 0x603620: smoke, else steam; magnitude 8 for 4 s
				const int visual = ecs::pot_resource::IsDryLand(data.centre) ? k_SpotVisualSmoke : k_SpotVisualSteam;
				manager::CreateSpotVisual(visual, data.centre, k_SmokeSeconds, entt::null, k_SmokeScale);
			}
		}
		else
		{
			// stopped or closing: GParticleContainer::CloseDown 0x63E370 on the FX, and the target list emptied
			if (data.beamFx != entt::null && Locator::entitiesRegistry::has_value())
			{
				manager::CloseSpotVisual(data.beamFx);
				data.beamFx = entt::null;
			}
			data.targets.clear();
		}
		Update(effect, data);
		return true;
	}

	int maxObjectsToDelete, maxObjectsToExplode;
	float maxDistance, blastSpeed, spreadSpeed, timeToDoEventsFor, initialDelay, smokeDelay, beamDelay;

private:
	/// The CollectionData of this modifier in that collection (the original's +0x24 list, Collection::modifierData)
	static CollectionData& DataOf(const Modifier* self, Collection& collection)
	{
		auto& slot = collection.modifierData[self];
		if (slot == nullptr)
		{
			slot = std::make_shared<CollectionData>();
		}
		return *std::static_pointer_cast<CollectionData>(slot);
	}

	/// InitCollection 0x67E200
	void InitCollection(Effect& effect, CollectionData& data) const
	{
		const auto* sink = effect.GetSink();
		const entt::entity spell = sink != nullptr ? sink->SpellEntity() : entt::null;
		const bool hasSpell = spell != entt::null && Locator::entitiesRegistry::value().Valid(spell);
		// the margin: the spell's GMagicEffectInfo radius (+0x2C in memory = the file's 0x1C; BEAM 5, 5, 10), else 5
		const float margin = hasSpell ? magic::EffectInfoOf(spell).radius : k_DefaultRadius;
		// fn_006D0BC0: inside a shield, the blast hits the shield: its point is where a ray from 200 m above meets the
		// sphere (vt 0xFC FindIntersect), a spark there (fn_006D0AF0), and a SpellEvent 4 at the centre with the shield's
		// spell as the target (fn_006D0B10). A 0 (the shield held) stops the blast for good.
		if (const auto* sphere = shields::FindShieldContainingPoint(data.centre, margin); sphere != nullptr)
		{
			glm::vec3 impact = data.centre;
			shields::FindIntersect(*sphere, data.centre + glm::vec3(0.0f, k_ShieldRayHeight, 0.0f), data.centre, margin, impact);
			shields::AddImpactTarget(*sphere, impact);
			SpellEventInfo event;
			event.type = SpellEventInfo::HitSpell;
			event.position = data.centre;
			event.velocity = glm::vec3(0.0f);
			event.strength = 1.0f;
			event.checkShields = false;
			event.target = shields::SpellOf(*sphere);
			if (effect.SendSpellEvent(event) == 0)
			{
				data.finished = true;
			}
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion: blast at ({:.1f}, {:.1f}) inside a shield, {}", data.centre.x,
				                   data.centre.z, data.finished ? "stopped" : "went through");
			}
		}
		data.targets.clear(); // fn_00681F10(0)
		if (data.finished || effect.Closing())
		{
			return;
		}
		// 0x67E347..0x67E55B: three water rings (PSys/PSysWaterRings, the one implementation: growth [0x9357D8] = 10 x 0.5,
		// x 0.7 and x 1, angle 0, aspect and rate 1, cell 0x30, 0xFFFFFFFF), or on dry land (MapCoords::IsDryLand, altitude
		// >= 4) the scorch mark
		if (!water_rings::AddExplosionRings(data.centre))
		{
			// 0x67E35C..0x67E395: fn_008251C0(centre, PSysFloatRand(2 pi) (0x40C90FDB), [0x9357D4] = 8, mesh 0x251), a
			// ground mark that melts into the land and fades after 15 s (ecs/GroundMarks.h; its SmokyStuff is not made)
			const float angle = effect.Random(6.28318548f);
			const auto mark = ecs::ground_marks::CreateExplosionMark(data.centre, angle);
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion: ground mark {} at ({:.1f}, {:.1f}), angle {:.2f}",
				                   static_cast<uint32_t>(mark), data.centre.x, data.centre.z, angle);
			}
		}
		// the targets: every available object of the ceil((r + 20) / 10)^2 cells of the spiral around the centre that is
		// counted in that cell (fn_00604F40: its own cell) and nearer than its Get2DRadius + r in x/z
		// (GUtils::GetDistanceInMetres 0x74CD70, a hypotenuse of dx, dz); r = MaxDistance x the tribal power (1..5)
		float r = maxDistance;
		if (hasSpell)
		{
			r *= std::clamp(magic::GetTribalPower(spell), k_TribalPowerMin, k_TribalPowerMax);
		}
		const int side = static_cast<int>(std::ceil((r + k_SearchExtra) / k_CellMetres));
		const int cells = side * side;
		const unsigned int turn = magic::CurrentTurn();
		if (Locator::entitiesMap::has_value())
		{
			const auto& map = Locator::entitiesMap::value();
			const auto& registry = Locator::entitiesRegistry::value();
			glm::ivec2 cell = glm::ivec2(ecs::MapInterface::GetGridCell(glm::vec2(data.centre.x, data.centre.z)));
			Spiral spiral;
			for (int n = 0; n < cells; ++n)
			{
				if (cell.x >= 0 && cell.y >= 0 && cell.x < ecs::MapInterface::k_GridSize.x && cell.y < ecs::MapInterface::k_GridSize.y)
				{
					const ecs::MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
					for (const auto* list : {&map.GetMobileInGridCell(id), &map.GetFixedInGridCell(id)})
					{
						for (const auto object : *list)
						{
							if (!registry.Valid(object) || !IsAvailable(object))
							{
								continue;
							}
							const auto p = PositionOf(object);
							if (ecs::MapInterface::GetGridCell(glm::vec2(p.x, p.z)) != id)
							{
								continue;
							}
							const float d = glm::length(glm::vec2(p.x - data.centre.x, p.z - data.centre.z));
							if (d < ecs::fire::traits::Radius(object) + r)
							{
								data.targets.push_back({turn, object});
								data.anyTarget = true;
							}
						}
					}
				}
				cell += spiral.Next();
			}
		}
		data.spread = 0.0f;
		// (no portado) five MSH_Z_SPELLROCK01 (567) rocks at centre + (rand(-4, 4), 0, rand(-4, 4)), a random Y angle and
		// scale rand(0.8, 1.2), thrown to pieces from 5 m under the centre at BlastSpeed (fn_006812B0 -> the
		// UR_ExplodeObject queue 0xD4E320, drawn by SF_ExplodeObject; UR_ExplodeObject::ExplodeMesh 0x6807B0)
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Explosion: started at ({:.1f}, {:.1f}, {:.1f}), margin {:.1f}, search r {:.1f} ({} cells), {} "
			                   "targets; the five rock pieces (ExplodeMesh 0x6807B0) are not ported",
			                   data.centre.x, data.centre.y, data.centre.z, margin, r, cells, data.targets.size());
			for (const auto& target : data.targets)
			{
				const auto p = PositionOf(target.object);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion:   target {} at ({:.1f}, {:.1f}) d {:.1f} radius {:.1f}{}{}{}",
				                   static_cast<uint32_t>(target.object), p.x, p.z,
				                   glm::length(glm::vec2(p.x - data.centre.x, p.z - data.centre.z)),
				                   ecs::fire::traits::Radius(target.object),
				                   Locator::entitiesRegistry::value().AllOf<ecs::components::Tree>(target.object) ? " tree" : "",
				                   Locator::entitiesRegistry::value().AllOf<ecs::components::Villager>(target.object) ? " villager" : "",
				                   Locator::entitiesRegistry::value().AnyOf<ecs::components::Abode, ecs::components::StoragePit>(target.object)
				                       ? " abode"
				                       : "");
			}
		}
	}

	/// fn_0067E900 (the symbol RecursiveUpdateForkStructure@UR_Lightning is wrong: it is UR_Explosion's update, called
	/// at the end of every ModifyAtomCollection)
	void Update(Effect& effect, CollectionData& data) const
	{
		TestShots();
		data.spread += effect.GetDt() * spreadSpeed; // [0xD4E0EC] x SpreadSpeed
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (data.beamFx != entt::null && !registry.Valid(data.beamFx))
		{
			data.beamFx = entt::null;
		}
		const unsigned int turn = magic::CurrentTurn();
		data.anyTarget = false;
		for (auto& target : data.targets)
		{
			if (target.object != entt::null)
			{
				if (IsAvailable(target.object))
				{
					target.turn = turn;
				}
				else
				{
					target.object = entt::null;
				}
			}
			if (target.object != entt::null)
			{
				data.anyTarget = true;
			}
		}
		if (data.exploded >= maxObjectsToExplode && data.deleted >= maxObjectsToDelete)
		{
			data.anyTarget = false;
		}
		if (!data.anyTarget)
		{
			data.targets.clear();
		}
		const auto refresh = [&](CollectionData::Target& target) {
			if (target.object != entt::null)
			{
				if (IsAvailable(target.object))
				{
					target.turn = turn;
				}
				else
				{
					target.object = entt::null;
				}
			}
		};
		// one object per step: the first that answers the CanBeDestroyed query stops the walk
		bool stop = false;
		for (size_t i = 0; i < data.targets.size() && !stop; ++i)
		{
			auto& target = data.targets[i];
			// +0x40: only objects with a 3D object (a mesh)
			if (target.object == entt::null || !registry.AllOf<ecs::components::Mesh>(target.object))
			{
				continue;
			}
			glm::vec3 p = PositionOf(target.object);
			const glm::vec3 d = p - data.centre;
			// reached by the ring: |p - centre|^2 <= (GetRadius (vt 0x60) + spread)^2, in 3D
			const float reach = ecs::fire::traits::Radius(target.object) + data.spread;
			if (reach * reach < glm::dot(d, d))
			{
				continue;
			}
			if (const auto* sphere = shields::FindShieldContainingPoint(p, k_TargetShieldMargin); sphere != nullptr)
			{
				shields::AddImpactTarget(*sphere, p);
				SpellEventInfo event;
				event.type = SpellEventInfo::HitSpell;
				event.position = p;
				event.velocity = p - data.centre;
				event.strength = 1.0f;
				event.checkShields = false;
				event.target = shields::SpellOf(*sphere);
				if (effect.SendSpellEvent(event) == 0)
				{
					target.object = entt::null;
				}
			}
			refresh(target);
			// GetActualObjectToEffect (vt 0x5D8, the spell's player, 1): the object itself (the CitadelHeart / CitadelPart
			// redirection 0x468C30 / 0x469780 is not ported)
			if (target.object == entt::null)
			{
				continue;
			}
			p = PositionOf(target.object);
			SpellEventInfo query;
			query.type = SpellEventInfo::CanDestroy;
			query.position = p;
			query.velocity = glm::vec3(0.0f);
			query.strength = 1.0f;
			query.checkShields = false;
			query.target = target.object;
			if (effect.SendSpellEvent(query) == 1)
			{
				stop = true;
				refresh(target);
				if (target.object != entt::null && !ecs::fire::traits::IsCreature(target.object) &&
				    data.exploded < maxObjectsToExplode)
				{
					++data.exploded;
					// (no portado) fn_00681260(object, centre - 5 m, BlastSpeed, 6, 0): its mesh thrown to pieces
					// (fn_006812B0 -> the UR_ExplodeObject queue 0xD4E320; UR_ExplodeObject::ExplodeMesh 0x6807B0)
					static_cast<void>(k_ExplodeSpread);
				}
				if (target.object != entt::null && data.deleted < maxObjectsToDelete)
				{
					++data.deleted;
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"),
						                   "Explosion: object {} at ({:.1f}, {:.1f}) destroyed by the beam (spread {:.1f}, {} exploded, {} "
						                   "deleted)",
						                   static_cast<uint32_t>(target.object), p.x, p.z, data.spread, data.exploded, data.deleted);
					}
					if (k_DestroyByBeam)
					{
						explosion::DestroyedByBeam(target.object);
					}
				}
			}
			target.object = entt::null;
		}
	}

};

/// SetPSysCloseDown::ModifyAtomCore 0x6A26D0 (DefineProperties 0x6ACC60: the base ones only): PSysManager::SetState(1)
/// 0x672FF0, the effect closes, for every atom that passes the condition
class SetPSysCloseDown final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& /*atom*/, Collection::Slot& /*slot*/) const override
	{
		effect.CloseDown();
		return true;
	}
};

/// UR_ChangeScaleXYZ::ModifyAtomCore 0x6A5240 (DefineProperties 0x6ADE60: +0x20 StartTime, +0x24 StopTime, +0x28
/// StartScaleXZ, +0x2C StopScaleXZ, +0x30 StartScaleY, +0x34 StopScaleY)
class ChangeScaleXYZ final: public Modifier
{
public:
	explicit ChangeScaleXYZ(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 0.0f))
	    , startXZ(object.Float("StartScaleXZ", 0.0f))
	    , stopXZ(object.Float("StopScaleXZ", 0.0f))
	    , startY(object.Float("StartScaleY", 0.0f))
	    , stopY(object.Float("StopScaleY", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		explosion::ChangeScaleXYZ(effect.AtomAge(atom), effect.GetDt(), startTime, stopTime, startXZ, stopXZ, startY, stopY,
		                          atom.ruleScale, atom.stretch);
		return true;
	}
	float startTime, stopTime, startXZ, stopXZ, startY, stopY;
};

/// UR_MoveAtom::ModifyAtomCore 0x6A5E50 (DefineProperties 0x6AE240: +0x20 StartTime, +0x24 StopTime, +0x28 MoveSmoothly,
/// +0x2C..+0x34 StartX/Y/Z, +0x38..+0x40 StopX/Y/Z): the atom's +0x80 (its position in its collection's frame)
class MoveAtom final: public Modifier
{
public:
	explicit MoveAtom(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 0.0f))
	    , smoothly(object.Bool("MoveSmoothly", false))
	    , start(object.Float("StartX", 0.0f), object.Float("StartY", 0.0f), object.Float("StartZ", 0.0f))
	    , stop(object.Float("StopX", 0.0f), object.Float("StopY", 0.0f), object.Float("StopZ", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		explosion::MoveAtom(effect.AtomAge(atom), effect.GetDt(), startTime, stopTime, smoothly, start, stop, atom.position);
		return true;
	}
	float startTime, stopTime;
	bool smoothly;
	glm::vec3 start, stop;
};
} // namespace

bool explosion::ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ,
                               float startY, float stopY, float& ruleScale, float& stretch)
{
	if (age < startTime)
	{
		return false;
	}
	float xz = stopXZ;
	float y = stopY;
	if (age <= stopTime)
	{
		// (port guard) StopTime == StartTime would divide by 0: taken as the end
		const float t = stopTime > startTime ? (age - startTime) / (stopTime - startTime) : 1.0f;
		xz = startXZ + (stopXZ - startXZ) * t;
		y = startY + (stopY - startY) * t;
	}
	else if (age - dt > stopTime)
	{
		return false; // [0xD4E0EC]: only the first step after StopTime writes the stop values
	}
	ruleScale = xz;
	// [0x8BF518] = 0.0001: a flat XZ has no stretch
	stretch = xz > 0.0001f ? y / xz : 0.0f;
	return true;
}

bool explosion::MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
                         const glm::vec3& stop, glm::vec3& out)
{
	if (age < startTime || age > stopTime)
	{
		return false;
	}
	float t = stopTime > startTime ? (age - startTime) / (stopTime - startTime) : 1.0f; // (port guard) as above
	if (dt + age >= stopTime)
	{
		t = 1.0f; // the last step lands on the stop point
	}
	if (smoothly)
	{
		t = t * t * (3.0f - 2.0f * t); // [0x8C2C50] = 3
	}
	out = start + (stop - start) * t;
	return true;
}

bool explosion::CanBeDestroyedBySpell(entt::entity object, entt::entity /*spell*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	using namespace ecs::components;
	// the classes that say no: Creature 0x47B1E0, Field 0x529FF0, CitadelPart 0x4695D0 (the citadel = openblack's Temple
	// and its parts, the worship sites and their totems)
	if (registry.AnyOf<Creature, Field, Temple, TempleInteriorPart, CitadelWorship, WorshipSite, WorshipTotem>(object))
	{
		return false;
	}
	// Object 0x639960: IsEffectReceiver(NULL) (vt 0x774) == 1. (inferido) Object +0x25 & 0x40 and the script test
	// (IsInScript vt 0x448 with g_game +0x25005C -> +0x45E8 / +0x45EC set, then the spell's +0x25 & 4) are not tracked by
	// openblack: taken as clear / not in a script
	const ecs::effects::EffectValues none;
	return ecs::effects::IsEffectReceiver(object, none);
}

void explosion::DestroyedByBeam(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	using namespace ecs::components;
	if (registry.AnyOf<Abode, StoragePit, SpellDispenser, TotemStatue, Field>(object))
	{
		// Abode::DestroyedByBeam 0x402CB0: ReduceLife(GetLife(0)) (vt 0x5B8 / 0x11C). (aproximado) Abode::ReduceLife
		// 0x405D90 (the repair site, the ghost at 0) is not ported: the building's life goes to 0 and it stays
		ecs::life::ReduceLife(object, ecs::life::LifeOf(object));
		return;
	}
	// Object::DestroyedByBeam 0x63AB20: ToBeDeleted(0) (vt 0xC)
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		ecs::DeleteTree(object); // Tree::ToBeDeleted 0x74A210 / DeadTree 0x510C90
		return;
	}
	if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::Remove(object);
		return;
	}
	if (registry.AllOf<Villager>(object))
	{
		// (aproximado) Villager::ToBeDeleted takes it out of its town and the world; life::Kill does that here
		ecs::life::Kill(object, "destroyed by the beam");
		return;
	}
	// the rest (rocks, mobile objects and statics, features, piles): Object::ToBeDeleted, as DestroyedByEffect's
	ecs::fire::traits::DestroyedByEffect(object);
}

void openblack::psys::RegisterExplosionRules()
{
	RegisterModifier("UR_Explosion", MakeModifierOf<Explosion>);
	RegisterModifier("SetPSysCloseDown", MakeModifierOf<SetPSysCloseDown>);
	RegisterModifier("UR_ChangeScaleXYZ", MakeModifierOf<ChangeScaleXYZ>);
	RegisterModifier("UR_MoveAtom", MakeModifierOf<MoveAtom>);
}
