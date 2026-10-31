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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/OrientedText.h"
#include "3D/TempleInteriorInterface.h"

namespace openblack
{
namespace gui
{
class GameFont;
class TextDatabase;
} // namespace gui

/// The labels the game writes on the rooms' "SIGN" submeshes: the main room's over its doors, the
/// creature's room's over its four scrolls and the library's over its seven
class TempleSigns
{
public:
	/// Finds each room's signs in its mesh and reads their texts
	TempleSigns(const gui::TextDatabase& texts, const gui::GameFont& font);

	/// Draws a room's labels: each label in its colour, centred on its sign, over a shadow. The highlighted label
	/// pulses grey with the time.
	void Append(std::vector<OrientedTextVertex>& vertices, TempleRoom room, std::optional<size_t> highlighted,
	            uint32_t milliseconds) const;

	/// The main room's label to highlight for the door the cursor is over, whose sectors run as the signs do but for
	/// the wall of scrolls, sector 6: the challenge room's door, sector 7, is the last sign, 6
	[[nodiscard]] static std::optional<size_t> MainRoomSignOfDoor(std::optional<uint32_t> door);

	/// A room's labels as the game lays them out: their size and stretch before k_LabelSize and k_LabelStretch,
	/// whether they run along the sign's x axis, and each sign's submesh and text, in the order the game lists them
	struct Layout
	{
		float size;
		float stretch;
		bool alongX;
		std::vector<std::pair<std::string_view, std::string_view>> signs;
	};
	[[nodiscard]] static std::optional<Layout> LayoutOf(TempleRoom room);

	/// The labels' letters are this high for a room's size of one
	static constexpr float k_LabelSize = 2.1f;
	/// How much the labels' letters are stretched along the text for a room's stretch of one
	static constexpr float k_LabelStretch = 0.8f;
	/// The highlighted label's grey, which pulses a full round every 1024 milliseconds
	[[nodiscard]] static uint8_t HighlightGrey(uint32_t milliseconds);

private:
	struct Sign
	{
		TempleRoom room;
		std::u16string text;
		glm::u8vec3 colour;
		/// The sign's frame and box, from its mesh
		glm::mat4 frame {1.0f};
		glm::vec3 min {0.0f};
		glm::vec3 max {0.0f};
		bool found {false};
	};

	const gui::GameFont& _font;
	std::vector<Sign> _signs;
};

} // namespace openblack
