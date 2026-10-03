/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Game.h"
#include "Influence.h"

// Private to ECS/Influence: the GGame fields the influence reads, kept on one registry entity so that a new land (the
// registry's Reset) puts them back to the GGame::Init defaults before the map script runs.

namespace openblack::influence::detail
{
struct InfluenceGlobals
{
	/// g_game+0x205C4C: the rings, newest first (the ctor pushes at the head of the list)
	std::vector<entt::entity> rings;
	/// GPlayer +0x8C of each player (PlayerNames), CalculateInfluencePower 0x64AD00. (aproximado) kept with the land:
	/// the original keeps it in the GPlayer, but recomputes it every turn before anyone reads it
	std::array<float, 8> power {};
	/// [0xEB9A14]: the InfluenceCircles, newest first. GGame::ClearMap's InfluenceCircle::Reset (Game.cpp:1310) is the
	/// registry's reset here
	std::vector<Circle> circles;
	/// g_game+0x250174, Update3DInfluence's dirty byte. (inferido) 0 after GGame::ClearVariables 0x54BF22 (the value
	/// written was not traced): the first circles of a land wait for the first radius that moves by more than 0.01 from
	/// the 0 of Citadel +0x78 / Town +0xF24, which every new citadel and town with influence gives
	bool circlesDirty {false};
	/// [0xEB9A1C]: the players' border latch (BoundaryShown), cleared by fn_00828A50 on every land load
	std::array<bool, 8> boundaryShown {};
	/// [0xEB9A44]: the hand's ripples, newest first. (inferido) cleared with the land: nothing that empties the list on
	/// ClearMap was traced, and a ripple lives 2 s
	std::vector<Ripple> ripples;
};

/// The land's globals (made on first use)
InfluenceGlobals& Globals();
/// Read only: the defaults when there is no registry entity yet
[[nodiscard]] const InfluenceGlobals& GlobalsOrDefault();
/// The land's globals when they were made, else nullptr (for the draw, which must not create the entity)
[[nodiscard]] InfluenceGlobals* TryGlobals();

/// The GGame fields the map script sets (land number g_game+0x205A08, town / player influence multipliers
/// g_game+0x250078 / +0x25007C): Game::GetMapScriptGlobals() (the one copy, set by FeatureScriptCommands and reset by
/// Game::LoadMap as GGame::Init 0x54F66F does); without a Game (the unit tests) a static one with the same defaults
[[nodiscard]] MapScriptGlobals& MapGlobals();

/// GetDistanceInMetres 0x74CD70: x,z only
[[nodiscard]] float DistanceXZ(const glm::vec3& a, const glm::vec3& b);
} // namespace openblack::influence::detail
