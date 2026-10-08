# Learning by observation

Besides the hand's rewards, the creature learns by watching. It picks up ordinary skills (building, fishing, dancing and
so on) by watching villagers practise them, learns miracles by seeing them cast often enough, and copies what its god
does: it notices a deed, does it itself, and may come to want what its god seemed to want. What it sees its god do also
tells it what its god wants.

**Progress: 14/80 done, 19 partial — 29%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Skills learnt from villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There are six ordinary skills to learn by watching: building, using a field, using a totem, using the storehouse, fishing and dancing | done | read from the game's tables in `src/Creature/CreatureMindTables.cpp` |
| Each skill needs the creature grown up to a stage first: fields, fishing and dancing early, the totem later, building and the storehouse late | partial | `SkillRule::minPhase` in `creature_watching::SeeSkill` (`src/Creature/CreatureWatching.cpp`); dormant: `CreatureMindSystem::SeeSkill` is only called from the debug spawner |
| A skill is learnt once the creature has watched it for six or seven seconds from first seeing it | partial | `SkillRule::watchSeconds`; test `CreatureWatching.LearnsASkillOnceWatchedLongEnough`; dormant: only the debug spawner shows a skill |
| How often it has seen each skill and when it first did are kept and saved in its mind | done | `creature_watching::Knowledge`, kept with the mind (`src/Creature/CreatureMindModel.cpp`, `components/creaturemind` `MindFile`) |
| Villagers practising a skill where creatures can see it teach them | partial | `CreatureMindSystem::SeeSkill` (`src/ECS/Systems/Implementations/CreatureMindLearning.cpp`) works, but no villager job reports what it practises; only the debug spawner calls it |
| Only creatures near enough and able to see it learn | partial | a fixed reach of 150 (`k_ViewDistance` in `CreatureMindLearning.cpp`), not the creature's sight; only reached from the debug spawner |
| Knowing a skill lets it do the actions that need it (help build, fish, dance, use the storehouse or a totem) | todo | those actions aren't carried out; see [town_actions.md](town_actions.md) |
| A skill seen too young to learn is ignored | partial | `Progress::ignored` in `creature_watching::SeeSkill`; only reached from the debug spawner |
| The player is told when it has learnt a skill, nearly has, or is too young to | todo | only a thought in the mind's log (`creature_mind_model::Think`); see [lessons_and_help.md](lessons_and_help.md) |

## Miracles learnt by seeing them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature learns a miracle by seeing it cast near it, whoever casts it | partial | `CreatureMindSystem::SeeMiracle` (`CreatureMindLearning.cpp`) works, but no miracle reports a sighting; only the debug spawner's "Cast it near" calls it |
| Each miracle needs seeing a set number of times (water 9, food 12, heal and wood 15, fireball 18, lightning and shield 21, tornado 39; the explosion so many it only comes from a script) | partial | `MiracleRule::timesToSee` from the tables (`CreatureMindTables.cpp`); dormant: sightings only come from the debug spawner |
| Some species need to see miracles more often than others | partial | `creature_mind_tables::MiracleMultiplier`, `creature_watching::TimesToLearn`; test `CreatureWatching.LearnsAMiracleBySightingsAndSpecies`; dormant: sightings only from the debug spawner |
| A sighting counts again only five seconds after the last | partial | `k_MiracleSightingTurns` (50 turns) in `src/Creature/CreatureWatching.h`; sightings only from the debug spawner |
| On the learning leash each sighting counts three times | partial | `CreatureLeash` sets `mind.leash.miracleSightingWeight` (`LeashSystem.cpp`), read by `SeeMiracle`; sightings only from the debug spawner; see [leash.md](leash.md) |
| Miracles need the creature grown up to a stage (the explosion line later than the rest) | partial | `MiracleRule::minPhase` in `creature_watching::SeeMiracle`; sightings only from the debug spawner |
| A power-up teaches nothing until its miracle is known; the storm with lightning needs the storm, the tornado needs that; the thirst and itch spells need the building skill | todo | no prerequisite rule in `src/Creature/CreatureWatching` |
| The lightning bolt's second power-up is never learnt by watching | todo | no such rule in our tree |
| A frozen creature learns nothing from what it sees | todo | `SeeMiracle` does not check the freeze spell |
| Some miracles are flagged as known from the start, which only the computer gods use to choose what to teach | done | `MiracleRule::knownAtStart` read (`CreatureMindTables.cpp`); `creature_watching::StartKnowledge` |
| Once it has seen a miracle half the times it needs, it may try it; tries count as sightings | todo | the creature casts no miracle; see [creature_casting.md](creature_casting.md) |
| It keeps counting sightings after it has learnt a miracle | todo | `creature_watching::SeeMiracle` stops counting once the miracle is known |
| Three quarters of the way there it is "nearly" there, and a meter of how far along it is moves | partial | how far along it is (`Progress::share`) shows as a percentage on the Creature Cave's magic scroll; there is no "nearly" moment |
| Learnt miracles appear on the Creature Cave's magic scroll | done | `CreatureCaveSystem` snapshot, drawn by the temple's creature room (`src/3D/Implementations/TempleInterior.cpp`); see [creature_cave.md](creature_cave.md) |

## Copying the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Copying goes in stages: noticing the deed, doing it itself, then (for some deeds) wanting what the player wanted | done | `creature_watching::StepMimicry`, stepped in `CreatureMindSystem::LearnTurn` (`CreatureMindLearning.cpp`); test `CreatureWatching.MimicryStages` |
| It copies only once past the third stage of growing up | done | `k_MinMimicPhase` (3) in `creature_watching::StartMimicry` |
| It copies only while the player holds the learning leash in the hand that did the deed, except playing with a toy | done | `MimicRule::needsLearningLeash`, `MimicConditions::learningLeashInHand` (from `LeashSystem`) |
| It doesn't copy while it reacts to something pressing | partial | the rule exists (`k_MaxReactionPriorityToMimic`) but `PlayerDid` always passes no reaction |
| It must be able to see where the deed was done | partial | a fixed reach of 150 (`k_ViewDistance`) rather than the creature's sight |
| Each deed has its own chance of being copied (stealing and sacrifice very likely, planting a tree unlikely) | done | `MimicRule::chance` from the tables, drawn in `creature_watching::StartMimicry` |
| Already copying, a new deed takes over only if it is as likely or more, and different | done | `creature_watching::StartMimicry` |
| Each stage lasts a number of steps from the tables, plus a little at random, a step a second | done | `MimicRule::stageSteps`, a step every `k_MimicStepTurns` (10 turns) in `CreatureMindLearning.cpp` |
| Noticing, it plays an action for the kind of deed: helpful, aggressive, neutral, stealing or playful | partial | our tree only turns it to look at the spot for two seconds (`CreatureMindSystem::PlayerDid`) |
| It does the deed itself, to the same thing or one like it | partial | `CreatureMindSystem::LearnTurn` adopts the copy actions; only with the plan actions our tree carries out (`src/Creature/CreaturePlanActions.cpp`) |
| For some deeds it then comes to want the desire behind it above all | done | `MimicStage::CopyDesire` then `creature_learning::MakeFullyDominant` in `CreatureMindLearning.cpp` |
| Stroked while copying, it skips straight to doing the deed and wants to follow its god more | done | `creature_watching::StrokedWhileMimicking`, `k_MimicStrokeBoost` in `CreatureMindLearning.cpp` |
| Only the player's own leashable creature copies that player | done | `CreatureMindSystem::PlayerDid` takes the player and only creatures it owns watch; test `CreatureMimicTest.OnlyThePlayersOwnCreatureWatchesAndDraws` |
| Being too young to copy is shown to the player | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Deeds the creature notices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Putting food in a worship site by hand | todo | not reported |
| Casting food at a worship site | todo | the creature is told of no miracle hit |
| Putting food in the storehouse by hand | todo | not reported |
| Casting food into the storehouse | todo | the creature is told of no miracle hit |
| Putting wood in the storehouse by hand | todo | not reported |
| Casting wood into the storehouse | todo | the creature is told of no miracle hit |
| Building a house | todo | not reported |
| Putting wood on a building site by hand | todo | not reported |
| Casting wood by a building site | todo | the creature is told of no miracle hit |
| Putting wood in the workshop, or casting wood by it | todo | not reported. Our wiki differs: the original never reports the casting part; wood cast on a workshop counts as cast into a storehouse ([creature.md](../../bw1-notes/creature.md#deed-table-complete-for-w120)) |
| Planting a tree | done | the hand's replanting reports it (`HandSystem::Replant` in `src/ECS/Systems/Implementations/HandTrees.cpp`, `ecs::creature_mimic::Consider`) |
| Shielding a town | n/a | the original never reports this deed. Our wiki differs: the shield miracle gives no deed ([creature.md](../../bw1-notes/creature.md#deed-table-complete-for-w120)) |
| Bringing people to worship | n/a | the original never reports this deed. Our wiki differs: no call site reports it ([creature.md](../../bw1-notes/creature.md#deed-table-complete-for-w120)) |
| Making an artefact | todo | not reported; artefacts not ported |
| Damaging something by throwing, or throwing something at it | partial | only a building hit with the building's own body thrown from the hand is reported (`src/ECS/Physics/Buildings.cpp`; test `CreatureMimicDecision.ABuildingHitCountsOnlyWithAPlayerAndItsOwnBodyFromTheHand`); a thrown thing's own damage is not |
| Damaging something with fire | todo | the creature is told of no miracle hit |
| Damaging something with magic | todo | the creature is told of no miracle hit |
| Impressing by throwing | n/a | the original never reports this deed. Our wiki differs: no call site reports it ([creature.md](../../bw1-notes/creature.md#deed-table-complete-for-w120)) |
| Impressing with magic | todo | the creature is told of no miracle hit |
| Throwing something into the sea | partial | a villager that sinks reports it for the hand that last dropped it (`src/ECS/VillagerDrowning.cpp`, `creature_mimic::ConsiderThrownInTheSea`; test `CreatureMimicTest.AVillagerInTheSeaReportsTheHandThatLastDroppedIt`); an animal does not |
| Making a disciple of each kind: farmer, forester, fisherman, builder, breeder, protector, missionary, craftsman, house changer, worshipper | todo | not reported; see [../villager/](../villager/) |
| Taking something home | todo | not reported |
| Casting water on crops | partial | a water miracle on a player's field reports it (`src/Magic/Spells/SpellWater.cpp`; test `CreatureMimicSiteTest.AFieldWateredByAPlayersMiracleIsTheirDeed`); the miracle-hit path for other things is not ported |
| Casting water to put out a fire | todo | the creature is told of no miracle hit |
| Stealing something and putting it in a town, or by the temple | todo | not reported |
| Breaking rocks | todo | not reported |
| Throwing the football into the goal, or catching it | n/a | the original never reports these deeds; see [../town/football.md](../town/football.md#the-player-and-the-creature). Our wiki differs: neither is reported anywhere ([creature.md](../../bw1-notes/creature.md#deed-table-complete-for-w120)) |
| Sacrificing | todo | not reported |
| Playing with a toy | todo | not reported; the toys: [../nature/toys.md](../nature/toys.md) |
| Healing | todo | the creature is told of no miracle hit |
| Stealing food from a farm | todo | not reported |

## What it thinks its god wants

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle it sees its god cast shows it the desires the miracle answers, and the town desire it helps | todo | no perceived desires in our tree; `src/Magic/Core/Spell.cpp` leaves it as a TODO |
| It only takes in what it can see: within about two thirds of a half turn of where it looks, or in its own patch of land | todo | no perceived desires in our tree |
| What it thinks fades slowly | todo | no perceived desires in our tree |
| What it thinks its god wants drives its desire to follow its god's wishes | todo | nothing feeds that desire's source; see [desires.md](desires.md) |
| What it thinks its god wants most is shown in the Creature Cave | todo | no perceived desires in our tree; see [creature_cave.md](creature_cave.md) |

## Watching others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Watching its god be nice, nasty or impressive stirs its own compassion, anger or wish to impress | todo | the desire sources exist but nothing feeds them from watching; see [desires.md](desires.md) |
| Watching villagers play or eat stirs its own wish to play or eat | todo | as above |
| Seeing an action done, it updates its desires as if it had done it | todo | not modelled |
| A friendly creature can teach it what it knows, and it can ask a friend to teach it | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It follows a friend doing something worth copying | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |

## Teaching by scripts and computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can make it learn everything, or everything but some actions | todo | `CREATURE_LEARN_EVERYTHING`, `CREATURE_LEARN_EVERYTHING_EXCLUDING` are stubs in `src/CHLApi.cpp` |
| Scripts can set whether it knows a single action | todo | `CREATURE_SET_KNOWS_ACTION` is a stub in `src/CHLApi.cpp` |
| Scripts can give it the most skill at one miracle or power-up | todo | not modelled |
| The developer cheats to learn everything, learn ordinary things, or move to the next stage | todo | not present; the debug spawner can show a skill, cast a miracle or do a deed near a creature (`src/Debug/CreatureSpawnerMind.cpp`) |
| Computer gods teach their creatures skills, miracles and the use of totems, and lead them on the leash to things | todo | no computer gods; see [../story/](../story/) and [../multiplayer/](../multiplayer/) |
| A computer god's creature's knowledge is balanced to the land's difficulty | todo | not modelled |
