# Clicking and activating

What the Action button does when the hand isn't picking something up: tapping things (knocking on houses, splitting
rocks, breaking scaffolds, opening reward chests, starting challenges), activating them (the temple's entrance, miracle
bubbles and icons, leash posts), double clicks, locked selection, and the force-feedback mouse the game supported.

**Progress: 26/44 done, 6 partial — 66%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Tapping in general

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A quick press and release of the Action button over a thing taps it rather than taking hold | done | `HandSystem::Update` (the grab state): a release within 225 ms sends `HandSystem::SendTap`; tappable classes are registered in `ecs::hand_tap` (rocks, abodes, scaffolds, spell icons, one-shot orbs, "Did you know?" signs, the temple's entrance); test `test_hand_press_chain` |
| Each kind of thing says whether it can be tapped at all; a thing that can't is left alone | done | `ecs::hand_tap` (`src/ECS/Systems/HandTap.h`): each owner registers its classes, an unregistered class is not tappable; `hand_tap::SendsTap` also checks the influence and the cannot-be-picked-up flag |
| Village centres, dead trees and bonfires can't be tapped | done | Dead trees and bonfires register no tap handler. A town centre is an abode in our tree and passes the abode's tap test, but its tap does nothing (no inhabitants, no knock) |
| Taps are applied at the next game turn, from the place the hand reported, so every player sees the same | done | `HandSystem::SendTap` pushes a tap packet (`game_packets`), applied at the next turn's start (`HandTurn.cpp` `ApplyTap`), which checks validity again. No multiplayer |

## Houses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any house can be tapped | done | `abodes::InterfaceValidToTap` is always true for an abode (`src/ECS/Abodes.cpp`), tapped through the press's tap-only branch (`hand_press::Branch::TapOnly`) |
| Tapping a house knocks on it: everyone inside comes out to see who knocked | done | `abodes::InterfaceTap` calls `villager::SetStateWhenTappedOnAbode` for each inhabitant at home: they walk out (`src/ECS/Villager/VillagerEmergency.cpp`) |
| The knock counts the people living there, for the town's figures | todo | Not ported: `abodes::InterfaceTap` says "(not ported) remembering the town and counting the knock" |
| On a finished house, the player's own hand plays its tap-house animation where it is | done | `abodes::InterfaceTap`: for living quarters tapped by the local player, `HandSystem::StartFixedPosAnimation("Ctap_house", ...)` (the play-anim state) |
| On a finished house, one of nine knocking sounds plays at the hand, in turn | done | `abodes::InterfaceTap`: living-quarters abodes only, InGame sample 110 plus `audio::Counter::KnockRoof` (0..8 in turn), 3D at the hand's point |

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rock can be tapped only when it is taller than 0.7 m (a rock too wide for the hand to lift, over 3.6, is tapped at once on a press; a liftable one by a short press), inside the player's influence | done | `Rocks::ValidToTap` (height > 0.7), `Rocks::ValidForPlaceInHand` (2D radius <= 3.6); a rock that can't be lifted is tapped at once (`HandSystem::Update`, Hovered branch), a liftable one by a short press; the hand must be in the influence (`SendTap`) |
| Tapping a rock splits it in two | done | `Rocks::Tap` then `Rocks::SplitInTwo` (`src/ECS/Rocks.cpp`): two halves of 0.7935 scale, the fire copied to both |
| Splitting a rock plays one of four cracking sounds at the hand, in turn | done | `Rocks::Tap`: InGame sample 130 plus `audio::Counter::RockTap` (0..3 in turn), 3D at the hand's point |
| The player's creature is shown the deed and leans towards copying it (a mild empathy of one half) | todo | TODO in `Rocks::Tap`: the creature's empathy waits for creature desires |

## Scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scaffold worth more than one, not in use and not tied to a building, can be tapped ("Tap To Break") | done | `scaffolds::ValidToTap` (value > 1, not flying, no site or still adjustable, any abode) registered by `scaffolds::RegisterTapHandler` (`src/ECS/Scaffolds.cpp`); its tooltip text is not checked here |
| Tapping it breaks it back into single scaffolds, clearing an old building site it stood for | done | `scaffolds::Tap`: `RemoveOldBuildingSite` when it has a site, then `scaffolds::Split` (a new one of value 1, into the workshop's free slot) |
| Breaking a scaffold plays one of four sounds at the hand, in turn | done | `scaffolds::Tap`: InGame sample 151 plus `audio::Counter::ScaffoldTap` (0..3 in turn), at the hand's point |

## Reward chests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a closed reward chest opens it ("Tap To Open") with its opening sounds; an open one only plays the tap sound | todo | No reward chests: `CREATE_REWARD` and `CREATE_REWARD_IN_TOWN` are stubs in `src/CHLApi.cpp` |
| A chest of food or wood spills a pile of it where it stood | todo | Not ported |
| A chest can give a scaffold for a town's building | todo | Not ported |
| A chest can give a random one-shot miracle, picked from the good list when the player is good, the evil list when evil, and either at random when neutral, among those allowed on the land | todo | Not ported |
| A chest can teach a miracle: the player or the town gains it, shown by a gesture drawn on the land or a miracle sign over the town | todo | Not ported |
| A chest can give belief to a town, with a spot of light | todo | Not ported |

## Challenge markers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a challenge's marker (the silver and gold scrolls, signposts) starts its challenge, stopping any help being given | todo | `ecs::script_highlight::InterfaceTap` (`src/ECS/ScriptHighlight.cpp`) marks it activated, but starting the challenge script and stopping the help scripts are "(pending)" |
| A "did you know" marker plays a chime and shows its text instead | done | `ecs::script_highlight::InterfaceTap`: its InGame chime for the local player and `HelpSystem::SetBubbleProperties` (the "Did you know?" bubble; a second tap on the same sign closes it); tapped in or out of the influence |
| Tapping one tells the help system, which can explain challenges the first times | done | `ecs::script_highlight::InterfaceTap` raises the Reminder or ScriptActivate help event (`help_profile::Trigger`), and the first "Did you know?" ever starts the FirstDYKExplained script |
| A marker tapped is remembered as activated | done | `script_highlight::SetActivated` in `InterfaceTap` for any marker with a script id |

## Miracles and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a one-shot miracle bubble or a dispenser's bubble puts its miracle in the hand | done | `worship::InterfaceTap` -> `magic::one_off::InterfaceTap` for a `OneOffSpellSeed` (one-shot orbs and the dispensers' orbs, `src/Worship/SpellDispenser.cpp`); registered in `HandSystem::RegisterTapHandlers` |
| Tapping a worship site's miracle icon takes its miracle into the hand once charged | done | `worship::icon::InterfaceTap` (`src/Worship/WorshipSpellIcon.cpp`) with its tap sound; registered in `HandSystem::RegisterTapHandlers` |
| Tapping another player's fireball catches it | partial | `magic::fireball::Catch` (`src/Magic/Objects/MagicFireBall.cpp`) turns another player's fireball into a seed in the hand, but nothing calls it: the hand never targets a fireball (dormant) |
| Tapping a leash post, or the player's creature, puts the leash on or takes it off | partial | The player's own creature: a short click that lets it go sends the leash toggle (`CreatureHandPackets.cpp`, `LeashSystem::PressKey`); the leash key works too. No leash posts in our tree |
| Tapping in a creature fight aims the player's creature's blows | partial | `CreatureFightSystem::Press` exists but nothing in the hand or input calls it (dormant) |
| Tapping a land puzzle's totem plays its sound and resets its choice | todo | Not ported (the puzzle totem's tap) |

## The temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping the player's own temple's entrance takes them inside | done | `worship::citadel::EntranceTap` (`src/Worship/Citadel.cpp`), registered by `CitadelArchetype::RegisterTapHandlers`, tapped through the press's tap-only branch |
| The entrance only works once the land's script allows the temple | done | `worship::citadel::EntranceValidToTap`: only while the script's SET_INTERFACE_CITADEL value is not 0 |

## Holding the button down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button over the player's creature takes hold of it to stroke or slap it | done | `hand_press::Branch::Creature` -> `SendStartLockedSelect`, `HandSystem::UpdateCreatureLock`; see [creature_contact.md](creature_contact.md) |
| Holding the Action button on a field, fish farm, pile, totem or creature locks onto it, and the hand works it until let go | partial | Locked selects for fields, piles, fish farms and the creature (`SendStartLockedSelect`, `HandTurn.cpp` `ProcessInInteractPile` / `Field` / `Fish`); not the totems (see [totem.md](totem.md)) |
| Locked onto a thing, the hand stays on it while the player's influence holds | done | `HandTurn.cpp`: the locked select lasts while the object is available, its influence > 0 at the hand and its process says so, else it ends |
| A double click on the land flies the camera there | done | `DefaultWorldCameraModel` (`UnbindableActionMap::DOUBLE_CLICK`, with the double-click feature bit); see [../camera/](../camera/) |
| A double click on a creature in Creature Mode locks onto it | done | `CreatureModeSystem` (`src/ECS/Systems/Implementations/CreatureModeSystem.cpp`): a double click on a creature locks onto it |
| A double click is a second press within half a second, within a small box of the first | partial | Our double click is SDL's (`event.button.clicks == 2` in `GameActionMap::ProcessEvent`): the system's double-click time and distance, not the game's own half second and box |
| Pressing the Action button where nothing takes it (the sky, out of reach) does nothing | done | `hand_press::Branch::None` (`src/ECS/HandPressChain.h`): no branch, nothing happens |

## Force-feedback mouse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With a force-feedback mouse, what the hand holds or is over is felt by its own texture (bonfires, dead trees, fragments, people, puzzle pieces, miracles) | todo | Not ported |
| Scripts start and stop force-feedback effects | todo | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` are stubs in `src/CHLApi.cpp` |
| Scripts ask whether a force-feedback mouse is present | partial | `IMMERSION_EXISTS` always pushes false (`src/CHLApi.cpp`), right for a player without one |
