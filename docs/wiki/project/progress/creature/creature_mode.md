# Creature Mode and the status panel

The camera can lock onto a creature and follow it about while the screen shows how damaged, hungry and tired it is.
Hovering the hand over any creature brings up the same panel, with the reward the hand has given it so far. General
camera controls are in [../camera](../camera/).

**Progress: 24/33 done, 0 chosen, 4 partial, 3 inferred — 80%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Locking onto a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| C locks the camera onto the player's own creature, and pressed again gives the camera back | done | `src/Creature/CreatureMode.*`, `CreatureModeSystem` (updated from `Game.cpp`, key `ZOOM_TO_CREATURE`); tests `CreatureMode.CreatureKeyLocksOntoYourCreatureAndLetsGo`, `CreatureModeSystemTest.CLocksOntoThePlayersCreatureAndLetsGoAgain` |
| Locked onto another god's creature, C moves over to the player's own | done | `creature_mode::OnCreatureKey`; test `CreatureModeSystemTest.MovingOverToAnotherCreatureStillHandsThePlayersModelBack` |
| Double clicking a creature, anyone's, locks onto that one | done | `CreatureModeSystem::ReadKeys` on the game's `DOUBLE_CLICK` action with the creature under the hand; tests `CreatureModeSystemTest.ADoubleClickOnACreatureLocksOntoItBeforeTheCameraMoves`, `ADoubleClickDoesNotLockOnWhenItsFlightIsNotAllowed` |
| A double click is a second press within half a second, inside a small box of the first, on the same creature | inferred | the half second, the box and the same-creature rule are not on the wiki; our tree takes the double click from the input layer's `DOUBLE_CLICK` action; the lock-on itself is the exe's double-click flight with camera feature 0x10, as ours ([overview](../../../creature/overview.md)) |
| The cursor keys alone, dragging the land, entering the temple, opening the editor or a script's cinema bars give the camera back | done | `CreatureModeSystem::Update`; tests `CreatureModeSystemTest.TheCursorKeysAloneGiveTheCameraBack`, `OnlyAFreshGripOfTheLandGivesTheCameraBack`, `AScriptsCinemaBarsRefuseAndEndIt`, `CreatureFollow.CursorKeysAloneGiveTheCameraBack` |
| The camera is handed back exactly as the player left it | done | `CreatureModeSystem` keeps the player's camera model; tests `CreatureModeSystemTest.EnterStartsFromWhereTheCameraIsHeadingAndLeaveHandsThePlayersModelBack`, `LeavingWhileAnotherHasTheCameraWaitsForItBack` |
| With no creature, C does nothing | done | `creature_mode::OnCreatureKey`; test `CreatureModeSystemTest.WithoutALookupNoPlayerHasACreatureAndCDoesNothing` |

## The follow camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera looks at the middle of the creature's body, by its height for its size | done | `src/Camera/CreatureFollow.*`, `src/Camera/CreatureCameraModel.*`; tests `CreatureFollow.LooksAtTheMiddleOfTheCreature`, `CreatureMode.CreatureHeightGrowsWithSize`, `CreatureCameraModel.StartsOnTheCreaturesMiddleWithTheFollowsView` |
| It starts the creature's viewing distance away from afar, or about as close as it already is | done | tests `CreatureFollow.StartsAtTheViewingDistanceFromAfar`, `StaysAboutAsCloseWhenAlreadyClose` |
| It keeps between a least and a most distance and never lower than about 14 degrees above | done | test `CreatureFollow.KeepsWithinItsBounds`, `EasesAndKeepsWithinTheScriptCamerasBounds` |
| It eases after the creature, arriving in two seconds as it starts and one later | done | tests `CreatureFollow.EasesInTwoSecondsAtFirstAndOneLater`, `CreatureModeSystemTest.FollowsTheCreatureAsItMoves` |
| Shift and the cursor keys turn it round the creature and tilt it | done | `CreatureCameraModel`; test `CreatureFollow.ShiftAndCursorKeysTurnAndTilt` |
| Ctrl and the cursor keys turn it and draw it in and out | done | `CreatureCameraModel`; test `CreatureFollow.CtrlAndCursorKeysTurnAndZoom` |
| The mouse wheel draws it in and out, counted twice as the game does | done | `CreatureCameraModel`; test `CreatureFollow.TheWheelZoomsTwice` |
| Ctrl and Shift together swing it round to where the land falls away, for a clear view, tilting towards about 22 degrees | done | `CreatureCameraModel::ClearView`; tests `CreatureFollow.ClearViewKeepsTheHeadingOnLevelLand`, `ClearViewSwingsAwayFromAHill`, `ClearViewLooksFromThreeEighthsOut`, `ClearViewTiltsTowardsTwentyTwoDegrees`, `ClearingDistanceIsDrawnTowardsFiftyToAHundred` |
| A creature turning too fast for the camera is followed more loosely | inferred | written from expectation: not found in the exe's reading of the follow camera; nothing in our tree |
| The camera follows the creature into a fight's view and back | partial 40% | `CreatureFightSystem` flies the camera to the arena's side and keeps the duel framed (not through Creature Mode); missing: the exe's double click near a fight arena starts the fight's camera (`StartFight` 0x45A4D0) in place of the follow, and the arena is not ported ([overview](../../../creature/overview.md)); see [fighting.md](fighting.md) |

## The status panel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the hand pressed and held on its player's creature, the left of the screen shows its damage, hunger and tiredness as bars | done | as the exe (the locked select, not a hover: [fights](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)): `creature_panel_choice::Choose` takes the creature the creature hand holds (`src/Creature/CreaturePanelChoice.h`, `src/Creature/CreatureStatusPanel.*`); tests `CreaturePanelChoice.HeldWinsWithTheReward`, `CreatureStatusPanel.ShowsOnHover`; `Game::UpdateHandInterface` chooses it and `GameInterface::DrawCreaturePanel` draws it |
| Below them, the reward the hand has given it so far, from "Bad Boy!" to "Good Boy!" or "No Reward" | done | `creature_panel` reward words (test `CreatureStatusPanel.RewardWords`), drawn; see [learning_from_feedback.md](learning_from_feedback.md) |
| Damage is life lost, hunger the energy missing, tiredness the exhaustion | done | test `CreatureStatusPanel.ValuesComeFromTheBody` |
| Values are clamped as the game does and shown as whole percentages cut short | done | tests `CreatureStatusPanel.ValuesAreClampedAsTheGameDoes`, `PercentagesAreCutShort` |
| Bars fill yellow; the reward bar fills from its middle, red for punishment and green for reward | partial 80% | tests `CreatureStatusPanel.BarsFillAndColour`, `BarFillRunsInsideTheFrame`; drawn by `GameInterface::DrawStatBar`, shared with the fight panel; the colours, the fill from half its colour to its colour, the middle line and the inner shadows are the exe's (`fn_00516B00`, [fights](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)); not compared: our frame against the exe's bevelled box (`DrawBevBox`) (imported) |
| The panel sits on a see-through black box fading to its right, laid out by the screen's size | partial 60% | the box (black at alpha 0x5F fading right) and the rows; tests `CreatureStatusPanel.LayoutWithTheReward`, `AtTheDefaultScreenEverythingIsInWholePixels`, `LayoutFollowingACreature`; missing: the exe's whole-number layout (H/16, size/2, the rows, the bars' ends, the label column) is queued as a fix ([fights](../../../creature/fights.md#pending)) |
| While the camera follows a creature, the panel shows its three bars near the top of the screen, without the reward | done | test `CreatureStatusPanel.LayoutFollowingACreature`; `creature_panel_choice::Choose` picks the followed creature |
| The labels and reward words come from the game's texts | done | `CreatureStatusPanel.h` names the text ids |
| The panel is drawn afresh each frame it is wanted, without fading in or out | done | `Game::UpdateHandInterface` sets it each frame, `GameInterface::DrawCreaturePanelLayer` draws it |
| The hand's "Interact" tooltip shows over the player's own creature, awake | partial 50% | `creature_hand::ShowsInteractTip`, used by `HandToolTips.cpp`; test `CreatureHand.TheInteractTipIsForTheOwnCreatureOrAnotherInTheInfluence`; ours also shows it over another player's creature in the influence and does not check that it is awake; the exe's owner rule (0x5D7406) is not read ([overview](../../../creature/overview.md)) |
| Not shown inside the temple | inferred | `Game::UpdateHandInterface` shows none in the temple; the exe's reading of the panel ([fights](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)) names no temple gate, and no test covers ours |
| Hidden while a script's cinema bars are on (the bars on and a script owning them), whether or not the interface is active | done | `creature_panel_choice::HiddenByWideScreen` in `Game::UpdateHandInterface`, as the exe's HelpSystem +0x45E8 / +0x45EC test; test `CreaturePanelChoice.HiddenOnlyWhileAScriptsWideScreenIsOn` |
| Hidden in the sequence mode and under an open setup box | pending | the exe's panel hides in both ([fights](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)); `Game::UpdateHandInterface` tests neither |

## Passing out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature passes out when its damage, hunger or tiredness reaches 100% | done | `CreaturePhysiologySystem` faints through `physiology::ShouldFaint`; tests `CreatureMode.PassesOutWhenAStatusReachesAHundredPercent`, `PassingOutAgreesWithTheBodysFainting` |
| It is carried to its pen to come round: the home it was given, else by its player's temple | done | `CreatureFightSystem` (`HomeOf`, `creature_mode::PenOf`, the temple's pen point); test `CreatureMode.PenIsTheHomeThenTheTempleThenTheFallback`; see [home_and_pen.md](home_and_pen.md) |
| The player is told their creature has fainted and been taken home | pending | the exe's faint test calls the creature help (`fn_004C9FE0(0x1E, 3, …)`) before `Faint` ([plans-and-needs](../../../creature/plans-and-needs.md)); no message in our tree; see [lessons_and_help.md](lessons_and_help.md) |
