# Statistics: what is counted

Every figure the game counts for its statistics: when each is counted, for whom, how the Game Statistics box shows it,
and what is kept in saved games and the profile. The box itself (tabs, list, graphs, controls) is in
[statistics.md](statistics.md). Rows follow the box's tabs in the order the list shows them.

**Progress: 1/73 done, 40 partial — 29%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## How the figures are kept

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has a statistics record of their own, made with the player | done | one `game_stats::Stats` per player for the whole game (`GameStatsSystem`, `Locator::gameStatsSystem`, `src/Game/GameStats.h`); the temple's scrolls do not read it yet (`src/3D/TempleScrolls.cpp`) |
| Clearing the map (a new land or a new game) clears every player's counters | partial | `game_stats::ClearAll` is ported but nothing calls it on a map clear yet; see [../story/portals.md](../story/portals.md) |
| The influence and population histories are not cleared with the counters: they run on from land to land | partial | our `ClearAll` (not called yet) resets the whole record, histories included; to check against the original |
| At the start of a land the game notes each player's alignment, their creature's alignment, the real time and the number of towns | partial | `game_stats::Init` stores the alignment, the creature's value, the time and the town count, but nothing calls it yet ([../pc_integration/real_clock.md](../pc_integration/real_clock.md)) |
| Town totals (population, homes, towns and buildings) are summed from the player's towns at most once a game turn, when a figure needs them | partial | `game_stats::TownTotals` is declared; nothing sums it yet |
| The global figures (lines of code, frame rates, objects created) are updated every 10 game turns, every 5 in one game mode | partial | `AddToTotalLinesOfCodeExecuted` and `TrackFrameRate` are ported (`src/Game/GameStats.cpp`) but nothing calls them; `ObjectCreated` neither |
| Everything is kept in saved games: the counters, both histories, the town totals and the global figures | partial | `game_stats::Serialize` writes the record in the original's order (test `GameStats.SerializeSize`); openblack has no saved games to put it in ([../engine/saving_and_loading.md](../engine/saving_and_loading.md)) |
| The hand and camera figures on Tech Stats are the player's help record, kept in their profile across every game | partial | the help record is counted (`src/Help/HelpProfile.cpp`) but there are no profiles: it starts at 0 every run; see [help_system.md](help_system.md) and [profiles.md](profiles.md) |
| In a network game each machine counts every player's figures itself; nothing extra is sent | n/a | see [../multiplayer/network_play.md](../multiplayer/network_play.md) |

## The influence and population histories

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game turn the player's influence is added to the influence history: their citadel's, each of their towns' and each influence ring they own | todo | nothing adds to `Stats::influence` yet |
| Every game turn the player's population (adults and children in all their towns) is added to the population history | todo | nothing adds to `Stats::population` yet |
| Each history point is the average of a run of turns: 50 at first | partial | `game_stats::History::Add` (test `GameStats.HistoryAveragesAndHalves`); nothing feeds it yet |
| When 500 points are full, neighbouring pairs are averaged into 250 and each later point covers twice as many turns | partial | same (`History::Add`, test `GameStats.HistoryAveragesAndHalves`) |

## Player Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Final Area Of Influence": the player's influence now, as for the history; a whole number | todo | see [../worship/influence.md](../worship/influence.md) |
| "Number Of Gods Defeated": when a god loses, each human god still playing counts one, if the loser was human or one game-wide setting is on; shown out of the number of gods less one | todo | (unconfirmed what the setting is) see [../multiplayer/multiplayer_rules.md](../multiplayer/multiplayer_rules.md) |
| "Alignment Change": the player's alignment now less their alignment at the land's start, two decimals, always a number | partial | `Stats::alignmentAtStart` is kept by `game_stats::Init`, which nothing calls yet; no box shows it; see [../worship/alignment.md](../worship/alignment.md) |
| "Total People Killed": villagers whose death is put down to this player | partial | `game_stats::VillagerDied` gives the killer one (test `GameStats.DeathsGoToTheTownAndTheKiller`), but nothing calls it yet |
| "Final Total Belief": the belief in the player summed over every town in the world, times 1000; a whole number | todo | the summed belief is computed each period in `src/ECS/Town/TownBelief.cpp` (for `WorldBelief`) but not kept as this figure; see [../worship/belief.md](../worship/belief.md) |
| "Max Total Belief" and "Min Total Belief": every 10 game turns the summed belief is checked; a new highest skips the lowest test, and the lowest starts at the largest number | partial | counted: `game_stats::WorldBelief` from `src/ECS/Town/TownBelief.cpp` (test `GameStats.WorldBeliefSkipsTheMinimumOnANewMaximum`); no box shows it |
| "Number Of Artifacts": the artifacts given to towns that belong to the player, counted over every town now | todo | see [../town/artefacts.md](../town/artefacts.md) |
| "Creature Growth": the creature's size now over its size when first noted, less one, as a percentage with two decimals | todo | see [../creature/growth_and_size.md](../creature/growth_and_size.md) |
| "Creature Alignment Change": the creature's alignment now less at the land's start, two decimals, always a number | todo |  |
| "Buildings Destroyed": a building the player puts out of action counts when its state is 200 or more | partial | counted: `game_stats::BuildingStoppedFunctional` from `abodes::StopBeingFunctional` (`src/ECS/Abodes.cpp`), at state 200 or more; no box shows it |

## Creature Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Damage", "Hunger" and "Tiredness": the creature's damage, hunger and tiredness now, as percentages with two decimals | todo | the same figures feed the creature's status panel (`src/Creature/CreatureStatusPanel.cpp`, not drawn yet); nothing for the statistics ([../creature/creature_mode.md](../creature/creature_mode.md)) |
| "Age:": the creature's age now, a whole number | todo | see [../creature/development_phases.md](../creature/development_phases.md) |
| "Alignment:": the creature's alignment now, two decimals, its bar out of 1 | todo |  |
| "Animals Killed:" | todo | counted by the creature ([../creature/fighting.md](../creature/fighting.md)); nothing in our tree |
| "Battles Fought:" and "Battles Won:" | todo | ([../creature/fighting.md](../creature/fighting.md)) |
| "Poos Done:" and "Mushrooms Eaten:" | todo | ([../creature/physiology.md](../creature/physiology.md)) |
| A player without a creature shows 0 for every creature figure | todo |  |
| The creature's people killed and creatures defeated are counted and sent online, but the box never lists them | todo | the labels "People Killed:" and "Creatures Defeated:" exist unused |

## Town Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Births": a child born in one of the player's towns | partial | `game_stats::ChildBorn` is ported but not called: `ChildBorn` in `src/ECS/Villager/VillagerBirth.cpp` leaves it as a TODO; see [../villager/life_cycle.md](../villager/life_cycle.md) |
| "Total Deaths": a villager dying in one of the player's towns; the player held to blame gets one "Total People Killed" | partial | `game_stats::VillagerDied` is ported (test `GameStats.DeathsGoToTheTownAndTheKiller`) but villager deaths do not call it yet; see [../villager/death.md](../villager/death.md) |
| "Final Total Population", "Final Male Population" and "Final Female Population": the people in the player's towns now; men and women shown out of the total | todo | the town stats exist (`Town::stats`) but nothing sums them for the record |
| "Final Total Population Capacity": the places in the player's homes now | todo |  |
| "Maximum Total/Male/Female Population": checked every game turn; men and women shown out of the highest total | partial | `game_stats::TrackPopulation` (test `GameStats.PopulationTrackerSkipsTheMinimumOnANewMaximum`); nothing calls it yet |
| "Minimum Total/Male/Female Population": a turn that sets a new highest skips the lowest test, and the lowest starts at the largest number; men and women out of the lowest total | partial | same `TrackPopulation`, not called yet |
| "Total People Converted": when the player takes over a town, its adults and children are added | partial | counted: `game_stats::TownTakenOver` from `src/ECS/Town/TownBelief.cpp`; no box shows it; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| "Average Satisfaction Of Villagers": each time a town weighs its desires, one less its average desire is added; shown as the mean of all these, two decimals, its bar out of 1 | partial | counted: `game_stats::TownDesire` from `src/ECS/Town/TownDesire.cpp`; no box shows it; see [../town/town_desires.md](../town/town_desires.md) |
| "Towns Owned": the player's towns now | todo |  |
| "Total Buildings": over the player's towns a running count of homes and civic buildings is added town by town, so with more than one town the earlier towns count again | todo | as the game does; `TownTotals::buildings` is declared, not summed |
| "Total Abodes", "Total Civic Buildings" and "Total Wonders": the player's homes, civic buildings and wonders now | todo | `TownTotals` declared, not summed |
| "Total Buildings Built": a home finished in one of the player's towns; also "Total Abodes Built", "Total Civic Buildings Built" or "Total Wonders Built" by its kind | partial | counted: `game_stats::BuildingBuilt` by kind from the finished abode (`src/ECS/Abodes.cpp`); no box shows it |
| "Number Of Disciple Farmers, Foresters, Fishermen, Builders, Breeders, Traders, Missionaries, Craftsmen": a villager dropped and made a disciple of that kind | partial | `game_stats::DiscipleMade` (test `GameStats.SpellsAndDisciples`); nothing calls it yet; see [../villager/disciples.md](../villager/disciples.md) |
| "Total Food Eaten": food a town uses | partial | counted: `game_stats::FoodEaten` from the town's food use (`src/ECS/Town/TownVillagers.cpp`); no box shows it |
| "Wood Used": wood a town uses | partial | `game_stats::WoodUsed` is ported; the building's wood used (`building_sites::AddWoodUsedForBuilding`) does not call it yet |

## Miracle Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle the player casts counts once for its kind and power, when it is made | partial | counted when the spell is made: `PlayerMagic::castCount` (`src/Magic/Core/Spell.cpp`), read through `game_stats::SpellsCast` (test `GameStats.SpellsAndDisciples`); no box shows it; see [../miracles/miracle_core.md](../miracles/miracle_core.md) |
| "Total Aggressive Miracles Cast": the fireballs, lightnings and explosions of every power, the two storms, the tornado and the ground flock | todo | the per-kind counts are there (`castCount`), the totals are not summed or shown |
| "Total Nice Miracles Cast": the two heals, teleport, forest, the two foods, the two shields, wood, the two waters and the flying flock | todo | same |
| "Total Creature Miracles Cast": every creature miracle | todo | same |
| "Number Of Fireballs / Bigger Fireballs / Biggest Fireballs Cast", and the same for Lightnings and Explosions | partial | counted per kind and power in `castCount`; no box shows it |
| "Number Of Heals / Bigger Heals Cast", "Number Of Teleport Cast", "Number Of Miracle Forests Cast", "Number Of Miracle Foods / Bigger Miracle Foods Cast" | partial | same |
| "Number Of Storms / Bigger Storms Cast", "Number Of Tornados Cast", "Number Of Spiritual / Physical Shields Cast" | partial | same |
| "Number Of Miracle Woods Cast", "Number Of Miracle Waters / Bigger Miracle Waters Cast", "Number Of Flying Flock Cast", "Number Of Ground Flocks Cast" | partial | same |
| "Number Of Creature Freeze, Small, Big, Weak, Strong, Fat, Thin, Invisible, Compassion, Angry Miracles Cast" | partial | counted per kind in `castCount`, when the creature casts; no box shows it |
| "Number Of Creature Itchy Miracles Cast" | partial | same |
| The Hungry, Frightened, Tired, Ill and Thirsty creature miracles are neither counted nor listed, though their labels exist | partial | `SpellsCast` gives 0 for the types the original does not count; nothing lists them |
| "Total Chants Used": the chants a worship site spends for the player, one decimal | partial | counted: `PlayerMagic::chantsUsed` from `src/Worship/WorshipSite.cpp`; no box shows it; see [../worship/worship_sites.md](../worship/worship_sites.md) and [../worship/prayer_power.md](../worship/prayer_power.md) |
| "Total Number Of Sacrifices": a villager or an animal sacrificed for the player | todo |  |

## Tech Stats

These figures are shown as one value, in the Total column only.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Lines Of Code Executed" is invented: each update adds the land's town count times 10,130, plus a random fifth of that again | partial | `game_stats::AddToTotalLinesOfCodeExecuted` is ported, not called |
| "Max Frame Rate": each update the frame rate is compared with it, one decimal | partial | `TrackFrameRate` is ported (keeps the minimum at 0 as the game does) but not called; openblack's own frame statistics are separate ([../debug/diagnostics.md](../debug/diagnostics.md)) |
| "Min Frame Rate" always reads 0: it starts at 0 and is only tested when no new highest is set | partial | `TrackFrameRate` keeps it at 0 as the game does; not called |
| "Total Objects Created": every game object made since the counters were cleared | partial | `game_stats::ObjectCreated` is ported, not called |
| "Time Played": game turns so far over 10, a whole number of seconds of game time | todo | `Statics::startTime` is kept by `Init`; the turns are not turned into this figure |
| "Script VM Instructions": the script engine's count of instructions run | todo | see [../scripts/](../scripts/) |
| "Total Number Of Statistics" always reads 113 | partial | `Statics::version` is 113 (`src/Game/GameStats.h`); no box shows it |
| "Distance Dragged": the help record's dragging figure times 3 | todo | the help record's Drag event is never triggered |
| "Time Looking At Sky" and "Time Looking At Ground" | todo | the look-at-sky and look-at-land events never come (`help_profile::ProcessSpecialTriggers` has no camera view) |
| "Gestures Done" | partial | the help record counts the gestures (`src/Magic/Gestures/PowerUpSystem.cpp` into `help_profile::Trigger`, `GestureTotal`); no box shows it; see [../gesture/miracle_gestures.md](../gesture/miracle_gestures.md) |
| "Zoom Count" (the help record's figure times 9), "Rotate Count" and "Pitch Count" | partial | the help record counts zoom, rotate and pitch from the player camera (`help_profile::OnPlayerCameraMove`, `DefaultWorldCameraModel.cpp`); no box shows it; see [../camera/](../camera/) |
| "Total Interface Actions", "Total Objects Thrown" and "Total Number Of Things Picked Up" | partial | the help record counts every interface event, throws and pick-ups (`HandHolding.cpp`); no box shows it; see [../hand/](../hand/) |
| "Number Of Fight Attacks", "Number Of Fight Blocks" and "Number Of Fight Steps" | todo | the fight events are never triggered; see [../creature/fighting.md](../creature/fighting.md) |
| "Total Polygons Drawn" has a label but is neither counted nor listed | n/a | nothing to do |

## Counted but not shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When a human god loses they get the next finishing place; when one god is left, it gets the place after | todo | sent online only |
| The number of players who have left the game | todo | sent online only |
| At the end of an online game one record goes to Lionhead's database: the game's version and map, the places, the players' accounts and clan, and every figure above by name | n/a | the server is gone; see [../pc_integration/online_services.md](../pc_integration/online_services.md) and [../multiplayer/clans.md](../multiplayer/clans.md) |
| If the database can't be reached the player is told "Could not connect to database. Your creature and/or statistics will not be updated." | n/a | the server is gone |
