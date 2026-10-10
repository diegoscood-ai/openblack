# The Rejuvenator

A third-land silver scroll that appears once the player wins the Egyptian village: an old woman at a lone hut can turn
old villagers back into children. After she has rejuvenated three, bringing her a child makes her magic go wrong: the
hut blows up and the child becomes an ape creature (a chimp if the player's creature already is an ape), which the
player can swap their creature for. Killing her ends the quest with no creature.

**Land:** 3 · **Giver:** an old woman at a lone Celtic hut, a one-house neutral village · **Script:** SwapToApe · **Reward:** an ape creature to swap to (a chimp if the player's creature is an ape) · **Repeatable:** no (the swap offer itself never ends)

Sources: the quest's script (the original source text, checked line by line against the decompile of the PC game's
compiled `challenge.chl`), Land 3's control, set-up and map scripts, the game's text table and its species list.
openblack is judged on this tree: the quest's challenge-record, disciple and creature commands are stubs in
`src/CHLApi.cpp` (they only log "not implemented"), `Create` makes villagers but no creatures (`CreateScriptObject`),
and `SwapCreature` is a stub. The land-loading command does nothing, so Land 3's control script, which starts this
quest, never runs. Every row is todo unless the notes say otherwise. The land is in [../land_3.md](../land_3.md); the
swap that ends the quest is described in full in [creature_swaps.md](./creature_swaps.md).

**Progress: 0/57 done, 6 partial — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script is started by the land's control script as soon as the player has come through the vortex onto the land | todo | `LandControl3` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)) |
| It then checks every 10 seconds whether the Egyptian village (Lethys's at the start) belongs to the player; nothing shows until it does | todo | reading a town's owner (`GetProperty` player) is not ported; no other condition, so it can appear while the creature is still Lethys's prisoner |
| Once the Egyptian village is the player's, the challenge is registered and a silver scroll appears 4 above the lone Celtic hut, some 440 from the Egyptian village | todo | `CreateHighlight` and the height property (`SetProperty` y position) work; `Snapshot` is a stub; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll and it is on screen, the evil advisor steps out at most every 30 seconds, points at it and says "A Silver Reward Scroll. Let's see what it's all about." | todo | the shared notify script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`); the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Clicking the scroll or the hut makes the scroll active and the quest goes on; no time limit before that | todo | `GameThingClicked` and `SetActive` on a highlight work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An old woman (a Norse housewife) comes out of the hut | todo | `Create` makes villagers; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A cut scene with the "spooky" script music starts; the camera glides up and out to look at her (3 seconds) | partial | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene never runs |
| After 2 seconds the camera moves to a low view in front of the hut (4 seconds) as she walks to a spot about 4 in front of the door | todo | `MoveCameraPosition` and `MoveGameThing` work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| When she is there the camera centres on her head (3 seconds) and she turns to face it | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The challenge record opens: title "The Rejuvenator", progress 0, reminder "This woman is giving people the power of youth." (good advisor) | todo | `Snapshot` is a stub |
| She gossips (animation) as the camera closes right in over 8 seconds: "Oooh 'Ello. Pleased to meet you. I can make the old young." | todo | the villagers' script animations and `RunText` work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A second gossip animation: "Bring me an old Villager and I'll wind back the years. Oh yes." (spoken in the game's "with interaction" form; undetermined what that changes for the player) | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The camera goes back to where the player had it (5 seconds for the position, 4 for the focus) while she starts wandering around her spot, within 6 of it | todo | `SetScriptStatePos` and `SetScriptFloat` (the wander) are stubs |
| Good advisor: "I don't like this old bat at all."; evil advisor: "I say we try her out. It might hurt people." | todo | `SpiritEject` and `RunText` work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The music stops and the scene ends | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Rejuvenating three villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player brings villagers to her, normally by carrying them in the hand and putting them down near the hut | partial | carrying villagers works in our hand (`src/ECS/Systems/Implementations/HandSystem.cpp`); the quest's checks never run |
| Every 3 seconds the quest picks one villager within 20 of the hut that no script is using | todo | `CreateTimer` and `CallNear` work; undetermined which villager is picked when there are several; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| It acts only when that villager is not in the hand and has more than 5% health, and the woman is not in the hand and is within 20 of the hut | todo | the in-hand and health properties (`GetProperty`) are not ported |
| A villager aged 13 or younger: the woman says "They're not old enough. Hardly worth the bother." and the quest waits until that villager is more than 20 from the hut, or 20 seconds | todo | needs the in-hand and health reads |
| An older villager is rejuvenated in a cut scene: the camera cuts to a view by the hut and creeps sideways over 10 seconds | partial | the camera cut works (`SetCameraPosition`, `SetCameraFocus` in `src/CHLApi.cpp`); the scene never runs |
| The villager and the woman both walk very slowly (speed 0.2) to the hut's door | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| When both are within 1 of it: a spell-success sparkle for 4 seconds, a light camera shake (radius 20, strength 0.1, 1 second), and the villager becomes four years old and loses their job | todo | `SetDisciple` is a stub; the age property, `ShakeCamera` and `SpecialEffectObject` work |
| A second later the child walks to the woman's spot in front of the hut; when the camera has finished its move it cuts back to where the player had it, and the woman wanders again | partial | the camera cut back works; the scene never runs |
| The child is then let go to live as a normal villager | todo | `ReleaseFromScript` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| After three villagers are rejuvenated the woman says "Phew. All this work has made me feel dizzy." and the record moves to half done | todo | `UpdateSnapshot` is a stub |
| There is no time limit; villagers of any tribe count | todo | read from the script; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Hut and woman rules (both stages)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the hut is damaged (its health drops below what it was when she came out) and the camera is within 100 with the hut on screen, she says "Oh come on! Leave my hut alone!"; this is checked only once, so damage seen from far away uses it up silently | todo | the health read is not ported |
| If the woman is thrown, while she flies the camera follows her and the game runs at half speed, until she lands | todo | `SetGameSpeed` and the camera follow natives work, but the flying check needs the in-hand read; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The script never sets the game speed back to normal after her flight (undetermined whether ending the scene restores it) | todo | undetermined whether ending the scene restores it; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| While she is in the hand, or more than 20 from the hut, nothing happens | todo | the in-hand read is not ported |

## The transformation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No hint is given: the next step is to bring a child; every 3 seconds the quest picks a child within 20 of the hut that no script is using, and acts if it is alive, the woman isn't in the hand and is within 20 of the hut | todo | the evil advisor's hint "We should bring her a kid, Boss. See what happens." is commented out as giving it away; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The three children she just made are let go beside her and count, so the transformation can follow at once without the player bringing anyone | todo | read from the script; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A cut scene: the woman is drawn in high detail, sound effects are switched on, the camera cuts to a view of the hut and both walk to its door | partial | the cut works (`SetCameraPosition`, `SetCameraFocus`); the scene never runs |
| When the child is within 1 of the door: the short epic sting plays, the camera shakes (4 seconds) and a spell-success sparkle covers the child for 5 seconds | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A second later the woman walks back to her spot (speed 0.4); after 2 seconds she turns to the hut, scared stiff, and the hut explodes and is gone | todo | `ObjectDelete` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Half a second later an ape creature appears where the child stood, or a chimp if the player's creature is already an ape; the child is removed | todo | CHL `Create` makes no creatures (`CreateScriptObject` in `src/CHLApi.cpp`); `CreatureArchetype::Create` builds both species, but our species list numbers the ape differently from the scripts (the scripts' ape is number 0, which our `Enums.h` calls Unknown; ours is `GiantApe`) |
| She turns to the creature, scared stiff again; a second later the record closes as a success (progress 1, alignment 0.3 towards evil) and its reminder becomes "But it's a special Creature. We can swap our Creature to it." | todo | `Snapshot` is a stub; no creature is made |
| The woman: "Oops. I've created a Creature. Sorry. What a faux pas." and she walks off towards the Egyptian village | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Good advisor: "But it's a special Creature. We can swap our Creature to it."; evil advisor: "Yeah? I suppose we bring our boy over and click on it." (the last token is the action-button picture); good advisor: "Precisely." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The camera cuts back to where the player had it, the music stops and the scene ends; the quest's scroll is removed | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The woman is removed the moment she is off screen | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears over the new creature, which keeps turning to look at the camera; when the camera is near, the evil advisor says "Swap your Creature with this one if you want." at most once a minute | todo | the shared swap script; full detail in [creature_swaps.md](./creature_swaps.md) |
| Clicking it with the player's creature further than 50 away: "You'll need to bring our Creature, Boss." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Otherwise the two creatures meet, the good advisor asks to confirm and clicking the new creature confirms, clicking the player's own cancels | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| On confirmation the creature's mind moves into the ape (or chimp) in a short scene, and the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | todo | `SwapCreature` is a stub; our tree cannot move the player's creature's mind into another body |
| The old body stays under a scroll and the offer never ends, so the player can swap back and forth | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Killing the woman

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the woman dies at any point before the transformation, the quest ends: the record closes as finished (progress 1) with alignment fully evil, and no creature is made | todo | the health read is not ported; `Snapshot` stub |
| Evil advisor: "You killed her. Don't feel bad. I'd have done the same. Ha."; good advisor: "Oh when will the horror ever end? Don't answer that, evil one." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The scroll over the hut is not removed in this case (undetermined whether the game removes it when the script ends) | todo | undetermined whether the game removes it when the script ends |

## Aftermath, advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut is destroyed for good; the rejuvenated villagers stay children and grow up normally | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The reminder line, replayed by clicking the scroll, is spoken by the good advisor | todo | the shared reminder script; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Music: the "spooky" script track for the introduction, the short epic sting for the transformation | partial | `StartMusic` works; counted in the scene rows; the scene never runs |
| There is no reward chest; the creature is the reward | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The player's creature only matters for its species (ape gives a chimp) and for the swap | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Bugs, quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rejuvenated child is let go near the hut, so the next check of the first stage can pick it again and the woman says "They're not old enough. Hardly worth the bother.", pausing the quest up to 20 seconds (undetermined whether the villager search also returns children) | todo | read from the script; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The opening record line was first written before the cut scene and moved inside it; a closing record line after the swap is commented out | n/a | source history only |
| The text table has no lines numbered 1, 6 or 7 for this quest, and line 17 (the evil advisor's hint) exists but is never said | n/a | cut material |
| The source creates a plain "female" villager; the compiled game turns that into a Norse housewife | n/a | data detail |
| The record's alignment is mildly evil for a successful transformation and fully evil for killing her | todo | `Snapshot` is a stub |
| A saved game keeps the stage, the woman and the offer | todo | openblack has no saved games |
