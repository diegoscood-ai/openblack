# Pointing and tooltips

What the hand tells the player about what it is over: the word, mouse button and arrows it shows by the cursor ("Pick Up",
"Throw", "Rotate" …) and the numbers it shows over buildings, towns and the creature. How tooltips fade, their levels and
their look belong to [../interface/](../interface/); this file covers which tooltip the hand shows where.

**Progress: 17/49 done, 4 partial — 39%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## How the hand picks its tooltip

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each turn the hand submits the tooltip for what it is over or holding, with the mouse button and arrows that do it | done | `HandSystem::SubmitToolTips` (`HandToolTips.cpp`), called once a turn by `help::tooltips::ProcessTurn` (`src/Help/ToolTips.cpp`), with the hand state from `HandSystem::InterfaceHandState`; the button picture and arrows are drawn by `Renderer::DrawInputPrompts` |
| A tooltip of a number fills the number into its text, rounded to a whole | done | `help::tooltips` builds the text with `helptext::Format` (the texts' %3.0f), the forced amount and the "/places" suffix |
| A tooltip with no button to show is shown as words only | done | An action of -1 gives row 3: no picture (`help::tooltips::ProcessTurn`) |
| A shown tooltip lingers for its display time after the hand moves off, if its info says so | done | `help::tooltips::ProcessTurn`: a text nobody submits stays its afterFocus turns; no unit test |
| The hand over a debug window shows no tooltip | todo | Not found in our tree (openblack's own debug windows) |
| No tooltips while a script's cinematic has the interface or the menu is open | done | `help::tooltips::ProcessTurn` deletes it while a script holds the widescreen; `help::tooltips::Frame` while paused outside the temple |
| Showing a tooltip triggers the help system's matching help event | todo | Not in `help::tooltips` |

## Over things in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the player's own creature, awake: "Interact" | done | States 18 and 19 give "Interact" (`SubmitToolTips`, `creature_hand::ShowsInteractTip`) |
| Over something it can take: "Pick Up" or "Grab" | done | State 9: "Pick Up" (`SubmitToolTips`) |
| Over something flying: "Catch" | done | State 9: "Catch" for a flying object (`PhysicsObjects::IsFlying`) |
| Holding something: "Throw", "Drop" | done | State 24: "Drop", then "Throw" (`SubmitToolTips`) |
| Holding something over the creature: "Give" | todo | The "Give" text is not ported |
| Holding resources over a worship site or workshop: "Supply"; a villager over a worship site: "Sacrifice" | partial | A held object valid to apply to the target gives the apply text (a store, a workshop or a worship site, `HandSystem::HeldValidToApplyTo`); the sacrifice text is not ported |
| Holding a tree: "Plant"; wood over a building site: "Build"; a scaffold over another: "Combine Scaffolds" | partial | A tree gives "Plant"; the building site's and the scaffolds' texts are not ported |
| Over things that can be tapped: "Tap", "Tap To Break", "Tap To Open", "Activate" | partial | "Tap" and a rock's "Tap to Break" (states 9 and 18); the other objects' own tap texts are pending |
| Over the player's temple entrance: "Enter Temple" | done | State 18: "Enter Temple" over my built temple's entrance (`worship::citadel::EntranceToolTipFor`) |
| Over a signpost: "Signpost"; over a scroll: "Scroll" | todo | The script highlight's texts are not ported (`SubmitToolTips`, state 18) |
| In a creature fight: "Fight", "Attack", "Block", "Stop" | todo | No creature fight in our tree; see [../creature/](../creature/) |
| Over a building, its name (Abode, Village Store, Crèche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Field, Totem, Miracle Dispenser) | todo | The building and site texts of state 18 are not ported |

## Numbers over things

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a village store: "Stored - Food: … Wood: …" | done | State 18: the "Stored" text over a storage pit, its food then its wood (`ecs::StoragePitStore::GetResource`) |
| Over a store or pot: "Food Amount: …" or "Wood Amount: …" | done | State 13: the food or wood amount text over a pile, with its amount |
| Over a pile or other amount: "Amount: …" | done | The amount in the hand, forced every turn of the scooping and once at its end (`HandSystem::Update`) |
| Over a village centre: "Population: …" followed by its total in all | done | State 18 over a town centre: the population text with the adults and children, then "/" and the places |
| Over a village centre: "People Worshipping: …%" | todo | Not in our tree |
| Over a worship site: "Prayer Energy: …" | todo | Not in our tree |
| Over the temple: "Temple Health: …%", its health times 100 | todo | The temple heart's health text is not ported |
| Over a scaffold: "Scaffold Value …" | todo | No scaffolds in our tree |
| Over a building site: "Wood Required: …", or "Building Completed: …%" once no wood is needed | todo | Not in our tree |
| Over the workshop making something: "Production Completed: …%", the part made of the current scaffold | todo | The built workshop's production text is pending (`SubmitToolTips`) |
| Over a town's desire flag: "Need: …%", the town's raw desire capped at 100% | todo | The town desire flag's "Need" text is not ported |
| Over a town: its desires as percentages (food, wood, play, protection, mercy, expansion, civic building, worship supply, children, building, rain, sun, repair, workshop supply, wonder, relaxation, sleep, rest) | todo | The needs visuals' texts are not ported |
| Over a town not yet won: "Belief Left", the belief still to win times 1000, or "Belief Needed" when it is below nothing | done | State 18 over a town centre: "Belief Left" with (belief in me minus the most in another) x 1000, or "Believers Needed" below 0 (`ecs::town_belief`) |
| Over a town: "Got … Belief" | todo | Not in our tree |
| Over a worship icon charging: the miracle's name and its charge as a percentage, its name alone when uncharged | todo | A spell icon's own text is pending |
| Over some things: their food and rest desires as percentages | todo | Not in our tree |

## The leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash on the hand and nothing under it: "Focus Creature" | todo | No leash tooltips (hand-and-interface.md: not ported) |
| With the leash on the hand over the thing it is tied to: "Detach Leash" | todo | Not in our tree |
| With the leash on the hand over something else it can be tied to: "Attach Leash", once the creature has grown out of its first stages | todo | Not in our tree |
| Over a leash post: "Leash" and the leash's kind (Aggression, Compassion, Learning) | todo | Not in our tree |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over the land: "Move" | done | `HandSystem::SubmitLandToolTips`: "Move" with the left button and the four arrows |
| At the edges of the screen: "Rotate", "Pitch" or "Tilt/Rotate", by the hint the hand gives | todo | The camera's tricons are not ported |
| "Zoom In", "Zoom Out", "Zoom" while zooming | todo | "Zoom Out" is not ported |
| "Zoom In (…)" naming the place a key zooms to | todo | Not in our tree |
| "Double-click" where double clicking would fly there | todo | Not in our tree |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, its name, and "Increase" or "Extreme" for its power-ups | todo | State 5 (Has Magic) submits only the land texts out of the influence; the seed's texts are pending |
| Holding a miracle that needs a gesture: "Cast", "Cast Circle", "Gesture" | todo | "Cast" and its second casting text are pending (`SubmitToolTips`, state 5) |
| "Repeat Previous Miracle." | todo | Not in our tree |

## Elsewhere

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the temple, the tooltips of its rooms, doors, scrolls and creature | done | `src/3D/TempleToolTips.cpp` (`UpdateTempleToolTip`, from `TempleInterior`); test `TempleToggles.SayWhatTheyShowWhenHovered` (`test/temple/test_temple_toggles.cpp`); see [../temple/](../temple/) |
| Over the player's creature, or holding it, the creature's status panel shows its needs and the hand's reward | partial | `src/Creature/CreatureStatusPanel.cpp` and test `test/creature/test_creature_status_panel.cpp` exist, but nothing in the game uses the panel yet |
