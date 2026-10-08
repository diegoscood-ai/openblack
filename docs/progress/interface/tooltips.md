# Tooltips

The words, mouse picture and arrows the hand shows beside it for what it is over: what a click would do, and numbers
such as a town's desires or a store's food. Which tooltip each thing in the world offers as the hand points at it is
in ../hand/; this file is the tooltip system and the status readouts it shows.

**Progress: 18/31 done, 4 partial — 65%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## The tooltip system

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game has 170 tooltips, each with a priority and how long it shows, from the info scripts | done | `src/Help/ToolTips.cpp` (`help::tooltips`, `k_Count` 170), priority and times from the info tables (`InfoConstants::toolTips`); tests `ToolTips.AreTheHelpSystemsTooltipTexts` |
| What the hand is over offers a tooltip every turn; once a turn the shown one lives on or ends | done | `help::tooltips::Submit`, `ProcessTurn` (from `help::Process`), the hand's part `HandSystem::SubmitToolTips` (`HandToolTips.cpp`) |
| No tooltips at the None level | done | level 0 in `help::tooltips::ProcessTurn` |
| At the Minimum level only the important ones (numbers and stats) show | done | level 1 keeps priorities of 0.9 and above (`Submit`, `ProcessTurn`) |
| At the Intelligent level each fades in a second slower each time it is shown, and fades out once read | done | the fade-in lasts as many seconds as the text was shown (capped at 80) and a low priority fades out in 1 s once its time is over (`ProcessTurn`) |
| At the All level they show at once and stay | done | level 3: no fade-in (`ProcessTurn`) |
| A tooltip keeps others off for its display time, and some linger after the hand leaves | done | the display and after-focus turns in `Submit` and `ProcessTurn` |
| A forced tooltip shows at once, over one still showing | done | `Submit(..., force)`, `Force` |
| The level is the one the player picked in the options | todo | the menu's choice is not passed to the tooltips (no caller of `help::tooltips::SetLevel`); the level stays at its default 2 ([options.md](options.md)) |
| How many times each tooltip has been shown is kept with the player's profile, so intelligent tooltips stay learnt | todo | the counts live in `help::tooltips` for the run only |

## How a tooltip looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Words beside the hand, sized by the screen's height and kept on the screen | done | the input prompt icon at the hand, S = screen height / 25, kept on the screen (`Renderer::DrawKeyOrMouse`) |
| A picture of the mouse with the button to press lit | done | `mousehelp.raw` with the bound button lit (`Renderer::DrawKeyOrMouseMouse`, `help::input_prompt::ResolveAction`); test `ToolTips.ShowTheButtonOfTheirAction` |
| Arrows about the mouse for the ways it can be dragged | done | the arrow bits of the align (`HandToolTips.cpp`), drawn by `Renderer::DrawKeyOrMouse`; test `ToolTips.ArrowsAreTheHandsOwn` |
| Soft glows behind the mouse and the words | done | the soft blob panels from `atmos.raw` behind the icon and the text (`Renderer::DrawKeyOrMouse`) |
| Right of the hand, or left of it near the right of the screen | done | the side with its hysteresis in `Renderer::DrawKeyOrMouse` |
| Numbers filled into the words, as in "Food Amount: 120" | done | `help::tooltips::Numbers` and the text builder: "Food Amount", "Wood Amount", "Food Stored ... Wood ...", the town's believers and population |
| The tooltip hides while the menu or a script's cinema bars are up | done | under a script's wide screen with an owner none is shown (`ProcessTurn`); while paused outside the temple the icon is deleted (`help::tooltips::Frame`), so the menu's pause hides it |

## Where tooltips are offered

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the player's own creature: interact | done | `HandSystem::SubmitToolTips` (`HandToolTips.cpp`): over a creature of the player's, "Interact" |
| Inside the temple: moving, zooming, doors, scrolls and the rooms' toggles | done | `src/3D/TempleToolTips.cpp`; see ../temple/ |
| Over everything else in the world (pick up, throw, cast, leash, tap and so on) | partial | pick up, catch, move, tap, drop, plant, throw, apply to a target and the piles' interact are offered (`HandToolTips.cpp`); cast, leash, scaffolds and giving to the creature are not ported |
| On the dialogs' controls | todo | see [main_menu.md](main_menu.md) |

## Status readouts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village centre: the town's population and its strongest desires as percentages | partial | the town's believers and population are shown (`HandToolTips.cpp`); the desires' readout is not ported |
| Over the village store: the food and wood stored | done | "Food Stored ... Wood ..." over a storage pit (`HandToolTips.cpp`) |
| Over a building being built: how far it has got, and the wood it still needs | todo | the site to build texts are not ported |
| Over a workshop: how far its scaffold is made, and a scaffold's value | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Over a worship site: people worshipping, prayer power, and a miracle's charge | todo |  |
| Over another god's town: the belief needed to win it, or left, or got | partial | a town centre's believers count turns into "Believers Needed" when negative (`HandToolTips.cpp`); the other readouts are not ported |
| Over a villager: their need as a percentage | todo |  |
| Over the temple: its health | todo | the temple's entrance offers "Enter Temple" (`worship::citadel::EntranceToolTipFor`); the heart's readout is not ported |
| Over a pile or a field: the amount of food or wood | partial | over a pile, "Food Amount" or "Wood Amount" with its amount (`HandToolTips.cpp`). Our wiki differs: over a field the original offers "Pick Up", not an amount ([hand and interface](../../bw1-notes/hand-and-interface.md#the-text-of-each-state-table-0xbf1c10)) |
| Over a signpost or a scroll: what it is and that it can be read | todo | a highlight's tap tooltip override is always 0 (`script_highlight::OverwriteTapToolTip`, test `ScriptHighlight.TapToolTip`); the script highlight texts are not ported; see [scrolls_and_signs.md](scrolls_and_signs.md) |
