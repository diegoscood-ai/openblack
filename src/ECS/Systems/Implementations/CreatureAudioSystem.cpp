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
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSoundQueue.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/SnowDust.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
const CreatureRig* RigOf(CreatureType species)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

const CreatureRig* RigOf(const Creature& creature)
{
	return RigOf(creature.species);
}

/// Whose voice a body's sounds are in: its species, its alignment and the size it is drawn at
struct Voice
{
	CreatureType species;
	float alignment;
	float size;
};

/// A creature's voice: keyed by the size it is drawn at (the pen's in its temple's pen)
Voice VoiceOf(entt::entity entity, const Creature& creature)
{
	return {.species = creature.species,
	        .alignment = creature.alignment,
	        .size = ecs::creature_pose::DrawnSize(std::as_const(Locator::entitiesRegistry::value()), entity)};
}

/// A step's dust: as many puffs as the step is heavy, at the body, each tinted by the snow lying there
void ThrowDust(glm::vec3 at, int32_t puffs, float size)
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto localRand = [](int32_t n) { return game_random::LocalRand(n); };
	for (int32_t i = 0; i < puffs; ++i)
	{
		const auto puff = creature_dust::Make(size, localRand);
		const auto colour = ecs::snow_dust::Tint(creature_dust::k_Colour, ecs::snow_dust::SnowAt(at));
		Locator::dynamicsSystem::value().AddPuff(at, puff.velocity, puff.size, colour);
	}
}

/// Plays an event's sound from the creature and notes it among the creature's last sounds
void Sound(entt::entity entity, const Voice& voice, const Transform& transform, const CreatureRig& rig, CreatureAudio& heard,
           const creature_audio::SoundEvent& event, const std::string& silence)
{
	const auto surface = static_cast<audio::SoundSurface>(ecs::sea_cells::GetSurfaceType(transform.position));
	const auto request =
	    creature_audio::Request(event, voice.size, voice.alignment, rig.soundObject, surface, rig.soundBankName, voice.species);
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
/// The walk the legs played on this turn while their tracks are the move's (standing, walking and running by weight, no
/// turn or step drawn as a pair): its time at the turn's start and how far it moved on, in whole milliseconds, 0 with no
/// walk among them. None for any other turn (no tracks, or a turn's or a step's)
std::optional<creature_audio::WalkAdvance> WalkOfMove(const ecs::Registry& registry, entt::entity creature)
{
	namespace locomotion = creature_locomotion::animations;
	const auto* legs = registry.TryGet<const CreatureLocomotion>(creature);
	if (legs == nullptr || legs->tracks.empty())
	{
		return std::nullopt;
	}
	const auto isMove = [](const CreatureLocomotion::Track& track) {
		return !track.pair.has_value() && (track.animation == locomotion::k_Stand || track.animation == locomotion::k_Walk ||
		                                   track.animation == locomotion::k_Run);
	};
	if (!std::ranges::all_of(legs->tracks, isMove))
	{
		return std::nullopt;
	}
	creature_audio::WalkAdvance walk {.animation = locomotion::k_Walk};
	if (const auto found = std::ranges::find(legs->tracks, locomotion::k_Walk, &CreatureLocomotion::Track::animation);
	    found != legs->tracks.end())
	{
		walk.fromMs = static_cast<int32_t>(found->fromMs);
		walk.deltaMs = static_cast<int32_t>(found->advanceMs);
	}
	return walk;
}

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

/// Whether the game has a camera to hear from (without the audio state, as in some tests, it is taken to have one)
bool HasCamera()
{
	return !Locator::audioState::has_value() || audio::ListenerPoint().has_value();
}

/// A queued sound as the drawing reaches it: nothing at all without a camera; else the sound itself, then, heard or
/// not, a footstep's dust or a creed moment's glow, and last a footstep's print
void PlayQueuedSound(entt::entity entity, const Voice& voice, const Transform& transform, const CreatureRig& rig,
                     CreatureAudio& heard, const creature_audio::Gate& gate, const creature_audio::QueuedSound& sound)
{
	if (!HasCamera())
	{
		return;
	}
	const auto event = EventOf(sound);
	if (creature_audio::IsHeard(event.kind, gate))
	{
		Sound(entity, voice, transform, rig, heard, event, {});
	}
	else if (!gate.muted)
	{
		Sound(entity, voice, transform, rig, heard, event, "another player's creature's voice");
	}
	// After the sound, heard or not: a footstep throws up its dust
	if (const auto puffs = creature_dust::PuffsFor(event.action); puffs.has_value())
	{
		ThrowDust(transform.position, *puffs, voice.size);
	}
	// a creed moment sets a glow on the body
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PlayCreedEvent(entity, static_cast<int32_t>(event.action));
	}
	// and a footstep leaves its print (and splashes the water where the foot goes in)
	if (creature_audio::IsFootstep(event.action) && Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().Step(entity);
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

/// Shows or hides one of a creature's tufts of hair, by its place among its file's groups; a number past the tufts a
/// creature can have, or a creature with no hair, changes nothing
void ShowHairGroup(ecs::Registry& registry, entt::entity creature, int32_t group, bool shown)
{
	auto* hair = registry.TryGet<CreatureHair>(creature);
	if (hair == nullptr || group < 0 || static_cast<size_t>(group) >= CreatureHair::k_MaxGroups)
	{
		return;
	}
	hair->hidden.set(static_cast<size_t>(group), !shown);
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
			PlayQueuedSound(entity, VoiceOf(entity, creature), lookup.Get<const Transform>(entity), *rig, heard, gate, sound);
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
			PlayQueuedSound(entity, VoiceOf(entity, creature), lookup.Get<const Transform>(entity), *rig, heard, gate, sound);
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
		QueueTurnOf(entity, passed, atEnd);
	}
}

void CreatureAudioSystem::QueueTurnOf(entt::entity entity, const CreatureAnimationSystemInterface::TurnMoments& passed,
                                      const CreatureAnimationSystemInterface::TurnInputs& atEnd)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(entity) || !lookup.AllOf<Creature, CreatureAnimation, Transform>(entity))
	{
		return;
	}
	{
		auto& heard = HeardOf(registry, entity);
		// What it plays together by weight as the turn leaves it
		std::vector<creature_audio::Played> slots;
		const auto inputs = std::ranges::find(atEnd, entity, &CreatureAnimationSystemInterface::TurnInputs::value_type::first);
		for (const auto& slot :
		     inputs != atEnd.end() ? inputs->second.slots : lookup.Get<const CreatureAnimation>(entity).slots)
		{
			slots.push_back({slot.animation, slot.timeMs});
		}
		const auto* rig = RigOf(lookup.Get<const Creature>(entity));
		if (rig != nullptr)
		{
			const auto found =
			    std::ranges::find(passed, entity, &CreatureAnimationSystemInterface::TurnMoments::value_type::first);
			const creature_layers::TurnPassed none {};
			// A moment that shows or hides a tuft of hair does so as the turn passes it
			const auto hair = [&registry, entity](int32_t group, bool shown) { ShowHairGroup(registry, entity, group, shown); };
			const auto& turnPassed = found != passed.end() ? found->second : none;
			// turning on the spot with its blended spins: the right 90 degree turn's moments over the blend's share, as the
			// game's turn sounds
			if (const auto* legs = lookup.TryGet<const CreatureLocomotion>(entity);
			    legs != nullptr && legs->spinStep.has_value())
			{
				const auto& spin = legs->spinStep;
				creature_audio::QueueSpinTurn(turnPassed, spin->fromMs, spin->toMs, spin->blendMs, InfoOfRig(*rig),
				                              heard.queued, hair);
			}
			// legs that stand, walk or run by weight pass the walk's moments alone, by its whole milliseconds
			else if (const auto walk = WalkOfMove(lookup, entity); walk.has_value())
			{
				creature_audio::QueueMoveTurn(turnPassed, *walk, InfoOfRig(*rig), heard.queued, hair);
			}
			else
			{
				// any other animations played together by weight (a reach's, a point's, a catch's, a kiss's) pass the
				// first one's moments alone, whatever the weights, as the game advances only the first of them
				creature_audio::QueueTurn(turnPassed, heard.turnSlots, slots, creature_audio::BlendSoundingSlot(slots.size()),
				                          InfoOfRig(*rig), heard.queued, hair);
			}
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
	Sound(creature, VoiceOf(creature, component), lookup.Get<const Transform>(creature), *rig, heard, event, {});
}

void CreatureAudioSystem::PlayNow(entt::entity creature, audio::SoundAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, Transform>(creature) || !HasCamera())
	{
		return;
	}
	const auto& component = lookup.Get<const Creature>(creature);
	const auto* rig = RigOf(component);
	if (rig == nullptr)
	{
		return;
	}
	const creature_audio::SoundEvent event {.kind = creature_audio::EventKind::Voice, .timeMs = 0, .action = action, .mode = 0};
	const auto gate = GateFor(_muted, AreOtherVoicesEnabled(), component);
	auto& heard = HeardOf(registry, creature);
	if (creature_audio::IsHeard(event.kind, gate))
	{
		Sound(creature, VoiceOf(creature, component), lookup.Get<const Transform>(creature), *rig, heard, event, {});
	}
	else if (!gate.muted)
	{
		Sound(creature, VoiceOf(creature, component), lookup.Get<const Transform>(creature), *rig, heard, event,
		      "another player's creature's voice");
	}
}

void CreatureAudioSystem::QueueCopyMoment(entt::entity copy, const creature_layers::TurnPassed& passed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(copy) || !lookup.AllOf<CreatureCopy, CreatureAnimation>(copy))
	{
		return;
	}
	const auto* rig = RigOf(lookup.Get<const CreatureCopy>(copy).species);
	auto& heard = HeardOf(registry, copy);
	// What it plays together by weight as the moment leaves it, its turn's spins
	std::vector<creature_audio::Played> slots;
	const auto& played = lookup.Get<const CreatureAnimation>(copy).slots;
	for (const auto& slot : played)
	{
		slots.push_back({slot.animation, slot.timeMs});
	}
	if (rig != nullptr)
	{
		// A moment that shows or hides a tuft of hair does so as the copy passes it
		const auto hair = [&registry, copy](int32_t group, bool shown) { ShowHairGroup(registry, copy, group, shown); };
		// Turning on the spot with its blended turn, it sounds the right 90 degree turn's moments over the blend's share,
		// from where the same blend stood at the moment before (its start for a turn just begun), as the game's turn does;
		// otherwise the first of what it plays sounds, as any animations played together do
		if (!played.empty() && played.front().pair.has_value() && played.front().pair->durationMs > 0)
		{
			const auto& blend = played.front();
			const auto before = std::ranges::find(heard.turnSlots, blend.animation, &creature_audio::Played::animation);
			const auto fromMs = before != heard.turnSlots.end() ? static_cast<int32_t>(before->timeMs) : 0;
			creature_audio::QueueSpinTurn(passed, fromMs, static_cast<int32_t>(blend.pair->timeMs),
			                              static_cast<int32_t>(blend.pair->durationMs), InfoOfRig(*rig), heard.queued, hair);
		}
		else
		{
			creature_audio::QueueTurn(passed, heard.turnSlots, slots, creature_audio::BlendSoundingSlot(slots.size()),
			                          InfoOfRig(*rig), heard.queued, hair);
		}
	}
	heard.turnSlots = std::move(slots);
}

void CreatureAudioSystem::PlayCopyQueued(entt::entity copy, std::optional<float> drawnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(copy) || !lookup.AllOf<CreatureCopy, CreatureAudio, Transform>(copy))
	{
		return;
	}
	auto& heard = registry.Get<CreatureAudio>(copy);
	const auto left = drawnFraction.has_value() ? heard.queued.TakeDrawn(*drawnFraction) : heard.queued.TakeAll();
	const auto& body = lookup.Get<const CreatureCopy>(copy);
	const auto* rig = RigOf(body.species);
	if (rig == nullptr)
	{
		return;
	}
	// The copy is no creature's own body, so it is never the local player's creature: its voice is heard as the
	// creature sounds' gate lets another player's be, its other sounds always
	const creature_audio::Gate gate {
	    .muted = _muted, .localPlayersCreature = false, .otherVoicesEnabled = AreOtherVoicesEnabled()};
	const Voice voice {.species = body.species, .alignment = body.alignment, .size = body.size};
	for (const auto& sound : left)
	{
		PlayQueuedSound(copy, voice, lookup.Get<const Transform>(copy), *rig, heard, gate, sound);
	}
}
