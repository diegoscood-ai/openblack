# Worshippers and dancing

The villagers who leave their town to dance at the worship site. The player sets how many of a town worship; the
dancers chant prayer power, tire, get hungry and can die of it.

**Progress: 10/23 done, 5 partial — 54%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## Going to worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each town has a totem whose height sets the share of its people who worship; the player holds it with the hand and pulls it up or down | partial | the town's worship percentage (`worship::percentage::SetWorshipPercentage`, `src/Worship/WorshipPercentage.cpp`) sets the share and raises the totem; land scripts set it (`src/LHScriptX/FeatureScriptCommands.cpp`); dragging the totem with the hand is not wired |
| The totem eases smoothly to its new height, with a sound as it moves | done | the totem's rise with its rising loop and stop sound (`percentage::ProcessTotem`, `src/Worship/WorshipPercentage.cpp`) |
| A town's worshippers are drawn from its people who aren't busy, up to the share | done | `percentage::AdjustWorshipersWorshipping`: available villagers, nearest first by distance and life; tests `WorshipTest.WorshipersNeeded`, `WorshipScoreFallsOffWithTheDistance` |
| Worshippers walk from their town to the site, at the worship walking speed | done | `villager_worship::GotoWorshipSiteForWorship` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`), the speed from the state table (`src/ECS/VillagerSpeed.cpp`) |
| A worshipper that can't reach the site doesn't go | done | `CanIGetToTheWorshipSite`: within the town info's distance |
| A site too far to walk to is still reached through the player's teleport stones | done | (added) `CanIGetToTheWorshipSite` and `teleport::FindRouteStone` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`); [our wiki](../../bw1-notes/miracles.md#the-villagers-go-to-the-worship-site-through-the-stones-faithful) |
| Worshippers ask to go home when tired or hungry, and the site lets them go in turn | done | below the life threshold a worshipper joins the site's go-home queue, sorted by its desire for life (`CheckRequestGoHome`, `CheckVillagerGoBackToTownFromWorship`) |
| Worshippers go home to sleep, or sleep at the site (unconfirmed) | done | state 248 goes home, sleeping in a tent on the way when it must (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`) |
| A town in danger stops worshipping, and its worship comes back once the danger is over | done | (added) `src/ECS/Town/TownEmergency.cpp` saves the percentage and sets it to 0, then puts it back |
| A villager dropped on the site by the hand becomes a worshipper disciple | todo | no worship disciple; see `../villager/` for disciples |
| Scripts can make villagers dance and stop them | todo | `DANCE_CREATE` is a stub in `src/CHLApi.cpp` |

## The dance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dancers move in group shapes and paths set out in the game's dance files | partial | the dancers stand on a ring round the dance centre (`site::DancePosition`, `src/Worship/WorshipSite.cpp`); the dance files are not ported |
| Each dancer plays its tribe's dance animations | todo | the dance clip needs a dance group, so dancers stand (`src/ECS/VillagerAnimations.cpp`) |
| The dance goes faster the more prayer power is drawn from the site | partial | the intensity is worked out (`magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`, then `site::SetDanceIntensity`; tests `WorshipTest.EndOfTurnDanceIntensityAndBattery`, `WorshipBattery.drawingSpeedsTheDanceAndDrainsTheBattery`), but nothing plays the dance faster |
| A dance takes a few turns to get going before it counts | todo |  |
| Coloured lights follow the dancers | n/a | the game keeps them but never draws them; see [../rendering/light_beams.md](../rendering/light_beams.md) |
| The chanting is heard, louder with more worshippers | done | the nearest site's chant music, the voiced chant above eight dancers (`GameMusic::ProcessChantMusic`, `src/Audio/Services/GameMusic.cpp`) |
| A strain sound plays when the miracles ask more than the dancers give | partial | the strain's sound fraction is kept (`src/Worship/Citadel.cpp`) and read by the advisors' worship remark (`src/ECS/AudioQueries.cpp`, `src/Audio/Services/Guidance.cpp`); no strain sound of its own plays |

## What worship costs the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The more a dancer chants, the more life it loses | done | `ReduceVillagerLifeByChant` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) |
| Dancers get hungry and eat the site's food | todo | villagers have no belly yet |
| Worshippers starving or worn out die, and their town counts the deaths | partial | a worshipper chanted to death dies and its town counts it (`TownDeaths`, read by `GET_TOWN_WORSHIP_DEATHS`); starving at the site is not modelled |
| Dancers rest at the altar to get their strength back | todo | the rest at the site is not ported (`CheckAllowedToRestAtWorshipSite` always says no) |

## Other worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers who are impressed enough by a creature worship it, dancing round it (unconfirmed exact trigger) | todo |  |
| Villagers dance round an artefact in their town | todo |  |

The camera that watches a dance is in `../camera/`.
