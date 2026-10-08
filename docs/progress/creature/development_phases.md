# Development phases

A creature grows up through stages, from a fresh young creature in the first land to a fully mature one. Each stage has
its own lesson from the story (eating from the hand, punishment, the leash, meeting its guide, fighting, helping a town),
brings and takes away desires, and gates what it can learn. The first stages come with a creature tutorial.

**Progress: 6/47 done, 12 partial — 26%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## How stages work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature is at one of fourteen stages, from the first to fully mature | partial | the stage is kept (`CreatureMindState::developmentPhase`, `src/ECS/Components/CreatureMind.h`, 0 to `k_FullyGrownUp` 13), but every creature starts fully mature unless its mind file or a script says otherwise; the original's starts at the first stage ([creature.md](../../bw1-notes/creature.md#the-players-creature)) |
| Each stage brings up to ten desires and takes away up to four, from the game's tables | done | `Phases` in `CreatureMindSystem.cpp` (from `creatureDevelopmentPhaseEntry`), `creature_desires::ActivateForPhase`; test `CreatureDesires.GrowingUpBringsAndTakesDesires` |
| Reaching a stage, no desire stays held back | done | `creature_desires::ActivateForPhase` clears every hold first |
| Each turn the creature checks whether it has met its stage's criteria and moves on | todo | nothing moves a creature to the next stage |
| Each stage lasts at least a time from the game's tables | todo | the duration table is not read |
| Moving to a new stage tells the player what the creature can now do | todo | see [lessons and help](lessons_and_help.md) |
| The stage is saved in and restored from the mind file | partial | read from the file in `CreatureMindSystem::TakeUpFile` (`CreatureMindLearning.cpp`); `creature_mind_model::ToFile` writes it, but nothing in the game saves the mind |
| The stage shows in the debug spawner and can be changed there | done | Debug > Creature spawner (`src/Debug/CreatureSpawner.cpp`, "set stage"; openblack-only) |
| The testbed can set a creature's stage | n/a | openblack-only tool, and our tree has no testbed; the debug spawner sets the stage |
| A script sets a creature's stage | partial | SET_CREATURE_DEV_STAGE (`src/CHLApi.cpp`, `player_creature::SetDevelopmentStage` in `src/ECS/PlayerCreature.cpp`); the desires follow at the mind's next turn, the original's at once |
| The tutorial's script starts the player's creature growing up again from the first stage, kept near its home | todo | DEV_FUNCTION 1 is not ported (`player_creature::DevFunction` handles only 2 and 3; the rest log `NotImplemented`) |
| The tutorial's scripts grant the learning leash, then the aggression and compassion leashes | done | DEV_FUNCTION 2 and 3 (`player_creature::DevFunction` in `src/ECS/PlayerCreature.cpp`, called from `src/CHLApi.cpp`); see [leash](leash.md) |

## The stages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First stage: the fresh creature, just chosen, learns its first simple things | partial | the stage exists; the initial lessons and their criteria are missing |
| Learning to take from the player and eat: stroked for holding food, it learns to eat what it is given | partial | `CreatureMindSystem::LearnFromFeedback` (eat when stroked, from the second stage on); the stage's criteria are missing; see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Punishment: it learns from slaps, and from here on strokes and slaps teach lessons | partial | lessons start at this stage (`k_MinLessonPhase` in `CreatureMindLearning.cpp`); the stage's criteria are missing |
| Leash pull and pick up: it is led on the leash and learns to pick things up; from here on it can copy the player | partial | copying the player starts here (`creature_watching::k_MinMimicPhase`); the stage's criteria are missing |
| Leash tied to something (planned as a house): it learns to stay where the leash is tied | todo | tying the leash works (see [leash](leash.md)); the stage's lesson doesn't; in the shipped lesson the creature is tied to a palm tree by the pen, not a house: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |
| Meeting its guide | todo | no shipped script sets this stage; see [../story/creature_guide.md](../story/creature_guide.md) |
| Making friends with its guide | todo | set from the meeting on; see [../story/creature_guide.md](../story/creature_guide.md) |
| The guide explains the history | todo | no shipped script sets this stage (the history is told while making friends); see [../story/creature_guide.md](../story/creature_guide.md) |
| The guide teaches it miracles; after this stage it can cast powered-up miracles | todo | creatures cast no miracles in our tree, powered up or not; set by the guide's food lesson ([../story/creature_guide.md](../story/creature_guide.md)) |
| Impressing a town | todo | see [../story/creature_guide.md](../story/creature_guide.md) |
| Learning to fight | todo | see [fighting](fighting.md) and [../story/creature_guide.md](../story/creature_guide.md) |
| Helping a town | todo | no shipped script sets this stage; see [../story/creature_guide.md](../story/creature_guide.md) |
| The good and evil leashes | partial | the leashes can be granted (DEV_FUNCTION 3; see [leash](leash.md)); the stage's lesson doesn't exist; the shipped game sets this stage at the end of the tying lesson, which also shows the two leashes; the stage's own lesson is compiled but never started: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |
| Fully mature: every desire it has is its own | done | stage 13 from the game's tables; the default for every creature in our tree |

## Limits on a young creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Skills it sees practised are only learnt once it is old enough for each | partial | `creature_watching::SeeSkill` (`minPhase`); test `CreatureWatching.LearnsASkillOnceWatchedLongEnough`; dormant: `CreatureMindSystem::SeeSkill` is only called by the debug spawner |
| Miracles it sees cast are only learnt once it is old enough for each | partial | `creature_watching::SeeMiracle`; test `CreatureWatching.LearnsAMiracleBySightingsAndSpecies`; dormant: `CreatureMindSystem::SeeMiracle` is only called by the debug spawner |
| A young creature can't copy the player, and the player is told so | partial | it can't copy before its stage (`creature_watching::k_MinMimicPhase`); the message is missing |
| A young creature can't pick things up until it has learnt to, and the player is told so | todo | |
| A young creature can't run until it has learnt to | todo | |
| A young creature is kept near its home until it is mature enough to leave | partial | `LeashSystem::ConfineToHome` keeps it near home, but only the debug spawner calls it; nothing lets it go as it matures |
| An action it can't learn at its stage tells the player so | todo | see [lessons and help](lessons_and_help.md) |
| The young creature is given its first lessons (including that villagers are nice to eat, for the computer gods' creatures) | todo | |
| Its growth in size goes with its age | done | see [growth and size](growth_and_size.md) |

## The creature tutorial

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Selecting the creature | todo | |
| Making it run | todo | |
| Handing it something | todo | |
| Deselecting it | todo | |
| More rocks are needed for its home | todo | see [home and pen](home_and_pen.md) |
| Picking things up | todo | |
| Helping build | todo | |
| Dropping things | todo | |
| Eating | todo | |
| Rewarding and punishing it | todo | |
| Drinking | todo | |
| Having a poo | todo | |
| Showing a lesson as the creature learns (running, picking up, throwing, stomping, dancing) | todo | see [lessons and help](lessons_and_help.md) |
