# Grabbing, holding and throwing

## Summary

A press on an object either taps it (a short click, or an object that cannot go into the hand) or grabs it: after
225 ms it goes into the hand by a pick-up packet, applied at the next turn. A standing tree is tugged from the press:
the hand pulls it with a spring until it comes out of the ground. Held, the object hangs from the hand, which follows
the cursor; a second press arms the throw, and while it is armed a spring drags the hand, so the release throws
with the hand's speed or drops the object when it is slow. A held object pressed on another (a tree on a store, a
villager on a totem) is applied to it; one pressed on the creature is given to it. A tree put down carefully and
upright is replanted.

openblack ports the grab and its timers, the tap table, the tug (the original's rule, chosen by the user on
2026-10-16), the holding spring and its start, the throw from the drawn pose, the locked apply's unlock packet, the
apply to an object, the release's building-site branch, BigForest and the replant's order. Left: the tug state of
what is not a tree, the put-down particles and a few draw details.

## The original

The hand works in three layers: the interface's action states (`GInterface`, the press, the grab timer, the packets),
the hand's own states (`CHand` and its `HandState*` classes, which place and pose the hand every frame) and the
object's class functions (whether it can be held, tapped or applied, and what it does then). Packets sent by the
interface are applied at the start of the next turn.

### Grabbing

#### What the hand can take

The gate is `GInterface::PlaceObjectInMagicHand` (0x5DA6F0); the hand never calls `CanBePickedUp`. It checks, in
order:

1. IsInteractable (0x5DA705). `GameThingWithPos::IsInteractable` vt 0x190 (0x5701B0) = IsAvailable (GameThing
   0x401810: !(+0xA & 1)); `Abode::IsInteractable` 0x407200 also needs GetPercentBuilt != 0.
2. IsSpaceInHands (0x5DA719).
3. ValidForPlaceInHand (vt 0x6FC, 0x5DA73B).
4. IsCannotBePickedUp (0x5DA74E).
5. Not carried by a particle system (GameThing +0xA & 0x10, 0x5DA766).

What the classes answer:

- Rocks with a 2D radius > 3.6 cannot be lifted (`Rock::ValidForPlaceInHand`). Pressing on them strikes them and
  splits them in two; see [the physics](../physics/physics.md).
- Piles cannot be touched: the press starts taking in batches immediately
  ([The scoop](states.md#the-scoops-hand-and-the-multi-pickup-state-that-never-runs-hand-state-6)).
- For pots and piles the gate's answers (a loose pot lifted whole, a pile only when it is the hand's row) are on
  [Pots and piles](../world/objects.md#pots-and-piles).
- `IsCannotBePickedUp` (0x401A10, flag 0x2000) is checked by all the hand's checks below too. The scripts set it with
  `SET_ID_PICKUPABLE` 169 (0x6FB450, pickupable 0 sets it) and the flag 0x1000 IMMOVABLE with `SET_ID_MOVEABLE` 168
  (0x6FB3E0). Its other setters are `GameOSFile::LoadInstance` 0x559999 and the puzzles (fn_006D71D0, HanoiBlock).

#### The player's influence

Every holdable class keeps `Object::InterfaceMustBeInInfluenceForInteraction` (0x4028A0 = 1) but ScriptHighlight,
whose 0x709840 is `xor eax, eax; ret` (0): a «Did you know?» sign or a scroll is tapped anywhere. Otherwise nothing is
picked up, scooped or tapped with the action position outside the player's influence (`GInterface +0x48`,
fn_005D1120: `CalculatePlayerInfluence(pos, type 1, allies) > 0`). The checks:

- `ActionPressed` fn_005D1330, before a locked select (piles, fields, fish farms);
- `StartGrab` 0x5D1740 turns an object out of the influence (or not placeable) into a tap;
- `GenericPickup` 0x5D2800 checks again when the 225 ms grab completes (anything held that long, a one-shot orb
  among them, `OneOffSpellSeed::InterfaceSetInMagicHand` 0x72A530, is picked up itself);
- `SendTap` 0x5D38A0 refuses the tap;
- while scooping, `GInterfaceStatus::Process` 0x5DC558 ends the locked select when the hand leaves the influence.

#### Grab press

`StartGrab` 0x5D1740 starts the grab and `State_Grab` 0x5D5250 runs it.

**The press.** StartGrab runs on the press's own frame (ProcessFrameInputs 0x54C3B6 → fn_005D9A20 → fn_005D1120). It
stores +0x454 = the frame's engine sample [0xEA9EB0], the real-time ms clock, and +0x458 = the turn (g_game +0x205A40,
0x5D174D..0x5D1769), sets action state 13 and starts the immersion 4 (grip force). An object not pickable, out of the
influence or with Flags & 0x20 is tapped instead (0x5D3930, packet 0x20).

**Free objects and the timer.** The object is free when +0x24 & 0x40 IN_PHYSICS && !(+0xA & 0x10) (0x5D529F..0x5D52B2;
+0xA & 0x10 = carried by a particle system). A free object takes the 225 ms timer path, as a forest does (+0x450,
0x5D17F4 / 0x5D17FE). 225 ms is `0xE1`, the threshold between touching and grabbing; it is also the IN_PHYSICS catch,
and a locked select uses `> 225`.

**Held objects and the tug hold.** Otherwise fn_0046DC30 (StartGrab 0x5D1813, `CHand::PickUp(tree, needsTug 1)`) holds
the object at once. `CHand::PickUp` 0x46DC30 sets the tug hold CHand +0x4908 = 1 (0x46DCF3..0x46DD10) for anything
picked up with needsTug that is not a spell seed (vt +0x4C4, 0x46DC6C; needsTug is forced 0 for a seed, 0x46DC76) and
not free in physics. A standing tree is tugged; one in physics (thrown, not landed) is caught like any flying object
unless a particle system carries it (0x46DCF9 / 0x5D52A5). Held with +0x4908, `GetRequiredState` gives TUG 3
(0x46CDD7), so a rock or a villager passes through the tug state too, for the tug hold's first frames.

**How the tug hold ends.** HandStateTug::Update 0x5B8070 runs for the +0x4908 hold. The first draw after the press
zeroes +0x49B0 (fn_0046C500, 0x46C768) with firstFrame set; the gate 0x5B8096 clears firstFrame once +0x49B0 >= 0.13
[0x900C08], and the Update after clears +0x4908 (0x5B8409) when the held object's vt +0x338 (`IsTree`) returns 0. The
tug blend does not advance while the hand is hidden, and a tuggable object without a Game3DObject (+0x40 == 0,
0x46DC58) is handled apart. For a tree the tug goes on: see [Tug](#tug).

**Every frame and every turn.** State_Grab measures elapsed = min(sample − +0x454, (turn + 1)·100 − +0x458·100)
(0x5D5267..0x5D5293): the real ms capped by 100 ms a turn. Then:

- First the pick-up: on the timer path at elapsed >= 0xE1 (0x5D52E4), else once CHand +0x4908 is 0 (fn_0046E480,
  0x5D533D); GenericPickup sends the forest's 0x13 at 0x5D5349. `GenericPickup` 0x5D2800 checks again
  `ValidForPlaceInHand`, `!IsCannotBePickedUp` and the influence, then `SetPacket(0x13, object)` (0x5D2864), state 7
  (0x5D286D), the pick-up sound, `StopImmersion(4)` and, unless the object `IsLiving`, `StartImmersion(5 UPROOT)`.
- Then the release (m_Buttons 0x4000, 0x5D52EC..0x5D5329, every object): StopImmersion(4), CHand::ThrowObject when it
  holds it (0x5D530B), and a Tap only if elapsed <= 0xE1 (0x5D5310).

#### The object entering the hand

After the pick-up packet, `GInterface::PlaceObjectInMagicHand` 0x5DA6F0 runs its gate (above), then calls
InterfaceSetInMagicHand (vt 0x700, 0x5DA77C).

- A class that returns 0 has put another object in the hand through
  `GInterfaceStatus::PlaceObjectInMagicHand` 0x5DC870 → 0x5DA6F0 again (BigForest 0x439421, MagicTeleport 0x5FC489: GenericPickup's 0x13 carries the stone
  itself, and `MagicTeleport::InterfaceSetInMagicHand` 0x5FC470 finds its seed), and the outer call only ends the
  action (0x5DA859). A refusal or a NULL object: EndAction (0x5DA88F).
- Locally the hand then takes the first object in the hand with `CHand::SetHeldObject` (0x46E590).
- fn_005DC330 (`GInterfaceStatus::fn_005DC330`, the `GMagicHand` add) takes the object out of the map cells
  (IsObjectInMap vt +0x178 at 0x5DC377, RemoveMapObject vt +0x548 at 0x5DC385) and stores it at +0x120 (0x5DC3EC);
  then MyInterfaceStatus's (fn_00555880) gesture buffer is cleared (0x5DC40C..0x5DC434).
- **A failed put-in-hand takes nothing out of the map.** PlaceObjectInMagicHand's failure path (fn_005DC330 != 1 at
  0x5DA7C9 → 0x5DA833) ends the action (fn_005D1260), then, while the object is available (vt 0x2C,
  GameThing::IsAvailable), files it in the map (vt 0x544, Fixed::InsertMapObject is Fixed+0x544, at 0x5DA849).
- **An object that hangs cannot be picked up again.** fn_005DC330 fails for an object that is IN_PHYSICS with no
  physics entry: it calls `RemoveObject(obj, 0, 0)` (0x5DC346) and returns 0 when that finds none (0x5DC35A). A thing a
  script made immovable and a hand let go of hangs in exactly that state
  ([An immovable thing let go hangs](../physics/physics.md)), so each pick goes back into the map, still IN_PHYSICS and
  in the air. The pick sound (`GenericPickupSounds`, 0x5D2899; 0xA, a villager's scream 0xB4 / 0xC2 / 0xBB) and the
  "Catch" tooltip (0x5D6DA3) read IN_PHYSICS.
- **The help event.** PickUp's help event (0x5DA7F4..0x5DA808) tests +0x24 & 0x40 IN_PHYSICS and altitude >= 0 after
  fn_005DC330's RemoveObject has cleared IN_PHYSICS (0x646B56), so the event is always PickUp 2 (0x5DA82C), never
  Catch 3 (0x5DA817). After the pick-up the reaction 0x10 is offered.

**Leaving the hand when no longer interactable.** `GInterfaceStatus::ValidateHands` 0x5DC610 →
`GMagicHand::Validate` 0x5FB130: a held object no longer IsInteractable leaves the hand with no physics (RemoveFromHand 0x5FB0B0), then
`TidyHands` 0x5DC6F0 and, for the local interface, CHand::ThrowObject (0x5DC68C).

#### Tooltips

"Pick up" is fn_005D6D70 (BINDABLE_ACTION 2 with text 0xE73, or the object's `GetOverwritePickUpToolTip`); "Tap" is
0xE7A with help event 8; a flying object shows 0xE80 with event 3. fn_005D6D70 (size 0x1C5, the table pointer
0xBF1CE8) is the tooltip over any object, ending in `ToolTips::SubmitToolTips`.

### Holding, spring and throwing

`HandStateHolding::Update` 0x5B3C70 places, poses and draws the hand while it holds. Its "ObtainRequiredHandPosition"
label is swapped in the symbols: the hand's mesh test `HandShouldFeelWithMeshIntersect` vt 0x59C is used only here,
at 0x5B6848; `GetHoldRadius` vt 0x590 only in the hand states (the required position, the multi pick-up, the totem
and the tug), never by the interface.

#### Where the object hangs

fn_0046DC30 (0x46DC3E) sets +0x4904 = the object; its hold type / lowering (+0x490C / +0x4944) come from it; only an
object with a G3D (obj +0x40, 0x46DC9A) gets the need-sorting flag. fn_0046DE40 swaps objects without ThrowObject: it
neither writes +0x48FC (lastReleased) nor calls GetWorldMatrix (0x46DE57); the new object's flags are set at 0x46DEA1.

The grip types (jump table 0x5B568C) give the base height h: ABOVE 0.2; MAGIC 3.2·scale; GRAIN/TREE/SIDE/VILLAGER
max(lowering, 1.9); +0.1·height if the object is rooted. `lowering = GetHeight·GetHoldLoweringMultiplier`. The height
terms are added to the *target* position before the spring, so they move the hand itself.

#### The hand's poses

The pose's clip and time come from the hold type and the grip. R = GetHoldRadius (constant for wood; food opens up
with the amount; a tree's is 0.2 × `Get2DRadius`, vt 0x64); s is the hand's scale; 3.2 is fn_0046C040, which returns
the constant 0x8C7C8C, f32 0x404CCCCD = 3.2000000477 by the oracle (`CameraModeNew3::WantToQuitFight` 0x45A41D and
`LandFeature::GetNearClipping` 0x5E2FDA read it too).

| Hold type | Holding (0x5B4EA5..0x5B50A8) | Tug (`HandStateTug::Update` 0x5B89A3..0x5B8C14, jump table 0x5B8E20) |
|---|---|---|
| ABOVE | Chold_above at dur·0.5·(1−grip), grip = min(1, R/(3.2·s·1.2)) | Chold_above (anim 6) at ftol(clip × 0.5 × (1 − grip)), grip = min(1, (`GetHoldRadius` × 1.2) ÷ (3.2 × s × 1.2)) |
| MAGIC | `dur sar 1` | anim 0 at clip >> 1 |
| GRAIN | | anim 60 at ftol((clip >> 1) × grip), grip = min(1, R ÷ (3.2 × s)) |
| FINGERS | | writes no frame |
| SIDE, TREE, VILLAGER | Chold_side at (dur>>1)·grip, grip = min(1, R/(3.2·s)) | Chold_side (anim 9) at ftol((clip >> 1) × grip) |

In the tug's ABOVE case fn_0046C040 is called at 0x5B89A3 and 1.2 is [0x8C6C98] on both sides (0x5B89E4..0x5B89FD),
so the hand is fully closed at a reach of its own height, where HOLDING's grip divides by 3.2 × s × 1.2 alone. The
tug's pose is written every Update, the first frames included.

**The pose's frame is whole milliseconds.** Each pose time is worked on the x87 (24-bit precision, so in floats) and
cut by `ftol` (`cvttsd2si`, towards zero) into the state's int frame (+0x8):

- Holding: ABOVE `ftol(fild(dur)·0.5·(1−grip))` 0x5B508E..0x5B50A8; MAGIC `dur sar 1`, already whole,
  0x5B512F..0x5B5134; the two side-pose cases `ftol(fild(dur sar 1)·grip)` 0x5B531F..0x5B5335 and 0x5B5520..0x5B5536.
- `HandStateTug::Update` 0x5B8070 takes the same times into its +0x8: ABOVE (clip 6) 0x5B8A64..0x5B8A7D, MAGIC
  (clip 0) `dur sar 1` 0x5B8ACB..0x5B8ACF, and the side poses (clips 0x3C and 9) 0x5B8B5B..0x5B8B70 and
  0x5B8BFC..0x5B8C11.
- The totem's grip (0x5B7B95..0x5B7BB7, `ftol(fild(dur sar 1)·(closure·0.5))`) and the multi pick-up's
  (0x5B5AFE..0x5B5B21, the same form) are cut the same way.

The frame goes to fn_00860E00 (0x5B5637, 0x5B8DCF) as its fifth argument, an int: the key is `frames·time / length`
by integer division, clamped to the last frame, and the blend to the next one
`fild(frames)/fild(length)·fimul(time) − key` (0x860E2A..0x860E8E), so it is milliseconds and only whole ones reach
it.

#### The spring

- **Spring (inertia)**: 10 ms steps, a = 260·d − 40·v, |v| ≤ 124. The constants are writable `.data`: 260 at
  0xBF0198, 40 at 0xBF019C and 124.0f at 0xC371FC, which is also the throw's speed cap.
- **Only active in the IN THROW state** (check `0x3AC == 0x17` at 0x5B4603). After grabbing and releasing the button,
  the hand follows the cursor without inertia.
- **What the spring drags is the hand** (CHand +0x78, 0x5B457E..0x5B45C5), not the held object: the object's 3D matrix
  is then put at the hand plus its hang (`g3d.pos = hand.pos + offset`, the hold-type cases from 0x5B4EA5), so the
  object always hangs from the hand wherever the spring has taken it.
- **When it starts and stops** (0x5B45DE..0x5B4655): with the spring off the hand is put at the required position;
  then, in IN THROW and not holding a seed, the flag +0x104 is set, the hand put at the required position again and the
  velocity zeroed, with no step that frame: it steps from the next frame. Nothing in the update clears +0x104 when the
  interface leaves IN THROW: the spring stays on through the release until the object leaves the hand (only the
  state's Enter and a refused release clear it). +0x48C8 = the spring's velocity and h = 0 every frame the flag is set,
  the starting frame included (0x5B46B4..0x5B46E8).

#### The held object's matrix and sway

**The matrix.** The held object's matrix fn_0046E2F0 (0x5B4B9D) has rows X = norm(d × up'), Y = up', Z = X × up', the
third row negated when CHand +0x484C == 0 (0x46E3B7..0x46E453); d = (−sin H, 0, cos H). It shares its prologue with
the hand's matrix fn_0046E160: k = 3.2 / [+0x8C] × the hand scale (+0x4834) × Size1 (+0x90), Size2 (+0x94) = k
(fn_0046D080), S = diag(k, k, k) with S[0][0] = −k for the left hand (+0x484C), a = the heading (+0x84) + π,
d = (sin a, 0, −cos a), side = normalize(d × up). The hand's rows are {side, side × up, −up}, the object's
{up × side, up, ±side}, each S·N with the translation the hand's position (+0x78); fn_0046E160 also keeps the hand's
position at g_C5E680 for the 0.13 s state blend (g_C5E660). The held object's matrix is a reflection for both hands
(det −1), which a tree does not show; HandStateHolding then normalises each row to the object's scale. Then
`GetHoldYRotate` (vt 0x598; Object 0x4025A0 = 0, so no turn) is applied on rows 0 and 2 (0x5B4BB3..).

**The heading.** H = atan2(ray.x, −ray.z) (0x7FAA50 → 0x7FA990) of the camera-to-mouse-point ray (fn_0074CBD0),
updated by `CHand::PrepareForDrawing` each frame only when |ray.x| or |ray.z| is above 0.01. The hand is right-handed
by default (the constructor sets +0x484C = 0; `ToggleLeftRight` flips it).

**The hang.** The held object hangs at the hand's position − lowering × up' (in the hand's model space
(0, 0, +lowering / k), since its Z is −up'); a tree's lowering is `GetHeight` × 0.1
(`Tree::GetHoldLoweringMultiplier` 0x74B620; `GetHeight` = `Object::GetHeight` 0x638120 = 2 × the mesh's half extent
y (+0x7C → +0x28) × the object's scale +0x50, from `LH3DMesh::ComputeBoundingBox` 0x8081B0), placed through
g_D13F20 = Rot·(0, −1, 0)·lowering. So the grip is 10 % of the tree's scaled height above its origin, along the tilted
trunk.

**The hand's size.** k = 3.2 × the hand scale × Size1 / f8c, f8c the bind height of `Hand_Boned_Base2` (`LoadBase`'s
`SetTransform` return, max(0, maxY) − min(0, minY) over the bind joints of meshes[1]) = 555.294, so k = 0.0057627 ×
the hand scale × Size1; Size1 is 1.0 by default (`MorphInit`; `SetSize` clamps 0.05..2). `SetDistanceFromView(d)`
0x46C0D0 runs every Holding frame with d = |camera − hand| (0x5B499B is the frame's last `SetDistanceFromView`): d clamped to
2..`MaxDistanceFromUser` (+0x4838 = 1800) into +0x4854; the hand scale = (d / 10)^0.8 below 10, else 1; above 150 ×
(d / 150) × (1 − 0.3 × (d − 150) / 1650). So the hand shrinks very close, keeps its world size (3.2 from wrist to tip)
from 10 to 150, and above grows about as its distance, its screen size falling to 70 % at 1800.

**The pose's bones.** `CAnim` fn_00860E00(out, mesh, A, B, frame, c, remap 0, fill 1), ret 0x20:

- A = +0x47F4 the bind world matrices, B = +0x47F8 their inverses (only the 3×3), c = +0x47FC the clip Cwiggle's key 0
  (set in `SelectMesh` 0x6177C8 and `MorphAnims` 0x6194ED; A and B in `LoadBase` 0x618562..0x61863E).
- div = loop ? dur : dur × FC / (FC − 1); i0 = clamp(FC × frame / div, 0, FC − 1), i1 = i0 + 1 (0 at FC),
  t = FC / div × frame − i0.
- A keyed bone's rotation is the row-normalised (0x7FB5C0) lerp of the two keys' YXZ matrices (`SetYXZMatrixOnly`
  0x7FAC10), an unkeyed one c's; out.rot = A·R·(B[parent] unless the root), out.pos = the keyed translation's lerp or
  c's. The root is not kept at its bind pose: it takes its animated rotation and translation.
- Then 0x839F10 multiplies each bone by its parent's world matrix (the root by the hand's matrix), and
  `PrepareForDrawing` (0x46CAC2..0x46CB3D) sets the hand object's matrix to the root bone's, its +0x44 to k and +0x80
  (and g_C37D9C) to the bone matrices; for 0.13 s after a state change the bones are lerped from the previous state's
  (+0x49AC, 0x46C949).
- The drawn mesh is meshes[0], a morph of `hand_boned_base2` towards `hand_boned_evil2` / `good2` by alignment
  (`GetSetAnim` 0x619690, weights +0xA0 / +0xA8: 🔍 inferred equal to the base clip at neutral).
- Chold_side (node 9): duration 533, looping, 3 keys; every bone's rotation keyed but bone 1, only the root's
  translation; the root at (x 0.3756, y −1.3391, z 0.0064) on all keys, T0 (68.9, −272.7, 177.1) then
  (85.0, −477.6, 197.1); the frame is ftol(266 × grip), so a grip below 0.668 blends keys 0 → 1 and above 1 → 2
  (grip 1 gives t = 0.497). At grip 0 the fingers close round the hand's position; a larger grip opens the hand and
  backs it off towards the camera and to the left, the palm about 1.0–1.1 × the hand scale below and 1.6–2.75 ×
  towards the viewer.

**The sway.** The object sways up to 0.3 rad depending on the mouse smoothing (±80 px, reference width 1024). The ±π/2
sideways turn only happens while a delivery to the creature is pending. The sway's up' is worked in
ObtainRequiredHandPosition 0x5B4860..0x5B4AE5, for HOLDING and GRAIN:

1. tiltX = clamp(+0x48B8 − +0x485C, −80, 80)·(0.3 / 80) (0x5B486C..0x5B48BE); D = normalize(camera − hand)
   (GetCameraPosition 0x5B480E minus CHand +0x78, 0x5B48C4..0x5B4994; 0 when the two meet).
2. R1 = fn_007FB180(D, extraRoll + roll + tiltX) (0x5B49A0..0x5B49C8): extraRoll is the state's vt 0x14 (Grain's
   tilt, 0 for Holding), roll the Zoomer +0xD4 (0 unless a creature takes the object, 0x5B4251..0x5B42DA).
3. tiltY = clamp(+0x4860 − +0x48BC, −80, 80)·(0.3 / 80) (0x5B49D0..0x5B4A2A); R2 = fn_007FB180(normalize(−D.z, 0,
   D.x), tiltY) (0x5B4A30..0x5B4AD5), the axis left as it is when zero.
4. M = R1·R2 (fn_007FAFF0, 0x5B4AE5) and up' = (0, 1, 0)·M (0x5B4AEA..0x5B4B53); with D all zero
   (0x5B4902..0x5B4936) up' = (0, cos a·cos b, 0), not unit (fn_007FB180 has no zero guard).

#### The angular velocity and the grain's smoothing

The angular velocity h (CHand +0x48D4) is read by state 12 (0x5D4EB3), SendApplyToObject 0x5D30D0 and 0x5D3340. It is
0 at construction (0x46BACF), 0 every frame while the Holding spring works (0x5B46DA), and written by
HandStateGrain::Update in state 8 (0x5B36BF). In `HandStateHolding::Update` 0x5B46B4..0x5B46E8 h = 0 only while the
spring works; otherwise h keeps its last value (0x5B46B4 `je 0x5B46EA`).

The Holding update also writes +0x48E0 = the drawn position + 0.2 × the spring's velocity and +0x48EC the drawn
matrix's YXZ angles (0x5B5553..0x5B55BD); the block +0x48C8..+0x48F7 is what the recorder saves and what
SendApplyToMapCoord's block path sends as it is (0x5D3509..0x5D35BA).

The grain state:

- `HandStateGrain::Enter` 0x5B3080..0x5B3172: the displacement, the smoothed velocity and the smoothed sideways
  acceleration all 0.
- `HandStateGrain::Update` 0x5B36BF..0x5B36D9 (state 8), each frame with CHand's dt: v_s smooths the hand's per-frame
  velocity with k = −10·ln(0.2), a_s the sideways displacement over dt² with k = −10·ln(0.8); +0x48C8 = v_s (no 124
  cap), +0x48D4 = (0, −(a_s / |v_s|), 0) when |v_s| > 0.0001.
- The old and new hand positions of that step are taken at 0x5B3317 and 0x5B33A0; the exps are the x87 sequence at 24
  bits (0x5B3452..0x5B348B, 0x5B3561..0x5B3583); k is rounded to float once (0x5B30F1 / 0x5B310C).
- The acos branch behind [0xD13F1C] is never written: off.

#### Drawing the held object

The held object is drawn by CHand's state (Tug `DrawTheHeldObject` 0x5B8E40, Holding 0x5B56D0:
held->DrawOutOfMap(0), vt 0x614) where the state put its G3D, also while the 0x13 still waits and the object is still
in the map; its logic (Transform, map cells) stays where it is.

`CHand::DrawTheHeldObject` 0x46DF00 calls the state's draw only when no box has the mouse ([0xC4CCEE] == 0), the
interface is active (vt +0x40C) and the state is TUG 3, HOLDING 4, MULTI_PICK_UP 6 or GRAIN 8 (the jump table
0x46DF48: 5 and 7 skip); it never reads the hidden flag +0x483C. So in INVISIBLE, NORMAL, CREATURE or CITADEL the held
object is not drawn.

`GetCameraTimeInc` 0x555820 has a playback branch: g_game_time_inc while a hand demo plays (interface +0x45E8 /
+0x45EC).

#### The release

**Where a held object goes on a press.** `ValidToApplyThisToMapCoord` (vt 0x724) is 1 for every holdable class but the
seed (Mobile 0x416F50, MobileStatic 0x4396C0, SingleMapFixed 0x52EB00). `ApplyOnlyAfterReleased` is 1 for Mobile
(0x416F60), MobileStatic (0x4396B0) and SingleMapFixed (0x52EB20), so for every physical object, trees included; only
the base Object returns 0 (0x402930). So a normal held object always goes press → state 12 → release → 0x4D + 0x12;
the drop at once (0x5D1850 directly, which would reuse the last 0x4D data) is only for plain Object types.
`ApplyOnlyAfterRecSystem` is 1 only for a SpellSeed whose info +0x110 is 1.

**State 12.** `State_ReleaseHeld` 0x5D4DB0 runs on the action's release. It tests the action's MapCoords (+0x3F0)
InBounds (0x5D4DF1), an object in the hand, its vt 0x724 (0x5D4E16) and the influence (+0x48, 0x5D4E24): then it sends
the packet 0x4D with the throw (0x5D513D); nothing without a held object (0x5D4E5F).

**The throw's data.**

- The throw's velocity is CHand +0x48C8 (the hand's, |v| ≤ 124).
- **Where a throw leaves from** (0x5D4E88..0x5D4EAD): the prediction body is `PhysOb::Initialise` on the held
  object's 3D object (obj +0x40, 0x5D4E93) and `SetUpPhysOb`, whose `SetUpPos` (0x7FC76B..0x7FC779) copies that 3D
  object's matrix (+0x14, 12 dwords): the matrix the Holding state drew it with this frame. The turn place (MapCoords)
  is not read: a throw leaves from where the object hangs from the hand.
- The prediction body starts with the same speed cap and `AdjustToGroundLevel`, and is simulated min(ping / 100, 5)
  steps (the lag's compensation; 0x644F20 clamps to 15). The turn count min([0xD44454] / 100, 5) uses an unsigned
  division and a signed min (0x5D5061..0x5D507B). Its angular velocity is the inertia tensor × the hand's angular
  velocity, as an LHMatrix by a point (🔍 inferred).
- The 0x4D's 0x30 bytes: the velocity (+0x44), the angular momentum (+0x50), the position (+0x5C) and the GetYXZ
  angles (+0x68); GetYXZ at 0x5D512B stores y → +0x28, x → +0x24, z → +0x2C. They go to the status's +0x44..+0x73.

**The apply packet.** Both paths go on to DropOnMapCoord 0x5D1850 (an object in the hand, vt 0x724, not
ValidForLockedApplyProcess: ResetActionState) → `SendApplyToMapCoord` 0x5D3340. SendApplyToMapCoord does nothing while
an apply packet waits (+0x444, 0x5D337C); the same three tests (0x5D3415..0x5D3445) give packet 0x12 (0x5D362D) with
the point, and fn_005D36A0, or `FailApply` fn_005D18F0 (0x5D365B): the fail spot (packet 0x2B) and sound, the object
stays in the hand.

**The apply's result.** ✅ `ApplyThisToMapCoord` (vt 0x728) results: Mobile 0x606BF0 and SingleMapFixed 0x52F420
return 0x16 after ThrowObjectFromHand. Four classes first look for a building site at **the object's own place**
(`MapCoords::IsWithinBuilingSite` 0x605250 on its MapCoords +0x14, not the release point; only these four call it,
xref): when there is one, it stores the object's resource (the site's vt 0x680 with the object's `GetResourceType` vt
0x690) and its `DeleteObjectAndTakeResource(object, status)` (vt 0x684) answers non-zero, the result is 3, with no
throw:
- `Tree::ApplyThisToMapCoord` 0x74BFD0 (Tree and MagicTree, by vtable slot 0x728): 3 at 0x74C014, else
  SingleMapFixed's throw (0x74C02A).
- `DeadTree::ApplyThisToMapCoord` 0x511050 (DeadTree and FelledTree): 3 at 0x511094, else MobileStatic's (0x5110AA).
- `MobileStatic::ApplyThisToMapCoord` 0x608B30 (MobileStatic, Rock, Bonfire, Fragment, MagicTeleport and the four
  LandscapeVortex classes) looks only for a fence: `IsFence` (vt 0x3CC; `MobileStatic::IsFence` 0x609110 is the info's
  `GetMesh`, vt 0x2C, equal to 0x38 BuildingAmericanFence or 0x51..0x52 the two Celtic fences); 3 at 0x608B80, else
  `ThrowObjectFromHand(status, 0)` (vt 0x758) and 0x16. Tree and MagicTree have `GameThingWithPos::IsFence`.
- `Pot::ApplyThisToMapCoord`, below.

The resource asked is wood for all but the pots: `GetResourceType` is 1 for a tree (0x74B820), a dead tree (0x5110C0)
and the rock classes (0x6E70C0), and MobileStatic's (0x6096B0) is 1 for a fence, -1 otherwise. Nothing else is done
on this path: no reaction, sound, deed, help event or indestructible test of its own (the press's MobileStatic apply
has one); what happens is the take's, `Object::DoDeleteObjectAndTakeResource` 0x63A940 (the resource added, the
creature's mimic, the drop remark for the local interface, the mulch sound for wood that is not a pot, the ghost, the
object deleted). A take that answers 0 would fall through to the throw with the object still in the hand; it does not
happen: StoragePit's 0x733750, Workshop's 0x779F20 and WorshipSite's 0x77E7B0 always answer 1, and MultiMapFixed's
0x52F460 answers 1 with a building site, which its IsResourceStore asks for.

A working storage pit is built, so a release never gives it anything: a store takes a held object on the press
(below) or when the thrown body hits it. `Pot::ApplyThisToMapCoord` (0x66DE70, every pot class's: Pot, PotStructure, PileResource,
PileWood, PileFood, MagicWood, MagicFood, PuzzleGrain) first looks for a building site at **the pot's own place**
(`IsWithinBuilingSite` on its MapCoords +0x14, not the release point): the site found stores the pot's resource
(vt 0x680 with the pot's `GetResourceType` vt 0x690) and takes it (vt 0x684 `DeleteObjectAndTakeResource(pot, status)`,
0x66DEA8), then it returns 3 (0x66DEB4): no reaction, no throw. `MultiMapFixed::IsResourceStore` 0x52F1F0 is wood
(type 1) with a building site (+0x74) only, so a handful of food is never taken by a building going up; a storage pit
(`StoragePit::IsResourceStore`), a workshop (`Workshop::IsResourceStore`) or a worship site
(`WorshipSite::IsResourceStore`) answers for itself (a scan of every vtable's 0x680).
`MultiMapFixed::DeleteObjectAndTakeResource` 0x52F460 is `Object::DoDeleteObjectAndTakeResource` (the press's give:
the resource added, the deed, the drop remark, the ghost, the pot deleted) and 1. `IsWithinBuilingSite` 0x605250
walks the whole fixed list of the point's cell (`GetFirstObjectFixed`, fn_00605120), keeping **the last** object that
casts to a MultiMapFixed (vt 0xB4), is not both built (vt 0x890 = 1) and repaired (vt 0x88C ≠ 0), and whose collide
data (vt 0x858) meets a 0.3 m sphere (0x3E99999A) at the point, or that has none. Otherwise (0x66DEBD) a pot put down
offers its food reaction again, once (the hungry grazers come to eat), and it is thrown as a Mobile (0x16).

**Out of the hand.** `Object::ThrowObjectFromHand(status, dont_replant)` 0x6385E0 builds the matrix from the status's
HandAngles (+0x68) and HandPos (+0x5C), the object's scale kept, and calls `HelpProfile::Trigger(4 Throw)` for the
local player (0x638622). It runs with the object still in the hand (no RemoveFromHand) and returns 0x16 (0x63871A);
the caller's HandleApplyResult fn_005DA100 takes it out (RemoveFirstFromHand fn_005CED60: `GMagicHand::RemoveFromHand`
0x5FB0B0, `FireEffect::SetOutMagicHand`, CHand::ThrowObject 0x46DDD0) and keeps it as the last dropped object.
`GMagicHand::RemoveFromHand` 0x5FB0B0 does +0x24 &= ~4, `InterfaceSetOutMagicHand` (vt 0x704) and
`FireEffect::SetOutMagicHand`.

**The last dropped object.** ✅ Each interface status keeps one object, `LastDroppedObject` (+0x124). Two writers set
it, both on any interface's status:
- `Object::InitialisePhysicsFromHand` 0x636F00, for a throw with a status and no thrower (0x636F37..0x636F4B; a
  creature's throw passes itself as the thrower and sets nothing): status +0x124 = the object, whatever another status
  keeps. Every class's override reaches it (Living 0x5EFDCC, from Villager 0x5EFEAF; Pot 0x66DFC3, after the put-down
  of a slow pot, which returns before it; Scaffold 0x6EA8CD).
- fn_005DC1E0 (the hand's drop, for results 0x16 and 0x17 through fn_005CED60, and 0x18 straight from
  `GInterface::ApplyChangesToHand` 0x5DA185), after the object left the status's hand list: the status that
  `Object::GetInterfaceStatusWhoLastDroppedMe` 0x63A710 finds keeping it gets +0x124 = 0 (0x5DC27E), then this
  status's +0x124 = the object (0x5DC28A), in place of what it kept; then
  `ConsiderCreatureMimickingWhenObjectLands` 0x4EAAB0 on the object (0x5DC294). Oracle run (stubs for the hand list,
  `MyInterface`, the finder and the mimic): the finder and the mimic both get the object in ecx, the found status's
  +0x124 ends 0 and this one's the object.

`GetInterfaceStatusWhoLastDroppedMe` walks `GetNextPlayer` (players 0..6, so never the neutral player) and each
player's 18 interfaces, and returns the first status whose +0x124 is the object. So after a throw by one hand of a
thing another kept, both keep it and the first in player order is found; the drop that follows clears only the one
found. `GInterfaceStatus::Process` 0x5DC5C4..0x5DC5D8 clears +0x124 at the status's turn once the object is not
`IsAvailable` (vt +0x2C: deleted, or a villager dying); `SetToZero` 0x5DBAD5 starts it at 0. The readers:
`Scaffold::BuildBuilding` (the plant sound, [building-sites.md](../world/building-sites.md)), `Living::HasSunk`,
`Villager::HasSunk`, `Villager::Landed`, `Tree::EndPhysics`, `Town::AddArtifact`, `WorshipSite::AddArtifact` and
`ConsiderCreatureMimickingWhenObjectLands`. No other code reads or writes +0x124 of a status (a scan of every
decoded function for `+ 0x124]`).

**Throw or drop.** On release in IN THROW there is a single path for throwing and dropping: `ApplyThisToMapCoord` and
then `Object::InitialisePhysicsFromHand` with the spring velocity, which throws if |v_xz|² > 4 and otherwise drops the
object (a pot from the hand: |v|² ≤ 5; the `AdjustToGroundLevel` flag takes |v|² > 1, not for trees). Details in
[Water in impacts and when dropping](../physics/physics-impacts.md#water-in-impacts-and-when-dropping).

**There is no "placing".** Dropping the object **over the sea does not place it**: `Object::InitialisePhysicsFromHand`
(0x636F00) only "lands" on dry land (altitude ≥ 4) or on a cell with altitude > 1, so over water the object stays in
physics and floats or sinks ([Sinking, drowning and being deleted](../world/water.md#sinking-drowning-and-being-deleted)).
On land there is no "placing" either: the body drops to the ground and a villager, an animal, a fence or an upright
tree leave physics immediately; everything else settles with physics.

- A **villager** dropped in the sea sinks in ~0.4 s and spends 60 s drowning (clip 252 with a scream and splashes)
  before dying; an animal disappears after ~7.5 s. If the water cell has altitude ≥ 2 the villager does "land", but
  `Villager::EndPhysics` sees `IsWater` and drowns too (in Land1 there is no water cell that high).
- A pot from the hand dropped in the sea does not leave a pile either: the resource is lost
  (`Pot::AddResourceToPos` 0x66F270).

**Pots and stores.**

- **A hand pot let go gently** makes no physics object: `Pot::InitialisePhysicsFromHand` 0x66DF00 puts the whole
  amount down as a pile and deletes the pot; the steps are on [the pot's side](../world/objects.md#a-pot-let-go-gently).
  A hand pot thrown merges on landing through `Pot::EndPhysics`.
- `Pot::ApplyThisToObject` 0x66DDD0 on the land: `Pot::AddResourceToPos` at the pot's position with its amount,
  IsPoisoned and no speed-up; it merges into the stores and same-resource pots of the 3×3 cells around (each within
  2 × its 2D radius, 1.2 for a store), else makes a MagicWood / MagicFood pile.
- `AddResourceToPos` 0x66F27A: off the map (MapCoords::InBounds) nothing is added anywhere: the resource is lost (and
  what is left over on water is dropped, 0x66F42D).
- `Tree::ReactToPhysicsImpact` 0x74B6B0: a thrown tree hitting a store calls h->DeleteObjectAndTakeResource(this, po
  +0x24) with the PhysicsObject still alive (0x74B6F9), so the store's Supply help trigger (0x7337A6) reads it; the
  thrower's interface is the giver.

**Forced drops.** ForceDropHeld 0x5D4350 (a power-up, a demo's start, a packet) sends the 0x4D with zero velocity and
L, the held object's matrix translation and GetYXZ (0x5D4393..0x5D43C3), then 0x1D → `ThrowObjectFromHand(status, 1)`.

**The release impulse** (GetRequiredState). With nothing held, +0x48F8 > 0 goes down by g_game +0x205D48 (the frame's
game ms, 0x46CE15); the frame it turns negative (`jns`: exactly 0 stops with nothing sent) and lastReleased is set,
+0x48F8 = 0 and the packet 0x40 is sent (fn_00550F10 0x46CE88) with g_C5E640 − g_C5E650 (0x46CE3D..0x46CE76) and
HandStateHolding +0x108; then lastReleased = 0 (0x46CE8D). Inside the citadel GetRequiredState does nothing of this
(0x46CD1C).

#### The locked apply's unlock

A seed cast while it is held in the hand (`ValidForLockedApplyProcess`, the IN_HAND seeds: lightning, water, food,
wood) is a locked apply: the press enters state 10 (the land) or 11 (an object) and sends the apply packet 0x12 /
0x11, and state 10's process 0x5D4C10 (state 11's 0x5D4D00) sends it again every frame while the button is held (one
waits at a time, +0x444).

- The release (0x5D4C2A..0x5D4C65; 0x5D4D03..0x5D4D4A for state 11) does not unlock the seed itself: it sends packet
  0x1A (ApplyUnlock, `fn_00550BE0`) with the held object and the turns held (turn − status +0x3C, the lock's turn),
  then `+0x38 = 0` and `ResetActionState` 0x5D29C0.
- The 0x1A handler 0x5DA8A0 (two arguments: the packet's object and value) sets status +0x9C = the value, the turns
  held, on every path. Then the packet's object (not the status's held object), if not null and `IsInteractable` (vt
  0x190), gets its `ApplyUnlockProcess(status)` (vt 0x730, the result ignored); else `EndAction` fn_005D1260.
  `Object` 0x402900 returns 1 and does nothing. The seed's 0x728EB0: a seed deleted once cast → `ToBeDeleted`, 3; else
  `StoreChantsAndAgeFromSpell` 0x728780 → `ClearSpellLink` 0x728200, the spell closes and the seed keeps the chants
  and age, 1, or `ToBeDeleted` and 3 when that gives 0.
- `EndAction` first runs the state's exit (table 0xD17DA8, stride 0x30, slot +0x10, 0x5D126D..0x5D1281); states 10
  and 11's exit 0x5D4CB0 also sends 0x1A, with an object in the hand whose byte +0xA bit 0 is clear.
- Packets are handled at the next turn's start in the order sent, so the unlock always follows the last apply: a click
  casts its spell, which strikes for about the click's length and closes the turn after the release.

#### Applying the held object to an object

`ActionPressedHolding` 0x5D1560 handles a press while something is held. The target is the creature to give to, or
the hovered object (`ValidAsInterfaceTarget` vt 0x6F0, Object 0x402840 = 1). With no target the press needs the
influence (else it is ignored) and goes by `ApplyOnlyAfterRecSystem` → state 8, `ApplyOnlyAfterReleased` → state 12,
else 0x5D1850 the drop at once. With a target:

1. **The give needs no influence** (0x5D15A4..0x5D15CC): `InterfaceValidToGiveObject(status, held)` (vt 0x748;
   `InterfaceValidToGiveObject` needs a creature) == 1 and the target's Flags +0x24 without 0x400 (refuses gifts) and 0x20 → 0x5D3080 (packet
   0x1F, state 15), with no influence test. Otherwise the press goes on to the apply tests, which need the influence
   only when the target's vt 0x714 is 1.
2. 0x5D1607: held->ValidToApplyThisToObject(status, target) == 1 → state 9 (a seed after the recorder), state 11 (a
   locked apply) or 0x5D30D0 (packet 0x11); else 0x5D168E (ApplyOnlyAfterReleased → state 12 / FailApply for a
   non-seed).

The class functions:

- `ValidToApplyThisToObject` vt 0x71C: Object 0x4028B0 = 0; SpellSeed 0x7286D0; `Villager::ValidToApplyThisToObject`
  0x752BD0: a WorshipTotem (RTDynamicCast 0x752BE7) → 1, a MagicTeleport (0x752C0C) → fn_005FC4B0 == 1, else 0.
- `ApplyOnlyAfterRecSystem` vt 0x738 (Object 0x402920) = 0 and `ValidForLockedApplyProcess` vt 0x72C (Object 0x4028F0)
  = 0, so a villager goes to SendApplyToObject (0x5D1684).
- With an apply packet still waiting (+0x444, 0x5D3135) no 0x11 is sent, only the action state (0x5D32B5); else
  fn_005D36A0 after it (0x5D32B0).
- The 0x11 handler 0x5DA1A0 calls `HelpProfile::Trigger(7)` for a sacrifice altar (IsSacrificeAltar vt 0x4B0), else 6,
  for the local interface (0x5DA268..0x5DA294), after the apply and whatever it did, then fn_005DA100.
  IsSacrificeAltar is true only for `WorshipTotem` (0x7807E0).

A pit's piles are objects of their own: a tree over a pit's wood pile is valid (the pile's `IsResourceStore(WOOD)`
asks the pit), over its food pile it is not. Per class (ValidToApplyThisToObject vt 0x71C / ApplyThisToObject vt
0x720):

- Tree 0x74BD50 / 0x74BDA0 and DeadTree / FelledTree 0x510E90 / 0x510EE0: valid when `target->IsResourceStore(WOOD)`;
  apply: `DeleteObjectAndTakeResource` → 3 (the worship totem branch is the sacrifice,
  [the trees](../world/trees.md)).

##### Giving a held object

The creature to give to is `CHand::GetCreatureToGiveTo` (0x46E4B0): a held object moved to the creature is given after
1100 ms, a roll below 300 ms and a move of more than 15 × Size1 cancels it (packet 0x52). Else the target is
`m_ActionCollide.object` (sub_5D1560, the press: `GetCreatureToGiveTo` first, else CInterface +0x400).

**How a giving lasts and ends.** The hand keeps the creature at CHand +0x49B4 (its 3D body at +0x49B8) and a giving
flag at +0x49BC. `HandStateHolding::Update` (0x5B6185..0x5B61D2) and `ObtainRequiredHandPosition` (0x5B4114..0x5B4191),
over a creature that would take the held thing (`InterfaceValidToGiveObject` vt +0x748 = 1), set +0x49B4/+0x49B8 to
that creature and the offer timer [0xD13F3C] to 1100; the timer runs down by `g_delta_time` only while +0x49BC is 0
(0x5B41A6..0x5B41CD, which then also clears +0x49B4/+0x49B8). `GetCreatureToGiveTo` returns +0x49B4 while the timer
is above 0 (`CanCurrentlyGiveToCreature` 0x5B3C60): so while a giving lasts a press gives again to that creature, and
a creature hovered over that would take it becomes the one given to. The give packet's handler fn_005DA2D0 starts the
giving with fn_0046C080 (+0x49BC = +0x49B4 = the creature, +0x49B8 its body, 0x5DA395) and then the creature's
fn_0047A1D0. While +0x49BC and +0x49B8 are set, a hand more than 15 × the body's +0x90 from its take point
(`GetPickUpFromHandPosition`) sends the call-off, packet 0x52 (0x5B41D6..0x5B424C), whatever the creature is doing.
+0x49BC is cleared only by `CHand::EndGiveToCreature` 0x46C0B0 (from `DoPickupFromHandAction` at the moment of
taking, 0x487C3C, and from the call-off packet, 0x63DB23), by `CHand::GameTurnUpdate` once the creature is no longer
valid (0x46E55D..0x46E57C) and by `OnClearMap`: a take refused, failed or abandoned leaves the giving on.

The pick: the mouse-ray pick of each drawn object (`SendObjectDrawCollision` 0x5D56C0,
`CheckTriangleCollide`, the nearest wins; `UpdateInterfaceCollide` 0x5D5A70 keeps an object behind the land hit only
when the land point is inside its XZ bounding box, [Object under the cursor](picking.md#object-under-the-cursor)). How
that pick is made, as the interface's reconstruction reads it:

- The renderer flags each object whose screen selection box holds the mouse (`LH3DObject::g_last_selected_box`), and
  `Game3DObject::AddForDrawing` / `AddJustForCollide` and several `Draw` overrides call
  `GInterface::SendObjectDrawCollision(obj, 0, NULL)`; with `LH3DObject::g_b_last_on_screen` the object is also sent to
  `HelpSystem::SendFOVObject(obj, LH3DObject::g_last_distance)`.
- `SendObjectDrawCollision` runs only while no action is latched; it skips an object in the hand or not interactable;
  it rejects cheaply by the depth along the view row (0xEA1D30.., the translation 0xEA1D54) at or beyond the best
  distance + the bounding radius; a human (villager) is a sphere distance to the camera, everything else an exact
  `LH3DObject::CheckTriangleCollide` (vt 0x1E0), class 6 `CheckPixelCollide` (vt 0x1E4) and an abode with a
  destruction mesh fn_005D5E80 (the fragments' mesh test). The nearest goes to `m_ActionCollide` (+0x3E8) through
  `GInterfaceCollide::SetObject` fn_005D5E40.
- `UpdateInterfaceCollide` runs in `PostDrawProcess` and settles the object, the bubble, the land (the land pick
  fn_005D5980 → `GLandscape::fn_005E5740`, its distance + 2.3) and the leash (fn_00848F90 at the mouse, +0x428 /
  +0x42C, with the radius 2 [0x92AB44]); it sets the action point to the object's and runs the hover timer (+0x3E0
  the seconds on the same object, +0x3E4). Its validate (fn_005D5DF0 on +0x3B0) drops an object no longer
  interactable.
- A click that hits nothing falls back on `GInterface::FindObjectNearMapCoord` 0x5D39E0: a fish shoal on water, else
  the nearest object that is not a Fragment within ±5 (its globals 0xD2017C / 0xD1A3A0 skimmed only).

**What the creature does with a thing given.** The creature takes it in `LH3DCreature::DoPickupFromHandAction`
0x487B30 (the carried object set, `InterfaceGiveObject` 0x47A320 = 0x16 for `ApplyChangesToHand`). The choice is
`sub_50A380` (no direct caller; its plans are forced with `ForceActivityAndForceAction`), on the creature's carried
object (+0x160 → +0x28). It is not called by the take: the give's handler fn_0047A1D0 (from the give packet's
fn_005DA2D0 at 0x5DA3B4) forces the take (desire 6, action 142, 0x47A2AD..0x47A2CB), sets mind +0x1C14 = 1 and
+0x1C00 = 1 and stores the member pointer 0x50A380 as the mind's next action at +0x21D0..+0x21DC (0x47A2F4), in place
of any (a fizzle's, a stroke's or a slap's; a mushroom's high is stored only into an empty slot, 0x5005C2). The next
action is run by the free choice fn_004F0370 (called from `ProcessState` 0x47369D, before `ProcessSubAction`) the
first turn no plan is under way, in place of `DecideWhatToDo` (vt +0x8C8) that turn (0x4F0495..0x4F04E7; see
[the fizzle shown](../creature/casting.md#the-fizzle-shown)). So it runs however the take ended (taken, refused with
something held, failed with nothing in the hand, or called off) and on whatever the creature carries then; nothing
carried, it returns at once (0x50A39B). `FinishActionUnsuccessfully` and `StopWhatIAmDoing` leave +0x21D0 alone (their
writes are +0x21C0, +0x21F4, +0x21F8). The take's sub-action also sets Creature +0x394 = 1, done or refused
(0x506D34); its only reader is the development-phase test sub_4C5C20 (the stage table's entry 2 at 0xC82680/0xC82690,
called through sub_4C59A0): the stage is over once +0x394 is set or after a time in it (+0x126c turns).

- A one-off spell seed (vt +0xAC), at development phase 8 or more (+0x1268): the seed's first magic type, cast by
  its known-about row (0x50A3B3..0x50A527).
- Else, with a plan under way (+0xF78) whose action takes a thing (`g_CreatureActionInfos` +0xA4): the gift becomes
  what it is done to (+0xF88, when `IsObjectSuitableForAction`) or what it is done with (+0xF8C, by the action table's
  +0x44 test) and that plan is forced again (0x50A55B..0x50A5EB).
- Else the four desires 6 Curiosity, 4 Hunger, 1 Compassion and 30 PlayWithPlayer (0x50A5F5..0x50A60D), Compassion
  skipped (`cmp ebx, 2`, 0x50A62B): each scores the desire's value (mind +0x150 + 4d) × **`GetObjectToActOnUsefulness`
  0x4E9A70** of the thing's belief for that desire (0x50A64D), the second tree's usefulness × the distance priority ×
  the desire's object factor, 0 outside the kept area ([the planner](../creature/plans-and-needs.md)). The highest
  score above 0 wins (strictly), Hunger not for a kind the creature learnt it can't eat (+0x18510 by the belief's
  type, 0x50A678..0x50A696); the strongest of the three desires is kept apart.
- No winner, or the strongest desire below 0.1 ([0x8AB22C], 0x50A6C0): the first gift of its kind (+0x183A8 by the
  belief's type, then set) is examined; a later one is acted on as when led to it (`ForceActOnObject` 0x4C5FF0).
  Otherwise the winner's action, 275 ExamineObjectInHand, 276 EatObjectInHand or 278 ThrowObjectInHand (0x50A6F8..
  0x50A710), is forced with its desire on the thing (0x50A731).

### Tapping objects

A short click (released within 225 ms), or a press on an object that cannot go into the hand, is a tap:
`Tap` 0x5D3930 → `SendTap` 0x5D38A0 → packet 0x20 → 0x5DA650 → the class's `InterfaceValidToTap` (vt 0x740, Object
0x4196B0 = 0) and `InterfaceTap` (vt 0x744; `Object::InterfaceTap` default 0x4196C0 returns 1). Villagers, animals and
trees keep Object's defaults: they cannot be tapped. The tapped object and the tapped land are remembered for 15 s
(0x5D3700, [Tap memory](picking.md#tap-memory)).

**The press.** `GInterface::ActionPressed` sends the collided object to `StartGrab` when it is valid to tap
(vt 0x740), and `StartGrab` taps at once an object that is not valid to place in the hand.

**The tap is applied at the next turn.** `SendTap` 0x5D38A0 does not call the class's tap: once
`InterfaceValidToTap` (0x5D38DB) is 1 and vt 0x180 is 0, it only queues packet 0x20 with the object's index
(fn_00550C80 at 0x5D3904, which writes the type at g_game +0x205A61 and calls `SendPacketCompressed` 0x551690).
`GPacket::ProcessPacket` 0x63C420 dispatches the type through its jump table 0x63DDCC: entry 0x1A (type 0x20 − 6, at
0x63DE34) is 0x63CC4B, which calls 0x5DA650 at 0x63CC55, its only caller. The packets are applied at the start of
the next turn ([The packets](../engine/loop.md)), so the tap reaches the abode one turn after the press.

**The handler.** The 0x20 handler 0x5DA650 checks IsInteractable (0x5DA664) and InterfaceValidToTap again, calls
InterfaceTap at the status's synced hand +0xC8 (Abode 0x4068C6, Rock 0x6E749C, Scaffold 0x6E9DF7, OneOffSpellSeed
0x72A689) and `HelpProfile::Trigger(8 Tap)` (0x5DA6A0). A rock's tap also reports the deed BREAK_ROCKS
(`ConsiderMakingCreatureMimicPlayer`, 0x5DA6C8; [Mimicry](../creature/mimicry.md#mimicry-and-the-town-hooks)).

The classes:

| Class | ValidToTap | Tap |
|---|---|---|
| Rock | 0x6E7450 (height > 0.7) | 0x6E7480 (SplitInTwo, G_RockTap) |
| Abode | 0x406820 (always 1) | 0x406830 (knock on the roof) |
| TownCentre | 0x55DB10 (0: `xor eax, eax`) | Abode's, never reached |
| SpellIcon | 0x7263C0 | 0x726430 |
| OneOffSpellSeed | 0x72A630 (1) | 0x72A640 |
| CitadelEntrance | 0x468F50 (multiplayer, or GScript +0xA0 != 0) | 0x468EF0 (GoInsideCitadel) |
| ScriptHighlight | 0x70ADD0 (a did-you-know with a script id; a scroll active with one) | 0x70AC70 (the did-you-know bubble) |
| LeashObj | 0x464450 (the local player's own post) | 0x464490 (pick or unpick the post's leash, sound 42, packet 0x65) |
| MagicFireBall | 0x682E40 (1) | 0x682E50 (the catch, fn_00682EA0; the ball deleted when it caught) |
| PuzzleTotem | 0x6DA5F0 (+0xE4 == −1; slot at 0x8EE2C4) | 0x6DA610 |
| Scaffold | — | 0x6E9DD0 |
| Reward | — | 0x6E5D00 |

**Abodes and the village centre.** Every Abode class (Abode, Windmill, Creche, Field, Football, Wonder, Graveyard,
Totem, StoragePit, SpellDispenser, Workshop) keeps Abode's two functions. The exceptions are TownCentre, whose
ValidToTap (vtable 0x8E4BFC, slot 0x740 at 0x8E533C) is 0x55DB10 = 0, and PuzzleTotem, which has both its own. So a
press on a village centre is not a tap: it is not tap-only for the press, sends no packet 0x20 and triggers no help
event.

**The hand's knock** is `CHand::StartFixedPosAnimation(hand +0x78, 0x39)` (push 0x39 at 0x4068B6, the call at
0x4068BB), only for `MyInterfaceStatus` (0x40687C) with a render hand (`GPlayer::GetRenderHand` 0x64AB20,
0x4068AA). Clip 0x39 = 57 is `Ctap_house` in `Data\hndspec5.txt`'s order (the 57th clip from 0, in the
`=individual` group).

**A press on a sign.** ScriptHighlight's `ValidForPlaceInHand` 0x709770 is `xor eax, eax; ret 4` (0) and its
influence test 0x709840 is 0, so a press on a «Did you know?» sign is a tap wherever the hand is: the first opens the
bubble with the sign's text, a second on the same sign closes it (`SetBubbleProperties`,
[The first land's intro](../scripts/land1-intro.md)). `ScriptHighlight::InterfaceValidToTap` 0x70ADD0 is asked at
0x5D38DB; its InterfaceTap 0x70AC70 is a «Did you know?» scroll; the tapping status is the local one when it is
MyInterfaceStatus (0x70ACFE / 0x70ACA7).

**The leash posts.** LeashObj (the temple's leash posts) is valid to tap (0x464450) only for the local player's own
posts; the tap picks the post's leash (sound 42, packet 0x65) or clears the pick on the picked one. The hand reaches a
post through `SendInvisibleDrawCollision` 0x519960, radius 1.0, each frame it is drawn
([The temple's leash posts](../creature/temple-leash-posts.md#the-temples-leash-posts)). A post keeps Object's
`ValidForPlaceInHand` (0) and influence test (1): a press on it is a tap, in the influence.

**Hand Move.** `HelpProfile::Trigger(1 Hand Move)` 0x5CECC0 comes from MOUSE_MOVE (0x5D9970) with a pixel delta
(+0x420 / +0x424).

### Uprooting and picking up

Uprooting: the tug starts **on press** (StartGrab 0x5D1740 calls `CHand::PickUp(obj, 1)` immediately; the 225 ms
threshold is only for the other objects); the hand pulls the tree with a spring from its grip, and it leans and
stretches towards the hand and comes out once the hand can lift it and pulls harder than it weighs (details in
[Tug](#tug)). When it comes out: TreeBreak sound, pile of roots (mesh 593, 15 s) and dangling roots (mesh 592).

- The dangling roots (fn_008263C0 / fn_008261D0, 0x8263C0 / 0x8261D0) start from the DeadTree's own drawn matrix
  (fn_00826280 copies it at their first draw, its rotation scaled by 0.15 × extent), fall as y = y0 − 20 t² down to
  altitude + 0.1 × extent × scale, fade from 18 s (alpha = 255 × (1 − (t − 18) / 2)) and are gone after 20 s.
- The pile of roots (the crater) is an `LH3DObject::Create(1)`, **morphable** (fn_00825240 → UpdateMelting vt+0x1E8
  once when created): it moulds itself to the terrain like the fields and stores
  ([Meshes stuck to the ground](../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph)).

#### Tug

`HandStateTug` Enter 0x5B7DF0 / Update 0x5B8070.

**The rule.** At the start, the anchor is the base of the tree and the drag plane passes through it with the terrain
normal, at the height of the hand seen at the anchor's distance. After 0.13 s (the state change fade), every frame the
hand goes to the intersection of the mouse ray with that plane; the grip is at `base + up × drop` (drop = 0.1 ×
height, minimum 3.2 × hand scale × 0.3 at the start, which no pull sees, below); a spring `F = 1000 × (hand − grip)`
(cap 600000) tilts it towards the hand (turning force `(F × r)/1000`, below) with quadratic friction 4 around the base,
and the trunk stretches up to ×1.3 (Zoomer 0.3 s). It comes out when `|F| > GetWeight` (scale³ × info.dat weight):
grabbed far from the grip point, it comes out straight away. Released earlier, it returns to its posture. At the end of
each Update (also in the first 0.13 s) the hand is placed at the grip of the stretched trunk
(`CHand+0x78 = matrix × (0, drop, 0)`); it is only drawn there, the next Update puts it back on the plane.

**Consequence**: since the hand before pressing is on the mouse ray, the plane ends up at the height at which that ray
crosses the tree's axis, so the first tug is that height minus the drop: a beech of scale 1 (18 m, grip at 1.8 m,
weight 1000) only stays if pressed within ~1 m of the grip (from 0.8 to 2.8 m above the base); pressed at the crown it
comes out after the 0.13 s fade, without the mouse moving.

**Enter: the plane and the first grip.** `HandStateTug::Enter` 0x5B7DF0 stores:

- the base (CHand +0x492C, from the tree's matrix: the 3D object +0x40 → +0x14 matrix's translation, copied at
  0x5B7E53..0x5B7E6C);
- the land normal there (`GetNormal` 0x5B7EC3, state +0xA4);
- the plane point (state +0xB0, stored at 0x5B7FD8..0x5B7FE7) = (base.x, eye.y − (eye.y − hand.y) ×
  range_2d(eye, base) / range_2d(eye, hand), base.z), the hand being CHand +0x78 as it was before the press and the
  eye `g_camera` (0x5B7F0E..0x5B7F5A; `range_2d` 0x6E8130 = √(dz² + dx²));
- the first grip = max(0.1 × GetHeight, 3.2 × CHand +0x4834 × 0.3), into CHand +0x4944 (0x5B7F5E..0x5B7FC7,
  0x5B7F9D..0x5B7FC7; `Tree::GetHoldLoweringMultiplier` 0x74B620 = 0.1);
- the stretch at 1 (0x5B8009..0x5B804B);
- the tug matrix (state +0xEC, the 3D object's world matrix with its scale, copied at 0x5B805C after
  `GetWorldMatrix` vt +0x63C);
- the first-frame flag (state +0xC, set at 0x5B7FD1).

**The first-frame flag.** While CHand +0x49B0 (the state blend's time) is below 0.13 the Update skips to its drawing
part (0x5B8096..0x5B80A7); the Update that finds it at 0.13 or more clears the flag and skips too (0x5B80AD); the pull
runs from the Update after. The state's Update is given the blend's dt (`GetCameraTimeInc` × 0.001, `g_delta_time`
inside the citadel: 0x46C5C9..0x46C608, pushed at 0x46C91C).

**Each Update** 0x5B8070, in order:

1. **The hand on the plane.** The hand (CHand +0x78) is the mouse ray (fn_0074CBD0) meeting that plane, the dot
   products summed z, y, x (0x5B80F6..0x5B81C2).
2. **The grip**: a copy of the tug matrix with its rows normalised (`NormaliseMatrixOnly` 0x7FB5C0, the table inverse
   square root), then base + up × drop (0x5B81E0..0x5B8228).
3. **The stretch**: d = hand − grip, measured √((x² + z²) + y²) (0x5B8252); target min((|d| + drop) / drop, 1.3),
   `SetDestinationWithSpeedAndTime(target, 0, 0.3)`, then the zoomer stepped inline (0x5B8355..0x5B83D6): its speed
   summed as `Zoomer::Update` sums it, its value as `UpdateInline` does.
4. **The uproot.** The pull is measured the same way (0x5B842B). `GetMaxForce` 0x46BFB0 = (CHand +0xAC + 1) × 600000
   (0x8CBF20); the only write of CHand +0xAC the scan finds is `CHand::Show` 0x46C1B9 (0), so the hand's greatest pull
   is 600000. The uproot needs `GetMaxForce ≥ 9.81 × GetWeight` and `|F| > GetWeight` (0x5B8414..0x5B8452), only for
   an `IsTree` (vt +0x338; any other object ends the tug at once, 0x5B8409).
5. **The turn**: the turning force is computed F × r (r = grip − base, three `fsubp` DE E9 at 0x5B84EB..0x5B850D)
   times 1/1000 (the inverse, 0x5B852D), then × dt; ω += τ − ω k, k = |ω| × 4000 × dt / 1000 (the damping) with
   |ω| = √((ωz² + ωx²) + ωy²) (0x5B858C..0x5B862D). The constants are at 0x900C08: 0.13, 1000, 1000, 4000. The step
   ω × dt, all three zero → no turn, else its length √((x² + y²) + z²), inv = 1/length and
   `fn_007FB180(step × inv, length)` (0x5B8674..0x5B8700); each row of the tug matrix times that matrix as a row
   vector, summed from the last term: r_i' = (R₂ M_i2 + R₁ M_i1) + R₀ M_i0 (0x5B8751..0x5B88C8). fn_007FB180 is the
   Rodrigues matrix by rows (oracle: six cases bit for bit); applied to rows it turns by −angle about its axis, so
   with F × r it turns about r × F by +angle: the tree leans towards the hand. `GetPhysicsConstantsType` (vt +0x788)
   = 3 turns the lean off; a Tree's is 6 (0x74B640).
6. **The pull velocity** CHand +0x4938 = (half point after − half point before) × (1/dt), the half points from the tug
   matrix before the stretch, (h × 0.5) × up + base (0x5B8716..0x5B874C, 0x5B88CB..0x5B88EF, 0x5B8963..0x5B898A); 0
   for a type that does not lean. A displacement scan of the exe finds it written only here and zeroed by
   fn_0046DC30 and `CHand::ResolveLoad`: no reader.
7. **The drop is reset** at the end of **every** Update, the fade's included, by the hold type's case
   (HOLD_TYPE_TREE, 0x5B8B78..0x5B8BEA): GetHoldLoweringMultiplier × GetHeight. So Enter's least hang (3.2 × the
   hand's scale CHand +0x4834 × 0.3) never reaches a pull. `GetHeight` is `Object::GetHeight` 0x638120 for a Tree
   (mesh half height +0x28 × Object +0x50 × 2): the stretch is only written to the drawn 3D object's up row
   (0x5B891D), so the height and the grip never see it.
8. **The hand drawn at the stretched grip**: the normalised copy's up row × the stretch, then (0, drop, 0) through it
   plus the base (0x5B8C1A..0x5B8CCD).

**Drawn only.** HandStateTug writes nothing to the tree (Object +0x14, angles). Its matrix goes to the G3D (0x5B88ED,
the up row × the stretch at 0x5B891D) and is re-applied after Tree::Draw by CHand::UpdateHeldObject 0x46E490 →
0x5B8E60. Exit is HandState::Exit 0x5B02D0 (a bare `ret`), so a release without the uproot draws the tree upright from
its logic on the next frame.

**The constants' sources.** 0.3 is the least lowering's factor (3.2 × scale × 0.3), the mouse sway's 0.3 / 80 rad a
pixel and the stretch's 0.3 s; any other object than a tree clears the tug hold at once.

**The end of the tug.** The uproot's dirt and smoke are `Tree::fn_0074BD20`, made when the tug ends (only for a tree
not flagged IN_PHYSICS, Flags 0x40 at 0x5B8476), before the next frame's `GetRequiredState` gives HOLDING (4). **The uproot packet**: `HandStateTug::Update` 0x5B8070 ends the tug with
only the mark (0x5B847E) and CHand +0x4908 = 0 (0x5B848C); then state 13's GenericPickup sends packet 0x13 with the
tree, state 7. During that wait of up to one turn the original's CHand already draws the held object (every pick-up
but the forest's, whose tree is made in the handler).

#### Pick-up rules and BigForest

- **Pick-up rules**: `Tree::ValidForPlaceInHand` = 1 and `IsTuggable` = 1 for the 22 types, at any scale (bushes,
  hedges, palm trees, copses, inside or outside villages). Only the flag 0x2000 (saved games and puzzles), being
  outside the influence or a locked selection prevent it; then it goes through the "tap" path, which for trees does
  nothing. DeadTree/FelledTree: picked up without a tug. Uprooting: `G_TREEBREAK` + 1 evil alignment push
  (`GAlignment::Update`); replanting, good ([Alignment](../world/trees.md#alignment)).
- **`BigForest`**: it is not tugged; on grabbing (225 ms) `BigForest::InterfaceSetInMagicHand` 0x4393C0 does
  `RemoveResource(WOOD, 350)` (the Conifer's wood, at 0x4393EC) and puts a new Conifer in the hand: `Tree::Create(&forest
  +0x14, Conifer, forest +0x80, 1.0, 0.0, 1.0)` 0x749EE0 (0x43940E), at the forest's MapCoords (0x43940A), not the
  hand's point. `RemoveResource` 0x4390D0: the forest's wood (+0x84; on creation woodValue × scale, `Create`
  0x438EC0) goes down by 350 and **only** when its wood drifts more than 250 away from life × scale × woodValue is it
  rescaled to wood/woodValue and plants at the edge; without enough wood it gives what is left and the forest is
  deleted ([Searches for trees and forests](../beings/villager-jobs.md#searches-for-trees-and-forests-for-the-villagers)).
  `AddTreeAround` 0x439220: up to 10 random angles at its radius; on land and without an object in the cell with
  distance + radius less than 4, a Pine of its forest (+0x80), scale 0.05, random angle and maximum size 0.75 +
  random(0.5).
- **Under the cursor** a tree is found by its own drawn, opaque pixels (a pixel test with the texture's alpha mask),
  never through its forest: there is no forest-patch pick (#130). A press between trees with nothing under the cursor
  takes the object nearest to the land behind the hand. See
  [Object under the cursor](picking.md#object-under-the-cursor).

### Dropping and replanting

**Dropping** (`Object::InitialisePhysicsFromHand` 0x636F00 + `Tree::EndPhysics` 0x74B830). The tree counts as "put
down carefully" (flag 8 LANDED of the physics object) if it is not thrown and it did not have to be raised
(`IsDryLand` or cell altitude > 1). The normal test (y < 0.7 ⇒ it does not land) is **only** for living beings and
fences, not for trees (bw1-decomp `src/Black/Object.cpp:539`). Then:

- **Upright and LANDED.** A LANDED tree without `FireEffect` over `IsLand` leaves the physics at once if it arrives
  almost upright (the x and z angles of its YXZ matrix ≤ 0.2 rad ≈ 11.5°; a tree in the hand takes the hand's "up",
  which follows the surface, so on a slope it is tilted) and `Tree::EndPhysics` replants it. "Upright" =
  `LHMatrix::GetYXZ` 0x7FAB30 of its matrix with |x| ≤ 0.2 and |z| ≤ 0.2 rad (x = asin(row2.y),
  z = atan2(−row0.y, row1.y), checked by emulation).
- **Tilted** (or `dont_replant`): it stays in physics without LANDED, falls and ends up as a dead tree.
- **Hot or burning, or LANDED on a water cell**: it stays in physics with LANDED and when it stops `Tree::EndPhysics`
  turns it into a dead tree (it keeps its fire).
- **Over the sea** (neither `IsDryLand` nor cell altitude > 1) it **does not land**: it floats ~19 s until it sinks
  ([Water in impacts and when dropping](../physics/physics-impacts.md#water-in-impacts-and-when-dropping)).
- **Thrown** = always a dead tree.
- The sound (`Tree::DropSfx` 0x74BC60, G_PLANTTREE + tick%3) is played by `PhysicsObject::RemoveObject` 0x646B44 on
  **every** careful drop that ends on land, replanted or not.

**Forest on replanting** (0x74B8BF). A spiral over the map cells up to 25 + 10 m; for each fixed object
`d = distance − its 2D radius`. An object of a village (or part of the temple) closer than 25 m ⇒ the tree is "of a
village" (bit 1 of +0x5E = `isNonScenic`, it is set to **1** inside the village!) and joins the forest **of the
village**, which beats any other; otherwise, it inherits the forest of the nearest tree with a forest (with no limit
of its own, only the 35 m of the search); with none and outside a village, it creates a new forest. The original takes
the village's forest from a list that the village keeps (Town +0x608): the last scenic forest of the list (details in
"Replanting in a village", in [Searches](../beings/villager-jobs.md#searches-for-trees-and-forests-for-the-villagers));
in a village without a scenic forest it keeps the nearest tree's forest and, with none, the tree is left **without a
forest**. Effects: white smoke `SmokyStuff` on the ground, `SPOT_VISUAL_FOREST_CREATED` (0x2C) **whenever it is not in
a village**, `StartImmersion(0x2E)`, creature mimicry and good alignment ([Alignment](../world/trees.md#alignment)).

The search in detail (`Tree::EndPhysics` 0x74B982..0x74BADB):

- It walks at most 1000 cells (0x3E8, 0x74B982) and stops at the first cell farther than 25 [0xC22FA4] + 10 [0x8AB414]
  m.
- It walks only the fixed list of each cell (GetFirstObjectFixed 0x74B9C0, GetMapChild 0x74BA66). For each object,
  d = fn_00605CD0(object, tree) minus its Get2DRadius (vt +0x64, 0x74B9D6..0x74B9E6).
- An object with d < 25 (0x74B9EE) puts the tree "in a town" (0x74BA7B) if it has a town (GetTown vt +0x48) or is a
  citadel part (`IsCitadelPart` 0x6380C0: info type 8). With a town, every scenic forest of its list becomes the best
  at distance 0, so the last one wins (0x74BA9C..0x74BAB4); then the search moves to the next cell (0x74BAB6).
- Otherwise a Tree (RTTI 0x74BA20) with a forest (GetForest vt +0x86C) nearer than the best lends its forest (the best
  starts at 99999 = 0x47C34F80, 0x74B902; test at 0x74BA46). Later cells go on with the same best, after a town too.
- Tree +0x5E bit 1 (isNonScenic) is the script's own flag (CREATE_NEW_TREE 0x716324); the hand sets it to "inside a
  town" when it replants the tree (`Tree::EndPhysics` 0x74BB5A), taking the "in a town" answer at 0x74BB10..0x74BB62.
  The new Forest outside a town is made at 0x74BB53.

**The replant's creature mimicry** (`Tree::EndPhysics` 0x74BB99..0x74BBB1). After `Forest::AddTree` (0x74BB62) and the
spot visual (0x74BB7C), with a player (`PhysicsObject::GetPlayer` 0x647460 of the tree's body: its status +0x24's
player, else none), `ConsiderMakingCreatureMimicPlayer(status = body +0x24, PLANT_TREE 11, the tree, magic 0)`
0x74BBB1, then `GAlignment::Update(player, tree, 1)` 0x74BBBD, then `Fixed::EndPhysics` 0x74BBCA
([Mimicry](../creature/mimicry.md#mimicry-and-the-town-hooks)).

**The dead-tree branch** (`Tree::EndPhysics` 0x74BBD9..0x74BC37). fn_00510B70 makes the DeadTree, which takes over the
tree's 3D object and fire (fn_00730960), and keeps the GetWoodValueMultiplier in +0x9C. InsertMapObject (vt +0x544,
0x74BC0A) puts it in the map; the tree's ToBeDeleted (0x74BC37) removes the tree.

### Sources

- The reconstruction of the hand's input layer and grip (`interface.cpp`, `multi.cpp`, `hand.cpp` and their notes):
  the grab, the tap, the release, the drop and the throw read instruction by instruction; the grip's matrices,
  `CAnim`'s evaluator and the hierarchy too, with an exact re-implementation (`grip_eval.py`) run on `hh.HBN` and
  `Hand_Boned_Base2.l3d`.
- The exe with the xref index (the tug's Enter and Update, the release, the pot's put-down), the oracle for
  fn_007FB180; bw1-decomp `src/Black/Object.cpp` for the landing test.

## openblack

**Done**: the grab and its timers, the influence and pick-up flags, the tap table, the tug, the holding spring, the
throw from the drawn pose, the locked apply's unlock, the apply to an object, the release's building-site branch,
BigForest and the replant. **Partial**: the tug state of other objects and the put-down particles. The port decisions
taken from raffclar's code are in [Port decisions: the hand, the camera and the interface](../project/port-decisions-hand-camera-interface.md).

### The grab and the pick-up in openblack

- **Influence**: `hand_tap::Register`'s `needsInfluence` (false for ScriptHighlight) and `hand_tap::SendsTap`.
- **The pick-up flags**: `SET_ID_PICKUPABLE` and `SET_ID_MOVEABLE` set tag components (`src/ECS/ObjectFlags.h`,
  `ecs::object_flags`; were `ThingFlags.h`, `thing_flags`). Not ported: the other setters (`LoadInstance`, the
  puzzles).
- **An object that hangs**: `from_hand::TakeOutOfPhysicsForHand` in `HandSystem::PickUp`, then `EndAction`. The pick
  sound and the "Catch" tooltip read the `InPhysics` tag, as the original reads IN_PHYSICS.
- **The tug's ABOVE time** is `magic::hand_hold::TugTimeMs` (raffclar's name), taken while `HandSystem` tugs
  (`_tug`). openblack's tug state is a tree's only, and a tree's hold is TREE, which pulls as it holds, so nothing
  changes yet; the tug hold of anything else keeps the hand in HOLDING (Pending).
- Not ported: a tuggable object without a Game3DObject, and the tug blend not advancing while the hand is hidden.

### Holding and throwing in openblack

- **The pose's frame**: `hand_hold::HoldTimeMs` returns the int frame, cut towards zero, for the holding and the tug
  poses alike, and `village_totem::GripTimeMs` the totem's ([The village totem](../magic/village-totem.md)). The multi
  pick-up's is not ported.
- **The spring** starts and stays on as above (`magic::hand_hold::StepHoldingSpring`).
- **Where a throw leaves from** (issue #134): `HandSystem::PushThrowData` passes the held object's `HandDrawPose`
  (`magic::hand_hold::ReleasePlace`) to `physics::from_hand::PredictRelease`, which builds the body there and puts the
  turn's place back after; before, it was built at the turn's place, the synced hand point, which trails a moving hand
  by one or two turns (and does not hang below it), so the throw left from behind the hand, further back the faster it
  moved.
- **Release**: a single path for throwing and dropping (`HandSystem::Release`).
- **A player with no resistance to wind** (2026-10-22, ours from the exe): `Object::ThrowObjectFromHand` 0x638700,
  the base of every class's throw, clears the new physics object's drag +0x174 (0x63870E) when the throwing hand's
  player (`GInterfaceStatus::GetPlayer`) has GPlayer +0x940 set, which the script native SET_PLAYER_WIND_RESISTANCE
  sets (`GPlayer::SetToZero` 0x649340 clears it; [the weather natives](../scripts/script-commands.md#weather)).
  openblack: `ecs::wind_resistance::AfterThrowFromHand` after `HandSystem::ThrowObjectFromHand`'s throw and a computer
  player's hand's, on the player's `NoWindResistance` component; test
  `WeatherTest.AScriptSetsAndReadsAPlayersResistanceToWind`.
- **The last dropped object**: one per player's interface, `LastDropped` on the player's entity (`ecs::last_dropped`):
  `MarkThrown` from `physics::from_hand::InitialisePhysicsFromHand` for a hand's throw, `RecordDrop` when the player's
  hand lets go (`HandSystem::RemoveFirstFromHand`, and the 0x18 result in `HandleApplyResult`) and when a computer
  player's hand does (`GameHandRunner::LetGo`), `WhoLastDropped` for the readers, first in player order. A player
  with no entity keeps nothing. Tests `BuildingSitesTest.AHandsNextDropTakesTheMarkOffWhatItDroppedBefore`,
  `ADropTakesTheMarkFromTheHandThatHadIt`, `AThrowMarksWithoutTakingTheMarkFromAnotherHand`. The plant sound reads
  it, and the landing mimicry (`creature_mimic::ConsiderObjectLands`), which every hand's drop now runs after the
  record, as fn_005DC1E0 does ([Landing](../creature/mimicry.md#landing)); the other readers are Pending.
- **Not drawn**: `HandSystem::PrepareState` puts `NotDrawn` on the held object while its state does not draw it:
  outside the tug, holding, multiple pick-up and grain states, with the interface inactive, or while a box has the
  mouse (`interface_active::BoxHasMouse`, what the last frame's interface pass found, as the original's draw comes
  before the frame's pass); the rule is `hand_detail::DrawsHeldObject`, test `HandHeldDraw.*`
  (`test/hand/test_hand_held_draw.cpp`).
- **The release to a pit**: the release no longer gives a tree to a pit within 8 m of the point (that disc was not the
  original's); a pit still going up takes it through the building-site branch (below).
- **The sway's roll** from a creature taking the object (Zoomer +0xD4) is not ported; nor the grain's acos branch
  (off in the original).
- **The grip pose**: openblack's matrices are proper rotations (the original's held matrix is a reflection, which a
  tree does not show). Chold_side needs its root rotation and translation: "the root locked except in Cgrip" is wrong
  for the hold poses.

### The locked apply's unlock in openblack

`HandSystem::UpdateSeedAction` sends `hand_seed::UnlockPacket` (`ECS/HandSeedUnlock.h`) on the release, `EndAction`
sends it when it leaves a locked apply with a seed in the hand, and the 0x1A handler is in `HandTurn.cpp` (the turns
held kept in `_unlockTurnsHeld`, the status +0x9C). Until 2026-10-09 the release called `ApplyUnlockProcess` at once,
while the applies were already deferred packets: a click unlocked before its own apply was handled, and the spell that
apply cast was never closed, so a lightning bolt struck every turn until its age ran out.

### Applying a held object in openblack

- The sacrifice altar's help event: `held_apply::IsSacrificeAltar` (a `WorshipTotem`) and `held_apply::ApplyHelpEvent`,
  in `HandSystem::ApplyHeldToObject` and in the seed's apply (`HandSystem::ApplySeedToObject`, both its branches).
- The give: openblack's holding press requires the influence for its apply (`HandSpellSeed.cpp`, `SeedActionPressed`);
  the give does not. **Giving a held object to the creature** (raffclar's PR #387, unverified against v1.42;
  `HandGiving.cpp`, `creature_giving` in `Creature/CreatureGiving`): over the player's own creature or one whose player
  is allied to them (`creature_giving::IsFriendly`, as `InterfaceValidToGiveObject` 0x47A357), when it could pick
  the held thing up wherever it is (`CanPickUpIgnoringMap`) and the thing is not a spell seed, the hand offers it for
  1100 ms after leaving the creature, the thing turning half a turn over (palm holds) or a quarter (side, tree and
  villager holds), the other way for a right hand, back once less than 300 ms is left. The press itself gives it, as
  `ActionPressedHolding` 0x5D1560 does (no hold to wait for; until 2026-10-19 both trees waited 225 ms), to the offered
  creature first, else the one under the hand, ahead of the press's apply and release: the Give packet (0x1F), or,
  with an apply packet still waiting (+0x444, sub_5D3080), nothing sent and the press used up all the same
  (`creature_giving::OnPress`, test `CreatureGiving.ThePressItselfGivesAndAWaitingApplyUsesItUpWithNothingSent`); the
  release then neither puts it down nor throws it. The creature drops what it did, turns to the camera curious and reaches with
  animation 86; within 500 ms of its take moment (`catchTimes[0]`) the hand is drawn to the creature's hand, and moved
  more than 15 creature sizes away it calls the giving off (packet 0x52). The giving lasts until the creature takes
  it, it is called off, or the creature is gone (`creature_giving::KeepOffer`, `givenStillThere`), whatever the
  creature does about it: a take refused, failed or given up leaves it on. While it lasts a press gives it again to
  that creature (`GiveTarget`), and over another creature that would take it the giving goes on with that one. Until
  2026-10-19 it ended as soon as the creature no longer meant to take it, and a press gave nothing while it lasted.
  Test `CreatureGivingHand.APressWhileOfferingGivesItToTheCreatureOffered`. At the moment the creature takes it the hand
  lets it go into the creature's hand (`HandOver`). Given something, the creature keeps that it has to decide about it
  (`CreatureMindState::useWhatItHolds`, in place of a stroke's, a slap's or a fizzle's next thing to do, and no
  mushroom's high while it waits): the first turn it is free to choose (`CreatureMindSystem::UseWhatItWasGiven`, in the
  free choice of `ProcessTurn`, which then plans nothing else that turn) its desires choose for what it holds then,
  however the take ended (taken, refused while holding something, failed or called off; nothing held, nothing):
  curiosity examines, hunger eats, wanting to play with the player throws it at the camera; with no desire a tenth
  strong, the first of a kind is examined and later ones acted on as when led to them. Each desire scores its value times how useful the thing it
  holds is to act on (`creature_planner::ActOnUsefulness`, held, so 1.6 times the tree's usefulness, times curiosity's
  factor, 0 beyond a kept area), as the original's gift choice (sub_50A380, 0x50A640..0x50A652: the desire's value at
  mind +0x150 + 4d times `GetObjectToActOnUsefulness` 0x4E9A70; the routine is the mind's next action +0x21D0, set by
  the give path fn_0047A1D0 (0x47A2F4) and by `ForceActOnObject` (0x4C6207), run on the carried object, so its
  distance priority is the held 1.6). Test `CreaturePlanner.AGiftIsWeighedByHowUsefulTheThingItHoldsIsToActOn`
  (oracle). Until 2026-10-19 it chose in
  the turn the take's step ended, only for the thing given and only when it had taken it. Test
  `CreatureMindSystemTest.GivenSomethingItDecidesAboutWhatItHoldsOnceFreeToChooseTakenOrNot`; the creature spawner's
  Hands part shows the decision waiting. The development flag the take sets is not kept: openblack's stages do not move
  on by their criteria ([development phases](../project/progress/creature/development_phases.md)). The original gives at the press itself (state 15, above);
  openblack, as raffclar's, after the grab's wait. The can't-pick-up help message (7) is not raised yet. Debug: the
  testbed's `hand take` / `hand give` commands and the `creature.give_food` / `creature.give_villager` scenarios; the
  creature spawner's Hands section shows the offer.
- **A pot, a tree, a dead tree or a fence let go over a building site** (2026-10-24): `HandSystem::Release` first
  asks `ecs::held_apply::ApplyHeldToMapCoord`. `BuildingSiteTakingReleased` takes only the classes the pure
  `ReleaseLooksForBuildingSite` names (`HeldKind` Tree, which is also a magic tree; DeadTree, also a felled one; Pot;
  Fence, a static object of a fence's mesh, `physics::from_hand::IsFence`), then `BuildingSiteAt` at the object's own
  place (the map cells' fixed list of its cell, the pure `BuildingSiteAmong` choosing the last building-like object not
  both built and repaired whose collide data meets the 0.3 m circle, or has none), then `ReleasedGoesToSite` with
  `resource_stores::IsResourceStore` for `object_resources::GetResourceType` (the pot's own, wood for the others). The
  object leaves the hand as a store's gift does (an uprooted tree's roots go, openblack's own, as on the press) and
  `resource_stores::DeleteObjectAndTakeResource` takes it (3), with no reaction, put-down or throw; an indestructible
  fence too, as the original asks nothing about it here. An animal, a gate stone and any other object are put down or
  thrown as before. Tests `HeldClassApply.APointIsWithinTheBuildingSiteOfTheLastBuildingGoingUpWhoseShapeMeetsIt`,
  `HeldClassApply.APotLetGoGoesToTheSiteUnderItOnlyWhenTheSiteStoresItsResource`,
  `HeldClassApply.ATreeADeadTreeAPotAndAFenceLetGoLookForABuildingSiteAndNothingElseDoes`,
  `HeldApplyTest.AHandPotLetGoOverAPitGoingUpGoesIntoItAndNotOverABuiltOne` and
  `HeldApplyTest.ATreeADeadTreeOrAFenceLetGoOverAPitGoingUpGoesIntoItAsWood`. Debug: the Land Island window shows
  whether a building site is under the held object, and whether it takes the object if it is let go now.
- **Giving a held object**: `ecs::held_apply` (`HeldClassApply`, `ValidToApplyThisToObject`, `ApplyThisToObject`) on
  `ecs::resource_stores`; `HandSystem::ApplyHeldToObject` takes the object out of the hand as a release does, drops an
  uprooted tree's roots (openblack's own), then the store takes it. The cursor object is `_cursorObject`
  (`HandPlacement.cpp`, the same pick).

### Tapping in openblack

Each owner registers its classes in `ecs::hand_tap` (`src/ECS/Systems/HandTap.h`):

| Class | Registered by |
|---|---|
| Rock | hand, through `object_physics::` (owner: physics) |
| Abode | hand, through the abode knock system (`Locator::abodeKnockSystem`) |
| TownCentre | hand, through `abodes::InterfaceValidToTap` (`abode_knock::IsTappable`) |
| SpellIcon, OneOffSpellSeed | hand, through `worship::` |
| CitadelEntrance | citadel, through `worship::citadel::` |
| ScriptHighlight | hand, through `ecs::script_highlight::` |
| LeashObj | hand, through `worship::temple_leash::` |
| MagicFireBall | hand, through `magic::fireball::Catch` |

- Not ported: PuzzleTotem, Scaffold and Reward's taps, and a rock's deed on a tap.
- **The village centre**: until 2026-10-14 `abodes::InterfaceValidToTap` said yes for every abode, the village centre
  included, so a press on one was a tap (the press's tap-only branch, the tapped object remembered, the help profile's
  Tap event and the empty inhabitants' loop). Now it says no for an abode whose ABODE_TYPE is the town centre
  (`abodes::TypeOf`, the type that makes the TownCentre class), as `abode_knock::IsTappable`.
- **The posts**: the stones' and the posts' spheres are one test, `ecs::hand_pick::InvisibleSphereAlong`
  (`HandSystem::PickObjectAlongRay`), and the press's tap-only objects take a post valid to tap.
- **A sign**: the press on a tap-only object under the cursor (`HandSystem::Update`, with abodes and the temple's
  entrance) also takes a sign valid to tap; until 2026-10-08 it took only abodes and entrances, and since a sign is
  never hovered for a pick-up, no press reached its tap (only the OPENBLACK_TEST_TAP_SCROLL hook did).
- **The knock**: the hand's Abode handler (`HandSystem::RegisterTapHandlers`) calls `AbodeKnockSystem::Tap`
  (raffclar's `AbodeKnockSystemInterface` and `abodeKnockSystem` slot, with openblack's tap behind it). It runs from
  `ApplyTap` with the next turn's tap packet, plays `Ctap_house` at the hand's own position for the local status, and
  the knock at the status's point. Until 2026-10-14 `AbodeArchetype::Create` registered the Abode class a second time;
  `hand_tap::Register` keeps one handler per class and `Find` answers with the first, and the hand's own registration
  always came first (`HandSystem::Initialize`, from `ResetRegistryForNewLand`, runs before a land or the testbed makes
  an abode), so the second never ran. It is gone.

### The tug in openblack

**The user chose the original's rule (2026-10-16).** openblack's earlier rule (2026-09-30: the hand's horizontal
distance from where it grabbed, a lean of up to 0.25 rad and the uproot at weight/1000 m) is gone.

- Ported as pure functions in `Hand/HandGrabRules` (`TugPlaneAt`, `TugHandOnPlane`, `PullAt`), pinned in
  `test/hand/test_hand_tug.cpp`. The stretch's zoomer is `Zoomer::UpdateInlineValue`. The turn: openblack keeps r × F
  and passes the opposite axis, the same arithmetic negated, bit for bit (as `physics::Body` does). The axis-angle
  matrix (the original's fn_007FB180) is pinned by the test `GameMatrix.AxisAngleIsTheOriginals`.
- `HandSystem::BeginTug` sets the plane (`hand_grab::TugPlaneAt`), the tree's matrix (`TugOf`: its rotation's axes
  times its scale), the first grip (`HoldDistance`) and the stretch at 1; `HandSystem::UpdateTug` waits for the hand's
  tug state (3), steps the fade (`StepTugFade`), then each frame takes the hand from `TugHandOnPlane`, runs `PullAt`
  (comes free → `Uproot`), moves the stretch, grips again at 0.1 × height, and draws the tree with a `HandDrawPose`
  (`TugRotation`, `upStretch`) and the hand at `TugHandPoint`. Tests: `test/hand/test_hand_tug.cpp`; the testbed's
  `hand.tug_at_grip`, `hand.tug_at_crown`, `hand.tug_let_go` and `hand.tug_heavy_tree`.
- **⚠️ approximate** the plane is taken at the press, with the press frame's eye and the hand as the frame before
  placed it; the original takes them at its state's Enter, the next frame, with the hand as the press frame's NORMAL
  state placed it (openblack's hand already takes its tug placement on the press frame: its placement follows the held
  object rather than the hand's state).
- **Drawn only**: openblack's lean is a `HandDrawPose` on the tree, and the Transform is untouched. Tree::Draw's bend /
  sway matrix is skipped for an object posed by the hand or the physics; its grey always applies.
- The pile of roots and the uproot's own smoke: `ecs::ground_marks::Create` from `HandSystem::Uproot`. Making the mark
  throws up its dust, `disappear_smoke::Create(position, 1, 1.0, -1)`, before the mesh is looked up, as the original's
  mark (fn_00825240 ends with `SmokyStuff::Create(pos, 1, 1.0f, -1)` at 0x8252EB): 15 puffs, six CRT `Random` draws
  each (three for the place, the turn, two for the direction; mode 1 has no speed draw). Test
  `Explosion.aGroundMarkThrowsUpItsDustWhereItIsMadeEvenWithoutItsMesh` (2026-10-24).
- **BigForest**: `HandSystem::TakeTreeFromForest` (HandTrees.cpp), `BigForest::wood`.

### Dropping and replanting in openblack

- Everything goes through the physics (`physics::from_hand::InitialisePhysicsFromHand`, `HandPhysics.cpp`).
- The forest on replanting: openblack already takes the village's forest from its list (`ecs::TownForestId` walks it,
  `HandTrees.cpp`); before, it did not model the list and the first tree planted in a village created its forest. The
  white smoke is openblack's grab dust. The town of a thing within reach (`HandSystem::Replant`'s search) is an
  abode's (storage pits and town centres included), a field's, a fish farm's, a totem statue's, a store pile's and,
  since 2026-10-21, a town desire flag's (its town;
  [the desire flags](../beings/towns.md#the-desire-flags)); any other object has none.
- The replant's deed: `HandSystem::Replant` publishes it (`creature_mimic::Consider`, the body's player as its
  alignment line) after the spot visual and before the tree goes back in its cell. openblack runs the replant's steps
  in the original's order (`tree_replant::k_Order`): the smoke first, the forest, the spot visual, the deed, then the
  alignment (2026-10-16).
- No hand put-down particles: openblack has no `StartMultiPutdown` (only the particle names in
  `src/Particles/ParticleTypes.cpp`). `PutDownHandPot` (HandResources.cpp) reads the pot row's resource type, which
  equals the original's per-class type for every row of the shipped info.dat; its early return for a type that is
  neither food nor wood cannot trigger on the shipped data.

### Where it is in the code

- `HandSystem.cpp`: `HandSystem::Initialize` makes one hand per player, at `Side::Left` (unmirrored; `Side::Right`
  stays `entt::null`). `HandSystem::SendTap` remembers the object (`RememberTapped`) and pushes the tap packet,
  applied at the next turn's start (`ApplyTap`, `HandTurn.cpp`). `HandSystem::OfferScreenObject` takes a screen
  object's offer when it is nearer than the pending one, and changes nothing while the hand grips one.
  `HandSystem::ValidForPlaceInHand` uses `object_physics::IsRockObject` for the rock's 3.6 rule.
- `HandHolding.cpp`: `HandSystem::ThrowObjectFromHand` is the physics' part of the throw, with the object still in
  the hand (the caller's `HandleApplyResult` takes it out). `HandSystem::UpdateThrowBlock` keeps the throw block: 0
  while the spring works, and the grain's smoothed velocity and sideways tilt (⚠️ approximate: the original rounds
  its two rates to float once, here the double is cast at the end). `HandSystem::HeldSway` is the held object's sway
  from the mouse and the camera (`at`, the grip, stands for the original's hand point: ⚠️ approximate).
  `HandSystem::ComputeHoldParameters` picks the hold type; a one-shot orb holds as `HoldType::Side`. With
  `HoldType::None`, or while the object is the prediction's, the hand does not draw it.
- `HandApplyToObject.cpp`: `VillagerValidToApplyThisToObject` (a totem or a teleport; the totem's sacrifice is not
  ported). `HandSystem::SendSeedOrStonePickup` sends the stone itself in packet 0x13. `HandSystem::HandleApplyResult`
  maps an apply's result and lets the object go. `HandSystem::RemoveFirstFromHand` empties the hand without physics
  and clears the gesture buffer.
- `HandTrees.cpp`: `HandSystem::MakeDeadTree` turns the tree into a DeadTree, out of and back into the map cells.
  `HandSystem::DropRoots` and `HandSystem::UpdateRootsAndPiles` drop the dangling roots from the tree's Transform after
  `MakeDeadTree` and fade them (Pending: the DeadTree's own angles). `HandSystem::CreateTreeFromForest` makes the
  BigForest's Conifer at the forest's position.
- `HandPhysics.cpp`: `HandSystem::RegisterPhysicsHandlers` registers the tree's physics handlers: the impact's
  resource take, the end of physics (replant, DeadTree, or left where it fell for a forest miracle's tree) and the drop
  sound.

### Test hooks

- `OPENBLACK_TEST_TUG="x,z,wait,hold"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`, the cursor moves 0.5 s later), traces with
  `OPENBLACK_HAND_TRACE=1` (`Tug trace`: the hand on the plane, |F| against the weight, the stretch).
- `OPENBLACK_HAND_TEST_FOREST=1`: takes a tree from a BigForest (Land1: 15000 → 14650, scale 0.977).
- `OPENBLACK_TEST_TAP_SCROLL`: taps a scroll.
- Testbed scenarios `hand.tug_at_grip`, `hand.tug_at_crown`, `hand.tug_let_go`, `hand.tug_heavy_tree`.

## Pending

- **The last dropped object's other readers** ([Out of the hand](#the-release)): `Living::HasSunk`,
  `Villager::HasSunk`, `Villager::Landed`, `Tree::EndPhysics`, `Town::AddArtifact`, `WorshipSite::AddArtifact` and
  `Object::EndPhysics`'s landing mimicry
  do not read `ecs::last_dropped` yet (each is ported with its own record of the player, or not at all). The status's turn
  clearing the mark of an object no longer available is not ported: a destroyed entity never matches (its version
  changes), so only a reader asking about a thing that is going (deleted or a dying villager) would see it.

- **What is done with a gift** ([What the creature does with a thing given](#giving-a-held-object)): the weighing
  follows the original since 2026-10-19 (`creature_planner::ActOnUsefulness` on the act-on tree, the held distance
  priority and the desire's factor). The seed's cast and the plan carried on with the gift are not ported (`UseGift`'s
  TODOs), nor the kinds it learnt it can't eat.

- **The tug state of what is not a tree** ([Grab press](#grab-press)): the original's hand is TUG 3 while the tug
  hold +0x4908 lasts, for a rock, a villager or anything else picked up with it. openblack's
  `HandSystem::RequiredHandState` gives 3 only while a tree is tugged (`_tug`), so during `_pendingTugHold` the hand
  takes HOLDING's pose and placement, not the tug's (the ABOVE time of `TugTimeMs`, the hand on the mouse ray at the
  grab's depth 0x5B80B9..0x5B81C2, the grip following the drawn lean), and the state blends at its start and end
  differ. Porting it needs the tug state's placement for any object (queued fix F-82).
- **The throw from the hand (#134)** ([Holding, spring and throwing](#holding-spring-and-throwing)): the body is now
  built at the drawn pose (`PredictRelease` takes it; `SetUpBody` still reads the Transform, so `PredictRelease` sets
  the pose there for the setup and puts the turn's place back). Still open: the 0.35 smoothing of the drawn object's
  velocity in `HandSystem::UpdateHeldObject` while the spring is off is not the original's (+0x48C8 keeps its last
  value then); no throw reads it today. +0x48E0 (the drawn position + 0.2 v) is not written by openblack's Holding;
  whether a held object other than a seed reaches SendApplyToMapCoord's block path has not been read (queued fix
  F-94).
- **The give's influence** (F-79, [Applying the held object](#applying-the-held-object-to-an-object)): the original's
  holding press gives with no influence test (0x5D15A4..0x5D15CC) and needs the influence otherwise only when the
  target's vt 0x714 is 1; openblack always requires it. Queued as a fix.
- **The hand's put-down particles** (F141e, [The release](#the-release)): `StartMultiPutdown` 0x68FAE0 on a hand pot
  let go gently, for the local interface; not ported. The velocity a creature's `DoDiscardAction` passes is not read.
- The hand passes through some rocks when grabbing them: when it happens is still unknown (needs a screenshot). See
  [Grabbing](#grabbing) (queued fix F-94).
- The prediction's angular velocity (the inertia tensor × the hand's) and the argument order of `GetYXZ` are
  🔍 inferred; the morph of the hand's mesh by alignment at neutral equalling the base clip is 🔍 inferred.
- **A hand pot's size**: the hand pots are piles (HandFood potType 1, HandWood 2), so their size is
  `PileResource::SetSize` 0x66E900 (the sink), not `Pot::GetScaleFromAmount`; openblack's `PotArchetype::SetSize`
  leaves HandWood and HandFood alone. Whether the pile's sink offset shows when the held pot is drawn out of the map
  (vt +0x614) has not been read. raffclar's `ResizePot` (his 73eea0f5) returns early for any pile, so it changes
  nothing for the handful.
- **`Pot::GetScaleFromAmount` with a scaleEvery of 0** (0x66D4A0): the original divides as x87 floats, so an amount
  over 0 gives +inf and then 5, and an amount of 0 gives NaN, which the unordered compare keeps (the jump at
  0x66D4DA); openblack divides by max(1, scaleEvery), raffclar's `PotScale` gives 5. Whether any simple pot in info.dat
  has a scaleEvery of 0 has not been checked.
- **The locked apply's exit test**: 0x5D4CB0 sends the unlock only for an object in the hand whose byte +0xA bit 0 is
  clear; what that bit is is not identified. openblack sends it for any seed in the hand.
- **The held object's matrix** (fn_0046E2F0): the game runs F = 1, {−side, up', up' × side} with CHand +0x484C; the
  scale S = 3.2 / +0x8C × +0x4834 × +0x90 of fn_0046D080 that fn_0046D9D0 puts in front; a seed's info +0x154 for
  `GetHoldYRotate` (vt 0x598). Not ported.
- **The sacrifice**: the WorshipTotem branch of `Villager::ValidToApplyThisToObject` (`ApplyThisToObject` 0x752C40 up
  to 0x752FB0) is not ported (with the sacrifice's port row).
- **`PlaceObjectInMagicHand`** 0x5DA6F0: two flags, named by bw1-decomp's (fabricated) enums, GameThingWithPos +0x24
  & 4 UNAVAILABLE_FOR_STATE_CHANGE (0x5DA726) and & 0x20 LOCKED_SELECT (0x5DA75C); the other re-entrant classes
  (Field, FishFarm, FieldCrop, PileResource) and `FireEffect::StartedMoving` on the object's fire (0x5DA79F); the
  reaction 0x10 (0x5DA7DD) and `HelpProfile::Trigger` 2 / 3 in PickUp: not ported.
- **The removal from the hand**: a deleted held object: `CHand::GameTurnUpdate` 0x46E4E0 (step 28) also drops it the
  same turn (IsAvailable != 1 → CHand::ThrowObject), for a deletion after step I; not ported (openblack's turn takes
  it out in `HandSystem::ValidateHands` only). The other classes' vt 0x704, which `GMagicHand::RemoveFromHand`
  0x5FB0B0 calls, are settled (read 2026-10-22, [clearing on a new land](states.md#clearing-on-a-new-land)): every
  class but the seed, the puzzle grain and the scaffold keeps the object's own, the fire's `SetOutMagicHand` alone
  (the villager's too), so `HandSystem::RemoveFirstFromHand`'s fire call and its scaffold surplus
  (`scaffolds::OnLeftHand`) are the whole of it.
- **A release during the wait** (state 7, `State_WaitPickup` 0x5D4A90 only watches the object's +0x24 & 4) after
  GenericPickup: not verified.
- **The tug's stretch in the draw** (`HandStateTug::Update` 0x5B891D..0x5B892B, at most 1.3): that the shadows and the
  fire, which go through DrawnModel (FireGraphic.cpp), take the stretch too is 🔍 inferred.
