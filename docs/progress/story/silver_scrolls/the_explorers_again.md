# The Explorers Again

A Land 5 silver scroll that only exists if the player helped the singing missionaries of the first land sail away
([The Explorers](./the_explorers.md)): their wrecked boat lies on Land 5's shore, they have founded a Norse settlement
in the player's name, and tapping the scroll by the wreck plays a short film in which three of them dance, thank the
player and present a polar bear brought from an icy island, which the player may then swap for their creature.

**Land:** 5 · **Giver:** three Norse sailors by their wrecked boat on the shore below the player's Norse town · **Script:** MissionariesReturned · **Reward:** a polar bear to swap for the player's creature (and a ready-built Norse town of 11 buildings and 20 villagers, plus the three sailors) · **Repeatable:** no (the swap offer stays open)

Sources: the challenge script and the shared creature-swap script (the original source text, which matches the PC game's
compiled `challenge.chl`), the land's control script that starts it, Land 1's missionaries script that sets the
condition, the land's own building script `Land5.txt`, the game's text table and the executable. The land as a whole is
in [../land_5.md](../land_5.md), its script program in [../../scripts/land5_script.md](../../scripts/land5_script.md),
creature swaps in [../challenges_and_rewards.md](../challenges_and_rewards.md). openblack's state is judged on this
tree: openblack never runs Land 5's control script (the land-loading native does nothing and the story always begins
with Land 1's control script), so this challenge never starts. Of the 57 script functions it and the swap script need,
53 work in `src/CHLApi.cpp`, among them building, making villagers and features, the scroll, dialogue, advisors, camera
moves and animations; the other 4, the creature's actions and species, the challenge record and the swap itself, only
log "not implemented", and a script cannot make the polar bear (`CreateScriptObject` makes no creatures). Every row is
todo unless the notes say otherwise.

**Progress: 0/39 done, 3 partial — 4%**

## Whether it happens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts it as Land 5 begins, but only if the missionaries' boat sailed in the first land, a flag set at the very end of [The Explorers](./the_explorers.md) when they leave happily (after the water-miracle dispenser reward) | todo | `LandControl5` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); the flag is a global of the challenge program and lives as long as the story does |
| If the boat never sailed (the challenge was ignored, failed, or the missionaries were killed) nothing at all is made: no wreck, no settlement, no scroll | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A single-quest test launcher that sets the flag and starts it exists in the sources but is not part of the shipped game | n/a | developer test only |
| It is started in the background alongside the land's other challenges and has no time limit | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Everything stops when Nemesis loses his last village (the land then stops every script but its own) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The settlement and the wreck

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wreck of the missionaries' boat is placed on the shore, about 157 from the player's second Norse town, at half health | todo | `Create` makes features, but the health property (`SetProperty`) is not ported; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The land file gives the player a Norse town there (town 1, starting belief 2, the lightning miracle, eight fields and two fish farms) with 11 planned buildings and nothing built | partial | our tree makes the town (`TownArchetype`) and records its planned buildings (`CREATE_PLANNED_ABODE`, `src/LHScriptX/FeatureScriptCommands.cpp`) when the land is loaded; the land is never reached in the story; see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| The script builds all 11 of them at once (village centre, two houses of one style, three of another, one of a third, the graveyard, the Wonder, the crèche and the storage pit), each strongly wanted (desire 9) and set fully built | todo | `BuildBuilding` (forcing a planned building, `ecs::building_sites`) and the built percentage property work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| 20 villagers are made within 20 of the village centre, 10 men then 10 women, each joined to the town and left to live there | todo | `Create` makes villagers, `FlockAttach` and `ReleaseFromScript` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Without this challenge the town stays an empty plan with no people for the whole land | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A silver scroll is put up by the wreck, 2 above the ground; no advisor points it out (the call to the shared "Your godly attention is required here, Leader." notice is switched off) | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The scroll waits for a tap however long it takes | todo | `GameThingClicked` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The arrival film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping the scroll records the challenge as started, titled "The Explorers Again", with the good advisor's reminder "This is the boat the Missionaries arrived in." | todo | `Snapshot` is a stub |
| A film starts with the missionaries' background tune; the camera flies to look down at the wreck over 4 seconds | partial | `StartMusic` works; the camera moves work too, but the film never runs |
| Three Norse sailors come out of the wreck one after another (half a second, then a third of a second apart) and walk to three spots on the beach | todo | `Create` and `MoveGameThing` work for villagers; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Both advisors come out; evil: "On no. It's those crazy missionaries again." (the text table's own typo); after 2 seconds the camera creeps a little further back over 12 seconds | todo | `SpiritEject`, `RunText` and the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| As each sailor reaches his spot he starts dancing: the first circling, the second as a partner, the third arm in arm | todo | the villagers' script animations (`SetScriptState`, `SetScriptUlong`) work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Evil: "I never thought their stupid boat could ever make it this far."; good: "Their faith in us is as strong as it ever was."; both go home | todo | `RunText`, `SpiritHome` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera drops close over 8 seconds; the first sailor turns to it and talks: "Our belief in you kept us going, Holy One." then dances again | todo | `RunText`, `SetFocus` and the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera moves again over 12 seconds; the second sailor turns and talks: "We're travelled far and done much in your name." (the text table's typo); meanwhile the third turns to talk and the camera swings over 3 seconds to look up at their new town, and each goes back to dancing | todo | as above; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| After 1 second the camera turns to the first sailor (2 seconds), who says: "We also picked up something you might find useful." and "We humbly offer it to you by way of thanks." | todo | as above; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The polar bear

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reward sting plays and a polar bear creature appears at the water's edge by the wreck | todo | `Create` makes no creatures; `PlaySoundEffect` works |
| The camera drops to the bear's level over 6 seconds; the sailors face it and talk and point at it one after another, then the first turns back to the camera and the others dance | todo | no bear |
| The bear walks up the beach towards the sailors; 2 seconds later the first sailor: "It's a big white animal we found on an icy island we visited." | todo | no bear |
| The bear turns to the camera; evil: "What the heck is that?"; good: "It's beautiful. These people have travelled a long way, haven't they?" | todo | no bear |
| The music stops, the advisors go home, and 1 second later the challenge is recorded as finished (success 1, alignment 0); the film ends | todo | `Snapshot` is a stub |
| The three sailors join the new town and are let go to live as ordinary villagers | todo | `FlockAttach` and `ReleaseFromScript` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The bear can't be picked up and a silver scroll hangs 1 above its head; it keeps turning to look at the camera | partial | the pick-up flag works (`SetIdPickupable`); the bear (creatures are not made by `Create`) and the scroll over it don't |
| While the camera is within 100 of the scroll with the bear on screen, the evil advisor points at it: "Swap your Creature with this one if you want." at most once a minute | todo | no bear |
| Tapping the scroll or the bear with the player's creature further than 50 away: evil: "You'll need to bring our Creature, Boss." and the offer starts over | todo | no bear |
| With the creature within 50 the scroll goes, the creature is unleashed and walked to within 15 of the bear (given up to 5 seconds), and both look each other over | todo | `CreatureDoAction` is a stub |
| Every 30 seconds the good advisor asks: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel." | todo | no bear |
| Tapping the bear confirms; tapping the player's own creature, or 110 seconds without an answer, cancels and the scroll comes back | todo | no bear |
| The swap: a two-creature camera, the creatures face each other, sparkles over both, each points at the other, the minds are swapped with the swap sound, and 3 seconds after both have finished pointing the camera returns | todo | `SwapCreature` is a stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| Evil: "Great, Boss. If you want your old Creature back, just return here later."; the old creature stays as the one on offer, idle and not pick-up-able, under a new scroll | todo | `SwapCreature` stub |
| The offer never ends: the player can swap back and forth for the rest of the land | todo | the swap script's loop has no way out |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature plays no part in the film; it is only needed, within 50 of the bear, for the swap | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sailors are switched to high-detail drawing before they are made, so the switch does nothing and they are drawn as normal villagers; switching it off at the end is likewise empty | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The storage pit, graveyard and Wonder are looked up at each other's places (mixed up), which does no harm because all of them are only set built | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The swap script makes a 60 to 120 second timer for the offer that it never reads | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Cut and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sailor's line "Eventually we landed and started this settlement in your honour." is in the text table but no script says it | n/a | never in the game |
| A second sailor's thanks ("You've been so devine we thought we'd make an offering of it") was written into the film and switched off; it has no text line | n/a | never in the game |
| An earlier start that waited for the camera to look at the wreck, instead of a scroll, is switched off | n/a | never in the game |
