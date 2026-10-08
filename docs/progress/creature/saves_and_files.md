# Creature saves and files

A creature outlives any one game: its mind (everything it has learnt), its body and its tattoos are kept in creature
files that belong to the player's profile, so the same creature can be carried from land to land and into new games,
taken into skirmish and network games, and uploaded to the game's website. The game's own computer gods and skirmish
opponents also come from creature files.

**Progress: 14/37 done, 8 partial — 49%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Creature mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mind is saved in its own file in the game's creature mind folder, named for the creature | partial | mind files are read from `Scripts/CreatureMind` (`resources::CreatureMindLoader`, `LOAD_MY_CREATURE` in `ECS/PlayerCreature.cpp`); the writer exists (`components/creaturemind` `MindFile`, test `CurrentVersionRoundTrips`) but nothing in the game or the debug window writes a creature's mind to disk |
| A small companion file keeps the creature's physique beside its mind file | todo | the physique file is neither written nor read |
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
| The game checks the mind and body loaded agree, and fixes or refuses them | todo |  |
| The game keeps a copy of the mind in memory to restore | todo |  |
| The game deletes a profile's creature files when the profile or creature is deleted | todo | no profiles |

## Carrying the creature on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is saved as the profile's creature, so a new game can start with it | partial | the loading side works: `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE` read the profile's file, which is the `--creature-file` setting (`ECS/PlayerCreature.cpp`; tests `PlayerCreature.*`, `ProfileCreatureTest.*`), and `IS_KEEPING_OLD_CREATURE` reads the land's skip flag; nothing ever saves the creature back |
| Moving from one land to the next, the player's creature comes along as it was | partial | each land's `LOAD_MY_CREATURE` loads the profile's file again, so it comes back as the file left it, not as it was; see [../story/portals.md](../story/portals.md#the-creature) |
| Scripts can load a creature from a file into the land | partial | land scripts' `CREATE_CREATURE_FROM_FILE` works (`src/LHScriptX/FeatureScriptCommands.cpp`) and so does `LOAD_MY_CREATURE`; the challenge scripts' `LOAD_CREATURE` is a stub in `src/CHLApi.cpp` |
| Scripts can ask whether the profile has a creature file | done | (added) `CURRENT_PROFILE_HAS_CREATURE`: whether the profile's mind file exists in `Scripts/CreatureMind` (`player_creature::ProfileHasCreature`, `src/ECS/PlayerCreature.cpp`); tests `ProfileCreatureTest.*` |
| Scripts can wipe a creature's mind | todo | `CLEAR_ACTOR_MIND` is a stub in `src/CHLApi.cpp`; the debug spawner can (`CreatureMindSystem::ClearLearning`) |
| A saved creature can be spawned with its species, name, alignment, strength, size and tattoos | partial | only the debug spawner's saved-creature list (`src/Debug/CreatureSpawnerMindFiles.cpp`); in the game, `LOAD_MY_CREATURE` takes species, size, alignment and strength (`CreatureMindFileBody`) |
| A mind can be loaded into an existing creature | partial | `CreatureMindSystem::LoadMind`, only called from the debug spawner |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature, its mind, body, spells, leash and what it is doing are saved and loaded with the game | todo | no saved games; see [../engine](../engine/) |
| Loading a saved game puts the creature back mid-action | todo |  |

## Sharing creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game makes a web page about the creature from a template: its name, age, alignment, health, energy and strength, with pictures | todo |  |
| The Creature Cave can take a picture of the creature for that page | todo | see [creature_cave.md](creature_cave.md) |
| The creature upload tool sends a creature file to the game's website under the player's account | n/a | the service no longer exists |
| A packed creature file is sent between machines | todo | no network play; see [../multiplayer](../multiplayer/) |

## Network and skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In a network game each player brings their own creature from their profile | todo | see [../multiplayer](../multiplayer/); a skirmish loads the profile's creature too: [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| A creature's state is added to the game's checksum, so the machines can tell when they drift apart | todo | no network checksum; the debug state hash used by our verification runs has a "creature" part (`player_creature::HashCreatures`, test `TheHashPartFollowsEachCreaturesFields`), which is not the game's |
| Selecting another player's creature waits for the network to agree | n/a | covered under network play |
| In skirmish, computer players' creatures load from the template minds | todo |  |
| A computer player balances its creature's knowledge to its difficulty | todo | unconfirmed: ../rival_gods/skirmish_opponents.md found no difficulty setting anywhere; needs checking what "difficulty" this reads |

## Debug output

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game can print a creature's information and statistics | partial | the debug spawner shows a creature's body, mind and needs (`src/Debug/CreatureSpawner*.cpp`); the game's own print-out isn't reproduced |
| A land script command lists all creatures | todo | `OUTPUT_CREATURES` throws "not implemented" (`src/LHScriptX/MapScriptCommands.cpp`) |
| Cheats: learn everything, learn ordinary things, next stage of growing up, make it fight | partial | only the debug spawner: set the stage of growing up, clear learning, start fights; see [development_phases.md](development_phases.md) |
