# Hand in scripts

Every command of the challenge scripts that reads or drives the god hand: where it is and what it is over, what it holds,
clicks and drops, hand demos, highlights, force feedback, locking the creature in an interaction, and how much of the
interface the hand may use. The script language itself is in [../story/](../story/).

**Progress: 22/38 done, 0 chosen, 10 partial, 1 inferred — 76%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Where the hand is and what it is over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the hand's position, to test it near a thing or drop a marker there | done | `GET_HAND_POSITION` in `src/CHLApi.cpp` pushes the hand's position, x then y then z as vectors, as the original pushes its hand object's own place ([picking.md](../../../hand/picking.md)); test `ScriptHandNatives.TheHandsPositionIsGivenXThenYThenZ` (`test/test_script_hand_natives.cpp`) |
| The position is that of the drawn hand, under the cursor | partial 50% | The original pushes the hand object's own position; ours pushes the hand entity's Transform. Whether the two agree while the land is gripped and dragged is not checked |
| Scripts read the object the hand is over | pending | `GET_OBJECT_HAND_IS_OVER` is a stub in `src/CHLApi.cpp`; the original pushes the object the interface has under the hand, registered as a found thing, or 0 |
| Scripts read the hand's state: gripping, turning or zooming the land | partial 70% | `GET_HAND_STATE` pushes the interface's hand state of the last turn (`HandSystem::InterfaceHandState`, `HandToolTips.cpp`), built from openblack's own hand states rather than the original's action states: some of the original's rows are still marked pending there (a creature field that gives 14). The land-grip rows are tested (`HandToolTips.GrippingTheLandWithTheActionButtonHeldIsZoomLandscapeWhenTheZoomIsAllowed`); the native itself is not. Readout: the Camera window's hand state |
| Scripts read whether the player's hand is busy with a miracle charging, or that miracle | done | `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING` (`src/Magic/Script/CHLWorship.cpp`, `worship::player::AnySpellCharging`, `IsThatSpellCharging`), as the original's: any icon of the player's temple (of that seed's first miracle) whose chants are above 0, the charging flag not read ([worship](../../../magic/worship.md#the-players-view-of-its-icons)); tests `ScriptHandNatives.ASpellIsChargingWhileAnIconOfThePlayersTempleHoldsChants`, `ThatSpellIsChargingWhenTheIconOfItsSeedHoldsChants` (`test/test_script_hand_natives.cpp`) |
| Scripts stop a player's miracle from charging | done | `CLEAR_PLAYER_SPELL_CHARGING` -> `worship::player::CancelAllSpellsCharging` (`src/Magic/Script/CHLWorship.cpp`), as the original's: each icon of that temple the hand charges stops at the plain miracle and gives its chants back to its site ([worship](../../../magic/worship.md#the-players-view-of-its-icons)); test `ScriptHandNatives.ClearingThePlayersChargingGivesTheChantsBackToTheSite` (`test/test_script_hand_natives.cpp`) |

## What the hand holds, clicks and drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the object the player's hand holds | done | both `GET_OBJECT_HELD` natives: the hand's held thing (199) and the creature's (273), in `src/CHLApi.cpp`, as the original's ([picking.md](../../../hand/picking.md#what-the-hand-holds-for-the-scripts)); tests `ScriptQueryNatives.GetObjectHeldGivesWhatTheHandHoldsAsAFoundThing`, `ScriptQueryNatives.GetObjectHeldOfACreatureGivesWhatItHoldsAsAFoundThing` |
| Scripts test whether an object is in a creature's hand | done | `IN_CREATURE_HAND`: `InCreatureHand` (`src/CHLApi.cpp`, rule `script::property_rules::InCreatureHandNative`), true when the creature holds the thing or is eating it; see [../scripts/challenge_natives_creature.md](../scripts/challenge_natives_creature.md) |
| Scripts read the last object a creature dropped, and clear it | pending | `GET_OBJECT_DROPPED`, `CLEAR_DROPPED_BY_OBJECT` are stubs; the original's pops a creature and pushes what that creature last dropped (a creature's own memory, not the hand's) |
| Scripts test whether an object was clicked, read it, and clear it | done | `GAME_THING_CLICKED`, `GET_OBJECT_CLICKED` and `CLEAR_CLICKED_OBJECT` read and clear the hand's tap memory (`HandSystem::RememberTapped`, 15 s), as the original's ([picking.md](../../../hand/picking.md#tap-memory)); tests `HandClick.*`, `ClickNatives.TakeAndGiveWhatTheGameDoes` (`test/hand/test_hand_click.cpp`) |
| Scripts test whether a place was clicked, and clear it | done | `POSITION_CLICKED`, `CLEAR_CLICKED_POSITION` (`HandSystem::PositionClicked`, `ClearClickedPosition`); test `HandClick.APlaceIsClickedWithinTheRadiusAlongTheGround` |
| Scripts make an object one the hand may or may not pick up | done | `SET_ID_PICKUPABLE` -> `ecs::object_flags::SetPickupable`; the hand's press, grab and tap all check `object_flags::IsCannotBePickedUp`; test `test_hand_pick_reject` |
| Scripts read the town's totem, to watch the hand work it | done | `GET_TOTEM_STATUE` (`src/CHLApi.cpp`, `VillageTotemSystem::TotemOfTownCentre`): a town centre's totem, 0 for anything else, as the original ([village-totem.md](../../../magic/village-totem.md)); checked by the testbed's totem property command (`ScriptTotemProperty`, `src/Debug/TestbedScenarioRunner.cpp`) |
| The last object picked up and dropped are forgotten when they are destroyed | partial 40% | The tap memory forgets a destroyed object (`GAME_THING_CLICKED` checks it is still valid, as the original's store); the last object dropped is kept per player (`ecs::last_dropped`, `src/ECS/LastDropped.cpp`) but no script reads it yet, and nothing keeps the last object picked up |

## The interface the hand may use

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts set the interface level, which sets what the camera and hand may do and how far the hand reaches | done | `SET_INTERFACE_INTERACTION` -> `help::interface_interaction::Set` (`src/Help/InterfaceInteraction.cpp`): the interface flags, the camera features and the hand's reach (75 for JUST_GRAB, 1800 otherwise) |
| The level also makes the tooltip of a held object come from elsewhere | inferred | Not ported, and not found in the original: its SET_INTERFACE_INTERACTION sets the interface flags, the camera features and the hand's reach, and no tooltip write was found there |
| Scripts let the hand into the temple, or keep it out | done | `SET_INTERFACE_CITADEL` -> `worship::citadel::SetInterfaceCitadel`, read by the entrance's tap check; back to 1 only when the scripts restart (a new game, a playground's start), a land change keeps it (`Game::ResetScripts`, [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)) |
| Scripts turn the cinema bars on, which hide the hand and take the interface away | done | `SET_WIDESCREEN` -> `help::script_control::SetWideScreen` (`src/Help/ScriptControl.cpp`); see [placement.md](placement.md) |
| Scripts ask whether the mouse has a wheel and how many buttons it has | pending | `HAS_MOUSE_WHEEL`, `NUM_MOUSE_BUTTONS` are stubs (false and 0); the original answers from the system's mouse (the wheel flag, and the button count as a float) |
| Scripts ask whether a key is held | done | `KEY_DOWN` -> `KeyDown`: the game's key number to SDL's scancode (`src/Input/GameKeys.h`), held as the game's input has it (`GameActionInterface::IsKeyHeld`); test `test/input/test_key_down_native.cpp` |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts put a highlight (a challenge or silver scroll marker) at a place, for the hand to tap | done | `CREATE_HIGHLIGHT` -> `ecs::script_highlight::Create` (`src/ECS/ScriptHighlight.cpp`); see [../story/](../story/) |
| Scripts show or hide a highlight and set how it looks | done | `SET_DRAW_HIGHLIGHT` (the camera control's draw highlight setting, kept by a land change and put back on only when the scripts restart, `Game::ResetScripts`) and `HIGHLIGHT_PROPERTIES` (`script_highlight::SetScriptId`) |
| Tapping a highlight marks it clicked for the script, with its chime | done | `ecs::script_highlight::InterfaceTap`: marked activated, with the chime for a "Did you know?"; see [clicking_and_activating.md](clicking_and_activating.md) |

## The creature and the leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts tie a creature's leash to the hand, or untie it | done | `ATTACH_OBJECT_LEASH_TO_HAND` -> `creature_leash::script::AttachToHand` |
| Scripts read what the creature is interacting with | pending | `CREATURE_INTERACTING_WITH` is a stub; the original pops two things and asks the first whether it is interacting with the second |
| Scripts read how hard the hand has stroked or slapped the creature | done | `GET_INTERACTION_MAGNITUDE` reads the mind's last feedback (`CreatureMindState::interactionMagnitude`), set where the mind takes it (`CreatureMindSystem::UpdateAttitudeFromFeedback`, from a stroke or slap and from a blow put down to its player); DEV_FUNCTION 7 clears it. Read in the v1.42 exe ([the native](../../../creature/players-creature.md#the-natives-land-1-reaches-on-it)): the attitude's last feedback as a float, 0 for no thing or a thing not a creature; test `ScriptHandNatives.TheInteractionMagnitudeIsHowStronglyTheCreatureWasLastRewardedOrPunished` (`test/test_script_hand_natives.cpp`) |
| Scripts test whether the creature is locked in an interaction with the hand | partial 90% | `IS_LOCKED_INTERACTION`: any thing in the hand's locked select (a creature, a totem, a field, a fish farm or a pile), as the original's flag, pushed as a boolean, false for no thing; see [the locked select](../../../hand/states.md#what-the-scripts-read-of-it). Tests `ScriptHandNatives.AThingIsLockedInAnInteractionWhileItIsInTheHandsLockedSelect` (the native), `HandLockedSelect.AnyThingInTheSelectIsLockedFromItsStartToItsEnd` (the rule). Left: where the select's start takes nothing (a refused creature lock, a field or pile that gave nothing) openblack ends the select at once, so the thing is not locked for the turn the original keeps its flag until the end packet runs (⚠️ on that page) |
| Scripts light glows on the creature's hands | done | `SET_CREATURE_CREED_PROPERTIES` gives the glows and the hum, and they are drawn; see [hand_effects_and_glows.md](hand_effects_and_glows.md) |

## Hand demos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts start a recorded hand demo by name | done | `PLAY_HAND_DEMO` -> `hand_demo::Play` (`src/Input/HandDemo.cpp`): `Data/HandDemo/<name>.hnd` replayed through the real hand, the camera set from each record. The throwing demo is played by [Throwing Stones](../story/silver_scrolls/throwing_stones.md); the totem and miracle demos by [Worship Site](../story/gold_scrolls/worship_site.md#the-miracle-hand-demo), the gestures demo by [Impress Village](../story/gold_scrolls/impress_village.md#the-gesture-hand-demo) and the influence demo by Khazar's lesson on influence ([impress_village.md](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence)) |
| A demo can be started with a pause on its triggers, or without moving the player's hand | done | `PLAY_HAND_DEMO`'s wait-for-trigger and keep-hand arguments go to `hand_demo::Play` (without keep-hand the held object is dropped and the charging spells cancelled) |
| Scripts wait until a demo has played | done | `IS_PLAYING_HAND_DEMO` pushes the negation of `hand_demo::IsPlaying`, as the original. A demo ends early when its task stops or the scripts restart (a new game, a playground's start); a land change lets it play on, as the original's (`Game::ResetScripts`, [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)) |
| Scripts wait for a demo's triggers, to talk over each step | done | `HAND_DEMO_TRIGGER` -> `hand_demo::ConsumeTrigger`; a trigger still pending is forgotten when the scripts restart, kept by a land change (`Game::ResetScripts`, 2026-10-22, #206, [the scripts' restart's other writes](../../../scripts/land-change-and-saving.md#the-scripts-restarts-other-writes)) |
| Scripts set which keys a demo shows being pressed | done | `SET_HAND_DEMO_KEYS` does nothing, as the original's empty handler |
| While a demo plays, the hand's camera hints come from the demo's recorded keys | partial 85% | `hand_demo::PlayingHints` and `PlayingHintAngle`: the hand's poses (`HandSystem::HoverPose`) and the helper icons by the hand (`Game::FillHandTricons`), with no cross; tests `HandDemoHints.*`; the demo's mouse picture beside them, with its word «Demo» and the buttons the recording holds (`hand_tricons`, tests `HandTricons.TheDemoMouseChangesSideOnlyInTheOuterThirds`, `TheDemoMouseSitsRightOfTheIconsThenItsWord`, `TheDemoMouseLightsTheButtonsTheRecordingHolds`) |

## Force feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts ask whether a force-feedback mouse is plugged in | partial 50% | `IMMERSION_EXISTS` always pushes false, which is what the original answers on a machine without such a mouse ([states.md](../../../hand/states.md)); with one the original says true, and openblack has no force-feedback support. The only quest that needs one: [The Immersion Mushrooms](../story/silver_scrolls/the_immersion_mushrooms.md) |
| Scripts start and stop a force-feedback effect, or stop them all | pending | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` are stubs |

## Gestures and miracles from scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts give the player a miracle straight into the hand | done | `CREATE` of a one-shot spell in the hand -> `magic::script::CreateOneShotSpellInHand` -> `magic::one_off::CreateSpellIntoHand` (`src/Magic/Script/CHLWorship.cpp`); see [../miracles/](../miracles/) |
| Scripts set a virtual influence that lets the hand act where the player has none | done | `SET_VIRTUAL_INFLUENCE` -> `InfluenceSystem::SetVirtualInfluence` (`src/CHLApi.cpp`), as the original ([influence-and-alignment.md](../../../magic/influence-and-alignment.md)); tests `VirtualInfluence.*`, the testbed's virtual influence scenario; see [../worship/](../worship/) |
