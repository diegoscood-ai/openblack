# Swap To Brown Bear

A fifth-land silver scroll that appears once the player wins the neutral Japanese village: a villager complains of a
stench from the forest, and the player follows a trail of eight piles of dung, picking each one up, to find the brown
bear that made them. The bear stays as a creature the player can swap their own creature for, and a heal miracle chest
falls beside it.

**Land:** 5 · **Giver:** a Japanese farmer from a house on the edge of the neutral Japanese village · **Script:** SwapToBrownBear · **Reward:** a brown bear creature to swap to, and a heal miracle chest given to the player's home village · **Repeatable:** no (the swap offer itself never ends)

Sources: the quest's script (the original source text, checked line by line against the decompile of the PC game's
compiled `challenge.chl`, where it is compiled), Land 5's control script and map script, the game's text table and its
reward table (`info.dat`). openblack is judged on this tree: the quest's challenge-record, reward and creature commands
are stubs in `src/CHLApi.cpp` (they only log "not implemented"), `Create` makes the villager and the dung but no
creature (`CreateScriptObject`), and `SwapCreature` is a stub. The land-loading command does nothing, so Land 5's
control script, which starts this quest, never runs at all. Every row is todo unless the notes say otherwise. The land
is in [../land_5.md](../land_5.md); the swap that ends the quest is described in full in
[creature_swaps.md](./creature_swaps.md).

**Progress: 0/53 done, 3 partial — 3%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started by the land's control script the first time the neutral Japanese village becomes the player's, however it was won | todo | `LandControl5` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); reading a town's owner (`GetProperty` player) is not ported |
| It is started only once: losing the village to Nemesis and winning it back does not start it again | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The check sits in the land's main loop that runs while the creature is cursed, until the three wonder villages (Greek, Tibetan and Aztec) are all the player's; if the player takes all three before the Japanese village, the loop ends and this quest is never offered | todo | read from the control script: no later code starts it; the curse: [../gold_scrolls/i_have_a_surprise_for_you.md](../gold_scrolls/i_have_a_surprise_for_you.md); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The control script's town checks run one after another with the cut scenes for taking the wonder villages, so the scroll can appear a little after the village is won if one of those scenes is playing | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| As soon as the quest starts, a silver scroll appears over a house on the village's edge (about 60 from its centre) and the challenge is registered | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`), `Snapshot` is a stub; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor steps out at most every 30 seconds, points at it and says "We've got something to do. Let's see what it is." | todo | the shared notify script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Clicking the scroll or the house makes the scroll active and the quest goes on; nothing happens before that click (no time limit) | todo | `GameThingClicked` and `SetActive` on a highlight work (see [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md)); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first pile of dung appears at once, about 100 west of the house, with a swarm of flies over it that lasts for ever | todo | `Create` makes the dung (a lump of poo, `CreateScriptObject`) and `SpecialEffectObject` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A Japanese farmer comes out of the house (the source notes he was a woman until the voice used turned out to be a man's) | todo | `Create` makes villagers; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A cut scene with the "happy" script music starts; the farmer is drawn in high detail | partial | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene never runs |
| The camera glides to a low view by the house (3 seconds for the position, 4 for the focus) | todo | `MoveCameraPosition`, `MoveCameraFocus` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| After 2 seconds the farmer walks slowly (speed 0.4) to a spot a few paces in front of the house, then turns to face the camera | todo | `MoveGameThing`, `SetFocus` and the speed property work for villagers; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He plays his "poisoned" (retching) animation once while the camera creeps closer over 12 seconds | todo | the villagers' script animations (`SetScriptState`, `SetScriptUlong`, `Played`) work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A quarter of a second after it ends, the challenge record opens: title "Swap To Brown Bear", progress 0, reminder "The people here are complaining about the stench." (good advisor) | todo | `Snapshot` is a stub |
| He looks around as if searching and says "Ugh. There's a frightful stench coming from the forest." | todo | `RunText` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He turns to the first pile of dung, talks and points, as the camera swings up and round to look west over the forest (3 seconds): "Could you check it out, big one. It's sickening." | todo | `RunText`, `SetFocus` and the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera goes back to where the player had it, but now looking at the first pile of dung (3 seconds) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The farmer walks back to his house, the music stops and the scene ends; once he is within 2 of the house he is removed | todo | `MoveGameThing`, `StopMusic`, `ObjectDelete` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Straight after, the good advisor steps out: "Sickening, is it? I don't believe you." then "Hmm. Yes it really is rancid. I wonder what's causing it?" (the evil advisor is popped out for the second line, but both lines are recorded in the good advisor's voice) | todo | `SpiritEject` and `RunText` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Following the trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player clears the dung by picking each pile up with the hand; the moment a pile is in the hand it vanishes (the hand is left empty) and the next pile appears with its own flies | todo | the in-hand property (`GetProperty`) is not ported; `ObjectDelete` works |
| Only one pile exists at a time; they lead from near the village west, then north through the forest, each 25 to 45 apart: eight piles in all, the last about 220 north-west of the house | todo | positions from the script; needs the in-hand test |
| After the first pile, the good advisor: "Ugh. I can still smell something. There must be several more around. Let's clear them up." | todo | needs the in-hand test |
| After the seventh pile, the evil advisor: "The smell is almost gone. There's one more out there, though." | todo | needs the in-hand test |
| Picking up the eighth pile ends the trail | todo | needs the in-hand test |
| The flies of a picked-up pile are never removed by the script (undetermined whether they go with the pile) | todo | needs the in-hand test |
| There is no time limit, no counter on screen and no way to fail; the creature can't do it for the player (only the hand holding a pile counts) | todo | read from the script; whether a creature picking a pile up counts as "in hand" is undetermined |
| If the farmer's house is damaged at any time during the trail (its health drops below what it was after the introduction), the good advisor steps out, points at it and says "The owner of that house will be a little distraught." (once only) | todo | the health property (`GetProperty`) is not ported |
| A further hint was meant for the first pile: once it was on screen with the camera within 100, the good advisor would point at it and say "We really should clean up this mess. Someone could step in it." It never plays (see bugs) | todo | script behaviour to keep; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Finding the bear

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell-success sparkle appears for 2 seconds at a spot about 30 past the last pile, almost at sea level, and a brown bear creature appears there | todo | `Create` makes no creatures (`CreatureArchetype::Create` exists, used by the debug creature spawner); `SpecialEffectPosition` works |
| A cut scene with the short epic sting starts; the camera moves to a low view of the spot over 2 seconds | partial | `StartMusic` plays the sting; the scene never runs |
| The good advisor: "Aha! So that's what's been causing this smell!" | todo | needs the trail (in-hand test) and the bear |
| The bear walks about 25 further north, the camera following it down over 2 seconds; when it is there the camera drops lower | todo | needs the bear |
| The heal miracle chest falls from the sky onto the spot the bear appeared on (see below) | todo | `CreateRewardInTown` is a stub |
| The evil advisor: "The brown bear. So they do do it in the woods!" while the bear looks back at the spot, for at least 3 seconds | todo | needs the bear |
| The bear turns to the camera, both advisors go home, the dialogue closes and a second later the challenge record closes as a success (progress 1, alignment 0) | todo | `Snapshot` stub |
| The music stops and the scene ends; the bear is then handed to the shared creature-swap offer | todo | needs the bear; `SwapCreature` stub |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest drops from the sky at the bear's spot, given in the player's home Norse village (the first village on the land), without its own camera film | todo | `CreateRewardInTown` is a stub; see [../rewards.md](../rewards.md) |
| The chest is the reward table's first heal kind: it holds a heal miracle seed (undetermined whether it is the powered-up heal its name suggests; the table's power-up field is empty for both heal kinds) | todo | read from the reward table in `info.dat` with our `GRewardInfo` layout (`src/InfoConstants.h`); no reward chests |
| Once the chest is opened, the good advisor says "Great! We can heal people with this." if no dialogue is in the way; if this were the game's first reward, the first-reward explanation would play instead | todo | no reward chests; `GetHelp` is a stub |
| The real reward is the bear itself: a new species for the player's creature, if they want it | todo | see the next section |

## The swap offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears over the bear and it keeps turning to look at the camera; when the camera is near, the evil advisor says "Swap your Creature with this one if you want." at most once a minute | todo | no bear; the shared swap script; full detail in [creature_swaps.md](./creature_swaps.md) |
| Clicking it with the player's creature further than 50 away: "You'll need to bring our Creature, Boss." | todo | no bear |
| Otherwise the two creatures meet, the good advisor asks to confirm ("Are you sure you want a new Creature? ...") and clicking the bear confirms, clicking the player's own creature cancels | todo | no bear; `SwapCreature` stub |
| On confirmation the creature's mind moves into the brown bear in a short scene, and the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | todo | `SwapCreature` is a stub; our tree has no way to change a creature's species or move its mind into another body |
| The old body stays under a scroll and the offer never ends, so the player can swap back and forth | todo | `SwapCreature` stub |

## Aftermath, advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The farmer is gone for good (removed once home); the house stays | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The scroll over the house is never removed by the script (undetermined whether the game removes it when the quest's script ends) | todo | undetermined whether the game removes it when the quest's script ends |
| The reminder line, replayed by clicking the scroll, is spoken by whichever advisor owns it (the good advisor) | todo | the shared reminder script's natives work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Music: the "happy" script track for the introduction, the short epic sting for the bear | partial | music itself works (`StartMusic`); counted in the scene rows above |
| The player's creature is not involved until the swap; nothing checks the creature during the trail | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Bugs, quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pointing hint for the first pile ("We really should clean up this mess. Someone could step in it.") can never play: the step counter is set past it just before the trail loop starts | todo | dead branch, the same in the compiled program |
| The source's to-do notes say the trail was meant to lead "through a forest to reach the brown bear" and the camera was meant to "zoom to bear"; the shipped version does both only roughly as described above | n/a | development notes, nothing to do |
| The text table has no lines numbered 5, 8 to 11 or 16 for this quest, so lines were cut; the source's comments quote older wordings of three lines ("I don't think the owner of that house is gonna be too pleased.", "Urgh. The smell still persists...", "The smell has improved drastically...") | n/a | the shipped wordings are the ones above |
| The quest is the only one on the land tied to winning the Japanese village, and can be missed for good by taking the wonder villages first | todo | see "How it appears" |
| A saved game keeps where the trail is, the bear and the offer | todo | openblack has no saved games |
