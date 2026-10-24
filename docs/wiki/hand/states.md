# Hand and interface states

## Summary

The hand is how the player touches the world. It grabs, holds and throws things, scoops wood and food out of piles
and fields, and strokes or slaps the creature. Behind it the original runs two state machines.

- The **interface** (`GInterface`) has an **action state** for what the buttons are doing: idle, a grab, a locked
  select, a throw, a release... Each state has its own process and its own end. The interface also has a **cursor
  state** for what the hand shows.
- The drawn hand (`CHand`) has a **hand state**: normal, tug, holding, totem, creature, grain... It asks every frame
  which state it should be in.

The buttons reach the interface as messages. What the hand does to the world goes out as packets, which every
machine applies at the start of the next turn. A box on screen (the escape menu, a dialog) takes the mouse and ends
the action. Taking from a pile, a field or a fish farm, and holding the creature, are both a **locked select**: the
object is locked to the hand until the button is let go. On a creature the press becomes a locked select, the hand
enters the CREATURE state, and strokes and slaps go to its mind.

openblack keeps the press's branch order, the locked select and its packets, the CREATURE state's timer and
feedback, the box rule, the per-turn packets and the grain state, and the CREATURE state's clips, their blends and
the interaction camera's place as the original's. Still different: the slap's speed, the CREATURE state's palm
zoomers and end-ready test, which camera modes the interaction camera takes the view from, some action states' ends,
and the leash's taps from a creature. The grab, the hold and the
throw are on [Grabbing, holding and throwing](grab-hold-and-throw.md#holding-spring-and-throwing).

## The original

The page goes from the input to the world: the interface's states and how the buttons reach them, the packets and
the turn, the locked select, the creature, the scoop, and then the drawn hand's own states.

### Interface action states

The action state is `GInterface+0x44`. Its table is 0xD17DA8: 24 states of 0x30 bytes, filled by the static
initializer 0x5D0890. In each row +0x00 is the state's process and +0x10 its end. (The hand states' table is filled
by 0x5D7960; see [The locked select](#the-locked-select).)

| State | Name in the exe | Use |
|---|---|---|
| 0 | NORMAL | idle (process 0x5D5190, shared with 1) |
| 2 | LANDSCAPE LOCK | grabbing the terrain (camera, from 0x5D1AB0) |
| 3–6 | LOCKED SELECT… | taking from a pile in batches, a field, a fish farm, the creature |
| 7 | WAIT FOR PLACE IN HAND | after taking, waits for the package |
| 8, 9 | apply after the recorder | a seed whose info +0x110 is 1 |
| 10, 11 | locked apply | the land / an object |
| 12 | **IN THROW** | second press with the hand full: drop/throw on release |
| 13 | grab (225 ms) | grab press |
| 15 | give | packet 0x1F (0x5D3080) |
| 17 | BUBBLE GRIP | a screen object |
| 19 | tap or lock | the 225 ms wait |
| 21 | leash pull | the leash in the action collide |

States 20 and 22 are never set through `SetActionState` in the interface's own code.

**Cursor states.** Each state has a cursor state (`GInterface+0x3AC`). IN THROW is **0x17**, which enables the
holding spring. The cursor states read are 1, 2, 12, 14, 19, 23 (0x17) and 25 (the interface inactive). GInterface
+0x3AC starts at 3 (`SetToZero` 0x5CE4F0).

**The object's flags.** The interface reads these bits of `Object +0x24`: 0x4 IN_HAND, 0x10 LOCKED_SELECT, 0x20 not
pickable now (a press then taps), 0x40 IN_PHYSICS, 0x400 refuses gifts. The hand holds one object at most
(`IsSpaceInHands`: count < 1).

#### How the buttons reach the interface

1. **The window's thread.** `LHMouse::SetButtons` 0x7E4B80 ORs each event into the held mask [0xE85304] and the
   latched events [0xE85350]: 1 / 2 / 0x40 the left / right / middle down, 4 / 8 / 0x80 their up. A down clears its
   button's pending up in both (jump table 0x7E4D88, index bytes 0x7E4DB0); an up clears the held bit only.
2. **Messages, every frame.** Paused or not (`GGame::Loop` 0x54D3E9 → `ProcessFrameInputs` 0x54C340),
   `CMouse::ProcessButtons` 0x61A150 turns the latched events into interface messages 1..6 (fn_005D9DD0). It is
   called at 0x54C398, not while a hand demo plays. It reads them through the ControlMap's two button bindings (g_game
   +0x250300, slot 1 +0x30C, slot 2 +0x618): press fn_00470E80 / fn_00470B30 (messages 1 / 3), release fn_00471170 /
   fn_00470E20 (2 / 4), the double ones fn_00471130 / fn_00470DE0 (5 / 6), else a move (7), sent as
   `GInterface::SendMessageA(0, coord)`. Then it clears the latched events (0x61A3FC..0x61A408).
   `GInterfaceMessageBuffer::Add` keeps each message with a snapshot of the action collide (+0x3E8) and the point.
3. **A binding's pressed flag** (+0x6514 / +0x6510) is set by its press and cleared only by its release; nothing
   else writes it but the ControlMap's constructor. A mouse press (+0x651C / +0x6518 = 1, 0x470F3F) is released only
   by the latched up (fn_0046F660 bits 4 / 8 / 0x80), a key press by the key no longer down (`IsKeyBeingPressed`
   0x46F4C0). No press is sent while the flag is set (0x470F32 / 0x470BE2), and no release while it is clear. So a
   second down with no up between sends nothing, and an up without its press sends nothing.
4. **The left double click is an event, taken once.** WM_LBUTTONDBLCLK latches 0x10 in [0xE85350]
   (`SetButtons(0x10)` 0x7DC280). `ProcessButtons` reads it through fn_00471130 (0x61A346), and fn_0046F5F0 clears
   the bit as it reads it (0x46F647). The message 5 it sends (fn_0061A4C0 0x61A4F5 → fn_005D9DD0, the temple's
   `ProcessMouse` instead while the interior is up) runs entry 5 of the table 0xD186B8, 0x5D99C0. That sets
   `GInterface` +0x40 |= 4 (0x5D99D9) unless its vt+0x40C returns 0 or the action state +0x44 is 1.
   `CameraModeNew3::Update` tests that bit (0x45DD83) and, in the double click's branch (feature 0x10, 0x45DDA3),
   clears it (0x45DDE9) together with the latched 0x10 (0x45DDB9) before the flight and the creature lock-on. So one
   double click flies, or locks on, once, however long its button stays down.
5. **The interface pass.** It comes after `ProcessButtons` in the same frame (`ProcessFrameUpdates` 0x54C3B6), and
   also runs each turn: `GInterface::Process` 0x5CEC10 → fn_005D2250 (the sync packets) → fn_005D9A20 (the messages).
   fn_005D9BC0, once per message, restores the collide snapshot when no action is latched. The message table
   0xD186B8 sets the button bits: 0x100 / 0x2000 grab down / up, 0x200 / 0x4000 action down / up, 0x800 the double.
   Then fn_005D1120 `InterfaceActionProcess` runs: `ValidateObjectsForStates` 0x5D1010, the influence (+0x48 =
   `CalculatePlayerInfluence(action point) > 0`), the GUI and help (which may swallow the click, 0x14), then the
   state's process.
6. **The idle state** dispatches the bits: 0x100 → the land grip; 0x200 → `ActionPressed`; 0x800 → the leash's double
   click (0x5D4210, [The leash](../creature/leash.md#tying-the-leash-with-the-hand)); 0x400 → 0x5D21B0, the remove
   from the hand. That last path, packet 0x14, is dead. Its `ProcessPacket` jump-table entry (0x63DDCC, entry 14) is
   the default exit 0x63DDBF, and nothing sets bit 0x400: the sender 0x5D21B0 has one caller, 0x5D521B, reached when
   GInterface +0x39 & 4, and the +0x39 bits are only ever set to 1, 0x20, 2, 8, 0x40 and 0x10.
7. **While the game is paused** (`g_game` +0x14 bit 4, tested at 0x5D51BC) the idle state takes only the land grip:
   with no bit of +0x38's low byte and 0x100 set it calls the grip 0x5D1AB0 at once (0x5D5227..0x5D5239), and
   nothing else. The action press, the double click and the remove start nothing, so the camera moves but nothing is
   picked up, tapped, given, cast or armed for a throw (verified 2026-10-20, fork issue #158). The other states have
   no pause test; their packets wait for the next turn, which pause holds back. openblack:
   `hand_press::IdleTakesActionPress` in `HandSystem::Update` drops the action press and its double click while
   `game_clock::IsPaused()`; the grip and the release go on (test
   `HandPressChain.WhilePausedTheIdleHandTakesNoActionPress`). The P key and the debug bar's "paused" readout are its
   controls.

#### The interface's functions

The names the reconstruction of the interface gives these functions (each read instruction by instruction there),
with what the other sections do not say:

| Address | Name | What |
|---|---|---|
| 0x5D98E0 | `GInterface::Msg_GrabDown` (message 1) | when the low byte of m_Buttons is 0 or has bit 0: \|= 0x100; m_Held (+0x40) \|= 2 |
| 0x5D9900 | `GInterface::Msg_GrabUp` (2) | \|= 0x2000; m_Held &= ~2 |
| 0x5D9910 | `GInterface::Msg_ActionDown` (3) | as the grab, \|= 0x200; m_Held \|= 1 |
| 0x5D9960 | `GInterface::Msg_ActionUp` (4) | \|= 0x4000; m_Held &= ~1 |
| 0x5D9930 | `GInterface::Msg_ActionDbl` (6) | \|= 0x800 \| 0x200 and m_Held \|= 1 under the same test (the nesting 🔍 inferred) |
| 0x5D99C0 | `GInterface::Msg_5` | active and not in action state 1: m_Held \|= 4 |
| 0x5D99E0 | `GInterface::Post_MouseMove` | fn_005CECF0, the land pick fn_005D5980, fn_005CEAD0 |
| 0x5D9A20 | `GInterface::ProcessMessages` (the messages come in through `GInterface::fn_005D9DD0`) | each message, then fn_005D9A10 (m_Buttons = 0 when its low byte is 0) |
| 0x5D9BC0 | `GInterface::ProcessOneMessage` | fn_005D9D20 copies the collide when a button went down |
| 0x5D1120 | `GInterface::InterfaceActionProcess` | the frame's and the turn's action pass |
| 0x5D1010 | `GInterface::InterfaceActionValidateObjectsForStates` | the state table's flag B (fn_005D1110: the state needs a valid object); flag A is fn_005D1100, and +0x2C "grab and action both held: the landscape" |
| 0x5D1260 | `GInterface::EndAction` | the state's exit, then the reset |
| 0x5D2980 | `GInterface::SetActionState` | stores the state when it changes; fn_005D27F0 is `SetActionState(12)` |
| 0x5D5190 | `GInterface::State_Idle` | 0x800 → fn_005D4210; 0x100 → fn_005D1320 (a `jmp` to the land grip 0x5D1AB0); 0x400 → fn_005D21B0 |
| 0x5D1330 | `GInterface::ActionPressed` | fn_005D1560 `ActionPressedHolding`, fn_005D4440 (the leash), fn_005D39E0, fn_005D1A00, fn_005D1740 `StartGrab`, fn_005D3D10 `ClickOnLand` |
| 0x5D3930 | `GInterface::Tap` (fn_005D3930) | the tap in place of a grab |
| 0x5D5250 | `GInterface::State_Grab` | fn_005D2800 `GenericPickup` (packet 0x13) |
| 0x5D4A90 | `GInterface::State_WaitPickup` | |
| 0x5D38A0 | `GInterface::SendTap` | |
| 0x5D1A00 | `GInterface::StartTapOrLockedSelect` | fn_005D1950 at once |
| 0x5D53D0 | `GInterface::State_TapOrLock` | |
| 0x5D4180 | `SendLeashCreatureAt` (fn_005D4180) | packet 0x5D, when `ValidAsInterfaceLeashTarget` |
| 0x5D1560 | `GInterface::ActionPressedHolding` | fn_005D3080 the give, fn_005D30D0 `GInterface::SendApplyToObject` |
| 0x5D4DB0 | `GInterface::State_ReleaseHeld` | the prediction body `PhysicsObject::PredictionPhysOb` (de-initialised first when `PredictionActive`), its object `PhysicsObject::PredictionObject`, the velocity cut to 124 m/s |
| 0x5D1850 | `GInterface::DropOnMapCoord` | the gesture's point (fn_0057C960 on +0x1A4), then fn_005D3340 |
| 0x5D3340 | `GInterface::SendApplyToMapCoord` | the 0x4D's block also copied to 0xD17D20; the hand and camera sync packet through fn_005515D0 (0x16) |
| 0x5D18F0 | `GInterface::FailApply` | |
| 0x5DA400 | `GInterface::Pkt_ApplyToMapCoord` | SingleMapFixed / `Mobile::ApplyThisToMapCoord` (0x52F420 / 0x606BF0) |
| 0x5DA100 | `GInterface::HandleApplyResult` | |
| 0x5D5A70 | `GInterface::UpdateInterfaceCollide` | ([Giving a held object](grab-hold-and-throw.md#giving-a-held-object)) |
| 0x5D39E0 | `GInterface::FindObjectNearMapCoord` | |
| 0x61A150 | `CMouse::ProcessButtons` | `ControlMap::fn_00471130` → fn_0061A4C0 (5), `ControlMap::fn_00470E80` → fn_0061A420 (1), `ControlMap::fn_00471170` → fn_0061A530 (2), `ControlMap::fn_00470DE0` → fn_0061A510 (6), `ControlMap::fn_00470B30` → fn_0061A470 (3), `ControlMap::fn_00470E20` → fn_0061A580 (4), none → fn_0061A5D0; the temple's `Temple::ProcessMouse` while the interior is up |

**The interface's fields.** +0x40 is m_Held: bit 0 the action held, bit 1 the grab held, bit 2 message 5's flag.
fn_005D6540 latches the hover (the byte 8 and the lock's turn). The local player is g_game +0x18 + 0xA60 × the byte
at g_game +0x205A59.

#### The bubble grip, the pick-up wait and the help pass

- **Action state 17, BUBBLE GRIP.** An empty hand pressing on a screen object (ActionPressed 0x5D1364 → fn_005D4330)
  sets +0x38 |= 0x10. The release (0x5D9960, +0x39 |= 0x40) → state 17's process 0x5D53B0 clears the dword +0x38 and
  calls `ResetActionState` 0x5D29C0.
- **Dragging a help bubble.** +0x3AC = 0x1D while GInterface +0x3D0 is the bubble; the scroll follows the mouse y and
  the first frame only takes the point (0x5772E9..0x57734A).
- **Action state 7, WAIT FOR PLACE IN HAND,** is `State_WaitPickup` 0x5D4A90: it waits for the object's Flags &
  IN_HAND (4), then idles. GenericPickup sends packet 0x13 at 0x5D2864 and sets state 7 at 0x5D286D. For a teleport
  stone the 0x13 carries the stone itself; its seed is found in the handler.
- **The help pass.** `HelpSystem::Process` recomputes the hand state every turn (fn_005D7E40 at 0x5C8FF0; also on the
  three exits of fn_005D1120) and sets +0x578 = 0 (0x5C8FF7), read by SendFOVObject 0x5C90B5.

#### Interface active

- **The flags.** `GInterface+0x28`: bit 0 = inactive; bits 1 and 2 = SET_INTERFACE_INTERACTION limits (jump table
  0x70B7A8). `GInterface::IsActive` 0x5CE2E0 = !(+0x28 & 1). While inactive the interface's hand state is 25
  (fn_005D7E40).
- **What it stops.** Each action pass (`InterfaceActionValidateObjectsForStates` 0x5D1010, from `Validate` 0x5D05F3
  and `InterfaceActionProcess` 0x5D1123, so each frame and each turn) first asks `IsActive` (vt 0x40C, 0x5D1016) and,
  while the interface is inactive and the action state (+0x44) is not 0xE, ends the action: `EndAction` fn_005D1260
  (the state's exit, the action objects cleared, `ResetActionState`) at 0x5D1059..0x5D106B. The press bits are not
  dropped before that (the idle state still dispatches them later in the pass) [?: whether a press's first frame
  takes effect before the next pass ends it, and what action state 0xE is, are not read]. While the script holds
  the bars and no hand demo plays, the same pass also sets the interface inactive again (0x5D1020..0x5D1050).
  openblack: not ported; the hand takes presses under a script's bars (raffclar's #325 drops the presses at the
  event instead, not taken). Pending.
- **Setting it.** `GInterface::SetActive` 0x5CEDC0 writes bit 0 = (active == 0). Its other effects: HelpSystem
  +0x460C = bit 0 of GInterface +0x39 (0x5CEDDF), +0x40 &= ~4, fn_005D1260, `ResetActionState` 0x5D29C0 and
  fn_005D81C0 (`StopAllImmersion`). `HelpSystem::ProcessInterface` clears +0x460C.
- **The script's wide screen.** `HelpSystem::SetWideScreen` 0x5C6AD0 → `SetActive(!(on && owner))` (0x5C6AF4 /
  0x5C6B01). A hand demo makes the interface active again so its recorded messages drive the real hand
  (`StartPlayBack` 0x5DAD60), and inactive when it ends with a script still holding the bars (`EndPlayBack`
  0x5DB3F0); the demos themselves are on [Hand demos](../project/debugging.md#hand-demos). While a script holds the
  widescreen, no events are processed [0x63EF90..0x63EFA8, 0x63F024..0x63F03E].
- **The keys.** `GGame::ProcessKey` (0x63F40C..0x63F446) skips the LH_KEY 2..15 block (KB_1..KB_TAB) while a script
  holds the widescreen (HelpSystem +0x45E8 && +0x45EC) or while a SET_INTERFACE_INTERACTION level has a ControlMap
  switch turned off (0x63F42A..0x63F446 requires both switches; see
  [Reach and the ControlMap switches](picking.md#reach-and-the-controlmap-switches)).
- **The force-feedback mouse.** `IMMERSION_EXISTS` 304 (`GScript::ImmersionExists` 0x6FDFA0) pushes (type 6)
  whether the global 0xD18588 is not null. That global is the Immersion force-feedback mouse (`CImmMouse`), made in
  sub_5D86A0, which only `GInterface::SetupStatics` (0x5CE401) reaches. When `CImmMouse::Initialize` fails (0x5D870B)
  the mouse is destroyed and the global set to 0 (0x5D88D4..0x5D88DE), so without such a mouse the answer is false.
  The hand's immersion effects are 4 GRIP_FORCE (during the grab) and 5 UPROOT (after a pick-up), with the flag
  0x80000000.

#### A box takes the mouse (the escape menu, a dialog)

While a box is up:

- **The box claims the mouse.** The frame's interface pass (`ProcessFrameUpdates` 0x5CEDB0 = fn_005D9A20 →
  fn_005D1120 at 0x5D9B1C) calls `InterfaceActionProcessFirstGUIFeatures` 0x5D2A70. It clears [0xC4CCEE]
  (0x5D2AF1); with a box shown (`SetupBox::GetCurrentActiveBox`, 0x5D2AFA) that wants the mouse it sets [0xC4CCEE] =
  1 (0x5D2B58), turns the cursor on and returns 0x14.
- **Which box wants the mouse** (0x5D2B03..0x5D2B25). [0xCC629C] is the `DialogBoxBase` last shown: the dialog whose
  SetupBox (+4) is the active one, found in the list `DialogBoxBase::First` [0xCC6298] (next at +0xC) by
  `DialogBoxBase::UpdateLastShown` 0x513680, which clears it first. When it wraps the active box, its vt +0x18
  `WantsMouseControl` decides; without one the box takes the mouse. `DialogBoxBase::WantsMouseControl` 0x512800 is
  `IsVisible` 0x513770 (its SetupBox is the active one): every dialog takes the mouse while it is shown (SkipBox,
  `DialogBoxOptions`, `MiniDialogBoxOptions`, `DialogBoxImmersion`, `DialogBoxSaveMessage`, `DialogBoxKeyBinding`,
  StatsBox, EndGameBox, StartGameBox, LoadingBox, ...) but two. `HistoryBox::WantsMouseControl` 0x545470 and
  `GatheringBox::WantsMouseControl` 0x573BF0 take it only over one of their controls (`SetupBox::FindControl`
  0x408100 at the cursor [0xE852C0] / [0xE852C4]) or while a button pressed with the box up ([0xC4CD0C], set at
  0x5D2B3A from [0xE85304] & 0x43) is held. The GatheringBox does not take it while a script holds the wide screen,
  and it keeps the mouse once it took it ([0xD06440]). The temple's `TempleRoom::UpdateMouse` 0x79A429 makes the same
  test.
- **The action ends.** On 0x14, fn_005D1120 (0x5D116C..0x5D1183) runs EndAction fn_005D1260, sets the action state to
  1 (fn_005D2980) and clears the dword +0x38 (the grab and action button bits), then the power-up system and the hand
  state, and returns: the action's processing does not run. It does so on every frame the box is up.
- **The hand stops.** The next frame's `CHand::PrepareForDrawing` 0x46C550 reads [0xC4CCEE] after its state change
  (0x46C781) and returns (to 0x46CB56): no mouse smoothing, no state Update, no blend and no new pose or place; the
  hand stays drawn as the last frame that ran left it, and its held object is not drawn (`CHand::DrawTheHeldObject`).
  The required state, its release impulse and a state change still run first
  ([the frame order](animation-and-placement.md#the-frame-order), step 6).
- **EndAction** first calls the action state's end, the entry +0x10 of the table 0xD17DA8:
  - states 3, 4, 5, 6 (the locked selects: scooping, fish, the creature): 0x5D4870, the object's
    `GetReadyForNetworkUnfriendlyEndLockedSelect` (vt 0x6E0) and `NetworkUnfriendlyEndLockedSelect` (vt 0x6E8), the
    end packet 0x1C, then `PSysGlobal::StopMultiPickup`;
  - 8, 9: 0x5D4940 (clears +0x38, fn_005D27B0); 10, 11: 0x5D4CB0, the packet 0x1A (ApplyUnlock) with the held object
    and the turns since +0x3C; 15: 0x5D49A0 (+0x38 &= ~0x20, ResetActionState);
  - 12 (the release state): 0x5D4D60, with CHand in HOLDING (+0x4878 == 4) its +0x488C's +0x104 = 0;
  - 13 (the grab): 0x5D1840, `StopImmersion(4)` only: no tap, no pick-up;
  - 22, 23: 0x5D4650, the packet 0x24 with the time since the press (+0x454 / +0x458, capped as the grab's),
    fn_0048A570, ResetActionState, +0x38 = 0;
  - 0, 1, 2, 7, 14, 16..21: none.

  Then the objects under the hand are cleared (+0x400, +0x3C8) and `ResetActionState` 0x5D29C0 runs.
- **The buttons are dropped.** With [0xC4CCEE] set, `ProcessButtons` (0x61A298) sends no button message: the latched
  downs and ups are dropped at its end (only the double clicks set [0xC4CD10] / [0xC4CD18]). As it runs before the
  interface pass, it also drops the events of the frame the box goes away in. A release the box takes is lost; the
  binding stays pressed, so after the box the first press of that button sends no press, and its release sends the
  release.
- **Modal boxes.** `FrontEnd::JustDoABox` 0x544F90, the front end's modal box loop (the new profile and skirmish
  boxes), keeps the latched ups when it ends: [0xE85350] &= 0xBC (0x545219..0x545223) drops only the downs. The
  game's boxes are not modal: they are made active with `SetupBox::SetCurrentActiveBox` and drawn in the frame
  (`GGame::ProcessKey` 0x63EFC6, fn_0054CB50, fn_0054CB90).
- **Hand demos.** `ProcessFrameInputs` has a demo gate (0x54C390, no `CMouse::ProcessButtons` while `IsPlayBack`),
  and fn_005D9A20 takes its playback branch fn_005DAEE0 first, then fn_005D1120 at 0x5D9B1C only when no message was
  dispatched.

### The hand's packets and per-turn pass

#### The per-turn pass

`GInterface::Process` 0x5CEC10 → fn_005D2250 (the sync packets) → fn_005CEBB0 → `GInterfaceStatus::Process`
0x5DC4E0. The status's pass does the movement (fn_005DBC60), the heart beat, the locked select and `ProcessHands`
0x5DC6A0 → `GMagicHand::Process` 0x5FB190. That places the held object at the synced hand +0xC8 (0x5FB1C5) and then
calls its `ProcessInHand` (vt 0x804), so the held object's `ProcessInHand` runs at the start of the turn.

On the drawn hand's side, `CHand::GameTurnUpdate` 0x46E4E0 (0x46E505..0x46E557) does the HandStateGrain raise
(`fn_005B2D70`), `CHand::ThrowObject` when the held object is no longer available, +0x490C = GetHoldType, the
+0x48FC clean-up (+0x49B4 / +0x4950 not read), then the hold.

GInterfaceStatus +0x10C is the synced hand's velocity in units per second (fn_005DBC60), read by GLandscape::Draw's
hand wind (0x5E435F). Each turn it eases towards the turn's move times k = 1000 / ms a turn:
`v += 0.6 × (k·delta − v)`, the 0.6 at [0x92ABC8].

#### The sync packets and the ping

- **What is sent.** fn_005D2250 sends 0x15 (hand and camera, 0x5D2562), 0x16 (hand, 0x5D24E6) or 0x17 (camera,
  0x5D2462) when the hand or the camera moved; with nothing to send it returns at 0x5D245C, leaving the countdown.
  Its statics are what was sent last (0xD17D48 / 0xD17D68 / 0xD17D58 / 0xD17D78) and the countdown 0xD18230.
- **"Moved"** is fn_005D2660: a component differs by more than 0.01 ([0x8C7A10], the float difference against the
  double). The hand also counts as moved in another map cell (fn_005D26B0, CellX / CellZ, the high words).
- **The countdown.** While it is not 0 and no hand cast needs continual packets (fn_005DC810), it goes down by one and
  nothing is compared (0x5D237D). After a send it is fn_005558B0 (0x5D261D): 3 when g_game +0x59A8 == 1 (the internet
  lobby), else 1. fn_005DC810 → fn_00721480 walks the spells (g_game +0x205BC4) until one answers vt 0x514
  `Spell::NeedsContinualPackets` 0x7214C0: IsCastFromHand, not closed down (+0x40), IsHumanPlayerCasting (+0x4C) and
  the status's player.
- **The ping ring.** GetTickCount is stored at each own 0x15 / 0x16 / 0x17 send ([0xD443D4], 32 entries, index
  [0xD1822C]; written at 0x5D2474 / 0x5D24F8 / 0x5D2574, and by SendApplyToMapCoord 0x5D35DF); the old index goes into
  the packet's byte +0x40. `GPacket::ProcessPacket` (0x63CAAE..0x63CB85) stores [0xD44454] = GetTickCount −
  ring[index] for MyInterface (0x63CAD3 / 0x63CB22 / 0x63CB85), 0 until the first one; state 12 reads it (0x5D5066).

#### Sending and applying

- **The buffer.** The interface sends with `GGame::SendPacketCompressed` 0x551690 into the buffer g_game +0x5318,
  which flushes itself once it holds 6 packets (0x551E0A). The loop flushes once an iteration after the turns
  (fn_005525E0 at 0x54D291 → `LHSession::Write`, the session's list), and the next turn applies them first thing
  (`ProcessGameInputs` 0x54C3D0 → `ProcessOneSuperpacket` 0x63C3D0 → `GPacket::ProcessPacket` 0x63C420). Nothing is
  applied in the turn that sent it.
- **The packet helpers.** `GGame::SetPacket` writes g_game +0x205A60, the type byte at +1, then
  `SendPacketCompressed`: 0x550DD0 (type), 0x550CB0 (type, MapCoords → +8), fn_00551090 (type, 0x30 bytes → +8),
  fn_00550EC0 (type, object index, the gesture's packet data), 0x550E90 (type, the gesture's data), fn_00550CF0
  (type, point, 4: the spot visual 0x2B).
- **The GPacket.** The type byte is at +1; the jump table 0x63DDCC is indexed by type − 6. An uncompressed GPacket is
  0x110 bytes (fn_00551EE0); +8 is the object. `GPacket::GetObject` 0x63DF70: index and unique id must match; if the
  object no longer exists the handler receives NULL and takes its NULL path, almost always EndAction (0x63C939 /
  0x63C971 / 0x63CC4B).

#### The handlers

| Type | Handler | What |
|---|---|---|
| 0x0F | 0x5DA4D0 | a fight's select point |
| 0x10 | 0x5DA590 | a fight's apply point |
| 0x12 | ApplyToMapCoord 0x5DA400 | below |
| 0x13 | `PlaceObjectInMagicHand` 0x5DA6F0 | pick up |
| 0x15, 0x16, 0x17 | 0x5DBF40 / 0x5DBF70 / 0x5DBF90 | hand and camera, hand, camera (below) |
| 0x1A | ApplyUnlock 0x5DA8A0 | |
| 0x1B | `NetworkFriendlyStartLockedSelect` 0x5DA950 | [The locked select](#the-locked-select) |
| 0x1C | `NetworkFriendlyEndLockedSelect` 0x5DAA10 | [The locked select](#the-locked-select) |
| 0x1D | `ThrowObjectFromHand(status, 1)` 0x5DA8F0 | |
| 0x1F | give 0x5DA2D0 | |
| 0x20 | tap 0x5DA650 | |
| 0x21 | 0x5DAA70 | `InterfaceInteractAsMapCoordsObject` |
| 0x27, 0x59, 0x5F, 0x63 | | the creature's ([The CREATURE state and its packets](#the-creature-state-and-its-packets)) |
| 0x2B | | the fail spot |
| 0x40 | 0x63CDF7 | the spin of the released object |
| 0x4D | | the throw's data |
| 0x6A | 0x5DAC30 | icon power-up charge |

- **The sync handlers** are in GPacket::ProcessPacket 0x63CA86..0x63CB9C: the cases call 0x5DBF40 / 0x5DBF70 /
  0x5DBF90 at 0x63CA9E / 0x63CAF7 / 0x63CB50, and each calls its worker, fn_005DBFB0 (0x5DBF5F) / fn_005DC060
  (0x5DBF80) / fn_005DC0D0 (0x5DBFA0): both old readings were right. They also copy the ring index +0x11C, and
  +0xD4 ← +0xC8, +0xE4 ← +0xE0, +0xE0 = 0 (no reader found).
- **The 0x12 handler** 0x5DA400: fn_005D3680 (+0x448, 0x5DA431); an empty hand (status +0x90) ends the action
  (0x5DA448); the held object IsAvailable (vt 0x2C, 0x5DA474) and vt 0x724 (0x5DA48B), else nothing; then its
  ApplyThisToMapCoord at the packet's MapCoords (vt 0x728, 0x5DA4A7) and HandleApplyResult (0x5DA4B6), and then the
  throw.
- **The apply gate.** GInterface +0x444 is the turn an apply packet (0x11 / 0x12) was sent (fn_005D36A0), +0x448 the
  turn its handler ran (fn_005D3680, 0x5DA1B7 / 0x5DA431). Both go back to 0 in fn_005D2250 once +0x444 <= +0x448
  (0x5D2634..0x5D264A) and in EndAction fn_005D1260 (0x5D1303 / 0x5D1309).
- **Others.** `WorshipSpellIcon::PutFullyChargedPowerUpSeedInHand` (+0x3F) also puts an object in the hand. The
  pick-up calls fn_0052B600 at the object's old point (🔍 inferred: a map update).

#### Clearing on a new land

`GInterfaceStatus::SetToZero` 0x5DBA00 (0x5DBA12..0x5DBAB0) clears the synced and turn fields, fn_005D2250's
last-sent values and countdown, the status's throw (+0x44..+0x70) and the action's object. Its callers are
`GInterface::SetToZero` 0x5CE4D0 (from the GInterface constructor 0x5CE271 / 0x5CE286 and `GInterface::Init`
0x5CE676), the GInterfaceStatus constructor 0x5DB8AD and `GInterfaceStatus::Init` 0x5DD1A9 (from `GInterface::Init`
0x5CE687); `GInterface::Init` and its constructor are called only by `GPlayer::Init` 0x649190, from
`GGame::SetupPlayers` 0x550410, whose one caller is `GGame::Init` (0x54F4D4). So it runs **only at a new game** (a
playground's start and a saved game's load too, all inside `GGame::Init`); a land change keeps every one of these
fields. The SetToZero listing was read up to 0x5DBADB.

At a land change (`ClearMap` 0x552BB0) the hand gets only:
- `CHand::OnClearMap` 0x46E8C0 (0x552D82): +0x48FC (the last object released) and +0x48F8 (the release's countdown)
  0, and also +0x4904 (the held object), +0x4900, +0x49B4..+0x49BC (the creature to give to), +0x499C, +0x49C0 0,
  the state +0x4878 and the texture set +0x4840 1, `fn_0046E780` and the hand object's vt+0x234(0);
- `GInterface::Validate` 0x5D05F0 (0x552E20, `ClearMap`'s only call to it, after `GlobalGameLists::ClearMap` 0x552DBF
  marked every thing of the land deleted and before `ProcessDeadList` 0x552E58 frees them):
  `InterfaceActionValidateObjectsForStates` 0x5D1010 (also the first step of every interface pass, fn_005D1120
  0x5D1123), then `fn_005CEBB0`, which runs `GInterfaceStatus::Process` 0x5DC4E0 for each player's interfaces (the
  status's locked select, `ValidateHands`, the leash and the hands). The check, in its order:
  1. **The bars**: when `IsActive` (vt+0x40C = `GInterface::IsActive` 0x5CE2E0, !(+0x28 & 1), GInterface's only
     vtable) and the help system's +0x45E8 and +0x45EC are both set (a script holds the wide screen) and no hand demo
     plays (`IsPlayBack(0)` 0x5DB710), `SetActive(0)` 0x5CEDC0 (0x5D1016..0x5D1050), which ends the action itself
     (fn_005D1260 and `ResetActionState`, which puts an inactive interface in state 14, 0x5D29DD).
  2. **Inactive**: when `IsActive` is false and the state +0x44 is not 14, EndAction fn_005D1260 (0x5D1055..0x5D106B).
  3. **The action's object** +0x400 (the action collide's object, copied from +0x3C8.. by fn_005D9A20 at 0x5D9B8A):
     EndAction (0x5D10B8 / 0x5D10F1) when it is not an `Object` (`__RTDynamicCast`), not `IsInteractable` (vt+0x190,
     0x5701B0 = !(+0xA & 1)) or in a hand (+0x24 & 4); with none, EndAction when the state's row has +0x28 set
     (`fn_005D1110` 0x5D1110 reads it, `[state × 0x30 + 0xD17DD0]`).
  4. **What the hand holds**: when the status's hand is empty (+0x90 == 0), or its first object
     (`GetFirstObjectInCurrentHand` 0x5DC130) is null or not `IsInteractable`, EndAction when the state's row has
     +0x24 set (`fn_005D1100` 0x5D1100, `[state × 0x30 + 0xD17DCC]`).

  The table's two words, read by running its static initializer 0x5D0890 in the oracle (24 rows of 0x30 bytes from
  0xD17DA8; +0x00 the process, +0x10 the end, +0x24 and +0x28 these needs): +0x28 is 1 for states 7 (the wait for the
  place in hand) and 13 (the grab); +0x24 is 1 for 8, 9, 10, 11 (the seed's applies), 12 (the release), 15 (the give)
  and 20; every other state has neither. Each EndAction ends in `ResetActionState` 0x5D29C0 (state 0, 14 when
  inactive, or 16 under a CameraModeNew3 mode with an empty hand), none of which needs anything, so at most one end is
  made. At the clear every thing of the land is deleted, so steps 3 and 4 end any action that has an object or needs
  one. The ends then test the object's unavailable bit (+0xA & 1) themselves: the locked selects' end 0x5D4870 sends no
  0x1C and makes no call on the object (0x5D487E), only `StopMultiPickup`; the locked applies' 0x5D4CB0 sends no 0x1A
  (0x5D4CC2). EndAction's `CHand::ThrowObject` (0x5D12F7) needs CHand +0x4904, which `CHand::OnClearMap` has zeroed.

openblack (`HandSystem::ResetTurnState(newGame)`, 2026-10-22): at every load the last released object and its
countdown, and the object the drawn hand holds (+0x4904) without letting it go; then
`HandSystem::ValidateAtClearMap`, the check above (`ecs::hand_validate::Validate`, the table's needs in
`hand_validate::NeedsOf`), with every thing counted gone (the registry is emptied after it). openblack keeps the
action in its own fields; `HandSystem::ActionStateNumber` gives the original's number for them (a locked select 3, a
seed's apply 8..11, the fireball feed 18, the grab 13, the wait for the place in hand 7, the release 12, the give 15,
the bubble grip 17, else 0, or 14 while inactive), and the action's object is the action collide's object or the
locked select's. When it ends: the locked select's object without an end packet (the multi pickup stopped), the
creature's lock, the grab and the tug, the locked apply without an unlock, then `EndAction` (the action's objects,
the apply gate) and the button bits. When it is kept (an idle hand over nothing, the land grip, the bubble grip), the
apply gate and the rest stay. The interface status's fields only at a new game (`land_load::Resets::newGame`). The
hand entity itself is made again at every load (`HandSystem::Initialize`), which stands for the rest of
`CHand::OnClearMap`, not compared field by field. Before (from the first commits of #206), every load cleared the
locked select and the apply gate (⚠️ approximate), and before that the status too (🔍 inferred). Tests:
`HandValidate.*` (`test/test_hand_validate.cpp`). The Land Island window's "Kept by a land change" shows the synced
hand's velocity and the hand's action state.

**The clear's status pass.** `GInterface::Validate` then calls `fn_005CEBB0` (0x5D05FA), which runs
`GInterfaceStatus::Process` 0x5DC4E0 (0x5CEBE1) on each real interface of each player (`GetRealInterface`, 0x12 per
player), still before `ProcessDeadList` (0x552E58), so on things that are deleted but in memory. It is the whole
turn pass, in its order:
1. the movement fn_005DBC60 (0x5DC4E7: the hand's move +0xA4 − +0xC8, the still count +0xA0, the velocity, +0xA4
   copied to +0xC8);
2. the virtual influence's turn fn_0076CD40 on +0x130 (0x5DC4F2);
3. for `MyInterface`'s status only, `GGuidance::ProcessHeartBeatSFX` 0x71C190 on +0x30 (0x5DC50D);
4. the status's locked object +0x3C: +0x40 goes up (0x5DC51F), and when the object is not `IsAvailable` (vt+0x2C, a
   deleted one is not) +0x3C and +0x40 go to 0 and, for `MyInterface`'s, EndAction fn_005D1260 (0x5DC57F..0x5DC5A5);
   the check before has already reset the action state, so this end sends nothing;
5. +0x120 (the object last put in the hand, written by fn_005DC330) and +0x124 (the last dropped object) go to 0 when
   their object is not available (0x5DC5AA..0x5DC5D8);
6. `ValidateHands` 0x5DC610 (0x5DC5E0): for each of the status's hands (+0x90 of them, +0x74 apart by 0x1C)
   `GMagicHand::Validate` 0x5FB130, which takes every object that is not `IsInteractable` (vt+0x190) out with
   `GMagicHand::RemoveFromHand` 0x5FB0B0: +0x24 &= ~4, the object's `InterfaceSetOutMagicHand` (vt+0x704) and, with a
   fire, `FireEffect::SetOutMagicHand` 0x730AB0. Then `TidyHands` 0x5DC6F0 (the hands packed, nothing else) and, for
   the local status, `CHand::ThrowObject` 0x46DDD0 only when CHand +0x4904 is set (0x5DC671), which `CHand::OnClearMap`
   has zeroed: nothing is thrown. ValidateHands calls neither fn_005DC1E0 nor anything else that touches the gesture
   buffer, so the clear leaves the buffer as it was ([gestures](../magic/gestures.md#a-land-change));
7. `GLeashStatus::Validate` 0x5E6B90 (0x5DC5EB): a leash on a creature that is not available (+0x24) is let go and
   the immersion 7 started (force feedback);
8. `ProcessHands` 0x5DC6A0 (0x5DC5F2), with nothing left in the hand.

For a seed, vt+0x704 is `SpellSeed::InterfaceSetOutMagicHand` 0x728940: the status's last seed type (fn_005DCA20,
0x728950), the icon's `WorshipSpellIcon::CancelCharge` 0x77F9A0 when the seed has one (+0x5C), and for the local
status the in-hand effect (fn_0046E890), `PHandFX` `SetPULevel(0, 0)` (vt+0xC, 0x728999) and `StopTribalPowerRing`
(vt+0x1C, 0x7289A4). The other classes' vt+0x704 (read 2026-10-22; slot 0x704 of all 134 vtables, `xref.py
vtcalls 0x704`, whose only callers are `RemoveFromHand` 0x5FB10E and fn_005FB060 0x5FB085) do nothing more for a
deleted object, here or in a turn's `ValidateHands`: every class but three keeps `Object::InterfaceSetOutMagicHand`
0x637670, the fire's `SetOutMagicHand` alone (with a fire +0x44), which `RemoveFromHand` repeats at 0x5FB11B and
which only puts the fire's +0x28 to 0 and takes its burning-object reactions (0x21) away, so twice is as once (the
villagers and the creature are among these); `PuzzleGrain` 0x561A00 returns 1 and does nothing; `Scaffold` 0x6E9770
leaves its surplus in a new scaffold put in the hand only while `IsPlannedValid(0)` 0x6EA950 finds its planned
building +0x6C available (0x6EA962..0x6EA969), and `Scaffold::ToBeDeleted` 0x6E83E0 has already deleted that
planned building (and +0x70, 0x6E8419..0x6E8429), so a deleted scaffold leaves none. openblack's
`TakeDeletedOutOfHand` (a seed's hook, then the fire's once) is the same. At a new game this finds the hands empty: `GGame::Init` makes the players' interfaces first
(`SetupPlayers` 0x54F4D4; `GInterfaceStatus::SetToZero` 0x5DBA00 puts the hands' count +0x90 to 0 at 0x5DBA77 and
the still count +0xA0 at 0x5DBA89) and only then starts a playground (`StartPlaygroundGame` 0x54F759, whose
`GGame::LoadMap` clears the map). The campaign's first land is loaded with no map clear, so no such pass runs.

**Step 2 at the clear** (read 2026-10-22). fn_0076CD40 asks `IsInAntiInfluence` 0x5CD490 and
`IsInPlayerRawInfluence` 0x5CD460 (→ `CalculatePlayerRawInfluence` 0x5CD230) about the status's synced place +0x14,
and what they read is already gone:
- the influence rings, g_game +0x205C4C (`GlobalGameLists` +0xA8): `ToBeDeletedAvailable<InfluenceRing>` 0x592790
  (0x5918A7) calls each ring's `InfluenceRing::ToBeDeleted` 0x5CD8A0, which **unlinks** it from that list (0x5CD8AC..
  0x5CD8E1) before `GameThingWithPos::ToBeDeleted`: the list is empty, so no anti ring shields the hand and no ring
  adds to its influence;
- the towns, player +0xA50: the town list's walk (0x5915BC) calls `Town::ToBeDeleted` 0x739970 → `DeleteDependancys`
  vt+0x500 0x739690 → `RemoveTownFromPlayer` 0x73A7D0 (0x7396AA) → `GPlayer::RemoveTown` (0x73A816): every player's
  list is empty;
- the citadel, player +0xA48: `Citadel::ToBeDeleted` 0x462B90 puts it to 0 when it is the player's (0x462C5E..
  0x462C66); a citadel is a `Container`, walked with the land's other things in `GlobalGameLists::ClearMap` (which of
  its walks reaches it was not pinned), and no citadel lasts into the next land. So the citadel adds nothing and the
  chants fn_0076D2A0 (the citadel's six worship sites) are 0;
- the camera's inclusion: `ResetExclusionFile` (0x552D5F, before) put `CameraModeNew3::DrawForceField` to 0, and
  `InsideInclusion` 0x455E20 then answers true at once (0x455E35..0x455E37).

So the raw influence is 0 (not above 0: the hand is out), unless the game's `GatheringFlag` (g_game +0x14 & 0x2000,
0x5CD27A) or player +0x934 (0x5CD283) gives 1 first. Nothing else the turn reads changed: the influence power player
+0x8C (`GPlayer::OnEndOfClearMap` 0x64CD00 writes only +0x68..+0x88 and +0xB8..+0xD8), the citadel info's settings,
and the game turn g_game +0x205A40 (the last land's: `ResetState` comes after, 0x552E62). A switched-off virtual
influence (+0x44, Land 1's `SetupLand1`) returns before anything. What it writes is the virtual influence's own
fields (+0x20 / +0x48 the hand's place, and outside +0x30 the strength, which waning under 0.1 puts to 0), kept with
the interface status into the next land. At a new game the status is new (`GPlayer::Init`, above), so the turn works
on a virtual influence with its start values.

**Step 3 at the clear** (read 2026-10-22). `ProcessHeartBeatSFX` 0x71C190 (its only caller is this pass, 0x5DC50D)
is the turn's heart beat ([voices](../audio/voices.md#gguidance)): on a tenth turn of the last land's turn the value
+0xA4 is worked out again, else the stored one is kept; then fn_0071C460 eases the pitch +0xA8, moves the phase +0xB4
and the pulses +0xAC / +0xB0, and plays or re-pitches the beat (InGame 45) only with the local player's citadel
(+0xA48) and its living heart. At the clear what it reads is gone, as for step 2: the local player's towns (+0xA50,
the protection desires `GetRawDesire`, 0x71C1C0..0x71C1FF) and so its believers
(`GetProportionOfWorldPopulationWhoBelieveInMe` 0x64B680 sums over +0xA50 and gives 0 for none, 0x64B6BE); every
player's towns, so no other player's creature finds a nearest town (`GetNearestTown` 0x601F90 walks each player's
+0xA50); and the citadel (+0xA48 = 0, 0x71C566..0x71C56E), so **no sample is played or re-pitched**. Only the belief
share (`GetProportionOfAllPlayersInfluence` 0x64B700, the players' power +0x8C) is as it was. Nothing is stopped:
`StopHeartBeat` fn_0071C650 has no caller, and the clear's `GAudio::Reset` (0x552D98, 0x552DB4) came before. The
guidance is kept with the interface status into the next land: `GGuidance::Init` runs only at a new game and a
saved game's load ([voices](../audio/voices.md#the-guidance-across-a-land-change)).

openblack (`HandSystem::ProcessStatusAtClearMap`, 2026-10-22): `Game::PrepareNewLand` calls it right after
`ResetTurnState` when the load clears the map (`land_load::Resets::mapClear`: every load but the campaign's first
land; not the testbed, a game of its own), before the registry is emptied: `UpdateTurnMovement` once more, then the
virtual influence's turn (`InfluenceSystem::ProcessVirtualInfluenceAtClearMap`, 2026-10-22: on the state the player
service keeps off the last land's entity, `PlayerSystem::VirtualInfluenceAtClearMap`, made afresh at a new game; the
turn's inputs `virtual_influence::AtClearMap`: unshielded, out of influence unless `IsInfluenceEverywhere`, no chants,
the influence power and the turn as they are; at a new game the original's are the last game's turn and the
player's power as its new game left it, which openblack's were not compared with), the guidance's heart beat
(`audio::guidance::UpdateHeartBeatAtClearMap`, 2026-10-22: `UpdateHeartBeat` with
`HeartBeatInputAtClearMap`, no towns' desire, no believers, no creatures near a town, no citadel heart, the belief
share as it is), the status's locked object
(`_pickSource`) let go with `EndAction`, then a held object taken out of the hand as the turn's `ValidateHands` does
(`TakeDeletedOutOfHand`: a seed's `SeedLeftHand`, which sets the last seed type, cancels its icon's charge, releases
the in-hand effect, puts the bracelets to level 0 and stops the tribe's ring; the fire's `SetOutMagicHand`). A new
game first empties the hands in `ResetTurnState`, with no out-of-hand call. Before, the held object stayed in the
hand through the load and was dropped on the next turn with its entity gone, so none of those calls was made, and
the movement did not step at the clear. Test: `LandLoadScripts.EveryLoadButTheCampaignsFirstLandClearsTheMap`. The
Land Island window's "Kept by a land change" shows the hand's still turns (and the last seed type) and the hand's
virtual influence (its strength, its last turn in and whether it is off). Before the virtual influence's step, the
clear left it as the last land's turn had, and a new game went on with the last game's. Tests:
`VirtualInfluence.AtTheMapsClearTheHandIsOutOfInfluenceUnlessItIsEverywhere`,
`PlayerSystemLands.TheMapsClearReachesTheKeptVirtualInfluenceAndANewGameMakesItAfresh`,
`GuidanceTest.TheMapsClearsHeartBeatFindsNoTownAndNoCitadel`; the window also shows the heart beat's value, pitch
and phase. What the heart beat's step writes goes on into the new land, since only a new game starts the guidance
again (`guidance::StartLand`, 2026-10-22; before, openblack's start of every land set all of it again but the value
+0xA4). The other classes' vt+0x704 need nothing more
(above; 2026-10-22: this page said they were not ported). Not compared: +0x120 has no field in
openblack, +0x124 is the last released object the hand's clear has already forgotten, and the leash's creature goes
with the hand entity, which every load makes again.

### The locked select

A locked select holds an object to the hand from the press to the release. Piles, fields, fish farms and the
creature use it. Its parts are the same for all of them; what each class does is in
[The locked select on a creature](#the-locked-select-on-a-creature) and [The scoop](#the-scoop).

#### The start

- **`StartTapOrLockedSelect` 0x5D1A00** refuses with the grab bit (+0x38 & 8) or no object, and stores the press's
  engine sample (+0x454) and turn (+0x458). It enters action state 19 (the 225 ms tap-or-lock wait, 0x5D53D0) when the
  object is `InterfaceValidToTap`, the leash is on and not tied, or the leash is tied to this object. Otherwise the
  locked select starts at once (0x5D1950).
- **`GInterface::StartLockedSelect` 0x5D1950** first latches the hover collide (fn_005D6540, +0x30: the locked byte 8
  and the lock's turn). Then it calls the object's `NetworkUnfriendlyStartLockedSelect` (vt 0x6D4; Object 0x4027D0
  does nothing), sends packet 0x1B (0x5D1985) and enters action state 3 (fn_005D2980(3)) with the object as the
  action's (+0x400). For a creature of another player it also flips the interface's word +0x37C[player number]
  (0x5D19C4..0x5D19E5, not named).
- **The 0x1B handler** (fn_005DA950, 0x5DA950..0x5DAA08; the reconstruction's `GInterface::Packet_StartLockedSelect`)
  ends the action (EndAction fn_005D1260, 0x5DA9FC) only when the object is null (0x5DA956), not `IsInteractable`
  (vt 0x190, 0x5DA965) or already in a locked select (Flags +0x24 & 0x10, 0x5DA97D). Past those checks it calls
  `NetworkFriendlyStartLockedSelect` (vt 0x6D0, 0x5DA98A) and **ignores its result**. It sets the status's locked
  object (0x5DC110: status +0x3C = object, +0x40 = 0) and Flags |= 0x10 (0x5DA99C), does the creature's extra work
  (below) and returns 1.
- **`NetworkFriendlyStartLockedSelect`** is Field 0x529900, FishFarm 0x52D770, PileResource 0x66E710 (the first
  amount into a hand pot put in the hand through 0x5DC870, and `PSysGlobal::StartMultiPickup` 0x68F8C0). The hand pot
  is made at the status's MapCoords +0x14 (Pile 0x66E79F, Field 0x52998B, FishFarm 0x52D80F).

#### Each turn

`GInterfaceStatus::Process` 0x5DC4E0 runs the locked select (+0x3C): the counter +0x40 goes up (the first call sees
1). The select lasts while the object IsAvailable (vt 0x2C, 0x5DC524); for an object that is not a creature
(`IsCreature` vt 0x34, 0x5DC531: a totem, a pile, a field, a fish farm) while the status's player (vt 0x1C) has
influence > 0 at MapCoords(+0xC8) (`CalculatePlayerInfluence(coords, player, 0, 0, 1)`, 0x5DC540 → 0x5DC558); and
while its `ProcessInInteract` (vt 0x808: Pile 0x66E520, Field 0x529730, FishFarm 0x52D950; 1 for every other class,
Object 0x639B10) returns 1. Otherwise +0x3C = +0x40 = 0 and, for the local interface only (0x5DC590), EndAction
(0x5DC5A5): the exit of states 3..6 (below) and the action state reset. So a creature or a totem that is gone ends the
action with nothing sent (the exit sends nothing for a gone object), and a totem held where the hand's
synced point has no influence is let go: the exit sends the totem's end (its share, packet 0x29) and 0x1C, whose
handler clears the grip.

The creature's lock bit stays set after a creature gone while locked. It is the interface status's +0x24 & 0x20 (the
status passed to the creature, not the creature's own flags): `Creature::NetworkFriendlyStartLockedSelect` sets it
(0x476E79) and `Creature::NetworkFriendlyEndLockedSelect` clears it (0x476EA0), reached only from the 0x1C handler; the
only other writer is `GInterface::ResolveLoadForCreature` (0x5D057F, from `GameOSFile::ResolveAllLoads` 0x561D1D, a
saved game's load). `GInterfaceStatus::SetToZero` 0x5DBA00 does not touch +0x24, and the interface is made once, by
`GPlayer::Init` from `GGame::SetupPlayers` (`GGame::Init`, `GGame::StartGame` from `pc_main`), so a new land keeps it.
A scan of every `[reg+0x24]` access testing or changing bit 0x20 finds two readers of the status's bit:
`GInterface::ProcessPowerUpSystem` (0x5CF328, its early out: no gesture is read while it is set) and
`GInterfaceStatus::IsHandReadyForObject` (0x5DC89C: no object goes into the hand; its callers are the power-up pass
0x5CF7EE / 0x5CF900, fn_005CF1C0, fn_005D03D0, fn_005D0410, fn_00682EA0, `OneOffSpellSeed::CreateSpellIntoHand`
0x72A73A and the worship icon's tap and fully charged seed 0x77F8B1 / 0x77F8FA / 0x77F976). The other hits of the scan
(0x48C8DE, 0x4FFD12, 0x5D15BB, 0x5D176F, 0x5DA75C) are on an object's own flags (bit 0x20: not pickable now). So
once a locked creature is gone, the player draws no gesture and the hand takes no seed until a creature's locked
select ends again.

`GInterfaceStatus::IsHandReadyForObject` 0x5DC890 asks only two things, in this order: `IsSpaceInHands` 0x5DC2F0 (the
status's hand count, byte +0x90, is 0, or the current magic hand's `HasObjectInHand`, its +0x14, is not 1), then the
status's +0x24 & 0x20 clear (0x5DC89C). Nothing else: a tree being tugged is held by the render hand only (`CHand::PickUp`
with the tug), and enters the status's magic hand only with the generic pick-up's place-in-hand packet once the tug lets
it go, so the hand is ready for an object during a tug; a pile, a field or a fish farm's locked select puts its hand pot
in the status's magic hand at its start (`GInterfaceStatus::PlaceObjectInMagicHand` 0x5DC870 from 0x66E7DE, 0x5299A3,
0x52D840), so the scoop already has no space. An object offered during a tug is refused later, where it is placed:
`GInterface::PlaceObjectInMagicHand` 0x5DA6F0 asks `IsSpaceInHands` again (0x5DA719).

#### The action states 3 to 6

- **State 3** (0x5D4960): state 4 once the object has Flags & 0x10.
- **State 4** (0x5D4830; the reconstruction's `GInterface::State_LockedSelectHeld`) waits for the action's release
  (+0x39 & 0x40, the latched release bit). Then +0x38 = 0x10, the object's
  `GetReadyForNetworkUnfriendlyEndLockedSelect`, then state 5's process at once.
- **State 5** (0x5D49B0; `GInterface::State_LockedSelectWaitUnlock`): when the object still has Flags & 0x10 and
  `IsReadyForNetworkUnfriendlyEndLockedSelect` is 1, it sends the end through 0x5D2210 (the object's
  `NetworkUnfriendlyEndLockedSelect`, packet 0x1C), then state 6; else it stays in 5.
- **State 6** (0x5D4A10): once Flags & 0x10 is clear (the 0x1C handler ran) the action resets.
- **The exit of states 3..6** (0x5D4870, run by `EndAction`): unless the action object (+0x400) is gone (its byte
  +0xA & 1 at 0x5D487E, UNAVAILABLE: the bit `GameThing::IsAvailable` 0x401810 reads), the object's
  `GetReadyForNetworkUnfriendlyEndLockedSelect` / `NetworkUnfriendlyEndLockedSelect` (vt 0x6E0 / 0x6E8; nothing for
  piles, fields and fish farms) and packet 0x1C (0x5D48A8), then `PSysGlobal::StopMultiPickup` at once (0x5D48B4).
- **The 0x1C handler** 0x5DAA10 calls the object's `NetworkFriendlyEndLockedSelect` (vt 0x6EC; PileResource 0x66E850:
  StopMultiPickup and the amount tooltip), clears the status's locked object (0x5DC110(0)) and Flags &= ~0x10. A NULL
  object only ends the action (0x5DAA2A).

#### What the scripts read of it

- **`IS_LOCKED_INTERACTION` 0x70AFE0** pops the thing (`GetScriptGameThing` 0x70D220) and pushes its Flags +0x24 bit
  0x10 as a boolean (`(byte +0x24 >> 4) & 1`, 0x70B01C..0x70B026): the flag the 0x1B handler sets for whatever object
  the select is on (0x5DA99C) and the 0x1C handler clears. No thing: its error message (0xC0D428) and false
  (0x70AFFE..0x70B015). So a creature, a village totem, a field, a fish farm or a pile is "locked" from the start
  packet until the end packet has run, the button held and the let-go included.

#### The hand states of the locked select

The interface's hand states are in the table 0xD18278, filled by 0x5D7960: 32 bytes a row, +0x00 a function, +0x10
a constant, +0x18 the name. The rows of the locked select:

| Action state | Name | Hand state |
|---|---|---|
| 3 | "WAIT LOCKED SELECT" | 18 (0xD182E8) |
| 4 | "IN LOCKED SELECT" | fn 0x5D80C0: no object 3, a creature (info +0x10 == 1) **19**, or 14 when the creature's +0x10AC is not 0, any other object 14 |
| 5 | "WAITING FOR LOCK OFF" | 18 (0xD18328) |
| 6 | "WAITING FOR LOCK OFF COMPLETION" | 18 (0xD18348) |

### The hand on a creature

#### The action press

`GInterface` 0x5D1330, the reconstruction's `ActionPressed`. In this order:

1. Something in the hand (status +0x90): the holding press (0x5D1560).
2. A bubble in the action collide (+0x408): action state 17 (0x5D4330).
3. The leash in the action collide (+0x410): the leash pull, action state 21 (0x5D4440).
4. The object: the action collide's (+0x400), or else `FindObjectNearMapCoord` (0x5D39E0) at the action point. When
   `ValidForLockedSelectProcess` (vt 0x6CC) is 1, `IsCannotBePickedUp` (vt 0x180) is 0 and Flags & 0x10 is 0:
   - not a creature: in the influence (+0x48) or `InterfaceMustBeInInfluenceForInteraction` (vt 0x714) false →
     `StartTapOrLockedSelect` (0x5D1A00);
   - a creature (vt 0x34): only when its player is mine, or its player is set and `IsAllied` (0x64D5D0) with mine,
     **in or out of the influence** (0x5D13FB..0x5D1481) → `StartTapOrLockedSelect`. A creature of no player or of an
     enemy falls through to step 5.
5. The pick-up / tap path: `IsSpaceInHands`; then `ValidForPlaceInHand` (Creature 0x473F60 = 0),
   `InterfaceValidToTap` (Object 0x4196B0 = 0 for a creature), or the leash on and not tied → `StartGrab` (0x5D1740);
   else `ClickOnLand` (0x5D3D10).

So a creature, being the collided object, keeps out the fish farm (the water branch of `FindObjectNearMapCoord`).
One the hand may not hold (an enemy's, nobody's, or one `ValidForLockedSelectProcess` refuses) goes on to step 5
with the creature still the object (0x5D148F..0x5D14AB: `FindObjectNearMapCoord` only with no object), where it is
neither placeable nor tappable.

`Creature::ValidForLockedSelectProcess` 0x476E10 asks for: not an Ogre (info +0x1F4 != 13), Flags bit 0x400 (byte
+0x25 & 4) clear, not (mind +0xF60 == 9 and mind +0xFB4 <= 2), a player (vt 0x1C), and
`CreatureReceiveSpell::IsCreatureSpellActive(0)` (+0x370, 0x4F51E0) false. So the hand holds only **its own or an
allied player's** creature.

#### The locked select on a creature

- **The press.** A creature is never valid to tap, so without a leash in the hand the locked select starts at once.
  `StartLockedSelect` calls `NetworkUnfriendlyStartLockedSelect` (vt 0x6D4), sends packet 0x1B and enters action
  state 3.
- **The start applies whatever the hand can do.** For a creature (`__RTDynamicCast` to Creature, 0x5DA9AF) the 0x1B
  handler also copies the 3D creature's pose (+0x5178 to +0x5180, +0x47B8 × 48 bytes, 0x5DA9C4..0x5DA9E8) and calls
  `Creature::DestroySpell` (0x4F81F0, 0x5DA9EC). The creature's own start
  (`Creature::NetworkFriendlyStartLockedSelect` 0x476E70) has no condition either: `GoIntoStateOfFinishingAction` (0x477B20), status +0x24 |= 0x20, return 1. So
  once the turn applies the start, the creature is locked whatever the hand can do with it, and the select runs on to
  the release. Action states 3..6 (0x5D4960, 0x5D4830, 0x5D49B0, 0x5D4A10) never call `ValidForLockedSelectProcess`,
  whose vt 0x6CC is called only at 0x4E3E8E (the totem statue), 0x5D13BB (the press) and 0x5D7FC8 (the hover).
- **The release.** In state 4 the creature's `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476ED0) zeroes the 3D
  creature's +0x48C0, then calls fn_004806D0. In state 5, `IsReadyForNetworkUnfriendlyEndLockedSelect` (0x476F00) is 1
  when its look reached `ReachedLookDestination`, no body action runs unless `IsActuallyDead`, and no facial action
  runs; then `NetworkUnfriendlyEndLockedSelect` (0x476F60) and packet 0x1C.
- **The end.** The 0x1C handler calls `Creature::NetworkFriendlyEndLockedSelect` 0x476E90: `LH3DCreature::ReconnectToGame`
  (0x480730), status +0x24 &= ~0x20.
- **The hover.** A creature merely under the hand goes through fn_005D7F20: not a pile or field, not valid to place
  in the hand → 18, in and out of the influence.

#### The CREATURE state and its packets

- **The frame.** `CHand::PrepareForDrawing` 0x46C550 adds the camera time step (`GetCameraTimeInc`, g_delta_time
  inside the citadel) to CHand +0x4900 every frame, then `GetRequiredState`, then fn_0046CF80(required), then the
  state change.
- **Asking for the state.** `GetRequiredState`, on the empty-hand path, calls
  `OperateInteractionCamera(GInterface +0x3AC)`, then `case 19` → CREATURE 7 when CHand +0x49B8 is set, else NORMAL 1.
- **The interaction camera.** `OperateInteractionCamera(mode)`, mode 19: +0x49B4 = the hover collide's object
  (+0x3C8) as a creature; when it is one, +0x49B8 = its 3D creature (+0x160 → +0x58). If +0x49C0 is 0: a camera mode
  object for the creature (fn_0044A850), +0x49C0 = 1 and **+0x4900 = 0**. Any other mode with +0x49C0 set:
  `GCamera::PopViewMode` (0x441C50), +0x49C0 = 0, then **if +0x4900 < 450 and the creature's player is the local
  player: packet 0x5F** (0x46CCBD). The hand is hidden meanwhile (+0xAC set from 0x46CBDC / 0x46CBEF).
- **Which camera gives way, and what comes back.** The game camera keeps a stack of up to 12 modes (GCamera +0x28,
  the current one's index at +0x58). The interaction's mode is refused while the current mode's `CanExit` (vt 0x1C)
  answers 0 (`CantExitCurrentMode` 0x441B70); read in the v1.42 vtables, every mode the game makes can be left
  (`CameraMode::CanExit` 0x44A2E0, `mov eax, 1`) but the interaction's own (0x44A920, 0) and the script mode
  (0x461B70: 1 only once its +0x48 is 0, so not while it lives): the player's `CameraModeNew3` (vtable 0x8C7BFC),
  the follow `CameraModeFollow` (0x8C7884) that Creature Mode is (the creature key, `GGame::DoAction` 0x63FB4B, and the
  double click, 0x45E34B), a miracle's placed path `CameraModePath` (0x8C7D10) and the script's dual camera
  `CameraModeTwoObjects` (0x8C7DD0), with the dance's (0x44ABE0, 1) and the modes only a saved game makes again
  (`CameraModeFree`, `CameraModeFollowHeading`, from `GCamera::Load` through fn_00441650). The citadel's and the fly
  and click modes are never made (no caller of their constructors); the temple's rooms have cameras of their own
  (`InnerCamera`), not on this stack. `SwitchToViewMode` 0x441CD0 calls the current mode's `Cleanup` (vt 0x18), pushes
  the new one (the bottom one deleted when the stack is full) and zeroes the camera's time in the mode (+0x68).
  `PopViewMode` 0x441C50 calls the current mode's `Cleanup`, deletes it and, with a mode under it, calls that one's
  `Restart` (vt 0x10) and zeroes +0x68 (with none, a new `CameraModeNew3`). So the mode the interaction took over from
  comes back as it was left, but for its `Restart` and its time: the player's `CameraModeNew3::Restart` 0x4587D0
  (`Reinitialise(1)` through vt 0x4C, then a flight back to its own place in 0.1, `0x3DCCCCCD`); the follow has the base
  `Restart` and `Cleanup` (0x44A390 and 0x44A2D0, a bare `ret`), so it goes on following its creature from where the
  camera is, at the pace its time gives (`CameraModeFollow::Update` 0x44C1A5..0x44C1DA: the camera's +0x68 against
  `CameraFollowBlendDuration`, from `CameraFollowStartZoomTime` to `CameraFollowEndZoomTime`), starting again.
- **The end of a hold that did not get the view.** `OperateInteractionCamera` sets +0x49C0 = 1 after the
  `CameraModeCtrInteract` constructor whether the mode was pushed or deleted itself (0x46CC2F..0x46CC30: the
  constructor's refusals at 0x44A867 / 0x44A875 / 0x44A8B9 delete the new mode and return, and nothing records it), and
  the end pops on +0x49C0 alone (0x46CC49..0x46CC55). So a hold refused by a living script camera (`CameraModeScript`'s
  `CanExit` 0x461B70 is 0 while +0x48 is set) or by an unavailable creature (byte +0xA bit 0) ends by popping the mode
  that kept the view: `PopViewMode` 0x441C50 calls its `Cleanup` (vt 0x18), deletes it (vt 0) and, with a mode under
  it, calls that one's `Restart` (vt 0x10) and zeroes +0x68; with none, it builds a `CameraModeNew3` (0x441C8F..0x441CB9),
  whose constructor 0x4572E0 runs `Initialise` and `Reinitialise(0)`, which drops the fight (+0x44 and +0x48 zeroed,
  0x458A5E..0x458A7F) and starts from the camera's zoomers, with no flight. Per mode that kept it: the script mode's
  `Cleanup` and `Restart` are the base's bare `ret` (vtable 0x8C7D5C: 0x44A2D0, 0x44A390), its destructor 0x461240
  frees its path (`ReleasePath` 0x461A60), and the script is not told (its camera owner, g_game +0x250090 +0xA8,
  stays, so its next camera opcodes find the wrong mode and its end of control warns, 0x6ECE24); a dual camera's are
  the base's too; a placed path's `Cleanup` 0x460F60 puts the near plane back, shows the hand and sets the light maps'
  state; Creature Mode's follow has the base's. The mode under then restarts: the player's `CameraModeNew3::Restart`
  0x4587D0 (`Reinitialise(1)`, which keeps a valid fight, and a flight back to its own place in 0.1 s), a path's
  `SetUpNearClipping`, nothing for the others. Which mode is under the player's own mode depends on the stack's
  history: `fn_006ECD70`, the end of a script's camera control, marks the script mode deleted (vt 0x30, 0x6ECDEF) and
  pushes a new `CameraModeNew3` over it (0x6ECE1D), so after any script camera the player's mode lies over the earlier
  one, which a pop restarts.
- **The creature key during the interaction.** C is `GGame::DoAction` slot 18 (the byte table 0x63FF58 sends 18 to
  0x63F9C7). With the player's creature (player +0xA64) it lets go only of a current `CameraModeFollow` on that
  creature (`__RTDynamicCast` of the current mode at 0x63FA17, its thing +8 compared at 0x63FA27): `PopViewMode`
  (0x63FA3D), then a new `CameraModeNew3` if the mode now current is not one (0x63FA6B..0x63FAAA). Any other current
  mode makes a new `CameraModeFollow` on the creature (0x63FB4B), whose constructor deletes itself when
  `CantExitCurrentMode` (0x44B816..0x44B829). `CameraModeCtrInteract` derives from `CameraMode` only (its constructor
  0x44A850 calls no follow's), so while the interaction is current the cast fails, the new follow refuses itself, and
  the key does nothing: a Creature Mode follow under the interaction is neither popped nor marked. (Not ported, and no
  part of this: the script mode is a `CameraModeFollow` (its constructor 0x461180 calls the follow's, 0x461193), so C
  pops a script camera whose followed thing, +8, is the player's creature, when no script's wide screen holds the keys,
  0x63F95B..0x63F965.)
- **Entering and leaving.** fn_0046CF80(required) enters 7 only when the creature (+0x49B4) has Flags & 0x10 and its
  +0x3CC is 0; otherwise the current state is kept. That is the start-ready test
  `IsReadyForNetworkUnfriendlyLockedSelect` 0x476EB0: set to 1 by `GoIntoStateOfFinishingAction` 0x477C31, cleared by
  fn_00477D00 from `ProcessState` 0x472FAB once no body action runs, which also freezes the pose (fn_004806E0 +0x5270
  = 1, fn_004806C0 +0x526C = 1). In `ProcessState`, while the creature is locked (byte +0x24 & 0x10, 0x472F77) the
  rest of its state is skipped (0x472FB0 → 0x4737DB) and +0x3CC chooses: 1 → fn_00477D00 (0x472FAB), 2 → fn_00477D70.
  fn_00477D00 returns 0 while `IsPerformingBodyAction` (0x4842D0: the 3D body's current action +0x4994 is set);
  otherwise +0x3CC = 0, `DisconnectFromGame`, `StartHandInteraction` (+0x526C = 1, `InHandInteraction` in
  bw1-decomp), and a creature help candidate 0x29 with fn_00477C50's value unless it is 0x28 (0x477D45..0x477D59).
  `StopHandInteraction` (0x4806D0) is called only from `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476EF1), at the
  release. So the hand enters 7, and its update acts, only from the first creature turn after the lock with no body
  action, and stays so until the release, whatever the strokes and slaps start. Leaving 7 with +0x49B4 set: if `IsReadyForNetworkUnfriendlyEndLockedSelect` is not
  1, **packet 0x59** with the creature's index and clamp(state +0x4898 → +0xD8, −1, 1), then +0x49B4 = +0x49B8 = 0;
  when it is 1 nothing is sent and the fields stay (0x46CFF4). So in one frame 0x5F is pushed before 0x59.
- **`HandStateCreature`** (Enter 0x5B13F0, Update 0x5B17B0, Exit 0x5B2A80). Enter sets the creature's mirror bones
  and three zoomers. Update (0x12D0 bytes, read whole, below) has `IsImmersionPlaying` / `StartImmersion` /
  `StopImmersion`; `LH3DCreature::ForceIndividualAction` at 0x5B1E51 and 0x5B2218 (a slap and a stroke), each
  followed by packet 0x27 (fn_00550F80); `InitialiseDestructionBones` / `StartDestruction`; packet 0x63 (fn_00551040,
  at 0x5B1DD0); zoomers 0.3 / 0.9 / 2.5; the clip `Morphable::GetAnim(state +0x114)` (0x5B29DE). Exit:
  `StopImmersion`.
- **The packets** (`GPacket::ProcessPacket` 0x63C420, jump table 0x63DDCC):
  - 0x27 0x63CD27: nothing when the sender's interface is the local one (0x63CD32); otherwise
    `ForceIndividualAction` on the creature's body. So the strokes and slaps act at frame time on the local machine.
  - 0x59 0x63D352: the creature from the packet's index (null when gone); its mind's "player feedback" fn_004E06A0
    with the float at packet +0x10 (`FinishActionUnsuccessfully("player feedback")`, then a plan when |v| > 0.01).
  - 0x5F 0x63D3B3: the sender interface's byte +0x370 = 0 (the leash picker's pending flag,
    [What is looked for and when](../magic/gestures.md#what-is-looked-for-and-when)),
    then fn_005D06E0: the TOGGLE_LEASH path ([TOGGLE_LEASH](../creature/leash.md#per-native)).
  - 0x63 0x63C98D: the player's creature loses 0.01 life, never below 0.01, then fn_004866F0 on its body.
- **The leash's tooltip** and its help events 40 / 41 / 47 come from `CalculateLeashToolTip` 0x5D67F0.

##### The CREATURE state's update

`HandStateCreature::Update` 0x5B17B0, read whole 📼 1.20 only:

- **When it acts.** It returns at once unless the 3D body's +0x526C is set (fn_004806C0, the freeze in
  `ProcessState`, 0x5B17EA..0x5B17F4), so strokes and slaps start once the creature has finished its body action.
  The height is H = 3D +0x90 × 15 inline (0x5B17C4), the drawn size.
- **The slap's gate.** A slap is tested only when the body ray hits this frame and missed the last (+0x110), or the
  cursor crossed the screen's middle column, (cur − W/2)·(prev − W/2) < 0 (0x5B1BF8..0x5B1C0F): it can fire with no
  ray hit. The share is (CHand +0x7C − `GetAltitude`(CHand +0x78 / +0x80)) / H, in (0, 1.1), the hand as placed last
  frame (it is placed again at 0x5B23C4, after the tests). +0xDC must be above 1000 ms; it is zeroed only by Enter
  (0x5B1455) and by a stroke (0x5B226C), never by a slap.
- **The slap's speed and kind.** The speed is the horizontal |Δ| of the two cursor points projected at the hand's
  depth (+0x485C now, +0x4864 last frame, `PrepareForDrawing` 0x46C6A7) over dt, not eased (0x5B1B53..0x5B1BAA), and
  must be above 5H. The bands 0.4 / 0.7 give 169 / 167 / 165, + 6 for a gentle one below 9H. It is mirrored when the
  cursor moves left (0x5B1E20 `sets`), into `ForceIndividualAction`'s second argument, ReverseAnim +0x522C. It is
  skipped whole while the body action is no more than 0.35 through. A hard slap adds one to the count (+0x11C, set
  to 0 when the band changes, 0x5B1D05..0x5B1D13), whether it hits or not (0x5B1D39..0x5B1D4E); with a hit and the
  count above 2 the count goes back to 0 (0x5B1D61) and packet 0x63 is sent (to 0x5B1DD0). So every third hard slap
  in a row at one band bruises, a miss counting towards it, and a hit past three after misses bruises at once. The
  sum rules: 0.1 / 0.2, doubled above 0.25, at least −1;
  a stroke + 0.1 to 1.
- **The stroke.** It needs the ms since the last stroke or slap (+0xCC) at least ftol((1 − sum) · 0.5 · 2500) and
  +0xD0 at least 2000, +0xD0 being set to 1000 after a stroke or a slap; a part other than the last; and the face
  (0x480C60) is pulled before the 0.8 test (0x5B2154..0x5B221D). Before any of that, a stroke is looked for only after
  the hand has rested on the body more than a second with the mouse moving across (✅ v1.42, read 2026-10-21): on a
  body hit (0x5B1F62) the state's +0xD4 gains the frame's ms (GGame +0x205D48, 0x5B205C..0x5B2077), and only when it
  is then above 1000 (`jle` at 0x5B207D) and CHand +0x4858 (the mouse moved in x this frame) is set (0x5B2086) does it
  find the nearest part (0x5B209E..0x5B2116), set the clip to Ctickle (0x5B2122..0x5B214F) and weigh the stroke.
  +0xD4 is zeroed by Enter (0x5B144F) and by a slap weighed past the share, speed and clock tests (0x5B1F1F, also
  when the body's action is too early to interrupt), never by a stroke or by leaving the body; a miss only forgets
  the last part (+0x108 = 9, 0x5B227B).
- **The clip** (`Morphable::GetAnim(+0x114)`, 0x5B29DE): 0 Cwiggle off the body, 0x1B (27, Ctickle) on it (Enter
  0x5B163F, 0x5B2122..0x5B214F), 0x21 (33, Cslap) above 5H (0x5B1BDB..0x5B1BF3), each change through
  `StartStateBlend` 0x46C500; the frame += [0xC38134] modulo the length (0x5B29E3..0x5B29F9). The counts follow
  `hndspec5.txt`'s order of names (0 Cwiggle, 0xC Ccan_pickup, 0x18 Cstroke). In detail (✅ v1.42, read 2026-10-22):
  Enter sets Ctickle and zeroes the clip's clock +8 (0x5B145B), so the state starts caressing. Cslap is set whenever
  the speed is above 5H and the clip is not Cslap already, with a blend start (0x5B1BDB..0x5B1BF3). Ctickle is set
  only on the stroke's path, past the second on the body with the mouse moving in x (above), once the nearest part is
  found, whichever part, when it is not Ctickle already, with a blend start (0x5B2118..0x5B214F). Off the body (the
  hit flag stored at +0x110, 0x5B23D5, is 0) a Ctickle clip becomes Cwiggle **without** a blend start
  (0x5B23E0..0x5B23ED); a Cslap clip stays off the body until the stroke's path sets Ctickle. So once the hand has left
  the body, the caress comes back only past the second on it in all (+0xD4, kept off the body) with the mouse moving.
  The clock +8 is the state's own: g_delta_time is added and the sum wrapped by the length of the clip in +0x114 at
  that frame, and a change of clip does not reset it (only Enter does).
- **The palm's zoomers.** +0xC / +0x3C / +0x6C ease towards minus the vector the 3D ray hit returns (fn_00867400;
  (0, −1, 0) with none; read as the surface normal) in 0.9 s, back to (0, 1, 0) in 2.5 s off the body, and the
  distance +0x9C in 0.3 s; the hand's matrix is built with CHand +0x84 + π (0x5B264F..0x5B29AF).
- **The end-ready test** `IsReadyForNetworkUnfriendlyEndLockedSelect` 0x476F00 = `ReachedLookDestination` 0x483750
  and (no body action, or 0x484E10) and no facial action (0x480D30).
- **The creature's side: its desire shown** (read 2026-10-18). `Creature::ProcessState` 0x472F77: in the hand
  (Creature +0x24 & 0x10) it sets mind +0x1C28 = 1 and, by Creature +0x3CC, calls fn_00477D00 (1) or fn_00477D70 (2),
  then skips the rest of its think. fn_00477D00: while `LH3DCreature::IsPerformingBodyAction` 0x4842D0 it waits
  (returns 0), and nothing else: not for a turn; then fn_00477C50 picks the desire, Creature +0x3CC = 0,
  `DisconnectFromGame`(emote) 0x4806E0 and `StartHandInteraction` 0x4806C0, and unless the desire is 0x28
  `SendCandidateHelpScript`(0x29 CURRENT_DESIRE, desire, 0, 0, 1). fn_00477C50: `GetBestContextToLearnFrom` 0x4E0410;
  its priority above 0 gives the context's desire, else the plan's desire (mind +0xF50, 0x28 when there is no plan); a
  desire of 0x28 or more (0x28 = 40, the desire count: none) or one whose `CreatureDesireTableEntry` +0x18 is 0 gives
  0x28 and no emote; otherwise mind +0x1C2C = 1 and the emote is the entry's +0x1C. openblack
  (`CreatureMindSystem::ShowDesireOnHold`, raffclar PR #354): the pick is the same (`creature_learning::BestContext`,
  strictly the highest priority above 0, else the active plan's desire; `creature_desires::EmoteFor` stands for the
  table's +0x18 / +0x1C); it waited for the turn to the player to end as well, which the original does not (fixed,
  ours from the exe). Not ported: mind +0x1C2C (its readers not read), `DisconnectFromGame`, state 2 (fn_00477D70).
  What the hold starts on the creature's side before this (its turn on the spot to the player's eye, and the dance,
  the pointing and its own side of a fight it ends first) is ported and pinned on
  [the creature's mind](../creature/mind.md) ("Held by the hand", 2026-10-21).

### The scoop

The incremental pick-up from a pile is the locked select with `ProcessInInteract`, not hand state 6 (the help text
DYK_44 matches it). Fields and fish farms work the same way. The same functions, with the same sizes, are in the 1.00
build (`PileResource::ProcessInInteract` 0x618180 of size 0x1F0 there, `PSysGlobal::StopMultiPickup` 0x637C30): the
scoop is the game's from its first release.

#### The scoop, turn by turn

1. **Press** with an empty hand over a pile: `ActionPressed` checks `ValidForLockedSelectProcess` (a pile is valid
   unless it is itself a hand pot) and the influence, then `StartTapOrLockedSelect`. Piles are not tappable, so
   `StartLockedSelect` 0x5D1950 runs at once (no 225 ms wait): packet 0x1B, state 3.
2. **Packet 0x1B**, on every machine: `PileResource::NetworkFriendlyStartLockedSelect` 0x66E710 removes n0 =
   min(the hand pot info's `amountPickedUpInitially` (25), the source) from the source, makes
   `Pot::Create(HandWood / HandFood, n0)` (a PileWood or PileFood of scale 1), puts it in the hand (locally
   `CHand::SetHeldObject`, then HOLDING with no tug), starts the SPRINKLE_FOOD / WOOD immersion and the FOOD / WOOD
   pick-up particle (`StartMultiPickup`), then status +0x3C = the pile, +0x40 = 0 and Flags |= LOCKED_SELECT.
3. **Every game turn**, `GInterfaceStatus::Process` 0x5DC4E0 counts ++turns (the first call sees 1), needs the pile
   available and the influence at the hand above 0, then `PileResource::ProcessInInteract` 0x66E520: ticks = (1000 /
   ms a turn) × the ramp time = 10 × 6.0 = 60; t = clamp(turns / ticks, 0, 1); n = (int)(perTurn + (perTurnEnd −
   perTurn) × t²) = (int)(8 + 62 t²); n = min(n, the source, max(0, `maxAmountCanBePickedUp` (20000) − the hand's
   amount)). n = 0 stops it (returns 0); else `RemoveResource(n)` from the source and `AddResource(n)` to the hand pot,
   with `G_PICKUPWOOD` / `G_PICKUPFOOD` at the hand at pitch 60 + 180·t² and tooltip 0xEEA with the hand's amount. All
   the pick-up fields come from the **hand** pot's info (only rows 11 and 12 have them); the pot's `maxAmountInPot`
   (1600) does not limit the hand (`JustAddResource` clips only when `nextPotForResource` is not 19). Ms a turn is
   taken as 100 (🔍 inferred: 0xD01A38 has no static writer).
4. **The ramp** (the total includes the first 25): 8 a turn at turn 1 (33 held), 9 at 10 (108), 14 at 20 (225), 23
   at 30 (413), 35 at 40 (706), 51 at 50 (1139), 70 at 60 (1746) and after (4546 at turn 100): 700 a second from 6 s,
   and the hand reaches 20000 at turn 321 (about 32 s).
5. **The end**, by any of: the action let go (state 4 → 5 → packet 0x1C → 6 → idle); `ProcessInInteract` returning
   0 (the source empty, the hand full, or a non-matching pot in the hand); the source deleted (a loose pile is deleted
   at 0; the pick-up does not go on to another pile); the hand out of the influence; or `EndAction` (its exit
   0x5D4870 sends 0x1C and calls `StopMultiPickup`). Packet 0x1C's `NetworkFriendlyEndLockedSelect` stops the multi
   pickup, shows the tooltip and clears the lock. **The hand keeps the pot**, and follows the mouse again.

Fields (0x529730 / 0x529900) do the same with the HandFood info, halved when `IsUnripe()`; a fish farm too, with
pick-up type 3. The source piles never scale: `PileResource::SetSize` sets a target sink of (`GetProportionRaised` −
1) × `GetHeight`, eased by a cubic over 1.0 s in `PileWood` / `PileFood::Draw`; the pile rises from or sinks into the
ground and is not drawn once buried. Store-owned piles (a storage pit, a workshop) read and remove the store's total.
`Pot::GetScaleFromAmount` = min(5, amount / scaleEvery + 0.25) is for plain pots only (potType 0). Putting down is not
incremental (a pot has no locked apply): see [The release](grab-hold-and-throw.md#the-release).

#### A pile in the locked select is HOLDING

The scooping hand is in the HOLDING state. `GetRequiredState` 0x46CD10, block 0x46CD5D..0x46CD9A:

- **The test:** an object is held (+0x4904), the status's locked select (+0x39C → +0x3C) is set, and that select is
  a pile: IsPileResource, vt +0x4CC. Then the state is HOLDING 4 (0x46CDF6), before GRAIN 8, TUG 3 and the 180 ms
  release impulse (0x46CDEC). Interface mode 0x19 still gives INVISIBLE first (0x46CD38). The impulse's own test
  (0x46CD6C) asks the same thing.
- **IsPileResource:** PileResource::IsPileResource 0x66ED60 returns true in the PileWood, PileFood, PileResource,
  MagicWood, MagicFood and PuzzleGrain vtables; GameThingWithPos 0x402490 (false) is every other class.
- **A field or a fish farm**, whose hand pot is held too, is HOLDING through the held object's branch
  (0x46CD9B..0x46CDF6, with the 180 ms release impulse). So the scooping hand neither tips down, nor hovers 3 above
  its source, nor pins the cursor: it holds its handful as it holds anything else.
- (🔍 inferred) the hand pot is in the hand once the action is IN LOCKED SELECT (cursor state 14, which with an empty
  hand would give the totem's state 5): the 0x1B handler puts it there (`NetworkFriendlyStartLockedSelect`) before it
  sets Flags 0x10, which action state 3 waits for. That the field's and the fish farm's starts place it through
  0x5DC870 as the pile's does has not been read.
- **The hold.** While the pile is locked, `HandStateHolding::Update` (0x5B3EA8, when the interact object
  `IsLockedInInteract`) freezes the required position at the hand's x, z, with y = the altitude + the pile's
  `GetHeight`; there is no mouse following and the camera tricons are off. The pose is the hand pot's SIDE hold
  (anim 9, grip = min(1, R ÷ (3.2·handScale)), frame (numFrames >> 1)·grip, R = `Get2DRadius` each frame): constant
  for wood (scale·max(bboxX, bboxZ)), opening with the amount for food (× `PileFood::GetProportionRaised`, p = 0.05 +
  0.95 × amount / 1600 clamped, q = 1 − (1 − p)², fully open at 1600). The held mesh keeps scale 1.

#### The scoop's functions

- **The pot table.** `GPotInfo` rows at 0xD4C660, 0x144 bytes each (the index (info − 0xD4C660) / 0x144 compiled as
  `imul 0x1948B0FD, sar 5`). The exe's row is 0x10 bytes longer than info.dat's 0x134, its tail end-aligned:

  | Offset | Field |
  |---|---|
  | +0x114 | potType (0 a plain pot, 1 a food pile, 2 a wood pile) |
  | +0x118 | resourceType (0 food, 1 wood) |
  | +0x11C | maxAmountInPot |
  | +0x120 | scaleEvery |
  | +0x124 | nextPotForResource (19 none) |
  | +0x128 | associatedReaction |
  | +0x12C | canBecomeAPhysicsObject |
  | +0x130 | amountPickedUpInitially |
  | +0x134 / +0x138 | the per-turn amounts at the start and the end of the ramp (getters 0x66CCE0 / 0x66CCF0) |
  | +0x13C | maxAmountCanBePickedUp |
  | +0x140 | the ramp time |

  `GPotInfo::GetTicksToChangeOver` 0x66CD00 = (int)((float)(1000 / ms a turn) × the ramp time);
  `GPotInfo::GetAmountPickedUpThisTurn` 0x66CD30 (name chosen in the reconstruction) clamps t (≤ 0 gives 0) and
  truncates; 0x66CDE0, the ramp fraction (name chosen there), is clamp(clamp(n / ticks, 0, 1)², 0, 1).
- **Which piles.** `PileResource::ValidForLockedSelectProcess` 0x66E4F0 (vt 0x6CC) is every pile but a hand pot lying
  on the ground (`PileWood::GetHandPotInfoType` 0x66EC40 → 11, `PileFood`'s 0x66EC30 → 12);
  `PileResource::IsLockedInInteract` 0x55D680 (vt 0x6A0) is 1. Hand pots are themselves PileWood / PileFood
  (`Pot::Create` 0x66CF10). Piles and fields keep `Object::InterfaceValidToTap` (0x4196B0, 0), so a short click scoops
  too.
- **The start.** `StartLockedSelect` 0x5D1950 latches the hover, calls the object's
  `NetworkUnfriendlyStartLockedSelect` (nothing for Object), sends packet 0x1B and enters state 3; the 0x1B handler is
  the one in [The locked select](#the-start). `PileResource::NetworkFriendlyStartLockedSelect` 0x66E710 (vt 0x6D0):
  n = min(the hand pot's amountPickedUpInitially, GetResource) (0x66E735..0x66E75F; a store pile offers the store's
  total), RemoveResource(type, n, interface, &poisoned) (0x66E77E), Pot::Create (0x66E7A3), SetPoisoned(poisoned ||
  its own) (0x66E7D5). RemoveResource's out flag: 0x66EE60 writes *out = IsPoisoned. It makes the hand pot at the
  hand's map point (status +0x14) with the source's town, puts it in the hand (`PileResource::ValidForPlaceInHand`
  takes a hand-type pot with an amount above 0; `InterfaceSetInMagicHand` returns 1; locally `CHand::SetHeldObject` →
  `PickUp(pot, needsTug 0)` → HOLDING), keeps the source's player at status +0x128 (🔍 inferred), and calls
  `PSysGlobal::StartMultiPickup` 0x68F8C0 with the pick-up type (0 food, 1 poisoned food, 2 wood, 3 fish; the table
  0x936C50 = {0, 0, 1, 0} says which are wood). That acts only for the local status: the immersion SPRINKLE_WOOD (0xC)
  or SPRINKLE_FOOD (0xB) with 0x80000000, and `CreatePickupPSys(type)` on the particle utility 0xD4E0E8 (fn_006715F0:
  the old one killed (+0x28), then FOOD_PICKUP 0x6B, FOOD_PICKUP_POISONED 0x6C, WOOD_PICKUP 0x6E or FOOD_PICKUP_FISH
  0x6D, which fn_006714F0 moves to the hand each frame and kills after 60, units 🔍 inferred).
- **Each turn.** `PileResource::ProcessInInteract` 0x66E520 (vt 0x808): no pot in the hand, the pile itself, or a
  pot of another type → 0. Then the amount, `PSysGlobal::UpdateMultiPickup` 0x68F930 (name chosen in the
  reconstruction; local status only: `G_PICKUPWOOD` 0x62 or `G_PICKUPFOOD` 0x2C from fn_0068FA40 at the synced hand
  +0xC8, played every turn, its pitch ftol(f × 180 + 60) through 0x428740), the source and hand limits (a signed
  compare), n = 0 → `StopMultiPickup` and 0; else `RemoveResource` (vt 0xA0, store-aware) and the hand pot's
  `AddResource` (vt 0x9C → `Pot::JustAddResource`); a source just deleted (+0xA & 1) also stops the multi pickup;
  tooltip 0xEEA for the local interface (`ToolTips::ForceToolTips`); 1.
- **The stop.** `PSysGlobal::StopMultiPickup` 0x68FA50 (local status only) stops both SPRINKLE immersions, kills the
  pick-up particle (fn_006714D0, +0x28) and both pick-up sounds. `PileResource::NetworkFriendlyEndLockedSelect`
  0x66E850 stops it, shows tooltip 0xEEA with the hand pot's amount for the local player, and returns 1. State 4
  waits for the release and state 5 sends the end (see [The action states 3 to 6](#the-action-states-3-to-6)). After
  the unlock the hand springs back to the cursor.
- **A field.** `Field::NetworkFriendlyStartLockedSelect` 0x529900: n = (int)min(HandFood's amountPickedUpInitially
  [0xD4D6C0], the food +0xDC), halved when `IsUnripe()`; `RemoveFood(n)`; the hand pot from HandFood (0xD4D590);
  status +0x128 = the point's most influential player; `StartMultiPickup(food)`. `Field::ProcessInInteract` 0x529730:
  n = (int)min(HandFood's amount this turn, the food); the hand limit with no clamp at 0; halved (signed) when unripe;
  n ≤ 0 stops; any pot in the hand takes the food (no hand pot type test). 
  `Field::ValidForLockedSelectProcess` 0x5299E0 reads +0xD0 > 0 and +0xDC > 1.0, growth > 0 and food > 1 (🔍 inferred: grown, with food left). The press
  (ActionPressed fn_005D1330) starts the scooping at once (`StartTapOrLockedSelect` 0x5D1A00) inside the influence;
  outside it the field is neither placeable (Object 0x402870) nor tappable (Object 0x4196B0): nothing.
- **A fish farm** (0x52D770 / 0x52D950) does the same, with pick-up type 3.
- **The held pot's grip.** `MobileObject::GetHoldType` is HOLD_TYPE_SIDE (6), so anim 9 Chold_side, R =
  `GetHoldRadius` = `Get2DRadius` (`Object::Get2DRadius` 0x638180 = the scale × max(the mesh's half x +0x24, half z
  +0x2C); `PileFood::Get2DRadius` 0x66F180 × `GetProportionRaised`), the lowering `GetHeight` × 0.7, every frame; the
  held mesh is drawn at scale 1 (`Pot::DrawOutOfMap` → `Object::DrawOutOfMap` with HandStateHolding's matrix). The
  wood hand pot is mesh 329. `PileWood::GetProportionRaised` 0x66F1B0 (vt 0x86C): p = amount / maxAmountInPot, above
  0 0.05 + 0.95 p (0x933014 = 0.05), clamped; `PileFood`'s 0x66EB60 the ease-out 1 − (1 − p)². `PileResource::GetLife`
  0x66E3D0 = amount / maxAmountInPot; `SetLife(x)` 0x66E400 adds or removes to reach x × max.

### The hand's state, the spring, the held object, power-ups and roots

#### The hand states and who sets them

CHand builds its states in its constructor, one pointer each at +0x487C + 4 × n (0x46BC80..0x46BDD1): 3
`HandStateTug` (+0x4888, 0x5B7DD0), 4 `HandStateHolding` (+0x488C, 0x5B3AA0), 5 `HandStateTotem` (+0x4890,
0x5B7670), 6 `HandStateMultiPickUp` (+0x4894, 0x5B5730, vtable 0x900B6C). The totem's state is on
[The village totem](../magic/village-totem.md#the-locked-select-on-a-village-totem-hand-state-5).

The current state (+0x4878) is written only by the constructor (0x46BBC3), by `CHand::PrepareForDrawing` with the
state `GetRequiredState` asks for (0x46C755), to 1 by `OnClearMap` (0x46E916) and at 0x46ED0A, and by the load and
save (0x46E948 / 0x46EB28). `GetRequiredState` 0x46CD10 returns 0, 1, 2, 3, 4, 5, 7, 8, 9 or 10, never 6 (its jump
table 0x46CF48 / 0x46CF5C gives 0, 5, 1 or 7, 0 and the default 1 or 2), and fn_0046CF80 returns the state asked
for, or keeps the current one while CREATURE 7 waits.

<a id="the-scoops-hand-and-the-multi-pickup-state-that-never-runs-hand-state-6"></a>
#### The multi pickup state that never runs (hand state 6)

**So `HandStateMultiPickUp` never runs** (the same in the 1.00 build: the functions have the same sizes). It spans
0x5B5730..0x5B5BF0. The scoop is HOLDING instead ([A pile in the locked select is
HOLDING](#a-pile-in-the-locked-select-is-holding)).

What it would do, had it run:

- Enter 0x5B5750 takes `SetDistanceFromView(fn_0046DB70(the interface's +0x3C8))`, fn_0074CAF0 and
  `LHMouse::PinImage(0x8000, 0x8000)` (0x5B57AB), as the totem's Enter. Exit 0x5B57C0 unpins (LHMouse +0xCC = 0).
- Update 0x5B57D0, with a locked select: the hand's y = the land under the hand's map point (0x5B5802..0x5B584F) +
  the locked object's `GetHeight` (vt +0x42C) + the integer 3 at [0xBF01A0] (0x5B5852..0x5B5868); the totem's tilt,
  7π/16 and, below a y of 2.5 ([0x900B64]), atan((2.5 − y) ÷ 3.2) more (0x5B57F8..0x5B589E); and Chold_side (anim 9)
  at the totem's closure, `GetHoldRadius` ÷ (3.2 × 0.5) (0x5B5AB5..0x5B5B4D; the frame cut as
  `ftol(fild(dur sar 1)·grip)` at 0x5B5AFE..0x5B5B21).

`PinImage` 0x7E5550 has three callers: this state's Enter (0x5B57AB, never run) and the totem's Enter and Update
(0x5B777A, 0x5B795E). The totem's is the only cursor pin the game makes, and it is not seen: the shown hand sets
the mouse's image to none ([the village totem](../magic/village-totem.md#the-locked-select-on-a-village-totem-hand-state-5)).

#### HandStateGrain

- **What it is.** HandStateGrain is CHand +0x489C, hand state 8 (the hand holds a spell seed); ctor 0x5B2B80,
  vtable 0x900B00. `UR_HandSprinkle` starts it (fn_005B2F70), CHand::GameTurnUpdate steps it, HandStateHolding::Update
  reads it.
- **Its functions.** vt 0x18 (0x5B2D30) / vt 0x14 (0x5B2D50): the height and the tilt interpolated between the last
  two turns with g_game +0x205D64 (the fraction of the turn drawn). vt 0x1C (0x5B32A0): with ClampHand the hand's
  required position is the one it had when it started. Enter 0x5B3080 (after HandStateHolding's) resets everything;
  Exit 0x5B3290 sets +0x124 = 0, +0x13C = 0. Its update's smoothing is in
  [Holding, spring and throwing](grab-hold-and-throw.md#holding-spring-and-throwing).
- **The throw block** (CHand +0x48C8 / +0x48D4) is written at the end of each frame's placement, from the hand's
  position before the frame's move (taken after `Grain::Update`, 0x5B3317) and after it; its writers are in
  [The angular velocity and the grain's smoothing](grab-hold-and-throw.md#the-angular-velocity-and-the-grains-smoothing).
- **When the seed leaves the hand** (`SpellSeed::InterfaceSetOutMagicHand` 0x728940) the hand stops drawing it; out
  of the hand `Spell::DrawSpellSeed` 0x721360 → 0x729020 draws a seed that follows its spell
  ([The hand seed over the forest](../magic/forest.md#the-hand-seed-over-the-forest)).

#### The spring on a refused release

`State_ReleaseHeld` 0x5D4DB0: when the action is not valid (0x5D5150), the HOLDING state's spring stops (states[4]
+0x104 = 0). That happens only when CHand +0x4878 is 4, then DropOnMapCoord 0x5D1850. There is no ClearBuffer on that
path; it is RemoveFirstFromHand's (0x5DC239).

#### The held object's turn place

`GMagicHand::Process` 0x5FB1C5..0x5FB250: MapCoords x, z = ftol(x or z × 6553.6). The land under it is looked up at a
second conversion, ftol((x × 65536 [0x8AC408]) × 0.1 [0x8AC404]) (0x5FB1FE / 0x5FB20E). The altitude +0x1C = y −
GetAltitude there, kept as a float. There is no quantisation of y.

#### The held object's sorting, shadow and specular

`CHand::SetHeldObject` fn_0046E590:

- **Need-sorting:** the press fn_0046DC30 saves IsNeedSorting (vt 0x44) at +0x4910 and sets it (SetNeedSorting vt
  0x40, 0x46DCD3), so the object goes through the Z-sorter (0x8133AE); another object (fn_0046DE40) gives the old one
  its bit back; CHand::ThrowObject 0x46DDD0 restores it (0x46DDEF).
- **Shadow:** vt 0x78 SetCastDynamicShadow (fn_008168A0) sets bit 0x40 of +4, which in fact means the object
  *receives* the projected shadows. Only the hand and the creatures cast them (CreateDynamicShadow 0x80C020). The
  hand's vt 0x234 fn_00816830 clears the bit on the held object (0x816842) and adds its outline to the hand's own
  shadow (0x816855).
- **Specular:** the press also zeroes the G3D's specular +0x50 (0x46DCDF). Every Draw rewrites it from the land light
  (🔍 inferred).

#### ProcessPowerUpSystem runs twice

`GInterface` 0x5CF300. Its callers are fn_005D1120's three exits (0x5D118A / 0x5D11E1 / 0x5D124D). It runs on both
of fn_005D9A20's paths: the turn's, `GInterface::Process` 0x5CEC10, with the last frame's g_game_time_inc; and the
frame's, `ProcessFrameUpdates` 0x5CEDB0 = `jmp 0x5D9A20`, with this frame's. fn_005D9A20 runs InterfaceActionProcess
once for each queued button message (types 1-6 of GInterfaceMessageBuffer +0x430, flag +0x30 of table 0xD186B8;
fn_005D9BC0 0x5D9CFD) and once with none, so a frame with two button edges runs it twice; the playback path
(fn_005DAEE0 0x5DB2EE) calls it too.

#### A tree's roots

- **A live tree:** the roots have no object of their own. fn_00511270 draws them from the tree's G3D, the rotation ×
  0.15 × extent (0x8CF110). It is called from Tree::DrawOutOfMap 0x74B295 (in the hand) and PhysicsObject::DrawAll
  0x646FCF (flying). A deleted tree's roots just stop.
- **The falling roots:** Tree::EndPhysics 0x74B830 sets the DeadTree's +0x98 |= 0x10 (0x74BC10). At its first draw,
  DeadTree::Draw 0x5107F0 makes the node: fn_00826280, mesh 0x250, list 0xEB9A10. The node copies the DeadTree's G3D
  matrix (0x8262F9..0x826304; rebuilt by SetScale 0x51092D) and scales the rotation (0x826306..0x826357). It falls
  y0 − 20 t², fades after 18 s and is freed after 20 s. A forester's felled tree never sets the flag, so its roots stay
  on.

### Sources

- The reconstruction of the hand's input layer (`interface.cpp`, `multi.cpp` and their notes): every caller read
  instruction by instruction, the tables 0xD17DA8 and 0xD186B8 by emulating their initialisers; states 2, 3–6, 10/11
  and 15–23 only summarised there and read since where this page gives addresses.
- The exe with the xref index; bw1-decomp for the names (`Creature.h`, `Player.h`).

## openblack

**Done**: the press's branch order, the locked select on a creature and its packets, the CREATURE state's timer and
feedback, the CREATURE state's clips, blends and clock, the stroke's rule, the interaction camera's place, the
scoop as HOLDING, the box rule, the hand's packets and the grain state. **Partial**: the slap's speed, the CREATURE
state's palm zoomers and end-ready test, the interaction camera's choice of the modes it takes the view from, the
ends of some action states. The port decisions taken from raffclar's code
are in [Port decisions: the hand, the camera and the interface](../project/port-decisions-hand-camera-interface.md).

### The hand on a creature in openblack

`HandSystem` (the press branch, `UpdateCreatureLock`, `UpdateCreatureInteraction`, `LeaveCreatureState`),
`ECS/HandPressChain.h` (the press's branch order), `ECS/CreatureHandPackets.h` (the packets 0x59 / 0x5F), the
creature hand `CreatureHandSystem` (strokes and slaps) and `Creature/CreatureHandRules.h`.

- **The press.** openblack keeps the order in `hand_press::Choose`: screen object, field, hovered object, tap-only
  object (abodes, temple entrances), creature, fish farm, seed, held object; while a creature is held or let go no
  press is taken (the interface is in action states 3..6). The holding press is the seed and held branches; the
  bubble is the screen object branch. A creature under the hand keeps the fish farm out (`cursorIsCreature`), with no
  branch of its own; the leash pull (step 3, it needs a pick of the rope) is not ported from this path. Step 5's fall
  through (2026-10-21, fork issue on the hand's press over a fixed thing): with a thing at the point that no branch
  takes, the untied leash's `StartGrab` taps it (`HandSystem::Tap`, the leash's act on it, packet 0x5D), else
  `ClickOnLand` (`TapLand`), as with nothing there (`hand_press::PressFallsThrough`, test
  `HandPressChain.APressOverAThingNoBranchTakesClicksTheLandUnlessTheLeashIsOn`). Before, a thing found there kept the
  press from doing anything.
- **Who may be held.** `creature_hand::MayHold` stands for `ValidForLockedSelectProcess`, with asleep and frozen.
  raffclar's rule allowed any god's creature; openblack holds the player's own or an allied player's
  (`creature_hand::IsFriendlyTo` over the owner's alliances, `ecs::alliances`), as the original.
- **The locked select.** `ApplyStartLockedSelect` locks the creature the hand pressed on, tries the creature hand's
  `Grab` (which refuses a creature `MayHold` rejects) without letting its result end the select, then releases the
  creature's cast. (openblack's own guard) a start for a creature other than the one the hand pressed on still ends
  the select; the hand cannot send one.
- **The CREATURE state.** The interaction timer runs in the hand's camera ms (the hand's dt,
  `game_clock::CameraFrameMs`), from the first frame of hand state 19; outside a hand demo's playback that is the
  frame's real time, as the original's camera time step, so whether a let-go is a click (and its 0x5F, which works the
  leash at the next turn) depends on real frame time. **This is a documented exception** to openblack's rule of no
  wall-clock time in game logic, kept for fidelity with the original (2026-10-08); deterministic runs see a fixed
  camera step. The CREATURE state places the hand where the creature hand puts it, drawn larger by the square root of
  the creature's drawn size, and plays the creature hand's cycle: Cwiggle off the body, Ctickle on it, Cslap once the
  hand has moved faster than 5H, until it strokes again (raffclar #348). Which clip plays and when is the original's
  (checked 2026-10-22, test `CreatureHandSystemTest.TheCaressWaitsForASecondOnTheBodyInAllWithTheCursorMoving`): the
  state starts on Ctickle, and once off the body the caress comes back only past a second on it in all with the cursor
  moving. Each change of the cycle to Cslap or Ctickle swaps the pose buffers and starts the state blend, twice when
  one step makes both, and the return to Cwiggle does not (`creature_feedback::NextCycle`'s `blendStarts`, read by
  `HandSystem::Update`), as the original's clip changes. The cycle is shown at the state's own clock
  (`HandStateClock`), zeroed at its Enter, on by the frame's real ms and wrapped at the length of the cycle shown, not
  restarted at a change of cycle, as the original's +8. As the original's,
  the CREATURE state begins only once the creature is locked and has finished the body action it was doing when the
  hand took hold of it: the hand's turn (`HandSystem::ProcessTurn`, after the start packet) checks it each turn until
  the body plays none, and from then on until the release (`hand_creature::StillFinishing`). **⚠️ approximate** the
  creature in the CREATURE state is the locked one, not the hover object read again each frame; the end-ready test is
  taken as "not ready when the hand leaves state 7, ready one frame after the release"; the help candidate the start
  sends is not ported. The turn the CREATURE state begins (`hand_creature::TakesBodyOffTheGame`) the body is taken off
  the game as the original's `DisconnectFromGame` takes it: `creature_turn_pose::Freeze` keeps the turn's pose, which
  the game reads (`creature_turn_pose::Safe`) while the body drawn goes on playing; the end packet (0x1C) brings it
  back (`creature_turn_pose::Reconnect`, as `ReconnectToGame`). **⚠️ approximate** the reconnect's look reset and
  re-pose, and the locked-select start's second copy of the pose (0x5DA9CA), are not ported; the readers of the kept
  pose are only those openblack reads through `Safe` (the creature's hands and its particles).
- **The packets.** 0x27 is not sent: openblack calls the strokes and slaps directly. 0x5F is the leash key
  (`PressKey(Leash)`). 0x59 reaches the mind's feedback.
- **The slap and the stroke** (`CreatureHandSystem::Update` / `Step`, `CreatureFeedback`; raffclar #348; the stroke's
  rule read against v1.42 on 2026-10-21, the slap's speed not the original's): one step a frame on that frame's ms, as the original. The slap is weighed only as the body ray hits
  after missing or the cursor crosses the screen's middle column, with the clock since taking hold or the last stroke
  above 1000 ms; its speed is the hand's movement across the land on the plane through the creature over the frame,
  not eased (the original projects the two cursor points at the hand's depth); mirrored when the cursor moves left.
  The stroke needs the ms since the last stroke or slap at least trunc((1 - sum) x 1250) and the ready clock at 2000
  (1000 after a stroke or slap), a part other than the last (forgotten off the body), and the hand on the body more
  than a second in all with the cursor moving across (`feedback::k_StrokeHoldMs`, the clock zeroed by taking hold and
  by a slap weighed, kept off the body and after a stroke): from raffclar's lane, and the original's (read
  2026-10-21, [The CREATURE state's update](#the-creature-states-update)). The face is pulled before the interrupt tests. Every
  third hard slap in a row at one band with a hit sends packet 0x63, the count starting again after each (ours, from
  the exe, 2026-10-18; it was the third and every later one): the player's own creature loses 0.01
  life, never below 0.01; its body's mark is not drawn. The height is the drawn size, as the original's. Ready one
  frame after the release (`StepLock`).
  - **raffclar's fixes kept beside, unused** (deviations, game logic): `creature_feedback::Pace`, his step at most 60
    times a second taking all the time since the last (at our fixed 16 ms frames it would step every other frame with
    32 ms, which is not the original's once a frame). His bruise on every third hard slap only, the count starting
    again after each, is the original's rule and is now `creature_feedback::CountSlap` itself.
  - **A vanilla bug kept:** a hard slap on another god's creature costs the player's own creature its life, as 0x63
    does; neither tree draws a mark.
- **The interaction camera** (`CreatureInteractCameraModel`; raffclar #348, ✅ v1.42 for the place, 2026-10-21): while
  hand state 19 lasts with a creature, `UpdateCreatureFrame` has the camera that has the view give way, when it can be
  left (the player's own, Creature Mode's, a miracle's placed path or a script's dual camera,
  `creature_interact::CanBeLeft`, below), to a camera 1.5 of the
  creature's drawn heights from its middle, in front of it (30 ahead and 15 - 0.7H up in its own space), gliding there
  in 0.7 s on the camera's zoomers, renewed every frame. That is the original's `CameraModeCtrInteract::Update`
  0x44A960, read whole and run in the oracle: with s the 3D body's scale (+0x90), H = 15s, θ its +0x5240 and P its
  +0x78..+0x80, the focus is P + (0, 7.5s, 0) and the eye the focus + k·(30 sin θ, 15 − 10.5s, −30 cos θ) with
  k = 22.5s / √((15 − 10.5s)² + 900), each of the six through `Zoomer::SetDestinationWithSpeedAndTime(v, 0, 0.7)` on
  the camera's focus (+0x88) and eye (+0x118) zoomers; it does nothing while the creature is unavailable (byte +0xA
  bit 0) or the local player has none, and stops for good once its creature is gone (+0xC = 0). So 15 − 0.7H up for
  every 30 ahead, 1.5H away, turned by the body's yaw (ours is minus the yaw, as in
  [Held by the hand](../creature/mind.md)); test `CreatureTouch.TheCameraTakesTheOriginalsPlaceForTheCreature`, pinned
  to the oracle's values. Not read: whether the body's +0x5240 (the yaw of its last turn, queued or started) can
  differ from the drawn yaw outside a turn; ours takes the drawn heading, or the target while it turns. On any other state, the box taking the input or the creature
  going, the camera it took over from has the view again, as the original's `PopViewMode`: the player's camera is put
  where the camera is (`PlaceAt`) and flies back to where it was going (`SetFlight`, wooshing beyond 150); Creature
  Mode's camera goes on following its creature from where the camera is, its view kept and its time in the mode started
  again (`CreatureCameraModel::Restart`), so it eases at its starting pace; and the camera's time in the mode
  (`script_camera::ModeChanged`) starts again both when the interaction takes the camera and when it gives it back,
  as the original's push and pop zero it. Which camera has the view is told by its model
  (`creature_interact::HolderOf`); the player's camera watching a fight is the player's camera, as the original's fight
  view is part of `CameraModeNew3`. The temple's and the editor's cameras keep the view: the temple's rooms' cameras are
  not the original's game camera, and no creature is held there; the editor is a developer's tool. Tests
  `CreatureTouch.TheCamerasThatCanBeLeftGiveWayAndNotWhileAScriptCameraLives`, `TheCamerasAreToldApartByTheirModels`,
  `CreatureCameraModel.GivenTheCameraBackItsTimeStartsAgainAndItsViewIsKept`. While the interaction has the view, the
  creature key and a double click do not lock Creature Mode on, as the original's `CameraModeFollow` refuses itself
  while the current mode cannot be left; Creature Mode under it stays as it is
  (`CreatureModeSystemTest.WhileAnotherHasItsCameraADoubleClickLeavesItAsItIs`). The creature key does nothing at all
  then (`creature_mode::OnCreatureKey`'s `currentCannotBeLeft`, from the camera's model being the interaction's;
  tests `CreatureMode.WhileACameraThatCannotBeLeftHasTheViewCDoesNothing`,
  `CreatureModeSystemTest.WhileTheCreatureInteractionHasTheViewCLeavesItAsItIs`,
  `WhileTheCreatureInteractionHasThePlayersViewCDoesNotLockOn`): with Creature Mode under the interaction it used to
  mark Creature Mode to leave once the camera came back (2026-10-22), where the original's key lets go only of a follow
  that is the current mode ([The creature key during the interaction](#the-creature-state-and-its-packets)).
  As the original's, the camera is asked for once, as the interaction starts: a camera that keeps it then keeps it for
  the whole hold, even if it would give way later (`creature_interact::AsksForTheCamera`); and, as the original's, it
  does not give way while a living script camera is the current one or for a creature no longer in the world
  (`creature_interact::CameraGivesWay`). The camera that has the view is a script's camera or dual camera that is
  current, over a placed path that has the camera, over the camera's model (`creature_interact::CurrentHolder`, test
  `CreatureTouch.TheCurrentCameraIsAScriptsModeOverAPlacedPathOverTheCamerasModel`): so a dual camera over a living
  script camera gives way too, as the original's current mode is the dual one (until 2026-10-22 any living script
  camera kept the view). Taking the view, the interaction's camera is current above the script camera's modes and
  the placed paths (2026-10-22): `script_camera::Cover` hands a moving dual camera's zoomers to the camera's, as the
  modes share them; while covered none of the script's modes is current, updated or moved, the script can neither
  change, release nor start one over it, and START_CAMERA_CONTROL is refused, as the original's script mode refuses
  itself under a mode that cannot be left (test
  `ScriptCameraDual.UnderTheCreatureInteractionsCameraItWaitsAndGoesOnFromTheCamerasZoomers`,
  `CoveredWithNoModeOfItsOwnTheScriptCannotTakeTheCamera`); `CameraPathSystemInterface::SetCovered` keeps every
  placed path from having or moving the camera, the player's keys from taking one back, and a new path from being
  placed, and the near plane is not the close one, as the path's `Cleanup` (test
  `CameraPathSystemTest.UnderTheCreatureInteractionsCameraThePathWaitsAndHasTheCameraAgainAfter`). Given back,
  `script_camera::Uncover` makes the top dual camera current again from the camera's zoomers as the interaction left
  them, and the top placed path has the camera again with the close near plane, as its `Restart`; the player's camera
  flies back and Creature Mode's restarts only when it is the one that has the view then (a dual camera or a path
  gone meanwhile leaves the view to the camera under it). Before, the path and the dual went on driving the camera
  while the interaction's model sat unused under them. (approximate) A dual camera started while the interaction has
  the view stays under it here, where the original pushes it on top (START_DUAL_CAMERA checks no mode). The hand is
  never hidden by a path in ours, as in the original, so the path's `Show(1)` has nothing to undo, and the light
  maps' state its `Cleanup` sets (0x6CA5D0) has no reader in ours. The debug switch is in the creature spawner's hands
  panel ("Creature hand"), with a readout of the camera the view was taken from (`CameraTakenFrom`), or the one that
  has it; the camera window's placed path panel says whether the interaction's camera has the view over the paths.
  As the original's, the end of a hold that asked for the camera and did not get it takes off the camera that kept the
  view (`creature_interact::AtTheEndOfTheHold`, `TakenOffAtTheEnd`; test
  `CreatureTouch.TheEndOfAHoldThatAskedTakesOffTheCameraThatKeptTheView`; 2026-10-22, ours gave back only a camera it
  had taken): a living script camera is deleted with its path and follows, the camera under it going on from its
  zoomers, the FOV left alone and the script still owning the camera (`script_camera::TakeOffScriptMode`, test
  `ScriptCameraDual.TakenOffByAnotherTheScriptModeGoesAndLeavesTheFieldOfViewAlone`); a dual camera is released as
  RELEASE_DUAL_CAMERA releases it (`script_camera::ReleaseDual`); the top placed path goes, its owner's release then
  finding nothing (`CameraPathSystemInterface::TakeOffCurrent`, test
  `CameraPathSystemTest.TakenOffByAnotherThePathUnderItHasTheCamera`); Creature Mode lets go of its creature. The
  camera's mode time starts again, and the player's camera, when it has the view after it, starts from where the
  camera is and flies back to where it was going, as for the interaction's own give back. The creature hand panel shows
  the camera the last such end took off (`LastTakenOff`). ⚠️ Not ported: a refused hold with the player's own camera
  current (only an unavailable creature refuses then) leaves it as it is; the original pops it, restarting the
  player's mode under it, flying back to where that one was left (after any script camera), or building a fresh one
  that drops the fight; openblack keeps no stack of the player's modes to tell which (the camera mode stack,
  [GCamera](../camera/game-camera.md#gcamera-and-its-zoomers)).

### The scoop in openblack

The scoop is HOLDING (`HandSystem::RequiredHandState`), as the original, so it pins no cursor (the totem's pin is
`HandSystem::GetCursorPin`, see [the village totem](../magic/village-totem.md#openblack)); openblack draws no
cursor image (the system cursor is switched off in `Sdl2WindowingSystem` and nothing draws one). The multi pick-up's
grip cut is not ported (the state never runs). raffclar's scoop rules are not taken (his `hand_grab::k_ScoopHoverAbove`,
`hand_grab::ScoopTip` are kept unused; his `HandGrabSystem`'s hover, `GetScoopTip` and `PinCursor`, and
`GameActionMap::GetCursorImagePosition`, are not ported): see the port decisions page.

### The interface states in openblack

- `HelpSystem::Process`'s +0x578 and its reader SendFOVObject are not ported.
- `IMMERSION_EXISTS`: openblack has no force-feedback mouse and always pushes false.
- `GInterface::SetActive` is `interface_active::SetActive` (`src/Input/InterfaceActive`, namespace
  `openblack::interface_active`; bit 0 only; the other effects are Pending);
  `interface_active::IsActive` and `GetFlags` read the same byte.
- **The double click.** `GameActionMap` keeps its previous unbindable map at the start of every `Frame`, so
  `GetChanged(DOUBLE_CLICK)` is true only on the frame the double click arrives (and the one it is let go in), and
  Creature Mode locks on once; until 2026-10-15 the previous map was never kept, so it read as changed on every frame
  the button was held and re-locked each frame (test `GameActionMap.ADoubleClickChangesOnlyOnTheFrameItHappens`). The
  world camera's flight and the temple camera still read the held `DOUBLE_CLICK`, not its change.

### A box and the mouse in openblack

`src/Input/MouseButtons.cpp`; Game.cpp; `HandSystem::BoxTakesInput`, HandTurn.cpp:

- `input::ApplyMouseButton` is the bindings: a press only when the button's binding is not pressed, a release only
  when it is; the press holds the interface's button, the release lets it go.
- `input::BoxTakesInput`, every frame before the hand's placement and update, is the interface's pass: a box takes
  the input while the escape menu or the SkipBox is up, or a debug window took a click since the last pass or still
  holds one (the debug windows follow the dialogs' rule; the original has none). Then the interface's buttons are
  cleared, and `HandSystem::BoxTakesInput` runs the action state's end as above (a locked select's end packet and the
  stopped multi pickup; a grab, a tug ((🔍 inferred) part of the grab state 13) and a release state end with nothing
  sent), then EndAction and the action state reset; the hand sees no release of what the box cut short. The button
  events that reach the game after a pass where a box took the input, until the next pass, send nothing.
- (openblack's own guard) while a demo the OPENBLACK_TEST_HAND_DEMO test hook started plays
  (`hand_demo::MarkStartedByTestHook`, `hand_demo::BoxRuleReachesHand`), the pass leaves the hand alone: the hook plays
  the demo from the land's start, under the SkipBox, a start the original does not have. A demo a script plays
  (PLAY_HAND_DEMO, the tutorial) gets the rule. Nothing changes while no button is held and the hand does nothing: the
  test runs, which ignore the real mouse, are the same.
- While the last pass found a box with the mouse (`interface_active::BoxHasMouse`), the hand's draw preparation hides
  the held object, as `CHand::DrawTheHeldObject` does (`hand_detail::DrawsHeldObject`, test `HandHeldDraw.*`; see
  [drawing the held object](grab-hold-and-throw.md#drawing-the-held-object)), and stops after the state change, as
  the original's does: no mouse smoothing, no state update, no blend, no pose or place, the hand drawn as last placed
  (`hand_detail::DrawPreparationStops`, test `HandDrawPreparation.*`; see
  [the frame order in openblack](animation-and-placement.md#the-frame-order-in-openblack)).

### The hand's state and packets in openblack

- **IS_LOCKED_INTERACTION** asks `HandSystemInterface::IsInLockedSelect` (pure rule `hand_creature::IsInLockedSelect`,
  test `HandLockedSelect.AnyThingInTheSelectIsLockedFromItsStartToItsEnd`; the native's boolean, and false for no thing,
  `ScriptHandNatives.AThingIsLockedInAnInteractionWhileItIsInTheHandsLockedSelect`): the creature the start locked for the hand,
  the totem it grips, or the field, fish farm or pile it takes from, as the flag above. ⚠️ Where the start takes
  nothing (a refused creature lock, a field or pile that gave nothing) openblack ends the select at once, so the thing
  is not locked in the turn the original keeps the flag until the end packet runs.
- **IsPileResource** is `ecs::object_resources::IsPileResource`: a pot whose GPotInfo potType is PileFood or PileWood.
- **The spring on a refused release** (⚠️ approximate): openblack has one spring flag for every state.
- **The turn place's second conversion** is `map_coords::MetresToFixedForHandLookup` (was `ToFixedTenthTimes65536`).
- **Need-sorting** is `components::NeedsSorting`, only for an object with a mesh. The held object's shadow: openblack
  clears its receive bit and adds its outline to the hand's shadow (ShadowList, RendererShadows).
- **ProcessPowerUpSystem**: `hand_casting::ProcessTurn` and `Update` are its two calls. Not ported: the extra call per
  queued action (fn_005D9BC0 0x5D9CFD) and the playback path (fn_005DAEE0 0x5DB2EE).
- **A tree's roots**: a deleted tree's roots are written with a zero matrix.
- **The packets** (`src/Input/GamePackets`, namespace `openblack::game_packets`): `Push` is `SendPacketCompressed`
  with the same flush at 6 (`k_AutoFlush`), `Flush` the loop's flush, and `ProcessOneSuperpacket` applies them at the
  next turn in order; a packet whose object no longer exists reaches its handler with `entt::null`. `Packet` holds the
  fields the interface's packets use; `Type` the type byte, its names `ApplyToMapCoord` (0x12), `HandAndCamera`
  (0x15), `Hand` (0x16), `Camera` (0x17), `ApplyUnlock` (0x1A), `StartLockedSelect` (0x1B), `EndLockedSelect` (0x1C),
  `ThrowHeld` (0x1D), `Give` (0x1F), `ReleaseImpulse` (0x40, `physics::from_hand::ApplyReleaseSpin`) and
  `SpellIconCharge` (0x6A). The sync handlers' copies (+0x11C, +0xD4, +0xE4, +0xE0) are not ported.
  A land load empties only the session's list (`ClearSession`, as `ClearMap` 0x552BB0 → `GGame::ResetState` 0x5557A0
  → `LHSession::EmptyEventQ` at 0x5557B9; the session g_game +0x205B80 exists in single player, the loop's flush
  `fn_005525E0` writes through it with no test); the buffer (g_game +0x5318, its length +0x5978) is kept: its only
  writers are `GGame::ClearVariables` 0x54BF20, from the GGame constructor alone (0x54B65E), and the flush
  (0x552609 / 0x552610). So what the interface sent since the last flush reaches the new land with the next flush,
  an object of the last land as `entt::null` (the registry's clear moves every entity's version on, as the original's
  unique ids no longer match, `GPacket::GetObject` 0x63DF70). Before 2026-10-22 openblack emptied both (`Reset`, 🔍
  inferred). `EmptyEventQ` is an import of the session library, so "empties the queue" is its name only. Test:
  `GamePackets.TheMapsClearDropsTheFlushedPacketsAndKeepsTheBuffer`.

#### Where it is in the code

`HandTurn.cpp` (the interface's turn side of `HandSystem`):

- `HandSystem::RegisterPacketHandlers` sets the handlers: `Type::HandAndCamera`, `Hand` and `Camera` store the synced
  map point, hand and camera and take the ping (`ReceivePing`); `Type::ApplyToObject` and `Type::ApplyToMapCoord`
  stamp `_applyHandledTurn` and apply the held object (an empty hand or a NULL target ends the action);
  `Type::PlaceInHand` runs `ApplyPlaceInHand`; `Type::StartLockedSelect` and `EndLockedSelect` the locked select's
  start and end; `Type::ThrowHeld` runs `ApplyForceDropHeld`; `Type::SpotVisual` makes the fail spot;
  `Type::ReleaseImpulse` spins the released object's body with the hand's move.
- `HandSystem::SendHandSync` is fn_005D2250: the countdown holds unless `HandCastNeedsContinualPackets`; a move is a
  component off by more than 0.01, or another map cell for the hand; it sends `HandAndCamera`, `Hand` or `Camera`
  with the ring entry from `NextPingIndex`, keeps what it sent in `_sentMapCoords` and the hand and camera fields,
  and sets the countdown to 1 (single player).
- `HandSystem::NextPingIndex` writes the ring entry (the engine timer's ms) and returns the old index;
  `HandSystem::ReceivePing` takes the difference for the ring entry a packet carries back. (⚠️ approximate) the
  engine timer's ms stand for GetTickCount: the same wall clock, without its 15.6 ms steps.
- `HandSystem::HandCastNeedsContinualPackets` walks the spells for one cast from the hand that is not closed down and
  that the human player casts.
- `HandSystem::SendRelease` is state 12's release: the throw's packet when the action's point is on the map, an
  object is held and the influence is there, else the spring stops; then the apply to the map point or the fail
  spot. (⚠️ approximate) no land under the cursor counts as off the map.
- `HandSystem::UpdateReleaseImpulse` is `GetRequiredState`'s impulse: nothing in the citadel; `_releaseImpulseMs`
  is 180 each HOLDING frame, then goes down by the frame's game ms and sends `ReleaseImpulse` the frame it goes
  below 0.
- `HandSystem::SendPlaceInHand` is GenericPickup: its sounds at once, the `PlaceInHand` packet and action state 7.
  `HandSystem::ApplyPlaceInHand` is the 0x13 handler: the checks again, then the classes that hand another object on
  (a big forest gives a tree, a teleport stone its seed, a seed through worship's `PlaceSeedInMagicHand`), else the
  pick-up; a refusal ends the action. The tree path also reaches it from `HandTrees.cpp` with `SendPlaceInHand`.
- `HandSystem::SendStartLockedSelect` pushes `StartLockedSelect` and enters action state 3;
  `HandSystem::SendEndLockedSelect` is state 3's end (for an object still available, `ecs::IsAvailable`, its
  network-unfriendly end and the end packet; for one that is gone neither; then the multi pickup stopped at once,
  `_lockedSelectStopped`); `HandSystem::ApplyEndLockedSelect` is the 0x1C handler (a NULL object only ends the
  action); `HandSystem::ProcessLockedSelect` is the status's per-turn locked select, ended by EndAction when the
  object, the influence or `ProcessInInteract` fails (so `ApplyEndLockedSelect` follows at the next turn). For the
  source the hand takes from (a pile, a field, a fish farm) the pass ends the action as the original does for every
  kind: `SendEndLockedSelect`, then `EndAction`, which lets go of the objects under the hand. For a
  creature or a totem (the interface's locked object that is not the hand's pick source)
  `hand_creature::StepLockedObject` lets go of it when it is gone, or a totem when the player's influence at the synced
  hand (`_turnHand`) is not above 0, and ends the action (`SendEndLockedSelect`, then `EndAction`): a totem's end sends
  its share (or a puzzle totem's height) and the end packet, whose handler drops it. The status's creature-lock bit is
  `HandSystem::_statusCreatureLocked` (`hand_creature::StepCreatureLockFlag`): set by a creature's start applied,
  cleared only by a creature's end applied, kept by `ResetTurnState`, and read by `HoldsLockedSelect` (the gestures'
  early out) and `IsHandReadyForObject` (the hand's and the worship code's, through `IsCreatureLockedForHand`). The
  hand's own `_creatureLocked`, which a creature gone while locked clears, drives only the hand's hold of it. openblack
  loads no saved game, so the load's clear has no counterpart. `HandSystem::IsHandReadyForObject` is the original's two
  conditions in order, `hand_creature::IsHandReadyForObject` (nothing held, then that flag clear;
  `HandReadyForObject` test); it no longer also refuses during a tree's tug or a locked select on a source (openblack's
  own additions), and an object offered during a tug is refused by the place in hand (`ApplyPlaceInHand`, nothing held).
  The Gestures debug window shows the readiness and the flag.
- `HandSystem::ApplyTap` is the 0x20 handler: an interactable object valid to tap, tapped at the synced hand.
- `HandSystem::ValidateHands` lets a held object that is no longer interactable leave the hand, with a seed's
  out-of-hand call, then the render hand's throw.
- `HandSystem::ResetTurnState(newGame)`: `CHand::OnClearMap`'s fields and the apply gate at every land load,
  `GInterfaceStatus::SetToZero` only at a new game ([clearing on a new land](#clearing-on-a-new-land)).

`HandSystem.h` (members and helpers):

- `_interfaceHandState` is the interface's hand state, 3 when reset; the bubble's own test reads 0x1D from the last
  turn.
- `_applySentTurn` / `_applyHandledTurn` are the apply gate (no other apply packet while one waits).
- `_pendingPressMs` keeps the grab's start (the frame's engine sample and the turn).
- `_handAngularVelocity` is the throw block's angular velocity; `UpdateThrowBlock` (`HandHolding.cpp`) writes the
  block at the end of the placement from the hand's position before and after the frame's move.
- `_renderHandHadNeedsSorting` is the held object's need-sorting bit before the hand set it.
- `PushThrowData` queues the throw's packet (the velocity and the held object's position and rotation);
  `ApplySeedToMapCoord` and `ApplyForceDropHeld` (`HandSpellSeed.cpp`) are the seed's 0x12 and the 0x1D handlers;
  `SeedLeftHand` stops the hand drawing a seed that left it.
- `FieldValidForLockedSelect` (growth above 0 and food above 1) and `ProcessInInteractField` (per turn; false when
  the source is not a field) are in `HandFish.cpp`.
- `LoadMorphMeshes` (`HandMorph.cpp`) loads Base2, Evil2 and Good2 on the CPU; `BlendHandWorld`
  (`HandPlacement.cpp`) blends the state change in world space for the draw data.

`HandGrain.h` (`hand_grain`): `ClampedPosition` gives the start position while ClampHand holds; `SetHoldingSeed`
enters (everything reset) or leaves the seed-holding state.

### Test hooks

- `OPENBLACK_TEST_HAND_DEMO` plays a hand demo from the land's start (the box rule left out while it plays); the
  cross-page list is in [Debugging](../project/debugging.md#hand-demos).

## Pending

- **An inactive interface** ([Interface active](#interface-active)): the action pass ending the action under way
  while the interface is inactive (except in action state 0xE), and setting it inactive again while a script holds
  the bars with no hand demo, are not ported; what action state 0xE is is not read.
- **The hand on a creature** ([The hand on a creature](#the-hand-on-a-creature)):
  - the interaction camera: ours is raffclar's model (above), whose place is the original's (read and pinned,
    2026-10-21). The original's mode (`CameraModeCtrInteract`'s constructor
    0x44A850) is made once as hand state 19 starts (+0x49C0 set either way) and refuses itself, deleting itself, when
    `CantExitCurrentMode` (0x441B70: the current mode's vt 0x1C answers 0) is 1, when the creature's byte +0xA has bit 0
    (0x44A871), or when the current mode is already a `CameraModeCtrInteract` (`__RTDynamicCast`, 0x44A8AB); else
    `SwitchToViewMode` and the creature stats display's HandValue, Exhaustion, EnergyLoss and LifeLoss are zeroed
    (0x44A8D4..0x44A8F2). bw1-decomp names them: vt 0x1C is `CameraMode::CanExit`, true but for
    `CameraModeCtrInteract` (false) and `CameraModeScript` (`!Valid`: not while the script's camera lives); the
    creature's bit is `GAME_THING_FLAG_UNAVAILABLE`. Taken in openblack: asked once a hold, refused while a script
    camera lives (`script_camera::Active`) or the creature is not available (`ecs::IsAvailable`), and taken from every
    camera of ours the original lets be left, the player's and Creature Mode's, which comes back at the end
    ([Which camera gives way](#the-hand-on-a-creature), above; ported 2026-10-22). The stats display's four values
    are not zeroed: openblack draws no creature stats display (they are set again each frame the creature is drawn
    while interacting). A miracle's placed path and a dual camera give way and come back as the original's, and the
    end of a refused hold takes off the camera that kept the view (ported 2026-10-22,
    [The hand on a creature in openblack](#the-hand-on-a-creature-in-openblack)), but for the player's own camera,
    which needs the camera mode stack ([The end of a hold that did not get the view](#the-creature-state-and-its-packets)).
    And the hand's hide (+0xAC from 0x46CBDC / 0x46CBEF);
  - the tap-or-lock wait (action state 19, 225 ms) with the leash in the hand or tied to this creature, and
    `Chold_fingers` with the leash pull (action state 21): they wait on the leash taps and a pick of the rope;
  - the CREATURE state's palm zoomers (0.3 / 0.9 / 2.5 s), read in
    [The CREATURE state](#the-creature-state-and-its-packets): queued as a fix (the clip changes through
    `StartStateBlend` and the state's own clip clock are ported since 2026-10-22, above); `StartImmersion` is force
    feedback, not ported;
  - the slap's speed from the two cursor points projected at the hand's depth. Read: both points go through
    `SetPointFromScreenPointAndDistance` (0x74CAF0: the current camera's eye plus the unit ray through the point times
    the distance) with this frame's camera and the same distance, CHand +0x4854 (0x5B1B1E..0x5B1B4E), the hand's last
    `SetDistanceFromView`, which this state sets at 0x5B196C from its distance zoomer (the 0.3 s one, queued with the
    palm zoomers); the speed is √((Δx/dt)² + (Δz/dt)²) (0x5B1B53..0x5B1BAA). Ours measures the hand's point on the
    level plane through the creature's middle, this frame's against last frame's (last frame's camera), so it also
    counts the camera's own glide; it waits for that zoomer;
  - the body's mark from 0x63 (fn_004866F0);
  - the end-ready test 0x476F00 (`ReachedLookDestination`, the body and facial actions) in place of "ready one frame
    after the release": queued as a fix;
  - the leash's tooltip and its help events 40 / 41 / 47 (`CalculateLeashToolTip` 0x5D67F0): queued as a fix;
  - not named: the creature's +0x10AC (19 against 14) and the interface's +0x37C[player];
  - the creature is picked by capsules round its posed bones, not by its drawn, posed triangles (openblack's creature
    has no CPU-skinned triangles).
- **A box takes the mouse** ([A box takes the mouse](#a-box-takes-the-mouse-the-escape-menu-a-dialog)):
  - the HistoryBox's and the GatheringBox's `WantsMouseControl` (only over a control): neither box is in openblack, so
    `input::BoxTakesInput` has no such case;
  - the ends of the action states 8 to 11 (fn_005D27B0; the packet 0x1A with the held seed) and 22, 23 (the packet
    0x24) are not in `HandSystem::EndAction` (it does let go of the objects under the hand); the creature's locked
    select ends through openblack's own `Release` (not the object's network-unfriendly calls);
  - while a box is up the original skips the action's processing (the hover, the near object); openblack's hand
    update still runs, with no button held;
  - to read: `ProcessFrameInputs`' demo gate (0x54C390) against the box pass: whether the original applies the box rule
    while a script's hand demo plays (fn_005D9A20's playback branch fn_005DAEE0 first, then fn_005D1120 at 0x5D9B1C
    only when no message was dispatched) (queued fix F-94).
- **The 0x12 handler** 0x5DA400: the call 0x76CD40 on status +0x130 (0x5DA46B) is not identified.
- **A land change**: Player +0x934, which the map clear's raw influence asks, has no field in openblack (only ever
  0, [influence](../magic/influence-and-alignment.md#pending)).
- **The locked select's start** (the 0x1B handler 0x5DA950): openblack has the one hand, so its +0x3C
  (🔍 inferred); a hand already holding something refuses it: state 3 begins only with an empty hand
  (`ActionPressed`, 🔍 inferred).
- **Hand demos driving the hand**: that the record's trigger byte [0xE853AD] is the space bar is 🔍 inferred. The
  recorded throw block (record +0x04..+0x33 into CHand +0x48C8) is applied: every message but a mouse move gives it
  to the hand through `HandSystem::SetThrowBlock` before the message is dispatched, and the release throws with it.
  An ACTION message's target is found again by script type and subtype within 3 m (fn_005DB4B0, FindNearPos
  0x6F7280, fn_005D5E40; `HandSystem::RetargetDemoClick`), unverified against v1.42. Two facts settled by disassembly
  (2026-10-17, xref note #484), no longer open:
  - the thing found is taken only when it is an `Object` (vt+0x460 `IsObject`, 0x5DB261), and set as the click's
    object only when `IsInteractable` (vt+0x190, fn_005D5E40 0x5D5E50) is 1; otherwise the collide was reset first
    (point zeroed, `SetToZero` 0x5DB26F) and the click lands on no thing. `IsInteractable` is `IsAvailable`, and for
    the abode family also "percent built != 0";
  - the press's fallback runs after a "no thing" retarget, as openblack's does: fn_005D9BC0 copies the retargeted
    collide into GInterface +0x3E8 (0x5D9BD8, its object +0x400) before the button's handler, and `ActionPressed`
    (0x5D138E-0x5D13A1) calls `FindObjectNearMapCoord(+0x3F0)` when +0x400 is null. openblack zeroes the action's
    point for a found thing it cannot act on, as vanilla does (its fallback then searches from the map's corner); with
    the Fix Vanilla Bugs switch on it keeps the point, raffclar's fix (2026-10-20): see
    [debugging](../project/debugging.md#hand-demos).
- **`GInterface::SetActive`'s other effects** (HelpSystem +0x460C, +0x40 &= ~4, fn_005D1260, `ResetActionState`,
  `StopAllImmersion`): not ported; beyond that, unknown.
- **The scoop**: ms a turn taken as 100 (0xD01A38 has no static writer); the sound's flags and pitch units and the text
  of tooltip 0xEEA are not read; the field's and the fish farm's starts placing the hand pot through 0x5DC870 is
  inferred.
