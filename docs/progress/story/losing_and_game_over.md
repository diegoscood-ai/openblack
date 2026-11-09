# Losing and game over

Every way a god can lose: in the story, the land is lost only when the player's temple is destroyed, which plays the
game-over sequence; in a skirmish or network game a god without a temple is out, and the last one standing wins.
Destroying a temple is slow on purpose: harm aimed at it goes to its god's towns first.

**Progress: 6/60 done, 0 chosen, 21 partial, 0 inferred — 31%**

## Can a story land be lost?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only way to lose a story land is the destruction of the player's temple. No land, challenge or global script checks for defeat, so losing every town, every believer or the creature does not end the game by itself | partial 60% | `TempleDestructionSystem` starts `GameOver` once the local player's temple heart is being destroyed; not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| At the end of every game turn the game checks: a one-player story game (not a skirmish or a network game), the player has had a temple on this land, and its heart's destruction has begun. When all hold, the game-over script starts | partial 60% | `TempleDestructionSystem::EndTurn` checks it at the end of each turn (`GameOverStarts`: not a skirmish, the local temple's heart being destroyed); not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| It starts at once, with no grace period or timer, and only once: a flag records that the game is over | partial 60% | the flag is the registry context's `gameOver`, set as the script starts; not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The flag is saved with the game and cleared when a new land is loaded | pending | the flag is the registry context's `gameOver`; whether a new land clears it is not checked, and openblack has no saved games; see ../engine/saving_and_loading.md |
| Only the local player's temple counts; a computer god's temple being destroyed never ends the game | done | only the local player's heart is read (`TempleDestructionSystem::EndTurn`); re-measured 2026-10-21: only the local player's heart is read, as the original |
| It cannot happen on the Gods' Playground: the player has no temple there | partial 90% | a playground game never starts it (`GameOverStarts`); the playground's land loads (`LandT.txt`, `--start-level`) without a temple; see [tutorial_island.md](tutorial_island.md); re-measured 2026-10-21: a playground never starts it; not checked in a run |
| The same script and check serve every land, from the first to the fifth: no land has its own losing condition, timer or text | pending | the shared `GameOver` script is loaded with the challenge file for every land, and `TempleDestructionSystem` starts it on any land |
| The player's creature is never lost for good: at no life it faints and is carried home | partial 80% | the creature faints at no life (`physiology::ShouldFaint`, `CreaturePhysiologySystem`) and a knocked-out creature is carried to its temple's pen (`TempleCreatureHome`, `CreatureFightSystem`); the advisors' line about it is not started; see ../creature/physiology.md; re-measured 2026-10-21: it faints and is carried home; the advisors' line is not started |
| While Lethys kidnaps the creature at the end of Land 2, it cannot be hurt, so it cannot faint and go home halfway; once it reaches the vortex it can be hurt again | pending | our `DevFunction` (`src/ECS/PlayerCreature.cpp`) does values 1, 2, 3, 6 and 7, not the switch that makes the creature unhurtable; the kidnapping is on Land 2, never reached; the kidnapping: [gold_scrolls/lethys_has_taken_our_creature.md](gold_scrolls/lethys_has_taken_our_creature.md) |
| Failing a gold or silver scroll never ends the game. Each failure has its own ending, and the story goes on | pending | the scrolls' endings are mostly not yet reached (Land 1 has not yet been run past the creature glade without skipping the training since `CREATE` of a creature became real, and the story cannot leave Land 2); see [challenges_and_rewards.md](challenges_and_rewards.md) |
| The first land's gate stones cannot be lost: one destroyed or taken out of the player's influence is made again where it started, and one dropped somewhere else is put back after a minute | pending | part of Choose Your Creature, not checked in play; the held read (property 9, `GetProperty` IN_HAND) is real, so the guards can tell a stone in the hand from a dropped one; see [land_1.md](land_1.md) |

## How a temple takes damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles, fire and other effects aimed at any part of a temple are passed on to its heart | partial 60% | a thrown thing striking the heart passes its blow on (`src/Physics/TempleHeart.h`); miracles, fire and other harm are not checked; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The heart passes the harm on to a building in one of its god's own towns: the first one found with more than a quarter of its life left, or else the first one found still standing with any life | partial 50% | for a thrown blow (`physics::temple_heart`, the first building with more than a quarter of its life); re-measured 2026-10-21: for a thrown blow only; miracles and fire are not passed on yet |
| With no such building left, the harm goes to a homeless villager of one of those towns | partial 50% | for a thrown blow (`physics::temple_heart`); re-measured 2026-10-21: for a thrown blow only; miracles and fire are not passed on yet |
| Only when its god has neither does the heart take the harm itself | partial 50% | for a thrown blow (`physics::temple_heart`); re-measured 2026-10-21: for a thrown blow only; miracles and fire are not passed on yet |
| Each time harm is passed on, a spark beam runs from the temple to whatever takes it, with one of five temple spark sounds in turn | done | `TempleDestructionSystem::BeamAtTarget`: a plasma beam from the heart's top to what takes the blow, at most every two seconds, with the next of five spark sounds (`PlaySpark`, its own counter) |
| An object thrown at the heart is passed on the same way; a hit on the heart itself does hit damage from the object's speed and weight, at most 0.2 a hit | done | `StrikeHeart` (`ECS/PhysicsGameHooks.cpp`) passes a thrown blow on to what `TempleDestructionSystem::ActualObjectToEffect` chooses (`physics::temple_heart::Choose`); with nothing to pass it to, `Harm` (speed x mass x 5e-6, at most 0.2) |
| A heart that takes harm itself, below full life, now and then gives off sparks, bigger the more it is hurt, and crackles once its life is at 60% or less | done | `TempleDestructionSystem`: a hurt heart beams across itself every (0.4 + 2.6 x life) s, its look by `DamageLookAt`, with a spark sound in turn from 0.6 of its life down (`DamageBeamSounds`, its own counter) |
| The heart's info table has a multiplier for passed-on damage | pending | undetermined: where the game reads it was not traced |

## How a temple is destroyed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the heart's life runs out, its destruction begins: every other part of the temple is removed | pending | the heart's own removal exists (`CitadelHeartToBeDeleted` in `src/ECS/Abodes.cpp`), and a heart whose life runs out starts the destruction (`TempleDestructionSystem`) |
| As its temple is destroyed the heart fades out from fourteen seconds, drawn see-through by then, and from three and a half seconds it glows ever more in its god's colour, with the local player's heartbeat (another god's at the faintest) | done | `heart_fade` (`src/Temple/HeartFade.h`): `GameTempleDestructionWorld::FadeHeart` (the alpha, the see-through materials and `components::Alpha`), the specular in `RenderingSystem`; from 21.8 s the heart is neither drawn nor found under the cursor (`temple_destruction::HeartDrawn`); every heart is drawn in its land light times white (`heart_fade::Tint`); tests `HeartFade.*` ([the heart's draw](../../../magic/temple-and-citadel.md#the-hearts-draw)) |
| As the heart fades, a glowing shell in its god's colour swells over it from fourteen seconds until the smoke, reaching out with the local player's heartbeat | done | `heart_shell` (`src/Temple/HeartShell.h`), `Renderer::DrawHeartShell` in the hearts' own pass before the models, newest first, each heart's shell right before its model (`heart_shell::Pass`), for every heart whatever the object draw list, the screen or its fade, and on past 21.8 s once the heart itself is no longer drawn (`heart_shell::PlacementOf`), with the grey the original adds at the inner edge; tests `HeartShell.*` ([the shell's draw](../../../magic/temple-and-citadel.md#the-shells-draw)) |
| A script can destroy a temple the same way ("delete ... with temple explode"); deleting a heart already being destroyed sets off an explosion at it | pending | `ObjectDelete` of a citadel heart with the temple-explode mode is real (`DeleteByScript`) |
| In the story, scripts blow up rival gods' temples: Khazar's when Nemesis kills him in Land 2, Lethys's as the player leaves Land 2 and again in Land 3 when his last town is won after the creed scene, and Nemesis's twice in the final fight | pending | never reached (the story cannot leave Land 2); deleting a heart from a script starts its destruction (`ObjectDelete`, `TempleDestructionSystem::DeleteByScript`); see ../rival_gods/, [ending.md](ending.md); Khazar's: [gold_scrolls/nemesis_no.md](gold_scrolls/nemesis_no.md#the-film-khazars-temple); Lethys's on the third land: [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| In the story a destroyed player temple loses the land; in a skirmish or network game it puts that god out (see below) | partial 50% | the story's game over starts (`TempleDestructionSystem`); a skirmish or network god's defeat is not done; re-measured 2026-10-21: the story's game over starts; a skirmish or network god's defeat is not done |

## The game-over sequence

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any text drawn on the screen fades out over a second, and the game goes into cinema mode | pending | `FadeAllDrawText` is a stub in `src/CHLApi.cpp`; cinema mode itself works (`SetWidescreen`) |
| The failure music starts | partial 80% | `StartMusic` plays the failure music (`Audio/Services/GameMusic`); `TempleDestructionSystem` starts the script; re-measured 2026-10-21: the music plays when the script starts; not checked in a story game |
| The time of day is moved to 23:00 over 10 seconds, so night falls | partial 60% | `MoveGameTime` is real (`src/CHLApi.cpp`); `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The camera shakes a little at the temple, then starts 90 m from its entrance and 40 m up and zooms in over 12 seconds to 50 m away and 10 m up, looking at the temple | partial 60% | the script camera natives and `ShakeCamera` are real, and so are `GetTemplePosition` and `GetTempleEntrancePosition` (the temple's place, and the point the radius from it toward its pen point, the height up), which place the shot; `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| A bigger shake, and Nemesis laughs: "Bwa ha ha ha ha!" (heard only) | partial 60% | `ShakeCamera` and `GamePlaySaySoundEffect` are real; `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| A cut closer to the temple; the evil advisor appears and says: "Ah. Okay, we're dead. We're all dead." | partial 60% | `SpiritEject` and `RunText` are real (see [advisors.md](advisors.md)); `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Another cut; the good advisor appears, the ground shakes, and the camera cuts again, still shaking | partial 60% | the advisor and shake natives are real; `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Once the line is read, the good advisor says: "I just hate goodbyes." | partial 60% | `RunText` and `TextRead` are real; `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The camera cuts higher and higher above the temple every half second while the land shakes hard for 16 seconds | partial 60% | the cuts are placed from the temple's position, `GetTemplePosition` (real); `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The good advisor leaves, the temple explosion sounds, the screen starts fading to black over 12 seconds, and the evil advisor leaves | partial 60% | `SpiritDisappear`, `PlaySoundEffect` and `SetFade` are real; `TempleDestructionSystem` starts the script, not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| "Game Over" fades in over 3 seconds in white, large, in the middle of the screen | pending | `GameDrawText` and `SetDrawTextColour` are stubs in `src/CHLApi.cpp` |
| The explosion sounds again and the camera rises 1000 m into the sky over 12 seconds | partial 60% | `PlaySoundEffect` is real, and the camera's rise is placed from `GetTemplePosition` (real); not checked in a story game; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Game time stops; after a pause the text fades out over 5 seconds | partial 40% | `GameTimeOnOff` is real; the text's fade (`FadeAllDrawText`) is a stub; re-measured 2026-10-21: part of what it calls is a stub (see the notes) |
| The player is taken into the temple's Save Game room and the music stops | pending | our `DevFunction` does not take the player into the temple's Save Game room; see ../temple/save_game_room.md |

## After the game over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no restart or retry button: the player is left in the Save Game room to load a save | pending | no game over and no saved games; see ../temple/save_game_room.md |
| Once the game is over, the automatic save, the quick-save on exit and the low-memory save all stop, so the saves the player had are kept | pending | openblack has no saved games; see ../engine/saving_and_loading.md |
| A game saved after the game over keeps the flag: loading it never plays the sequence again, and those saves stay off | pending | openblack has no saved games |
| What the player can do on the land after leaving the Save Game room | pending | undetermined: game time has been stopped and nothing found starts it again |

## Skirmish and network games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's game over never runs | pending | openblack has no skirmish or network games; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Each turn, a god among players one to four with no temple is out of the game. Its creature is taken off the land (the player's own is copied first) | pending | openblack has no skirmish or network games |
| When another god is out and more than one is left, "<name> is out of the game." is shown | pending | openblack has no skirmish or network games |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" with only Leave Game | pending | openblack has no skirmish or network games; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game. The network game keeps running | pending | openblack has no skirmish or network games |
| When that box opens, the good advisor sometimes says "How shall I put this, Leader? You're, erm, not very good at this game. Sorry." (a 2% chance, at most once a game) | pending | openblack has no skirmish or network games |
| Watch Game closes the box and play goes on, so the player can watch the other gods | pending | openblack has no skirmish or network games |
| Leave Game closes the box and ends the game: a skirmish goes back to the skirmish box, a network game is left | pending | openblack has no skirmish or network games |
| When the player is out and only one god is left, the game ends, the network game stops, and the end box says "You have lost the game." with only Leave Game | pending | openblack has no skirmish or network games; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| A looker-on (players five to eight) sees the winner's name with "Won" | pending | openblack has no skirmish or network games |
| In a network game (patch 1.2), the game also ends when one god completes all the winning conditions or the time limit is reached; gods are ranked by their share of the conditions, ties broken by a second measure, and the winner gets the winning box while everyone else gets "You have lost the game." | pending | openblack has no network games; see [../multiplayer/multiplayer_rules.md](../multiplayer/multiplayer_rules.md); the time limit is skipped in a skirmish |
| A game never ends in a draw | pending | openblack has no skirmish or network games |
| There is no surrender. Leaving from the menu opens the same box asking "Are you sure you want to quit this game?" (or, in some internet games, "If you leave the game now, you'll forfeit your credits!") with Back to Game and Leave Game | pending | openblack has no skirmish or network games; see [../multiplayer/network_play.md](../multiplayer/network_play.md) |
| The end box has a "Game Over" tab and a "Statistics" tab | pending | openblack has no end box; see [../interface/statistics.md](../interface/statistics.md) |

## Computer gods losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's rival gods are beaten by their land's scripts, which check their towns and blow up their temples; they do not lose by the temple rule | pending | never reached (the story cannot leave Land 2); the town checks they use (`GetTownWithId`, `GetPlayerTownTotal`) and the heart delete are real; see ../rival_gods/ and [ending.md](ending.md) |
| A story rival's temple can still be worn down once it has no towns, but no story script reacts to that | pending | no temple wear in our tree; undetermined in play |
| In a skirmish, computer gods are out by the same rule as the player, stop thinking and lose their creature | pending | openblack has no skirmish; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |

