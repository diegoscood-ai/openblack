/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fireballs' balls in a saved game's world file, written from their entities
// (docs/wiki/engine/saved-games.md#openblack, the fireballs).

#include <cstdint>

#include <optional>
#include <utility>
#include <variant>

#include <EffectRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::MagicFireBallRecord;

namespace
{
/// A ball's row of the game's tables runs down from this one, by its row of the fireball's three
constexpr uint32_t k_FirstFireBallRow = 316;

/// A fireball's ball: an object's parts (its row the fireball's; its target the place it was made at), the manager of
/// the effect whose atom carries it (none once that effect has gone) and whether rain cools it
std::optional<gamesave::SavedRecord> WriteFireBall(WriteContext& context, entt::entity entity)
{
	const auto& ball = context.GetRegistry().Get<const ecs::components::MagicFireBall>(entity);
	std::optional<MagicFireBallRecord> kept;
	if (auto record = context.KeptRecordOf(entity, MagicFireBallRecord::k_Type); record.has_value())
	{
		kept = std::get<MagicFireBallRecord>(std::move(*record));
	}
	MagicFireBallRecord record = kept.value_or(MagicFireBallRecord {});
	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr,
	                             gamesave::MapCoords {.x = ball.madeAt.x, .z = ball.madeAt.z, .altitude = ball.madeAt.altitude},
	                             k_FirstFireBallRow - static_cast<uint32_t>(ball.infoRow));
	const bool live = Locator::particleSystem::has_value() && Locator::particleSystem::value().Find(ball.effect) != nullptr;
	record.manager = {live ? context.Reference(SourceKey {.type = gamesave::ParticleManagerRecord::k_Type, .id = ball.effect})
	                       : 0};
	record.affectedByRain = ball.affectedByRain ? 1 : 0;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterFireBallWriters(ClassWriters& writers)
{
	writers.Register(
	    MagicFireBallRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::MagicFireBall>(entity); },
	    WriteFireBall);
}
