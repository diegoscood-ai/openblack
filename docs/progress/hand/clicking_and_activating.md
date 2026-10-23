# Clicking and activating

What the Action button does when the hand isn't picking something up: tapping things (knocking on houses, splitting
rocks, breaking scaffolds, opening reward chests, starting challenges), activating them (the temple's entrance, miracle
bubbles and icons, leash posts), double clicks, locked selection, and the force-feedback mouse the game supported.

**Progress: 35/47 done, 0 chosen, 7 partial, 0 inferred — 84%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/grab-hold-and-throw.md).

## Tapping in general

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A quick press and release of the Action button over a thing taps it rather than taking hold | done | `HandSystem::Update` (the grab state): a release within 225 ms sends `HandSystem::SendTap`; tappable classes are registered in `ecs::hand_tap` (rocks, abodes, scaffolds, spell icons, one-shot orbs, "Did you know?" signs, the temple's entrance); test `test_hand_press_chain` |
| Each kind of thing says whether it can be tapped at all; a thing that can't is left alone | done | `ecs::hand_tap` (`src/ECS/Systems/HandTap.h`): each owner registers its classes, an unregistered class is not tappable; `hand_tap::SendsTap` also checks the influence and the cannot-be-picked-up flag |
| Village centres, dead trees and bonfires can't be tapped | done | Dead trees and bonfires register no tap handler. A town centre is an abode in our tree, and the abode's tap test refuses it (`abodes::InterfaceValidToTap` with `abode_knock::IsTappable`), so a press on it is not a tap |
| While the game is paused the idle hand takes only the land grip: the camera moves, but the action press and its double click start nothing | done | `hand_press::IdleTakesActionPress` (`src/ECS/HandPressChain.h`), read by `HandSystem`; test `HandPressChain.WhilePausedTheIdleHandTakesNoActionPress`; before, the press was taken and its packets applied on the first turn after the pause |
| Taps are applied at the next game turn, from the place the hand reported, so every player sees the same | done | `HandSystem::SendTap` pushes a tap packet (`game_packets`), applied at the next turn's start (`HandTurn.cpp` `ApplyTap`), which checks validity again. No multiplayer |

## Houses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any house can be tapped | done | `abodes::InterfaceValidToTap` is true for every abode but the village centre (`src/ECS/Abodes.cpp`, `abode_knock::IsTappable`; the tap also needs it begun), tapped through the press's tap-only branch (`hand_press::Branch::TapOnly`) |
| Tapping a house knocks on it: everyone inside comes out to see who knocked | done | `AbodeKnockSystem::Tap` calls `villager::SetStateWhenTappedOnAbode` for each inhabitant at home: they walk out (`src/ECS/Villager/VillagerEmergency.cpp`) |
| The knock shows how many people live in each of the town's drawn buildings for 8 s: a gold person per adult, a green one per free place | done | `AbodeKnockSystem` (`Tap`, `Update` on game time, `PeopleOver`), `Renderer::DrawKnockRow`, each row right after its building; float products as the original's FPU (`test/test_abode_knock.cpp`) |
| While a villager is held, its home shows one teal person over itself | done | IN_HAND marks the home every turn and its exit unmarks it when the hand lets the villager go (`components::Abode::heldVillagersHome`, `villager::MarkHomeOfHeld`); `AbodeKnockSystem::PeopleOver`; tests `test_abode_knock`, `test_villager_home`, `test_living_turn` |
| Holding a villager near one of your houses brings the population read-out up | pending | not in our tree: it goes with the disciples (see ../villager/disciples.md) |
| On a finished house, the player's own hand plays its tap-house animation where it is | done | `AbodeKnockSystem::Tap`: for living quarters tapped by the local player, `HandSystem::StartFixedPosAnimation("Ctap_house", ...)` (the play-anim state) |
| On a finished house, one of nine knocking sounds plays at the hand, in turn | done | `AbodeKnockSystem::Tap`: living-quarters abodes only, InGame sample 110 plus `audio::Counter::KnockRoof` (0..8 in turn), 3D at the hand's point |

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rock can be tapped only when it is taller than 0.7 m (a rock too wide for the hand to lift, over 3.6, is tapped at once on a press; a liftable one by a short press), inside the player's influence | done | `object_physics::CanTapRock` (height > 0.7), `object_physics::RockValidForPlaceInHand` (2D radius <= 3.6); a rock that can't be lifted is tapped at once (`HandSystem::Update`, Hovered branch), a liftable one by a short press; the hand must be in the influence (`SendTap`) |
| Tapping a rock splits it in two | done | `object_physics::TapRock` then `object_physics::SplitRock` (`src/ECS/ObjectPhysics.cpp`): two halves of 0.7935 scale, the fire copied to both |
| Splitting a rock plays one of four cracking sounds at the hand, in turn | done | `object_physics::TapRock`: InGame sample 130 plus `audio::Counter::RockTap` (0..3 in turn), 3D at the hand's point |
| The player's creature is shown the deed and leans towards copying it (a mild empathy of one half) | pending | TODO in `object_physics::TapRock`: the creature's empathy waits for creature desires |

## Scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scaffold worth more than one, not in use and not tied to a building, can be tapped ("Tap To Break") | partial 80% | `scaffolds::ValidToTap` (value > 1, not flying, no site or still adjustable, any abode) registered by `scaffolds::RegisterTapHandler` (`src/ECS/Scaffolds.cpp`); read against the exe's scaffold tap test (value above one, not in the physics, free to build with, no abode tied), the same order. Missing: no test or scenario taps a scaffold. The tap text is the plain "Tap" in the original (a scaffold has no tap text of its own) |
| Tapping it breaks it back into single scaffolds, clearing an old building site it stood for | partial 80% | `scaffolds::Tap`: `RemoveOldBuildingSite` when it has a site, then `scaffolds::Split` (a new one of value 1, into the workshop's free slot). Missing: no test or scenario covers the tap's split |
| Breaking a scaffold plays one of four sounds at the hand, in turn | partial 80% | `scaffolds::Tap`: InGame sample 151 plus `audio::Counter::ScaffoldTap` (0..3 in turn), at the hand's point. Missing: no test or scenario covers it |

## Reward chests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a closed reward chest opens it ("Tap To Open") with its opening sounds; an open one only plays the tap sound | done | the Action button's tap opens it with sample 173 for the player at this computer, then 40 and 41 (`RewardSystem::Tap`, registered in `HandSystem::RegisterTapHandlers`); only its own player, only until open, as the exe's chest tap test; tests `RewardTap.*` (`test/test_reward_tap.cpp`); testbed `story.reward_chest_opens`; see [../story/rewards.md](../story/rewards.md). The "Tap To Open" text is in [pointing_and_tooltips.md](pointing_and_tooltips.md) |
| A chest of food or wood spills a pile of it where it stood | done | `reward::Open` (`src/ECS/RewardOpening.cpp`): food first, else wood, poured at the chest (`OpeningWorld::Pour`), as the exe's chest tap adds the resource at its place; test `RewardOpening.FoodComesFirstThenWoodPouredAtTheChest` |
| A chest can give a scaffold for a town's building | done | `reward::Open`: `OpeningWorld::MakeScaffold` (a scaffold for the town's workshop, as the exe makes one and sets its value); test `RewardOpening.AnObjectOrAScaffoldIsMadeAtTheChest` |
| A chest can give a random one-shot miracle, picked from the good list when the player is good, the evil list when evil, and either at random when neutral, among those allowed on the land | done | `reward::TakesGoodList` and `reward::RandomMiracle` (`src/ECS/RewardOpening.cpp`), from the alignment and the progress lists as the exe's chest tap reads them; tests `RewardOpening.TheProgressListIsChosenByAlignment`, `RewardOpening.ARandomMiracleIsDrawnUntilOneIsAllowed`, `RewardOpening.TheNextMiracleChestFindsATownAndGivesTheNextMiracle` |
| A chest can teach a miracle: the player or the town gains it, shown by a gesture drawn on the land or a miracle sign over the town | done | `reward::Open` (`GiveSeed`): the town learns it and shows it, or the first power-up the player lacks with its gesture on the land, else a globe; tests `RewardOpening.ATownLearnsASeedThePlayerCantCastAndShowsIt`, `RewardOpening.ASeedThePlayerCastsGivesTheFirstPowerUpLackedWithItsGesture`, `RewardOpening.AGestureShownOnTheLandIsLaidFromTheChestEastAndSouth` |
| A chest can give belief to a town, with a spot of light | done | `reward::Open`: only a town is given belief (`OpeningWorld::GiveTownBelief`, with the spot visual, as the exe's chest tap); test `RewardOpening.BeliefGoesOnlyToATown` |

## Challenge markers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a challenge's marker (the silver and gold scrolls, signposts) starts its challenge, stopping any help being given | partial 80% | `ecs::script_highlight::InterfaceTap` (`src/ECS/ScriptHighlight.cpp`) marks it activated and calls `CHLApi::StartChallengeReminder`, which stops a help script holding the dialogue and starts the challenge's reminder. Missing: no test covers the tap starting it (the LHVM Viewer's remind button calls the same) |
| A "did you know" marker plays a chime and shows its text instead | done | `ecs::script_highlight::InterfaceTap`: its InGame chime (sample 173) for the local player and the did-you-know bubble (`TipBubbleSystemInterface::Show`; a second tap on the same sign closes it); tapped in or out of the influence; tests `ScriptHighlight.ValidToTap`, `ScriptHighlight.TappedOutsideTheInfluence`, `ScriptHighlight.DidYouKnowReadLists` |
| Tapping one tells the help system, which can explain challenges the first times | done | `ecs::script_highlight::InterfaceTap` raises the Reminder or ScriptActivate help event (`help_profile::Trigger`), and the first "Did you know?" ever starts the FirstDYKExplained script |
| A marker tapped is remembered as activated | done | `script_highlight::SetActivated` in `InterfaceTap` for any marker with a script id |

## Miracles and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a one-shot miracle bubble or a dispenser's bubble puts its miracle in the hand | done | `worship::InterfaceTap` -> `magic::one_off::InterfaceTap` for a `OneOffSpellSeed` (one-shot orbs and the dispensers' orbs, `src/Worship/SpellDispenser.cpp`); registered in `HandSystem::RegisterTapHandlers` |
| Tapping a worship site's miracle icon takes its miracle into the hand once charged | done | `worship::icon::InterfaceTap` (`src/Worship/WorshipSpellIcon.cpp`) with its tap sound; registered in `HandSystem::RegisterTapHandlers` |
| Tapping another player's fireball catches it | done | a tap handler for `MagicFireBall` (`HandSystem::RegisterTapHandlers`) calls `magic::fireball::Catch` (`src/Magic/Objects/MagicFireBall.cpp`), which refuses the player's own ball and turns the ball into a seed in the hand; test `MagicObjectPlayerTest.AFireBallCanBeCaughtUnlessItIsTheCatchersOwn`; see [the tap table](../../../hand/grab-hold-and-throw.md) |
| Tapping a leash post, or the player's creature, puts the leash on or takes it off | done | The player's own creature: a short click that lets it go sends the leash toggle (`CreatureHandPackets.cpp`, `LeashSystem::PressKey`). The temple's leash posts: `worship::temple_leash::InterfaceTap` (`src/Worship/TempleLeash.cpp`, registered in `HandSystem::RegisterTapHandlers`), only the local player's own posts, picking or unpicking the post's leash; test `test/test_citadel_plan.cpp` |
| Tapping in a creature fight aims the player's creature's blows | partial 30% | `CreatureFightSystem::Press` exists (the blow aimed from the press ray), but nothing in the hand or input calls it yet (dormant) |
| Tapping a land puzzle's totem plays its sound and resets its choice | pending | The puzzle totem has its own tap test and tap in the exe (the tap table in [grab-hold-and-throw.md](../../../hand/grab-hold-and-throw.md)); our tree registers no tap for it (its locked select is ported) |

## The temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping the player's own temple's entrance takes them inside | done | `worship::citadel::EntranceTap` (`src/Worship/Citadel.cpp`), registered by `CitadelArchetype::RegisterTapHandlers`, tapped through the press's tap-only branch |
| The entrance only works once the land's script allows the temple | done | `worship::citadel::EntranceValidToTap`: only while the script's SET_INTERFACE_CITADEL value is not 0 |

## Holding the button down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button over the player's creature takes hold of it to stroke or slap it | done | `hand_press::Branch::Creature` -> `SendStartLockedSelect`, `HandSystem::UpdateCreatureLock`; see [creature_contact.md](creature_contact.md) |
| Holding the Action button on a field, fish farm, pile, totem or creature locks onto it, and the hand works it until let go | done | Locked selects for fields, piles, fish farms, the creature, village totems and puzzle totems (`SendStartLockedSelect` from the press in `HandSystem::Update`; `HandTurn.cpp` `ApplyStartLockedSelect`, `ProcessInInteractPile` / `Field` / `Fish`; a village totem through `VillageTotemSystem` `StartLockedSelect` / `SetGripped`, a puzzle totem through `puzzle_totem_hand`); every kind ends with the select's end and `EndAction`; tests `test/hand/test_hand_grab.cpp`, `test/test_village_totem.cpp`; testbed `test_testbed_scenarios` (the totem scenarios) |
| Locked onto a thing, the hand stays on it while the player's influence holds | done | `HandTurn.cpp`: the locked select lasts while the object is available, its influence > 0 at the hand and its process says so, else it ends; a creature or a totem that is gone is let go of and the action ends, and a totem also once the hand has no influence (a creature is kept outside it) (`hand_creature::StepLockedObject`, `HandLockedObject` tests) |
| A double click on the land flies the camera there | done | `DefaultWorldCameraModel` (`UnbindableActionMap::DOUBLE_CLICK`, with the double-click feature bit); see [../camera/](../camera/) |
| A double click on a creature in Creature Mode locks onto it | done | `CreatureModeSystem` (`src/ECS/Systems/Implementations/CreatureModeSystem.cpp`): a double click on a creature locks onto it |
| A double click is a second press within half a second, within a small box of the first | partial 40% | Our double click is SDL's (`event.button.clicks == 2` in `GameActionMap::ProcessEvent`): it exists, but with the system's double-click time and distance, not the game's own half second and box |
| Pressing the Action button where nothing takes it (the sky, out of reach) does nothing | done | `hand_press::Branch::None` (`src/ECS/HandPressChain.h`): no branch, nothing happens |

## Force-feedback mouse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With a force-feedback mouse, what the hand holds or is over is felt by its own texture (bonfires, dead trees, fragments, people, puzzle pieces, miracles) | pending | Not ported |
| Scripts start and stop force-feedback effects | pending | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` are stubs in `src/CHLApi.cpp` |
| Scripts ask whether a force-feedback mouse is present | partial 50% | `IMMERSION_EXISTS` always pushes false (`src/CHLApi.cpp`), the original's answer for a player without one; detecting a device is not ported |
