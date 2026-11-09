# Creature saves and files

A creature outlives any one game: its mind (everything it has learnt), its body and its tattoos are kept in creature
files that belong to the player's profile, so the same creature can be carried from land to land and into new games,
taken into skirmish and network games, and uploaded to the game's website. The game's own computer gods and skirmish
opponents also come from creature files.

**Progress: 15/32 done, 0 chosen, 6 partial, 4 inferred — 60%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/mind.md).

## Creature mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At a land change the local creature's mind is saved over the profile's mind file in the game's creature mind folder | partial 75% | `ecs::player_creature::SaveMyCreature` from `Game::ChangeLand`, `ToMindFile`, `resources::SaveCreatureMind`, as the exe's `GGame::ClearMap` save ([players-creature](../../../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again)); tests `PlayerCreature.TheSaveWritesTheMindAndThePhysiqueBesideIt`, `CreatureFileWriterTest.AMindWrittenIsReadBackThroughTheCache`, `CreatureFilesOfALiveCreature.TheMindTakesTheBodysFatnessAndKeepsTheRest`; missing: the body's age, energy and needs are written as the loaded file had them, not as they are, and with no profile file the exe still saves to the empty name (ours writes no mind) |
| A small companion file keeps the creature's physique beside its mind file | partial 50% | written at the land change (`creaturemind` `PhysiqueFile`, `creature_mind_body::ToPhysique`; tests `CreaturePhysiqueFile.TheLayoutIsTheSpeciesFourFloatsAndTwoCountedLists`, `CreatureFileWriterTest.APhysiqueWrittenIsReadBack`, `PlayerCreature.WithNoProfileFileOnlyThePhysiqueIsSavedAsPhysique`); missing: the exe also writes it every 600 ticks for the local creature and when the creature is deleted, and the front end's tattoo editor reads it ([players-creature](../../../creature/players-creature.md#the-creatures-files)) |
| The file keeps the creature's species, name, the name it was saved as and the profile it belonged to, the last two lightly encrypted | done | `components/creaturemind` `MindFile`; test `CreatureMindFile.EncryptionRoundTrips` |
| It keeps the 40 desires with their sources and thresholds | done | `MindFile`, `src/Creature/CreatureMindModel`; test `CreatureMindFile.ReferenceSavesParseAndRoundTrip` |
| It keeps the examples its decision trees learnt from | done | `MindFile`, `creature_mind_model` load in `src/Creature/CreatureMindModel.cpp`; test `CreatureMindFile.ModelKeepsAFileItWasLoadedFrom` |
| It keeps its opinion of each action, how often it has seen actions and miracles, and which it knows | done | `MindFile` |
| It keeps how it sees the player, the desires it thinks the player has, and what towns want | done | `MindFile` (kept; partly used, see [beliefs_and_opinions.md](beliefs_and_opinions.md)) |
| It keeps its alignment, stage of growing up, body (age, energy, poo, size and so on) and tattoos | done | `src/Creature/CreatureMindFileBody`; tests `TakesTheBodyAFileKeeps`, `TattooWordsRoundTrip` |
| Every version from the earliest the game wrote to the newest (33) is read, each with its own fields | done | tests `CreatureMindFile.OlderVersionsLeaveOutTheirFields`, `ReferenceSavesParseAndRoundTrip` (the two shipped `.erc` files, skipped without the game's data) |
| A file read and written again is the same byte for byte | done | tests `CreatureMindFile.CurrentVersionRoundTrips`, `KeepsTrailingBytes` |
| Files that aren't minds, or are newer than known, are refused, and the creature starts with a fresh mind of its species | done | test `CreatureMindFile.RejectsWhatIsNotAMind`, `CreatureMindLoaderTest.AFileWithNoSpeciesRowIsNotAMind`; `creature::CreatureMind::Loaded` |
| Files whose species row is unknown give no creature | done | test `UnknownSpeciesIsNoCreature`; `LOAD_MY_CREATURE` makes nothing then (test `PlayerCreature.NoCreatureIsLoadedWhenThePlayerHasOneOrTheFileGivesNone`) |
| Values out of range in a file are kept within bounds | done | `CreatureMindFileBody`; test `OutOfRangeValuesAreKeptInBounds` |
| A fresh mind is written as a current file | done | test `CreatureMindFile.FreshMindWritesACurrentFile` |
| Mind files are loaded once through the resource cache | done | `resources::CreatureMindLoader` (`src/Resources/Loaders.cpp`), used by `LOAD_MY_CREATURE` and `CREATE_CREATURE_FROM_FILE`; tests `CreatureMindLoaderTest.*` |
| The game checks a loaded mind (from version 21 of the file) and fixes or refuses it | pending | the exe's load runs `CheckValidation` before the mind ([players-creature](../../../creature/players-creature.md#loading-the-profiles-creature)); what it checks is not read; ours refuses only a file its reader cannot parse |
| The game keeps a copy of the mind in memory to restore | inferred | written from expectation: no such copy is found on the wiki; the exe's mind swap goes through temporary files ([players-creature](../../../creature/players-creature.md#swapping-minds)) |
| The game deletes a profile's creature files when the profile or creature is deleted | inferred | not read in the exe (the front end's profile deletion is not on the wiki); no profiles in our tree |

## Carrying the creature on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is saved as the profile's creature, so a new game can start with it | partial 75% | `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE` read the profile's file, which is the `--creature-file` setting or a kept profile's own (`ECS/PlayerCreature.cpp`; tests `ProfileCreatureTest.TheCreatureFileIsAKeptProfilesOwnElseTheSetting`, `PlayerCreature.*`), `IS_KEEPING_OLD_CREATURE` reads the land's skip flag, and the land change saves it back (rows above); missing: the exe's save every 600 ticks (the physique) and the fields the save does not write |
| Moving from one land to the next, the player's creature comes along as it was | partial 75% | it is saved at the land change and the next land's `LOAD_MY_CREATURE` makes it again from the file, as the exe; it comes back with its mind, stage, size, strength, alignment and fatness, but with its age, energy and needs as the first file had them and without its tattoos (the load does not take them); see [../story/portals.md](../story/portals.md#the-creature) |
| Scripts can load a creature from a file into the land | done | `LOAD_CREATURE`, the land scripts' `CREATE_CREATURE_FROM_FILE` (`src/LHScriptX/FeatureScriptCommands.cpp`) and a computer god's creature all take one step, `ecs::player_creature::MakeFromMindFile` (`src/ECS/PlayerCreature.cpp`), as the original's three share one; `LOAD_MY_CREATURE` too |
| Scripts can ask whether the profile has a creature file | done | `CURRENT_PROFILE_HAS_CREATURE` (the exe's native 463): whether the profile's mind file exists in `Scripts/CreatureMind` (`player_creature::ProfileHasCreature`, `src/ECS/PlayerCreature.cpp`); tests `ProfileCreatureTest.*` |
| A creature loaded from a file takes its species, name, alignment, strength, size, fatness, needs and tattoos | partial 85% | `ecs::player_creature::MakeFromMindFile` with `CreatureMindFileBody` takes all but the tattoos (tests `PlayerCreatureNativesTest.ALoadedCreatureTakesTheFilesFatnessAndShowsItEasedTwice`, `ALoadedCreaturesBodyTakesTheFilesNeedsBeforeItsFirstTurn`, `CreatureMindFileBody.TakesTheBodyAFileKeeps`); missing: the tattoos on a load (a swap takes them); the debug spawner's saved-creature list (`src/Debug/CreatureSpawnerMindFiles.cpp`) spawns them too |
| A mind can be loaded into an existing creature: a script swaps two creatures' minds, each body keeping its name, size and alignment | partial 70% | `SWAP_CREATURE`, `CreatureMindSystem::SwapMinds` (`CreatureMindSwap.cpp`, `creature_swap`), as the exe's `SwapMinds` ([swapping minds](../../../creature/players-creature.md#swapping-minds)); tests `PlayerCreatureNativesTest.ASwappedBodyTakesTheFilesStrengthAndTattoosButKeepsItsSizeAndAlignment`, `test/creature/test_creature_mind_system.cpp`; unverified against v1.42: that the body takes the file's strength, fatness, needs and tattoos, and the ease towards the other's size and alignment (imported) |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature, its mind, body, spells, leash and what it is doing are saved and loaded with the game | pending | no saved games; see [../engine](../engine/) |
| Loading a saved game puts the creature back mid-action | pending |  |

## Sharing creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A packed creature file is sent between machines | pending | no network play; see [../multiplayer](../multiplayer/) |

## Network and skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In a network game each player brings their own creature from their profile | pending | see [../multiplayer](../multiplayer/); a skirmish loads the profile's creature too: [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| A creature's state is added to the game's checksum, so the machines can tell when they drift apart | pending | no network checksum; the debug state hash used by our verification runs has a "creature" part (`player_creature::HashCreatures`, test `TheHashPartFollowsEachCreaturesFields`), which is not the game's |
| In skirmish, computer players' creatures load from the template minds | pending |  |
| A computer player balances its creature's knowledge to its difficulty | inferred | ours reads no difficulty: the creature learns every miracle its god has, and heal, food, wood and water (`ComputerPlayerSystem`, `computer_player::MiraclesToLearn`; test `test/test_computer_player_start.cpp`); what the exe's computer mind teaches its creature at the start is not on the wiki ([computer minds](../../../hand/computer-minds.md)) |

## Debug output

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cheats: learn everything, learn ordinary things, next stage of growing up, make it fight | inferred | whether the retail exe has these as player cheats is not read; ours are the debug spawner's controls (set the stage of growing up, clear learning, start fights); see [development_phases.md](development_phases.md) |
