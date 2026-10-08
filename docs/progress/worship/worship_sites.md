# Worship sites

Each tribe a player wins over gets a worship site beside the player's temple, where that tribe's villagers dance to make
prayer power. The site holds the tribe's miracle icons, a food store for the dancers and an altar. Artefacts put down at a site gain worth there: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 10/25 done, 4 partial — 48%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## The site in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place worship sites (built and planned) for the players' citadels | partial | `CREATE_WORSHIP_SITE` makes a built site at the citadel (`src/Magic/Script/MapScriptMagic.cpp` to `worship::citadel::CreateBuiltWorshipSite`) and the first turn gives every town of a player with a citadel its site (`citadel::PostLoadCleanup`); `CREATE_PLANNED_WORSHIP_SITE` does nothing: sites are born built |
| A site stands round the temple at its tribe's distance from the citadel, facing out from it | done | one of six slots at the heart's angle plus n sevenths of a turn (`src/Worship/Citadel.cpp`), at the heart's arrival point turned to the slot (`src/Worship/WorshipSite.cpp`) |
| One site per tribe: every town of the same tribe a player owns is assigned to that tribe's site | done | `citadel::FindOrCreateWorshipSite` and `AddTown`; a new site takes the player's towns of its tribe, oldest first (`site::Create`) |
| A site is made for a newly claimed town's tribe if the player has none yet | partial | a town looks for its site when it gets its first villager (`worship::town::CheckAddWorshipSite`, `src/ECS/Town/TownVillagers.cpp`) and on the land's first turn; a town taken over gets none (`town_belief::TakeOverTown` leaves it pending); see `../town/` |
| A town the player loses is taken off its site; a site with no towns left goes (unconfirmed) | todo | `site::RemoveTown` exists but nothing calls it when a town changes owner (`town_belief::TakeOverTown`) |
| The site has its tribe's model, base and the player's colours | done | B_WORSHIP wearing its temple's texture (`src/Worship/WorshipSite.cpp`), with the tribe's altar from the site info's mesh |
| A ring of light stands round the altar in the player's colour, as bright as the site's prayer battery is full | todo | see [../rendering/light_beams.md](../rendering/light_beams.md) |
| The site has a gate the worshippers come in by, a dance floor, a rest place and a food place | done | the site's points: arrival, dance centre, hiding place (`src/Worship/SpecialPoints.cpp`), and its food pot (`src/Worship/WorshipSite.cpp`) |
| Worship site upgrades: built on from a planned upgrade, they make the site bigger (unconfirmed what else they change) | todo | sites are born built, with no building site or upgrade |
| The site can be set on fire, burning about its centre | done | a site burns about its own centre (`object::WorshipSiteCentre`, `src/ECS/Fire/FireObjectTraits.cpp`) |
| Scripts can stop a player building worship sites | done | `SET_CAN_BUILD_WORSHIPSITE` (`src/Magic/Script/CHLWorship.cpp`), read by `worship::town::CheckAddWorshipSite` |
| Scripts can read how many of a town's people died worshipping | done | `GET_TOWN_WORSHIP_DEATHS` (`src/Magic/Script/CHLWorship.cpp`): the town's chant deaths (`TownDeaths`) |

## Food for the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The site has its own food pile that the dancers eat from | partial | the site has its food pot (`src/Worship/WorshipSite.cpp`), but the dancers don't eat: villagers have no belly yet (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) |
| How much food the site needs is worked out from its dancers and how hungry they are | done | `site::CalculateDesireForFood` from its dancers' hunger, read by the advisors (`src/Audio/Services/Guidance.cpp`); test `WorshipTest.DesireForFoodOfASite` |
| Villagers carry food from their town's store to the site when it runs short | todo | the supply states are not ported; see `../villager/` |
| Disciples and the creature bring fish and crops straight to the site | todo |  |
| Food poured on the site by the food miracle or dropped there by hand goes into its store | done | `site::AddResource` (`src/Worship/WorshipSite.cpp`), reached from `pot_resource::AddResourceToPos` and `object_resources::AddResource`; tests `WorshipSiteAddResource.FoodGoesToThePotWoodOnlyToABuildingSite`, `WorshipTest.FoodGivenToASiteFillsItsFoodPot` |
| Casting food on a site teaches the creature that feeding worshippers is a thing its god does | todo | the deed has only a name in `src/Creature/CreatureDeeds.h`; nothing records it |
| The site shows a sign of what it needs (food) over it | todo | the food need only feeds the advisors' remark |
| Computer players drop the resources they give at their own site | todo | no computer players |

## Icons, artefacts and shelter

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Artefacts kept at the site make power-ups cheaper by the artefact multiplier | todo | no artefacts; the multiplier is in the table, unused |
| In danger, a town's villagers run to the site to hide | todo | the hiding place is used only by worshippers beyond the site's room; a town in danger stops worshipping instead (`src/ECS/Town/TownEmergency.cpp`) |
| A site with more worshippers than it has room for is overloaded; only so many dancers are drawn | done | beyond `maxDancersVisible` a worshipper waits at the hiding place (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) |
| The creature can be sent to the site, and treats it as a place to work, e.g. casting food there | todo | see `../creature/` |
| A cheat gives a site endless prayer power | partial | each site's infinite flag, set from the debug Miracles window (`src/Debug/Miracles.cpp`) |

The miracle icons round the site, and summoning from them, are in `../miracles/`.
