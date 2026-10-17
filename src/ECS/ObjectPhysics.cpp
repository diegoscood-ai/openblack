/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectPhysics.h"

#include <cmath>

#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/Physics/Hitter.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/ObjectRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// info.dat's mobile static mobileType of the Rock class
constexpr int k_RockMobileType = 2;
} // namespace

bool object_physics::IsRock(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type));
	return static_cast<int>(info.mobileType) == k_RockMobileType;
}

float object_physics::RockRadius2D(entt::entity entity)
{
	return object::Get2DRadius(entity);
}

float object_physics::RockHeight(entt::entity entity)
{
	return object::GetHeight(entity);
}

bool object_physics::RockValidForPlaceInHand(entt::entity entity)
{
	return RockRadius2D(entity) <= 3.6f;
}

bool object_physics::CanTapRock(entt::entity entity)
{
	return openblack::physics::objects::RockBreaksWhenTapped(RockHeight(entity));
}

std::array<entt::entity, 2> object_physics::TapRock(entt::entity entity, glm::vec3 handPosition)
{
	const auto halves = SplitRock(entity);
	// after SplitRock: bank InGame, sample 130 G_RockTap_01 + a counter (0..3 in turn), owner the rock, 3D, not
	// tracked, at the hand's point
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 130 + audio::NextCounter(audio::Counter::RockTap)};
	options.owner = audio::Owner::Thing(entity);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
	// TODO: the player's creature empathises with CREATURE_DESIRE_TO_PLAY (0.5, at the rock) once creatures have desires
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: tapped at ({:.1f}, {:.1f}, {:.1f})", handPosition.x, handPosition.y,
	                   handPosition.z);
	return halves;
}

std::array<entt::entity, 2> object_physics::SplitRock(entt::entity entity, glm::vec3 velocity, glm::vec3 angularMomentum)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	const auto type = registry.Get<const MobileStatic>(entity).type;
	constexpr float k_Factor = 0.7935f; // about the cube root of 1/2: each half has half the volume
	const float scale = transform.scale.x * k_Factor;
	const float offset = RockRadius2D(entity) * k_Factor;
	// Only the Y angle is kept. MobileStaticArchetype's rotation has column 2 = (-sin y cos x, sin x, cos y cos x).
	const float yAngle = std::atan2(-transform.rotation[2].x, transform.rotation[2].z);
	// the first draw: GameFloatRand(2pi)
	const float a = game_random::GameFloatRand(glm::two_pi<float>());
	// o = GetPosFromAngle(a, R2D x 0.7935); the halves at the rock's MapCoords + o and - o: both keep the rock's
	// altitude (o's is 0), so each half is the ground at its own point plus that altitude
	const auto o = gutils::GetPosFromAngle(a, offset);
	const auto centre = map_coords::FromWorld(transform.position);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: split type {} scale {:.2f} -> 2 x {:.2f}", static_cast<int>(type),
	                   transform.scale.x, scale);
	// Each half in turn: made, put into the physics (it falls and settles, or flies on), then given the rock's fire.
	// Starting to move takes a burning thing out of its blaze, so a half that took the fire first would leave the
	// rock's blaze at once; this way both halves burn on in it.
	std::array<entt::entity, 2> halves {};
	for (size_t i = 0; i < halves.size(); ++i)
	{
		const auto at = map_coords::ToWorld(i == 0 ? centre + o : centre - o);
		const auto half = archetypes::MobileStaticArchetype::Create(at, type, 0.0f, 0.0f, yAngle, 0.0f, scale);
		halves.at(i) = half;
		if (auto* po = Locator::dynamicsSystem::value().AddObject(half, velocity, glm::vec3(0.0f)))
		{
			po->body.angularMomentum = angularMomentum;
		}
		Locator::fireSystem::value().CopyFire(entity, half);
	}
	// Then the rock goes the common way, out of the physics and the map, its fire with it
	ecs::ToBeDeleted(entity);
	return halves;
}

bool object_physics::KnockRock(entt::entity entity, PhysicsEntry& po, float g)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a rock: not from another rock
	const auto* hitBy = ecs::physics::hitter::HitterEntry(Locator::dynamicsSystem::value().GetState().objects, po);
	const bool byRock = hitBy != nullptr && registry.Valid(hitBy->entity) && IsRock(hitBy->entity);
	if (g > 4.0f && RockHeight(entity) > 0.7f && !byRock)
	{
		auto& life = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
		life.value -= (g - 4.0f) * 0.005f;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: rock hit G {:.1f}, life {:.3f}", g, life.value);
		if (life.value < 0.01f)
		{
			// Rock::SplitInTwo: the halves carry on with its velocity and half its angular velocity
			const bool flying = !po.body.resting;
			const auto velocity = flying ? po.body.velocity : glm::vec3(0.0f);
			const auto spin = flying ? po.body.angularMomentum * 0.5f : glm::vec3(0.0f);
			SplitRock(entity, velocity, spin);
			return false;
		}
	}
	return true;
}

void object_physics::ConsiderArtefact(const PhysicsEntry& entry, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Only a gentle put-down from a hand, by a player who isn't neutral (the hand's player, PLAYER_ONE, as the dead
	// tree's reaction takes it), of a thing worth making an artefact of
	if (!entry.byPlayer || (entry.flags & PhysicsEntry::k_FromHand) == 0 || (entry.flags & PhysicsEntry::k_Landed) == 0 ||
	    !Locator::artefactSystem::has_value() || !registry.Valid(object))
	{
		return;
	}
	const auto player = PlayerNames::PLAYER_ONE;
	const auto* info = fire::traits::InfoOf(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	// Nor a thing a script holds
	if (info == nullptr || transform == nullptr || !(info->artifactMultiplier > 0.0f) || registry.AllOf<InScript>(object))
	{
		return;
	}
	// The nearest building or worship site within reach, found as the game finds the nearest of a kind of thing
	const auto own = map_coords::FromMetres({transform->position.x, transform->position.z});
	const auto nearest = map_cells::FindNearestInSpiral(
	    own, [&registry](entt::entity thing) { return registry.AnyOf<Abode, WorshipSite>(thing); },
	    openblack::physics::objects::k_ArtefactReach, object);
	if (nearest == entt::null)
	{
		return;
	}
	auto& artefacts = Locator::artefactSystem::value();
	// An artefact already made, worth enough, impresses a town or site it didn't last belong to: its people look at it
	const auto impress = [&](bool sameAsLast) {
		if (const auto* record = registry.TryGet<const TownArtefact>(object);
		    record != nullptr && artefact_rules::WillImpress(record->value, sameAsLast))
		{
			static_cast<void>(effects::reactions::CreateReaction(object, Reaction::LookAtObject, player, true));
		}
	};
	if (registry.AllOf<WorshipSite>(nearest))
	{
		const auto* record = registry.TryGet<const TownArtefact>(object);
		impress(record != nullptr && record->site == nearest);
		artefacts.AddToSite(object, nearest, player);
		return;
	}
	const auto town = town_queries::GetTown(nearest);
	if (town == entt::null)
	{
		return;
	}
	const auto* record = registry.TryGet<const TownArtefact>(object);
	impress(record != nullptr && record->town == town);
	// It becomes the town's artefact, worth what it impresses villagers when it is new
	artefacts.AddToTown(object, town, player);
}

void object_physics::ArtefactTaken(entt::entity object, PlayerNames player)
{
	if (Locator::artefactSystem::has_value())
	{
		Locator::artefactSystem::value().Taken(object, player);
	}
}
