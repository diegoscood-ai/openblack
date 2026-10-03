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
