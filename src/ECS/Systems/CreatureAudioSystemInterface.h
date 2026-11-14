/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureAudio.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"

namespace openblack::ecs::systems
{

/// Sounds the creatures' animations as they play past the moments their sounds are placed on: footsteps and blows from
/// the bank all creatures share, roars and cries from the species' own, each played from the creature and following it
/// (see creature_audio and components::CreatureAudio). The moments are found once a creature turn, as its animations
/// move on, and each sound waits for the body's drawing, a turn behind, to reach it
class CreatureAudioSystemInterface
{
public:
	virtual ~CreatureAudioSystemInterface() = default;

	/// Once a frame, by the game time, after the bodies are posed: each creature's queued sounds that the drawing,
	/// `turnFraction` of the way from its last turn pose but one to its last (taken as at most 2), has passed play, in
	/// order, each with its footprint and its dust
	virtual void Update(float turnFraction, std::chrono::duration<float, std::milli> gameTime) = 0;
	/// At the creatures' turn, just before their animations move on: every sound still queued plays, in order, and
	/// the queues are left empty
	virtual void PlayQueued() = 0;
	/// Right after the animations moved on for the turn: the moments each body passed (`passed`, from
	/// CreatureAnimationSystemInterface::AdvanceTurn), or, while it plays animations together by weight, those the
	/// heaviest of them (`atEnd`, as the turn leaves it) passed since the turn before, are queued at how far through
	/// their stretch each lies (creature_audio::QueueTurn)
	virtual void QueueTurn(const CreatureAnimationSystemInterface::TurnMoments& passed,
	                       const CreatureAnimationSystemInterface::TurnInputs& atEnd) = 0;

	/// Silences every creature, for trying things out
	[[nodiscard]] virtual bool IsMuted() const = 0;
	virtual void SetMuted(bool muted) = 0;
	/// Whether the creatures of players other than the local one are heard in their own voices, as a script can ask.
	/// Their footsteps and blows are always heard.
	[[nodiscard]] virtual bool AreOtherVoicesEnabled() const = 0;
	virtual void SetOtherVoicesEnabled(bool enabled) = 0;

	/// Sounds an event on a creature at once, as its animation would, whoever it belongs to and whether or not the
	/// creatures are muted. For trying the sounds out.
	virtual void Play(entt::entity creature, const creature_audio::SoundEvent& event) = 0;
	/// Sounds one of the creature's own sounds at once, as the game's code does outside its animations (an order
	/// acknowledged, say): from its species' bank, with nothing heard without a camera and another player's creature
	/// heard only as the creature sounds' gate lets it
	virtual void PlayNow(entt::entity /*creature*/, audio::SoundAction /*action*/) {}
};

} // namespace openblack::ecs::systems
