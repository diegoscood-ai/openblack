/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StatisticsBox.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <chrono>
#include <utility>

#include "Common/MachineClock.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"
#include "TextDatabase.h"

namespace openblack::gui
{

namespace statistics_box
{
namespace
{
using Justify = DialogPainter::Justify;

constexpr int k_Small = DialogPainter::k_SmallTextSize;
/// How long a graph's bars take to grow to their full size
constexpr float k_GrowSeconds = 0.5f;
/// The see-through foot of a vertical bar, and the edges of a row's bar
constexpr float k_BarFoot = static_cast<float>(0x3F) / 255.0f;
constexpr float k_RowBarEdge = static_cast<float>(0x7F) / 255.0f;
constexpr glm::vec4 k_White {1.0f};
constexpr glm::vec4 k_Shadow {0.0f, 0.0f, 0.0f, 1.0f};

std::u16string Widen(const std::string& text)
{
	return {text.begin(), text.end()};
}

/// A box of one colour at its top and another at its bottom
void FillBox(const DialogPainter& painter, glm::vec2 min, glm::vec2 max, glm::vec4 top, glm::vec4 bottom)
{
	painter.DrawShape({min, glm::vec2 {max.x, min.y}, max, glm::vec2 {min.x, max.y}}, {top, top, bottom, bottom});
}

/// The game cuts a value towards nothing to place it
int Cut(float value)
{
	return static_cast<int>(value);
}

glm::ivec2 PointerIn(const DialogRect& rect, glm::ivec2 pointer)
{
	return {std::clamp(pointer.x, rect.min.x, rect.max.x), std::clamp(pointer.y, rect.min.y, rect.max.y)};
}

/// A graph's name under it with a shadow, orange while the pointer is over the graph
void DrawCaption(const DialogPainter& painter, const DialogRect& rect, const std::u16string& caption, bool hovered)
{
	if (caption.empty())
	{
		return;
	}
	const auto places = CaptionPlacesOf(rect);
	painter.DrawTextWrapped(places.shadow, true, caption, k_Small, k_Shadow);
	painter.DrawTextWrapped(places.text, true, caption, k_Small, CaptionColour(hovered));
}

/// The value at the pointer's height, written with a shadow by the graph's left edge
void DrawReadout(const DialogPainter& painter, const DialogRect& rect, int pointerY, const std::string& text)
{
	constexpr int k_Width = 100;
	const auto words = Widen(text);
	painter.DrawText({rect.min.x - 1, pointerY - (k_Small / 2) + 1}, k_Width, Justify::Right, words, k_Small, k_Shadow);
	painter.DrawText({rect.min.x - 2, pointerY - (k_Small / 2)}, k_Width, Justify::Right, words, k_Small, k_White);
}

std::string Printed(const char* format, double value)
{
	std::array<char, 32> text {};
	std::snprintf(text.data(), text.size(), format, value);
	return {text.data()};
}
std::string PrintedWhole(const char* format, int value)
{
	std::array<char, 32> text {};
	std::snprintf(text.data(), text.size(), format, value);
	return {text.data()};
}
} // namespace

CaptionPlaces CaptionPlacesOf(const DialogRect& graph)
{
	return {
	    .shadow = {.min = {graph.min.x + 1, graph.max.y + 1}, .max = {graph.max.x + 1, graph.max.y + 1 + k_Small}},
	    .text = {.min = {graph.min.x, graph.max.y}, .max = {graph.max.x, graph.max.y + k_Small}},
	};
}

glm::vec4 CaptionColour(bool hovered)
{
	return hovered ? DialogPainter::k_HoverColour : k_White;
}

std::string LineReadout(bool shares, float share, LineScale scale)
{
	if (shares)
	{
		constexpr float k_Hundred = 100.0f;
		return PrintedWhole("%d%%", Cut(share * k_Hundred));
	}
	const auto range = scale.highest - scale.lowest;
	const auto value = std::clamp((share * range) + scale.lowest, scale.lowest, scale.highest);
	constexpr float k_DecimalsUpTo = 10.0f;
	return scale.highest <= k_DecimalsUpTo ? Printed("%0.2f", static_cast<double>(value)) : PrintedWhole("%d", Cut(value));
}

/// A graph box: one bar for each player in their colour, from a row of the list. Each bar grows to its height in half a
/// second each time it is filled
class BarGraph final: public Control
{
public:
	explicit BarGraph(DialogRect rect)
	    : Control(rect)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		painter.DrawBevelBox(rect, 1, DialogPainter::Edges::All, k_White);
		DrawCaption(painter, rect, caption, hovered);
		const auto x0 = static_cast<float>(rect.min.x);
		const auto y1 = static_cast<float>(rect.max.y);
		const auto width = static_cast<float>(rect.Width() - 4);
		const auto height = static_cast<float>(rect.Height() - 4);
		const auto range = _scale.highest - _scale.lowest;
		const auto grown = _growth.GetValue();
		if (!_values.empty())
		{
			const int barWidth = Cut(width) / static_cast<int>(_values.size());
			for (size_t i = 0; i < _values.size(); ++i)
			{
				const auto left = static_cast<float>(rect.min.x + 2 + (static_cast<int>(i) * barWidth) + 2);
				const auto right = static_cast<float>(rect.min.x + 2 + ((static_cast<int>(i) + 1) * barWidth) - 2);
				const auto top = std::clamp(Cut((y1 - 2.0f) - (height * grown * (_values[i] - _scale.lowest) / range)),
				                            rect.min.y + 2, rect.max.y - 2);
				const auto foot = Cut((y1 - 2.0f) - (height * grown * -_scale.lowest / range)) - 2;
				const auto colour = _colours[i];
				FillBox(painter, {left, static_cast<float>(top)}, {right, static_cast<float>(foot)}, colour,
				        glm::vec4(glm::vec3(colour), k_BarFoot));
			}
		}
		// The guide lines across, over the bars
		const auto step = GuideStep(range, static_cast<float>(rect.Height()));
		const auto first = static_cast<float>(static_cast<int64_t>(_scale.lowest / step)) * step;
		auto index = static_cast<int64_t>(first / step) & 1;
		for (auto value = first; value < _scale.highest; value += step, ++index)
		{
			const auto y = static_cast<float>(
			    std::clamp(Cut((y1 - 2.0f) - (height * (value - _scale.lowest) / range)), rect.min.y + 2, rect.max.y - 2));
			FillBox(painter, {x0 + 2.0f, y}, {static_cast<float>(rect.max.x - 2), y + 1.0f}, GuideColour(index, false),
			        GuideColour(index, false));
		}
		if (hovered)
		{
			const auto pointer = PointerIn(rect, _pointer);
			const auto value = std::clamp((y1 - static_cast<float>(pointer.y) - 2.0f) / height * range + _scale.lowest,
			                              _scale.lowest, _scale.highest);
			constexpr float k_WholeAbove = 10.0f;
			DrawReadout(painter, rect, pointer.y,
			            std::abs(range) > k_WholeAbove ? PrintedWhole("%d", Cut(value))
			                                           : Printed("%0.1f", static_cast<double>(value)));
		}
	}
	[[nodiscard]] bool IsInteractive() const override { return visible; }
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void Update(float deltaSeconds) override { _growth.Update(deltaSeconds); }

	/// Shows a row, or nothing; its bars grow again
	void Show(const Row* row, int32_t landNumber)
	{
		_values.clear();
		_colours.clear();
		caption.clear();
		_growth.SetPosition(0.0f);
		_growth.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_GrowSeconds);
		_scale = {};
		if (row == nullptr)
		{
			return;
		}
		_values = row->values;
		for (const auto player : row->players)
		{
			_colours.push_back(PlayerColour(player, landNumber));
		}
		_scale = VerticalScale(*row);
	}
	/// Empty, and the bars grow again, when the player picks a new tab
	void Clear() { Show(nullptr, 0); }

	std::u16string caption;

private:
	std::vector<float> _values;
	std::vector<glm::vec4> _colours;
	BarScale _scale;
	Zoomer _growth;
	glm::ivec2 _pointer {-1, -1};
};

/// A line graph of the players' histories, one line each in their colour. Shown as shares, the lines are stacked
/// bands that fill the graph; shown as values, they are lines over guide lines. A click turns one into the other.
class LineGraph final: public Control
{
public:
	LineGraph(DialogRect rect, std::u16string caption, bool shares)
	    : Control(rect)
	    , _caption(std::move(caption))
	    , _shares(shares)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		painter.DrawBevelBox(rect, 1, DialogPainter::Edges::All, k_White);
		DrawCaption(painter, rect, _caption, hovered);
		if (_shares)
		{
			DrawShares(painter);
		}
		else
		{
			DrawValues(painter);
		}
		if (hovered)
		{
			const auto pointer = PointerIn(rect, _pointer);
			const auto height = static_cast<float>(rect.Height() - 4);
			const auto share = std::clamp((static_cast<float>(rect.max.y - pointer.y) - 2.0f) / height, 0.0f, 1.0f);
			DrawReadout(painter, rect, pointer.y, LineReadout(_shares, share, _scale));
		}
	}
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void Activate(glm::ivec2 /*point*/) override { _shares = !_shares; }

	void SetLines(std::vector<std::vector<float>> lines, std::vector<glm::vec4> colours)
	{
		_lines = std::move(lines);
		_colours = std::move(colours);
		_scale = LinesScale(_lines);
	}

private:
	void DrawShares(const DialogPainter& painter) const
	{
		// Each stretch in turn, every line's band stacked on the ones before, solid at its top and clear at its foot
		for (const auto& band : ShareBands(rect.min, rect.max, _lines))
		{
			const auto solid = _colours[band.line];
			const auto clear = glm::vec4(glm::vec3(solid), 0.0f);
			painter.DrawShape({glm::vec2(band.corners[0]), glm::vec2(band.corners[1]), glm::vec2(band.corners[2]),
			                   glm::vec2(band.corners[3])},
			                  {solid, solid, clear, clear});
		}
	}

	void DrawValues(const DialogPainter& painter) const
	{
		const auto bottom = static_cast<float>(rect.max.y - 2);
		const auto height = static_cast<float>(rect.Height() - 4);
		const auto range = _scale.highest - _scale.lowest;
		// Each player's line in turn: all of its one pixel stretches, then all of their soft edges
		for (size_t l = 0; l < _lines.size(); ++l)
		{
			const auto solid = _colours[l];
			const auto clear = glm::vec4(glm::vec3(solid), 0.0f);
			for (const auto& piece : ValueLinePieces(rect.min, rect.max, _lines[l], _scale))
			{
				const auto from = glm::vec2(piece.from);
				const auto to = glm::vec2(piece.to);
				switch (piece.kind)
				{
				case LinePiece::Kind::Line:
					painter.DrawLine(piece.from, piece.to, solid);
					break;
				case LinePiece::Kind::EdgeAbove:
					painter.DrawShape({from - glm::vec2 {0.0f, 2.0f}, to - glm::vec2 {0.0f, 2.0f}, to, from},
					                  {clear, clear, solid, solid});
					break;
				case LinePiece::Kind::EdgeBelow:
					painter.DrawShape({from + glm::vec2 {0.0f, 2.0f}, to + glm::vec2 {0.0f, 2.0f}, to, from},
					                  {clear, clear, solid, solid});
					break;
				}
			}
		}
		// The guide lines across, over the lines; the line at nothing white
		const auto step = GuideStep(range, static_cast<float>(rect.Height()));
		const auto first = static_cast<float>(static_cast<int64_t>(_scale.lowest / step)) * step;
		auto index = static_cast<int64_t>(first / step);
		for (auto value = first; value <= _scale.highest; value += step, ++index)
		{
			const auto y = Cut(bottom - ((value - _scale.lowest) / range * height));
			if (y > rect.min.y + 2 && y < rect.max.y - 2)
			{
				const auto colour = GuideColour(index, true);
				FillBox(painter, {static_cast<float>(rect.min.x + 2), static_cast<float>(y)},
				        {static_cast<float>(rect.max.x - 2), static_cast<float>(y + 1)}, colour, colour);
			}
		}
	}

	std::u16string _caption;
	bool _shares;
	std::vector<std::vector<float>> _lines;
	std::vector<glm::vec4> _colours;
	LineScale _scale;
	glm::ivec2 _pointer {-1, -1};
};

/// Each player's creature on the Creature Stats tab, in a box of the player's colour
class CreatureViews final: public Control
{
public:
	CreatureViews()
	    : Control({.min = {40, 100}, .max = {530, 330}})
	{
	}

	void Draw(const DialogPainter& painter, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		// The boxes are drawn at half the box's opacity
		const auto boxes = CreatureBoxes(colours.size());
		for (size_t i = 0; i < boxes.size(); ++i)
		{
			const auto colour = glm::vec4(glm::vec3(colours[i]), 0.5f);
			FillBox(painter, glm::vec2(boxes[i].first), glm::vec2(boxes[i].second), colour, colour);
		}
		ShowCreatures(painter, boxes);
	}
	[[nodiscard]] bool IsInteractive() const override { return false; }
	[[nodiscard]] bool IsOnTop() const override { return true; }

	std::vector<glm::vec4> colours;
	/// The players whose creatures show, in the order of their boxes
	std::vector<PlayerNames> players;

private:
	/// Each player's primary creature shows over its box, turning with the machine's clock. The renderer draws them
	/// over the interface.
	void ShowCreatures(const DialogPainter& painter, const std::vector<std::pair<glm::ivec2, glm::ivec2>>& boxes) const
	{
		if (!Locator::rendererInterface::has_value() || !Locator::playerSystem::has_value())
		{
			return;
		}
		// The creatures are placed and sized in the interface's space, as on the game's default screen, and drawn over
		// its place on the window
		const auto origin = glm::vec2(painter.ToScreenPoint({0, 0}));
		const auto corner = glm::vec2(painter.ToScreenPoint(glm::ivec2(graphics::interface_creatures::k_Space)));
		graphics::interface_creatures::Request request {
		    .creatures = {},
		    .turn = CreatureTurn(machine_clock::Ticks()),
		    .viewOrigin = origin,
		    .viewSize = corner - origin,
		};
		for (size_t i = 0; i < boxes.size() && i < players.size(); ++i)
		{
			const auto creature = Locator::playerSystem::value().GetPrimaryCreature(players[i]);
			if (!creature.has_value())
			{
				continue;
			}
			const auto& [min, max] = boxes[i];
			// The feet stand nine tenths of the way down the box, in the middle across it
			const auto width = max.x - min.x;
			const auto height = max.y - min.y;
			const glm::vec2 anchor {min.x + (width / 2),
			                        static_cast<int>((static_cast<float>(height) * 0.9f) + static_cast<float>(min.y))};
			const int size = std::max(std::min(width, height), graphics::interface_creatures::k_SmallestSize);
			request.creatures.push_back({.creature = *creature, .anchor = anchor, .size = size});
		}
		Locator::rendererInterface::value().ShowInterfaceCreatures(std::move(request));
	}
};

} // namespace statistics_box

using namespace statistics_box;

StatisticsBox::StatisticsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back)
    : _texts(texts)
    , _font(font)
    , _mainMenuLabel(texts.Get(k_TabTexts.at(static_cast<size_t>(Tab::Back))))
    , _gameOverLabel(texts.Get(k_GameOverText))
    , _totalLabel(texts.Get(k_TotalText))
{
	const auto label = [&texts](Tab tab) { return std::u16string(texts.Get(k_TabTexts.at(static_cast<size_t>(tab)))); };
	std::vector<Dialog::Tab> tabs {
	    {.label = _mainMenuLabel, .onSelect = back},
	    {.label = label(Tab::Player), .onSelect = [this] { SetTab(Tab::Player); }},
	    {.label = label(Tab::Creature), .onSelect = [this] { SetTab(Tab::Creature); }},
	    {.label = label(Tab::Town), .onSelect = [this] { SetTab(Tab::Town); }},
	    {.label = label(Tab::Miracle), .onSelect = [this] { SetTab(Tab::Miracle); }},
	    {.label = label(Tab::Tech), .onSelect = [this] { SetTab(Tab::Tech); }},
	};
	// The box's tabs are labelled in small text
	for (auto& tab : tabs)
	{
		tab.textSize = k_TabTextSize;
	}
	_dialog = std::make_unique<Dialog>(std::move(tabs), static_cast<size_t>(Tab::Player), k_Box);
	auto& dialog = *_dialog;

	// A dark band under the heading, drawn before the box at half its opacity
	dialog.drawUnder = [this](const DialogPainter& painter) {
		_totalHeadingDrawn = false;
		constexpr float k_Band = static_cast<float>(0x20) / 255.0f;
		const glm::vec4 colour {k_Band, k_Band, k_Band, 0.5f};
		painter.DrawShape({glm::vec2 {40, 60}, glm::vec2 {760, 60}, glm::vec2 {760, 88}, glm::vec2 {40, 88}},
		                  {colour, colour, colour, colour});
	};
	dialog.Add<StaticText>(k_Heading, std::u16string(texts.Get(k_HeadingText)), StaticText::Layout::Centre, k_HeadingTextSize);
	dialog
	    .Add<BigButton>(font, k_BackAt, k_BackSize, std::u16string(texts.Get(k_BackText)), BigButton::LabelSide::Right,
	                    BigButton::Look::LeftArrow)
	    .onClick = back;
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		_graphs.at(i) = &dialog.Add<BarGraph>(k_GraphBoxes.at(i));
	}
	_influence = &dialog.Add<LineGraph>(k_Influence, std::u16string(texts.Get(k_InfluenceText)), true);
	_population = &dialog.Add<LineGraph>(k_Population, std::u16string(texts.Get(k_PopulationText)), false);
	_list = &dialog.Add<List>(font, k_List, k_ListTextSize, false, true, true);
	_list->drawOver = [this](const DialogPainter& painter, const List::ItemPlace& place) { DrawRow(painter, place); };
	_list->onClick = [this] {
		if (const auto row = _list->GetSelection(); row.has_value())
		{
			PickRow(*row);
		}
	};
	_graphicBox = &dialog.Add<CheckBox>(font, k_GraphicAt, std::u16string(texts.Get(k_GraphicText)), true);
	_numericBox = &dialog.Add<CheckBox>(font, k_NumericAt, std::u16string(texts.Get(k_NumericText)), false);
	_graphicBox->onChange = [this](bool) { SetGraphic(true); };
	_numericBox->onChange = [this](bool) { SetGraphic(false); };
	_creatures = &dialog.Add<CreatureViews>();
}

StatisticsBox::~StatisticsBox() = default;

void StatisticsBox::Open(statistics::BoxFigures figures, PlayerNames local, bool fromGameOver)
{
	_figures = std::move(figures);
	_local = local;
	_dialog->SetTabLabel(0, fromGameOver ? _gameOverLabel : _mainMenuLabel);
	// It opens on Player Stats, as it was. Its graphs do not grow again
	_tab = Tab::Player;
	SetTab(Tab::Player);
	_dialog->Reset();
}

void StatisticsBox::SetTab(Tab tab)
{
	_dialog->SelectTab(static_cast<size_t>(tab));
	if (tab != _tab)
	{
		for (auto* graph : _graphs)
		{
			graph->Clear();
		}
	}
	_tab = tab;
	_page = MakePage(tab, _figures, _local);
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		_graphs.at(i)->visible = _page.graphShown.at(i);
	}
	_influence->visible = _page.lineGraphsShown;
	_population->visible = _page.lineGraphsShown;
	_creatures->visible = _page.creaturesShown;

	std::vector<List::Item> items;
	items.reserve(_page.rows.size());
	for (const auto& row : _page.rows)
	{
		items.push_back({.text = std::u16string(_texts.Get(row.label)), .mouseCell = std::nullopt, .mouseMirrored = false});
	}
	_list->SetItems(std::move(items));
	_shownRow.reset();
	// The graph boxes are filled in the order the rows are listed; the right one, emptied on the Creature Stats tab,
	// stays empty
	std::vector<std::pair<size_t, Graph>> fills;
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		if (const auto row = _page.graphs.at(i); row.has_value())
		{
			fills.emplace_back(*row, static_cast<Graph>(i));
		}
	}
	std::ranges::sort(fills);
	for (const auto& [row, graph] : fills)
	{
		Fill(graph, row);
	}
	if (tab == Tab::Creature)
	{
		_graphs.at(static_cast<size_t>(Graph::Right))->Clear();
	}
	_list->SetSelection(_page.selected);
	_shownRow = _page.selected;

	// The line graphs: each player's influence and population over the whole game
	std::vector<std::vector<float>> influence;
	std::vector<std::vector<float>> population;
	std::vector<glm::vec4> colours;
	for (const auto& player : _figures.players)
	{
		influence.push_back(player.influenceHistory);
		population.push_back(player.populationHistory);
		colours.push_back(PlayerColour(player.player, _figures.landNumber));
	}
	_influence->SetLines(std::move(influence), colours);
	_population->SetLines(std::move(population), colours);
	_creatures->colours = std::move(colours);
	_creatures->players.clear();
	for (const auto& player : _figures.players)
	{
		_creatures->players.push_back(player.player);
	}
}

void StatisticsBox::Fill(Graph graph, std::optional<size_t> row)
{
	auto& box = *_graphs.at(static_cast<size_t>(graph));
	box.Show(row.has_value() && *row < _page.rows.size() ? &_page.rows.at(*row) : nullptr, _figures.landNumber);
	if (row.has_value() && *row < _page.rows.size())
	{
		box.caption = std::u16string(_texts.Get(_page.rows.at(*row).label));
	}
	_shownRow = row;
	_list->SetSelection(row);
}

void StatisticsBox::PickRow(size_t row)
{
	if (!ShowsPickedRow(row, _shownRow))
	{
		return;
	}
	Fill(Graph::Right, row);
}

void StatisticsBox::SetGraphic(bool graphic)
{
	_graphic = graphic;
	_graphicBox->SetChecked(graphic);
	_numericBox->SetChecked(!graphic);
}

void StatisticsBox::Update(float deltaSeconds)
{
	_frameSeconds = deltaSeconds;
}

void StatisticsBox::DrawRow(const DialogPainter& painter, const List::ItemPlace& place)
{
	if (place.index >= _page.rows.size())
	{
		return;
	}
	const auto& row = _page.rows.at(place.index);
	const auto values = row.values.size();
	if (DrawnAsBar(row, _graphic))
	{
		// One bar split between the players, over the right hand 300 of the row
		const auto top = place.shownTop + 2;
		const auto bottom = place.shownBottom - 2;
		if (place.shownBottom - place.shownTop <= 10 || values < 1 || values > statistics::k_BoxPlayers)
		{
			return;
		}
		auto& growth = _rowGrowth.at(values - 1);
		if (growth.GetDuration() == 0.0f && growth.GetValue() == 0.0f)
		{
			growth.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_GrowSeconds);
		}
		growth.Update(_frameSeconds);
		const DialogRect bar {.min = {place.right - k_ValuesWidth, top}, .max = {place.right - 4, bottom}};
		painter.DrawBevelBox(bar, 1, DialogPainter::Edges::All, glm::vec4(1.0f));
		const auto parts = BarParts(row);
		const auto left = static_cast<float>(bar.min.x + 2);
		const auto width = static_cast<float>(bar.Width() - 4);
		const auto middle = static_cast<float>((bar.min.y + bar.max.y) / 2);
		auto start = left;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			const auto end = static_cast<float>(
			    std::clamp(static_cast<int>(left + (width * growth.GetValue() * parts[i])), bar.min.x + 2, bar.max.x - 2));
			auto solid = PlayerColour(row.players[i], _figures.landNumber);
			auto edge = glm::vec4(glm::vec3(solid), k_RowBarEdge);
			if (row.values[i] < 0.0f)
			{
				std::swap(solid, edge);
			}
			FillBox(painter, {start, static_cast<float>(bar.min.y + 2)}, {end, middle}, edge, solid);
			FillBox(painter, {start, middle}, {end, static_cast<float>(bar.max.y - 2)}, solid, edge);
			start = end;
		}
		// The guide lines down the bar, every fourth brighter
		float scale = row.most;
		if (values != 1 || scale <= 0.0f)
		{
			scale = 0.0f;
			for (const auto value : row.values)
			{
				scale += std::abs(value);
			}
			scale = scale == 0.0f ? 1.0f : scale;
		}
		const auto step = GuideStep(scale, static_cast<float>(bar.Width()));
		int64_t index = 1;
		for (auto value = step; value < scale; value += step, ++index)
		{
			const auto x = static_cast<float>(Cut(left + (width * value / scale)));
			const auto colour = GuideColour(index, false);
			FillBox(painter, {x, static_cast<float>(bar.min.y + 2)}, {x + 1.0f, static_cast<float>(bar.max.y - 2)}, colour,
			        colour);
		}
		return;
	}

	// Numbers: each player's value in their colour, then the total; with one value, the total alone
	const auto y = place.top + 2;
	const auto numbers = NumbersOf(row, place.right);
	const auto width = numbers.columnWidth;
	for (const auto& number : numbers.numbers)
	{
		painter.DrawText({number.centre, y}, width * 5, Justify::Centre, Widen(number.text), k_Small,
		                 number.player.has_value() ? PlayerColour(*number.player, _figures.landNumber) : k_TotalColour);
	}
	// "Total" over the first row of one value shown
	if (numbers.totalHeading && !_totalHeadingDrawn)
	{
		_totalHeadingDrawn = true;
		painter.DrawText({ColumnCentre(place.right, values, values), place.top - k_Small}, width * 5, Justify::Centre,
		                 _totalLabel, k_Small, k_TotalColour);
	}
}

} // namespace openblack::gui
