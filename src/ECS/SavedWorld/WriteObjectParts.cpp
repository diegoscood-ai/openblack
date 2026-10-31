/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WriteObjectParts.h"

#include <EffectRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Footpaths.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
/// Whether the entity stands where coordinates put it: its place in metres is theirs
bool StandsAt(const ecs::Registry& registry, entt::entity entity, const gamesave::MapCoords& coords)
{
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	return transform == nullptr ||
	       (transform->position.x == map_coords::ToMetres(coords.x) && transform->position.z == map_coords::ToMetres(coords.z));
}
/// A fixed object's target place: its place's coordinates with the altitude its object part keeps (the game's fixed
/// objects keep 0 there whatever their place's altitude), as loaded while it stands where its load put it
gamesave::MapCoords FixedTargetOf(const gamesave::PositionPart& position, const gamesave::ObjectPart* kept)
{
	if (kept != nullptr && kept->coords.x == position.position.x && kept->coords.z == position.position.z)
	{
		return kept->coords;
	}
	if (kept == nullptr)
	{
		return position.position; // one made since: where it stands
	}
	return {.x = position.position.x, .z = position.position.z, .altitude = kept->coords.altitude};
}
} // namespace

uint16_t openblack::ecs::saved_world_write::PositionFlagsOf(const Registry& registry, entt::entity entity, uint16_t kept)
{
	using namespace position_flags;
	namespace components = ecs::components;
	uint16_t flags = kept & k_OnAStructure;
	const auto set = [&flags](bool on, uint16_t bit) {
		if (on)
		{
			flags |= bit;
		}
	};
	set(ecs::map_cells::IsObjectInMap(entity), k_InMap);
	set(ecs::map_cells::IsMultiCellStaticClass(entity), k_OfSeveralCells);
	set(Locator::handSystem::has_value() && Locator::handSystem::value().IsInMagicHand(entity), k_InTheHand);
	set(registry.AllOf<components::InLockedSelect>(entity), k_InLockedSelect);
	set(registry.AllOf<components::InPhysics>(entity), k_InPhysics);
	set(registry.AllOf<components::DrawListed>(entity), k_DrawListed);
	set(registry.AllOf<components::InScript>(entity), k_InScript);
	set(registry.AllOf<components::ScriptControlled>(entity), k_ControlledByScript);
	set(registry.AllOf<components::Immovable>(entity), k_Immovable);
	set(registry.AllOf<components::CannotBePickedUp>(entity), k_CannotBePickedUp);
	set(registry.AllOf<components::Indestructible>(entity), k_Indestructible);
	set(ecs::map_cells::CountsAsFixed(ecs::map_cells::TypeOf(entity)), k_CountsAsFixed);
	return flags;
}

gamesave::PositionPart openblack::ecs::saved_world_write::PositionPartOf(const WriteContext& context, entt::entity entity,
                                                                         const gamesave::PositionPart* kept)
{
	const auto& registry = context.GetRegistry();
	gamesave::PositionPart part;
	part.flags = PositionFlagsOf(registry, entity, kept != nullptr ? kept->flags : uint16_t {0});
	if (kept != nullptr && StandsAt(registry, entity, kept->position))
	{
		part.position = kept->position;
		return part;
	}
	if (kept != nullptr)
	{
		// the altitude a fixed object's place keeps is not read: as kept
		part.position.altitude = kept->position.altitude;
	}
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entity))
	{
		// one made since: its height over the land under it as well
		const auto coords = map_coords::FromWorld(transform->position);
		part.position.x = coords.x;
		part.position.z = coords.z;
		if (kept == nullptr)
		{
			part.position.altitude = coords.altitude;
		}
	}
	return part;
}

gamesave::ObjectPart openblack::ecs::saved_world_write::ObjectPartOf(WriteContext& context, entt::entity entity,
                                                                     const gamesave::ObjectPart* kept,
                                                                     const gamesave::MapCoords& target, uint32_t info)
{
	const auto& registry = context.GetRegistry();
	gamesave::ObjectPart part = kept != nullptr ? *kept : gamesave::ObjectPart {};
	part.info = info;
	part.coords = target;
	if (const auto* index = registry.TryGet<const ecs::components::ObjectCreationIndex>(entity))
	{
		part.creationIndex = index->value;
	}
	part.life = ecs::life::LifeOf(entity);
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entity))
	{
		part.scale = transform->scale.x;
	}
	part.heading = ecs::placed_turn::AnglesOf(registry, entity).y;
	// its fire, by the fire's own id
	if (const auto* fire = ecs::fire::Find(entity); fire != nullptr)
	{
		part.fire = context.Reference(SourceKey {.type = gamesave::FireEffectRecord::k_Type, .id = fire->id});
	}
	else
	{
		part.fire = 0;
	}
	// the body the physics moves is the things in flight's writer's (not written yet): as kept
	return part;
}

gamesave::FixedPart openblack::ecs::saved_world_write::FixedPartOf(WriteContext& /*context*/, entt::entity /*entity*/,
                                                                   const gamesave::FixedPart* kept)
{
	// the town artifact a fixed object holds is not modelled on the object: as kept, none for one made since
	return kept != nullptr ? *kept : gamesave::FixedPart {};
}

gamesave::MultiMapFixedPart openblack::ecs::saved_world_write::MultiMapFixedPartOf(WriteContext& context, entt::entity entity,
                                                                                   const gamesave::MultiMapFixedPart* kept)
{
	const auto& registry = context.GetRegistry();
	gamesave::MultiMapFixedPart part = kept != nullptr ? *kept : gamesave::MultiMapFixedPart {};
	// how much is built: an abode's, a citadel part's or a feature's own; else as kept (1 for one made since)
	if (kept == nullptr || registry.AnyOf<ecs::components::Abode>(entity))
	{
		part.percentBuilt = ecs::abodes::GetPercentBuilt(entity);
	}
	part.footpathLink = context.Reference(ecs::footpaths::GetFootpathLink(entity));
	if (const auto* abode = registry.TryGet<const ecs::components::Abode>(entity))
	{
		part.buildingSite = context.Reference(abode->buildingSite);
	}
	return part;
}

gamesave::ObjectNumber openblack::ecs::saved_world_write::PlayerReference(WriteContext& context, PlayerNames player)
{
	const auto index = static_cast<size_t>(std::to_underlying(player));
	const auto* kept = context.KeptWorldRecord(1);
	if (const auto* head = kept != nullptr ? std::get_if<gamesave::GameHead>(kept) : nullptr; head != nullptr)
	{
		return index < head->players.size() ? head->players.at(index) : 0;
	}
	gamesave::ObjectNumber number = 0;
	context.GetRegistry().Each<const ecs::components::Player>(
	    [&context, &number, player](entt::entity entity, const ecs::components::Player& each) {
		    if (each.name == player && number == 0)
		    {
			    number = context.Reference(entity, gamesave::k_PlayerType);
		    }
	    });
	return number;
}

gamesave::FixedFields openblack::ecs::saved_world_write::FixedFieldsOf(WriteContext& context, entt::entity entity,
                                                                       const gamesave::FixedFields* kept, uint32_t info)
{
	gamesave::FixedFields fields;
	fields.position = PositionPartOf(context, entity, kept != nullptr ? &kept->position : nullptr);
	fields.object = ObjectPartOf(context, entity, kept != nullptr ? &kept->object : nullptr,
	                             FixedTargetOf(fields.position, kept != nullptr ? &kept->object : nullptr), info);
	fields.fixed = FixedPartOf(context, entity, kept != nullptr ? &kept->fixed : nullptr);
	return fields;
}

gamesave::MultiMapFixedFields
openblack::ecs::saved_world_write::MultiMapFixedFieldsOf(WriteContext& context, entt::entity entity,
                                                         const gamesave::MultiMapFixedFields* kept, uint32_t info)
{
	gamesave::MultiMapFixedFields fields;
	fields.position = PositionPartOf(context, entity, kept != nullptr ? &kept->position : nullptr);
	fields.object = ObjectPartOf(context, entity, kept != nullptr ? &kept->object : nullptr,
	                             FixedTargetOf(fields.position, kept != nullptr ? &kept->object : nullptr), info);
	fields.fixed = FixedPartOf(context, entity, kept != nullptr ? &kept->fixed : nullptr);
	fields.multi = MultiMapFixedPartOf(context, entity, kept != nullptr ? &kept->multi : nullptr);
	return fields;
}
