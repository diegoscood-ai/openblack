# Statistics

The game counts what each god and their people do and shows it in a Game Statistics box: six tabs of figures, a
list of every figure with each player's value side by side, bar graphs comparing the players, line graphs of influence
and population over the whole game, and on the creature tab each player's creature turning in 3D. This file is the
box and how it is reached; what each figure counts, when, and what is saved is in
[statistics_counted.md](statistics_counted.md).

**Progress: 20/45 done, 0 chosen, 20 partial, 3 inferred — 83%**

How the original does it, in our wiki: [Screen, text and read-outs](../../../interface/screen-and-readouts.md).

## Getting there

Story lands and the end of the story show no statistics screen of their own, and no key opens the box: it is reached
only from the in-game menu and from the end-of-game box of a network or skirmish game. Other places show some of the
same figures in their own way: the temple's world scroll ([../temple/main_room.md](../temple/main_room.md)), the
creature cave's scrolls ([../creature/creature_cave.md](../creature/creature_cave.md)), the creature's status panel
([../creature/creature_mode.md](../creature/creature_mode.md)) and the creature's web page
([../creature/lessons_and_help.md](../creature/lessons_and_help.md)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The in-game menu's Statistics tab opens the Game Statistics box | done | `GameMenu::OpenStatistics` (`src/Gui/GameMenu.cpp`), `gui::StatisticsBox` (raffclar's PR #525); test `GameMenu.ButtonsActWhenLetGoOverTheOneTheyWentDownOn` |
| The box's first tab reads "Main Menu" and goes back to the in-game menu | done | `src/Gui/StatisticsBox.cpp`; the first tab's label is picked by the box it was opened from, checked against the exe's set-up of the box (2026-10-21); test `GameMenu.ButtonsActWhenLetGoOverTheOneTheyWentDownOn` |
| A "Back" button at the bottom left closes the box | partial 90% | the big button at 20, 530 (`src/Gui/StatisticsBox.cpp`), checked against the exe's set-up of the box (2026-10-21); no test |
| The end-of-game box of a network or skirmish game has a "Statistics" tab that opens the same box | pending | the end-of-game box itself (won or lost, scores, credits, Watch Game, Leave Game) is the multiplayer domain's: [../multiplayer/network_play.md](../multiplayer/network_play.md), [../multiplayer/skirmish.md](../multiplayer/skirmish.md); when each version of it opens: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| Opened from the end of a game, the box's first tab reads "Game Over" and goes back to the end-of-game box | pending |  |

## The box

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The box is built from the menu's dialog controls: tab buttons, a heading, a list, check boxes and a big button | partial 90% | `src/Gui/StatisticsBox.cpp` on `Dialog`'s box of its own size, the same controls as the exe's box (checked against the exe's set-up of the box (2026-10-21)); no test of the set-up |
| Laid out in the menu's 800 by 600 space and scaled with it, like every dialog | done | `StatisticsBox::k_Box` = `TabbedBox({780, 500})`; test `StatisticsBoxLayout.ABoxWithTabsIsFortyDeeperBelowTheMiddle`; the box's own size is passed in by its caller in the exe, not read |
| Six tabs along the top in small text: "Main Menu" (or "Game Over"), "Player Stats", "Creature Stats", "Town Stats", "Miracle Stats" and "Tech Stats" | partial 90% | `StatisticsBox` (`src/Gui/StatisticsBox.cpp`), six tab buttons in small text, the first named for the box it was opened from, checked against the exe's set-up of the box (2026-10-21); no test of the labels |
| It opens on Player Stats | partial 90% | `StatisticsBox::Open`, checked against the exe's set-up of the box (2026-10-21); no test |
| A heading, "Game Statistics", across the top of the box in medium text | partial 90% | a `StaticText` in mid text at 40..760 by 60..88 (`src/Gui/StatisticsBox.cpp`), checked against the exe's set-up of the box (2026-10-21); no test |
| Below the heading, an area for graphs: three boxes side by side, each 220 by 230, at 40, 290 and 540 across | partial 90% | three `BarGraph` boxes at 40, 290 and 540, 100 down, 220 by 230, checked against the exe's set-up of the box (2026-10-21) (xref note #969); no test |
| Below the graphs, a list 720 wide and 150 tall of the tab's figures, in small text, scrolling with a bar and the wheel | partial 90% | the menu's `List` at 40..760 by 365..515 in small text, checked against the exe's set-up of the box (2026-10-21); no test of the box's list |
| At the bottom, the "Back" button on the left and two check boxes, "Graphic" and "Numeric", which work as a pair: ticking one unticks the other | partial 80% | `StatisticsBox::SetGraphic` keeps the pair; Graphic (at 290) starts ticked and Numeric (at 490) not, checked against the exe's set-up of the box (2026-10-21); the pairing on a click is not read in the exe; no test |
| The figures are taken once, as the box opens, and don't change while it stays open: picking a tab lists the same figures again | partial 90% | `player_statistics::Box()` as the box opens (`StatisticsBox::Open`); in the exe the box takes the figures once as its controls are made and every tab reads them; no test. (Until 2026-10-21 this row said the figures were taken again at each tab, which the exe does not do) |
| Only the first four players are shown: human and computer gods, never the neutral player | partial 90% | `statistics::k_BoxPlayers` (`src/ECS/Statistics.cpp`); the exe's figures are those of players 0 to 3, at most the number in the game; no test |
| The controls light up and click like the rest of the menu's | done | the menu's dialog controls (`src/Gui/Controls.cpp`), the exe's box using the same controls as its other dialogs; see [main_menu.md](main_menu.md) |

## The list of figures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each row has the figure's name on the left and the players' values in the right-hand 300 pixels of the row | done | `statistics_box::k_ValuesWidth`, `ColumnCentre` (`src/Gui/StatisticsBoxRules.h`), checked against the exe's row draw (xref notes #967, #968); test `StatisticsBoxLayout.ColumnsShareTheRightHand300` |
| With Numeric ticked, each player's value is written in that player's colour, one column each | partial 90% | `StatisticsBox::DrawRow` in `PlayerColour`, checked against the exe's row draw (xref notes #967, #968): 300 pixels shared by the players and the total; the draw itself has no test |
| After the players' columns a Total column in white adds them up; it is left out for figures shown as percentages | done | `statistics_box::Total`, none for a percent row, checked against the exe's row draw (xref notes #967, #968); test `StatisticsBoxRows.PercentRowsHaveNoTotal` |
| In a one-player game only the Total column is drawn, with a "Total" heading over it | partial 90% | `StatisticsBox::DrawRow`, checked against the exe's row draw (xref notes #967, #968): with one player no player column, the total and the heading drawn once; no test |
| With Graphic ticked, a figure is drawn instead of numbers as one horizontal bar split into the players' parts, each in the player's colour | done | `StatisticsBox::DrawRow`, `statistics_box::BarParts`: one split bar, checked against the exe's row draw (xref notes #967, #968): one stacked bar over the row's right-hand 300 pixels, one part per player in its colour; test `StatisticsBoxLayout.ARowsBarIsSplitBetweenThePlayers`. Not compared: the bar's growth speed, its colours and the guide lines' step |
| Some figures are always written as numbers, whatever is ticked: the alignment changes, the creature's alignment change and every Tech Stats figure | done | `Display::Number` rows (`src/Gui/StatisticsBoxRules.cpp`); the exe's row draw keeps such a row to numbers (checked against the exe's row draw (xref notes #967, #968)), and its tab builder marks the same rows (checked against the exe's tab builder (the Creature and Tech tabs read row by row, 2026-10-21)); test `StatisticsBoxRows.SingleFiguresAreAlwaysNumbersInTheLocalColour` |
| Values are written as whole numbers, as "value/most" (a value out of a maximum) or as a percentage of a maximum | done | `statistics_box::Format`, `ShowMost`, checked against the exe's row draw (xref notes #967, #968) (a percentage is the value over the row's maximum times 100); tests `StatisticsBoxRows.ARowOutOfItsHighestTakesTheHighestValue`, `StatisticsBoxRows.MenAndWomenAreOutOfTheFirstPlayersPeople` |
| Counts have no decimals; alignments, growth, satisfaction and the creature's damage, hunger and tiredness have two; chants and frame rates have one | done | each row's `decimals` (`src/Gui/StatisticsBoxRules.cpp`), checked against the exe's tab builder (the Creature and Tech tabs read row by row, 2026-10-21); test `StatisticsBoxRows.DecimalsAndHeldValues` |
| A row's bar is scaled to the figure's maximum when one player is shown; with more, the players' parts fill it | done | `statistics_box::BarParts`, checked against the exe's row draw (xref notes #967, #968) (the scale is the row's maximum with one player, else none, so the parts share the whole bar); test `StatisticsBoxLayout.ARowsBarIsSplitBetweenThePlayers`. (Until 2026-10-21 this row said a bar's length was the largest player's value, which the exe does not do) |
| Values are held between minus and plus 10^20 before they are shown | done | `statistics_box::Held`, as the exe's row makers clamp them; test `StatisticsBoxRows.DecimalsAndHeldValues` |
| Clicking a row shows that figure in the right-hand graph box as bars, one per player; clicking the row already shown does nothing | partial 90% | `StatisticsBox::PickRow`; the exe's row-to-box copy fills the right-hand box by default and picks the row in the list; that the same row does nothing is not read in the exe; test `StatisticsBoxPages.TheGraphBoxesAndThePickedRowFollowTheRowsCopied` covers the copy |

## The bar graphs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each graph box holds one vertical bar per player in the player's colour | done | `statistics_box::BarGraph` (`src/Gui/StatisticsBox.cpp`); the exe's three boxes are vertical bar graphs, given one bar per player by its row-to-box copy (xref note #969); test `StatisticsBoxLayout.GraphBoxesScaleToTheirMostOrTheTallest`. The bars' own drawing (growth, guide lines) is not compared |
| With one player, the bar is scaled to the figure's own maximum; with more, to the tallest player | done | `statistics_box::VerticalScale`, as the exe's row-to-box copy sets the scale; test `StatisticsBoxLayout.GraphBoxesScaleToTheirMostOrTheTallest` |
| Picking Town Stats fills its graphs with Average Satisfaction Of Villagers, Total Buildings Built and Final Total Population | done | `MakePage` (`src/Gui/StatisticsBoxRules.cpp`), checked against the exe's tab builder (the Creature and Tech tabs read row by row, 2026-10-21); test `StatisticsBoxPages.TheGraphBoxesAndThePickedRowFollowTheRowsCopied` |
| Miracle Stats fills them with the aggressive, nice and creature miracle totals | done | `MakePage`, checked against the exe's tab builder (the Creature and Tech tabs read row by row, 2026-10-21); test `StatisticsBoxPages.EachTabListsItsRowsInOrder` |
| Tech Stats fills them with Min Frame Rate, Max Frame Rate and Total Lines Of Code Executed | done | `TechPage` in `src/Gui/StatisticsBoxRules.cpp`: lines of code to the right box, the highest frame rate to the middle and the lowest to the left, as the exe's Tech tab; test `StatisticsBoxPages.TheGraphBoxesAndThePickedRowFollowTheRowsCopied` |
| Player Stats shows Final Area Of Influence on the right, with the two line graphs in place of the other two boxes | done | `MakePage`, `Page::lineGraphsShown`; test `StatisticsBoxPages.TheGraphBoxesAndThePickedRowFollowTheRowsCopied`; the line graphs' places are the exe's (checked against the exe's set-up of the box (2026-10-21)) |
| Creature Stats shows the creatures in place of the first two boxes; the right-hand box stays empty until a row is clicked | partial 60% | the exe's Creature tab puts the battles fought and won into the two hidden left boxes and leaves the right one empty, as `MakePage` does (test `StatisticsBoxPages.TheGraphBoxesAndThePickedRowFollowTheRowsCopied`); the creatures are not drawn: `RendererInterface::ShowInterfaceCreatures` draws nothing |

## The line graphs (Player Stats)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two graphs on the left, each 470 by 100: "Influence" above and "Population" below | partial 90% | two `LineGraph`s at 40..510, 100 and 230 down (`src/Gui/StatisticsBox.cpp`), checked against the exe's set-up of the box (2026-10-21); no test |
| Population draws one line per player in the player's colour across the whole game so far, two pixels thick | partial 80% | `statistics_box::LineGraph`; test `StatisticsBoxLayout.LineGraphsRunFromTheLowestPointToTheHighest`; the exe's line graph draws its lines with one line call, its thickness not read |
| Influence stacks the players' shares of the summed influence at each moment, filled bands that always reach the top | partial 70% | `statistics_box::LineGraph`; a click turns the shares into values and back; the exe's line graph fills bands with quads (its stacking rule not read); no test of the stacking |
| Each graph is a bevelled box with its name written under its bottom edge with a shadow; the name lights up when the pointer is over the graph | partial 90% | `DrawCaption` (`src/Gui/StatisticsBox.cpp`); the exe's line graph draws a bevelled box and its name in the shadow and then the normal or highlight colour; no test |
| Population has horizontal guide lines: their step starts at a quarter and doubles until they are spread out, every fourth line brighter and the zero line white | done | `statistics_box::GuideStep`, `GuideColour`; test `StatisticsBoxLayout.GuideLinesDoubleUntilSpreadOut`; the exe's line graph uses the same constants (its 4 and 1), the step's loop read only in part |
| With the pointer over a graph, the value at the pointer's height is written by its left edge: Population with two decimals when the graph's top is 10 or less, else a whole number; Influence as a percentage | partial 90% | `DrawReadout` in `src/Gui/StatisticsBox.cpp`; the exe's line graph writes the value with its 10 limit and a whole percentage; no test |
| The graphs cover the whole game: they hold 500 points, each the average of 50 game turns at first; when full, pairs are merged and each point then covers twice as many turns | done | `game_stats::History` (tests `GameStats.HistoryAveragesAndHalves`, `GameStats.WhenFullThePairsAreAveragedAndTheBytesClearedLeaveTheLastBuckets`), as the exe's everlasting graph of 500 points of 50; fed by `player_statistics::ProcessTurn` (see [statistics_counted.md](statistics_counted.md)) |
| Only finished points are drawn, not the one still being averaged | partial 90% | `Finished` in `src/ECS/Statistics.cpp` gives the box the points before the one being filled, as the exe copies them; no test. The exe also empties the point being filled as it copies: a row of its own in [statistics_counted.md](statistics_counted.md) |

## The creatures (Creature Stats)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player's creature is drawn in 3D in a box filled with the player's colour | inferred | the boxes in the players' colours are drawn (`CreatureViews`); the creatures are not: `ShowInterfaceCreatures` draws nothing yet. The look of the boxes is not read in the exe |
| The creatures turn slowly on the spot, once every 8.2 seconds, all in step | inferred | `statistics_box::CreatureTurn` gives the angle (test `StatisticsBoxLayout.CreaturesFillTheLeftOfTheGraphs`); nothing draws the creatures yet; the period is not read in the exe |
| One creature fills the area of two graph boxes; two stand side by side; three are two above and one centred below; four are two by two | inferred | `statistics_box::CreatureBoxes` lays them out (test `StatisticsBoxLayout.CreaturesFillTheLeftOfTheGraphs`); nothing draws the creatures yet; the layout is not read in the exe |
