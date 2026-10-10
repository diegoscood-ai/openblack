# Creature tools

The creature spawner: openblack's window for making creatures, looking inside them and telling them what to do. It
stands on its own and is also hosted in the editor's inspector for a picked creature.

**Progress: 26/29 done, 2 partial — 93%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## Spawning and picking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spawn a creature of any species with its body set up (size, alignment, fatness, strength, age), facing a way or at random | done | `src/Debug/CreatureSpawner.cpp` "Spawn" tab (facing, or "Random") |
| Put the body back to the species' defaults | done | "Species defaults" (`src/Debug/CreatureSpawner.cpp`) |
| List the creatures on the land, pick one, remove one or all | done | "Creatures" tab ("Select", "Remove", "Remove all") |
| The picked creature's tab, which the editor's inspector shows too | done | "Selected" tab; the editor's inspector has a Creature view (`src/Editor/Panels/InspectorPanel.cpp`) |

## Movement and actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Order it to walk or run to a point, flee from it or face it | done | the orders "Walk here", "Run here", "Flee from", "Face" (`src/Debug/CreatureSpawner.cpp`) |
| Order it to pick up, throw at, knock down or point at a thing | done | the orders "Pick up", "Throw at", "Knock down", "Point at" |
| Stop it, turn it to the camera, and show its route | done | "Stop", "Face the camera", "Show route" |
| Pause its mind so the body is posed by hand | done | "Pause mind" |
| Play any animation, gesture, sit, stand | done | "Play", "Gesture", "Sit down", "Stand up" |
| Stroke or slap it as the hand would | done | "Stroke", "Slap" |
| Show its hair, and its footprints, with the first of April smileys | done | "Show hair" (`src/Debug/CreatureSpawner.cpp`), "Show footprints" and "1 April" (`src/Debug/CreatureSpawnerFootprints.cpp`) |

## Body and needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fill up, starve, tire out, parch or bloat it | done | `src/Debug/CreatureSpawnerBody.cpp` ("Fill up", "Starve", "Tire out", "Parch", "Bloat") |
| Have it sleep, wake, eat, drink, poo, puke or faint, or drop food in front of it | done | `src/Debug/CreatureSpawnerBody.cpp` ("Sleep", "Wake", "Eat", "Drink", "Poo", "Puke", "Faint", "Drop food in front") |
| Turn fainting from exhaustion on and off | done | "Creatures faint" (`src/Debug/CreatureSpawnerBody.cpp`) |

## Looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tattoos: pick a slot and colour, put it on or clear it | done | `src/Debug/CreatureSpawnerAppearance.cpp` ("Set slot", "Put on", "Clear slot") |
| Wounds, burns and blood anywhere on its body, healed a step at a time, with the heal effect | done | `src/Debug/CreatureSpawnerAppearance.cpp` ("Add wound", "Add burn", "Add blood", "Heal a step", "Heal effect") |
| Drawing switches: blending its seams and its shadows | todo | no switches for the seams or the shadows in our spawner |

## Hands, leash and fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hold things in either hand; put down, toss, lob, eat, drop or look over what it holds | done | `src/Debug/CreatureSpawnerHands.cpp` |
| Make it leashable, put the leash on or off, tie it to a thing or another creature, keep it home or let it roam | done | `src/Debug/CreatureSpawnerLeash.cpp` ("Leashable", "Tie to nearest thing", "Tie to other creature", "Keep at home", "Let roam") |
| Place leash posts at the hand and show the rope's points | partial | "Show rope points" (`src/Debug/CreatureSpawnerLeash.cpp`); no placing of leash posts |
| Start a fight between two creatures, or let them fight by themselves; knock out, kill or bring round | done | `src/Debug/CreatureSpawnerFight.cpp` ("Start fight", "Fights by itself", "Knock out", "Kill for good", "Bring round") |
| Block and special moves on command, and the camera watching the fight | done | "Block", "Special", "Camera watches" (`src/Debug/CreatureSpawnerFight.cpp`) |

## Mind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Open a mind file through a file dialog, start a fresh mind, save the mind back | partial | the mind files of the game's folder are listed to load or spawn from, and "Fresh mind" (`src/Debug/CreatureSpawnerMindFiles.cpp`); no file dialog and no saving the mind back |
| Load one of the game's own mind files | done | the game's own minds listed (`src/Debug/CreatureSpawnerMindFiles.cpp`) |
| See and change its desires and likes, opinions of actions, decision trees and attitudes | done | `src/Debug/CreatureSpawnerMind.cpp` |
| Teach it by rewarding or punishing, clear what it has learnt | done | "Stroke", "Slap", "Clear learning" (`src/Debug/CreatureSpawnerMind.cpp`) |
| Show it a skill or a miracle to copy, cast or do it near | done | "Show it", "Cast it near", "Do it near" (`src/Debug/CreatureSpawnerMind.cpp`) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Mute creatures, or other players' creatures' voices | done | "Mute creatures", "Other players' creatures' voices" (`src/Debug/CreatureSpawnerAudio.cpp`) |
| Play each animation's sounds | done | "Play" per sound (`src/Debug/CreatureSpawnerAudio.cpp`) |
