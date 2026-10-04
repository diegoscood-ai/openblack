# The hand and the interface

Full detail in `C:\Users\diewgarc\dev\decomp_pickup` (hand.cpp, interface.cpp and their NOTES).

## Animation and mesh

- Animations `Data\CTR\hh.HBN` (Lionhead pack, morph block at 0x2C). Specification `Data\hndspec5.txt`
  (69 nodes, 26 present). C nodes = central poses; L*_lr / L*_fb = directional layers −1..+1.
- Mesh `Data\CreatureMesh\Hand_Boned_Base2.l3d`: 22 bones (0 palm, 8–19 fingers, tips 10/13/16/19, index = 19,
  20/21 thumb). The model origin **is the grip point** in the holding states.
- Scale: `3.2 * handScale / 555.294`; `CHand::SetDistanceFromView` (0x46C0D0): d<10 → (d/10)^0.8; d>150 →
  ×(d/150)·(1 − 0.3·(d−150)/1650).

## Where the hand is placed: `ObtainRequiredHandPosition` (0x5B5E70)

1. Object under the cursor (see below). If there is none, the terrain.
2. With an object:
   - villagers (Living that are not the creature): distance = |centre − camera| − 2D radius, corrected for height;
   - the rest: the point where the ray intersects the mesh;
   - if you are carrying something: the search ray aims at the terrain point + 0.6·grip height, and the final point is
     moved towards the camera by half the 2D radius of the held object.
3. The distance to the camera is smoothed with the Zoomer: **0.1 s** when approaching, **0.28 s** when moving away (2.0 s when
   giving to the creature or in one particular camera mode: not implemented). Never further than the terrain under the cursor.
4. `HandStateNormal` puts the hand exactly there (with no further smoothing).

## Object under the cursor

- `GInterface::SendObjectDrawCollision` (0x5D56C0): every drawn object is tested with exact per-triangle
  collision; the closest wins; whatever is in the hand is ignored.
- **Villagers** ("human" flag of the LH3DObject, only `Villager::CallVirtualFunctionsForCreation` 0x74FC70): no
  per-triangle test. They count if the mouse is inside the **on-screen circle of their bounding sphere**
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80; radius = scale × half-diagonal of the box), at distance
  `|(x, y + semialto·escala, z) − cámara| − (R + plano cercano)`; the near plane is 0.3 + 0.16 × height of the camera
  above the terrain (0.3..3.5, `LandFeature::GetNearClipping` 0x5E2F30). Animals and creature: triangles, like everything else.
  Report: `documentacion\iface\hover_humans.md`.
- `UpdateInterfaceCollide` (0x5D5A70): the terrain distance counts 2.3 more (fn_005D5980); if it is still
  in front, the object only counts if the terrain point falls inside its XZ footprint.
- If there is nothing when clicking: `FindObjectNearMapCoord` (0x5D39E0), the closest within ±5 units and only if it is
  closer than the clicked point; first the fish of a fish farm if it is water. **It is not a hover range**:
  openblack uses only the object from the per-triangle pick (it used to have an invented radius of ≥3.5 units).
- Message for the amount in the hand ("Cantidad: N", font `j0`, yellow with shadow, no background): see
  [rendering.md](rendering.md#text-the-originals-fonts-and-the-hand-message); it is shown while the hand holds food or wood.
- Boned meshes (villagers, animals) are tested in their rest pose, which is how they are drawn
  (`L3DSubMesh`: collision positions × the group's bone chain). Animals count as Living when placing the
  hand (distance to the centre minus the 2D radius).
- **Exclude everything that moves with the hand** (held object, tree being uprooted, roots, particles): otherwise the
  ray hits it and the hand climbs towards the camera endlessly.

## Interface action states (`GInterface+0x44`, table 0x5D7960)

| State | Name in the exe | Use |
|---|---|---|
| 0 | NORMAL | idle |
| 2 | LANDSCAPE LOCK | grabbing the terrain (camera) |
| 3–6 | LOCKED SELECT… | taking from a pile in batches |
| 7 | WAIT FOR PLACE IN HAND | after taking, waits for the package |
| 12 | **IN THROW** | second press with the hand full: drop/throw on release |
| 13 | grab (225 ms) | grab press |

Each state has a "cursor state" (`GInterface+0x3AC`); IN THROW = **0x17**.

## Holding, spring and throwing (`HandStateHolding::Update` 0x5B3C70)

- Grip types (jump table 0x5B568C), base height h: ABOVE 0.2; MAGIC 3.2·scale; GRAIN/TREE/SIDE/VILLAGER
  max(lowering, 1.9); +0.1·height if the object is rooted. `lowering = GetHeight·GetHoldLoweringMultiplier`.
- Poses: ABOVE = Chold_above at dur·0.5·(1−grip), grip = min(1, R/(3.2·s·1.2)); SIDE/TREE/VILLAGER = Chold_side at
  (dur>>1)·grip, grip = min(1, R/(3.2·s)). R = GetHoldRadius (constant for wood; food opens up with the amount).
- **Spring (inertia)**: 10 ms steps, a = 260·d − 40·v, |v| ≤ 124. **Only active in the IN THROW state**
  (check `0x3AC == 0x17` at 0x5B4603). After grabbing and releasing the button, the hand follows the cursor without inertia.
- On release in IN THROW: a single path for throwing and dropping (`HandSystem::Release`): `ApplyThisToMapCoord` and then
  `Object::InitialisePhysicsFromHand` with the spring velocity, which throws if |v_xz|² > 4 and otherwise drops the object
  (a pot from the hand: |v|² ≤ 5). Details in [physics.md](physics.md#water-in-impacts-and-when-dropping).
- Dropping the object **over the sea does not place it**: `Object::InitialisePhysicsFromHand` (0x636F00) only "lands" on
  dry land (altitude ≥ 4) or on a cell with altitude > 1, so over water the object stays in physics and floats or
  sinks (see [physics.md](physics.md) and [water.md](water.md#sinking-drowning-and-being-deleted)). On land there is no "placing" either: the body drops to the ground and a villager, an
  animal, a fence or an upright tree leave physics immediately; everything else settles with physics. A pot from the hand dropped in the sea does not
  leave a pile either: the resource is lost (`Pot::AddResourceToPos` 0x66F270). A **villager** dropped there sinks in ~0.4 s and
  spends 60 s drowning (clip 252 with a scream and splashes) before dying; an animal disappears after ~7.5 s. If the water cell
  has altitude ≥ 2 the villager does "land", but `Villager::EndPhysics` sees `IsWater` and drowns too (in
  Land1 there is no water cell that high).
- Sway: up to 0.3 rad depending on the mouse smoothing (±80 px, reference width 1024). The ±π/2 sideways turn only
  happens while a delivery to the creature is pending.

## Grabbing

- 225 ms threshold between touching and grabbing. Piles cannot be touched: the press starts taking in batches
  immediately.
- Rocks with a 2D radius > 3.6 cannot be lifted (`Rock::ValidForPlaceInHand`). Pressing on them strikes them and splits them in two; see [physics.md](physics.md).
- The hand never calls `CanBePickedUp`; the gate is `GInterface::PlaceObjectInMagicHand` (0x5DA6F0).
- **Influence.** Every ported class keeps `Object::InterfaceMustBeInInfluenceForInteraction` (0x4028A0 = 1; only
  ScriptHighlight 0x709840 overrides it, not ported), so nothing is picked up, scooped or tapped with the action
  position outside the player's influence (`GInterface +0x48`, fn_005D1120: `CalculatePlayerInfluence(pos, type 1,
  allies) > 0`). The checks: `ActionPressed` fn_005D1330 before a locked select (piles, fields, fish farms);
  `StartGrab` 0x5D1740 turns an object out of the influence (or not placeable) into a tap; `GenericPickup` 0x5D2800
  checks again when the 225 ms grab completes; `SendTap` 0x5D38A0 refuses the tap. While scooping,
  `GInterfaceStatus::Process` 0x5DC558 ends the locked select when the hand leaves the influence.
- `IsCannotBePickedUp` (0x401A10, flag 0x2000) is checked by all of these too. The scripts set it with
  `SET_ID_PICKUPABLE` 169 (0x6FB450, pickupable 0 sets it) and the flag 0x1000 IMMOVABLE with `SET_ID_MOVEABLE` 168
  (0x6FB3E0); in openblack both are tag components (`src/ECS/ThingFlags.h`, `ecs::thing_flags`). **(not ported)**: the
  other setters, `GameOSFile::LoadInstance` 0x559999 and the puzzles (fn_006D71D0, HanoiBlock).

## Tapping objects

A short click (released within 225 ms), or a press on an object that cannot go into the hand, is a tap:
`Tap` 0x5D3930 → `SendTap` 0x5D38A0 → packet 0x20 → 0x5DA650 → the class's `InterfaceValidToTap` (vt 0x740, Object
0x4196B0 = 0) and `InterfaceTap` (vt 0x744). Villagers, animals and trees keep Object's defaults: they cannot be
tapped. In openblack each owner registers its classes in `ecs::hand_tap` (`src/ECS/Systems/HandTap.h`):

| Class | ValidToTap | Tap | Registered by |
|---|---|---|---|
| Rock | 0x6E7450 (height > 0.7) | 0x6E7480 (SplitInTwo, G_RockTap) | hand, through `Rocks::` (owner: physics) |
| Abode | 0x406820 (always 1) | 0x406830 (knock on the roof) | hand, through `abodes::` (owner: buildings) |
| SpellIcon | 0x7263C0 | 0x726430 | hand, through `worship::` |
| OneOffSpellSeed | 0x72A630 (1) | 0x72A640 | hand, through `worship::` |

**(not ported)**: CitadelEntrance 0x468EF0, PuzzleTotem 0x6DA610, Scaffold 0x6E9DD0, Reward 0x6E5D00,
ScriptHighlight 0x70AC70, LeashObj 0x464490, MagicFireBall 0x682E50.

## Hand demos

The tutorial's hand demos (`Data\HandDemo\*.hnd`) replay recorded interface input through the real hand, and set the
camera from each record. Full reverse engineering: `dev\documentacion\hand\handdemo\README.md`. openblack:
`src/Input/HandDemo.{h,cpp}` (`hand_demo::Play / IsPlaying / ConsumeTrigger / End / EndIfTask / Update`); the script
opcodes are wired by the intro session.

- `PLAY_HAND_DEMO(string, waitTrigger, keepHand)` 0x6FDAD0: `.\Data\HandDemo\%s.hnd`, where the name is the CHL string
  itself (challenge.chl offset 279 = "drag"). Then `GInterface::StartPlayBack` 0x5DAD60:
  - without keepHand, it drops the held object (fn_005D4350) and cancels the spells being charged;
  - it sets the game speed to 1, turns the widescreen on (`HelpSystem::SetWideScreen(1, 0)`) and snaps it (fn_005C6C40);
  - it processes the first record at once.
- `IS_PLAYING_HAND_DEMO` 0x6FDB80 pushes **not** `IsPlayBack(0)`. `HAND_DEMO_TRIGGER` 0x6FE280 reads and clears the
  pending trigger. `SET_HAND_DEMO_KEYS` 0x709540 is an empty `ret`.
- **File:** no header. Records are 124 bytes:
  - +0 the message: 0 mouse move, 1/2 grab down/up, 3/4 action down/up;
  - +0x34 the mouse (normalised);
  - +0x3C the camera eye and +0x48 its focus;
  - +0x5C the trigger (**(inferred)** the space key);
  - +0x60 the game ms (g_game +0x25053C).
- **Playback** (fn_005DAEE0, from the interface's message pump fn_005D9A20, which runs both every frame (ProcessFrameUpdates
  0x5CEDB0) and every turn (GInterface::Process 0x5CEC10); openblack: every frame only, (approximate)):
  - every record that is due is applied, in order, at the visual clock;
  - while waiting for the trigger with one pending, the time stands still;
  - message 0 moves the mouse; the camera goes through `GCamera::SetPositionAndFocus` 0x4438C0 and
    `LH3DTech::UpdateCamera` 0x819920;
  - the end of the file ends the demo.
  - The real buttons (0x54C390), the real mouse and the camera keys are blocked meanwhile.
- **(approximate)**: openblack's hand reads the button state once a frame, so a press and a release in the same frame
  would be lost.
- **(pending)**:
  - the recorded throw information (CHand +0x48C8);
  - finding the target object of an action message again within 3 m (FindNearPos 0x6F7280);
- **(not ported)**: the camera tricon flags (+0x54 / +0x58).
- **Test hook:** `OPENBLACK_TEST_HAND_DEMO=<name>` plays `Data\HandDemo\<name>.hnd` once the landscape exists.

## Tooltips

The original shows a single tooltip next to the hand, and the hand's state picks it every game turn. Research:
`dev\documentacion\hand\tooltips\README.md` and `README_part2.md`. In openblack:
- `src/Help/ToolTips.{h,cpp}` (`help::tooltips`) is the help system's part;
- `HandToolTips.cpp` holds the hand's state and its texts;
- `Renderer::DrawHandToolTip` draws it.

### The hand's state

`fn_005D7E40` works out GInterface +0x3AC from the action state (table 0xD18278) and, when the table gives nothing, from
`fn_005D7F20`:
- gripping the land: 20, whatever the hand holds;
- something in the hand: 24, or 5 for a spell seed;
- a tug: the object branch, so 9 (inferred);
- scooping: 14;
- IN THROW: 23;
- nothing under the hand: 3;
- an object under the hand: 18 out of the influence; 13 for a locked select (piles, fields); 9 for one that can be
  picked up; 18 for the rest.

`GET_HAND_STATE` 413 is this number (`HandSystemInterface::GetInterfaceHandState`).

### The text of each state (table 0xBF1C10)

| State | Original | Texts |
|---|---|---|
| 3 Normal | fn_005D6980 | over a fish in the water and in the influence 0xE73 «Recoger»; otherwise 0xE7E «Mover» (left button, four arrows) |
| 9 Can Pick up | fn_005D6D70 | a flying object 0xE80 «Atrapar»; otherwise 0xE73 «Recoger», plus 0xEF7 «Golpear para Romper» (rocks) or 0xE7A «Golpear» when it can also be tapped; a one-shot orb only its own text, forced (pending) |
| 13 Can Select Lock | fn_005D77C0 | piles 0xEFD / 0xEFE «Cantidad Comida / Madera: N»; fields 0xE73 |
| 14 Select Lock | constant | 0xE85 «Interactuar» (up and down arrows) |
| 18 Over Object | 0x5D7190 | tappable and not pickable: 0xEF7 (rocks) or 0xE7A (abodes); otherwise the land texts |
| 24 Object In Hand | fn_005D6F40 | a target that takes it: 0xE8E; otherwise 0xEEF «Plantar» (trees) or 0xEEE «Soltar», then 0xE74 «Lanzar» |

### The help system's part

- **SubmitToolTips** 0x5C9A70:
  - the texts 0xE73..0xF1C, with {priority, display, afterFocus} from info.dat;
  - a higher priority keeps the tooltip;
  - while the display timer runs (2.5 × display × 10 turns), only a forced text takes over.
  - So «Soltar» and «Lanzar» alternate every 25 turns.
- **ForceToolTips** 0x5C9C60: the amount in the hand, 0xEEA (priority 0.925).
- **Every turn** (fn_005C9D00):
  - the hand submits;
  - a text nobody submits stays afterFocus × 25 turns;
  - at TOOLTIP_LEVEL 2 a priority below 0.9 fades out in 1 s once its display time is over.
- **The icon** fades in, in real time, over as many seconds as the text has been shown (forced: at once).
- **Drawing** (CameraHelp::DrawKeyOrMouse 0x447EA0):
  - S = H/25, at the hand on screen; the text at int(2S/3);
  - black copies at (−1, −1) and (+1, +1), then yellow;
  - S/2 to the left of the hand, or ending S/2 to its right past 2/3 of the screen (hysteresis back below 1/3).

### Differences

- **(pending)**:
  - the mouse-button icon (`mousehelp.raw`), the panels and the arrows of DrawKeyOrMouse;
  - the storage pit's 0xEF9 (two numbers) and the buildings' and town's texts of state 18;
  - the seeds' 0xE81 / 0xEF1 (state 5);
  - OneOffSpellSeed's own pick-up text;
  - SpellIcon's tap text.
- **(not ported)**: the camera's tricons (0xE76 «Inclinar», 0xE77 «Rotar»), 0xE8B «Alejar», the leash, the scaffolds,
  giving to the creature.
- The amount 0xEEA is forced every turn of the scooping and once when it ends (Pile 0x66E8DA, Field 0x529AD9, FishFarm
  0x52D92A). It then stays about 13 turns, until «Soltar» takes over.
- Out of the influence a second press with something in the hand does nothing (ActionPressedHolding 0x5D16BE).

## Hito 2 (2026-10-04): placement, near object, morph, tap memory

**Where the hand goes, as the original** (dev\documentacion\hand\placement\README.md)
- **Empty hand:** HandStateNormal::Update 0x5B71A0 puts the model origin at the required position. That point is on the
  mouse ray, one hand length (3.2 × handScale) short of the land, not over the sea, clamped to [2, reach].
- **Matrix:** fn_0046E160, with the heading from the camera → mouse ray. The up is the land's normal at the hand, through
  three 0.4 s Zoomers that are snapped to (0, 1, 0) on every HandStateNormal::Enter. Nothing lifts the hand above the
  land.
- **Land grip:** HandStateCamera puts the origin at the grabbed land point, freezes the up, keeps the heading live, and
  takes the scale from the grip point.
- **Root bone:** every clip applies its root.
- **Removed:** openblack's own placement (index fingertip 0.45 above the point, palm-down frame, vertex lift, fingertips
  dug in 0.12, dead roll).
- **Held object pull-in:** 0.5 × Get2DRadius of the held object (0x5B676B). **(not ported)**: the creature's push.
- **Action point:** `_interactionPoint` is GInterface +0x3F0: the collided object's position, otherwise the land under the
  cursor. GetPlayerHandPositions returns the hand itself.

**FindObjectNearMapCoord fn_005D39E0** (dev\documentacion\hand\fonmc\README.md)
- When a click hits nothing, it takes the nearest object within ±5 of the land behind the hand, as seen from the camera.
  That point is GLandscape::Draw 0x5E4848, the box of the bones without the root.
- The search goes through the cells, skips fragments, and keeps the nearest one if it is not farther than the action
  point. A fish shoal is tried first.
- **(approximate)**: the press path filters the result with the hover's class filter.

**Grip dust:** SPOT_VISUAL 2 through psys::manager::CreateSpotVisual, made by packet 0x2B → 0x63D6D8.
**(not ported)**: the packet's one-turn delay.

**Good and evil** (dev\documentacion\hand\morph\README.md, HandMorph.cpp)
- CHand::PrepareForDrawing 0x46C550 morphs the hand when the local player's alignment moves 0.03 or more.
- **Texture:** Blend4444 fn_00870640 between Base2 and Evil2 / Good2, at t = min(255, trunc(|a|·256)).
- **Vertices:** MorphVertices 0x618D10. Evil2 grows claws.
- MorphAnims changes nothing for the hand.
- Test hook: `OPENBLACK_TEST_HAND_ALIGNMENT=<-1..1>`.

**Reach:** CHand +0x4838 (1800; at most 1800, fn_0046BF20) is set by SET_INTERFACE_INTERACTION
(HandSystemInterface::SetHandReach).

**Tap memory** (dev\documentacion\hand\clicked\README.md)
- **The slot:** GInterface +0x45C (the object) and +0x46C (the land point). It is written by RememberTapped fn_005D36D0,
  from Tap and from fn_005D3700 when the action button is released idle, and it lasts 15 s of game time.
- **Readers:** GAME_THING_CLICKED, CLEAR_CLICKED_OBJECT, CLEAR_CLICKED_POSITION and POSITION_CLICKED (fn_005D0460)
  (session Intro wires them).
- **(approximate)**: the per-tick check uses last frame's hover.

**Store:** Object::DoDeleteObjectAndTakeResource 0x63A940 is in ecs::object_delivery (ObjectDelivery.h).
DepositInStore passes the giver's interface. **(pending)**: until session Edificios' take_resource lands, the store's
Supply trigger and reaction 0x16.
