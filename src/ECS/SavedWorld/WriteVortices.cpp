/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The vortices a script makes (a way into another land, a way out of one, a volcano) and their crossing files' saves:
// see docs/wiki/engine/saved-games.md#the-game-objects-global-lists, "The shields and vortexes"

#include <cstdint>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <InterfaceRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/VortexSave.h"
#include "Locator.h"
#include "Particles/ParticleManager.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::LandscapeVortexInRecord;
using gamesave::LandscapeVortexOutRecord;
using gamesave::LandscapeVortexVolcanoRecord;
using gamesave::VortexSaveRecord;

namespace
{
/// The flag of an object of several cells that is built: a vortex is made so
constexpr uint8_t k_Constructed = 0x08;
/// The thing's marks of an object out of play and of a hidden name
constexpr uint8_t k_Unavailable = 0x01;
constexpr uint8_t k_NameHidden = 0x40;
/// The counts a vortex keeps of what it took in or made, one for each of its stats: the objects, the food and wood
/// they held, and the villagers
constexpr size_t k_VortexStats = 5;
constexpr size_t k_StatObjects = 0;
constexpr size_t k_StatResources = 2;
constexpr size_t k_StatVillagers = 3;

/// The class a vortex of the type is written as
uint32_t ClassOf(VortexType type)
{
	switch (type)
	{
	case VortexType::In:
		return LandscapeVortexInRecord::k_Type;
	case VortexType::Out:
		return LandscapeVortexOutRecord::k_Type;
	case VortexType::Volcano:
		return LandscapeVortexVolcanoRecord::k_Type;
	}
	return 0;
}

template <typename Record>
std::optional<Record> KeptAs(const WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(std::move(*kept));
	}
	return std::nullopt;
}

/// A text as the game writes one: its characters and the zero after them
gamesave::Counted<uint8_t> TextOf(const std::string& text)
{
	gamesave::Counted<uint8_t> counted;
	counted.values.assign(text.begin(), text.end());
	counted.values.push_back(0);
	return counted;
}

/// Whether a vortex has a crossing file's save: a way in with its writer, a way out until its reader has read the
/// file to its end
bool HasCrossingFile(const ecs::components::LandscapeVortex& vortex)
{
	return (vortex.type == VortexType::In && vortex.writer != nullptr) ||
	       (vortex.type == VortexType::Out && vortex.reader != nullptr);
}

/// What every vortex writes: the parts of a mobile static of the vortex's row (no owner, upright), then its centre,
/// its type, state and radius, the mean of the heights it flattened and the heights themselves
template <typename Record>
Record VortexOf(WriteContext& context, entt::entity entity, const std::optional<Record>& kept)
{
	const auto& registry = context.GetRegistry();
	const auto& vortex = registry.Get<const ecs::components::LandscapeVortex>(entity);
	Record record = kept.value_or(Record {});
	record.base = MultiMapFixedFieldsOf(context, entity, kept.has_value() ? &kept->base : nullptr,
	                                    ecs::saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Vortex));
	if (!kept.has_value())
	{
		record.thing.flags = static_cast<uint8_t>((registry.AllOf<ecs::components::Unavailable>(entity) ? k_Unavailable : 0) |
		                                          (registry.AllOf<ecs::components::NameHidden>(entity) ? k_NameHidden : 0));
		record.base.multi.fixedFlags = k_Constructed;
	}
	record.point = {vortex.position.x, vortex.position.y, vortex.position.z};
	record.words = {static_cast<uint32_t>(vortex.type), static_cast<uint32_t>(vortex.state),
	                std::bit_cast<uint32_t>(vortex.radius), std::bit_cast<uint32_t>(vortex.meanAltitude)};
	record.bytes.values = vortex.savedAltitudes;
	return record;
}

/// The number its crossing file's save is named by, 0 for none
gamesave::ObjectNumber CrossingFileOf(WriteContext& context, entt::entity entity)
{
	const auto& vortex = context.GetRegistry().Get<const ecs::components::LandscapeVortex>(entity);
	if (!HasCrossingFile(vortex))
	{
		return 0;
	}
	return context.Reference(SourceKey {.type = VortexSaveRecord::k_Type, .id = PartId(entity, 0)});
}

/// A way into another land: a vortex's parts, its crossing file's save, its object mover's particle interface while
/// it runs and the counts of what it took in, as none taken (openblack counts nothing an In takes yet)
std::optional<gamesave::SavedRecord> WriteWayIn(WriteContext& context, entt::entity entity)
{
	const auto kept = KeptAs<LandscapeVortexInRecord>(context, entity);
	auto record = VortexOf(context, entity, kept);
	if (!kept.has_value())
	{
		record.values.values.assign(k_VortexStats, 0);
	}
	record.objects[0] = {CrossingFileOf(context, entity)};
	const auto mover = context.GetRegistry().Get<const ecs::components::LandscapeVortex>(entity).objectMover;
	record.objects[1] = {mover != 0 && Locator::particleSystem::has_value() && particles::manager::Find(mover) != nullptr
	                         ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = mover})
	                         : 0};
	return record;
}

/// A way out of another land: a vortex's parts, the flock of the animals it let out, the flock they join, the flock's
/// place and its two lengths, its crossing file's save, the counts of what it made (the objects, the food and wood
/// they held and the villagers; the other two as kept, none for a new one), the villagers it threw out, the newest
/// first, and the town they join
std::optional<gamesave::SavedRecord> WriteWayOut(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& vortex = registry.Get<const ecs::components::LandscapeVortex>(entity);
	const auto kept = KeptAs<LandscapeVortexOutRecord>(context, entity);
	auto record = VortexOf(context, entity, kept);
	const auto reference = [&context, &registry](entt::entity object) -> gamesave::ObjectNumber {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : 0;
	};
	record.animalFlock = {reference(vortex.animalFlock)};
	record.flockToJoin = {reference(vortex.flock)};
	const auto at = map_coords::FromWorld(vortex.flockPosition);
	record.flockCoords = {.x = at.x, .z = at.z, .altitude = at.altitude};
	record.flockDistance = vortex.flockA;
	record.domainRadius = vortex.flockB;
	record.saver = {CrossingFileOf(context, entity)};
	if (record.created.values.size() != k_VortexStats)
	{
		record.created.values.assign(k_VortexStats, 0);
	}
	record.created.values[k_StatObjects] = vortex.statObjects;
	record.created.values[k_StatResources] = vortex.statResources;
	record.created.values[k_StatVillagers] = vortex.statVillagers;
	record.thrown.numbers.clear();
	for (const auto villager : vortex.thrownVillagers)
	{
		if (const auto number = reference(villager); number != 0)
		{
			record.thrown.numbers.push_back(number);
		}
	}
	record.town = {reference(vortex.town)};
	return record;
}

/// A volcano: a vortex's parts only
std::optional<gamesave::SavedRecord> WriteVolcano(WriteContext& context, entt::entity entity)
{
	return VortexOf(context, entity, KeptAs<LandscapeVortexVolcanoRecord>(context, entity));
}

/// Every vortex's crossing file save that is still there, by its vortex
std::vector<uint64_t> CrossingFileKeys(const ecs::Registry& registry)
{
	std::vector<uint64_t> keys;
	registry.Each<const ecs::components::LandscapeVortex>(
	    [&keys](entt::entity entity, const ecs::components::LandscapeVortex& vortex) {
		    if (HasCrossingFile(vortex))
		    {
			    keys.push_back(PartId(entity, 0));
		    }
	    });
	return keys;
}

/// The crossing file's path in the save folder, as a crossing file's save names it
std::string CrossingFileName()
{
	constexpr char k_Backslash = 0x5C;
	const auto folder = ecs::vortex_save::SaveFolder();
	std::string name = folder.has_value() ? (*folder / "vortex.txt").string() : std::string("vortex.txt");
	std::ranges::replace(name, '/', k_Backslash);
	return name;
}

/// A crossing file's save: by whom (0 a way in, which writes it, 1 a way out, which reads it), the lines a way out has
/// read, the crossing file's path in the save folder (a loaded one's as kept), and the name of its copy beside the
/// world file, which the save makes as it writes the world file (none written without a world file: a loaded one's as
/// kept, else empty)
std::optional<gamesave::SavedRecord> WriteCrossingFile(WriteContext& context, uint64_t id)
{
	const auto entity = PartOf(id).owner;
	const auto& registry = context.GetRegistry();
	const auto* vortex = registry.Valid(entity) ? registry.TryGet<const ecs::components::LandscapeVortex>(entity) : nullptr;
	if (vortex == nullptr)
	{
		context.Fail("the crossing file of vortex " + std::to_string(entt::to_integral(entity)) + " has no vortex");
		return std::nullopt;
	}
	VortexSaveRecord record;
	auto kept = context.KeptRecordOf(SourceKey {.type = VortexSaveRecord::k_Type, .id = id});
	if (auto* saved = kept.has_value() ? std::get_if<VortexSaveRecord>(&*kept) : nullptr; saved != nullptr)
	{
		record = std::move(*saved);
	}
	else
	{
		record.name = TextOf(CrossingFileName());
		record.file = TextOf("");
	}
	if (const auto& copy = context.Source().crossingCopy; !copy.empty())
	{
		record.file = TextOf(copy);
	}
	record.byte = vortex->type == VortexType::Out ? 1 : 0;
	record.word = vortex->type == VortexType::Out && vortex->reader != nullptr ? vortex->reader->reads : 0;
	return record;
}

/// The writer of a vortex of the type
ClassWriter WriterOf(VortexType type)
{
	switch (type)
	{
	case VortexType::In:
		return WriteWayIn;
	case VortexType::Out:
		return WriteWayOut;
	case VortexType::Volcano:
		return WriteVolcano;
	}
	return {};
}
} // namespace

void openblack::ecs::saved_world_write::RegisterVortexWriters(ClassWriters& writers)
{
	for (const auto type : {VortexType::In, VortexType::Out, VortexType::Volcano})
	{
		writers.Register(
		    ClassOf(type),
		    [type](const Registry& registry, entt::entity entity) {
			    const auto* vortex = registry.TryGet<const ecs::components::LandscapeVortex>(entity);
			    return vortex != nullptr && vortex->type == type;
		    },
		    WriterOf(type));
	}
	writers.RegisterSource(VortexSaveRecord::k_Type, CrossingFileKeys, WriteCrossingFile);
}

std::vector<std::filesystem::path> openblack::ecs::saved_world_write::CrossingFiles(const ClassWriters& writers,
                                                                                    const WorldSource& source)
{
	WriteContext context(writers, source);
	std::vector<std::filesystem::path> files;
	for (const auto id : CrossingFileKeys(source.registry))
	{
		std::string name = CrossingFileName();
		auto kept = context.KeptRecordOf(SourceKey {.type = VortexSaveRecord::k_Type, .id = id});
		if (const auto* saved = kept.has_value() ? std::get_if<VortexSaveRecord>(&*kept) : nullptr; saved != nullptr)
		{
			name = ecs::vortex_save::TextOfRecord(saved->name.values);
		}
		const auto file = ecs::vortex_save::PathOfText(name);
		if (!file.empty() && std::ranges::find(files, file) == files.end())
		{
			files.push_back(file);
		}
	}
	return files;
}
