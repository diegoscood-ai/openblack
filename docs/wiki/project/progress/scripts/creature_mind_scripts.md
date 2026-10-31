# Creature mind scripts

`Scripts/CreatureMind/` holds the ready-made minds the game gives creatures it creates itself: the rival gods' creatures in
the story and the computer players' creatures in the skirmish lands. A mind file is a whole creature's mind (its desires,
what it has learnt, its opinions of actions, the miracles it knows and its body) saved in the same format as the player's
own creature. The player's creature, saved between lands and in profiles, is the creature domain's
([../creature/](../creature/)); reading the format is `components/creaturemind` (`MindFile`).

**Progress: 4/13 done, 0 chosen, 8 partial, 0 inferred — 72%**

## The shipped mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `ComputerControlledCreature` (5,748 bytes, format 30): a fully grown, trained mind with 40 desires, 80 decision trees, 322 action opinions, 6 actions and 5 miracles known; the skirmish lands give it to every computer player's creature (12 lines in five playground scripts, as horse, tortoise, zebra, leopard and lion) | done | `components/creaturemind` reads it (`test/test_creature_mind_file.cpp`); `CREATE_CREATURE_FROM_FILE` loads it through the resource cache (`Locator::resources`, creature minds) |
| `KhazarCreature`, `LethysCreature` and `NemesisCreature` are one and the same file (5,268 bytes, format 25, a fully grown mind with 313 action opinions and no miracles known); the story loads each for its rival god | partial 80% | read by `components/creaturemind`, whose older formats are tested only on made-up minds (`CreatureMindFile.OlderVersionsLeaveOutTheirFields`); no test reads a format 25 file |
| A mind file is found by its name in the creature mind folder, without an extension | done | the creature mind path of the file system in `FeatureScriptCommands::CreateCreatureFromFile` and `ecs::player_creature::LoadMyCreature`; test `ScriptLoadCreatureTest.TheScriptsSpeciesIsMadeAtThePointOnTheGroundThenLedAndGrownUp` |
| A name written in capitals in a land script (`COMPUTERCONTROLLEDCREATURE` in *Three Gods*) finds the same file, as file names on Windows ignore case | partial 50% | works on Windows; `DefaultFileSystem::FindPath` looks names up with case, so on systems with case-sensitive file names it would not be found |

## Rival creatures in the land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script creates a rival god's creature of a species from a mind file at a place | partial 90% | `FeatureScriptCommands::CreateCreatureFromFile` through `ecs::player_creature::MakeFromMindFile`: the species row, the file's body, on the ground facing 0, then the leashes known, stage 13 and led by its owner (tests `PlayerCreature` in `test/creature/test_player_creature.cpp`); left: which field of the mind file holds the fatness, and a version 30 file's fight health is not read ([land_script_commands.md](land_script_commands.md)) |
| A rival creature is made to like a player (`SET_COMPUTER_PLAYER_CREATURE_LIKE`, 6 lines in three playgrounds, always liking the human player) | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42); test `ComputerPlayerStart.TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize` |
| A rival creature then acts on its mind (wanders, eats, impresses or attacks towns) | partial 70% | `ecs::creature_loop::ProcessTurn` runs the creatures each turn from `Game.cpp`; how far the mind's decisions match the original is the creature domain's (see ../creature/) |

## Rival creatures in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's set-up script loads the rival gods' creatures from their mind files: Khazar's as a tortoise, Lethys's as a wolf and Nemesis's as a lion, for their players at given places | done | `LoadCreature` in `src/CHLApi.cpp` makes the player's creature of the script's species from the mind file at the point (`ecs::player_creature::ScriptLoadCreature`; tests `PlayerCreature.AScriptLoadTakesTheScriptsSpeciesAndTheFilesBody`, `AScriptLoadIsMadeAtThePointItselfNotItsCellsMiddle`); the story's set-up script for the computer players' creatures calls only functions openblack has ([challenge_scripts.md](challenge_scripts.md)); unverified against v1.42 |
| Each is then named, made fully grown, sized and taught what it needs (dozens of actions taught at once) | partial 85% | `SetCreatureName` (`ecs::player_creature::SetName`), `CreatureSetKnowsAction` (`CreatureMindSystem::SetKnowsAction`), `CreatureForceFriends` (`ecs::player_creature::ForceFriends`) and `CreatureAutoscale` (`ecs::player_creature::SetAutoscale`) in `src/CHLApi.cpp`; a script-loaded creature is grown up; tests in `test/creature/test_player_creature.cpp` (`SetCreatureNameNamesASetUpMindAtOnce`, `ForceFriendsMakesTwoCreaturesEachOthersFriends`, `CreatureAutoscaleSetsOnlyACreature`, `AScriptLoadedCreatureIsLedKnowsTheLeashesAndIsGrownUp`); unverified against v1.42. Missing: the teaching drops a skill past the table and gives no learning message ([challenge_natives_creature.md](challenge_natives_creature.md)) |
| A rival creature heals slowly between fights, from nothing to full in ten minutes | partial 20% | the object property functions work for villagers and animals; a creature's properties and its healing are not ported |
| In the final fight the last rival's creature is loaded as the same species as the player's | pending | `LoadCreature` makes a player's creature of the script's species from the mind file (`player_creature::ScriptLoadCreature`) and `GameSubType` reads the player's creature's species, both real; the final fight is not checked in play |
| The player's own creature is loaded into each new land at a place (`load my_creature`) | partial 75% | `LoadMyCreature`: `ecs::player_creature::LoadMyCreature` loads the profile's mind file (`--creature-file`) at the land's place, the file the land change saved it to (`player_creature::SaveMyCreature` from `Game::ChangeLand`); what the file leaves out and the fizz are missing ([../creature/saves_and_files.md](../creature/saves_and_files.md), [challenge_natives_creature.md](challenge_natives_creature.md)) |
| A creature can be made from another's mind as another species (the creature swaps at the breeder) | partial 70% | ported from raffclar PR #384, unverified against v1.42: `SwapCreature` swaps the two creatures' minds (`CreatureMindSystem::SwapMinds`); test `CreatureMindSystemTest.SwappedMindsCrossOverWhileEachBodyKeepsItsNameSizeAndAlignment`; unverified: the body taking the file's strength, fatness, needs and tattoos, and the ease towards the other's size and alignment ([challenge_natives_creature.md](challenge_natives_creature.md)) |
