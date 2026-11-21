/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A town centre's totem, on the entity of its plinth (the tribe's BuildingPlayerIconPlinth mesh, its totem statue
/// info). `icon` is the entity of the icon standing on it: the player's creature (BuildingPlayerIcon<Species>) or,
/// without a creature, the hand (BuildingSpellHand). Both rise out of the ground with the town's worship
/// (village_totem::RiseOf), the icon village_totem::k_IconAbovePlinth above the plinth.
struct VillageTotem
{
	entt::entity townCentre {entt::null};
	entt::entity icon {entt::null};
	float restY {0.0f}; ///< the plinth's y with no worship
	float rise {0.0f};  ///< the rise it was last placed at, 0 .. village_totem::k_RiseAtFullShare
	/// The share the hand holds it at; it is the share it was last set to while the hand doesn't hold it
	float held {0.0f};
	/// Whether the hand holds it
	bool gripped {false};
	/// Whether its town centre has it as its totem: not yet while it is being made, so that its town's share set as it
	/// is made does not reach it
	bool onCentre {true};
	/// The second, see-through totem at the other share, its plinth and its icon: there only while the two shares stand
	/// apart. A picture only, which the hand never picks
	entt::entity ghost {entt::null};
	entt::entity ghostIcon {entt::null};
	/// Its next re-place forces no share tooltip: set when its icon changes with its town's player's creature
	bool quietOnce {false};
	/// How much of it is built, 0..1: it is always made built, and only a script's BUILT_PERCENTAGE changes it
	/// (village_totem::PercentBuiltSetTo). It is never marked as under construction, so this alone says whether it is
	/// built (village_totem::IsBuilt)
	float percentBuilt {1.0f};
};

} // namespace openblack::ecs::components
