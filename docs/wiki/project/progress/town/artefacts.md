# Town artefacts and totems

An artefact is a rock or other loose fixed object the player has put down by a town or worship site. The town's
villagers gather and dance round it; their dances make it worth more to its owner, and in a town that isn't the owner's
they win the owner belief. A worthy artefact glows with a magic effect and then shows its owner's symbol. Each town also
has its town centre totem showing its worship share and, once it has seen the creature, a statue of the creature.

Holding an object in the hand doesn't change it: nothing in the game paints held objects or counts how long they are
held. An artefact's only marks are the magic effect and the owner's symbol.

openblack has no artefacts: the table values are read (`InfoConstants.h`), the artefact effect's particle type exists
(`ParticleTypes.cpp`), the two villager dance states are placeholders (`LivingActionSystem.cpp`) and the script command
is a stub (`FeatureScriptCommands::MakeLastObjectArtifact`). Only the town centre's totem and spell icons are done
(`src/Worship/WorshipPercentage.cpp`, `src/Worship/TownCentreSpellIcon.cpp`).

**Progress: 21/34 done, 3 partial — 66%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## Becoming an artefact

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose fixed object (a rock, a dead tree, a toy, a lantern, the idol and the like) put down gently from the hand by a player who isn't neutral becomes an artefact of the town of the nearest building within 50 m, or of the nearest worship site there | partial | ported from raffclar PR #278, unverified against v1.42: the trigger `object_physics::ConsiderArtefact` at the end of physics covers rocks and other mobile statics and dead trees, nearest building or worship site by the spiral search; other fixed kinds (statics, features) are not wired |
| Only kinds whose table gives an artefact multiplier above 0 qualify (all rocks 0.0002, volcanic 0.00025, singing stones and the idol 0.0003, gate totems 0.0006, the bowling ball 0.0002, other toys, lanterns, fences and dead trees 0.0001), and nothing a script is holding | done | ported from raffclar PR #278, unverified against v1.42: the kind's `artifactMultiplier` > 0 (`object_physics::ConsiderArtefact`) |
| Something thrown (faster than 2 m/s across the land as it leaves the hand), or put down by the creature, never becomes an artefact | done | ported from raffclar PR #278, unverified against v1.42: only a landed put-down from the hand (FROM_HAND and LANDED, by the hand's player); a creature's put-down has no player |
| A new artefact starts worth the kind's villager-impressing value from the tables (0.01 for rocks) | done | ported from raffclar PR #278, unverified against v1.42: `villagerImpressiveValue` (`ECS/Systems/Implementations/ArtefactSystem.cpp`) |
| An artefact belongs to the player who put it down; it joins its town's (or site's) artefacts once, and the game's list of all artefacts | done | ported from raffclar PR #278, unverified against v1.42: `components::TownArtefact` (owner, town and site, newest first) |
| An artefact sent through a vortex comes out in the next land still an artefact of the same god with the same worth; only its town is forgotten | todo | no artefacts; see [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |
| An artefact worth more than 1 put down by a town (or site) other than its own impresses it: the villagers come to look at it | done | ported from raffclar PR #278, unverified against v1.42: a LOOK_AT_OBJECT reaction (`artefact_rules::WillImpress`) |
| Making an artefact can teach the player's creature to copy it | todo | the creature's deed exists as a number only (`CreatureDeeds::MakeArtefact`, `src/Creature/CreatureDeeds.h`); see [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |
| Taking an artefact in the hand takes it out of its town or site and stops its dance; whoever took it becomes its owner, and it keeps its worth | done | ported from raffclar PR #278, unverified against v1.42: `ArtefactSystem::Taken` from the hand's and the creature's pick-up |
| An artefact whose object is gone (or broken in two) goes, with its dance and its effect | partial | ported from raffclar PR #278, unverified against v1.42: the artefact goes with its object and its dance ends when the object goes; whether its glow (a spot visual on the object) closes with it is not checked |
| A land's feature script can make the last object it made an artefact of a numbered town (or else of the nearest town if that is the named player's, or else of no town), for a named player, at a set worth; no shipped land uses it | todo | `FeatureScriptCommands::MakeLastObjectArtifact` (`src/LHScriptX/FeatureScriptCommands.cpp`) is an empty stub |

## The villagers' dance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town looks after its artefacts every time it is processed; artefacts at worship sites get no dance of their own | done | ported from raffclar PR #278, unverified against v1.42: `town_process` step 7 (`ArtefactSystem::ProcessTown`) |
| A villager wants to join an artefact's dance less the farther it is (out to 250 m) and the fuller the dance already is; not at all while it is under way | done | ported from raffclar PR #278, unverified against v1.42: `artefact_rules::JoiningDesire`, offered by `villager::CheckSatisfyRelaxation` |
| The dance is chosen by the object's width: under 7 m the small object dance, under 10 m the medium one, otherwise the dance for objects wider than 20 m (the dance for objects under 20 m is never chosen) | done | ported from raffclar PR #278, unverified against v1.42: `artefact_rules::DanceForRadius` |
| If the artefact has moved since its dance was set out, the dance moves to it and footpaths are laid to it from the nearest town's storage pit | todo | see [../terrain/footpaths.md](../terrain/footpaths.md) |
| A villager who joins goes to wait by the artefact: from farther than 12 m it walks to a clear spot up to 12 m away on its own side; there it finds itself room, and after waiting its time (200 turns for villagers) it starts the dance | done | ported from raffclar PR #278, unverified against v1.42: WAIT_FOR_ARTIFACT_DANCE (`VillagerArtefactDance.cpp`) |
| When the dance starts, every dancer switches to the artefact dance, which lasts the dance's length | done | ported from raffclar PR #278, unverified against v1.42: ARTIFACT_DANCE (`VillagerArtefactDance.cpp`, the dance step shared with the scripts' dance) |
| When the dance is over, if the town most wants relaxation the dancers go back to waiting for another dance, otherwise they go back to their lives | done | ported from raffclar PR #278, unverified against v1.42 (`ECS/Systems/Implementations/ArtefactSystem.cpp`) |
| A dance left with no dancers ends | done | ported from raffclar PR #278, unverified against v1.42 (`ECS/Systems/Implementations/ArtefactSystem.cpp`) |
| The creature can go to an artefact (as near as three of its heights, at most 50 m) and dance round it | todo | see [../creature/town_actions.md](../creature/town_actions.md) |

## Worth and belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each dance finished in its owner's own town adds to its worth: the town's belief in its owner (counted fully up to 2, a third above, at most 3; 1 for a neutral town), times the kind's artefact multiplier, the share of the dance's dancers it had, the share of the dance's length danced and the land's artefact speed (1 unless a script sets it) | done | ported from raffclar PR #278, unverified against v1.42: `artefact_rules::DanceGain` (his rule caps the town's modifier at 4) |
| What is added is multiplied by 300 and, once the artefact is worth more than 1, divided by its worth, so it grows ever more slowly | done | ported from raffclar PR #278, unverified against v1.42: `artefact_rules::AddToWorth` |
| While its dance is on in a town that isn't its owner's, every 100 turns (plus a random part up to a quarter) the town gives the owner belief: its worth (only once over 1) × the object's impressiveness × dancers per head of the town's people × 5 | done | ported from raffclar PR #278, unverified against v1.42: `artefact_rules::DancingBelief` and `BeliefGiftDue`, the belief through `town_stores::AddToBelief` |
| Every 1000 turns each artefact at a worship site gains a little worth from the site's worship dance (0.00001 × the land's artefact speed × the site's dancers over a count of the player's) | done | ported from raffclar PR #278, unverified against v1.42: `ArtefactSystem::PowerUpSite` at the end of the site's turn (`Worship/WorshipSite.cpp`) |
| An artefact worth more than 1 makes the object 1 + its worth times as impressive to the villagers that see it | todo | `ArtefactSystem::ImpressiveModifier` is ported but unused: our reactions impress only for miracles so far (see [../worship/belief.md](../worship/belief.md)) |
| Worth more than 1, a magic effect plays over it, as strong as it is worth | todo | the effect's particle type exists (`src/Particles/ParticleTypes.cpp`) but nothing plays it |
| Worth more than 2, its owner's symbol floats over it at half size | done | ported from raffclar PR #278, unverified against v1.42: drawn beside the town belief symbols (`psys::town_belief`); Debug > Windows > Artefacts raises one's worth |
| The rules are the same for every player, the computer's and other people's in multiplayer | partial | ported from raffclar PR #278, unverified against v1.42: the rules take any player, but a hand's put-down is always PLAYER_ONE's (no other hands yet) |

## Totems and statues

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town centre's totem pole rises and falls with the town's worship share | done | the plinth and its icon rise by the town's worship percentage, eased (`percentage::UpdateTotems` and `percentage::ProcessTotem`, `src/Worship/WorshipPercentage.cpp`); made with the functional town centre (`src/ECS/Archetypes/AbodeArchetype.cpp`); see ../worship/ |
| The hand can pull the totem up or down to set how many worship | todo | only land scripts and a debug variable set the percentage (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/Worship/WorshipDebugHooks.cpp`); the hand does not drag the totem |
| A statue of the player's creature is put up in the town centre once the town knows it | todo | the plinth's icon is always the hand (`VillageTotemArchetype::Create`, `src/ECS/Archetypes/VillageTotemArchetype.cpp`); no creature statue |
| The creature can be impressed by, play with or steal the statue | todo | See ../creature/ |
| A totem is not destroyed by miracles | done | an effect can lower the totem's life, but only villagers and animals are destroyed by one (`effects::ApplyEffect`, `src/ECS/Effects/EffectValues.cpp`); the totem goes only with its town centre (`src/ECS/Abodes.cpp`) |
| The town centre's spell icons show the miracles the town gives its worship site | done | one icon per seed the town holds, on the town centre's special points (`src/Worship/TownCentreSpellIcon.cpp`), made by the land's feature script (`CreateTownCentreSpellIcon`, `src/LHScriptX/FeatureScriptCommands.cpp`) and drawn by the rendering system; See ../miracles/ |
