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
scripts it runs (notice, reward, did-you-know) call, 13 only log "not implemented" in `src/CHLApi.cpp` (among them the
held-object read, the creature's actions, a dance and making a random villager). The 80 that work include the camera
set/read commands, widescreen, music start and stop, distance and position reads, random numbers, the game time,
stopping a script, the fire/indestructible/pick-up/moveable flags, the leash question and the leash key, and making
rocks, mobile statics, villagers and dispensers. Every row below is todo unless the notes say otherwise.

**Progress: 0/85 done, 40 partial — 24%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest (in the background) only after the guide's food lesson, when the player has been sent to impress the Aztec village; with the creature training skipped, the guide's lessons are left out and it starts right after the Saviour's script | partial | only with the fourth tutorial answer (creature training skipped): `LandControl1` starts it right after the Saviour's script; with the other answers the land control script never gets there |
| A silver challenge scroll is put over the crèche house of the Norse village | partial | `CREATE_HIGHLIGHT` works (`src/ECS/ScriptHighlight.cpp`); not checked in game |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out, points at it and says "Your attention is required here." — at most once every 30 seconds, and only when no film is playing | partial | the shared notice script runs on working natives (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, `SPIRIT_EJECT`, `SPIRIT_POINT_POS`, `RUN_TEXT`); not checked in game |
| Clicking the scroll or the crèche house itself starts the quest; the scroll is then switched to active | partial | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight work; not checked in game |
| While the guide sleeps between lessons, the good advisor may fly the camera high over the land to the crèche to suggest this scroll ("You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it."), after the Missionaries and the Lost Flock; once this scroll is finished the guide's nap may end early | todo | the guide's lessons are skipped with the only tutorial answer that starts this quest, so this never happens |

## Setting the scene (on clicking)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Norse housewife, the crèche woman, is made at the crèche, with a small attraction around her that draws villagers | partial | `CREATE` makes villagers (`VillagerArchetype`) and `INFLUENCE_OBJECT` works (`src/Magic/Script/CHLInfluence.cpp`); not checked in game |
| The piper is made at his cave (a villager of his own kind, with his own model and dance animations) and leads a little flock of his own | partial | `CREATE` makes him as a villager and `FLOCK_CREATE` / `FLOCK_ATTACH` work (`src/ECS/Flocks.cpp`); his dance clips are not checked |
| The piper can't be picked up by the hand, can't be hurt or killed, is not hurt by fire and can't be set on fire | partial | the four flag natives work on him (`SET_ID_PICKUPABLE`, `SET_INDESTRUCTABLE`, `SET_HURT_BY_FIRE`, `SET_SET_ON_FIRE`); not checked in game |
| Two little boys and two little girls (age 3) are made at the crèche and join the Norse village, so there are children for him to take | partial | `CREATE`, the age (`SET_PROPERTY` age on a villager) and `FLOCK_ATTACH` work; not checked in game |
| The game pretends five children are already shut in the cave from the start | partial | a script counter; nothing to port |
| The muffled crying of the children in the cave starts and the piper's round of collecting children begins, both before the introduction | partial | the cave cries (`PLAY_SOUND_EFFECT`) work; the round starts but stalls (see below) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen): the crèche woman is drawn in high detail and the second generic script music starts | partial | `SET_WIDESCREEN`, `SET_HIGH_GRAPHICS_DETAIL` and `START_MUSIC` work (`src/Audio`); not checked in game |
| The camera glides to the crèche over 7 seconds, following the woman as she walks out to a soapbox spot and paces between two places, then turns to face the camera | partial | `MOVE_CAMERA_POSITION`, `SET_FOCUS_FOLLOW`, `MOVE_GAME_THING` and `SET_FOCUS` on a villager work; not checked in game |
| She looks around as if for something (four times): "Oh no! Some of my poor children are lost! They'll need rescuing!" while the camera closes in on her over 5 seconds | partial | the clips (`SET_SCRIPT_ULONG` on a villager), `RUN_TEXT` and the camera work; not checked in game |
| She stands in despair (four times): "A stranger has been seen putting the children into trances." — the camera cuts to the piper and drifts up beside him | partial | the clips (`SET_SCRIPT_ULONG` on a villager), `RUN_TEXT` and the camera work; not checked in game |
| "He has them imprisoned in a secret place!" — the camera cuts to the cave and zooms in over 4 seconds while three muffled child cries play one after another from the cave | partial | the camera cut and the cries (`PLAY_SOUND_EFFECT`) work; not checked in game |
| Back on the woman, the camera drifts slowly (14 seconds) to her crying: "This stranger is to blame. It's got to be him." | partial | the camera, the clips, `RUN_TEXT` and the sound work; not checked in game |
| She mourns (four times): "But if he should die before we find the children, they could be lost forever. Oh, my babies!" and a woman's cry plays | partial | the camera, the clips, `RUN_TEXT` and the sound work; not checked in game |
| The dialogue box closes and the quest is recorded under the title "The Pied Piper" at 0% with the good advisor's reminder "We must find the person behind this child-stealing business." | todo | the dialogue closes (`GAME_CLOSE_DIALOGUE` works) but the record is `SNAPSHOT`, a stub |
| The camera returns over 3 seconds to where the player had it; the woman goes back to wandering near the crèche (within 6, pausing 4 to 20) and the music stops | partial | the camera return and `STOP_MUSIC` work; her wandering needs `SET_SCRIPT_STATE_POS` and `SET_SCRIPT_FLOAT`, both stubs |
| The good advisor: "We must act. Tiny lives are at stake." The evil advisor: "Tiny lives? Who cares? Anyway, let's move out! I've always wanted to say that." | partial | `SPIRIT_EJECT` and `RUN_TEXT` work; not checked in game |
| The woman's high detail is switched off and the film ends | partial | `SET_HIGH_GRAPHICS_DETAIL` and the film's end work; not checked in game |

## The piper's round (stealing children)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He starts at the back of his cave, plays his summoning animation, the cave opens and he walks out slowly; the cave shuts behind him | partial | the clips, `MOVE_GAME_THING` and the cave sounds work; the cave doors (`SET_OPEN_CLOSE`) are a stub |
| He walks to a piping spot near the crèche, faces the crèche, dances on the spot and his piping tune plays from him; a dance ring of up to 30 seconds is set up around him | partial | the walk, `SET_FOCUS`, the clip and `ATTACH_MUSIC` (the tune) work; `DANCE_CREATE` is a stub |
| He looks for a Norse child within 100 of him that no script is using; the child leaves the village and joins his flock (following him) | todo | `CALL_IN_NEAR` is a stub: he never finds a child, so he never takes one |
| If the hand picks the child up on its way, he loses it and plays a disappointed animation | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| When the child reaches him (within 10) it joins his dance and he spins twice; he then waits 10 seconds before looking for the next one (3 seconds if none was found) | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| He leaves for his cave once five children are dancing (after another 15 seconds), or, after ten minutes from the quest's start, as soon as two are dancing | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| He dances the children up to his cave entrance, spins there for at least 15 seconds until they are all close, summons, and the cave opens | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| One by one the children walk to the back of the cave and fade away; each adds one to the count shut in the cave | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| He walks in after them, the cave tune plays from him, the cave shuts and 10 seconds later the round begins again | todo | never happens: he never finds a child (`CALL_IN_NEAR` is a stub); `DANCE_CREATE` and `SET_OPEN_CLOSE` are stubs too |
| The children in the cave cry, muffled while the cave is shut and normally while it is open, more often the more children there are | partial | the cries play at random (`PLAY_SOUND_EFFECT`), muffled since the cave never opens (`SET_OPEN_CLOSE` is a stub); only for the five pretend children |
| Children following him laugh now and then, more often the more he has | todo | he never has followers |
| Clicking the piper makes him say "Prod me. I don't care. I won't believe in you." at most once every 15 seconds | partial | `GAME_THING_CLICKED`, `CLEAR_CLICKED_OBJECT`, `GAME_PLAY_SAY_SOUND_EFFECT` and the timer work; not checked in game |
| Hurting him does nothing: his health is put back and he says one of "You can't hurt me.", "I'm invulnerable to you.", "I feel no pain." or "Do your worst. I won't be hurt.", at most once every 15 seconds | todo | `GET_PROPERTY` of health is not ported (answers 0), so he complains as hurt from the start; health cannot be set back |
| The crèche woman cries (one of seven sobs) when the camera is within 10 of her, every so often | todo | the test reads her health, which answers 0, so she counts as dead |

## Catching him (what the player must do)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player's creature comes within 30 of him, or is leashed to him, his round stops: the dance is broken up and the children following him are let go (a child cheers "Hurrah! Saved by our god!" if any were with him) | partial | the distance to the player's creature, `FLOCK_DETACH` and `FLOCK_DISBAND` work; the dance (`DANCE_CREATE`) and the child's cheer depend on children he never has |
| He then runs (faster than normal) to his cave shouting "Get away from me you big monster!", the cave opens for him and shuts behind him; if the creature was leashed to him the leash is let go | partial | `MOVE_GAME_THING`, speed, `GAME_PLAY_SAY_SOUND_EFFECT`, `IS_LEASHED_TO_OBJECT` and `TOGGLE_LEASH` work; the cave doors are a stub |
| From inside: "Your Creature won't catch me. I'm too fast." The evil advisor: "Tie him up Boss. He's getting on my nerves." The good advisor: "He's right. And he won't get away when he's Leashed to our Creature." | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| While hiding he taunts every 25 seconds: "I know you're out there.", "Leave me alone!", "I'm not coming out!", "Go away.", "Wait all you like. I'm staying in here." or "I'm staying put. So there." | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| If the creature goes more than 30 away (and isn't leashed to him), he comes out and starts collecting children again | partial | the advisors, the timer, `GAME_PLAY_SAY_SOUND_EFFECT` and the distance test work; not checked in game |
| The way to catch him is to tie the creature's leash to him and drag him more than 10 from the cave mouth | partial | the leash on a villager and `IS_LEASHED_TO_OBJECT` work (`LeashSystem`); not checked in game |
| Leashed and dragged out, he panics ("Help! I'm having a panic attack!"), his music stops and the creature walks up to him | partial | `DETACH_MUSIC`, `GAME_PLAY_SAY_SOUND_EFFECT` and `MOVE_GAME_THING` on the piper work; the creature walking up to him is not driven by the script in our tree |
| The leash only holds for 20 seconds: then it is let go and he says "You can't keep me tied up for long. I'm an expert at slipping away." and runs back to his cave | partial | the timer and `TOGGLE_LEASH` work; not checked in game |
| If the creature gets within 15 of him while leashed, it grabs him | partial | the distance test works; the grab itself does not (next rows) |

## The creature holds him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: he can now be picked up and hurt; he screams "Arrgghh!", the leash is let go, the creature puts down whatever it holds and is made to pick the piper up | todo | `CREATURE_DO_ACTION` (put down, pick up) is a stub |
| If the creature hasn't got him in its hand within 10 seconds, the catch fails and he runs back to his cave | partial | the timer and the fall-back run work, and in our tree the catch always fails this way (`IN_CREATURE_HAND` is a stub) |
| The first time: "Ah! You got me! Let me live! Don't eat me!" — "If you spare me and take me to my cave I'll release the children!" — "I only wanted a family. But the Village women said I was too ugly." | todo | never reached: the creature never holds him (`CREATURE_DO_ACTION`, `IN_CREATURE_HAND` and `GET_OBJECT_HELD` are stubs) |
| The evil advisor: "Oh boo hoo. Let's turn him into a Creature snack." The good advisor: "But if we take him to the cave, we'll save the little ones!" | todo | never reached: the creature never holds him (`CREATURE_DO_ACTION`, `IN_CREATURE_HAND` and `GET_OBJECT_HELD` are stubs) |
| Later catches: "Ah. You've got me again." then only "If you spare me and take me to my cave I'll release the children!" | todo | never reached: the creature never holds him (`CREATURE_DO_ACTION`, `IN_CREATURE_HAND` and `GET_OBJECT_HELD` are stubs) |
| The creature is then given back to the player, still holding him; the player decides, through the creature, what happens | todo | never reached: the creature never holds him (`CREATURE_DO_ACTION`, `IN_CREATURE_HAND` and `GET_OBJECT_HELD` are stubs) |
| When he leaves the creature's hand he becomes unpickable and unhurtable again | todo | never reached: the creature never holds him (`CREATURE_DO_ACTION`, `IN_CREATURE_HAND` and `GET_OBJECT_HELD` are stubs) |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good: put down (or thrown, once he lands) within 35 of the inside of his cave, a film: "Thanks for letting me go." — "I've released the children." | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| The cave opens and every child counted in the cave comes out as a random Norse villager aged 3 to 11, joins the village and walks home in a flock at a child's pace; the flock breaks up after 30 seconds | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| He goes on: "From now on I'll be a proper, normal Villager." — "Spreading belief of you and trying to forget how bad I've been." | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| He joins the Norse village as an ordinary villager: he can be picked up, moved and killed from now on | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| Set down anywhere else, he runs back to his cave: "Ha! I'm nowhere near my cave. And you set me free!" | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| Evil: if the creature eats him (he stops existing while caught), a film: the evil advisor "Hey, wow. I was going to suggest that! Yummy." and the good advisor "That's extreme bad taste if you ask me." | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| Evil: if he lands in the sea and drowns, he fades away and the evil advisor says "Evil. I like that in a god. So I approve of your actions, Boss." | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| The children in the cave stay shut in after the evil endings | todo | never reached: the creature never holds him; freeing the children also needs `CREATE_RANDOM_VILLAGER_OF_TRIBE`, a stub |
| If the crèche woman dies, a film: the camera flies to her body over 3 seconds; the evil advisor points at her: "That's one crèche leader we don't have to worry about!" — the record moves to 50% — the good advisor: "This is just too appalling for words." The quest goes on | todo | her health answers 0 (`GET_PROPERTY`), so this runs as soon as the introduction ends; `UPDATE_SNAPSHOT` is a stub |
| If the crèche house is damaged (the first time only), the woman (if alive) faces the camera, waves for attention and says "Please spare the Creche! You'll hurt the children!", then wanders again 2 seconds later | todo | the house's health answers 0, so the script takes it as damaged at once |
| If she is already dead when the crèche is damaged, the evil advisor instead: "Good thing the Creche woman's dead. She'd give you an earful for that!" | todo | follows the same wrong health tests |
| Failure: once 30 real children are shut in the cave (35 counting the pretend five), a film with the second generic music: the evil advisor "The piper's lured thirty children to their deaths. He's going on vacation now. My kinda guy." and the good advisor "This is a complete disaster. Thirty little mites! Gone!" | todo | never: no child is ever taken (`CALL_IN_NEAR` is a stub) |
| The failure records the quest as finished with alignment −0.8 and gives no reward | todo | `UPDATE_SNAPSHOT` is a stub, and the failure never comes |
| The quest can't be abandoned; if it is started but unfinished when Nemesis's storm comes, it is closed (recorded as finished with no alignment) and its scripts are stopped | todo | the storm (`TheStorm`) is skipped with the only tutorial answer that starts this quest; `STOP_SCRIPT` works |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good ending: a miracle dispenser is built beside the crèche; it gives heal if the player hasn't been given heal by a quest before (this or the Ogre's reward), otherwise a heal power-up | todo | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| Good ending: recorded as fully successful, alignment +1 | todo | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| Evil endings: a lightning miracle dispenser beside the crèche, recorded as fully successful with alignment −0.3 | todo | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| The dispenser reward plays the reward sting and flies the camera to it; for the game's first dispenser the evil advisor explains it ("This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." — "Click on the signpost for more info.") and a signpost and a "Miracles hidden all over Eden" did-you-know are placed; later ones get "Nice. Another of those cool Miracle Dispensers." Then the dispenser's own help lines | todo | never reached: no ending is reachable; the dispenser reward script itself works for other quests |
| The dispenser is given a refill time of 0 seconds (what that means for its charging is undetermined) | todo | never reached: no ending is reachable; the dispenser reward script itself works for other quests |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the good ending the piper lives on in the Norse village as an ordinary villager and the freed children grow up there | todo | never reached |
| The cave sounds, the piper's round and the crèche checks stop once the quest is finished; the land is told the quest is done, which can end the guide's nap early | todo | never reached |
| The crèche woman stays as a villager of the village | todo | never reached |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second generic script music for the introduction and the failure | partial | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio`); the failure never comes |
| The piper's tune while he pipes and his cave tune while he is in the cave, played from him | partial | `ATTACH_MUSIC` and `DETACH_MUSIC` work and the tune plays from him at his piping spot; not checked in game |
| The cave door's slide and slam sounds as it opens and shuts | partial | `PLAY_SOUND_EFFECT` plays them; the doors themselves do not move (`SET_OPEN_CLOSE`) |
| Children's cries (muffled or clear), children's laughs and the crèche woman's sobs | partial | the cave cries and her sobs play (`PLAY_SOUND_EFFECT`); no laughs (he never has children); her sobs follow a wrong health test |
| The piper's lines while running, hiding, prodded or hurt are spoken as sounds from him without the dialogue box | partial | `GAME_PLAY_SAY_SOUND_EFFECT` works for the running, hiding and prodded lines; the hurt lines follow a wrong health test |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the creature can catch the piper: he flees from it, and only a creature leashed to him and close by can grab him | partial | the leash works (`LeashSystem`) and he flees; the grab does not (`CREATURE_DO_ACTION` is a stub) |
| The creature's choice afterwards (carry him to the cave, drop him elsewhere, throw him in the sea or eat him) decides the ending; what the creature learns from eating him is the creature mind's own business | todo | the creature never holds him |

## Script quirks and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The thirty-children failure checks a "piper dead" flag that nothing ever sets, so it always applies | todo | never reached |
| The freed children include the five the game pretended were in the cave at the start | todo | never reached |
| The good ending and the eaten ending have no camera work ("cinema needed" was never filled in): the lines play in widescreen with the camera where it was | todo | never reached |
| Cut lines in the text table, never used: "You killed the piper!", "You've knocked away the rock. Here come the little brats.", "Oh this is enchanting!", "Humph.", "Nice bit of assassination there, Boss.", "And no children hurt. Wonderful!", "Scratch one piper.", "But what about the children? You haven't saved them!", "So many children are missing." — from an older design where the cave was sealed by a rock to knock away and the piper could be killed | n/a | text only |
| An "anti-influence" around the piper (so villagers keep away) was written and commented out | n/a | |
| The general challenge line "Your attention is required here." is the only notice; the piper's own "This isn't just a normal tribesperson." line is the villager question text for his kind (where it shows is undetermined) | todo | the challenge line plays (shared notice script); the villager question text for his kind is not shown in our tree |
