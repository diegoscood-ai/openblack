/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "ParticleManager.h"

namespace openblack::particles::town_belief
{
/// The belief symbols over every town centre (TOWN_BELIEF: each centre's own town belief effect, whose
/// TownCentreBelief makes and moves them, RegisterTownBeliefRules), and the owners' symbols over the artefacts worth
/// enough (half size, their glows only).
/// Every rendered frame, paused too: one step of each centre's effect (made by MakeEffect) with dt = the ms of a turn x
/// 0.001 (0.1 s, not the frame's time), and the glows' frame
void Step();
/// The owners' symbols over the artefacts, from the last Step; draws nothing and may be called several times a frame.
/// The centres' symbols and sparkles are their effects' atoms, drawn with the other effects (a symbol atom as its glows
/// and symbol, Particles/PlayerSymbol.h)
void Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out);
void Clear();
/// The centre's effect made as the town centre is made functional or loaded with a town, last: an earlier one deleted,
/// a new town belief effect with the centre as its target, processed at once (two steps, the first one making the
/// symbols and drawing their local random numbers). Nothing without a particle system or the game's files
void MakeEffect(entt::entity townCentre);
/// Where the centre's symbols turn about: its totem's icon's drawn place, raised by the totem's height (its icon's mesh
/// at the totem's scale) and HeightAt1. None while the centre has no totem
[[nodiscard]] std::optional<glm::vec3> SymbolBase(entt::entity townCentre, float heightAt1);
/// The centre's effect deleted, as when the town centre is deleted
void RemoveCentre(entt::entity townCentre);
/// What the town centres' effects hold now, for the debug window: the centres with one, their symbols, the symbols
/// whose fight sparks, and the sparkles
struct Counts
{
	size_t centres {0};
	size_t symbols {0};
	size_t sparking {0};
	size_t sparkles {0};
};
[[nodiscard]] Counts Count();
} // namespace openblack::particles::town_belief
