# Pointing and tooltips

What the hand tells the player about what it is over: the word, mouse button and arrows it shows by the cursor ("Pick Up",
"Throw", "Rotate" …) and the numbers it shows over buildings, towns and the creature. How tooltips fade, their levels and
their look belong to [../interface/](../interface/); this file covers which tooltip the hand shows where.

**Progress: 35/48 done, 0 chosen, 4 partial, 4 inferred — 79%**

How the original does it, in our wiki: [The hand and the interface](../../../interface/tooltips.md).

## How the hand picks its tooltip

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each turn the hand submits the tooltip for what it is over or holding, with the mouse button and arrows that do it | done | `HandSystem::SubmitToolTips` (`HandToolTips.cpp`), called once a turn by `help::tooltips::ProcessTurn` (`src/Help/ToolTips.cpp`), with the hand state from `HandSystem::InterfaceHandState`; the button picture and arrows are drawn by `Renderer::DrawInputPrompts` |
| A tooltip of a number fills the number into its text, rounded to a whole | done | `help::tooltips` builds the text with `helptext::Format` (the texts' %3.0f), the forced amount and the "/places" suffix |
| A tooltip with no button to show is shown as words only | done | An action of -1 gives row 3: no picture (`help::tooltips::ProcessTurn`) |
| A shown tooltip lingers for its display time after the hand moves off, if its info says so | partial 80% | `help::tooltips::ProcessTurn`: a text nobody submits stays its afterFocus turns. Missing: no test pins the turns it stays (only the stats overlay's tooltip readout shows it) |
| No tooltips while a script's cinematic has the interface or the menu is open | done | `help::tooltips::ProcessTurn` deletes it while a script holds the widescreen, and in a turn paused outside the temple; between turns a pause leaves it drawn; an open menu box (`GameInterface::IsBoxActive`) leaves the frame's icons undrawn (`input_prompt::Snapshot`); tests `HelpToolTipsTest.PausedOutsideTheCitadelTheTurnDeletesTheIconAndInsideKeepsIt`, `InputPromptIcons.AreNotDrawnUnderAnActiveBoxWhileTheirFadeGoesOn` |
| Showing a tooltip triggers the help system's matching help event | pending | Each builder in the exe passes a help event with its text (for example 0x22 / 0x23 with a highlight's texts, 41 / 40 / 47 with the leash's, [tooltips.md](../../../interface/tooltips.md)); `help::tooltips::Submit` has no counterpart for them (only the focus-leash `Trigger(0x2F)` is ported) |

## Over things in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the player's own creature: "Interact" | done | States 18 and 19 give "Interact" (`SubmitToolTips`, `creature_hand::ShowsInteractTip`): the player's own creature, or another player's in the influence, with no asleep test, as the exe; test `HandToolTips.CreatureInteractHasNoButton` |
| Over something it can take: "Pick Up" | done | State 9: "Pick Up" (`SubmitToolTips`), the exe's only pick-up text but a class's own (a one-shot orb's, done; a scaffold's, below); test `HandToolTips.ObjectHandStateTakesTheOriginalsOrder` |
| Over something flying: "Catch" | done | State 9: "Catch" for a flying object (`PhysicsObjects::IsFlying`) |
| Holding something: "Throw", "Drop" | done | State 24: "Drop", then "Throw" (`SubmitToolTips`) |
| Holding something over the creature: "Give" | done | State 24: "Give" over a creature that would take it, before the store's apply text (`hand_tooltips::GiveToolTip`, `HandSystem::GiveVerdict`); from the exe, with raffclar PR #494 |
| Holding resources over a worship site or workshop: "Supply"; a villager over a worship site: "Sacrifice" | done | A held object valid to apply to the target gives the apply text (a store, a workshop or a worship site, `HandSystem::HeldValidToApplyTo`): "Sacrifice" over a worship site's altar, else "Supply" (`hand_tooltips::HeldToolTips`). The query text 0xE7C before them never shows in v1.42: no class answers its query test |
| Holding a tree: "Plant"; holding a scaffold: "Build" or the building it makes; a scaffold over another: "Combine Scaffolds" | done | State 24, as the exe's builder: a held tree's own drop text "Plant", a held scaffold's own drop text (the building its plan makes, else "Build", `scaffolds::GetOverwriteDropToolTip`), a scaffold over one it may join "Combine Scaffolds" first and over one it may not only the drop and throw texts (`hand_tooltips::HeldDropText`, `CombineScaffoldsToolTip`, `HeldToolTips`); tests `HandToolTips.AHeldTreeOrScaffoldShowsItsOwnDropText`, `HandToolTips.TwoScaffoldsThatJoinOfferOnlyCombineScaffolds`, `HandToolTips.TwoScaffoldsThatDoNotJoinSkipTheGiftAndTheApplyForTheDropTexts`. Wood over a building site gives the apply text "Supply" (row above) |
| Over things that can be tapped: "Tap", "Tap To Break", "Tap To Open", "Activate" | partial 75% | "Tap" and a rock's "Tap to Break" (states 9 and 18); a script highlight's "Activate" or "Reminder", in or out of the influence (`hand_tooltips::ScriptHighlightToolTip`; test `HandToolTips.AScriptHighlightShowsActivateUntilLitThenReminder`). Missing: a reward chest's own tap text "Tap To Open" (the chest's tap-text override in the exe); our state 18 gives every tappable thing but a rock the plain "Tap" |
| Over the player's temple entrance: "Enter Temple" | done | State 18: "Enter Temple" over my built temple's entrance (`worship::citadel::EntranceToolTipFor`) |
| Over a help bubble on the screen, or dragging it: "Scroll" on the action button with the up and down arrows | done | Hand states 0x1C / 0x1D: `hand_tooltips::BubbleToolTip` (`SubmitToolTips`); test `HandToolTips.AHelpBubbleOffersScrollOnTheActionButtonWithTheUpAndDownArrows`; debug: the stats overlay's hand state and tooltip |
| Over a signpost: "Signpost"; over a scroll: "Scroll" | inferred | Unverified: in the exe the hand over a challenge scroll or sign shows only "Activate" or "Reminder" (done above), and "Scroll" is the help bubble's text (done above); no tooltip builder read so far shows a "Signpost" text ([tooltips.md](../../../interface/tooltips.md)) |
| In a creature fight: "Fight", "Attack", "Block", "Stop" | pending | The exe has a hand tooltip builder (not identified by state) that gives one of three texts, over the player's own creature, over its creature's opponent, or else; which state uses it and their words are not read yet. Not ported (the fight's press is dormant); see [../creature/](../creature/) |
| Holding a scaffold, the building it would make, by name (Abode, Village Store, Crèche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Field, Miracle Dispenser) | done | The building names are a held scaffold's own drop text, the building its plan makes (the exe's scaffold drop-text override): `scaffolds::GetOverwriteDropToolTip` through `hand_tooltips::HeldDropText`; test `HandToolTips.AHeldTreeOrScaffoldShowsItsOwnDropText`. The hand over a building shows no name of its own in the exe's state 18 |

## Numbers over things

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village store: "Stored - Food: … Wood: …" | done | State 18: the "Stored" text over a storage pit, its food then its wood (`ecs::StoragePitStore::GetResource`) |
| Over a store or pot: "Food Amount: …" or "Wood Amount: …" | done | State 13: the food or wood amount text over a pile, with its amount |
| Over a pile or other amount: "Amount: …" | done | The amount in the hand, forced every turn of the scooping and once at its end (`HandSystem::Update`) |
| Over a village centre: "Population: …" followed by its total in all | done | State 18 over a town centre: the population text with the adults and children, then "/" and the places |
| While a village's totem moves: "People Worshipping: …%" | done | While a village's totem is re-placed, the local player's totem forces its worship share as a percentage (the exe's totem draw, [village-totem.md](../../../magic/village-totem.md)): `village_totem::ShowsShareToolTip` and `help::tooltips::Force(helptext::k_ToolTipWorshipShare, ...)` in `src/Worship/WorshipPercentage.cpp`; test `test/test_village_totem.cpp` |
| Over a worship site: "Prayer Energy: …" | done | over the site's altar (`WorshipTotem`) in the influence, after the storage pit and before the town desire flag: `hand_tooltips::PrayerEnergyToolTip` with `WorshipSite::battery` (`HandToolTips.cpp` state 18); test `HandToolTips.AWorshipSitesAltarShowsThePrayerPowerItsSiteStores`; debug: Magic window, "What each altar shows" (unverified against v1.42 in game) |
| Over the temple: "Temple Health: …%", its health times 100 | pending | The exe's state 18 has a temple-heart text after the need signs and before the town centre ([tooltips.md](../../../interface/tooltips.md), step 11); not ported |
| Over a scaffold: its own pick-up text, "Scaffold Value …" | pending | In the exe a scaffold overrides its pick-up text (state 9 shows the scaffold's own text in place of "Pick Up"); our state 9 gives every class but a one-shot orb the plain "Pick Up" (`HandToolTips.cpp`). The scaffold's value is `scaffolds::GetValue` |
| Over a building site: "Wood Required: …", or "Building Completed: …%" once no wood is needed | done | State 18, first: over the local player's abode, field, temple heart or worship site that is not built or not repaired, while the scripts allow the tilt (`hand_tooltips::BuildingToolTips`), and over a village totem or a town centre's spell icon the same way, a worship site's icon and a town centre's with its site's icon of the same spell skipped; from the exe, with raffclar PR #494 |
| Over a built workshop: "Production Completed: …%", shown as written (the exe fills in no number) | done | State 18 over a built workshop: its text with no button and no number filled in, as the exe (its number builder has no case for that text, so it shows as written): `hand_tooltips::WorkshopToolTip`; test `HandToolTips.OnlyABuiltWorkshopShowsItsRawText` |
| Over a town's desire flag: "Need: …%", the town's raw desire capped at 100% | done | `hand_tooltips::TownDesireFlagToolTip`, inside the influence, between the altar and the highlight as the exe's state 18 ([tooltips.md](../../../interface/tooltips.md)); tests `HandToolTips.ATownDesireFlagShowsTheNeedInItsOwnWords`, `HandToolTips.ATownDesireFlagsNeedIsHeldToAHundredAndADesireWithoutWordsShowsNone` |
| Over a town: its desires as percentages (food, wood, play, protection, mercy, expansion, civic building, worship supply, children, building, rain, sun, repair, workshop supply, wonder, relaxation, sleep, rest) | done | The town's desires are shown on its desire flags, each in its own words (the row above, `hand_tooltips::TownDesireFlagToolTip`), and the altar's and workshop's need signs: the food desire, the rest desire and the wood wanted (`hand_tooltips::NeedsSignToolTip`, state 18, in or out of the influence; test `AltarNeeds.ASignsToolTipIsItsRows`; debug: Temple window, "Altar need signs"; testbed `worship.altar_needs`; [the altar's need signs](../../../magic/worship.md#the-altars-need-signs)). The exe's state 18 has no other desire text |
| Over a town not yet won: "Belief Left", the belief still to win times 1000, or "Belief Needed" when it is below nothing | done | State 18 over a town centre: "Belief Left" with (belief in me minus the most in another) x 1000, or "Believers Needed" below 0 (`ecs::town_belief`) |
| When the player gains believers: "Got … Believers" | done | When the local player gains belief in a turn, the exe forces "Got … Believers" with the gain × 1000 ([towns.md](../../../beings/towns.md)): `events::TownBeliefToolTip` with `k_ToolTipBelieversGained` (`src/ECS/Town/TownBelief.cpp`), shown by `help::tooltips::Force`; test `test/test_town_belief.cpp` |
| Over a worship icon charging: the miracle's name and its charge as a percentage, its name alone when uncharged | partial 70% | State 18 over a spell icon: its seed's own tooltip, forced, with no button, and nothing else (`hand_tooltips::SpellIconToolTip`; test `HandToolTips.SpellIconShowsItsSeedsTooltipForcedWithNoButton`). Not checked: whether the number builder fills a charge into that text |
| Over an altar's need signs: the food and rest desires | done | The altar's need signs: the food desire «… %» and the rest desire (no per cent sign, as the exe), `hand_tooltips::NeedsSignToolTip`; test `AltarNeeds.ASignsToolTipIsItsRows` |

## The leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash on the hand and nothing under it: "Focus Creature" | done | "Bring Creature", which also counts the help's focus-leash event: `hand_tooltips::LeashToolTipOf` (`SubmitLeashToolTip`, asked first by the land, pick-up, locked-select and over-object tooltips); test `HandToolTips.TheLeashOffersDetachOverItsTieAttachOverAThingAndBringCreatureOverNothing`; debug: the stats overlay's hand state and tooltip |
| With the leash on the hand over the thing it is tied to: "Detach Leash" | done | `hand_tooltips::LeashToolTipOf` (`SubmitLeashToolTip`, asked first by the land, pick-up, locked-select and over-object tooltips); test `HandToolTips.TheLeashOffersDetachOverItsTieAttachOverAThingAndBringCreatureOverNothing`; debug: the stats overlay's hand state and tooltip |
| With the leash on the hand over something else it can be tied to: "Attach Leash", once the creature has grown out of its first stages | done | from the fourth development stage, never over the creature, a script highlight, a one-shot orb or a spell icon: `hand_tooltips::LeashToolTipOf` (`SubmitLeashToolTip`, asked first by the land, pick-up, locked-select and over-object tooltips); test `HandToolTips.TheLeashOffersDetachOverItsTieAttachOverAThingAndBringCreatureOverNothing`; debug: the stats overlay's hand state and tooltip |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the land: "Move" | done | `HandSystem::SubmitLandToolTips`: "Move" with the left button and the four arrows |
| At the edges of the screen: "Rotate", "Pitch" or "Tilt/Rotate", by the hint the hand gives | done | `hand_tooltips::LandToolTip`: "Pitch" then "Rotate" from the player's camera's hints, each with its camera feature; from the exe, with raffclar PR #494 |
| "Zoom Out" near the land, "Zoom" while zooming | done | "Zoom" while the land is gripped with the action button held too: hand state 22, `hand_tooltips::ZoomLandscapeToolTip` (test `HandToolTips.GrippingTheLandWithTheActionButtonHeldIsZoomLandscapeWhenTheZoomIsAllowed`); "Zoom Out" near the land looking down, with the wheel's picture, or both buttons on a mouse with no wheel (`hand_tooltips::LandToolTip`, `input_prompt::MouseClickTypeOf`; tests `HandToolTips.ZoomOutNearTheLandWithTheCameraLookingDown`, `HandToolTips.ZoomOutShowsTheWheelOfTheZoomInControl`, `HandToolTips.OnAMouseWithNoWheelTheZoomControlsShowBothButtons`). The exe's land texts have no "Zoom In" |
| "Zoom In (…)" naming the place a key zooms to | inferred | Unverified: none of the exe's hand tooltip builders read so far ([tooltips.md](../../../interface/tooltips.md)) shows it; which part, if any, shows this text is not known |
| "Double-click" where double clicking would fly there | inferred | Unverified: none of the exe's hand tooltip builders read so far shows it; which part, if any, shows this text is not known |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, its name at its power, shown when it is taken or powered up | pending | In the exe, setting a held seed's power-up for the local interface forces the tooltip of that level's miracle effect (its name at that power). `magic::seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`) does the rest of that step but not the tooltip |
| Holding a miracle: "Cast", or its sized casting text for the storms, the tornado and the shields | done | State 5, as the exe: in the influence with a seed, "Cast" over an object it can take, and over the land "Cast" or the sized casting text for the storms, the tornado and the shields; otherwise the land texts (`hand_tooltips::SeedToolTip`); tests `HandToolTips.SeedOutOfTheInfluenceShowsTheLand`, `HandToolTips.NoSeedShowsNothing`, `HandToolTips.SeedOverAnObjectCastsOnItOrShowsTheLand`, `HandToolTips.SeedOverTheLandShowsTheSizedMiraclesOwnText` |
| "Repeat Previous Miracle." | inferred | Unverified: none of the exe's hand tooltip builders read so far shows it; which part, if any, shows this text is not known |

## Elsewhere

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the temple, the tooltips of its rooms, doors, scrolls and creature | done | `src/3D/TempleToolTips.cpp` (`UpdateTempleToolTip`, from `TempleInterior`); test `TempleToggles.SayWhatTheyShowWhenHovered` (`test/temple/test_temple_toggles.cpp`); see [../temple/](../temple/) |
| Over the player's creature, or holding it, the creature's status panel shows its needs and the hand's reward | partial 85% | `src/Creature/CreatureStatusPanel.cpp`, set each frame from the creature the hand holds (with the hand's reward) or the one Creature Mode follows (`Game.cpp`, `creature_panel_choice::Choose`) and drawn by `GameInterface`; test `test/test_creature_status_panel.cpp`; testbed `hand.status_panel`. Missing: the panel's whole-pixel layout ([fights.md](../../../creature/fights.md)) |
