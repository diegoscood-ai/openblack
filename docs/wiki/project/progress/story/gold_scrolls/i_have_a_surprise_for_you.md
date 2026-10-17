# I have a surprise for you.

Land 5's first story quest: as the player arrives, Nemesis curses the creature, and every night after that three of
his Wonders drain its strength, shrink it and turn its alignment towards his own while a fourth pulls it towards his
last village. Winning the three villages that hold the Wonders lifts the curse in a healing scene that gives a heal
miracle dispenser. The quest is only ever logged: it has no scroll on the map, and its entry in the challenge log is
titled with Nemesis's line "I have a surprise for you." (the game's text table has no proper title for it).

**Land:** 5 · **Giver:** Nemesis, in the land's opening film; lifted by a man dressed as a crusader at "the place of healing" by the Greek village · **Script:** BeginLand5, CreatureCurse and the land's control script LandControl5 (challenge CREATURE_CURSE) · **Reward:** the creature put back as it was when the land began, and a heal (powered-up) miracle dispenser that recharges every 5 minutes · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, which matches the PC game's compiled `challenge.chl`),
the game's text table (`Scripts/InfoScript2.txt`) for every line and its speaker, and the executable for what the
creature actions do. openblack's state is judged on this tree: the story's top script always runs Land 1's control
script first and the land-loading command (`LoadMap` in `src/CHLApi.cpp`) has an empty body, so Land 5's control script,
and this quest with it, never runs. Every row is todo unless its notes say a working command would do that part. The
land as a whole is in [../land_5.md](../land_5.md), its map script in
[../../scripts/land5_script.md](../../scripts/land5_script.md); the shielded Tibetan village that gates the third step
is in [nemesis_shielded_village.md](nemesis_shielded_village.md) and the end of the game in
[so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md).

**Progress: 0/79 done, 0 partial — 0%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts by setting the time of day to 13:00 | todo | `SetGameTime` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| It orders the three Wonders built in Nemesis's Aztec, Greek and Tibetan villages (build desire 0.6) and sets each Wonder, and a fourth in Nemesis's own village, to 60% built | todo | `BuildBuilding` is real, but the building look-ups by town need `GetTownWithId` (a stub) |
| It then runs the land's set-up (land number 5, the player's virtual influence on, no alliance with Nemesis, the volcano's glowing crater, scaffolds for a storage pit and a workshop in the home town, Nemesis's creature made at twice normal size) and the opening film below | todo | `SetVirtualInfluence`, `SetPlayerAlly` and the rival creatures (`LoadCreature`) are stubs; the volcano vortex and scaffolds are real; see [../portals.md](../portals.md) |
| The quest is a gold (story) quest logged in the challenge log with no scroll on the map: nothing in the land's scripts puts up a gold highlight for it | todo | `Snapshot` and `UpdateSnapshot` are stubs; see [../../interface/scrolls_and_signs.md](../../interface/scrolls_and_signs.md) |
| Its log title is the text of Nemesis's line "I have a surprise for you."; its reminder, replayed from the log, is the evil advisor stepping out to say "What's wrong with our Creature?" | todo | the log is a stub; the shared reminder itself calls real natives |
| The log entry is made at the end of the opening film with progress 0 and no alignment change | todo | the log is a stub |
| After the film, the land's control script notes the creature's size, strength and alignment as they are then; the cure puts these back | todo | strength and size are not handled by `GetProperty` for a creature |
| The curse script is started in the background together with the Japanese Traitor, the shielded village, the Magic Dragon, the Explorers Again (only if the ark sailed on Land 1) and Nemesis's town defences | todo | plain script; see [../silver_scrolls/the_japanese_traitor.md](../silver_scrolls/the_japanese_traitor.md), [nemesis_shielded_village.md](nemesis_shielded_village.md); never reached: Land 5 needs `LOAD_MAP`, an empty native |

## The opening film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis is paused, his attitude to the player set to 2.0 and the two players made sure not to be allies; the vortex lets the player's people and creature through (shared vortex arrival) | todo | `EnableDisableComputerPlayer`, `SetComputerPlayerAttitude` and `SetPlayerAlly` are stubs; arrival: [../portals.md](../portals.md) |
| Nemesis's hand is moved to his citadel; the film starts with the Nemesis theme and a 6-second fade in, the camera at the vortex rising over 10 seconds | todo | `StartMusic`, `SetFadeIn` and the camera are real; moving Nemesis's hand is a computer-player stub |
| After 3 seconds the evil advisor steps out: "Time to face the ultimate foe, Boss." and "But hey - let's show no fear."; then goes home | todo | `SpiritEject` and `RunText` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The camera turns to Nemesis's citadel over 6 seconds; Nemesis: "I have been watching your power grow." | todo | real camera and text natives; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The lens widens to 100 over 15 seconds while Nemesis's hand drifts towards the vortex and the camera pans across the land for 20 seconds; Nemesis: "You are the only foe left and I have enjoyed your successes." and "The stronger you are, the more impressive it will be when I crush you." | todo | `MoveCameraLens` and the camera are real; Nemesis's hand drifting is a computer-player stub |
| His next line depends on the player's alignment: an evil player hears "Once I destroy you every living creature in Eden will worship me, Nemesis, the embodiment of good."; any other player hears "... the embodiment of evil." | todo | `GetAlignment` and `RunText` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| A dark red fade (3 seconds), the lens back to 70, then three 10-second fly-pasts of his Wonders, each opened with a 2-second fade in and closed with a red fade: the Tibetan one ("I gain nothing from defeating the weak. Only now are you worth beating."), the Greek one ("And all your power, all your worshippers will be mine.") and the Aztec one ("That, my enemy, is the ultimate prize.") | todo | `SetFade` takes the colour (`src/3D/ScreenFade`); the lens and camera are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The creature is put at its start spot by the home town, the camera back at the vortex; the music stops; Nemesis: "I have a surprise for you." | todo | `SetPosition` is real; turning a creature (`SetFocus`) is not done |
| The curse itself is shown without seeing who casts it: the creature looks confused; sounds named for an imp hum, giggle and patter about while the camera shakes six times a tenth of a second apart and runs along the ground as if from the imp's eyes; the creature sneezes (to muzak), looks confused again, scratches itself (to muzak again) and is walked back to its spot | todo | `PlaySoundEffect` is real; creature animations by script are not done |
| The camera climbs to the creature's head and dives in (a suspense sound), a 1-second red fade, a punch sound, the Nemesis theme returns and the creature recoils, then collapses: the game's "die permanently" creature action only makes it faint and wait, it does not kill it | todo | `CreatureDoAction` is a stub |
| Two seconds later a 30-second high pan with a 2-second fade in; Nemesis: "You'll find your Creature is different since I cursed him." and his laugh "Bwa ha ha ha ha!" is played as speech only, not waited on | todo | real camera, fade and text natives; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| 8 seconds later the quest is logged (see above); a 2-second fade to black, a view over the home town towards the vortex, a 2-second fade in; 3 seconds later the creature is let go, 2 more and the music stops | todo | the log is a stub |
| The film ends in dialogue: the good advisor: "Phew! Our Creature seems to be all right." and "We'd better keep a close eye on him, though." | todo | `RunText` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Nemesis is let go; 2 seconds later the home villagers are told to build the citadel and the town centre (desire 1.0) | todo | `ReleaseComputerPlayer` is a stub; `BuildBuilding` is real |

## The curse at night

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Which Wonder does what: the Aztec Wonder drains strength, the Greek Wonder shrinks the creature, the Tibetan Wonder turns its alignment, and the Wonder in Nemesis's own village draws it towards that village | todo | plain script; see [../silver_scrolls/the_japanese_traitor.md](../silver_scrolls/the_japanese_traitor.md); never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The curse strikes once a night: when the game time is before 6:00 or after 18:00 it strikes, then waits until daytime (between 6:00 and 18:00) before it can strike again | todo | `GetGameTime` is real; day and night: [../../sky/day_night_cycle.md](../../sky/day_night_cycle.md); never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Each strike makes a dust cloud on the creature for 8 seconds, seven times normal size | todo | `SpecialEffectPosition` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| While the Aztec Wonder stands and strength is above 0.2, strength falls by 0.16, and wobbles up and down round the new value for a short while | todo | a creature's strength is not handled by `SetProperty`; see [../../creature/physiology.md](../../creature/physiology.md) |
| While the Greek Wonder stands and the creature is taller than 3, its height falls by 1, with a small wobble | todo | a creature's height is not handled by `SetProperty`; see [../../creature/growth_and_size.md](../../creature/growth_and_size.md) |
| Alignment: the creature is pushed towards the opposite of the player's alignment (a good or neutral player's creature towards -1, an evil player's towards +1) by 0.4 a night, with a wobble | todo | `SetAlignment` and `GetAlignment` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The creature is frightened on the spot; the advisors cling to the left and right edges of the screen and one line is picked at random: evil "What's wrong with our Creature?", or good "Is this something to do with Nemesis' curse?", "What's going on with him?" or "What's the matter with our poor Creature?" | todo | `ClingSpirit` and the texts are real; frightening the creature (`CreatureDoAction`) is a stub |
| When the fright ends the creature looks at the hand | todo | creature actions by script are not done |
| While Nemesis's own Wonder stands: a leashed creature stares and points at Nemesis's village and is frightened again; an unleashed one is walked off towards it | todo | creature actions and walks by script are not done |
| The good advisor then steps out, points at the creature and says "Phew! Our Creature seems to be all right." every night, straight after the curse has struck | todo | advisors and texts are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Once the three Wonders are gone the strikes stop, but the pull towards Nemesis's village goes on each night while his own Wonder stands | todo | the creature's pull is a creature action, not done |
| The curse script is stopped outright when the three villages have been won (see "Lifting the curse"), so the nightly pull ends there too | todo | `StopScript` is real, but the three villages are never counted won: the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |

## Lifting the curse, step by step

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the curse is on, Nemesis's own village cannot be won: whenever the player's belief there reaches 0.1 it is set back to 0 | todo | `BeliefForPlayer` and `SetPlayerBelief` are stubs |
| Each time that happens with the camera within 200 of the village and the village in view, the good advisor points at it: "Oh no. The people here are frantically zealous about Nemesis." and "We're not yet powerful enough to stand a chance!" (no once-only guard: it can repeat) | todo | follows from the belief reads, which are stubs |
| Winning the neutral Japanese village for the first time starts Swap To Brown Bear (once) | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); [../silver_scrolls/swap_to_brown_bear.md](../silver_scrolls/swap_to_brown_bear.md) |
| Each Wonder village is a step: the first time it is won, a film plays, the Wonder is blasted and the quest's progress goes up by a third (0.33, 0.66, 0.99) in whatever order the villages fall | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| Aztec village film: fade to black (1 second), a short epic sting; Nemesis's hand rushes in and drifts over the Wonder as the camera pans for 12 seconds; Nemesis: "How can this happen? You have stolen a town from me!" and "I will deny you the Wonder, though."; his hand rises, the camera pans 8 seconds, and after 2 seconds a blast (radius 50, 15 seconds) falls on the Wonder from 30 above; 1 second later the step is logged; fade back to where the player was, Nemesis let go | todo | the blast is cast by `SpellAtPos` ([../../miracles/blast.md](../../miracles/blast.md)), but the film starts on a capture: the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| Greek village film: Nemesis is paused; a Greek farmer is made by the Wonder: "We bow down to our new god!", "Nemesis forced us to build this Wonder.", "But now it will help you in your conquest of his realm!", "There's a nomad in the woods near the Village.", "He told us of the special powers this Wonder has." and "He said…" | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| ...he is cut off by a blast on the Wonder; the step is logged; the Nemesis theme starts, the farmer ducks and Nemesis's hand swoops down beside him: "Silence! You will not have this wonder!" and "I would rather it were destroyed!" | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| ...the farmer stands in despair: "And as for you, underling.", "Your change of alliance… disappoints me." and "You will regret your fickle spirit."; scared stiff, he screams; 1.5 seconds later a tornado (radius 25, 30 seconds) is cast on him and 4 seconds after that he is thrown into it | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); the tornado is cast by `SpellAtPos`, throwing the farmer (`SetTarget`) is a stub |
| ...the camera tilts up over 20 seconds; Nemesis: "I WILL win this place back."; 10 seconds, a 5-second fade, and the film ends; the Heavenly Fire starts as it fades back in | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); [../silver_scrolls/the_heavenly_fire.md](../silver_scrolls/the_heavenly_fire.md) |
| Tibetan village film: the epic sting, Nemesis paused, a 20-second pan; both advisors out; the evil advisor: "That's one of Nemesis' towns!", "I can feel our power growing!" and "Nemesis must be pretty shook up. We can beat him, I know it!"; a blast on the Wonder; the step logged; Nemesis's hand rushes up: "Your cunning does you credit. But this is my land." and "And this town will be mine once more." | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); see [nemesis_shielded_village.md](nemesis_shielded_village.md) |
| A village only counts the first time it is won; losing it to Nemesis afterwards does not undo the step | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| When all three have been won once, the curse script is stopped and the healing scene plays | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |

## The healing scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A 2-second fade to black; a man dressed as a crusader is made by the Greek village and walks to his place; the creature is put at "the place of healing" beside him | todo | reached only after the three captures: the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| He turns to the camera and does a Mexican wave: "Your power is plain to see.", then "I sense you are nearly a match for Nemesis himself." and, with a thank-you: "I have brought you to this place of healing." | todo | reached only after the three captures |
| The camera frames the creature by its height; the glorious epic theme starts | todo | reached only after the three captures |
| Four magic beams rise round the creature's spot, circling and closing in (radius 10 down to 1) until the change is done; a spell sound plays and the creature plays its summoning animation as the camera moves to frame it at its old size | todo | reached only after the three captures |
| The man: "Your mighty Creature can now shake off the poisoned curse of Nemesis." | todo | reached only after the three captures |
| The creature is put back: its alignment at once to what it was when the land began (after the opening film); its strength in steps of 0.1 and its height in steps of 0.5 until both are back at those values | todo | a creature's strength and height are not handled by `SetProperty` |
| 5 seconds into a 10-second camera move the beams stop; 5 seconds after the move the creature sits and the man walks round in front of it | todo | reached only after the three captures |
| The quest is logged complete (progress 1) | todo | `Snapshot` is a stub |
| The man: "Your Creature is better, but Nemesis' sickly curse could still be lingering." | todo | reached only after the three captures |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera cuts high above the healing place and a miracle dispenser of the powered-up heal is set up there (Norse style, turned 270), recharging every 5 minutes | todo | `CREATE` of a spell dispenser is real, but it is reached only after the three captures; see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md), [../../miracles/heal.md](../../miracles/heal.md) |
| Being given with the "tell" option, the shared reward script plays its own reward sting and film over it: the evil advisor's dispenser lines ("This pedestal is a Miracle Dispenser...", the signpost and "Did you know" tip for a player's first dispenser, otherwise "Nice. Another of those cool Miracle Dispensers.") and the advisors' help on the miracle | todo | the reward script needs `CreateReward` (a stub) |
| The man goes on: "Your Creature is being drawn towards Nemesis' last Village!" (camera pans 15 seconds), "Your powers are strong enough to defeat him utterly!", "But be careful. He is weaker but he is not beaten." and "We will always be your servants. Return here should you need healing." | todo | reached only after the three captures |
| He is deleted, the music stops, the film ends and the creature is let go | todo | reached only after the three captures |

## Aftermath and what it unlocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land moves on to its last part: Nemesis's blast barrages on the player's villages and citadel every 15 seconds, and his last village can now be won whenever he holds none of the land's other villages | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); [nemesis_shielded_village.md](nemesis_shielded_village.md) |
| Winning his last village ends the land's script loop, stops every other Land 5 script and starts the big fight | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); [so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md), [../ending.md](../ending.md) |
| Swap To Brown Bear is only offered while the curse is on; once the curse loop ends, the Japanese village no longer starts it | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| The dispenser and the healing place stay for the rest of the land | todo | reached only after the three captures |

## Failure and soft-locks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: the curse only weakens the creature (strength never below 0.2, height never below 3) and has no time limit | todo | the strength and height changes are not handled |
| It cannot soft-lock on the villages: each counts once it has been won once, so Nemesis taking villages back never undoes progress | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| The Tibetan village can only be won once the spiritual shield over it is down (it keeps the player's influence out); breaking it is always possible, by scaring or killing its three worshippers or moving their stones | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties); [nemesis_shielded_village.md](nemesis_shielded_village.md) |
| Losing all the player's people still ends the game as on any land | todo | see [../losing_and_game_over.md](../losing_and_game_over.md); never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Nemesis theme plays over the opening film, is stopped before "I have a surprise for you." and comes back with the punch | todo | `StartMusic` and `StopMusic` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The curse is told in script sound effects: the imp's hum (three variants), giggles, footsteps, a muzak tune, the mushroom suspense sting and a body punch | todo | `PlaySoundEffect` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| Each Wonder village film opens with the short epic sting; the Greek film changes to the Nemesis theme after the blast; the healing scene plays the glorious epic theme and a spell sound | todo | `StartMusic` and `StopMusic` are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Script quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The alignment part ignores its Wonder half the time: an evil player's creature is pushed 0.4 towards good every night whether or not the Tibetan Wonder stands (the "push down" branch can never apply to it), and once the Tibetan Wonder is gone a good player's creature is pushed 0.4 towards good every night instead of being left alone, for as long as any of the three Wonders stands | todo | the same script and `SetAlignment`; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The cure puts strength and size back to what they were just after the opening film, so anything the creature gained on Land 5 before the cure is lost, and growth is undone | todo | the strength and size reads and writes are not handled for creatures |
| The man says the creature "is being drawn towards Nemesis' last Village" just after the script that did the drawing has been stopped | todo | reached only after the three captures |
| Nemesis blows up each Wonder himself in its film, so the player never gets to keep one, although the Greek farmer says it "will help you in your conquest" | todo | the village captures are read through stubs (`GetTownWithId`, `BeliefForPlayer`, unhandled owner properties) |
| The "village taken back" lines can never play: the check for Nemesis retaking a village tests the same condition as the check for the player owning it, so it is never reached, and the player's "retaken the town" lines never play either because a village is never marked as lost. Unreachable lines: good "Excellent! We've retaken the town!", Nemesis "Ha! That is how it should be!", evil "We're the best. We are! Way to go!", Nemesis "Ha. That was too easy to be a Challenge!", evil "Ha! Maybe Nemesis isn't as powerful as he likes to think." (which would have pointed at the Greek village while speaking of the Tibetan one) and Nemesis "Welcome back, my worshippers. Do not stray again." | n/a | never reachable in the game |
| The opening film's laugh "Bwa ha ha ha ha!" is started as sound only and the film moves straight on | todo | `PlaySoundEffect` is real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The advisors' "Phew! Our Creature seems to be all right." follows every nightly strike | todo | advisors and texts are real; never reached: Land 5 needs `LOAD_MAP`, an empty native |
| The changes are made by script loops without pauses (the cure's strength and size steps, and the beams' spiral) so they finish within the same moment or over a few frames rather than visibly | todo | the same script; never reached: Land 5 needs `LOAD_MAP`, an empty native |

## Unused and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's "You'll find he won't be of much use to you now." is commented out of the opening film | n/a | never in the game |
| The advisors' "Where's our Creature going?" and "Where's he off to now?" were cut from the nightly lines ("the creature doesn't really move too much into Nemesis's land") | n/a | never in the game |
| Nemesis's "I have one more surprise for you." has no script that says it | n/a | never in the game |
| An earlier curse script is compiled into the game but nothing starts it: it struck every 60 seconds at night, took 0.1 strength (Aztec), 0.1 fatness (Greek, not height) and 0.2 alignment (Tibetan) per strike, played a first film ("Nemesis curse has harmed our Creature!") and later "Our Creature is suffering further from Nemesis' curse!" then "He's heading towards Nemesis himself!" or, if leashed, "The dark of night must be the key to this!", kept the creature's gains while cursed so they could be added back, and ended only when the player owned Nemesis's village | n/a | never started by the game |
| Its helper that walks the creature to Nemesis's village and waits until it is leashed or within 100 is also unused | n/a | never started by the game |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is the quest's subject: placed, animated and made to faint in the opening film, frightened, shrunk, weakened and turned each night, pulled to Nemesis's village, then healed and seated in the healing scene | todo | creature commands are stubs in `src/CHLApi.cpp`; see [../../creature/physiology.md](../../creature/physiology.md), [../../creature/growth_and_size.md](../../creature/growth_and_size.md) |
| Being on the leash changes the nightly pull: it only stares and points instead of walking off | todo | `IsLeashed` is real, but the pull is a creature action, not done; see [../../creature/leash.md](../../creature/leash.md) |
