# The hand and the interface

How the original's hand (`CHand`, the drawn hand) and interface (`GInterface`, `GInterfaceStatus`, the input and the
action states) work, and how openblack reproduces them.

Progress: [hand](../progress/hand/), [interface](../progress/interface/) (what our tree does of it, row by row).

> **Code rules.** The hand's state lives in ECS components and Locator services (`HandSystemInterface`), never in
> globals; its assets (hh.HBN, the hand meshes, `mousehelp.raw`) load through the resource caches; pure logic (the
> placement, the spring, the tooltip choice) is unit tested in `test/` with fakes; comments describe behaviour in
> plain English, with no decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

## Animation and mesh

- Animations `Data\CTR\hh.HBN` (Lionhead pack, morph block at 0x2C). Specification `Data\hndspec5.txt`
  (69 nodes, 26 present). C nodes = central poses; L*_lr / L*_fb = directional layers −1..+1.
- Mesh `Data\CreatureMesh\Hand_Boned_Base2.l3d`: 22 bones (0 palm, 8–19 fingers, tips 10/13/16/19, index = 19,
  20/21 thumb). The model origin **is the grip point** in the holding states.
- Scale: `3.2 * handScale / 555.294`; `CHand::SetDistanceFromView` (0x46C0D0): d<10 → (d/10)^0.8; d>150 →
  ×(d/150)·(1 − 0.3·(d−150)/1650).
- 555.294 in the scale `3.2 * handScale / 555.294` is the bind joint Y extent of Hand_Boned_Base2, so the hand is 3.2 units long; fn_0046C040 returns that 3.2.
- `CHand::SetDistanceFromView` clamps the distance to [2 ([0x8CBEA4]), CHand +0x4838] (0x46C0E4) before the scale.
- One CHand per player (`CHand::CHand` 0x46BA80); +0x484C = 1 after LoadBinary's `ToggleLeftRight` (0x46D56F), so the hand is mirrored; the left-handed mode is `ToggleLeftRight` 0x46C260 on the same hand.
- CHand +0x4840 is the texture set: 1 in the ctor (0x46BBC9) and in `OnClearMap` (0x46E91C).

### The hand's clip (`HandStateNormal`)

- The clip is ObtainRequiredHandPosition's tmpInt (0x5B71D0..0x5B736D, in hndspec5.txt's order): 0 Cwiggle at the entry (0x5B608E).
- With an object under the cursor (+0x3C8; none if it is a LandscapeVortex, 0x5B6096..0x5B60AF), not the held one (CHand +0x4904, 0x5B60B1) and with a G3D (+0x40, 0x5B60F7): 0xC Ccan_pickup when the interface hand state +0x3AC is 9, else 0x18 Cstroke (0x5B60FE..0x5B6129).
- A SpellIcon under the hand sets 0xC on its own, later (0x5B627C).
- **(not ported)**: 0xF Chold_fingers with the hover collide's leash (+0x3D8 = the hover collide's +0x10; the action collide's is +0x410), that is the hand over the leash **rope**, not a leash held in the hand, or action state 0x15 (the leash pull); it needs a pick of the rope. Block 10's 0x2D Crotate / 0x30 Cpitch (CameraModeNew3's tricons) and CHand +0x4874's 0x2D (0x5B69D0).
- The L lean poses (0x5B72AD..0x5B742A): the _lr pose at ftol((lag + 80) / 160 × its duration), lag = clamp(+0x48B8 − +0x485C, −80, 80) (smoothed mouse x minus mouse x), only when |lag| > 0.0001 [0x8C79D8].
- The _fb pose the same with clamp(+0x4860 − +0x48BC, −80, 80) (mouse y minus smoothed y), only when |+0x48BC| (the smoothed y itself, 0x5B73BF) > 0.0001; each pose is the difference from its middle frame ((count − 1) >> 1, fn_00861EE0).
- In HandStateCamera (Update 0x5B09B4 / 0x5B0A80) the sideways lag is the other way round (+0x485C − +0x48B8).
- Mouse smoothing: smooth += dt·vel, clamped to ±80 px of the mouse; vel = (vel + (mouse − smooth)·20·dt)·0.03^dt.
- The mouse is LHMouse's position in the game's pixels ([0xE852C0] / [0xE852C4], ints), copied by PrepareForDrawing (0x46C6C5..0x46C6D8) into +0x485C / +0x4860, so the lean depends on the resolution.
- The HOLDING poses come from the jump table 0x5B56A8 (the grip heights from 0x5B568C).
- MAGIC (0x5B50B0, a spell seed until it is ready): GetAnim(0, 0) = Cwiggle held at half its length.
- The original blends only at a change of CHand's state (PrepareForDrawing 0x46C73F..0x46C768: Exit, Enter, `CHand::StartStateBlend` fn_0046C500), 0.13 s ([0x8CBE9C], fdiv 0x46C970), linear; a clip change inside a state is not blended. StartStateBlend makes the current pose the blend source (+0x47F0 ↔ +0x49AC) and sets the timer +0x49B0 = 0.
- While the blend runs, every float of the world bone matrices +0x47F0 = prev + (cur − prev)·t from +0x49AC (0x46C96A..0x46C9A4, the fstp at 0x46C991..0x46C9A1).
- The place blend: g_C5E670 is the hand's place when its state changed (fn_0046C500 0x46C522 = g_C5E680, fn_0046E160's last); g_C5E660 = g_C5E670 + (g_C5E680 − g_C5E670)·t (0x46C9A6..0x46CA9A).
- In HOLDING (+0x4878 == 4) the held object's G3D (+0x40 +0x38) is put at g_C5E660 + the hold offset g_D13F20 during the blend, so it is drawn from the blended place.
- The blend timer +0x49B0 advances by dt = `GetCameraTimeInc` 0x555820 × 0.001 (g_delta_time inside the citadel, 0x46C5C9..0x46C608).

### Hidden hand and CHand's states (`CHand::GetRequiredState` 0x46CD10)

- CITADEL 0xA when g_game +0x205A28 == 1 (0x46CD1C); it comes first, so inside the citadel the hand stays drawn.
- INVISIBLE 0 sets +0x4844 = 1 (0x46CD38..0x46CD5C) for the interface's modes 1 / 2 / 12 / 25; the rest goes through the jump table 0x46CF48 / 0x46CF5C.
- INVISIBLE → `HandStateInvisible::Enter` 0x46BE90 → `CHand::Hide(0)` 0x46C2E0: the hand's LH3DObject +0xAC = 1 (not drawn), +0x483C set; Exit 0x46BED0 → `Show(0)` 0x46C1B0, the bones kept for the blend back.
- Only INVISIBLE hides; an inactive interface alone only stops PrepareForDrawing (no state Update, no pose).
- GetRequiredState reads the turn's +0x3AC, and the Enter's Hide comes before that frame's draw.
- **(not ported)**: the system cursor (`FrontEnd::SetCursorOn` when +0x4844 is 0).
- `OperateInteractionCamera` 0x46CB60 (called at 0x46CEA0) and fn_0046CF80 (0x46C738) act only on CREATURE (state 7, packets 0x59 / 0x5F). See [The hand on a creature](#the-hand-on-a-creature).

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

- `PreDrawProcess` 0x5CEA5C zeroes the action point GInterface +0x3F0 every frame.
- +0x3F0 is the collided object's Pos (obj +0x14, 0x5D5D09..0x5D5D2B), else the land under the cursor (fn_005D5980, from PreDrawProcess 0x5CEAA6).
- The Zoomer target is at least 1; with a screen object under the hand (+0x3D0) and no leash (+0x3D8 == 0) the target is its depth +0x3D4 (0x5B6AA5..0x5B6ABF).
- The empty hand stops 3.2 (fn_0046C040, 0x46E0D3) × handScale (+0x4834) × Size1 (+0x90) short of the land, not over the sea (land y < 0.1, [0x8AB22C] at 0x46E02E); every branch is clamped to [2, CHand +0x4838] (0x46E0EA..0x46E147). (inferred) Size1 is 1 for the hand.
- **(not ported)**: with no land hit the original takes |camera − hand->pos|.
- Block 6 (0x5B622A..0x5B629C): a SpellIcon under the hand gives its MapCoords and altitude as the point, the depth target |camera − point| − 3.2 × the hand size ([esp+0x7C], 0x5B6298; used at 0x5B6B08..0x5B6B59), and the flag esp+0x50 (0x5B6251) makes the up the land normal (0x5B6FA1..0x5B6FC4).
- The up target over an object the hand feels (0x5B6808..0x5B6848): 0.25 [0x8AB3D4]·d − n + (0, 0.5, 0), d = norm(camera − hit), n the hit face's normal as the hand's mesh test gives it, turned away from the eye (see [The hand's mesh test](#the-hands-mesh-test-fn_00865020)).
- `HandShouldFeelWithMeshIntersect` vt 0x59C: Object 0x4025B0 = 1; Tree 0x55D9E0, DeadTree 0x5109C0, TotemStatue 0x561120, LandscapeVortex 0x5FD600, SpellSeed 0x727F60, MapShield 0x72C010 = 0.
- Over a felt object the land normal is used instead when the hand is lower over the land than half its size (3.2 × +0x4834 × 0.5, 0x5B6DC8..0x5B6DD9 / 0x5B6F8C..0x5B6F9F); the result is normalised (0x5B6FFC..0x5B7038).

### The mouse ray

The original's mouse ray is a direction only, from the eye: a user that needs a point starts it at `g_camera`
(0xEA1DB8), as `HandStateTug::Update` does (0x5B8070, decomp_pickup `hand.cpp` 1671: `hand->position = cam + dir * t`);
the hand's heading in `CHand::PrepareForDrawing` takes only the direction.

- fn_0074CBD0 (0x74CBD0, size 0xD0) builds it: `LH3DTech::Get3DPointFromScreen` (0x81B370) with depth 0 gives the
  mouse's point on the near plane in world space (0x74CBF6); the eye is taken away (0x74CBFB..0x74CC24) and the
  vector normalised in float as 1 / sqrt(z² + y² + x²) times each axis (0x74CC58..0x74CC85). It is left as it is only
  when all three axes are exactly 0 (0x74CC2D..0x74CC56), which a point on the near plane never is.
- `Get3DPointFromScreen` (0x81B370, size 0xE0), mouse in integer pixels: x = (mouse.x − W/2) · [0xC3812C] / (W/2),
  y = (H/2 − mouse.y) · [0xC38130] / (H/2), z = the near plane [0xE839E0]; with a depth d ≠ 0 all three are scaled by
  d / near (0x81B3B0..0x81B3BA). The point is R · (x, y, z) + `g_camera`, R the camera's axes at [0xEA1D28..0xEA1D48],
  each row summed z, y, x (0x81B3BE..0x81B43F). Nothing clamps the mouse: the screen's edges are at ±1 (W/2 and H/2
  off the centre), and a point beyond them extends the same way.
- [0xC3812C] = near · tan(fov / 2) and [0xC38130] = [0xC3812C] / aspect, the aspect [0xE839EC] = W / H
  (`UpdateViewPort` 0x819030): written by `ChangeFov` (0x8195B0, 0x819647..0x819663) and again by `GCamera::Update`
  with the new near plane (0x4424BA..0x4424E2). The field of view [0xEA1DD0] is the horizontal one (70°, 0x3F9C61AA,
  passed to `ChangeFov` by `UpdateViewPort` at 0x819045).
- So the direction is normalize(R · (ndcX · tan(fov / 2), ndcY · tan(fov / 2) / aspect, 1)): the near plane only
  scales the point, and the ray does not depend on it beyond rounding.
- openblack: `Camera::RayFromEye` (the eye, and that direction from the view's axes, `_xFov` and `_aspect`, summed and
  normalised in the original's order). The hand's mouse ray (Game.cpp, to `HandSystem::ResolveCursorPoint`, the
  creature's hand and the tug), `Camera::RaycastScreenCoordToLand` / `RaycastMouseToLand` (the default camera model's
  mouse and centre rays and its vertical line of rays), the gestures (`GestureInput`) and the temple camera's mouse use
  it. `Camera::DeprojectScreenToWorld` started the ray at NDC depth 0 of glm's −1..1 projection, 2·near·far /
  (near + far) ≈ 2·near in front of the eye, with its direction through the inverse view-projection; it stays only for
  the console's tooltip and the editor's picks.
- Because the hand is `origin + mouseDir × distance`, the eye start moves it by about 2·near along the ray wherever
  its distance is clamped (at least 1, [2, reach]) or the hand's distance zoomer lags; where the distance follows the
  surface the point is the same. That is the original's placement.

### Holding and grain place

- g_C5E650: the required hand position of the last HOLDING / GRAIN frame (`HandStateHolding::Update` 0x5B3EFC / 0x5B3FC9): the cursor's ground point g_C5E640 while a pile is locked (0x5B3EFC), else ORHP's position before vt 0x1C / 0x18 (0x5B3FC9).
- g_C5E640: the cursor's ground point, written by fn_0046DF60 (0x46E057), which only the state Updates call (Holding 0x5B3DAE / 0x5B5FC7, Camera 0x5B0438), not HandStateNormal's.
- The tug's grip follows the drawn lean (HandStateTug's objectMatrix +0xEC, 0x5B81C5..0x5B822C).

### The hand's mesh test (fn_00865020)

Over an object with a mesh (not a Living other than the creature, not a SpellIcon, not one it lifts to the surface)
`ObtainRequiredHandPosition` places the hand with a ray test of the object's mesh of its own, not with the cursor's
pick: the call at 0x5B6529 to `fn_00865020` (size 0xA00, `ret 0x20`), a method of a `MeshIntersect` (ecx, 0x28 bytes
at esp+0x8C). Its arguments: the mesh, its matrices (the 3D object's at +0x14; a creature's `TransformedMatrices`), the
ray's start (`g_camera`, esp+0x60), its direction (esp+0x54, unit: the mouse direction, or while holding the raised
one, normalised at 0x5B6335..0x5B6367 / 0x5B6384..0x5B643D), a byte choosing the sub-mesh filter (0 here), the out
point (esp+0x3C), the out normal (esp+0x10) and a byte that lets t be 0 or less (0 here).

- **Sub-meshes** (0x86505D..0x865091): with the filter byte 0 only those with flag 0x20000000, bit 0 of `lodMask` (the
  first level of detail); with 1 only those with 0x2000 (`isPhysics`). Nothing else is read: status, window and
  physics sub-meshes are tested when they carry the first level. `LH3DSubMesh::Create` 0x87FA00 first gives a
  sub-mesh with no level and bit 11 clear every level (|= 0xE0000800, 0x87FA06..0x87FA1F). In AllMeshes.g3d no
  physics sub-mesh carries the first level, no sub-mesh takes the load default, the status sub-mesh 0 of every abode,
  temple and wonder carries it (190 sub-meshes), and 86 meshes have drawn sub-meshes of the lower levels only.
- **World space**: each primitive's vertices are moved first (0x8650C8..0x86525E, into a buffer at 0xF963C0): a mesh
  with bones (mesh flag 0x100) takes each vertex group's matrix (matrices + bone × 0x30), any other the one matrix,
  x' = ((z·m[6] + y·m[3]) + x·m[0]) + m[9] (rows of three, the position last). Distances, the epsilon and the normal
  are in world units, with the object's scale.
- **No bounding volume**: every triangle of every primitive is tested (0x865260..0x8659C5).
- **The plane**: n = (V1 − V0) × (V2 − V0), made unit unless it is zero (0x8652B7..0x865405); denom = n · dir; the
  plane is missed when |denom| ≤ 0.005 ([0x9A3BC4], 0x86544C..0x86546E), on either side: **no back-face cull**.
  t = (n · V0 − n · origin) / denom (0x8654A0), missed when t ≤ 0 (0x8654AC..0x8654BD); P = origin + t · dir.
- **Inside**: for each edge V0→V1, V1→V2, V2→V0, whether (edge × (P − its start)) · n > 0 (0x865520..0x865737); the
  hit needs three or none (0x865739..0x865742). With n from the same triangle an inside point gives three; a point
  on an edge gives 0 for that edge and misses.
- **Nearest**: the first hit is kept and a later one replaces it only when its t is strictly smaller
  (0x865748..0x86575B); a tie keeps the first in order (sub-meshes, primitives, triangles).
- **Out**, with both out pointers (0x865761..0x86580B): P (also kept at 0xFA23C0) and the unit n turned along the
  ray, n when denom > 0 else −n (0x8657A1..0x865805): normal · dir > 0, away from the eye whatever the winding.
  Returns 1 on a hit, else 0. Without both pointers (0x86580E..0x8659A8) it fills the `MeshIntersect` instead (the
  primitive and sub-mesh, the three vertices and their bones, two barycentric coordinates in [0, 1] with the 0.005
  guards, a hit outside them dropped); the hand does not take that branch.
- **After the call** (0x5B652E..0x5B66F6): up = norm(normal) (0x5B6536..0x5B65A5); toCam = camera − P;
  s = −(toCam.z · dir.z + toCam.x · dir.x + toCam.y · dir.y) (0x5B65A9..0x5B65ED). Holding ([esi+0x4904]): the
  point is camera + mouseDir · s on the mouse direction (0x5B65F5..0x5B668F). s < 1: the point is camera + dir and
  toCam = −dir (0x5B6693..0x5B66F6). Otherwise the point stays P.
- In AllMeshes.g3d (V1 − V0) × (V2 − V0) points along the vertex normals for 83% of the drawn triangles (97381
  against 19579), so on a face the eye sees the test's normal is mostly the face's inward one, and the up target
  0.25·d − n + (0, 0.5, 0) leans out of the surface.
- openblack: `ecs::hand_mesh_ray` (src/ECS/HandMeshRay.h): `Tested`, `ToWorld`, `Triangle`, `Nearest` and
  `AlongRay`, the products worked in double and the stored values rounded to floats. `HandSystem::PickObjectAlongRay`
  runs it for the object it picks by its mesh (and the temple's entrance), with the matrix it is drawn with, into
  `CursorHit::surface`; `ResolveCursorPoint` places the hand at its point (s by `AlongRay` when holding) and takes its
  normal for the up target, and a miss keeps the land's point and up. The pick (`L3DMesh::RayIntersect`) is
  unchanged; until 2026-10-09 its hit also placed the hand, which differed: the lower levels of detail were tested,
  the temple heart's status shell was not, the normal was the winding's (so the up target leant into most surfaces),
  there was no 0.005 epsilon, points on edges hit, and the test was in the mesh's space.
  Tests: test/hand/test_hand_mesh_ray.cpp.

## Object under the cursor

- `GInterface::SendObjectDrawCollision` (0x5D56C0): every drawn object is tested with exact per-triangle
  collision (trees with a per-pixel test instead, see [A tree under the cursor](#object-under-the-cursor) below); the closest wins; whatever is in the hand is ignored.
- **Villagers** ("human" flag of the LH3DObject, only `Villager::CallVirtualFunctionsForCreation` 0x74FC70): no
  per-triangle test. They count if the mouse is inside the **on-screen circle of their bounding sphere**
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80; radius = scale × half-diagonal of the box), at distance
  `|(x, y + halfHeight·scale, z) − camera| − (R + near plane)`, measured from the eye `g_camera` (0x5D5890..0x5D58C2).
  The near plane is the drawn one, [0xE839E0] (read at 0x5D58CA), which `GCamera::Update` writes every update from
  `LandFeature::GetNearClipping` (0x5E2F30, at 0x4424AF): (h·0.05)·3.2 + 0.3 in float steps over the camera's
  height above the terrain, 0.3..3.5, 0.1 under `SET_GRAPHICS_CLIPPING`. openblack: `HandSystem::PickObjectAlongRay` reads the drawn near clip (`EngineConfig::cameraNearClip`).
  The other draw collisions sent without a triangle distance (the branch 0x5D58DC..0x5D5942, whose tests are not read
  here) take the distance near / [0xF05164] (0x5D5944..0x5D5950), the drawn near plane too; [0xF05164] is not
  identified. Animals and creature: triangles, like everything else.
- `UpdateInterfaceCollide` (0x5D5A70): the terrain distance counts 2.3 more (fn_005D5980); if it is still
  in front, the object only counts if the terrain point falls inside its XZ footprint. Both distances are view depths
  (the w of the world-to-clip matrix: the land point's w from fn_005E5620 0x5E56F6..0x5E5719 plus 2.3 at 0x5D59BF,
  against the object's pending +0x404 at 0x5D5ABB..0x5D5ACE), so the 2.3 is a depth, not a length along the
  cursor's ray. openblack compares them the same way (`ecs::hand_cursor_depth`): both distances along the ray are
  turned into depths by the camera's forward before the 2.3 is added.
- If there is nothing when clicking: `FindObjectNearMapCoord` (0x5D39E0), the closest within ±5 units and only if it is
  closer than the clicked point; first the fish of a fish farm if it is water. **It is not a hover range**:
  openblack uses only the object from the per-triangle pick (it used to have an invented radius of ≥3.5 units).
- **A tree under the cursor (issue #130, read 2026-10-09).** There is no forest-wide or patch pick: a tree is under
  the cursor only where its own drawn pixels are opaque, and nothing in the cursor search resolves a Forest, a forest's
  area or its cells to one of its trees.
  - `Tree::Draw` 0x74AB00 hands the tree itself (not its forest) to `Game3DObject::AddForDrawing` 0x63B5D0 (0x74B0C7..0x74B0CC),
    which sends `SendObjectDrawCollision(tree, 0, NULL)` 0x5D56C0 only when the 3D object's on-screen test marked it
    (`g_last_selected_box`, 0x63B653..0x63B66E).
  - `SendObjectDrawCollision`, with no distance given, tests the object's info type (0x5D58DC): type 6
    (`OBJECT_TYPE_FOREST_TREE`, every `Tree`; `Object::IsPartOfForest` 0x6380F0 is the same test) first refreshes the 3D
    object's matrix from `GetWorldMatrix` (vt +0x63C) when the tree is in the map (0x5D58E9..0x5D58FB), then asks the 3D
    object's vt +0x1E4, `CheckPixelCollide` (0x5D5905). The 3D objects with the human flag (villagers) took the sphere
    test before (3D object vt +0xB0, 0x5D585A); an abode with +0x90 set asks fn_005D5E80 (0x5D5917..0x5D5922); every
    other object asks vt +0x1E0, `CheckTriangleCollide` (0x5D592C / 0x5D5938). A miss sends nothing (0x5D5942..0x5D5954).
    The hit's distance is the near clip over [0xF05164] (0x5D5944..0x5D5950), the view depth at the mouse pixel that
    the pixel test leaves there.
  - A tree's 3D object is an `LH3DStaticObject` (vtable 0x9A2974; (inferred) from `LH3DObject::Create`'s type 0, not traced
    from the tree's creation): vt +0x1E0 = fn_00811300, vt +0x1E4 = fn_008111C0. The two
    walk the same sub-meshes and primitives in the same order (status mask, LOD); they differ only in the primitive's
    test: fn_00855440 (triangle) against fn_008561D0 (pixel).
  - fn_008561D0 projects the primitive's vertices to the screen, keeps the triangles facing the camera (both sides when the
    primitive is double-sided), and for each one that holds the mouse pixel (0x856877..0x8569C6) fn_00856B30
    interpolates its u, v at that pixel, perspective-correct, and writes near clip / z there ([0xF05164]). With a
    material texture it then asks fn_00838A70(u, v), and the first triangle that passes is the hit: a texture of kind 2 (a
    mesh pack texture, read back as ARGB 4444) is hit only when its 64 x 64 alpha mask (fn_00838F00, built once:
    the high nibble of each sampled texel, texture +0x12C) is not 0 at (u x 64, v x 64), each clamped to 0..63; any other
    kind counts as hit. So the transparent parts of a tree's leaf cards are not under the cursor.
  - The details, read 2026-10-09 from the x86:
    - **Projection** (0x856218..0x8563A6 and 0x85648C..0x85662E): x, y and w through the world-to-clipping matrix; outcode
      0x20 when w < near clip, 0x10 when x > w else 8 when -w > x, 4 when y > w else 2 when -w > y. Only a vertex with
      outcode 0 is projected: 1/w stored, x' = (x/w + 1) x half width clamped to 0..g_MaxScreen x (639), y' = half
      height - (y/w) x half height clamped to 0..479, rhw = near clip x 1/w; u, v copied.
    - **Triangles** (0x856690..0x85685D): in the primitive's order. All three outcodes 0: kept, after the facing test when
      the primitive is not double-sided (byte +5 bit 0 clear, g_NoBackfaceCull): (c.y - a.y)(b.x - a.x) - (b.y - a.y)
      (c.x - a.x) > 0. All three sharing a bit: dropped. Otherwise fn_0081A760 clips the triangle against the planes
      from 0x20 down and appends the pieces in its place, with the same facing test (0x81A99C..0x81AA7E).
    - **The pixel in the triangle** (0x8568A0..0x8569C6): the mouse (integers [0xEA1AC8], [0xEA1ACC]) as floats; each
      corner minus the mouse stored as floats; cross(d0, d1) > 0 needs cross(d1, d2) and cross(d2, d0) both >= 0,
      otherwise both must be <= 0. So the edges count, but on the first edge (cross 0) a triangle wound to the positive
      side loses its pixels.
    - **u, v at the pixel** (fn_00856B30): the corners sorted by y (top, middle, bottom, in the order of its compares),
      the long edge top to bottom and the short one (top to middle while the mouse row is above the middle corner, else
      middle to bottom) at the mouse row, each with rhw, x, u rhw / rhw and v rhw / rhw; then along the row between the
      two (s = (x2 - mx) / (x2 - x1)). rhw at the pixel to [0xF05164], u to [0xF05170], v to [0xF05174]. The short
      edge's v stays on the float stack, unstored.
    - **The mask** (fn_00838A70, fn_00838F00): kind = texture +0x10 & 0x3F. Column = __ftol(u x 64), row = __ftol(v x
      64), truncated, each clamped to 0..63 (no wrap); hit = byte (row x 64 + column) != 0. The mask (4096 bytes at
      +0x12C, built at the first test): byte (r, c) = the high byte of the 16-bit texel (r h / 64, c w / 64), both
      truncated, & 0xF0.
    - **Which texture**: each primitive's own material texture (primitive +8); no texture counts as hit. Kind 2 is the
      mesh pack's textures (`_SetPackedTexture` 0x8377E0 makes them with fn_008379E0(0, 2, ...)); the lock for the mask
      (fn_00838AF0, kind 2 at 0x838B30) blits the texture into a system-memory surface of the device's first 16-bit
      alpha format (table 0xF05190, filled by fn_0085DC20, which settles on A 0xF000 R 0xF00 G 0xF0 B 0xF when the
      device offers it), so the high nibble is the ARGB 4444 alpha. The pack's textures are DXT1 (133), DXT3 (17) or
      DXT5 (74) in AllMeshes.g3d. Which of a tree's primitives have transparent texels is the data's, not read here.
    - **Depth**: near clip / [0xF05164] = the view depth w at the pixel, of the first triangle that passes (not the
      nearest of the tree's).
    - **The triangle test** fn_00855440 has the same projection, clipping and pixel test on the mouse pixel and asks
      fn_00856A00 (rhw only) instead: every other object is also hit by the first of its triangles that holds the pixel.
  - **What the depth is used for** (read 2026-10-09). Every distance the cursor search compares is a view depth, the w
    of `g_world_to_clipping` (0xEA9E40), but for the villagers' sphere:
    - `SendObjectDrawCollision` keeps an object only when its distance is strictly less than the pending one's
      (GInterface +0x404), both with a distance given (0x5D570E..0x5D5732) and with the pixel or triangle test's
      near / [0xF05164] (0x5D5956..0x5D5965); `fn_005D5E40` (0x5D5E40, called at 0x5D5744) stores the object at +0x400
      and its distance at +0x404, or clears both (0, FLT_MAX) when the object is not interactable. With an object
      pending, a mesh whose bounding sphere's centre lies deeper than the pending distance + its radius x scale is not
      tested: the centre's camera z, from the camera's axes at [0xEA1D30], [0xEA1D3C], [0xEA1D48] + [0xEA1D54]
      (0x5D5770..0x5D5854; radius 1 at the 3D object's position when it has no mesh). The villagers' distance,
      |centre - camera| - (radius x scale + near) (0x5D5872..0x5D58D2), goes through the same strict compare.
    - The land's distance is a view depth too: `fn_005E5620` gives the land point under the mouse and its w,
      x [0xEA9E48] + y [0xEA9E54] + z [0xEA9E60] + [0xEA9E6C] (0x5E56F6..0x5E5719), the row the pixel test's
      projection takes w from (its matrix is `g_world_to_clipping` times the object's, 0x8563CF..0x856472);
      `fn_005D5980` adds 2.3 (0x5D59BF) and stores it at +0x3FC (0x5D59E5). `UpdateInterfaceCollide` compares the
      object's +0x404 with it directly (0x5D5ABB..0x5D5ACE) and, the object chosen, copies the object's Pos to +0x3F0
      and its distance to +0x3FC (0x5D5D09..0x5D5D2B). The bubble's depth (+0x40C) is compared with the same +0x404.
    - The hand is not placed at that depth. `ObtainRequiredHandPosition` takes the object of the render collide
      (+0x3C8) and tests its mesh again along its own ray from `g_camera`, the mouse direction or, while holding, the
      raised one: fn_00865020 (0x5B6529), whose hit point gives the hand's distance, s = -(camera - point) . dir
      (0x5B65A9..0x5B65ED); when it misses (0x5B6530 to 0x5B6852) the hand keeps the point at the view distance along
      the mouse ray and takes the land's up. See [The hand's mesh test](#the-hands-mesh-test-fn_00865020). No hand
      code reads the collide's distances +0x3C4 / +0x3CC (a scan of every instruction with those offsets: only their
      writes).
    - So for a tree: the first passing triangle's depth takes the cursor (against other objects and the land), and the
      hand goes to the nearest of its triangles along the ray, whatever their texture.
  - There is no draw of the Forest object (a `GameThingWithPos`); a `BigForest` is a `MultiMapFixed` with a mesh of its own,
    collided by triangles like any other object (`BigForest::Draw` 0x438F60 jumps to `MultiMapFixed::Draw` 0x518090), and
    taking it gives a new Conifer ([trees.md](trees.md#pick-up-rules-and-bigforest)).
  - The only reach beyond what is drawn is the press with nothing collided: `ActionPressed` calls `FindObjectNearMapCoord`
    0x5D39E0 with the action collide's point (GInterface +0x3F0) only when the press has no object yet and the action collide (+0x400) holds none
    (0x5D148F..0x5D14A6). It takes the nearest object of any class but a fragment, at its 2D distance from the land behind
    the hand, under 5 m, and only when that distance is not more than the action point's own distance from the land behind
    the hand (0x5D3CB4..0x5D3CDB). In a forest that object is usually a tree: the one nearest to the land behind the hand,
    first of a tie in the cell walk (x outer, z inner, fixed list then mobile). No random draw is made.
  - openblack: `HandSystem::PickObjectAlongRay` gives a tree (the `Tree` component) the pixel test
    (`ecs::hand_pixel_pick`, src/ECS/HandPixelPick.h): its drawn sub-meshes (no physics, no status, the LOD 0 bit),
    primitives and triangles in order, through the camera's view-projection on the window's screen, at the pixel the
    pick's ray goes through, rounded to a whole pixel; each primitive with its texture's mask (`PickMask`,
    src/3D/PickMask.h), asset data kept in the resource system's `GetPickMasks()` under the texture's id: `PickMaskLoader`
    makes it once for every mesh pack texture, from `Texture2DLoader`'s pack path, out of its DXT alpha
    (`dxt_alpha`, src/3D/DxtAlpha.h, then `graphics::shadow_math::MakeAlphaMap`, the sampling of fn_00838F00); the
    renderer never reads it. A mesh's own skins have none, as the original's other texture kinds. The first triangle that
    passes gives the depth, turned into a distance along the ray (`hand_pixel_pick::AlongRay`: on one ray it orders
    the objects as the view depths do) to compete with the other objects and the land. `ResolveCursorPoint` places the
    hand over the tree, as over every object picked by its mesh, with the hand's own mesh test along the same ray
    (`ecs::hand_mesh_ray`, `CursorHit::surface`, see [The hand's mesh test](#the-hands-mesh-test-fn_00865020)), and
    keeps the land point when that ray crosses none. Every other object is picked with `L3DMesh::RayIntersect`. Tests: test/hand/test_hand_pixel_pick.cpp, test/test_dxt_alpha.cpp.
    `HandSystem::FindObjectNearMapCoord` (HandNearObject.cpp) is the press fallback above, already ported, and
    `HandSystem::Update` calls it at the same point.
    Its search is `ecs::hand_near::NearestToLandBehindHand` (src/ECS/HandNearObject.h), extracted unchanged and
    characterised by test/hand/test_hand_near_object.cpp: PressBetweenTwoTreesTakesTheNearest, PressJustInsideAndJustOutsideFiveMetres,
    ActionPointGateOnBothSidesOfEquality, TieGoesToTheFirstCellInTheWalk, TieInOneCellGoesToTheFixedList,
    FragmentIsNeverTaken, SkippedObjectIsNeverTaken, NothingWithinFiveMetres, WalksTheSquareXOuterZInner and
    CellsOffTheMapAreNotWalked.
- Message for the amount in the hand ("Cantidad: N", font `j0`, yellow with shadow, no background): see
  [rendering.md](rendering.md#text-the-originals-fonts-and-the-hand-message); it is shown while the hand holds food or wood.
- Boned meshes (villagers, animals) are tested in their rest pose, which is how they are drawn
  (`L3DSubMesh`: collision positions × the group's bone chain). Animals count as Living when placing the
  hand (distance to the centre minus the 2D radius).
- **Exclude everything that moves with the hand** (held object, tree being uprooted, roots, particles): otherwise the
  ray hits it and the hand climbs towards the camera endlessly.
- **The temple's entrance** is an object of its own (`CitadelEntrance`, made by `CallVirtualFunctionsForCreation`
  0x4675A0 at 0x4676B9) whose 3D object (type 1, 0x4676CD) holds `Data\Citadel\OutsideMeshes\Entrance.l3d`: the
  heart's 3D object's vt +0x204 (0x80BBC0) returns the mesh that `InitTemple` loads once (0x88297A..0x882988, global
  0xFAA7E4). It is placed at the heart's point, angle and scale 1 after the land is flattened (0x46774F). Its draw
  does nothing; `CitadelHeart::DrawNow` collides it instead, through `Game3DObject::AddJustForCollide` 0x63B7E0
  (0x4672F4..0x46731B: the on-screen box test, then `SendObjectDrawCollision`, an exact triangle test), only while the
  per-player flag 0xEB9A1C[player] is set. `LH3DCitadel::SetPercent` 0x883120 sets it when the draw percent is 1
  (0x8831AD); `LH3DIsland::Create` clears it (fn_00828A50). Entrance.l3d is one 50-triangle sub-mesh,
  x -8.47..-2.32, y 0..6.41, z -13.89..-7.40 in the heart's frame: the doorway. For the hand it is an Object with the
  defaults (not placeable, needs the influence, felt with the mesh), so a press on it taps it as an abode is tapped.
  openblack: `HandSystem::PickObjectAlongRay` tests the mesh at the entrance's Transform, on the land, while
  `worship::citadel::EntranceCollides` (approximate: the entrance's own heart's draw percent, not the per-player latch),
  and the press taps it while `IsEntranceValidToTap`. Its tooltip is state 18's (see [Tooltips](#the-text-of-each-state-table-0xbf1c10)).
  - **Why the doorway finds the entrance and not the heart.** Entrance.l3d's 50 triangles are, vertex for vertex, 50
    triangles of the first temple's drawn sub-mesh 3 (b_first_temple.l3d; b_temple00 has them unchanged too): the
    recessed faces of the opening. `DrawNow` collides the entrance (0x46731B) before it sends the heart's own collision
    (0x467336), and `SendObjectDrawCollision` takes a new object only when it is strictly nearer than the pending one
    (0x5D5723..0x5D5732, 0x5D5956..0x5D5965), so on the doorway's tie the entrance keeps the cursor and the hand goes
    into the opening (its hit point is the recessed face).
  - Every temple mesh's sub-mesh 0 has status 1: a closed 224-triangle shell, x -19.01..18.93, y 0..21.41,
    z -17.45..17.57, in front of the doorway. The built temple does not draw it, and the collision is of the drawn
    triangles, so it is not collided. openblack's renderers skip status sub-meshes; the pick tested them, so the shell
    took the cursor in front of the doorway (an invisible wall, 2026-10-08). openblack: the heart is picked with
    `L3DMesh::RayIntersect(..., skipStatuses)` and the entrance wins a tie. (pending) the other objects' status
    sub-meshes (abode scaffolds) are still picked while undrawn.
- Screen objects (the «Did you know?» bubble): fn_005D5A30 is `if (+0x38) return 1; if (!(+0x40C > depth)) return 0; +0x408 = it; +0x40C = depth; return 1` — the nearest offer wins, a tie keeps the first, and while the hand grips one (+0x38 & 0x10) nothing changes.
- `UpdateHandRenderCollide` 0x5D0610 (PreDrawProcess 0x5CE9E5): the pending offer becomes this frame's (+0x3D0 = +0x408 at 0x5D0657, +0x3D4 = +0x40C at 0x5D0660); unless gripping, the pending one is cleared (0x5CEA5C..0x5CEA97).
- `UpdateInterfaceCollide` 0x5D5A70 arbitrates the offer (skipped while gripping, 0x5D5A9C): the object +0x400 against the bubble, the nearer wins (bubble nearer: 0x5D5B77 clears the object; else 0x5D5C0D / 0x5D5C23 drop the bubble).
- A land hit nearer than the bubble (+0x3FC, the land's distance + 2.3, fn_005D5980 0x5D59BF) drops it (0x5D5B8F..0x5D5BAA); the bubble's depth is its camera z, compared as it is with the ray distances.
- **(not ported)**: the second land test fn_00848F90 (+0x414, 0x5D5C7F, under [[+0x39C] +0x12C] +0x24).
- `Animal::ValidForPlaceInHand` 0x419B40: the species' playerCanPickUp.
- `Villager::ValidForPlaceInHand` 0x7564A0 = IsReachable (vt 0x530); Villager's InterfaceValidToTap is Object's 0x4196B0 = 0, so an unreachable villager is not the target at all.
- `BigForest::ValidForPlaceInHand` 0x438DB0 = 1; BigForest is not tuggable (0x438DC0).

## The hand on a creature

openblack: `HandSystem` (the press branch, `UpdateCreatureLock`, `UpdateCreatureInteraction`, `LeaveCreatureState`),
`ECS/HandPressChain.h` (the press's branch order), `ECS/CreatureHandPackets.h` (the packets 0x59 / 0x5F), the creature
hand `CreatureHandSystem` (strokes and slaps) and `Creature/CreatureHandRules.h`.

### The action press (`GInterface` 0x5D1330, decomp_pickup `interface.cpp` "ActionPressed")

In this order:

1. Something in the hand (status +0x90): the holding press (0x5D1560). Ours: the seed and held branches.
2. A bubble in the action collide (+0x408): action state 17 (0x5D4330). Ours: the screen object branch.
3. The leash in the action collide (+0x410): the leash pull, action state 21 (0x5D4440). **(not ported)**: needs a pick of the rope.
4. The object: the action collide's (+0x400), or else `FindObjectNearMapCoord` (0x5D39E0) at the action point. When `ValidForLockedSelectProcess` (vt 0x6CC) is 1, `IsCannotBePickedUp` (vt 0x180) is 0 and Flags & 0x10 is 0:
   - not a creature: in the influence (+0x48) or `InterfaceMustBeInInfluenceForInteraction` (vt 0x714) false → `StartTapOrLockedSelect` (0x5D1A00);
   - a creature (vt 0x34): only when its player is mine, or its player is set and `IsAllied` (0x64D5D0) with mine, **in or out of the influence** (0x5D13FB..0x5D1481) → `StartTapOrLockedSelect`. A creature of no player or of an enemy falls through to step 5.
5. The pick-up / tap path: `IsSpaceInHands`; then `ValidForPlaceInHand` (Creature 0x473F60 = 0), `InterfaceValidToTap` (Object 0x4196B0 = 0 for a creature), or the leash on and not tied → `StartGrab` (0x5D1740); else `ClickOnLand` (0x5D3D10).

So a creature, being the collided object, keeps out the fish farm (the water branch of `FindObjectNearMapCoord`):
one the hand may not hold (an enemy's, nobody's, or one `ValidForLockedSelectProcess` refuses) goes on to step 5
with the creature still the object (0x5D148F..0x5D14AB: `FindObjectNearMapCoord` only with no object), where it is
neither placeable nor tappable. Ours: no branch (`cursorIsCreature` keeps the fish farm out); (not ported) the leash's
`StartGrab` and `ClickOnLand`.
Ours keeps the order in `hand_press::Choose`: screen object, field, hovered object, tap-only object (abodes, temple
entrances), creature, fish farm, seed, held object; while a creature is held or let go no press is taken (the
interface is in action states 3..6).

- `Creature::ValidForLockedSelectProcess` 0x476E10: not an Ogre (info +0x1F4 != 13), Flags bit 0x400 (byte +0x25 & 4) clear, not (mind +0xF60 == 9 and mind +0xFB4 <= 2), it has a player (vt 0x1C), and `CreatureReceiveSpell::IsCreatureSpellActive(0)` (+0x370, 0x4F51E0) false. openblack's `creature_hand::MayHold` stands for it with asleep and frozen.
- The hand holds only **its own or an allied player's** creature (raffclar's rule allowed any god's). openblack has no alliances: only the player's own (`creature_hand::IsFriendlyTo`).

### The locked select on a creature

- `StartTapOrLockedSelect` 0x5D1A00: refuses with the grab bit (+0x38 & 8) or no object; stores the press's engine sample (+0x454) and turn (+0x458). Action state 19 (the 225 ms tap-or-lock wait, 0x5D53D0) when the object is `InterfaceValidToTap`, the leash is on and not tied, or the leash is tied to this object; otherwise the locked select starts at once (0x5D1950). A creature is never valid to tap, so without a leash in the hand it starts at once.
- 0x5D1950: `NetworkUnfriendlyStartLockedSelect` (vt 0x6D4), packet 0x1B, action state 3. For a creature of another player it also flips the interface's word +0x37C[player number] (0x5D19C4..0x5D19E5, not named).
- The 0x1B handler sets Flags & 0x10 and calls `Creature::NetworkFriendlyStartLockedSelect` 0x476E70: `GoIntoStateOfFinishingAction` (0x477B20), then the interface status +0x24 |= 0x20. Ours: `ApplyStartLockedSelect` takes hold through the creature hand (`Grab`).
- Action state 3 (0x5D4960): state 4 once the object has Flags & 0x10.
- Action state 4 (0x5D4830): on the release (+0x39 & 0x40, the latched release bit): +0x38 = 0x10, `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476ED0: zeroes the 3D creature's +0x48C0, then fn_004806D0), then state 5's process at once.
- Action state 5 (0x5D49B0): when the object still has Flags & 0x10 and `IsReadyForNetworkUnfriendlyEndLockedSelect` (0x476F00) is 1 (its look reached `ReachedLookDestination`, no body action unless `IsActuallyDead`, no facial action): `NetworkUnfriendlyEndLockedSelect` (0x476F60) and packet 0x1C, then state 6; else it stays in 5.
- Action state 6 (0x5D4A10): once Flags & 0x10 is clear (the 0x1C handler ran) the action resets.
- The 0x1C handler calls `Creature::NetworkFriendlyEndLockedSelect` 0x476E90: `LH3DCreature::ReconnectToGame` (0x480730), status +0x24 &= ~0x20.
- The exit of states 3..6 (0x5D4870, `EndAction`): unless the object's byte +0xA & 1, `GetReady...`, `NetworkUnfriendlyEnd...` and packet 0x1C; then `PSysGlobal::StopMultiPickup`.
- The hand states of these rows (table 0xD18278, filled by 0x5D7960; 32 bytes a row: +0x00 a function, +0x10 a constant, +0x18 the name): 3 "WAIT LOCKED SELECT" 18 (0xD182E8); 4 "IN LOCKED SELECT" fn 0x5D80C0: no object 3, a creature (info +0x10 == 1) **19**, or 14 when the creature's +0x10AC is not 0, any other object 14; 5 "WAITING FOR LOCK OFF" 18 (0xD18328); 6 "WAITING FOR LOCK OFF COMPLETION" 18 (0xD18348).
- A creature merely under the hand goes through fn_005D7F20: not a pile or field, not valid to place in the hand → 18, in and out of the influence.

### The CREATURE state and its packets

- `CHand::PrepareForDrawing` 0x46C550 adds the camera time step (`GetCameraTimeInc`, g_delta_time inside the citadel) to CHand +0x4900 every frame, then `GetRequiredState`, then fn_0046CF80(required), then the state change.
- `GetRequiredState`, on the empty-hand path, calls `OperateInteractionCamera(GInterface +0x3AC)`, then `case 19` → CREATURE 7 when CHand +0x49B8 is set, else NORMAL 1.
- `OperateInteractionCamera(mode)`: mode 19: +0x49B4 = the hover collide's object (+0x3C8) as a creature; when it is one, +0x49B8 = its 3D creature (+0x160 → +0x58). If +0x49C0 is 0: a camera mode object for the creature (fn_0044A850), +0x49C0 = 1 and **+0x4900 = 0**. Any other mode with +0x49C0 set: `GCamera::PopViewMode` (0x441C50), +0x49C0 = 0, then **if +0x4900 < 450 and the creature's player is the local player: packet 0x5F** (0x46CCBD).
- fn_0046CF80(required): entering 7 only when the creature (+0x49B4) has Flags & 0x10 and its +0x3CC is 0 (not named; `IsReadyForNetworkUnfriendlyLockedSelect` 0x476EB0 tests the same field), otherwise the current state is kept. Leaving 7 with +0x49B4 set: if `IsReadyForNetworkUnfriendlyEndLockedSelect` is not 1, **packet 0x59** with the creature's index and clamp(state +0x4898 → +0xD8, −1, 1), then +0x49B4 = +0x49B8 = 0; when it is 1 nothing is sent and the fields stay (0x46CFF4).
- So in one frame 0x5F is pushed before 0x59.
- `HandStateCreature` (Enter 0x5B13F0, Update 0x5B17B0, Exit 0x5B2A80): Enter sets the creature's mirror bones and three zoomers. Update (0x12D0 bytes, not traced whole): `IsImmersionPlaying` / `StartImmersion` / `StopImmersion`; `LH3DCreature::ForceIndividualAction` at 0x5B1E51 and 0x5B2218 (a slap and a stroke), each followed by packet 0x27 (fn_00550F80); `InitialiseDestructionBones` / `StartDestruction`; packet 0x63 (fn_00551040, at 0x5B1DD0); zoomers 0.3 / 0.9 / 2.5; the clip `Morphable::GetAnim(state +0x114)` (0x5B29DE). Exit: `StopImmersion`.
- Packets (`GPacket::ProcessPacket` 0x63C420, jump table 0x63DDCC):
  - 0x27 0x63CD27: nothing when the sender's interface is the local one (0x63CD32); otherwise `ForceIndividualAction` on the creature's body. So the strokes and slaps act at frame time on the local machine; openblack calls them directly.
  - 0x59 0x63D352: the creature from the packet's index (null when gone); its mind's "player feedback" fn_004E06A0 with the float at packet +0x10 (`FinishActionUnsuccessfully("player feedback")`, then a plan when |v| > 0.01).
  - 0x5F 0x63D3B3: the sender interface's byte +0x370 = 0 (not named), then fn_005D06E0: the TOGGLE_LEASH path ([creature.md](creature.md), "TOGGLE_LEASH"). openblack: the leash key (`PressKey(Leash)`).
  - 0x63 0x63C98D: the player's creature loses 0.01 life, never below 0.01, then fn_004866F0 on its body.
- openblack: the interaction timer runs in the hand's camera ms (the hand's dt, `game_clock::CameraFrameMs`), from the first frame of hand state 19; outside a hand demo's playback that is the frame's real time, as the original's camera time step, so whether a let-go is a click (and its 0x5F, which works the leash at the next turn) depends on real frame time. **This is a documented exception** to openblack's rule of no wall-clock time in game logic, kept for fidelity with the original (coordinator, 2026-10-08); deterministic runs see a fixed camera step; the CREATURE state places the hand where the creature hand puts it; the clip stays NORMAL's rule over a creature (Cstroke).
- **(approximate)** the creature in the CREATURE state is the locked one, not the hover object read again each frame; the ready test is taken as "not ready when the hand leaves state 7, ready one frame after the release" (see [Pending](#pending)).

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

- Dragging a help bubble: +0x3AC = 0x1D while GInterface +0x3D0 is the bubble; the scroll follows the mouse y and the first frame only takes the point (0x5772E9..0x57734A).
- `HelpSystem::Process` recomputes the hand state every turn (fn_005D7E40 at 0x5C8FF0; also on the three exits of fn_005D1120) and sets +0x578 = 0 (0x5C8FF7), read by SendFOVObject 0x5C90B5 **(not ported)**.
- Action state 17 BUBBLE GRIP: an empty hand pressing on a screen object (ActionPressed 0x5D1364 → fn_005D4330) sets +0x38 |= 0x10; the release (0x5D9960, +0x39 |= 0x40) → state 17's process 0x5D53B0 clears the dword +0x38 and calls `ResetActionState` 0x5D29C0.
- Fields are a locked select (`ValidForLockedSelectProcess` 0x5299E0): ActionPressed fn_005D1330 starts the scooping at once (`StartTapOrLockedSelect` 0x5D1A00) inside the influence; outside it the field is neither placeable (Object 0x402870) nor tappable (Object 0x4196B0): nothing.
- `GInterface::StartLockedSelect` 0x5D1950: the object's `NetworkUnfriendlyStartLockedSelect` (vt 0x6D4; Object 0x4027D0 does nothing; the totems' not ported), packet 0x1B (0x5D1985) and action state 3 (fn_005D2980(3)) with the object as the action's (+0x400).
- State 3's end 0x5D4870: with an action object, its `GetReadyForNetworkUnfriendlyEndLockedSelect` / `NetworkUnfriendlyEndLockedSelect` (vt 0x6E0 / 0x6E8; nothing for piles, fields and fish farms), packet 0x1C (0x5D48A8), then `PSysGlobal::StopMultiPickup` at once (0x5D48B4).
- Packet 0x1B handler 0x5DA950: an interactable object (0x5DA965) not already in a locked select (Flags +0x24 & 0x10), then `NetworkFriendlyStartLockedSelect` (vt 0x6D0), the status's locked object (0x5DC110) and Flags |= 0x10; otherwise EndAction fn_005D1260.
- `NetworkFriendlyStartLockedSelect`: Field 0x529900, FishFarm 0x52D770, PileResource 0x66E710 (the first amount into a hand pot put in the hand through 0x5DC870, and `PSysGlobal::StartMultiPickup` 0x68F8C0); the hand pot is made at the status's MapCoords +0x14 (Pile 0x66E79F, Field 0x52998B, FishFarm 0x52D80F).
- `PileResource::NetworkFriendlyStartLockedSelect` 0x66E710: n = min(the hand pot's amountPickedUpInitially, GetResource) (0x66E735..0x66E75F; a store pile offers the store's total), RemoveResource(type, n, interface, &poisoned) (0x66E77E), Pot::Create (0x66E7A3), SetPoisoned(poisoned || its own) (0x66E7D5).
- RemoveResource's out flag: 0x66EE60 writes *out = IsPoisoned.
- Packet 0x1C handler 0x5DAA10: the object's `NetworkFriendlyEndLockedSelect` (vt 0x6EC; PileResource 0x66E850: StopMultiPickup and the amount tooltip), the status's locked object cleared (0x5DC110(0)) and Flags &= ~0x10; a NULL object only ends the action (0x5DAA2A).
- `GInterfaceStatus::Process` 0x5DC4E0, the locked select (+0x3C): the counter +0x40 goes up; it lasts while the object IsAvailable, has influence > 0 at MapCoords(+0xC8) (0x5DC540 → 0x5DC558) and its `ProcessInInteract` (vt 0x808: Pile 0x66E520, Field 0x529730, FishFarm 0x52D950) returns 1; otherwise +0x3C = +0x40 = 0 and EndAction.
- `Field::ValidForLockedSelectProcess` 0x5299E0: growth > 0 and food > 1.
- State 7 WAIT FOR PLACE IN HAND is `State_WaitPickup` 0x5D4A90; GenericPickup sends packet 0x13 at 0x5D2864 and sets state 7 at 0x5D286D (for a teleport stone, the 0x13 carries the stone itself; its seed is found in the handler).
- `GInterfaceStatus::SetToZero` 0x5DBA00 (0x5DBA12..0x5DBAB0) clears the synced and turn fields, fn_005D2250's last-sent values and countdown, the status's throw (+0x44..+0x70) and the action's object; `CHand::OnClearMap` 0x46E8C0 clears lastReleased and the countdown.
- GInterface +0x3AC starts at 3 (`SetToZero` 0x5CE4F0).

### Interface active (`GInterface+0x28`)

- `GInterface+0x28`: bit 0 = inactive; bits 1 and 2 = SET_INTERFACE_INTERACTION limits (jump table 0x70B7A8).
- `GInterface::IsActive` 0x5CE2E0 = !(+0x28 & 1).
- `GInterface::SetActive` 0x5CEDC0 writes bit 0 = (active == 0); its other effects are in Pending. `HelpSystem::ProcessInterface` clears +0x460C.
- Script wide screen: `HelpSystem::SetWideScreen` 0x5C6AD0 → `SetActive(!(on && owner))` (0x5C6AF4 / 0x5C6B01).
- While a script holds the widescreen, no events are processed [0x63EF90..0x63EFA8, 0x63F024..0x63F03E].
- `GGame::ProcessKey` (0x63F40C..0x63F446): the LH_KEY 2..15 block (KB_1..KB_TAB) is skipped while a script holds the widescreen (HelpSystem +0x45E8 && +0x45EC) or while a SET_INTERFACE_INTERACTION level has a ControlMap switch turned off (0x63F42A..0x63F446 requires both switches; see [Reach and the ControlMap switches](#reach-and-the-controlmap-switches)).

### A box takes the mouse (the escape menu, a dialog)

How the buttons reach the interface:
- `LHMouse::SetButtons` 0x7E4B80 (the window's thread) ORs each event into the held mask [0xE85304] and the latched
  events [0xE85350]: 1 / 2 / 0x40 the left / right / middle down, 4 / 8 / 0x80 their up. A down clears its button's
  pending up in both (jump table 0x7E4D88, index bytes 0x7E4DB0); an up clears the held bit only.
- Every frame, paused or not (`GGame::Loop` 0x54D3E9 → `ProcessFrameInputs` 0x54C340), `CMouse::ProcessButtons`
  0x61A150 (0x54C398, not while a hand demo plays) turns the latched events into interface messages (fn_005D9DD0)
  through the ControlMap's two button bindings (+0x30C and +0x618, g_game +0x250300): press fn_00470E80 /
  fn_00470B30 (messages 1 / 3), release fn_00471170 / fn_00470E20 (2 / 4), the double ones fn_00471130 /
  fn_00470DE0 (5 / 6), else a move (7). Then it clears the latched events (0x61A3FC..0x61A408). The interface pass
  (`ProcessFrameUpdates` 0x54C3B6) comes after it in the same frame.
- A binding's pressed flag (+0x6514 / +0x6510) is set by its press and cleared only by its release; nothing else
  writes it but the ControlMap's constructor. A mouse press (+0x651C / +0x6518 = 1, 0x470F3F) is released only by
  the latched up (fn_0046F660 bits 4 / 8 / 0x80), a key press by the key no longer down (`IsKeyBeingPressed`
  0x46F4C0). No press is sent while the flag is set (0x470F32 / 0x470BE2), and no release while it is clear. So a
  second down with no up between sends nothing, and an up without its press sends nothing.

While a box is up:
- The frame's interface pass (`ProcessFrameUpdates` 0x5CEDB0 = fn_005D9A20 → fn_005D1120 at 0x5D9B1C) calls
  `InterfaceActionProcessFirstGUIFeatures` 0x5D2A70. It clears [0xC4CCEE] (0x5D2AF1); with a box shown
  (`SetupBox::GetCurrentActiveBox`, 0x5D2AFA) that wants the mouse it sets [0xC4CCEE] = 1 (0x5D2B58), turns the
  cursor on and returns 0x14.
- Which box wants the mouse (0x5D2B03..0x5D2B25): [0xCC629C] is the `DialogBoxBase` last shown, the dialog whose
  SetupBox (+4) is the active one, found in the list `DialogBoxBase::First` [0xCC6298] (next at +0xC) by
  `DialogBoxBase::UpdateLastShown` 0x513680, which clears it first. When it wraps the active box, its vt +0x18
  `WantsMouseControl` decides; without one the box takes the mouse. `DialogBoxBase::WantsMouseControl` 0x512800 is
  `IsVisible` 0x513770 (its SetupBox is the active one): every dialog takes the mouse while it is shown (SkipBox,
  `DialogBoxOptions`, `MiniDialogBoxOptions`, `DialogBoxImmersion`, `DialogBoxSaveMessage`, `DialogBoxKeyBinding`,
  StatsBox, EndGameBox, StartGameBox, LoadingBox, ...) but two: `HistoryBox::WantsMouseControl` 0x545470 and
  `GatheringBox::WantsMouseControl` 0x573BF0 take it only over one of their controls (`SetupBox::FindControl`
  0x408100 at the cursor [0xE852C0] / [0xE852C4]) or while a button pressed with the box up ([0xC4CD0C], set at
  0x5D2B3A from [0xE85304] & 0x43) is held; the GatheringBox not while a script holds the wide screen, and it keeps
  the mouse once it took it ([0xD06440]). The temple's `TempleRoom::UpdateMouse` 0x79A429 makes the same test.
- On 0x14, fn_005D1120 (0x5D116C..0x5D1183) runs EndAction fn_005D1260, sets the action state to 1 (fn_005D2980) and
  clears the dword +0x38 (the grab and action button bits), then the power-up system and the hand state, and returns:
  the action's processing does not run. It does so on every frame the box is up.
- EndAction first calls the action state's end: the entry +0x10 of the table 0xD17DA8 (0x30 bytes a state, filled by
  the static initializer 0x5D0890; +0x00 is the state's process):
  - states 3, 4, 5, 6 (the locked selects: scooping, fish, the creature): 0x5D4870, the object's
    `GetReadyForNetworkUnfriendlyEndLockedSelect` (vt 0x6E0) and `NetworkUnfriendlyEndLockedSelect` (vt 0x6E8), the end
    packet 0x1C, then `PSysGlobal::StopMultiPickup`;
  - 8, 9: 0x5D4940 (clears +0x38, fn_005D27B0); 10, 11: 0x5D4CB0, the packet 0x1A (ApplyUnlock) with the held object and
    the turns since +0x3C; 15: 0x5D49A0 (+0x38 &= ~0x20, ResetActionState);
  - 12 (the release state): 0x5D4D60, with CHand in HOLDING (+0x4878 == 4) its +0x488C's +0x104 = 0;
  - 13 (the grab): 0x5D1840, `StopImmersion(4)` only: no tap, no pick-up;
  - 22, 23: 0x5D4650, the packet 0x24 with the time since the press (+0x454 / +0x458, capped as the grab's),
    fn_0048A570, ResetActionState, +0x38 = 0;
  - 0, 1, 2, 7, 14, 16..21: none.
  Then the objects under the hand are cleared (+0x400, +0x3C8) and `ResetActionState` 0x5D29C0 runs.
- With [0xC4CCEE] set, `ProcessButtons` (0x61A298) sends no button message: the latched downs and ups are dropped at
  its end (only the double clicks set [0xC4CD10] / [0xC4CD18]). As it runs before the interface pass, it also drops
  the events of the frame the box goes away in. A release the box takes is lost; the binding stays pressed, so after
  the box the first press of that button sends no press, and its release sends the release.
- `FrontEnd::JustDoABox` 0x544F90, the front end's modal box loop (the new profile and skirmish boxes), keeps the
  latched ups when it ends: [0xE85350] &= 0xBC (0x545219..0x545223) drops only the downs. The game's boxes are not
  modal: they are made active with `SetupBox::SetCurrentActiveBox` and drawn in the frame (`GGame::ProcessKey` 0x63EFC6,
  fn_0054CB50, fn_0054CB90).

In openblack (`src/Input/MouseButtons.cpp`; Game.cpp; `HandSystem::BoxTakesInput`, HandTurn.cpp):
- `input::ApplyMouseButton` is the bindings: a press only when the button's binding is not pressed, a release only when
  it is; the press holds the interface's button, the release lets it go.
- `input::BoxTakesInput`, every frame before the hand's placement and update, is the interface's pass: a box takes the
  input while the escape menu or the SkipBox is up, or a debug window took a click since the last pass or still holds
  one (the debug windows follow the dialogs' rule; the original has none). Then the interface's buttons are cleared,
  and `HandSystem::BoxTakesInput` runs the action state's end as above (a locked select's end packet and the stopped
  multi pickup; a grab, a tug ((inferred) part of the grab state 13) and a release state end with nothing sent), then EndAction and the action state reset;
  the hand sees no release of what the box cut short. The button events that reach the game after a pass where a box
  took the input, until the next pass, send nothing.
- (openblack's own guard) while a demo the OPENBLACK_TEST_HAND_DEMO test hook started plays
  (`hand_demo::MarkStartedByTestHook`, `hand_demo::BoxRuleReachesHand`), the pass leaves the hand alone: the hook plays
  the demo from the land's start, under the SkipBox, a start the original does not have. A demo a script plays
  (PLAY_HAND_DEMO, the tutorial) gets the rule. Nothing changes while no button is held and the hand does nothing: the
  test runs, which ignore the real mouse, are the same.

### Grab press (`StartGrab` 0x5D1740, `State_Grab` 0x5D5250)

- StartGrab runs on the press's own frame (ProcessFrameInputs 0x54C3B6 → fn_005D9A20 → fn_005D1120): +0x454 = the frame's engine sample [0xEA9EB0], +0x458 = the turn (0x5D174D..0x5D1769), action state 13.
- The object is free when +0x24 & 0x40 IN_PHYSICS && !(+0xA & 0x10) (0x5D529F..0x5D52B2; +0xA & 0x10 = carried by a particle system): the 225 ms timer path, as for a forest (+0x450, 0x5D17F4 / 0x5D17FE).
- Otherwise fn_0046DC30 (StartGrab 0x5D1813, `CHand::PickUp(tree, needsTug 1)`) holds it with CHand +0x4908 = 1 (0x46DCF3..0x46DD10); 0 for a seed (needsTug forced 0, 0x46DC76).
- A standing tree is tugged; one in physics (thrown, not landed) is caught like any flying object unless a particle system carries it (0x46DCF9 / 0x5D52A5).
- State_Grab, every frame and every turn: elapsed = min(sample − +0x454, (turn + 1)·100 − +0x458·100) (0x5D5267..0x5D5293).
- First the pick-up: the timer path at elapsed >= 0xE1 (0x5D52E4), else once CHand +0x4908 is 0 (fn_0046E480, 0x5D533D); GenericPickup sends the forest's 0x13 at 0x5D5349.
- Then the release (m_Buttons 0x4000, 0x5D52EC..0x5D5329, every object): StopImmersion(4), CHand::ThrowObject when it holds it (0x5D530B), and a Tap only if elapsed <= 0xE1 (0x5D5310).
- HandStateTug::Update 0x5B8070 for the +0x4908 hold: the first draw after the press zeroes +0x49B0 (fn_0046C500, 0x46C768) with firstFrame set; the gate 0x5B8096 clears firstFrame once +0x49B0 >= 0.13 [0x900C08], and the Update after clears +0x4908 (0x5B8409).
- **(not ported)**: a tuggable object without a Game3DObject (+0x40 == 0, 0x46DC58), the MagicFireBall's 225 ms (0x682A40), and the tug blend not advancing while the hand is hidden.

## Holding, spring and throwing (`HandStateHolding::Update` 0x5B3C70)

- Grip types (jump table 0x5B568C), base height h: ABOVE 0.2; MAGIC 3.2·scale; GRAIN/TREE/SIDE/VILLAGER
  max(lowering, 1.9); +0.1·height if the object is rooted. `lowering = GetHeight·GetHoldLoweringMultiplier`.
- Poses: ABOVE = Chold_above at dur·0.5·(1−grip), grip = min(1, R/(3.2·s·1.2)); SIDE/TREE/VILLAGER = Chold_side at
  (dur>>1)·grip, grip = min(1, R/(3.2·s)). R = GetHoldRadius (constant for wood; food opens up with the amount).
- **Spring (inertia)**: 10 ms steps, a = 260·d − 40·v, |v| ≤ 124. **Only active in the IN THROW state**
  (check `0x3AC == 0x17` at 0x5B4603). After grabbing and releasing the button, the hand follows the cursor without inertia.
- **What the spring drags is the hand** (CHand +0x78, 0x5B457E..0x5B45C5), not the held object: the object's 3D matrix is
  then put at the hand plus its hang (`g3d.pos = hand.pos + offset`, the hold-type cases from 0x5B4EA5), so the object
  always hangs from the hand wherever the spring has taken it.
- **When it starts and stops** (0x5B45DE..0x5B4655): with the spring off the hand is put at the required position; then,
  in IN THROW and not holding a seed, the flag +0x104 is set, the hand put at the required position again and the
  velocity zeroed, with no step that frame: it steps from the next frame. Nothing in the update clears +0x104 when the
  interface leaves IN THROW: the spring stays on through the release until the object leaves the hand (only the state's
  Enter and a refused release, below, clear it). +0x48C8 = the spring's velocity and h = 0 every frame the flag is set,
  the starting frame included (0x5B46B4..0x5B46E8).
- **Where a throw leaves from** (state 12 0x5D4E88..0x5D4EAD): the prediction body is `PhysOb::Initialise` on the held
  object's 3D object (obj +0x40, 0x5D4E93) and `SetUpPhysOb`, whose `SetUpPos` (0x7FC76B..0x7FC779) copies that 3D
  object's matrix (+0x14, 12 dwords): the matrix the Holding state drew it with this frame. The turn place (MapCoords,
  below) is not read: a throw leaves from where the object hangs from the hand. openblack (issue #134, 2026-10-09):
  `HandSystem::PushThrowData` passes the held object's `HandDrawPose` (`magic::hand_hold::ReleasePlace`) to
  `physics::from_hand::PredictRelease`, which builds the body there and puts the turn's place back after; before, it
  was built at the turn's place,
  the synced hand point, which trails a moving hand by one or two turns (and does not hang below it), so the throw left
  from behind the hand, further back the faster it moved. The spring now also starts and stays on as above
  (`magic::hand_hold::StepHoldingSpring`).
- The Holding update also writes +0x48E0 = the drawn position + 0.2 × the spring's velocity and +0x48EC the drawn
  matrix's YXZ angles (0x5B5553..0x5B55BD); the block +0x48C8..+0x48F7 is what the recorder saves and what
  SendApplyToMapCoord's block path sends as it is (0x5D3509..0x5D35BA).
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
- fn_0046DC30 (0x46DC3E): +0x4904 = the object; its hold type / lowering (+0x490C / +0x4944) come from it; only an object with a G3D (obj +0x40, 0x46DC9A) gets the need-sorting flag.
- fn_0046DE40 swaps objects without ThrowObject: it neither writes +0x48FC (lastReleased) nor calls GetWorldMatrix (0x46DE57); the new object's flags are set at 0x46DEA1.
- The held object is drawn by CHand's state (Tug `DrawTheHeldObject` 0x5B8E40, Holding 0x5B56D0: held->DrawOutOfMap(0), vt 0x614) where the state put its G3D, also while the 0x13 still waits and the object is still in the map; its logic (Transform, map cells) stays where it is.
- The held object's matrix fn_0046E2F0 (0x5B4B9D): rows X = norm(d × up'), Y = up', Z = X × up', the third row negated when CHand +0x484C == 0 (0x46E3B7..0x46E453); d = (−sin H, 0, cos H).
- Then `GetHoldYRotate` (vt 0x598; Object 0x4025A0 = 0, so no turn) on rows 0 and 2 (0x5B4BB3..).
- The sway up' (ObtainRequiredHandPosition 0x5B4860..0x5B4AE5, HOLDING and GRAIN): tiltX = clamp(+0x48B8 − +0x485C, −80, 80)·(0.3 / 80) (0x5B486C..0x5B48BE); D = normalize(camera − hand) (GetCameraPosition 0x5B480E minus CHand +0x78, 0x5B48C4..0x5B4994; 0 when the two meet).
- R1 = fn_007FB180(D, extraRoll + roll + tiltX) (0x5B49A0..0x5B49C8): extraRoll is the state's vt 0x14 (Grain's tilt, 0 for Holding), roll the Zoomer +0xD4 (0 unless a creature takes the object, 0x5B4251..0x5B42DA, not ported).
- tiltY = clamp(+0x4860 − +0x48BC, −80, 80)·(0.3 / 80) (0x5B49D0..0x5B4A2A); R2 = fn_007FB180(normalize(−D.z, 0, D.x), tiltY) (0x5B4A30..0x5B4AD5), the axis left as it is when zero.
- M = R1·R2 (fn_007FAFF0, 0x5B4AE5) and up' = (0, 1, 0)·M (0x5B4AEA..0x5B4B53); with D all zero (0x5B4902..0x5B4936) up' = (0, cos a·cos b, 0), not unit (fn_007FB180 has no zero guard).
- 0x5B499B is the frame's last `SetDistanceFromView`.
- The angular velocity h (CHand +0x48D4), read by state 12 (0x5D4EB3), SendApplyToObject 0x5D30D0 and 0x5D3340: 0 at construction (0x46BACF), 0 every frame while the Holding spring works (0x5B46DA), and written by HandStateGrain::Update in state 8 (0x5B36BF).
- `HandStateHolding::Update` 0x5B46B4..0x5B46E8: h = 0 only while the spring works; otherwise h keeps its last value (0x5B46B4 `je 0x5B46EA`).
- `HandStateGrain::Enter` 0x5B3080..0x5B3172: the displacement, the smoothed velocity and the smoothed sideways acceleration all 0.
- `HandStateGrain::Update` 0x5B36BF..0x5B36D9 (state 8), each frame with CHand's dt: v_s smooths the hand's per-frame velocity with k = −10·ln(0.2), a_s the sideways displacement over dt² with k = −10·ln(0.8); +0x48C8 = v_s (no 124 cap), +0x48D4 = (0, −(a_s / |v_s|), 0) when |v_s| > 0.0001.
- The old and new hand positions of that step are taken at 0x5B3317 and 0x5B33A0; the exps are the x87 sequence at 24 bits (0x5B3452..0x5B348B, 0x5B3561..0x5B3583); k is rounded to float once (0x5B30F1 / 0x5B310C).
- **(not ported)**: the acos branch behind [0xD13F1C] (never written: off).
- `GetCameraTimeInc` 0x555820 has a playback branch: g_game_time_inc while a hand demo plays (interface +0x45E8 / +0x45EC).
- `Object::ThrowObjectFromHand(status, dont_replant)` 0x6385E0 builds the matrix from the status's HandAngles (+0x68) and HandPos (+0x5C), the object's scale kept, and calls `HelpProfile::Trigger(4 Throw)` for the local player (0x638622).
- It runs with the object still in the hand (no RemoveFromHand) and returns 0x16 (0x63871A); the caller's HandleApplyResult fn_005DA100 takes it out (RemoveFirstFromHand fn_005CED60: `GMagicHand::RemoveFromHand` 0x5FB0B0, `FireEffect::SetOutMagicHand`, CHand::ThrowObject 0x46DDD0).
- `ApplyThisToMapCoord` (vt 0x728) results: Mobile 0x606BF0, MobileStatic 0x608B30, SingleMapFixed 0x52F420 return 0x16 after ThrowObjectFromHand; a tree / dead tree on a building site returns 3 (`Tree` 0x74BFD0, DeadTree 0x511050, 0x511094; `MapCoords::IsWithinBuilingSite` 0x605250: the first fixed object of the cell that is a MultiMapFixed not built and repaired, whose collide data meets a 0.3 m sphere at the point). A working storage pit is built, so a release never gives it anything: a store takes a held object on the press (below) or when the thrown body hits it. openblack: the release no longer gives a tree to a pit within 8 m of the point (that disc was not the original's); (pending) the building-site branch.
- `ValidToApplyThisToMapCoord` (vt 0x724) is 1 for every holdable class but the seed (Mobile 0x416F50, MobileStatic 0x4396C0, SingleMapFixed 0x52EB00).
- `Pot::ApplyThisToMapCoord` (0x66DED8): a pot put down offers its food reaction again, once (the hungry grazers come to eat).
- `Pot::ApplyThisToObject` 0x66DDD0 on the land: `Pot::AddResourceToPos` at the pot's position with its amount, IsPoisoned and no speed-up; it merges into the stores and same-resource pots of the 3×3 cells around (each within 2 × its 2D radius, 1.2 for a store), else makes a MagicWood / MagicFood pile.
- `AddResourceToPos` 0x66F27A: off the map (MapCoords::InBounds) nothing is added anywhere: the resource is lost (and what is left over on water is dropped, 0x66F42D).
- `Tree::ReactToPhysicsImpact` 0x74B6B0: a thrown tree hitting a store calls h->DeleteObjectAndTakeResource(this, po +0x24) with the PhysicsObject still alive (0x74B6F9), so the store's Supply help trigger (0x7337A6) reads it; the thrower's interface is the giver.
- `GMagicHand::RemoveFromHand` 0x5FB0B0: +0x24 &= ~4, `InterfaceSetOutMagicHand` (vt 0x704), `FireEffect::SetOutMagicHand`.

### The release (state 12 0x5D4DB0)

- The release tests: the action's MapCoords (+0x3F0) InBounds (0x5D4DF1), an object in the hand, its vt 0x724 (0x5D4E16) and the influence (+0x48, 0x5D4E24): then the packet 0x4D with the throw (0x5D513D); nothing without a held object (0x5D4E5F).
- Both paths go on to DropOnMapCoord 0x5D1850 (an object in the hand, vt 0x724, not ValidForLockedApplyProcess: ResetActionState) → `SendApplyToMapCoord` 0x5D3340.
- SendApplyToMapCoord does nothing while an apply packet waits (+0x444, 0x5D337C); the same three tests (0x5D3415..0x5D3445) give packet 0x12 (0x5D362D) and fn_005D36A0, or `FailApply` fn_005D18F0 (0x5D365B): the fail spot and sound, the object stays in the hand.
- The 0x4D's 0x30 bytes: the velocity (+0x44), the angular momentum (+0x50), the position (+0x5C) and the GetYXZ angles (+0x68); GetYXZ at 0x5D512B stores y → +0x28, x → +0x24, z → +0x2C.
- The prediction's turn count min([0xD44454] / 100, 5) uses an unsigned division and a signed min (0x5D5061..0x5D507B).
- ForceDropHeld 0x5D4350 sends the 0x4D with zero velocity and L, the held object's matrix translation and GetYXZ (0x5D4393..0x5D43C3).
- Release impulse (GetRequiredState): with nothing held, +0x48F8 > 0 goes down by g_game +0x205D48 (the frame's game ms, 0x46CE15); the frame it turns negative (`jns`: exactly 0 stops with nothing sent) and lastReleased is set, +0x48F8 = 0 and the packet 0x40 is sent (fn_00550F10 0x46CE88) with g_C5E640 − g_C5E650 (0x46CE3D..0x46CE76) and HandStateHolding +0x108; then lastReleased = 0 (0x46CE8D).
- Inside the citadel GetRequiredState does nothing of this (0x46CD1C).

### The locked apply's unlock (states 10 and 11, packet 0x1A)

A seed cast while it is held in the hand (`ValidForLockedApplyProcess`, the IN_HAND seeds: lightning, water, food,
wood) is a locked apply: the press enters state 10 (the land) or 11 (an object) and sends the apply packet 0x12 / 0x11,
and state 10's process 0x5D4C10 (state 11's 0x5D4D00) sends it again every frame while the button is held (one waits
at a time, +0x444).

- The release (0x5D4C2A..0x5D4C65; 0x5D4D03..0x5D4D4A for state 11) does not unlock the seed itself: it sends packet
  0x1A (ApplyUnlock, `fn_00550BE0`) with the held object and the turns held (turn − status +0x3C, the lock's turn),
  then `+0x38 = 0` and `ResetActionState` 0x5D29C0.
- The 0x1A handler 0x5DA8A0 (two arguments: the packet's object and value): status +0x9C = the value, the turns held,
  on every path; then the packet's object (not the status's held object), if not null and `IsInteractable` (vt
  0x190) → its `ApplyUnlockProcess(status)` (vt 0x730, the result ignored; `Object` 0x402900 returns 1 and does
  nothing; the seed's 0x728EB0: a seed deleted once cast → `ToBeDeleted`, 3; else `StoreChantsAndAgeFromSpell`
  0x728780 → `ClearSpellLink` 0x728200, the spell closes and the seed keeps the chants and age, 1, or `ToBeDeleted`
  and 3 when that gives 0); else `EndAction` fn_005D1260.
- `EndAction` first runs the state's exit (table 0xD17DA8, stride 0x30, slot +0x10, 0x5D126D..0x5D1281); states 10
  and 11's exit 0x5D4CB0 also sends 0x1A, with an object in the hand whose byte +0xA bit 0 is clear.
- Packets are handled at the next turn's start in the order sent, so the unlock always follows the last apply: a click
  casts its spell, which strikes for about the click's length and closes the turn after the release.

openblack: `HandSystem::UpdateSeedAction` sends `hand_seed::UnlockPacket` (`ECS/HandSeedUnlock.h`) on the release,
`EndAction` sends it when it leaves a locked apply with a seed in the hand, and the 0x1A handler is in `HandTurn.cpp`
(the turns held kept in `_unlockTurnsHeld`, the status +0x9C).
Until 2026-10-09 the release called `ApplyUnlockProcess` at once, while the applies were already deferred packets: a
click unlocked before its own apply was handled, and the spell that apply cast was never closed, so a lightning bolt
struck every turn until its age ran out.

### Applying the held object to an object (`ActionPressedHolding` 0x5D1560)

- `ValidAsInterfaceTarget` vt 0x6F0 (Object 0x402840 = 1); `InterfaceValidToGiveObject` vt 0x748 needs a creature.
- 0x5D1607: held->ValidToApplyThisToObject(status, target) == 1; else 0x5D168E (ApplyOnlyAfterReleased / FailApply for a non-seed).
- `ValidToApplyThisToObject` vt 0x71C: Object 0x4028B0 = 0; SpellSeed 0x7286D0; `Villager::ValidToApplyThisToObject` 0x752BD0: a WorshipTotem (RTDynamicCast 0x752BE7) → 1, a MagicTeleport (0x752C0C) → fn_005FC4B0 == 1, else 0.
- `ApplyOnlyAfterRecSystem` vt 0x738 (Object 0x402920) = 0 and `ValidForLockedApplyProcess` vt 0x72C (Object 0x4028F0) = 0, so a villager goes to SendApplyToObject (0x5D1684).
- With an apply packet still waiting (+0x444, 0x5D3135) no 0x11 is sent, only the action state (0x5D32B5); else fn_005D36A0 after it (0x5D32B0).
- The 0x11 handler 0x5DA1A0 calls `HelpProfile::Trigger(7)` for a sacrifice altar (IsSacrificeAltar vt 0x4B0), else 6, for the local interface (0x5DA268..0x5DA294).
- **Giving a held object.** The target is the creature to give to, else `m_ActionCollide.object`: the mouse-ray pick of each drawn object (`SendObjectDrawCollision` 0x5D56C0, `CheckTriangleCollide`, the nearest wins; `UpdateInterfaceCollide` 0x5D5A70 keeps an object behind the land hit only when the land point is inside its XZ bounding box). A pit's piles are objects of their own: a tree over a pit's wood pile is valid (the pile's `IsResourceStore(WOOD)` asks the pit), over its food pile it is not. Per class (ValidToApplyThisToObject vt 0x71C / ApplyThisToObject vt 0x720):
  - Tree 0x74BD50 / 0x74BDA0 and DeadTree / FelledTree 0x510E90 / 0x510EE0: valid when `target->IsResourceStore(WOOD)`; apply: `DeleteObjectAndTakeResource` → 3 (the worship totem branch is the sacrifice, [trees.md](trees.md)).
  - openblack: `ecs::held_apply` (`HeldClassApply`, `ValidToApplyThisToObject`, `ApplyThisToObject`) on `ecs::resource_stores`; `HandSystem::ApplyHeldToObject` takes the object out of the hand as a release does, drops an uprooted tree's roots (openblack's own), then the store takes it. The cursor object is `_cursorObject` (`HandPlacement.cpp`, the same pick).
## Grabbing

- 225 ms threshold between touching and grabbing. Piles cannot be touched: the press starts taking in batches
  immediately.
- Rocks with a 2D radius > 3.6 cannot be lifted (`Rock::ValidForPlaceInHand`). Pressing on them strikes them and splits them in two; see [physics.md](physics.md).
- The hand never calls `CanBePickedUp`; the gate is `GInterface::PlaceObjectInMagicHand` (0x5DA6F0).
- **Influence.** Every ported class keeps `Object::InterfaceMustBeInInfluenceForInteraction` (0x4028A0 = 1) but
  ScriptHighlight, whose 0x709840 is `xor eax, eax; ret` (0): a «Did you know?» sign or a scroll is tapped anywhere.
  openblack: `hand_tap::Register`'s `needsInfluence` (false for ScriptHighlight) and `hand_tap::SendsTap`. Otherwise
  nothing is picked up, scooped or tapped with the action
  position outside the player's influence (`GInterface +0x48`, fn_005D1120: `CalculatePlayerInfluence(pos, type 1,
  allies) > 0`). The checks: `ActionPressed` fn_005D1330 before a locked select (piles, fields, fish farms);
  `StartGrab` 0x5D1740 turns an object out of the influence (or not placeable) into a tap; `GenericPickup` 0x5D2800
  checks again when the 225 ms grab completes; `SendTap` 0x5D38A0 refuses the tap. While scooping,
  `GInterfaceStatus::Process` 0x5DC558 ends the locked select when the hand leaves the influence.
- `IsCannotBePickedUp` (0x401A10, flag 0x2000) is checked by all of these too. The scripts set it with
  `SET_ID_PICKUPABLE` 169 (0x6FB450, pickupable 0 sets it) and the flag 0x1000 IMMOVABLE with `SET_ID_MOVEABLE` 168
  (0x6FB3E0); in openblack both are tag components (`src/ECS/ObjectFlags.h`, `ecs::object_flags`; were `ThingFlags.h`, `thing_flags`). **(not ported)**: the
  other setters, `GameOSFile::LoadInstance` 0x559999 and the puzzles (fn_006D71D0, HanoiBlock).
- PickUp's help event (0x5DA7F4..0x5DA808) tests +0x24 & 0x40 IN_PHYSICS and altitude >= 0 after fn_005DC330's RemoveObject has cleared IN_PHYSICS (0x646B56), so the event is always PickUp 2 (0x5DA82C), never Catch 3 (0x5DA817).
- fn_005DC330, the object entering the hand: out of the map cells (IsObjectInMap vt +0x178 at 0x5DC377, RemoveMapObject vt +0x548 at 0x5DC385); the object is stored at +0x120 (0x5DC3EC); then MyInterfaceStatus's gesture buffer is cleared (0x5DC40C..0x5DC434).
- (inferred) A failed put-in-hand takes nothing out of the map: PlaceObjectInMagicHand's failure path (fn_005DC330 != 1 at 0x5DA7C9) calls InsertMapObject (0x5DA849).
- `GInterface::PlaceObjectInMagicHand` 0x5DA6F0 checks, in order: IsInteractable (0x5DA705), IsSpaceInHands (0x5DA719), ValidForPlaceInHand (vt 0x6FC, 0x5DA73B), IsCannotBePickedUp (0x5DA74E), and not carried by a particle system (GameThing +0xA & 0x10, 0x5DA766).
- Then InterfaceSetInMagicHand (vt 0x700, 0x5DA77C); a class that returns 0 has put another object in the hand through `GInterfaceStatus::PlaceObjectInMagicHand` 0x5DC870 → 0x5DA6F0 again (BigForest 0x439421, MagicTeleport 0x5FC489), and the outer call only ends the action (0x5DA859); a refusal or a NULL object: EndAction (0x5DA88F).
- `GameThingWithPos::IsInteractable` vt 0x190 (0x5701B0) = IsAvailable (GameThing 0x401810: !(+0xA & 1)); `Abode::IsInteractable` 0x407200 also needs GetPercentBuilt != 0.
- `GInterfaceStatus::ValidateHands` 0x5DC610 → `GMagicHand::Validate` 0x5FB130: a held object no longer IsInteractable leaves the hand with no physics (RemoveFromHand 0x5FB0B0), then `TidyHands` 0x5DC6F0 and, for the local interface, CHand::ThrowObject (0x5DC68C).

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
| CitadelEntrance | 0x468F50 (multiplayer, or GScript +0xA0 != 0) | 0x468EF0 (GoInsideCitadel) | citadel, through `worship::citadel::` |
| ScriptHighlight | 0x70ADD0 (a did-you-know with a script id; a scroll active with one) | 0x70AC70 (the did-you-know bubble) | hand, through `ecs::script_highlight::` |
| LeashObj | 0x464450 (the local player's own post) | 0x464490 (pick or unpick the post's leash, sound 42, packet 0x65) | hand, through `worship::temple_leash::` |

**(not ported)**: PuzzleTotem 0x6DA610, Scaffold 0x6E9DD0, Reward 0x6E5D00, MagicFireBall 0x682E50.

LeashObj (the temple's leash posts, read): valid to tap 0x464450 only for the local player's own posts; the tap picks
the post's leash (sound 42, packet 0x65) or clears the pick on the picked one; the hand reaches a post through
`SendInvisibleDrawCollision` 0x519960, radius 1.0, each frame it is drawn ([creature.md](creature.md#the-temples-leash-posts)).
A post keeps Object's `ValidForPlaceInHand` (0) and influence test (1): a press on it is a tap, in the influence.
openblack: the stones' and the posts' spheres are one test, `ecs::hand_pick::InvisibleSphereAlong`
(`HandSystem::PickObjectAlongRay`), and the press's tap-only objects take a post valid to tap.

- **A press on a sign.** `GInterface::ActionPressed` sends the collided object to `StartGrab` when it is valid to tap
  (vt 0x740), and `StartGrab` taps at once an object that is not valid to place in the hand. ScriptHighlight's
  `ValidForPlaceInHand` 0x709770 is `xor eax, eax; ret 4` (0) and its influence test 0x709840 is 0, so a press on a
  «Did you know?» sign is a tap wherever the hand is: the first opens the bubble with the sign's text, a second on the
  same sign closes it (`SetBubbleProperties`, [intro.md](intro.md)). openblack: the press on a tap-only object under
  the cursor (`HandSystem::Update`, with abodes and the temple's entrance) also takes a sign valid to tap; until
  2026-10-08 it took only abodes and entrances, and since a sign is never hovered for a pick-up, no press reached its
  tap (only the OPENBLACK_TEST_TAP_SCROLL hook did).

- `Object::InterfaceTap` (vt 0x744) default 0x4196C0 returns 1.
- The 0x20 handler 0x5DA650 checks IsInteractable (0x5DA664) and InterfaceValidToTap again, calls InterfaceTap at the status's synced hand +0xC8 (Abode 0x4068C6, Rock 0x6E749C, Scaffold 0x6E9DF7, OneOffSpellSeed 0x72A689) and `HelpProfile::Trigger(8 Tap)` (0x5DA6A0).
- **(not ported)**: a rock's `ConsiderMakingCreatureMimicPlayer` (0x5DA6C8).
- `ScriptHighlight::InterfaceValidToTap` 0x70ADD0 (asked at 0x5D38DB); its InterfaceTap 0x70AC70 is a «Did you know?» scroll; the tapping status is the local one when it is MyInterfaceStatus (0x70ACFE / 0x70ACA7).
- `HelpProfile::Trigger(1 Hand Move)` 0x5CECC0 comes from MOUSE_MOVE (0x5D9970) with a pixel delta (+0x420 / +0x424).

## Hand demos

The tutorial's hand demos (`Data\HandDemo\*.hnd`) replay recorded interface input through the real hand, and set the
camera from each record. openblack: `src/Input/HandDemo.{h,cpp}` (`hand_demo::Play / IsPlaying / ConsumeTrigger / End /
EndIfTask / Update`); the script opcodes call them.

- `PLAY_HAND_DEMO(string, waitTrigger, keepHand)` 0x6FDAD0: `.\Data\HandDemo\%s.hnd`, where the name is the CHL string
  itself (challenge.chl offset 279 = "drag"). Then `GInterface::StartPlayBack` 0x5DAD60:
  - without keepHand, it drops the held object (fn_005D4350) and cancels the spells being charged;
  - it sets the game speed to 1, turns the widescreen on (`HelpSystem::SetWideScreen(1, 0)`) and snaps it (fn_005C6C40);
  - it processes the first record at once.
  - The path is built with sprintf on [0xC0DA18] and opened with `LHOSFile::Open` mode 2.
  - Without keepHand: fn_005D4350 is ForceDropHeld's packet 0x4D; spells are cancelled with `GPlayer::CancelAllSpellsCharging` 0x64BC60; then, with the leash on (+0x14) and not tied (+0x24 null), `GLeashStatus::SetOn(player->creature, 0)` (0x5DADCD): the leash held in the hand comes off (openblack: `creature_loop::ReleaseLeashHeldInHand`).
  - waitTrigger is stored at 0x6FDB59 / 0x6FDB6D, whatever StartPlayBack does.
- `IS_PLAYING_HAND_DEMO` 0x6FDB80 pushes **not** `IsPlayBack(0)`. `HAND_DEMO_TRIGGER` 0x6FE280 reads and clears the
  pending trigger. `SET_HAND_DEMO_KEYS` 0x709540 is an empty `ret`.
- `GInterface::IsPlayBack` 0x5DB710: +0x15C playing; with task ≠ 0 it also compares +0x160.
- When a task stops, the callback 0x6EC72A..0x6EC748 calls EndPlayBack if the task owns the demo.
- **File:** no header. Records are 124 bytes:
  - +0 the message: 0 mouse move, 1/2 grab down/up, 3/4 action down/up; it is an index into the message table 0xD186B8 (0 MOUSE_MOVE, 1 GRAB_DOWN, 2 GRAB_UP, 3 ACTION_DOWN, 4 ACTION_UP);
  - +0x04..+0x33: the CHand throw block +0x48C8..+0x48F7 at recording time (velocity, angular velocity, HandPos, angles). Recorder fn_005DB4D0;
  - +0x34 the mouse (normalised to 0..1 with fn_0081E8E0);
  - +0x3C the camera eye and +0x48 its focus;
  - +0x5C the trigger: the byte [0xE853AD] if it changed since the previous record, else 0 (**(inferred)** the space key);
  - +0x60 the game ms (g_game +0x25053C).
  - Count of records read: [0xD18854] / 0x7C.
- **Playback** (fn_005DAEE0, from the interface's message pump fn_005D9A20, which runs both every frame (ProcessFrameUpdates
  0x5CEDB0) and every turn (GInterface::Process 0x5CEC10); openblack: every frame only, (approximate)):
  - every record that is due is applied, in order, at the visual clock;
  - while waiting for the trigger with one pending, the time stands still;
  - message 0 moves the mouse; the camera goes through `GCamera::SetPositionAndFocus` 0x4438C0 and
    `LH3DTech::UpdateCamera` 0x819920;
  - the end of the file ends the demo.
  - The real buttons (0x54C390), the real mouse and the camera keys are blocked meanwhile.
  - It runs inside the pump before the hand ray (`GInterface::Process` 0x5CEC1F → fn_005D9A20 0x5D9AB3).
  - Each read is 0x7C bytes; a short read calls EndPlayBack (0x5DB33D). Each read sets the speed to 1 (`GGame::SetSpeed(1.0)` 0x5DAFC4).
  - Due time (0x5DB00A): the record is not due while (time − first time) > (now − start), unsigned; if it is not due, it steps back one record.
  - Trigger wait (0x5DB034 / 0x5DB3A4): the start is rebased, +0x168 = +0x164 − time + now.
  - MOUSE_MOVE (0x5DB054): fn_0081E920 and `LHMouse::SetPosition` 0x5DB081. Any other message first copies the recorded throw block into the render hand (`GPlayer::GetRenderHand`, CHand +0x48C8, rep movsd 0x5DB0A5..0x5DB0B6): a release throws with the recorded v and h.
  - Camera: `GCamera::SetPositionAndFocus` at 0x5DB10D and `LH3DTech::UpdateCamera` at 0x5DB120; the camera mode takes it from the records (`CameraModeNew3` [0x45D276, 0x46057A]).
  - A trigger in the record becomes the script's pending trigger (+0x88 = 1, 0x5DB125).
  - The message goes through the dispatcher fn_005D9BC0 (button bits from table 0xD186B8, 0x5DB2EE).
  - Tricon flags (0x5DB0BC..0x5DB0EA): [0xC5B0F4] (0 in the first and the last 20 records) and [0xC5B0AC].
- **EndPlayBack** (0x5DB3F0): only acts if there is a demo: closes the file, +0x15C = +0x160 = 0, `SetTurnOffMouseMove(0)`, script +0x8C = 0, and `SetActive(0)` if HelpSystem +0x45EC is set (a script still has the bars). The bars and the pending trigger (+0x88) stay.
- **(approximate)**: openblack's hand reads the button state once a frame, so a press and a release in the same frame
  would be lost.
- **(not ported)**: the camera tricon flags (+0x54 / +0x58).
- **Test hook:** `OPENBLACK_TEST_HAND_DEMO=<name>` plays `Data\HandDemo\<name>.hnd` once the landscape exists.

## Tooltips

The original shows a single tooltip next to the hand, and the hand's state picks it every game turn. In openblack:
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

- fn_005D7E40: an inactive interface (IsActive vt 0x40C) gives 25 at 0x5D7E54; inside the citadel (g_game +0x205A28 == 1) 30 (0x5D7E73); otherwise the row of the action state.
- Row 17 (BUBBLE GRIP) of table 0xD18278 is [0xD184A8] = 0x1D Drag Bubble (0x5D7CEF).
- LANDSCAPE LOCK (StartLandscapeGrip 0x5D1F81): 0x5D8090 is `(+0x38 & 1 && +0x39 & 1) ? ((+0x40 & 1 && EnabledFeatures & 4) ? 22 : 20) : 0`, 20 Grip Landscape or 22 Zoom Landscape with both buttons (the hand state is not ported; the camera's both buttons are, from the raw buttons, M15 C7); without the grab and action bits of GInterface +0x38 / +0x39, fn_005D7F20. State 22 is only one of three ways the camera takes both buttons (with the raw both-buttons bits, and the grip with the right button); the camera handles it in `CameraModeNew3::Update` 0x45B784 ([script-camera.md](script-camera.md#player-camera-the-land-grip)).
- Action state 4 IN LOCKED SELECT: 0x5D80C0 with an object gives 14 Select Lock; with a creature 19 Creature Interaction, or 14 when the creature's +0x10AC is not 0 (not named). States 3, 5 and 6 give 18 ([The locked select on a creature](#the-locked-select-on-a-creature)).
- A tug is action state 13 WAIT FOR PLUCK with nothing placed in the hand yet (CHand::PickUp 0x46DC30 only writes +0x4904 / +0x4908), so fn_005D7F20's object branch. (inferred) the tree stays the collide object (+0x400) during it.
- fn_005D7F20 reads the object under the hand at GInterface +0x400; with none but a screen object (+0x408) it gives 28 Over Bubble (0x5D8051..0x5D8069, plus fn_00459210(0xFA), not ported).
- **(not ported)**: the leash (+0x410) 32 and the bubble (+0x408) 28; an object that is not IsInteractable (vt 0x190) gives 3 (`Abode::IsInteractable` 0x407200 for Abode, Field, StoragePit needs vt 0x880).
- 13 comes from `ValidForLockedSelectProcess` (vt 0x6CC): a pile that is not the hand's own type (0x66E4F0), a field with growth and food (0x5299E0).
- 3 on the frame the grab-land button is pressed (GInterface byte +0x38 and +0x39 & 1, 0x5D7FD3).
- `GET_HAND_STATE` 413 is opcode function 0x6FF730: GInterface +0x3AC, the hand state of the last turn.
- The bubble's own test (0x5772E0) is GInterface +0x3AC == 0x1D, the hand state of the last turn (not 0x1E in the citadel).
- The hand's part of the tooltip constructor is fn_005D78D0 with the hand state; it is called once per turn, before the lifetime.

### The text of each state (table 0xBF1C10)

| State | Original | Texts |
|---|---|---|
| 3 Normal | fn_005D6980 | over a fish in the water and in the influence 0xE73 «Recoger»; otherwise 0xE7E «Mover» (left button, four arrows) |
| 9 Can Pick up | fn_005D6D70 | a flying object 0xE80 «Atrapar»; otherwise 0xE73 «Recoger», plus 0xEF7 «Golpear para Romper» (rocks) or 0xE7A «Golpear» when it can also be tapped; a one-shot orb only its own text, forced (pending) |
| 13 Can Select Lock | fn_005D77C0 | piles 0xEFD / 0xEFE «Cantidad Comida / Madera: N»; fields 0xE73 |
| 14 Select Lock | constant | 0xE85 «Interactuar» (up and down arrows) |
| 18 Over Object | 0x5D7190 | a creature: 0xE85 (the player's own, or another's in the influence); my built temple's entrance 0xECB «Entrar en Templo»; tappable and not pickable: 0xEF7 (rocks) or 0xE7A (abodes, a temple's leash post); otherwise the land texts |
| 19 Creature Interaction | constant | action 2, 0xE85 «Interactuar», arrows 0xF00 (all four) |
| 24 Object In Hand | fn_005D6F40 | a target that takes it: 0xE8E; otherwise 0xEEF «Plantar» (trees) or 0xEEE «Soltar», then 0xE74 «Lanzar» |

- State 3 (fn_005D6980): the leash first (`CalculateLeashToolTip` 0x5D67F0, not ported); the camera's tricons 0xE76 «Inclinar» (Tricon +0x90 & 2) and 0xE77 «Rotar» (& 1) are set by `CameraModeNew3::UpdateTricons` 0x459230 from the cursor near the screen's **edges**, not its centre: |x| > 0.45, y > 0.43 / 0.49, y < −0.49 / −0.4. In a window (LHScreen +0x64 `windowed`, [0xE850B4]) the 0.49 is 0.45 (0x459280; [script-camera.md](script-camera.md#player-camera-the-mouse)).
- State 3 over the water: a shown fish within 2 units (fn_00824B10 on the FishFarm list) gives 0xE73.
- State 5 Has Magic is fn_005D6C10.
- State 9: a flying object is Flags +0x24 & 0x40 IN_PHYSICS and altitude >= 0 (0x5D6DA3) → 0xE80; IsCannotBePickedUp gives nothing (0x5D6DC6); the one-shot orb's text is forced at 0x5D6E61.
- The tap text is `GetOverwriteTapToolTip` (vt 0x19C) or 0xE7A; Rock's is 0x6E7A60 (0xEF7); SpellIcon 0x726420 and the abodes keep 0xE7A.
- State 13: a pile is `Pot` 0x66F540: IsPileFood (vt 0x480) ? 0xEFD : 0xEFE, with the builder's GetResource; a field is 0x52A000 (0xE73).
- State 13's own text is `GetOverwriteInteractableToolTip` (vt 0x194, 0x5D787E, its only reader) or 0xE85. LeashObj's
  (0x464580: 0xEC8 / 0xECA / 0xEC9 for posts 0 / 1 / 2) is never shown: a post is no locked select
  (`ValidForLockedSelectProcess` Object 0x419330 = 0), so the hand over it is in 18, which gives 0xE7A.
- State 13: a TownCentre or a TotemStatue gives 0xECC / 0xEFC (the believers, the population); a select locked by someone (Flags & 0x10, 0x5D7867) gives the land tooltips.
- State 18 (0x5D7190): IsTownCentre (vt 0x1E0, 0x5D73A2) gives 0xECC then 0xEFC (0x5D73BE / 0x5D73DC), before the influence test.
- The believers number (fn_00740EA0, 0x5CA723): (GetBeliefInPlayer(me) − GetMaxBeliefMeNotIncluded) × 1000 (0x5CA728); below 0 it is × −1 and the text is 0xEE0 «%3.0f Creyentes Necesarios» (0x5CA73F..0x5CA74E).
- «Población: %3.0f» is Town +0x618 + +0x61C (adults + children), then "%s/%d" with +0x644 + +0x650 (the adult and child places) (0x5CA7B6..0x5CA824).
- State 18: after the town centre test (0x5D73B8 → 0x5D7402), a creature (vt 0x34) with Flags bit 0x400 clear and a player, when it is the local player's or the hand is in the influence (+0x48), gives 0xE85 with the town centre's action and arrows (0x5D7406); any other creature the land tooltips (fn_005D6980). There is no asleep test. Then, out of the influence (+0x48 and vt 0x714, 0x5D7485..0x5D74A3), the land tooltips.
- State 18, a CitadelEntrance (RTDynamicCast at 0x5D74B8), after the influence test and before the storage pit: with its
  heart (+0x54) `CitadelPart::IsBuilt` (vt 0x890, 0x464AD0: +0x58 bit 1 «under construction» clear and GetPercentBuilt
  >= 1), the heart's GetPlayer against the local player (0x5D7544..0x5D7570): mine gives action 2, 0xECB «Entrar en
  Templo» (HELP_TEXT_TOOLTIP_89), the row's align, no help event, not forced (0x5D7576..0x5D7589 → 0x5D7686);
  another player's returns with no tooltip at all (0x5D7570 → 0x5D76B4). SET_INTERFACE_CITADEL is not read there: a
  locked entrance still says «Entrar en Templo». A heart that is not built goes on to the other tests, so the tap's
  0xE7A while the entrance is valid to tap (CitadelEntrance keeps GameThingWithPos's GetOverwriteTapToolTip 0x5705C0 =
  0). The entrance is state 18, not 9: it keeps `Object::ValidForPlaceInHand` 0x402870 = 0 (vt 0x6FC). openblack:
  `worship::citadel::EntranceToolTipFor`, read by `HandSystem::SubmitToolTips`.
- State 18: `IsStoragePit(0)` (vt 0x300, 0x5D7594) gives action −1 and 0xEF9 «Comida Almacenada: %3.0f Madera: %3.0f», GetResource(FOOD) then GetResource(WOOD) (0x5CA878 / 0x5CA894, through a float).
- State 18 **(not ported)**: the leash first (0x5D71A8); a site to build (MultiMapFixed: 0xEFB, 0xEF4), ShowNeedsVisuals (0xEFB / 0xECE / 0xEFF), IsCitadelHeart 0xEE8; IsSacrificeAltar 0xEE9, IsTownDesireFlag 0xE88, IsScriptHighlight 0xEA0 / 0xE83 (0x5D75B6..0x5D7631).
- State 18: tappable and not IsCannotBePickedUp (0x5D7649 / 0x5D7657) gives GetOverwriteTapToolTip or 0xE7A.
- State 24: `GetOverwriteDropToolTip` (vt 0x198): Tree 0x74B790 gives 0xEEF «Plantar», else 0xEEE «Soltar»; then 0xE74 «Lanzar».

### The help system's part

- **SubmitToolTips** 0x5C9A70:
  - the texts 0xE73..0xF1C, with {priority, display, afterFocus} from info.dat;
  - a higher priority keeps the tooltip;
  - while the display timer runs (2.5 × display × 10 turns), only a forced text takes over.
  - So «Soltar» and «Lanzar» alternate every 25 turns.
  - Its arguments are (text, action, align, force); `action` is the BINDABLE_ACTION whose button or key the icon shows (−1: none).
  - The same text again only takes over again if it is forced.
  - 0xD16668[cur] += 1 is read by nobody.
  - It resolves the action with fn_005C78F0 (0x5C9BC4); if that fails, animType 0, clickType 0 and row fn_005C4800() (0x5C9BD0..0x5C9BE0).
- **ForceToolTips** 0x5C9C60: the amount in the hand, 0xEEA (priority 0.925); it is Submit(text, −1, 0, 1) plus the value in [0xD17BBC].
- **Every turn** (fn_005C9D00):
  - the hand submits;
  - a text nobody submits stays afterFocus × 25 turns;
  - at TOOLTIP_LEVEL 2 a priority below 0.9 fades out in 1 s once its display time is over: fn_005C9C80 returns 0 with no icon, with the display used up and priority < 0.9 (0x5C9CC3); then no icon is created (0x5C9E1A).
- **Global variables:**
  - [0xBF19C4] index of the shown text (text − 0xE73);
  - [0xD17BC8] display turns (while > 0 only a forced text gets in);
  - [0xD17BCC] afterFocus turns; [0xD17BC4] forced; [0xD17BD0] align; [0xBF19C8] action;
  - [0xD17BDC] "sent this turn", which HelpSystem::Process sets to 0 after fn_005C9D00 (0x5C9016);
  - [0xD17BBC] the ForceToolTips value;
  - 0xD163C0: times each text took over, capped at 80 (0x50, 0x5C9C32); they are the seconds of the fade-in.
- **The constructor** 0x5C9FC0:
  - the text's number: the forced value for 0xEEA / 0xEE1 / 0xEA2 / 0xEEB; for the other texts with a number, the object's number (UNICODE_sprintf of the first conversion);
  - 0xEF9 carries two numbers in order (food, then wood);
  - 0xEFC appends "%s/%d" ([0xBF19EC], 0x5CA81E..0x5CA824) with the places;
  - 0xECC becomes 0xEE0 if the believers are negative (0x5CA745);
  - it resolves the action again on its own (0x5CA07C): −1 sets animType 0 and row 3 (0x5CA06C / 0x5CA072); a failure gives clickType 0, row 3 and animType 0 (0x5CA088..0x5CA090), that is, no picture;
  - the icon is kept while clickType (+0x18), row (+0x1C), animType (+0x14), align | 0x16 (+0x12C) and the text (wcscmp +0x20) are equal (0x5C9E57..0x5C9E91). A key's name is not compared: its row is the pointer [0xD16110];
  - KMIcon creation (0x5C9F83): start 0, target 1; duration = the fade (times shown, in s; immediate if forced **or at TOOLTIP_LEVEL 3**); x 0, y 0 as offsets from the hand (align | 0x16 anchors it there); S = H/25 (0x5C9D6C..0x5C9D8D, imul 0x51EB851F, sar 3); c1 yellow {b0, gFF, rFF, aFF}, c2 white, alpha8 0x80.
- **The icon** fades in, in real time, over as many seconds as the text has been shown (forced: at once).
- **Pause:** with g_game +0x14 & 4 (PauseGame 0x54AE20) and outside the citadel, the icon is deleted (0x5C9D44..0x5C9D5E); inside the citadel it stays. The condition includes [0xD17BB8] <= 0, which is only ever written to 0 (0x54A86C).
- `HelpSystem::ResetIcons` 0x5C5610 deletes the KMIcons +0x24, +0x2C (the tooltip's, 0x5C5632..0x5C5649) and +0x28; it is called by Load 0x5C73E6 and `HelpDudeControl::Uninit` 0x5C56ED.
- **Drawing** (CameraHelp::DrawKeyOrMouse 0x447EA0):
  - S = H/25, at the hand on screen; the text at int(2S/3);
  - black copies at (−1, −1) and (+1, +1), then yellow;
  - S/2 to the left of the hand, or ending S/2 to its right past 2/3 of the screen (hysteresis back below 1/3).

### KMIcon (the button or key drawing)

- Object of 0x140 bytes (camerahelp.cpp). Ctor fn_004489D0 with 14 arguments (start, target, duration, animType, clickType, row, text, x, y, S, align, c1*, c2*, alpha8). fn_00448BE0 links it at the head of the list [0xC5AFE0] (newest first, next at +0x13C); the dtor ??_GKMIcon 0x448C00 unlinks it.
- Fields:
  - +0x00 start; +0x10 elapsed;
  - +0x14 animType (−1 key; 0..3 button blink, fn_00447450);
  - +0x18 clickType; +0x1C row (0, 1, 2; 3 = no icon) or key code;
  - +0x20 wchar[0x80] of the text (L"" if null, 0x448A75); the original points to the shared buffer [0xD16110] (or [0xD16184]), which every key resolution rewrites (0x5C797D / 0x5C799F);
  - +0x124 y; +0x12C align;
  - +0x130 c1 (white if null); +0x134 c2 (c1 if null).
- Update (fn_00448AC0, 0x448AC0..0x448B6C):
  - elapsed += dt, clamped to [0, duration];
  - current = start + (target − start)·elapsed/duration, or target if the duration is 0;
  - a = clamp(current, 0, 1), factor for the text, the icon and the panels;
  - c1.a = c2.a = ftol(a·255), panels at ftol(alpha8 × a) (0x448B57..0x448B6C);
  - SetupBox::GetCurrentActiveBox() && box +0x94 skips the draw (0x448B72..0x448B88).
- Loop fn_00447850 with g_delta_time [0xC38134] × 0.001:
  - runs **twice per frame** with the same dt: #1 at the end of `HelpSystem::Draw3D` (0x5C5B31, from GGame::Process3dEngine 0x54E2ED), under FinishFrame's overlays; #2 at the end of HelpText's FinishFrame callback fn_005CCAB0 (0x5CCE41, also on its early exit 0x5CCAD4); so the fades last half their nominal time;
  - before that it draws a stray mouse picture from 0xC5AFBC..0xC5AFDC, which nobody writes (**inferred**: it draws nothing).
- The hand point is ProjectPoint 0x819390 of [[GInterface +0x3A0] +0x482C] +0x38 (0x447EB9..0x447F01).
- Created by the tooltip (0x5C9F83), the click prompt of `HelpSystem::Draw3D` (0x5C59D0..0x5C5ACE, HelpSystem +0x24) and the $M control icon (fn_005C5B50, +0x24 / +0x28). The «Did you know?» bubble's $m icons go through fn_005760C0 0x57636B..0x5763CC.
- The KMIcons +0x24 / +0x28 are deleted when HelpText has no text (fn_005C5E50, once per turn).
- `CameraHelp::DrawKeyOrMouse` 0x447EA0 in detail:
  - Y clamped to [0, H − S] then X to [0, W − S] (0x447EB9..0x447F85); y is the box's vertical centre (0x447F87..0x447FA0); a null c1 is white, a null c2 is c1, nothing when both alphas are under 4 (0x447FA7..0x44800B);
  - a key's text (0x448011..0x448108, "%s + %s" 0x9CDFA8, key names fn_0046EE60) and width fn_00447910 = max(1, GetStringWidth(text, 0.4S) + int(2S/3)) (0.4 at 0x8C7664); a mouse button is S wide, none for row 3 (0x448111..0x448128); + S/2 per arrow stub 0x400 / 0x800 (0x448130..0x448183);
  - text size int(2S/3) (0x448187; int(4S/5) with NeedsBiggerText 0x4079C0); total = textW + keyW' + 2.0 (0x8AB478);
  - layout 0x448214..0x448353: align & 1 right-aligns, align & 2 the side with hysteresis [0xC5AFE4] (1 past 2W/3, 0 below W/3); align & 0x10 text first, & 0x20 the side flag instead (0x4482E5..0x448301).
- Panels only with alpha8 ≠ 0 (0x448357..0x448541), fn_00447BA0 in AdditiveMaterial [0xEDC364]: behind the icon c2's rgb at alpha8 (0x448366..0x448399), the rest c1's rgb at alpha8/3 (0x4483A8..0x4483CD), behind the text with o = (S − textSize)·0.5 (0x4483CF..0x44841E); stubs 0x400 / 0x800 / 0x100 / 0x200 (0x448426..0x44853C).
- fn_00447BA0 is a 9-slice of the soft blob (0, 0.25)–(0.24609375, 0.49609375) of atmos.raw, DrawAlpha = argb >> 24 (0x447C03), b = (y1 − y0)/4 (0x447BAD..0x447BBB), corners 2b squares (0x447BD6..0x447C2B), breaks u 0 / 0.078125 / 0.16796875 / 0.24609375, v 0.25 / 0.328125 / 0.41796875 / 0.49609375 (0x447C38..0x447E83).
- Key cap fn_00447990 (0x448554..0x44857B): a 3-slice in AtmosMaterial [0xEDC368], white with DrawAlpha = c2.a (0x4479D4), v 0.001953125..0.248046875, u 0.501953125 / 0.564453125 / 0.685546875 / 0.748046875 (0x447A1C, 0x447A83, 0x447ADE); its text (0x447AF2..0x447B93) at (x0 + h/5, y0 + h/7), size 0.4h, black with the cap's alpha.
- Arrows (0x4485A6..0x44888D): AtmosMaterial, colour c2, grown by g = S × 0.1111111 (0x8C7668); left 0x4485D3..0x44866B, right 0x44867F..0x44871E, up 0x448736..0x4487D3, down 0x4487EB..0x44888D. The text (0x448895..0x4489BC): DrawTextRaw three times in j0, black at (−1, −1) (0x448913) and (+1, +1) (0x448968), then c1 (0x4489BC); y offset o is a float.
- Resolving an action's picture (fn_005C78F0):
  - −1 and 0x21 have no picture;
  - mouse binding (fn_00471940): animType 0, GetClickTypeForAction 0x471620, row fn_005C4800;
  - key binding (fn_00471850): animType −1, clickType 0 and the name written to [0xD16110] by fn_004712D0 (Windows name via fn_0046EE60, **inferred**).
- GetClickTypeForAction 0x471620 (0x47168C..0x47173A): code 1 LMB → 1, 2 MMB → 4, 3 wheel up → 0x10, 4 wheel down → 8, 5 RMB → 2, other → −1. Actions 3 / 4 / 6 give code 0 (fn_00471910 → fn_00471760) **(not ported)**.
- Mouse row (fn_005C4800): 2 with a wheel ([0xE85248]), 1 with more than 2 buttons ([0xE8524C] > 2), else 0.
- `mousehelp.raw` (fn_00447450, called at 0x44859E):
  - an S×S quad of mousehelp.raw (256×256, 4×4 cells of 64 px) with mousehelpa.raw, material mode 5 [0xC5A318] made on the first draw (0x44757B..0x447591);
  - a mouse with a wheel uses the third row; the left button is the right one mirrored; the wheel turns on the fourth row;
  - lit state from GetTickCount (0x447461..0x4474EA): animType 1 (T/100) % 5 < 2, animType 2 (T/80) % 12 in {0, 1, 4, 5}, else always;
  - the cell by clickType (jump tables 0x447838 / 0x447818, 0x4474EC..0x44756E; 1 and others = the right button's cell mirrored); nothing for a negative cell or colour 0 (0x4475A9..0x4475B7); u = (c & 3)·0.25, v = (c >> 2)·0.25, mirrored u + 0.25 → u (0x4475BD..0x44761F);
  - mode 5, tiling off (0x447709), ZFUNC ALWAYS (0x44776E), ZWRITEENABLE 0 (0x447795) around DrawAndClip2D 0x4477BB.
- ControlMap (g_game +0x250300), `LoadDefaults` 0x46F890:
  - action 1 → mouse 1 (LMB, +0x30C / +0x514, text 0xA5C);
  - action 2 → mouse 5 (RMB, +0x618 / +0x820, text 0xA5D);
  - action 0 → key 0x3B (F1);
  - the rest not decoded.

### Differences

- **(not ported)**: the camera's tricons (0xE76 «Inclinar», 0xE77 «Rotar»), 0xE8B «Alejar», the leash, the scaffolds,
  giving to the creature.
- The amount 0xEEA is forced every turn of the scooping, from ProcessInInteract (Pile 0x66E6E4, Field 0x52989F, FishFarm
  0x52DAD6), and once when it ends (Pile 0x66E8DA, Field 0x529AD9, FishFarm 0x52D92A). It then stays about 13 turns,
  until «Soltar» takes over. The help system's turn (`HelpSystem::Process` 0x5C8FE0) is `Game::GameLogicLoop`'s step
  0x54E69E.
- Out of the influence a second press with something in the hand does nothing (ActionPressedHolding 0x5D16BE).
- The tooltips still missing in openblack are in [Pending](#pending).

## Placement, the near object, the morph and the tap memory

**Where the hand goes, as the original**
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
- fn_0046E160: d = (−sin H, 0, cos H), horizontal towards the camera; H (CHand +0x84, written at 0x46C725) is taken in PrepareForDrawing from the camera → mouse ray (fn_0074CBD0) only when |ray.x| or |ray.z| > 0.01 ([0x8C7A10], 0x46C6FB / 0x46C70E).
- side = norm(d × up), skipped when zero; the model's X = side, Y = side × up, Z = −up (the scale and the left hand's mirrored X are the transform's).
- The empty hand's up over plain land is `LH3DIsland::GetNormal` 0x803630 at the hand's last position (arg5 is read before its last write).
- The three up Zoomers are 0xD13FB0 / 0xD13FE0 / 0xD14010 (`Zoomer::SetDestinationWithSpeedAndTime` 0x407D60 per axis at 0x5B7075 / 0x5B708E / 0x5B70A7, 0.4 s pushed at 0x5B7065).
- They take a new target only when the mouse's x changed this frame (CHand +0x4858, 0x46C6EA) or the roll is not 0 (0x5B7043..0x5B705F; 0 in HandStateNormal), and are updated every frame; g_D13F60 is their value normalised (fn_00460710 at 0x5B712A).
- `HandStateNormal::Enter` 0x5B5D00 snaps them to (0, 1, 0) at 0x5B5D0C / 0x5B5D4D / 0x5B5D93.
- Land grip: the change to Cgrip (0x5B075B) sets hand->pos = the land point under the mouse (g_D13F30, fn_0046DF60(false)); HandStateCamera keeps it at the grabbed point with no offset (0x5B04C4) and uses the last Normal up (g_D13F60, frozen).
- **(not ported)**: with an object, a bubble or the leash in the interface (+0x3C8 / +0x3D0 / +0x3D8) the original keeps the Normal position at the change to Cgrip.

**FindObjectNearMapCoord fn_005D39E0**
- When a click hits nothing, it takes the nearest object within ±5 of the land behind the hand, as seen from the camera.
  That point is GLandscape::Draw 0x5E4848, the box of the bones without the root.
- The search goes through the cells, skips fragments, and keeps the nearest one if it is not farther than the action
  point. A fish shoal is tried first.
- **(approximate)**: the press path filters the result with the hover's class filter.
- The point behind the hand (GLandscape::Draw 0x5E4848..0x5E4870, once a drawn frame): the box of the hand's bones 1..n−1 (TransformedMatrices +0x80, element 1 first, 0x5E46DC..0x5E47CD), the camera's ray through its centre onto the land (`LH3DIsland::RayCast` fn_00802550), with the plane y = 0 within 7500 m when the land is missed; g_0xD2017C = hit, g_0xD1A3A0 = (x, 0, z).
- MapCoords c of that point: x, z = ftol(m × 6553.6), altitude = 0 − GetAltitude (so 0 only on a cell at height 0).
- c on the water (MapCoords::IsWater 0x6035B0 at 0x5D3A2F, (inferred)): the shoal near the action's point (fn_00824B10 on +0x3F0, xz distance² < 4), then the fish farm of that shoal (g_game +0x205C0C, +0x88 == shoal).
- The search square is ±5 m (0x40A00000 at 0x5D3B1B) around c (fild × 10 / 65536, 0x5D3AC9..0x5D3B9C; ftol((c ∓ 5) × 65536 / 10), signed high words), x outer, z inner, the fixed list then the mobile one.
- Every object but the fragments (0x5D3C29), at its 2D distance from c (`GUtils::GetDistanceInMetres` 0x74CD70); the nearest under 5 ([0x8AB6E4]; strict, the first of a tie).

**Grip dust:** SPOT_VISUAL 2 through the particle system's `StartSpotVisual`, made by packet 0x2B → 0x63D6D8.
**(not ported)**: the packet's one-turn delay.
- The grip dust packet 0x2B is sent by fn_00550CF0 at 0x5D1FC9; SPOT_VISUAL 2 GRIP_LANDSCAPE lasts 100 turns, is Z-sorted, stepped once a turn and drawn with the turn fraction.

**Good and evil** (openblack: `HandMorph.cpp`)
- CHand::PrepareForDrawing 0x46C550 morphs the hand when the local player's alignment moves 0.03 or more.
- **Texture:** Blend4444 fn_00870640 between Base2 and Evil2 / Good2, at t = min(255, trunc(|a|·256)).
- **Vertices:** MorphVertices 0x618D10. Evil2 grows claws.
- MorphAnims changes nothing for the hand.
- Test hook: `OPENBLACK_TEST_HAND_ALIGNMENT=<-1..1>`.
- `CHand::PrepareForDrawing` calls `SetTextureSet(GInterface +0x48)` 0x46C60C (SetTextureSet 0x46BF60); a change blends the texture again; the set-0 branch fn_0046BFA0 is an empty ret.
- The local player's alignment, clamped to −1..1, goes into Morphable +0x9C (0x46C63A..0x46C665); `Morphable::UpdateMorphing` 0x618C40 re-morphs when it moved 0.03 ([0x900AD4]) from the one applied (+0xA0, zeroed by MorphInit 0x617310).
- `Morphable::LoadBase` 0x618360 / `ReadBinary` 0x617AE0: the base twice (the drawn mesh [0] and the pristine [1]), then hh.HBN's variants [2] hand_boned_evil2, [3] hand_boned_good2 ([4..7] empty).
- `Morphable::MorphTexture` 0x619500: a < 0 → Evil [2] with f = −a, else Good [3] with f = a (0 counts as good); the base [1] when the variant is missing. It writes the skin's texels and sets the material's dirty flag +0x138.
- `MorphVertices` 0x618D10: out = base + w·(m − base) on the position (+0) and the normal (+0x14), w = |a|; the hand has no second or third axis (slots [4..7] empty).
- `Morphable::MorphAnims` 0x619100 changes nothing because the three files have the same bones and hh.HBN's evil and good animation tables are empty.

### Reach and the ControlMap switches

**Reach:** CHand +0x4838 (1800; at most 1800, fn_0046BF20) is set by SET_INTERFACE_INTERACTION
(HandSystemInterface::SetHandReach).
- Cap [0x8CBEAC] = 1800; set by the CHand ctor (0x46BC3E).
- Who reads the reach:
  - GInterface fn_005D1AB0 (5 calls via fn_0046BF50);
  - CHand fn_0046DF60 (0x46E00F, 0x46E123);
  - `CHand::SetDistanceFromView` 0x46C0D0, which at 0x46C0E4 clamps the distance to [2, reach].
- SET_INTERFACE_INTERACTION (0x70B220) calls fn_0046BF20 with 75 in JUST_GRAB (0x42960000 at 0x70B30D) and with 1800 in the other levels that set it.
- These do not touch the reach: JUST_HAND_MOVE 8 (0x70B4BC), JUST_HAND_INTERACTION 12 (0x70B520, which only sets bit 2) and the invalid levels (9 and > 15, 0x70B785).
- ControlMap switches +0x652C / +0x6530 (set to 1 in the ctor, 0x46F719 / 0x46F724):
  - `GGame::ProcessKey` 0x63F42A..0x63F446 requires both for the LH_KEY 2..15 block (KB_1..KB_TAB);
  - fn_0046F750(action) is called by `ControlMap::IsActionPerformed` 0x470AF9, fn_00470A00 and fn_00470A60. With switch 1 off it blocks actions 3..19 except 5 (byte table 0x46F7A4); with switch 1 on and 2 off, it blocks 17..19 (0x46F783..0x46F78B).
  - **(inferred)**: with openblack's bit order, that is all camera movement except TALK, and then ZOOM_TO_TEMPLE / ZOOM_TO_CREATURE / ZOOM_TO_REALM.

### Tap memory

- **The slot:** GInterface +0x45C (the object) and +0x46C (the land point). It is written by RememberTapped fn_005D36D0,
  from Tap and from fn_005D3700 when the action button is released idle, and it lasts 15 s of game time.
- **Readers:** GAME_THING_CLICKED, CLEAR_CLICKED_OBJECT, CLEAR_CLICKED_POSITION and POSITION_CLICKED (fn_005D0460).
- **(approximate)**: the per-tick check uses last frame's hover.
- `RememberTapped` fn_005D36D0: `BaseInfo::Set(+0x45C, obj)` 0x436BB0 (null clears it) and +0x468 = the game turn; `BaseInfo::GetBase` 0x436B80 returns null when nothing is stored or the object no longer exists.
- GAME_THING_CLICKED is 0x70AEB0 (GInterface +0x45C, the last object tapped with the action button); `GameThingClicked` 0x70AF84 and CLEAR_CLICKED_OBJECT 0x70B0E0 set +0x460 = +0x464 = 0; the turn +0x468 stays.
- POSITION_CLICKED 0x70B120 pops the radius, then the position (0x70B130..0x70B170); a multiplayer game logs "This is not multiplayer friendly yet!" and pushes true (0x70B183..0x70B1B2); then fn_005D0460(MapCoords(pos), radius) (0x70B1C7..0x70B1DA): `GUtils::GetDistanceInMetres(+0x46C, pos) <= radius` (`test ah, 0x41`); +0x46C is the land point of the last land tap in the last 15 s, (0, 0, 0) once cleared.
- CLEAR_CLICKED_POSITION 0x70B100: +0x46C / +0x470 / +0x474 = 0; the turn +0x478 stays.

**Store:** Object::DoDeleteObjectAndTakeResource 0x63A940 is in ecs::object_delivery (ObjectDelivery.h); the class
tables are `ecs::resource_stores` and `ecs::held_apply`. DepositInStore (a thrown tree that hit a pit) passes the
giver's interface; the storage pit's handler makes reaction 0x16 (`take_resource::StoragePit`).

## The hand's state, the spring, the held object, power-ups and roots

**A pile in the locked select is HOLDING** (GetRequiredState 0x46CD10, block 0x46CD5D..0x46CD9A)
- The test: an object is held (+0x4904), the status's locked select (+0x39C → +0x3C) is set, and that select is a pile: IsPileResource, vt +0x4CC.
- Then the state is HOLDING 4 (0x46CDF6), before GRAIN 8, TUG 3 and the 180 ms release impulse (0x46CDEC). Interface mode 0x19 still gives INVISIBLE first (0x46CD38).
- The impulse's own test (0x46CD6C) asks the same thing.
- **IsPileResource:** PileResource::IsPileResource 0x66ED60 returns true in the PileWood, PileFood, PileResource, MagicWood, MagicFood and PuzzleGrain vtables; GameThingWithPos 0x402490 (false) is every other class. In openblack it is `ecs::object_resources::IsPileResource`: a pot whose GPotInfo potType is PileFood or PileWood.

**The spring on a refused release** (State_ReleaseHeld 0x5D4DB0)
- When the action is not valid (0x5D5150), the HOLDING state's spring stops (states[4] +0x104 = 0). That happens only when CHand +0x4878 is 4, then DropOnMapCoord 0x5D1850.
- There is no ClearBuffer on that path; it is RemoveFirstFromHand's (0x5DC239).
- **(approximate):** openblack has one spring flag for every state.

**The held object's turn place** (GMagicHand::Process 0x5FB1C5..0x5FB250)
- MapCoords x, z = ftol(x or z × 6553.6).
- The land under it is looked up at a second conversion, ftol((x × 65536 [0x8AC408]) × 0.1 [0x8AC404]) (0x5FB1FE / 0x5FB20E), in openblack `map_coords::MetresToFixedForHandLookup` (was `ToFixedTenthTimes65536`).
- The altitude +0x1C = y − GetAltitude there, kept as a float. There is no quantisation of y.

**The held object's sorting, shadow and specular** (CHand::SetHeldObject fn_0046E590)
- **Need-sorting:**
  - the press fn_0046DC30 saves IsNeedSorting (vt 0x44) at +0x4910 and sets it (SetNeedSorting vt 0x40, 0x46DCD3), so the object goes through the Z-sorter (0x8133AE);
  - another object (fn_0046DE40) gives the old one its bit back;
  - CHand::ThrowObject 0x46DDD0 restores it (0x46DDEF).
  - In openblack: `components::NeedsSorting`, only for an object with a mesh.
- **Shadow:**
  - vt 0x78 SetCastDynamicShadow (fn_008168A0) sets bit 0x40 of +4, which in fact means the object *receives* the projected shadows. Only the hand and the creatures cast them (CreateDynamicShadow 0x80C020).
  - The hand's vt 0x234 fn_00816830 clears the bit on the held object (0x816842) and adds its outline to the hand's own shadow (0x816855). openblack does both (ShadowList, RendererShadows).
- **Specular:** the press also zeroes the G3D's specular +0x50 (0x46DCDF). Every Draw rewrites it from the land light (inferred).

**ProcessPowerUpSystem runs twice** (GInterface 0x5CF300)
- Its callers are fn_005D1120's three exits (0x5D118A / 0x5D11E1 / 0x5D124D). It runs on both of fn_005D9A20's paths:
  - the turn's: GInterface::Process 0x5CEC10, with the last frame's g_game_time_inc;
  - the frame's: ProcessFrameUpdates 0x5CEDB0 = `jmp 0x5D9A20`, with this frame's.
- openblack's hand_casting::ProcessTurn and Update are those two.
- **(not ported):** the extra call per queued action (fn_005D9BC0 0x5D9CFD) and the playback path (fn_005DAEE0 0x5DB2EE). fn_005D9A20 runs InterfaceActionProcess once for each queued button message (types 1-6 of GInterfaceMessageBuffer +0x430, flag +0x30 of table 0xD186B8; fn_005D9BC0 0x5D9CFD) and once with none, so a frame with two button edges runs it twice.

**A tree's roots**
- **A live tree:** the roots have no object of their own. fn_00511270 draws them from the tree's G3D, the rotation × 0.15 × extent (0x8CF110). It is called from Tree::DrawOutOfMap 0x74B295 (in the hand) and PhysicsObject::DrawAll 0x646FCF (flying). A deleted tree's roots just stop; openblack writes them with a zero matrix.
- **The falling roots:**
  - Tree::EndPhysics 0x74B830 sets the DeadTree's +0x98 |= 0x10 (0x74BC10).
  - At its first draw, DeadTree::Draw 0x5107F0 makes the node: fn_00826280, mesh 0x250, list 0xEB9A10.
  - The node copies the DeadTree's G3D matrix (0x8262F9..0x826304; rebuilt by SetScale 0x51092D) and scales the rotation (0x826306..0x826357).
  - It falls y0 − 20 t², fades after 18 s and is freed after 20 s.
  - A forester's felled tree never sets the flag, so its roots stay on.

### The hand's packets and per-turn pass

- The per-turn pass: `GInterface::Process` 0x5CEC10 → fn_005D2250 (the sync packets) → fn_005CEBB0 → `GInterfaceStatus::Process` 0x5DC4E0: the movement (fn_005DBC60), the heart beat, the locked select and `ProcessHands` 0x5DC6A0 → `GMagicHand::Process` 0x5FB190, which places the held object at the synced hand +0xC8 (0x5FB1C5) and then calls its `ProcessInHand` (vt 0x804). The held object's `ProcessInHand` thus runs at the start of the turn.
- `CHand::GameTurnUpdate` 0x46E4E0 (0x46E505..0x46E557): the HandStateGrain raise (`fn_005B2D70`), `CHand::ThrowObject` when the held object is no longer available, +0x490C = GetHoldType, the +0x48FC clean-up (+0x49B4 / +0x4950 not read), then the hold.
- fn_005D2250 sends 0x15 (hand and camera, 0x5D2562), 0x16 (hand, 0x5D24E6) or 0x17 (camera, 0x5D2462) when the hand or the camera moved; with nothing to send it returns at 0x5D245C, leaving the countdown.
- Its statics: what was sent last (0xD17D48 / 0xD17D68 / 0xD17D58 / 0xD17D78) and the countdown 0xD18230.
- While the countdown is not 0 and no hand cast needs continual packets (fn_005DC810), it goes down by one and nothing is compared (0x5D237D).
- After a send the countdown is fn_005558B0 (0x5D261D): 3 when g_game +0x59A8 == 1 (the internet lobby), else 1.
- "Moved": fn_005D2660, a component differs by more than 0.01 ([0x8C7A10], the float difference against the double); the hand counts as moved in another map cell (fn_005D26B0, CellX / CellZ, the high words).
- fn_005DC810 → fn_00721480 walks the spells (g_game +0x205BC4) until one answers vt 0x514 `Spell::NeedsContinualPackets` 0x7214C0: IsCastFromHand, not closed down (+0x40), IsHumanPlayerCasting (+0x4C) and the status's player.
- The ping ring: GetTickCount at each own 0x15 / 0x16 / 0x17 send ([0xD443D4], 32 entries, index [0xD1822C]; written at 0x5D2474 / 0x5D24F8 / 0x5D2574, and by SendApplyToMapCoord 0x5D35DF); the old index goes into the packet's byte +0x40.
- `GPacket::ProcessPacket` (0x63CAAE..0x63CB85) stores [0xD44454] = GetTickCount − ring[index] for MyInterface (0x63CAD3 / 0x63CB22 / 0x63CB85), 0 until the first one; state 12 reads it (0x5D5066).
- The GPacket: type byte at +1; jump table 0x63DDCC indexed by type − 6. Uncompressed GPacket: 0x110 bytes (fn_00551EE0); +8 the object.
- `GPacket::GetObject` 0x63DF70: index and unique id must match; if the object no longer exists the handler receives NULL and takes its NULL path, almost always EndAction (0x63C939 / 0x63C971 / 0x63CC4B).
- Handlers: 0x12 ApplyToMapCoord 0x5DA400; 0x15 hand and camera, 0x16 hand, 0x17 camera (the sync handlers, GPacket::ProcessPacket 0x63CA86..0x63CB9C; their addresses disagree between two readings, see [Pending](#pending)); 0x1A ApplyUnlock 0x5DA8A0; 0x1B `NetworkFriendlyStartLockedSelect` 0x5DA950; 0x1C `NetworkFriendlyEndLockedSelect` 0x5DAA10; 0x1D `ThrowObjectFromHand(status, 1)` 0x5DA8F0; 0x1F give 0x5DA2D0; 0x27, 0x59, 0x5F and 0x63 the creature's ([The CREATURE state and its packets](#the-creature-state-and-its-packets)); 0x40 the spin of the released object 0x63CDF7; 0x6A icon power-up charge 0x5DAC30.
- **(not ported)**: the sync handlers' copy of the ring index +0x11C and the copies +0xD4 ← +0xC8, +0xE4 ← +0xE0, +0xE0 = 0 (no reader found).
- 0x12 handler 0x5DA400: fn_005D3680 (+0x448, 0x5DA431); an empty hand (status +0x90) ends the action (0x5DA448); the held object IsAvailable (vt 0x2C, 0x5DA474) and vt 0x724 (0x5DA48B), else nothing; then its ApplyThisToMapCoord at the packet's MapCoords (vt 0x728, 0x5DA4A7) and HandleApplyResult (0x5DA4B6), and then the throw.
- The apply gate: GInterface +0x444 is the turn an apply packet (0x11 / 0x12) was sent (fn_005D36A0), +0x448 the turn its handler ran (fn_005D3680, 0x5DA1B7 / 0x5DA431); both go back to 0 in fn_005D2250 once +0x444 <= +0x448 (0x5D2634..0x5D264A) and in EndAction fn_005D1260 (0x5D1303 / 0x5D1309).
- GInterfaceStatus +0x10C is the synced hand's velocity in units per second (fn_005DBC60), read by GLandscape::Draw's hand wind (0x5E435F).

### HandStateGrain

- HandStateGrain is CHand +0x489C, hand state 8 (the hand holds a spell seed); ctor 0x5B2B80, vtable 0x900B00.
- `UR_HandSprinkle` starts it (fn_005B2F70), CHand::GameTurnUpdate steps it, HandStateHolding::Update reads it.
- vt 0x18 (0x5B2D30) / vt 0x14 (0x5B2D50): the height and the tilt interpolated between the last two turns with g_game +0x205D64 (the fraction of the turn drawn).
- vt 0x1C (0x5B32A0): with ClampHand the hand's required position is the one it had when it started.
- Enter 0x5B3080 (after HandStateHolding's) resets everything; Exit 0x5B3290 sets +0x124 = 0, +0x13C = 0.

## Pending

- **The locked apply's exit test**: 0x5D4CB0 sends the unlock only for an object in the hand whose byte +0xA bit 0 is
  clear; what that bit is is not identified. openblack sends it for any seed in the hand.
- **The status +0x9C** (the turns the last locked apply was held, written by the 0x1A handler): no reader is known;
  openblack keeps it in `HandSystem::_unlockTurnsHeld` and nothing reads it.
- **The throw from the hand (#134)** ([Holding, spring and throwing](#holding-spring-and-throwing-handstateholdingupdate-0x5b3c70)):
  the body is now built at the drawn pose (`PredictRelease` takes it; `SetUpBody` still reads the Transform, so
  `PredictRelease` sets the pose there for the setup and puts the turn's place back). Still open: the 0.35 smoothing of the
  drawn object's velocity in `HandSystem::UpdateHeldObject` while the spring is off is not the original's (+0x48C8
  keeps its last value then); no throw reads it today. +0x48E0 (the drawn position + 0.2 v) is not written by
  openblack's Holding; whether a held object other than a seed reaches SendApplyToMapCoord's block path has not been
  read.
- **The mouse ray's rounding** ([The mouse ray](#the-mouse-ray)): `Camera::RayFromEye` takes the screen point in
  0..1 (the cursor over the window size), not integer pixels over W/2, and does not round the point through the near
  plane and `g_camera` as fn_0074CBD0 does (the eye added in `Get3DPointFromScreen` and taken away again): its
  direction can differ from the original's in the last bits. While the falling spell draws a view of its own, the ray starts at that
  view's eye; whether `g_camera` is that eye then has not been read.
- **A tree under the cursor (#130)** ([Object under the cursor](#object-under-the-cursor)): the original has no
  forest-patch pick, so nothing was ported for it. Still open:
  - the trees' pixel collide is ported (a measured behaviour change, 2026-10-09). Left: (inferred) DXT5's steps and their
    drop to 4 bits as DirectDraw's conversion rounds them (only alphas near 16 can differ); openblack cuts a triangle
    only at the near clip (the original also at the screen's sides, the same for a pixel on the screen) and does not
    clamp a corner that is off a side; the original's extended precision is approached with doubles; while an object is
    held the pixel is that of openblack's raised object ray, not the mouse's (the original's draw collide is at the
    mouse pixel, and only the hand's placement takes the raised ray); whether a `DeadTree` (its info type) takes the
    pixel test has not been read; the original's LOD and status masks stand as openblack's drawn sub-meshes;
  - every other object's pick: the original takes the first of its triangles that holds the mouse pixel, openblack the
    nearest hit along the ray (`L3DMesh::RayIntersect`, over every sub-mesh but the physics ones). The hand's placement
    no longer uses it: it is the original's own test, fn_00865020, ported as `ecs::hand_mesh_ray` (2026-10-09, see
    [The hand's mesh test](#the-hands-mesh-test-fn_00865020));
  - the hand's mesh test: (inferred) the temple heart's mesh (its vt +0xF8, the blend of two `B_TEMPLE` meshes) keeps
    the status shell of sub-mesh 0, so that the hand stands on the shell over the heart, as openblack now does; the
    heart's bake of its vertices with its matrix ([magic.md](magic.md)) and whether the matrix it is then tested with
    is still the 3D object's have not been read. A mesh with bones is tested in openblack at rest (its bone chain), the
    original with each vertex group's matrix; for objects other than the creature that matrix array is the 3D object's
    +0x14, not read further. The original's extended precision is approached with doubles. Which objects the original
    lifts to the surface instead (vt +0x1AC of the 3D object, not an AnimatedStatic) has not been compared with
    openblack;
  - the units of the cursor search: the original compares view depths (w) everywhere, openblack distances along the
    ray. Between meshes on one ray from the eye the order is the same; it is not where a fixed length is added or a
    distance is not a depth (the land's + 2.3 is compared in depths already, `ecs::hand_cursor_depth`): the villagers'
    |centre - camera| - (R + near), the teleport stone's and the creature's distances, and the bubble's camera z
    against the object's (`HandSystem::Update`). The original's skip of a mesh whose sphere lies behind the pending
    hit stands as openblack's ray-against-sphere reject (both only skip meshes that cannot be nearer);
  - the press fallback's reach: how far the land behind the hand lies from the land under the cursor in the original
    (it sets how much of a forest's ground a press reaches) has not been measured against openblack's
    `UpdatePointBehindHand`, nor has it been checked in a run that a press between Land 1's trees takes the nearest tree;

- **The barn's "Did you know?" sign on Land 1** was not under the cursor at any screen point tried while checking the
  sign tap (2026-10-08), where the sign by the fence was. Whether its pick (its mesh, its height or the land under it)
  differs from the original's has not been looked into.

- **The hand on a creature** ([The hand on a creature](#the-hand-on-a-creature)), what C29 left out:
  - the interaction camera (fn_0044A850's camera mode, `PopViewMode`); the timer is ported without it;
  - the tap-or-lock wait (action state 19, 225 ms) with the leash in the hand or tied to this creature: needs the leash taps;
  - `Chold_fingers` and the leash pull (action state 21): the hand over the leash rope needs a pick of the rope;
  - the CREATURE state's own clip (state +0x114), its zoomers and `StartImmersion`: `HandStateCreature::Update` 0x5B17B0 not traced whole;
  - packet 0x63 (the creature's life −0.01, fn_004866F0) and when Update sends it;
  - `IsReadyForNetworkUnfriendlyEndLockedSelect` 0x476F00 (state 5's wait, and "no 0x59 when ready" at 0x46CFF4): it reads the 3D body's state; openblack takes the creature as ready one frame after the release, so the feedback always reaches the mind (approximate). An RE request: fn_004806D0 and the look-destination test;
  - the creature's +0x3CC in the state-7 guard, +0x10AC (19 against 14), Flags bit 0x400, and the interface's +0x37C[player] and +0x370: not named;
  - the creature is picked by capsules round its posed bones, not by its drawn, posed triangles (our creature has no CPU-skinned triangles).

- **A box takes the mouse** ([A box takes the mouse](#a-box-takes-the-mouse-the-escape-menu-a-dialog)):
  - the HistoryBox's and the GatheringBox's `WantsMouseControl` (only over a control): neither box is in openblack, so
    `input::BoxTakesInput` has no such case;
  - the ends of the action states 8 to 11 (fn_005D27B0; the packet 0x1A with the held seed) and 22, 23 (the packet
    0x24), and the clearing of the objects under the hand, are not in `HandSystem::EndAction`; the
    creature's locked select ends through openblack's own `Release` (not the object's network-unfriendly calls);
  - while a box is up the original skips the action's processing (the hover, the near object); openblack's hand
    update still runs, with no button held.
  - to read: `ProcessFrameInputs`' demo gate (0x54C390, no `CMouse::ProcessButtons` while `IsPlayBack`) against the
    box pass: whether the original applies the box rule while a script's hand demo plays (fn_005D9A20 takes its
    playback branch fn_005DAEE0 first, then fn_005D1120 at 0x5D9B1C only when no message was dispatched).
- The hand passes through some rocks when grabbing them: when it happens is still unknown (needs a screenshot). See [Grabbing](#grabbing).
- The packet handlers 0x15 / 0x16 / 0x17: one reading gives 0x5DBFB0 / fn_005DC060 / 0x5DC0D0 (objects-and-resources.md agrees on 0x5DBFB0), another 0x5DBF40 / 0x5DBF70 / 0x5DBF90. Check which is right.
- Hand demos: the recorded throw block (record +0x04..+0x33, CHand +0x48C8) is not applied by openblack; finding the target object of an ACTION message again by script type and subtype within 3 m (fn_005DB4B0, FindNearPos 0x6F7280, fn_005D5E40).
- `HandDemo` `Record::trigger`: "+0x5C the byte [0xE853AD] when it changed since the last record, else 0 ((inferred) the space key)". That [0xE853AD] is the space bar is not verified.
- Tooltips still missing in openblack (the mouse-button icon, `mousehelp.raw`, with the panels and the arrows of
  DrawKeyOrMouse is drawn: `Renderer::DrawKeyOrMouse`, for the tooltips, the help's click icon and the "did you
  know" pages):
  - the storage pit's 0xEF9 (two numbers) and the buildings' and town's texts of state 18;
  - the seeds' 0xE81 / 0xEF1 (state 5): "(pending) the seed's ValidToApplyThisToObject / ToMapCoord (0xE81 «Lanzar», 0xEF1)" — fn_005D6C10;
  - OneOffSpellSeed's own pick-up text: "(pending) its MagicEffectInfo +0x110 / the table 0xD9D7FC" — a one-shot orb's GetOverwritePickUpToolTip;
  - SpellIcon's tap text: "SpellIcon 0x726420 (pending: its info's +0x184)";
  - "(pending) a SpellIcon's GetOverwriteTapToolTip forced (0x5D74E0), a built Workshop 0xEF3" — state 18;
  - "(pending) IsTotemStatue vt 0x1E4" — state 18's TownCentre test.
- The store: the storage pit's Supply help trigger is still a log line (`take_resource::TriggerSupplyHelpIfThrownByMe`).
- `GInterface::SetActive` 0x5CEDC0: "(pending) its other effects: HelpSystem+0x460C = bit 0 of GInterface+0x39 (0x5CEDDF), +0x40 &= ~4, fn_005D1260, ResetActionState 0x5D29C0 and fn_005D81C0 (StopAllImmersion)". Their meaning beyond the offsets is unknown.
- `tooltips::Reset`: "(inferred) a new land starts with no tooltip; the show counts stay (0xD163C0 is a dword array capped at 0x50, 0x5C9C32)". Unverified who calls it when a land is loaded.
- `tooltips::Frame`: it is not known whether the original processes the help (HelpSystem::Process) while paused outside the citadel. Inside it, it does: `Temple::ProcessGameTurn` runs it every 100 ms ([audio.md](audio.md#the-citadel-interior-in-openblack)), and `tooltips::Frame` keeps the icon there.
- `IsActionBlocked`: the mapping of actions 3..19 / 17..19 to specific camera actions is inferred from openblack's bit order, not from a table in the exe.
- "(pending) the game runs F = 1, {-side, up', up' x side}" — the held object's matrix fn_0046E2F0 with CHand +0x484C.
- "(pending) a seed's info +0x154" — GetHoldYRotate (vt 0x598) of a seed.
- "(pending) the scale S = 3.2 / +0x8C x +0x4834 x +0x90 of fn_0046D080 that fn_0046D9D0 puts in front" — the held object's matrix.
- "(pending) with |camera - CHand +0x78| in HOLDING / GRAIN" — the frame's last SetDistanceFromView at 0x5B499B.
- "(pending) over an object, 0.25 toCam - hitNormal + (0, 0.5, 0) (with HandShouldFeelWithMeshIntersect vt +0x59C, and the hand higher than 1.6 hs above the ground); the rotate tricon." — the empty hand's up (ORHP's tail).
- "(pending) the WorshipTotem branch (the sacrifice of ApplyThisToObject 0x752C40 up to 0x752FB0) is not ported" — Villager::ValidToApplyThisToObject.
- "(pending) the altar test IsSacrificeAltar vt 0x4B0: no sacrifice altar in openblack, so 6" — HelpProfile::Trigger in the 0x11 handler.
- "(not verified) fn_005D3680" — the 0x11 handler 0x5DA1A0.
- "(not identified) the call 0x76CD40 on status +0x130 (0x5DA46B)" — the 0x12 handler 0x5DA400.
- "(pending) two flags, named by bw1-decomp's (fabricated) enums: GameThingWithPos +0x24 & 4 UNAVAILABLE_FOR_STATE_CHANGE (0x5DA726) and & 0x20 LOCKED_SELECT (0x5DA75C)" — PlaceObjectInMagicHand 0x5DA6F0.
- "(pending) the other re-entrant classes (Field, FishFarm, FieldCrop, PileResource, MagicFireBall); FireEffect:: StartedMoving on the object's fire (0x5DA79F)" — PlaceObjectInMagicHand.
- "(pending) the reaction 0x10 (0x5DA7DD); HelpProfile::Trigger 2 / 3 in PickUp" — PlaceObjectInMagicHand's success path.
- "(not verified) The test of the object's byte +0xA bit 1 (0x5D487E)" — state 3's end 0x5D4870.
- "(inferred) openblack has the one hand, so its +0x3C" and "(inferred) a hand already holding something refuses it: state 3 begins only with an empty hand (ActionPressed)" — the 0x1B handler 0x5DA950.
- "(inferred) on a new land, with game_packets::Reset (GInterface::SetToZero 0x5CE4D0's callers not traced)" — GInterfaceStatus::SetToZero 0x5DBA00.
- "(pending) vt 0x704 of the other classes (the villager's), as in RemoveFirstFromHand" — RemoveFromHand 0x5FB0B0.
- "(pending) CHand::GameTurnUpdate 0x46E4E0 (step 28) also drops it the same turn (IsAvailable != 1 -> CHand::ThrowObject), for a deletion after step I" — a deleted held object.
- "(not verified) a release during the wait (state 7, State_WaitPickup 0x5D4A90 only watches the object's +0x24 & 4)" — GenericPickup.
- "(not identified) the exception [0xD47824] && +0x4904 == [0xD47828] (0x46CA3A..0x46CA4E)" — the held object's blended place in PrepareForDrawing.
- "(pending) CAMERA 2 when the camera mode is CameraModeNew3 with +0x8C & 1 (0x46CEFA..0x46CF3B)" — CHand::GetRequiredState.
- The camera's edge rotate writes the ring cursor to CHand +0x486C / +0x4870 with +0x4874 = 1 (0x45D24A..0x45D259); what the hand does with it is not read, and openblack's hand does not read the camera's warp (`GetCursorWarp`).
- The camera's clear view (Ctrl + Shift) calls `Morphable::SetPos(CHand, target)` with the object under the hand each frame (0x45B568), and `HandStateCamera::Update` 0x5B05F6 changes the hand while it is above 0.01 unless HandStatus == 3: neither is read nor ported ([script-camera.md](script-camera.md#player-camera-the-mouse)).
- "(pending) unmirrored here" — openblack draws the hand unmirrored; the mirrored CHand (+0x484C) and its left-handed mode are not ported.
- "(inferred) the shadows and the fire, which go through DrawnModel (FireGraphic.cpp), take the stretch too" — HandDrawPose's tug stretch (HandStateTug::Update 0x5B891D..0x5B892B, at most 1.3).
