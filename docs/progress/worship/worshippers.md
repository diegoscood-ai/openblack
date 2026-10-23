# Worshippers and dancing

The villagers who leave their town to dance at the worship site. The player sets how many of a town worship; the
dancers chant prayer power, tire, get hungry and can die of it.

**Progress: 13/24 done, 0 chosen, 8 partial, 1 inferred — 75%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/worship.md).

## Going to worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each town has a totem whose height sets the share of its people who worship; the player holds it with the hand and pulls it up or down | partial 85% | the town's worship percentage (`worship::percentage::SetWorshipPercentage`, `src/Worship/WorshipPercentage.cpp`) sets the share and raises the totem; land scripts set it (`src/LHScriptX/FeatureScriptCommands.cpp`); the hand grips, drags and lets go of the totem ([the village totem](../../../magic/village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go), tests `VillageTotemRules.*`, `VillageTotemHandHold.*`); missing: the immersion at the grip, the creature's desires at the grip, the release's grey floating number and the hand's size in the hold ([pending](../../../magic/village-totem.md#pending)) |
| The totem eases smoothly to its new height, with a sound as it moves | done | the totem's rise with its rising loop and stop sound (`percentage::ProcessTotem`, `src/Worship/WorshipPercentage.cpp`); tests `VillageTotemEase.*` |
| A town's worshippers are drawn from its people who aren't busy, up to the share | done | `percentage::AdjustWorshipersWorshipping`: available villagers, nearest first by distance and life, disciples only once nobody else is left (ported from raffclar PR #308, checked against v1.42 on 2026-10-20: the rules in `src/ECS/WorshipperRules.cpp`); tests `WorshipTest.WorshipersNeeded`, `WorshipScoreFallsOffWithTheDistance`, `WorshipperTest.ADiscipleIsSentOnlyOnTheSecondPass`, `WorshipperRules.*` |
| Worshippers walk from their town to the site, at the worship walking speed | done | `villager_worship::GotoWorshipSiteForWorship` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`), the speed from the state table (`src/ECS/VillagerSpeed.cpp`); `test/test_worship.cpp` |
| A town sends nobody to a site still being built | done | a villager sets off only if its town's site is built, not under construction and all up; a worship disciple is still handed over, without raising its town's share (ours, from the exe; `src/ECS/Systems/Implementations/VillagerWorship.cpp`); test `WorshipperTest.NobodyIsSentToASiteStillBeingBuilt` |
| A worshipper that can't reach the site doesn't go | done | `CanIGetToTheWorshipSite`: within the town info's distance; test `WorshipperRules.FarSitesNeedAStone` |
| A site too far to walk to is still reached through the player's teleport stones | done | (added) `CanIGetToTheWorshipSite` and `teleport::FindRouteStone` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`); [our wiki](../../../magic/teleport.md#the-villagers-go-to-the-worship-site-through-the-stones); test `WorshipperRules.FarSitesNeedAStone` |
| Worshippers go home when fewer are needed, whoever is first in the site's go-home queue | done | as in the original, nobody joins the site's go-home queue (its request check has no caller), so any worshipper may go home once fewer are needed (`CheckVillagerGoBackToTownFromWorship`); tests `WorshipperTest.ATiredWorshipperDoesNotAskToGoHome`, `WorshipperRules.GoingBack` |
| Worshippers leaving the site walk back home | partial 90% | state 248 goes home, sleeping in a tent on the way when it must (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`); no test pins it; sleeping at the site was not found in the exe |
| A town in danger stops worshipping, and its worship comes back once the danger is over | done | (added) `src/ECS/Town/TownEmergency.cpp` saves the percentage and sets it to 0, then puts it back; test `RepairsTest.ProcessTownEmergencySavesAndRestoresTheWorship` |
| A villager dropped on the site by the hand becomes a worshipper disciple | done | ported from raffclar PR #308, checked against v1.42 on 2026-10-20: the worship disciple stops being one as it sets off for the site, and its town's share is raised to keep it there (`GotoWorshipSiteForWorship`, `percentage::IncrementWorshipPercentage`); tests `WorshipperTest.AWorshipDiscipleSettingOffStopsBeingOneAndRaisesTheShare`, `WorshipperTest.AWorshipDiscipleWithNoSiteStopsBeingOneAndLeavesTheShare`; the choice on the drop is in [../villager/disciples.md](../villager/disciples.md) |
| Scripts can make villagers dance and stop them | partial 75% | `DANCE_CREATE` makes the dance and the attached villagers and animals dance in it (`src/CHLApi.cpp`, `src/ECS/Systems/Implementations/VillagerDance.cpp`); the script's reminder, the creatures and two of the moves are not done ([dances.md](../../../beings/dances.md#pending)); not checked in game |

## The dance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dancers move in group shapes and paths set out in the game's dance files | partial 20% | the dancers stand on a ring round the dance centre (`site::DancePosition`, `src/Worship/WorshipSite.cpp`); the dance files are not ported |
| Each dancer plays its tribe's dance animations | pending | the dance clip needs a dance group, so dancers stand (`src/ECS/VillagerAnimations.cpp`) |
| The dance goes faster the more prayer power is drawn from the site | partial 50% | the intensity is worked out (`magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`, then `site::SetDanceIntensity`; tests `WorshipTest.EndOfTurnDanceIntensityAndBattery`, `WorshipBattery.drawingSpeedsTheDanceAndDrainsTheBattery`), but nothing plays the dance faster |
| A dance takes a few turns to get going before it counts | inferred | not found in the exe: the site's end of turn counts every dancer at once ([the battery](../../../magic/worship.md#each-turn)); whether the dance file's first key frames hold the dancers back is unverified |
| A dancer leaving the dance (to eat, for a reaction, or leaving worship) comes out of the dance it is in, even when its town has lost its site | done | the villager's own dance is left; only the site's count needs the site (ours, from the exe; `src/ECS/Systems/Implementations/VillagerWorship.cpp`); test `WorshipperTest.ADancerLeavesTheDanceEvenWhenItsTownsSiteIsGone` |
| The chanting is heard, louder with more worshippers | done | the nearest site's chant music, the voiced chant above eight dancers (`GameMusic::ProcessChantMusic`, `src/Audio/GameMusic.cpp`); `test/audio/test_game_music.cpp` |
| When the miracles ask more than the dancers give, the advisors remark on the strain | partial 50% | the strain's sound fraction is kept (`src/Worship/Citadel.cpp`) and read by the advisors' worship remark (`src/ECS/AudioQueries.cpp`, `src/Audio/Services/Guidance.cpp`); whether the original also plays a strain sound of its own is not read |

## What worship costs the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The more a dancer chants, the more life it loses | done | `ReduceVillagerLifeByChant` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`): the site's chant damage times the life rate of the worshipper's own villager info row (ours, from the exe); test `WorshipperTest.TheChantCostsAWorshipperByItsOwnInfo` |
| Dancers get hungry and eat the site's food | done | ported from raffclar PR #308, checked against v1.42 on 2026-10-20: a worshipper strictly below its hunger level walks to the site's pot when it holds a meal, eats, and goes back to the dance (`CheckAllowedToRestAtWorshipSite`, states 241 and 65, `src/ECS/Systems/Implementations/VillagerWorship.cpp`); test `WorshipperTest.AHungryWorshipperEatsOnlyWithAMealInThePot`; the Magic window's Worshippers readout counts the hungry and those eating |
| Worshippers starving or worn out die, and their town counts the deaths | partial 50% | a worshipper chanted to death dies and its town counts it (`TownDeaths`, read by `GET_TOWN_WORSHIP_DEATHS`); starving at the site is not modelled |

## Other worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature may dance with villagers to impress them | pending | the creature's own plan in the exe (`CreatureAgenda::ConstructSubActionsForDanceImpressivelyWithVillagers` 0x49C960, [dances](../../../beings/dances.md#a-creatures-turn-in-a-dance-with-villagers)); see `../creature/` |
| Villagers dance round an artefact in their town | partial 80% | ported from raffclar PR #278, not read against v1.42 (the original's artefact dance: [dances](../../../beings/dances.md#making-a-dance)); tests `ArtefactRules.*`; see [../town/artefacts.md](../town/artefacts.md) |

The camera that watches a dance is in `../camera/`.
