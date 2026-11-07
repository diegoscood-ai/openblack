/*******************************************************************************
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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/AffineMatrix.h"
#include "Gui/VillagerLabels.h"

/// Which named villagers show their name this frame, where, and in what colour
namespace openblack::ecs::villager_label_source
{

struct Frame
{
	/// The objects whose draw the land's object draw list called this frame, in the order it called them: a villager
	/// adds its name as it is drawn
	std::span<const entt::entity> drawn;
	/// The camera as the game computes it, and the near clip a point on the screen is found with
	affine::AffineMatrix worldToClipping;
	float nearClip {0.0f};
	/// The screen in pixels
	glm::ivec2 screen {0};
	/// The names display's size this frame
	float nameFade {0.0f};
	/// Every villager shows its details (the game's switch); a villager with components::ShowsDetails shows its own
	bool allDetails {false};
	/// The game's turn, for the villagers' ages
	uint32_t turn {0};
	/// The help system has the wide screen on: no name is made
	bool wideScreen {false};
	/// The thing the player's hand holds, which is not drawn on the land
	std::optional<entt::entity> inHand;
	/// The land's number, for the players' colours
	int32_t landNumber {0};
};

/// The labels of the villagers drawn this frame whose point over the head falls on the screen, in the order they were
/// drawn: a villager's details when its own or every villager's are on, else a named villager's name
[[nodiscard]] std::vector<gui::villager_labels::Request> Gather(const Frame& frame);

} // namespace openblack::ecs::villager_label_source
