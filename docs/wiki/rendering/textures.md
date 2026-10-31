# Textures

## Summary

The original keeps most of its textures in 16 bits: those created with the alpha flag 0x40 in ARGB4444 (the high
nibble of each byte, cut at load time, so the filter works on nibbles already cut), the others in RGB555; the L3D
skins come already in 4444. A material's type picks its render mode, and its byte +5 decides culling and wrap or
clamp. Frame-animated textures never blend two frames: each user picks a whole frame from its own clock, and what
looks smooth comes from many frames and scrolling UVs.

openblack cuts the same textures at load time, takes the material states from the render-mode table, and keeps one
function per original clock in `graphics::frame_anim`.

## The original

### L3D material blending

**Faithful**.

- The hand (`Hand_Boned_Base2`, material `AlphaTextured`) has an alpha gradient in the bottom rows of its skin:
  the wrist fades out. It used to end in a hard white edge.
- Meshes in `AllMeshes.g3d` by material type: `Textured` 512, `TexturedChroma` 217, `Smooth` 181,
  `AlphaTextured` 116 (almost all the `MSH_B_*` buildings), `TexturedChromaAlpha` 6 (bushes, palm trees).
### Texture wrapping or clamping

**Faithful** (done).

- `fn_00850FC0` 0x851779: after setting the mode, `SetD3DTillingOn` if `g_b_need_tilling` (0xECA614) or bit 2 of byte
  +5 of the material; otherwise `SetD3DTillingOff` (CLAMP). `g_b_need_tilling` is copied on each Draw of the 3D object from its bit
  0x200 of Flags1 (vt+0xE4 = `fn_007F9B30`), and that bit is only set by the setter vt+0xE0 from particle meshes
  (`ParticleMeshCreatorAnimTextured`, `ParticleVolBlendMeshCreator`): world objects depend only on the material.
- `AllMeshes.g3d`: 1675 primitives with the bit, 161 without it; none of the latter has UVs outside 0..1, so clamping
  only changes bilinear filtering at the texture edges.

### Frame-animated textures

**Faithful**, except what is marked. **The original never blends two frames.** Each user picks a whole frame
(`__ftol`, which truncates, or an integer division; only the hand's glow uses `fistp`, which rounds) and draws only
once. What looks smooth comes from two things: many frames, 15 to 25 per second, and continuous UV
offsets (scrolling). PSys interpolates the frame **number** between two steps and then truncates it: that is
a step, not a blend.

There is no common clock either: each user keeps its own accumulator (per object, or global where the original has it
global) with its own constants. That is why openblack's API has **one function per original clock** and the state is
kept by the caller, in the place where the original keeps it
([Frame-animated textures in openblack](#frame-animated-textures-in-openblack)).

The clocks are computed in float because the original runs the FPU at 24 bits: `fn_007DEE00` clears the precision bits
(see [the script camera and camera tracks](../camera/script-camera.md)). So each fadd, fmul or fsub on the stack rounds like a float operation,
and the value left on the stack between an `fmod` and the `__ftol` is the float one. That is why, when the `fmod` gives a tiny
negative, the vials' "+ 32" rounds to 32.

| Clock | Original | Rule | Users in openblack |
|---|---|---|---|
| `OneOffFrame` | OneOffSpellSeed::UpdateFrame 0x72A570 | phase = fmod(phase + ms·18·0.001, 16) ([0x981FB4], double [0x982820]); cell (f % 4, f / 4)·0.25 ([0x981FB8]) | the bubble (`one_off::UpdateFrames`) |
| `SpellIconFrame` | DrawSpellGraphic 0x519AD0, vials branch (0x519B79..0x519C1B) | +0x34 = fmod(+0x34 − 15·dt, 32), +32 if < 0 ([0x8D86F0], [0x8D8740], [0x8CF134]); u = (f % 8)·(1/256)·32, v = (f / 8)·(1/256)·32 | the creature spell vials (`seed_graphic::DrawSpellGraphic`) |
| `HandFlowFrame` | PHandFX::Draw 0x68D0C0 (0x68D29B..0x68D374) | +0x58 += dt·(−20); with rate > 0, fmod(.., 64) if it exceeds 64; with rate ≤ 0, fmod(.., 64) + 64 if < 0; f = **fistp** (rounds) % 32; (f % 8, f / 8)·0.125 | the hand's glow (`hand_fx`, computed and not drawn) |
| `ParticleFrameAdvance` | AtomCore, fn_00673EA0 (0x673FB8..0x6740D8) | prev = cur; **only with [0xC029DC] and PlayAnim (+0x118) = 1** (otherwise, 0x673FC6..0x673FDE jump to 0x67406A: neither step nor wrap), cur = dt·rate + prev; with rate > 0, while both exceed 2N, −N; with rate ≤ 0, while either is < 0, +2N | all atoms (`Effect::PostUpdate`, with `Atom::playAnim`), the dust, grains and fish of `HandEffects` |
| `ParticleFrameLerp` / `ParticleFrameIndex` | fn_00679920 (0x679A7D..0x679B61) | t' = clamp(t, 0, 5) ([0x9357B8]) when looping and clamp(t, 0, 1) without it; f = prev + (cur − prev)·t'; looping ftol(fmod(f, N)) (+N first if negative), without loop ftol(f) in 0..N−1 | PSys sprites, AnimTextured meshes, TownBelief, FireGraphic, light maps |
| `MistCell` / `MistCellUv` / `MistAdvance` | LH3DMist, fn_007FA300 | +0x84 += ftol(ms·0.255) ([0x9A2BA8] = 0x3E828F5C; `fild g_game_time_inc`, `fmul`, `__ftol` at 0x7FA5FC..0x7FA608: the fraction is dropped every frame), %= 900 if > 900 (0x384); cell (c·45/900) & 15; UV ((f & 7)·0.125, (f >> 3)·0.125 (+0.25 in the effect branch, 0x7FA466; without it, 0x7FA69E)) | map mists, clouds, storm puffs, PSys mists |
| `MistCell` / `SmokeAgeStep` | LH3DSmoke, fn_007F8E00 | dt = min(ms·0.001, 100) ([0x8AC418], [0x8AB41C]; 0x7F8F25..0x7F8F56); one step ftol(dt·255) ([0x8AB270]; 0x7F8F79..0x7F8FE5) added to every puff (0x7F901E): the fraction is dropped every frame, so a frame under 4 ms leaves the ages; cell = MistCell(age) | chimney smoke, the creature room's smokes (`CreatureRoom::Draw` 0x78697B, 0x786993, with `g_game_time_inc` = `g_delta_time` 0x786868) |
| `FireCell` | fn_007321B0 | ftol(fmod(−25·age, 32) + 32) ([0x999668]); age = SpritePos +0x2C | FireGraphic flames |
| `SteamCell` | SteamGetOffsetFromAge 0x7321E0, fn_0073250A | ftol(fmod(25·age, 32)) ([0x99966C]) | FireGraphic steam and grey smoke |
| `FishFrame` | fn_008248E0 (0x824960..0x8249CE) | dt = min(dt, 0.1) ([0x8AB22C]); +0x1C += dt·vel·25 ([0x8C7BD0]); cell (8 + (ftol & 15)) & 63 **before** the wrap; then −= 15·ftol(+0x1C·(1/15)) | fish-farm fish |
| `LanternAdvance` / `LanternStart` / `LanternCell` | fn_00823570 (0x823599..0x82362D); fn_00823240 (0x8233E4..0x8233F8) | **global** clock [0xEB99C4] += ms, %= 700 if > 700 ([0xC383C8]); a = c·31/700; flame i: (10i + 31 − ((start[i] + a) & 31)) & 31. Start = the **global** table 0xC383BC ({0, 13, 0} in the file): each new light writes ftol(Random(0, 31)) into its three entries, so all lights use the values of the last one created | lanterns and bonfires (`night_lights`) |
| `LeashCell` / `LeashScroll` | fn_00466730 (0x46690A..0x466955, 0x466855..0x46687B) | +0x74 += 10·dt, −= 15·ftol(+0x74/15), cell ftol & 63; +0x60 += 0.5·dt, −= ftol | not ported (citadel leashes) |
| `GoldenShowerCell` | fn_006CA990 (0x6CAB1F..0x6CAB5B) | (t/50 + base + drop) % 32, signed | the gold scrolls' sparkles and the objects' golden showers (`script_highlight::SparkleSpriteOf`) |
| `CreatureRoomCell` | CreatureRoom::DrawAdditional 0x788630 (0x7889A5..0x7889CC) | 31 − (((GetTickCount() >> 5) + i) & 31): **real** clock | not ported |
| `CursorCell` | CameraModeNew3 0x456BED | (GetTickCount() / 50) & 15: real clock, 20 per s | not ported (3D cursor) |
| interface pointer | fn_0053C0F0 (0x53C0F7..0x53C103), drawn from the frame finish callback 0x6424E0 | table 0x8DE928 {9, 10, 11, 12, 41, 49, 57, 23} by (GetTickCount() >> 5) & 7: **real** clock, also while paused and in the temple; its tooltip fades by the real frame time | `GameInterface::Draw`, timed by `game_clock::TickCount()` |
| edit box caret | SetupEdit::Draw (0x40CB5B..0x40CB70) | shown when (GetTickCount() / 200) is odd (magic 0x51EB851F >> 6, bit 0): **real** clock, 200 ms hidden then 200 ms shown, a 400 ms period | `gui::IsCaretShown`, fed by `game_clock::TickCount()` |
| `HelpSystemCell` | fn_005C0700 (0x5C0A7A..0x5C0AAF) | [0xD15AB0] += fn_005557E0(); (c / 200) & 15 | the help spirits' halo (`help::spirits::Runtime`, `SpiritsRuntime.cpp`) |
| `InTheCloudsCell` | fn_00828A70 (0x828CDE..0x828D7A) | f += ms·0.01; if f > 15, f = 0 (no fmod); ftol & 15 | not ported |
| `PlayerSymbolCell` / `PlayerSymbolSpin` | PlayerSymbolSprite::Draw 0x69D7E0 | layer 0: +0xC −= ms·0.02 ([0x937538]); layer 1: +0x10 −= ms·0.023 ([0x937534]); +32 while < 0; ftol & 63. Spin of the second sparkle: +0x14 += ms·0.002 ([0x92A544]), −2π while > 2π. The ctor fn_0069D5A0 sets them to 0 | TownBelief sparkles (one accumulator per symbol) |
| `DisappearSmokeCell` | fn_00823F70 (0x8240F9..0x824115) | ftol(life·15) & 63 | DisappearSmoke puffs |
| `DustCell` | fn_00846010 (Dust.cpp) | 16 + ((rand % 16 + ftol(2·age)) & 15) | impact dust |
| `InfluenceScroll` | InfluenceCircle::Draw 0x826D2D..0x826D83 | **global** clock [0xEB9A40] = (c + ms) % 10000 (signed); u = c·0.0001 ([0x9A391C]), v = −c·0.0002 ([0x9000DC]); only on frames that pass the camera gate (y > 100) | the influence border (`RendererInfluence.cpp`) |
| `WaterfallScroll` | DesignedWaterFall 0x5E392E..0x5E3972 | V −= 0.5·dt ([0x8AA3B4]), minus its integer part; SetAnimatedUV_1(0, V) | the Land 3 waterfall |
| `GhostFrame` | fn_005E6390 | t from 500 ms to 0; x = t/500; UV (2·cos x, 1.7·sin(0.7·x)); material byte +4 = 255 − ftol(255·t/500) | ported: `ecs::object_ghosts` (the ghost of an object a store took, a merged pot, a hand pot put down, a building an effect destroyed, a tree sacrificed); the byte is the mask's ALPHAREF (`object_ghosts::Passes`) |
| `RotatingUv` / `RotatingUvClock` | RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 and GameUpdate 0x6C8BC0 | Draw: lerp(+0x24 → +0x2C, t), lerp(+0x28 → +0x30, t) with t = DrawData +0x14 (0x67CBA8..0x67CBC1); the period (+0x3C, +0x40) is subtracted while it exceeds it (0x67CBC8..0x67CBFC; nothing if negative). Step: the rule adds dt·SpeedU/V to the **target** +0x34/+0x38 and `GameUpdate`, at the end of `PostUpdateAtoms` fn_00673EA0 (0x674080, `vt+0x108`), raises +0x34 and +0x2C by one period while both are below −2·period (0x6C8BDF..0x6C8C5A) and lowers them while both exceed +2·period (0x6C8C5B..0x6C8CCC) — they move as a pair, so the difference the draw interpolates does not change —, and then copies +0x2C → +0x24 and +0x34 → +0x2C (0x6C8CCD..0x6C8CE2) | SurfRevol discs |
| `ChainScroll` | fn_0067B3F0 (0x67BE88..0x67BED5) | +0x3C += ms·rate·0.001, fmod(FrameHeight/256), + that if < 0 | PSys chains |
| `ChainSegmentUv` | fn_006C8920 | see "Chains" below | PSys chains |

- Constants: 0.001 [0x8AA3B0] / [0x8AC418]; 1/256 [0x8D86CC], [0x9357A8], [0x938EBC]; 0.125 [0x8AB620]; 1/15
  [0x8C9D38]; the influence's 10000 is an idiv at 0x826D2D..0x826D59.
- MistCell: c·45/900 is computed as × 0x91A2B3C5 `sar 9`; c = 900 is kept (the wrap is c > 900), so c/20 reaches 45:
  the cells run 0..15, 0..15, 0..13 (14 at c = 900) and back to 0, and cells 14 and 15 are skipped once every 900.
- HandFlowFrame: the rate is +0x5C (−20) and N the byte +0x60 (32); the wrap is at 2N. The original writes the result
  into the hand mesh's vertices, not through SetAnimatedUV_1.
- ParticleFrameLerp: looping is +0x119 LoopAnim; ParticleFrameIndex's result goes to DrawData +0x10.
- RotatingUv only runs while [0xC029B8] = 1 and writes [0xECA62C] / [0xECA630] with the tiling forced; its t =
  DrawData +0x14 is the draw fraction of the step that `PSysManager::AddDrawing` 0x6797D4 keeps at +0xB0. The chains'
  scroll is also gated by [0xC029B8].
- Lanterns: the village light alpha is [0xEB99BC] and the light list [0xEB99B8].
- Leashes: LeashCell runs from `CitadelHeart::DrawNow` 0x46733D; LeashScroll calls SetAnimatedUV_1(+0x60, the colour
  row n × 0.125, or 0.375) at 0x4668A8; constants 10 [0x8C8404] and 0.5 [0x8C8400].
- HelpSystemCell: `fn_005557E0` gives `g_delta_time` capped to 500 when g_game+0x205A28 is 1, else `g_game_time_inc`;
  the cell is (c/200) & 15 (× 0x51EB851F `sar 6`), 5 a second.
- Gooloo (GoolooGooloo 0x5E6540 → `fn_005E6390`), a dissolve in two passes: first every primitive with the material
  [0xEA1AB4] (`CreateMaterial(18, cool_effect.raw)` 0x80BCE1: mode 18, only Z where the alpha test passes, wrapped,
  two-sided; the update flag [0xC38714] off so the model's own materials are not set), its ALPHAREF (+4) 255 −
  ftol(255·t/500) and the moving UV offset; then the model's own materials at (0, 0) with the ALPHAREF forced to 10
  ([0xECA65C] under the switch [0xECA658], read as D3DRS_ALPHAREF by every alpha tested mode, not a render mode) and
  ZFUNC EQUAL (0x5E6495..0x5E64CB), so only where the first pass wrote Z; the timer falls by `g_game_time_inc` each
  draw. before it the ghost was the model's materials drawn twice and never dissolved. Its material byte +4 is
  [0xEA1AB4]'s; constants 500 [0xBF3588], 1.7 as the double [0x92B338] (1.7000000476837158) and 0.7 [0x900AE0]. Callers:
  `Object::DoDeleteObjectAndTakeResource` 0x63AAA3, `Pot::ApplyThisToObject` 0x66DE4C (a pot merged or put down),
  `MobileStatic::ApplyThisToObject` 0x608C72 (a gate stone), `Tree` 0x74BE21 and `DeadTree` 0x510FDF (the sacrifice,
  `altar_offering::Offer`), `Abode::DestroyedByEffect` 0x403F86
  (the only `DestroyedByEffect` that calls it, `callers.py 5e6540`; never reached in the check runs). `magic::Update`
  lowers the time by the frame's game ms (`frame_anim::GhostStep`; 🔍 inferred: the game step, not the real one);
  `RenderingSystem::CollectMeshAtoms` draws each ghost as two Sorted mesh atoms, the second with
  `mesh_atoms::Instance::mode` = 10 (AlphaTexturedAlphaAdditiveChroma). **Approximate**: the material byte is not used
  as the alpha reference, and the two draws are two Z objects at the same point (the original draws them one after the
  other).
  The mask is cool_effect's alpha: the game makes cool_effect one ARGB4444 texture of cool_effect.raw and
  cool_effecta.raw (cool_effect is one of the alpha-flag stems), and the first pass's alpha test cuts the holes with it.
  openblack (2026-10-27) packs the two the same way for the ghost (`object_ghosts::LoadMaskTexture`, through the
  colour-and-alpha loader); before, the ghost's mask was `raw/cool_effect` alone, a texture of the colour file whose
  alpha reads 1 everywhere, so no texel failed the test and the ghost showed whole until it went.

**Chains.** fn_006C8920 gives the UVs of segment i of S = joints − 1:
- k = ((i + 1)·T − 1) / S, b = k·S / T, n = (k + 1)·S / T − b and j = i − b, all in integers. T is NumTexturesForWholeChain,
  or S when it is −1 (CreateChain 0x6AA8DC).
- F = FileOffset + (k = T − 1 ? FrameOfHead : k = 0 ? FrameOfTail : 0).
- uv0 / uv1 = (F·W, H·j/n) and ((F+1)·W, H·j/n); uv2 / uv3 = the same with j + 1. All × 1/256 ([0x938EBC]) and the scroll
  +0x3C added to the four v.
- That is, **V runs along the chain and U crosses the ribbon** over a column of W pixels. The creator's default
  values are FrameHeight 64, FrameWidth 32 and NumTexturesForWholeChain −1 (0x6AA739..0x6AA747).
- The scroll rate (+0x4C) is only set by UR_SimpleBeam (SpeedV +0x48, 0x6762EE) and `UR_Plasma::CreateArc` (its
  AtomData +0x54, not read, × SpeedV +0x58, 0x676898); the chain ctor 0x6C8830 sets +0x3C = +0x4C = 0. UR_SimpleBeam is
  ported (`Collection::chainScrollRate`); UR_Plasma is not, so every other openblack chain scrolls at 0.
- Chain sheet fields: +0x1C FrameHeight, +0x20 FrameWidth (ctor 0x6AA740: 0x20), +0x24 FrameOfHead, +0x28
  FrameOfTail, +0x30 the textures over the whole chain, +0x34 FileOffset (copied by CreateChain 0x6AA8B8..0x6AA8EB).
  fn_006C8920 divides by S (idiv 0x6C8936) and T (0x6C893E, 0x6C8949) unchecked, and by n in float (0x6C896B,
  0x6C8971); DefineProperties allows −1..32 for T, so T = 0 would fault there.
- fn_0067B3F0 makes four vertices per segment (0x67BA82..0x67BB0D): v0 = head + side, v1 = head − side,
  v2 = tail + side and v3 = tail − side. It gives them uv0..uv3 (0x67BEE4..0x67BEFD). fn_0081C780 copies each vertex's UV
  as is into LH3DP3::Table1 +0x18 / +0x1C (0x81C9C7..0x81C9D0) and draws with DrawTriangle 0x82F810 (0x81CCB2). That is
  why U = F·W goes on the +side.
- Then 0x67BFAC..0x67BFEE puts the same scroll in [0xECA630] ([0xECA62C] = 0, [0xECA628] = 1), and DrawTriangle
  0x82F8BE adds it again: the material of fn_006AA800 only has bits 0 and 2 of +5. (An earlier reading said that the
  strip used fn_0082F920. That is not so: the call at 0x81D152 is in another function, the one at 0x81CCD0,
  because fn_0081C780 ends at 0x81CCBE.)
- The FPU is set to 24 bits by `fn_007DEE00` at start-up (0x641D7E), at the start of every turn and in the frame; the C runtime's 53-bit default (`__setdefaultprecision` 0x7CC962) only holds before it.
- The lanterns' start table is the three entries 0xC383BC, 0xC383C0 and 0xC383C4; each new light writes ftol(Random(0, 31)) into them ([0x41F80000] = 31).
- The citadel leashes' cell runs through 15 cells at 10 a second, and their scroll sets the colour row n × 0.125 or 0.375 (0x3EC00000).

### ARGB4444 textures

**Faithful**.

- **The cut.** `LH3DTexture` stores in ARGB4444 the textures created with the alpha flag 0x40 (`Create` 0x8379E0
  with flags 0x41). Format 0 has the masks 0xF000/0xF00/0xF0/0xF (0x85DCA1..0x85DCC2). `fn_00837400` keeps the high
  nibble of each byte, without rounding:
  - B = byte[+2] >> 4 (0x8374F4), G = byte[+1] & 0xF0 (0x837502), R = (byte[+0] & 0xF0) << 4 (0x837512);
  - the alpha of `xa.raw` = byte & 0xF0 (0x837681).
  - D3D7 expands the nibble when sampling, n·17 (inferred: the driver does it). The original **filters nibbles that are
    already cut**, so the cut goes at load time and not in a shader.
  - API: `Quantize(v) = v >> 4`, `Expand(n) = n·17`, `Cut(v) = Expand(Quantize(v)) = (v & 0xF0) | (v >> 4)`;
    `Pack`/`Unpack` of the 16-bit texel; `PackRaw(rgb, alpha)` for the `x.raw` + `xa.raw` pair.
- **Without the flag**, the 4444 branch does not run (`cmp [esp+0x834],0 / je`, 0x8374CB/0x8374D2). It goes to 565
  (0x8376E3..0x83771E) or to 555 (0x837765..0x83779F) depending on [0xEDD46C] (0 on the hardware the game targets:
  555, see `Graphics/Rgb16.h`), with 5 bits per channel: texel = ((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3),
  without rounding (confirmed: xref immediates). **Done**: `Texture2DLoader` cuts each byte with `rgb16::Cut5`
  (= `Unpack555(Pack555(...))`, the bit-repetition expansion D3D does when sampling, inferred) in the
  `rgb16::k_Rgb555Stems` textures (`Graphics/Rgb16.h`, read by `Loaders.cpp`) that are exactly 0x30000 bytes (the same guard as `fn_00837300`).
  - [0xEDD46C] is set to 1 only by `fn_0085D7F0` (0x85D929..0x85D930), when the 16-bit format the engine picks has the
    green mask 0x7E0 (565), i.e. the device has no X1R5G5B5; `fn_00837400` tests it at `cmp [0xEDD46C]` 0x8376B9.
  - The 565 branch (0x8376E3..0x83771E) packs ((R & 0xF8) << 8) | ((G & 0xF8) << 3) | (B >> 3) (`shl edx, 5` 0x8376FA,
    `lea edi, [edx + eax·8]` 0x83770C): green is G & 0xF8, so the lowest bit of the 6-bit field is always 0. Only for
    .raw textures, never for the video.
  - Who goes that way: of the 69 calls to `Create` 0x8379E0, the ones that load a file without 0x40 are `Sun.raw`
    (flags 1, 0x81E844..0x81E851; confirmed: xref immediates) and the saved-game images, `screenshots_lores_%i_map_0.raw` and
    `screenshots_hires_%i_map_0.raw` (flags 1, `fn_00784070` 0x78408F and `fn_00784640` 0x78466A, names from
    `fn_00784430` / `fn_007843F0`), which openblack does not load. The others are 0x41 (the list below), 0x44 / 0xC4 /
    0x104 (in memory, type 4), 0x48, 4 (mesh skins), 2 (`SetPackedTexture`) or the video; the one at 0x822874 is in
    `fn_008227A0`, a command-line mode (the terrain .cmp converter), not in the game.
- **The 0x44 ones do carry the flag** (0x44 & 0x40; `Create` stores the whole flags in [tex+0x10], 0x837A76): they are
  4444 surfaces, but of type 4 (flags & 0x3F, tables 0x837CD4 / 0x838E84 → 0x838C2E), in-memory textures that do not
  load their file through `fn_00837400`. Such as `ChallengeScroll.raw` (0x44, 0x781BDC and 0x79D59F; strings 0xC25048
  and 0xC2A5D0) and `human_shadow` (below). `ChallengeScroll` is not in the list because the game fills it in itself,
  not because it lacks the flag; how it fills it in has not been read (see [Pending](#pending)).
- **Guards.** The colour has to be exactly 0x30000 bytes (`fn_00837300`, `cmp ecx,0x30000 / sete`, 0x837318); if
  not, it goes through DDS. That is why `S_IceEnvMapGrey.raw`, of 194823 bytes, is not cut, nor is its
  `S_IceEnvMapGreya.raw` (0x10000 bytes): without a valid colour `fn_00837400` never gets to read the alpha. The alpha
  is read with 0x10000 bytes (0x837600): `LHLoadData` reads min(length, 0x10000) (0x7BCEC0..0x7BCECD) and does not fail
  if it is shorter; the rest of the buffer keeps the colour stream (the leftover rule below, from its length onwards),
  and a longer one is truncated without error.
- **The ice maps.** The table 0xBEF478 holds three names: `S_IceEnvMap`, `S_IceEnvMapGrey`, `S_IceEnvMapColor`.
  Only `fn_0057D4C0` reads it (0x57D4D7): once per index it loads the name through `fn_0057DBE0` (flags 0x41, so
  ARGB4444 with the alpha of its `a.raw`), makes a mode 13 material (SRCALPHA / ONE, additive) and sets bit 0x10 of
  the material's +5 (no UV offset). Its only two callers, `Creature::ProcessState` 0x4738EF and
  `SpellSeedGraphic::DrawSpellGraphic` 0x519E12, push `ebx`, which is 0 at both: only `S_IceEnvMap` is ever loaded,
  and the grey and colour maps never are (for direct calls). The ice's alpha is the 4-bit alpha of that one texture;
  the original has no sampler stage of its own for it.
  - The frozen look is compiled out of every program today (no program defines `USE_CREATURE_SPELL_LOOK`), so the test
    that no program samples two textures at one stage does not cover stage 5 yet; it will once a program defines it.
    `S_IceEnvMapGrey.raw`, which is no whole texture, is skipped at start-up instead of failing to load;
    `S_IceEnvMapGreya.raw` still loads, unused.
- **Without `a.raw`.** The alpha's name is the colour's without its last 4 characters plus `"a.raw"` (0xC384AC,
  0x8375B4..0x8375C1). If `LHLoadData` fails, it only calls `Report3D` (0x837616) and carries on at 0x83761E with the
  buffer that still holds the colour. The alpha of pixel i then comes from byte i of the colour stream (R0, G0, B0,
  R1…) & 0xF0.
  - In practice it does not happen: all the 0x41 textures come with their `a.raw` (perhaps the saved-game images,
    inferred). `PackRaw` reproduces it.
- **Who carries it.** There are only two calls to `fn_00837400`: 0x838087 in `fn_00837DF0` and 0x838D41 in
  `fn_00838AF0`. Both pass `[tex+0x10] & 0x40` (0x838079 / 0x838D37).
  - Of the 69 calls to `Create`, 32 push an immediate 0x41 and 22 use 0x44.
  - One more computes it: `fn_008227A0` (not `fn_00822560`) 0x822855..0x822874 sets 1, plus 0x40 if the `a.raw` exists (the terrain
    .cmp converter).
- **The `k_AlphaFlagStems` list**, each name with its address:
  - fixed: `Front_end_buttons`, `mousehelp`, `forcefield`, `pin`, `rainbow`, `PlayersSymbols`, `ChooseSymbol`,
    `OriginalChooseSymbol`, `sky` (0x5E5432), `gatheringtext`, `Data\C_Ape_Hair`, `icons`, `PictureTexture`,
    `smallbump`, `Weather`, `atmos`, `snow`, `Data\blobs`, `leash`;
  - particles from `fn_0080BBD0`: `p4t`, `p4`, `smoke`, `s_fire`, `cool_effect`, `misc0`, `burn`;
  - environment maps (table 0xC37EAC): `envmap`, `envmap_glass_fx`, `envmap_glass_fx_inv`, `envmap_glass_fx2_inv`;
  - through `fn_0057DBE0` → `fn_0057DB10` (0x57DB78/0x57DB7B, flags 0x41):
    - GlobalTextures: `S_SpriteSheet1/2/3`, `S_Static`, `S_Hand_Flow`, `S_Fire` and the tables 0xBEF478, 0xBEF484 and
      0xBEF4A0;
    - the `TextureFileName` entries of the spell files, through ParticleSpriteCreator (`fn_006AA030` 0x6AA047) and
      ParticleChainCreator (`fn_006AA800` 0x6AA817): `S_Beam`, `S_lightning`, `S_Spangle_A` 🔍 inferred,
      `S_Teleport_Vortex_Texture(01)`, `S_Volcano_Fire`, `S_Volcano_Rock`;
    - ZR_SurfRevol (0x6863E4);
    - LandscapeVortex `fn_005FEA70`, tables 0xBF3F5C / 0xBF3F68: `S_VortexBaseMultiRing`, `S_Volcano_Base`,
      `S_VortexBaseAlphacopy`, `S_Volcano_Base_Alpha`.
  - Load site of each fixed stem: `Front_end_buttons` 0x4120CA, `mousehelp` 0x447584 (0x44757F, [0xC5A314]),
    `forcefield` 0x4568B9, `pin` 0x53C658, `rainbow` 0x5C386B, `PlayersSymbols` 0x5DE410, `ChooseSymbol` 0x5DE425,
    `OriginalChooseSymbol` 0x5DE43D, `sky` 0x5E5432 / 0x5E5461, `gatheringtext` 0x5F8A92 / 0x72E00D / 0x83109F
    (`GatheringText::SetupGameFonts` 0x831096), `Data\C_Ape_Hair` 0x6186C7, `icons` 0x787358, `PictureTexture`
    0x7900FD, `smallbump` `fn_00804830` 0x804844, `Weather` 0x81E831, `atmos` 0x835C29 ([0xEDC370]), `snow` 0x835C7C,
    `Data\blobs` 0x845E08, `leash` 0x8485F2.
  - `fn_0080BBD0` loads, in order: `p4t` 0x80BC1B, `p4` 0x80BC44, `smoke` 0x80BC6D ([0xEA1A98]), `s_fire` 0x80BC99,
    `cool_effect` 0x80BCD4, `misc0` 0x80BD0E ([0xEA1A90]), `burn` 0x80BD37 ([0xEA1A9C]; path string 0xC37F90,
    `Create(path, 0x41, −1, 0)` 0x80BD2F..0x80BD3F).
  - `envmap`: `fn_0080B380` 0x80B393 (table 0xC37EAC).
  - LandscapeVortex `fn_005FEA70` creates its textures at 0x5FEA92 / 0x5FEB1B; the table 0xBF3F5C is filled by
    `fn_005FD4C0` and 0xBF3F68 by `fn_005FD4D0`.
  - `HasAlphaFlag(stem)` accepts the colour's name and the alpha's (the same plus `a`), case-insensitively.
- **`human_shadow.raw`** is not in the list. `fn_0081FAA0` creates its own 0x44 texture (0x81FC58) and reads 0x400 bytes
  (0x81FC86). It writes texel = (v & 0xF0) << 8 (`and cl,0xF0 / mov bh,cl`, 0x81FCDD..0x81FCEC): alpha `Quantize(v)`
  and RGB 0. It is the same cut, with its own entry (`k_HumanShadowStem`).
  - It reads the 0x400 bytes at 0x81FC86 / 0x81FC91 and locks the new texture through `fn_00838AF0` (0x81FCA4).
- **L3D skins**: they are copied as they are (`rep movsd` 0x837B41), because they already come in 4444. With mesh flag
  0x10000 (`test edi,edx` 0x80656D) they would be RGB555 (see [Pending](#pending)).
- **Which skin a primitive wears.** `LH3DMesh::CreatePackInternal` 0x807BE0 creates the pack's
  meshes one after the other with `Create(data, 0)` (0x807C34). `LH3DMesh::Create` 0x806460 first makes the mesh's
  embedded skins (`LH3DTexture::Create(texels, 4, id, ...)` 0x806579 / 0x806590; with mesh flag 0x1000 the skin table
  holds texture ids instead, `GetThisTexture` 0x806548), then its sub-meshes (0x8065CF). `LH3DPrimitive::Create`
  0x84AB50 turns every material skin id other than −1 into `GetThisTexture(id)` 0x84AB6C. `LH3DTexture::Create` puts
  each new texture at the head of the list `g_first` [0xEDD460] (0x837A2B..0x837A31, 0x837C63), and `GetThisTexture`
  0x838480 returns the first one with that id (+0x18), or, with none, `g_first` itself after a report (0x8384BB).
  So a primitive wears the newest texture with its id when its mesh is made: the mesh's own skin when it has one with
  that id (made just before), else an older texture with that id, else the newest texture of all.
  - In `AllMeshes.g3d` (626 meshes) this gives what openblack's lookup gives (own skin, then the pack's texture, then
    the mesh's first skin: `world_triangles::PrimitiveTexture`): no mesh has flag 0x1000 with skins, no primitive
    names a skin of an earlier mesh, and the palms `MSH_T_PALM` .. `MSH_T_PALM_C` (586..589, skins 0x1001, primitives
    0x85..0x88) fall back to their own last skin in both.
  - `MSH_BOULDER1_CHALK` (253), the mesh of mobile static info 16 (`GMobileStaticInfo` in `info.dat`), has one skin,
    id 0, whose 65536 texels are all 0xFFFF: plain white. It is the only flat skin in `AllMeshes.g3d`. Its primitive
    (Textured, skin 0) wears it in the original as in openblack, so the boulder is drawn white and lit, with no
    pattern. Only Land 5 uses it: four of them at (1350.38, 3489.84), (1348.41, 3490.74), (1350.94, 3488.34) and
    (1349.66, 3486.24) in `Land5.txt`, scales 0.339 to 1.719. The other chalk rocks (`MSH_BOULDER2_CHALK` 257,
    `MSH_O_LONGROCK_CHALK` 373 and the rest) have patterned skins. `Rock::Draw` 0x517F10 only adds the land's light
    and fog, and the Rock constructor 0x6E6F70 keeps the info's mesh.
- A missing `xa.raw` is the failed LHLoadData(name, [0xEDD3D8], 0x10000) at 0x837600.
- The hand's good and evil morph rewrites a skin's ARGB4444 texels (`Morphable::MorphTexture` 0x619500, the material's dirty flag +0x138): 4444 texels go in as they are.
- The video frames are 16-bit too: `LHVideoPlayer::DecodeNextFrame` (`fn_008450B0`) copies each Bink frame with BINKSURFACE565 when [0xEDD46C] is set, else BINKSURFACE555.

### The mesh pack's textures on the card

Read 2026-10-20, when the user asked whether the original's 16-bit cut should be reproduced; openblack keeps 32-bit
colour and the textures' own sizes by the user's choice ([below](#the-mesh-packs-textures-in-openblack)). What the exe
shows, and what it leaves to DirectDraw:
- **Every pack texture is type 2, format 0.** `_SetPackedTexture` 0x8377E0 calls `Create(0, 2, id, fmt)` 0x83788E with
  `fmt` NULL when the INFO entry's second dword is non-zero, else `&1` (`setne` / `dec` / `and` 0x83787C..0x837886);
  with NULL, `Create` takes a local 0 (0x8379EB, 0x837A0E). The shipped pack has 55 entries with flag 1 (all DXT1 256²)
  and 55 with flag 2 (all DXT3 256²); the checks' modded pack also has only flags 1 and 2 (DXT1, DXT3, DXT5 up to
  2048²). Type 2 makes no surface at load: `Create`'s table 0x837CD4 → 0x837ADB only stores the pack id at +0x11C.
- **The source is the file's DXT surface, undecoded.** On first bind (table 0x838410[1] → 0x8381E1), with no +0x120
  yet, `fn_00838580` reads the segment `"%x"` (`"LOW%x"` with `g_b_use_low_res`) and `fn_0087F6D0` copies the file's
  own DDSURFACEDESC2 (its FOURCC pixel format kept), makes a system-memory texture surface (dwCaps 0x1800,
  `IDirectDraw7::CreateSurface`) and copies the blocks in with `rep movsd`. The exe holds no DXT decoder: its only
  FourCC is the `'DXT1'` (and `'DXT3'`, +0x2000000) of the blank 256² placeholder at 0x87F799.
- **The destination is a pooled 16-bit surface with alpha.** `fn_0085DAC0` → `fn_0085DA00` takes the least recently
  used slot of a pool made beforehand (`fn_0085DEB0`: flags 0x21007, one mip level, 256², or 128² in low-res), whose
  pixel format is the record 0xF05190 (formats 0, 2 and 3; 0x85DF5D..0x85DF75). `EnumTextureFormats`' callback
  `sub_85DC20` (registered in `fn_0085D7F0`) fills that record with each 16-bit RGB format with alpha the card lists,
  until the one with masks A 0xF000, R 0xF00, G 0xF0, B 0xF, which latches it ([0xF051B0], 0x85DCA1..0x85DCC2). So it is
  ARGB4444 when the card offers it, otherwise the last 16-bit alpha format the card listed (1555, say). FourCC formats
  match none of the records: the card never keeps the texture in DXT, whatever it supports.
- **The conversion is DirectDraw's or the driver's.** `fn_00837DF0` Blts the DXT surface into the slot,
  `Blt(slot, NULL, dxt, NULL, DDBLT_WAIT, NULL)` (0x838265..0x83827F), or, with [0xEDD450] set by
  `DDrawInitialiseDevices` 0x7DD522 from the device record's +0xEC, through the staging surface [0xEDD474] made by
  `fn_008396A0` in the same format (0x838246). The exe asks only for that pixel format; how the DXT colours reach 4
  bits (truncated or rounded, before or after the block's interpolation) is done outside it, and its Blt results are
  not checked. With NULL rectangles a texture larger than its slot (the modded pack's 512² to 2048²) is scaled to the
  256² slot by the Blt, if DirectDraw accepts that source at all (a DXT5 source depends on the runtime).
- The same record-0 slots hold every format 0, 2 or 3 texture: the alpha `.raw` textures (already cut to 4444 by the
  exe itself, `fn_00837400`, above), the 0x44 in-memory ones, the L3D skins (type 4), the fonts and, through
  `GLandscape::Open` 0x5E5461 (its format register not read), probably the landscape.

## openblack

Status: **done** for the ARGB4444 and 16-bit cuts, the material blending, the wrap or clamp and most frame clocks;
the clocks of the citadel, the 3D cursor and a few effects are not ported (see the table).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Texture wrapping or clamping**
  - openblack: `Primitive::wrap` and sampler flags in `Renderer::DrawSubMesh`.
- **Frame-animated textures**
  - openblack: `object_ghosts::Passes`, drawn as two PSys mesh atoms (the mask through
    `L3DMeshSubmitDesc::textureOverride`, the forced ALPHAREF and the depth test EQUAL)
  - openblack `abodes::DestroyedByEffect`, reached from `abodes::OnPhysicalDamage` at life 0.
  - openblack: `ecs::object_ghosts` keeps {mesh, matrix, ms left} in the world effects' state (no entity, so nothing
    of the game state)
- **ARGB4444 textures**
  - openblack: [The ARGB4444 cut in openblack](#the-argb4444-cut-in-openblack)
  - openblack: only `sun`.
  - openblack: `S_IceEnvMap` and `S_IceEnvMapa` are cut to 4444 as above.
  - openblack splits colour and alpha into two textures, so the alpha takes a sampler stage of its own (5), the
    shader's declared choice.

### Materials and blending

- `L3DSubMesh` translates the material type into `blend`/`depthWrite`/`thresholdAlpha`, but the renderer did not use
  `blend`: everything came out opaque. Now materials with blending and no alpha cut-out (`AlphaTextured`, `TexturedAlpha`,
  `SmoothAlpha`, `*Nz`, additives without chroma) are drawn with the texture's alpha (`u_skyAlphaThreshold.w`),
  `SRCALPHA/INVSRCALPHA` (or `SRCALPHA/ONE` for the additives), without writing Z for the `Nz` ones, in the `MainBlended` view
  (after everything opaque). `TexturedChroma` keeps the alpha test. The states of each type come from
  `render_modes` ([Render modes and materials](transparency-and-render-modes.md#render-modes-and-materials-render_modes)).

### Frame-animated textures in openblack

**API.** `graphics::frame_anim` (`src/3D/FrameAnim.{h,cpp}`), logic only:
- Engine primitives:
  - `SpriteCell` / `SpriteCellUv`: the LH3DSprite cell, `flags & 0x3F` (LH3DSprite::Draw 0x840530). Its UVs are those
    of `billboard::CellUv`, which is not duplicated.
  - `UvOffset` / `IsAnimatedUv` / `OffsetUv`: `SetAnimatedUV_1` (vt +0xE8, 0x7F9B70; +0x68 / +0x6C). Each draw of an
    LH3DObject copies it to [0xECA62C] / [0xECA630], with [0xECA628] = 1 if they are not both 0. DrawTriangle 0x82F8BE..0x82F8F7
    adds it to each vertex **except** if the material has bit 0x10 of byte +5. fn_0082F920, fn_0082FD70 and
    fn_00884750 add it without checking the bit. [0xECA628] is set at 0x80C42C..0x80C485: the offset is on unless
    both components are 0.
  - `PackUvOffset` (openblack): the transport to `vs_object.sc`, v + 4·round(256·frac(u)). It is exact for all the
    original's users: the bubble's quarters, the vials' 32/256 steps and the whole-pixel cells of
    AnimTextured. Only a SlideU of 1000 frames gets rounded to 1/256.
  - `AnimTexturedCell`: Particle3DObjAnimTextured::DrawAt 0x67A530, read in full. With cells, cols = 256 / W (idiv),
    u = (W/256)·(f % cols) and v = (H/256)·(f / cols), with f unsigned. With sliding, u = W·f / (N·256) and
    v = H·f / (N·256) ([0x8D45CC] = 256). openblack adds a guard: cols ≥ 1.
- Clocks (table below).
- Loaders: `psys::LoadStackedBitmap` (`components/psys/StackedBitmap`, raffclar's;
  `GJBitmap::LoadBitmapFromFile` 0x57CA90: only with the exact size,
  0x57CAD2; min(framesInUse, framesInFile) frames (0x57CADB) of Pitch × Pitch taken from the grid of √n per row of
  `fn_0057CB40`) and `StackedBitmap::Frame` (one frame, `fn_006CA280` 0x6CA2E3); `land_light::LoadBitmapFile` reads
  the file.
  - `GJBitmap::LoadBitmapFromFile` callers: `ParticleLightMapCreator::GetBitmap` 0x6A9D40 (bpp 3),
    `ParticleMistCreator::GetBitmap` 0x6AA540 (bpp 1 or 3) and `fn_007311A0` 0x7312B6 (S_LMFireBall). `fn_0057CB40`
    (0x57CB40..0x57CC3F) takes frame f from column f % n, row f / n of a grid of ftol(sqrt(framesInFile)) frames per
    row.

**Deliberate fixes** (the original is different from what openblack did):
- PSys:
  - the frame is kept in [0, 2N) with its previous one (fn_00673EA0), and the index is chosen as in fn_00679920 (fmod and
    +N only once; before it was fmod(fmod + N));
  - without PlayAnim there is neither step nor wrap (0x673FC6..0x673FDE), so a negative frame stays as it is;
  - a non-looping atom with a negative rate jumps to ~2N and shows the last frame, not the first. No .zzz in the
    game has LoopAnim 0 with FrameRate < 0 or with RandomiseFrameDirection, so it is not seen in the game. There are 169
    creators with the PlayAnim property (137 sprites, 21 light maps, 6 AnimTextured meshes, 4 animation ones and 1
    animation-with-camera one), and the ten with a negative rate (fireballs, SF_Flash, the tornado and the rain cone)
    all loop.
- PSys mists: each atom carries its own counter (`Atom::mist`). It starts at ftol(Random(0, 16)) & 15 (0x7F95F8) and only
  advances if the mist appears on screen. Before there was a single global counter.
- Storm puffs: the counter only advances if the puff exceeds alpha 5 (AddDrawing) **and** its sphere is on
  screen (`mists::InView`, the test of LH3DMist::AddDrawing 0x7FA7F0). It is no longer ⚠️ approximate.
- Mists and smokes ✅ faithful (2026-10-22): every mist counter (map mists, clouds, storm puffs, PSys mists, the
  creature room's mists) and every smoke age (chimneys, the creature room's smokes) moves on by the frame's whole
  milliseconds and the whole part of its step only, the fraction dropped every frame as the original's `__ftol`
  (`mists::Advance`, `frame_anim::MistAdvance`, `frame_anim::SmokeAgeStep`). They kept the fraction before, so the
  animation kept its pace at any frame rate; now a frame under 4 ms leaves them where they are, as the original.
  The world's take `game_clock::FrameGameMs()`, the creature room's `game_clock::FrameRealMs()` (its draw swaps
  `g_game_time_inc` for `g_delta_time`). The smoke step was run on the exe with the oracle (fn_007F8E00 on a fake
  smoke: 0, 1, 3 ms give 0; 4, 5, 7 give 1; 8 gives 2; 16 gives 4; 33 gives 8; 1000 gives 255; the clamp at
  100 s gives 25500); tests `FrameAnim.smokeAgeStepAsTheOriginal`, `FrameAnim.mistStepAsTheOriginal`,
  `Mists.AnimationTakesEachFramesWholeStepsOnly`, `MistSystemTest.TheFractionOfAStepIsDroppedEachFrame`,
  `ChimneySmoke.DropsThePartOfAStepLeftOver`, `ChimneySmoke.ADyingSmokeGoesOutByWholeSteps`,
  `CreatureCaveEffects.AgeTakesEachFramesWholeStepsOnly`; debug: the Weather window's mists show their counter
  and cell.
- Fish: the cell is taken before the wrap (cell 23 can show up) and the wrap is −15·ftol(f/15).
- Lanterns: the start of each flame comes from the global table 0xC383BC, which every new light rewrites with
  ftol(Random(0, 31)). So all lights run in phase with the values of the last one created (before, each light had
  its own). The clock is integer and only runs if the town's alpha is not 0 and there are lights (0x82357A..0x823593). The halo is
  cell 56 of `smoke` ((flags & ~7) | 0x38, 0x8233BC..0x8233EB).
- TownBelief: the sparkles carry their accumulator per symbol (+0xC, +0x10 and the spin +0x14, from 0), advanced with the
  whole ms of each frame. Before it used the global time (−s·20, −s·23).
- Hand glow: the frame is rounded (fistp 0x68D323), not truncated, and the wrap is "> 64", not "≥ 64".
- Chains: the UVs of fn_006C8920 (before, U ran along the chain with the whole texture repeated), the creator's default
  values, FileOffset and the scroll. In addition, uv0 goes on the +side (before on the −side: U was
  mirrored) and the half-width is the scale, not 0.5·scale, so **the ribbons come out twice as wide**.
- Creature spell vials: the UV animation of 0x519AD0, which was missing before.
- Power bands: `billboard::BandToEye`, see [billboards](meshes-and-lod.md#objects-that-face-the-camera-billboards).
- SurfRevol discs: the UV offset is interpolated between two steps (`frame_anim::RotatingUvClock`, DrawAt
  0x67CBA0 with t = DrawData +0x14 and GameUpdate 0x6C8BC0 in full). Before, the rule wrapped the value into [0, period) every
  step and the draw used it as is, so the disc rotated in jumps of one PSys step.

**Same as before, bit for bit:** the bubble, the flames and the steam (cell 32 on the first frame was already there), the
chimney smoke, DisappearSmoke, the boat wake, the rings (fixed cell `& 63` of the 8×8 sheet of `smoke.raw`,
fn_005E5100), the map mists and the clouds, the waterfall, the food piles (0x51C0FD), the AnimTextured cell
layout, the mesh and UVs of the SurfRevol discs, the dust and the hand. The dust when
gripping land and the grains and fish when picking up food now go through `ParticleFrameAdvance` / `ParticleFrameIndex`: they give the
same cells, except for the rounding of adding dt·rate instead of multiplying age·rate.

### The ARGB4444 cut in openblack

Code: `src/Graphics/Argb4444.h` (`graphics::argb4444`, stateless and, on purpose, without a shader twin); tests
`test_argb4444` (they emulate 0x8374F0..0x837533 and 0x83767E..0x837687).

- `Texture2DLoader` (FromDiskTag, `Resources/Loaders.cpp`) cuts with `Cut` at load time:
  - every `.raw` whose name satisfies `HasAlphaFlag` and is 0x30000 (colour) or, if it is the alpha (the colour plus
    `a`), is 0x10000 and its sibling colour exists and is 0x30000 (approximate: the original only looks at the colour;
    an alpha of another size is not cut here);
  - and `human_shadow`.
- openblack stores `x.raw` and `xa.raw` as two textures. Each one is cut separately, and bgfx's linear filter
  already works on the 16 levels, like D3D.
- `PackRaw` is used by the small bump (`LandIsland.cpp`), which joins colour and alpha in one texture. A shorter alpha
  takes the tail of the colour stream and a longer one is truncated, like `LHLoadData`; a colour that is not 0x30000
  is rejected (approximate: the original would go through DDS).
- The copies that only expand nibbles that are already done use `Expand` (`CoastAlpha.cpp`, `GameFont.cpp`) or
  `Unpack` (`BlockTexture.cpp`).
- Shaders:
  - `fs_blob.sc` no longer quantises. Before, it did floor(v/17) after filtering: one level less in 120 of the 256
    values and the gradient in steps. Now the blob comes out somewhat darker and without steps.
  - The river channels' alpha is no longer a shader's: the footprints are printed into the blocks' 4-bit texels at their
    prints step (`block_texture::PrintFootprint`), so `fs_land_alpha.sc` and its rounding went with the live pass.
- Whoever uses these textures already receives the 16 levels: also the glow of the night lights (`NightLights.cpp`
  loads `S_Firea` and `smokea`), the water rings and the boats' puffs.

### The mesh pack's textures in openblack

🎯 **Chosen difference** (the user, 2026-10-20): the mesh pack's textures keep 32-bit colour and their own sizes.
openblack uploads each as the file stores it (BC1, BC2 or BC3, or RGBA8 for a DXT1 with transparent texels, above):
there is no ARGB4444 cut of their colour and no 256 x 256 slot, so the modded pack's larger textures keep their
resolution. The original's 16-bit surfaces and its DirectDraw conversion ([The mesh pack's textures on the
card](#the-mesh-packs-textures-on-the-card)) are recorded for reference only. The `.raw` textures' cuts ([The ARGB4444
cut in openblack](#the-argb4444-cut-in-openblack)) stay as they are.

### The pack's DXT1 textures with transparent texels

`Texture2DLoader` (FromPackTag, `Resources/Loaders.cpp`) uploads a DXT1 pack texture as BC1, except when its top level
has a transparent texel (`dxt_alpha::HasTransparentTexels`, `src/3D/DxtAlpha.h`: a three-colour block that uses its
fourth code). Such a texture is decoded on the CPU (`dxt_alpha::DecodeDxt1`) and uploaded as RGBA8, because bgfx's
Vulkan renderer takes BC1 as `VK_FORMAT_BC1_RGB_UNORM_BLOCK`, with no alpha, which drew those texels opaque black. The
shipped `AllMeshes.g3d` has three: 0x8 (the Aztec stick fence, TexturedChroma with ALPHAREF 98, on BuildingAztec1,
BuildingAztec3, BuildingAztecCreche and BuildingAztecWorkshop), 0xA and 0x29. The checks' modded file has none: its
DXT1 textures are opaque, and the ones with alpha were re-encoded as DXT5, so the checks take only the BC1 path. The
texture viewer counts the decoded textures. Tests: `test_dxt_alpha`. **⚠️ approximate**: the decoder rounds each colour
step to the nearest, while a graphics card's own BC1 decoder may round a step one apart.

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/FrameAnim.h`: the frame clocks of the tables above and `FishDt`.
- `test/test_argb4444.cpp`: `EmulateTexel`; `test/test_rgb16.cpp`: `EmulateRaw565`.

### Test hooks

- Test hook `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fixed at a fraction of the window (the hand appears in captures
  without a real mouse).
- The full list: [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

### Frame clocks still open

- ⚠️ approximate Storm puffs start with the counter at 0, not at Random(0, 16) & 15: it is the same cell 0.
- `game_random::crt::Random` (`src/Common/GameRandom.h`) is the original's Random 0x81D180 (on the
  CRT's `rand()`, seed 1 at startup: `__initptd` 0x7D2323; the `srand(time)` at 0x577721 only runs when saving
  `creature.lhp`). It is shared by the map mists, the PSys mists, the storm puffs, the rain and the camera
  shake. ⚠️ approximate the original has one seed per thread; here one.
- TownBelief takes g_game_time_inc from `game_clock::FrameGameMs()` (0x69D855; before, approximately, from the wall clock).
- The lanterns (fn_00823570), the sky clouds (their movement, their atlas counter and the sky alignment,
  `Renderer::UpdateClouds`), the map mists (`CollectMists`, fn_007FA300) and the chimney smoke
  (`CollectChimneySmoke`, fn_007F8E00) also take g_game_time_inc from `game_clock::FrameGameMs()`. Before it came
  from the wall clock, scaled by the game speed and capped at 100 ms, and the lanterns kept the fraction of
  ms (`WholeMilliseconds`, which is removed: the game clock already gives whole ms and keeps the rest of the turn itself). The
  mists and the smoke are collected once per frame, only in the main view.
- 🔍 inferred That S_Fire is drawn in 8×8 like S_SpriteSheet3.
- 🔍 inferred GoldenShower: t in milliseconds.
- HandEffects (dust when gripping land, grains and fish when picking up food) is still a hand-made copy of effects that in
  the original are PSys (SF_GripLandscape, ER_MultiPickup). Its frame clocks are already the PSys ones.
- Not ported, with their clock and their test in the API: the leashes, the creature room and
  the citadel world map, the 3D cursor and JCSpecial. Also HandGlow / fn_0083F270, the scroll that does not belong to LightSheet but to the object of
  fn_0083F100 / fn_0083F210, read by fn_0084F910.

### ARGB4444, still open

- [ARGB4444 textures](#argb4444-textures), what is missing:
  - `ChallengeScroll.raw` (0x44, type 4 in-memory texture): how the game fills it in is not yet read.
  - [The ice maps](#argb4444-textures): what the draw's +0x98 (0x354F8D) means, and how +0x94 (from the creature's
    +0x12BC, **🔍 inferred** how frozen it is) weighs the additive ice, are not yet read.
  - The ice alpha's sampler stage (5) is checked by no test: the frozen look is compiled out of every program
    until one defines `USE_CREATURE_SPELL_LOOK`, and only then does the stage test see it.
  - [Which skin a primitive wears](#argb4444-textures): checked for `AllMeshes.g3d` only. The meshes loaded one by one
    (`CreateFromHD`, the citadel's `.zzz`, the creatures, the hand) are not checked against the textures made before
    them; a primitive whose id openblack finds nowhere is drawn untextured, where the original takes the newest texture
    (`GetThisTexture` 0x8384BB). The white chalk boulder of Land 5 (`MSH_BOULDER1_CHALK`) is faithful: its skin is
    white in the data.
  - The L3D skins with flag 0x10000 (`L3DMeshFlags::Unknown17`): the original uploads them as X1R5G5B5, openblack as
    BGRA4 (`L3DMesh.cpp`). Only `Data\d_sky.l3d` carries the flag and openblack does not load it, so today it is not
    visible; the meshes in `AllMeshes.g3d` are not yet checked 🔍 inferred (queued fix).
  - The halving of `[0xEDD470]` with fmt ≠ 3 (step 6 and jump 0x300, 0x8374BB/0x8374C3): at detail levels 0 and 1 the
    original keeps the top-left pixel of each 2×2, at 128; openblack does not halve it (queued fix).
  - The light maps of `Data\Spells\LightMaps` (`Particles/Creators/LightMap.cpp`) also go through `fn_0057DBE0`
    (🔍 inferred, not yet read) and are not cut.
  - The save-game pictures: "the save-game pictures (0x7926DA, 0x79286F) are made by the game: neither has a stem
    here". This page says the save-game images are loaded from files with flags 1 (`fn_00784070` 0x78408F,
    `fn_00784640` 0x78466A); whether 0x7926DA / 0x79286F are a different pair of `Create` calls (with a computed 0x41)
    is not yet read.

### From the model rendering notes

- Animated textures:
  - port the users that only have a clock (Gooloo, the leashes and the creature
    room, the 3D cursor, JCSpecial) and HandGlow / fn_0083F270;
  - the vials: the original's branch of 0x519AD0 also bounces and squashes them (the switch 0x519D76); openblack
    does neither (queued fix);
  - in the chains, UseDynamicLighting (midpoint smoothing is already done; the SurfRevol interpolation too,
    `frame_anim::RotatingUvClock`: GameUpdate 0x6C8BC0 in full);
  - HandEffects as real PSys effects.
