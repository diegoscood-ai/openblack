# Picking: the object under the cursor and the near object

## Summary

Each drawn frame the original tests every drawn object against the mouse pixel: most by their triangles, the
villagers by the circle of their bounding sphere on screen, the trees by their drawn pixels so that the transparent
parts of a leaf card are not under the cursor. The nearest by view depth wins, the land counting 2.3 more. A press that
finds nothing takes the nearest object within 5 m of the land behind the hand. The interface keeps an **action
point**, the collided object's position or the land under the cursor, which the press, the release, the leash and the
influence test read; and it remembers the last thing tapped for 15 seconds for the scripts.

openblack picks along the mouse ray (`HandSystem::PickObjectAlongRay`), with the trees' pixel test, the villagers'
sphere, the temple's entrance and the press fallback ported; the action point and the tap memory are the original's.
Other objects are still picked by their nearest triangle along the ray, not the first that holds the pixel. Where the
hand is then placed is on [The hand: animation and placement](animation-and-placement.md#where-the-hand-is-placed).

## The original

### Object under the cursor

- `GInterface::SendObjectDrawCollision` (0x5D56C0): every drawn object is tested with exact per-triangle collision
  (trees with a per-pixel test instead, see [A tree under the cursor](#a-tree-under-the-cursor) below); the closest
  wins; whatever is in the hand is ignored.
- **What it takes** (0x5D56C0..0x5D56ED): nothing while an action is in progress (+0x38, 0x5D56CD); otherwise only an
  object that is `IsInteractable` (vt 0x190, 0x5D56DB: available, an abode also partly built, `Abode::IsInteractable`
  0x407200) and without +0x24 & 4 (0x5D56E9).
- **Villagers** ("human" flag of the LH3DObject, only `Villager::CallVirtualFunctionsForCreation` 0x74FC70): no
  per-triangle test. They count if the mouse is inside the **on-screen circle of their bounding sphere**
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80; radius = scale × half-diagonal of the box), at distance
  `|(x, y + halfHeight·scale, z) − camera| − (R + near plane)`, measured from the eye `g_camera` (0x5D5890..0x5D58C2).
  The near plane is the drawn one, [0xE839E0] (read at 0x5D58CA), which `GCamera::Update` writes every update from
  `LandFeature::GetNearClipping` (0x5E2F30, at 0x4424AF): (h·0.05)·3.2 + 0.3 in float steps over the camera's height
  above the terrain, 0.3..3.5, 0.1 under `SET_GRAPHICS_CLIPPING`. The other draw collisions sent without a triangle
  distance (the branch 0x5D58DC..0x5D5942) take the distance near / [0xF05164] (0x5D5944..0x5D5950), the drawn near
  plane too; [0xF05164] is the view depth the pixel or triangle test leaves (below). Animals and creature: triangles,
  like everything else. Boned meshes (villagers, animals) are tested as they are drawn.
- `UpdateInterfaceCollide` (0x5D5A70): the terrain distance counts 2.3 more (fn_005D5980); if it is still in front,
  the object only counts if the terrain point falls inside its XZ footprint. Both distances are view depths (the w of
  the world-to-clip matrix: the land point's w from fn_005E5620 0x5E56F6..0x5E5719 plus 2.3 at 0x5D59BF, against the
  object's pending +0x404 at 0x5D5ABB..0x5D5ACE), so the 2.3 is a depth, not a length along the cursor's ray.
- If there is nothing when clicking: `FindObjectNearMapCoord` (0x5D39E0), the closest within ±5 units and only if it is
  closer than the clicked point; first the fish of a fish farm if it is water. **It is not a hover range**.
- **The second land test** fn_00848F90 (+0x414, 0x5D5C7F, under [[+0x39C] +0x12C] +0x24): the leash rope's pick.
- **Everything that moves with the hand** (held object, tree being uprooted, roots, particles) is left out of the
  pick: otherwise the ray hits it and the hand climbs towards the camera endlessly.
- The message for the amount in the hand ("Cantidad: N", font `j0`, yellow with shadow, no background) is shown while
  the hand holds food or wood: see
  [Text: the original's fonts and the hand message](../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message).

#### A tree under the cursor

There is no forest-wide or patch pick: a tree is under the cursor only where its own drawn pixels are opaque, and
nothing in the cursor search resolves a Forest, a forest's area or its cells to one of its trees.

- `Tree::Draw` 0x74AB00 hands the tree itself (not its forest) to `Game3DObject::AddForDrawing` 0x63B5D0
  (0x74B0C7..0x74B0CC), which sends `SendObjectDrawCollision(tree, 0, NULL)` 0x5D56C0 only when the 3D object's
  on-screen test marked it (`g_last_selected_box`, 0x63B653..0x63B66E).
- `SendObjectDrawCollision`, with no distance given, tests the object's info type (0x5D58DC): type 6
  (`OBJECT_TYPE_FOREST_TREE`, every `Tree`; `Object::IsPartOfForest` 0x6380F0 is the same test) first refreshes the 3D
  object's matrix from `GetWorldMatrix` (vt +0x63C) when the tree is in the map (0x5D58E9..0x5D58FB), then asks the 3D
  object's vt +0x1E4, `CheckPixelCollide` (0x5D5905). The 3D objects with the human flag (villagers) took the sphere
  test before (3D object vt +0xB0, 0x5D585A); an abode with +0x90 set asks fn_005D5E80 (0x5D5917..0x5D5922); every
  other object asks vt +0x1E0, `CheckTriangleCollide` (0x5D592C / 0x5D5938). A miss sends nothing (0x5D5942..0x5D5954).
  The hit's distance is the near clip over [0xF05164] (0x5D5944..0x5D5950), the view depth at the mouse pixel that the
  pixel test leaves there.
- A tree's 3D object is an `LH3DStaticObject` (vtable 0x9A2974; (🔍 inferred) from `LH3DObject::Create`'s type 0, not
  traced from the tree's creation): vt +0x1E0 = fn_00811300, vt +0x1E4 = fn_008111C0. The two walk the same sub-meshes
  and primitives in the same order (status mask, LOD); they differ only in the primitive's test: fn_00855440 (triangle)
  against fn_008561D0 (pixel).
- fn_008561D0 projects the primitive's vertices to the screen, keeps the triangles facing the camera (both sides when
  the primitive is double-sided), and for each one that holds the mouse pixel (0x856877..0x8569C6) fn_00856B30
  interpolates its u, v at that pixel, perspective-correct, and writes near clip / z there ([0xF05164]). With a
  material texture it then asks fn_00838A70(u, v), and the first triangle that passes is the hit: a texture of kind 2
  (a mesh pack texture, read back as ARGB 4444) is hit only when its 64 x 64 alpha mask (fn_00838F00, built once: the
  high nibble of each sampled texel, texture +0x12C) is not 0 at (u x 64, v x 64), each clamped to 0..63; any other
  kind counts as hit. So the transparent parts of a tree's leaf cards are not under the cursor.
- **Projection** (0x856218..0x8563A6 and 0x85648C..0x85662E): x, y and w through the world-to-clipping matrix;
  outcode 0x20 when w < near clip, 0x10 when x > w else 8 when -w > x, 4 when y > w else 2 when -w > y. Only a vertex
  with outcode 0 is projected: 1/w stored, x' = (x/w + 1) x half width clamped to 0..g_MaxScreen x (639), y' = half
  height - (y/w) x half height clamped to 0..479, rhw = near clip x 1/w; u, v copied.
- **Triangles** (0x856690..0x85685D): in the primitive's order. All three outcodes 0: kept, after the facing test when
  the primitive is not double-sided (byte +5 bit 0 clear, g_NoBackfaceCull): (c.y - a.y)(b.x - a.x) - (b.y - a.y)
  (c.x - a.x) > 0. All three sharing a bit: dropped. Otherwise fn_0081A760 clips the triangle against the planes from
  0x20 down and appends the pieces in its place, with the same facing test (0x81A99C..0x81AA7E).
- **The pixel in the triangle** (0x8568A0..0x8569C6): the mouse (integers [0xEA1AC8], [0xEA1ACC]) as floats; each
  corner minus the mouse stored as floats; cross(d0, d1) > 0 needs cross(d1, d2) and cross(d2, d0) both >= 0,
  otherwise both must be <= 0. So the edges count, but on the first edge (cross 0) a triangle wound to the positive
  side loses its pixels.
- **u, v at the pixel** (fn_00856B30): the corners sorted by y (top, middle, bottom, in the order of its compares), the
  long edge top to bottom and the short one (top to middle while the mouse row is above the middle corner, else middle
  to bottom) at the mouse row, each with rhw, x, u rhw / rhw and v rhw / rhw; then along the row between the two
  (s = (x2 - mx) / (x2 - x1)). rhw at the pixel to [0xF05164], u to [0xF05170], v to [0xF05174]. The short edge's v
  stays on the float stack, unstored.
- **The mask** (fn_00838A70, fn_00838F00): kind = texture +0x10 & 0x3F. Column = __ftol(u x 64), row = __ftol(v x 64),
  truncated, each clamped to 0..63 (no wrap); hit = byte (row x 64 + column) != 0. The mask (4096 bytes at +0x12C,
  built at the first test): byte (r, c) = the high byte of the 16-bit texel (r h / 64, c w / 64), both truncated,
  & 0xF0.
- **Which texture**: each primitive's own material texture (primitive +8); no texture counts as hit. Kind 2 is the
  mesh pack's textures (`_SetPackedTexture` 0x8377E0 makes them with fn_008379E0(0, 2, ...)); the lock for the mask
  (fn_00838AF0, kind 2 at 0x838B30) blits the texture into a system-memory surface of the device's first 16-bit alpha
  format (table 0xF05190, filled by fn_0085DC20, which settles on A 0xF000 R 0xF00 G 0xF0 B 0xF when the device offers
  it), so the high nibble is the ARGB 4444 alpha. The pack's textures are DXT1 (133), DXT3 (17) or DXT5 (74) in
  AllMeshes.g3d. Which of a tree's primitives have transparent texels is the data's, not read here.
- **Depth**: near clip / [0xF05164] = the view depth w at the pixel, of the first triangle that passes (not the
  nearest of the tree's).
- **The triangle test** fn_00855440 has the same projection, clipping and pixel test on the mouse pixel and asks
  fn_00856A00 (rhw only) instead: every other object is also hit by the first of its triangles that holds the pixel.
- **What the depth is used for.** Every distance the cursor search compares is a view depth, the w of
  `g_world_to_clipping` (0xEA9E40), but for the villagers' sphere:
  - `SendObjectDrawCollision` keeps an object only when its distance is strictly less than the pending one's
    (GInterface +0x404), both with a distance given (0x5D570E..0x5D5732) and with the pixel or triangle test's
    near / [0xF05164] (0x5D5956..0x5D5965); `fn_005D5E40` (0x5D5E40, called at 0x5D5744) stores the object at +0x400
    and its distance at +0x404, or clears both (0, FLT_MAX) when the object is not interactable. With an object
    pending, a mesh whose bounding sphere's centre lies deeper than the pending distance + its radius x scale is not
    tested: the centre's camera z, from the camera's axes at [0xEA1D30], [0xEA1D3C], [0xEA1D48] + [0xEA1D54]
    (0x5D5770..0x5D5854; radius 1 at the 3D object's position when it has no mesh). The villagers' distance,
    |centre - camera| - (radius x scale + near) (0x5D5872..0x5D58D2), goes through the same strict compare.
  - The land's distance is a view depth too: `fn_005E5620` gives the land point under the mouse and its w,
    x [0xEA9E48] + y [0xEA9E54] + z [0xEA9E60] + [0xEA9E6C] (0x5E56F6..0x5E5719), the row the pixel test's projection
    takes w from (its matrix is `g_world_to_clipping` times the object's, 0x8563CF..0x856472); `fn_005D5980` adds 2.3
    (0x5D59BF) and stores it at +0x3FC (0x5D59E5). `UpdateInterfaceCollide` compares the object's +0x404 with it
    directly (0x5D5ABB..0x5D5ACE) and, the object chosen, copies the object's Pos to +0x3F0 and its distance to +0x3FC
    (0x5D5D09..0x5D5D2B). The bubble's depth (+0x40C) is compared with the same +0x404.
  - The hand is not placed at that depth. `ObtainRequiredHandPosition` takes the object of the render collide (+0x3C8)
    and tests its mesh again along its own ray from `g_camera`, the mouse direction or, while holding, the raised one:
    fn_00865020 (0x5B6529), whose hit point gives the hand's distance, s = -(camera - point) . dir (0x5B65A9..0x5B65ED);
    when it misses (0x5B6530 to 0x5B6852) the hand keeps the point at the view distance along the mouse ray and takes
    the land's up. See [The hand's mesh test](animation-and-placement.md#the-hands-mesh-test). No hand code reads the
    collide's distances +0x3C4 / +0x3CC (a scan of every instruction with those offsets: only their writes).
  - So for a tree: the first passing triangle's depth takes the cursor (against other objects and the land), and the
    hand goes to the nearest of its triangles along the ray, whatever their texture.
- There is no draw of the Forest object (a `GameThingWithPos`); a `BigForest` is a `MultiMapFixed` with a mesh of its
  own, collided by triangles like any other object (`BigForest::Draw` 0x438F60 jumps to `MultiMapFixed::Draw`
  0x518090), and taking it gives a new Conifer
  ([Pick-up rules and BigForest](grab-hold-and-throw.md#pick-up-rules-and-bigforest)).
- The only reach beyond what is drawn is the press with nothing collided: `ActionPressed` calls
  `FindObjectNearMapCoord` 0x5D39E0 with the action collide's point (GInterface +0x3F0) only when the press has no
  object yet and the action collide (+0x400) holds none (0x5D148F..0x5D14A6). It takes the nearest object of any class
  but a fragment, at its 2D distance from the land behind the hand, under 5 m, and only when that distance is not more
  than the action point's own distance from the land behind the hand (0x5D3CB4..0x5D3CDB). In a forest that object is
  usually a tree: the one nearest to the land behind the hand, first of a tie in the cell walk (x outer, z inner, fixed
  list then mobile). No random draw is made.

#### The temple's entrance

The temple's entrance is an object of its own (`CitadelEntrance`, made by `CallVirtualFunctionsForCreation` 0x4675A0
at 0x4676B9) whose 3D object (type 1, 0x4676CD) holds `Data\Citadel\OutsideMeshes\Entrance.l3d`: the heart's 3D
object's vt +0x204 (0x80BBC0) returns the mesh that `InitTemple` loads once (0x88297A..0x882988, global 0xFAA7E4). It
is placed at the heart's point, angle and scale 1 after the land is flattened (0x46774F). Its draw does nothing;
`CitadelHeart::DrawNow` collides it instead, through `Game3DObject::AddJustForCollide` 0x63B7E0 (0x4672F4..0x46731B:
the on-screen box test, then `SendObjectDrawCollision`, an exact triangle test), only while the per-player flag
0xEB9A1C[player] is set. `LH3DCitadel::SetPercent` 0x883120 sets it when the draw percent is 1 (0x8831AD);
`LH3DIsland::Create` clears it (fn_00828A50). Entrance.l3d is one 50-triangle sub-mesh, x -8.47..-2.32, y 0..6.41,
z -13.89..-7.40 in the heart's frame: the doorway. For the hand it is an Object with the defaults (not placeable,
needs the influence, felt with the mesh), so a press on it taps it as an abode is tapped. Its tooltip is state 18's
([The text of each state](../interface/tooltips.md#the-text-of-each-state)).

- **Why the doorway finds the entrance and not the heart.** Entrance.l3d's 50 triangles are, vertex for vertex, 50
  triangles of the first temple's drawn sub-mesh 3 (b_first_temple.l3d; b_temple00 has them unchanged too): the
  recessed faces of the opening. `DrawNow` collides the entrance (0x46731B) before it sends the heart's own collision
  (0x467336), and `SendObjectDrawCollision` takes a new object only when it is strictly nearer than the pending one
  (0x5D5723..0x5D5732, 0x5D5956..0x5D5965), so on the doorway's tie the entrance keeps the cursor and the hand goes
  into the opening (its hit point is the recessed face).
- Every temple mesh's sub-mesh 0 has status 1: a closed 224-triangle shell, x -19.01..18.93, y 0..21.41,
  z -17.45..17.57, in front of the doorway. The built temple does not draw it, and the collision is of the drawn
  triangles, so it is not collided.

#### Screen objects

- The «Did you know?» bubble: fn_005D5A30 is `if (+0x38) return 1; if (!(+0x40C > depth)) return 0; +0x408 = it;
  +0x40C = depth; return 1` — the nearest offer wins, a tie keeps the first, and while the hand grips one
  (+0x38 & 0x10) nothing changes.
- `UpdateHandRenderCollide` 0x5D0610 (PreDrawProcess 0x5CE9E5): the pending offer becomes this frame's (+0x3D0 =
  +0x408 at 0x5D0657, +0x3D4 = +0x40C at 0x5D0660); unless gripping, the pending one is cleared (0x5CEA5C..0x5CEA97).
- `UpdateInterfaceCollide` 0x5D5A70 arbitrates the offer (skipped while gripping, 0x5D5A9C): the object +0x400 against
  the bubble, the nearer wins (bubble nearer: 0x5D5B77 clears the object; else 0x5D5C0D / 0x5D5C23 drop the bubble).
- A land hit nearer than the bubble (+0x3FC, the land's distance + 2.3, fn_005D5980 0x5D59BF) drops it
  (0x5D5B8F..0x5D5BAA); the bubble's depth is its camera z, compared as it is with the ray distances.

#### What may go into the hand

- `Animal::ValidForPlaceInHand` 0x419B40: the species' playerCanPickUp (info +0x228).
- `Villager::ValidForPlaceInHand` 0x7564A0 = IsReachable (vt 0x530); Villager's InterfaceValidToTap is Object's
  0x4196B0 = 0, so an unreachable villager is not the target at all.
- `BigForest::ValidForPlaceInHand` 0x438DB0 = 1; BigForest is not tuggable (0x438DC0).
- **ValidForPlaceInHand is an allowlist.** It is vtable slot 447 (offset 0x6FC, `int (GInterfaceStatus*)`, `ret 4`;
  all 134 object vtables agree). `Object` 0x402870 is `xor eax, eax`: 0, inherited by every building (Abode,
  TownCentre, StoragePit, Workshop, Wonder, Creche, Graveyard, SpellDispenser, CreaturePen), the totems (TotemStatue,
  WorshipTotem, PuzzleTotem), Feature, AnimatedStatic, Fixed, the citadel parts, the worship and prayer sites, every
  spell icon, LeashObj, the street lanterns and waterfalls. Also 0: Creature 0x473F60 (so the hand never holds the
  creature this way: the creature is the CREATURE state's and the leash's), Bonfire 0x439790, HelpSpirit 0x5C4AF0,
  LandscapeVortex 0x5FF300, Reward 0x6E69A0, ScriptHighlight 0x709770, the shields 0x72C000, Fragment 0x76E940.
  1: `Mobile` 0x425B00 (MobileObject, Living, Arrow, Ball, MobileWallHug, Creed, FieldCrop, Poo, Pot, PotStructure,
  HanoiBlock, OneOffSpellSeed, Whale), BigForest 0x438DB0, SingleMapFixed 0x52EAF0 (Tree, MagicTree, PrayerIcon),
  MobileStatic 0x55D730. Conditional: Animal 0x419B40 and Villager 0x7564A0 (above); Rock 0x6E7030, shared by DeadTree
  and FelledTree, `!(IsRock() && Get2DRadius() > 3.6)` (DeadTree's IsRock 0x5109D0 = 0, so always 1); Field 0x528EF0,
  growth (+0xD0) > 0 and food (+0xDC) > 0; FishFarm 0x52D670, `GetFoodValue(3)` 0x52D1E0 > 0, which for food is the
  info's foodValue (+0x68), not the stock left; PileResource 0x66ECE0 (PileFood, PileWood, MagicFood, MagicWood,
  PuzzleGrain), the pile's info row equals `GetHandPotInfoType()` (vt 0x870: 0x66EC30 HandFood, 0x66EC40 HandWood) and
  `GetResource(GetResourceType())` != 0, so only a hand pile with something in it; Scaffold 0x6E96A0; SpellSeed
  0x728580 and MagicTeleport 0x5FC440 (through its seed); MagicFireBall 0x682DD0, not the status player's own ball. The
  callers (vt 0x6FC): the cursor state 0x5D8030, `ActionPressed` 0x5D14EB, `StartGrab` 0x5D1784, `GenericPickup`
  0x5D281A, `PlaceObjectInMagicHand` 0x5DA73B, MagicTeleport 0x5FC45D, the fireball catch 0x682EB8,
  `TownCentre::Process` 0x743E48, `Scaffold::CanBePickedUpByCreature` / `CanBeStolenByCreature` 0x4E3FC7 /
  0x4E3FF7, `Field::IsEffectReceiver` 0x528904. A village totem is dragged without it: its press is a locked select
  (vt 0x6CC, 0x738500; [The village totem](../magic/village-totem.md)).
- `PileResource::ValidForPlaceInHand` only accepts hand-type piles, so the "scoop min(capacity, amount)" branch of its
  `InterfaceSetInMagicHand` (below) cannot be reached through `PlaceObjectInMagicHand`,
  and vt +0x700 has no other caller (0x5DA77C is the only site; `PileResource::InterfaceSetInMagicHand` 0x66EF80 is
  only in the vtables of PileResource, PileWood, PileFood, MagicFood and MagicWood), so the branch is unreachable.
  fn_005D1260 does not reach it. The callers of `GInterface::PlaceObjectInMagicHand` are
  `GInterfaceStatus::PlaceObjectInMagicHand` 0x5DC87C, `GPacket::ProcessPacket` 0x63C97B and
  `WorshipSpellIcon::PutFullyChargedPowerUpSeedInHand` 0x77F92F.
- **The order of the refusals** in `GInterface::PlaceObjectInMagicHand` before vt +0x700: `IsInteractable` (vt +0x190,
  0x5DA705), `IsSpaceInHands` 0x5DA719, `GameThingWithPos::Flags` (+0x24) & 4 (0x5DA726), `ValidForPlaceInHand`
  (vt +0x6FC) == 1 (0x5DA73B..0x5DA744; all five pile classes use `PileResource::ValidForPlaceInHand` 0x66ECE0),
  `IsCannotBePickedUp` (vt +0x180) == 0 (0x5DA74E), +0x24 & 0x20 (0x5DA75C), then `GameThing::Flags` (+0xA) & 0x10
  (0x5DA766); any failure returns (0x5DA88F).
- **The refusals before the hand**: an object whose `GameThing::Flags` (+0xA) has 0x10 (PSYS_FLYING) is refused; so is
  one with `UNAVAILABLE_FOR_STATE_CHANGE` (Flags & 0x4) or `LOCKED_SELECT` (0x20). A villager with its own +0xE0 & 0x4
  or with `GameThingWithPos::Flags` & 0x2000 cannot be picked up (`Villager::CanBePickedUp` 0x55CA50); flag 0x2000 is
  set by `GameOSFile::LoadInstance` (0x559999..), the puzzle code fn_006D71D0, fn_006DACD0, `HanoiBlock::Save`,
  fn_006DC070 and `PuzzlePig::AddToBoxPositionForChessGame` (a scan of `or byte [r+0x25], 0x20`; the script side not
  searched).

#### The hand's virtual functions of an object

The decompiled reconstruction of the pickup path lists the defaults and the overrides the hand calls (each checked in
the disassembly; the names are the 1.20 symbols):

| Function | Address | Slot | Returns or does |
|---|---|---|---|
| `GameThing::Get2DRadius` | 0x405150 | vt +0x064 | 0.0 |
| `GameThing::GetPlayerWhoLastPickedMeUp` | 0x4018A0 | vt +0x068 | null (no override found) |
| `GameThing::DrawInHand` | 0x405230 | vt +0x0D0 | nothing |
| `GameThingWithPos::GetHeight` | 0x405500 | vt +0x42C | 0.0 |
| `GameThingWithPos::CanBeThrownByPlayer` | 0x5701C0 | vt +0x4BC | a mobile object, a mobile static or any tree (used by the computer player) |
| `Object::GetHoldYRotate` | 0x4025A0 | vt +0x598 | 0.0 |
| `Object::HandShouldFeelWithMeshIntersect` | 0x4025B0 | vt +0x59C | 1 |
| `Object::IsG3DObjectDrawnInHand` | 0x402670 | vt +0x618 | true |
| `Object::CanBePickedUp` | 0x4026A0 | vt +0x628 | 0 |
| `Object::ValidForPlaceInHand` | 0x402870 | vt +0x6FC | 0 (above) |
| `Object::InterfaceSetInMagicHand` | 0x637660 | vt +0x700 | 1 |
| `Object::ValidToRemoveFromHand` | 0x402880 | vt +0x708 | 0 |
| `Object::RemoveFromHand` | 0x402890 | vt +0x70C | 0x17 |
| `Object::ValidToShakeFromHand` | 0x636AA0 | vt +0x710 | 1 |
| `Object::InterfaceMustBeInInfluenceForInteraction` | 0x4028A0 | vt +0x714 | 1 |
| `Object::IsTuggable` | 0x636A90 | vt +0x718 | 1 |
| `Object::ValidToApplyThisToObject` | 0x4028B0 | vt +0x71C | 0 |
| `Object::ApplyThisToObject` | 0x4028C0 | vt +0x720 | 0 |
| `Object::ValidToApplyThisToMapCoord` | 0x4028D0 | vt +0x724 | 0 |
| `Object::ApplyThisToMapCoord` | 0x4028E0 | vt +0x728 | 0 |
| `Object::IsInterfacePowerUpWhenInHand` | 0x402910 | vt +0x734 | 0 |
| `Object::ApplyOnlyAfterReleased` | 0x402930 | vt +0x73C | 0 |
| `Object::InterfaceValidToTap` | 0x4196B0 | vt +0x740 | 0 |
| `Object::InterfaceTap` | 0x4196C0 | vt +0x744 | 1 |
| `Object::InterfaceValidToGiveObject` | 0x402940 | vt +0x748 | 0 |
| `Object::InterfaceGiveObject` | 0x402950 | vt +0x74C | 0x16 |
| `Object::GetPhysicsConstantsType` | 0x6376A0 | vt +0x788 | `CanBecomeAPhysicsObject() != 0` |
| `Object::DropSfx` | 0x63A7B0 | vt +0x794 | 0 |
| `Object::DeleteObjectAndTakeResource` | 0x63A930 | vt +0x684 | false |
| `Object::ReactToPhysicsImpact` | 0x6377C0 | vt +0x7AC | nothing |
| `Object::CanBecomeAPhysicsObject` | 0x6377E0 | vt +0x7B0 | 0 |

The overrides:

- **Fixed** (`MultiMapFixed`, `SingleMapFixed`, `MobileStatic`, `Rock`, `DeadTree`, `FelledTree`, `Bonfire`...):
  `Fixed::InterfaceSetInMagicHand` 0x52E0E0 takes a town artifact out of its town and gives it the status's player,
  then returns 1. `Fixed::EndPhysics` 0x52DF50 makes a fixed object thrown from the hand by a player and landed within
  50 of an abode or a worship site an artifact of that town (`fn_0073FDA0`, the town's `AddArtifact`) or worship site
  (`WorshipSite::AddArtifact`), with a look-at reaction when the artifact's checks pass (fn_00426880 for a town,
  fn_004268B0 for a worship site). `SingleMapFixed::ApplyOnlyAfterReleased` 0x52EB20 is 1;
  `SingleMapFixed::ApplyThisToMapCoord` 0x52F420 throws it from the hand (vt +0x758, not replanted) and returns 0x16,
  the same body as `Mobile::ApplyThisToMapCoord` 0x606BF0.
- **Mobile**: `ValidForPlaceInHand` 0x425B00 = 1, `ValidToApplyThisToMapCoord` 0x416F50 = 1,
  `ApplyOnlyAfterReleased` 0x416F60 = 1.
- **MobileStatic**: `MobileStatic::ApplyThisToMapCoord` 0x608B30: a fence in a building site that stores its resource
  is taken by the site (returns 3); otherwise thrown from the hand, 0x16.
- **MobileObject**: `MobileObject::GetHoldLoweringMultiplier` 0x607130 is 0.7 for a SIDE hold, else −0.3 (Poo, held
  ABOVE); `MobileObject::ValidToApplyThisToObject` 0x607B70 is 1 only for the mushrooms (info +0x104 = 0x11..0x13) on a
  store of their resource; `MobileObject::ApplyThisToObject` 0x607BC0 gives it to the target's store (3) or nothing;
  `MobileObject::ReactToPhysicsImpact` 0x607A20: a mushroom that hits a store of its resource is taken by it;
  `GetPhysicsConstantsType` 0x6079F0 gives 0x15, 0x16, 0x17 for CHAMPI 17, MAGIC_MUSHROOM 18 and TOADSTOOL 19
  (`g_MobileObjectInfo` at 0xD38448, 0x114 bytes a row).
- **Animal**: `Animal::GetHoldType` 0x419310 = 7 (HOLD_TYPE_VILLAGER), `GetHoldLoweringMultiplier` 0x419320 = 0.65,
  `ValidForPlaceInHand` 0x419B40 (above). `Animal::InterfaceSetInMagicHand` 0x419B60 stores its previous state, takes
  it out of its flock (for a flock a script controls, its script reference dropped through
  `GScript::FindScriptGameThing` first) and sets the state IN_HAND. `ValidToApplyThisToObject` 0x41B320: a storage pit
  is 1, else the Living rule (the worship totem); `ApplyThisToObject` 0x41B360 gives it to a food store, or on a
  `WorshipTotem` reports the SACRIFICE deed, counts it in the player's stats, updates the alignment and sets the death
  reason 7. `Animal::ReactToPhysicsImpact` 0x41BC10: thrown into a store of its resource, it is taken as food.
- **Villager**: `CanBePickedUp` 0x55CA50 (above), `ValidForPlaceInHand` 0x7564A0, `CanBecomeAPhysicsObject` 0x7564C0
  = IsReachable.
- **Creature**: `CanBePickedUp` 0x473FE0 = 1 (only the creature's own code asks), `ValidForPlaceInHand` 0x473F60 = 0,
  `GetWeight` 0x47CD60 = the 3D body's mass, `GetHeight` 0x477F50 = 3D +0x90 × 15, `Get2DRadius` 0x477F40 = 3D
  +0x5228, `CanBecomeAPhysicsObject` 0x479D10 = 0, `GetPhysicsConstantsType` 0x479B80 = 0;
  `InterfaceValidToGiveObject` 0x47A330: the same or an allied player, not a spell seed, the creature's info +0x1F4 not
  0xD, and the creature able to pick it up, else a refusal (fn_004C9FE0(0x25, 7, 0, 0, 0)); `InterfaceGiveObject`
  0x47A320 = 0x16. `Creature::ReactToPhysicsImpact` 0x479990: hit by a thrown thing that is not a toy, the damage
  (above 0.005) is applied and, from its own player, `CreatureMental::UpdateAttitudeToPlayerFromFeedback(−damage)` and
  the fear and anger from being damaged grow; skipped for a creature with Flags 0x400 whose owner is the local player
  (g_game +0x205A59, 0x4799F3..0x479A26: `test byte [esi+0x25], 4`, then its GetPlayer (vt +0x1C, 0x479A1F) against
  the local player, returning when equal); the push of `LH3DCreature::ApplyForce` (0x4799EE) comes before the skip,
  after the toy tests (0x4799B3 / 0x4799CC). The hits are on [Struck by a thrown thing](../creature/hits-and-catch.md#struck-by-a-thrown-thing).
- **Pot and piles** (`g_PotInfo` at 0xD4C660, 0x144 bytes a row; 0xB POT_INFO_HAND_WOOD, 0xC POT_INFO_HAND_FOOD): a pot
  holds as a MobileObject (SIDE, its 2D radius, lowering 0.7); `Pot::CanBecomeAPhysicsObject` 0x66E8F0 = info +0x12C;
  `Pot::GetPhysicsConstantsType` 0x66CEF0 = 5 for mesh 0x143, else 4; `PotStructure::CanBeThrownByPlayer` 0x55D640 =
  +0x70 != 0; `PileResource::IsTuggable` 0x66E3B0 = 0; `PileFood::Get2DRadius` 0x66F180 = the raised share × the
  object's radius; `Pot::InterfaceSetInMagicHand` 0x66D710 removes its reaction and returns 1.
  `PileResource::InterfaceSetInMagicHand` 0x66EF80: a hand pile goes to the hand whole; any other pile gives a handful
  (the hand pot's capacity or what is left), made as a new hand pot (poisoned if the pile was) and placed in the hand,
  the pile staying on the ground. `Field::InterfaceSetInMagicHand` 0x529520 makes a hand food pot of the field's food
  per grab (its type info +0x12C) and places it in the hand.
- **SpellSeed**: `SpellSeed::InterfaceSetInMagicHand` 0x728810 sets its power-up, triggers the help (events 12 / 13)
  for the local hand, is deleted when its chants cannot be stored, registers its seed type with the status
  (fn_005DCA20), becomes a held seed, and for the local hand loads its particles (`GJPSysInterface::LoadFileData` of
  its magic info +0x20) and sets the hand's tribal power.

#### Hold types and the other hand overrides

How each class is held (`GetHoldType` vt +0x58C, `GetHoldRadius` vt +0x590, `GetHoldLoweringMultiplier` vt +0x594)
and its physics constants type, from the same reconstruction. The hold itself is on
[Holding, spring and throwing](grab-hold-and-throw.md#holding-spring-and-throwing).

| Class | Hold type | Radius | Lowering | Physics constants | Other |
|---|---|---|---|---|---|
| Object | `Object::GetHoldType` 0x638BF0: ABOVE | `GetHoldRadius` 0x638C00: ABOVE 0.75 × height, else the 2D radius | `Object::GetHoldLoweringMultiplier` 0x638C30: 0.0 | (table above) | `Object::GetHeight` 0x638120: the mesh box's y size × scale × 2 |
| Tree | 0x74B600: TREE (5) | 0x74B610: 0.2 × 2D radius | 0x74B620: 0.1 | 0x74B640: 6 (`CanBecomeAPhysicsObject` 0x74B630 = 1) | `Tree::CanBePickedUp` 0x55D8B0: !(Flags & 0x2000); `HandShouldFeelWithMeshIntersect` 0x55D9E0 = 0 |
| DeadTree, FelledTree | 0x5110D0: 5 | 0x5110E0: 0.2 × 2D radius | 0x5110F0: 0.1 | 0x511100: 1 for mesh 0x196 of the pack, else 6 🔍 inferred; FelledTree 0x511910: 6 | `HandShouldFeelWithMeshIntersect` 0x5109C0 = 0; `InterfaceValidToTap` 0x5109A0 = 0, `InterfaceTap` 0x5109B0 = 1; `CanBecomeAPhysicsObject` 0x511A10 = 1; `GetWeight` 0x511590 = scale³ × the info's +0xAC |
| MobileStatic | `MobileStatic::GetHoldType` 0x6091B0: SIDE for the gate totems and weeping stones (fn_00609130 0x609130: info rows GATE_TOTEM_APE 0x31, _BLANK 0x32, _COW 0x33, _TIGER 0x34, WEEPING_STONE 0xE, WEEPING_STONE_REWARD 0xF), the first singing stone (fn_00609180 0x609180: row 5) and the toys of mesh 0x191 / 0x193; else ABOVE | Object's | `MobileStatic::GetHoldLoweringMultiplier` 0x609230: 0.7 for the gate totems and weeping stones, 0.4 for the singing stone, else 0.0 | `MobileStatic::GetPhysicsConstantsType` 0x609270: 3 for those and info +0x128 == 2, 0x12 for a fence, the toys by mesh (0x18F 0xE, 0x190 0x14, 0x191 0x10, 0x192 0xF, 0x193 0x13), else 3 | `g_MobileStaticInfo` at 0xD3A6D8, 0x12C bytes a row; `CanBePickedUp` 0x439670 !(Flags & 0x2000); `IsFence` 0x609110 meshes 0x38, 0x51, 0x52 |
| Rock | MobileStatic's (ABOVE for plain rocks: 0.75 × height, 0.0) | | | 0x6E6FF0: 3 | `CanBeThrownByPlayer` 0x6E7070: 2D radius ≤ 3.6 (DeadTree, FelledTree and Bonfire too) |
| MobileObject | 0x607120: SIDE (6) | | 0.7 (above) | 0x6079F0 (above) | `CanBePickedUp` 0x425B30 !(Flags & 0x2000); `CanBecomeAPhysicsObject` 0x607AA0 = 1 |
| Poo | 0x6083D0: ABOVE (1) | | −0.3 | 0x6079C0: 0xC | |
| Ball | | | | 0x4360F0: 2 | |
| Villager | 0x750310: VILLAGER (7) | 2D radius | 0x750320: 0.65 | 0x5EFF30: 7 | |
| Animal | 0x419310: 7 | 2D radius | 0x419320: 0.65 | 0x5F04D0: 8 | |
| SpellSeed | 0x728680: MAGIC while not yet a held seed (+0x90 == 0), else the info's +0x158 | 0x728640: scale × info +0x150 | 0x728660: info +0x14C | | `GetHoldYRotate` 0x728670 = info +0x154 |
| OneOffSpellSeed | MobileObject's (SIDE, 0.7) | | | 0x72A920: 9 | |
| Field | | 0x528E80: 2D radius 5.0 | | | `IsTuggable` 0x528040 = 0, `CanBecomeAPhysicsObject` 0x528030 = 0 |
| MagicFireBall | | 2D radius = scale × 1.0, height = 2D radius | | | `IsTuggable` 0x682A40 = 0, `CanBecomeAPhysicsObject` 0x682A70 = 0 |

- **Throwing from the hand**, `Object::ThrowObjectFromHand` 0x6385E0 (vt +0x758; the release's chain is `ApplyThisToMapCoord` → `ThrowObjectFromHand` → `InitialisePhysicsFromHand`): for the local player
  (`GGame::g_game` → `MyPlayer`) a help trigger (event 4); the status's throw velocity and angular velocity; the
  UNAVAILABLE_FOR_STATE_CHANGE flag cleared; the paper and disappearing flags of the physics object passed to the 3D
  object; back into the map when in bounds, else deleted; then
  `Reaction::RemoveAllReactionsOfTypeInitiatedByObject(this, REACT_TO_FLYING_OBJECT)` and the landing's mimicry
  ([Landing](../creature/mimicry.md#landing)). `SpellSeed::ThrowObjectFromHand` 0x72ACD0 applies a seed not replanted to
  a worship site first (3), else `Object::ThrowObjectFromHand`.
- **A tree thrown at a store**: `Tree::ReactToPhysicsImpact` 0x74B6B0: when the thing that hit
  (`PhysicsObject::GetGameObjectWhoHitMe` 0x644F00, a PDB name that looks mis-associated) stores wood, the impact data
  (12 dwords of the physics object's +0x1A8) are copied to 0xD414A8 and the tree is taken as wood; else Object's.
  `DeadTree::ReactToPhysicsImpact` 0x510CA0 is the same without the copy; `DeadTree::EndPhysics` 0x5113F0 adds a
  REACT_TO_WOOD reaction (villagers come for the wood); `FelledTree::EndPhysics` 0x511970 does not.
  `MultiMapFixed::DeleteObjectAndTakeResource` 0x52F460 takes a dropped resource only with a building site (+0x74), the
  same body as Scaffold's 0x6EAEC0.
- **Applying a tree**: `Tree::ValidToApplyThisToObject` 0x74BD50 (the same in DeadTree 0x510E90): a worship totem (the
  sacrifice) or a wood store. `Tree::InterfaceSetInMagicHand` 0x74B730: uprooting a standing tree plays a tree-break
  sample and updates the player's alignment, then the plant-tree sample (0x53..0x55 by the tick count).
- **The Living sacrifice**: `Living::ValidToApplyThisToObject` 0x5ED590 is a worship totem; `Living::ApplyThisToObject`
  0x5ED5C0 adds the sacrifice value × life to the site; `Villager::ApplyThisToObject` 0x752C40 refuses an unbuilt site
  or an indestructible villager (0x17) and counts a child 1.25 times (the double at 0x8AA8D8).
- **The artifact rule**: `Fixed::EndPhysics` searches with `Object::IsSuitableForArtifact` within 50; the town
  artifact leaves its town by `TownArtifact::RemoveFromTown` as it goes into the hand.
- **SpellSeed**: `ValidForPlaceInHand` 0x728580: a neutral player's seed under the game flag 0x2000, else the status's
  player's own available seed; `HandShouldFeelWithMeshIntersect` 0x727F60 = 0; `IsG3DObjectDrawnInHand` 0x728600 =
  the magic info's +0x38 == 1; `IsInterfacePowerUpWhenInHand` 0x7286A0 = 1; `InterfaceSetOutMagicHand` 0x728940
  unregisters the seed type, cancels a worship icon's charge and drops the hand graphic; `ValidToRemoveFromHand`
  0x728760 = on land; `RemoveFromHand` 0x728F00 applies it to a worship site there, else 0x17;
  `ValidToApplyThisToObject` 0x7286D0 / `ValidToApplyThisToMapCoord` 0x728720 need a held seed that can cast there (not
  on a script highlight); `ApplyThisToObject` 0x728D10 and `ApplyThisToMapCoord` 0x728E20 cast, keeping the spell in
  the hand (1) or showing SPOT_VISUAL 3 (0x16); `DrawOutOfMap` 0x5190A0 draws a held seed when its 3D object is drawn
  in the hand.
- **OneOffSpellSeed**: `InterfaceSetInMagicHand` 0x72A530 marks the magic type as ever enabled for the player;
  `InterfaceValidToTap` 0x72A630 = 1; `InterfaceTap` 0x72A640 makes the spell into the hand, pops (immersion 0xE,
  sample 0x6D) and deletes the bubble.
- **MagicFireBall**: `ValidForPlaceInHand` 0x682DD0, 1 unless it is the status's own player's; `InterfaceSetInMagicHand`
  0x682E80, `InterfaceValidToTap` 0x682E40 = 1 and `InterfaceTap` 0x682E50 catch it through fn_00682EA0: with the hand
  ready, a charged Fire seed (`g_SpellSeedInfo[SPELL_SEED_TYPE_FIRE]` 0xD9D998) is placed in the hand and the ball is
  deleted.
- **Pot**: `Pot::InitialisePhysicsFromHand` 0x66DF00: a release slower than |v|² = 5 is no throw: the pot is put down
  as a pile (`PSysGlobal::StartMultiPutdown` with 0.75, `Pot::AddResourceToPos`) and deleted.

### Placement, the near object, the morph and the tap memory

#### Where the hand goes

- **Empty hand:** HandStateNormal::Update 0x5B71A0 puts the model origin at the required position. That point is on
  the mouse ray, one hand length (3.2 × handScale) short of the land, not over the sea, clamped to [2, reach].
- **Matrix:** fn_0046E160, with the heading from the camera → mouse ray. The up is the land's normal at the hand,
  through three 0.4 s Zoomers that are snapped to (0, 1, 0) on every HandStateNormal::Enter. Nothing lifts the hand
  above the land.
- **Land grip:** HandStateCamera puts the origin at the grabbed land point, freezes the up, keeps the heading live,
  and takes the scale from the grip point.
- **Root bone:** every clip applies its root.
- **Held object pull-in:** 0.5 × Get2DRadius of the held object (0x5B676B).
- **Action point:** GInterface +0x3F0 ([The action point](#the-action-point)), never the hand's own point.
  GetPlayerHandPositions returns the hand itself.
- fn_0046E160: d = (−sin H, 0, cos H), horizontal towards the camera; H (CHand +0x84, written at 0x46C725) is taken in
  PrepareForDrawing from the camera → mouse ray (fn_0074CBD0) only when |ray.x| or |ray.z| > 0.01 ([0x8C7A10],
  0x46C6FB / 0x46C70E).
- side = norm(d × up), skipped when zero; the model's X = side, Y = side × up, Z = −up (the scale and the left hand's
  mirrored X are the transform's).
- As the FPU works it (0x46E193..0x46E2C3): b = H + π, a float sum ([0x8C36A0]); d = (float(sin b), 0,
  −float(cos b)), the sine and the cosine rounded once when stored. d × up is (−(d.z·up.y), d.z·up.x − d.x·up.z,
  d.x·up.y), each product a float, the zero of d not multiplied. It is normalised (1 ÷ sqrt of x² + y² + z², each step
  a float) unless all three parts compare equal to 0 (`fcom`, so a NaN counts as 0), and then kept as it is. The second
  row is worked as the negation of (side.z·up.y − side.y·up.z, side.x·up.z − side.z·up.x, side.y·up.x − side.x·up.y),
  the third is −up. The up is used as given: nothing in fn_0046E160 normalises it (the normal state's up is normalised
  by its caller, `LHPoint::Normalise` 0x460710 at 0x5B712A: x, y and z each compared with 0.0 (`fcomp`, C3, so a NaN
  counts as 0); all three zero → returns 0 and leaves the vector as it is; else `fmul` z·z, + y·y, + x·x, `fsqrt`,
  1.0 ÷ it (`fdiv`), and each part times that, every step a float on the game thread's 24-bit FPU).
- The empty hand's up over plain land is `LH3DIsland::GetNormal` 0x803630 at the hand's last position (arg5 is read
  before its last write).
- The three up Zoomers are 0xD13FB0 / 0xD13FE0 / 0xD14010 (`Zoomer::SetDestinationWithSpeedAndTime` 0x407D60 per axis
  at 0x5B7075 / 0x5B708E / 0x5B70A7, 0.4 s pushed at 0x5B7065). They take a new target only when the mouse's x changed
  this frame (CHand +0x4858, 0x46C6EA) or the roll is not 0 (0x5B7043..0x5B705F; 0 in HandStateNormal), and are updated
  every frame; g_D13F60 is their value normalised (fn_00460710 at 0x5B712A). `HandStateNormal::Enter` 0x5B5D00 snaps
  them to (0, 1, 0) at 0x5B5D0C / 0x5B5D4D / 0x5B5D93.
- Land grip: the change to Cgrip (0x5B075B) sets hand->pos = the land point under the mouse (g_D13F30,
  fn_0046DF60(false)); HandStateCamera keeps it at the grabbed point with no offset (0x5B04C4) and uses the last Normal
  up (g_D13F60, frozen). With an object, a bubble or the leash in the interface (+0x3C8 / +0x3D0 / +0x3D8) the original
  keeps the Normal position at the change to Cgrip.

#### FindObjectNearMapCoord

- When a click hits nothing, fn_005D39E0 takes the nearest object within ±5 of the land behind the hand, as seen from
  the camera. The search goes through the cells, skips fragments, and keeps the nearest one if it is not farther than
  the action point. A fish shoal is tried first.
- The point behind the hand (GLandscape::Draw 0x5E4848..0x5E4870, once a drawn frame): the box of the hand's bones
  1..n−1 (TransformedMatrices +0x80, element 1 first, 0x5E46DC..0x5E47CD), the camera's ray through its centre onto the
  land (`LH3DIsland::RayCast` fn_00802550), with the plane y = 0 within 7500 m when the land is missed; g_0xD2017C =
  hit, g_0xD1A3A0 = (x, 0, z).
- MapCoords c of that point: x, z = ftol(m × 6553.6), altitude = 0 − GetAltitude (so 0 only on a cell at height 0).
- c on the water (MapCoords::IsWater 0x6035B0 at 0x5D3A2F, (🔍 inferred)): the shoal near the action's point
  (fn_00824B10 on +0x3F0, xz distance² < 4), then the fish farm of that shoal (g_game +0x205C0C, +0x88 == shoal).
- The search square is ±5 m (0x40A00000 at 0x5D3B1B) around c (fild × 10 / 65536, 0x5D3AC9..0x5D3B9C; ftol((c ∓ 5) ×
  65536 / 10), signed high words), x outer, z inner, the fixed list then the mobile one.
- Every object but the fragments (0x5D3C29), at its 2D distance from c (`GUtils::GetDistanceInMetres` 0x74CD70); the
  nearest under 5 ([0x8AB6E4]; strict, the first of a tie).

#### The grip dust

SPOT_VISUAL 2 through the particle system's `StartSpotVisual`, made by packet 0x2B → 0x63D6D8. The grip dust packet
0x2B is sent by fn_00550CF0 at 0x5D1FC9; SPOT_VISUAL 2 GRIP_LANDSCAPE lasts 100 turns, is Z-sorted, stepped once a
turn and drawn with the turn fraction.

#### Good and evil

- CHand::PrepareForDrawing 0x46C550 morphs the hand when the local player's alignment moves 0.03 or more.
- **Texture:** Blend4444 fn_00870640 between Base2 and Evil2 / Good2, at t = min(255, trunc(|a|·256)).
- **Vertices:** `Morphable::MorphVertices` 0x618D10. Evil2 grows claws.
- MorphAnims changes nothing for the hand.
- `CHand::PrepareForDrawing` calls `SetTextureSet(GInterface +0x48)` 0x46C60C (SetTextureSet 0x46BF60); a change
  blends the texture again; the set-0 branch fn_0046BFA0 is an empty ret.
- The local player's alignment, clamped to −1..1, goes into Morphable +0x9C (0x46C63A..0x46C665);
  `Morphable::UpdateMorphing` 0x618C40 re-morphs when it moved 0.03 ([0x900AD4]) from the one applied (+0xA0, zeroed
  by MorphInit 0x617310).
- `Morphable::LoadBase` 0x618360 / `ReadBinary` 0x617AE0: the base twice (the drawn mesh [0] and the pristine [1]),
  then hh.HBN's variants [2] hand_boned_evil2, [3] hand_boned_good2 ([4..7] empty).
- `Morphable::MorphTexture` 0x619500 (Morphable vt +0x10) reads only +0x9C (0x619509), the target. `UpdateMorphing`
  writes +0xA0 = +0x9C before it calls it, so a re-morph blends at the new alignment; `SetTextureSet` 0x46BF60, on a
  change of the texture set (0x46C617), calls it through CHand vt +0x10 = 0x46BF80 before the frame's alignment is
  stored (0x46C66D), so that blend is at the target taken the frame before.
- `Morphable::MorphTexture` 0x619500: a < 0 → Evil [2] with f = −a, else Good [3] with f = a (0 counts as good); the
  base [1] when the variant is missing. It writes the skin's texels and sets the material's dirty flag +0x138.
- `MorphVertices` 0x618D10: out = base + w·(m − base) on the position (+0) and the normal (+0x14), w = |a|; the hand
  has no second or third axis (slots [4..7] empty).
- `Morphable::MorphAnims` 0x619100 changes nothing because the three files have the same bones and hh.HBN's evil and
  good animation tables are empty.

#### The action point

Read in the frozen runblack.exe.

- The action collide is a `GInterfaceCollide` at GInterface +0x3E8 (vtable; +0x3EC its Base field, 0 from the ctor
  0x5CE1DB and only ever copied): **pos +0x3F0** (MapCoords: x, z fixed point, altitude), PosDistance +0x3FC, object
  +0x400 (+0x404), bubble +0x408 (+0x40C), leash +0x410 (+0x414). The hover / render collide has the same layout at
  +0x3B0 (pos +0x3B8). There is no "no point" flag: +0x3EC is not a hit type and +0x3FC is FLT_MAX after the zeroing
  but also after the grip and keyboard writes; no action reader reads it.
- **Writers.**
  - `PreDrawProcess` 0x5CEA57..0x5CEA97 zeroes the whole action collide (pos 0, distances FLT_MAX) **only when the
    action flags byte +0x38 is 0**, then always calls fn_005D5980 (0x5CEAA6). The mouse-move message's handler 0x5D99E0
    calls it too (0x5D99F1), then the gesture recorder fn_005CEAD0.
  - fn_005D5980 (0x5D59B3..0x5D59F3): when fn_005E5740 finds land under the mouse, +0x3F0 = that land (x, z =
    ftol(m × 6553.6), altitude 0, 0x5E577E) and +0x3FC = its depth + 2.3; the hover pos +0x3B8 gets the same. No land:
    +0x3F0 is **not touched**, +0x3B8 is zeroed (0x5D5A06).
  - `UpdateInterfaceCollide` 0x5D5A70 (PostDrawProcess 0x5CEABA) picks the object, the bubble and the leash only when
    +0x38 is 0 (0x5D5A9C); then +0x3F0 = the object's Pos (0x5D5D09..0x5D5D2B), else the bubble's position as MapCoords
    (0x5D5D30..0x5D5D80). The object branch runs with +0x38 set too: the action's object keeps +0x3F0 on itself.
  - The grip start fn_005D1AB0 (0x5D1E89..0x5D1E9D), the keyboard moves 0x5D65F0 / 0x5D6700, playback fn_005DAEE0.
  - Every input message carries its own copy of the action collide, taken when it is queued (`SendMessage` 0x5D9DB0 /
    0x5D9DFA, `GInterfaceMessageBuffer::Add`). `ProcessInterfaceMessages` fn_005D9A20 (from `GInterface::Process`
    0x5CEC1F) copies each message's collide into +0x3E8 only when +0x38 is 0 (fn_005D9BC0 0x5D9BCD..0x5D9BDE), runs its
    handler, then fn_005D1120; at the end it restores the collide it saved at the start (0x5D9B46..0x5D9BAE; with
    +0x38 set the saved object is replaced by +0x400, 0x5D9B2A..0x5D9B41). So a press reads the collide of the last
    draw.
- **The action flags byte +0x38** (the action in progress), set by: the grip start (|= 1, 0x5D1F7E, state 2),
  `StartGrab` (|= 0x10, 0x5D17D1, state 13; it also sets the collide's object, fn_005D5E40 at 0x5D17C3, when the
  object's vt 0x190 is 1), 0x5D2730 (|= 0x10, the apply-on-release states 8 / 9 from the holding press 0x5D1651 /
  0x5D1700, which also restarts the gesture recorder), `GenericPickup` (0x10, 0x5D2879, state 7, until the action
  object's +0x24 & 4, 0x5D4A90), `SendApplyToObject` 0x5D3080 (|= 0x20, 0x5D30C1, state 15), the bubble press (|= 0x10,
  0x5D433A, state 17) and the locked select's release (|= 0x10, 0x5D4850, state 4 to 5, then state 6 0x5D4A10 until
  the object's +0x24 & 0x10 clears). Not by the release state 12, the locked selects 3 / 10 / 11, nor state 0.
- **The grip start** (fn_005D1AB0, action state 2) reads +0x3F0, the press's action point (the collided object's
  position, else the land, else zero), and puts the dust packet 0x2B and G_HandGrabLand there when InBounds &&
  IsLand, else the ripple and the sound at its x, z. During the grip +0x38 bit 1 stays, so +0x3F0 is the land under the
  cursor of each frame, kept over the sky; it is never set to the grip point after the start. The hand's own grip
  position is CHand +0x78, the hand's land under the mouse at the change to Cgrip (0x5B075B). When the hand's reach is
  under 1799 (fn_0046BF50 below the camera's distance, fn_00800C30) the grip start re-projects: +0x3F0 = that point and
  +0x3FC = FLT_MAX (0x5D1E89..0x5D1E9D); a second hit there returns without gripping (0x5D1F78). With nothing under the
  cursor the original grips with the cleared point (the ripple at the map's corner).
- Three readers take x == 0 && z == 0 && altitude == 0.0 as "no point": `ClickOnLand` 0x5D3D16..0x5D3D32 (returns 1,
  nothing done), the leash-pull release 0x5D55E3..0x5D55FD (no packet 0x5E), `UpdateCollideImmersion`'s fn_005D82A0
  0x5D82A0..0x5D82C9. (FindObjectNearMapCoord's (0, 0, 0) test, 0x5D3A0A..0x5D3A25, is on the point behind the hand.)
  Every other reader uses the zeroed point as the map's corner (`MapCoords::InBounds` 0x6042C0 is cellX < width &&
  cellZ < height, so (0, 0) is in bounds).
- **The influence +0x48** = `CalculatePlayerInfluence(+0x3F0, player, 0, 1, 1) > 0` (fn_005D1120 0x5D1136..0x5D1164),
  computed at the start of fn_005D1120, before the action state's process (0x5D123E) that runs ActionPressed (state 0
  0x5D5190 to 0x5D5207) and every release. fn_005D1120 runs after each message (0x5D9CFD) and once when no message ran
  it (0x5D9B1C). So the press reads +0x3F0 and +0x48 of the same point. With the point zeroed it is the influence at
  the map's corner (in practice 0 on every land seen: 🔍 inferred, not measured), so the hand's texture shows "out of
  influence" (`CHand::PrepareForDrawing` 0x46C611 `SetTextureSet(+0x48)`).
- **Readers**:
  - the press with nothing collided: `ActionPressed` calls `FindObjectNearMapCoord` with &+0x3F0 unconditionally when
    +0x400 is null (0x5D138E..0x5D13A1, 0x5D1493..0x5D14A6); with the zero point on land it takes the nearest object
    within 5 of the point behind the hand when that distance is not more than the corner's (0x5D3CB4..0x5D3CDB), so
    almost always;
  - the fish farm: the shoal within 2 of +0x3F0 in `FindObjectNearMapCoord`'s water branch (0x5D3A3C..0x5D3AB8), then
    the locked select's +0x48 test (0x5D13E6);
  - the press on nothing: `ClickOnLand` 0x5D153F..0x5D1548 to 0x5D3D10, which takes the zero point as no point;
  - the leash's double click: fn_005D4210 0x5D4216..0x5D4227, `FindObjectNearMapCoord(&+0x3F0)` unconditionally;
  - the tap memory: fn_005D3700 0x5D3782..0x5D37B8, +0x46C = +0x3F0 always, zero included;
  - the seed's press, apply and locked apply: the holding press 0x5D16BE..0x5D16FC (+0x48 first, then InBounds(+0x3F0),
    vt 0x724, state 8; else FailApply(&+0x3F0), 0x5D16B0), `SendApplyToMapCoord` 0x5D3354..0x5D3445, state 10
    0x5D4C6E..0x5D4CA4;
  - the release (state 12, 0x5D4DC0..0x5D4E24): InBounds(+0x3F0), held, vt 0x724, +0x48, no presence test;
  - the hand sync: `SendHandSync` fn_005D2250 0x5D2383..0x5D2408 compares +0x3F0's cell with the last sent (0xD17D48,
    fn_005D26B0) always;
  - the land tooltip: fn_005D6980 0x5D6A7D..0x5D6AED reads the hover pos +0x3B8 (this frame's land or zero, never an
    object);
  - no +0x3F0 in `StartTapOrLockedSelect` 0x5D1A00 / 0x5D1950, and none in HandStateTug 0x5B7DD0 or CHand;
  - the grip start: see above.

#### Reach and the ControlMap switches

**Reach:** CHand +0x4838 (1800; at most 1800, fn_0046BF20) is set by SET_INTERFACE_INTERACTION.

- Cap [0x8CBEAC] = 1800; set by the CHand ctor (0x46BC3E).
- Who reads the reach: GInterface fn_005D1AB0 (5 calls via fn_0046BF50); CHand fn_0046DF60 (0x46E00F, 0x46E123);
  `CHand::SetDistanceFromView` 0x46C0D0, which at 0x46C0E4 clamps the distance to [2, reach].
- SET_INTERFACE_INTERACTION (0x70B220) calls fn_0046BF20 with 75 in JUST_GRAB (0x42960000 at 0x70B30D) and with 1800 in
  the other levels that set it. These do not touch the reach: JUST_HAND_MOVE 8 (0x70B4BC), JUST_HAND_INTERACTION 12
  (0x70B520, which only sets bit 2) and the invalid levels (9 and > 15, 0x70B785).
- ControlMap switches +0x652C / +0x6530 (set to 1 in the ctor, 0x46F719 / 0x46F724):
  - `GGame::ProcessKey` 0x63F42A..0x63F446 requires both for the LH_KEY 2..15 block (KB_1..KB_TAB);
  - fn_0046F750(action) is called by `ControlMap::IsActionPerformed` 0x470AF9, fn_00470A00 and fn_00470A60. With
    switch 1 off it blocks actions 3..19 except 5: it indexes the byte table 0x46F7A4 with action - 3 (0..16), whose
    bytes are `00 00 01 00 00 ...` (only entry 2, action 5, is 1), into the jump table 0x46F79C {0x46F78D return 1,
    0x46F795 return 0}. With switch 1 on and 2 off, it blocks 17..19 (0x46F783..0x46F78B). Otherwise nothing is
    blocked. That is all camera movement except TALK, and then ZOOM_TO_TEMPLE / ZOOM_TO_CREATURE / ZOOM_TO_REALM.
  - It is asked at each query, not once a frame: `IsActionPerformed` 0x470AF0 returns 0 for a blocked action
    (otherwise the held state at +0x648C[action], and 0 while `SetupBox::WantKey`); fn_00470A00 (key to action) and
    fn_00470A60 (mouse to action) scan the 33 slots (stride 0x30C, fn_0046F480 / fn_0046F460) and return -1 at the
    first slot bound to the key or button if that slot is blocked.
  - The citadel's cameras read the moves through `IsActionPerformed` too, so they are blocked as well:
    `InnerCamera::UpdateMain` 0x795CE0 (actions 7..10 at 0x795F36..0x795FC5), `CreatureRoomCamera::UpdateMain`
    0x78A3E0 (0x78A58E.., 0x78AC2E..), `ChallengeRoomCamera::UpdateMain` 0x7859D0 (0x785E40.., 0x786081..) and
    `WorldRoomCamera::UpdateMain` 0x79FCF0 (0x7A0261.., 0x7A09AF..). Only the one-press keys go to the citadel's own
    handler ([The one-press keys](../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)).

#### Tap memory

- **The slot:** GInterface +0x45C (the object) and +0x46C (the land point). It is written by RememberTapped
  fn_005D36D0, from Tap and from fn_005D3700 when the action button is released idle, and it lasts 15 s of game time.
- **Readers:** GAME_THING_CLICKED, GET_OBJECT_CLICKED, CLEAR_CLICKED_OBJECT, CLEAR_CLICKED_POSITION and
  POSITION_CLICKED (fn_005D0460).
- GET_OBJECT_CLICKED 421 (`GScript::GetObjectClicked` 0x6F3350): despite its name, `GInterface::GetLastRClickedObject`
  0x5D0730 is `BaseInfo::GetBase(+0x45C)`, this same memory. Found: `AddScriptGameThing(obj, 0)` and its id pushed as an
  object; none: 0.
- `RememberTapped` fn_005D36D0: `BaseInfo::Set(+0x45C, obj)` 0x436BB0 (null clears it) and +0x468 = the game turn;
  `BaseInfo::GetBase` 0x436B80 returns null when nothing is stored or the object no longer exists.
- GAME_THING_CLICKED is 0x70AEB0 (GInterface +0x45C, the last object tapped with the action button);
  `GameThingClicked` 0x70AF84 and CLEAR_CLICKED_OBJECT 0x70B0E0 set +0x460 = +0x464 = 0; the turn +0x468 stays.
- POSITION_CLICKED 0x70B120 pops the radius, then the position (0x70B130..0x70B170); a multiplayer game logs "This is
  not multiplayer friendly yet!" and pushes true (0x70B183..0x70B1B2); then fn_005D0460(MapCoords(pos), radius)
  (0x70B1C7..0x70B1DA): `GUtils::GetDistanceInMetres(+0x46C, pos) <= radius` (`test ah, 0x41`); +0x46C is the land
  point of the last land tap in the last 15 s, (0, 0, 0) once cleared.
- CLEAR_CLICKED_POSITION 0x70B100: +0x46C / +0x470 / +0x474 = 0; the turn +0x478 stays.
- **The store**: `Object::DoDeleteObjectAndTakeResource` 0x63A940 gives a thing to a store; a thrown tree that hit a
  pit passes the giver's interface; the storage pit's handler makes reaction 0x16.

### Sources

- The cursor search, the pixel and triangle tests and the action collide read in the exe with the xref index; the
  temple meshes and AllMeshes.g3d read from the frozen game data; the hand's virtual functions from the decompiled
  reconstruction of the pickup path (`decomp_pickup`, `objects.cpp`), each checked in the disassembly.

## openblack

**Done**: the pick's interactable test, the villagers' sphere, the trees' pixel test, the temple's entrance, the bubble
arbitration, the press fallback, the place-in-hand allowlist, the hand matrix, the morph, the action point, the reach,
the blocked actions and the tap memory. **Partial**: other objects are picked by their nearest triangle along the ray.
The port decisions taken from raffclar's code are in
[Port decisions: the hand, the camera and the interface](../project/port-decisions-hand-camera-interface.md).

### The pick in openblack

- `HandSystem::PickObjectAlongRay` passes over whatever `HandSystem::Interactable` refuses
  (`hand_action_point::Interactable`), so over an abode not yet started the land behind it is found (test
  `HandActionPoint.ThePickPassesOverAnAbodeNotYetStarted`). The +0x24 & 4 test is not ported. It reads the drawn near
  clip (`EngineConfig::cameraNearClip`) for the villagers' sphere.
- The land's 2.3 is compared the same way (`ecs::hand_cursor_depth`): both distances along the ray are turned into
  depths by the camera's forward before the 2.3 is added.
- The press fallback uses only the object from the pick; it used to have an invented radius of ≥3.5 units.
- **The trees.** `HandSystem::PickObjectAlongRay` gives a tree (the `Tree` component) the pixel test
  (`ecs::hand_pixel_pick`, src/ECS/HandPixelPick.h): its drawn sub-meshes (no physics, no status, the LOD 0 bit),
  primitives and triangles in order, through the camera's view-projection on the window's screen, at the pixel the
  pick's ray goes through, rounded to a whole pixel; each primitive with its texture's mask (`PickMask`,
  src/3D/PickMask.h), asset data kept in the resource system's `GetPickMasks()` under the texture's id: `PickMaskLoader`
  makes it once for every mesh pack texture, from `Texture2DLoader`'s pack path, out of its DXT alpha (`dxt_alpha`,
  src/3D/DxtAlpha.h, then `graphics::shadow_math::MakeAlphaMap`, the sampling of fn_00838F00); the renderer never reads
  it. A mesh's own skins have none, as the original's other texture kinds. The first triangle that passes gives the
  depth, turned into a distance along the ray (`hand_pixel_pick::AlongRay`: on one ray it orders the objects as the
  view depths do) to compete with the other objects and the land. `ResolveCursorPoint` places the hand over the tree,
  as over every object picked by its mesh, with the hand's own mesh test along the same ray (`ecs::hand_mesh_ray`,
  `CursorHit::surface`), and keeps the land point when that ray crosses none. Every other object is picked with
  `L3DMesh::RayIntersect`. Tests: test/hand/test_hand_pixel_pick.cpp, test/test_dxt_alpha.cpp.
- **The press fallback.** `HandSystem::FindObjectNearMapCoord` (HandNearObject.cpp) is the press fallback above, and
  `HandSystem::Update` calls it at the same point. Its search is `ecs::hand_near::NearestToLandBehindHand`
  (src/ECS/HandNearObject.h), extracted unchanged and characterised by test/hand/test_hand_near_object.cpp:
  PressBetweenTwoTreesTakesTheNearest, PressJustInsideAndJustOutsideFiveMetres, ActionPointGateOnBothSidesOfEquality,
  TieGoesToTheFirstCellInTheWalk, TieInOneCellGoesToTheFixedList, FragmentIsNeverTaken, SkippedObjectIsNeverTaken,
  NothingWithinFiveMetres, WalksTheSquareXOuterZInner and CellsOffTheMapAreNotWalked. (⚠️ approximate) the press path
  filters the result with the hover's class filter.
- **Boned meshes** (villagers, animals) are tested in their rest pose, which is how they are drawn (`L3DSubMesh`:
  collision positions × the group's bone chain). Animals count as Living when placing the hand (distance to the centre
  minus the 2D radius).
- **The temple's entrance.** `HandSystem::PickObjectAlongRay` tests the mesh at the entrance's Transform, on the land,
  while `worship::citadel::EntranceCollides` (the latch of the heart's player, `influence::BoundaryShown`: once one of
  that player's temples has been drawn whole on the land, even if a script later sets its percent lower), and the
  press taps it while `IsEntranceValidToTap`. openblack's renderers skip status sub-meshes; the pick tested them, so
  the shell took the cursor in front of the doorway (an invisible wall, fixed 2026-10-08). The heart is now picked with
  `L3DMesh::RayIntersect(..., skipStatuses)` and the entrance wins a tie.
- **The second land test** fn_00848F90 (the rope's pick) is not ported.
- **What may go into the hand.** `HandSystem::ValidForPlaceInHand` classifies the entity with
  `hand_grab::PlaceInHandRuleOf` (the most particular class first) and answers with the class's rule; anything unnamed
  is 0. The scaffold's is `scaffolds::ValidForPlaceInHand`.

### Placement and the near object in openblack

- openblack's own placement (index fingertip 0.45 above the point, palm-down frame, vertex lift, fingertips dug in
  0.12, dead roll) is removed; the hand goes where the original's does. The creature's push on the held object is not
  ported.
- `_interactionPoint` is GInterface +0x3F0, never the hand's own point.
- **The matrix**: `hand_orientation::HandRotation(H, up)` (`src/3D/HandOrientation.cpp`), called by
  `HandSystem::HandMatrixRotation` with `_handHeading`, and `hand_orientation::NormaliseUp`, used by
  `HandSystem::UpdateNormalUp` (2026-10-14); until then d was the flattened, normalised mouse ray (`_handHeadingBack`),
  and the up and the side were normalised with 1e-6 fallbacks.
- **The land grip**: with an object, a bubble or the leash in the interface the original keeps the Normal position at
  the change to Cgrip; openblack does not (Pending).
- **The grip dust**: the packet's one-turn delay is not ported.
- **Good and evil** (`HandMorph.cpp`): `hand_morph::Advance`'s `skin`, passed to `HandSystem::MorphTexture`.
  `HandSystem::LoadMorphMeshes` reads the base twice and hh.HBN's evil and good variants through
  `Locator::filesystem::value`; `HandSystem::MorphVertices` blends the positions and normals by |a|, as the original.
- **The point behind the hand**: `HandSystem::UpdatePointBehindHand`, once a drawn frame in the landscape draw: the
  box of the bones but the root, the camera's ray through its centre onto the land (or the plane y = 0 within
  7500 m), for `FindObjectNearMapCoord`.

### The action point in openblack

`ecs::hand_action_point::ActionPoint`, run by `HandSystem::UpdateActionPoint` after the hand is placed, in the normal
branch of `Place` and in the land grip's: `_interactionPoint` stays a `std::optional`, empty standing for the zeroed
point (0, 0, 0), the map's corner. With no action in progress it is the frame's collide (`_collidePoint`: the object's
position, else the land), empty with neither. With one (`HandSystem::ActionInProgress`: the land grip, `_pendingPick`
for state 13, `_seedAction` `ApplyOnReleaseMap` / `ApplyOnReleaseObject` for 8 / 9, `_screenGrip` for 17) it is the
action's object (`_pendingPick`, else the seed press's `_seedTarget`), else the frame's land under the cursor
(`_cursorLand`, the interface's land), else the value it had. It no longer falls back to the hand's own point.

- **The grip start**: the dust, the sound and the splash at the press's `_interactionPoint` (the corner when it was
  cleared); the hand's own grip position stays `_gripPoint`. The grip start's re-projection under a reach of 1799 is
  not ported (the reach is 1800); openblack does not grip without a ground point.
- **The influence**: `HandSystem::InInfluence` and the hand status' `inInfluence` (`HandSpellSeed.cpp`) both measure at
  the cleared point's (0, 0, 0) instead of returning false.
- **The readers** (`p` is `_interactionPoint` with the cleared point as (0, 0, 0)): the press,
  `FindObjectNearMapCoord(p)`, no presence test (`FindObjectUnderHand` no longer returns nothing without a point; its
  point is only traced); the fish farm, `FishFarmUnderHand(p)` in the press facts and the fish branch, no presence test
  (no shoal at the corner); the press on nothing, `TapLand(p)`, whose origin test is that zero test; the leash's double
  click, `LeashDoubleClick`, `FindObjectNearMapCoord(p)`, no presence test; the tap memory, `HandClicked.cpp`, the
  cleared point is `MapCoords{}`; the seed's press, apply and locked apply take `p`, and the point is kept through the
  apply on release; the release, `SendRelease` tests InBounds(p) and the influence, no presence test (with the point
  cleared the throw data is not sent, no influence at the corner, and the drop fails at the corner, FailApply); the hand
  sync, `SendHandSync` takes p's cell every time (it kept the last point's before); the land tooltip still reads the
  action point, and nothing without one (Pending).

### Reach and the blocked actions in openblack

- The reach is `HandSystemInterface::SetHandReach`, set by SET_INTERFACE_INTERACTION.
- openblack's bit order is the original's action order (proven by `ControlMap::LoadDefaults` and the byte table
  above). `help::interface_interaction::BlockedActions()` (`src/Help/InterfaceInteraction.h`) is the same set as one
  mask. The game gives it to the action map (`GameActionInterface::SetBlockedActions`) every frame before the input is
  read, and `Set` / `Reset` give it at once, so what reads the actions after a script turn in the same frame (Creature
  Mode's C key, the cave's room key) sees the new set. A blocked action reads as not held, everywhere: the world
  camera, Creature Mode, the creature camera, the camera path's keys and the citadel's cameras. So the C key
  (`ZOOM_TO_CREATURE`) respects the script interaction modes: it reads as not held when camera moves or realm zooms are
  not allowed, and Creature Mode does not lock on.
- **(differs, rebinding only)** openblack masks every slot of a blocked action; the original stops at the first slot
  bound to the key. Two actions bound to one key are not possible with the default bindings, so this only matters
  after a player rebinds them.

### The tap memory in openblack

- GET_OBJECT_CLICKED: `HandClicked::thing` of the local hand while its entity is valid, then
  `ScriptObjects().Register(thing, false)`, as raffclar's.
- `HandSystem::GetClickedObject` (`GetClickedObject` on the interface, `HandClicked.cpp`) is null with no hand,
  nothing stored or the object gone; `HandSystem::RememberTapped` stores the object (null clears it) and the turn.
  The script functions `ClearClickedObject`, `ClearClickedPosition` and `PositionClicked` (CHLApi.cpp) clear the
  thing or the place (their turns stay) through `hand_click`, and test the place along the ground within the radius.
- (⚠️ approximate) the per-tick check uses last frame's hover.
- **The store**: `Object::DoDeleteObjectAndTakeResource` is in `ecs::object_delivery` (ObjectDelivery.h); the class
  tables are `ecs::resource_stores` and `ecs::held_apply`. DepositInStore passes the giver's interface; the storage
  pit's handler makes reaction 0x16 (`take_resource::StoragePit`).

### Where it is in the code

`HandSystemInterface` (src/ECS/Systems/HandSystemInterface.h) also answers the interface's other queries:

- `GetSpellInfo`: for a spell cast from this hand, the point under the hand, the hand, the camera's forward and the
  hand's velocity.
- `SetThrowBlock`: a hand demo's record sets the throw block (velocity, angular velocity, hand position, angles)
  before its message.
- `ResetTurnState`: on a new land, the synced hand, the turn motion and the throw data.
- `GetTurnHandVelocity`: the synced hand's velocity in units per second, read by the landscape's hand wind.
- `GetInterfaceHandState` and `UpdateInterfaceHandState`: the interface's hand state of the last turn (GET_HAND_STATE
  413), worked out once a turn before the tooltips.
- `OfferScreenObject`: a screen object under the mouse offers itself with its depth from the «Did you know?» bubble's
  draw; the nearest wins, a tie keeps the first, and nothing changes while the hand grips one.

### Test hooks

None of its own on this page; the hand's test hooks are on
[The hand: animation and placement](animation-and-placement.md#test-hooks).

## Pending

- **A tree under the cursor (#130)** ([A tree under the cursor](#a-tree-under-the-cursor)): the original has no
  forest-patch pick, so nothing was ported for it. Still open:
  - the trees' pixel collide is ported (a measured behaviour change). Left: (🔍 inferred) DXT5's steps and their drop
    to 4 bits as DirectDraw's conversion rounds them (only alphas near 16 can differ); openblack cuts a triangle only at
    the near clip (the original also at the screen's sides, the same for a pixel on the screen) and does not clamp a
    corner that is off a side; the original's extended precision is approached with doubles; while an object is held
    the pixel is that of openblack's raised object ray, not the mouse's (the original's draw collide is at the mouse
    pixel, and only the hand's placement takes the raised ray); whether a `DeadTree` (its info type) takes the pixel
    test has not been read; the original's LOD and status masks stand as openblack's drawn sub-meshes;
  - every other object's pick: the original takes the first of its triangles that holds the mouse pixel, openblack the
    nearest hit along the ray (`L3DMesh::RayIntersect`, over every sub-mesh but the physics ones). The hand's placement
    no longer uses it: it is the original's own test, fn_00865020, ported as `ecs::hand_mesh_ray`
    ([The hand's mesh test](animation-and-placement.md#the-hands-mesh-test));
  - the hand's mesh test: (🔍 inferred) the temple heart's mesh (its vt +0xF8, the blend of two `B_TEMPLE` meshes)
    keeps the status shell of sub-mesh 0, so that the hand stands on the shell over the heart, as openblack now does;
    the heart's bake of its vertices with its matrix ([the spells page](../magic/spell-core.md)) and whether the matrix
    it is then tested with is still the 3D object's have not been read. A mesh with bones is tested in openblack at rest
    (its bone chain), the original with each vertex group's matrix; for objects other than the creature that matrix
    array is the 3D object's +0x14, not read further. The original's extended precision is approached with doubles.
    Which objects the original lifts to the surface instead (vt +0x1AC of the 3D object, not an AnimatedStatic) has not
    been compared with openblack;
  - the units of the cursor search: the original compares view depths (w) everywhere, openblack distances along the
    ray. Between meshes on one ray from the eye the order is the same; it is not where a fixed length is added or a
    distance is not a depth (the land's + 2.3 is compared in depths already, `ecs::hand_cursor_depth`): the villagers'
    |centre - camera| - (R + near), the teleport stone's and the creature's distances, and the bubble's camera z
    against the object's (`HandSystem::Update`). The original's skip of a mesh whose sphere lies behind the pending
    hit stands as openblack's ray-against-sphere reject (both only skip meshes that cannot be nearer);
  - the press fallback's reach: how far the land behind the hand lies from the land under the cursor in the original
    (it sets how much of a forest's ground a press reaches) has not been measured against openblack's
    `UpdatePointBehindHand`, nor has it been checked in a run that a press between Land 1's trees takes the nearest
    tree;
  - the other objects' status sub-meshes (abode scaffolds) are still picked while undrawn.
- **The action point** ([The action point](#the-action-point)), not as the original:
  - the actions in progress with no state in openblack keep nothing: the wait for the object to be placed in the hand
    (state 7), the apply to an object waiting for its result (state 15) and the locked select's end that waits for its
    object (states 5 and 6). In them the original keeps the point (the last land under the cursor, as it does in every
    action in progress), and openblack clears it when the cursor finds nothing, until those states are ported;
  - the action's object at the land grip and the bubble's grip: the original keeps the object the press collided
    (+0x400) or the bubble's position (+0x408); openblack has neither, so they take the land, else keep the point. A
    bubble under the cursor with no action gives its position in the original (0x5D5D30..0x5D5D80), the land in
    openblack;
  - `StartGrab` makes the grabbed object the action's only when its vt 0x190 is 1, not identified; openblack always;
  - the point while the hand is hidden, or when `Place` returns before placing the hand (no ground point and no totem):
    openblack keeps the last frame's; whether `PreDrawProcess` runs the same then has not been read. When
    `ResolveCursorPoint` does not run (a NaN ray, no window) `_collidePoint` keeps its last value;
  - the grip start's re-projection when the hand's reach is under 1799 is not ported: the reach is 1800. With nothing
    under the cursor the original grips with the cleared point (the ripple at the map's corner); openblack does not
    grip without a ground point;
  - openblack has no message queue: a press reads the point the last frame's `Place` left, which stands for the press
    message's own copy of the collide. When the input messages are queued against PreDraw and PostDraw in the frame
    (`GGame::Loop`) has not been traced;
  - +0x48 is one field; openblack computes it twice, `InInfluence` with `CalculatePlayerInfluence` (interface type) and
    the hand status with `HandInInfluence` at (x, 0, z). Whether the two agree everywhere has not been compared;
  - the land tooltip's fish test reads the action point (and nothing when it is cleared); the original reads the hover
    pos +0x3B8, this frame's land under the cursor or zero (fn_005D6980 0x5D6A7D..0x5D6AED);
  - `_pickLock` (`HandFish.cpp`, `HandHolding.cpp`) copies the action point but is never read; the original copies no
    +0x3F0 in `StartTapOrLockedSelect` 0x5D1A00 / 0x5D1950. Kept as it is;
  - with no original reader of +0x3F0 found, these openblack uses are kept as they are: the field pot's position
    (`HandFish.cpp`, at the given point, else the action point, else the field; the pot is made in the field's locked
    select, not traced), the tug's grab point (`BeginTug`, `HandTrees.cpp`; no +0x3F0 reader in HandStateTug 0x5B7DD0
    or CHand) and the trace logs (`HandHolding.cpp`, `FindObjectUnderHand`).
- **The land grip** with an object, a bubble or the leash in the interface keeps the Normal position at the change to
  Cgrip in the original; not ported.
