# The frame and its draw stages

## Summary

A frame of the original is drawn by the CPU-transforming Direct3D 7 renderer (LH3D): the sky first (the colour buffer
is never cleared), then the reflected land, the reflections and the sea, the land blocks front to back, the models of
the object draw list, the hand, the interface, the particles and the weather, and at the end of the frame one
back-to-front flush of the transparent queue, the end-of-frame callbacks (help text, film), a quad that resets the
depth and the screen fade. Each L3D material type is one of 19 render modes. There is no hardware fog, no mipmaps, no
antialiasing and no far plane.

openblack draws the same stages in the same order with bgfx (a modern renderer on purpose, with the same pixels as the
aim: [A reconstruction, not a decompilation](../project/reconstruction.md)); the table under [Parity by stage](#parity-by-stage) gives
the state of each one. Everything on this page is ✅ faithful (read in the executable) except what is marked
**🔍 inferred** or **⚠️ approximate**. Progress, row by row:
[Render pipeline](../project/progress/rendering/render_pipeline.md), [rendering](../project/progress/rendering),
[sky](../project/progress/sky), [Landscape rendering](../project/progress/terrain/landscape_rendering.md). The frame
loop itself is in [The engine loop](../engine/loop.md#frame-loop).

## The original

### Draw stages in order

The world case (case 0) of the frame, in order:

| # | Stage | Function | What it does / states | Order / culling |
|---|---|---|---|---|
| 1 | landscape.PreDraw | GLandscape::PreDraw 0x5E3F60 → LH3DIsland::PreDraw 0x7FF2D0 → fn_00877210 | list of visible blocks (32×32 blocks of 160), fog class per block +0x940 | frustum and near-plane culling with each block's box; list [0xFA92D8] (next +0x9B8) sorted by distance +0x9BC **front to back** ([stage A](visible-blocks-and-lod.md#stage-a-the-visible-blocks)) |
| 2 | shadow textures | TemporaryShadow::UpdateAll 0x825190 (and fn_00874850 per SuperVillager) | silhouettes rasterised by the CPU into per-object shadow textures (fn_008801D0, **🔍 inferred**), list 0xFAA7E0 | — |
| 3 | preparation | LH3DCreature::PrepareForDrawing 0x4ED320 per creature, CHand::PrepareForDrawing 0x46C550, PSysLightMaps::AddDrawing 0x6CA6E0, LH3DLandscape::TextureUpdateThread 0x871F00 (the block texture queue: at a block's 16th step its static shadows, footprints and snow fn_008721A0 / fn_00872FA0 and its low-resolution tile fn_008002C0; [the block texture queue](../world/terrain.md#the-block-texture-queue)), LH3DAtmos::Update3D | does not draw | — |
| 4 | GLandscape::Draw 0x5E42E0 | (detail in 4a–4o) | | |
| 4a | preparation | 3D cursor (Get3DPointFromScreen), fn_00802550, Windmill::PreDraw, Tree::PreDraw (swaying in the wind), fn_008296D0 (8 s fade timer, mode 0xF), fn_005E5830 (hand position; hand night light fn_00823460/fn_0086D360; clouds fn_005E25C0 with the Clouds key) | | |
| 4b | **sky** | GLandAlignement::DrawSky 0x5E2160 (not in wireframe) → fn_0086A330 → light table fn_00869850 + fog parameters, fn_0086B7F0 (sky_{good,ntrl,evil}_{day,dusk,night}.555 → 3 textures, mode 2) → fn_0086B010 | **moon** moon.l3d + additive glow quad 500 (AdditiveMaterial mode 13), and a copy mirrored in Y (reflection, **🔍 inferred**); **dome** sky.l3d with ZFUNC ALWAYS, 2 passes (texture of the base alignment with α255, then the 2nd alignment with global alpha ftol(X·255) or ftol((X−1)·255), X the sky alignment 0 good..2 evil), towards the haze colour with the overcast, darkened on the good side, towards white with lightning; **sun** sun.l3d fn_0086C140, colour 0x957C63, visible from 6 to 18 h, ÷(1+8·clouds). **No stars.** | the sky covers the whole screen (the colour buffer is not cleared) |
| 4c | **reflected land** (LandRef key) | fn_007FF4F0, ZWRITEENABLE off around it (0x5E48B3–0x5E4900) | heights ×−1, light table ×0.5, small bump forced off; land only, no models | mirrored block list |
| 4d | reflections in the sea | PetitNavire::PreDraw 0x5DFF20 (ships); hand DrawUnderWater vt+0x118 with specular 0x65A0A0A0 through the **alternative mode table 0xC387C8**; object in the creature's hand; fn_00646FE0 (physics objects), fn_00775120 (sharks, cut by the water plane, 0x5E4B26), fn_00824B90 (FishFarm fish and the fish puzzle nets), fn_005DFCE0 (footprint UV2, **🔍 inferred**) | before the sea. The hand, what it holds, the physics objects and the ships are **reflections**: `DrawUnderWater` draws them mirrored in y = 0 and clipped to what is above the water (see [object reflections and hand shadow on objects](shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects); it had previously been assumed they were their parts under the water); the FishFarm fish are sprites under the water | |
| 4e | SuperVillagers | list 0xEB9A08: Draw vt+0x610 + shadow fn_00874850; swimming villagers generate water rings | | |
| 4f | hand night glow | fn_005E3F70: additive quad on the ground of ±60, ZFUNC ALWAYS | | |
| 4g | **sea** | fn_00879930 (skipped if [0xECA664] or in wireframe): sky.raw/skya.raw mode 5, rows of 2 px, ZFUNC ALWAYS, no Z write, colour = light table[255], alpha 255→80 between 7000 and 14000, period P = 2000−1800·WaterTiling | | |
| 4h | vortex | fn_005FF310 → fn_005FFBB0 (LandscapeVortex, **🔍 inferred**) | | |
| 4i | **land** | fn_007FF610: per visible block front to back, transition blocks fn_00877D20, block fn_00874AA0 / SSE fn_007A1800 (blocks in mode 14, per-vertex light table, software haze, 2nd small bump pass in mode 14), then **projected dynamic shadows** per block fn_00878350 (shadow material mode 6) | front to back |
| 4j | debug | fn_0081F820: field-of-view triangles of the scripts | | |
| 4k | citadel heart | fn_00467360 → CitadelHeart::DrawNow 0x4670D0 | | |
| 4l | water rings | fn_005E5100 over 1024×0x38 at 0xEAB7C8 (GWater circle sprite, direct LH3DSprite::Draw) | | |
| 4m | **models** | fn_005E5CD0: object draw list (≤3000, GLandscape::DrawObjects 0xD1D28C) of the visible blocks (dist < VanishObjectDist 100000) + global list; it is rebuilt according to DrawListRebuildCount / more than 10 turns since the last rebuild / a block's land visibility change; with the camera still only the last objects on screen are redrawn ([6. The object draw list](object-draw-list.md#the-object-draw-list-stage-4m)). Draw of an object (e.g. MobileObject::Draw 0x518150): light fn_00801C90 ([model lighting](model-lighting.md#model-lighting)), fog fn_007FEB30, Game3DObject::AddForDrawing 0x63B5D0 → LH3DObject::AddDrawing fn_00815A70: frustum CheckRegionOnScreen 0x868C80, **LOD** by distance (23.3f/66.7f/86.7f → LOD 1/2/4, fade 86.7f–173.3f, culled beyond that if IsDisappear; distant humans → impostor sprite; **inactive in this executable**: the LevelOfDetail loads are disabled, always LOD 1, see [villager blobs, object reflections and LOD](meshes-and-lod.md#villager-blobs-object-reflections-and-lod)), IsGlowing → glow sprite, **NeedSorting (mesh with alpha, flag 0x200) → Z-sorter; otherwise immediate Draw** | opaque: block order, unsorted; with alpha: Z-sorter |
| 4m′ | the creature, in the list | Creature::Draw 0x517910 → LH3DCreature::AddForDrawing 0x48E1C0 → DrawNow 0x48E260 | body (fn_00813340: at once; the Z-sorter only with the mesh flag 0x200 or global alpha, which no creature has), the shadows over it, its hair, then its eyes at once ([drawing in the frame](../creature/drawing.md#drawing-in-the-frame)) | at once, in the list's order |
| 4n | rest of 5E5CD0 | the falling roots' list 0xEB9A10 (its only writers are the dead trees' roots, fn_00826280 / fn_00826370) (OverrideMaterial with forced alpha-ref 0xA), game list vt+0x610, markers, Reward sprites, PetitNavire::PostDraw, hand_intro, sprite emitters fn_008274A0/fn_00823570/fn_00827B90/fn_00828E50 | | |
| 4o | end | storage pit / LandFeature::DrawWorm | | |
| 5 | creature fight sparkles | LH3DCreature::DrawFightSparkles 0x48DD70 | | |
| 6 | leashes | GInterface::DrawAllLeashes 0x5D9310 (fn_008491B0; leash material mode 15) | | |
| 7 | physics objects | PhysicsObject::DrawAll 0x646DE0 (AddForDrawing, DrawOutOfMap) | like the models | |
| 8 | hand | CHand::UpdateHeldObject; **CHand::AddDrawing 0x46D100 → Z-sorter** (light ×1.5); objects in other players' hands with DrawInHand | Z-sorter | |
| 9 | interface | GInterface::Draw 0x518640 → fn_005FAF80 (magic hand), state vt+0x500 (gesture trail, etc.; gestures in mode 13) | | |
| 10 | liquid particles | DrawLiquidParticles 0x845C50 → Z-sorter | Z-sorter | |
| 11 | debug overlay | GGame::Draw 0x5533B0 (only if g_game->field_0x14 & 0x4000: alignment and belief bars) | | |
| 12 | miscellaneous | CreatureLessonChooser::UpdateDraw, EditorIconBase::DrawMouseOver | | |
| 13 | particles | PSysGlobal::DrawLoop 0x68F5E0 (PSys managers → PSysManager::AddDrawing 0x6797D0 → Z-sorter), FireFly::DrawAll, Spell::DrawSpells 0x7203F0, GParticleContainer::DrawParticleContainers, GPlayer::DrawPlayers (player sparkles, blobs.raw mode 13), TownCentre::DrawAll (DrawPSys) | Z-sorter | |
| 14 | counters | ValueSpinner::Update on each (dt = the frame's game ms × 0.001), then, unless the help system's wide screen +0x45E8 is on, each one's Draw queued at its squared distance (0x54E037..0x54E0CD; [the floating numbers (ValueSpinner)](../world/buildings.md#the-floating-numbers-valuespinner)) | Z-sorter | |
| 15 | debug | LH3DStorm::DebugDrawAll, LH3DAtmos::DrawWindField | | |
| 16 | **weather** | LH3DAtmos::Render3D 0x836250: per storm (list 0xFA92D8…, storms at dist < 560) and per 80×80 tile, intensity from GetWeather at the 4 corners; if >5: rain fn_008341B0 → Z-sorter callback 0x833F80 (streaks, AtmosMaterial mode 6 atmos.raw, splashes g_water_drop_cb, amount according to the RainSplash key); snow fn_00834290 → callback 0x834120 (snow.raw mode 9). Not lightning: LightSheet::DoTheDrawing 0x83E8C0 (mode 13) is LH3D's general sheet, queued in the Z-sorter (callback 0x4246E0) by its owners, the recognised gesture (step 13, fn_00688320), the arena's GLightSheet::Draw, the spell icon and the worship totem, and called directly for the camera force field (step 17, 0x54E198); see [the light sheet, as read from the exe](../magic/hand-casting.md#the-light-sheet-as-read-from-the-exe). Rain colour = (light table base/2 & 0x7f7f7f)+0x7f7f7f. | Z-sorter by the tile's distance² | |
| 17 | camera force field | ForceField of CameraModeNew3 (only with the flag) | | |
| 18 | villager names | VillagerName::AddDrawing → Z-sorter | | |
| 19 | fade | GScript::ProcessFade 0x6EB9D0 or Temple::UpdateFade 0x794280 → sets the screen fade colour [0xFA51D8] (drawn in FinishFrame) | | |
| 20 | 3D help | HelpSystem::Draw3D 0x5C59A0 (help character, arrows) | | |
| 21 | ClearLight 0x5E57B0 | removes the hand light (fn_00822F90/fn_00823780/fn_0086D460) | | |
| 22 | power spins | PowerSpinRunner → Z-sorter (PowerSpin::Draw) | | |
| 23 | influence ring | InfluenceCircle::Draw 0x826C90 (in the world only if the camera is more than 100 high: alpha 0→120 between 100 and 200; mode 6, no culling, wrap, UV offset 0.0001/−0.0002 per ms), Draw3DWorldTriangle **immediate** | | |
| 24 | **FinishFrame 0x82F460** | (a) **Z-sorter flush fn_0082F280** back to front (key = dist² to the camera, max. 2048 entries); (b) "before" callbacks (ascending priority, 0xEC8130): FallingSpell (0x526480), HelpDude 0x5C2E30 (100), CameraModeNew3 0x4562E0 (1000); (c) fn_0086BB60 the sun's glare (verified, 0x82F4C9: [the sun and its glare](../world/sky.md#the-sun-and-its-glare)); (d) **Z-reset quad**: full-screen quad FVF 0x1C4, z=1, rhw=0, colour 0, material [0xEDD494] mode 1, ZFUNC ALWAYS → Z=1 over the whole screen; (e) cinema bars if [0xEB9950] (2D rectangles queued fn_0081E590, height fn_0081E8B0); (f) "after" callbacks: HelpDude 0x5C2E10 (100), **flush of the 2D rectangle queue 0x81E7D0→fn_0081E3C0 (10000; mode 1, ZFUNC ALWAYS, no Z write)**, HelpText 0x5CD020 (20000: help text boxes), LHVideoPlayer::thedraw 0x844E30 (0x8000), start_system 0x6424E0 (0xA0000); (g) fn_00836200 debug weather; (h) **screen fade fn_0086FEE0**: full-screen quad of colour [0xFA51D8], mode 1, ZFUNC ALWAYS, if alpha≠0; then the bar rectangles and the fade reset; (i) EndScene (vt+0x18) | | |

#### Other cases: temple, video and 2D

- Case 1 (citadel/temple): Update3D, LH3DSky::g_b_we_are_inside_citadel=1, TemporaryShadow::UpdateAll, DrawSky
  (no sun, hence no glare at the end of the frame), Temple::Draw 0x794370 (its own light at (−1000, 1000, −500) with
  fn_0081E1F0 SetLight, put back at its end; the depth fog's table; rooms; Citadel* detail keys), Temple::Update, liquid particles. Details in
  [the temple interior](../interface/temple-interior.md#the-temple-interior).
- Case 2: FallingSpell video + liquid particles.
- Video: LHVideoPlayer::DrawToScreen (16:9 bars, alpha fade; FallingSpell 80).
- HUD and 2D: B&W has no classic HUD; the 2D consists of the help text boxes (callback), tooltips and text with
  GatheringText (fonts in mode 6), queued rectangles, fade, bars and the software cursor in Flip
  (**🔍 inferred**: the cursor is usually the 3D hand).
- The end of the frame's 2D, in `LH3DRender::FinishFrame` 0x82F460: the cinema bars (fn_0081E590, drawn at once,
  0x82F652..0x82F6DD); the "after" callbacks, among them HelpText's (fn_005CD020, priority 20000): the dialogue box and
  its texts, the fight's panel (fn_00456270 at 0x5CCCD5), fn_0050B2B0, the multiplayer block and the input prompt icons
  (0x5CCE41), then the creature's status panel (fn_005178D0 at 0x5CD03C); the film's callback (0x8000) after it;
  fn_00836200 (0x82F74E); then the screen fade fn_0086FEE0 (0x82F753), only while its alpha byte is not 0, which draws
  the bars again over itself (0x870130..0x8701AB). So the panels are over the bars and the dialogue and under the fade.

#### The depth tests

- The frame's ZFUNC is LESSEQUAL (4), set at 0x82CCC1..0x82CCC5 ([0xECA24C] caches it); the device init's 3 at
  0x82BE76 comes before it. A scan finds 91 ZFUNC writes in the exe (76 through the vtable, 15 through the wrapper). Every
  draw keeps 4 (models, land, sky, particles, chains, SurfRevol, sprites, clouds, mists, smoke, rain, snow, 3D text, the
  temple rooms) except:
  - ALWAYS (8): the 2D (SetupThing DrawLine/Box/Quad 0x412753, 0x412C7E, 0x4131B9; rectangles 0x81E447, 0x81E649),
    CameraHelp's key and mouse (0x447770), CameraModeNew3 (0x456FB9), HelpDude::Feel (0x5B9C7F), the hand's night glow
    (0x5E4283), PSysGesture (0x68A83C) and 0x68B5C3, WorldRoom (0x79E182, 0x79E6EA), the intro light while depthAlways
    (0x828485, 0x828675), the Z-reset quad (0x82F5E1), GatheringText when its depth-test argument is 0 (0x833426,
    0x832B24), the debug weather (0x836F79), the video when its argument is 0 (0x845A16), the sky dome (0x86B185), the
    glare (0x86BC93), the screen fade (0x8700FD) and the sea (0x879FD6, 0x87A26F);
  - EQUAL (3): the object ghost's redraw (0x5E64BB), the land block vortex pass (0x7A3305, 0x7A39DB, 0x87537F,
    0x875C02), FragMesh (0x7F7C8A), the shadows on objects (0x80E486; 0x810C90 restores), the alpha-test object fade
    (0x80EBD8, 0x80F338, 0x80F668), an overlay redraw (0x813618, 0x813AEC, not mapped) and 0x84F83F (not mapped);
  - LESS (2): only the advisor spirits' trails (0x5B939D).

### Render modes

Table 0xC38728, 19 entries {fn, flag}; the L3D material type is the mode index.

Common to all: colour = TEXTURE×DIFFUSE; ALPHAFUNC GREATEREQUAL set only once (0x82CBA6); ALPHAREF = mat+4 (or the
forced one [0xECA65C] if [0xECA658]); bit 0 of byte +5 of the material → cull NONE, otherwise CCW; bit 2 or g_b_need_tilling →
WRAP, otherwise CLAMP; mode cache [0xC38718] (reset to 0x14 every frame). No mode touches fog, specular,
lighting or stage 1.

| # | fn | States | L3D type / uses |
|---|---|---|---|
| 0 | 82D470 | untextured, opaque, writes Z | Smooth |
| 1 | 82D5C0 | untextured, SA/ISA, writes Z, stage 0 untouched | SmoothAlpha; 2D rectangles, fade, Z-reset quad |
| 2 | 82D820 | textured, opaque, α=tex | Textured; sky domes |
| 3 | 82D920 | SA/ISA, α=tex×diff, writes Z | TexturedAlpha |
| 4 | 82DC20 | SA/ISA, α=tex, writes Z | AlphaTextured (buildings, hand; the wrist fades out with the alpha) |
| 5 | 82DD90 | SA/ISA, α=tex×diff, writes Z | AlphaTexturedAlpha; sea |
| 6 | 82DF10 | like 5, no Z write | …AlphaNz; fonts, influence, human_shadow, dynamic shadows, atmos rain, video |
| 7 | 82D6F0 | untextured, SA/ISA, no Z write | SmoothAlphaNz |
| 8 | 82DAA0 | like 6 | TexturedAlphaNz |
| 9 | 82E080 | SA/ISA **+ alpha test**, α=tex, writes Z | TexturedChroma (trees 0x96), snow |
| 10 | 82E830 | additive SA/ONE + alpha test, α=tex×diff, writes Z | …AdditiveChroma |
| 11 | 82E9C0 | like 10, no Z write | …AdditiveChromaNz |
| 12 | 82EB50 | additive SA/ONE, writes Z | …Additive |
| 13 | 82ECD0 | additive SA/ONE, no Z write | …AdditiveNz: gestures, lightning, moon glow, sparkles, fire, smoke |
| 14 | 82DD90 | = 5 (flag 0) | land blocks, small bump |
| 15 | 82E470 | SA/ISA + alpha test, α=tex×diff, writes Z | TexturedChromaAlpha; leash |
| 16 | 82E6A0 | like 15, no Z write | text cache |
| 17 | 82D820 | = 2 | — |
| 18 | 82E2A0 | Z only (ZERO/ONE) + alpha test | ChromaJustZ; vortex, fizz |

Alternative table 0xC387C8 (object fade / DrawWithGlobalAlpha, hand reflection with DrawUnderWater):
0, 1 → D5C0; 2, 3, 17 → D920; 4, 5 → DD90; 9 → E470 (ref scaled by the object's alpha); the rest the same. Flags word:
nobody reads it ([render modes and materials (render_modes)](transparency-and-render-modes.md#render-modes-and-materials-render_modes)).

### Global state

- **Projection.** No hardware T&L: CPU transformation to XYZRHW. Horizontal FOV [0xEA1DD0], default 70°
  (1.22173), aspect width/height [0xE839EC]; sz = 1−near/z, rhw = near/z, **no far plane**. Near plane
  [0xE839E0] dynamic according to the camera's height above the ground: (h·0.05)·3.2 + 0.3 in float steps, 0.3 at or
  below the ground, 3.5 above 20 (0.1 with `SET_GRAPHICS_CLIPPING`, 0.2 in the citadel with fov 90°; see
  [camera](../camera/game-camera.md#camera)).
- **Fog.** No D3D fog. Software haze (Fog key [0xC37204]): start and end according to the sky type
  (noon/midnight 400→900, dusk 100→800; storm →15/350), colour = light table base/3; per vertex
  it darkens the diffuse towards "dark" and adds the fog RGB to the specular (**🔍 inferred**); land per block +0x940,
  objects fn_007FEB30.
- **Clearing.** In Flip, after presenting, only every ~2000 ms (2 frames) in game: black 0xFF000000, z=1. The Z
  is reset every frame with the FinishFrame quad; the colour buffer depends on the sky.
- **Gamma and filtering.** No gamma. LightBoost (only in custom detail) changes the light table divisor.
  Bilinear filtering, no mips, no AA, dithering enabled ([the Direct3D 7 states](#the-originals-direct3d-7-states)).
- **Level of detail.** fn_00823AD0 from start_system 0x643026; level = detailidx from the registry (<5), otherwise **4**;
  5 = custom (fn_008237B0 reads each key), never reached at start-up (see
  [The detail index](visible-blocks-and-lod.md#the-detail-index)). Table 0x9A3704 (L0..L6):
  - Live: LevelOfDetail (dead), UseSmallBump 1 in all, Clouds/CloudShadows 0001111, WaterTiling
    0/.2/.4/.6/**.8**/.5/1 (level 4 → sea period **560**; 200 only in L6), LandRef 0001111, CitadelReflections
    0000111, CitadelLightmaps 0111111, CitadelGlows/People 0011111, CitadelVolumeLight 0001111, RainSplash
    0,0,3,5,8,8,8, LightBoost 0, Fog 0001111.
  - Only at startup: Weather 0001111, Light 0011111 (object dynamic light flag 0x20), ShadowsOnObjects 0001111
    (flag 0x40), UseHighTexture 0000111 (land textures of 256 versus 128 px), UseMultiLayerOnLandscape
    inverted (1 only in L0), FixeLand.
  - HardwareTnL/MaxObjectDistance are read but ignored. The land detail distances 0xE9C508 depend
    on the number of textures in VRAM. VanishObjectDist 100000 (script).

### The original's Direct3D 7 states

`LH3DRender` sets the states with two wrappers, `LH3DRender::SetRenderState` (0x412940) and
`LH3DRender::SetTextureStageState` (0x82B9C0), and also with direct calls into the `IDirect3DDevice7` vtable: `+0x50`
is SetRenderState and `+0x94` is SetTextureStageState.

- **Filtering**: in the default configuration (`0x82CA40`, inside `fn_0082C8F0`, next to `OpenD3D`),
  MINFILTER and MAGFILTER = 2 (**linear**) in stages 0 and 1. `SetupThing` (0x4133A2… and `DrawBevBox` 0x413C20),
  in the 2D frontend, switches to 1 (point) while it draws and restores it afterwards.
- **There are no mipmaps**: `D3DTSS_MIPFILTER` is never set (D3D7's default value is NONE) and the DDS textures
  in `AllMeshes.g3d` only have one level.
- **There is no antialiasing**: neither `D3DRENDERSTATE_ANTIALIAS` nor `EDGEANTIALIAS` appear, nor
  `MAXANISOTROPY` either.
- Dither enabled (`DITHERENABLE`), which was for the 16-bit modes.
- **No cubic filter** (reviewed at the user's request): every MAGFILTER write (cache 0xEC8230/0xEC8630;
  `fn_0082C8F0` 0x82CADB, `fn_00836E20` 0x836FA4, `HelpDude::Feel` 0x5B9CAF, `SetupThing::DrawTab`/`DrawBevBox`) uses
  1 (point) or 2 (linear); never 3/4 (D3DTFG_FLATCUBIC / GAUSSIANCUBIC). There is no filtering option in the registry.
  The installation's `d3dim.dll` / `d3dim700.dll` are UCyborg's "resolution limit remover" (2016), they do not touch
  filtering. A softer look in the original would come from the driver or from the Windows compatibility layer.
- Cut-outs: the `TexturedChroma` materials (trees, fences) use threshold 0x96 (0.588); the pine has
  `MSH_T_CONIFER` 577 and `MSH_T_PINE` 590. Its skin (BGRA4) has ~94.5 % of texels with alpha 0 or 15 and the rest
  intermediate.
- The default configuration also turns on `D3DRENDERSTATE_SPECULARENABLE` (`fn_0082C8F0` 0x82CC54): the vertex
  specular is added to texture × diffuse.
- The mode table 0xC38728 is installed by `LH3DRender::Open` (0x82B4EA).

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x5E5510` | GLandscape::Open entry; it calls GLandAlignement::Open -> CloudInSky::Open, a new sky of clouds per land | `Clouds::OnLandscapeOpened (Locator.cpp)` |
| `si+0x1C..0x28` | projected shadow's box: x0, z0, x1, z1 (uploaded as x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)) | `u_shadowBox (vs_land_shadow.sc), u_dynamicShadowBox (fs_object_shadow.sc)` |
| `si+0x440` | projected shadow's least k = x d.x + z d.z; a vertex below it gets the clip code 0x400 | `u_shadowCull.z / u_dynamicShadowCull.z, ShadowKept (shadow.sh)` |
| `0x7FEB98..0x7FEBBE` | fn_007FEB30 (object haze): t = (min(max(z, near), far) - near) / (far - near) | `haze.sh HazeT` |
| `0x874C5B..0x874C9C` | fn_00874AA0 (land haze, class 1): the same t from the clamped view depth | `haze.sh HazeT, vs_terrain.sc` |
| `0x874C90` | fn_00874AA0: f = 256 - __ftol((256 - k) t) (object side 0x7FEBC3) | `haze.sh HazeFactor` |
| `0x7FEBED..0x7FEC30` | fn_007FEB30: diffuse (c f) >> 8 per byte, only when f < 256 (land side 0x874DA1..0x874DF0) | `haze.sh ApplyHazeDiffuse` |
| `0x7FEC32..0x7FEC7E, 0x874CD2..0x874D06` | object / land haze colour c t, each channel with fistp (round to nearest, halves to even) | `haze.sh HazeColour, RoundHalfEven` |
| `0x874D21..0x874D9E (and 0x874D21..0x874DF0)` | fn_00874AA0: specular + haze colour saturated at 0xFF, then the diffuse (c f) >> 8 | `haze.sh HazeAddSaturated, vs_terrain.sc haze block` |
| `0x8784A7..0x8784EC (0x8784A7..0x8784B8 for t')` | fn_00878350: land shadow uv u = (Lx + (x - Lx) t' - x0) / (x1 - x0), v in z | `shadow.sh LandShadowUv, vs_land_shadow.sc u_shadowLight` |
| `0x878502` | fn_00878350: per-vertex clip code 0x400 on the land (k below si+0x440) | `shadow.sh ShadowKept` |
| `0x87846D..0x87848A` | fn_00878350: shadow vertex diffuse 0 at altitude 1 or less | `vs_land_shadow.sc a_color3 / v_shoreFade` |
| `0x874B89 / 0x874B7A` | fn_00874AA0: land vertex diffuse = table[cell byte 3], specular = the cell colour | `vs_terrain.sc LandCell / LandTable` |

## openblack

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Other cases: temple, video and 2D**
  - openblack: see [Comparison with openblack](#comparison-with-openblack)

### Comparison with openblack

Status: **partial**. openblack draws the stages of the frame in the original's order; [Parity by stage](#parity-by-stage)
lists each one. The one harmless difference: openblack clears the colour to 0x274659 (the original does not clear it;
it is not visible). Missing: the drawn snow and the raindrops on the ground; leashes and gestures; the Bink decoder;
the creature's reflections and shadows. The Direct3D 7 states already match by default: bilinear, no mips and no
MSAA. Four-sample multisampling of the back buffer is a debug switch only (the debug bar's View menu, off by default;
the user's choice, 2026-10-21). Port decisions taken from raffclar's renderer: [Port decisions: engine and world](../project/port-decisions-engine-world.md).

**Depth tests.** openblack uses an inverted Z: `ZFunc::LessEqual` is GREATER, strict, where the original's 4 lets an
equal depth pass; `LessEqualInclusive` (GEQUAL) is the exact 4, `Less` the trails' 2. Equal depth needs the same
vertices through the same transform: a second pass over the same surface (SurfRevol's specular, the ghost's redraw),
or a triangle a mesh repeats. The passes are moved to the exact test one at a time, each measured; the SurfRevol
passes are.

**End-of-frame panels.** The creature's panels are drawn over the bars and the dialogue and under the fade through
`DrawSceneDesc::interfaceLayers` (`Renderer::DrawFinishFrameOverlays`), the interface's panel canvas sharing the end
of frame view.

Facts of the original that the parity table relies on:

- TexturedChroma = alpha test ≥0x96 **and** SA/ISA blending.
- The dynamic shadows fall on the land and, with ShadowsOnObjects, on the objects.
- Footprints, static shadows and snow baked into the block textures at the block texture queue's 16th step ([the prints](../world/terrain.md#the-prints-the-16th-step)).
- Clouds and cloud shadows from detail level 3.

openblack runs a logic-side copy of the object draw list of stage 4m ([6. The object draw list](object-draw-list.md#the-object-draw-list-stage-4m)),
with the script highlight as its only consumer (its pose, its glints, its active effect and its sparkles). Its rebuilds
come from all three of the original's triggers: the rebuild requests, the turn rule and the land flag. What else depends
on it in the original is stepped by openblack without it, every frame: the vortex's over-land effects and the seed
graphic's holder (see
[Pending](object-draw-list.md#pending)).

#### Parity by stage

Each stage links to its section. Status: **same** (verified), **approx.** (works but differs), **missing**.

| Stage | Original | openblack | Status |
|---|---|---|---|
| Clear | Colour is not cleared; the sky covers the screen | Clears to 0x274659 | approx. (not visible) |
| [Camera](../camera/game-camera.md#camera) | Horizontal FOV 70°, no far plane, near 0.3–3.5 depending on height | FOV 70°, near (height·0.05)·3.2 + 0.3 (0.3–3.5; 0.1 with `SET_GRAPHICS_CLIPPING`), far 65536 | same |
| [Sky](../world/sky.md#sky-sun-moon-and-clouds-original) | 9 images (alignment × time of day), 2 passes, darkens with storms, white with lightning; time thresholds 3.5/7.5/8/8.5 | Type and alignment, the original's thresholds | approx. (no weather) |
| [Sun and moon](../world/sky.md#sky-sun-moon-and-clouds-original) | `sun.l3d` (6–18 h) and its glow; `moon.l3d` with phase from the real clock and an additive halo; no stars | Same (glow occluded by the terrain with CPU rays); the moon reflected in the sea (halo + mirrored mesh) | approx. |
| [Clouds and cloud shadows](../world/sky.md#sky-sun-moon-and-clouds-original) | 70 clouds (`mist.l3d` + `smoke.raw`) moving with the wind, colour by alignment; shadows from `sclouds.raw` in the cell brightness | Same; a new layout (420 CRT draws) at every land open, at the script's `LOAD_LANDSCAPE`, as the original | same |
| [Reflected land](../world/sea-coast-and-rivers.md#sea) | Land only, no Z, half light, no small bump | Same | same |
| [Object reflections in the sea](shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects) | Hand (grey 0xA0A0A0 unlit) and what it holds, physics objects, the creature's body (y < 6, 0x65A0A0D0) and boats, mirrored and clipped above the water, with the colour left on them by `fn_00801C90` | The hand, what it holds, physics objects and boats (`DrawBoatReflection`); the creature is missing (it does not exist yet) | approx. |
| [Underwater](meshes-and-lod.md#cutting-by-the-water-plane) | Fish-farm fish (15 `misc0.raw` sprites per farm), before the sea; sharks and swimming SuperVillagers cut by y = 0 (`DrawCutByPlane`, 0xFF303070) | The fish shoals, the puzzle net and the sharks (`DrawCutByPlane`), inside the reflection target; there are no SuperVillagers | approx. |
| [Sea](../world/sea-coast-and-rivers.md#sea) | Mode 5 (ARGB4444), period 560 (level 4), wind (0 in game), 2 px rows from the edge of the 30000 quad, rippling, alpha 255→80 and top row at 0x20, level 0 with a ±70000 quad; no fog; the hand's night glow underneath | Same (rows redone per pixel in `fs_water`) | same |
| [Light table](../world/terrain.md#terrain-light-table) | `palette.raw` by time of day and alignment, overcast cap and lightning flash | Same (`LandLightTable::Build` with the overcast and the flash at the camera) | same |
| [Land](../world/terrain.md#terrain-detail-small-bump) | Blocks front to back, mode 14, per-vertex light table, small bump | Same | same |
| [Distance fog](../world/haze-and-mist.md#distance-haze-original-fog-detail-levels-36) | In software: land per vertex, models once per object; not on sea, sky or PSys particles | Same (with the storm and lightning haze) | same |
| [Static shadows](shadows-and-reflections.md#shadows-three-systems-in-the-original) | Baked into the block textures: Fixed/MobileObject/trees, shear x += h, z += h, ×0.5 | `StaticShadow` pass (256 px/block) into the blocks whose prints are drawn again that frame, as the block texture queue puts the prints in ([terrain.md](../world/terrain.md#the-block-texture-queue-in-openblack)), ×0.5 in `fs_terrain` | same (without the original's 4×2 AA) |
| [Projected shadows](shadows-and-reflections.md#projected-shadows-shadowinfo) | `ShadowInfo` list: 32×32 silhouettes rasterised on the CPU (alpha n/15, empty ring, fade at 50–80 radii with the 9-block test, baked to nibbles) of the hand, the creature, thrown objects, the boat (with the sun), the SuperVillagers and the PSys meshes; onto every land block they touch, and the hand, the creature and the boat also onto objects (mode 6, ZFUNC EQUAL, vertical projection), at the end of each receiver's Draw | Same (`graphics::shadow_list`, `RendererShadows.cpp`: one block redraw per shadow; onto the objects behind each one, in Main or in their slot in the queue). The hand as in the original (compared against the user's observation of the original, 2026-10-02); the creature as a complex caster in half rows; the prediction, the SuperVillagers and the PSys meshes are missing | approx. |
| [Villager and animal blobs](meshes-and-lod.md#villager-blobs-object-reflections-and-lod) | Two quads from the feet (bones 21 and 18) towards +X+Z (2·scale, on the terrain plane), width 0.4, alpha 1 → 0; animals: 4 quads from the EBone points of their mesh | Same (villagers and land animals) | same |
| Footprints | Burned into the block textures at a block's prints step | Footprint pass into the blocks whose prints are drawn again that frame ([terrain.md](../world/terrain.md#the-block-texture-queue-in-openblack)) | approx. |
| [Model lighting](model-lighting.md#model-lighting) | Land under the object (bilinear), integer rule `fn_0084BA90` (`I = fistp(255·N·L)` with ties to even, `f = amb + ((255 − amb)·I >> 8)` with ambient 90, diffuse `(c·f) >> 8`), point light [0xEA9E90] brought into mesh space (fixed sun by day, next to the hand in full night), additive specular, hand ×1.5 | Same (`src/Graphics/ModelLight.h` and `assets/shaders/model_light.sh`) | same |
| [L3D materials](textures.md#l3d-material-blending) | Type = mode; chroma with alpha test (ref − 5) **and** blending | Same, with per-material culling (bit 0 of byte +5; D3DCULL_CCW = CCW in bgfx) and wrap/clamp (bit 2) | same |
| [Transparent ordering](transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter) | Back-to-front Z-sorter (max. 2048, stable) | A single queue, `graphics::zsort` (stable insertion, cap 0x800, dist² key in `float`): models with their mesh's 0x200 flag, whole (`LH3DObject::AddDrawing` 0x815F0B; the rest immediately, 0x815F62), the single-use bubble, the hand, the elements of Sorted effects one by one and Queued ones whole, sprites, mists, clouds, smoke, rain per tile and the boat's sprites, in `MainBlended` (sequential); shadows on objects inside each receiver | same |
| [Model LOD](meshes-and-lod.md#villager-blobs-object-reflections-and-lod) | In this executable the loading of `LevelOfDetail` is disabled (NOP at 0x823810 / 0x823B43): always LOD 1, no fading or disappearing | Always LOD 1 | same |
| [Windows](../world/day-and-night.md) | `Abode::Draw` 0x515F70: unlit `isWindow` submeshes, grey 224..252 that flickers, with someone at home and at night | `night_lights::WindowColour` (+0x54 0xFFgggggg or 0, `PackInstanceWindow`); "someone at home" = `PresentAtHome` (+0xB6), raised and lowered by the villagers, then `IsVisualNight` ([day and night](../world/day-and-night.md)) | same |
| Hand | Always Z-sorter (`CHand::AddDrawing` 0x46D203), light ×1.5, wrist with alpha | Same: whole in the queue | same |
| [Chimney smoke](../world/objects.md#chimney-smoke-lh3dsmoke) | `LH3DSmoke` (10 `smoke.raw` sprites, mode 6) at the chimney (extra point [1] of meshes with flag 0x400) while there are villagers inside or the workshop is producing; the hand pushes it | Same (`RendererSmoke.cpp`); `PresentAtHome` is raised by the villagers; age not truncated; the workshops' scaffold count is missing | approx. |
| [Water rings](../world/water.md#water-rings) | `fn_005E5100`: horizontal `smoke.raw`, mode 13, 700 ms; from the hand's splash, objects falling into the water, swimmers, sharks and bait | Same (hand, thrown objects, sharks, waterfall and the fish puzzle; there are no swimmers) | same |
| [Boats](../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire) | `PetitNavire` (`PLAY_JC_SPECIAL(6)`): launch via `boat1.anm` with sailors, dust, splashes and 3 sounds; 60 s voyage via `boat2.anm` with deck and wake; reflection 0xFF303070 | Same (`ecs/MissionaryBoat`, `DisappearSmoke`; the sprites, one by one in the common queue); the hull shadow with the fixed sun, also onto objects | approx. |
| [Particles, spells, fireflies, sparkles](../physics/particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules) | PSys, everything in the Z-sorter | Generic PSys engine (`src/Particles`): the spell files with the ~25 most used classes, sprites sorted per effect; script effects (`SPECIAL_EFFECT_*`); fireflies; mesh atoms, mists, chains and light maps ([model lighting and colour](model-lighting.md)). Missing: the rules of specific spells and moving the hand's effects to this engine | approx. |
| [Rain, snow, lightning](../world/weather.md#the-drawn-rain) | By storm, in 80×80 tiles | The rain per tile in the common queue (`Rain.cpp`, `RendererRain.cpp`), the lightning flash and the storm clouds; no drawn snow, no drops on the ground | approx. |
| Leashes, gestures | Modes 15 and 13; the leash's shadow strip on the land | The leash's rope and its shadow ([drawing the leash](../creature/leash-rope.md#drawing-the-leash), `Graphics/RendererLeash.cpp`; no leash is worn on Land 1 or Land 2); no gestures | approx. (leashes), missing (gestures) |
| [Influence border](../magic/influence-and-alignment.md#influence) | `InfluenceCircle::Draw(1)` 0x826C90 every frame in the world view: a 40-high curtain per citadel and town, `burn.raw`, mode 6, scrolled, at once before the Z-sorter drain; rebuilt every 10 turns; same-player overlaps pruned; hand-crossing ripple (7 `smoke.raw` rings, Z-sorted) | Same (`ECS/Influence/InfluenceCircles.cpp`, `RendererInfluence.cpp`); the border latch is set when the player has a temple (no temple fade); no per-land colour remap | approx. |
| [Names, counters, help](../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message) | Z-sorter and end-of-frame callbacks; fonts `j0`/`f1`/`f3`; texts from `InfoScript2.txt` | Original `j0` font and the message for the amount in the hand; the other messages, names and counters are missing | approx. |
| [Sprites](transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter) | `LH3DSprite` in the Z-sorter | In the main pass, in the common queue (`components::Sprite`, smoke, boat sprites one by one) | same |
| [Screen fade and bars](../interface/screen-and-readouts.md#screen-fade-and-cinema-bars) | `SET_FADE`/`SET_FADE_IN` per turn; `SET_WIDESCREEN` bars over 2 s; quads in FinishFrame | Same (`ScreenOverlay` view), with the initial black of `OnNewGame` under its conditions: Land 1 only, and never after the question's pause; the citadel's `Temple::UpdateFade` is not ported | approx. |
| [Bink video](../video/bink.md#openblack) | `DrawToScreen` 0x54DC6D → `thedraw` 0x844E30 → fn_00845740: one quad per 256x256 tile in mode 6, ZFUNC ALWAYS, no Z; no world while it covers the screen (0x54DD7D) | `Renderer::DrawVideoOverlay` in `ScreenOverlay` (one texture, the same texels per tile), the world is not drawn with `CoversScreen()`; still without decoder (black) or opcode 203 | in progress |
| Temple and citadel | Their own lights, Citadel* keys | `TempleInterior` | approx. |
| Gamma, post-processing, D3D fog | None | None | same |

Level of detail: the original's, 4 by default (`--detail-level`, `Graphics/DetailLevel.h`).

> **Code rules.** Each stage keeps its state in ECS components or Locator services, loads its assets through the
> resource caches and keeps its pure logic testable with fakes in `test/`; comments in the code describe behaviour
> in plain English, with no decompiled names or addresses (those belong in these pages).

### Code map

The source files of this page and the symbols their notes describe:

- `src/Graphics/Renderer.h`: `DrawIntroLight`.
- `src/Graphics/RendererIntroLight.cpp`: `Renderer::DrawIntroLight`.

### Test hooks
The page's own variables; the full list is in [Debug environment variables](../project/debugging.md#debug-environment-variables).

- `OPENBLACK_SEA_TRACE=1` (sea rows every 500 frames) and the test `test_sea_rows`.
- `OPENBLACK_DUMP_BLOCK_TEXTURE` (coast block texture) and the test `test_land_light` (light table).
- `test_haze_land_light` (haze, land light and stamps); `OPENBLACK_TEST_STORM_CLOUDS`, `OPENBLACK_TEST_SPELL`
  and `OPENBLACK_TEST_WEATHER` to see the stamps (storm shadow, lightning light and fireball).
- `OPENBLACK_CLOUD_SEED=<n>`, `OPENBLACK_TIME_OF_DAY=<h>` and `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (sky and clouds).
- `OPENBLACK_TEST_FADE="r,g,b,seconds"` and `OPENBLACK_TEST_WIDESCREEN=1` (fade and bars).
- `OPENBLACK_TEST_TOOLTIP=<n>` (hand message).
- Projected shadows: `OPENBLACK_SHADOW_TRACE=1` (once per second, each `ShadowInfo` with its light, alpha, fade,
  box, t' and maximum n, and the objects that receive a shadow with the view in which it is drawn) and
  `OPENBLACK_DUMP_SHADOWS=<folder>` (each texture ×8 as PNG every 300 frames); the test `test_shadow_math`.

## Pending

- The model level of detail of the v1.42 exe is settled; still to read before it is wired: which mesh the
  reflections, the shadows and the pick use. See
  [The model level of detail in v1.42](visible-blocks-and-lod.md#the-model-level-of-detail-in-v142).
- The stages still missing or in progress in the table: the drawn snow and the raindrops on the ground, leashes and
  gestures, the Bink decoder (and opcode 203), the creature (reflections and shadows), the prediction, the
  SuperVillagers and the PSys meshes in the projected shadows.
- CRT draws that differ from the original: none is listed any more. The citadel heart's 2 lantern lights (12 draws) are
  made and the font cache's 255 `rand()` at start-up are spent, see
  [day and night](../world/day-and-night.md).
- Fade: `Temple::UpdateFade` in the citadel.
- Text: the other messages (when hovering over piles and stores, "Pick up"...) are not yet read; the offset of the
  text relative to the hand is an estimate, not yet read in the original.
- Sea: the role of `fn_008792E0`, listed with `fn_00879500` / `fn_00879930` / `fn_0087A090` as "the CPU side of the
  original's sea", is not yet read.
- The creature's projected shadow, still to read:
  - its scale (obj+0x44 of the body, which the fade and the light radius read) while the creature is drawn smaller in
    its pen: openblack takes the Transform's scale (its real size), not the drawn one; where `LH3DCreature::SetSize`'s
    drawn size (+0x94) reaches the body object has not been found;
  - its held object (si+0x00 = obj+0x8C, written only by `SetHeldObject` vt+0x234): no call on a creature's body was
    found, so openblack rasterises none **🔍 inferred**;
  - whether the silhouette is the morphed body or, as in openblack, the posed base mesh;
  - it has not been compared with the original's picture: no capture of the creature's shadow, and none of the
    checked land shots has the creature in view.
- A lost GPU device (information, openblack only, not the original): under load (three or four games drawing at
  once on one NVIDIA GPU) the driver resets a game's channel, and the game crashes on its next frame inside
  nvoglv64.dll, reading 0x108. The call is bgfx's Vulkan renderer: `RendererContextVK::submit` starts the frame's
  GPU timer (`TimerQueryVK::begin`, `vkCmdResetQueryPool` and `vkCmdWriteTimestamp`) on the frame's command buffer,
  whose allocation after the reset failed (`vkWaitForFences` with the device lost), which bgfx's release build does
  not check. Each crash second has an `nvlddmkm` event 153 in Windows' System log. Not a regression: every build has the same bgfx. The test runs keep to two games at
  a time and count such a crash as a run to repeat. What is not done: bgfx exposes no lost device, so the game
  cannot stop drawing before it; that needs bgfx itself.
- Particle meshes: `UseScriptHightlightPulse`, `UseDynamicLighting`, `UseGlobalAlpha`, the per-object Z
  order and `FaceCameraSprite`; from `ParticleAnimCreator`, `AnimEnum`, the blend to `MeshFileName1/2`,
  `UseDynamicLighting` and `UseGlobalAlpha`. The original lighting of the particles depends on these
  `UseDynamicLighting` flags.
