# Hand in scripts

Every command of the challenge scripts that reads or drives the god hand: where it is and what it is over, what it holds,
clicks and drops, hand demos, highlights, force feedback, locking the creature in an interaction, and how much of the
interface the hand may use. The script language itself is in [../story/](../story/).

**Progress: 19/39 done, 4 partial — 54%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Where the hand is and what it is over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the hand's position, to test it near a thing or drop a marker there | done | `GET_HAND_POSITION` in `src/CHLApi.cpp` (the hand's Transform) |
| The position is that of the drawn hand, under the cursor | partial | Our `GET_HAND_POSITION` gives the drawn hand's Transform; the game gives the position the interface reports, the same outside drags (unconfirmed during drags) |
| Scripts read the object the hand is over | todo | `GET_OBJECT_HAND_IS_OVER` is a stub in `src/CHLApi.cpp` |
| Scripts read the hand's state: gripping, turning or zooming the land | done | `GET_HAND_STATE` pushes the interface's hand state of the last turn (`HandSystemInterface::GetInterfaceHandState`: 20 gripping the land, and so on) |
| Scripts read whether the player's hand is busy with a miracle charging, or that miracle | done | `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING` (`src/Magic/Script/CHLWorship.cpp`, `worship::player::AnySpellCharging`, `IsThatSpellCharging`) |
| Scripts stop a player's miracle from charging | done | `CLEAR_PLAYER_SPELL_CHARGING` -> `worship::player::CancelAllSpellsCharging` (`src/Magic/Script/CHLWorship.cpp`) |

## What the hand holds, clicks and drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the object the player's hand holds | todo | Both `GET_OBJECT_HELD` natives (199, 273) are stubs in `src/CHLApi.cpp` |
| Scripts test whether an object is in a creature's hand | todo | `IN_CREATURE_HAND` is a stub |
| Scripts read the last object the hand dropped, and clear it | todo | `GET_OBJECT_DROPPED`, `CLEAR_DROPPED_BY_OBJECT` are stubs |
| Scripts test whether an object was clicked, read it, and clear it | partial | `GAME_THING_CLICKED` and `CLEAR_CLICKED_OBJECT` read and clear the hand's tap memory (`HandSystem::RememberTapped`, 15 s); `GET_OBJECT_CLICKED` is a stub |
| Scripts test whether a place was clicked, and clear it | done | `POSITION_CLICKED`, `CLEAR_CLICKED_POSITION` (`HandSystem::PositionClicked`, `ClearClickedPosition`) |
| Scripts make an object one the hand may or may not pick up | done | `SET_ID_PICKUPABLE` -> `ecs::object_flags::SetPickupable`; the hand's press, grab and tap all check `object_flags::IsCannotBePickedUp`; test `test_hand_pick_reject` |
| Scripts read the town's totem, to watch the hand work it | todo | `GET_TOTEM_STATUE` is a stub |
| The last object picked up and dropped are forgotten when they are destroyed | partial | The tap memory forgets a destroyed object (`GAME_THING_CLICKED` checks it is still valid); nothing keeps the last object picked up or dropped |

## The interface the hand may use

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts set the interface level, which sets what the camera and hand may do and how far the hand reaches | done | `SET_INTERFACE_INTERACTION` -> `help::interface_interaction::Set` (`src/Help/InterfaceInteraction.cpp`): the interface flags, the camera features and the hand's reach (75 for JUST_GRAB, 1800 otherwise) |
| The level also makes the tooltip of a held object come from elsewhere | todo | Not ported |
| Scripts let the hand into the temple, or keep it out | done | `SET_INTERFACE_CITADEL` -> `worship::citadel::SetInterfaceCitadel`, read by the entrance's tap check |
| Scripts turn the cinema bars on, which hide the hand and take the interface away | done | `SET_WIDESCREEN` -> `help::script_control::SetWideScreen` (`src/Help/ScriptControl.cpp`); see [placement.md](placement.md) |
| Scripts ask whether the mouse has a wheel and how many buttons it has | todo | `HAS_MOUSE_WHEEL`, `NUM_MOUSE_BUTTONS` are stubs (false and 0) |
| Scripts ask whether a key is held | todo | `KEY_DOWN` is a stub |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts put a highlight (a challenge or silver scroll marker) at a place, for the hand to tap | done | `CREATE_HIGHLIGHT` -> `ecs::script_highlight::Create` (`src/ECS/ScriptHighlight.cpp`); see [../story/](../story/) |
| Scripts show or hide a highlight and set how it looks | done | `SET_DRAW_HIGHLIGHT` (the camera control's draw highlight setting) and `HIGHLIGHT_PROPERTIES` (`script_highlight::SetScriptId`) |
| Tapping a highlight marks it clicked for the script, with its chime | done | `ecs::script_highlight::InterfaceTap`: marked activated, with the chime for a "Did you know?"; see [clicking_and_activating.md](clicking_and_activating.md) |

## The creature and the leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts tie a creature's leash to the hand, or untie it | done | `ATTACH_OBJECT_LEASH_TO_HAND` -> `creature_leash::script::AttachToHand` |
| Scripts read what the creature is interacting with | todo | `CREATURE_INTERACTING_WITH` is a stub |
| Scripts read how hard the hand has stroked or slapped the creature | todo | `GET_INTERACTION_MAGNITUDE` is a stub; our tree keeps the sum (`CreatureHandSystem::GetFeedbackSum`) |
| Scripts test whether the creature is locked in an interaction with the hand | todo | `IS_LOCKED_INTERACTION` is a stub |
| Scripts light glows on the creature's hands | todo | `SET_CREATURE_CREED_PROPERTIES` is a stub; see [hand_effects_and_glows.md](hand_effects_and_glows.md) |

## Hand demos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts start a recorded hand demo by name | done | `PLAY_HAND_DEMO` -> `hand_demo::Play` (`src/Input/HandDemo.cpp`): `Data/HandDemo/<name>.hnd` replayed through the real hand, the camera set from each record. The throwing demo is played by [Throwing Stones](../story/silver_scrolls/throwing_stones.md); the totem and miracle demos by [Worship Site](../story/gold_scrolls/worship_site.md#the-miracle-hand-demo), the gestures demo by [Impress Village](../story/gold_scrolls/impress_village.md#the-gesture-hand-demo) and the influence demo by Khazar's lesson on influence ([impress_village.md](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence)) |
| A demo can be started with a pause on its triggers, or without moving the player's hand | done | `PLAY_HAND_DEMO`'s wait-for-trigger and keep-hand arguments go to `hand_demo::Play` (without keep-hand the held object is dropped and the charging spells cancelled) |
| Scripts wait until a demo has played | done | `IS_PLAYING_HAND_DEMO` pushes the negation of `hand_demo::IsPlaying`, as the original |
| Scripts wait for a demo's triggers, to talk over each step | done | `HAND_DEMO_TRIGGER` -> `hand_demo::ConsumeTrigger` |
| Scripts set which keys a demo shows being pressed | done | `SET_HAND_DEMO_KEYS` does nothing, as the original's empty handler |
| While a demo plays, the hand's camera hints come from the demo's recorded keys | todo | Not ported: the demo's camera tricon flags |

## Force feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts ask whether a force-feedback mouse is plugged in | partial | `IMMERSION_EXISTS` always pushes false; the only quest that needs one: [The Immersion Mushrooms](../story/silver_scrolls/the_immersion_mushrooms.md) |
| Scripts start and stop a force-feedback effect, or stop them all | todo | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` are stubs |

## Gestures and miracles from scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts draw a gesture with the hand, as the tutorial does | todo | `PLAY_GESTURE` is a stub; see [../gesture/](../gesture/) |
| Scripts give the player a miracle straight into the hand | done | `CREATE` of a one-shot spell in the hand -> `magic::script::CreateOneShotSpellInHand` -> `magic::one_off::CreateSpellIntoHand` (`src/Magic/Script/CHLWorship.cpp`); see [../miracles/](../miracles/) |
| Scripts set a virtual influence that lets the hand act where the player has none | todo | `SET_VIRTUAL_INFLUENCE` is a stub; see [../worship/](../worship/) |
