# The wonder and tribal powers

Each tribe can build a wonder, a huge building raised from scaffolds and wood like any other. A finished, working wonder adds
its tribe's power (from the tribe table) to its owner's power for that tribe. Each miracle's table says which tribes'
powers make it stronger or cheaper; the Indian power also speeds up villagers and the Aztec power raises worship.

**Progress: 8/12 done, 3 partial — 79%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## The wonder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wonders of every tribe are placed by the land scripts as buildings, sitting into the land | done | `src/ECS/Archetypes/AbodeArchetype.cpp` (morphs with the terrain), the wonder part from `wonders::Create` (`src/ECS/Town/Wonders.cpp`) |
| The town wants a wonder by its belief, once food, wood and abodes are seen to | done | `DesireToBuildWonder` (`src/ECS/Town/TownDesire.cpp`); `test/test_town_desire.cpp` |
| A wonder is built from a big scaffold made at the workshop and much wood | done | seven joined scaffolds offer the wonder, with the scaffold's tribe (`src/ECS/Scaffolds.cpp`, `src/ECS/Town/ScaffoldPlans.cpp`); its site is built with wood like any other; See ../building/construction.md and [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A finished, working wonder adds its tribe's power to its owner's power for that tribe | done | a built wonder adds its power to its owner's per-tribe wonder sum (`wonders::AddToPlayer`, `PlayerMagic::wonderPower`, `src/ECS/Town/Wonders.cpp`); see the row on each player's power below |
| A destroyed or lost wonder takes the power away again | partial | deleting a wonder takes its power off (`wonders::RemoveFromPlayer`); a wonder whose town changes hands keeps it with the old owner |
| A wonder's power level is set as the wonder is built and kept | partial | the scaffold sets the power from the town's wonder power when the building is made (`scaffolds`, `scaffold_plans::GetWonderPower`, `src/ECS/Town/ScaffoldPlans.cpp`), but the objects' artefact and impressive values it sums are not ported (0), so it is the 0.25 floor |
| Villagers can shelter in a wonder | todo | `GO_AND_HIDE_IN_NEARBY_BUILDING` is a TODO row of the state table |

## Tribal powers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player keeps a power for each tribe, 1 until a wonder raises it | done | `PlayerMagic::tribalPower`, 1 for every tribe (`src/ECS/Components/PlayerMagic.h`), read by the miracles. Our wiki differs: a wonder adds its power to a separate per-tribe wonder sum, whose reader was not found; the tribal power the miracles read is not raised by it ([buildings](../../bw1-notes/buildings.md#scaffolds-choosing-the-building-and-the-wonders-power)) |
| Indian power: villagers move faster | partial | the villagers' speed has the wonder term, fixed at 1 (`src/ECS/VillagerSpeed.cpp`) |
| Aztec power: worship sites chant more prayer power | done | the site's chants per dancer times the player's Aztec tribal power (`src/Worship/WorshipSite.cpp`) |
| A miracle is made stronger or cheaper by the product of its caster's powers for the tribes its table marks | done | `magic::GetTribalPower` (`src/Magic/Core/Spell.cpp`, `src/Magic/Core/Players.cpp`), cheaper through `divideCostsByTribalPower`; See ../miracles/ |
| Forest miracle's wood worth more by tribal power | done | the forest's wood value multiplier times the spell's tribal power (`components::MagicTree`, `src/Magic/Spells/SpellForest.cpp`) |
