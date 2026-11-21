/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleDestructionSystem.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "3D/ModelSurface.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "GameTempleDestructionWorld.h"
#include "Particles/PlasmaCommand.h"
#include "Physics/TempleHeart.h"
#include "Temple/TempleDestruction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace td = openblack::temple_destruction;
namespace temple_heart = openblack::physics::temple_heart;

namespace
{
/// The sound that loops over the temple until the explosion, and the explosion's, of the game's sounds
constexpr int k_LoopSample = 171;
constexpr int k_ExplosionSample = 168;
/// A turn's milliseconds as seconds
constexpr float k_SecondsPerMillisecond = 0.001f;
/// The script the game starts when the local player's temple is being destroyed
constexpr std::string_view k_GameOverScript = "GameOver";

/// The heart's temple, which its sounds play from; none when it has none
entt::entity CitadelOf(Registry& registry, entt::entity heart)
{
	const auto* part = registry.TryGet<const CitadelHeart>(heart);
	return part != nullptr && part->citadel != entt::null && registry.Valid(part->citadel) ? part->citadel : entt::null;
}
} // namespace

TempleDestructionSystem::TempleDestructionSystem()
    : TempleDestructionSystem(std::make_unique<GameTempleDestructionWorld>())
{
}

TempleDestructionSystem::TempleDestructionSystem(std::unique_ptr<temple_world::World> world)
    : _world(std::move(world))
{
}

TempleDestructionSystem::~TempleDestructionSystem() = default;

void TempleDestructionSystem::Start(entt::entity heart)
{
	auto& registry = _world->Entities();
	auto* temple = registry.Valid(heart) ? registry.TryGet<Temple>(heart) : nullptr;
	if (temple == nullptr)
	{
		return;
	}
	// Started again, as by a heart healed and destroyed anew while it goes, its clock starts again from nothing
	temple->destroying = true;
	temple->destructionClock = 0.0f;
	_world->DrawHeart(heart, true);
	// Every other part of its temple goes at once, the newest first, each leaving the temple's parts as it goes. A part
	// that would come again is not deleted twice
	entt::entity last = entt::null;
	for (;;)
	{
		const auto parts = _world->PartsOf(heart);
		const auto other = std::ranges::find_if(parts, [heart](entt::entity part) { return part != heart; });
		if (other == parts.end() || *other == last)
		{
			break;
		}
		last = *other;
		_world->ToBeDeleted(*other);
	}
}

void TempleDestructionSystem::DeleteByScript(entt::entity heart)
{
	auto& registry = _world->Entities();
	const auto* temple = registry.Valid(heart) ? registry.TryGet<const Temple>(heart) : nullptr;
	if (temple == nullptr)
	{
		return;
	}
	// A heart already being destroyed only has its model broken up, and stays
	if (temple->destroying)
	{
		_world->BreakUpModel(heart);
		return;
	}
	Start(heart);
}

void TempleDestructionSystem::Beam(entt::entity heart, Temple& temple, glm::vec3 position)
{
	if (!td::Beaming(temple.destructionClock))
	{
		return;
	}
	// As many beams as the beam clock falls behind the clock, each between two random points of the heart's model
	// facing no more than a little down, leaving along the first's normal and arriving against the second's. The points
	// are drawn even when there is nothing to fire the beam from
	std::vector<model_surface::Triangle> triangles;
	bool triangulated = false;
	while (temple.destructionBeamClock < temple.destructionClock)
	{
		const float share = td::BeamShare(temple.destructionClock);
		temple.destructionBeamClock = td::NextBeam(temple.destructionBeamClock, share);
		if (temple.destructionBeamSource == entt::null)
		{
			temple.destructionBeamSource = _world->StartSpotVisual(
			    SpotVisualType::MagicBeamOnCitadel, position, td::TurnsFor(_world->MillisecondsPerTurn(), td::k_BeamsOver));
			if (temple.destructionBeamSource != entt::null)
			{
				_world->SetSpotVisualPlayer(temple.destructionBeamSource, temple.owner);
			}
		}
		auto* random = _world->LocalRandom();
		if (random == nullptr)
		{
			continue;
		}
		if (!triangulated)
		{
			triangles = _world->DrawnTrianglesOf(heart);
			triangulated = true;
		}
		const auto placement = _world->PlacementOf(heart);
		const auto from = model_surface::RandomUpwardPoint(triangles, placement, *random);
		const auto to = model_surface::RandomUpwardPoint(triangles, placement, *random);
		if (temple.destructionBeamSource == entt::null || !from.has_value() || !to.has_value())
		{
			continue;
		}
		const auto look = td::BeamLookAt(share);
		_world->AddPlasma(temple.destructionBeamSource, {
		                                                    .start = from->position,
		                                                    .end = to->position,
		                                                    .startTangent = from->normal,
		                                                    .endTangent = -to->normal,
		                                                    .life = look.life,
		                                                    .speed = look.speed,
		                                                    .alpha = look.alpha,
		                                                });
	}
}

bool TempleDestructionSystem::Step(entt::entity heart, Temple& temple)
{
	auto& registry = _world->Entities();
	const auto citadel = CitadelOf(registry, heart);
	const auto millisecondsPerTurn = _world->MillisecondsPerTurn();
	const float before = temple.destructionClock;
	const float seconds = static_cast<float>(millisecondsPerTurn) * k_SecondsPerMillisecond;
	temple.destructionClock = seconds + temple.destructionClock;
	const auto events = td::Between(before, temple.destructionClock);
	const auto position = registry.Get<const Transform>(heart).position;
	const bool local = _world->LocalPlayer() == temple.owner;
	// The local player's heartbeat quickens with the clock, held there until the end
	if (local)
	{
		_world->HoldHeartbeat(td::HeartbeatPace(temple.destructionClock));
	}
	// The heart fades out from fourteen seconds, under a shell shown over it until the smoke; from a little after the
	// explosion it is no longer drawn, nor found under the cursor, until it goes
	_world->FadeHeart(heart, td::HeartAlpha(temple.destructionClock));
	_world->DrawHeart(heart, td::HeartDrawn(true, temple.destructionClock));
	if (events.shellShown)
	{
		_world->ShowShell(heart, true);
	}
	if (events.shellGoes)
	{
		_world->ShowShell(heart, false);
	}
	// From the first turn the loop sounds over the temple, a new one each time the clock starts again, and the beams'
	// clock starts with the clock
	if (events.loopStarts)
	{
		temple.destructionBeamClock = temple.destructionClock;
		_world->TagSound(citadel, k_LoopSample, true);
	}
	Beam(heart, temple, position);
	// The glow over the heart, acting on each of its temple's parts
	if (events.glow && citadel != entt::null)
	{
		temple.destructionGlow = _world->StartSpotVisual(SpotVisualType::MagicFxOnCitadel, position,
		                                                 td::TurnsFor(millisecondsPerTurn, td::k_GlowSeconds));
		if (temple.destructionGlow != entt::null)
		{
			_world->SetSpotVisualPlayer(temple.destructionGlow, temple.owner);
			for (const auto part : _world->PartsOf(heart))
			{
				_world->AddSpotVisualTarget(temple.destructionGlow, part);
			}
		}
	}
	// While it lasts, the local player's glow beats with their heart
	if (temple.destructionGlow != entt::null && local)
	{
		_world->PowerGlowByHeartbeat(temple.destructionGlow);
	}
	// The loops over the temple stop, and the explosion sounds and is seen
	if (events.explosion)
	{
		_world->RemoveSoundTags(citadel, k_LoopSample);
		_world->TagSound(citadel, k_ExplosionSample, false);
		const auto explosion = _world->StartSpotVisual(SpotVisualType::ExplosionCitadel, position,
		                                               td::TurnsFor(millisecondsPerTurn, td::k_ExplosionSeconds));
		if (explosion != entt::null)
		{
			_world->SetSpotVisualPlayer(explosion, temple.owner);
		}
	}
	// The smoke lasts a random time and is made at the usual scale, then ten times as big
	if (events.smoke)
	{
		const float share = _world->GameFloatRand(td::k_SmokeShareSpread);
		const auto smoke =
		    _world->StartSpotVisual(SpotVisualType::EvilSmoke, position, td::SmokeTurns(millisecondsPerTurn, share));
		if (smoke != entt::null)
		{
			_world->SetSpotVisualScale(smoke, td::k_SmokeMagnitude);
		}
	}
	// The end: the heartbeat is let go, the heart is drawn whole and without its shell, and goes
	if (events.end)
	{
		temple.destroying = false;
		temple.destructionClock = 0.0f;
		if (local)
		{
			_world->HoldHeartbeat(0.0f);
		}
		_world->FadeHeart(heart, td::HeartAlpha(0.0f));
		_world->ShowShell(heart, false);
		_world->DrawHeart(heart, true);
		return true;
	}
	return false;
}

bool TempleDestructionSystem::BeamDue(Temple& temple, entt::entity target)
{
	const auto interval = temple_heart::BeamInterval(_world->MillisecondsPerTurn());
	temple_heart::Beam beam {.target = temple.beamTarget, .turn = temple.beamTurn};
	const bool due = target != entt::null ? temple_heart::BeamAtTargetDue(beam, target, _world->Turn(), interval)
	                                      : temple_heart::BeamAtItselfDue(beam, _world->Turn(), interval);
	temple.beamTarget = beam.target;
	temple.beamTurn = beam.turn;
	return due;
}

entt::entity TempleDestructionSystem::BeamSource(entt::entity heart, Temple& temple)
{
	// Made the first time it is wanted, at the heart's place for ever, for the heart's player
	if (temple.beamSource == entt::null)
	{
		temple.beamSource = _world->StartSpotVisual(SpotVisualType::MagicBeamOnCitadel,
		                                            _world->Entities().Get<const Transform>(heart).position, -1);
		if (temple.beamSource != entt::null)
		{
			_world->SetSpotVisualPlayer(temple.beamSource, temple.owner);
		}
	}
	return temple.beamSource;
}

glm::vec3 TempleDestructionSystem::TopOf(entt::entity thing) const
{
	auto top = _world->MapPointOf(thing);
	top.y = _world->HeightOf(thing) + top.y;
	return top;
}

void TempleDestructionSystem::BeamAtTarget(entt::entity heart, Temple& temple, entt::entity target)
{
	if (!BeamDue(temple, target))
	{
		return;
	}
	const auto source = BeamSource(heart, temple);
	const auto start = TopOf(heart);
	const auto end = TopOf(target);
	if (source == entt::null)
	{
		return;
	}
	// One of the five spark sounds in turn, then the beam from the top of the heart to the top of what takes the blow
	_world->PlaySpark(heart, false);
	_world->AddPlasma(source, {
	                              .start = start,
	                              .end = end,
	                              .startTangent = particles::k_HeartPlasmaStartTangent,
	                              .endTangent = particles::k_HeartPlasmaEndTangent,
	                              .life = particles::k_HeartPlasmaLife,
	                              .speed = particles::k_HeartPlasmaSpeed,
	                              .alpha = particles::k_HeartPlasmaAlpha,
	                          });
}

void TempleDestructionSystem::BeamAtItself(entt::entity heart, Temple& temple)
{
	if (!BeamDue(temple, entt::null))
	{
		return;
	}
	const auto source = BeamSource(heart, temple);
	const auto start = TopOf(heart);
	// The beam ends at a random point of the heart's model facing no more than a little down, arriving against the
	// surface; the point is drawn even when there is nothing to fire the beam from
	auto* random = _world->LocalRandom();
	if (random == nullptr)
	{
		return;
	}
	const auto point = model_surface::RandomUpwardPoint(_world->DrawnTrianglesOf(heart), _world->PlacementOf(heart), *random);
	if (source == entt::null || !point.has_value())
	{
		return;
	}
	_world->AddPlasma(source, {
	                              .start = start,
	                              .end = point->position,
	                              .startTangent = particles::k_HeartPlasmaStartTangent,
	                              .endTangent = -point->normal,
	                              .life = particles::k_HeartPlasmaLife,
	                              .speed = particles::k_HeartPlasmaSpeed,
	                              .alpha = particles::k_HeartPlasmaAlpha,
	                          });
}

entt::entity TempleDestructionSystem::ActualObjectToEffect(entt::entity heart, bool breaksBuildings)
{
	auto& registry = _world->Entities();
	auto* temple = registry.Valid(heart) ? registry.TryGet<Temple>(heart) : nullptr;
	if (temple == nullptr)
	{
		return heart;
	}
	// Over the towns of the heart's player; while what strikes it breaks buildings, it beams at what it chose
	const auto target = temple_heart::Choose(_world->HeartTowns(temple->owner));
	if (target.kind == temple_heart::TargetKind::Heart)
	{
		if (breaksBuildings)
		{
			BeamAtItself(heart, *temple);
		}
		return heart;
	}
	if (breaksBuildings)
	{
		BeamAtTarget(heart, *temple, target.entity);
	}
	return target.entity;
}

void TempleDestructionSystem::TransferredDamageEffect(entt::entity heart, Temple& temple)
{
	// Only a heart with some life lost, and one with none left at its fiercest; only while it would take a blow itself
	const auto life = temple_heart::DamageEffectLife(_world->LifeOf(heart));
	if (!life.has_value() || ActualObjectToEffect(heart, false) != heart)
	{
		return;
	}
	const auto interval = temple_heart::DamageInterval(_world->MillisecondsPerTurn(), *life);
	if (!temple_heart::DamageBeamDue(temple.beamTurn, _world->Turn(), interval))
	{
		return;
	}
	const auto source = BeamSource(heart, temple);
	// Between two random points of the heart's model facing no more than a little down, leaving along the first's
	// normal and arriving against the second's; the points are drawn even when there is nothing to fire from
	auto* random = _world->LocalRandom();
	if (random == nullptr)
	{
		return;
	}
	const auto triangles = _world->DrawnTrianglesOf(heart);
	const auto placement = _world->PlacementOf(heart);
	const auto from = model_surface::RandomUpwardPoint(triangles, placement, *random);
	const auto to = model_surface::RandomUpwardPoint(triangles, placement, *random);
	if (source == entt::null)
	{
		return;
	}
	// With little life left, one of the five spark sounds in turn
	if (temple_heart::DamageBeamSounds(*life))
	{
		_world->PlaySpark(heart, true);
	}
	if (!from.has_value() || !to.has_value())
	{
		return;
	}
	const auto look = temple_heart::DamageLookAt(*life);
	_world->AddPlasma(source, {
	                              .start = from->position,
	                              .end = to->position,
	                              .startTangent = from->normal,
	                              .endTangent = -to->normal,
	                              .life = look.life,
	                              .speed = particles::k_HeartPlasmaSpeed,
	                              .alpha = look.alpha,
	                          });
}

void TempleDestructionSystem::ProcessHeart(entt::entity heart)
{
	auto& registry = _world->Entities();
	auto* temple = registry.Valid(heart) ? registry.TryGet<Temple>(heart) : nullptr;
	if (temple == nullptr)
	{
		return;
	}
	// The beams' and the glow's spot visuals are let go once they have ended, every turn
	if (temple->destructionBeamSource != entt::null && !_world->SpotVisualGoing(temple->destructionBeamSource))
	{
		temple->destructionBeamSource = entt::null;
	}
	if (temple->destructionGlow != entt::null && !_world->SpotVisualGoing(temple->destructionGlow))
	{
		temple->destructionGlow = entt::null;
	}
	// Only a heart being destroyed that still has its temple moves on; one whose destruction ends goes at the end of
	// its turn
	const bool ends = temple->destroying && CitadelOf(registry, heart) != entt::null && Step(heart, *temple);
	// The beams' source for blows and hurts is let go once it has ended, and what it beamed at once that has gone
	if (temple->beamSource != entt::null && !_world->SpotVisualGoing(temple->beamSource))
	{
		temple->beamSource = entt::null;
	}
	if (temple->beamTarget != entt::null && !_world->Available(temple->beamTarget))
	{
		temple->beamTarget = entt::null;
		temple->beamTurn = 0;
	}
	TransferredDamageEffect(heart, *temple);
	if (ends)
	{
		_world->ToBeDeleted(heart);
	}
}

void TempleDestructionSystem::EndTurn()
{
	auto& registry = _world->Entities();
	const auto local = _world->LocalPlayer();
	if (!local.has_value())
	{
		return;
	}
	// The local player whose temple's heart is being destroyed has lost the game: the game's end is played out by its
	// script, once. The players have always taken a turn by the end of one
	const auto heart = _world->CitadelHeartOf(*local);
	const auto* temple = heart != entt::null && registry.Valid(heart) ? registry.TryGet<const Temple>(heart) : nullptr;
	const bool destroying = temple != nullptr && temple->destroying;
	auto& context = registry.Context();
	if (td::GameOverStarts({
	        .over = context.gameOver,
	        .skirmish = _world->PlaygroundGame(),
	        .multiplayer = false,
	        .localTempleDestroying = destroying,
	    }))
	{
		context.gameOver = true;
		_world->StartScript(k_GameOverScript);
	}
}
