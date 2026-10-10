# Workshop and scaffolds

The workshop turns wood into scaffolds, the building blocks the player carries by hand. A scaffold stands for one or
more scaffolds joined together; held over a town it shows the building the town most wants of that size, and put down
gently it becomes that building's site, which the villagers then build with wood. Scaffolds can be joined up to the
size of a wonder, tapped apart again, given to other towns, stolen by creatures and handed out by scripts and rewards.

**Progress: 53/115 done, 25 partial — 57%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## The workshop building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place workshops already built, with their starting wood, or as plans for the town to build | done | built ones with their wood (CREATE_ABODE, `src/ECS/Archetypes/AbodeArchetype.cpp`, `workshops::Create`) and plans (CREATE_PLANNED_ABODE, `ecs::plans`) |
| Each of the nine tribes has its own workshop model (the African one is stored under an "American" name) | done | the tribe's workshop info and model (`src/ECS/Archetypes/AbodeArchetype.cpp`); the workshop part is `src/ECS/Town/Workshops.cpp` |
| A workshop is a civic building raised from 3 joined scaffolds and 3500 wood (the Tibetan one 7000), by up to 6 builders | done | from the abode info's scaffolds and wood, built like any building ([construction.md](construction.md)); the scaffold rules are below |
| While a workshop is being built, the wood brought for it lies on a wood pile at the workshop itself | done | the wood pile is made with the workshop, during construction too (`workshops::CreatePileWood`, `src/ECS/Town/Workshops.cpp`) |
| When finished it joins its town's list of workshops; only finished workshops make scaffolds or are supplied | done | `workshops::MakeFunctional` adds it to the town's list; production and supply need it functional (`workshops::Process`, `GetBestWorkshop`) |
| The hand feels a different surface over each tribe's workshop: straw (Celtic, Aztec), smooth (African, Egyptian, Greek, Tibetan), wood (Japanese, Norse) or canvas (Indian) | todo | See [../hand/](../hand/) |
| Its "?" help gives seven lines ("Get a Workshop and you can place Scaffolding." … "You can place more than one Scaffolding unit next to each other, too.") | todo | See [../interface/help_system.md](../interface/help_system.md) |
| A workshop is a store for wood only | done | `workshops::IsResourceStore`: wood (or any) only |
| Destroying a workshop frees its scaffolds: they stay where they are, no longer counted as the workshop's | done | `workshops::DeleteDependants` frees every owned scaffold, and the pile goes with its wood (`workshops::ToBeDeleted`); see [damage_and_repair.md](damage_and_repair.md) |

## Wood for the workshop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wood dropped on the workshop by the hand (a handful, a tree), thrown at it or poured on it by the wood miracle goes into it | done | a held handful or tree goes in as a wood store (`resource_stores::IsResourceStore`, `src/ECS/ResourceStores.cpp`), as do a thrown pot and the wood miracle's grains that reach it (`pot_resource::AddResourceToPos`); See [../resources/resource_handling.md](../resources/resource_handling.md) and [../miracles/wood.md](../miracles/wood.md) |
| The workshop keeps its wood on a wood pile beside it, made the first time wood arrives | done | the "Magic Wood" pile at the workshop's special point 4 (`workshops::CreatePileWood`) |
| There is no limit to the wood a workshop can hold | done | no cap: the amount over the maximum is 0 for a workshop (`workshops::RemoveResourceFromPile`) |
| A wood flag beside the workshop rises with the wood still missing for the next scaffold (1 − wood ÷ 2500, at most 3.9 high) | done | the workshop's needs sign takes the wood still missing (`workshops::GetVisualWoodDesire`, `src/ECS/ShowNeeds.cpp`), drawn each frame (`show_needs::UpdateFrame`) |
| Ordinary villagers never supply the workshop: the town's wish to supply it is always nil | done | `DesireToSupplyWorkshop` (`src/ECS/Town/TownDesire.cpp`); see [../town/town_desires.md](../town/town_desires.md) |
| A villager of the player's own town dropped by the workshop becomes a craftsman disciple | todo | no craftsman disciple yet; see [../villager/disciples.md](../villager/disciples.md) |
| A craftsman carrying wood takes it to the workshop; otherwise he goes to the storage pit when it has wood and picks up as much as he can carry | todo | `ARRIVES_AT_WORKSHOP_FOR_DROP_OFF` and `ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS` are TODO rows of the state table (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`) |
| The craftsman takes the wood to the town's best finished workshop: the one in need of wood, nearer ones preferred (out to 500 m) | partial | `workshops::GetBestWorkshop` (out to 500 m, by `GetDesireToBeSupplied`) is ported, but no villager asks it yet |
| Craftsmen raise the town's desire for wood by their share of the town's adults | partial | the formula counts craftsmen (`src/ECS/Town/TownDesire.cpp`); there are no craftsmen yet. See [../town/town_desires.md](../town/town_desires.md) |
| The creature learns from seeing the player put wood in the workshop, and can do it itself | todo | See [building_by_creature.md](building_by_creature.md) and [../creature/town_actions.md](../creature/town_actions.md) |

## Making scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A finished workshop with room in its yard and at least 2500 wood starts a scaffold, taking the 2500 wood at once | done | `workshops::Process`: room and 2500 wood start a scaffold, the wood taken at once (`src/ECS/Town/Workshops.cpp`) |
| Each scaffold takes 200 game turns to make | done | the 200-turn countdown (`workshops::Process`) |
| A working sound plays at the workshop while it makes a scaffold, stopping when it is done | done | the working loop's sound tag while the countdown runs (`src/ECS/Town/Workshops.cpp`); See [../audio/sound_effects.md](../audio/sound_effects.md) |
| The workshop's chimney smokes while a scaffold is being made | done | the grey chimney smoke is lit while the scaffold countdown runs (`src/Graphics/RendererSmoke.cpp`) |
| Over the workshop the hand shows how far the current scaffold is made ("Production Completed: …%") | todo | the workshop's tooltip is pending (`src/ECS/Systems/Implementations/HandToolTips.cpp`); See [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |
| A finished scaffold (worth one) appears at the first free of three places in the yard, turned to that place, and belongs to the workshop's town | done | a scaffold of value 1 at the first free slot (special points 6 to 8), turned to it, of the workshop's town (`src/ECS/Town/Workshops.cpp`) |
| A horn sounds at the workshop when a scaffold is ready | done | the ready horn at the workshop (`src/ECS/Town/Workshops.cpp`) |
| The yard holds three: room = 3 − one being made − scaffolds the workshop still counts as its own | done | `workshops::GetSpaceInStore`; the table's figure is not read |
| Scaffolds sitting in the yard cannot be knocked about: they only move when picked up | done | a scaffold snapped in a slot cannot become a physics object (`scaffolds::CanBecomePhysicsObject`) |
| A scaffold taken from the yard keeps its place reserved: the workshop still counts it as its own | partial | the slot stays reserved when the scaffold starts its physics (`workshops::ScaffoldMoved`); the hand's pick-up hook (`scaffolds::OnPickedUp`) is not called, as the hand cannot pick one up |
| Once it has lain somewhere else for 150 game turns (and is not a script's scaffold), the workshop lets it go and its place is free again | done | the workshop's turn releases its head scaffold once it can no longer be adjusted (`workshops::Process`, `scaffolds::CanStillBeAdjusted`) |
| A scaffold put down within 2 m of a yard place snaps into it, joining that workshop if it has room | done | `workshops::CheckScaffoldSnapToPoint` when a scaffold's physics ends (`scaffolds::OnEndPhysics`) |
| No scaffold can make a building in the workshop's yard: within 10 m of the yard or 5 m of each of its three places | done | `workshops::IsPosWithinScaffoldAreas` and `IsScaffoldAwayFromWorkshops`, read when choosing a plan |
| Every tribe's workshop makes scaffolds the same way; only the model, its cost and the hand's feel differ | done | one workshop part for every tribe (`src/ECS/Town/Workshops.cpp`) |

## What a scaffold is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scaffold is a movable wooden frame worth one or more scaffolds ("Scaffold Value …" under the hand) | partial | the scaffold and its value (`components::Scaffold`, `scaffolds::GetValue`); the hand's "Scaffold Value" text is not ported |
| Its size shows its worth: drawn at 0.5 + 0.2 × its worth (0.7 for one, 1.9 for seven) | done | `scaffolds::SetValue`: 0.2 x value + 0.5 (`src/ECS/Scaffolds.cpp`) |
| All tribes share one scaffold model | done | the scaffold info's mesh (`scaffolds::Create`) |
| A scaffold is worth 2500 wood for each scaffold it stands for | done | `scaffolds::GetDefaultResource`: value x 2500 (`src/ECS/Scaffolds.cpp`, `src/ECS/ObjectResources.cpp`) |
| It weighs 400 and has its own physics material and collision sound | partial | a physics class of its own (`PhysicsClass::Scaffold`, `src/ECS/Physics/PhysicsObjects.cpp`); its weight and collision sound were not found; see [../physics/object_dynamics.md](../physics/object_dynamics.md) |
| The hand feels wood over it | todo |  |
| A scaffold remembers the town whose workshop made it | done | `Scaffold::town`, set by the workshop that makes it |
| The first time the player holds a scaffold the help system gives its message, once | todo | the hand cannot hold a scaffold; See [../interface/help_system.md](../interface/help_system.md) |
| Its "?" help: "A piece of Scaffolding." | todo |  |
| Scaffolds never wear out or rot while they lie about | done | nothing wears a scaffold away |
| An "old scaffold" object exists in the tables but nothing places it | n/a | no land or script uses it |

## Which building a scaffold makes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town is that of the nearest building within 50 m; failing that, the nearest town or planned building within 50 m, whoever it belongs to | done | `scaffolds::GetTownForBuilding` (`src/ECS/Scaffolds.cpp`) |
| It offers every kind of building that needs scaffolds and needs no more than its worth, keeping the one that fits the ground there and the town most wants | done | `scaffold_plans::ChoosePlanForScaffold` and `GetNewPlannedBuilding` with `GetDesireToBeBuilt` (`src/ECS/Town/ScaffoldPlans.cpp`); See [../town/town_desires.md](../town/town_desires.md) |
| Each building is tried at eight angles 45° apart from a random start, the first that fits being kept | done | eight tries 45 degrees apart from a random angle (`scaffold_plans::ChoosePlanForScaffold`) |
| The two smallest house kinds need one scaffold everywhere; the other four need, by tribe: Celtic, Greek and Tibetan 1, 2, 2, 2; African 1, 1, 2, 2; Aztec 1, 2, 2, 1; Japanese, Indian and Norse 2, 2, 2, 1; Egyptian 2, 1, 2, 2 | done | the abode info's scaffolds required, by tribe |
| A storage pit, crèche, workshop or graveyard needs 3; a field 4; a village centre 5; a miracle dispenser 6; a wonder 7 | done | the abode info's scaffolds required |
| A football pitch needs 8, possible only with football turned on; a totem is never made from scaffolds | done | the pitch needs the football flag, fixed off; a totem needs none, so it is skipped; see [../town/football.md](../town/football.md) |
| The wonder takes the tribe of the town the scaffold came from; other buildings take the receiving town's tribe | done | the scaffold's tribe for the wonder (`scaffold_plans::GetNewPlannedBuilding`); See [../town/wonder.md](../town/wonder.md) |
| The wonder's size comes from the town's wonder power at that spot | partial | the wonder's scale from `scaffold_plans::GetWonderPower`, but the objects' artefact and impressive values are not ported, so it is the floor; See [../town/wonder.md](../town/wonder.md) |
| Away from every town, a scaffold of five or more with a home town offers a village centre that will found a new town of its home town's tribe | done | the town centre that founds a new town of the home town's tribe (`scaffolds::ChoosePlan`, `BuildBuilding`, `src/ECS/Scaffolds.cpp`) |
| Each tribe's buildings cost their own wood (houses 1000 to 4800, Tibetan dearest; civic 2000 to 4000; village centre 6000, Tibetan 12000; field 2000; dispenser 5500; football pitch 6000; wonder 24000) | done | the abode info's wood values; building with it is [construction.md](construction.md) |
| A script can fix which building a scaffold makes; such a scaffold offers only that one | done | SET_SCAFFOLD_PROPERTIES' building limit (`scaffolds::SetScaffoldProperties`, `src/CHLApi.cpp`) |

## Holding a scaffold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose scaffold can always be picked up | todo | the hand cannot pick up a scaffold yet; See [../hand/picking_up.md](../hand/picking_up.md) |
| A scaffold on a building site can be picked up only before building starts and within 150 game turns of putting it down (a script's scaffold: any time before building starts) | partial | the rule is ported (`scaffolds::ValidForPlaceInHand`) but dormant: the hand cannot pick up a scaffold |
| Picking one up from a site cancels the site and its unbuilt building, and the town does not keep the plan | partial | ported in `scaffolds::OnPickedUp` but dormant: nothing calls it |
| Held over land, a see-through ghost of the building it would make stands under the hand, seen only by the holder | todo | the scaffold's phantom building is not drawn (`src/ECS/Systems/Implementations/RenderingSystem.cpp`), and the in-hand turn (`scaffolds::ProcessInHand`) is never called |
| The ghost turns slowly all the time (0.06 radians a turn, smoothed between turns) | todo | the turn is ported in `scaffolds::ProcessInHand`, which nothing calls, and the phantom is not drawn |
| When the building on offer changes, the old ghost shrinks away and the new one grows in, over four turns | todo | the cross-fade is ported (`scaffolds::UpdatePhantomBuildingPointers`) but the phantom is not drawn |
| The new ghost's sound is pitched by the kind of building; the old one has its own vanishing sound | todo | See [../hand/hand_sounds.md](../hand/hand_sounds.md) |
| Held over another scaffold it can join, the ghost shows the building the two would make there ("Combine Scaffolds") | todo |  |
| Where no building can go there is no ghost, and a small puff can appear under the hand | todo |  |
| The drop tooltip names the building on offer (Abode, Village Store, Crèche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Miracle Dispenser, Field) or says "Build" | todo | `scaffolds::GetOverwriteDropToolTip` is ported but the hand does not ask it; See [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |

## Putting a scaffold down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Put down gently with a building on offer, the scaffold becomes that building's site in the town | partial | a gentle landing from the hand builds (`scaffolds::OnInitialisePhysicsFromHand`, `OnEndPhysics`, `BuildBuilding`), but it is dormant: the hand cannot hold a scaffold; scripts and destroyed buildings build through `ForceBuildBuilding` |
| The building faces the way the ghost was turned when let go; if it doesn't fit that way, the way chosen with the plan | partial | `BuildBuilding` tries the phantom's angle, then the plan's; dormant like the drop |
| A scaffold worth more than the building needs puts down only what it needs: the rest stays in the hand as a new scaffold | partial | the surplus scaffold is made in `scaffolds::OnLeftHand`, which nothing calls |
| A planting sound plays: for the player who dropped it as a plain sound, for others at the scaffold | done | the planting sound in `scaffolds::BuildBuilding`, plain for the dropper's own hand, 3D at the scaffold for others |
| The town's own planned buildings under the new site are removed | done | `scaffolds::DeletePlannedBuildingsUnderMe` |
| The scaffold stays on the site, drawn at the building's scale and shrinking as the building rises until it is gone at two-thirds built | partial | the scaffold joins its building's site and stays until the site goes (`building_sites::AddScaffold`); its shrinking draw was not found; the building's own partial look is in [construction.md](construction.md) |
| The ghost of the planned building stays drawn see-through over the site while it is built | todo | the phantom is not drawn |
| Builders build it with wood as any site | done | See [construction.md](construction.md) |
| Wood dropped on a scaffold that stands on a site goes to the site | todo | a scaffold is not a store, so wood dropped on it is not passed to its site |
| When the building is finished, its scaffold is gone | done | the site's scaffolds are deleted with it when the building is finished (`building_sites::ToBeDeleted`) |
| If the unfinished building is destroyed, its scaffold at once makes a fresh site there | done | the site's head scaffold force-builds it again (`src/ECS/Abodes.cpp`); See [damage_and_repair.md](damage_and_repair.md) |
| A scaffold put down where no building is on offer just lies there with a little puff | partial | `scaffolds::OnEndPhysics` makes the puff when no building is on offer; dormant for the hand |
| A thrown scaffold never builds: it flies, lands and lies there, with a little puff where it comes down | partial | only a gentle drop from the hand builds; a split piece that flies lands with the puff and does not build (`scaffolds::OnEndPhysics`); the hand cannot throw one; See [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| A founding village centre first makes the new town: it gets the forests around it and the player's starting belief there | partial | the new town is made with its forests (`scaffolds::BuildBuilding`); the starting belief and the magic types it should copy are TODO; See [../town/growth_and_housing.md](../town/growth_and_housing.md) |
| A scaffold a script marks to destroy what is under it pushes movable things out of the way and removes the rest when it is let go | partial | `scaffolds::DestroyThingsInWay`, run when the scaffold leaves the hand, so dormant |

## Belief, reactions and alignment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Planting a scaffold makes the villagers around react to it (out to 60 m) | partial | the scaffold reaction is made when it builds (`effects::reactions::CreateReaction` in `scaffolds::BuildBuilding`); See [../villager/reactions.md](../villager/reactions.md) |
| Every town within 300 m is impressed: a tenth of its people (at least one) gain belief | todo | `scaffolds::ImpressTowns` walks the towns within 300 m, but the villagers' impression is a TODO; See [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| How impressive it is grows with its worth and with the building it stands for | todo |  |
| The reaction leans the player a little towards good, as caring for homes | todo | the reaction's alignment figures; see [../worship/](../worship/) |
| Any town takes a scaffold, believers or not: giving scaffolds to other villages impresses them | done | the town of the nearest building, any player's (`scaffolds::GetTownForBuilding`) |
| The creature learns building from seeing the player drop a scaffold | todo | See [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |

## Joining and breaking scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Let go onto another scaffold (not held, not in flight), the two join when together they are worth at most 7 (8 with football), neither is a script's and the other's building hasn't started | partial | `scaffolds::ValidToApplyThisToObject` and the combine maximum (7 with football off) are ported, but the hand does not apply a scaffold; see [../town/football.md](../town/football.md) |
| The joined scaffold is the one on the ground, worth both, with the held one's building on offer; the held one is gone | partial | `scaffolds::Combine`, dormant |
| The joined scaffold at once tries to become its building's site where it lies | partial | dormant, with the combine |
| Joining shows a puff, plays one of four sounds in turn at the hand and gives a force-feedback jolt | partial | the puff and the four combine sounds in `scaffolds::ApplyThisToObject`, dormant; no jolt; See [../hand/hand_sounds.md](../hand/hand_sounds.md) |
| A scaffold worth more than one, at rest, not a script's and with no started building can be tapped ("Tap To Break") | done | `scaffolds::ValidToTap`, registered with the hand's taps; See [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md) |
| A tap breaks one single scaffold off at a random quarter turn, the two moving apart; any site it stood for is cancelled; one of four sounds plays in turn at the hand | done | `scaffolds::Tap` and `Split`, with the four tap sounds in turn |
| The piece broken off takes a place in its workshop's yard if there is room | done | the piece takes the workshop's first free slot when it has room (`scaffolds::Split`) |
| A burning scaffold passes its fire to the piece broken off | todo | `scaffolds::Split` passes no fire |
| Let go onto a storage pit, a scaffold is taken in as its wood (2500 for each scaffold it stands for) | partial | `scaffolds::ApplyThisToObject` gives it to the pit as wood (`take_resource::StoragePit`), dormant from the hand; See [../resources/resource_handling.md](../resources/resource_handling.md) |
| A scaffold is not taken in by the workshop | done | the workshop is not among the targets of `scaffolds::ValidToApplyThisToObject` |

## Scaffolds in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose scaffold flies, rolls and lands as a movable object; one in the yard or on a site never does | done | `scaffolds::CanBecomePhysicsObject`: not in a slot nor on a site; See [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Blows don't hurt a scaffold | done | no blow reaction for scaffolds (`Buildings::ReactToPhysicsImpact`); See [../physics/impact_damage.md](../physics/impact_damage.md) |
| A scaffold catches fire (it lights at 200, heat capacity 400) and fire hurts it at a fifth of full strength | todo | no fire values for a scaffold were found (`src/ECS/Fire/FireObjectTraits.cpp`); see [damage_and_repair.md](damage_and_repair.md) |
| A loose scaffold standing in a vortex's pull is sucked in and carried to the next land like other loose objects, coming out as a scaffold of the same kind; one in a workshop's yard, on a building site or tied to a building plan is neither taken nor written down | todo | the vortex (`src/ECS/Vortex.cpp`) takes no scaffold; See [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |
| A saved land writes each loose scaffold not owned by a workshop as a scaffold line (town, place, angle, size) | todo | the land save skips scaffolds (`src/ECS/LandScriptSave.cpp`); See [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |

## The creature and other gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can pick up, carry, inspect and build with scaffolds, give them away and bring them home, but not attack or play with them | todo | See [building_by_creature.md](building_by_creature.md) and [../creature/object_actions.md](../creature/object_actions.md) |
| The creature steals scaffolds made for other gods' towns (never its own god's): it picks one up, walks off and puts it down | todo | See [../creature/town_actions.md](../creature/town_actions.md) |
| The hand can take another god's scaffold where it may pick things up | todo | See [../hand/picking_up.md](../hand/picking_up.md) |
| Rival gods fill their workshops, join scaffolds and place them in their towns | todo | See [../rival_gods/ai.md](../rival_gods/ai.md) and [../rival_gods/towns_and_influence.md](../rival_gods/towns_and_influence.md) |
| Villagers never carry scaffolds: the game leaves that empty | n/a | See [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md) |

## Scripts and rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land files place scaffolds (town, place, angle, size): the two-god playground has twelve on an Egyptian building site | todo | `CREATE_SCAFFOLD` is empty (`src/LHScriptX/FeatureScriptCommands.cpp`); see [../multiplayer/maps/two_gods.md](../multiplayer/maps/two_gods.md) |
| Challenge scripts make scaffolds and set their building, size and "destroys when placed" | done | CREATE of a scaffold (`src/CHLApi.cpp`) and SET_SCAFFOLD_PROPERTIES (`scaffolds::SetScaffoldProperties`); see [../scripts/challenge_natives_towns_and_players.md](../scripts/challenge_natives_towns_and_players.md) |
| A script can make a scaffold build at once where it stands ("enable … active") | done | SET_ACTIVE on a scaffold force-builds it (`scaffolds::ForceBuildBuilding`, `src/CHLApi.cpp`) |
| Scripts can stop a scaffold being picked up or moved | todo |  |
| Land 2: Khazar places a village centre scaffold (size 5) for the player, then a storage pit (3) and a house (2), and later a workshop (3) that clears its ground | partial | the script commands it uses are in (CREATE of a scaffold, SET_SCAFFOLD_PROPERTIES, SET_ACTIVE); See [../rival_gods/khazar.md](../rival_gods/khazar.md) |
| Lands 3 and 5: the home town's storage pit and workshop are laid out as scaffolds and built on arrival | partial | as above, through the script's scaffolds; See [../story/land_3.md](../story/land_3.md) and [../story/land_5.md](../story/land_5.md) |
| A reward chest opened in a town with a workshop gives a single scaffold, counted as that workshop's | todo | no reward chests; See [../story/rewards.md](../story/rewards.md) |

## Advisors, help and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 2's "The Workshop" scroll: an engineer explains the workshop, asks for wood, makes a scaffold, the advisors show where to place it, and he gives the forest miracle once the homeless are housed | todo | See [../story/land_2.md](../story/land_2.md); every step: [../story/gold_scrolls/the_workshop.md](../story/gold_scrolls/the_workshop.md) |
| Land 4's opening: "Excellent. The Workshop is built. Now let's concentrate on making a Village Store." "You'll need three combined Scaffolds to build it." | todo | See [../story/land_4.md](../story/land_4.md) |
| Did-you-know pages on the workshop, joining scaffolds and giving them to other villages | todo | See [../interface/help_system.md](../interface/help_system.md) |
| The sounds: the workshop at work, the ready horn, the ghost appearing and vanishing, planting, joining (four) and tapping (four) | partial | the working loop, the ready horn, planting, joining and tapping play (`src/ECS/Town/Workshops.cpp`, `src/ECS/Scaffolds.cpp`); the ghost's appearing and vanishing sounds do not. See [../audio/sound_effects.md](../audio/sound_effects.md) |
