# The Pied Piper

A silver scroll of the first land: a strange piper (a lonely, ugly man) lures the Norse village's little children into a
trance, dances them up to his cave and shuts them in. The crèche woman asks for help; the player's creature has to
catch him (he runs from it and hides in his cave), and the player then either has him carried back to his cave to free
the children or lets the creature eat him. The land as a whole is in [../land_1.md](../land_1.md).

**Land:** 1 · **Giver:** the crèche woman at the Norse village's crèche (scroll over the crèche) · **Script:** PiedPiper ·
**Reward:** good ending, a heal miracle dispenser (a heal power-up dispenser if the player already has heal) and
alignment +1; evil ending, a lightning miracle dispenser and alignment −0.3; no reward if thirty children are lost ·
**Repeatable:** no

Sources: the quest's challenge script (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's control and guide scripts, the shared reward and notice scripts, the game's text table and
the executable (how a scroll record and its alignment are applied). openblack's state is judged on this tree: the land's
control script starts this quest only in a game that skips the creature training (the start-up box's fourth answer; see
[../../scripts/land1_script.md](../../scripts/land1_script.md)), and of the 93 script functions the quest and the shared
scripts it runs (notice, reward, did-you-know) call, 12 only log "not implemented" in `src/CHLApi.cpp` (counted on an
earlier tree: at this tree the regenerated native table shows no stub on these scripts; the held-object read and making
a random villager are real; `CREATURE_DO_ACTION` is real but the two creature actions this quest needs, put down and
hold an object, have no executor yet, see below). The 81 that work include the camera set/read commands,
widescreen, music start and stop, distance and position reads, random numbers, the game time, stopping a script, the
fire/indestructible/pick-up/moveable flags, the leash question and the leash key, and making rocks, mobile statics,
villagers and dispensers. Every row below is pending unless the notes say otherwise.

**Blocker (checked on this tree):** the catch forces the creature's actions 67 PutDown and 64 HoldObject, and neither
has an executor (`src/Creature/CreaturePlanActions.cpp`, not in the executor table nor in the forced-only one), so
`CREATURE_DO_ACTION` logs "not implemented" and the creature never takes the piper: the catch times out after 10
seconds, he runs back to his cave and the quest loops; none of its endings can be reached until those two executors
exist.

**Progress: 1/85 done, 0 chosen, 39 partial, 0 inferred — 16%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest (in the background) only after the guide's food lesson, when the player has been sent to impress the Aztec village; with the creature training skipped, the guide's lessons are left out and it starts right after the Saviour's script | partial 40% | only with the fourth tutorial answer (creature training skipped): `LandControl1` starts it right after the Saviour's script; with the other answers the land control script never gets there; re-measured 2026-10-21: the natives it calls work; the land's control script does not get there in a run |
| A silver challenge scroll is put over the crèche house of the Norse village | partial 60% | `CREATE_HIGHLIGHT` works (`src/ECS/ScriptHighlight.cpp`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out, points at it and says "Your attention is required here." — at most once every 30 seconds, and only when no film is playing | partial 60% | the shared notice script runs on working natives (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, `SPIRIT_EJECT`, `SPIRIT_POINT_POS`, `RUN_TEXT`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Clicking the scroll or the crèche house itself starts the quest; the scroll is then switched to active | partial 60% | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| While the guide sleeps between lessons, the good advisor may fly the camera high over the land to the crèche to suggest this scroll ("You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it."), after the Missionaries and the Lost Flock; once this scroll is finished the guide's nap may end early | pending | the guide's lessons are skipped with the only tutorial answer that starts this quest, so this never happens |

## Setting the scene (on clicking)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Norse housewife, the crèche woman, is made at the crèche, with a small attraction around her that draws villagers | partial 60% | `CREATE` makes villagers (`VillagerArchetype`) and `INFLUENCE_OBJECT` works (`src/Magic/Script/CHLInfluence.cpp`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The piper is made at his cave (a villager of his own kind, with his own model and dance animations) and leads a little flock of his own | partial 60% | `CREATE` makes him as a villager and `FLOCK_CREATE` / `FLOCK_ATTACH` work (`src/ECS/Flocks.cpp`); his dance clips are not checked; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The piper can't be picked up by the hand, can't be hurt or killed, is not hurt by fire and can't be set on fire | partial 60% | the four flag natives work on him (`SET_ID_PICKUPABLE`, `SET_INDESTRUCTABLE`, `SET_HURT_BY_FIRE`, `SET_SET_ON_FIRE`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Two little boys and two little girls (age 3) are made at the crèche and join the Norse village, so there are children for him to take | partial 60% | `CREATE`, the age (`SET_PROPERTY` age on a villager) and `FLOCK_ATTACH` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The game pretends five children are already shut in the cave from the start | done | a script counter; nothing to port; re-measured 2026-10-21: a script counter, as the original |
| The muffled crying of the children in the cave starts and the piper's round of collecting children begins, both before the introduction | partial 50% | the cave cries (`PLAY_SOUND_EFFECT`) work; the round starts but stalls (see below); re-measured 2026-10-21: the cries work; the round starts and stalls |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen): the crèche woman is drawn in high detail and the second generic script music starts | partial 60% | `SET_WIDESCREEN`, `SET_HIGH_GRAPHICS_DETAIL` and `START_MUSIC` work (`src/Audio`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The camera glides to the crèche over 7 seconds, following the woman as she walks out to a soapbox spot and paces between two places, then turns to face the camera | partial 60% | `MOVE_CAMERA_POSITION`, `SET_FOCUS_FOLLOW`, `MOVE_GAME_THING` and `SET_FOCUS` on a villager work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| She looks around as if for something (four times): "Oh no! Some of my poor children are lost! They'll need rescuing!" while the camera closes in on her over 5 seconds | partial 60% | the clips (`SET_SCRIPT_ULONG` on a villager), `RUN_TEXT` and the camera work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| She stands in despair (four times): "A stranger has been seen putting the children into trances." — the camera cuts to the piper and drifts up beside him | partial 60% | the clips (`SET_SCRIPT_ULONG` on a villager), `RUN_TEXT` and the camera work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| "He has them imprisoned in a secret place!" — the camera cuts to the cave and zooms in over 4 seconds while three muffled child cries play one after another from the cave | partial 60% | the camera cut and the cries (`PLAY_SOUND_EFFECT`) work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Back on the woman, the camera drifts slowly (14 seconds) to her crying: "This stranger is to blame. It's got to be him." | partial 60% | the camera, the clips, `RUN_TEXT` and the sound work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| She mourns (four times): "But if he should die before we find the children, they could be lost forever. Oh, my babies!" and a woman's cry plays | partial 60% | the camera, the clips, `RUN_TEXT` and the sound work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The dialogue box closes and the quest is recorded under the title "The Pied Piper" at 0% with the good advisor's reminder "We must find the person behind this child-stealing business." | pending | the dialogue closes (`GAME_CLOSE_DIALOGUE` works) and the record is made by `SNAPSHOT`, real now; not checked in play |
| The camera returns over 3 seconds to where the player had it; the woman goes back to wandering near the crèche (within 6, pausing 4 to 20) and the music stops | partial 90% | the camera return and `STOP_MUSIC` work; her wandering's `SET_SCRIPT_STATE_POS` and `SET_SCRIPT_FLOAT` and state 199 are real (checked against v1.42 on 2026-10-20); re-measured 2026-10-21: the camera return, the music and the wandering are real |
| The good advisor: "We must act. Tiny lives are at stake." The evil advisor: "Tiny lives? Who cares? Anyway, let's move out! I've always wanted to say that." | partial 60% | `SPIRIT_EJECT` and `RUN_TEXT` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The woman's high detail is switched off and the film ends | partial 60% | `SET_HIGH_GRAPHICS_DETAIL` and the film's end work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## The piper's round (stealing children)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He starts at the back of his cave, plays his summoning animation, the cave opens and he walks out slowly; the cave shuts behind him | partial | the clips, `MOVE_GAME_THING` and the cave sounds work; the cave's open state is set (`SET_OPEN_CLOSE`, `src/CHLApi.cpp:6941`); not checked in game |
| He walks to a piping spot near the crèche, faces the crèche, dances on the spot and his piping tune plays from him; a dance ring of up to 30 seconds is set up around him | partial | the walk, `SET_FOCUS`, the clip and `ATTACH_MUSIC` (the tune) work; `DANCE_CREATE` makes the dance ring (not checked in game) |
| He looks for a Norse child within 100 of him that no script is using; the child leaves the village and joins his flock (following him) | pending | `CALL_IN_NEAR` is ported (a Norse child within 100, not held by a script), but the scene is not checked in play |
| If the hand picks the child up on its way, he loses it and plays a disappointed animation | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| When the child reaches him (within 10) it joins his dance and he spins twice; he then waits 10 seconds before looking for the next one (3 seconds if none was found) | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| He leaves for his cave once five children are dancing (after another 15 seconds), or, after ten minutes from the quest's start, as soon as two are dancing | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| He dances the children up to his cave entrance, spins there for at least 15 seconds until they are all close, summons, and the cave opens | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| One by one the children walk to the back of the cave and fade away; each adds one to the count shut in the cave | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| He walks in after them, the cave tune plays from him, the cave shuts and 10 seconds later the round begins again | pending | not checked in play: `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) and `CALL_IN_NEAR`, which finds the child, are ported |
| The children in the cave cry, muffled while the cave is shut and normally while it is open, more often the more children there are | partial | the cries play at random (`PLAY_SOUND_EFFECT`), muffled while the cave is shut (`SET_OPEN_CLOSE` is real; not checked in play); only for the five pretend children |
| Children following him laugh now and then, more often the more he has | pending | he never has followers |
| Clicking the piper makes him say "Prod me. I don't care. I won't believe in you." at most once every 15 seconds | partial | `GAME_THING_CLICKED`, `CLEAR_CLICKED_OBJECT`, `GAME_PLAY_SAY_SOUND_EFFECT` and the timer work; not checked in game |
| Hurting him does nothing: his health is put back and he says one of "You can't hurt me.", "I'm invulnerable to you.", "I feel no pain." or "Do your worst. I won't be hurt.", at most once every 15 seconds | pending | his health is read (`GET_PROPERTY`, `src/CHLApi.cpp:1596`) and set back (`SET_PROPERTY`, `src/CHLApi.cpp:2029`) now; not checked in play |
| The crèche woman cries (one of seven sobs) when the camera is within 10 of her, every so often | pending | the test reads her health, real now (`src/CHLApi.cpp:1596`); not checked in play |

## Catching him (what the player must do)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player's creature comes within 30 of him, or is leashed to him, his round stops: the dance is broken up and the children following him are let go (a child cheers "Hurrah! Saved by our god!" if any were with him) | partial | the distance to the player's creature, `FLOCK_DETACH` and `FLOCK_DISBAND` work; the dance (`DANCE_CREATE`) and the child's cheer depend on children he never has |
| He then runs (faster than normal) to his cave shouting "Get away from me you big monster!", the cave opens for him and shuts behind him; if the creature was leashed to him the leash is let go | partial | `MOVE_GAME_THING`, speed, `GAME_PLAY_SAY_SOUND_EFFECT`, `IS_LEASHED_TO_OBJECT` and `TOGGLE_LEASH` work; the cave's open state is set (`SET_OPEN_CLOSE`, `src/CHLApi.cpp:6941`); not checked in game |
| From inside: "Your Creature won't catch me. I'm too fast." The evil advisor: "Tie him up Boss. He's getting on my nerves." The good advisor: "He's right. And he won't get away when he's Leashed to our Creature." | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| While hiding he taunts every 25 seconds: "I know you're out there.", "Leave me alone!", "I'm not coming out!", "Go away.", "Wait all you like. I'm staying in here." or "I'm staying put. So there." | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| If the creature goes more than 30 away (and isn't leashed to him), he comes out and starts collecting children again | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| The way to catch him is to tie the creature's leash to him and drag him more than 10 from the cave mouth | partial | the leash on a villager and `IS_LEASHED_TO_OBJECT` work (`LeashSystem`); not checked in game |
| Leashed and dragged out, he panics ("Help! I'm having a panic attack!"), his music stops and the creature walks up to him | partial | `DETACH_MUSIC`, `GAME_PLAY_SAY_SOUND_EFFECT` and `MOVE_GAME_THING` on the piper work; `MOVE_GAME_THING` on a creature now walks it there under the script's control (`CreatureMindSystem::MoveForScript`); not checked in play |
| The leash only holds for 20 seconds: then it is let go and he says "You can't keep me tied up for long. I'm an expert at slipping away." and runs back to his cave | partial | the timer and `TOGGLE_LEASH` work; not checked in game |
| If the creature gets within 15 of him while leashed, it grabs him | partial | the distance test works; the grab itself does not (next rows) |

## The creature holds him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: he can now be picked up and hurt; he screams "Arrgghh!", the leash is let go, the creature puts down whatever it holds and is made to pick the piper up | pending | `CREATURE_DO_ACTION` is real (`src/CHLApi.cpp:3617`, the row forced as a plan by its name), but rows 67 PutDown and 64 HoldObject have no executor: it logs "not implemented" and the creature never picks him up (the blocker above) |
| If the creature hasn't got him in its hand within 10 seconds, the catch fails and he runs back to his cave | partial 60% | the timer and the fall-back run work, and in our tree the catch always fails this way, since the creature has no executor for holding him (`IN_CREATURE_HAND` answers as the original since 2026-10-21, not checked in play); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The first time: "Ah! You got me! Let me live! Don't eat me!" — "If you spare me and take me to my cave I'll release the children!" — "I only wanted a family. But the Village women said I was too ugly." | pending | never reached: the creature has not been seen to hold him (`IN_CREATURE_HAND` answers as the original since 2026-10-21; the creature form of `GET_OBJECT_HELD` answers) |
| The evil advisor: "Oh boo hoo. Let's turn him into a Creature snack." The good advisor: "But if we take him to the cave, we'll save the little ones!" | pending | never reached: the creature has not been seen to hold him (`IN_CREATURE_HAND` answers as the original since 2026-10-21; the creature form of `GET_OBJECT_HELD` answers) |
| Later catches: "Ah. You've got me again." then only "If you spare me and take me to my cave I'll release the children!" | pending | never reached: the creature has not been seen to hold him (`IN_CREATURE_HAND` answers as the original since 2026-10-21; the creature form of `GET_OBJECT_HELD` answers) |
| The creature is then given back to the player, still holding him; the player decides, through the creature, what happens | pending | never reached: the creature has not been seen to hold him (`IN_CREATURE_HAND` answers as the original since 2026-10-21; the creature form of `GET_OBJECT_HELD` answers) |
| When he leaves the creature's hand he becomes unpickable and unhurtable again | pending | never reached: the creature has not been seen to hold him (`IN_CREATURE_HAND` answers as the original since 2026-10-21; the creature form of `GET_OBJECT_HELD` answers) |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good: put down (or thrown, once he lands) within 35 of the inside of his cave, a film: "Thanks for letting me go." — "I've released the children." | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| The cave opens and every child counted in the cave comes out as a random Norse villager aged 3 to 11, joins the village and walks home in a flock at a child's pace; the flock breaks up after 30 seconds | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| He goes on: "From now on I'll be a proper, normal Villager." — "Spreading belief of you and trying to forget how bad I've been." | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| He joins the Norse village as an ordinary villager: he can be picked up, moved and killed from now on | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| Set down anywhere else, he runs back to his cave: "Ha! I'm nowhere near my cave. And you set me free!" | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| Evil: if the creature eats him (he stops existing while caught), a film: the evil advisor "Hey, wow. I was going to suggest that! Yummy." and the good advisor "That's extreme bad taste if you ask me." | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| Evil: if he lands in the sea and drowns, he fades away and the evil advisor says "Evil. I like that in a god. So I approve of your actions, Boss." | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| The children in the cave stay shut in after the evil endings | pending | never reached: the creature never holds him (`CREATE_RANDOM_VILLAGER_OF_TRIBE`, which frees the children, is ported) |
| If the crèche woman dies, a film: the camera flies to her body over 3 seconds; the evil advisor points at her: "That's one crèche leader we don't have to worry about!" — the record moves to 50% — the good advisor: "This is just too appalling for words." The quest goes on | pending | her health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`), so this no longer runs when the introduction ends; `UPDATE_SNAPSHOT` is real; not checked in play |
| If the crèche house is damaged (the first time only), the woman (if alive) faces the camera, waves for attention and says "Please spare the Creche! You'll hurt the children!", then wanders again 2 seconds later | pending | the house's health is read now (`src/CHLApi.cpp:1596`: 1 while undamaged); not checked in play |
| If she is already dead when the crèche is damaged, the evil advisor instead: "Good thing the Creche woman's dead. She'd give you an earful for that!" | pending | follows the same health tests, real now; not checked in play |
| Failure: once 30 real children are shut in the cave (35 counting the pretend five), a film with the second generic music: the evil advisor "The piper's lured thirty children to their deaths. He's going on vacation now. My kinda guy." and the good advisor "This is a complete disaster. Thirty little mites! Gone!" | pending | not checked in play: `CALL_IN_NEAR`, which takes the children, is ported, and so is the cave's `SET_OPEN_CLOSE` (`src/CHLApi.cpp:6941`) |
| The failure records the quest as finished with alignment −0.8 and gives no reward | pending | `UPDATE_SNAPSHOT` is real, and the failure never comes |
| The quest can't be abandoned; if it is started but unfinished when Nemesis's storm comes, it is closed (recorded as finished with no alignment) and its scripts are stopped | pending | the storm (`TheStorm`) is skipped with the only tutorial answer that starts this quest; `STOP_SCRIPT` works |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good ending: a miracle dispenser is built beside the crèche; it gives heal if the player hasn't been given heal by a quest before (this or the Ogre's reward), otherwise a heal power-up | pending | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| Good ending: recorded as fully successful, alignment +1 | pending | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| Evil endings: a lightning miracle dispenser beside the crèche, recorded as fully successful with alignment −0.3 | pending | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| The dispenser reward plays the reward sting and flies the camera to it; for the game's first dispenser the evil advisor explains it ("This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." — "Click on the signpost for more info.") and a signpost and a "Miracles hidden all over Eden" did-you-know are placed; later ones get "Nice. Another of those cool Miracle Dispensers." Then the dispenser's own help lines | pending | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| The dispenser is given a refill time of 0 seconds (what that means for its charging is undetermined) | pending | never reached: no ending is reachable; the dispenser reward script itself works for other quests |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the good ending the piper lives on in the Norse village as an ordinary villager and the freed children grow up there | pending | never reached |
| The cave sounds, the piper's round and the crèche checks stop once the quest is finished; the land is told the quest is done, which can end the guide's nap early | pending | never reached |
| The crèche woman stays as a villager of the village | pending | never reached |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second generic script music for the introduction and the failure | partial | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio`); the failure never comes |
| The piper's tune while he pipes and his cave tune while he is in the cave, played from him | partial | `ATTACH_MUSIC` and `DETACH_MUSIC` work and the tune plays from him at his piping spot; not checked in game |
| The cave door's slide and slam sounds as it opens and shuts | partial | `PLAY_SOUND_EFFECT` plays them; `SET_OPEN_CLOSE` sets the cave's open state (`src/CHLApi.cpp:6941`); not checked in play |
| Children's cries (muffled or clear), children's laughs and the crèche woman's sobs | partial | the cave cries and her sobs play (`PLAY_SOUND_EFFECT`); no laughs (he never has children); her sobs follow her health, read now (`src/CHLApi.cpp:1596`); not checked in play |
| The piper's lines while running, hiding, prodded or hurt are spoken as sounds from him without the dialogue box | partial | `GAME_PLAY_SAY_SOUND_EFFECT` works for the running, hiding and prodded lines; the hurt lines follow his health, read now (`src/CHLApi.cpp:1596`); not checked in play |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the creature can catch the piper: he flees from it, and only a creature leashed to him and close by can grab him | partial 40% | the leash works (`LeashSystem`) and he flees; the grab does not: `CREATURE_DO_ACTION` is real but its rows 64 HoldObject and 67 PutDown have no executor (the blocker above) |
| The creature's choice afterwards (carry him to the cave, drop him elsewhere, throw him in the sea or eat him) decides the ending; what the creature learns from eating him is the creature mind's own business | pending | the creature never holds him |

## Script quirks and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The thirty-children failure checks a "piper dead" flag that nothing ever sets, so it always applies | pending | never reached |
| The freed children include the five the game pretended were in the cave at the start | pending | never reached |
| The good ending and the eaten ending have no camera work ("cinema needed" was never filled in): the lines play in widescreen with the camera where it was | pending | never reached |
| The general challenge line "Your attention is required here." is the only notice; the piper's own "This isn't just a normal tribesperson." line is the villager question text for his kind (where it shows is undetermined) | pending | the challenge line plays (shared notice script); the villager question text for his kind is not shown in our tree |
