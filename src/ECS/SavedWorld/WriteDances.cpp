/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dances', their groups', their key frames' and their actions' records of a saved game's world file
// (docs/wiki/engine/saved-games.md#openblack, the dances).

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <bit>
#include <iterator>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include <DanceFile.h>
#include <SavedRecords.h>
#include <WorshipRecords.h>
#include <fmt/format.h>

#include "ECS/Components/Dance.h"
#include "ECS/Dances.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;
using gamesave::DanceGroupRecord;
using gamesave::DanceKeyActionRecord;
using gamesave::DanceKeyFrameRecord;
using gamesave::DanceRecord;

namespace
{
/// The game object is the file's first object
constexpr gamesave::ObjectNumber k_GameObject = 1;
/// What the dance's constructor leaves that openblack does not hold: the dance camera's travel time, and a group's
/// speed, the first villager row's walking speed in whole map units a turn, until it first moves
constexpr float k_MadeCameraTravelTime = 2.0f;
constexpr int32_t k_UnmovedGroupSpeed = 1311;
/// What a dance without a file is made with
constexpr std::array<uint32_t, 2> k_NoFileEditorValues {30, 0};
/// The label every dance writes
constexpr std::string_view k_Label = "begin";
/// The key frame and the action of a source id's part number: the key frame in the high half, the action in the low
constexpr uint32_t k_KeyFrameShift = 16;
constexpr uint32_t k_ActionMask = 0xFFFF;

/// A dance no load made and that has not started yet: what openblack does not hold of it is still as it was made
bool IsAsMade(const components::Dance& dance)
{
	return dance.state == components::Dance::State::Stopped && dance.startTurn == 0 && !dance.firstDancerTurn.has_value();
}

gamesave::ObjectNumber GroupReference(WriteContext& context, entt::entity dance, size_t group)
{
	return context.Reference(SourceKey {.type = DanceGroupRecord::k_Type, .id = PartId(dance, static_cast<uint32_t>(group))});
}

gamesave::RefList GroupReferences(WriteContext& context, entt::entity dance, const std::vector<size_t>& groups)
{
	gamesave::RefList list;
	for (const auto group : groups)
	{
		list.numbers.push_back(GroupReference(context, dance, group));
	}
	return list;
}

const components::Dance* DanceOf(const WriteContext& context, entt::entity dance)
{
	return context.GetRegistry().Valid(dance) ? context.GetRegistry().TryGet<const components::Dance>(dance) : nullptr;
}

/// A dance: its group behaviour (its place, clock, key frames, groups, dancers, speeds and camera) and its own values
std::optional<gamesave::SavedRecord> WriteDance(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& dance = registry.Get<const components::Dance>(entity);
	std::optional<DanceRecord> kept;
	if (auto saved = context.KeptRecordOf(entity, DanceRecord::k_Type); saved.has_value())
	{
		kept = std::get<DanceRecord>(std::move(*saved));
	}
	if (!kept.has_value() && !IsAsMade(dance))
	{
		context.Fail(fmt::format("dance {} has started: its camera's values are not held", entt::to_integral(entity)));
		return std::nullopt;
	}
	DanceRecord record = kept.value_or(DanceRecord {});
	auto& group = record.group;
	group.position.position = {.x = dance.place.x, .z = dance.place.z, .altitude = dance.place.altitude};
	group.position.flags = PositionFlagsOf(registry, entity, kept.has_value() ? kept->group.position.flags : uint16_t {0});
	group.time = dance.clock;
	group.keyFrames.numbers.clear();
	const auto* file = dance.file.get();
	if (file != nullptr)
	{
		for (size_t frame = 0; frame < file->keyFrames.size(); ++frame)
		{
			group.keyFrames.numbers.push_back(context.Reference(
			    SourceKey {.type = DanceKeyFrameRecord::k_Type, .id = PartId(entity, static_cast<uint32_t>(frame))}));
		}
		group.groups = file->groupCount;
		group.editorValues = {file->unknown44, file->unknown48};
		group.groupAngle = file->angle.value_or(0.0f);
	}
	else
	{
		group.editorValues = k_NoFileEditorValues;
	}
	group.length = dance.loopLength;
	std::vector<size_t> all(dance.groups.all.size());
	std::ranges::generate(all, [next = size_t {0}]() mutable { return next++; });
	group.danceGroups = GroupReferences(context, entity, all);
	group.fixedGroups = GroupReferences(context, entity, dance.groups.limited);
	group.proportionGroups = GroupReferences(context, entity, dance.groups.shared);
	group.dancers = dance.dancers;
	group.proportionCounter = dance.groups.round;
	group.denominator = dance.groups.roundLength;
	group.speed = dance.rate;
	group.lastSpeed = dance.dancingRate;
	// the camera: as the dance is made, made by a script or not
	// the camera: as its load kept it, else as the dance is made; made by a script or not
	if (!kept.has_value())
	{
		group.flags = {};
		group.cameraTravelTime = k_MadeCameraTravelTime;
		group.cameraNearTurns = 0;
	}
	group.flags[1] = dance.madeByScript ? 1 : 0;

	record.type = static_cast<uint32_t>(dance.type);
	record.worshipSpeed = dance.speed;
	record.target.number = context.Reference(dance.owner);
	record.words = {dance.state == components::Dance::State::Dancing ? 0U : 1U,
	                dance.duration,
	                dance.startTurn,
	                dance.firstDancerTurn.has_value() ? 1U : 0U,
	                dance.firstDancerTurn.value_or(0U),
	                dance.onTheirWay};
	std::ranges::copy(k_Label, record.label.begin());
	// its footpath link: openblack's, else as its load kept it
	if (const auto link = ecs::footpaths::GetFootpathLink(entity); link != entt::null || !kept.has_value())
	{
		record.footpathLink.number = context.Reference(link);
	}
	return record;
}

/// A group of a dance, by its dance and its number there
std::optional<gamesave::SavedRecord> WriteDanceGroup(WriteContext& context, uint64_t id)
{
	const auto [dance, index] = PartOf(id);
	const auto* owner = DanceOf(context, dance);
	if (owner == nullptr || index >= owner->groups.all.size())
	{
		return std::nullopt;
	}
	const auto& group = owner->groups.all.at(index);
	std::optional<DanceGroupRecord> kept;
	if (auto saved = context.KeptRecordOf(SourceKey {.type = DanceGroupRecord::k_Type, .id = id}); saved.has_value())
	{
		kept = std::get<DanceGroupRecord>(std::move(*saved));
	}
	if (!kept.has_value() && (!IsAsMade(*owner) || group.name.empty()))
	{
		context.Fail(fmt::format("dance {}'s group {}: its {} not held", entt::to_integral(dance), index,
		                         group.name.empty() ? "untitled name is" : "centres and speed are"));
		return std::nullopt;
	}
	DanceGroupRecord record = kept.value_or(DanceGroupRecord {});
	record.members.numbers.clear();
	record.stack.values.clear();
	record.objects[0].number = context.Reference(dance);
	record.objects[1].number = record.objects[0].number;
	for (const auto dancer : group.dancers)
	{
		record.members.numbers.push_back(context.Reference(dancer));
	}
	record.words = {index, group.flag ? 1U : 0U, group.formation, group.shape};
	std::memcpy(record.eightBytes.data(), &group.offset, record.eightBytes.size());
	record.secondWords = {std::bit_cast<uint32_t>(group.radius), 0, std::bit_cast<uint32_t>(group.rotation)};
	const std::array<uint32_t, 4> move {group.move.action, group.move.first, group.move.second, group.move.end};
	std::memcpy(record.sixteenBytes.data(), move.data(), record.sixteenBytes.size());
	record.value = group.spin;
	record.other.number = group.parent.has_value() ? GroupReference(context, dance, *group.parent) : 0;
	record.byte = group.indexInParent;
	record.thirdWord = group.limitedDancers;
	record.firstInt = group.moved ? 1 : 0;
	// its speed and centres: as its load kept them, else as made, as it has not moved
	if (!kept.has_value())
	{
		record.secondInt = k_UnmovedGroupSpeed;
	}
	record.danceType = group.danceType;
	record.fourthWord = group.inFormation;
	// its name: as its load kept it while it is the same, with what the game's text held past its end; else as named
	const auto keptName =
	    kept.has_value() ? std::string_view(kept->text.begin(), std::ranges::find(kept->text, '\0')) : std::string_view {};
	if (!kept.has_value() || (!group.name.empty() && group.name != keptName))
	{
		record.text = {};
		std::ranges::copy(group.name.substr(0, record.text.size() - 1), record.text.begin());
	}
	const std::array<uint32_t, 3> proportion {group.limited ? 1U : 0U, group.quota, group.weight};
	std::memcpy(record.proportion.data(), proportion.data(), record.proportion.size());
	for (const auto child : group.children)
	{
		record.stack.values.push_back(static_cast<uint32_t>(child));
	}
	record.villagerType = group.sexes;
	return record;
}

/// A key frame of a dance's file: its time, its actions and its flags
std::optional<gamesave::SavedRecord> WriteKeyFrame(WriteContext& context, uint64_t id)
{
	const auto [dance, index] = PartOf(id);
	const auto* owner = DanceOf(context, dance);
	if (owner == nullptr || owner->file == nullptr || index >= owner->file->keyFrames.size())
	{
		return std::nullopt;
	}
	const auto& frame = owner->file->keyFrames.at(index);
	DanceKeyFrameRecord record;
	record.word = std::bit_cast<uint32_t>(frame.time);
	for (size_t action = 0; action < frame.actions.size(); ++action)
	{
		record.actions.numbers.push_back(
		    context.Reference(SourceKey {.type = DanceKeyActionRecord::k_Type,
		                                 .id = PartId(dance, (index << k_KeyFrameShift) | static_cast<uint32_t>(action))}));
	}
	record.lastWord = frame.flags;
	return record;
}

/// An action of a key frame: the groups it is done to, in its file's order as the game keeps them, what it does and its
/// arguments
std::optional<gamesave::SavedRecord> WriteKeyAction(WriteContext& context, uint64_t id)
{
	const auto [dance, part] = PartOf(id);
	const auto* owner = DanceOf(context, dance);
	const auto frameIndex = part >> k_KeyFrameShift;
	const auto actionIndex = part & k_ActionMask;
	if (owner == nullptr || owner->file == nullptr || frameIndex >= owner->file->keyFrames.size() ||
	    actionIndex >= owner->file->keyFrames.at(frameIndex).actions.size())
	{
		return std::nullopt;
	}
	const auto& action = owner->file->keyFrames.at(frameIndex).actions.at(actionIndex);
	DanceKeyActionRecord record;
	record.words.values.assign(action.groups.begin(), action.groups.end());
	record.word = action.type;
	std::memcpy(record.values.data(), action.arguments.data(), record.values.size());
	return record;
}

/// The dances' parts of a kind: each dance's, in the dances' order, by its part numbers
std::vector<uint64_t> PartKeys(const ecs::Registry& registry, uint32_t type)
{
	std::vector<uint64_t> keys;
	for (const auto entity : ecs::dances::InTurnOrder(registry))
	{
		const auto& dance = registry.Get<const components::Dance>(entity);
		if (type == DanceGroupRecord::k_Type)
		{
			for (size_t group = 0; group < dance.groups.all.size(); ++group)
			{
				keys.push_back(PartId(entity, static_cast<uint32_t>(group)));
			}
			continue;
		}
		const auto* file = dance.file.get();
		for (size_t frame = 0; file != nullptr && frame < file->keyFrames.size(); ++frame)
		{
			if (type == DanceKeyFrameRecord::k_Type)
			{
				keys.push_back(PartId(entity, static_cast<uint32_t>(frame)));
				continue;
			}
			for (size_t action = 0; action < file->keyFrames.at(frame).actions.size(); ++action)
			{
				keys.push_back(
				    PartId(entity, (static_cast<uint32_t>(frame) << k_KeyFrameShift) | static_cast<uint32_t>(action)));
			}
		}
	}
	return keys;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterDanceWriters(ClassWriters& writers)
{
	writers.Register(
	    DanceRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Dance>(entity); }, WriteDance);
	writers.RegisterSource(
	    DanceGroupRecord::k_Type, [](const Registry& registry) { return PartKeys(registry, DanceGroupRecord::k_Type); },
	    WriteDanceGroup);
	writers.RegisterSource(
	    DanceKeyFrameRecord::k_Type, [](const Registry& registry) { return PartKeys(registry, DanceKeyFrameRecord::k_Type); },
	    WriteKeyFrame);
	writers.RegisterSource(
	    DanceKeyActionRecord::k_Type, [](const Registry& registry) { return PartKeys(registry, DanceKeyActionRecord::k_Type); },
	    WriteKeyAction);
	// the dances' list: newest first, as each new one goes to its head; for a loaded game the dances made since, then
	// the list as loaded, each dance the load did not make as kept
	writers.RegisterList(GlobalList::Dances, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> made;
		for (const auto entity : ecs::dances::InTurnOrder(context.GetRegistry()))
		{
			made.push_back(context.Reference(entity));
		}
		const auto* kept = context.KeptWorldRecord(k_GameObject);
		const auto* head = kept != nullptr ? std::get_if<gamesave::GameHead>(kept) : nullptr;
		if (head == nullptr)
		{
			return made;
		}
		const auto& loaded = head->lists.at(static_cast<size_t>(GlobalList::Dances)).numbers;
		std::vector<gamesave::ObjectNumber> numbers;
		std::ranges::copy_if(made, std::back_inserter(numbers), [&loaded](gamesave::ObjectNumber number) {
			return std::ranges::find(loaded, number) == loaded.end();
		});
		for (const auto number : loaded)
		{
			const auto target = context.TargetOf(number);
			const bool notMade = target.has_value() && !target->id.has_value() && target->entity == entt::null;
			if (notMade || std::ranges::find(made, number) != made.end())
			{
				numbers.push_back(number);
			}
		}
		return numbers;
	});
}
