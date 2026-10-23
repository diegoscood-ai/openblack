# Totem

Each village has a totem pole by its village centre. The player takes hold of the totem with the hand and slides it up
or down to choose how many of the village's people worship rather than work. The hand locks onto the pole while the
button is held. Worship itself is in [../worship/](../worship/). Khazar teaches the totem on the second land, in
[Worship Site](../story/gold_scrolls/worship_site.md#khazars-lesson-on-worship).

**Progress: 18/32 done, 0 chosen, 7 partial, 0 inferred — 73%**

**Progress: 16/31 done, 0 chosen, 7 partial, 0 inferred — 69%**

How the original does it, in our wiki: [the village totem](../../../magic/village-totem.md) and
[the puzzle totem](../../../magic/puzzle-totem.md#the-hand-and-the-tap).

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button on a totem locks the hand onto it until the button is let go | done | `hand_press::Branch::Totem` (the totem is the object under the hand, after the hovered object and before the tap-only object and the fish), the start and end packets setting `components::VillageTotem::gripped`, the hand's TOTEM state; tests `HandPressChain.ATotemIsTheObjectUnderTheHandBeforeTheTapOnlyObjectAndTheFish`, testbed `objects.village_totem_drag`; [the locked select](../../../magic/village-totem.md#the-locked-select-on-a-village-totem-hand-state-5) |
| With the leash in the hand, the press waits the tap-or-hold time before it locks onto the totem | pending | the 225 ms wait of a press with the leash in the hand is not ported (wiki Pending, the hold) |
| Only a totem of the player's own village can be worked | done | `village_totem::ValidForLockedSelect` over `GripFacts` (the town's player); test `VillageTotemRules.OnlyTheTownsPlayerWithTheirTempleAndAWorshipSiteMayTakeIt` |
| Only a built, living totem (life above three quarters), with the player's temple heart built and a built worship site for the village, inside the player's influence, can be worked | done | `VillageTotemSystem::ValidForLockedSelect` and the press's influence test; test as above |
| Taking hold, the hand moves to 0.7 of the icon's height above the icon's base, at the totem's place, and stays there | done | `village_totem::GripY` from `VillageTotemSystem::GetHandHold`, every frame; test `VillageTotemRules.TheHandHoldsTheIconSevenTenthsOfItsHeightUp` |
| Taking hold, the hand is sized by its distance from the camera to the totem | pending | the TOTEM state's Enter sizes the hand from the camera's distance to the totem; openblack keeps the size the pointer's ground point gives it (wiki Pending, the hold) |
| The pointer's image is pinned at the hand's place on screen while the totem is held | done | pinned where the mouse is as the TOTEM state begins, then each drawn frame on the hand's point, kept on the screen and rounded half to even, let go as the state ends (`HandSystem::PrepareState` / `Update`, `village_totem::CursorPin`; test `VillageTotemHandHold.TheCursorsImageIsPinnedOnTheHandKeptOnTheScreen`; readout "Cursor Image Pinned" in the Game Details overlay). As in the original no cursor image is drawn while the hand is shown, so the pin is not seen ([the locked select](../../../magic/village-totem.md#the-locked-select-on-a-village-totem-hand-state-5)) |
| The player's creature, if it can see the hand, is less eager to impress the player when the totem is gripped | pending | the creature's desire update at the grip is not ported: openblack's creature mind has nothing that divides a desire's rise (wiki Pending, the grip) |
| Villagers and animals nearby are told the hand is using the totem | partial 50% | the reaction to the hand using the totem is made at the grip (`VillageTotemSystem::FriendlyStartLockedSelect`, `ecs::effects::reactions::CreateReaction`); how the living take it is not read, and they have no handling for it (wiki Pending) |
| The totem's raise sound starts with the share gripped | done | `audio::guidance::StartTotemRaiseSound` from `VillageTotemSystem` at the grip |
| A force-feedback mouse starts its totem effect | pending | No force feedback (the grip's immersion effect is not ported) |

## Sliding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving the mouse up or down slides the hand by three icon heights for each screen height moved, and the share the hand holds by a tenth of the slide, between none and all | done | `village_totem::HandSlide` and `SlideShare` from `VillageTotemSystem::Slide`, with the frame's mouse move up the screen against the window's height (`VillageTotemRules.TheHandSlidesThreeIconHeightsAScreenAndStopsAtTheLand`); check `objects.village_totem_drag` |
| Sliding down stops when the hand would go below the land under it | done | `HandSlide`'s clamp at the land at the totem's place, where the hand stays (`VillageTotemRules.TheHandSlidesThreeIconHeightsAScreenAndStopsAtTheLand`, `AHandBelowTheLandIsBroughtUpToItWhicheverWayItSlides`) |
| Held, the totem stands risen by the share the hand holds, eight units at a full share | done | `village_totem::RiseOf` and `ShownShares` (`VillageTotemRules.TheStatueRisesEightUnitsAtFullShare`, `TheSecondTotemShowsTheOtherShareWhileTheyStandApart`), drawn by `worship::percentage::UpdateTotems` |
| The hand follows the icon as the totem rises and sinks | done | `GetHandHold` reads the icon's place every frame (`HandSystem::Place` in hand state 5) |
| Near the ground (below 2.5 units) the hand tips over towards the ground; higher up it lies at seven sixteenths of a turn | done | `village_totem::HandTiltAt` (`VillageTotemHandHold.TheHandTipsOverMoreTheNearerTheLand`) |
| The hand faces away from the camera, half a turn from its heading | partial 90% | `village_totem::HoldRotation` with b = heading + π worked in floats as the exe (`VillageTotemHandHold.TheHandIsTippedAboutItsSideThenTurned`, `TheTippedRowsTakeTheTurnsSineWiderRoundedOnce`); missing: the heading's mouse ray is not rounded as the original's (queued fix F-94) |
| The hand holds the icon in its side hold, closed by how thick the icon is for the hand's size | partial 80% | `Chold_side` at `village_totem::GripTimeMs(GripClosure(radius), the clip's length)` (`VillageTotemHandHold.TheSideHoldClosesWithTheIconsHoldRadius`); missing: what the clip's first field is (its length in ms or its frame count) is not read |
| While the totem is placed again (its rise changed, or a second totem shows) the tooltip gives the share the hand holds as a percentage; not for another player's totem, nor on the first re-place after its icon changed | done | `village_totem::Replaces` and `ShowsShareToolTip`, `helptext::k_ToolTipWorshipShare` (`VillageTotemRules.ItIsPlacedAgainWhenItsRiseChangesOrTheSecondShows`, `TheShareTooltipOnlyForTheLocalPlayerAndNotOnceAfterTheIconChanged`) |
| While the held share and the share the town has stand apart, a see-through second totem at half alpha shows the other one | partial 90% | `ShownShares`' second share, drawn as two `SeeThrough` entities at alpha 128 just before the totem's own (`VillageTotemRules.TheSecondTotemShowsTheOtherShareWhileTheyStandApart`; check `objects.village_totem_drag`); missing: the copy's colour on the frame before the solid's first draw is not read |

## Letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go sends the share the hand held to the town, once per grip, which sends its villagers to worship or back; the totem then eases to it, 5.2 seconds for the whole way | done | `VillageTotemSystem::EndLockedSelect` pushes the share packet (`game_packets::Type::TotemShare`), whose handler `LetGo` sets the town's share and the totem's (`SetShareAndHold`); the ease is `VillageTotemEase.*`; check `objects.village_totem_drag` |
| The share is applied by the let go's packet, as every machine applies it, not where the hand is | done | only the packet's handler (`LetGo`) sets it; openblack has no other machines, so the packet goes to its own game |
| Let go raised, the creature of the town's player, when it can see the totem, takes it as the player trying to impress it | partial 50% | `VillageTotemSystem::LetGo` calls `EmpathiseWithPlayer(owner, ToImpress, 0.5, the totem's place)` for a share above none, as the original; no test or check reaches it with a creature, and (inferred) a town of the neutral player is taken to have no creature |
| The share floats up over the totem in grey, as a percentage, for five seconds | pending | the 0x29 handler's floating number is not made ([Pending](../../../magic/village-totem.md#pending)) |
| A force-feedback mouse stops its totem effect | pending | No force feedback |

## Puzzle totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button on a puzzle totem takes hold of it: it needs only to be working, not unpickupable and in the influence, whoever's it is | done | the hand's totem branch with the puzzle totem's own test (`IsFunctional`, no player test) and `puzzle_totem_hand`'s start; `IS_LOCKED_INTERACTION` is true while held; check `story.undead_totem_drag`; see [the puzzle totem](../../../magic/puzzle-totem.md#the-hand-and-the-tap) |
| A puzzle totem slides by the mouse's movement divided by 1.5, between its top and its lowest | partial 90% | `puzzle_totem::DragOffset` / `Drag`, `puzzle_totem_hand::Slide` (`src/ECS/PuzzleTotemHand.cpp`); tested by `PuzzleTotemRules.TheDragStaysBetweenItsLowestAndItsRestAndSendsOnlyNewSteps`; missing: the hand's move is clamped at the land at the totem's place, where the original reads the land under the hand |
| A puzzle totem slides in whole steps, sending each new step to the other players | done | `puzzle_totem::DragAltitude`, the `PuzzleTotemPosition` packet and `puzzle_totem_hand::ApplyPosition`; the round trip pinned to the exe's numbers (`PuzzleTotemRules.TheHandsDragGivesTheOriginalsAltitudeAndHeight`, `TheDragCanLeaveItAStepBelowTheStepTheHandChose`) |
| Scripts set a puzzle totem's highest point | done | `SET_PROPERTY` MAX_HEIGHT on a `components::PuzzleTotem` (`puzzle_totem::SetMaxHeightProperty`, `src/CHLApi.cpp`); tested by `PuzzleTotemRules.ANewHighestLeavesTheHeightAsItIs`; see [the puzzle totem](../../../magic/puzzle-totem.md) |
| A puzzle totem a script made unpickupable is never held; letting the button go remembers it as the thing clicked | partial 80% | the press does not take it and the idle release records it (`HandClicked`); check `story.bell_towers`; missing: the original's press then goes on to its click on the land, not read |
| The totem puzzle games lay out their own totems, which are only tapped: a tap sinks one with a sound | pending | the totem puzzle games and the tap of a tap-only totem are not in our tree (`PuzzleTotemRules.ATapOnlyTotemAlwaysStandsAtAllItsHeight` covers only where it stands) |

## Others working totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature raises and lowers totems itself | done | the village totem's by its plan actions `RaiseTotemPole` / `LowerTotemPole` ([../creature/town_actions.md](../creature/town_actions.md)); a puzzle totem's by a script's `CREATURE_DO_ACTION`, moving the totem's own share 0.02 a turn (`PuzzleTotemPlaceTest.TheCreatureRaisesItByItsOwnShare`; [the puzzle totem](../../../magic/puzzle-totem.md#the-creature)) |
