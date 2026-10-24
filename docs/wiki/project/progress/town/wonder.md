# The wonder and tribal powers

Each tribe can build a wonder, a huge building raised from scaffolds and wood like any other. A finished, working wonder adds
its tribe's power (from the tribe table) to its owner's power for that tribe. Each miracle's table says which tribes'
powers make it stronger or cheaper; the Indian power also speeds up villagers and the Aztec power raises worship.

**Progress: 5/11 done, 0 chosen, 6 partial, 0 inferred — 84%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/building-sites.md).

## The wonder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wonders of every tribe are placed by the land scripts as buildings, sitting into the land | partial 80% | `src/ECS/Archetypes/AbodeArchetype.cpp` (the Celtic, Japanese, Indian, Norse and Tibetan wonders, and a wonder of no tribe, morph with the terrain, as `Wonder::Get3DType`: test `AbodeMorph.ThePitchAndFiveTribesWondersDrapeOverTheLand`), the wonder part from `wonders::Create` (`src/ECS/Town/Wonders.cpp`), whose power from Create's scale argument is inferred (from the argument order, [building sites](../../../world/building-sites.md#wonder-details)); no test of the power |
| The town wants a wonder by its belief, once food, wood and abodes are seen to | partial 50% | `DesireToBuildWonder` (`src/ECS/TownDesire.cpp`) has the formula, but the town's belief in its player is fed as 0 (TODO in `town_desire`), so no town wants a wonder in play; `test/test_town_desire.cpp` sets the row's trigger only, not the formula |
| A wonder is built from a big scaffold made at the workshop and much wood | done | seven joined scaffolds offer the wonder, with the scaffold's tribe (the town's for a scaffold of no tribe) and sized by the town's wonder power there (`src/ECS/Scaffolds.cpp`, `scaffold_plans::GetNewPlannedBuilding`, `src/ECS/Town/ScaffoldPlans.cpp`), as `Town::GetNewPlannedBuilding` ([building sites](../../../world/building-sites.md)); its site is built with wood like any other; test `BuildingSitesTest.SevenScaffoldsOfferTheWonderOfTheScaffoldsTribeSizedByTheTownsWonderPower` (`test/test_building_sites.cpp`); See ../building/construction.md and [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A finished, working wonder adds its tribe's power to its owner's power for that tribe | done | a built wonder adds its power to its owner's most tribal power for its tribe (`wonders::AddToPlayer`, `PlayerMagic::maxTribalPower`, `src/ECS/Town/Wonders.cpp`), which the next player turn copies into the tribal power; see the row on each player's power below |
| A destroyed or lost wonder takes the power away again | partial 50% | deleting a wonder takes its power off (`wonders::RemoveFromPlayer`, as `Wonder::DeleteDependancys`); a wonder whose town changes hands keeps it with the old owner, not checked against the exe; no test |
| A wonder's power level is set as the wonder is built and kept; its size is min(power, 5) × (1 − power / 10) | partial 75% | the scaffold takes the town's wonder power where it is made (`scaffold_plans::GetWonderPower`: the town's wonder desire plus, within max(15 × it, 15) m, the objects' artefact worth and a tenth of their impressive value, at least 0.25, as `Town::GetWonderPower` / `GetWonderScale`); that the scale is the wonder's power is inferred; no test |

## Tribal powers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player keeps a power for each tribe, 1 until a wonder raises it | done | `PlayerMagic::tribalPower` and `maxTribalPower`, 1 for every tribe (`src/ECS/Components/PlayerMagic.h`); every player's turn sets both to the most power, at least 1 (`magic::players::ProcessTribalPowers`, `src/Magic/Core/Players.cpp`), as the original's player turn does; test `PlayerSystem.eachTurnTheMostTribalPowerAtLeastOneBecomesTheTribalPower` ([buildings](../../../world/building-sites.md#where-the-originals-objects-live)) |
| Indian power: villagers move faster | done | the villagers' speed takes the town player's Indian tribal power, at least 0.1 (`villager_speed::PlayerTermsOf`, `src/ECS/VillagerSpeed.cpp`), as `Villager.cpp` (MaxTribalPower[INDIAN], equal to it after each player turn); tests `VillagerSpeed.ThePlayersScaleOfEverySpeed`, `VillagerSpeed.TheTownsPlayerGivesThePowerAndTheBelief`; it stays 1 until wonders raise it (row above) |
| Aztec power: worship sites chant more prayer power | done | the site's chants per dancer times the player's Aztec tribal power (`src/Worship/WorshipSite.cpp`), as `WorshipSite::GetChantsVillagerCanSupply`; test `WorshipTest.CapacityAndBattery` |
| A miracle is made stronger or cheaper by the product of its caster's powers for the tribes its table marks | done | `magic::GetTribalPower` (`src/Magic/Core/Spell.cpp`, `src/Magic/Core/Players.cpp`), cheaper through `divideCostsByTribalPower`; tests `SpellChantRules.tribalPowerDividesCost`, `SpellChants.shieldDividesByTribalPower`, the tribal power test of `test/test_magic_spells.cpp`; See ../miracles/ |
| Forest miracle's wood worth more by tribal power | partial 80% | the forest's wood value multiplier times the spell's tribal power (`components::MagicTree`, `src/Magic/Spells/SpellForest.cpp`); no test of the product |
