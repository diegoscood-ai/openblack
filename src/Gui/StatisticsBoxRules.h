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
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "ECS/StatisticsRules.h"
#include "Enums.h"

/// The rules of the Game Statistics box: the rows each tab lists, how a row writes its values or draws its bar, and
/// how the graphs are scaled and laid out. Free of the game's state so they can be tested on their own.
namespace openblack::gui::statistics_box
{

/// The box's tabs: the first returns to the box it was opened from
enum class Tab : uint8_t
{
	Back,
	Player,
	Creature,
	Town,
	Miracle,
	Tech,
	_Count
};
inline constexpr size_t k_TabCount = static_cast<size_t>(Tab::_Count);

/// How a row writes its value
enum class ShowMost : uint8_t
{
	/// The value alone
	Number,
	/// The value out of the row's most, "value/most"
	OutOfMost,
	/// The value as a share of the row's most, in hundredths with a percent sign, and no total
	Percent,
	/// The value alone; the row's most is the highest player's value
	NumberToHighest,
};

/// How a row is shown
enum class Display : uint8_t
{
	Bar,
	Number,
	/// A bar while Graphic is ticked, numbers while Numeric is
	AsTicked,
};

/// One row of the box's list: its label and a value for each player shown, or one value of the game's own
struct Row
{
	/// The name of its text
	std::string label;
	std::vector<float> values;
	/// Whose colour each value is drawn in
	std::vector<PlayerNames> players;
	float most {0.0f};
	int decimals {0};
	ShowMost show {ShowMost::NumberToHighest};
	Display display {Display::AsTicked};
};

/// The three graph boxes along the top of the box, left to right
enum class Graph : uint8_t
{
	Left,
	Middle,
	Right,
};
inline constexpr size_t k_GraphCount = 3;

/// A tab's rows, the rows its graph boxes show, and the row picked in the list
struct Page
{
	std::vector<Row> rows;
	std::array<std::optional<size_t>, k_GraphCount> graphs {};
	std::optional<size_t> selected;
	/// The graph boxes shown: the player tab shows the two line graphs in place of the left and middle boxes, and the
	/// creature tab its creatures
	std::array<bool, k_GraphCount> graphShown {true, true, true};
	bool lineGraphsShown {false};
	bool creaturesShown {false};
};

/// The tab's rows from the figures. The single figures are drawn in the local player's colour.
[[nodiscard]] Page MakePage(Tab tab, const statistics::BoxFigures& figures, PlayerNames local);

/// Whether a click on the list shows the row it picked in the right hand graph box: only with a row picked, and not
/// when it is the row the graph boxes last took
[[nodiscard]] constexpr bool ShowsPickedRow(std::optional<size_t> picked, std::optional<size_t> shown)
{
	return picked.has_value() && picked != shown;
}

/// A value as the box keeps it: held between minus and plus 10^20 for the rows of decimal figures
[[nodiscard]] float Held(float value);

/// A value written as its row writes it
[[nodiscard]] std::string Format(const Row& row, float value);
/// The players' values added up, which a row writes after them; none for a percent row
[[nodiscard]] std::optional<float> Total(const Row& row);
/// Whether a row is drawn as a bar
[[nodiscard]] bool DrawnAsBar(const Row& row, bool graphicTicked);

/// The values' columns along a row's right hand 300: one for each value and one for the total, each value's text
/// centred in it. With one value only the total's column is drawn.
inline constexpr int k_ValuesWidth = 300;
[[nodiscard]] int ColumnWidth(size_t values);
/// The centre of a column from a row's right edge
[[nodiscard]] int ColumnCentre(int right, size_t values, size_t column);

/// One of the numbers a row writes while it is not drawn as a bar: the centre of its column, its text, and the player
/// whose colour it is in, none for the total
struct RowNumber
{
	int centre {0};
	std::string text;
	std::optional<PlayerNames> player;
};
/// The numbers a row writes from its right edge: with more than one value each value in its player's column, then the
/// total after them unless the row is a percent row. A row of one value writes no player's column, and the "Total"
/// heading goes over its column once.
struct RowNumbers
{
	std::vector<RowNumber> numbers;
	int columnWidth {0};
	bool totalHeading {false};
};
[[nodiscard]] RowNumbers NumbersOf(const Row& row, int right);

/// How far along a row's bar each value's part ends, 0 to 1, each from where the one before ended. With one value the
/// bar is out of the row's most; with more, out of the values added up, so the parts fill the bar.
[[nodiscard]] std::vector<float> BarParts(const Row& row);

/// A graph box's bars, one for each value: their scale, the row's most with one value and the highest value with more,
/// and the lowest value under nothing, or nothing
struct BarScale
{
	float lowest {0.0f};
	float highest {1.0f};
};
[[nodiscard]] BarScale VerticalScale(const Row& row);

/// The gap between the graphs' guide lines: from a quarter, doubled until the lines are at least about four units
/// apart over the length given
[[nodiscard]] float GuideStep(float range, float length);
/// Every fourth guide line is brighter; with value lines the line at nothing is opaque white
[[nodiscard]] glm::vec4 GuideColour(int64_t index, bool zeroWhite);

/// The players' colours, by their place, as some lands give them
[[nodiscard]] glm::vec4 PlayerColour(PlayerNames player, int32_t landNumber);
/// The colour of the totals and their heading
inline constexpr glm::vec4 k_TotalColour {208.0f / 255.0f, 208.0f / 255.0f, 208.0f / 255.0f, 1.0f};

/// The boxes the creatures are drawn in, in player order, for so many players
[[nodiscard]] std::vector<std::pair<glm::ivec2, glm::ivec2>> CreatureBoxes(size_t players);
/// The creatures turn round once every 8,192 milliseconds of the machine's clock, all together: the angle at a time
[[nodiscard]] float CreatureTurn(uint32_t milliseconds);

/// The line graphs' scale: from the lowest point of any line to the highest, one apart when they are the same
struct LineScale
{
	float lowest {0.0f};
	float highest {1.0f};
};
[[nodiscard]] LineScale LinesScale(const std::vector<std::vector<float>>& lines);

/// One piece of a value line graph's line: the line itself between two points, one pixel wide, or a soft edge that
/// fades from the line to two units above or below it
struct LinePiece
{
	enum class Kind : uint8_t
	{
		Line,
		EdgeAbove,
		EdgeBelow,
	};
	Kind kind {Kind::Line};
	glm::ivec2 from {0};
	glm::ivec2 to {0};

	bool operator==(const LinePiece&) const = default;
};
/// A value line graph's line in the graph box from min to max, in the order it is drawn: every stretch of the line
/// first, then the two soft edges of each stretch. The points are spaced evenly from two units in, the place of each
/// a running sum cut to the whole unit, and each held four units in from the top and the bottom.
[[nodiscard]] std::vector<LinePiece> ValueLinePieces(glm::ivec2 min, glm::ivec2 max, std::span<const float> line,
                                                     LineScale scale);

/// One band of a share line graph between two neighbouring points: the line whose share it is, and its corners round
/// it, the top left and right at the share's height (solid) and the bottom right and left where the band under it
/// ended (clear)
struct ShareBand
{
	size_t line {0};
	std::array<glm::ivec2, 4> corners {};

	bool operator==(const ShareBand&) const = default;
};
/// A share line graph's bands in the graph box from min to max, in the order they are drawn: for each stretch between
/// two points, every line's band, stacked from the bottom on the ones before. The fewest points of any line are drawn.
/// The points are spaced evenly from two units in, the place of each a running sum cut to the whole unit, as the
/// value lines
[[nodiscard]] std::vector<ShareBand> ShareBands(glm::ivec2 min, glm::ivec2 max, const std::vector<std::vector<float>>& lines);

} // namespace openblack::gui::statistics_box
