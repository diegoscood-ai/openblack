/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureAudioSystem.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureDust.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSoundQueue.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/SnowDust.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
const CreatureRig* RigOf(const Creature& creature)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// A step's dust: as many puffs as the step is heavy, at the creature, each tinted by the snow lying there
void ThrowDust(entt::entity entity, glm::vec3 at, int32_t puffs)
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto size = ecs::creature_pose::DrawnSize(std::as_const(Locator::entitiesRegistry::value()), entity);
	const auto localRand = [](int32_t n) { return game_random::LocalRand(n); };
	for (int32_t i = 0; i < puffs; ++i)
	{
		const auto puff = creature_dust::Make(size, localRand);
		const auto colour = ecs::snow_dust::Tint(creature_dust::k_Colour, ecs::snow_dust::SnowAt(at));
		Locator::dynamicsSystem::value().AddPuff(at, puff.velocity, puff.size, colour);
	}
}

/// Plays an event's sound from the creature and notes it among the creature's last sounds
void Sound(entt::entity entity, const Creature& creature, const Transform& transform, const CreatureRig& rig,
           CreatureAudio& heard, const creature_audio::SoundEvent& event, const std::string& silence)
{
	const auto surface = static_cast<audio::SoundSurface>(ecs::sea_cells::GetSurfaceType(transform.position));
	// keyed by the size it is drawn at (the pen's in its temple's pen)
	const auto size = ecs::creature_pose::DrawnSize(std::as_const(Locator::entitiesRegistry::value()), entity);
	const auto request =
	    creature_audio::Request(event, size, creature.alignment, rig.soundObject, surface, rig.soundBankName, creature.species);
	const bool hasAudio = Locator::audio::has_value();
	audio::BankId bank = audio::k_NoBank;
	if (request.generic)
	{
		bank = audio::Bank(audio::SfxBank::Creature);
	}
	else if (hasAudio && !request.voiceStem.empty())
	{
		bank = Locator::audio::value().CreatureBank(request.voiceStem);
	}
	CreatureAudio::Heard note {
	    .atMs = heard.clockMs,
	    .kind = event.kind,
	    .keys = request.keys,
	    .bank = bank,
	    .channel = audio::k_NoChannel,
	    .played = false,
	    .note = silence,
	};
	if (silence.empty())
	{
		const auto listener = Locator::audioState::has_value() ? audio::ListenerPoint() : std::nullopt;
		if (!hasAudio)
		{
			note.note = "no audio";
		}
		else if (!request.generic && bank == audio::k_NoBank)
		{
			note.note = "no voice bank";
		}
		else if (!listener.has_value())
		{
			note.note = "no listener";
		}
		else
		{
			// Played from the creature, which the sound follows; its distance summed in the game's order
			note.channel = Locator::audio::value().PlayAnimationEffect(
			    audio::Owner::Thing(entity), creature_audio::CameraDistance(*listener, transform.position),
			    request.keys.ToArray(), request.action, bank, true, 0.0f, 0.0f);
			note.played = note.channel != audio::k_NoChannel;
			if (!note.played)
			{
				note.note = "filtered out or not started";
			}
		}
	}
	const auto keys = request.keys.ToArray();
	if (auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Creature {} {} ({}) from bank {} keys [{} {} {} {} {}]: {}", static_cast<uint32_t>(entity),
		                    creature_audio::Name(event.action), static_cast<int32_t>(event.action), bank, keys[0], keys[1],
		                    keys[2], keys[3], keys[4],
		                    note.played ? fmt::format("channel {}", note.channel) : fmt::format("silent: {}", note.note));
	}
	heard.recent.push_back(std::move(note));
	while (heard.recent.size() > CreatureAudio::k_RecentCount)
	{
		heard.recent.pop_front();
	}
}

/// What the sounds need to know of each of a species' animations: the moments it lists, its length and whether it loops
creature_audio::InfoOf InfoOfRig(const CreatureRig& rig)
{
	return [&rig](size_t index) -> std::optional<creature_audio::AnimationInfo> {
		const auto* played = rig.GetAnimation(CreatureRig::Mesh::Base, index);
		if (played == nullptr)
		{
			return std::nullopt;
		}
		return creature_audio::AnimationInfo {
		    .events = index < rig.soundEvents.size() ? std::span(rig.soundEvents[index])
		                                             : std::span<const creature_audio::SoundEvent> {},
		    .durationMs = played->duration,
		    .looping = played->looping,
		};
	};
}

/// The animation event a queued sound came from
creature_audio::SoundEvent EventOf(const creature_audio::QueuedSound& sound)
{
	return {
	    .kind = sound.generic ? creature_audio::EventKind::Generic : creature_audio::EventKind::Voice,
	    .timeMs = 0,
	    .action = sound.action,
	    .mode = sound.mode,
	};
}

/// A queued sound as the drawing reaches it: a footstep's print and its dust, heard or not, then the sound itself
void PlayQueuedSound(entt::entity entity, const Creature& creature, const Transform& transform, const CreatureRig& rig,
                     CreatureAudio& heard, const creature_audio::Gate& gate, const creature_audio::QueuedSound& sound)
{
	const auto event = EventOf(sound);
	// A footstep leaves its print even when it isn't heard
	if (creature_audio::IsFootstep(event.action) && Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().Step(entity);
	}
	// and throws up its dust, heard or not
	if (const auto puffs = creature_dust::PuffsFor(event.action); puffs.has_value())
	{
		ThrowDust(entity, transform.position, *puffs);
	}
	if (creature_audio::IsHeard(event.kind, gate))
	{
		Sound(entity, creature, transform, rig, heard, event, {});
	}
	else if (!gate.muted)
	{
		Sound(entity, creature, transform, rig, heard, event, "another player's creature's voice");
	}
}

/// Whether a creature's sounds are heard: none while muted, and its voice only as the gate lets another player's be
creature_audio::Gate GateFor(bool muted, bool otherVoicesEnabled, const Creature& creature)
{
	return {
	    .muted = muted,
	    .localPlayersCreature = creature.owner == creature::LocalPlayer(),
	    .otherVoicesEnabled = otherVoicesEnabled,
	};
}

/// The creature's sounds, made the first time it is heard
CreatureAudio& HeardOf(ecs::Registry& registry, entt::entity creature)
{
	return std::as_const(registry).AllOf<CreatureAudio>(creature) ? registry.Get<CreatureAudio>(creature)
	                                                              : registry.AssignState<CreatureAudio>(creature);
}
} // namespace

bool CreatureAudioSystem::AreOtherVoicesEnabled() const
{
	return !Locator::audioState::has_value() ||
	       creature_audio::OtherVoicesEnabled(audio::GetScriptAudioState().creatureSound.load());
}

void CreatureAudioSystem::SetOtherVoicesEnabled(bool enabled)
{
	if (Locator::audioState::has_value())
	{
		audio::GetScriptAudioState().creatureSound.store(enabled ? 1 : 0);
	}
}

void CreatureAudioSystem::Update(float turnFraction, std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureAnimation, const Transform>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureAnimation&, const Transform&) {
		    creatures.push_back(entity);
	    });

	const bool otherVoices = AreOtherVoicesEnabled();
	for (const auto entity : creatures)
	{
		auto& heard = HeardOf(registry, entity);
		heard.clockMs += gameTime.count();
		// The sounds the last turn queued that the drawing has now passed, in their order
		const auto drawn = heard.queued.TakeDrawn(turnFraction);
		const auto& creature = lookup.Get<const Creature>(entity);
		const auto* rig = RigOf(creature);
		if (rig == nullptr)
		{
			continue;
		}
		const auto gate = GateFor(_muted, otherVoices, creature);
		for (const auto& sound : drawn)
		{
			PlayQueuedSound(entity, creature, lookup.Get<const Transform>(entity), *rig, heard, gate, sound);
		}
	}
}

void CreatureAudioSystem::PlayQueued()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// Only a creature already heard has sounds waiting
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureAudio, const Transform>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureAudio&, const Transform&) {
		    creatures.push_back(entity);
	    });

	const bool otherVoices = AreOtherVoicesEnabled();
	for (const auto entity : creatures)
	{
		auto& heard = registry.Get<CreatureAudio>(entity);
		// Whatever the drawing has not reached plays now, in its order
		const auto left = heard.queued.TakeAll();
		const auto& creature = lookup.Get<const Creature>(entity);
		const auto* rig = RigOf(creature);
		if (rig == nullptr)
		{
			continue;
		}
		const auto gate = GateFor(_muted, otherVoices, creature);
		for (const auto& sound : left)
		{
			PlayQueuedSound(entity, creature, lookup.Get<const Transform>(entity), *rig, heard, gate, sound);
		}
	}
}

void CreatureAudioSystem::QueueTurn(const CreatureAnimationSystemInterface::TurnMoments& passed,
                                    const CreatureAnimationSystemInterface::TurnInputs& atEnd)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureAnimation, const Transform>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureAnimation&, const Transform&) {
		    creatures.push_back(entity);
	    });

	for (const auto entity : creatures)
	{
		auto& heard = HeardOf(registry, entity);
		// What it plays together by weight as the turn leaves it, with the one among them whose moments sound
		std::vector<creature_audio::Played> slots;
		std::vector<float> weights;
		const auto inputs = std::ranges::find(atEnd, entity, &CreatureAnimationSystemInterface::TurnInputs::value_type::first);
		for (const auto& slot :
		     inputs != atEnd.end() ? inputs->second.slots : lookup.Get<const CreatureAnimation>(entity).slots)
		{
			slots.push_back({slot.animation, slot.timeMs});
			weights.push_back(slot.weight);
		}
		const auto* rig = RigOf(lookup.Get<const Creature>(entity));
		if (rig != nullptr)
		{
			const auto found =
			    std::ranges::find(passed, entity, &CreatureAnimationSystemInterface::TurnMoments::value_type::first);
			const creature_layers::TurnPassed none {};
			// Hair groups shown and hidden by animations are not drawn differently yet
			creature_audio::QueueTurn(found != passed.end() ? found->second : none, heard.turnSlots, slots,
			                          creature_audio::SoundingSlot(weights), InfoOfRig(*rig), heard.queued, {});
		}
		heard.turnSlots = std::move(slots);
	}
}

void CreatureAudioSystem::Play(entt::entity creature, const creature_audio::SoundEvent& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto& component = lookup.Get<const Creature>(creature);
	const auto* rig = RigOf(component);
	if (rig == nullptr || event.kind == creature_audio::EventKind::HairGroup)
	{
		return;
	}
	auto& heard = HeardOf(registry, creature);
	Sound(creature, component, lookup.Get<const Transform>(creature), *rig, heard, event, {});
}
