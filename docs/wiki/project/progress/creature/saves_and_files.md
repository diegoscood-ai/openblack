# Creature saves and files

A creature outlives any one game: its mind (everything it has learnt), its body and its tattoos are kept in creature
files that belong to the player's profile, so the same creature can be carried from land to land and into new games,
taken into skirmish and network games, and uploaded to the game's website. The game's own computer gods and skirmish
opponents also come from creature files.

**Progress: 18/31 done, 0 chosen, 5 partial, 0 inferred — 69%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/mind.md).

## Creature mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At a land change the local creature's mind is saved over the profile's mind file in the game's creature mind folder | partial 75% | `ecs::player_creature::SaveMyCreature` from `Game::ChangeLand`, `ToMindFile`, `resources::SaveCreatureMind`, as the exe's `GGame::ClearMap` save ([players-creature](../../../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again)); tests `PlayerCreature.TheSaveWritesTheMindAndThePhysiqueBesideIt`, `CreatureFileWriterTest.AMindWrittenIsReadBackThroughTheCache`, `CreatureFilesOfALiveCreature.TheMindTakesTheBodysFatnessAndKeepsTheRest`; missing: the body's age, energy and needs are written as the loaded file had them, not as they are, and with no profile file the exe still saves to the empty name (ours writes no mind) |
| A small companion file keeps the creature's physique beside its mind file | partial 50% | written at the land change (`creaturemind` `PhysiqueFile`, `creature_mind_body::ToPhysique`; tests `CreaturePhysiqueFile.TheLayoutIsTheSpeciesFourFloatsAndTwoCountedLists`, `CreatureFileWriterTest.APhysiqueWrittenIsReadBack`, `PlayerCreature.WithNoProfileFileOnlyThePhysiqueIsSavedAsPhysique`); missing: the exe also writes it every 600 ticks for the local creature and when the creature is deleted, and the front end's tattoo editor reads it ([players-creature](../../../creature/players-creature.md#the-creatures-files)) |
| The file keeps the creature's species, name, the name it was saved as and the profile it belonged to, the last two lightly encrypted | done | `components/creaturemind` `MindFile`; test `CreatureMindFile.EncryptionRoundTrips` |
| It keeps the 40 desires with their sources and thresholds | done | `MindFile`, `src/Creature/CreatureMindModel`; test `CreatureMindFile.ReferenceSavesParseAndRoundTrip` |
| It keeps the examples its decision trees learnt from | done | `MindFile`, `creature_mind_model` load in `src/Creature/CreatureMindModel.cpp`; test `CreatureMindFile.ModelKeepsAFileItWasLoadedFrom` |
| It keeps its opinion of each action, how often it has seen actions and miracles, and which it knows | done | `MindFile`; test `CreatureMindFile.CurrentVersionRoundTrips` |
| It keeps how it sees the player, the desires it thinks the player has, and what towns want | done | `MindFile` (kept; partly used, see [beliefs_and_opinions.md](beliefs_and_opinions.md)); test `CreatureMindFile.CurrentVersionRoundTrips` |
| It keeps its alignment, stage of growing up, body (age, energy, poo, size and so on) and tattoos | done | `src/Creature/CreatureMindFileBody`; tests `CreatureMindFileBody.TakesTheBodyAFileKeeps`, `TattooWordsRoundTrip` |
| Every version from the earliest the game wrote to the newest (33) is read, each with its own fields | done | tests `CreatureMindFile.OlderVersionsLeaveOutTheirFields`, `ReferenceSavesParseAndRoundTrip` (the two shipped `.erc` files, skipped without the game's data) |
| A file read and written again is the same byte for byte | done | tests `CreatureMindFile.CurrentVersionRoundTrips`, `KeepsTrailingBytes` |
| Files that aren't minds, or are newer than known, are refused, and the creature starts with a fresh mind of its species | done | test `CreatureMindFile.RejectsWhatIsNotAMind`, `CreatureMindLoaderTest.AFileWithNoSpeciesRowIsNotAMind`; `creature::CreatureMind::Loaded` |
| Files whose species row is unknown give no creature | done | test `CreatureMindFileBody.UnknownSpeciesIsNoCreature`; `LOAD_MY_CREATURE` makes nothing then (test `PlayerCreature.NoCreatureIsLoadedWhenThePlayerHasOneOrTheFileGivesNone`) |
| Values out of range in a file are kept within bounds | done | `CreatureMindFileBody`; test `CreatureMindFileBody.OutOfRangeValuesAreKeptInBounds` |
| A fresh mind is written as a current file | done | test `CreatureMindFile.FreshMindWritesACurrentFile` |
| Mind files are loaded once through the resource cache | done | `resources::CreatureMindLoader` (`src/Resources/Loaders.cpp`), used by `LOAD_MY_CREATURE` and `CREATE_CREATURE_FROM_FILE`; tests `CreatureMindLoaderTest.*` |
| The game checks a loaded mind (from version 21 of the file) and fixes or refuses it | pending | the exe's load runs `CheckValidation` before the mind ([players-creature](../../../creature/players-creature.md#loading-the-profiles-creature)); what it checks is not read; ours refuses only a file its reader cannot parse |
| At a skirmish or network game's end the local creature's mind is copied to memory for the online creature database | pending | online, parked. `Creature::SaveMindToMemory` 0x4E7660, from `GPlayer::ProcessPlayers` 0x649E0D and 0x64A1DD (bw1-decomp Player.cpp:552, 657), into `GPlayer::CreatureMindData`, read only by `CreatureDatabase::Send` 0x6337B2; nothing restores a mind from it; the mind swap uses temporary files ([players-creature](../../../creature/players-creature.md#swapping-minds)) (read 2026-11-05) |
| The game deletes a profile's creature files when the profile or creature is deleted | partial 50% | deleting a profile on the Players page removes the profile's own folder with all it holds (its creature's files and saved games) and its creature's mind file in the game's `Scripts/CreatureMind`, as the original's Delete Player ([the Players page](../../../interface/screen-and-readouts.md), Delete Player); `PlayerProfileSystem::DeleteProfile` through `FileSystemInterface::RemoveFolder` and `Remove`, nothing for a check run's profile; tests `PlayerProfileSystem.DeletingAProfileRemovesItsFolderAndItsCreaturesMindFile`, `PlayerProfileSystem.DeletingTheRunProfileOfACheckRemovesNoFile`. Missing: whether anything deletes a creature's files without its profile has not been read |

## Carrying the creature on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is saved as the profile's creature, so a new game can start with it | partial 75% | the profile's creature's load (`player_creature::LoadMyCreature`) and `CURRENT_PROFILE_HAS_CREATURE` read the profile's file, which is the `--creature-file` setting or a kept profile's own (`ECS/PlayerCreature.cpp`; tests `ProfileCreatureTest.TheCreatureFileIsAKeptProfilesOwnElseTheSetting`, `PlayerCreature.*`), `IS_KEEPING_OLD_CREATURE` reads the land's skip flag, and the land change saves it back (rows above); missing: the exe's save every 600 ticks (the physique) and the fields the save does not write |
| Moving from one land to the next, the player's creature comes along as it was | partial 75% | it is saved at the land change and the next land's load (`player_creature::LoadMyCreature`) makes it again from the file, as the exe; it comes back with its mind, stage, size, strength, alignment, fatness, age, energy and the other needs the file keeps (`creature_mind_body::WithSavedNeeds`) and its tattoos (`player_creature::RestoreBody`, test `PlayerCreatureNativesTest.ACreatureMadeFromAMindFileWearsTheFilesTattoos`), as the exe's mind reader puts them back; see [../story/portals.md](../story/portals.md#the-creature); finished by [saves_and_files.md](saves_and_files.md) "At a land change the local creature's mind is saved" |
| Scripts can load a creature from a file into the land | done | `LOAD_CREATURE`, the land scripts' `CREATE_CREATURE_FROM_FILE` (`src/LHScriptX/FeatureScriptCommands.cpp`) and a computer god's creature all take one step, `ecs::player_creature::MakeFromMindFile` (`src/ECS/PlayerCreature.cpp`), as the original's three share one; `LOAD_MY_CREATURE` too; test `ScriptLoadCreatureTest.TheFileStepMakesTheSameCreatureForEveryCaller` |
| Scripts can ask whether the profile has a creature file | done | `CURRENT_PROFILE_HAS_CREATURE` (the exe's native 463): whether the profile's mind file exists in `Scripts/CreatureMind` (`player_creature::ProfileHasCreature`, `src/ECS/PlayerCreature.cpp`); tests `ProfileCreatureTest.*` |
| A creature loaded from a file takes its species, name, alignment, strength, size, fatness, needs and tattoos | done | `ecs::player_creature::MakeFromMindFile` with `CreatureMindFileBody` takes all of it, the tattoos put on by `player_creature::RestoreBody` as the file is loaded, as a swap does (tests `PlayerCreatureNativesTest.ALoadedCreatureTakesTheFilesFatnessAndShowsItEasedTwice`, `ALoadedCreaturesBodyTakesTheFilesNeedsBeforeItsFirstTurn`, `ACreatureMadeFromAMindFileWearsTheFilesTattoos`, `CreatureMindFileBody.TakesTheBodyAFileKeeps`); the debug spawner's saved-creature list (`src/Debug/CreatureSpawnerMindFiles.cpp`) spawns them too |
| A mind can be loaded into an existing creature: a script swaps two creatures' minds, each body keeping its name, size and alignment | done | `SWAP_CREATURE`, `CreatureMindSystem::SwapMinds` (`CreatureMindSwap.cpp`, `creature_swap`, `player_creature::TakeUpBody`), as the exe's `SwapMinds` ([swapping minds](../../../creature/players-creature.md#swapping-minds)); checked against runblack.exe v1.42 (2026-11-03, [what the load carries](../../../creature/players-creature.md#swapping-minds)): each body takes up everything `LoadMind` 0x4E84A0 reads of the other's mind file but its own name, size (+0x6C) and alignment, which `SwapMinds` 0x47BEE0 puts back: the strength, fatness, needs and age (`LoadBody` 0x4E8FF0), the tattoos (0x4E8AB5), the leashes known (0x4E8A88), the stage (0x4E89E8), the counts of kills, poos, mushrooms and fights (0x4E855C..0x4E85DB, `CreatureTallies`, `CreatureFightRecord`), the growing-up script flag (0x4E8A6A), the fight health (0x4E8BEB) and the cuts and bruises (0x4E9120, `CreatureMarks`), the shown fatness brought three steps (0x4E9128, 0x4E8A05, 0x47C257), the 3D body given the file's size (0x4E9119), and while a spell is on the file's alignment, size or strength left for the spell to put back (0x4F5FE0, 0x4F5EA0, 0x4F5F40); the ease towards the other's size and alignment is the original's (0x50776C..0x5077DF, `creature_swap::Ease`); tests `CreatureMindSystemTest.SwappedMindsTakeTheirStageCountsAndFightHealthToTheOtherBody`, `PlayerCreatureNativesTest.ASwappedBodyUnderASpellLeavesTheFilesValuesForTheSpellToPutBack`, `PlayerCreatureNativesTest.ASwappedBodyTakesTheFilesShownFatnessStepsLessonFlagAndFightHealth`, `PlayerCreatureNativesTest.ASwappedBodyKnowsTheLeashesTheFileSays`, `CreatureSpells.AStrengthOrAlignmentBeforeSpellsGoesToTheSpellThatIsOn`. What openblack keeps no live state of (the angry-avoidance flag, a lesson feedback value, the confinement, three body numbers, the speeds, the environment mapping) travels on in the mind file; the load's stage message is the first stage's in every swap the story plays, which tells nothing |

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
| A creature's state is added to the game's checksum, so the machines can tell when they drift apart | pending | no network checksum; the debug state hash used by our verification runs has a "creature" part (`player_creature::HashCreatures`, test `PlayerCreatureNativesTest.TheHashPartFollowsEachCreaturesFields`), which is not the game's |
| In skirmish, computer players' creatures load from the template minds | pending |  |
| A computer player's creature, with a temple, learns at its start every miracle its player can cast, and heal, food, wood and water (no difficulty is read) | done | `ComputerPlayerSystem`, `computer_player::MiraclesToLearn`; test `test/test_computer_player_start.cpp` (`ComputerPlayerStart.TheCreatureLearnsThePlayersMiraclesAndHealFoodWoodAndWater`); as `Creature::BalanceKnowlegeForComputerPlayer` 0x47DEB0 ([computer minds](../../../hand/computer-minds.md), [players-creature](../../../creature/players-creature.md#a-computer-players-start-on-the-land)) (read 2026-11-05) |
