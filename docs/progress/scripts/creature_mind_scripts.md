# Creature mind scripts

`Scripts/CreatureMind/` holds the ready-made minds the game gives creatures it creates itself: the rival gods' creatures in
the story and the computer players' creatures in the skirmish lands. A mind file is a whole creature's mind (its desires,
what it has learnt, its opinions of actions, the miracles it knows and its body) saved in the same format as the player's
own creature. The player's creature, saved between lands and in profiles, is the creature domain's
([../creature/](../creature/)); reading the format is `components/creaturemind` (`MindFile`).

**Progress: 3/14 done, 6 partial — 43%**

## The shipped mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `ComputerControlledCreature` (5,748 bytes, format 30): a fully grown, trained mind with 40 desires, 80 decision trees, 322 action opinions, 6 actions and 5 miracles known; the skirmish lands give it to every computer player's creature (12 lines in five playground scripts, as horse, tortoise, zebra, leopard and lion) | done | `components/creaturemind` reads it (`test/creature/test_creature_mind_file.cpp`); `CREATE_CREATURE_FROM_FILE` loads it through the resource cache (`Locator::resources`, creature minds) |
| `KhazarCreature`, `LethysCreature` and `NemesisCreature` are one and the same file (5,268 bytes, format 25, a fully grown mind with 313 action opinions and no miracles known); the story loads each for its rival god | done | read by `components/creaturemind` |
| The four older files (`CreatureDestroyOtherCreatures`, `CreatureDestroyTowns`, `CreatureImpressTowns`, `CreatureProtectTowns`, 54 to 130 KB) are in a format the shipped game can't load, and nothing names them | n/a | left over from development; our reader rejects them the same way |
| A mind file is found by its name in the creature mind folder, without an extension | done | the creature mind path of the file system in `FeatureScriptCommands::CreateCreatureFromFile` and `ecs::player_creature::LoadMyCreature` |
| A name written in capitals in a land script (`COMPUTERCONTROLLEDCREATURE` in *Three Gods*) finds the same file, as file names on Windows ignore case | partial | works on Windows; `DefaultFileSystem::FindPath` looks names up with case, so on systems with case-sensitive file names it would not be found |

## Rival creatures in the land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script creates a rival god's creature of a species from a mind file at a place | partial | `FeatureScriptCommands::CreateCreatureFromFile` makes a `CreatureArchetype` with the mind loaded; it faces a fixed way and takes our starting size and body for the species |
| A rival creature is made to like a player (`SET_COMPUTER_PLAYER_CREATURE_LIKE`, 16 lines in five playgrounds, always liking the human player) | todo | the command is empty |
| A rival creature then acts on its mind (wanders, eats, impresses or attacks towns) | partial | `ecs::creature_loop::ProcessTurn` runs the creatures each turn from `Game.cpp`; how far the mind's decisions match the original is the creature domain's (see ../creature/) |
| *Island Wars* gives its three rivals the Nemesis mind as a leopard, a gorilla and a brown bear | partial | made as above |

## Rival creatures in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's set-up script loads the rival gods' creatures from their mind files: Khazar's as a tortoise, Lethys's as a wolf and Nemesis's as a lion, for their players at given places | todo | `LoadCreature` in `src/CHLApi.cpp` logs "not implemented" |
| Each is then named, made fully grown, sized and taught what it needs (dozens of actions taught at once) | todo | `SetCreatureName`, `CreatureSetKnowsAction`, `CreatureForceFriends` and `CreatureAutoscale` log "not implemented" |
| A rival creature heals slowly between fights, from nothing to full in ten minutes | partial | the object property functions work for villagers and animals; a creature's properties and its healing are not ported |
| In the final fight the last rival's creature is loaded as the same species as the player's | todo | `LoadCreature` logs "not implemented" |
| The player's own creature is loaded into each new land at a place (`load my_creature`) | partial | `LoadMyCreature`: `ecs::player_creature::LoadMyCreature` loads the profile's mind file (`--creature-file`) at the land's place; saving the creature between lands is the creature domain's and not ported |
| A creature can be made from another's mind as another species (the creature swaps at the breeder) | todo | `SwapCreature` logs "not implemented" |
