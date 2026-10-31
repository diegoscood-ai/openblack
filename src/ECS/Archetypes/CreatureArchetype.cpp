/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArchetype.h"

#include <optional>
#include <utility>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureCreation.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureNaming.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/LivingConstruct.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::creature;

namespace
{
// What a new creature of a species starts at without the game's creature tables (a stand-in)
constexpr float k_UnknownStartScale = 0.22f;

const GCreatureInfo* SpeciesInfo(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

/// The species' base mesh, or nothing when it is not loaded
const graphics::L3DMesh* BaseMesh(CreatureType species)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto meshId = creature::GetIdFromType(species, CreatureBody::Appearance::Base);
	const auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(meshId) ? &*meshes.Handle(meshId) : nullptr;
}

/// The draws a new creature of the species makes on the game's synchronised stream, none without the game's tables
std::optional<creature_creation::Made> MakeDraws(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const creature_creation::Random random {
	    .rand = [](uint32_t range) { return game_random::GameRand(range); },
	    .floatRand = [](float range) { return game_random::GameFloatRand(range); },
	};
	return creature_creation::Make(creature_creation::SetupFor(Locator::infoConstants::value(), species), random);
}

} // namespace

float CreatureArchetype::DrawnScale(CreatureType species, float size)
{
	// Without the meshes the rest height is unknown, and the body is drawn at its size
	const auto* mesh = BaseMesh(species);
	const float restHeight = mesh != nullptr ? creature_morph::RestHeight(mesh->GetBoneMatrices()) : 0.0f;
	return creature_morph::DrawnScale(size, restHeight);
}

CreatureBodyMetrics CreatureArchetype::BodyMetrics(CreatureType species)
{
	const auto* mesh = BaseMesh(species);
	if (mesh == nullptr)
	{
		return {};
	}
	float reach = 0.0f;
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	if (const auto rigId = creature::GetRigId(species); rigs.Contains(rigId))
	{
		const auto* stand = rigs.Handle(rigId)->GetAnimation(CreatureRig::Mesh::Base, creature_animation::k_StandAnimation);
		if (stand != nullptr)
		{
			reach = creature_cast_moves::BoneReach(*stand, mesh->GetBoneParents(), mesh->GetBoneLocals());
		}
	}
	return {.restHeight = creature_morph::RestHeight(mesh->GetBoneMatrices()), .reach = reach};
}

float CreatureArchetype::SpeciesStrength(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	return info != nullptr ? info->strength : creature_morph::k_UnknownSpeciesStrength;
}

float CreatureArchetype::StartScale(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	// as the species' row has it: only the size its body is drawn at is kept within limits
	return info != nullptr ? info->startScale : k_UnknownStartScale;
}

CreatureArchetype::Body CreatureArchetype::StartBody(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	if (info == nullptr)
	{
		return {};
	}
	return {.alignment = 0.0f, .fatness = info->startFatness, .strength = info->strength};
}

entt::entity CreatureArchetype::Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
                                       entt::id_type creatureMindId, float yAngleRadians, float scale,
                                       const std::optional<Body>& given)
{
	// the living part's own age, the first draw of a creature's making (a creature keeps its own age)
	if (const auto* info = SpeciesInfo(creatureType); info != nullptr)
	{
		static_cast<void>(ecs::living::ConstructorAgeDraw(*info));
	}
	// It draws as it is made, whatever body it is given
	auto made = MakeDraws(creatureType);
	auto body = StartBody(creatureType);
	if (made.has_value())
	{
		body.fatness = made->fatness;
		body.strength = made->strength;
	}
	if (given.has_value())
	{
		body = *given;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	const auto morph =
	    creature_morph::FromAttributes(body.alignment, body.fatness, body.strength, SpeciesStrength(creatureType));
	registry.Assign<Creature>(entity, Creature {
	                                      .owner = playerName,
	                                      .leashable = false,
	                                      .species = creatureType,
	                                      .mind = creatureMindId,
	                                      .alignment = body.alignment,
	                                      .fatness = body.fatness,
	                                      .strength = body.strength,
	                                      .size = scale,
	                                  });
	// The body is drawn with the base mesh's skins, its shape blended towards the other meshes
	registry.Assign<Mesh>(entity, creature::GetIdFromType(creatureType, CreatureBody::Appearance::Base));
	registry.AssignState<CreatureMorph>(entity, CreatureMorph {.shownFatness = body.fatness, .drawn = morph, .revision = 0});
	registry.AssignState<CreatureAnimation>(entity);
	registry.AssignState<CreatureEyes>(entity);
	auto& mind = registry.AssignState<CreatureMindState>(entity);
	if (made.has_value())
	{
		mind.desires = std::move(made->desires);
		mind.personality = made->personality;
		mind.previousActions = std::move(made->previousActions);
	}
	// It is named as it is made, by who it belongs to and how many creatures there are with it
	creature_naming::NameNewCreature(mind, playerName, registry.Size<Creature>());
	registry.AssignState<CreatureNeeds>(entity);
	registry.AssignState<CreatureHair>(entity);
	registry.AssignState<CreatureTattoos>(entity);
	registry.AssignState<CreatureMarks>(entity);
	registry.AssignState<CreatureSkin>(entity);
	registry.AssignState<CreatureLocomotion>(entity);
	// measured once, as the game measures a body when it loads it; its rest height is the one DrawnScale finds
	const auto metrics = BodyMetrics(creatureType);
	registry.AssignState<CreatureBodyMetrics>(entity, metrics);
	// it keeps the size it is made with, and its body is given that size, drawn within the limits a body is drawn at
	const auto drawn = creature_morph::ApplySize(scale, metrics.restHeight);
	registry.AssignState<CreatureDrawPose>(entity, CreatureDrawPose {
	                                                   .scale = glm::vec3(drawn.scale),
	                                                   .size = drawn.size,
	                                                   .given = drawn.given,
	                                               });
	// the creature's world matrix is the generic object one: Ry(-a), not Ry(+a)
	registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(drawn.scale));
	// the creature's reaction, which the living things about react to for as long as it lives: for its player (none for
	// a creature of no player), not yet started, spread at once from where it stands, before it is in its cell. Without
	// the reactions (a test's world) none is made
	if (Locator::reactionSystem::has_value())
	{
		effects::reactions::CreateReactionBeforeFiling(
		    entity, openblack::Reaction::ReactToCreature,
		    playerName != PlayerNames::_COUNT ? std::optional(playerName) : std::nullopt, false);
	}
	// on creation, as soon as it stands somewhere: the head of its cell's mobile list. From then on every move of the
	// turn refiles it (creature_pose::CommitTurnPose)
	ecs::map_cells::InsertMapObject(entity);
	// It is its owner's creature, whichever they had, and their list keeps it after the ones they got before
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().AddCreature(entity);
	}
	// so it is the one a player who leads creatures leads, the one before losing its leash
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().ClaimOnArrival(entity);
	}
	// it has a home from the start: its player's temple's first point, else where it is made
	ecs::player_creature::GiveMadeHome(entity, playerName, position);
	// its owner's creature is set: the totems of the owner's towns wear the icon of the creature they lead
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().PlayersCreatureSet(playerName);
	}
	// it appears where it is: drawn there from the next frame, with no slide from anywhere
	ecs::NotifyTeleported(entity);
	return entity;
}
