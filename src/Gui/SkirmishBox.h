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
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class BigButton;
class GameFont;
class List;
class TextDatabase;

/// The box in which the player chooses the land of a skirmish, over the swirls of the front end, after Start Skirmish
/// Game in the menu.
///
/// A box of 780 by 580 in the middle of the dialog space. "Choose a map for skirmish game." is across its top, on a light
/// grey band at half opacity. The list of the lands is under the text. Back is an arrow at the bottom left, and Start
/// Game an arrow at the bottom right. Start Game shows only while the player has picked a land. The box keeps the land
/// that the player picked when it opens again, while the list has a land at its place. Escape does nothing. The box
/// fades in over half a second and out over a fifth.
class SkirmishBox
{
public:
	enum class Choice : uint8_t
	{
		None,
		/// The player goes back to the story
		Back,
		/// The player starts a skirmish on the land picked
		Start,
	};

	static constexpr DialogRect k_Box {.min = {10, 10}, .max = {790, 590}};
	static constexpr DialogRect k_Title {.min = {40, 30}, .max = {760, 70}};
	static constexpr DialogRect k_List {.min = {100, 110}, .max = {700, 490}};
	static constexpr glm::ivec2 k_BackPosition {30, 530};
	static constexpr glm::ivec2 k_StartPosition {730, 530};
	static constexpr int k_ArrowSize = 40;

	SkirmishBox(const TextDatabase& texts, const GameFont& font);
	SkirmishBox(const SkirmishBox&) = delete;
	SkirmishBox& operator=(const SkirmishBox&) = delete;
	SkirmishBox(SkirmishBox&&) = delete;
	SkirmishBox& operator=(SkirmishBox&&) = delete;
	~SkirmishBox();

	/// Brings the box up with the names of the lands, in the order of the list
	void Show(std::vector<std::u16string> lands);
	/// Every dialog is put away: the box fades out as it does when it closes, with no land started
	void PutAway();
	/// The box shows and takes the input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// The box shows, or it fades out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void KeyDown(int key);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// What the player chose, and for Start the place in the list of the land picked
	struct Answer
	{
		Choice choice {Choice::None};
		std::optional<size_t> land;
	};
	/// What the player chose, once
	[[nodiscard]] Answer TakeAnswer() noexcept { return std::exchange(_answer, Answer {}); }
	/// The place in the list of the land that the player picked, if there is one
	[[nodiscard]] std::optional<size_t> GetSelection() const noexcept;
	/// Picks a land of the list, as a click on it does
	void Select(std::optional<size_t> selection);
	[[nodiscard]] const List& GetList() const noexcept { return *_list; }
	[[nodiscard]] bool StartShows() const noexcept;

private:
	void Hide();
	/// Start Game shows only while a land is picked
	void FollowSelection();

	Dialog _dialog;
	List* _list {nullptr};
	BigButton* _start {nullptr};
	Answer _answer;
	bool _active {false};
	Zoomer _fade;
};

} // namespace openblack::gui
