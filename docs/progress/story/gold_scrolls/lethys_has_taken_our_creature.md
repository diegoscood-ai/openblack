# Lethys has taken our Creature!

The kidnapping that ends Land 2's story: after Khazar's death, once Lethys is down to his last town (or the player
holds two of his three), Lethys opens a vortex by his temple, mesmerises the player's creature with a beam and walks it
into the vortex; the leash doesn't work. The advisors argue, and for a few seconds a scroll over the vortex lets the
player follow at once; miss it and the vortex closes, leaving the player to destroy Lethys's temple to open it again.
It records a gold (quest) entry in the challenge log, filed under the good advisor's line "Lethys has taken our
Creature! What are we going to do?". The land as a whole is in [../land_2.md](../land_2.md), Lethys in
[../../rival_gods/lethys.md](../../rival_gods/lethys.md#losing-the-second-land), the vortex itself in
[../portals.md](../portals.md#opening-the-exit-per-land).

**Land:** 2 · **Giver:** none (a story event at Lethys's temple; Lethys speaks) · **Script:** LethysVortex · **Reward:** none (following through the vortex takes the player to Land 3 straight away) · **Repeatable:** no

Sources: the quest's script source (`LethysVortex.txt`) and the land's control script (`LandControl2.txt`), checked
against the compiled form in the shipped `challenge.chl` (which also shows the log entry is filed under its own
challenge number); the game's English text table; and the executable (read-only) for stopping scripts by name and for
the challenge log. openblack's state is judged on this tree: of the 52 script functions this film needs, 6 only logged
"not implemented" in `src/CHLApi.cpp` when they were counted (the computer player's hand, the beam effect, the
creature-in-temple switch and the challenge log); the challenge log, the creature-in-temple switch and the hand's moves
are real now, the beam (`AddSpotVisualTargetObject`) still logs it, and openblack never runs Land 2's control script at all (see [../land_2.md](../land_2.md) row
1), so the kidnapping never happens; every row below is pending unless the notes say otherwise.

**Progress: 1/52 done, 0 chosen, 0 partial, 0 inferred — 2%**

## Is it a gold scroll?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Yes, as a story step: it records a quest (gold) entry in the challenge log, like Khazar's death, but has no gold scroll of its own | pending | `Snapshot` is real |
| The scroll it puts over the vortex is a silver (challenge) highlight, used only as a "follow now" button: no advisor nags about it and it gives nothing | pending | `CreateHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Its log entry is filed under its own challenge number, kept apart from the land's exit through the temple vortex (which shares the script file's challenge) | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |

## When it happens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds, but only after Khazar has died ([nemesis_no.md](nemesis_no.md)), so never on the same check as his death | pending | Khazar's death needs computer-player natives (real now; not checked in play); see [nemesis_no.md](nemesis_no.md) |
| It starts when the player (or anyone) holds at least two of Lethys's three starting towns (the near Celtic town, the far one and his home town), or Lethys has one town or none in all | pending | the town owners (`GET_PROPERTY` Player of a town) and totals (`GetPlayerTownTotal`) are real; not checked in play |
| It happens once; the control script waits for it to finish | pending | plain script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If Lethys has no towns left and his temple is nearly destroyed, it still comes first, and the exit through the temple ([leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md)) is checked straight after it on the same loop | pending | the town and temple reads are real natives; not checked in play |

## The kidnapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's taunt script and the rival's confront-the-player script are stopped by name, and the land is told Lethys is gone | pending | `StopScript` in `src/CHLApi.cpp` stops only a script whose name is exactly the given string; the game splits the string, so neither script would stop here |
| A film starts and Nemesis's music plays; the camera's position and aim are remembered | pending | `StartMusic` and the camera reads are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lethys's computer player is paused, and his hand flies (speed 200) to above his temple | pending | pausing and moving a computer player are real (`Locator::computerPlayerSystem`); not checked in play |
| The camera is put behind his temple looking out over it; the time of day is set to 20:00 and stopped | pending | setting the camera, `SetGameTime` and `GameTimeOnOff` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Lethys's hand slowly sinks to 20 above the vortex spot in front of his temple; Lethys: "You are truly a powerful god but you failed to defeat me." | pending | Lethys's hand is real now (a computer player's hand natives); not checked in play; `RunText` is real |
| The camera cuts to a side view; the vortex opens there | pending | `CREATE` of a vortex is ported; see [../portals.md](../portals.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The player's creature is moved to a spot below the temple, facing the vortex, wherever it was | pending | `SetPosition` and turning a creature with `SetFocus` are real; not checked in play |
| The camera swings up over 11 seconds (its aim over 7); after 2 seconds Lethys: "Let's see how powerful you are without your Creature." | pending | real camera and text natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A magic beam runs from Lethys's hand to the creature for 60 seconds | pending | `SpecialEffectObject` is real; the beam `AddSpotVisualTargetObject` is a stub |
| The creature starts walking to the vortex; its leash is switched off | pending | `SetLeashWorks` is real; walking a creature by script (`MoveGameThing`) is not done |
| The creature is kept from appearing in the creature cave in the temple while it is taken | done | `SetCreatureInTemple` keeps the creature room from making its copy of the creature from the next visit to the temple (raffclar #349, unverified against v1.42) |
| The camera cuts to look at the creature from behind and follows down over 6 seconds | pending | real camera natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The challenge log records a quest entry titled "Lethys has taken our Creature! What are we going to do?", success 1, alignment 0, its picture the view of the walking creature, no reminder line | pending | `Snapshot` is real |
| The evil advisor appears: "Look! He's heading for the Vortex! He's mesmerised!"; the good advisor appears: "The Creature! Get him on a Leash quickly!"; both disappear | pending | `SpiritAppear`, `SpiritDisappear` and `RunText` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The creature is made unable to die, so being hurt can't send it home to the temple halfway | pending | only `DevFunction` values 2 and 3 are handled |
| The film ends, handing control back while the creature walks | pending | the camera release is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## What the player can do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has control while the creature walks to the vortex; nothing they do stops it | pending | the creature's walk by script is not done |
| If the player puts the creature on a leash, the good advisor says once: "The Leash isn't working." | pending | `IsLeashed`, the leash switch and the dialogue are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The walk ends when the creature is within 5 of the vortex, or no longer exists | pending | the creature's walk by script is not done |
| The creature can die again from then on | pending | only `DevFunction` values 2 and 3 are handled |

## Following

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 1.5 seconds Lethys's hand sinks into the vortex (to the ground, speed 20); after 2 more a second film starts | pending | Lethys's hand is real now (a computer player's hand natives); not checked in play |
| The camera looks at the vortex from the south-west and swings round over 36 seconds (its aim, 20 above the vortex, over 20) | pending | real camera natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor appears: "NO! He's gone through! And so has Lethys!"; the good advisor appears: "Lethys has taken our Creature! What are we going to do?"; evil: "We're going to get him back. Let's go through!"; good: "Yes! No! Let's think about this. It could be a trap."; the music stops | pending | advisors and texts are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The film ends but the dialogue goes on, and the silver scroll appears 20 above the vortex | pending | `CreateHighlight` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil: "No! There isn't time to think! The Vortex is closing!"; good: "But we don't know what's on the other side!"; evil: "Our Creature is on the other side! That's all we need to know. Let's go!"; both disappear | pending | real text natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The player has 10 seconds after these lines (plus however long the three lines take) to click the scroll or the vortex | pending | `GameThingClicked` and the timer are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Clicking in time: the land is marked as being left; the camera flies to the start of the exit camera path over 4 seconds, runs the path, and after 2 seconds fades to black over 2 seconds | pending | `RunCameraPath` and `SetFade` are real; see [../portals.md](../portals.md#going-through); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Game time starts again, the land's control loop ends and the story moves on to Land 3 | pending | `LOAD_MAP` works, but play has not yet got here: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Missing the chance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the player hasn't clicked within the 10 seconds, the chance is marked missed: the scroll stops waiting, the vortex fades out and game time starts again | pending | `VortexFadeOut` and `GameTimeOnOff` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor appears: "All we gotta do is destroy his Temple to eradicate all belief in Lethys!", goes home and disappears | pending | advisors and texts are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scroll is not deleted by the script | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| From then on the only way out is to take every one of Lethys's towns and wreck his temple to a tenth of its health ([leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md)) | pending | the town and temple reads are real natives; not checked in play; see [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-takes-damage) |
| Lethys's computer player stays paused for the rest of the land: no land 2 script resumes it | pending | Lethys's computer player does not exist in our tree |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has no creature for the rest of Land 2 and arrives in Land 3 without it, whichever way they leave | pending | no land loading; Land 3: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| Lethys's taunts end (his taunt script stops when it sees he is gone) | pending | Lethys's taunts need his computer player; see ../../rival_gods/lethys.md |
| Once both gods are gone, the land's town-ownership watcher, which made them comment on towns changing hands, stops | pending | the watcher's town-owner reads are real; not checked in play |
| It can't fail; missing the click only means the longer way out. It can't soft-lock unless Lethys's temple can't be brought down | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors appear and disappear in place rather than stepping out of their corners | pending | `SpiritAppear` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays from the start of the first film until the end of the second | pending | `StartMusic` and `StopMusic` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is moved, made to walk into the vortex, can't be led, can't die while walking, and can't appear in the temple | pending | walking, turning and the can't-die switch are not done for creatures |
| What the vortex does with the creature when it arrives is not done by this script | pending | the vortex's take is ported (`ecs::vortex`); see [../portals.md](../portals.md#being-sucked-in); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script stops "LethysTauntAI, ComputerPlayerConfrontPlayer" as one string; the game splits it, so both stop, but the script that actually voices Lethys's taunts (run by the taunt script, and in the background by the town-ownership reactions) is a third one, which is not named | pending | our `StopScript` does not split the names |
| The vortex here is 12 away from the one the temple exit opens, though the source comment says to keep them the same; both use the same recorded camera path | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The creature's walk is checked in a loop with no pause | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The window to follow is not a fixed 10 seconds: it also covers the three closing lines, however long the player takes to read them | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

