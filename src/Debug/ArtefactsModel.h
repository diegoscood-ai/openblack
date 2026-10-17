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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// What the Artefacts debug window works out before it shows anything: the rows of the artefacts on the land and how
// each reads. Pure functions and small value types, tested with hand-made data.

namespace openblack::debug::artefacts
{

/// An artefact on the land, as the window reads it
struct ArtefactAt
{
	entt::entity entity {entt::null};
	entt::entity town {entt::null};
	entt::entity site {entt::null};
	bool inTown {false};
	bool inSite {false};
	std::optional<PlayerNames> player;
	float value {0.0f};
	/// Whether its dance has started since its town last looked, and the dance's dancers (none without a dance)
	bool dancing {false};
	std::optional<uint32_t> dancers;
};

/// How an artefact's worth shows: plain, glowing, or glowing with its owner's symbol over it
[[nodiscard]] std::string_view LookOf(float value);

/// Where an artefact belongs now: its town, its worship site, or nowhere (held in a hand)
[[nodiscard]] std::string WhereOf(const ArtefactAt& artefact);

/// One artefact as the window lists it
struct Row
{
	entt::entity entity {entt::null};
	std::string where;
	std::string owner;
	float value {0.0f};
	std::string_view look;
	std::string dance;
};

/// The rows of the artefacts, oldest entity first, so the list keeps its order from frame to frame
[[nodiscard]] std::vector<Row> Rows(std::span<const ArtefactAt> artefacts);

/// What the window's "+1 worth" button gives an artefact: one more, so its glow and symbol can be seen without a dance
inline constexpr float k_WorthStep = 1.0f;

} // namespace openblack::debug::artefacts
