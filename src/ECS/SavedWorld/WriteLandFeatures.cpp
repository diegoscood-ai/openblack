/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land's features, animated statics and scripts' highlights in a saved game's world file
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the features, the animated statics and the scripts'
// highlights).

#include <cstdint>

#include <algorithm>
#include <bit>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <WorldListRecords.h>

#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The numbers of the first feature row, the first animated static row and the first script highlight row in the
/// game's tables; the other rows of each follow it downwards
constexpr uint32_t k_FirstFeatureRow = 2064;
constexpr uint32_t k_FirstAnimatedStaticRow = 1986;
constexpr uint32_t k_FirstScriptHighlightRow = 2088;
/// A building made whole: the fixed object's flag its constructor leaves
constexpr uint8_t k_Constructed = 0x08;
/// The thing's mark every script highlight carries in the original's saves
constexpr uint8_t k_HighlightThingFlags = 0x40;

template <typename Record>
[[nodiscard]] std::optional<Record> KeptAs(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return std::nullopt;
}

/// A feature: the parts of a building of several cells of its kind's row, made whole, built as far as a script set it
std::optional<gamesave::SavedRecord> WriteFeature(WriteContext& context, entt::entity entity)
{
	const auto& feature = context.GetRegistry().Get<const components::Feature>(entity);
	const auto kept = KeptAs<gamesave::FeatureRecord>(context, entity);
	gamesave::FeatureRecord record = kept.value_or(gamesave::FeatureRecord {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    k_FirstFeatureRow - static_cast<uint32_t>(feature.type));
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
	}
	return record;
}

/// An animated static: the parts of a building of several cells of its kind's row, made whole, its three gate stones'
/// slots, each the model of the stone laid in it or 0 when empty, and whether a script opened it
std::optional<gamesave::SavedRecord> WriteAnimatedStatic(WriteContext& context, entt::entity entity)
{
	const auto& still = context.GetRegistry().Get<const components::AnimatedStatic>(entity);
	const auto kept = KeptAs<gamesave::AnimatedStaticRecord>(context, entity);
	gamesave::AnimatedStaticRecord record = kept.value_or(gamesave::AnimatedStaticRecord {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    k_FirstAnimatedStaticRow - static_cast<uint32_t>(still.type));
	if (!kept.has_value())
	{
		record.base.multi.fixedFlags = k_Constructed;
	}
	record.frames.values.clear();
	std::ranges::transform(still.gateStones, std::back_inserter(record.frames.values),
	                       [](const auto& stone) { return stone.has_value() ? static_cast<uint32_t>(*stone) : 0U; });
	record.word = std::bit_cast<uint32_t>(still.openState);
	return record;
}

/// A script's highlight: the parts of a fixed object of its row, then whether it is hidden, whether it is active,
/// whether a script set its height and that height, its script id and its did-you-know category
std::optional<gamesave::SavedRecord> WriteScriptHighlight(WriteContext& context, entt::entity entity)
{
	const auto& highlight = context.GetRegistry().Get<const components::ScriptHighlight>(entity);
	const auto kept = KeptAs<gamesave::ScriptHighlightRecord>(context, entity);
	gamesave::ScriptHighlightRecord record = kept.value_or(gamesave::ScriptHighlightRecord {});
	if (!kept.has_value())
	{
		record.thing.flags = k_HighlightThingFlags;
	}
	record.base = FixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                            k_FirstScriptHighlightRow - highlight.infoIndex);
	record.words = {std::bit_cast<uint32_t>(highlight.hidden),
	                highlight.active ? 1U : 0U,
	                highlight.drawHeight.has_value() ? 1U : 0U,
	                std::bit_cast<uint32_t>(highlight.drawHeight.value_or(0.0f)),
	                highlight.scriptId,
	                static_cast<uint32_t>(highlight.category)};
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterLandFeatureWriters(ClassWriters& writers)
{
	writers.Register(
	    gamesave::FeatureRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Feature>(entity); },
	    WriteFeature);
	writers.Register(
	    gamesave::AnimatedStaticRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::AnimatedStatic>(entity); },
	    WriteAnimatedStatic);
	writers.Register(
	    gamesave::ScriptHighlightRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::ScriptHighlight>(entity); },
	    WriteScriptHighlight);
}
