# Development phases

A creature grows up through stages, from a fresh young creature in the first land to a fully mature one. Each stage has
its own lesson from the story (eating from the hand, punishment, the leash, meeting its guide, fighting, helping a town),
brings and takes away desires, and gates what it can learn. The first stages come with a creature tutorial.

**Progress: 13/49 done, 0 chosen, 23 partial, 6 inferred — 52%**

How the original does it, in our wiki: [The development phases](../../../creature/players-creature.md#the-development-phases).

## How stages work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature is at one of fourteen stages, from the first to fully mature | done | the stage is kept (`CreatureMindState::developmentPhase`, `src/ECS/Components/CreatureMind.h`, 0 to `k_FullyGrownUp` 13); a creature the game makes (a script's `CREATE`, the land scripts' `CREATE_CREATURE`, `CREATURE_CREATE_RELATIVE_TO_CREATURE`) is at the first stage with every desire off and no stage's desires brought until a script or its mind file moves it (`player_creature::StartAtFirstStage`), as the original's made creature ([players-creature.md](../../../creature/players-creature.md#the-development-phases)); a loaded creature is moved to the last stage as the original's; tests `ScriptLoadCreatureTest.AScriptsCreateMakesANewCreatureOfNobodyOutOfReactions`, `ScriptLoadCreatureTest.TheLandScriptsCreateCreatureMakesANewCreatureOfTheTypeForThePlayer`. The debug tools' creatures start fully mature (openblack only) |
| Each stage brings up to ten desires and takes away up to four, from the game's tables | done | `Phases` in `CreatureMindSystem.cpp` (from `creatureDevelopmentPhaseEntry`), `creature_desires::ActivateForPhase`; test `CreatureDesires.GrowingUpBringsAndTakesDesires` |
| Reaching a stage, no desire stays held back | done | `creature_desires::ActivateForPhase` clears every hold first; test `CreatureDesires.GrowingUpBringsAndTakesDesires` |
| Each turn the creature checks whether it has met its stage's criteria and moves on | inferred | the exe has a table of stage tests (filled at start-up, 0xC82640; one reads whether it was ever handed something, others the duration table), but its only reader, the dispatcher 0x4C59A0, has no caller or reference in the exe (xref): if it never runs, this row goes. Stages move only by script (the stage native, `player_creature::SetDevelopmentStage`, and `DEV_FUNCTION` 1), packets, the mind file and computer creatures (callers of `MoveToDevelopmentPhase`) |
| Each stage lasts at least a time from the game's tables | inferred | the duration table (`CreatureDevelopmentDurationEntry`) is read only by the stage tests above, which nothing is seen to call |
| Moving to a new stage tells the player what the creature can now do | inferred | the stage help kinds and their lesson scripts are ported (`creature_help::StageScript`; see [lessons and help](lessons_and_help.md)), but which code raises them in the original is not found ([mind.md](../../../creature/mind.md) Pending) |
| The stage is saved in and restored from the mind file | partial 75% | written by `creature_mind_model::ToFile` when the game saves the creature (`player_creature::SaveMyCreature`), read in `CreatureMindSystem::TakeUpFile`; test `test_creature_mind_file`; the original then moves a loaded player's creature to stage 13, ours keeps the file's ([players-creature.md](../../../creature/players-creature.md) Pending) |
| A script sets a creature's stage | done | SET_CREATURE_DEV_STAGE (`src/CHLApi.cpp`, `player_creature::SetDevelopmentStage` in `src/ECS/PlayerCreature.cpp`): kept near home off the leash it is let go from stage 5 (`creature_leash::ReleasedFromHome`, the radius only), its desires switch at once, even for the stage it is at (`CreatureMindSystem::MoveToStage`), and it gives up its plan, as the original's stage move; a stage out of range is refused with a warning; tests `LeashRules.ReachingTheLeavingStageLetsAKeptCreatureGo`, `CreatureMindSystemTest.MovedToAStageItsDesiresAreTheStagesAtOnce`, `PlayerCreatureNativesTest.SetCreatureDevStageGoesToTheLeashesAndTheMindAtOnce`; two unnamed fields the original clears are not written |
| The tutorial's script starts the player's creature growing up again from the first stage, kept near its home | done | DEV_FUNCTION 1 (`player_creature::DevFunction`): the same stage move to 0, then `ConfineToHome` within 12, as the original; nothing without the player's creature; test `PlayerCreatureNativesTest.DevFunctionOneStartsThePlayersCreatureGrowingUp` |
| The tutorial's scripts grant the learning leash, then the aggression and compassion leashes | done | DEV_FUNCTION 2 and 3 (`player_creature::DevFunction` in `src/ECS/PlayerCreature.cpp`, called from `src/CHLApi.cpp`); tests `PlayerCreatureNativesTest.DevFunctionTwoTeachesTheRopeLeash`, `DevFunctionThreeTeachesTheOtherLeashesAndHandsTheCreatureOver`; see [leash](leash.md) |

## The stages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First stage: the fresh creature, just chosen, learns its first simple things | partial 40% | the stage and its desires work and the lessons' script moves it there (DEV_FUNCTION 1); no creature starts there (see the first row), and its stage message does nothing in the original either |
| Learning to take from the player and eat: stroked for holding food, it learns to eat what it is given | partial 50% | `creature_learning::StrokeTeachesEating` (eat when stroked, from the second stage on), its desires; the stage is set by the eating lesson's script ([../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md)), not yet reached in play; see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Punishment: it learns from slaps, and from here on strokes and slaps teach lessons | partial 50% | lessons start at this stage (`k_MinLessonPhase` in `CreatureMindLearning.cpp`), and before it the player is told it is too young (`creature_help::TooYoungToLearn`; test `CreatureHelp.FeedbackTooYoungToLearnFrom`); the lesson's script is not yet reached in play |
| Leash pull and pick up: it is led on the leash and learns to pick things up; from here on it can copy the player | partial 50% | copying the player starts here (`creature_watching::k_MinMimicPhase`; test `CreatureWatching.MimicryStages`), as do thirst and growing (`k_GrowingPhase`) and leaving home to look out to sea or sit on the beach (`k_LeaveHomeStage`); the lesson's script is not yet reached in play |
| Leash tied to something (planned as a house): it learns to stay where the leash is tied | pending | tying the leash works (see [leash](leash.md)); the stage's lesson doesn't; in the shipped lesson the creature is tied to a palm tree by the pen, not a house: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |
| Meeting its guide | pending | no shipped script sets this stage; see [../story/creature_guide.md](../story/creature_guide.md) |
| Making friends with its guide | pending | set from the meeting on; see [../story/creature_guide.md](../story/creature_guide.md) |
| The guide explains the history | pending | no shipped script sets this stage (the history is told while making friends); see [../story/creature_guide.md](../story/creature_guide.md) |
| The guide teaches it miracles; after this stage it can cast powered-up miracles | partial 30% | a creature tries power-ups only past the seventh stage (`mind_detail::MayCast`; test `CreatureMindCastingTest.APowerUpOnlyOnceGrownPastItsSeventhStage`; see [creature_casting.md](creature_casting.md)); the guide's teaching is not in; set by the guide's food lesson ([../story/creature_guide.md](../story/creature_guide.md)) |
| Impressing a town | pending | see [../story/creature_guide.md](../story/creature_guide.md) |
| Learning to fight | pending | see [fighting](fighting.md) and [../story/creature_guide.md](../story/creature_guide.md) |
| Helping a town | pending | no shipped script sets this stage; see [../story/creature_guide.md](../story/creature_guide.md) |
| The good and evil leashes | partial 50% | the leashes are granted (DEV_FUNCTION 3; see [leash](leash.md)); the shipped game sets this stage at the end of the tying lesson, which also shows the two leashes, not yet reached in play; the stage's own lesson is compiled but never started: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |
| Fully mature: every desire it has is its own | done | stage 13 from the game's tables (33 desires on); test `CreatureDesires.GrowingUpBringsAndTakesDesires` |

## Limits on a young creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Skills it sees practised are only learnt once it is old enough for each | partial 50% | `creature_watching::SeeSkill` (`minPhase`); test `CreatureWatching.LearnsASkillOnceWatchedLongEnough`; nothing in play shows it a skill: `CreatureMindSystem::SeeSkill` is called only by the debug spawner and the testbed |
| Miracles it sees cast are only learnt once it is old enough for each, and the player is told it can't learn one yet | done | `creature_watching::SeeMiracle` (the row's stage), reached in play through the miracle reactions (`WatchMiracle`, see [reactions](reactions.md)), told as `CantLearnMagicActionYet`; test `CreatureWatching.LearnsAMiracleBySightingsAndSpecies` |
| A creature before the third stage doesn't copy the player | done | `creature_watching::k_MinMimicPhase`, as the original's mimic gate ([mimicry.md](../../../creature/mimicry.md#gate-in-considermakingcreaturemimicplayer)); test `CreatureWatching.MimicryStages` |
| Its body only starts using energy at the first stage, grows and gets thirsty from the third, and faints from the fifth | done | `k_BodyPhase`, `k_GrowingPhase`, `k_FaintingPhase` (`src/Creature/CreaturePhysiology.h`); tests `CreaturePhysiology.OnlyItsTurnsAreCountedBeforeTheBodyStage`, `TurnsBeforeTheBodyStageCountTowardsItsAge`; see [physiology](physiology.md) |
| Past the fifth stage, worn out, it no longer wants to wander | done | `k_WanderlustPhase` (`src/Creature/CreaturePlanner.cpp`); test `CreatureExploration.AYoungOrRestedCreatureWantsToExplore` |
| How far it strays from home before it wants to go back is its stage's | partial 50% | `GoHomeDistance` from the stage table (`CreatureMindSystem.cpp`); no test pins it |
| A young creature can't pick things up until it has learnt to, and the player is told so | inferred | the game's skills learnt by watching are building, fields, totems, the store, fishing and dancing (no picking up); a stage gate on picking up is not found |
| A young creature can't run until it has learnt to | inferred | no running skill or stage gate on running is found |
| A young creature is kept near its home until it is mature enough to leave | done | DEV_FUNCTION 1 keeps it within 12 of home (`LeashSystem::ConfineToHome`), let go once it reaches the fifth stage off the leash (`creature_leash::ReleasedFromHome`); tests `LeashSystemTest.AYoungCreatureOnTheFirstLandIsKeptAtItsHome`, `LeashRules.ReachingTheLeavingStageLetsAKeptCreatureGo` |
| An action it can't learn at its stage tells the player so | partial 50% | a miracle (`CantLearnMagicActionYet`) and a stroke or slap too young to teach (`TooYoungToLearn`) are told; a skill only through the debug spawner's sightings; see [lessons and help](lessons_and_help.md) |
| The young creature is given its first lessons (including that villagers are nice to eat, for the computer gods' creatures) | inferred | not found in the exe: the computer creatures are moved to stage 13 (`MoveToDevelopmentPhase` callers); what they are taught is not read |
| Its growth in size goes with its age | done | test `CreaturePhysiology.YoungCreaturesGrowFastThenSlowly`; see [growth and size](growth_and_size.md) |

## The creature tutorial

The twelve tutorial prompts of the creature help are rows of [lessons and help](lessons_and_help.md#the-first-lessons-tutorial-prompts).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Showing a lesson as the creature learns (running, picking up, throwing, stomping, dancing) | partial 30% | a skill learnt by watching is told (LessonNormalAction), but skills are only shown to it from the debug spawner; see [lessons and help](lessons_and_help.md) |
