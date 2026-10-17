# Transparency and render modes

## Summary

In the original everything drawn with blending in the world goes through one back-to-front queue, the LH3DZSorter
(up to 2048 entries, stable, keyed by the squared distance to the camera), flushed once at the end of the frame;
opaque models are drawn at once as the object list visits them. Every draw's blending, alpha test, Z write and texture
stage come from a 16-byte material and one of 19 mode functions, so a material's L3D type is its render mode.

openblack has one queue with the same insertion, cap and key, and one render-mode table that every draw asks for its
state; the arrival order of equal keys and a few draws still differ (see [Pending](#pending)).

## The original

### The single transparent queue (LH3DZSorter)

**Faithful**, with what is marked. Everything that is drawn with blending in the world goes through **a single
queue**.

- `LH3DZSorter::NewZObject` 0x83F310 (32 calls in 31 functions): linked list in a static buffer of 0x800
  entries of 0x18 bytes (0xEDDD30; count 0xEE9D34, head 0xEE9D38). The new entry goes **in front of the first one with a
  strictly smaller key** (`fld cur.clave; fcomp clave; test ah, 1`, 0x83F36A..0x83F376) or at the end (0x83F39A):
  from far to near and, with equal keys, the one that arrived first goes first (stable). With the queue full the **new one is
  lost**, whatever its key (0x83F315/0x83F31C). With a NaN key, `fcomp` says "less" (C0).
- Reset `fn_0083F3B0` from `StartFrame` 0x82F1F9. Drain `fn_0082F280` from `FinishFrame` 0x82F480, only once
  ([0xECA610]), from head to end, copying the user datum K into [0xEE9D30] (0x82F29D) before each
  callback. It goes **after** everything drawn immediately in the frame and **before** the end-of-frame
  callbacks ([the frame and its draw stages](frame.md)).
- The key is `LH3DTech::GetValueForZSorter` (inline in W120; Mac 0x010E7360): |P − g_camera|² in `float`
  (g_camera 0xEA1DB8). The x87 sum follows the order of the loads, and with the FPU at 24 bits (fn_007DEE00, 0xFCFF at
  0x7DEE0D; from `InitOneTimeOnly` and from `Process3dEngine` after `FinishFrame`, 0x54E426) each step rounds to
  `float` **(inferred: that nothing between fn_007DEE00 and the AddDrawing calls raises the precision again; D3D7 without FPUPRESERVE
  also leaves it at 24 bits)**: (x² + y²) + z² in the sprites 0x840C70, the mists 0x7FA83C, the smoke 0x7F8D3E, the effects
  0x6797E5, the hand 0x46D1BD, fn_00813340 and fn_00679F60; **(x² + z²) + y²** in `LH3DObject::AddDrawing` 0x815F0F and in
  the rain 0x834215 (`zsort::SumOrder`). Only the last bit can change.
- The `g_zsorter` object (0xECA648, `fn_0083F2B0` in `OpenD3D` 0x82CDC1) is never read: it is not ported.
- The two callers that submit with key 0 (0x5E66B4, 0x68AB5C) are drawn last.
- The rain's K: ((alpha << 8) − ftol(z × −0.0125)) << 8 − ftol(x × −0.0125), constant [0x9A3AC0] = −1/80, in 32-bit
  integers (0x834233..0x834259); outside [0, 20480) the bytes run into each other. The caller clamps alpha to ≤ 0xFF
  and queues nothing with a negative one (0x8341B8, 0x8341CC).
- The rain's callback 0x833F80 reads K back from [0xEE9D30] (0x833F83..0x833FB4): byte 0 the tile x, byte 1 the tile z
  (each × 80 [0x8D060C] to get the tile's corner), byte 2 ([0xEE9D32]) the alpha.
- `CheckRegionOnScreen` 0x868C80 (path with [0xEA9EB4] = 0; the other is `fn_007ACC60`, not read): the centre is the
  box centre through the object's matrix (0x868CBC..0x868D04), the radius the object's scale +0x44 × the box's +0x1C,
  the origin the object's position (matrix +0x38). Z + r < near → off (0x868D88); the eye closer than r to the origin
  → on (0x868DA1..0x868DE1); else rs = r·halfW/(Z·T) around the centre's (sx, sy), not truncated: off when sx + rs <
  0, sx − rs > W, sy + rs < 0 or sy − rs > H (0x868E59..0x868EFD); partly on screen counts.
- Its side effects: `g_b_last_on_screen` [0xEA1AF0], `g_last_selected_box` [0xEA1AD0], `g_last_distance` [0xEA1AF4],
  [0xC37EA0] and the 3D object's vt+0xA0.
- Screen globals of `g_info_transform`: size [0xE839E4] / [0xE839E8], half size [0xE839F0] / [0xE839F4]; [0xC3812C] =
  near·tan(horizontal fov/2).
- Its users: `LH3DObject::AddDrawing`, the PSys atoms (0x679F75), `LH3DMist::AddDrawing` (0x7FA7CE) and the
  SuperVillagers' `fn_00825400` (0x82541D).
- The point test `fn_0081F1D0`: Z < near → off (`fcom [0xE839E0]`, 0x81F1FA); sx = ftol((X/Z + 1)·halfW), sy = ftol((1
  − Y/Z)·halfH) (0x81F214..0x81F27F), truncated towards 0, so a point less than a pixel left of or above the screen
  still counts; on screen when 0 ≤ sx < W and 0 ≤ sy < H (0x81F286..0x81F2A8).
- LH3DMist vt+0x104 = `fn_007FA790`: the same sphere (0x7FA79E..0x7FA7CA) through `CheckRegionOnScreen` (0x7FA7CE),
  then the Draw vt+0x108 at once (0x7FA7E0).
- The influence ripples are one Z object each (`fn_008274A0`, from `fn_005E5CD0` 0x5E6264..0x5E628D); the callback
  0x827500 draws its 7 smoke.raw sprites with DrawSpecial1 in the border's plane.

**What goes in and with which point** (`Renderer::DrawPass`, the `sorted` queue, only in the main view):

| What | Original | Key point |
|---|---|---|
| models whose mesh has the 0x200 flag, whole (also those fading out) | `LH3DObject::AddDrawing` 0x815F53 | instance translation (+0x38), (x² + z²) + y² |
| the one-shot ball's bubble, always | 0x815F53 (bit forced at 0x72A4AA) | `sortPoint` (`OneOffSpellSeed::Draw` 0x518E90) |
| the hand, whole | `CHand::AddDrawing` 0x46D203 | the origin +0x38 of the LH3DObject [CHand+0x482C] (0x46D1B7); here the instance translation **(inferred: that it is that origin)** |
| `Sorted` effect: each sprite | `LH3DSprite::AddDrawing` 0x840CB3, from 0x67B0D2 | the sprite (`SortedFrame::sprites`) |
| `Sorted` effect: each mesh atom, also the opaque and cut ones | `fn_00679F60` 0x679FC7, from 0x67A246 | the translation (+0x38..+0x40) |
| `Sorted` effect: each chain | `fn_0067B380`, from 0x6798DF | the joint n/2 |
| `Sorted` effect: each mist | `LH3DMist::AddDrawing` 0x7FA87B, from 0x67A782 | the mist (via `mists::Submit`) |
| `Queued` effect, whole | `PSysManager::AddDrawing` 0x679834 | the effect's `GetOrigin` |
| `components::Sprite` | `LH3DSprite::AddDrawing` 0x840CB3 | the sprite |
| map mists and those of `mists::Submit` | `LH3DMist::AddDrawing` 0x7FA87B | the mist |
| **clouds** | 0x7FA87B, from `fn_005E25C0` 0x5E2813 | the cloud |
| chimney smoke | `LH3DSmoke::AddDrawing` 0x7F8D8E | the chimney |
| **rain, one entry per tile** | `fn_008341B0` 0x83427F | (x, `GetAltitude`, z) of the block centre (+0x90C/+0x910 + 80, 0x8362DB); here `tile.origin` with the height from `LandHeightAt` **⚠️ approximate** |
| **boat sprites, one by one** | 0x840CB3 (wake `PetitNavire::PostDraw` 0x5E08D0, puffs fn_00823F70 0x82411A) | the sprite |

Clouds go in with the sky (as in `GLandscape::Draw`); the rest, with the models. Without entities (debug
view) the queue has only clouds and mists and is drained at the end of the pass.
The `components::Sprite` (dust, pick-up particles, sparkles, fireflies...) go with the transparent models;
before, openblack drew them before all of them and a transparent model behind covered them.

**What goes in and what does not: the models**. `LH3DObject::AddDrawing` 0x815A70 (vt+0x100 of the static, animated
and morphable objects) sends an object to the queue **only** because of bit 0x10 of its +4 (vt+0x44 = `fn_007F97C0`,
read at 0x815AC2 and tested at 0x815F0B): with it, the **whole** object (NewZObject 0x815F53, callback 0x7FA980 → Draw
vt+0x108); without it, the Draw **immediately** (0x815F62), blended primitives included. The bit is copied by
`SetMesh` 0x7F9E10 from the mesh's 0x200 flag (vt+0x3C = `fn_007F9D40`, mesh+4 & 0x200, 0x7F9E48; vt+0x40 =
`fn_007F97A0`, 0x7F9E51..0x7F9E64). mesh+4 are the L3D header flags (`GetChimneyPos` 0x7F9F17 tests 0x400 there,
HasChimney): `L3DMeshFlags::Unknown10`, `L3DMesh::IsZSorted`. The one-shot ball forces the bit (vt+0x40(1) 0x72A4AA,
after its `SetMesh` 0x72A49D). `SetGlobalAlpha` (bit 0x80, vt+0x48 = `fn_007F9D60`) does **not** count in 0x815A70:
only `fn_00813340` (vtable 0x9A3068, that of `Particle3DAnim`) queues because of it (vt+0x4C 0x8133B9). That is why an
object that fades with a mesh without 0x200 is drawn immediately with the table 0xC387C8
(`render_modes::Table::GlobalAlpha`), in the main view. It affects the physical shield (`PhysicalShield::DrawShield`:
`SetGlobalAlpha(1)` 0x72D0CC, `AddForDrawing` 0x72D0E2): its mesh `SpellSolidShield` has the flag in
`Data\AllMeshes.g3d` (the game's one, read in full: 626 L3D), so it stays in the queue. With the flag, in that file,
40 meshes: almost all the miracle ones (`SpellBlast*`, `SpellPhile*`, `SpellRainCone`, `SpellSolidShield`,
`SpellSpellBallSurface02`, `SpellSpellDispenser`, `SpellPulseIn/Out`...), the caves, the Aztec storage pit,
`I_SpellGem`, `ObjectBoxFrame`, `RewardChestExplode`, the vulture and `TreeWheatInField`. the other models,
immediately, with all their primitives. **🔍 inferred** that every model goes through 0x815A70
(`Game3DObject::AddForDrawing` 0x63B5D0 → vt+0x100). Not all do: the creature's and the hand's bodies are of class
0x9A3068 (`LH3DObject::Create(3)` from `Morphable::MorphInit` 0x61731A), whose `AddDrawing` `fn_00813340` also queues
by the global-alpha bit; the hand always goes to the queue anyway, and the creature has neither bit ([drawing in the
frame](../creature/drawing.md#drawing-in-the-frame)).

**The three paths of a PSys effect** (`psys::DrawPath`; [particles: the PSys core](../physics/particles.md)). `fn_00679860` copies the manager's +0xAE
into [0xC0215D] (0x679884) and each atom checks it:

- **`Sorted`** (`Draw_(t, 1)` 0x55EDA0 → `fn_00679840`, +0xAE = 1 at 0x67984E; `Spell::Draw` 0x720441, storm
  0x72DCB7, shield 0x72D160, teleport 0x5FCDC5, dispenser 0x722A13, flock 0x72420D, hand utilities,
  town belief 0x69BF19): the effect **has no Z-object**. `manager::CollectSorted` gives each element with its
  point and `DrawPass` does one `Submit` per element, (x² + y²) + z²: each sprite (`LH3DSprite::AddDrawing` 0x840C70
  from 0x67B0D2, at the LH3DSprite's position, raised by height·size·0.5 with `CentreAtBase`, 0x67AFAB..0x67AFD6),
  each mesh atom, **opaque and cut ones too** (`fn_00679F60` from 0x67A246, at its translation, after
  `CheckRegionOnScreen` 0x679F75; here the box's sphere **⚠️ approximate**), each chain (`fn_0067B380`, at the
  joint n/2: base +0x44, stride 0x1C, index (n − (n >> 31)) >> 1, 0x67B389..0x67B3A1) and each mist
  (`fn_007FA7F0` from 0x67A782, at mist+0x38: it arrives via `mists::Submit`). The `ZR_SurfRevol` discs do not check
  [0xC0215D] (0x67CBA0 → `RenderParticleGJMesh::DrawAt` 0x67C150 → `Draw3DWorldTriangle` 0x81C090, 0x67C9F2): they are
  drawn **immediately**, in the main view after the models (`Spell::DrawSpells` 0x7203F0 comes after the
  models, from 0x54E023), unsorted. When draining, consecutive sprites in the queue go in a single call if they
  share a material (`DrawParticleSprites`, was `DrawPSysSprites`: same order and same states, the same pixels).
- **`Queued`** (`AddDrawing` 0x55EDC0 → `PSysManager::AddDrawing` 0x6797D0, +0xAE = 0 at 0x6797DE; only the seed in
  the ball, the icon, the reward or the map 0x51A2CA and the containers with `GSpotVisualInfo+0x4C == 1` 0x63E26A; and,
  outside PSys, the fire `FireGraphic` 0x73261D): **one** Z-object per effect with key = `GetOrigin`
  (0x6797E5..0x679834). On draining (callback `fn_00679860`) all its elements are drawn immediately, in the
  order of `fn_006798B0` (0x6798B0..0x679912: the collection's atoms, its chain, the child collections;
  `manager::OrderedEffect::items`): sprites with `LH3DSprite::Draw` (0x67B0DF), meshes with `fn_00679F20` (0x67A458;
  vt+0x104, or vt+0x11C cut with bit 4 of +0x24), mists with vt+0x104 = `fn_007FA790` (0x67A78C: the screen
  test and the Draw), discs (0x67CBA0) and chains with `fn_0067B370` (0x6798DD).
- **`Immediate`** (`Draw_(t, 0)`): the effect of the seed in the hand. `CHand::Draw` 0x46D210 draws, inside the hand's
  Z-object, the mesh (0x46D258), the held object (0x46D27C) and then `DrawSpellInHand` (0x46D2AE → 0x46E680), which does
  `Draw_(1.0, 0)` (0x46E76A): `manager::HandEffects`, drawn like a `Queued` right behind the hand mesh, in the same
  entry.

That is why the rain and fire of a miracle no longer fight with a dispenser's bubble: before, the whole effect went
with its origin's key, in front of or behind the bubble all at once; now each drop and each flame goes with its own. The
dispenser's disc (which belongs to the dispenser's `Sorted` effect, `Draw_(1.0, 1)` 0x722A13) is drawn immediately, before
the whole queue, so the bubble (mode 12, additive and which **writes Z**, 0x82ECA6) adds its light on top, as in the
original. Test: `test_psys_sorted_queue` (key of each sprite and of the chain, a `Queued` between two sprites of the
same `Sorted`). Trace: `OPENBLACK_ORB_TRACE=1` (the discs with their path and their place).

Rain, on the other hand, does **not** go through `PSysManager::AddDrawing`: it carries its own entry per tile with its own
point (table above); giving it an effect's key would be wrong.

**What does not go in, on purpose.** The landscape block order (`LH3DIsland::PreDraw` 0x7FF2D0, opaque, near to far),
the water rings (`LH3DSprite::Draw` **immediate**, 0x5E526C), the villager blobs, the hand's glow in the water
(0x5E4D89) and the sun's glare (`fn_0086BB60`, after the drain, 0x82F4C9). Projected shadows are not Z objects either
(no shadow routine is among the 32 callers): the land ones go with each block (`fn_007FF610` 0x7FF749) and the object
ones at the end of each receiver's Draw (`fn_0080DB30` 0x80E457..0x80E4D7, `fn_00812170` 0x81311A..0x81317C), so they
go immediately with an object drawn immediately (without its mesh's 0x200 flag, 0x815F62) and **inside its Z object**
with a queued one (0x7FA980 → vt+0x108). before, they all went behind the drain and darkened the bubbles, clouds and
sprites in front. The reflection has no queue (in the original it has not been read **🔍 inferred**); it stays as it
was.

### Render modes and materials (render_modes)

**Faithful**, except what is marked. In the original all the blending, alpha test, Z writing and texture stage of a
draw come from **a 16-byte material** (`LH3DMaterial`, `LH3DRender::CreateMaterial` 0x82FD30) and **one of 19 mode
functions** (table 0xC38728). nobody writes the blending or the Z by hand any more.

**The material.** `CreateMaterial(mode, texture)`: `new(0x10)`, `inc [0xECA654]` (0x82FD40), +0 mode, +4 = 0
(ALPHAREF), +5 = 0 (flags: bit 0 two-sided, bit 2 wrapping, bit 4 no UV offset), +8 texture,
+0xC = 0xFF0000FF (0x82FD59; nobody reads it, 🔍 inferred). The texture does not belong to the material: smoke.raw is shared by mode 6
[0xEA1ABC] and mode 13 [0xEA1AC4] (0x80BC7D / 0x80BCB8); atmos.raw, AtmosMaterial (mode 6, +5 |= 1 | 4: 0x835C58,
0x835C6F..0x835C79) and AdditiveMaterial (mode 13, +5 |= 1: 0x835C61). In the API: `Material` (the mode and the flags
+5), `State(material, options)` (the material's culling if the draw does not set another) and `materials::k_Smoke`,
`k_SmokeAdditive`, `k_Misc0`, `k_Atmos`, `k_AtmosAdditive`, used by their eight draws, and `k_InfluenceCircle`
([0xEB9A18], burn.raw, mode 6, +5 |= 1 | 4 at 0x826D18 / 0x826D27: the influence border). The texture and the wrapping
(+5 bit 2, `SetD3DTillingOn/Off` 0x82FF10 / 0x82FF50 with `g_b_need_tilling` [0xECA614]) are set by each draw; in the
L3D meshes, `Primitive::wrap`.

- The flags +5 are also written by `fn_0057D4C0` (0x57D4F0); SetMaterialProperties sets bit 0 (two-sided) at
  0x57E1B7..0x57E1C9. `fn_0057E1D0` applies SetMaterialProperties to every primitive of every sub-mesh (from
  PGetSharedMesh 0x57DF24, when MaterialProperties +3 is 1).
- `g_smoke_mat` [0xEA1ABC]: smoke.raw [0xEA1A98], mode 6, +5 |= bl = 1 (0x80BC8D; bl set at 0x80BBF3). Users: the
  chimney smoke, the mists and clouds (`fn_007FA300` 0x7FA30E), the boats' wake, the SmokyStuff puffs, the influence
  ripples and the halo / puff of the advisor spirits.
- [0xEA1AC4] (mode 13) is two-sided (0x80BCC8). [0xEA1AB0] is misc0.raw, mode 6, made at 0x80BD1B, two-sided
  (0x80BD2B).
- [0xD19C8C] is CreateMaterial(13, misc0.raw [0xEA1A90]) with +5 = 0, one-sided (`fn_005DF9C0` 0x5DFA07): made once by
  PLAY_JC_SPECIAL 0 and freed by `Intro::ReleaseAll` 0x5DFCA3; the intro light.
- AtmosMaterial is [0xEDC368] (made at 0x835C36), used by the rain streaks; AdditiveMaterial is [0xEDC364] (0x835C49),
  used by the hand's glow on the sea (0x5E4281).
- [0xEB9A18] is made on the influence border's first draw (0x826D0E..0x826D2A), CreateMaterial(6, burn.raw [0xEA1A9C])
  with ALPHAREF 0; bit 0x10 is clear, so DrawTriangle adds the UV offset. `fn_0080BBD0` makes another material from
  the same texture, [0xEA1AC0] (mode 6, +5 |= 4 only).

**The inline SetMaterial** (some 105 copies, e.g. `SetupThing::DrawLine` 0x412662..0x4126BD): calls
`g_set_render_mode_data[0xECA618][mode].fn(m, stage 0)`, sets the wrapping and `CULLMODE = ((~m[5]) & 1)·2 + 1`
(1 NONE, 3 CCW). In the API: `Select(mode, table)` and `CullFor(twoSided, mirrored)`.

**The 19 modes** (`k_Modes`, table 0xC38728, dumped from the executable; 2, 6, 9, 10, 11, 16 and 18 reread):

| Mode | Function | Blend | ATEST | ZWRITE | Stage 0 alpha | L3D type |
|---|---|---|---|---|---|---|
| 0 | 0x82D470 | no | no | yes | no texture | Smooth |
| 1 | 0x82D5C0 | SA/ISA | no | yes | no texture (does not touch the stage) | SmoothAlpha |
| 2 | 0x82D820 | no | no | yes | texture | Textured |
| 3 | 0x82D920 | SA/ISA | no | yes | texture × diffuse | TexturedAlpha |
| 4 | 0x82DC20 | SA/ISA | no | yes | texture | AlphaTextured |
| 5 | 0x82DD90 | SA/ISA | no | yes | texture × diffuse | AlphaTexturedAlpha |
| 6 | 0x82DF10 | SA/ISA (0x82DF6A / 0x82DF91) | no | **no** (0x82E063) | texture × diffuse | AlphaTexturedAlphaNz |
| 7 | 0x82D6F0 | SA/ISA | no | no | no texture | SmoothAlphaNz |
| 8 | 0x82DAA0 | SA/ISA | no | no | texture × diffuse | TexturedAlphaNz |
| 9 | 0x82E080 | SA/ISA | yes | yes | texture | TexturedChroma |
| 10 | 0x82E830 | **SA/ONE** (0x82E87C) | yes (0x82E88E) | yes | texture × diffuse | …AdditiveChroma |
| 11 | 0x82E9C0 | SA/ONE | yes | no (0x82EAAF) | texture × diffuse | …AdditiveChromaNz |
| 12 | 0x82EB50 | SA/ONE | no | yes | texture × diffuse | …Additive |
| 13 | 0x82ECD0 | SA/ONE | no | no | texture × diffuse | …AdditiveNz |
| 14 | 0x82DD90 | = 5 | | | | (the land) |
| 15 | 0x82E470 | SA/ISA | yes | yes | texture × diffuse | TexturedChromaAlpha |
| 16 | 0x82E6A0 | SA/ISA | yes | no (0x82E78F) | texture × diffuse | TexturedChromaAlphaNz |
| 17 | 0x82D820 | = 2 | | | | — |
| 18 | 0x82E2A0 | **ZERO/ONE** (SRCBLEND 5 at 0x82E2F3 and 1 at 0x82E320) | yes | yes | texture | ChromaJustZ |

ALPHAFUNC is GREATEREQUAL for all of them (0x82CBA6). No mode touches ZFUNC, fog or culling. The table's word
(1 in the blended ones) is not read by anyone.

- Addresses per mode: 2: ABLEND = ATEST = 0 (0x82D841..0x82D862); 6: blend 0x82DF45..0x82DFA8; 7: ZWRITE 0 at
  0x82D807; 8: ZWRITE 0 at 0x82DBF6; 10: Z at 0x82E91F; 11: blend 0x82EA0C, ATEST 0x82EA1E; 16: ATEST
  0x82E6EC..0x82E700.
- Mode 14 is the land blocks' material (`fn_007FEDB0` 0x7FEDE2, `LH3DIsland::Create` 0x80442A).
- ZFUNC EQUAL (3) is also set by the object fade (0x80EBD6); the frame's LESSEQUAL is SetRenderState(0x17, 4) at
  0x82CCC1..0x82CCC5.

**The global alpha table** 0xC387C8 (`k_GlobalAlphaModes`, `Table::GlobalAlpha`): 0, 1 → 1; 2, 3, 17 → 3; 4, 5 → 5; 9
→ 15; the rest, unchanged. It is chosen by the Draw of an object with its own alpha (vt+0x4C = `fn_007F9D80`, bit 7 of
obj+4; 0x80DF09). The leash also draws through it (`fn_008491B0` switches the table around its one triangle call,
0x84996E / 0x84999F; `world_triangles::SubmitRaw`'s `table`, [drawing the
leash](../creature/leash-rope.md#drawing-the-leash)), with an A of 255 (likely; the object is not read).

**ALPHAREF** (`AlphaRef`): the material's +4, or [0xECA65C] if the switch [0xECA658] is set. With the table
0xC387C8, modes 9 and 15 scale it: `max(0, ftol(ref·A·(1/255) − 5))` (0x82E15C..0x82E1CE, 0x82E557..0x82E5C9;
[0x900058], [0x8AB6E4], [0x8AA398]), with A the alpha of the object's diffuse [0xC37D8C]. D3D tests the alpha that **comes out of
stage 0**: the texture's in 9 and 18 (SELECTARG1, 0x82E120 / 0x82E384), texture × object alpha in 10, 11, 15 and
16 (MODULATE, 0x82E510 in 15). `PrimitiveAlpha(drawn mode, table, ref, A)` gives `fs_object` the two uniforms:
`u_skyAlphaThreshold.y` = ALPHAREF / 255 (−1 without test) and `.w` = the stage alpha (0 none: modes with no blending or
test; 1 the texture; 2 texture × diffuse). The shader discards if `round(a·255) < ref`. The drawn mode is the real
one: the primitive's through the table, or the forced one (`L3DMeshSubmitDesc::mode`), so an additive atom (13) or the
hand shadow (6) has no alpha test, as in the original. Static and physics shadows
(`fs_static_shadow`) use the same `PrimitiveAlpha` with the normal table (🔍 inferred).

- AlphaRef reads [0xECA65C] or +4 at 0x82E166..0x82E17E.
- `SetMaterialProperties` 0x57E120 branch sites: 4 → 6 at 0x57E126; no alpha → 3 at 0x57E138; additive → 13 at
  0x57E142 (`cmp byte [ecx], 1`); the Z test at 0x57E14C; the switch at 0x57E182.

**What each draw sets** (`StateOptions`): ZFUNC (LESSEQUAL 0x82CCC5, EQUAL for the shadows on objects 0x80E488,
ALWAYS for drawing on top), the ZWRITEENABLE 0 written by hand (2D rectangles 0x81E64C, sea 0x879FD9, reflected
land 0x5E48C5..0x5E4900) and openblack's own: writing destination alpha, MSAA, the premultiplied
ONE/INVSRCALPHA formulation of mode 6 (smoke, sprites, land; same colour), the sea that composes the reflection in its
shader and the primitive type. `k_ModelPass` is the opaque model pass. `ModeFromProperties` is
`GJUtils::SetMaterialProperties` 0x57E120 (4 → 6; no alpha → 3; additive → 13; with Z 6 → 5, 13 → 12, 8 → 3, 16 → 9; without Z
5 → 6, 12 → 13, 3 / 2 → 8, 9 → 16), used by `L3DSubMesh` and the particles.

**World triangles, SurfRevol and the influence border** (original).
- `LH3DTech::Draw3DWorldTriangle` 0x81C090: its last argument 0 takes `g_world_to_clipping` as it is
  (0x81C09F..0x81C128; `RenderParticleGJMesh::DrawAt` sets the identity world matrix first, 0x67C8ED..0x67C979); no
  specular (0x81C2BF); `g_NoBackfaceCull` = material +5 bit 0 (0x81C30C..0x81C319); CULLMODE ((~m[5]) & 1)·2 + 1
  (0x81C556..0x81C58F); SetMaterial through the current table (0x81C48E..0x81C4A0); ONE DrawTriangle for the whole
  primitive (0x81C5B1), at once, no Z object. An exploded piece's GJMesh +0 is its source primitive (0x680C49).
- Its indexed sibling `fn_0081C780` is called by FragMesh (`fn_007F7ED0` 0x7F86D8 / 0x7F8730) and
  `RenderParticleGJMesh::DrawAt` (0x67CAEE): the same T&L with `g_world_to_clipping` (0x81C783..0x81C80A), culling
  0x81CA0A..0x81CA1C and 0x81CC57..0x81CC90, SetMaterial 0x81CB79..0x81CBA1.
- `RenderParticleGJMesh::DrawAt` takes `fn_0081C780` instead of Draw3DWorldTriangle (branch 0x67C9CD) when the
  primitive has as many speculars (+0x30, count +0x38) as colours; ZR_SurfRevol sizes both to NumU × NumV
  (0x685A0E..0x685A3D), so the SurfRevol discs always go that way. With a DrawData alpha ≠ 0xFF DrawAt selects the
  table 0xC387C8 (0x67C9B7..0x67C9C0), which leaves modes 5, 6, 12 and 13 unchanged.
- The SurfRevol material: CreateMaterial(6) + SetMaterialProperties (`ZR_SurfRevol::ModifyAtomCollection` 0x6863EC /
  0x6863F9): 13 / 6, or 12 / 5 with MaterialUpdateZBuffer; colour = texture × diffuse + specular, alpha = texture
  alpha × diffuse alpha.
- `RenderParticleGJMeshRotatingUV::DrawAt` 0x67CBA0 never reads [0xC0215D]: a surface has no Z object of its own on
  any path.
- The influence border: `InfluenceCircle::Draw(1)` 0x826C90 is called once a frame from `GGame::Process3dEngine`
  (0x54E3CB..0x54E419, the call at 0x54E414), after everything drawn at once and before FinishFrame's drain; the
  camera gate and the middle row's alpha come from g_camera.y [0xEA1DBC] (0x826CA9..0x826CF3); the middle rows' alpha
  is set at 0x826F0D..0x826F57; [0xEA9EA0] = a copy of `g_world_to_clipping` (0x826D8F..0x826E11);
  Draw3DWorldTriangle(3N + 3, positions, colours, uvs, 4N, indices, [0xEB9A18], 1) (0x826F59..0x826F80) draws every
  circle, the invisible ones too; [0xECA628] = 0 at 0x826F8B.
- A rain tile's streaks (`fn_00833DC0` 0x833DC0): each streak runs from (x, −50, z) to (x + dx, elevation, z + dz)
  around the tile's origin, its u from its scroll to scroll + 1 along row 129 of the texture (v 0x3F010000); the
  colours are white and `fn_00834370` sets the alphas, the bottom's and the top's, times the streak's phase fade.
- The rain material is `LH3DAtmos::AtmosMaterial` [0xEDC368] (`fn_00835AD0` 0x835C36): atmos.raw [0xEDC370], mode 6, +5 |= 1 | 4 (0x835C58, 0x835C6F..0x835C79).

## openblack

Status: **done**. One transparent queue, `graphics::zsort` (`src/Graphics/ZSort.{h,cpp}`, Renderer core), and one
render-mode table, `render_modes`, that every draw asks for its bgfx state.

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **The single transparent queue (LH3DZSorter)**
  - openblack: [The transparent queue in openblack](#the-transparent-queue-in-openblack)
  - openblack (`Renderer::DrawPass`): `instancedDrawDescs` and `translucentDrawDescs` by the mesh's flag (and the
    bubble by its `sortPoint`)
  - openblack's held object is not drawn from that entry **(inferred: it does not change anything, it is opaque and
    goes first)**.
  - openblack does the same (`Renderer::DrawShadowsOnObject` behind its `DrawMesh`, in Main or from the drain; see
    [Dynamic shadow on objects](shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects))
- **Render modes and materials (render_modes)**
  - In openblack all those draws ask a single API for their bgfx state, `openblack::graphics::render_modes`
    (`src/Graphics/RenderModes.{h,cpp}`)
  - In openblack, the objects with `components::Alpha` (in the queue if their mesh has the 0x200 flag, otherwise
    immediately) and the translucent PSys mesh atoms (`L3DMeshSubmitDesc::table`).

### The transparent queue in openblack

**The API.**

| Function | Original |
|---|---|
| `Queue<Item>::Begin()` | `fn_0083F3B0` (and the `[0xECA610] = 0` of `StartFrame` 0x82F123) |
| `Queue<Item>::Submit(item, key, user = 0)` | `NewZObject` 0x83F310: stable insertion, cap `k_Capacity` = 0x800 |
| `Queue<Item>::Drain()` | `fn_0082F280`: the entries from far to near, with their K; empty if already drained |
| `Key(p, camera, order)` | `GetValueForZSorter`, in `float`, with the caller's summation order |
| `PackRainUser` / `UnpackRainUser` | 0x834233..0x834259 / 0x833F83: alpha·65536 + 256·trunc(z/80) + trunc(x/80) |

**What changed when unifying** (fixes read from the binary):

1. Everything is stable: the clouds' own sorting and that of the fallback mists were `std::sort` (0x83F36A).
2. Cap of 0x800 with the new one lost (0x83F315/0x83F31C).
3. The clouds go in the queue, mixed by distance with everything (before, always on top; 0x7FA87B, 0x5E2813).
4. The chain ribbons, now inside their effect's object (`fn_006798B0` 0x6798DD), go through the API.
5. The rain goes in the queue, one entry per tile (before, a group behind the list; 0x83427F).
6. The boat sprites, each one in the queue (before, a batch behind the list; 0x840CB3).
7. The hand always goes to the queue and whole (before, its opaque parts in the main view and only the blended ones in the
   list, and only if it had any; 0x46D1B7..0x46D203 tests nothing).
8. PSys effects by their path (above); models to the queue only by their mesh's 0x200 flag, whole.

In addition the key is the squared distance in `float` and not the distance: the same order except for keys that the square root
merged.

### The render modes in openblack

**Who uses it.** L3D meshes (`L3DSubMesh` and `Renderer::DrawSubMesh`: the material type is the mode); the clouds,
mists, smoke, boat, rain, blobs, fish, rings, sun, moon, sea, hand glow in the sea, land, hand
text, screen fade, sprites and the PSys sprites, chains and surfaces, the hand shadow on objects (all
primitives in mode 6 with ZFUNC EQUAL), the additive PSys atoms (mode 13) and the mesh viewer. Left out, because
they are openblack's own techniques with no original mode: the MAX / MIN passes of shadows and rivers and the footprints.

**Fixes when unifying** (before, openblack's table and the branches of `DrawSubMesh`):
1. Modes 10 and 11 came out SA/ISA (now SA/ONE) and 11 wrote Z.
2. Mode 16 wrote Z.
3. Mode 18 painted colour with SA/ISA; now ZERO/ONE, Z only.
4. ALPHAREF: the −5 was always applied and without the A/255 factor (`fs_object`, `fs_static_shadow`); now it is the exact +4
   except 9 / 15 with the table 0xC387C8.
5. The Z-less primitives (6, 7, 8, 16) of a fading object wrote Z.
6. Entries 14 and 17 of the table were {no blending, no Z}; now 14 = 5 and 17 = 2 (latent: there are no L3Ds with those types).
7. The hand shadow on objects set SA/ONE on additive primitives (12, 13); now they all go in mode 6
   of the shadow's material: `fn_0080B050` 0x80B06A..0x80B08B does the SetMaterial of [si+0x460] (`CreateMaterial(6)`
   in `fn_0087FD50` 0x87FE12) through the current table, and `fn_0084E200` draws each primitive with no state of its own.
8. A chroma primitive with ALPHAREF 0 (or a 9 / 15 that fades with `ref·A < 1530`, which gives 0) blends with its
   texture's alpha (mode 9: SELECTARG1 0x82E120, SA/ISA); before it came out opaque.
9. The hand shadow on objects with its material's culling: +5 = 0 (0x87FE12) gives CULLMODE CCW
   (`fn_0080B050` 0x80B0AD..0x80B0E6) on all primitives; before, two-sided ones also received it from behind.
10. The alpha test and the stage 0 alpha are those of the drawn mode (see ALPHAREF): a fading 9 (→ 15) tests
    texture × A (before, the texture's raw alpha: with ref 0x96 and A = 128 texels from 70 were kept instead of
    from ~140); a fading 2 (→ 3) blends with texture × A (before, only A); in an additive atom (13) types 0 / 2
    use texture × diffuse (before, the whole quad) and the chroma ones lose the alpha test, the same as under the hand
    shadow (6).

In `AllMeshes.g3d` there are no primitives of types 10, 11, 16 or 18 (the counts in [L3D material
blending](textures.md#l3d-material-blending)): fixes 1-3 are seen in the miracle meshes and in those that go through
`SetMaterialProperties` (a Z-less `TexturedChroma` becomes 16). Fix 4 is seen on the edges of all the chroma ones (trees with
cut-off 0x96: a little thinner).

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/L3DSubMesh.cpp`: `L3DSubMesh::SetMaterialProperties`.
- `src/Graphics/RenderModes.cpp`: `Mode::AlphaTexturedAlphaAdditiveNz`, `Mode::AlphaTexturedAlphaNz`, `Mode::TexturedAlpha`.
- `src/Graphics/RenderModes.h`: `Blend::Additive`, `Blend::Disabled`, `Blend::Standard`.
- `src/Graphics/RendererBoat.cpp`: `Renderer::DrawBoatSprite`.
- `src/Graphics/RendererInfluence.cpp`: `Renderer::UpdateInfluenceCurtain` does the influence border's per-frame work
  before its draw (the camera gate and the middle rows' alpha from the camera's height, the material made on the first
  draw, the scroll clock that advances with or without circles); `Renderer::DrawInfluenceCircles` draws every circle,
  the invisible ones too, in plain world space, with no fog, haze or specular and no culling, the scroll offset kept
  for that draw only.
- `src/Graphics/RendererRain.cpp`: `Renderer::CollectRain` queues one entry per rain tile with the distance key
  (`zsorter::SumOrder::XZY`), carrying the tile by its index rather than in K (see [Pending](#pending));
  `Renderer::DrawRainTile` draws a tile's streaks as `fn_00833DC0` does.
- `src/Graphics/RendererSurfRevol.cpp`: `Renderer::DrawPSysSurface`.
- `src/Graphics/WorldTriangles.h`: `SubmitRaw`, `Vertex::specular`.
- `src/Tests/test_render_modes.cpp`: `Mode::ChromaJustZ`; `src/Tests/test_psys_draw_path.cpp`: `DrawPath::Queued`.

### Test hooks

- The `OPENBLACK_ORB_TRACE` trace still writes the distance (the square root of the key), as before. New trace:
`OPENBLACK_ZSORTER_TRACE=1`.
- The model rendering pages' hooks (the orb and Z-sorter traces, the render-mode scenes) are listed in
  [Test hooks](model-lighting.md#test-hooks); the full list is in
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

### The transparent queue: approximate

- The arrival order (the tie-break with equal keys) is not the original's: here clouds, models per mesh (a
  `std::map`), faded ones, sprites, meshes and chains of the `Sorted`, `Queued` effects, sprites, mists, smoke,
  rain, boat, influence ripples; there the order of the frame's AddDrawing calls ([draw stages in order](frame.md#draw-stages-in-order),
  steps 4m..22).
- The rain point is `tile.origin` with the height from Rain.cpp's `LandHeightAt`, not `GetAltitude` 0x803090 on
  `MapCoords(x × 65536 × 0.1, z × 65536 × 0.1)` (0x8341D2..0x834210).
- `CheckRegionOnScreen` of a `Sorted` mesh (0x679F75) is its box's sphere; that of a model (0x815AB1) is not
  done: a model outside the view takes up a slot of the cap that the original does not spend.
- The animated meshes of a `Sorted` (`Particle3DAnim::DrawAt` 0x67A9D9 → `fn_00813340`: to the queue only with bit
  0x10, 0x80 or +0xB8, otherwise immediately) go like the other meshes **(not ported)**.
- The rain travels by its index and not by K: `CollectTiles` already applies the distance fade of fn_00834370, so
  a K built from that alpha would not be the original's.
- The blended primitives of a model drawn immediately go to the `MainBlended` view (`DrawSubMesh`), in front
  of the whole queue but behind everything opaque in the main view; in the original, at the place of its Draw.

### Render modes: inferred and approximate

- 🔍 inferred The object's alpha as a byte: `AlphaByte` rounds `1 − [0][3]` of the instance.
- 🔍 inferred The alpha test comparison in rounded bytes (`fs_object`, `fs_static_shadow`).
- 🔍 inferred The moon without the Z of mode 4 (0x82DC20) and the sky with its meshes' modes.
- 🔍 inferred The modes of the chains (`fn_006AA860`, not read) and of SurfRevol's specular pass (13).
- 🔍 inferred The water rings and fish shoals with [0xEA1AC4] / [0xEA1AB0]: only the references 0x54BA72 and
  0x8247CC, not decoded inside the routine.
- ⚠️ approximate The fish with ZFUNC ALWAYS: the original leaves LESSEQUAL (0x82CCC5); it makes no difference because the reflected
  land below did not write Z (0x5E48C5).
- ⚠️ approximate The sea without culling: its material (+5 = 4, 0x5E5474; `GLandscape::Draw` passes it [this+4] at 0x5E4E85)
  gives CULLMODE CCW (0x879F54..0x879F86), which keeps all the rows of the original's sea; openblack's sea mesh
  is not those rows.
- ⚠️ approximate With the "texture" stage alpha (4, 9, 18) `fs_object` still multiplies it by the object's alpha; in
  the original it is the texture's alone (SELECTARG1). It is the same as long as the object has alpha 255 (outside the
  fades, which switch to 5 / 15, and the plane cut).
- The forced ALPHAREF of its writers: the burning tree's (`fn_0074B3A0` 0x74B4D6..0x74B51E, 180..254) is ported
  (`L3DMeshSubmitDesc::forcedAlphaRef`, [fire](../world/fire.md#fire)); not ported: `TownArtifact::Draw`
  0x51CBB3, the fixed cut-off of 10 in `fn_005E6350` 0x5E649C, `Scaffold::Draw` 0x6EA730, `fn_00826470` 0x826488,
  FragMesh `fn_007F7960`. `AlphaRef` and `PrimitiveAlpha` accept it.
- Object fading by alpha test (`fn_0080E940` and copies 0x80F0A0 / 0x80F3D0) is another system; openblack does
  the crosshatch pattern of `fs_object`.

### The boat

- The missionaries' boat (`ecs/MissionaryBoat`): its sprites (wake, `DisappearSmoke`) already go one by one in the common
  transparent queue (`Renderer::CollectBoatSprites` / `DrawBoatSprite`, LH3DSprite::AddDrawing 0x840CB3; see
  [the single transparent queue (LH3DZSorter)](#the-single-transparent-queue-lh3dzsorter)); the mode ≠ 0 of
  `SmokyStuff::Create` (branch 0x823DA7) is not yet read.

### From the model rendering notes

- Transparent queue, not yet seen or read: before and after captures (clouds against models and mists, rain, boat,
  hand); the reflection; the animated meshes of a `Sorted` (fn_00813340).
- Transparent queue: the original tests `CheckRegionOnScreen` before queuing a model (0x815AB1), so an off-screen
  model is not queued; openblack queues it (queued fix).
- Transparent queue: the original sends LightSheet, HandGlow fn_0083F100, VillagerName, ValueSpinner, PowerSpin,
  LandscapeVortex, PlayerSymbolSprite, DrawLiquidParticles, fn_006CA930, Gooloo and the two with key 0 through
  LH3DZSorter, sorted with the rest; openblack draws those it has outside the shared queue, each caller to move to
  `Submit` on its own (queued fix).
- `world_triangles::SubmitRaw`: Draw3DWorldTriangle in a CreateMaterial'd `.raw` material (program WorldTextured, `X.raw`
  + `Xa.raw`), used by the influence border and its ripples. `RendererRevolvedSurface.cpp`, `RendererBoat.cpp` and the
  other WorldTextured draws still build their own transient buffers and can move to it.
- Question for the user (transparent queue): **flames behind trees** (only meshes with the 0x200 flag go to the queue,
  SetMesh 0x7F9E48 / AddDrawing 0x815F0B): trees are now drawn immediately and with Z, so the flames of a tree at the
  back are hidden by the foliage of the ones in front, and above they look lighter and without the dark smoke. Was it
  like that in the original?
