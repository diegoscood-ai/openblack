# Losing and game over

Every way a god can lose: in the story, the land is lost only when the player's temple is destroyed, which plays the
game-over sequence; in a skirmish or network game a god without a temple is out, and the last one standing wins.
Destroying a temple is slow on purpose: harm aimed at it goes to its god's towns first.

**Progress: 0/58 done, 9 partial — 8%**

## Can a story land be lost?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only way to lose a story land is the destruction of the player's temple. No land, challenge or global script checks for defeat, so losing every town, every believer or the creature does not end the game by itself | todo | the `GameOver` script is in the challenge file, but nothing in our tree starts it (see Unused for the test that does) |
| At the end of every game turn the game checks: a one-player story game (not a skirmish or a network game), the player has had a temple on this land, and its heart's destruction has begun. When all hold, the game-over script starts | todo | nothing in our tree watches for the player's temple heart being destroyed |
| It starts at once, with no grace period or timer, and only once: a flag records that the game is over | todo | no game-over check or flag in our tree |
| The flag is saved with the game and cleared when a new land is loaded | todo | no game-over flag, and openblack has no saved games; see ../engine/saving_and_loading.md |
| Only the local player's temple counts; a computer god's temple being destroyed never ends the game | todo | no game-over check in our tree |
| It cannot happen on the Gods' Playground: the player has no temple there | todo | no game-over check in our tree; the playground's land loads (`LandT.txt`, `--start-level`) without a temple; see [tutorial_island.md](tutorial_island.md) |
| The same script and check serve every land, from the first to the fifth: no land has its own losing condition, timer or text | todo | the shared `GameOver` script is loaded with the challenge file for every land, but nothing starts it |
| The player's creature is never lost for good: at no life it faints and is carried home | partial | the creature faints at no life (`physiology::ShouldFaint`, `CreaturePhysiologySystem`) and a knocked-out creature is carried to its temple's pen (`TempleCreatureHome`, `CreatureFightSystem`); the advisors' line about it is not started; see ../creature/physiology.md |
| While Lethys kidnaps the creature at the end of Land 2, it cannot be hurt, so it cannot faint and go home halfway; once it reaches the vortex it can be hurt again | todo | our `DevFunction` (`src/ECS/PlayerCreature.cpp`) does only values 2 and 3, not the switch that makes the creature unhurtable; the kidnapping is on Land 2, never reached; the kidnapping: [gold_scrolls/lethys_has_taken_our_creature.md](gold_scrolls/lethys_has_taken_our_creature.md) |
| Failing a gold or silver scroll never ends the game. Each failure has its own ending, and the story goes on | todo | the scrolls' endings are mostly never reached (Land 1 stops at the creature gate, Lands 2-5 need `LOAD_MAP`); see [challenges_and_rewards.md](challenges_and_rewards.md) |
| The first land's gate stones cannot be lost: one destroyed or taken out of the player's influence is made again where it started, and one dropped somewhere else is put back after a minute | todo | part of Choose Your Creature, never reached (the plinth reading `ObjectInfoBits` is a stub); see [land_1.md](land_1.md) |

## How a temple takes damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles, fire and other effects aimed at any part of a temple are passed on to its heart | todo | no damage passing to the heart in our tree |
| The heart passes the harm on to a building in one of its god's own towns: the first one found with more than a quarter of its life left, or else the first one found still standing with any life | todo | no damage passing in our tree |
| With no such building left, the harm goes to a homeless villager of one of those towns | todo | no damage passing in our tree |
| Only when its god has neither does the heart take the harm itself | todo | no damage passing in our tree |
| Each time harm is passed on, a spark beam runs from the temple to whatever takes it, with one of five temple spark sounds in turn | todo | nothing in our tree makes the spark beam or plays the temple spark sounds |
| An object thrown at the heart is passed on the same way; a hit on the heart itself does hit damage from the object's speed and weight, at most 0.2 a hit | todo | no hit damage on the temple heart found in our tree |
| A heart that takes harm itself, below full life, now and then gives off sparks, bigger the more it is hurt, and crackles once its life is at 60% or less | todo | no heart sparks or crackle in our tree |
| The heart's info table has a multiplier for passed-on damage | todo | undetermined: where the game reads it was not traced |

## How a temple is destroyed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the heart's life runs out, its destruction begins: every other part of the temple is removed | todo | the heart's own removal exists (`CitadelHeartToBeDeleted` in `src/ECS/Abodes.cpp`), but no life runs out to start it |
| A script can destroy a temple the same way ("delete ... with temple explode"); deleting a heart already being destroyed sets off an explosion at it | todo | `ObjectDelete` on a citadel heart logs "not implemented" in `src/CHLApi.cpp` |
| In the story, scripts blow up rival gods' temples: Khazar's when Nemesis kills him in Land 2, Lethys's as the player leaves Land 2 and again in Land 3 when his last town is won after the creed scene, and Nemesis's twice in the final fight | todo | never reached (Lands 2-5 need `LOAD_MAP`), and the heart delete is not implemented; see ../rival_gods/, [ending.md](ending.md); Khazar's: [gold_scrolls/nemesis_no.md](gold_scrolls/nemesis_no.md#the-film-khazars-temple); Lethys's on the third land: [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| In the story a destroyed player temple loses the land; in a skirmish or network game it puts that god out (see below) | todo | no game-over check in our tree |

## The game-over sequence

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any text drawn on the screen fades out over a second, and the game goes into cinema mode | todo | `FadeAllDrawText` is a stub in `src/CHLApi.cpp`; cinema mode itself works (`SetWidescreen`) |
| The failure music starts | partial | `StartMusic` plays the failure music (`Audio/Services/GameMusic`); nothing starts the game-over script |
| The time of day is moved to 23:00 over 10 seconds, so night falls | partial | `MoveGameTime` is real (`src/CHLApi.cpp`); nothing starts the script |
| The camera shakes a little at the temple, then starts 90 m from its entrance and 40 m up and zooms in over 12 seconds to 50 m away and 10 m up, looking at the temple | todo | the script camera natives and `ShakeCamera` are real, but the shot is placed from `GetTemplePosition` and `GetTempleEntrancePosition`, both stubs |
| A bigger shake, and Nemesis laughs: "Bwa ha ha ha ha!" (heard only) | partial | `ShakeCamera` and `GamePlaySaySoundEffect` are real; nothing starts the script |
| A cut closer to the temple; the evil advisor appears and says: "Ah. Okay, we're dead. We're all dead." | partial | `SpiritEject` and `RunText` are real (see [advisors.md](advisors.md)); nothing starts the script |
| Another cut; the good advisor appears, the ground shakes, and the camera cuts again, still shaking | partial | the advisor and shake natives are real; nothing starts the script |
| Once the line is read, the good advisor says: "I just hate goodbyes." | partial | `RunText` and `TextRead` are real; nothing starts the script |
| The camera cuts higher and higher above the temple every half second while the land shakes hard for 16 seconds | todo | the cuts are placed from the temple's position, `GetTemplePosition` (a stub) |
| The good advisor leaves, the temple explosion sounds, the screen starts fading to black over 12 seconds, and the evil advisor leaves | partial | `SpiritDisappear`, `PlaySoundEffect` and `SetFade` are real; nothing starts the script |
| "Game Over" fades in over 3 seconds in white, large, in the middle of the screen | todo | `GameDrawText` and `SetDrawTextColour` are stubs in `src/CHLApi.cpp` |
| The explosion sounds again and the camera rises 1000 m into the sky over 12 seconds | todo | `PlaySoundEffect` is real, but the camera's rise is placed from `GetTemplePosition` (a stub) |
| Game time stops; after a pause the text fades out over 5 seconds | partial | `GameTimeOnOff` is real; the text's fade (`FadeAllDrawText`) is a stub |
| The player is taken into the temple's Save Game room and the music stops | todo | our `DevFunction` does not take the player into the temple's Save Game room; see ../temple/save_game_room.md |

## After the game over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no restart or retry button: the player is left in the Save Game room to load a save | todo | no game over and no saved games; see ../temple/save_game_room.md |
| Once the game is over, the automatic save, the quick-save on exit and the low-memory save all stop, so the saves the player had are kept | todo | openblack has no saved games; see ../engine/saving_and_loading.md |
| A game saved after the game over keeps the flag: loading it never plays the sequence again, and those saves stay off | todo | openblack has no saved games |
| What the player can do on the land after leaving the Save Game room | todo | undetermined: game time has been stopped and nothing found starts it again |

## Skirmish and network games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's game over never runs | todo | openblack has no skirmish or network games; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Each turn, a god among players one to four with no temple is out of the game. Its creature is taken off the land (the player's own is copied first) | todo | openblack has no skirmish or network games |
| When another god is out and more than one is left, "<name> is out of the game." is shown | todo | openblack has no skirmish or network games |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" with only Leave Game | todo | openblack has no skirmish or network games; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game. The network game keeps running | todo | openblack has no skirmish or network games |
| When that box opens, the good advisor sometimes says "How shall I put this, Leader? You're, erm, not very good at this game. Sorry." (a 2% chance, at most once a game) | todo | openblack has no skirmish or network games |
| Watch Game closes the box and play goes on, so the player can watch the other gods | todo | openblack has no skirmish or network games |
| Leave Game closes the box and ends the game: a skirmish goes back to the skirmish box, a network game is left | todo | openblack has no skirmish or network games |
| When the player is out and only one god is left, the game ends, the network game stops, and the end box says "You have lost the game." with only Leave Game | todo | openblack has no skirmish or network games; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| A looker-on (players five to eight) sees the winner's name with "Won" | todo | openblack has no skirmish or network games |
| In a network game (patch 1.2), the game also ends when one god completes all the winning conditions or the time limit is reached; gods are ranked by their share of the conditions, ties broken by a second measure, and the winner gets the winning box while everyone else gets "You have lost the game." | todo | openblack has no network games; see [../multiplayer/multiplayer_rules.md](../multiplayer/multiplayer_rules.md); the time limit is skipped in a skirmish |
| A game never ends in a draw | todo | openblack has no skirmish or network games |
| There is no surrender. Leaving from the menu opens the same box asking "Are you sure you want to quit this game?" (or, in some internet games, "If you leave the game now, you'll forfeit your credits!") with Back to Game and Leave Game | todo | openblack has no skirmish or network games; see [../multiplayer/network_play.md](../multiplayer/network_play.md) |
| The end box has a "Game Over" tab and a "Statistics" tab | todo | openblack has no end box; see [../interface/statistics.md](../interface/statistics.md) |

## Computer gods losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's rival gods are beaten by their land's scripts, which check their towns and blow up their temples; they do not lose by the temple rule | todo | never reached (Lands 2-5 need `LOAD_MAP`), and the town checks and the heart delete they use are stubs; see ../rival_gods/ and [ending.md](ending.md) |
| A story rival's temple can still be worn down once it has no towns, but no story script reacts to that | todo | no temple wear in our tree; undetermined in play |
| In a skirmish, computer gods are out by the same rule as the player, stop thinking and lose their creature | todo | openblack has no skirmish; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |

## Unused

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A test script that just runs the game-over script | n/a | in the script sources but not in the shipped challenge file |
| The game-over script's own temple explosion, two more of Nemesis's laughs and a closing window are commented out | n/a | the temple is already being destroyed when the script starts |
| "You have lost the game. Click YES to watch the other players, NO to quit the Multiplayer Game." (twice) and the same for "the Skirmish Game" | n/a | nothing in the 1.2 program shows them; the end box uses "You have lost the game." with Watch Game and Leave Game buttons |
| "You have lost the game. The winner was %s.", "Game winner: %s" and "All the players lost. It's a draw." | n/a | nothing in the 1.2 program shows them |
| There is no game-over video | n/a | the game ships only the intro, logo, tips and falling spell videos |
