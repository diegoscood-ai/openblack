# Script highlights

## Summary

Script highlights are the objects the challenges put in the world to be tapped: the bronze, silver and gold scrolls
that start a challenge, and the "Did you know?" sign, whose tap opens a speech bubble with a tip. They spin, pulse,
glow, sparkle (the gold scroll) and light up once their text has been read; tapping one plays a sound and starts its
challenge script or opens the bubble. openblack has the scripts' natives, the object, its turn, its draw, the gold
scroll's sparkles and the bubble, measured against the original's numbers.

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the highlights.

### Script highlights

The highlights (CREATE_HIGHLIGHT 272, HIGHLIGHT_PROPERTIES, SET_DRAW_HIGHLIGHT, ProcessHighlights 0x70A460 at
turn step 0x54E6D5): the bronze / silver / gold scrolls and the Did You Know sign (info rows at 0xD96390 in the
order 0 bronze, 1 sign, 2 silver, 3 gold: the CHL enum's order, which openblack's `Enums.h` copied, is wrong for BW1), script type
37. GAME_THING_CLICKED 016 compares GInterface+0x45C, the hand's tap memory (15 s). A script-created highlight is
deleted when its last reference goes, unless RELEASE_FROM_SCRIPT released it (0x70F600..0x70F670).

The script side:
- `CREATE_HIGHLIGHT` (`GScript::CreateHighlight` 0x6F1C20): pops the challenge id (0x6F1C30), the point (z, y, x:
  0x6F1C44..0x6F1C70) and the info row (0x6F1C88: &GScriptHighlightInfo[row] = 0xD96390 + row × 0x110);
  `ScriptHighlight::Create(MapCoords(point), info, challenge, 0.0, 1.0)` 0x709A40 (scale 1.0, 0x6F1C92);
  `AddScriptGameThing(thing, 1)` (0x6F1CC6) and `PUSH(id, 4)` (0x6F1CD7); none → "Highlight not created" (0xC0CC18,
  0x6F1CF5) and 0.
- `HIGHLIGHT_PROPERTIES` 334 0x6FE1F0: pops the DYK_CATEGORY, the text, then the thing (confirmed: xref disassembly) ("Thing not valid" 0xC0CD88);
  `ScriptHighlight::SetScriptId(text, category)` 0x709A20: +0x78, +0x84.
- `SET_DRAW_HIGHLIGHT` 306 0x708CB0: GScript +0x80 = the popped value as it is (0x708CCD).
- `GAME_THING_CLICKED` (`GScript::GameThingClicked` 0x70AEB0): a multiplayer game logs "This is not multiplayer friendly
  yet!" and pushes true without the pop (0x70AEC0..0x70AEEE). The test is GInterface +0x45C (`BaseInfo::GetBase`
  0x436B80) == the thing; result pushed at 0x70AFCA (type 6).
- `GAME_THING_CLICKED` on a scroll (vt +0x48C, not IsDidYouKnow 0x70AC20) of the challenges 0x38, 0x3B, 0x3C, 0x3D
  (fn_0070AC50, 0x70AF67): +0x45C cleared (+4 / +8), `SaveGameRoom::InstantSaveGame(14)` 0x792FB0, then tapped again
  (fn_005D36D0: +0x45C = it, +0x468 = the turn).
- `SET_ACTIVE` 255 (`GScript::SetActive` 0x6FD720) tests a ScriptHighlight (vt +0x48C) first.
  Then a spell dispenser (vt +0x1F4, `fn_00723030`) and a scaffold (vt +0x49C, `ForceBuildBuilding(0)` 0x6E8860 only
  when active). Anything else, or no thing, goes to `ScriptErrorMessage` 0x6F62B0, a bare `ret`: nothing happens.
- `GET_PROPERTY` altitude (0x70E70A): Pos.altitude (+0x1C) of any thing as a float. `SET_PROPERTY` altitude
  (0x70EF9B..0x70EFBF): a ScriptHighlight → `SetDrawHeight(val)` 0x709C40 (+0x7C = 1, +0x80 = h); then, for any thing,
  +0x1C = val and its 3D object moved there with LH3DObject::SetPosition(GetAltitude + altitude, angle, scale).
- The finders see a highlight as script type 37 (0x70AE30), with its info row as the sub-type (fn_006F6D00 case 17).
  FollowUs' CALL_NEAR(HIGHLIGHT, 1, ...) at CHL L53068 / L53085 finds the Did You Know sign.

The class:
- `ScriptHighlight` is 0x8C bytes, vtable 0x94228C, derived from SingleMapFixed. It lives in ScriptHighlight.cpp
  0x7095D0..0x70AE60.
- Highlights are made by scripts (CREATE_HIGHLIGHT 272) or by an abode by its door (`Abode::CreateAbodeSurroundingObjects`).
- `ScriptHighlight::Create` 0x709A40(coords, info, scriptId, yAngle, scale) allocates it with new (0x8C bytes) and runs
  the ctor 0x7098A0.
- The ctor goes through FixedObject 0x52DDC0, clears the fields with fn_00709910, puts the highlight at the head of the
  list g_game+0x205C94 (count +0x205C98, next at +0x88) and sets +0x78 = scriptId (0x7098FB).
- `CallVirtualFunctionsForCreation` 0x709AA0 then runs SingleMapFixed's version 0x52E880, which makes the 3D object.
  `InsertMapObject` 0x52E620 puts it at the head of its cell's fixed list.
- An info index past the 4 rows reads past the array in the original.
- GetSaveType 0x709850 = 0x3F. vt +0x48C (IsHighlight) is 1 for ScriptHighlight (0x709740) and 0 for GameThingWithPos
  (0x4023A0).
- The info rows are 4 GScriptHighlightInfo of 0x110 bytes (GetBaseInfo 0x709640), in info.dat order.
- Row 1 "Script Did You Know Sign" uses MSH_O_INFO_SIGN as both meshes and has no particles. `IsDidYouKnow` 0x70AC20
  tests row index == 1.
- Fields: +0x5C hidden, +0x60 active (IsActive 0x709750), +0x6C the glints effect (info +0x108), +0x70 the active effect
  (info +0x10C) and +0x74 the sparkles (0x1C bytes, fn_006CA7A0, in PSysRenderParticle.cpp; below).
- More fields: +0x78 the script id, +0x7C / +0x80 the draw height flag and value, +0x84 the did-you-know category. +0x64
  is the sprite and +0x68 the glow object.
- +0x78 starts as CREATE_HIGHLIGHT's challenge id. HIGHLIGHT_PROPERTIES then sets it to its text through `SetScriptId`:
  the HelpText of a did-you-know, or the challenge of a scroll.
- +0x5C hidden is kept by Save / Load (0x70A78A, 0x70A911) and read by Draw (0x709C7C). Its only other writer is the
  reset to 0 (fn_00709910 0x70991B, from the constructor 0x709728 and fn_007098A0 0x7098D1); nothing in play sets it,
  so a highlight is never hidden in play. (`GScript::ChangeObjectProperties` 0x70F53C writes a +0x5C, but of a Flock.)

Creation and meshes:
- The mesh is the info's normal mesh (+0x100, through SingleMapFixed::GetMesh vt +0x608) or its active mesh (+0x104,
  GetActiveMesh 0x70A750). An index out of range gives MeshPack[0] (0x709BAC).
- `SetActivated` 0x70A656..0x70A6BE and CallVirtualFunctionsForCreation 0x709B8A..0x709BA4 choose between the two meshes.
- The glow object's mesh is MeshPack[0x20F] MSH_S_BLAST_CENTRE (0x709B07), with SetMaterialProperties {1, 0, 0, 1, 1}
  (0x709B4B).
- The sprite is `LH3DSprite::Create(1, 1)` (one sprite, +0x28 |= 0x80), cell 0, in CreateMaterial(13,
  ".\data\textures\S_SpriteSheet3.raw") with material +5 |= 1 (fn_0057D590, mode 13 pushed at 0x57D5A4).
- At creation (0x709B60..0x709B85), a did-you-know whose text was already read is lit at once.
- At 0x709BC7, unless +0xA & 1, the glints (info +0x108) start at GetLHPoint with the highlight as their target (vt +0x114
  AddTarget_), followed by the +0x74 particle.
- Effects are made with `GJPSysInterface::Create(nullptr, type, GetLHPoint, (0, 0, 0), 1.0, 0)` 0x68E910
  (0x709BDA..0x709C0E, 0x70A6D2..0x70A707). The result is 0 when the row has no particle type or the type has no file.

Turn processing:
- `ProcessHighlights` 0x70A460 runs after Bookmark::ProcessAll and before GClimate::ProcessAll.
- It steps the shared pulse first: [0xD967D4] phase, [0xD967D8] value, [0xD967DC] last value.
- The phase grows by [0xD01A38] ms per turn × 5 [0x942244] × 0.001. It drops by 2π once when above 2π. Then previous =
  value and value = (1 − cos phase) × 0.5 (0x70A463..0x70A4D6).
- Then it walks the list from the head and calls `Process` 0x70A580 (vt +0x5FC) on each available one (vt +0x2C). The
  next link is read before Process runs (0x70A4DC..0x70A506).
- `Process` 0x70A580: with a draw height, Pos.altitude = the draw height. Otherwise it is fn_006022C0(Pos, this, 1), the
  top of what the highlight stands on (0x70A583..0x70A59D).
- A did-you-know not lit yet is checked every 8 turns, by (turn + its unique id) & 7 (fn_00436A80
  UniqueKeyHeap::GetUniqueIdFromAddress). It is lit once its text has been read (0x70A5A0..0x70A5E7).
- `ScriptHighlight::OnClearMap` 0x7096E0 (from GGame::ClearMap, after Bookmark::ClearAll) zeroes the pulse.

Activation and taps:
- `SetActivated` 0x70A630 sets +0x60 at 0x70A643 and closes the old active effect (0x70A63D..0x70A64F).
- It plays sample 0x86 with GAudio::PlaySoundEffect (LH_SamplePlayOptions, bank GGlobal.audio +0x3AC, owner this) at
  0x70A67A..0x70A6B6.
- It then starts the active effect (0x70A6C3..0x70A70F) and sets the mesh on the 3D object with vt +0xF4
  (0x70A712..0x70A732).
- `InterfaceTap` 0x70AC70 is reached from GInterface fn_005D38A0 through packet 0x20 and always returns 1.
- For the local player's tap (GetPlayer() == the local player, 0x70ACA4..0x70ACA9) it calls HelpProfile::Trigger with
  0x22 when +0x78 is set, else 0x23 (0x70AC83..0x70ACCF).
- When the tapping status is GGame::MyInterfaceStatus it plays sample 0xAD, bank InGame (GGlobal +0x3AC), through
  GAudio::PlaySoundEffect 0x429E30 (0x70ACF9..0x70AD4A), with the other fields 0 (an LH_SamplePlayOptions built by
  0x8A975C).
- A did-you-know then goes to fn_0070AC00 → `HelpSystem::SetBubbleProperties(this, +0x78, +0x84)` 0x5C8330 (the bubble,
  below).
- A scroll goes on at 0x70AD68..0x70ADAD. A help task holding the dialogue (HelpSystem +0x45CC,
  ScriptDLL::GetScriptType == 2) leads to GScript::StopHelpScripts 0x6EC780.
- With no dialogue owner it calls Temple::StartScript(+0x78) 0x794A20 → ChallengeRoom::StartScript 0x784C10.
- `InterfaceValidToTap` 0x70ADD0 (vt +0x740, called from fn_005D38A0 0x5D38DB): a did-you-know needs a script id, a
  scroll needs to be active and have a script id.
- `GetOverwriteTapToolTip` 0x70AE10 gives 0xEF2 when the info's OBJECT_TYPE (+0x10) is 1, else 0. Every row is 35, so
  it is always 0. It is never read for the hand over a highlight anyway: the hand's Over Object text takes the
  highlight first, 0xEA0 «Recordatorio» once lit, else 0xE83 «Activar», in or out of the influence
  ([Tooltips: a script highlight](tooltips.md#tooltips-script-highlight)).
- The texts already read are kept by fn_0078CC10 (the question) and fn_0078CD20 (the append). Each DYK_CATEGORY 0..4 has
  one list of at most 48 entries (`cmp edx, 0x30`).
- The counts are at [0xE01E6C + 4 c] and the entries at 0xDF9D1C, 0xDF5C58, and so on.
- `ToBeDeleted` 0x709980 takes the highlight out of the list (0x709980..0x7099CF) and closes the glints
  (0x7099D5..0x7099E3), the active effect (0x7099E6..0x7099F4) and the +0x74 particle before Object::ToBeDeleted.

Draw (0x709C60):
- Nothing is drawn while +0x5C is set, or for a scroll while the scripts' SET_DRAW_HIGHLIGHT is 0 (GScript +0x80). The 3D
  object stays where it was (0x709C7C..0x709CB9).
- A lit did-you-know faces the eye. Every other highlight turns (0x709D13..0x709DBD).
- The turn adds g_game_time_inc × 0.00314159 [0x942AF4] (π / 1000 per game ms) to the 3D object's Y angle (its +0x48).
- The angle then wraps by ftol(a × 1 / 2π [0x8C6CAC]) × 2π [0x8AB210] (0x709CE8..0x709DBD). ftol truncates.
- The scale (0x709E5D..0x709EA6): d = ftol(distance from the camera), bound to [10, 30] with unsigned compares.
- A scroll's scale is scale × d × 1 / 30 [0x8CF3F8]. A did-you-know keeps its scale. The camera is read at GCamera +0x118
  / +0x148 / +0x178.
- The 3D object is placed with LH3DObject::SetPosition(point, angle, scale) at 0x709EB7..0x709ED2.
- All of this pose is set inside Draw only, so only on the frames the
  [object draw list](../rendering/object-draw-list.md#the-script-highlight-in-the-list) draws the highlight, with no catch-up.
- The centre is the mesh's +0x18 point through the 3D object's matrix, with half extents |M × (mesh +0x24)|
  (0x709ED7..0x709FC4).
- An active scroll's glow (0x709FCF..0x70A10A) uses the rows that face the eye, with the Y row × 3 [0x8C2C50] (its +0x44 =
  3.0), placed at the centre.
- The glow's ARGB (0x70A0BD..0x70A0E6) is 0x14B4DCFF for Silver (row 2). Otherwise it is ((byte)ftol(0.6 t + 0.4) × 0x50)
  << 24 | 0xFFFF00, using [0x942AF8] and [0x8C7A44].
- Because the active pulse is 0.4..1 and ftol truncates, that alpha is 0x50 only at 1.0 and 0 otherwise.
- A scroll is clicked through an invisible sphere of radius r + 0.5 [0x942230] (GInterface::SendInvisibleDrawCollision,
  0x70A2BA..0x70A2DC).
- The sprite is drawn for everything but a lit did-you-know (0x70A2DF..0x70A447, AddDrawing 0x70A447, Screen mode).
- It stands 1.4 [0x8C7E18] radii towards the eye and is 2 [0x94222C] radii wide, at least 0.0001 [0x8BF518]
  (0x70A36C..0x70A3D3).
- The sprite's alpha by row (0x70A3E2..0x70A416): 1 → 0x32, 2 → 0x96, 3 → 0x64, otherwise 0.
- The active effect +0x70 (0x70A1C7..0x70A262), after the glints' step: `GJPSysInterface::SetOrigin` (vt +0x124,
  0x55ECF0 → PSysManager::SetOrigin 0x672E10, the origin only) to the centre, then `Process_` (vt +0xFC 0x6736B0) with
  the glints' info and `g_game_time_inc`, then `Draw_(1.0, true)` (vt +0x104 0x55EDA0). It is made exactly as the glints
  (`GJPSysInterface::Create(nullptr, type, GetLHPoint, (0, 0, 0), 1.0, NET 0)` at 0x70A707), without a target, so it is
  stepped only here, on drawn frames, on the local stream, and its first `Process_` steps twice.

The sparkles (+0x74, the gold scroll's rising sparkles):
- Made for **every** highlight at creation, after the glints (0x709C28 `fn_006CA7A0`, also when the row has no glints,
  skipped only with the glints by +0xA & 1). Only Draw's row 3 test (0x70A26F `cmp [ebp+0x28], 0xD966C0`) draws them, so
  only a gold scroll shows them. `ToBeDeleted` frees them.
- fn_006CA7A0: new 0x1C bytes (PSysRenderParticle.cpp line 0x5BC), ctor 0x6CA7F0: +0 array, +4 count, +8 a byte, +9 =
  0xFF (the greatest alpha), +0xC..+0x14 the point, +0x18 a rebuild flag. Once for the game it makes the shared sprite
  [0xD4EE14] = `LH3DSprite::Create(1, 0)` (no own material) and the material [0xD4EE18] = CreateMaterial(13,
  fn_0057DBE0(".\data\textures\S_SpriteSheet1.raw")), which loads the `a.raw` alpha file too; +5 is not set.
- fn_006CA870 builds 20 entries of 8 bytes: entry i = {ms = (−8000 i) / 20 = −400 i (0x66666667 division),
  angle = `GRand::LocalFloatRand(2π)` 0x6DE590 with 0x40C90FDB}, in order (0x6CA8C3..0x6CA8FB). Then +8 =
  `GRand::LocalRand(0x20)` 0x6DE570 (0x6CA850..0x6CA85B). So each highlight's creation draws 20 local floats and one
  local integer.
- In Draw (0x70A268..0x70A28D), whatever the on-screen test: fn_006CA910 copies (centre.x, centre.y + radius, centre.z)
  into +0xC (the point is made at 0x70A10F..0x70A132, after the glow branch joins), then fn_006CA930 queues one Z object
  `NewZObject(fn_006CA990)` with the key (dx² + dy²) + dz² from g_camera. edi = 1 keeps the highlight on screen.
- The Z callback fn_006CA990, when the queue drains at the frame's end: first every entry's ms += g_game_time_inc (a
  32-bit add); one above 8000 (signed `jle`) loses 8000 once and draws a new angle `LocalFloatRand(2π)`
  (0x6CA9AA..0x6CA9DC). Then the material (only with g_b_do_update_material; cull mode 1 or 3 by the material's +5 bit
  0) and, for each entry with ms ≥ 0, the shared sprite drawn with `LH3DSprite::Draw` 0x840530 (mode A):
  - position = the point, with y − (−20 [0x938EC8]) × s, s = ms² (`imul`) × 1.5625e-8 [0x93A22C] (0x6CAA78..0x6CAA9D);
  - half width +0xC = (1.2 [0x938ED0] − 2 [0x938ECC]) × s + 2 (0x6CAAA6..0x6CAABA); the stretch stays 1;
  - alpha = ftol(+9 × f) & 0xFF, f = ms × 0.0002 [0x9000DC] below 5000 (0x1388), else 1 − (ms − 5000) × 0.00033333333
    [0x93A228, bits 0x39AEC33E] (0x6CAABD..0x6CAB00); colour 0xFFFFFF;
  - cell (flags & 0x3F) = (ms / 50 + i + byte +8) mod 32 (0x6CAB1F..0x6CAB5B); angle +0x14 = the entry's angle.
  With the FPU at 24 bits each step rounds to a float.
- The centre and the radius the Draw uses (0x709CE5..0x709D0C, 0x709ED7..0x709FC0): vt +0xF8 of the 3D object
  (LH3DStaticObject vtable 0x9A2974 → fn_007F9E70, its mesh +0x7C) gives the LH3DMesh. Its +0x18..+0x20 is the box
  centre (max + min) × 0.5 and +0x24..+0x2C the half extents (max − min) × 0.5, in the mesh's own space, made at load by
  `LH3DMesh::ComputeBoundingBox` 0x8081B0 over every submesh (0x8082BB..0x80835B; see
  [Object size](../engine/object-size-and-map-cells.md#object-size)). Both go through the matrix `LH3DObject::SetPosition` 0x423140 has
  just written at the 3D object's +0x14 (rows +0x14 / +0x20 / +0x2C, translation +0x38): x = ((cz m6 + cy m3) + cx
  m0) + m9, y = ((cx m1 + cz m7) + cy m4) + m10, z = ((cy m5 + cx m2) + cz m8) + m11 (0x709ED7..0x709F36); the radius
  is the same three sums on the half extents without the translation, then fsqrt((X² + Z²) + Y²)
  (0x709F42..0x709FC0). The scroll meshes have no bones (flags 0x23000 for meshes 313..320, 0 for 368 in
  AllMeshes.g3d), so no LH3DAnim::SetTransform runs first.
The did-you-know bubble (HelpSystem +0x18, Bubble 0xB0 bytes, made by fn_005C8490):
- Drawn by `HelpSystem::Draw3D` [0x5C5B38] → fn_005C83D0 → `Bubble` fn_00576F20, a Z object in the world (0x5767D0):
  GatheringText::DrawBubble 0x8334C0 (9-slice and tail); the text lines in j0 (the bubble's +0xAC) through
  GatheringText::DrawText 0x8315B0 (shadow, then text); gatheringtext.raw goes in CreateMaterial(6).
- Its place in the frame ✅: fn_00576E90 queues it with `LH3DZSorter::NewZObject` 0x83F310(draw 0x5767D0, key, flags 0)
  (0x576F06..0x576F11), only while `LH3DRender::g_started_frame` [0xECA644] and alpha × the distance fade (+0x70) > 0
  (0x576E90..0x576ECC; a NaN fails too). The key is |point − camera|² summed (x² + y²) + z² (0x576ECE..0x576F03), the
  point the bubble +0..8, the sign's point that fn_00575EA0 copies (0x575EAB..0x575EBA), the camera
  `LH3DTech::g_camera` [0xEA1DB8]. `HelpSystem::Draw3D` runs in `Process3dEngine` before `LH3DRender::FinishFrame`
  0x82F460, so the bubble is drawn in FinishFrame's Z flush (fn_0082F280), sorted far to near with the world's other Z
  objects (a nearer one, the hand for one, is drawn over it), and before HelpText's callback (priority 20000): the
  dialogue texts are drawn over it ([Land 1 intro, drawing order](../scripts/land1-intro.md#drawing-order)).
- Its depths and Z states ✅ (the frame's ZFUNC is LESSEQUAL; mode 6 and the font's mode 16 write no Z):
  - the draw depth is fn_005766E0(+0x58) (0x576825..0x57686D), +0x58 the sign's camera depth that fn_00575EA0 stores
    at 0x576006; its pitch is `GCamera::GetHeadingAndPitchFromPoints` 0x4428D0(g_camera.pos, g_camera.foc)
    (0x5766E3..0x576731), its near plane [0xE839E0];
  - the box: DrawBubble's corners through Get3DPointFromScreen at the draw depth, world triangles Z-tested;
  - the lines: DrawText with i3 = 1 (pushes 0x576D20 / 0x576D54 / 0x576DF6), so DrawTextRaw keeps the frame's ZFUNC
    (0x83340D), the shadow's call at the draw depth − 1 and the text's at − 2;
  - the $g symbols: fn_005760C0's arg 12 is the draw depth (stored at 0x576875, loaded for both calls at 0x576C1D /
    0x576CC0), and fn_005763E0's arg 8 the depth of `LH3DTech::Draw3DScreenTriangle` 0x81C5C0 (rhw = near / z, sz =
    1 − near / z, the world's own formula, 0x81C5DD..0x81C60D; it leaves ZFUNC): the shadow at arg 12 − 1 (0x576132..
    0x57613F), the symbol at arg 12 − 2 (0x5761A0..0x5761BC);
  - the scroll arrows: `SetupThing::DrawBox` with its bool 0 (0x57693A / 0x5769E0), ZFUNC ALWAYS (0x412C7C);
  - the $m icons: `CameraHelp::DrawKeyOrMouse` 0x447EA0, which has no depth argument, its 2D boxes ZFUNC ALWAYS.
- `Bubble::SetDefaultSizes` 0x575E20 with (400, 250, 19): the near sizes and the far ones (× 1/7 [0x8FFF3C], 1/35
  [0x9000D4]). The far margin is half the width × 1/35 (0x575E84), as the margin is half the width × 0.2: 400 / 2 / 35
  = 5.714, not half the height's 3.571.
- `GatheringText::GetFrac` 0x831550: 0 at or below a, 1 at or above b, else (v − a) / (b − a); a == b gives 0
  or 1 (v <= a, v >= b); the 0.5 branch at 0x831589 ([0x8AA3B4]) is reached only with b NaN, and a NaN v gives 0
  (oracle: (1, 1, 1) → 0, (2, 1, 1) → 1, (1, 0, NaN) → 0.5).
- fn_00575EA0(point, offset, place): nothing below a depth of 1 (0x575EE5); the distance fade in below the fade-in end and
  out above the fade-out start, both bounded by the middle of the range (0x575F58..0x575FFB).
- The bubble's lerps are a + (b − a)·f in float, as the exe's `fsub` / `fmul` / `fadd` (0x575F0F..0x575F1D): at f = 1 they
  land a few ulp off b.
- The sign's point is projected by fn_00442C20, which fails only when its camera depth is less than the near clip
  [0xE839E0]; there is no test against the screen's edges, so the bubble is laid out, drawn and hovered with its point
  past an edge too, for as long as the sign itself is on the screen (fn_005C82F0).
- The tail (fn_00576F20 0x5770DB..0x577134): u = 2 sx / W − 1 clamped to −1..1; offset +10 when u <= 0, −10 when u > 0;
  place |u| × 0.6 + 0.2.
- fn_005766E0: drawn 5 m in front of the sign, pulled toward n = 2 × near as the camera looks down: p = clamp(|pitch|, 0,
  π/2) × 2/π, e = max(d − 5, n) − n, depth max(n + e (1 − p²), n).
- `GatheringText::DrawBubble` 0x8334C0(left, top, right, bottom, depth, corner, tailX, tailRight, colour, hasTail, a0,
  a1): a 9-slice of gatheringtext.raw cell (0, 0) on a 4 × 4 grid (u {0, 0.5, 0.5, 1} × 0.25, v {0 or 0.3, 0.5, 0.5, 1} ×
  0.25; jump tables 0x8339A4 / 0x8339B4), corner = min(corner, w/2, h/2), nothing when w or h <= 0.
- Each of its corners, the tail's too, is cut to a whole pixel (`ftol`) before Get3DPointFromScreen turns it into a
  world point at the draw depth; the vertex alpha is taken at the uncut height.
- Its alpha fades from a0 / 255 at the top to a1 / 255 at the bottom (0x8335AE..0x833614); the tail (cell (1, 0),
  mirrored on the right) is as wide as the corner, kept 1.8 corners from the ends, 0.85 / 0.15 of it left of the point,
  from 0.091 corners above the bottom down, in the bottom's colour.
- Dark colour of the bubble 0xFF3BBDED, at 255 × alpha [0x57714E..0x57715A, 0x576819..0x57683B].
- Text area [0x576A62..0x576B06]. m = margin · 0.333333 [0x8AB26C]; t = (bottom − top) · 0.333333 + top
  [0x576A87..0x576A99]; the bottom b = bottom − m, or t if smaller [0x576A9D..0x576AB6]; the clip, b + m/4; the initial y,
  b + the pixel offset.
- Text parts [0x576B9F..0x576CB3]: a part that starts with $g / $m keeps only that token (4 characters). Otherwise, the
  text up to the first $g (or, if there is none, $m) is taken and then that token; whatever follows is not drawn.
- Each DrawText call with i2 set (both draw calls) draws nothing when size · 0.5 > f4 − f1, when f6 − f2 < size, or
  when its start f7 > f6 (0x8315BF..0x83161D); after each line the loop stops once the next line's top is below f6
  (0x831977..0x83198C), and a line is drawn only when its bottom is below f2. The measure call (i2 = 0) has none of
  these, so the height is counted either way.
- After each part (its lines and its $g / $m step), its height already added to +0x7C, the loop stops once the part's
  top is above the box's top (0x576E67..0x576E74): the parts above it are neither drawn nor counted, so +0x7C, and the
  scroll range it gives, ends with that part.
- The parts come from the bubble's callback fn_005C8530, from the bottom up: 0 the tip, 1 the string at 0x9CEE0C (`20
  00 00 00`, a single space: one blank line between the tip and the title), 2 the title, help text 0x1016; past 2 none.
- Line breaking: `GatheringText::DrawText` 0x8315B0 grows a line up to 4 characters at a time while GetStringWidth fits;
  on overflow it backs off to the last whitespace or U+F8FE (dropped) or after a '-' (kept), else one character at a time
  (0x831762); a CR / LF ends the line and is dropped. Lines go from bottom to top [0x576D4E..0x576D66].
- Both draw calls pass DrawText's centre flag (i1 = 1, 0x576D49 / 0x576DEF / 0x576E3E), so each line is centred across
  the area: its left is (f4 − f1 − GetStringWidth(line)) · 0.5 + f1 (0x831936..0x83194D).
- The shadow's call shifts all seven bounds by +2 (seven `fadd 2.0` at 0x576D86..0x576DDE): the left f1, the clip top
  f2, the fade in end f3, the right f4, the fade out start f5, the clip bottom f6 and the start y f7, so its fades and
  its clip are its own, two pixels down. Its depth f8 is the draw depth − 1, the text's − 2.
- $g<n> (0 < n < 24), fn_005760C0: a square of 2 × size in its own step, with cell n % 16 of S_Gesture<n/16>
  (fn_0057D5D0, CreateMaterial(6) with alpha), a shadow at +2 in the dark colour and then the glyph. The following step is
  a product × 0.7 [0x576C62..0x576C90]. For $m<n>, the key bound to the action and then `DrawKeyOrMouse`
  [0x5761E7..0x5763D1] (fn_005C78F0 → `CameraHelp::DrawKeyOrMouse` 0x447EA0; the icons at 0x57636B..0x5763CC).
- fn_005763E0 clips the quad to [top, clip] and moves its v according to the clipped part (a quarter of the cell per
  side) [0x5765B1..0x576680].
- +0x7C (0 at 0x576AFB; then the height of each part and icon is added to it) is written only by the Z object's draw.
  fn_00576E90 queues nothing while alpha × fade ≤ 0 [0x576ECC], and then +0x7C keeps its last value. Restarting the
  bubble sets +0x7C = 0 (0x5C83AF).
- Bubble arrows [0x5768AF..0x576A5F]: visible while GetTickCount (import 0x8A91C4) % 750 < 400. With h = box height, s = 0.2 h and k = s/9
  (the float −1/9 [0x9000D8]): arrow +0x94 at top right [0x576927..0x5769BB] and +0x95 at bottom right
  [0x5769CC..0x576A5F]. They are squares of 0.5·s + 2·k in atmos.raw (SetupThing::DrawBox 0x412980 in AtmosMaterial),
  colour {0, 0xFF, 0xFF, 0xFF}, and DrawAlpha = dark colour's alpha / 2 (0x5768D4..0x5768FD).
- Pre-transformed rectangles: SetupThing::DrawBox 0x412980 / DrawAndClip2D 0x82A5B0 (FVF 0x1C4; DrawBox clips at ±0xA000
  with inv_w 100.0).
- The hand drags [0x5772AE..0x5773AF, 0x5772C8] (GInterface +0x3D0 / +0x3AC == 0x1D) and hovers over it (fn_005D5A30); it
  offers the screen object [0x5773A0]; while it stays alive, +0x98 = 3.0 [0x5773A5].
- `SetBubbleProperties` 0x5C8330(thing, text, category): the same thing again closes the bubble (+0x20 = 0); else +0x1C =
  text, +0x20 = thing, scroll +0xA8 = 1000 (the last line), +0x7C = 0; with no did-you-know read yet (the five counts sum
  to 0) it starts "FirstDYKExplained" through fn_006EB7E0 (0x5C834F..0x5C8385), and the text is marked read (fn_0078CD20,
  0x5C8393); a box that closes marks nothing (0x5C83BA).
- fn_006EB7E0(name): a dialogue owner of type Help (2) → StopHelpScripts 0x6EC780; then, with no owner left,
  GScript::StartScript 0x6EB710.
- fn_005C82F0 once a turn: the thing gone (IsAvailable vt +0x2C) or off the screen (fn_0081F1A0 on its mesh, no temple
  test) closes the bubble; else its life +0x98 = 3.0.
- The life ages by g_game_time_inc × 0.0002, not below 0 (0x5773B3..0x5773D9): 3.0 lasts 15 s of game time.
- The scroll is in lines, clamped to the text that does not fit, measured with the text height +0x7C of the last draw
  (0x577141..0x577262).

- 0x577116..0x57712D: the doubles 0.6 [0x9000E0] and 0.2 [0x8C6B38] (in the code: `openblack::help::TailFor`)
- [0x8C7B18] (in the code: `openblack::help::DrawDepth`)
- ftol((y - top) x slope + a0') with slope = alpha (a1 - a0) / (255 h), a0' = alpha a0 / 255 (0x8335CE..0x83373E) (in the code: `openblack::help::BubbleShape`)
- fn_005C8490 (in the code: `Bubble::Bubble`)
- [0x447A0000] 0x5C83A1 (in the code: `Bubble::Restart`)
- 0x577145 (in the code: `Bubble::ClampScroll`)
- GatheringText::GetFrac 0x831550: 0 at or below a, 1 at or above b, else (v - a) / (b - a); a == b gives 0 or 1, the 0.5 branch only for a NaN b, a NaN v gives 0 (in the code: `GetFrac`)
- fn_00575EA0(point, offset, place) after the projection: nothing below a depth of 1 (0x575EE5). `screen` is the point's projection in pixels. (in the code: `LayoutBubble`)
- fn_005766E0 0x5766E0: the screen depth the bubble is drawn at, 5 m in front of the sign and pulled toward n = 2 x near as the camera looks down (pitch 0..pi/2) (in the code: `DrawDepth`)
- GatheringText::DrawBubble 0x8334C0(left, top, right, bottom, depth, corner, tailX, tailRight, colour, hasTail, a0, a1) as triangles (three vertices each): the 9-slice of gatheringtext.raw's cell (0, 0) on a 4 x 4 grid and, with a tail, its quad from cell (1, 0). The colour's alpha fades from a0 / 255 at the top to a1 / 255 at the bottom (0x8335AE..0x833614). ⚠️ approximate the exe draws one list of 16 or 20 vertices through the index table 0x9A399C; here two triangles a quad, in the grid's order (in the code: `BubbleShape`)
- GatheringText::DrawText 0x8315B0's line breaking: a line grows by up to 4 characters at a time (stopping at a CR / LF or the end, 0x83164A..0x83169D) while GetStringWidth fits `maxWidth`; on overflow it backs off to the last whitespace or U+F8FE (the break goes there and drops it) or after a '-' (kept), 0x8316B1..0x831793, else one character at a time until it fits (0x831762). A CR / LF inside the line ends it there and is dropped (0x831730..0x8317A1). Each line is `size` high (0x8317D0..0x83187F). (in the code: `WrapGatheringText`)
- fn_005C82F0 0x5C82F0's "on the screen": +0x98 = 3.0 (in the code: `KeepAlive`)
- fn_005766E0 0x576AFB..: +0x7C, the drawn text's height (0 when the draw starts, then each part and icon added) (in the code: `SetTextHeight`)

### What the world does with a scroll

✅ `ScriptHighlight`'s own answers, read in the frozen exe, every one `xor eax, eax; ret`:
- `CanBecomeAPhysicsObject` 0x709810 and `InteractsWithPhysicsObjects` 0x709820: it never flies and stops nothing that
  flies, so nothing knocks it over;
- `IsEffectReceiver` 0x7097E0: no effect reaches it (no burn, hit or heal), so `Object::ApplyEffect` 0x637AB0, the only
  caller of `CanBeCrushed` (0x7097D0, also false; vtable slot 0x62C), is never reached for it;
- `CanBePickedUpByCreature` 0x709830 and `CanBeDestroyedByStoning` 0x7097B0: a creature never picks it up or stones it;
- `CanBeSetOnFire(Creature*)` 0x7097C0 and `CanBeSleptNextToByCreature` 0x7097A0: false, and no code calls their
  vtable slots (0x264, 0x254);
- `ValidForPlaceInHand` 0x709770: the hand never takes it.

## openblack

Status: **done** for the scripts' side, the object, its turn, its draw, sprite and glow and the bubble; the open
points are in Pending.

- `ecs::script_highlight` (`script_highlight::*`): CREATE_HIGHLIGHT, HIGHLIGHT_PROPERTIES, SET_DRAW_HIGHLIGHT and the
  turn's ProcessHighlights. SET_ACTIVE has the same three
  cases, and the last one is only an error line ("Object no longer valid" for no thing, "Invalid object type"
  otherwise), not an unwritten native.
- SET_DRAW_HIGHLIGHT's switch (the original's GScript +0x80, 1 from `GScript::Reset` 0x6EB2FA, confirmed: xref immediates) is kept in the script
  control's camera control (`help::script_control::GetCameraControl`) and goes back to 1 only when the scripts
  restart (`CameraControl::Reset` in `Game::ResetScripts`: a new game, a playground's start, the testbed), so a land
  change keeps a script's hidden highlights, as the original's map clear does; the Land Island window's "Kept by a
  land change" readout shows it. See [what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets).
- The pose: `script_highlight::Draw` sets it the same way (`PoseOfDraw`), from the list's walk.
- The sparkles: `components::HighlightSparkles` and `script_highlight::MakeSparkles` / `StepSparkles` /
  `SparkleSpriteOf` / `SparklesPoint` (pure, test_script_highlight), made in `Create` on the local stream. The Draw
  takes the centre and radius with `DrawnMeshBounds` (the sums above, in that order) from the mesh's
  `GetBoundingBox()` (`Center()` and `Size() × 0.5` are the same float operations as 0x8082BB..0x80835B, and its
  minimum and maximum are the original's: [Object size](../engine/object-size-and-map-cells.md#object-size)) through `affine::PlacementMatrix(position, angle, scale)`,
  the SetPosition matrix; it queues them (`graphics::zsort::Queue`, the Z-sorter's insertion). `DrainSparkles`, called
  by `Renderer::PreDraw` (the end of the frame's logic, where the main pass's other shared writes are made) before the
  particles are collected, drains that queue far first, stepping each; only those are drawn, through a
  `particles::manager` DrawableSource (one Queued Z object at the point, S_SpriteSheet1 additive, 8 cells a row).
- Why the step is at the PreDraw: in the original local draws can come between the Draw and the drain. They come from
  the Draws after it in the list (`Tree::Draw` +0x635, `Abode::Draw` +0x1EA, `LH3DCreature::DrawNow` +0x2F4..), from
  `HelpSystem::Draw3D` (stage 20: 0x5C5AEE → HelpDudeControl::Process 0x5C3A30 → fn_005BF5D0 → HelpDude::Update1
  0x5BDDA0 → UpdateHoverPos 0x5BA350, four local draws) and from the drain itself (`ParticleChainJoint::DrawAt`
  0x679E80's three LocalFloatRand, for a chain drawn farther than the sparkles). The first two always come before the
  sparkles' step, so it is made after the whole frame's logic: in openblack the tree rustle, the help Draw3D and the
  advisors' update (`help::spirits`) now come before it, as there.
- The click sphere: each scroll's Draw, under the same gate as the original's (on screen, or the glow or the gold
  particles ran), sends its centre and r + 0.5 (`ClickRadius`); the hand reads the last drawn frame's
  (`DrawnClickSpheres`) as invisible draw collisions.
- The glow (2026-10-30): a started scroll's Draw works it out from the Draw's mesh centre (`script_highlight::GlowOf`:
  `graphics::light_column::Model` at the centre, `ActiveGlowArgb` at `ActivePulse` of the turn fraction) and queues it;
  the frame's end of the object draw list (`EndDrawnFrame`) hands the frame's glows to the draw (`DrawnGlows`), and
  `Renderer::CollectLightColumns` draws each, its alpha above 0 and its box on screen, as a column of light: one Z
  object at its place, mode 13, unlit in its colour (`light_column::ScrollGlow`). The original draws the glow object
  with the same calls as `TownCentre::DrawPSys` 0x69BEB1..0x69BF01 draws a village centre's column (the rows, +0x44 =
  3, `SetColour(argb, 0)` vt +0x2C, `SetGlobalAlpha(1)` vt +0x48, vt +0x100), and MSH_S_BLAST_CENTRE has the mesh flag
  0x200 (flags 0x23200 in the original `AllMeshes.g3d`), so vt +0x100 puts it in the Z-sorter. A scroll's creation sets
  the blast centre's materials {additive, no Z, one-sided, change, alpha} on the pack's mesh, for every user of it, as
  0x709B4B. A glow of alpha 0 (a gold scroll's, but at the top of its pulse) adds nothing and is not queued. The
  Citadel window's light column switch hides the glows too; the Magic window's particle stats count the frame's
  glows. Tests `ScriptHighlight.AStartedScrollGlowsAsAColumnAtItsCentre`,
  `LightColumn.AScrollsGlowIsAColumnOfItsOwnMeshAndColourUnlessItsAlphaIsNone`.
- The sprite: `script_highlight::SpriteOf` from the Draw's mesh centre and radius, under the same gate, queued with
  the frame's sprites (S_SpriteSheet3, added, cell 0) and drawn with the sparkles (`CollectSparkles`). Compared with
  the original at the same pose on a Land 1 scroll (2026-10-25): the same soft white halo.
- The bubble: the words (the text area, the parts, their lines and fades, the $g / $m steps and +0x7C) are laid out by
  `help::LayOutBubbleWords`, and the three parts come from `help::BubbleParts`; both are pure and pinned by
  `test_did_you_know_bubble`. `FillOverlayFrame` hands them the bubble's wrap caches and the key bindings.
- The bubble's place in the frame ✅: `FillOverlayFrame` gives `OverlayFrame::didYouKnow` its key point (the sign's
  point) and its draw depth (`help::DrawDepth` with the pitch of `script_camera::HeadingAndPitchFromPoints(origin,
  focus)` and the near clip); `help::DepthsOf` gives the box, shadow and text depths (pinned by
  `test_did_you_know_bubble`), and each line and symbol says whether it is a shadow. It is active only while alpha ×
  the fade is above 0. `Renderer::DrawPass` queues it as one Z object (`ZObject::didYouKnow`, keyed with
  `zsort::Key(keyPoint, origin)`, after the intro light) and the drain draws it in `MainBlended` with
  `DrawDidYouKnowBubble`: its pixels become world points at their depths through the view's own camera
  (`screen_point::ClipToWorldOf` / `WorldAtDepth`, pinned by `test_screen_point`: in front of the eye, as the original's Get3DPointFromScreen puts them, since openblack's view looks down its +z), the box, the shadows, the lines and
  the symbols tested against the world's depth (inverted Z, `render_modes::ZFunc::LessEqual`), the arrows and the $m
  icons (`Renderer::_keyOrMouseInWorld`) not. `DrawHelpText` comes after, at the end of the frame, over it.
- Where it is in the code (`ECS/ScriptHighlight`, namespace `ecs::script_highlight`):
  - `SetMeshOf`: the normal or the active mesh, as SetActivated and the creation choose; MeshPack[0] out of range.
  - `StartOwnedEffect`: `GJPSysInterface::Create`'s call; 0 when the row has no particle type or the type no file.
  - `StepPulse`: the shared pulse's turn step (phase, previous, value); `ActivePulse` is `t * 0.6 + 0.4`.
  - `IsHighlight`: vt +0x48C, true only for a highlight. `IsDidYouKnowInfo`: the row index is 1. The row enum's
    `DidYouKnow` is row 1, "Script Did You Know Sign".
  - `k_SpriteRenderMode` = 13, the sprite's render mode.
  - `SpriteAlpha`, `DistanceScale`, `SpinAngle` and `ActiveGlowArgb`: the Draw's sprite alpha by row, scroll scale,
    turning angle and glow colour.
  - `SavesGameWhenClicked`: fn_0070AC50's challenge ids (0x38, 0x3B, 0x3C, 0x3D) whose scroll saves the game.
  - `OverwriteTapToolTip`: `GetOverwriteTapToolTip`, 0 for every row; the test pins it with
    `ObjectType::ScriptHighlight`.
  - The hand's text over a highlight: `hand_tooltips::ScriptHighlightToolTip` (HandToolTipRules.cpp, submitted in
    HandToolTips.cpp's Over Object), pinned by `HandToolTips.AScriptHighlightShowsActivateUntilLitThenReminder`.
  - `MarkRead`: the did-you-know read list's append (🔍 inferred an append when the list has room).
  - `GetYPos`: `GET_PROPERTY` altitude, the thing's altitude.
  - `InterfaceTap` plays the tap sample only for the local player's tap. `OnClearMap` is called from `Game::PrepareNewLand`
    at the map clear, after the bookmarks are cleared.
  - What the world does with a scroll ([above](#what-the-world-does-with-a-scroll)): no effect reaches it
    (`effects::IsEffectReceiver`), nothing burns it (it has no fire info row, so no combustion temperature), the physics
    leaves it out (`DynamicsSystem::CanBecomeAPhysicsObject` and `InteractsWithPhysicsObjects` list their classes), and a
    creature's hands see it as no kind of thing (`CreatureObjectActionSystem::KindFactsOf`: `Thing::Other`, never picked
    up, not stoned); tests `RepairsTest.NoEffectReachesAWorshipIconAVortexAHighlightOrASpiritualShieldNorFireABareField`
    and `CreaturePhysicsTest.AScriptsHighlightIsNothingToThePhysicsOrACreature`.

### Test hooks

None of its own.

## Pending

- openblack differs: `GetFrac` returns NaN for a NaN v (the original 0) and for a NaN b (the original 0.5)
  (`help::GetFrac`, `src/Help/Bubble.cpp`), a fix row.
- The glow's look against the original's on a started silver scroll: not compared on a shot yet.
- The sprite's quad: 🔍 inferred to be the sparkles' (the original's `LH3DSprite::Create(1, 1)` against their
  `Create(1, 0)` is not read), and its material's +5 bit (fn_0057D590) is not read.
