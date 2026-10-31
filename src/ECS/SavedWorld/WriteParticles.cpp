/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spot visuals' containers in a saved game's world file, each with the particle interface and the particle manager
// of its effect, in the original's order, written from the particle engine's containers and effects
// (docs/wiki/engine/saved-games.md#openblack, the particles). They are not entities: a container, its interface and its
// manager are named by the number of the container's effect.

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SpellLink.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ParticleContainerRecord;
using gamesave::ParticleInterfaceRecord;
using gamesave::ParticleManagerRecord;

namespace
{
/// The manager's state bit of an effect closing down
constexpr uint32_t k_ClosingDown = 1;
/// The persistent index that names a particle file itself rather than one of its parts
constexpr uint32_t k_FileItself = 0xFFFFFFFF;
/// Below this length the effect's start velocity has no direction: it is taken as along x
constexpr double k_NoSpeed = 0.0001;

/// The particle engine's state, when there is one
particles::manager::State* EngineState()
{
	return Locator::particleSystem::has_value() ? &Locator::particleSystem::value().GetState() : nullptr;
}

/// The live container of the effect, when there is one
const particles::manager::Container* ContainerOf(uint64_t effect)
{
	auto* state = EngineState();
	if (state == nullptr)
	{
		return nullptr;
	}
	const auto found =
	    std::ranges::find(state->containers, static_cast<uint32_t>(effect), &particles::manager::Container::effect);
	return found != state->containers.end() ? &*found : nullptr;
}

/// The running effect of the number, when there is one
const particles::manager::Running* RunningOf(uint64_t effect)
{
	auto* state = EngineState();
	if (state == nullptr)
	{
		return nullptr;
	}
	const auto found = state->effects.find(static_cast<uint32_t>(effect));
	return found != state->effects.end() && found->second.effect != nullptr ? &found->second : nullptr;
}

/// The live containers' effects, the newest first, as the game's list of containers holds them
std::vector<uint64_t> ContainerKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> keys;
	if (auto* state = EngineState(); state != nullptr)
	{
		for (const auto& container : state->containers)
		{
			if (RunningOf(container.effect) != nullptr)
			{
				keys.push_back(container.effect);
			}
		}
	}
	return keys;
}

/// The spell whose effect it is, when one has it
entt::entity SpellOfEffect(const ecs::Registry& registry, uint64_t effect)
{
	for (const auto spell : magic::Spells())
	{
		const auto* made = registry.Valid(spell) ? registry.TryGet<ecs::components::Spell>(spell) : nullptr;
		if (made != nullptr && made->effect == effect)
		{
			return spell;
		}
	}
	return entt::null;
}

/// The particle type that names the effect's file in a save, as the original loads a file for each particle type: its
/// container's spot visual's, or the type its spell's class plays; none for a container whose spot visual's type
/// openblack does not know, or an effect of neither
std::optional<uint32_t> FileIdOf(const ecs::Registry& registry, uint64_t effect)
{
	if (const auto* container = ContainerOf(effect); container != nullptr)
	{
		if (container->particleType == ParticleType::None)
		{
			return std::nullopt;
		}
		return static_cast<uint32_t>(container->particleType);
	}
	if (const auto spell = SpellOfEffect(registry, effect); spell != entt::null)
	{
		const auto& made = registry.Get<ecs::components::Spell>(spell);
		return static_cast<uint32_t>(magic::OpsOf(made.spellClass).particleType(spell));
	}
	return std::nullopt;
}

/// A container of a spot visual: its place (the map coordinates its effect is kept at), whether its effect is drawn as
/// one, the thing it hangs on (while that is still there), its effect's interface, the turns it has left (-1 for
/// ever) and its strength (the value it gives its effect each turn)
std::optional<gamesave::SavedRecord> WriteContainer(WriteContext& context, uint64_t effect)
{
	const auto* container = ContainerOf(effect);
	const auto* running = RunningOf(effect);
	if (container == nullptr || running == nullptr)
	{
		context.Fail("the container of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	ParticleContainerRecord record;
	record.position.position = {.x = container->coords.x, .z = container->coords.z, .altitude = container->coords.altitude};
	record.byte = running->path == particles::DrawPath::Queued ? 1 : 0;
	const bool owned = container->hadOwner && container->owner != entt::null && context.GetRegistry().Valid(container->owner);
	record.object = {owned ? context.Reference(container->owner) : 0};
	record.system = {context.Reference(SourceKey {.type = ParticleInterfaceRecord::k_Type, .id = effect})};
	record.word = static_cast<uint32_t>(container->turns);
	record.lastWord = std::bit_cast<uint32_t>(container->strength);
	return record;
}

/// The particle interface of the effect: its manager
std::optional<gamesave::SavedRecord> WriteInterface(WriteContext& context, uint64_t effect)
{
	return ParticleInterfaceRecord {
	    .system = {context.Reference(SourceKey {.type = ParticleManagerRecord::k_Type, .id = effect})}};
}

/// The particle manager of the effect: its atoms' count, its player, its process info, its origin, alpha, age and
/// state (closing down), its start velocity with its direction and speed, its magnitude and the age it closed down at,
/// the spell whose effect it is, whether it is synced, its file (by its container's spot visual's or its spell's
/// particle type: a write without one fails) and the things it aims at. Its collections and their atoms, the word +0x78, the
/// second flag, the words of its targets and its target points are not written from openblack yet (empty, 0, the manager's
/// start value 1)
std::optional<gamesave::SavedRecord> WriteManager(WriteContext& context, uint64_t effect)
{
	const auto* running = RunningOf(effect);
	if (running == nullptr)
	{
		context.Fail("particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& made = *running->effect;
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (!file.has_value())
	{
		// the original names the file by the particle type of the container's spot visual or of its spell: an effect of
		// neither (a container made without the game's info, the debug hook's test effect) cannot be saved as the
		// original does
		context.Fail("the container of particle effect " + std::to_string(effect) + " (" + made.GetFile().name +
		             ") has no spot visual's particle type to name its file by");
		return std::nullopt;
	}
	ParticleManagerRecord record;
	record.word = static_cast<int32_t>(made.AtomCount());
	record.object = {PlayerObject(context, made.GetPlayer())};
	record.processInfo = ProcessInfoBytes(made.GetProcessInfo());
	const auto origin = made.GetOrigin();
	record.point = {origin.x, origin.y, origin.z};
	record.byte = static_cast<uint8_t>(std::clamp(made.GetGlobalAlpha(), 0.0f, 255.0f));
	record.value = made.GetAge();
	record.secondWord = made.Closing() ? k_ClosingDown : 0;
	// the start velocity, its direction (along x when it has none) and its speed, as the effect's start works them out
	const auto velocity = made.GetDirection();
	const auto speed = std::sqrt(static_cast<double>(velocity.x) * velocity.x + static_cast<double>(velocity.y) * velocity.y +
	                             static_cast<double>(velocity.z) * velocity.z);
	record.points[1] = {velocity.x, velocity.y, velocity.z};
	if (speed > k_NoSpeed)
	{
		const auto over = static_cast<float>(1.0 / speed);
		record.points[2] = {velocity.x * over, velocity.y * over, velocity.z * over};
		record.words[0] = std::bit_cast<uint32_t>(static_cast<float>(speed));
	}
	else
	{
		record.points[2] = {1.0f, 0.0f, 0.0f};
		record.words[0] = 0;
	}
	record.words[1] = std::bit_cast<uint32_t>(made.GetMagnitude());
	record.words[2] = std::bit_cast<uint32_t>(made.GetCloseAge());
	// the spell whose effect it is; none for a container's
	record.other = {context.Reference(SpellOfEffect(context.GetRegistry(), effect))};
	record.bytes = {static_cast<uint8_t>(made.NetType() == game_random::particles::NetGameType::Synced ? 1 : 0), 1};
	record.persistent = {.first = *file, .second = k_FileItself};
	for (const auto target : made.GetTargets())
	{
		if (target != entt::null && context.GetRegistry().Valid(target))
		{
			record.targets.targets.elements.push_back({.word = 0, .object = {context.Reference(target)}});
		}
	}
	return record;
}
} // namespace

std::array<uint8_t, 60> openblack::ecs::saved_world_write::ProcessInfoBytes(const particles::ProcessInfo& info)
{
	std::array<uint8_t, 60> bytes {};
	const auto put = [&bytes](size_t at, const auto& value) { std::memcpy(bytes.data() + at, &value, sizeof(value)); };
	const auto point = [&put](size_t at, const glm::vec3& value) {
		put(at, value.x);
		put(at + 4, value.y);
		put(at + 8, value.z);
	};
	point(0, info.interfacePos);
	point(12, info.handPos);
	point(24, info.cameraForward);
	point(36, info.direction);
	put(48, info.power);
	put(52, info.curl);
	bytes[56] = info.enabled ? 1 : 0;
	return bytes;
}

void openblack::ecs::saved_world_write::RegisterParticleWriters(ClassWriters& writers)
{
	writers.RegisterSource(ParticleContainerRecord::k_Type, ContainerKeys, WriteContainer);
	writers.RegisterSource(ParticleInterfaceRecord::k_Type, ContainerKeys, WriteInterface);
	writers.RegisterSource(ParticleManagerRecord::k_Type, ContainerKeys, WriteManager);
	// the game's containers: the live ones, the newest first, then those the load kept, as the loaded file listed them
	writers.RegisterList(GlobalList::ParticleContainers, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto effect : context.Keys(ParticleContainerRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ParticleContainerRecord::k_Type, .id = effect}));
		}
		if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
		{
			const auto& kept = game->lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers;
			numbers.insert(numbers.end(), kept.begin(), kept.end());
		}
		return numbers;
	});
}
