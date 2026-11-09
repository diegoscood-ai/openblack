# The Creature's Learning

The first half of the first land's creature story: once the creature is chosen, Sable the creature trainer comes out of
the temple four times to teach the player to look after it — its new home and stroking, feeding it, slapping it, the
Leash of Learning (pulling it and making it pick up a cow) and tying the leash to a tree, where she also shows the
Leashes of Aggression and Compassion before saying farewell. The same gold scroll then carries on with the big creature
that wanders the valley; that half is in [creature_guide.md](../creature_guide.md).

**Land:** 1 · **Giver:** Sable the creature trainer, at the creature pen by the temple · **Script:** CreatureDevelopment (CreatureDevSeeHome, CreatureDevLearnToEat, CreatureDevPunishment, CreatureDevLeashIntro, CreatureDevLeashAttachToHouse) · **Reward:** none (the lessons hand over the three leashes) · **Repeatable:** no

Sources: the lessons' script source (checked against the shipped compiled program), the land's control script and the
game's text table. openblack is judged on this tree: a run on 2026-10-22 (Land 1, SkipBox answer 0) reached Choose Your Creature's
first gate stone, so the lessons are not yet reached without skipping (see
[../../scripts/land1_script.md](../../scripts/land1_script.md)). No command the lessons use only logs "not implemented" in
`src/CHLApi.cpp`; the creature's actions are forced by their row's name (`CREATURE_DO_ACTION`, `src/CHLApi.cpp:3617`),
and of the rows used, 168 PointAtCamera, 82 LookAtCamera, 161 BeIdle and 15 Hurl have executors
(`src/Creature/CreaturePlanActions.cpp`) while 17 SleepAtHome has none. The desires, the held object, the slap and stroke
reads, the creature's energy and exhaustion and the challenge log (`SNAPSHOT`, `UPDATE_SNAPSHOT`) are real. The rest work: making the trainer, the scroll, the dialogue, the camera moves, the leash questions and
the leash-granting developer functions, drawing the leash, setting the game time, music, fades and the
moveable/pick-up/indestructible flags. Rows are pending unless the notes say otherwise.

**Progress: 3/136 done, 0 chosen, 5 partial, 0 inferred — 3%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lessons are a gold scroll (a story highlight) titled "The Creature's Learning", the same title the guide's chapters use after them | pending | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); the challenge record is real (`Snapshot`, `UpdateSnapshot`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| They start once the creature is chosen ([Choose Your Creature](choose_your_creature.md)) and the breeder, explorers and the guide's wandering have been started | pending | not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones; a run of 2026-10-22 reached only its first gate stone (see [Choose Your Creature](choose_your_creature.md)) |
| The land's control script runs them one after another, each waiting for the last: the new home, eating, punishment, then the leash introduction, then tying the leash | pending | `LandControl1` runs them in order (plain script); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Between lessons, other quests start: after punishment the immersion mushrooms and the singing stones; after the leash introduction the hermit; after tying the leash the lost flock is wanted and the saviour waits for the creature to grow | partial 50% | `LandControl1` starts them in order; each then needs its own natives. See [the_immersion_mushrooms.md](../silver_scrolls/the_immersion_mushrooms.md), [the_singing_stones.md](../silver_scrolls/the_singing_stones.md), [the_hermit.md](../silver_scrolls/the_hermit.md), [the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [the_saviour.md](../silver_scrolls/the_saviour.md); re-measured 2026-10-21: started in order; each then needs its own natives |
| What comes next: the guide's meeting starts straight after the last lesson | pending | the guide's meeting needs a script-made guide creature, which `CREATE` makes now; not checked in play; see [creature_guide.md](../creature_guide.md) |
| The scroll's progress mark rises by lesson: 0 for the new home, 0.08 eating, 0.15 punishment, 0.23 the leash, 0.31 tying the leash; the guide's chapters carry on from 0.5 | pending | `Snapshot` and `UpdateSnapshot` are real |
| A quiet pause follows each lesson before the next trainer visit: 31 seconds after the new home, 61 after eating, 61 after punishment, 31 after the leash introduction | pending | plain script waits; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The creature's own development script is switched on once, as the first lesson starts, and switched off at the end of every lesson; the later lessons don't switch it on again | pending | script ips: on at 6046 (the only "on" in the lessons), off at 6722, 7258, 7870, 9167 and 9884; `CreatureInDevScript` is real (`src/CHLApi.cpp:6647`, `Creature::inDevScript`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A file loaded with the lessons marks the temple's entrance, where the trainer comes and goes | pending | the marker comes from the script's own `CREATE` calls (real); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Skipping the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game that skips the creature guide still runs the new-home step, but only as a wake-up camera at the pen with a two-second fade in; no trainer, no scroll | done | with the third and fourth SkipBox answers `CreatureDevSeeHome` takes its skip branch in our game: dawn (`SetGameTime`), the camera snap onto the pen and `SetFadeIn` (2 s), all real; see [land-change-and-saving.md](../../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| The creature is put in its pen and that becomes its home | partial | `SetPosition` and `SetCreatureHome` are real (`LeashSystemInterface::SetHome`) on the player's creature, which exists only with the fourth answer (`LoadMyCreature`); `SET_FOCUS` turns a creature now (`src/CHLApi.cpp:2816`) |
| The other lessons are skipped: the creature is made fully mature at once, given the Leash of Learning and the other two leashes, its development script switched off and released | done | `LandControl1` sets the stage (`SetCreatureDevStage` 13), grants the three leashes (`DevFunction` 2 and 3), clears `CreatureInDevScript` and releases it (`ReleaseFromScript`); see [players-creature.md](../../../../creature/players-creature.md#the-players-creature). Only with the fourth answer is there a creature |
| The quests that start between lessons still start as usual | partial 50% | `LandControl1` starts them; each stops on its own missing natives; re-measured 2026-10-21: started; each stops on its own missing natives |

## How the trainer calls the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The trainer (a special villager) walks slowly (speed 0.3) out of the temple to her spot by the pen; while teaching she can't be moved, picked up or hurt | pending | `CREATE` makes the trainer (a villager), `MoveGameThing` on villagers and the three flags are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A gold scroll stands over the pen, raised six above the ground | pending | `CreateHighlight` and the height through `SetProperty` (YPos) are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Until it is clicked she faces the camera and beckons, then points and talks, in turn | pending | villager animations and `SetFocus` on villagers are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Every 40 seconds, or every 12 when the camera is more than 30 from the scroll, the player's creature points the scroll out | pending | `DEV_FUNCTION` 6 (point out the scroll) is handled (`src/ECS/PlayerCreature.cpp:1181`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| When the camera is near the scroll (within 200 for the eating lesson, 100 for the others) and it is on screen, the evil advisor steps out every 30 seconds, points at it and says "Hey. Sable the Creature Trainer is trying to attract our attention." | pending | the notify script calls real natives (`SpiritEject`, `RunText`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Clicking the scroll or the trainer starts the lesson | pending | `GameThingClicked` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The first lesson needs no click: it starts as a cut scene | pending | plain script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Reminders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the active scroll again repeats its reminder; an advisor line is said by the advisor who owns it, stepping out to say it, and a trainer line by the trainer's voice | partial | the shared reminder calls real natives; the reminder texts come from the log, real now, not yet checked in this quest |
| While the player must do something with the creature, a nag repeats its line every 30 seconds — only while the camera is within 150 of the creature, the creature is on screen and the player isn't holding it; the good advisor steps out, points at it, says it and goes home | pending | real field-of-view, distance and advisor natives; the "isn't holding it" test is `IS_LOCKED_INTERACTION` (script ip 5771), real (`src/CHLApi.cpp:7432`), not the held read; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Lesson 1: the new home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is set to just before sunrise and game time runs | pending | `SetGameTime` and `GameTimeOnOff` are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A did-you-know sign by the pen: "This is your Creature's home. He feels safe here and when he rests he'll grow in size faster…" | pending | `DidYouKnow` and `CreateHighlight` are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones. See [../../creature/home_and_pen.md](../../creature/home_and_pen.md) |
| The creature is put at its first stage of growing up | pending | `SetCreatureDevStage` is real on the player's creature; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones. See [../../creature/development_phases.md](../../creature/development_phases.md) |
| The creature is put in the pen, which becomes its home, and it faces the camera | pending | `SetCreatureHome` is real, and `SET_FOCUS` and `MOVE_GAME_THING` act on a creature now (`src/CHLApi.cpp:2816`, `:2704`); not yet reached in play |
| A cut scene opens with the guide's music; the creature is made exhausted and put to sleep | pending | `StartMusic` is real and the exhaustion is set (`SET_PROPERTY`, `src/CHLApi.cpp:2197`); the sleep is `CREATURE_DO_ACTION` 17 SleepAtHome (script ip 6124), which has no executor and logs "not implemented" (`src/CHLApi.cpp:3617`): the creature does not lie down to sleep; no wait follows it, so the lesson goes on |
| After six seconds the picture fades in over five seconds on a sunrise view of the pen, which pans up over seven seconds | pending | `SetFadeIn` and the camera moves are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The camera cuts to the pen as the creature wakes and moves in on the trainer | pending | the creature's wake-up is a creature script state (`SET_SCRIPT_STATE` on a creature, real, `src/CHLApi.cpp:1412`); not yet reached in play |
| The scroll is logged at 0 with the reminder "Hold down the Action Button over the Creature to zoom in on him." (trainer) | pending | `UpdateSnapshot` is real |
| The creature points at the camera; the trainer: "This is your Creature's new home." The creature yawns tiredly and loses its tiredness | pending | `CREATURE_DO_ACTION` 168 PointAtCamera has an executor (`src/Creature/CreaturePlanActions.cpp:173`), the yawn is a script clip (`SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, `src/CHLApi.cpp:1528`, `:1412`) and the exhaustion is set (`src/CHLApi.cpp:2197`); the trainer's line is real; not yet reached in play |
| The camera turns to the creature, which looks frightened; trainer: "He's small at the moment but look after him and one day he'll be big and powerful." | pending | the creature's clips are played by script through `SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, real (`src/CHLApi.cpp:1528`, `:1412`), while `OVERRIDE_STATE_ANIMATION` on a creature still logs "not implemented" (`src/CHLApi.cpp:3475`); which of them this step uses was not checked here; not yet reached in play |
| It cries; the camera goes back to the trainer: "He isn't comfortable in his new home yet. In fact he's a little frightened." — the creature looks frightened again | pending | the creature's clips are played by script through `SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, real (`src/CHLApi.cpp:1528`, `:1412`), while `OVERRIDE_STATE_ANIMATION` on a creature still logs "not implemented" (`src/CHLApi.cpp:3475`); which of them this step uses was not checked here; not yet reached in play |
| "You can stroke him to reassure him." The creature looks sad and the camera rises; the music stops | pending | the creature's clips are played by script through `SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, real (`src/CHLApi.cpp:1528`, `:1412`), while `OVERRIDE_STATE_ANIMATION` on a creature still logs "not implemented" (`src/CHLApi.cpp:3475`); which of them this step uses was not checked here; not yet reached in play |
| The reminder becomes "Click and hold the Action Button, then move your hand to stroke him." | pending | the reminder goes through the log, real now, not yet checked in this quest |
| The creature is left wanting only to be sad; the trainer says the same line again | pending | `SetCreatureOnlyDesire` is real |
| The good advisor nags "Let's click on the Creature so we can stroke him." | pending | the advisor nag is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The player must take hold of the creature (zoom in on it) and keep hold for two seconds; letting go sooner gets "Holy One, you're clicking and letting go too quickly." the first time and "I humbly suggest you click and hold the mouse button for longer." after that | pending | `IS_LOCKED_INTERACTION` (holding the creature) is real (`src/CHLApi.cpp:7432`); not yet reached in play |
| Then: "Now stroke him by holding the Action Button and moving your hand over him gently." and "Stroking tells him you approve of him and it makes him happy." | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The player has two minutes to stroke or slap; the good advisor nags "Click on the Creature and stroke him some more." | pending | `GET_INTERACTION_MAGNITUDE` (`src/CHLApi.cpp:6502`) gives the last slap or stroke the creature's mind took (`src/ECS/Systems/Implementations/CreatureMindSystem.cpp:1986`); not yet reached in play |
| A slap: the creature looks sad, the trainer is unimpressed: "Hey, remember he's only little."; a stroke: it looks happy, she is impressed: "Excellent. See how he likes that? He feels much better now."; neither: no comment, the lesson goes on | pending | the slap or stroke read (`GET_INTERACTION_MAGNITUDE`, `src/CHLApi.cpp:6502`) is real; not checked in play whether the hand's gestures give it a value; see [../../creature/learning_from_feedback.md](../../creature/learning_from_feedback.md) |
| The creature is let go; the trainer walks back to the temple: "I'll leave you two alone for a while. I'll return later to teach you more." | pending | `ReleaseFromScript`, villager moves and texts are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The scroll goes; she fades away at the temple entrance | pending | `ObjectDelete` of the scroll and the villager are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Lesson 2: learning to eat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature moves to the stage of taking things from the player and eating them, and its energy is set to nothing (starving) | pending | `SetCreatureDevStage` is real, and the energy is set (`SET_PROPERTY`, `src/CHLApi.cpp:2194`); not yet reached in play |
| The trainer comes out under a new scroll and calls the player as above | pending | real villager and highlight natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The scroll is logged at 0.08: "You need to give your Creature something to eat to move to the next stage." (good advisor) | pending | `UpdateSnapshot` is real |
| A cut scene: the creature walks to a spot in front of the pen; the camera moves in; it looks at the camera and acts hungry | pending | `MOVE_GAME_THING` walks a creature now (`src/CHLApi.cpp:2706`); not yet reached in play |
| Trainer: "Your Creature is getting hungry." then "I suggest you find him something to eat."; the camera turns to the creature, which acts hungry again | pending | the hungry animation is a script clip (`SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, `src/CHLApi.cpp:1528`, `:1412`), real; the lines are real; not yet reached in play |
| "Choose carefully though - some things are better for him than others."; the camera goes back to where the player had it | pending | real text and camera natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A did-you-know sign: "If you want your Creature to eat something, rub his tummy straight after you give it to him. He'll eat it even if he's not hungry." | pending | `DidYouKnow` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The reminder becomes "I suggest you go and get the Creature some food." (good advisor) | pending | the reminder goes through the log, real now, not yet checked in this quest |
| While the player looks for food, the advisors comment on what the hand is over (within 5.5): a pig — evil: "Hey. What about some meat? Pork chops? He'd love them!" (again after 3 minutes at most) | pending | `CheckForCommentOnFoodItems` uses real natives (hand position, `CallNear`, `RunText`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones. See [../../creature/feeding_and_thrown_things.md](../../creature/feeding_and_thrown_things.md) |
| A food store — good: "Now that sounds tasty. Corn from the fields." (every 190 seconds at most) | pending | real natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A cow — evil: "What about some beef steak? Meat-eaters are more aggressive, after all." (every 190 seconds at most) | pending | real natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A villager, only shortly after a pig or cow comment — evil: "Ooh. That would be deeply evil. Yeah." for a child under 11, else "Eating people? Yep. That's evil, all right." | pending | real natives; the age read is handled for villagers; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The village's storage pit (hand within 15.5) — good: "Why not take some grain from the Village Store to feed the Creature?" | pending | real natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Scripted objects are never commented on | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| As soon as the hand holds anything within 50 of the creature the comments stop; reminder "Give him the food by keeping your hand in front of him and holding the Action Button." and the good advisor says nearly the same line | done | `GetObjectHeld` reads the hand's held thing; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones (unverified against v1.42) |
| The creature is starved again and kept wanting only food, its energy held where it is, until it eats once — something alive, something after looking it over, something from the hand or from a magic food pile | pending | `SetCreatureOnlyDesire` and `GetActionCount` are real; see [../../creature/feeding_and_thrown_things.md](../../creature/feeding_and_thrown_things.md) |
| There is no time limit; whatever it eats is accepted, good or evil | pending | the eating test (`GET_ACTION_COUNT`, `src/CHLApi.cpp:6548`) is real; not checked in play whether feeding counts an eating action to its end |
| A cut scene: the good advisor points at it: "He seemed to like that."; the camera turns to the trainer: "Try feeding him some more. I'll be back later." | pending | real advisor, camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| She walks back into the temple and fades away; the scroll goes; the creature's energy is filled up | pending | the trainer leaves (real), and the creature's energy is set (`src/CHLApi.cpp:2194`); not yet reached in play |

## Lesson 3: punishment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature moves to the punishment stage; from here on slaps and strokes teach it lessons | pending | `SetCreatureDevStage` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones. See [../../creature/development_phases.md](../../creature/development_phases.md) |
| The trainer comes out under a new scroll; the scroll is logged at 0.15: "You must punish your Creature to move on. Click on him with the Action Button." (good advisor) | pending | `UpdateSnapshot` is real; the trainer and scroll are real |
| The creature walks to its spot; the cut scene moves the camera beside the pen and the creature looks at the camera | pending | moving and turning the creature by script are real (`MOVE_GAME_THING`, `SET_FOCUS`, `src/CHLApi.cpp:2706`, `:2816`); not yet reached in play |
| For this scene the trainer can be moved and picked up | pending | the flags are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| She beckons, turns to the creature and points: "Your Creature already has his own personality." as the camera turns to it | pending | real villager, camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A tiny rock (1% of its size, so all but invisible) is made by the pen | pending | `CreateWithAngleAndScale` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A two-shot of creature and trainer: it acts happy twice; "But you can alter and shape it by… hey what's going on?" as the creature is made to pick her up and hurl her at the rock | pending | the forced throw (`CREATURE_DO_ACTION` 15 Hurl, script ip 7573) has an executor (`Build::Hurl`, `src/Creature/CreaturePlanActions.cpp:158`); unverified that a villager is picked up and thrown: the lesson waits for her to fly (`GET_PROPERTY` Flying), so if she never leaves the ground it stalls there; not yet reached in play |
| A second after she is flying: "Aiee!" | pending | follows from the forced throw (see above); not checked in play |
| The creature is put back in place: "Punish your Creature to stop him doing things like that."; it looks at the camera, asks for attention and acts happy while she walks back to her spot | pending | moving and turning the creature by script are real (`MOVE_GAME_THING`, `SET_FOCUS`, `src/CHLApi.cpp:2706`, `:2816`); the creature's clips are played by script through `SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE` on a creature, real (`src/CHLApi.cpp:1528`, `:1412`), while `OVERRIDE_STATE_ANIMATION` on a creature still logs "not implemented" (`src/CHLApi.cpp:3475`); which of them this step uses was not checked here; not yet reached in play |
| The camera goes back to the player; she is made fixed and unhurtable again, faces the creature and shakes her fist | pending | real villager and camera natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The creature's record of the player's feedback is cleared | pending | `DEV_FUNCTION` 7 is handled (`src/ECS/PlayerCreature.cpp:1190`); not yet reached in play |
| Every two seconds: when the player has hold of the creature, "Now slap him by moving your hand quickly from side to side across him." (once per hold); when not, "Hold down the Action Button over the Creature to zoom in on him." at most once a minute | pending | `IsLockedInteraction` is real |
| The lesson waits, with no time limit, for any slap or stroke | pending | `GetInteractionMagnitude` is real |
| A slap: she shakes her fist: "Thanks. He won't try that again in a hurry." then "I'll come back soon to explain more about your wondrous, slightly headstrong beast." | pending | the slap test (`GET_INTERACTION_MAGNITUDE`, `src/CHLApi.cpp:6502`) is real; not checked in play whether the hand's gestures give it a value; see [../../creature/learning_from_feedback.md](../../creature/learning_from_feedback.md) |
| A stroke: she is impressed: "Oh, you want to encourage him to do things like that, do you?" then shakes her fist: "Well, it's up to you. I will return when your Creature is behaving a bit better." — she comes back for the next lesson all the same | pending | the stroke test (`GET_INTERACTION_MAGNITUDE`, `src/CHLApi.cpp:6502`) is real; not checked in play whether the hand's gestures give it a value |
| She walks back into the temple and fades away; the scroll goes | pending | real villager and highlight natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Lesson 4: the Leash of Learning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A palm tree is planted east of the pen; it can't be moved, picked up, hurt or burned | pending | `CREATE` of a tree is real (`CreateScriptObject`), and the four flags are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The trainer comes out under a new scroll; the scroll is logged at 0.23: "Click the Action Button on the Leash." (good advisor) | pending | `UpdateSnapshot` is real |
| The creature walks to its spot and is put at the leash stage, so it can't be leashed before the lesson | pending | `SetCreatureDevStage` is real, and moving and turning the creature by script are real (`MOVE_GAME_THING`, `SET_FOCUS`, `src/CHLApi.cpp:2706`, `:2816`); not yet reached in play |
| A cut scene looks down on the leash post at the pen: "It's time to take him for a walk."; she points at the leash as it is switched on | pending | `DevFunction` 2 switches the Leash of Learning on (see [../../creature/leash.md](../../creature/leash.md)); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| "This is the Leash of Learning." (the good advisor points at it) and "Please click on it and I'll show you how to use it with your Creature." | pending | real advisor and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The lesson waits until the player holds the Leash of Learning | pending | `GetObjectLeashType` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| "Good! Now click on your Creature."; reminder "Now click the Action Button on the Creature to attach the Leash to him." (trainer); she points at the creature | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The lesson waits until the creature is on the leash | pending | `IsLeashed` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A did-you-know sign on the leash: "Shake your hand when you've got the Leash and you'll release it. You can Leash your Creature to any object by double-clicking…" | pending | `DidYouKnow` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A cut scene with the leash drawn: she is impressed: "Now you've got some direct control of the little beast!" | pending | `SetDrawLeash` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| She beckons; the camera turns to her: "You can pull the Creature about with the Leash." | pending | real villager, camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The camera turns to the tree: "You see that tree over there?"; a target sparkle marks it for 30 seconds and the good advisor steps out and points at it | pending | `SpecialEffectObject` and the advisor natives are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| "Try pulling your Creature towards it. Click the Action Button on the ground to hurry him up."; reminder "Now walk your Creature over to the tree over there." (good advisor) | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| She walks (speed 0.5) to the tree and beckons the creature from there | pending | villager moves are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The good advisor nags "Click the Action Button on the ground to get him to move faster." | pending | the advisor nag is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| As the creature nears the tree the good advisor calls out once each: "Yes." within 40, "Yes." within 30, "You've got it. Very well done!" within 20; "No." once if it gets 10 further away than it started | pending | distance reads and advisor lines are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The step ends when the creature is on the leash within 20 of the tree and she has reached it | pending | real natives; see [../../creature/leash.md](../../creature/leash.md); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A cut scene at the tree: "Excellent! Well done."; then "Did you know that you can use a Leash to make your Creature interact with objects?" | pending | real camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A cow appears south of the tree; it can't be hurt, moved or picked up; "Try getting him to pick up that cow over there." with a sparkle on it and the camera on it | pending | `CREATE` of an animal is real and the flags are; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| "Move your hand with the Leash over the cow." and "Then click the Action Button."; reminder "Click the Action Button on the cow to get the Creature to interact with it." (good advisor), which also becomes the nag | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The creature is kept wanting only to look at things | pending | `SetCreatureOnlyDesire` is real |
| Once the hand comes within 10 of the cow it can be picked up; the step ends when the creature holds it | pending | the creature-holds-it test (`GET_OBJECT_HELD` of the creature, script ips 8854 and 9014) is real (`src/CHLApi.cpp:6539`); not yet reached in play |
| If the player picks the cow up with the hand it vanishes and a new one appears in its place | pending | the hand-held test is the in-hand read (property 9, script ip 8876), real (`src/CHLApi.cpp:1679`); not yet reached in play |
| A cut scene at the tree: "Well done. You see! The Leash is very useful.", "Why not take him for a walk? It'll do him good." and "You can drop the Leash by shaking your hand left and right quickly." | pending | real camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The trainer and the scroll vanish there and then, and the cow fades away if it is still there | pending | `ObjectDelete` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Lesson 5: tying the leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is put at the stage of tying the leash; the trainer appears by the tree, fixed and unhurtable | pending | `SetCreatureDevStage` and the trainer's flags are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Two gold scrolls: one over the tree (raised 12) and one at the pen (raised 6); clicking either, or the trainer, starts the lesson | pending | `CreateHighlight` and `GameThingClicked` are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The scroll is logged at 0.31: "Attach your Creature to that sparkly tree." (good advisor) | pending | `UpdateSnapshot` is real |
| A cut scene at the tree with the leash drawn: "There's more to learn about Leashes." | pending | `SetDrawLeash`, camera and text natives are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A sparkle on the tree for 30 seconds: "I didn't tell you that you can attach the Leash to objects." then, looking at her hand, "Try Leashing the Creature to this tree!" | pending | `SpecialEffectObject` and texts are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The camera goes back: "With the Leash attached to your hand, move it over the tree." | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The good advisor nags "With the Leash attached to your Creature, double-click on the tree."; with the hand within 10 of the tree, the trainer: "Double-click the Action Button to attach the Leash." at most every 30 seconds | pending | real advisor and distance natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The step ends when the creature is tied to that tree; the creature moves on to the good-and-evil-leash stage | pending | `IsLeashedToObject` and `SetCreatureDevStage` are real (see [../../creature/leash.md](../../creature/leash.md)); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A cut scene at the tree: "Good. The Leash is very useful. Your Creature is tied to the tree now. He'll interact with whatever he's Leashed to, as well." then "If you click on a Leash when the Creature's attached to something you can adjust its length." | pending | real camera and text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The other two leashes are switched on: "There are two other Leashes for you here." | pending | `DevFunction` 3 grants them; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The camera looks at each by the pen: "This is the Leash of Aggression." "It encourages your Creature to get angry." then "This is the fluffy Leash of Compassion " "It makes your Creature behave nicely." | pending | real camera and text natives; see [../../creature/leash.md](../../creature/leash.md); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A did-you-know sign on the leash's length by the tree | pending | `DidYouKnow` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Back at the tree: "Why not experiment with these Leashes? They're powerful tools." and "I'll bid you farewell. You now know as much about Creatures as I do." | pending | real text natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The guide's music starts as the camera closes in over 15 seconds: "One thing remains. Legends tell of an ancient Creature somewhere on this land." and, looking around, "I've heard him but I've never seen him. He hides from us humans. But he has great knowledge, I think." | pending | `StartMusic` and the camera are real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The camera goes back and the music stops; the good advisor appears, points at the leash sign: "There are advanced techniques for using the Leash. Find them on this signpost." | pending | real advisor and music natives; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| A did-you-know sign on the pen and the leashes | pending | `DidYouKnow` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## The trainer's exit and aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the last lesson the trainer doesn't go back to the temple: she joins the player's Norse village, walks to it (giving up after 30 seconds) and becomes an ordinary villager who can be moved, picked up and hurt | pending | she rejoins through `FLOCK_ATTACH` (script ip 5018; the lessons never call `ATTACH_TO_GAME`), real (`src/CHLApi.cpp:2863`); not yet reached in play |
| The village store's food is cut down to 1000 | partial | `GetResource` and `RemoveResource` read and take the store's food (`src/CHLApi.cpp`, `ecs::object_resources`); not checked in play against the original |
| The creature keeps its home in the pen, the three leashes and the stage reached | pending | the home, leashes and stage are real on the player's creature; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The scroll stays on at 0.31 until the guide's meeting moves it on | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The two scrolls of the last lesson are never removed by the script (undetermined whether they stay visible: the scroll commands are not researched past being clicked) | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Failing and soft locks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No lesson can be failed: each waits, with no time limit, for the one thing it asks (holding the creature, any slap or stroke, eating once, holding the rope, leashing, reaching the tree, the cow, the tie) — only the stroking step after the first hold has a two-minute limit | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| Killing or losing the creature isn't handled; every step that waits on the creature would then wait for ever (undetermined whether the game can lose the creature here at all) | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The trainer can't be hurt while she teaches, so she can't stall the lessons | pending | `SetIndestructable` is real; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The story can't go on until the last lesson ends, as the guide's meeting waits for it | pending | the guide's meeting needs a script-made guide creature, which `CREATE` of a creature makes now; not yet reached in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the cow step the near-the-cow "Yes." calls reuse the flags already set while walking to the tree, so they almost never play; only "No." can | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| In the cow step a check turns the trainer to the camera if she is at a spot in the north village, where she never is | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The thing the leash is tied to is called a house in the script but is the palm tree planted in the leash lesson | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The punishment step's one-minute timer is never used | pending | the same script; not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |

## Music, sound and voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide's theme plays over the first scene and the trainer's farewell | pending | `StartMusic` and `StopMusic` are real (`src/Audio/GameMusic`); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The trainer speaks in her own narrator voice; the advisors in theirs | pending | the advisors' and narrators' texts are real (`RunText`, `src/Help`); see [../advisors.md](../advisors.md); not yet reached: a no-skip game has not yet been run past Choose Your Creature's gate stones |
| The creature's moods are played as animations: tired, frightened, crying, sad, happy, hungry, look-at-me | pending | the creature's clips by script (`SET_SCRIPT_ULONG` / `SET_SCRIPT_STATE`) and its forced actions (`CREATURE_DO_ACTION`) are real, `OVERRIDE_STATE_ANIMATION` on a creature is not; not yet reached in play; see [../../creature/animation.md](../../creature/animation.md) |

## Saves and other modes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the lesson reached, the trainer and the scroll | pending | openblack has no saved games |

