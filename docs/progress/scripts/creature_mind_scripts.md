# Creature mind scripts

`Scripts/CreatureMind/` holds the ready-made minds the game gives creatures it creates itself: the rival gods' creatures in
the story and the computer players' creatures in the skirmish lands. A mind file is a whole creature's mind (its desires,
what it has learnt, its opinions of actions, the miracles it knows and its body) saved in the same format as the player's
own creature. The player's creature, saved between lands and in profiles, is the creature domain's
([../creature/](../creature/)); reading the format is `components/creaturemind` (`MindFile`).

**Progress: 5/14 done, 6 partial — 57%**

## The shipped mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `ComputerControlledCreature` (5,748 bytes, format 30): a fully grown, trained mind with 40 desires, 80 decision trees, 322 action opinions, 6 actions and 5 miracles known; the skirmish lands give it to every computer player's creature (12 lines in five playground scripts, as horse, tortoise, zebra, leopard and lion) | done | `components/creaturemind` reads it (`test/test_creature_mind_file.cpp`); `CREATE_CREATURE_FROM_FILE` loads it through the resource cache (`Locator::resources`, creature minds) |
| `KhazarCreature`, `LethysCreature` and `NemesisCreature` are one and the same file (5,268 bytes, format 25, a fully grown mind with 313 action opinions and no miracles known); the story loads each for its rival god | done | read by `components/creaturemind` |
| A mind file is found by its name in the creature mind folder, without an extension | done | the creature mind path of the file system in `FeatureScriptCommands::CreateCreatureFromFile` and `ecs::player_creature::LoadMyCreature` |
| A name written in capitals in a land script (`COMPUTERCONTROLLEDCREATURE` in *Three Gods*) finds the same file, as file names on Windows ignore case | partial 50% | works on Windows; `DefaultFileSystem::FindPath` looks names up with case, so on systems with case-sensitive file names it would not be found; re-measured 2026-10-21: works on Windows; on case-sensitive file systems the name is not found |

## Rival creatures in the land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script creates a rival god's creature of a species from a mind file at a place | partial 70% | `FeatureScriptCommands::CreateCreatureFromFile` makes a `CreatureArchetype` with the mind loaded; it faces a fixed way and takes our starting size and body for the species; re-measured 2026-10-21: made with its mind; the facing and size are ours |
| A rival creature is made to like a player (`SET_COMPUTER_PLAYER_CREATURE_LIKE`, 16 lines in five playgrounds, always liking the human player) | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42) |
| A rival creature then acts on its mind (wanders, eats, impresses or attacks towns) | partial 70% | `ecs::creature_loop::ProcessTurn` runs the creatures each turn from `Game.cpp`; how far the mind's decisions match the original is the creature domain's (see ../creature/); re-measured 2026-10-21: the creatures run each turn; the mind's match is the creature domain's |
| *Island Wars* gives its three rivals the Nemesis mind as a leopard, a gorilla and a brown bear | partial 50% | made as above; re-measured 2026-10-21: as the row above |

## Rival creatures in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's set-up script loads the rival gods' creatures from their mind files: Khazar's as a tortoise, Lethys's as a wolf and Nemesis's as a lion, for their players at given places | pending | `LoadCreature` in `src/CHLApi.cpp` logs "not implemented" |
| Each is then named, made fully grown, sized and taught what it needs (dozens of actions taught at once) | pending | `SetCreatureName`, `CreatureSetKnowsAction`, `CreatureForceFriends` and `CreatureAutoscale` log "not implemented" |
| A rival creature heals slowly between fights, from nothing to full in ten minutes | partial 20% | the object property functions work for villagers and animals; a creature's properties and its healing are not ported; re-measured 2026-10-21: a creature's properties and healing are not ported |
| In the final fight the last rival's creature is loaded as the same species as the player's | pending | `LoadCreature` makes a player's creature of the script's species from the mind file (`player_creature::ScriptLoadCreature`) and `GameSubType` reads the player's creature's species, both real; the final fight is not checked in play |
| The player's own creature is loaded into each new land at a place (`load my_creature`) | partial 60% | `LoadMyCreature`: `ecs::player_creature::LoadMyCreature` loads the profile's mind file (`--creature-file`) at the land's place; saving the creature between lands is the creature domain's and not ported; re-measured 2026-10-21: loaded at the land's place; saving it between lands is not ported |
| A creature can be made from another's mind as another species (the creature swaps at the breeder) | done | ported from raffclar PR #384, unverified against v1.42: `SwapCreature` swaps the two creatures' minds (`CreatureMindSystem::SwapMinds`) |
