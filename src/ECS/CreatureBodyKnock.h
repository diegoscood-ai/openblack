/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

/// The creatures' knock in the world (creature_knock): what starts and stops it, and the bones knocking over what they
/// move into once the bodies are posed for the turn. Docs: docs/wiki/creature/pose-and-animation.md#the-knock.
namespace openblack::ecs::creature_body_knock
{
/// Each creature's knock follows what its body did since it last looked: the states its fighter entered, the change of
/// what its body plays, then a slap of the hand, in that order. Starting reads where the bones are in the pose the game
/// reads (creature_turn_pose::Safe), the one posed the turn before. Called as the creatures' turn begins and once their
/// animations have moved on, before their bodies are posed
void TakeMarks();
/// One creature's part of that, in its own turn
void TakeMarksOf(entt::entity creature);

/// Once the bodies are posed for the turn: each creature whose knock is on knocks over what its bones moved into
void Knock();
/// One creature's part of that, in its own turn
void KnockOf(entt::entity creature);

/// The hand slapped the creature and it reels from it: its knock is set up and started when it next looks
void Slapped(entt::entity creature);
} // namespace openblack::ecs::creature_body_knock
