# Tooltips

The words, mouse picture and arrows the hand shows beside it for what it is over: what a click would do, and numbers
such as a town's desires or a store's food. Which tooltip each thing in the world offers as the hand points at it is
in ../hand/; this file is the tooltip system and the status readouts it shows.

**Progress: 20/33 done, 0 chosen, 11 partial, 1 inferred — 87%**

How the original does it, in our wiki: [The hand and the interface](../../../interface/tooltips.md).

## The tooltip system

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game has 170 tooltips, each with a priority and how long it shows, from the info scripts | done | `src/Help/ToolTips.cpp` (`help::tooltips`, `k_Count` 170), priority and times from the info tables (`InfoConstants::toolTips`); tests `ToolTips.AreTheHelpSystemsTooltipTexts` |
| What the hand is over offers a tooltip every turn; once a turn the shown one lives on or ends | done | `help::tooltips::Submit`, `ProcessTurn` (from `help::Process`), the hand's part `HandSystem::SubmitToolTips` (`HandToolTips.cpp`), as the original's ([the help system's part](../../../interface/tooltips.md#the-help-systems-part)); tests `HelpToolTipsTest.*`, `HandToolTips.*`; the hand demos |
| No tooltips at the None level | partial 80% | level 0 in `help::tooltips::ProcessTurn` deletes the icon and ends the turn, as the original's turn; no test |
| At the Minimum level only the important ones (numbers and stats) show | partial 80% | level 1 drops priorities under 0.9 (`Submit`, `ProcessTurn`), as the original's Submit; no test |
| At the Intelligent level each fades in a second slower each time it is shown, and fades out once read | done | the fade-in lasts as many seconds as the text was shown (capped at 80) and a priority under 0.9 fades out in 1 s once its time is over (`ProcessTurn`), as the original; test `HelpToolTipsTest.ANewLandStartsWithNoTooltipAndKeepsTheShowCounts`; the hand demos play at this, the default level |
| At the All level they show at once and stay | partial 80% | level 3: no fade-in (`ProcessTurn`), as the original's icon; no test |
| A tooltip keeps others off for its display time, and some linger after the hand leaves | done | the display and after-focus turns in `Submit` and `ProcessTurn`; tests `HelpToolTipsTest.ANewLandStartsWithNoTooltipAndKeepsTheShowCounts`, `HelpToolTipsTest.ATownsPopulationIsReadAgainEveryTurnFromTheObjectUnderTheHand` |
| A forced tooltip shows at once, over one still showing | done | `Submit(..., force)`, `Force`; test `HelpToolTipsTest.ANewLandStartsWithNoTooltipAndKeepsTheShowCounts` |
| The level is the one the player picked in the options | partial 70% | the profile's level is applied at start and the menu's choice at once (`help::tooltips::SetLevel` from `Game::ApplyPlayerSettings`), default 2 as the original's; no test that the menu's choice reaches the tooltips ([options.md](options.md)) |
| A new land clears the tooltip shown, but the times each one was shown are kept for the whole run; its icon stays drawn until the new land's first turn, which keeps it when the same text comes back | done | `help::tooltips::Reset` at the map clear keeps the show counts and the icon (since 2026-10-22, #206); the original never resets the counts and keeps them in no profile ([resets and a new land](../../../interface/tooltips.md#resets-and-a-new-land)); tests `HelpToolTipsTest.ANewLandStartsWithNoTooltipAndKeepsTheShowCounts`, `HelpToolTipsTest.ANewLandKeepsTheIconDrawnAndTheSameTextKeepsItsIcon` |

## How a tooltip looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Words beside the hand, sized by the screen's height and kept on the screen | partial 80% | the input prompt icon at the hand, S = screen height / 25, kept on the screen (`Renderer::DrawKeyOrMouse`), as the original's drawing; no test |
| A picture of the mouse with the button to press lit | partial 80% | `mousehelp.raw` with the bound button lit (`Renderer::DrawKeyOrMouseMouse`, `help::input_prompt::ResolveAction`); the button chosen is tested (`ToolTips.ShowTheButtonOfTheirAction`, `InputPromptIcons.TheZoomOnControlShowsBothButtons`), the picture's drawing is not |
| Arrows about the mouse for the ways it can be dragged | done | the arrow bits of the align (`HandToolTips.cpp`), drawn by `Renderer::DrawKeyOrMouse`; test `ToolTips.ArrowsAreTheHandsOwn` |
| Soft glows behind the mouse and the words | partial 75% | the soft blob panels from `atmos.raw` behind the icon and the text (`Renderer::AddKeyOrMousePanel`), compared with the original's code; no test, and a possible half-pixel shift of every 2D box is still open ([pending](../../../interface/tooltips.md#pending)) |
| Right of the hand, or left of it near the right of the screen | partial 80% | the side with its hysteresis in `Renderer::DrawKeyOrMouse`, as the original's; no test |
| Numbers filled into the words, as in "Food Amount: 120" | done | `help::tooltips::Numbers` and the text builder, read again every turn from the object under the hand, written as the original's `sprintf` writes them (`fixed_format::Fixed`); tests `HelpToolTipsTest.ATownsPopulationIsReadAgainEveryTurnFromTheObjectUnderTheHand`, `HandToolTips.StoragePitFoodGoesThroughAFloatAndTheWoodDoesNot`, `test_floating_number` |
| The tooltip hides while the menu or a script's cinema bars are up | partial 85% | an open menu box skips its draw (`input_prompt::Snapshot`, test `InputPromptIcons.AreNotDrawnUnderAnActiveBoxWhileTheirFadeGoesOn`); a turn paused outside the temple deletes it (test `HelpToolTipsTest.PausedOutsideTheCitadelTheTurnDeletesTheIconAndInsideKeepsIt`); under a script's wide screen with an owner none is shown (`ProcessTurn`), as the original, with no test |

## Where tooltips are offered

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a creature: Interact (the player's own, or another's in the influence) | done | `hand_tooltips::CreatureInteractToolTip` (`HandToolTips.cpp`); test `HandToolTips.CreatureInteractHasNoButton` |
| Inside the temple: moving, zooming, doors, scrolls and the rooms' toggles | done | `src/3D/TempleToolTips.cpp`; tests `TempleToolTips.*`; see ../temple/ |
| Over the land: Move, the camera's Tilt and Rotate hints near the screen's edges, Zoom Out close to the land, Pick Up over a fish | done | `hand_tooltips::LandToolTip` with the camera's hints (`HandToolTips.cpp`), as the original's land texts ([state 3](../../../interface/tooltips.md#state-3-normal-the-land-texts)); tests `HandToolTips.OverTheLandTheHandOffersToMoveOnTheLeftButtonWithAllArrows`, `HandToolTips.TheTiltHintComesBeforeTheTurnHint`, `HandToolTips.AHintComesBeforeAFishAndAFishBeforeTheMove`, `HandToolTips.ZoomOutNearTheLandWithTheCameraLookingDown` |
| Over everything else in the world (pick up, throw, cast, leash, tap and so on) | partial 90% | pick up, catch, tap, cast with a seed, the leash's texts, the one-shot orbs and spell icons, and with something in the hand combine scaffolds, give to the creature, sacrifice, apply, plant, drop and throw (`HandToolTips.cpp`; tests `HandToolTips.*`), in the original's order ([the text of each state](../../../interface/tooltips.md#the-text-of-each-state)); missing: the temple heart's text, and the help events the tooltips count |
| Over the player's built temple entrance: Enter Temple; another player's shows nothing | done | `worship::citadel::EntranceToolTipFor`, read by `HandSystem::SubmitToolTips`; test in `test/test_citadel_plan.cpp` |
| Over a help bubble, or dragging it: Scroll | done | `hand_tooltips::BubbleToolTip`; test `HandToolTips.AHelpBubbleOffersScrollOnTheActionButtonWithTheUpAndDownArrows` |
| On the dialogs' controls | inferred | whether the original's dialog controls show tooltips has not been read: its tooltip system serves only the hand; see [main_menu.md](main_menu.md) |

## Status readouts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village centre or its totem: the believers, then the people over the town's places; over a town's desire flag, the need it shows | done | `hand_tooltips::TownToolTips` from the town's running counts at that moment (`HandToolTips.cpp`, `town_stats::RoomOf`), read again every turn; the desire flag's own words; tests `HandToolTips.TownCentreShowsBelieversThenPopulationWithNoButton`, `HandToolTips.VillageTotemShowsItsTownCentresTexts`, `HandToolTips.ATownDesireFlagShowsTheNeedInItsOwnWords`, `HandToolTips.ATownDesireFlagsNeedIsHeldToAHundredAndADesireWithoutWordsShowsNone` |
| Over the village store: the food and wood stored | done | "Food Stored ... Wood ..." over a built storage pit (`HandToolTips.cpp`); tests `HandToolTips.OnlyABuiltStoragePitShowsItsStore`, `HandToolTips.StoragePitFoodGoesThroughAFloatAndTheWoodDoesNot` |
| Over a building being built: how far it has got, and the wood it still needs | done | `hand_tooltips::BuildingToolTips` (the player's own or nobody's, not whole), as the original's first test over an object; tests `HandToolTips.ABuildingThatIsNotWholeShowsTheWoodItWantsThenHowFarItIsBuilt`, `HandToolTips.ADamagedBuildingShowsHowFarItIsRepairedAndNoSiteShowsItTwice`, `HandToolTips.AWholeBuildingAnotherPlayersOrAStoppedTiltShowNoBuildingTexts` |
| Over a built workshop: its own text, as written; over a workshop's need sign: the wood it wants | done | `hand_tooltips::WorkshopToolTip`, `hand_tooltips::NeedsSignToolTip`; test `HandToolTips.OnlyABuiltWorkshopShowsItsRawText`; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Over a worship site's altar: the prayer power its site stores; over its need signs: the hunger or the rest wanted | done | `hand_tooltips::PrayerEnergyToolTip`, `hand_tooltips::NeedsSignToolTip`; tests `HandToolTips.AWorshipSitesAltarShowsThePrayerPowerItsSiteStores`, `test/test_altar_needs.cpp` ([the altar's need signs](../../../magic/worship.md#the-altars-need-signs)) |
| Over another god's town: the believers it still needs | done | a town centre's believers count turns into "Believers Needed" when negative (`HandToolTips.cpp`); test `HandToolTips.TownCentreBehindShowsTheBelieversNeeded` |
| Over the temple's heart: its own text | pending | the heart's tooltip, between the need signs and the town centre in the original's order, is not ported ([state 18](../../../interface/tooltips.md#state-18-over-object)) |
| Over a pile: the food or wood in it; over a field: Pick Up | partial 80% | over a pile, "Food Amount" or "Wood Amount" with its amount; over a field, "Pick Up", as the original ([the text of each state](../../../interface/tooltips.md#the-text-of-each-state)); `HandToolTips.cpp`; no test |
| Over a signpost or a scroll: what it is and that it can be read | done | "Activate" until it is lit, then "Reminder", on the action button, in or out of the influence (`hand_tooltips::ScriptHighlightToolTip` and `OverObjectShowsLand` in `HandToolTips.cpp`, tests `HandToolTips.AScriptHighlightShowsActivateUntilLitThenReminder`, `HandToolTips.OutOfTheInfluenceOnlyAScriptHighlightKeepsItsOwnText`); its tap tooltip override is always 0 (`script_highlight::OverwriteTapToolTip`, test `ScriptHighlight.TapToolTip`); see [scrolls_and_signs.md](scrolls_and_signs.md) and [the original](../../../interface/tooltips.md#tooltips-script-highlight) |
