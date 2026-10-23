# Statistics: what is counted

Every figure the game counts for its statistics: when each is counted, for whom, how the Game Statistics box shows it,
and what is kept in saved games and the profile. The box itself (tabs, list, graphs, controls) is in
[statistics.md](statistics.md). Rows follow the box's tabs in the order the list shows them.

**Progress: 44/74 done, 0 chosen, 23 partial, 0 inferred — 82%**

How the original does it, in our wiki: [Screen, text and read-outs](../../../interface/screen-and-readouts.md).

## How the figures are kept

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has a statistics record of their own, made with the player | done | one `game_stats::Stats` per player for the whole game (`GameStatsSystem`, `Locator::gameStatsSystem`, `src/Game/GameStats.h`), as the exe makes one with each player; tests `GameStats.*`; the temple's scrolls do not read it yet (`src/3D/TempleScrolls.cpp`) |
| Clearing the map (a new land or a new game) clears every player's counters | done | `game_stats::ClearAll` at each land's map clear (`src/Game.cpp`), as the exe's map clear does for every player and the neutral one (xref note #965); test `GameStats.EachLandsClearKeepsTheHistoriesAndClearsTheRest` |
| The influence and population histories are not cleared with the counters: they run on from land to land | done | our `ClearAll` keeps both histories (`src/Game/GameStats.cpp`), as the exe's clear leaves them (xref note #965); test `GameStats.EachLandsClearKeepsTheHistoriesAndClearsTheRest` |
| At the start of the game the game notes the local player's alignment (every player's in a network game), their creature's alignment, the real time and the number of towns; on later lands the clear leaves these at 0 | partial 80% | `player_statistics::Start` on the game's first land after its map script, the later lands cleared (`game_stats::ClearAll`); test `GameStats.SacrificesWoodAndTheStartGoToTheirPlayer`. Missing: openblack notes every player at the land's load; in a one-player game the exe notes only the local player then, the others when each player is made (as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21). (Until 2026-10-21 this row said every land's start) |
| Town totals (population, homes, towns and buildings) are summed from the player's towns at most once a game turn, when a figure needs them | partial 80% | summed from the player's towns as the box opens (`TownFigures` in `src/ECS/Statistics.cpp`), from each town's running counts as they stand (`town_stats::AsTheyStand`), the same sums as the exe's (xref note #966); test `StatisticsBuildings.EarlierTownsCountAgainInTheTotal`. Missing: they are not kept for the rest of the turn as the exe keeps them, so a second read in the same turn could differ |
| The global figures (lines of code, frame rates, objects created) are updated every 10 game turns, every 5 in one game mode | partial 70% | the lines of code and the frame rate take their step every 10 turns, 5 when the turn ends paused, at the turn's end (`src/Game.cpp`, xref note #367; test `GameStats.TheLinesOfCodeStepEveryTenTurnsOrFiveWhenTheTurnEndsPaused`). Missing: the objects created are the registry's count of entities made, not the exe's count of game objects made |
| Everything is kept in saved games: the counters, both histories, the town totals and the global figures | partial 30% | `game_stats::Serialize` writes the record in the original's order (test `GameStats.SerializeSize`); openblack has no saved games to put it in, and nothing reads it back ([../engine/saving_and_loading.md](../engine/saving_and_loading.md)) |
| The hand and camera figures on Tech Stats are the player's help record, kept in their profile across every game | partial 50% | the help record is counted (`src/Help/HelpProfile.cpp`) and its figures are read as the exe reads them (test `StatisticsGlobal.TheHelpRecordGivesTheTechFigures`); missing: it is not kept with the profile, so it starts at 0 every run; see [help_system.md](help_system.md) and [profiles.md](profiles.md) |

## The influence and population histories

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game turn the player's influence is added to the influence history: their citadel's, each of their towns' and each influence ring they own | partial 80% | `player_statistics::ProcessTurn` adds `influence::InfluencePower` each turn after the players' influence power, and once more after each land's script for every player with a temple (`influence::NoteInfluencePowerAfterLoad`); test `InfluenceTest.influencePowerAndRatio` for the power. Missing: the exe adds it inside the power's own calculation, each time the power is worked out, not once a turn after it (as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21) |
| Every game turn the player's population (adults and children in all their towns) is added to the population history | done | `player_statistics::ProcessTurn` adds the men and women of the player's towns each turn, after the towns, as the exe's player turn adds its towns' population; the history is tested (`GameStats.HistoryAveragesAndHalves`) |
| Each history point is the average of a run of turns: 50 at first | done | `game_stats::History::Add` (test `GameStats.HistoryAveragesAndHalves`), as the exe's everlasting graph of 500 points of 50 |
| When 500 points are full, neighbouring pairs are averaged into 250 and each later point covers twice as many turns | done | `History::Add`, with the exe's clearing of only part of the second half's bytes (tests `GameStats.HistoryAveragesAndHalves`, `GameStats.WhenFullThePairsAreAveragedAndTheBytesClearedLeaveTheLastBuckets`) |
| Each time the figures are taken for the box, the history point still being averaged is emptied, so the turns it had gathered are lost | pending | (added 2026-10-21) the exe's copy of a history for the box sets the point being filled to 0 before copying the finished ones (as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21); openblack leaves it |

## Player Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Final Area Of Influence": the player's influence now, as for the history; a whole number | done | `influence::InfluencePower`, as the exe's figure is the player's influence power; tests `InfluenceTest.*`; shown in the Game Statistics box (`player_statistics::Box`, `src/ECS/Statistics.cpp`) |
| "Number Of Gods Defeated": when a god loses, each human god still playing counts one, if the loser was human or the game is a skirmish; shown out of the number of gods less one | pending | the box's row reads 0: openblack does not count the gods a player defeated yet (`FiguresOf` in `src/ECS/Statistics.cpp`); the rule is the exe's (xref note #407) |
| "Alignment Change": the player's alignment now less their alignment at the start, two decimals, always a number | done | `Stats::alignmentAtStart` from `game_stats::Init`, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `GameStats.SacrificesWoodAndTheStartGoToTheirPlayer`; shown in the Game Statistics box (`player_statistics::Box`, `src/ECS/Statistics.cpp`) |
| "Total People Killed": villagers whose death is put down to this player | done | `game_stats::VillagerDied` from `villager::VillagerDead` (`VillagerDeath.cpp`), as the exe's town counts a villager's death; test `GameStats.DeathsGoToTheTownAndTheKiller` |
| "Final Total Belief": the belief in the player summed over every town in the world, times 1000; a whole number | partial 90% | the belief in the player summed over every town, times 1000, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; shown in the Game Statistics box (`player_statistics::Box`, `src/ECS/Statistics.cpp`); no test |
| "Max Total Belief" and "Min Total Belief": every 10 game turns the summed belief is checked; a new highest skips the lowest test, and the lowest starts at the largest number | done | `game_stats::WorldBelief` from the belief's turn in `src/ECS/Town/TownBelief.cpp`, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21 (shown times 1000); test `GameStats.WorldBeliefSkipsTheMinimumOnANewMaximum` |
| "Number Of Artifacts": the artifacts in every town that the player gave | done | `ArtefactSystem::CountFor` counts the artefacts in a town given by the player, as the exe counts them over every town; test in `test/test_creature_giving.cpp`. (Until 2026-10-21 this row said the towns had to be the player's) |
| "Creature Growth": the creature's size now over its size when first noted, less one, as a percentage with two decimals | partial 70% | the size noted the first time the creature is seen (`Stats::creatureValue`, `player_statistics::ProcessTurn`) and the formula as the exe's; missing: where the exe notes the first size is not read, and no test; see [../creature/growth_and_size.md](../creature/growth_and_size.md) |
| "Creature Alignment Change": the creature's alignment now less at the start, two decimals, always a number | done | `Stats::creatureAtStart` from `game_stats::Init`, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `GameStats.SacrificesWoodAndTheStartGoToTheirPlayer`; shown in the Game Statistics box (`player_statistics::Box`, `src/ECS/Statistics.cpp`) |
| "Buildings Destroyed": a building the player puts out of action counts when its age is 200 or more | done | `game_stats::BuildingStoppedFunctional` from `abodes::StopBeingFunctional` (`src/ECS/Abodes.cpp`), at 200 or more, as the exe's abode; test in `test/test_game_stats.cpp` |

## Creature Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Damage", "Hunger" and "Tiredness": the creature's damage, hunger and tiredness now, as percentages with two decimals | partial 90% | from the creature's needs (`CreatureFigures` in `src/ECS/Statistics.cpp`): one less its life, one less its energy, its exhaustion, times 100, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; no test |
| "Age:": the creature's age now, a whole number | partial 90% | the creature's needs' age, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; no test |
| "Alignment:": the creature's alignment now, two decimals, its bar out of 1 | partial 90% | the creature's alignment, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; no test |
| "Animals Killed:" | partial 40% | the creature's tallies count the animals it killed by eating them (`player_statistics::CountCreatureMeal` from `src/Creature/CreatureEating.cpp`; tests `CreatureTallies.*`); missing: every other way the exe's creature counts an animal killed (not read); see [../creature/fighting.md](../creature/fighting.md) |
| "Battles Fought:" and "Battles Won:" | partial 40% | read from the creature's fight record (`CreatureTalliesOf`); missing: where the exe counts a battle fought and won is not read, and no test ([../creature/fighting.md](../creature/fighting.md)) |
| "Poos Done:" and "Mushrooms Eaten:" | done | the creature's tallies (`player_statistics::CountCreaturePoo`, `CountCreatureMushroom`), the figures the exe's box reads from the creature; tests `CreatureTallies.*` ([../creature/physiology.md](../creature/physiology.md)) |
| A player without a creature shows 0 for every creature figure | partial 90% | `CreatureFigures` leaves them at 0 (`src/ECS/Statistics.cpp`), as every creature figure of the exe's; no test |
| The creature's people killed and creatures defeated are counted and sent online, but the box never lists them | pending | the labels "People Killed:" and "Creatures Defeated:" exist unused |

## Town Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Births": a child born in one of the player's towns | done | `game_stats::ChildBorn` from `VillagerBirth.cpp`; tests in `test/test_game_stats.cpp` and `test/test_villager_birth.cpp`; see [../villager/life_cycle.md](../villager/life_cycle.md) |
| "Total Deaths": a villager dying in one of the player's towns; the player held to blame gets one "Total People Killed" | done | `game_stats::VillagerDied` from `VillagerDeath.cpp`, as the exe's town counts both; test `GameStats.DeathsGoToTheTownAndTheKiller` |
| "Final Total Population", "Final Male Population" and "Final Female Population": the people in the player's towns now; men and women shown out of the total | done | summed from the player's towns as the box opens (`TownFigures`), as the exe's town totals (xref note #966); test `StatisticsBoxRows.MenAndWomenAreOutOfTheFirstPlayersPeople` |
| "Final Total Population Capacity": the places in the player's homes now | partial 90% | the adult and child places of the player's towns (`TownFigures`), as the exe's (xref note #966); no test |
| "Maximum Total/Male/Female Population": checked every game turn; men and women shown out of the highest total | done | `game_stats::TrackPopulation` each turn from `player_statistics::ProcessTurn`, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `GameStats.PopulationTrackerSkipsTheMinimumOnANewMaximum` |
| "Minimum Total/Male/Female Population": a turn that sets a new highest skips the lowest test, and the lowest starts at the largest number; men and women out of the lowest total | done | the same `TrackPopulation`, each turn, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `GameStats.PopulationTrackerSkipsTheMinimumOnANewMaximum` |
| "Total People Converted": when the player takes over a town, its adults and children are added | done | `game_stats::TownTakenOver` from `src/ECS/Town/TownBelief.cpp`, as the exe's town take-over; test in `test/test_game_stats.cpp`; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| "Average Satisfaction Of Villagers": each time a town weighs its desires, one less its average desire is added; shown as the mean of all these, two decimals, its bar out of 1 | done | `game_stats::TownDesire` from `src/ECS/TownDesire.cpp`, the mean as the exe's; test `TownDesireTest.NoPlayerNoAverageAndNoGameStatsSample`; see [../town/town_desires.md](../town/town_desires.md) |
| "Towns Owned": the player's towns now | partial 90% | `TownFigures`, as the exe's town totals; no test |
| "Total Buildings": over the player's towns a running count of homes and civic buildings is added town by town, so with more than one town the earlier towns count again | done | `statistics::TotalBuildings` (`src/ECS/StatisticsRules.cpp`), as the exe's town totals; test `StatisticsBuildings.EarlierTownsCountAgainInTheTotal` |
| "Total Abodes", "Total Civic Buildings" and "Total Wonders": the player's homes, civic buildings and wonders now | partial 90% | `TownFigures`, as the exe's town totals (xref note #966); no test |
| "Total Buildings Built": a building finished in one of the player's towns; also "Total Abodes Built", "Total Civic Buildings Built" or "Total Wonders Built" by its kind | partial 90% | `game_stats::BuildingBuilt` by kind from the finished abode (`src/ECS/Abodes.cpp`), as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; no test. (Until 2026-10-21 this row said a home: every building counts) |
| "Number Of Disciple Farmers, Foresters, Fishermen, Builders, Breeders, Traders, Missionaries, Craftsmen": a villager dropped and made a disciple of that kind | done | `game_stats::DiscipleMade` from `src/ECS/Villager/VillagerDisciple.cpp`, as the exe counts it as the villager lands; test `GameStats.SpellsAndDisciples`; see [../villager/disciples.md](../villager/disciples.md) |
| "Total Food Eaten": food a town's villagers eat | partial 90% | `game_stats::FoodEaten` from the town's food use (`src/ECS/Town/TownVillagers.cpp`), called when a villager eats the food it holds, as the exe's only use; no test |
| "Wood Used": wood a town uses | done | `game_stats::WoodUsed` beside `AddWoodUsedForBuilding` (`VillagerBuild.cpp`), the exe's only use of a town's wood being a villager building; test `GameStats.SacrificesWoodAndTheStartGoToTheirPlayer` |

## Miracle Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle the player casts counts once for its kind and power, when it is made | done | counted when the spell is made: `PlayerMagic::castCount` (`src/Magic/Core/Spell.cpp`), as the exe counts it when a spell starts at its place; read through `game_stats::SpellsCast` (test `GameStats.SpellsAndDisciples`); see [../miracles/miracle_core.md](../miracles/miracle_core.md) |
| "Total Aggressive Miracles Cast": the fireballs, lightnings and explosions of every power, the two storms, the tornado and the ground flock | done | summed by kind (`statistics::MiracleKind::Aggressive`), as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `StatisticsMiracles.EachTypeCountsTowardsItsTotal` |
| "Total Nice Miracles Cast": the two heals, teleport, forest, the two foods, the two shields, wood, the two waters and the flying flock | done | summed by kind (`MiracleKind::Nice`), as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `StatisticsMiracles.EachTypeCountsTowardsItsTotal` |
| "Total Creature Miracles Cast": every creature miracle | done | summed by kind, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `StatisticsMiracles.EachTypeCountsTowardsItsTotal` |
| "Number Of Fireballs / Bigger Fireballs / Biggest Fireballs Cast", and the same for Lightnings and Explosions | done | counted per kind and power in `castCount`, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `StatisticsBoxPages.EachTabListsItsRowsInOrder` |
| "Number Of Heals / Bigger Heals Cast", "Number Of Teleport Cast", "Number Of Miracle Forests Cast", "Number Of Miracle Foods / Bigger Miracle Foods Cast" | done | same |
| "Number Of Storms / Bigger Storms Cast", "Number Of Tornados Cast", "Number Of Spiritual / Physical Shields Cast" | done | same |
| "Number Of Miracle Woods Cast", "Number Of Miracle Waters / Bigger Miracle Waters Cast", "Number Of Flying Flock Cast", "Number Of Ground Flocks Cast" | done | same |
| "Number Of Creature Freeze, Small, Big, Weak, Strong, Fat, Thin, Invisible, Compassion, Angry Miracles Cast" | done | counted per kind in `castCount`, when the creature casts, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test `GameStats.SpellsAndDisciples` |
| "Number Of Creature Itchy Miracles Cast" | done | same |
| The Hungry, Frightened, Tired, Ill and Thirsty creature miracles are neither counted nor listed, though their labels exist | done | `SpellsCast` gives 0 for the types the exe's counter skips, and the box lists none of them (test `StatisticsBoxPages.EachTabListsItsRowsInOrder`) |
| "Total Chants Used": the chants a worship site spends for the player, one decimal | done | `PlayerMagic::chantsUsed` from `src/Worship/WorshipSite.cpp`, as the exe's worship site; test in `test/test_player_system.cpp`; see [../worship/worship_sites.md](../worship/worship_sites.md) and [../worship/prayer_power.md](../worship/prayer_power.md) |
| "Total Number Of Sacrifices": a villager or an animal sacrificed for the player | partial 50% | a villager sacrificed counts (`game_stats::Sacrificed` from `VillagerDeath.cpp`; test `GameStats.SacrificesWoodAndTheStartGoToTheirPlayer`); missing: an animal sacrificed, which the exe also counts (xref note #368) |

## Tech Stats

These figures are shown as one value, in the Total column only.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Lines Of Code Executed" is invented: each update adds the town count at the start times 10,130, plus a random fifth of that again | done | `game_stats::AddToTotalLinesOfCodeExecuted`, at the turn's step (`src/Game.cpp`) and once as the box opens, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; tests `StatisticsGlobal.LinesOfCodeAreTheTownsTimesTenThousandAndMore`, `GameStats.TheLinesOfCodeStepEveryTenTurnsOrFiveWhenTheTurnEndsPaused`; the exe also adds one step whenever the figures are packed at a network game's end, which openblack does not have |
| "Max Frame Rate": each update the frame rate is compared with it, one decimal | done | `TrackFrameRate` at the turn's step, fed by `statistics::FrameRateMeter`, as the exe's turn end; test `StatisticsGlobal.TheFrameRateIsMeasuredOverThreeSeconds`; openblack's own frame statistics are separate ([../debug/diagnostics.md](../debug/diagnostics.md)) |
| "Min Frame Rate" always reads 0: it starts at 0 and is only tested when no new highest is set | done | `TrackFrameRate` keeps it at 0 as the exe's turn end does; test `StatisticsGlobal.TheFewestFramesASecondStaysAtNothing` |
| "Total Objects Created": every game object made since the counters were cleared | partial 20% | the row is shown, but with every entity the registry made since the land started (`Registry::GetEntitiesMade`), not the exe's count (every allocation of its game-object heap since the land started, beliefs, agendas, footpaths and file objects included: [engine loop](../../../engine/loop.md)). By the user's decision of 2026-10-21 the figure is to count what the exe counts and the entity count is to become a debug readout |
| "Time Played": game turns so far over 10, a whole number of seconds of game time | done | `statistics::TimePlayed`, the turns over 10, as the decompiled statistics record (bw1-decomp `GameStats.cpp`), read 2026-10-21; test in `test/test_statistics_box.cpp` |
| "Script VM Instructions": the script engine's count of instructions run | partial 90% | the script machine's `GetExecutedInstructions`, as the exe asks its script engine; no test |
| "Total Number Of Statistics" always reads 113 | done | `Statics::version` is 113 (`src/Game/GameStats.h`), the exe's statistics version; test in `test/test_statistics_box.cpp` |
| "Distance Dragged": the help record's dragging figure times 3 | pending | the help record's Drag event is never triggered |
| "Time Looking At Sky" and "Time Looking At Ground" | pending | the look-at-sky and look-at-land events never come (`help_profile::ProcessSpecialTriggers` has no camera view) |
| "Gestures Done" | done | the help record's gesture total (`src/Magic/Gestures/PowerUpSystem.cpp` into `help_profile::Trigger`, `GestureTotal`), the help record entry the exe reads; test `StatisticsGlobal.TheHelpRecordGivesTheTechFigures`; where the gestures are counted is in [../gesture/miracle_gestures.md](../gesture/miracle_gestures.md) |
| "Zoom Count" (the help record's figure times 9), "Rotate Count" and "Pitch Count" | done | the help record counts zoom, rotate and pitch from the player camera (`help_profile::OnPlayerCameraMove`, `DefaultWorldCameraModel.cpp`), the entries and the factor the exe reads; test `StatisticsGlobal.TheHelpRecordGivesTheTechFigures`; see [../camera/](../camera/) |
| "Total Interface Actions", "Total Objects Thrown" and "Total Number Of Things Picked Up" | done | the help record counts every interface event, throws and pick-ups (`HandHolding.cpp`), the entries the exe reads; test `StatisticsGlobal.TheHelpRecordGivesTheTechFigures`; see [../hand/](../hand/) |
| "Number Of Fight Attacks", "Number Of Fight Blocks" and "Number Of Fight Steps" | partial 60% | the help record counts the local creature's fight moves (`src/Creature/CreatureFight.cpp`) and the box reads the exe's entries (test `StatisticsGlobal.TheHelpRecordGivesTheTechFigures`); missing: where the exe counts each move is not read; see [../creature/fighting.md](../creature/fighting.md) |

## Counted but not shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When a human god loses they get the next finishing place; when one god is left, it gets the place after | pending | sent online only; the rule is the exe's (xref note #407) |
| The number of players who have left the game | pending | sent online only |
