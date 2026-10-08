# Creature Mode and the status panel

The camera can lock onto a creature and follow it about while the screen shows how damaged, hungry and tired it is.
Hovering the hand over any creature brings up the same panel, with the reward the hand has given it so far. General
camera controls are in [../camera](../camera/).

**Progress: 16/31 done, 11 partial — 69%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Locking onto a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| C locks the camera onto the player's own creature, and pressed again gives the camera back | done | `src/Creature/CreatureMode.*`, `CreatureModeSystem` (updated from `Game.cpp`, key `ZOOM_TO_CREATURE`); tests `CreatureMode.CreatureKeyLocksOntoYourCreatureAndLetsGo`, `CreatureModeSystemTest.CLocksOntoThePlayersCreatureAndLetsGoAgain` |
| Locked onto another god's creature, C moves over to the player's own | done | `creature_mode::OnCreatureKey`; test `CreatureModeSystemTest.MovingOverToAnotherCreatureStillHandsThePlayersModelBack` |
| Double clicking a creature, anyone's, locks onto that one | done | `CreatureModeSystem::ReadKeys` on the game's `DOUBLE_CLICK` action with the creature under the hand; tests `CreatureModeSystemTest.ADoubleClickOnACreatureLocksOntoItBeforeTheCameraMoves`, `ADoubleClickDoesNotLockOnWhenItsFlightIsNotAllowed` |
| A double click is a second press within half a second, inside a small box of the first, on the same creature | partial | our tree takes the double click from the input layer's `DOUBLE_CLICK` action; no own rule of half a second, a small box and the same creature is checked here |
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
| Ctrl and Shift together swing it round to where the land falls away, for a clear view, tilting towards about 22 degrees | done | `CreatureCameraModel::ClearView`; tests `CreatureFollow.ClearViewKeepsTheHeadingOnLevelLand`, `ClearViewSwingsAwayFromAHill`, `ClearViewTiltsTowardsTwentyTwoDegrees`, `ClearingDistanceIsDrawnTowardsFiftyToAHundred` |
| A creature turning too fast for the camera is followed more loosely | todo | nothing in our tree (unconfirmed in the original) |
| The camera follows the creature into a fight's view and back | partial | `CreatureFightSystem` flies the camera to the arena's side and keeps the duel framed (not through Creature Mode); see [fighting.md](fighting.md) |

## The status panel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the hand over any creature, the left of the screen shows its damage, hunger and tiredness as bars | partial | `src/Creature/CreatureStatusPanel.*` has the values and layout (test `CreatureStatusPanel.ShowsOnHover`), but dormant: only tests use it, nothing draws the panel in the game |
| Below them, the reward the hand has given it so far, from "Bad Boy!" to "Good Boy!" or "No Reward" | partial | `creature_panel` reward words (test `CreatureStatusPanel.RewardWords`); dormant, not drawn; see [learning_from_feedback.md](learning_from_feedback.md) |
| Damage is life lost, hunger the energy missing, tiredness the exhaustion | partial | test `CreatureStatusPanel.ValuesComeFromTheBody`; dormant, only tests |
| Values are clamped as the game does and shown as whole percentages cut short | partial | tests `CreatureStatusPanel.ValuesAreClampedAsTheGameDoes`, `PercentagesAreCutShort`; dormant, only tests |
| Bars fill yellow; the reward bar fills from its middle, red for punishment and green for reward | partial | tests `CreatureStatusPanel.BarsFillAndColour`, `BarFillRunsInsideTheFrame`; dormant, not drawn |
| The panel sits on a see-through black box fading to its right, laid out by the screen's size | partial | tests `CreatureStatusPanel.LayoutWithTheReward`, `LayoutFollowingACreature`; dormant, not drawn |
| While the camera follows a creature, the panel shows its three bars near the top of the screen, without the reward | partial | test `CreatureStatusPanel.LayoutFollowingACreature`; dormant: Creature Mode does not draw it |
| The labels and reward words come from the game's texts | partial | `CreatureStatusPanel.h` names the text ids; dormant, not drawn |
| The panel is drawn afresh each frame it is wanted, without fading in or out | todo | nothing draws the panel in our tree |
| The hand's "Interact" tooltip shows over the player's own creature, awake | partial | `creature_hand::ShowsInteractTip`, used by `HandToolTips.cpp`; test `CreatureHand.TheInteractTipIsForTheOwnCreatureOrAnotherInTheInfluence`. Ours also shows it over another player's creature in the influence, and whether it is awake is not checked (pending: a creature flag that skips it) |
| Not shown inside the temple | todo | the panel is not shown anywhere yet |

## Passing out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature passes out when its damage, hunger or tiredness reaches 100% | done | `CreaturePhysiologySystem` faints through `physiology::ShouldFaint`; tests `CreatureMode.PassesOutWhenAStatusReachesAHundredPercent`, `PassingOutAgreesWithTheBodysFainting` |
| It is carried to its pen to come round: the home it was given, else by its player's temple | done | `CreatureFightSystem` (`HomeOf`, `creature_mode::PenOf`, the temple's pen point); test `CreatureMode.PenIsTheHomeThenTheTempleThenTheFallback`; see [home_and_pen.md](home_and_pen.md) |
| The player is told their creature has fainted and been taken home | todo | no message in our tree; see [lessons_and_help.md](lessons_and_help.md) |
