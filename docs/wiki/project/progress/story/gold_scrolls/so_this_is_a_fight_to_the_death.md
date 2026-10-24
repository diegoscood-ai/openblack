# So this is a fight to the death

The last gold scroll of the game: once Nemesis has lost every town of the fifth land he makes a mirror image of the
player's creature and the two creatures fight. Winning it leads straight into the end of the game: Nemesis's farewell
threat, the creature's walk into the volcano, the fall of Nemesis's temple, the creature's return and the closing
credits with the crowd of the story's characters. The ending as a whole is summarised in [ending.md](../ending.md); this
file is the quest step by step.

**Land:** 5 · **Giver:** Nemesis (the land's control script, once all his towns are taken) · **Script:** TheBigFightMain (TheBigFight), with EndOfGameCreatureSequenceGoToVolcano (FinalCreatureSequence), NemesisCitadelDestroyed (TheBigFight) and VillageWavingSequence (LandControl5) · **Reward:** the end of the game; no object or miracle · **Repeatable:** no (the fight can be retried until won)

Sources: the land's control script and the fight, volcano and credits scripts (the original source text, matching the
compiled `challenge.chl`), the game's text table (`Scripts/InfoScript2.txt`) and the game's data files. openblack is
judged on this tree: the story's top script always runs the first land's control script first and the map-loading
command (`LoadMap` in `src/CHLApi.cpp`) changes land now, but only through the earlier lands' exits, which no check has reached, so the fifth land's scripts never run; on top of that, 24 of
the 78 commands these scripts use only logged "not implemented" when they were counted (the computer players' hands,
the town reads and the temple's destruction are real since; the drawn text, the clipping window,
`CreatureLearnEverything` and `SetAnimationModify` still log it). Rows are pending unless the notes say otherwise. The curse
and the shielded village before this are in [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) and
[nemesis_shielded_village.md](nemesis_shielded_village.md).

**Progress: 0/77 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is the last step of the fifth land's control script: after the curse is lifted the script loops until the player owns Nemesis's own town, then stops every other script of the land and runs the fight, the volcano walk, the temple's fall and the credits one after another | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; `StopAllScriptsInFilesExcluding` itself works |
| It is logged in the story's log as a gold quest under the title "So this is a fight to the death" (a Nemesis line); the log entry is made at 0 when the mirror creature appears and set to 1 when it is beaten | pending | `Snapshot` and `UpdateSnapshot` are real |
| The log title is borrowed from a cut Nemesis speech ("My dream was to be the only god. … So this is a fight to the death") that is not in the shipped scripts, so the line is never heard in the game; the log's reminder is the same line | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll has no highlight: nothing has to be clicked, the fight begins by itself | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Nemesis's last stand (before the fight)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the curse is lifted, Nemesis's last village defends itself (burning meteors, a physical shield) and his blast barrages hit the player's villages and temple every 15 seconds; his last village can only be won while he holds no other village, with the advisors' retaken and "we can conquer" lines | pending | see [nemesis_shielded_village.md](nemesis_shielded_village.md#nemesiss-last-village); the town capture reads are real; not checked in play |
| Owning Nemesis's last town ends the last stand: every other script of the land is stopped and the fight begins | pending | the town capture reads are real; not checked in play |

## The mirror creature (introduction)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's hand is moved over the arena by his temple and his own creature is put in the arena facing it | pending | `SetComputerPlayerPosition` is real and Nemesis's creature is not yet made in play (`LoadCreature` is real); `SetPosition` is real |
| A cut scene begins with Nemesis's music | pending | `StartMusic` is real (`src/Audio/GameMusic`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera flies 16 seconds from near Nemesis's last town towards the arena | pending | `SetCameraPosition`, `MoveCameraPosition`, `MoveCameraFocus` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Both advisors come out; the evil advisor cheers: "Unbelievable! We did it! Oh yeah!" and "We have all his towns! He has no power! We've won!"; both go home | pending | `SpiritEject`, `SpiritHome`, `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The player's creature is put by the arena facing the camera; Nemesis speaks: "You are indeed a worthy opponent." and "But stop celebrating. I am not finished yet." | pending | `SetPosition` on the player's creature and `RunText` are real; so is turning it (`SetFocus` on a creature); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature plays its sad animation; a slow 20-second shot on it while Nemesis says "Call that a Creature?" and "It's not worthy of the name." | pending | creature animations by script are not done |
| A 20-second shot towards his temple: "THIS is a Creature. And I will make him the opposite image of yours."; his hand rises | pending | Nemesis's creature is not yet made in play; the camera and text are real |
| Glittering smoke and sparkle rings spiral around Nemesis's creature (two rings 15 out, turning 20 degrees a step and rising and falling up to 16) until the fight starts | pending | `SpecialEffectPosition` is real, but Nemesis's creature is not yet made in play |
| His creature plays its angry animation, is made invisible by a creature spell and plays its summoning animation | pending | Nemesis's creature is not yet made in play |
| A new creature of the same species as the player's is loaded with Nemesis's creature mind file (`Scripts/CreatureMind/NemesisCreature`) as Nemesis's, named "Mirror", fading in over 6 seconds and auto-scaled | pending | `LoadCreature` and `SetCreatureName` are real |
| Its alignment is the opposite of the player's creature, pushed to the extreme: fully evil if the player's creature is good, fully good otherwise (a creature of exactly neutral alignment gets a fully good mirror) | pending | the mirror is never made; `SetAlignment` itself is real |
| It is fully grown, the same size as the player's creature, at full strength and knows every action | pending | the mirror is never made and `CreatureLearnEverything` is a stub |
| After 6 seconds Nemesis's old creature fades away and is deleted; the mirror plays its summoning animation and the gold scroll is logged | pending | the mirror is never made; the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The player's creature walks to its mark and taunts; the camera rises over the arena in 10 seconds; the music stops and the cut scene ends | pending | walking and animating a creature by script are not done |

## The fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The fight starts only when the player's creature is within 300 of the arena; both creatures are then forced to fight each other | pending | `CreatureDoAction` forces the fight as a plan through the creature's mind (a script's fight has an executor, `k_ForcedOnly` in `src/Creature/CreaturePlanActions.cpp`); not checked in play; fighting itself: [fighting.md](../../creature/fighting.md) |
| The player fights it as any creature fight | pending | there is no opponent; see [fighting.md](../../creature/fighting.md) |
| Nemesis cheats: whenever the mirror's fight health drops below half, a level-two heal spell is cast on it and its fight health and health go back to full, then the script waits 10 seconds; at most four times per bout | pending | there is no opponent and fight health is not a handled property |
| When the fight ends the creature with the higher fight health wins | pending | `IsFighting` is real |
| Winning: the mirror dies for good and the scroll is set to 1 | pending | there is no opponent |
| Losing: the player's creature's health is set to nothing and it is put in the dead state; Nemesis's creature slowly heals between fights (1% every 6 seconds, up to 10 minutes from empty to full, stopping if it fights) | pending | creature health and states by script are not done |
| Losing cut scene: the camera turns to the mirror, which summons; Nemesis says "Your weakness is clear." and "You do not have what it takes!"; a one-second fade to black; the mirror is put back in the arena and the player's creature at the edge of the arena facing it; fade in; the evil advisor: "Boss. Come on. Give it another go!" | pending | `SetFade` and `SetFadeIn` are real, but the mirror is never made |
| After losing the fight can be retried at once: walking the creature back within 300 of the arena starts a new bout with four fresh heals | pending | there is no opponent |
| Losing the fight does not lose the game; the land can still be lost in the usual ways (see [../losing_and_game_over.md](../losing_and_game_over.md)) | pending | see [../losing_and_game_over.md](../losing_and_game_over.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| How the player's creature recovers from the dead state before the next bout is not said by the script | pending | undetermined, and creature states by script are not done |

## Nemesis's farewell (success)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music: his hand rises over the arena, the creature turns to the camera and plays its happy animation; 5 seconds later a high shot over the land | pending | `StartMusic` and Nemesis's hand (the computer player's hand natives) are real; the creature's animation is not checked |
| Nemesis: "It's not over yet!", "You have defeated me in this land but I have learned much from this struggle.", "I will rise again from the ashes of this defeat stronger than you can possibly imagine." and "There are many, many lands in this world and they will soon follow in the way of Nemesis." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera drifts 30 seconds towards the volcano; both advisors come out | pending | real camera and advisor natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Evil: "This is pointless! It could go on forever if we don't stop it now." Good: "Remember, we can use the power of the creeds to destroy Nemesis forever." and "Fusing the three creeds will confirm our superiority and leave no hiding place for him." Evil: "Three creeds? But we've only got two!" Good: "No the third is within OUR creature, you fool." Evil: "Of course, why didn't I think of that?" Good: "Hmm." and "You need to lead our creature into the volcano. The searing heat will do the rest." Evil: "But just a minute, doesn't that mean our creature might die?" and "Well, its up to you boss." Good: "Do you want to risk our noble creature to rule as you see fit?" Evil: "Or just let Nemesis spread his influence throughout the world again?" | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The mirror's body is moved far across the land; the advisors go home, the music stops and control returns | pending | the mirror is never made |
| The game waits until the player brings the creature within 50 of the causeway's start below the volcano; the leash is then let go and the mirror is deleted | pending | `DetachObjectLeash` and the distance wait are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| This is the "choice": there is no other way on; a player who never leads the creature there simply goes on playing the land | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Into the volcano

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music: the creature is put at the causeway's start facing the volcano's top; the camera pans behind it over 10 seconds | pending | `StartMusic` and `SetPosition` are real; turning the creature is not done; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It walks halfway along the causeway; the screen rumbles (sound and an 8-second camera shake of radius 800) and the creature looks up at the top | pending | `ShakeCamera` and `PlaySoundEffect` are real, but walking the creature by script is not done |
| It walks up to near the top, then to the top, the camera following in two moves (8 and 14 seconds) | pending | walking the creature by script is not done |
| At the top a fixed shot: the creature plays a special scripted animation (the creeds fusing), and after 3 seconds the game drops to half speed | pending | `SetAnimationModify` is a stub |
| The creed sound plays; the camera watches the creeds appear in two shots; another rumble with a 16-second shake | pending | `PlaySoundEffect` and `ShakeCamera` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The music stops; at 80% through the animation the creature's frozen moan is heard and the creature's end-sequence music starts | pending | `StartMusic` and `PlayedPercentage` (the creature's body share) are real; never reached in play |
| At 90% the game's sound effects are switched off and the falling film plays: the creature falling into the volcano | pending | the film plays through our Bink decoder (`SetAviSequence`, `src/Video`), reached at 90% of the clip (`PlayedPercentage`, real), but never reached in play; see [../../video/bink_videos.md](../../video/bink_videos.md) |
| After the film, sound and normal speed return and the creature is moved to the far side of the land, out of sight | pending | `SetGameSound` and `SetPosition` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the player has no creature at this point the whole walk is skipped | pending | `CallPlayerCreature` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Nemesis's temple falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music fades in on his temple; his hand flies to his temple at speed 50 while the camera follows in 20- and 12-second moves | pending | music, fade and Nemesis's hand (`MoveComputerPlayerPosition`) are real; not checked in play |
| A shot from high above the temple; 3 seconds later rings of beam explosions circle it (four points 15 out, turning 10 degrees a step) | pending | `SpecialEffectPosition` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 6 seconds later the temple explodes; a 20-second sweep over the ruins; 20 seconds after the first, a second temple explosion, and Nemesis's creature is deleted | pending | `ObjectDelete` of a citadel heart starts the temple's destruction (`Locator::templeDestructionSystem`); not checked in play |
| Both advisors: Good "Our noble friend, gone! But can you feel the difference?" Evil "I can't feel even a trace of Nemesis' influence." Good "Everything is clear. You're the one true God, now." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The creature reappears, burnt, lying near the Greek town: it is put in the dying state at a tenth of its health, with four smoke clouds (scale 7, for 25, 15, 10 and 30 seconds) | pending | creature health and states by script are not done; `SpecialEffectPosition` is real |
| The screen rumbles (sound and a 2-second shake); Good: "Crikey! What was that?"; a 10-second shot on the creature; Evil: "Hang on, I recognise that smell." Good: "He's back! He doesn't look too hot though." Evil: "Maybe we can fix him up?" | pending | `PlaySoundEffect`, `ShakeCamera` and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The advisors leave, the music stops and the player gets control back: the game waits until the creature's health is above 80% | pending | the creature's health is not a handled property |
| Once healed, the glows of the creeds on both of the creature's hands are taken away | pending | `SetCreatureCreedProperties` is real; see ../../creature/creature_tattoos.md |
| 5 seconds later a last cut scene with the epic music: the creature looks at the camera, summons, the camera circles it for 20 seconds, it plays its happy animation, then kneels to pray while the camera rises and everything fades to black over 10 seconds | pending | `StartMusic` is real; the creature's animations by script are not done |
| Nemesis's hand is switched off for good | pending | `EnableDisableComputerPlayer` is real; not checked in play |

## The credits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The credits run as one cut scene with the sound effects switched off and the outro music | pending | `StartMusic` with the outro bank and `SetGameSound` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| They are 17 windows, alternately on the left and right of the screen (across 0.1 or 0.5, 0.2 down, 0.4 wide, 0.6 high), each opening over half a second and fading in over 2 seconds on a little scene somewhere on the land | pending | `SetClippingWindow` and `ClearClippingWindow` are stubs |
| In each window the job title is written from the text table and the names are written as plain text (white, fading in over 2 seconds), held 10 seconds, faded out over 2; the window then fades to black and closes | pending | `GameDrawText`, `FadeAllDrawText` and `SetDrawTextColour` are stubs |
| Testing for Lionhead runs over five pages in one window and Testing for EA over three; between pages the text fades out and the next page fades in | pending | the drawn text natives are stubs |
| Each window has its own time of day (noon, 4 pm, or 11 pm for the ark) with the clock stopped | pending | `SetGameTime` and `GameTimeOnOff` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scenes are the story's people: three crusaders sitting in council ("Black & White", "Designed and Created by Lionhead Studios Ltd."; the Magic Dragon's crusaders, see [../silver_scrolls/the_magic_dragon.md](../silver_scrolls/the_magic_dragon.md)), an impressed Greek farmer (Game Programming), a nomad dancing with an Aztec housewife (3D Programming), a crusader looking for something (Art), an Aztec housewife at the volcano (Animation, Script & Dialogue, Sound & Music), an ogre (Internet, Library & Tools; Scenario & Level Design), a Shaolin monk sitting (Gameplay & Testing), a fisherman (Lionhead Studios Ltd., PR), two Africans talking (Lead Instrumentalist, Musicians), an engineer on the volcano (Voice Characterisation, Additional Art), a priest and priestess praying (Testing for Lionhead), a Greek woman mourning among shepherds (Production for EA), the creature trainer swinging his legs on a pier (Testing for EA), the ark at night (Marketing and Legal for EA), a sculptor at work (Localisation for EA), a hippy chopping at a giant magic mushroom (For Electronic Arts) and the Pied Piper dancing (Special Thanks To) | pending | `CREATE` makes villagers, features and animals now (`CreateScriptObject`); the creature in a scene is not made; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Each scene's people are made with high graphics detail and deleted when its window closes | pending | `SetHighGraphicsDetail` (`ecs::super_villager`) and `ObjectDelete` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The last scene: the story's characters gather and applaud, 30 of them — the monk, the idol builder, the hermit, the hippy, the priest and priestess, the marauder, the engineer, a footballer, the nomad, the Aztec leader, the creature trainer, the sailor, the breeder, the healer, the sculptor, the crusader, a Norse mother and father, and ten villagers of the land's tribes | pending | `CREATE` makes villagers and their animations are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fireworks go off at four spots for five minutes; sound effects come back; the view fades in | pending | `SpecialEffectPosition` and `SetGameSound` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Each person starts clapping or cheering a tenth of a second after the last; the camera moves through the crowd for 30 seconds | pending | villager animations and the camera are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After 15 seconds a small boy (age 8, three-quarters size) skips in to his mother; they hug, then both cheer | pending | `CREATE` of a child, the age and scale for villagers are handled; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 10 seconds later the scene holds until the player presses the space bar | pending | `KeyDown` is real (the space bar held); never reached in play: the story cannot leave Land 2 |
| Everyone is released, the music stops and the cut scene ends; the land's control script then sleeps for 50,000 seconds, so the player is left in the fifth land with no more story | pending | `ReleaseFromScript` and `StopMusic` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors only speak in the cut scenes listed above; there is no reminder nagging during the fight (the log's reminder line is the title line) | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Music: Nemesis's piece for the mirror's making, the farewell, the volcano walk and the temple's fall; the creature's end-sequence piece at the top of the volcano; the second epic for the creature's return; the outro for the credits | pending | all named in `src/Audio/GameMusic` and `StartMusic` / `StopMusic` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Sounds: two screen rumbles on the volcano and one when the creature returns, the creed sound at the top, the creature's frozen moan | pending | `PlaySoundEffect` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The "retaken" warning is given the first time any other town is Nemesis's after the curse lifts, even if he never took it back; the "we can conquer" line is only heard after such a warning | pending | the town owner reads are real; not checked in play |
| The Pied Piper of the closing crowd is never made: his spot, focus and clapping are set but no villager is created for him, so he does not appear (he does appear in the last credit window) | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The hippy in the crowd is never given high graphics detail, unlike everyone else | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The winner is decided by comparing fight health once the fight ends, however it ended | pending | there is no opponent in play yet; fight health (`GET_PROPERTY` CreatureFightHealth) is read for a creature |
| A source comment asks for the invisibility spell on Nemesis's old creature to be put back once fixed; in the shipped scripts it is cast | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

