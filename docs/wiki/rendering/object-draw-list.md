# The object draw list

## Summary

Each drawn frame the original decides which objects have their Draw called through one list of up to 2999 objects:
the land pass finds and sorts the visible blocks, on some frames the list is rebuilt from those blocks (on request,
every 11 turns, or when a block's land visibility changes), a pass calls Draw on every listed object (or, with the
camera still, only on those that were on screen at the last full pass), and each Draw reports whether its object was
on screen. Anything an object does inside its Draw (a particle step, a light map, a pose) therefore happens only on
the frames the list reaches it.

openblack runs a logic-side copy of the list with all three of the original's rebuild triggers; the renderer draws
only what the list drew, in the list's order. Its first consumer is the script highlight (its pose, its glints, its
active effect and its sparkles). The visible blocks and their level of detail (stage A) are in
[Visible blocks and level of detail](visible-blocks-and-lod.md#stage-a-the-visible-blocks).

## The original

### The object draw list (stage 4m)

fn_005E5CD0 decides, each drawn frame, which objects have their Draw (vt +0x610) called. In four steps: the land pass
finds and sorts the visible blocks (A); on some frames the list is rebuilt from those blocks (B); a pass calls Draw on
every listed object, or only on those that were on screen at the last full pass (C); and each Draw tells the list
whether its object was on screen (D). Anything an object does inside its Draw (a particle step, a light map) therefore
happens only on the frames the list reaches it.

#### Reference executable

- This section reads the copy that runs: `Black and White Complete Collection\runblack.exe`, sha1
  `1e73ba9a96c3d6316e9f8f41a8947a25790d2698`. That copy is the one reproduced, and the one the checks measure.
- bw1-decomp's W120 build (`orig/BW1W120/runblack-decrypted.exe`, sha1 `bccccbd3a08fe9a5d6cf59cd114e916401abfb51` in
  its `config/BW1W120/build.sha1`) only guides the reading. Where the two differ, the running copy wins.
- Known differences: the running copy has nop runs where code was removed. They are in fn_00804E60
  (`0x804E60..0x804E68`, `0x804E72..0x804E7A`), in start_system (`0x642FF8..0x643003`, `0x643009..0x64301E`), at
  `0x823810` in fn_008237B0 and at `0x823B43` in fn_00823AD0 (6 bytes each, where an `fstp` of the LevelOfDetail value
  × 0.0001 would fit; see [The model level of detail in v1.42](visible-blocks-and-lod.md#the-model-level-of-detail-in-v142)).
  What the bw1-decomp build has at those places was not compared.

> **Which exe this is.** These bytes come from the community **Infinite Drawing Distance** patch, which the original used here includes: the plain published v1.42 (sha1 `9c9aea96…`) keeps both `fstp`s at 0x823810 and 0x823B43, and holds 700, 5 and 1 at [0xC37200], [0xC37EC8] and [0xC3813C] instead of 100000. The patched exe stays the original (user, 2026-10-17); see [Reading this wiki](../project/reading-the-wiki.md).

#### Where it runs in a frame

- `GGame::Loop` calls ProcessGraphicsEngine at `0x54D3BE`. The call is skipped while the window is minimized
  (`0x54D3B8`).
- ProcessGraphicsEngine runs, in this order:
  1. `GCamera::Update` (`0x54D879`). It writes the near clip `[0xE839E0]` (at `0x4424B4`), `tn [0xC3812C]` (at
     `0x4424D6`), and the camera `g_camera [0xEA1DB8]` and its focus `[0xEA1DC4]` through `UpdateCamera` (`0x442622`).
  2. `PreDrawProcess` (`0x54D887`).
  3. `Process3dEngine`, which calls `GLandscape::Draw` (`0x54DEDE`). That calls the land pass fn_007FF610
     (`0x5E4E96`), then the object list fn_005E5CD0 (`0x5E4EDD`).
- Each link of that chain has exactly one caller, and GLandscape::Draw has a single `ret`, at its end. The only call to
  fn_007FF610 that can run is `0x5E4E96`: the other two (`0x822774` and `0x822992`) are in unnamed code with no
  references, which the symbol table wrongly gives to fn_00822560.
- The game turns run before ProcessGraphicsEngine. PhysicsObject::GameTurnUpdate runs only from ProcessTurn
  `0x54E67E`; its call in Process3dEngine (`0x54DAB0`) is dead, because the byte `[0xD46A74]` is static 0 and never
  written.
- The world, and so the list, is skipped:
  - when the mode `g_game+0x205A28` is not 0. Mode 1 is the temple (Temple::Draw), mode 2 the falling-spell film
    (FallingSpell::Draw). The mode is set to 0 at `0x54FCA4`, `0x553B1F` and `0x553A1C`, to 1 at `0x554004` and to 2 at
    `0x5539D5`;
  - while a fullscreen film is up: a film exists, its alpha `[g_game+0x250194]` is exactly 1.0f and FallingSpellVideo
    `[0xCD3B10]` is 0 (`0x54DD56..0x54DD7D`). During the film's fade-out (`0x54DB27..0x54DB7F`) the world does run;
  - in the intro, logo and loading loops, which have their own StartFrame loops (`0x642820`, `0x642A4A`, DoLogo,
    RenderLoadingFrame) and never reach GLandscape::Draw.
- RenderLoopEnabled `[0xBEC280]` gates the world draw, but it is static 1 and never written.
- Pause does not stop the list. Paused, the turn does not move, so only the rebuild count and the land flag can force a
  rebuild. Inside fn_005E5CD0, pause only skips the influence ring's crossing (`0x5E61A6`).

#### Stage B: rebuilding the list

```
rebuild = RebuildCount[0xBF358C] != 0
       || turn[g_game+0x205A40] > [0xD2019C] + 10      // unsigned ja: with no other trigger, every 11 turns
       || [0xC37208] != 0
if (rebuild) {
  [0xBF3590] = 1                                       // 0x5E5D6C
  if (RebuildCount) RebuildCount--                     // 0x5E5D78: a 2 gives two rebuilds in a row
  [0xD2019C] = turn                                    // 0x5E5D8E; static 0, only written here
  for i < DrawObjectCount: if (DrawObjects[i]) DrawObjects[i]->+0x24 &= 0xFEFF       // 0x5E5D9A..0x5E5DB9
  DrawObjectCount[0xD20198] = 0
  for (b = [0xFA92D8]; b; b = b->+0x9B8) {
    if (!(b->+0x9BC < VanishObjectDist[0xC37200])) break          // a break, not a skip; NaN does not break
    gb = GameBlock table[0xD189D8 + 4*((b->+0x914 << 5) + b->+0x918)]
    if (gb) for k in 0 .. gb.count-1: TryAdd(gb.arr[k])           // count +0, array +8
    for k in 0 .. [0xD199E0]-1: TryAdd(Global[0xD199E8][k])       // inside the block loop
  }
}
TryAdd(o):
  skip if count == 0xBB7 (2999)                // a skip, not a break: later objects are dropped
  skip if o->+0x24 & 0x100                     // already listed
  skip if o->+0xA & 1                          // unavailable
  skip if o->+0x40 && o->+0x40->GetDontDraw()  // vt+0x74 on the Game3DObject
  DrawObjects[0xD1D28C + 4*count] = o; o->+0x24 |= 0x100; count++
```

- The turn `g_game+0x205A40` goes up only in StartTurn `0x54E507`, and only when not paused (`+0x14 & 4`,
  `0x54E4FD`).
- VanishObjectDist is static 100000.0f. Its only writer is SetVanishObjectDist `0x87FD44`, from the
  SET_VANISH_OBJECT_DIST script command, which passes float(int)². No shipped file was found with that command's name
  (packed files would hide it). At 100000 the vanish test never cuts anything: the land's diagonal is about 7241.
- Because of the 0x100 bit, the global list is in effect added once, right after the first block that passes, and not
  at all when no block passes. So the list is: the blocks nearest first, each block's array in order, and the global
  list after the first block.
- The skip at vt +0x74 is GetDontDraw: `+0x40` is the Game3DObject (bw1-decomp Object.h:69, LH3DObject.h:99). The
  disassembler's GetFootpathLink label for that slot is wrong.
- **Which block an object is in.** GameBlock::Insert `0x5DDCE0` uses `table[((cellX>>4)<<5) + (cellZ>>4)]`:
  - an index outside 0..31, or a null slot, sends the object to the global block `0xD199E0`;
  - slots exist only where `g_index_block` and `g_ptr_blocks` have a land block (fn_005DDC00, `0x5DDC3D..0x5DDC66`);
  - off-map objects are in no list at all: ToMap `0x603430` returns null, and Object::InsertMapObject skips them
    (`0x636750`);
  - the callers are `0x52DEC9` and `0x52DF09` (Fixed) and `0x6368C0` (Object), once for every cell the object is filed
    in. Removes come only from Object::RemoveMapObjectFromCell `0x636A24`.
- **The order inside a block.**
  - Insert `0x5DDDC0` appends `arr[count++] = obj` and sets `+0xC = obj` (`0x5DDE45..0x5DDE4F`). It first sets
    `+0x10 = 0`, then returns at once if `obj == +0xC`, the last object inserted (`0x5DDDC8`); there is no other
    duplicate check. A full array grows by 0x14 and keeps its order (`0x5DDDD4..0x5DDE41`).
  - Remove fn_005DDE60 swaps the last entry into the hole. It first sets `+0xC = 0`, then returns at once if
    `obj == +0x10`, the last object removed. With count 1 it clears `arr[0]` without checking which object it is
    (`0x5DDE73..0x5DDE85`). Otherwise it searches from index 0 and, if the match is not last, does
    `arr[idx] = arr[count-1]` (`0x5DDEB1..0x5DDEB7`). It always does `count--` (`0x5DDEBA`), even when nothing matched.
  - A moving object is swap-removed and appended again at every cell change.
  - Nothing sorts the arrays. GLandscape::Open creates the table (`0x5E5531`), ClearMap's GameBlock::Clean zeroes the
    counts (`0x552F1D`) and GLandscape::Close frees it (`0x5E52CB`).
- **The other writers of bit 0x100 and of the list:**
  - the Object constructors set the whole word to 0 (`0x63648B`, `0x636577`); CreaturePhysical's constructor writes a
    constructor argument there (`0x4EF451`);
  - GameThingWithPos::Load clears the bit (`0x57052F`);
  - in GGame::ProcessKey, LH key 1 in single player with TutorialState `[0xD019A4] != 0` clears the bit on every
    non-null entry and sets the count to 0 (`0x63F07E..0x63F0BD`);
  - Reward::Process, when `+0x8C` is set, appends itself to DrawObjects with the same tests and sets 0x100
    (`0x6E68FD..0x6E6947`);
  - two writers leave the bit set: ClearMap zeroes the entries and the count but not the flags (`0x552E9A..0x552EA3`),
    and EndTurn nulls DrawObjects[i] and DrawObjectActive[i] for unavailable entries (`0x54E9F9..0x54EA18`). An object
    nulled this way keeps 0x100; the next rebuild skips null entries, so it is not listed again until a reload or the
    tutorial key, or never.

#### Stage C: the draw passes

```
[0xEC81BC] = count                                     // statistics only
full = [0xBF3590] != 0                                 // a rebuild this frame: P and F are NOT updated
if (!full) {
  // P [0xD1A350] and F [0xD1A340] are statics, zeroed on first use (guard 0xD1A35C)
  if (|g_camera[0xEA1DB8] - P|^2 > [0xBF3594] (1.0) || |focus[0xEA1DC4] - F|^2 > 1.0) {   // test ah,0x41
    P = g_camera; F = focus; full = true               // <= 1 or NaN counts as still; drift adds up against P/F
  }
}
if (full) {
  [0xBF3590] = 0                                       // 0x5E605B
  for i in 0 .. count-1:
    o = DrawObjects[i]
    if (!o) { Active[i] = 0; continue }                // Active = DrawObjectActive 0xD1A3AC
    g_b_last_on_screen[0xEA1AF0] = 1                   // 0x5E6077
    if (o->+0xA & 1) { DrawObjects[i] = 0; Active[i] = 0; continue }
    o->Draw()                                          // vt+0x610
    Active[i] = g_b_last_on_screen ? 1
              : (o->+0x40 && (o->+0x40->IsHuman() [vt+0xB0] || o->+0x40->IsComplex() [vt+0x1A8])) ? 1 : 0
} else {
  [0xEC81BC] = 0
  for i: if (Active[i]) {
    o = DrawObjects[i]
    if (!o || (o->+0xA & 1)) { DrawObjects[i] = 0; Active[i] = 0 } else o->Draw()
    [0xEC81BC]++
  }                                                    // the flag is not reset and Active is not recomputed
}
```

- In the full pass a null entry is tested before the flag is set: the flag is set to 1 only for a non-null entry
  (`0x5E606E`, then `0x5E6077`).
- The slots vt +0xB0 and vt +0x1A8 are on the Game3DObject `+0x40`, so they are IsHuman and IsComplex
  (LH3DObject.h:174-176); the disassembler labels them CastAbode and GetImpressiveType.
- Who answers 1: no LH3DObject vtable overrides IsHuman (`fn_007F9980`, `(Flags1[+4] >> 19) & 3`, in all nine vtables);
  its only writer is SetHuman (`fn_007F9990`, vt +0xB4), called on a game object's +0x40 only from
  Villager::CallVirtualFunctionsForCreation (`0x74FD3C`), so **IsHuman is 1 for the villagers and the special
  villagers** (not PieceVillager or PuzzleVillager, which take Animal's). Only the COMPLEX vtable (`0x9A3068`) overrides
  IsComplex (`fn_0080B9F0`, returns 1); the default `Object::Get3DType` (`0x6364F0`) never returns COMPLEX, and the game
  objects with a COMPLEX +0x40 are **the Creature** (`0x474B24..0x474B42`, from Morphable::MorphInit `0x617315`) and
  **the HelpSpirits** (HelpSpirit::Create `0x5C4B30`, the object fn_005C0F30 builds at `0x5C0FD5`). Every other object's
  Active is its on-screen flag.
- g_camera and the focus are written by UpdateCamera at `0x819A1C..0x819A4D`, from GCamera::Update `0x442622`, before
  PreDrawProcess and the draw. In world mode outside playback, their values come from these steps, in order:
  1. the interpolated position and focus: the zoomers at `+0x118` and `+0x88`, after Zoomer::Update;
  2. a NaN coordinate is replaced by the last good value (`0xC59B48` for the eye, `0xC59B38` for the focus);
  3. if the eye and the focus are closer than √0.001, the eye moves by (−1, +1);
  4. the floor step: an eye less than 1 above the ground raises eye.y and focus.y by the same amount (skipped for
     CameraModeFree and when `+0x78` is set);
  5. the debug override `[0xEA9EC8]`, which is 0 in play;
  6. the shake fn_008210C0.

  CameraModeNew3::Update changes g_camera and puts it back (`0x45D3D4..0x45D489`).

#### Stage D: what sets g_b_last_on_screen inside Draw

The usual chain is SingleMapFixed::Draw `0x518100`, then `Game3DObject::AddForDrawing` `0x63B5D0` (at `0x518140`, or
through fn_00518050 at `0x518079` when `+0x44` is set), which calls vt +0x100 at `0x63B628`:

- vt +0x100 is fn_00815A70 in the STATIC (`0x9A2974`), MORPHABLE (`0x9A2E34`), ANIMATED (`0x9A32A0`) and CITADEL
  (`0x9A2BFC`) vtables, and in LH3DMeshedObject (`0x9A2748`). LH3DObject::Create `0x80B4D0` builds them (jump table
  `0x80B844`).
- COMPLEX (`0x9A3068`) uses fn_00813340, which calls the same test.

fn_00815A70:

```
if (GetDontDraw() [vt+0x74]) { g_b_last_on_screen = 1; return }                   // 0x815A8E
mesh = GetMesh() [vt+0xF8]
LH3DBoundingBox::CheckRegionOnScreen(&mesh->BoundingBox, obj)                      // 0x868C80
  // [0xEA9EB4] != 0: fn_007ACC60 -> fn_007AC640 (SSE); else the x87 body 0x868CA2..0x868FBC. Same logic.
  r  = box[+0x1C] * obj.scale[+0x44]
  W  = c.x*R0 + c.y*R1 + c.z*R2 + T     // c = box+4 (local); rows +0x14/+0x20/+0x2C carry the scale; T = +0x38
  (cx, cy, cz) = W through g_world_to_clipping 0xEA9E40 (translation +0x24); g_last_distance[0xEA1AF4] = cz
  flag = 0                                                                         // 0x868CB2 / 0x7AC665
  if (cz + r < near[0xE839E0]) return                                              // 0x868D90 / 0x7AC785; no far plane
  if (|T - g_camera|^2 < r^2) { SetNeedClipping(1); flag = 1; return }             // around the ORIGIN T, not W
  sx = (cx/cz + 1) * W/2;  sy = (1 - cy/cz) * H/2                                   // W, H = [0xE839E4], [0xE839E8]
  rr = r*W / (2*cz*tan(fov/2))                                                     // tn [0xC3812C] = tan(fov/2)*near
  if (sx+rr < 0 || sx-rr > W || sy+rr < 0 || sy-rr > H) return                     // edges count as on screen
  flag = 1; [0xC37EA0] = r                                                         // 0x868F09 / 0x7AC938
// then the LOD and distance cull:
L = min((importance + 1) * r * [0xC3813C], 100000)    // [0xC3813C] = detail, clamped to 0..1 (0x5E7919)
// cz against 23.3L, 66.7L, 86.7L and 173.3L picks LOD 1, 2 or 4 (3 is never set); alpha fades between 86.7L and 173.3L
if (cz >= 173.33*L && IsDisappear [+4 bit 3, vt+0x9C] && !IsHuman) flag = 0      // 0x815EED
```

- The SSE form computes 1/cz with an approximate reciprocal and one refinement step; the x87 form divides. In the
  camera-inside compare, a NaN takes the inside path in SSE and the frustum path in x87.
- The constants at `0xFC0710`, `0xFC0714` and `0xFC0718` are 0.0, 0.5 and 1.0, static, with no writer.

The near clip `[0xE839E0]`:

- It is 0 in the image; RenderInitialization `0x818F99` sets it to 3.5.
- In play, GCamera::Update stores LandFeature::GetNearClipping `0x5E2F30` at every update (`0x4424B4`), in every mode:
  - 0.1 while `[0xD1A2F8] != 0`. Only SET_GRAPHICS_CLIPPING sets that flag (`0x708C77`), and
    CleanGameForScriptReboot clears it (`0x6EBBEF`);
  - otherwise, with `h = g_camera.y - GetAltitude(ftol(x*65536*0.1), ftol(z*65536*0.1))`: 0.3 if h ≤ 0 or NaN, 3.5 if
    h > 20, otherwise `(h*0.05)*3.2 + 0.3` as a float.
  - GetNearClipping reads g_camera before this update's UpdateCamera (`0x442622`), so frame N's near clip uses frame
    N−1's drawn eye, shake included. A turn-side UpdateCamera call in between changes that eye: playback (`0x5DB120`)
    and the power-up icon camera's restore (`0x68B7DA`).
- The other writers: CameraModePath::SetUpNearClipping `0x460F24` sets 0.1, but its only constructor call is turn-side
  (`0x6C8792`), so `0x4424B4` overwrites it before the draw; CameraModePath::Cleanup `0x460F69` restores the saved
  value; InnerCamera::PreDraw `0x79693C` sets 0.2 (temple); RunAlexFX `0x5F8FE1` sets 0.1 (start logo).

The game's own Draw overrides also write the flag: ChessPion (`0x422863`, `0x422A62`), TownCentre (`0x5164F5`,
`0x5164FF`), OneOffSpellSeed (`0x518E75`, `0x51908B`), WorshipTotem (`0x5193BB`, `0x780EEC`), DrawSpellGraphic
(`0x51A7C4`, `0x51A7DF`, `0x51A7FA`), fn_0051BAF0 (`0x51BB3E`), LandscapeVortex (`0x5FFDFD`, `0x5FFF70`), ScriptHighlight
(`0x70A29E`, `0x70A2B0`), TotemStatue (`0x738E98`) and fn_0074B3A0 (`0x74B5E8`).

#### The script highlight in the list

- Vtable `0x94228C` (constructor fn_007098A0, `0x7098CB`). Slot +0x610 is ScriptHighlight::Draw `0x709C60`, slot
  +0x658 its CallVirtualFunctionsForCreation `0x709AA0`.
- Creation: Create `0x709A40` calls CallVirtualFunctionsForCreation, which calls SingleMapFixed's `0x52E880`, then
  Object's `0x636BE0` (which skips the insert if `Flags & 4` or `+0xA & 0x11`, `0x636CA5..0x636CB2`), then
  InsertMapObject (`0x52E620`, `0x52E530`, `0x636740`; Flags |= 1), then InsertMapObjectToCell (`0x52F440`,
  `0x52DEA0`), which calls GameBlock::Insert at `0x52DEC9`.
- So the highlight is appended to the block `(cellX>>4)*32 + (cellZ>>4)`, or to the global list when that block has no
  land, before its sprite, 3D object and glint effect are made (`0x709C0E`). Its flags start at 0 and it has no
  RebuildCount writer, so it is listed only at the next rebuild.
- A highlight that is not a DidYouKnow one calls `+0x40->SetDisappear(0)` at creation (`0x709ACE`), so the distance
  cull can never clear its flag.
- Draw:

```
if (+0x5C || !(g_game+0x250090->+0x80 || IsDidYouKnow()) || !+0x40) return   // 0x709C8B/0x709CAE/0x709CB9 -> 0x70A44C
                                                                              // the flag keeps the list's 1: Active = 1
SingleMapFixed::Draw()                     // the on-screen test of +0x40, as in stage D
saved = g_b_last_on_screen                 // 0x709FD4..0x709FDA
edi = 1 if the +0x60/+0x68 branch ran (0x70A10A) or the +0x74 branch with +0x28 == 0xD966C0 ran (0x70A28D)
glint step                                 // 0x70A126
g_b_last_on_screen = saved || edi          // 0x70A292..0x70A2B0
```

- For a listed highlight's main object (+0x40), outside the early exits:

```
saved = DontDraw ? 1 :
        (cz + r >= near)
     && (|T - cam|^2 < r^2  ||  the disc (sx, sy, rr) overlaps [0,W]x[0,H], edges included)
     && !(cz >= 173.33*L && IsDisappear && !IsHuman)
```

- Its glints step once in every frame in which the highlight is listed and either the pass is full, or Active[i] was 1
  at the last full pass. Active[i] is `saved || edi`, or 1 after an early exit. The glint step itself is in
  [the glints on a target](../physics/particle-rules.md#the-glints-on-a-target).
- Its pose is set in the same frames only: after the early exits and before the on-screen test, Draw turns the 3D
  object (or faces the eye, for a lit did-you-know), reads the distance scale afresh and places it with
  LH3DObject::SetPosition (0x709CE8..0x709ED2, [script highlights](../interface/script-highlights.md#script-highlights)). Nothing per turn touches it. On
  a frame the list does not draw the highlight its 3D object keeps the last pose, and the turn does not catch up: the
  angle grows by the drawn frame's g_game_time_inc only. Before its first drawn frame it stands at its creation pose
  (Object::CallVirtualFunctionsForCreation 0x636BE0, SetXYZAnglesAndScale at 0x636C9F), with no distance scale.

#### Every writer of RebuildCount `[0xBF358C]`

RebuildCount is static 1. It has 20 references, 2 of them in fn_005E5CD0.

| Writer | Address | Value | Condition |
|---|---|---|---|
| CHand::ThrowObject | `0x46DE25` | 2 | The hand holds an object (`hand+0x4904 != 0`) |
| GGame::ClearMap | `0x552E2A`, `0x552EA9` | 1 | Always |
| HandStateTug::Update | `0x5B84AE` | 1 | The held object passes IsTree, the force and pull checks pass, and it also casts to Living. **🔍 inferred** unreachable: only Tree and MagicTree override IsTree, but the RTTI was not dumped |
| GLandscape::Draw | `0x5E4B89` | 1 | The SuperVillager list `[0xEB9A08]` is not empty but the widescreen condition fails; all of them are released, so this fires on the first world frame after a script's widescreen ends |
| PhysicsObject::GameTurnUpdate | `0x645F27` | 1 | The step result is 2 (came to rest, or forced sunk at `0x645A3E`), then EndPhysics |
| PhysicsObject::GameTurnUpdate | `0x645FE4` | 1 | The result is 3, CanBecomeAPhysicsObject, `!(obj+0x25 & 0x10)` and `InitialisePhysics().+4 == 1` |
| Reward::Create | `0x6E597D` | 1 | The 4th argument is not 0 (the reward goes into the map at once) |
| SpellFlockFlying::Process, SpellFlockGround::Process | `0x723F6F`, `0x724637` | 1 | Each animal spawned (`0x723ED8`, `0x72458D`) |
| GScript::SetCameraPos, SetCameraFocus, SetCameraToFaceObject | `0x6EC98D`, `0x6ECA83`, `0x6ED5EC` | 1 | The current camera mode casts to CameraModeScript |
| GScript::RestoreCameraDetails, SetCameraPosFocLens | `0x6EE3B0`, `0x6EE49C` | 1 | Always |
| GScript::SetHighGraphicsDetail | `0x708DF3` (on), `0x708E0C` (off) | 1 | Any thing found. "On" also sets the object's DontDraw and creates the SuperVillager (fn_00825F20, `0x708DD8`). "Off" calls `+0x40->vt+0x70(0)`, SetDontDraw(0) (`0x708E09`), right before its write, and releases it |
| Camera-control release (unnamed, from `0x6ECD70`) | `0x6ECEC4` | 1 | Always, once reached. Called from EndCameraControl `0x6ECF2C` when `GScript+0xA8` equals the task argument, and from `0x6ECF20` through GScript::Load |

- The write at `0x6ECEC4` is not in StartCameraControl, which returns at `0x6ECD6F`; the symbol table's size for
  StartCameraControl (0x250) is wrong.
- The widescreen condition is that help_system (`g_game+0x25005C`) has both `+0x45E8` and `+0x45EC` set. Only
  HelpSystem::SetWideScreen writes those fields, and only GScript::SetWideScreen (`0x6F7C5A`) passes a task into
  `+0x45EC`.
- MOVE_CAMERA_POS_FOC_LENS is not a writer. Its native GScript::MoveCameraPosFocLens `0x6EE4B0` (table entry
  `[0xC17D08]`, set at `0x705600`, 8 arguments) pops its arguments, makes its two error checks and, only in the script
  camera mode, calls MoveCameraPosition `0x4616F0`, MoveCameraFocus `0x461430` and SetCameraFov `0x443680`; none of the
  20 references to `0xBF358C` is in it or in those three. SET_CAMERA_POS_FOC_LENS, the entry before it, is.

#### Blended models drawn at once: their place among the objects

The original does not batch its objects: each `AddDrawing` (0x815A70) of an object whose mesh lacks the Z-sort flag
(0x200) draws it at once (`Draw` 0x80DB30) as the list visits it, so the frame's draw order is the visit order (the
blocks nearest first, each block's array in insertion order, the global list once after the first block). An object
drawn in the global-alpha mode (`SetDrawWithGlobalAlpha`, vt +0x48; mode 1: source alpha over inverse source alpha, **Z
write on**) therefore shows only what was drawn before it, and hides, behind it, the opaque objects drawn after it.
Its users drawn at once in the list: the village totem's second totem (before the totem's own plinth and icon, in
`TotemStatue::Draw`), the fading `SpellWolf` and `SpellDove`, `Scaffold::Draw` and `ScriptHighlight`; in the list's tail
(stage 4n): the falling roots, the souls and the ground marks. The physical shield and the spell icon's charge ring are
Z-sorted (their meshes have 0x200). The distance fade inside `AddDrawing` never triggers in play (both of its LOD
distances are 100000).

The list's tail after its passes (0x5E6135..0x5E62F6), each part newest first (every list is head inserted): T1 the
falling roots (list 0xEB9A10, 0x826290), T2 the objects walking a script's WALK_PATH (g_game+0x205CD4, vt+0x610: the
sharks' part above the water, `Whale::Draw` 0x774E10, only while they walk), T3 the souls (0xEB9A7C, 0x8287B0), T5 the
chests falling from the sky (0xD559A0, `Reward::ProcessDrawing` 0x6E64C0, 0x6E5869), T7 the ground marks (0xEB9A00,
0x825277); then (Game.cpp, decomp) the leashes and `PhysicsObject::DrawAll` 0x646DE0, the physics list's dense array
in its slot order, every awake entry (+0x19C clear) not wholly under the water (y +0xCC > -r +0x178).

An asleep entry (+0x19C, set when a resting proxy is added 0x644E90 or a body comes to rest 0x645F0E, cleared when
it is knocked 0x645FDA and by AddObject 0x644780) is back in the map and drawn by the list; an awake one is out of
the map (InitialisePhysics: RemoveMapObject), so one wholly under the water (y <= -r) is drawn by nothing.

The physics objects' projected shadows are updated by `fn_00646FE0` (once a frame, from `GLandscape::Draw` 0x5E49DC,
before the land), under the same two skips, through `fn_007FCE80` (the holder made lazily, then the update
`fn_00874850`); nothing resets a shadow entry, and the land pass draws every active one. So a sunk flying object's
shadow stays as it was last updated (its alpha too) until the object is deleted, and one that sinks before it had
one never gets one; an asleep object has released its holder (0x645EE5) before it is asleep. The building fragments
`DrawAll` draws (`FragMesh::Draw`) fall under the same skips, so a sunk one is drawn by nothing.

## openblack

Status: **done** for the list itself; the gate, the draw order and the sunk physics objects are in. The list runs on
the logic side and decides which models the renderer draws on each frame.

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Stage C: the draw passes**
  - openblack: see [How openblack runs the list](#how-openblack-runs-the-list)

### How openblack runs the list

openblack runs the list on the logic side: the pure stages in `src/ECS/DrawList/`, the state in the service
`ObjectDrawListSystem` (`Locator::objectDrawListSystem`), run by `UpdateObjectDrawList` in src/Game.cpp once per drawn frame,
after the hand demo has set the camera. It is not run inside the temple, under the falling-spell film or behind a full
screen film, and it runs while the game is paused. The script highlights have a Draw of their own, and so do the
town desire flags (`TownDesireSystem::DrawFlag`: the flag steps there, and only a flag that shows takes its mesh's
on-screen test; a hidden one leaves the flag set, [the desire flags](../beings/towns.md#the-desire-flags)). The flags
are in the map as any object (their cell's fixed list and their block's array, `map_cells::InsertMapObject`), and the
gate counts one as in the map even off it, as only the list draws a flag. Every other object's Draw is its mesh's on-screen test (`draw_list::DrawOnScreen`, src/ECS/DrawList/ObjectDraw.cpp: the box's
centre through the object's model, half the box's diagonal times the largest scale, and true with no mesh), which
sets its Active for the still passes. The objects each pass drew are kept (`ObjectDrawListSystem::DrawnThisFrame`). Each
frame `draw_gate::FillFrame` (src/ECS/DrawList/DrawGate.cpp) turns them into the objects the list leaves out: in the
map, not DontDraw, and not drawn by the pass (`graphics::ObjectListFrame`, carried to the renderer in the frame's
snapshot). `Renderer::PreDraw` makes a mask of their instance rows, and the object draw skips them (the gate below).
`components::Villager` answers IsHuman and `components::Creature` IsComplex (src/ECS/DrawList/RegistryObjects.cpp);
the advisor spirits are drawn by the help system and are not registry entities, so they never reach the list.

- **Stage A.** The cull, the distance and the sort over `LandIslandInterface::GetBlocks()` in the land's order. The
  camera is `draw_list::MakeDrawCamera` (src/ECS/DrawList/FrameInputs.cpp): `affine::FrameMatrices` from the drawn eye
  and focus, the horizontal field of view and the aspect; half the viewport; the clamp (W − 1, H − 1). The near clip is
  `NearClipFor` of the previous frame's drawn eye over the land read at the hand's quantised point, 0.1 while
  SET_GRAPHICS_CLIPPING is on. The GPU's own near plane is not changed: it still follows the current eye and the
  continuous height.
- **The level of detail and the land flag.** Before the cull the two lines are rebuilt from the drawn eye and the view
  direction, which is the third column of the frame's world-to-camera matrix, as `[0xEA1DD4..0xEA1DDC]` is
  (`DrawCamera::viewDirection`). The cull gives each visible block its level against them, then the seam pass
  (`MarkSeams`) looks up each block's neighbours by its place on the block grid. The land pass then runs
  `KeepsFrontTriangle` for each visible block, nearest first, on the block's cell records as the land holds them that
  frame. The block's triangles at its level of detail, in the order they are emitted, with the altitudes its points
  take after the blend, are `BlockTriangles` (points of the 17 × 17 grid of cell corners, each square's triangles
  with the corner they leave out, then the seam triangles), which the land clip tests in order. `TakeLandClipResult` writes the result into bits 0 and 1, and a change of bit 1 raises the land flag that
  `RebuildDue` reads. A block without its cells keeps its bits, and so does one the land clip gives no answer for. In
  the game the second case cannot happen: the cull gives only steps 0 to 2, the seam pass only bits 0 to 15, and the
  frame always has its clamp.
- **Stage B.** The block arrays live beside the map cells (`MapCellsSystem`, `draw_list::GameBlocks`), written at the
  map cells' insert and remove points. The listed mark is the component `DrawListed` on the entity: it goes when the
  entity is destroyed, so an index made again (a new entt version) is not taken as listed. DontDraw is
  `components::DontDraw`, which SET_HIGH_GRAPHICS_DETAIL writes. An object is available while its entity exists. The
  RebuildCount writers publish `events::DrawListRebuildRequested`.
- **Stage C.** The full and the still passes; a destroyed entity's entry is nulled when a pass meets it, which stands for
  EndTurn's nulling.
- **The gate in the draw.** An object is drawn on a frame only when the list ran that frame and its pass called the
  object's Draw (stage 4m): in openblack, an entity with a mesh is left out when the list ran, the map files it
  (`map_cells::IsObjectInMap`), it is not DontDraw, and it is not in `DrawnThisFrame`
  (`ObjectListFrame::LeftOut`). A wood or food pile sunk wholly into the land is left out too, drawn or not, as its own
  Draw draws nothing then (`draw_gate::PileBuried`, [the piles' drawn height](../world/terrain.md#where-each-class-takes-its-drawn-height)).
  `Renderer::DrawPass` applies it at five places: the posed boned models, the batched
  opaque models (one draw per run of drawn rows, `instance_runs::Runs`, and the shadows over an object only for a
  drawn one), the Z-queued models, the fading models, and the villagers' and animals' ground blobs (drawn by their own
  Draw). `EngineConfig::drawFollowsList` (on by default; Land Island > Object draw list, the switch "Draw only what
  the list draws") turns the gate off; the same node reads out the list and what it leaves out.
- **Never left out, as in the original:** what is drawn outside the list. The sky (4b), the reflected land (4c), the
  reflections in the sea of the hand, the held object, the physics objects and the ships (4d), the SuperVillagers
  through their own list (4e, 4n; DontDraw), the sea (4g), the land and the shadows cast on it (4i), the leash posts
  (4k), the water rings (4l), the leashes (6), the physics objects (7, not in the map), the hand and what it holds (8;
  openblack keeps a held object's rows, `RenderContext::sortedInstances`, out of the mask), the particles (13) and the
  weather (16). Nothing is left out on a frame the list does not run: the temple interior (its own pass), the falling
  spell film and a full screen film.
- **What the gate changes on screen.** The state is not touched: only the draw reads the mask. An object made after
  the last rebuild is not drawn until the next one (at most 11 turns, about 1.1 s, when nothing asks for a rebuild);
  an object of a block the list does not walk is not drawn; past 2999 entries the later objects are not drawn; and
  with a still camera only the objects found on screen at the last full pass are drawn.
- **The cycle (Land 1 → Land 2 → Land 1).** At each land load the map clear asks for a rebuild (RebuildCount 1, as
  ClearMap) and keeps the last rebuild's turn, and the turn starts again at 0. The first drawn frame of the new land
  rebuilds and stores its turn, so the turn rule runs again from there: no land freezes on the old land's turn, as in
  the original. The land clip's flag runs too (above), so a block that comes into view rebuilds the list at once, and
  the objects of a view turned back to are drawn on that frame instead of at the next rebuild by turns.
- **The turn at a land load (a separate fix to check).** The original sets the turn to 0 in GGame::ClearMap
  (GData::Reset `0x510750` through ResetState `0x5557A9`, from `0x552E62`), before the map script
  (GSetup::LoadMapFeatures) and Town::AsssignTownFeature run. openblack's `Game::LoadMap` calls `game_clock::SetTurn(0)`
  (through `Game::StartNewLand`, src/Game.cpp:4503) after `script.Load` (:4195) and `AssignTownFeatures` (:4208), so on a second or later land the
  previous land's turn is still in place while the map script runs. The value at the end is the same; whether anything
  reads the turn in between was not checked. The object draw list does not: it runs only on a drawn frame, and none
  falls between the map clear and the turn's reset. The last-rebuild turn `[0xD2019C]` is never reset (its one write is
  `0x5E5D90`): ClearMap's RebuildCount = 1 forces the first rebuild instead.

### The draw order of the models

- **The list's order.** `graphics::object_order` keys each instance row by the place of its object in the list's
  visit order (`ObjectListFrame::visited`), and a model drawn inside its owner's draw (`components::DrawnWith`: the
  totem's icon and its second totem) by its owner's place and a step of that draw. The renderer draws the opaque
  models in one segment of their ranges per blended model drawn at once, that model after its segment; with none on
  screen the opaque draw is one segment, as before. Opaque models are batched by mesh inside a segment (the same
  pixels but where two are at the same depth).
- **The tail.** `object_order::Band` (List, NotPlaced, Tail, AfterLeashes) and `TailOrder`; Game.cpp fills `ObjectListFrame::tail` (the roots, the souls, the falling chests, the marks) and `physics`; the sharks' tops are drawn in their segment, and the physics objects in a pass of their own after the leashes. What no part claims (`NotPlaced`) stays between the list and its tail. The View menu's Object Tail Unplaced switch draws the old order.
- **Blended physics objects.** ✅ (2026-10-20, issue #146) `PhysicsObject::DrawAll` 0x646DE0 draws each awake entry through its `DrawOutOfMap` (vt+0x614, 0x646EFC); the one-shot orb's (0x518C50) turns its bubble to the camera and queues it in the Z-sorter with `AddForDrawing` (0x518E0B), as its `Draw` 0x518E90 does on the land, so a moving orb keeps its bubble. openblack took the physics objects' rows out of the list's mask for their own opaque pass, and the depth sorted blended draw read that mask: a thrown or nudged orb lost its bubble while its seed went on drawing. `object_order::SplitPhysics` now gives the list's rows, the physics pass's and the queued blended rows (both, less the sunk ones); the View menu's Flying Blended Objects switch (on) leaves them out as before.
- **Sunk physics objects.** `ObjectListFrame::sunk`, those rows left out of every object draw; the View menu's Sunk Physics Objects switch draws them as before (and so does Object Tail Unplaced, which turns the whole order off).
- **Their shadows and fragments.** `shadow_math::PhysicsShadowStepOf`, a kept shadow `frozen` (not updated, drawn on). The damage-mesh pass (`BuildingDamageSystem::SnapshotDamageMeshes`) leaves out a fragment whose physics entry is awake and sunk.

The building fragments and the physics shadows of the original are described under
[Blended models drawn at once](#blended-models-drawn-at-once-their-place-among-the-objects).

### Code map

The source files of this page and the symbols their notes describe:

- `src/Graphics/RegionOnScreen.h`: `PointOnScreen`, `SphereOnScreen`.

### Test hooks

- Land Island > Object draw list: the switch "Draw only what the list draws" (`EngineConfig::drawFollowsList`, on by
  default) turns the gate off, and the same node reads out the list and what it leaves out.
- View menu: Object Tail Unplaced draws the old order; Sunk Physics Objects draws the sunk rows as before.
- The full list of variables: [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- Stage 4d: `fn_005DFCE0` (footprint UV2) and stage 4h (`fn_005FF310` → `fn_005FFBB0`, LandscapeVortex) are
  **🔍 inferred**, not yet read.

### Still undetermined in the original

- The x87 precision under D3D7: the device's FPU flags were not traced. In single precision the x87 path matches the
  SSE one; in double precision a back-face area A near 0 can differ, and the SSE path's approximate reciprocal can move
  an on-screen edge case. The level of detail depends on it too: the morph's fistp uses the current rounding mode, and
  every v-against-T compare depends on the precision.
- Bit 2 of `+0x920` (set with bit 0 by `|= 5`, cleared on reject): its readers were not looked for.
- The straight-down camera: when |f.x| and |f.z| are both ≤ 0.01, the lines keep the previous frame's p and an
  unnormalised n. Whether the camera can get there in play was not checked.
- Whether, in real play, a level change alone flips fn_00874AA0's result for some block (so that the level decides a
  rebuild frame) is not settled statically; it needs the per-block geometry.
- fn_0086BD00's `cmp [0xC381EC], 3` (`0x86BE3B`): what it gates was not read. Nor how the Options box's 7 detail labels
  (0xDC9..0xDCF) read.
- Not read, because they do not touch the land flag: fn_00869850 (the fog start and end each frame), the uses of
  `+0x92C`, `+0x938` and `+0x928` after `0x87539B`, and L2's exact value at Create (it depends on the VRAM count).
- Who writes g_world_to_clipping `0xEA9E40` each frame: an address scan finds only the `[0xEA9EA0] = 0xEA9E40` stores
  (for example `0x46A689`, `0x519979`, `0x5F9754`), not the matrix stores, which go through a register. That the matrix
  is built from this frame's g_camera before `0x5E4E96` is likely, not shown.
- Camera-mode switches during the draw: whether CameraModePath::Restart or Cleanup (vt +0x10, vt +0x18, GCamera's
  stack code `0x441C5C..0x441DEB`, SwitchToViewMode's 10 callers) run inside PreDrawProcess or Process3dEngine before
  `0x54DEDE`, and whether another UpdateCamera caller sits inside Process3dEngine. Only the direct callers were listed;
  the power-up chain leads back to GInterface::Process, on the turn side.
- Whether a start-menu sub-loop inside GGame::Loop (`0x54CF20..0x54D1F6`) bypasses ProcessGraphicsEngine was not
  traced.
- `box+0x1C`: whether it is half or the full diagonal. It is used unchanged as the radius; the mesh loader was not
  read.
- Whether a DidYouKnow highlight starts with IsDisappear set: it skips SetDisappear(0), and the creation defaults were
  not read.
- The rest of fn_00813340 (the COMPLEX type's path after the on-screen test) was not read; it is not one of the flag
  writers.
- The physics step's return codes (fn_007FE260 `0x7FE824..0x7FEB49`) were not decoded beyond 2 (came to rest or sunk)
  and 3 (knocked).
- HandStateTug's write at `0x5B84AE`: that it cannot be reached is inferred, not proven (the RTTI was not dumped).
- SuperVillager: which slot vt +0x184 is on its 3D object (probably the current animation's name), and whether
  fn_00825E70 in SuperVillager::Release clears DontDraw.
- Bit 0x100 of `+0x24`: the scan read only symbols that have a size, and only the operand forms `[reg+0x24]` and
  `[reg+0x25]` (not esp, not a scaled index). A write through a pointer with a register mask, or a memset or memcpy
  over the whole object, would be missed. LH key 1 is not identified.
- The map's cell dimensions `[g_game+0x59C8]` and `[+0x59C4]`, and so whether a cell ≥ 512 can exist, were not read;
  nor whether an InsertMapObjectToCell override without a symbol bypasses GameBlock::Insert.
- SET_VANISH_OBJECT_DIST: that no shipped script calls it is not proven, since packed files were not searched.
- How each object type turns its position into the cell it is filed in was not read (openblack's map cells are the
  existing port).
- Land 1's first rebuild after a highlight is created depends on all the triggers above and has not been computed.
- The land edits: openblack's `SetCellAltitude` (called from src/ECS/Archetypes/CitadelArchetype.cpp and
  src/ECS/Vortex.cpp) writes the new altitudes into the block records at once, and the land clip reads them on the
  next drawn frame. Whether the original's edits reach the records that fn_00874AA0 reads at once, or later, was not
  checked.
- A screen or device reopen: fn_00822560, one of LHScreen::Open's callers, also calls LH3DIsland::Create (`0x822746`,
  `0x82298D`) and then fn_007FF610, so it may remake the blocks outside a land load. It was not followed.
- Between LH3DIsland::Release and the next PreDraw `[0xFA92D8]` still points at the freed blocks; whether anything reads
  it during the loading frames was not checked.
- Whether a gold scroll ever lacks its render particle (the creation's `+0xA & 1` test skips it with the glints) was
  not read beyond Create.
- IsHuman after a load: whether a villager's +0x40 is ever replaced by a fresh 3D object without passing through
  Villager::CallVirtualFunctionsForCreation again (for example Villager::ResolveLoad) was not read. SetHuman is the only
  writer of the bit, so such an object would answer 0 until it runs again; openblack answers 1 for every villager.
- Whether the Creature's and the HelpSpirits' objects are actually filed in the block arrays and so reach the list
  (DrawObjects) was not checked; the HelpDudeControl `+0x10` array being the HelpDude objects is inferred from the
  matching `+0x20` field, its layout was not read.

### Still to do in openblack

- ScriptHighlight::Draw 0x709C60 against openblack, separate fixes still to make (each its own measured change; Draw
  runs only on the frames the [object draw list](#the-script-highlight-in-the-list) reaches it):
  - The active effect and the gold scroll's sparkles (Draw 0x70A1C7..0x70A28D) are ported (see Draw above). What
    still differs or is not settled:
    - the point the 3D object is placed at: the original's is ConvertMapCoordToLandscapePoint 0x613750's, openblack's
      is its pose's (`UpdateFrame`'s `WorldOf`) (queued fix);
    - the sparkles' step is at the end of our frame's logic. Our audio's per-frame local draws (Guidance, SoundTags,
      SpookyVoices, run late in `Game::Update`) come before it; where the original makes them against FinishFrame's
      drain is not yet read;
    - the chain joints: the original's make their local random draws in the drain (`ParticleChainJoint::DrawAt`
      0x679E80), so a chain farther than the sparkles draws before them; openblack has none of those draws
      (queued fix);
    - the sprite angle reaches the particle path through a matrix (atan2 of its cosine and sine), equal up to the last
      bit;
    - the sprites are drawn two-sided while the original's material leaves its cull mode at 3 (a sprite of mode A
      faces the eye, so nothing is culled: inferred).
  - SET_PROPERTY YPOS (0x70EF9B..0x70EFBF → SetDrawHeight 0x709C40) only stores +0x7C = 1 and the height +0x80.
    Process copies it into the altitude at the next turn (0x70A597..0x70A59D), and the 3D object moves at the next
    drawn frame. openblack's `SetYPos` (src/ECS/ScriptHighlight.cpp) sets the altitude and the Transform at once
    (queued fix).
  - The click sphere (0x70A2BA..0x70A2DC) and the sprite (0x70A2DF..0x70A447) are only made when the main object was
    on screen or the glow or gold particle branch ran (0x70A292..0x70A29C). openblack's `ExtrasOf` is not gated
    (queued fix).
  - Two eyes: the facing, the glow and the sprite use g_camera (0xEA1DB8..0xEA1DC0); the distance scale uses GCamera
    +0x118 / +0x148 / +0x178 (0x709DCF..0x709E13). Whether they are the same point at Draw time is not yet read.
    openblack's Draw uses the list's eye (`OnScreenView::eye`, this frame's drawn `camera.GetOrigin()`) for both.

- The Post step and the light-map alpha only for a vortex in the land's object draw list (a visible block within
  VanishObjectDist, or the global list, at the last rebuild; see [The vortex](../world/vortex.md)). openblack steps
  them every frame; the logic-side list ([How openblack runs the list](#how-openblack-runs-the-list)) can now gate them.

### From the model rendering notes

- The object pass's order: openblack draws the models mesh by mesh (one batch per mesh, `Renderer::DrawPass`), not in
  the original's object-list order (stage 4m, [the frame and its draw stages](frame.md)). What an object's Draw does at
  its end follows its mesh's batch: the shadows over objects, and the knock read-out's row over a building
  ([the hand: animation and placement](../hand/animation-and-placement.md), "The population read-out"), which come after every object of
  that mesh, where the original draws each right after its own object and before the next one in the list. Mode 15
  rows blend and write Z, so where a row overlaps another object the two can come out in the other order.
- The script's `SET_VANISH_OBJECT_DIST` (`SetVanishObjectDist` 0x87FD44, the only writer of [0xC37200]) is not
  ported: the creature's reflection and the shadows' block test use its default, 100000.
- Draw order of the models: the original runs each object's Draw in the order of the object draw list, so what a
  Draw adds after its model comes right after that model. `Abode::Draw` draws the building (`MultiMapFixed::Draw`
  0x516132, at once or queued by its mesh's flag), queues its chimney smoke (0x5162E0) and then, as its last call,
  draws the knock's population read-out (`DrawPercentFull` 0x516311, whose markers `LH3DSprite::Draw` 0x8299FF draws
  at once with `DrawTriangle` 0x840C57, never through the Z-sorter). openblack draws the models mesh by mesh, one
  draw per run of drawn rows (`instance_runs::Runs` in `Renderer::DrawPass`), not in the list's order, so what it
  draws after a model, such as the shadows over an object, follows the model's batch (queued fix).
