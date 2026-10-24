# Worship sites

Each tribe a player wins over gets a worship site beside the player's temple, where that tribe's villagers dance to make
prayer power. The site holds the tribe's miracle icons, a food store for the dancers and an altar. Artefacts put down at a site gain worth there: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 12/22 done, 0 chosen, 4 partial, 0 inferred — 70%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/worship.md).

## The site in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place built worship sites for the players' citadels; a planned one is read and ignored | done | `CREATE_WORSHIP_SITE` makes a built site at the citadel (`src/Magic/Script/MapScriptMagic.cpp` to `worship::citadel::CreateBuiltWorshipSite`) and the land's load, right after its script, gives every town of a player with a citadel its site (`citadel::PostLoadCleanup`); `CREATE_PLANNED_WORSHIP_SITE` does nothing, as in the original, which reads the line and ignores it ([the script](../../../magic/worship.md#script)); test `FootpathsTest.APlannedWorshipSiteMakesNothingAndKeepsTheLastCreatedObject`; the Land 2 check of [the worship page](../../../magic/worship.md#checks-in-the-game) |
| A site stands round the temple at its tribe's distance from the citadel, facing out from it | done | one of six slots at the heart's angle plus n sevenths of a turn (`src/Worship/Citadel.cpp`), at the heart's arrival point turned to the slot (`src/Worship/WorshipSite.cpp`); the Land 2 check of [the worship page](../../../magic/worship.md#checks-in-the-game) |
| One site per tribe: every town of the same tribe a player owns is assigned to that tribe's site | done | `citadel::FindOrCreateWorshipSite` and `AddTown`; a new site takes the player's towns of its tribe, oldest first (`site::Create`); the Land 2 check (the Greek site added for town 2) |
| A site is made for a newly claimed town's tribe if the player has none yet | done | a town looks for its site when it gets its first villager (`worship::town::CheckAddWorshipSite`, `src/ECS/Town/TownVillagers.cpp`, as the original's `Town::AddVillagerToTown`) and on the land's first turn; a town taken over joins the new player's citadel (`citadel::AddTown` from `town_belief::TakeOverTown`, ported from raffclar PR #236; the original's `Town::AddTownToPlayer` 0x73A8F0 calls `Citadel::AddTown` at 0x73A928 for a player that is not neutral and has a citadel); see `../town/`; tests `VillagerBirthTest.ATownLooksForItsWorshipSiteAsItGetsItsFirstVillager` (`test/test_villager_birth.cpp`), `TownBeliefTest.TakeOverTownMovesTheTownFromItsSiteToItsNewOwnersSite` (`test/test_town_belief.cpp`) |
| A site the citadel asks for is built by the villagers through a building site, and takes its slot as it is built | pending | sites are born built (`Citadel::RequestANewWorshipSite`, [the temple](../../../magic/temple-and-citadel.md)); issue #175 |
| A town the player loses is taken off its site, with its miracles' icons; the site stays, even with no towns left | partial 90% | ported from raffclar PR #236: a town changing owner leaves its site (`site::RemoveTown` from `town_belief::TakeOverTown`), as the original's `WorshipSite::RemoveTown` (0x77C950; its caller `Town::RemoveTownFromPlayer` 0x73A7D0 deletes no site, read in the v1.42 bytes); tests `WorshipTest.ATownLeavingItsSiteTakesTheIconsNoOtherTownOfTheSiteHolds` (`test/test_worship.cpp`), `TownBeliefTest.TakeOverTownMovesTheTownFromItsSiteToItsNewOwnersSite` (`test/test_town_belief.cpp`) |
| The site has its tribe's model, base and the player's colours | done | B_WORSHIP wearing its temple's texture (`src/Worship/WorshipSite.cpp`), with the tribe's altar from the site info's mesh; `test/test_worship_site_skin.cpp` |
| A ring of light stands round the altar in the player's colour, as bright as the site's prayer battery is full | pending | see [../rendering/light_beams.md](../rendering/light_beams.md) |
| The site has a gate the worshippers come in by, a dance floor, a rest place and a food place | done | the site's points: arrival, dance centre, the altar's rest point (`src/Worship/SpecialPoints.cpp`), and its food pot at the site's food point (`site::GetFoodPos`, `src/Worship/WorshipSite.cpp`); the hidden worshippers wait at its door; tests `WorshipTest.ASiteKeepsItsFoodAtItsLocalPoint`, `WorshipperTest.TheHiddenWorshipAtTheSitesDoor` |
| The site can be set on fire, burning about its centre | done | a site burns about its own centre (`object::WorshipSiteCentre`, `src/ECS/Fire/FireObjectTraits.cpp`); `test/test_object_metrics.cpp` |
| Scripts can stop a player building worship sites | done | `SET_CAN_BUILD_WORSHIPSITE` (`src/Magic/Script/CHLWorship.cpp`), read by `worship::town::CheckAddWorshipSite`, as the exe's `GScript::SetBuildWorshipSite` 0x6FEC40 (the town's flag set to the switch's opposite, then its check when on); test `WorshipTest.AScriptForbidsATownItsWorshipSiteAndAllowsItAgain` (`test/test_worship.cpp`) |
| Scripts can read how many of a town's people died worshipping | done | `GET_TOWN_WORSHIP_DEATHS` (`src/Magic/Script/CHLWorship.cpp`): the town's chant deaths (`TownDeaths`); `test/test_villager_death.cpp` |

## Food for the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The site has its own food pile that the dancers eat from | done | the site's food pot (`src/Worship/WorshipSite.cpp`), part of the site: it answers for the site's food, gives it, and stays when emptied (`src/ECS/ObjectResources.cpp`); hungry worshippers eat from it (ported from raffclar PR #308, checked against v1.42 on 2026-10-20); tests `TakeResourceTest.AWorshipSiteGivesTheFoodInItsPot`, `AWorshipSitesFoodPotIsPartOfTheSiteAndStaysWhenEmptied`, `WorshipperTest.AHungryWorshipperEatsOnlyWithAMealInThePot` |
| How much food the site needs is worked out from its dancers and how hungry they are | done | `site::CalculateDesireForFood` from its dancers' hunger, read by the advisors (`src/Audio/Services/Guidance.cpp`); test `WorshipTest.DesireForFoodOfASite` |
| The creature puts crops from a field by the site for the dancers | pending | see `../creature/town_actions.md`; no disciple bringing food to a site was found in the exe |
| Food poured on the site by the food miracle or dropped there by hand goes into its store | done | `site::AddResource` (`src/Worship/WorshipSite.cpp`), reached from `pot_resource::AddResourceToPos` and `object_resources::AddResource`; tests `WorshipSiteAddResource.FoodGoesToThePotWoodOnlyToABuildingSite`, `WorshipTest.FoodGivenToASiteFillsItsFoodPot` |
| Casting food on a site teaches the creature that feeding worshippers is a thing its god does | pending | the deed has only a name in `src/Creature/CreatureDeeds.h`; nothing records it |
| Three signs over the altar rise with the dancers' hunger, their want of rest and how many are hiding | done | `ecs::show_needs` made with the altar, set each turn by `worship::site::ProcessNeedsSigns` (rules in `src/Worship/AltarNeeds.cpp`), drawn from the object draw list ([the need signs](../../../magic/worship.md#the-altars-need-signs-in-openblack)); tests `AltarNeeds.*`, `WorshipTest.TheAltarsNeedSignsTakeTheSitesDesiresEachTurn` |
| Computer players drop the resources they give at their own site | pending | no computer god's drop is ported; see `../rival_gods/` |

## Icons, artefacts and shelter

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 1000 turns the artefacts kept at a site gain worth from its dance | partial 80% | ported from raffclar PR #278: `ArtefactSystem::PowerUpSite` at the end of the site's turn, by the artefact multiplier, the site's dancers and the player's count, as the original's end of turn ([the battery](../../../magic/worship.md#each-turn)); no test pins it; see [../town/artefacts.md](../town/artefacts.md) |
| A site with more worshippers than it has room for is overloaded; only so many dancers are drawn | done | beyond `maxDancersVisible` a worshipper worships hidden at the site's door, the door point of the site's mesh (the site's position without one), and starts once it has arrived by the walks' own whole-unit test, within its speed and one map unit (ours, from the exe; `villager_worship::HidingPlace`, `src/ECS/Systems/Implementations/VillagerWorship.cpp`); test `WorshipperTest.TheHiddenWorshipAtTheSitesDoor` |
| The creature can be sent to the site, and treats it as a place to work, e.g. casting food there | pending | see `../creature/` |

The miracle icons round the site, and summoning from them, are in `../miracles/`.
