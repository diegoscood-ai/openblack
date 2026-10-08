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
command (`LoadMap` in `src/CHLApi.cpp`) has an empty body, so the fifth land's scripts never run; on top of that, 24 of
the 78 commands these scripts use only log "not implemented". Rows are todo unless the notes say otherwise. The curse
and the shielded village before this are in [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) and
[nemesis_shielded_village.md](nemesis_shielded_village.md).

**Progress: 0/77 done, 0 partial — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is the last step of the fifth land's control script: after the curse is lifted the script loops until the player owns Nemesis's own town, then stops every other script of the land and runs the fight, the volcano walk, the temple's fall and the credits one after another | todo | the land never runs: `LOAD_MAP` is empty; `StopAllScriptsInFilesExcluding` itself works |
| It is logged in the story's log as a gold quest under the title "So this is a fight to the death" (a Nemesis line); the log entry is made at 0 when the mirror creature appears and set to 1 when it is beaten | todo | `Snapshot` and `UpdateSnapshot` are stubs |
| The log title is borrowed from a cut Nemesis speech ("My dream was to be the only god. … So this is a fight to the death") that is not in the shipped scripts, so the line is never heard in the game; the log's reminder is the same line | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The scroll has no highlight: nothing has to be clicked, the fight begins by itself | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Nothing comes after it: it is the game's last quest | n/a | story order: curse lifted, Nemesis's last town, this |

## Nemesis's last stand (before the fight)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the curse is lifted, Nemesis's last village defends itself (burning meteors, a physical shield) and his blast barrages hit the player's villages and temple every 15 seconds; his last village can only be won while he holds no other village, with the advisors' retaken and "we can conquer" lines | todo | see [nemesis_shielded_village.md](nemesis_shielded_village.md#nemesiss-last-village); the town capture reads are stubs |
| Owning Nemesis's last town ends the last stand: every other script of the land is stopped and the fight begins | todo | the town capture reads are stubs |

## The mirror creature (introduction)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's hand is moved over the arena by his temple and his own creature is put in the arena facing it | todo | `SetComputerPlayerPosition` is a stub and Nemesis's creature is never made (`LoadCreature` is a stub); `SetPosition` is real |
| A cut scene begins with Nemesis's music | todo | `StartMusic` is real (`src/Audio/Services/GameMusic`); never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The camera flies 16 seconds from near Nemesis's last town towards the arena | todo | `SetCameraPosition`, `MoveCameraPosition`, `MoveCameraFocus` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Both advisors come out; the evil advisor cheers: "Unbelievable! We did it! Oh yeah!" and "We have all his towns! He has no power! We've won!"; both go home | todo | `SpiritEject`, `SpiritHome`, `RunText` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The player's creature is put by the arena facing the camera; Nemesis speaks: "You are indeed a worthy opponent." and "But stop celebrating. I am not finished yet." | todo | `SetPosition` on the player's creature and `RunText` are real; turning it (`SetFocus`) is not done for creatures; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The creature plays its sad animation; a slow 20-second shot on it while Nemesis says "Call that a Creature?" and "It's not worthy of the name." | todo | creature animations by script are not done |
| A 20-second shot towards his temple: "THIS is a Creature. And I will make him the opposite image of yours."; his hand rises | todo | Nemesis's creature is never made; the camera and text are real |
| Glittering smoke and sparkle rings spiral around Nemesis's creature (two rings 15 out, turning 20 degrees a step and rising and falling up to 16) until the fight starts | todo | `SpecialEffectPosition` is real, but Nemesis's creature is never made |
| His creature plays its angry animation, is made invisible by a creature spell and plays its summoning animation | todo | Nemesis's creature is never made |
| A new creature of the same species as the player's is loaded with Nemesis's creature mind file (`Scripts/CreatureMind/NemesisCreature`) as Nemesis's, named "Mirror", fading in over 6 seconds and auto-scaled | todo | `LoadCreature` and `SetCreatureName` are stubs |
| Its alignment is the opposite of the player's creature, pushed to the extreme: fully evil if the player's creature is good, fully good otherwise (a creature of exactly neutral alignment gets a fully good mirror) | todo | the mirror is never made; `SetAlignment` itself is real |
| It is fully grown, the same size as the player's creature, at full strength and knows every action | todo | the mirror is never made and `CreatureLearnEverything` is a stub |
| After 6 seconds Nemesis's old creature fades away and is deleted; the mirror plays its summoning animation and the gold scroll is logged | todo | the mirror is never made; the log is a stub |
| The player's creature walks to its mark and taunts; the camera rises over the arena in 10 seconds; the music stops and the cut scene ends | todo | walking and animating a creature by script are not done |

## The fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The fight starts only when the player's creature is within 300 of the arena; both creatures are then forced to fight each other | todo | `CreatureDoAction` is a stub; fighting itself: [fighting.md](../../creature/fighting.md) |
| The player fights it as any creature fight | todo | there is no opponent; see [fighting.md](../../creature/fighting.md) |
| Nemesis cheats: whenever the mirror's fight health drops below half, a level-two heal spell is cast on it and its fight health and health go back to full, then the script waits 10 seconds; at most four times per bout | todo | there is no opponent and fight health is not a handled property |
| When the fight ends the creature with the higher fight health wins | todo | `IsFighting` is a stub |
| Winning: the mirror dies for good and the scroll is set to 1 | todo | there is no opponent |
| Losing: the player's creature's health is set to nothing and it is put in the dead state; Nemesis's creature slowly heals between fights (1% every 6 seconds, up to 10 minutes from empty to full, stopping if it fights) | todo | creature health and states by script are not done |
| Losing cut scene: the camera turns to the mirror, which summons; Nemesis says "Your weakness is clear." and "You do not have what it takes!"; a one-second fade to black; the mirror is put back in the arena and the player's creature at the edge of the arena facing it; fade in; the evil advisor: "Boss. Come on. Give it another go!" | todo | `SetFade` and `SetFadeIn` are real, but the mirror is never made |
| After losing the fight can be retried at once: walking the creature back within 300 of the arena starts a new bout with four fresh heals | todo | there is no opponent |
| Losing the fight does not lose the game; the land can still be lost in the usual ways (see [../losing_and_game_over.md](../losing_and_game_over.md)) | todo | see [../losing_and_game_over.md](../losing_and_game_over.md); never reached: Land 5 needs `LOAD_MAP`, an empty native |
| How the player's creature recovers from the dead state before the next bout is not said by the script | todo | undetermined, and creature states by script are not done |

## Nemesis's farewell (success)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music: his hand rises over the arena, the creature turns to the camera and plays its happy animation; 5 seconds later a high shot over the land | todo | `StartMusic` is real; the creature's animation and Nemesis's hand are stubs |
| Nemesis: "It's not over yet!", "You have defeated me in this land but I have learned much from this struggle.", "I will rise again from the ashes of this defeat stronger than you can possibly imagine." and "There are many, many lands in this world and they will soon follow in the way of Nemesis." | todo | `RunText` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The camera drifts 30 seconds towards the volcano; both advisors come out | todo | real camera and advisor natives; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Evil: "This is pointless! It could go on forever if we don't stop it now." Good: "Remember, we can use the power of the creeds to destroy Nemesis forever." and "Fusing the three creeds will confirm our superiority and leave no hiding place for him." Evil: "Three creeds? But we've only got two!" Good: "No the third is within OUR creature, you fool." Evil: "Of course, why didn't I think of that?" Good: "Hmm." and "You need to lead our creature into the volcano. The searing heat will do the rest." Evil: "But just a minute, doesn't that mean our creature might die?" and "Well, its up to you boss." Good: "Do you want to risk our noble creature to rule as you see fit?" Evil: "Or just let Nemesis spread his influence throughout the world again?" | todo | `RunText` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The mirror's body is moved far across the land; the advisors go home, the music stops and control returns | todo | the mirror is never made |
| The game waits until the player brings the creature within 50 of the causeway's start below the volcano; the leash is then let go and the mirror is deleted | todo | `DetachObjectLeash` and the distance wait are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| This is the "choice": there is no other way on; a player who never leads the creature there simply goes on playing the land | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Into the volcano

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music: the creature is put at the causeway's start facing the volcano's top; the camera pans behind it over 10 seconds | todo | `StartMusic` and `SetPosition` are real; turning the creature is not done; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| It walks halfway along the causeway; the screen rumbles (sound and an 8-second camera shake of radius 800) and the creature looks up at the top | todo | `ShakeCamera` and `PlaySoundEffect` are real, but walking the creature by script is not done |
| It walks up to near the top, then to the top, the camera following in two moves (8 and 14 seconds) | todo | walking the creature by script is not done |
| At the top a fixed shot: the creature plays a special scripted animation (the creeds fusing), and after 3 seconds the game drops to half speed | todo | `SetAnimationModify` is a stub |
| The creed sound plays; the camera watches the creeds appear in two shots; another rumble with a 16-second shake | todo | `PlaySoundEffect` and `ShakeCamera` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The music stops; at 80% through the animation the creature's frozen moan is heard and the creature's end-sequence music starts | todo | `StartMusic` is real, but `PlayedPercentage` is a stub |
| At 90% the game's sound effects are switched off and the falling film plays: the creature falling into the volcano | todo | the film plays through our Bink decoder (`SetAviSequence`, `src/Video`), but it is reached at 90% of a stubbed animation read (`PlayedPercentage`); see [../../video/bink_videos.md](../../video/bink_videos.md) |
| After the film, sound and normal speed return and the creature is moved to the far side of the land, out of sight | todo | `SetGameSound` and `SetPosition` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| If the player has no creature at this point the whole walk is skipped | todo | `CallPlayerCreature` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Nemesis's temple falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with Nemesis's music fades in on his temple; his hand flies to his temple at speed 50 while the camera follows in 20- and 12-second moves | todo | music and fade are real; Nemesis's hand is a computer-player stub |
| A shot from high above the temple; 3 seconds later rings of beam explosions circle it (four points 15 out, turning 10 degrees a step) | todo | `SpecialEffectPosition` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| 6 seconds later the temple explodes; a 20-second sweep over the ruins; 20 seconds after the first, a second temple explosion, and Nemesis's creature is deleted | todo | `ObjectDelete` of a citadel heart only logs "not implemented" |
| Both advisors: Good "Our noble friend, gone! But can you feel the difference?" Evil "I can't feel even a trace of Nemesis' influence." Good "Everything is clear. You're the one true God, now." | todo | advisors and texts are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The creature reappears, burnt, lying near the Greek town: it is put in the dying state at a tenth of its health, with four smoke clouds (scale 7, for 25, 15, 10 and 30 seconds) | todo | creature health and states by script are not done; `SpecialEffectPosition` is real |
| The screen rumbles (sound and a 2-second shake); Good: "Crikey! What was that?"; a 10-second shot on the creature; Evil: "Hang on, I recognise that smell." Good: "He's back! He doesn't look too hot though." Evil: "Maybe we can fix him up?" | todo | `PlaySoundEffect`, `ShakeCamera` and texts are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The advisors leave, the music stops and the player gets control back: the game waits until the creature's health is above 80% | todo | the creature's health is not a handled property |
| Once healed, the glows of the creeds on both of the creature's hands are taken away | todo | `SetCreatureCreedProperties` is a stub; see ../../creature/creature_tattoos.md |
| 5 seconds later a last cut scene with the epic music: the creature looks at the camera, summons, the camera circles it for 20 seconds, it plays its happy animation, then kneels to pray while the camera rises and everything fades to black over 10 seconds | todo | `StartMusic` is real; the creature's animations by script are not done |
| Nemesis's hand is switched off for good | todo | `EnableDisableComputerPlayer` is a stub |

## The credits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The credits run as one cut scene with the sound effects switched off and the outro music | todo | `StartMusic` with the outro bank and `SetGameSound` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| They are 17 windows, alternately on the left and right of the screen (across 0.1 or 0.5, 0.2 down, 0.4 wide, 0.6 high), each opening over half a second and fading in over 2 seconds on a little scene somewhere on the land | todo | `SetClippingWindow` and `ClearClippingWindow` are stubs |
| In each window the job title is written from the text table and the names are written as plain text (white, fading in over 2 seconds), held 10 seconds, faded out over 2; the window then fades to black and closes | todo | `GameDrawText`, `FadeAllDrawText` and `SetDrawTextColour` are stubs |
| Testing for Lionhead runs over five pages in one window and Testing for EA over three; between pages the text fades out and the next page fades in | todo | the drawn text natives are stubs |
| Each window has its own time of day (noon, 4 pm, or 11 pm for the ark) with the clock stopped | todo | `SetGameTime` and `GameTimeOnOff` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The scenes are the story's people: three crusaders sitting in council ("Black & White", "Designed and Created by Lionhead Studios Ltd."; the Magic Dragon's crusaders, see [../silver_scrolls/the_magic_dragon.md](../silver_scrolls/the_magic_dragon.md)), an impressed Greek farmer (Game Programming), a nomad dancing with an Aztec housewife (3D Programming), a crusader looking for something (Art), an Aztec housewife at the volcano (Animation, Script & Dialogue, Sound & Music), an ogre (Internet, Library & Tools; Scenario & Level Design), a Shaolin monk sitting (Gameplay & Testing), a fisherman (Lionhead Studios Ltd., PR), two Africans talking (Lead Instrumentalist, Musicians), an engineer on the volcano (Voice Characterisation, Additional Art), a priest and priestess praying (Testing for Lionhead), a Greek woman mourning among shepherds (Production for EA), the creature trainer swinging his legs on a pier (Testing for EA), the ark at night (Marketing and Legal for EA), a sculptor at work (Localisation for EA), a hippy chopping at a giant magic mushroom (For Electronic Arts) and the Pied Piper dancing (Special Thanks To) | todo | `CREATE` makes villagers, features and animals now (`CreateScriptObject`); the creature in a scene is not made; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Each scene's people are made with high graphics detail and deleted when its window closes | todo | `SetHighGraphicsDetail` (`ecs::super_villager`) and `ObjectDelete` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The last scene: the story's characters gather and applaud, 30 of them — the monk, the idol builder, the hermit, the hippy, the priest and priestess, the marauder, the engineer, a footballer, the nomad, the Aztec leader, the creature trainer, the sailor, the breeder, the healer, the sculptor, the crusader, a Norse mother and father, and ten villagers of the land's tribes | todo | `CREATE` makes villagers and their animations are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Fireworks go off at four spots for five minutes; sound effects come back; the view fades in | todo | `SpecialEffectPosition` and `SetGameSound` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Each person starts clapping or cheering a tenth of a second after the last; the camera moves through the crowd for 30 seconds | todo | villager animations and the camera are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| After 15 seconds a small boy (age 8, three-quarters size) skips in to his mother; they hug, then both cheer | todo | `CREATE` of a child, the age and scale for villagers are handled; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| 10 seconds later the scene holds until the player presses the space bar | todo | `KeyDown` is a stub, so the scene would hold for ever |
| Everyone is released, the music stops and the cut scene ends; the land's control script then sleeps for 50,000 seconds, so the player is left in the fifth land with no more story | todo | `ReleaseFromScript` and `StopMusic` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors only speak in the cut scenes listed above; there is no reminder nagging during the fight (the log's reminder line is the title line) | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Music: Nemesis's piece for the mirror's making, the farewell, the volcano walk and the temple's fall; the creature's end-sequence piece at the top of the volcano; the second epic for the creature's return; the outro for the credits | todo | all named in `src/Audio/Services/GameMusic` and `StartMusic` / `StopMusic` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Sounds: two screen rumbles on the volcano and one when the creature returns, the creed sound at the top, the creature's frozen moan | todo | `PlaySoundEffect` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The "retaken" warning is given the first time any other town is Nemesis's after the curse lifts, even if he never took it back; the "we can conquer" line is only heard after such a warning | todo | the town owner reads are stubs |
| The Pied Piper of the closing crowd is never made: his spot, focus and clapping are set but no villager is created for him, so he does not appear (he does appear in the last credit window) | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The hippy in the crowd is never given high graphics detail, unlike everyone else | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The winner is decided by comparing fight health once the fight ends, however it ended | todo | there is no opponent and fight health is not handled |
| A source comment asks for the invisibility spell on Nemesis's old creature to be put back once fixed; in the shipped scripts it is cast | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Unused and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An earlier mirror fight ("Creature Mirror Fight") is compiled into the game but nothing starts it (only its single-quest test launcher): Nemesis's hand flies around the player's creature, he says "Your Creature has proved itself many times. But can it beat its true self?", makes a copy of the creature next to it with the opposite alignment, only anger as a desire (at half) and fully grown, and says "This is your Creature if you'd been the god you should have been." and "Watch and weep as it shows its superior strength and intellect." | n/a | never started by the game |
| In it the advisors cling to the screen edges and say "We'll give it a pummelling!" (evil) and "Er, yes. I hope so." (good); the fight starts when the creatures are within 50; winning: "We did it! Did you see that? Oh yeah! The crowd goes wild!" and the copy is deleted; losing: "We're losing. Stop and let our Creature heal." and "Come on! Crush him!" | n/a | never started by the game |
| A cut speech by Nemesis over a monsoon storm at 4:30 pm ("My dream was to be the only god.", "To have the whole of Eden worship me.", "But you stand in opposition.", "So this is a fight to the death") is not compiled into the game; its last line is used as this quest's log title | n/a | not in the shipped `challenge.chl` |
| A script for Nemesis's battle tactics is compiled but never started | n/a | see [ending.md](../ending.md) |
| A commented-out line would have made the mirror from the player's creature directly instead of loading Nemesis's mind file; a commented-out step would have grown a tree after the temple's fall; a commented-out script would have pushed the creature's alignment to an extreme during its return | n/a | comments in the source only |
