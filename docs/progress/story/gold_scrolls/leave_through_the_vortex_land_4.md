# Leave Through the Vortex (Land 4)

The last gold scroll of the fourth land. Once all three Guardian Stones are broken and the Undead Village's totems give up
the second piece of the Creed, the creature takes it, the ground starts to rumble near a hidden spot, and Nemesis opens a
vortex to his own realm and dares the player through. Clicking the gold scroll over the vortex ends the land and goes on
to the fifth.

**Land:** 4 · **Giver:** Nemesis (the vortex west of the home village; the scroll hangs over it) · **Script:**
LeaveThroughVortexL4, after CreatureGetsCreed (in Land4Meteorites) · **Reward:** the creature's second Creed piece; the way
to the fifth land · **Repeatable:** no

How a vortex works (what can be sent through, what crosses to the next land, the rumbles' numbers) is in
[../portals.md](../portals.md); this file is the quest's flow. The land as a whole is in [../land_4.md](../land_4.md); the
step before it is [Undead Village](undead_village.md); what follows is the fifth land ([../land_5.md](../land_5.md)).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table (`Scripts/InfoScript2.txt`). openblack's state is judged on this tree: the story's top script always
runs the first land's control script first, the map-loading command (`LoadMap` in `src/CHLApi.cpp`) is empty and the
first land stalls long before its end, so none of this runs. Rows are partial only where every command they need works
in openblack.

**Progress: 0/36 done, 2 partial — 3%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script waits until the lightning, darkness and fire Guardian Stones are all broken ([The Defending Ogres](the_defending_ogres.md), [The Heartbroken Man](the_heartbroken_man.md), [The Totem Puzzle](the_totem_puzzle.md)) | todo | `LandControl4` itself calls no natives (it waits on script flags); never reached: Land 4 needs `LOAD_MAP`, an empty native. See [../../scripts/land4_script.md](../../scripts/land4_script.md) |
| It then runs the man's explanation of the Undead Village, then the creature's taking of the Creed, then this quest, each after the last has finished | todo | plain script order; the Creed scene needs `InCreatureHand` and `SetCreatureCreedProperties` (stubs); never reached: Land 4 needs `LOAD_MAP`, an empty native. See [undead_village.md](undead_village.md) |
| When this quest's script ends, the land's control script ends; the story's top script then stops every other script and loads the fifth land | todo | `StopAllScriptsExcluding` works; `LOAD_MAP` is empty, so no fifth land |

## The Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before the vortex, the creature collects the second Creed on the hill east of the home village (only the creature can take it), and the Creed scene ends with a rumble and the good advisor's "What was that?", which starts this quest | todo | `CreatureGetsCreed` needs `InCreatureHand` and `SetCreatureCreedProperties` (stubs); see [undead_village.md](undead_village.md#the-creed) |

## The rumbles (finding the vortex)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No scroll yet: the ground rumbles from a hidden spot west of the home village, with a screen-rumble sound and a camera shake at every rumble | todo | `LeaveThroughVortexL4` calls only real natives: `PlaySoundEffect`, `ShakeCamera` (`src/Camera/CameraShake`); never reached: Land 4 needs `LOAD_MAP`, an empty native. See [../portals.md](../portals.md) |
| The closer the camera is (within 500), the stronger (up to 3) and longer (up to 3 seconds) each shake; farther away the shake has no strength | todo | `ShakeCamera` and the camera distance are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Rumbles start 30 seconds apart; once the camera has been within 500 they come a second sooner each time, down to 3 seconds | todo | plain script timing; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The vortex opens when the camera comes within 100 of the spot, or after five more rumbles at 3 seconds apart | todo | real natives (`GetCameraPosition`, `GetDistance`); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The first time the camera is within 450, an advisor comments by the strength of that rumble: strong (1.5 to 2.5), the evil one pointing: "That was big. It came from over there."; middling (0.8 to 1.5), the good one pointing: "That was a mighty rumble. It came from here."; weak (0.3 to 0.8), the good one: "Did you feel that rumble, Leader? What could cause that?" then, pointing, "It seemed to come from over there." | todo | `SpiritEject`, `SpiritPointPos`, `RunText` are real; the same script, so the same gaps; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Later, when the rumbles are 25 or 20 seconds apart and strong enough (above 0.8): the evil advisor, pointing: "Ooh. I got another rumble then. Boss." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| At 15 or 10 seconds apart: the good advisor, pointing: "These rumblings seems to be getting faster, Leader." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| At 5 seconds apart: the evil advisor, pointing: "The rumbling's really fast now, Boss." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Nemesis opens the vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: if the five rumbles opened it, the screen fades to black and the camera is set looking down at the spot from 50 across and 30 up, then fades in, and a second passes; if the camera came close, it instead rises 10 and turns to the spot over 3 seconds | todo | `SetFade`, `SetCameraPosition`, `MoveCameraPosition` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| An entry vortex is made at the spot | todo | `CREATE` of a vortex is ported (`ecs::vortex::Create`); never reached: Land 4 needs `LOAD_MAP`, an empty native. See [../portals.md](../portals.md) |
| 7 seconds later the camera drifts in close over 20 seconds while Nemesis speaks: "Ah yes. My adversary." | todo | `MoveCameraPosition` and `RunText` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "You have the second part of the Creed. But it is useless unless you have three parts." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "But I am still more powerful than you can possibly imagine." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A slight shake (amplitude 0.03, radius 300, 10 seconds): "I have opened a Vortex to my realm." | todo | `ShakeCamera` and `RunText` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Come through. I invite you. For only you and I remain." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Come and face your destiny." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The camera pulls back over 6 seconds and the cut scene ends into a dialogue | todo | real camera and dialogue natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Evil advisor: "Yeah. Time to take on Nemesis. I've been waiting for this." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Good advisor: "If we go through, there's no coming back. This is where we stand or fall." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Evil: "Hey. You're right. Boss, are we ready for this? I mean we might die through there." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Good: "We might. If we go through, we must be well prepared for what we face." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Good: "And we should send plenty of food, wood and followers into the Vortex as well." | todo | advisors and texts are real; what crosses: [../portals.md](../portals.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Both go home | todo | `SpiritHome` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The scroll and leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears over the vortex, 20 off the ground | todo | `CreateHighlight` and the height through `SetProperty` (YPos) are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Until it is clicked, whenever the camera is within 100 and looking at it, at most every 30 seconds the evil advisor steps out, points at it and says "Boss, when you're ready, let's get ourselves to Nemesis' realm!" | todo | the shared notify script `ChallengeHighlightNotify` calls only real natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The player has as long as they like to throw food, wood and followers in before clicking | todo | the vortex takes things in (`ecs::vortex`), but nothing is carried to another land |
| Clicking the scroll starts the leaving cut scene: the camera flies up to 250 above the vortex over 6 seconds, then dives into it over 3 seconds while the screen fades to black over 3 | todo | real camera natives and `SetFade`; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades back in and the vortex counts as closed; the land is over | partial | `SetFadeIn` works; the next land's arrival is not (see [../portals.md](../portals.md)) |
| No challenge-log entry is made for leaving: unlike the land's other gold scrolls, this one is never recorded | todo | the same script runs, so no entry either; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature must fetch the Creed itself; if it never picks it up the land cannot end | todo | the Creed pick-up by the creature needs `InCreatureHand` (a stub) |
| Two leftover lines that would dim the creature's Creed glow and let it go at the end are commented out, so it keeps its full glow into the next land | partial | the same script runs; the Creed glow itself (`SetCreatureCreedProperties`) is a stub |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed; the only ways to stall it are the creature never taking the Creed, or the player never clicking the scroll | todo | the same script runs; never reached: Land 4 needs `LOAD_MAP`, an empty native |
