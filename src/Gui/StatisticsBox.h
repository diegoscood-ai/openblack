/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "StatisticsBoxRules.h"

namespace openblack::gui
{

class GameFont;
class TextDatabase;

namespace statistics_box
{
class BarGraph;
class LineGraph;
class CreatureViews;

/// Where a graph's name is written under it: its shadow one down and one to the right, then the name over it
struct CaptionPlaces
{
	DialogRect shadow;
	DialogRect text;
};
[[nodiscard]] CaptionPlaces CaptionPlacesOf(const DialogRect& graph);
/// A graph's name is orange while the pointer is over the graph, else white
[[nodiscard]] glm::vec4 CaptionColour(bool hovered);
/// What a line graph writes by its left edge with the pointer at a share of its height, from 0 at its foot to 1 at its
/// top: of shares, a whole percentage; of values, the value there, with two decimals when the graph's top is 10 or less
/// and else a whole number
[[nodiscard]] std::string LineReadout(bool shares, float share, LineScale scale);
} // namespace statistics_box

/// The Game Statistics box: six tabs along its top, of which the first returns to the box it was opened from and the
/// others list the players' figures, a heading, three graph boxes, a list of the tab's figures, a Back arrow and the
/// Graphic and Numeric check boxes, which work as a pair.
///
/// The figures are taken as the box opens and stay as they were while it is open; a tab that the player picks lists the same
/// figures again. A figure's row shows each player's value in their colour and the total after them, or, while Graphic
/// is ticked, one bar split between the players. A click on a row shows it in the right hand graph box. The Player Stats
/// tab shows the influence and population over the whole game in two line graphs. A click on a line graph changes it
/// from shares to values, or back. The Creature Stats tab shows each player's creature.
class StatisticsBox
{
public:
	/// The box, 780 by 500 in the middle of the dialog space
	static constexpr DialogRect k_Box = TabbedBox({780, 500});
	/// The heading across the top of the box
	static constexpr DialogRect k_Heading {.min = {40, 60}, .max = {760, 88}};
	/// The three graph boxes side by side, left to right, each 220 by 230
	static constexpr std::array<DialogRect, statistics_box::k_GraphCount> k_GraphBoxes {
	    DialogRect {.min = {40, 100}, .max = {260, 330}},
	    DialogRect {.min = {290, 100}, .max = {510, 330}},
	    DialogRect {.min = {540, 100}, .max = {760, 330}},
	};
	/// The two line graphs of Player Stats, each 470 by 100, in place of the left and middle graph boxes
	static constexpr DialogRect k_Influence {.min = {40, 100}, .max = {510, 200}};
	static constexpr DialogRect k_Population {.min = {40, 230}, .max = {510, 330}};
	/// The list of the tab's figures, 720 by 150
	static constexpr DialogRect k_List {.min = {40, 365}, .max = {760, 515}};
	/// The Back arrow at the bottom left, and the Graphic and Numeric check boxes beside it
	static constexpr glm::ivec2 k_BackAt {20, 530};
	static constexpr int k_BackSize = 40;
	static constexpr glm::ivec2 k_GraphicAt {290, 530};
	static constexpr glm::ivec2 k_NumericAt {490, 530};
	/// The tabs and the list are in small text, the heading in mid text
	static constexpr int k_TabTextSize = DialogPainter::k_SmallTextSize;
	static constexpr int k_HeadingTextSize = DialogPainter::k_MidTextSize;
	static constexpr int k_ListTextSize = DialogPainter::k_SmallTextSize;
	/// The names of the texts: the tabs' in their order, the first as opened from the in-game menu
	static constexpr std::array<std::string_view, statistics_box::k_TabCount> k_TabTexts {
	    "HELP_TEXT_DIALOG_ADDITION_88",  "HELP_TEXT_DIALOG_ADDITION_111", "HELP_TEXT_DIALOG_ADDITION_46",
	    "HELP_TEXT_DIALOG_ADDITION_138", "HELP_TEXT_DIALOG_ADDITION_87",  "HELP_TEXT_DIALOG_ADDITION_124",
	};
	/// The first tab's text opened from the end of a game
	static constexpr std::string_view k_GameOverText = "HELP_TEXT_GAME_OVER";
	static constexpr std::string_view k_HeadingText = "HELP_TEXT_DIALOG_ADDITION_61";
	static constexpr std::string_view k_BackText = "HELP_TEXT_DIALOG_TATOOBACK";
	static constexpr std::string_view k_InfluenceText = "HELP_TEXT_PAUSE_STATS_05";
	static constexpr std::string_view k_PopulationText = "HELP_TEXT_PAUSE_STATS_06";
	static constexpr std::string_view k_GraphicText = "HELP_TEXT_PAUSE_STATS_07";
	static constexpr std::string_view k_NumericText = "HELP_TEXT_PAUSE_STATS_08";
	/// The heading over the totals of a one player game
	static constexpr std::string_view k_TotalText = "HELP_TEXT_PAUSE_STATS_03";

	StatisticsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back);
	~StatisticsBox();
	StatisticsBox(const StatisticsBox&) = delete;
	StatisticsBox& operator=(const StatisticsBox&) = delete;

	/// Opens on Player Stats with the figures as they are now, for the player at this machine, its first tab named for
	/// the box it was opened from
	void Open(statistics::BoxFigures figures, PlayerNames local, bool fromGameOver);
	/// Picks a tab: its rows are listed and its graph boxes filled
	void SetTab(statistics_box::Tab tab);
	/// A click on a row shows it in the right hand graph box, unless it is shown there already
	void PickRow(size_t row);
	void SetGraphic(bool graphic);

	/// Every frame
	void Update(float deltaSeconds);

	[[nodiscard]] Dialog& GetDialog() noexcept { return *_dialog; }
	[[nodiscard]] const Dialog& GetDialog() const noexcept { return *_dialog; }
	[[nodiscard]] statistics_box::Tab GetTab() const noexcept { return _tab; }
	[[nodiscard]] const statistics_box::Page& GetPage() const noexcept { return _page; }
	[[nodiscard]] bool IsGraphic() const noexcept { return _graphic; }

private:
	void Fill(statistics_box::Graph graph, std::optional<size_t> row);
	/// Draws a row's values or bar over its name in the list
	void DrawRow(const DialogPainter& painter, const List::ItemPlace& place);

	const TextDatabase& _texts;
	const GameFont& _font;
	std::u16string _mainMenuLabel;
	std::u16string _gameOverLabel;
	std::u16string _totalLabel;
	std::unique_ptr<Dialog> _dialog;
	List* _list {nullptr};
	CheckBox* _graphicBox {nullptr};
	CheckBox* _numericBox {nullptr};
	std::array<statistics_box::BarGraph*, statistics_box::k_GraphCount> _graphs {};
	statistics_box::LineGraph* _influence {nullptr};
	statistics_box::LineGraph* _population {nullptr};
	statistics_box::CreatureViews* _creatures {nullptr};

	statistics::BoxFigures _figures;
	PlayerNames _local {PlayerNames::PLAYER_ONE};
	statistics_box::Tab _tab {statistics_box::Tab::Player};
	statistics_box::Page _page;
	/// The row last shown in a graph box
	std::optional<size_t> _shownRow;
	bool _graphic {true};

	/// The rows' bars grow once, one for each number of players; each row drawn with them makes them grow more
	mutable std::array<Zoomer, statistics::k_BoxPlayers> _rowGrowth;
	float _frameSeconds {0.0f};
	/// Whether the Total heading has been drawn this frame
	mutable bool _totalHeadingDrawn {false};
};

} // namespace openblack::gui
