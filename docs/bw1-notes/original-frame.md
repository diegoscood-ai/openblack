# The original B&W frame (runblack.exe W120, D3D7, LH3D)

Map of a game frame of the original, with addresses: how the frame is launched, the draw order of each
stage, the 19 render modes (D3D states per L3D material type) and the global state (projection, haze,
clearing, levels of detail). The status of each stage in openblack is in [parity.md](parity.md) and the details of what has been
done in [rendering.md](rendering.md) (the world) and [rendering-objects.md](rendering-objects.md) (the models).

Progress: [Render pipeline](../progress/rendering/render_pipeline.md) (what our tree does of it, row by row).

Everything is **faithful** (read in the executable) except what is marked **(inferred)**.

- [1. Frame loop](#1-frame-loop)
- [2. Draw stages in order](#2-draw-stages-in-order)
  - [Other cases: temple, video and 2D](#other-cases-temple-video-and-2d)
- [3. Render modes](#3-render-modes)
- [4. Global state](#4-global-state)
- [5. Comparison with openblack](#5-comparison-with-openblack)
- [6. The object draw list (stage 4m)](#6-the-object-draw-list-stage-4m)
- [Pending](#pending)

## 1. Frame loop

`GGame::Loop` 0x54CF20 (decomp `src/Black/Game.cpp`), on each iteration:

- ProcessGraphicsEngine 0x54D850 → [mouse, camera->Update (sets the FOV and the dynamic near plane),
  GInterface::PreDrawProcess 0x5CE9E0 (hand collision, disciple icon), **Process3dEngine 0x54DA80**,
  BMan/debug camera editor, GInterface::PostDrawProcess (only updates leash and collision, does not draw),
  HelpSystem::PostDrawProcess (flag)] → ScriptedScreenShot → **GGame::FlipScreen 0x54D800 → LHScreen::Flip
  0x7DE090** (timing text, software LHMouse::Draw, LHFlip 0x7DE580 and then the frame *clear*
  fn_0082EE70, see [4. Global state](#4-global-state)).
- Process3dEngine: LH3DRender::StartFrame 0x82F0E0 → video (Bink) → switch field_0x205a28 (0 = world,
  1 = citadel/temple, 2 = falling-spell video) → fade / help / influence → **LH3DRender::FinishFrame
  0x82F460** (Z-sort flush + callbacks + EndScene) → debug 2D (CreatureMentalEditor, leash info,
  DisplayHowImpressed, CPU players, countdown text) → LH3DAtmos::Render2D (debug weather map
  with Lock, **(inferred)**).
- StartFrame: fninit/FPU control, delta time (g_delta_time 0xC38134, smoothed fps 0xEC7FC0),
  **BeginScene** (vt+0x14), g_frame++, g_started_frame=1, **zsorter reset fn_0083F3B0**, fn_00813770,
  fn_0085BF00, SetLight/SetProjMatrix only with hardware T&L (never: start_system forces [0xC386E4]=1),
  fn_00821270.

## 2. Draw stages in order

Case 0 = world.

| # | Stage | Function | What it does / states | Order / culling |
|---|---|---|---|---|
| 1 | landscape.PreDraw | GLandscape::PreDraw 0x5E3F60 → LH3DIsland::PreDraw 0x7FF2D0 → fn_00877210 | list of visible blocks (32×32 blocks of 160), fog class per block +0x940 | frustum and near-plane culling with each block's box; list [0xFA92D8] (next +0x9B8) sorted by distance +0x9BC **front to back** ([stage A](#stage-a-the-visible-blocks-fn_007ff610)) |
| 2 | shadow textures | TemporaryShadow::UpdateAll 0x825190 (and fn_00874850 per SuperVillager) | silhouettes rasterised by the CPU into per-object shadow textures (fn_008801D0, **(inferred)**), list 0xFAA7E0 | — |
| 3 | preparation | LH3DCreature::PrepareForDrawing 0x4ED320 per creature, CHand::PrepareForDrawing 0x46C550, PSysLightMaps::AddDrawing 0x6CA6E0, LH3DLandscape::TextureUpdateThread 0x871F00 (block textures: footprints and decals fn_008721A0/fn_00872FA0), LH3DAtmos::Update3D | does not draw | — |
| 4 | GLandscape::Draw 0x5E42E0 | (detail in 4a–4o) | | |
| 4a | preparation | 3D cursor (Get3DPointFromScreen), fn_00802550, Windmill::PreDraw, Tree::PreDraw (swaying in the wind), fn_008296D0 (8 s fade timer, mode 0xF), fn_005E5830 (hand position; hand night light fn_00823460/fn_0086D360; clouds fn_005E25C0 with the Clouds key) | | |
| 4b | **sky** | GLandAlignement::DrawSky 0x5E2160 (not in wireframe) → fn_0086A330 → light table fn_00869850 + fog parameters, fn_0086B7F0 (sky_{good,ntrl,evil}_{day,dusk,night}.555 → 3 textures, mode 2) → fn_0086B010 | **moon** moon.l3d + additive glow quad 500 (AdditiveMaterial mode 13), and a copy mirrored in Y (reflection, **(inferred)**); **dome** sky.l3d with ZFUNC ALWAYS, 2 passes (texture of the base alignment with α255, then the 2nd alignment with global alpha ftol(X·255) or ftol((X−1)·255), X the sky alignment 0 good..2 evil), towards the haze colour with the overcast, darkened on the good side, towards white with lightning; **sun** sun.l3d fn_0086C140, colour 0x957C63, visible from 6 to 18 h, ÷(1+8·clouds). **No stars.** | the sky covers the whole screen (the colour buffer is not cleared) |
| 4c | **reflected land** (LandRef key) | fn_007FF4F0, ZWRITEENABLE off around it (0x5E48B3–0x5E4900) | heights ×−1, light table ×0.5, small bump forced off; land only, no models | mirrored block list |
| 4d | reflections in the sea | PetitNavire::PreDraw 0x5DFF20 (ships); hand DrawUnderWater vt+0x118 with specular 0x65A0A0A0 through the **alternative mode table 0xC387C8**; object in the creature's hand; fn_00646FE0 (physics objects), fn_00775120 (sharks, cut by the water plane, 0x5E4B26), fn_00824B90 (FishFarm fish and the fish puzzle nets), fn_005DFCE0 (footprint UV2, **(inferred)**) | before the sea. The hand, what it holds, the physics objects and the ships are **reflections**: `DrawUnderWater` draws them mirrored in y = 0 and clipped to what is above the water (see [rendering-objects.md](rendering-objects.md#object-reflections-and-hand-shadow-on-objects); it had previously been assumed they were their parts under the water); the FishFarm fish are sprites under the water | |
| 4e | SuperVillagers | list 0xEB9A08: Draw vt+0x610 + shadow fn_00874850; swimming villagers generate water rings | | |
| 4f | hand night glow | fn_005E3F70: additive quad on the ground of ±60, ZFUNC ALWAYS | | |
| 4g | **sea** | fn_00879930 (skipped if [0xECA664] or in wireframe): sky.raw/skya.raw mode 5, rows of 2 px, ZFUNC ALWAYS, no Z write, colour = light table[255], alpha 255→80 between 7000 and 14000, period P = 2000−1800·WaterTiling | | |
| 4h | vortex | fn_005FF310 → fn_005FFBB0 (LandscapeVortex, **(inferred)**) | | |
| 4i | **land** | fn_007FF610: per visible block front to back, transition blocks fn_00877D20, block fn_00874AA0 / SSE fn_007A1800 (blocks in mode 14, per-vertex light table, software haze, 2nd small bump pass in mode 14), then **projected dynamic shadows** per block fn_00878350 (shadow material mode 6) | front to back |
| 4j | debug | fn_0081F820: field-of-view triangles of the scripts | | |
| 4k | citadel heart | fn_00467360 → CitadelHeart::DrawNow 0x4670D0 | | |
| 4l | water rings | fn_005E5100 over 1024×0x38 at 0xEAB7C8 (GWater circle sprite, direct LH3DSprite::Draw) | | |
| 4m | **models** | fn_005E5CD0: object draw list (≤3000, GLandscape::DrawObjects 0xD1D28C) of the visible blocks (dist < VanishObjectDist 100000) + global list; it is rebuilt according to DrawListRebuildCount / more than 10 turns since the last rebuild / a block's land visibility change; with the camera still only the last objects on screen are redrawn ([6. The object draw list](#6-the-object-draw-list-stage-4m)). Draw of an object (e.g. MobileObject::Draw 0x518150): light fn_00801C90 ([rendering-objects.md](rendering-objects.md#model-lighting)), fog fn_007FEB30, Game3DObject::AddForDrawing 0x63B5D0 → LH3DObject::AddDrawing fn_00815A70: frustum CheckRegionOnScreen 0x868C80, **LOD** by distance (23.3f/66.7f/86.7f → LOD 1/2/4, fade 86.7f–173.3f, culled beyond that if IsDisappear; distant humans → impostor sprite; **inactive in this executable**: the LevelOfDetail loads are disabled, always LOD 1, see [rendering-objects.md](rendering-objects.md#villager-blobs-object-reflections-and-lod)), IsGlowing → glow sprite, **NeedSorting (mesh with alpha, flag 0x200) → Z-sorter; otherwise immediate Draw** | opaque: block order, unsorted; with alpha: Z-sorter |
| 4m′ | the creature, in the list | Creature::Draw 0x517910 → LH3DCreature::AddForDrawing 0x48E1C0 → DrawNow 0x48E260 | body (fn_00813340: at once; the Z-sorter only with the mesh flag 0x200 or global alpha, which no creature has), the shadows over it, its hair, then its eyes at once ([creature.md](creature.md#drawing-in-the-frame)) | at once, in the list's order |
| 4n | rest of 5E5CD0 | SuperVillagers list 0xEB9A10 (OverrideMaterial with forced alpha-ref 0xA), game list vt+0x610, markers, Reward sprites, PetitNavire::PostDraw, hand_intro, sprite emitters fn_008274A0/fn_00823570/fn_00827B90/fn_00828E50 | | |
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
| 14 | counters | ValueSpinner::Update/AddDrawing (not during the help cinematic) | Z-sorter (**(inferred)**) | |
| 15 | debug | LH3DStorm::DebugDrawAll, LH3DAtmos::DrawWindField | | |
| 16 | **weather** | LH3DAtmos::Render3D 0x836250: per storm (list 0xFA92D8…, storms at dist < 560) and per 80×80 tile, intensity from GetWeather at the 4 corners; if >5: rain fn_008341B0 → Z-sorter callback 0x833F80 (streaks, AtmosMaterial mode 6 atmos.raw, splashes g_water_drop_cb, amount according to the RainSplash key); snow fn_00834290 → callback 0x834120 (snow.raw mode 9). Not lightning: LightSheet::DoTheDrawing 0x83E8C0 (mode 13) is LH3D's general sheet, queued in the Z-sorter (callback 0x4246E0) by its owners, the recognised gesture (step 13, fn_00688320), the arena's GLightSheet::Draw, the spell icon and the worship totem, and called directly for the camera force field (step 17, 0x54E198); see [magic.md](magic.md#the-light-sheet-lh3d-lightsheet-as-read-from-the-exe). Rain colour = (light table base/2 & 0x7f7f7f)+0x7f7f7f. | Z-sorter by the tile's distance² | |
| 17 | camera force field | ForceField of CameraModeNew3 (only with the flag) | | |
| 18 | villager names | VillagerName::AddDrawing → Z-sorter | | |
| 19 | fade | GScript::ProcessFade 0x6EB9D0 or Temple::UpdateFade 0x794280 → sets the screen fade colour [0xFA51D8] (drawn in FinishFrame) | | |
| 20 | 3D help | HelpSystem::Draw3D 0x5C59A0 (help character, arrows) | | |
| 21 | ClearLight 0x5E57B0 | removes the hand light (fn_00822F90/fn_00823780/fn_0086D460) | | |
| 22 | power spins | PowerSpinRunner → Z-sorter (PowerSpin::Draw) | | |
| 23 | influence ring | InfluenceCircle::Draw 0x826C90 (in the world only if the camera is more than 100 high: alpha 0→120 between 100 and 200; mode 6, no culling, wrap, UV offset 0.0001/−0.0002 per ms), Draw3DWorldTriangle **immediate** | | |
| 24 | **FinishFrame 0x82F460** | (a) **Z-sorter flush fn_0082F280** back to front (key = dist² to the camera, max. 2048 entries); (b) "before" callbacks (ascending priority, 0xEC8130): FallingSpell (0x526480), HelpDude 0x5C2E30 (100), CameraModeNew3 0x4562E0 (1000); (c) fn_0086BB60 the sun's glare (verified, 0x82F4C9: [day-night-weather.md](day-night-weather.md#the-sun-and-its-glare)); (d) **Z-reset quad**: full-screen quad FVF 0x1C4, z=1, rhw=0, colour 0, material [0xEDD494] mode 1, ZFUNC ALWAYS → Z=1 over the whole screen; (e) cinema bars if [0xEB9950] (2D rectangles queued fn_0081E590, height fn_0081E8B0); (f) "after" callbacks: HelpDude 0x5C2E10 (100), **flush of the 2D rectangle queue 0x81E7D0→fn_0081E3C0 (10000; mode 1, ZFUNC ALWAYS, no Z write)**, HelpText 0x5CD020 (20000: help text boxes), LHVideoPlayer::thedraw 0x844E30 (0x8000), start_system 0x6424E0 (0xA0000); (g) fn_00836200 debug weather; (h) **screen fade fn_0086FEE0**: full-screen quad of colour [0xFA51D8], mode 1, ZFUNC ALWAYS, if alpha≠0; then the bar rectangles and the fade reset; (i) EndScene (vt+0x18) | | |

### Other cases: temple, video and 2D

- Case 1 (citadel/temple): Update3D, LH3DSky::g_b_we_are_inside_citadel=1, TemporaryShadow::UpdateAll, DrawSky
  (no sun, hence no glare at the end of the frame), Temple::Draw 0x794370 (its own lights with fn_0081E1F0 SetLight,
  rooms; Citadel* detail keys), Temple::Update, liquid particles. Details in
  [rendering.md](rendering.md#the-temple-interior).
- Case 2: FallingSpell video + liquid particles.
- Video: LHVideoPlayer::DrawToScreen (16:9 bars, alpha fade; FallingSpell 80).
- HUD and 2D: B&W has no classic HUD; the 2D consists of the help text boxes (callback), tooltips and text with
  GatheringText (fonts in mode 6), queued rectangles, fade, bars and the software cursor in Flip
  (**(inferred)**: the cursor is usually the 3D hand).

## 3. Render modes

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
nobody reads it ([rendering-objects.md](rendering-objects.md#render-modes-and-materials-render_modes)).

## 4. Global state

- **Projection.** No hardware T&L: CPU transformation to XYZRHW. Horizontal FOV [0xEA1DD0], default 70°
  (1.22173), aspect width/height [0xE839EC]; sz = 1−near/z, rhw = near/z, **no far plane**. Near plane
  [0xE839E0] dynamic according to the camera's height above the ground: (h·0.05)·3.2 + 0.3 in float steps, 0.3 at or
  below the ground, 3.5 above 20 (0.1 with `SET_GRAPHICS_CLIPPING`, 0.2 in the citadel with fov 90°; see
  [rendering.md](rendering.md#camera)).
- **Fog.** No D3D fog. Software haze (Fog key [0xC37204]): start and end according to the sky type
  (noon/midnight 400→900, dusk 100→800; storm →15/350), colour = light table base/3; per vertex
  it darkens the diffuse towards "dark" and adds the fog RGB to the specular (**(inferred)**); land per block +0x940,
  objects fn_007FEB30.
- **Clearing.** In Flip, after presenting, only every ~2000 ms (2 frames) in game: black 0xFF000000, z=1. The Z
  is reset every frame with the FinishFrame quad; the colour buffer depends on the sky.
- **Gamma and filtering.** No gamma. LightBoost (only in custom detail) changes the light table divisor.
  Bilinear filtering, no mips, no AA, dithering enabled ([rendering.md](rendering.md)).
- **Level of detail.** fn_00823AD0 from start_system 0x643026; level = detailidx from the registry (<5), otherwise **4**;
  5 = custom (fn_008237B0 reads each key), never reached at start-up (see
  [The detail index](#the-detail-index)). Table 0x9A3704 (L0..L6):
  - Live: LevelOfDetail (dead), UseSmallBump 1 in all, Clouds/CloudShadows 0001111, WaterTiling
    0/.2/.4/.6/**.8**/.5/1 (level 4 → sea period **560**; 200 only in L6), LandRef 0001111, CitadelReflections
    0000111, CitadelLightmaps 0111111, CitadelGlows/People 0011111, CitadelVolumeLight 0001111, RainSplash
    0,0,3,5,8,8,8, LightBoost 0, Fog 0001111.
  - Only at startup: Weather 0001111, Light 0011111 (object dynamic light flag 0x20), ShadowsOnObjects 0001111
    (flag 0x40), UseHighTexture 0000111 (land textures of 256 versus 128 px), UseMultiLayerOnLandscape
    inverted (1 only in L0), FixeLand.
  - HardwareTnL/MaxObjectDistance are read but ignored. The land detail distances 0xE9C508 depend
    on the number of textures in VRAM. VanishObjectDist 100000 (script).

## 5. Comparison with openblack

The up-to-date parity table is [parity.md](parity.md). The list that was here (2026-09-29) has been removed because a good
part of it was no longer true: it listed as absent in openblack the sun and the moon, the clouds, the object reflections in the sea,
the haze, the dynamic shadows, the particles, the fade and the bars and the water rings, and as different the
FOV, the order of transparents and sprites, the LOD and the TexturedChroma alpha test; today all of that is done (identical
or approximate to the original) according to parity.md.

The only harmless difference remains the same: openblack clears the colour to 0x274659 (the original does not clear it; it is not visible).
What is missing according to parity.md: the drawn snow and the raindrops on the ground; leashes and gestures; the Bink
decoder; the creature (reflections and shadows).

Original data that was only in that list:

- TexturedChroma = alpha test ≥0x96 **and** SA/ISA blending.
- The dynamic shadows fall on the land and, with ShadowsOnObjects, on the objects.
- Footprints and decals baked into the block textures.
- Clouds and cloud shadows from detail level 3.

openblack runs a logic-side copy of the object draw list of stage 4m ([6. The object draw list](#6-the-object-draw-list-stage-4m)),
with the script highlight's glints as its only consumer. Its rebuilds come from all three of the original's triggers:
the rebuild requests, the turn rule and the land flag. What else depends on it in the original is stepped by openblack
without it, every frame: the highlight's pose, the vortex's over-land effects and the seed graphic's holder (see
[Pending](#pending)).

## 6. The object draw list (stage 4m)

fn_005E5CD0 decides, each drawn frame, which objects have their Draw (vt +0x610) called. In four steps: the land pass
finds and sorts the visible blocks (A); on some frames the list is rebuilt from those blocks (B); a pass calls Draw on
every listed object, or only on those that were on screen at the last full pass (C); and each Draw tells the list
whether its object was on screen (D). Anything an object does inside its Draw (a particle step, a light map) therefore
happens only on the frames the list reaches it.

### Reference executable

- This section reads the copy that runs: `Black and White Complete Collection\runblack.exe`, sha1
  `1e73ba9a96c3d6316e9f8f41a8947a25790d2698`. That copy is the one reproduced, and the one the checks measure.
- bw1-decomp's W120 build (`orig/BW1W120/runblack-decrypted.exe`, sha1 `bccccbd3a08fe9a5d6cf59cd114e916401abfb51` in
  its `config/BW1W120/build.sha1`) only guides the reading. Where the two differ, the running copy wins.
- Known differences: the running copy has nop runs where code was removed. They are in fn_00804E60
  (`0x804E60..0x804E68`, `0x804E72..0x804E7A`), in start_system (`0x642FF8..0x643003`, `0x643009..0x64301E`), at
  `0x823810` in fn_008237B0 and at `0x823B43` in fn_00823AD0 (6 bytes each, where an `fstp` of the LevelOfDetail value
  × 0.0001 would fit). What the bw1-decomp build has at those places was not compared.

### Where it runs in a frame

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

### Stage A: the visible blocks (fn_007FF610)

```
[0xEA9EA0] = &g_world_to_clipping (0xEA9E40)
LH3DIsland::PreDraw()                 // once per frame: does nothing if [0xE9CD74] != 0, sets it at 0x7FF4E2;
                                      // fn_007FF610 clears it at 0x7FF7AD
  for n in 1 .. [0xE9CD70]-1:         // g_ptr_blocks[0xE9C564 + 4n]; slot 0 is never used
    fn_00877210(block n)              // called at 0x7FF32A
      box: x = +0x90C+80, z = +0x910+80, half extents 80; H = int(+0x924) * [0xC3720C] (0.67)
           LandRef [0xE9CD8C] != 0: y from -H to +H;  otherwise y from 0 to H
      for each of the 8 corners: (cx, cy, d) = corner through g_world_to_clipping (rows 0xEA9E40/4C/58, t 0xEA9E64)
           near  : !(d >= [0xE839E0])                 // fcom/test ah,1: a NaN depth sets near (0x8773E4..0x8773F1)
           right : cx > d,  else left  : -d > cx
           top   : cy > d,  else bottom: -d > cy       // no far plane
      if any of the 5 masks == 0xFF: +0x920 &= ~5; +0x930 = 0; +0x934 = 4; return     // 0x877CDD, old +0x9BC kept
      +0x920 |= 5;  +0x91C = (all masks 0) ? 0 : 1
      +0x9BC = sqrt((oz*oz + oy*oy) + ox*ox), o = centre - cam  // 0x877C8A..0x877CCD, not squared
               centre (x+80, LandRef ? 0 : H/2, z+80), cam = g_camera 0xEA1DB8
  [0xFA92D8] = 0
  for n in 1 .. count-1 with +0x920 & 1: insertion sort ascending by +0x9BC (next = +0x9B8)
    // walk while cur.+0x9BC < new.+0x9BC or the compare is unordered (test ah,1), insert before the first cur
    // that is >=, so on equal distances the later block goes before the earlier one, a NaN new distance walks
    // to the end, and a NaN already in the list is walked past     (0x7FF45F..0x7FF4DD)
[0xC37208] = 0                                        // 0x7FF62B
for each block b in [0xFA92D8]:                       // 0x7FF639..0x7FF791
  if b.+0x934 in {1, 3}: copy b to 0xE9B6E0; fn_00877D20(copy, +0x934 == 1); t = copy   else t = b
  bit0 = fn_00874AA0(t) != 0
  b.+0x920 bit0 = bit0 (the copy too)
  if bit0 != b.+0x920 bit1: b.+0x920 bit1 = bit0; [0xC37208] = 1          // 0x7FF759..0x7FF783
```

fn_00874AA0 returns 1 exactly when the block keeps at least one front-facing land triangle after clipping. It clears
the list size at `0x875314`, sets the result to 0 at `0x875350` and to 1 at `0x87539B` only for a non-empty list, and
returns it at `0x875C49`.

- The vertices are a grid with a step of `1 << +0x930` cells (`0x874AC9`), `16/step + 1` per side (17, 9 or 5),
  stored compactly (index `row*n + col`) with the altitude of record `(R*step, C*step)`; `g_NumClipVerts` is
  `(16/step + 1)^2` (`0x875326`). The level of detail is [below](#the-blocks-level-of-detail):
  - height `y = alt > 3 ? alt * [0xC3720C] : 0` (the compares with 3 are at `0x874B98` and `0x875065`);
  - screen `sx = (x/d + 1) * [0xE839F0]`, clamped to `[0, [0xC2AB00]]`, and `sy = [0xE839F4] - (y/d) * [0xE839F4]`,
    clamped to `[0, [0xC2AB04]]`. `[0xE839F0]` and `[0xE839F4]` are half the viewport's width and height:
    UpdateViewPort `0x819030` multiplies the width `0xE839E4` and the height `0xE839E8` by 0.5 and stores them at
    `0x81905E` and `0x819070`.
- Outcodes are computed only when `+0x91C != 0`, that is, when the block is partly outside (`0x874E04..0x874E72`): 0x20
  near, 0x10 or 8 for x outside ±d, 4 or 2 for y outside ±d.
- A triangle counts when all of these hold:
  - its cell is not skipped: a cell with cell word `+6 & 0x200` is skipped, but only when `+0x934 == 0` (`0x876A8A`,
    `0x875DD8`); the path at `0x876CEF` has no such test. The cell read is the one the walk's pointer reaches, which
    does not follow the level of detail ([the cell-flag pointer](#the-blocks-level-of-detail));
  - the diagonal comes from cell word `+6 & 0x80`, and the seam fans (`+0x93C`, `+0x930`) are included;
  - either its three outcodes are 0, or `(c0 & c1 & c2) == 0` and a piece survives fn_0081A760's clip against the near,
    left, right, top and bottom planes. Clipped vertices come from fn_0081DD90 and are projected and clamped the same
    way (`0x81E0EB..0x81E158`);
  - `A = (P1.x-P0.x)*(P2.y-P0.y) - (P1.y-P0.y)*(P2.x-P0.x) > 0`, strictly (0 and NaN fail), on the clamped screen points
    in emit order (`0x81A99C..0x81AA7C`). g_NoBackfaceCull is forced to 0 (`0x876A3D`, `0x875D8E`), and `[0xFA92DC]`
    is 0 in this pass.
- The frustum-culled blocks are not in the list. fn_00877210 clears their bits 0 and 2 but keeps bit 1, so they never
  set `[0xC37208]`.
- LH3DIsland::Release also sets `[0xC37208] = 1` (`0x804816`).
- `[0xEA9EB4]` picks the code path:
  - its one writer is fn_007AC9E0 (`0x7AC9ED`), called once from LH3DRender::Open `0x82B52C`; it copies IsPentium4;
  - IsPentium4 is set when the CPUID base family is 0xF with SSE2 and OS FXSR (`0x8A2610`, `0x7ACA00`), with no vendor
    test;
  - set (Pentium 4, AMD K8 to Zen), the SSE path fn_007A1800 runs, and CheckCPU also turns on flush-to-zero
    (`0x7ACA83..0x7ACA95`). It builds the same tests (`0x7A2D02..0x7A2D23`, then fn_007A98B0, fn_007AA8F0 and
    fn_007A3A50, `0x7AAB14..0x7AABAD`) and returns 0 for an empty list (`0x7A2D28`), 1 otherwise (`0x7A2D45`);
  - not set (Intel family 6, so every Core and later), the x87 path runs.
- `[0xC3720C]` is 0.67 in every read that matters here. Its only writers are two `fstp` in the reflected land pass
  fn_007FF4F0 (called from GLandscape::Draw `0x5E48DD`, active only with LandRef), which negates it at `0x7FF52F`,
  after its own PreDraw (`0x7FF510`), and restores it exactly at `0x7FF5F3`. That pass calls fn_00874AA0 for every
  listed block and discards the result: it writes no `+0x920` bit and no `[0xC37208]`. With LandRef on, the frame's
  PreDraw therefore runs inside that pass and the main pass's PreDraw does nothing; the camera is the same for both, so
  the block list is the same.

#### The block's level of detail

What follows is the x87 path. The SSE path (fn_007A1800, fn_007ACCE0, fn_007ACD00; it reads the seam table at
`0x7AA499` and `0x7AB12C`) was not read for it.

**The lines.** Four lines across the land, structs of 0x28 bytes: `+0x00` n, a horizontal unit direction along the
line (n.y is 0); `+0x0C` p, a point on it; `+0x18` valid (nothing in the cull or the morph reads it); `+0x1C` dist;
`+0x20` W, the band's full width (the morph's ramp); `+0x24` T, its half width (the cull's test; T = W/2 in every set).

| Line | Built by | Static init (pointer table `0x9C7D24..0x9C7D38`) | LH3DIsland::Create, detail index 4 only |
|---|---|---|---|
| L0 `0xE9C4B8` | fn_007FF0E0 (y_ref 0) | 600, W 50, T 25 (fn_007FE930) | 2500, W 550, T 275 (`0x803E25..0x803E7B`) |
| L1 `0xE9C4E0` | fn_007FF0E0 (y_ref 0) | 300, W 50, T 25 (fn_007FE8B0) | 1600, W 550, T 275 (`0x803DC5..0x803E1B`) |
| L2 `0xE9C508` | fn_007FEE60 (y_ref = min(cam.y, 110.55)) | 300, W 40, T 20 (fn_007FE830) | 400/70/35 (`0x803C2B`), then by the live LH3DVRAMTex count `[0xF05200]`: > 600 gives 800/170/85, > 500 700/170/85, > 400 600/170/85 |
| L3 `0xE9C530` | fn_007FEE60 (y_ref as L2) | 50, W 40, T 20 (fn_007FE7B0) | never changed |

- Create's two tests are `[0xC381EC] == 4` (`0x803C25`) and fn_00804E60, which in the running copy also returns
  `[0xC381EC] == 4` (`0x804E69`). Nothing else writes dist, W or T. `[0xF05200]` counts live LH3DVRAMTex textures (+1
  at `0x85DF00`, −1 at `0x85DB41`, reset at `0x85D825`); it only affects L2.
- Once per frame, in LH3DIsland::PreDraw (`0x7FF2E1..0x7FF30E`, before any cull), each line is rebuilt from the camera:

```
f   = [0xEA1DD4..0xEA1DDC]      // unit view direction, written by UpdateWorldToCamera 0x819773..0x819782 when its
                                // bool is set (UpdateCamera passes 1 at 0x819A41, CameraModeNew3 passes 0 at 0x459A66);
                                // f = normalize(focus - eye), with dx replaced by +-1e-4 (its sign) first when
                                // |dx| < 1e-4 and |dz| < 1e-4 (0x8196B9..0x8196F8)
P   = g_camera + dist * f       // per component
d   = -(P.z*f.z + P.y*f.y + P.x*f.x)
n   = (f.z, 0, -f.x)            // cross((0,1,0), f), fn_006A3E20
m   = -y_ref                    // y_ref = 0 (fn_007FF0E0); (110.55 < cam.y || NaN) ? 110.55 : cam.y (fn_007FEE60,
                                // 110.55 = 0.67f * 165f, 0x7FEE63..0x7FEE90)
sx = n.x^2, sy = 0, sz = n.z^2
if (sz > sy && sz > sx && sz > 0.0001)  p = (d - m*f.y, m*f.x, 0) * (1/n.z)     // strict compares
elif (sx > 0.0001)                      p = (0, -m*f.z, m*f.y - d) * (1/n.x)    // (the sy branch is unreachable)
else { valid = 0; return }              // n stays (f.z, 0, -f.x) unnormalised, p keeps last frame's value
// the scale by 1/n.z or 1/n.x is done once, by fn_007FF0C0
n *= 1/sqrt(sz + sy + sx); valid = 1
```

  The cull and the morph then use, for a point (x, z), `v = (x - p.x) * n.z - (z - p.z) * n.x` in exactly this order:
  the signed horizontal distance to the line, positive on the camera's side.

**The choice** (fn_00877210 after the frustum masks, `0x87751B..0x877C84`). The block's 4 ground corners (x, z),
(x, z+160), (x+160, z), (x+160, z+160), with x = `+0x90C` and z = `+0x910` (the 160 added before p is subtracted), are
each *near* (v > T), *far* (v < −T, and a NaN v) or *in the band*:

```
L1:  any band            -> +0x934 = 1, +0x930 = 0     // 0x877A6A
     any near and any far -> +0x934 = 1, +0x930 = 0     // 0x877A84
     all near             -> +0x934 = 0, +0x930 = 0     // 0x877A92
     all far              -> +0x934 = 2, +0x930 = 1     // 0x877AA0
if +0x934 == 2, L0:
     any band             -> +0x934 = 3, +0x930 = 1     // 0x877C5A
     all far              -> +0x934 = 4, +0x930 = 2     // 0x877C7A
     all near             -> +0x934 = 2, +0x930 = 1     // 0x877C6C
     near and far         -> +0x934 = 3, +0x930 = 1     // 0x877C5A
```

- `+0x934`: 0 full detail; 1 full detail morphing towards step 2 (straddles L1); 2 step 2; 3 step 2 morphing towards
  step 4 (straddles L0); 4 step 4, also the value a frustum-culled block is given. `+0x930` is the matching step shift.
- L2 and L3 set only `+0x938` and `+0x928`, and the fog band sets `+0x940`. With the detail and bump lines they change
  colours, UVs and passes only: fn_00874AA0's result does not depend on them (it is fixed at `0x87539B`).

**The seams** (fn_00878210, from PreDraw `0x7FF33C..0x7FF359`). It runs for every block after all of the frame's
culls, so it sees this frame's `+0x930` of every neighbour; culled blocks have `+0x930 = 0` and never cause a seam.

```
+0x93C = 0;  nb(i, j) = block g_index_block[i*32 + j] if 0 <= i, j <= 31 and that index is not 0
nb(bx-1, bz).+0x930 > +0x930: |= 1      // bx = +0x914, bz = +0x918, signed compares
nb(bx, bz-1).+0x930 > +0x930: |= 2
nb(bx, bz+1).+0x930 > +0x930: |= 4
nb(bx+1, bz).+0x930 > +0x930: |= 8
```

- The seam table (`0xE9A130`, 24 bytes per entry: has_fans, skipRow0, skipRowLast, skipCol0, skipColLast) is built
  once by fn_00803890 (`0x803983..0x803A15`, guarded by `[0xC37BF8]`). 1 → {1,1,0,0,0}, 2 → {1,0,0,1,0}, 3 →
  {1,1,0,1,0}, 4 → {1,0,0,0,1}, 5 → {1,1,0,0,1}, 8 → {1,0,1,0,0}, 10 → {1,0,1,1,0}, 12 → {1,0,1,0,1}. Every other
  value (0, 6, 7, 9, 11, 13, 14, 15) is all zero: no edge is skipped and no fan is added, so two opposite coarser
  sides are left as they are.
- The quad walk (fn_00876910 `0x876936..0x876A26`, fn_00875C60 `0x875C86..0x875D77`): at steps 0 and 1 the rows run
  from skipRow0 to `16/step - skipRowLast` and the columns likewise; at step 2 all 5×5 vertices, with no seam handling
  (`0x876964`). The first quad is `v00 = (skipRow0 ? n : 0) + (skipCol0 ? 1 : 0)`, and at a row end each index moves by
  2 if a column is skipped, otherwise 1. A quad with the cell's split bit 0x80 emits (v01, v11, v10) and (v01, v10,
  v00), otherwise (v00, v01, v11) and (v00, v11, v10).
- The fans (`0x876F26..0x877200`, outside `0x876562..0x8768FA`) run only when has_fans is set, at steps 0 and 1. Bit 1
  is tested before bit 8 and, within either, bit 4 before bit 2. Step 0 tables: 1+4 `0xC37584`, 1+2 `0xC37470`, 1
  `0xC37230`, 8+4 `0xC377AC`, 8+2 `0xC37698`, 8 `0xC372C0`, 4 `0xC373E0`, 2 `0xC37350` (pairs 46 triangles, single
  sides 24). Step 1, in the same order: `0xC37A64`, `0xC379E0`, `0xC378C0`, `0xC37B6C`, `0xC37AE8`, `0xC37908`,
  `0xC37998`, `0xC37950` (22 and 12). The step 1 tables are stored with 17-wide indices and converted once, in place,
  to 9-wide ones, `v = (9*(v/17) + v%17) / 2` (`0x803A1D..0x803BCC`). Each triangle is emitted in table order if
  `A > 0` (`0x877159..0x8771E8`), or clipped first on the outside path.

**The cell-flag pointer.** The walk's cell pointer does not follow the level of detail or the seams: it starts at
record 0 and moves one record per quad and one more per row (`0x876C8E`/`0x876CB1`, `0x876EEC`/`0x876F0F`,
`0x876141`/`0x87616F`). At step 0 with no seam, quad (r, c) reads record (r, c). At step 1 it reads record `9r + c` of
the 17-wide array, at step 2 record `5r + c`; with a skipped first row the flags are one row behind, and with a skipped
column they drift by one record per row. The flags read this way are the split bit 0x80 and, at `+0x934 == 0` only,
the open-sea bit 0x200.

**The open-sea bit.** Cell word `+6` is `properties | flags << 8`, so 0x200 is bit 1 of the LND flags byte, the open-sea
cell of [rendering.md](rendering.md#coast). A quad is skipped for it only when `+0x934 == 0`, and only for the record the
pointer reaches.

**The morph** (fn_00877D20, fastcall: ecx = the copy at `0xE9B6E0`, edx = `+0x934 == 1`). Stage A copies the whole
block first (`rep movsd` of 0x276 dwords, `0x7FF64B`). Only the copy's altitude bytes change: record i at
`block + 8*i`, altitude at +4, i = row*17 + col, the row along x (`+0x90C`) and the column along z (`+0x910`), 10 units
per cell.

```
edx ? (L = L1, s = 2, h = 1) : (L = L0, s = 4, h = 2)          // 0x877D2B..0x877DD0
K = 256 / L.W;  half = L.W * 0.5
blend(r, c, a, b):
    v = (X(r) - L.p.x) * L.n.z - (Z(c) - L.p.z) * L.n.x       // X(r) = +0x90C + 10r, Z(c) = +0x910 + 10c
    if (v >= half) return                                      // on the near side of the band; ordered, NaN goes on
    mid = (alt[a] + alt[b]) >> 1
    if (-half >= v) alt[r,c] = mid                             // beyond the band; ordered, NaN goes on
    else { t = fistp((half - v) * K); alt[r,c] = ((256 - t)*alt[r,c] + t*mid) >> 8 }   // unsigned, byte store
rows 0, s, .., 16;  cols h, h+s, ..:   blend(r, c, (r, c-h), (r, c+h))          // 0x877DD3..0x877F42
rows h, h+s, ..;    cols h, h+s, ..:   blend(r, c, (r-h, c-h), (r+h, c+h))      // 0x877F48..0x8780AF
rows h, h+s, ..;    cols 0, s, .., 16: blend(r, c, (r-h, c), (r+h, c))          // 0x8780B5..0x878203
```

- X and Z are float accumulators: they start from `+0x90C - p.x` (or with 10h added first) and add `10*s` per step, in
  that order.
- a and b always lie on the coarser grid, which no pass writes, so the passes are independent. The centre vertex always
  uses the (−,−)/(+,+) diagonal, whatever the split bit. A NaN v takes the partial branch with fistp's
  integer-indefinite result.

#### The detail index

- The running detail index is `[0xC381EC]` (static 3). Its only writer is fn_00823AD0 at `0x823B32`, called once from
  start_system (`0x643026`), so it is set at start-up and never changes while the game runs.
- start_system (`0x642FCB..0x643004`) reads the DWORD `detailidx` from `HKEY_CURRENT_USER\Software\Lionhead Studios
  Ltd\Black & White\BWSetup` (key `0x9CAFF8`, value `0xBE8D68`, LHLogR RegistryRetrieveULong, import `0x8A9344`; the
  root `[0x1001DEE0]` is HKCU). Missing gives **4**; any value of 5 or more (unsigned, `jae` at `0x642FF2`) also gives 4.
  No ini, profile or command-line option is read.
- fn_00823AD0 then sets, from tables indexed by d (0..4): the fog flag `[0xC37204]` (0x9A3854: 0,0,0,1,1),
  UseSmallBump `[0xC37210]` (1 in all), LandRef `[0xE9CD8C]` (0,0,0,1,1) and `[0xE9CD90]` (ends 1,0,0,0,0; it seeds
  `+0x92C`, texturing only). Index 5, the custom set (fn_008237B0), is never reached from start-up.
- At d 0..3 the lines keep their static values (L0/L1 600/300, ±25); at d 4 they are 2500/1600, ±275.
- The Options box (ReadRegistrySettings `0x51482A..0x514875`, saved by `0x5148B0`) writes `detailidx` back to the
  registry but does not call fn_00823AD0: a change applies at the next start. `maxdetailidx` (default 5,
  `[0xC381E8]`) only limits the dialog.
- The test machine's registry has no `detailidx` (nor `maxdetailidx`), so the original runs there with **d = 4**. This
  holds until someone saves the Options box with another level.
- openblack: `EngineConfig::detailLevel` (src/EngineConfig.h, default 4), from `--detail-level` (src/main.cpp, default
  4, 0..6). Its 5 (custom) and 6 (top) have no counterpart in this executable, which never runs above 4.

#### What the level of detail changes in the list

For one rebuild, the list's membership and order do not depend on the level of detail (`+0x930`, `+0x934`, `+0x93C`,
`+0x940`):

- The block list `[0xFA92D8]` is linked in PreDraw from bit 0 of `+0x920`, which only the cull's frustum test writes,
  every frame, before any level-of-detail code (`0x8774E8`, `0x877CEB`). fn_00878210 writes only `+0x93C`.
- The sort key and the vanish break use `+0x9BC`, made from the box centre only (`0x877C8A..0x877CCD`).
- The land pass and its callees never write `+0x9B8`, `+0x9BC` or `[0xFA92D8]`. A block whose bit 0 the land pass
  clears stays linked.
- Stage B's walk reads per block only `+0x9BC`, `+0x914`, `+0x918` and `+0x9B8`, not `+0x920`. So it walks every
  frustum-accepted block, including those whose land triangles were all clipped or back-facing.

The level of detail reaches the list only through the rebuild trigger `[0xC37208]`: fn_00874AA0's result depends on
`+0x930`, `+0x934` and `+0x93C`, so a block can flip visible or invisible because of its level, and decide which frame
rebuilds. `+0x940` does not reach that result.

#### A land load

Every land load frees the land's blocks and makes them again from the land file, so each block's state starts afresh:

- **The paths.** LOAD_MAP (GScript::LoadMap `0x6FB320`) and the first land (GGame::Init, then
  ResetAndStartPlaygroundGame `0x5559A7`) both reach StartPlaygroundGame `0x552F40`. It runs ClearMap,
  LH3DLandscape::Release and LH3DIsland::Release (`0x552F59`), then LoadMapFeatures `0x7180B0`, whose feature
  dispatcher `0x715180` has the only call to GLandscape::Open (`0x716FE7`; its case number was not resolved). A saved
  game's load (LoadAllGame) runs ClearMap and then GLandscape::Open (`0x558AC9`). The vortex reaches neither: the next
  land comes through the script's LOAD_MAP.
- **GLandscape::Open `0x5E52E0`** has no "same land" test: it always runs both Releases, then LH3DIsland::Create
  (`0x5E533B`), which hands a `.lnd` file to fn_008015D0 (`0x803E98`).
- **LH3DIsland::Release `0x804790`** frees every block (its texture and material, then LH3DMem::Free at `0x878767`),
  zeroes `g_ptr_blocks` and `g_index_block`, sets the block count `[0xE9CD70]` to 0, and sets `[0xC37208] = 1`
  (`0x804816`).
- **The `.lnd` load.** Each block 1..count−1 is allocated again (0x9D8 bytes, zeroed) and copied whole from the file
  (`0x80169B`). Then `+0x91C..+0x943` are zeroed (from `0x8016A4`): the partly-outside flag `+0x91C`, all of `+0x920`
  (bit 1, the last land-clip result, included), `+0x930`, `+0x934`, `+0x93C` and `+0x940`. `+0x9D4` is zeroed too, and
  fn_007FEA60 sets `+0x924` to the block's highest altitude (`0x7FEA93`). The other path (an LH_FILE, not a `.lnd`)
  zeroes the whole block.
- **Not cleared:** `+0x9B8` (next) and `+0x9BC` (the distance) keep the file's bytes. Nothing reads them first: PreDraw
  culls every block (`0x7FF32A`), the cull writes `+0x9BC` for every block it keeps, then PreDraw empties `[0xFA92D8]`
  (`0x7FF468`) and links the visible blocks again, writing `+0x9B8`. Only listed blocks are read after that.
- **What a land load keeps:** the last rebuild's turn `[0xD2019C]` (its only writer is `0x5E5D90`), and `[0xFA92D8]`,
  which points at the freed blocks until the next PreDraw.
- **`[0xC37208]`.** Release's 1 is never seen: fn_007FF610 sets the flag to 0 at its start (`0x7FF62B`), and
  GLandscape::Draw always calls it (`0x5E4E96`) before the rebuild check (`0x5E4EDD`); nothing in Draw jumps over that
  call. The check fn_005E5CD0 only reads the flag (`0x5E5D5E`). Since bit 1 is 0 after a load, the first drawn frame
  sets the flag for every listed block that keeps a triangle; that rebuild is forced anyway by ClearMap's RebuildCount
  of 1.

### Stage B: rebuilding the list (`0x5E5D32..0x5E5ED2`)

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

### Stage C: the draw passes (`0x5E5EDA..0x5E6135`)

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
- Who answers 1: no LH3DObject vtable overrides IsHuman (`fn_007F9980`, `(Flags1[+4] >> 19) & 3`, in all nine
  vtables); its only writer is SetHuman (`fn_007F9990`, vt +0xB4), called on a game object's +0x40 only from
  Villager::CallVirtualFunctionsForCreation (`0x74FD3C`), so **IsHuman is 1 for the villagers and the special
  villagers** (not PieceVillager or PuzzleVillager, which take Animal's). Only the COMPLEX vtable (`0x9A3068`) overrides
  IsComplex (`fn_0080B9F0`, returns 1); the default `Object::Get3DType` (`0x6364F0`) never returns COMPLEX, and the
  game objects with a COMPLEX +0x40 are **the Creature** (`0x474B24..0x474B42`, from Morphable::MorphInit
  `0x617315`) and **the HelpSpirits** (HelpSpirit::Create `0x5C4B30`, the object fn_005C0F30 builds at `0x5C0FD5`).
  Every other object's Active is its on-screen flag. openblack: `components::Villager` answers IsHuman and
  `components::Creature` IsComplex (src/ECS/DrawList/RegistryObjects.cpp); the advisor spirits are drawn by the help
  system and are not registry entities, so they never reach the list.
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

### Stage D: what sets g_b_last_on_screen inside Draw

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

### The script highlight in the list

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
  [particles.md](particles.md#the-glints-on-a-target-er_glintsontarget).

### Every writer of RebuildCount `[0xBF358C]`

RebuildCount is static 1. It has 20 references, 2 of them in fn_005E5CD0.

| Writer | Address | Value | Condition |
|---|---|---|---|
| CHand::ThrowObject | `0x46DE25` | 2 | The hand holds an object (`hand+0x4904 != 0`) |
| GGame::ClearMap | `0x552E2A`, `0x552EA9` | 1 | Always |
| HandStateTug::Update | `0x5B84AE` | 1 | The held object passes IsTree, the force and pull checks pass, and it also casts to Living. **(inferred)** unreachable: only Tree and MagicTree override IsTree, but the RTTI was not dumped |
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

## Pending

- Stage 4d: `fn_005DFCE0` (footprint UV2) and stage 4h (`fn_005FF310` → `fn_005FFBB0`, LandscapeVortex) are
  **(inferred)**.

### The object draw list: still undetermined in the original

- The x87 precision under D3D7: the device's FPU flags were not traced. In single precision the x87 path matches the
  SSE one; in double precision a back-face area A near 0 can differ, and the SSE path's approximate reciprocal can move
  an on-screen edge case. The level of detail depends on it too: the morph's fistp uses the current rounding mode, and
  every v-against-T compare depends on the precision.
- The level of detail on the SSE path (fn_007A1800, fn_007ACCE0, fn_007ACD00) was not read; everything in
  [The block's level of detail](#the-blocks-level-of-detail) is the x87 path. fn_007A98B0 and fn_007A3A50 were checked
  for structure only, not instruction by instruction.
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

### The object draw list: what openblack needs

openblack runs the list on the logic side: the pure stages in `src/ECS/DrawList/`, the state in the service
`DrawListSystem` (`Locator::drawListSystem`), run by `UpdateObjectDrawList` in src/Game.cpp once per drawn frame,
after the hand demo has set the camera. It is not run inside the temple, under the falling-spell film or behind a full
screen film, and it runs while the game is paused. The script highlights have a Draw of their own; every other
object's Draw is its mesh's on-screen test (`draw_list::DrawOnScreen`, src/ECS/DrawList/ObjectDraw.cpp: the box's
centre through the object's model, half the box's diagonal times the largest scale, and true with no mesh), which
sets its Active for the still passes. The objects each pass drew are kept (`DrawListSystem::DrawnThisFrame`). Each
frame `draw_gate::FillFrame` (src/ECS/DrawList/DrawGate.cpp) turns them into the objects the list leaves out: in the
map, not DontDraw, and not drawn by the pass (`graphics::ObjectListFrame`, carried to the renderer in the frame's
snapshot). `Renderer::PreDraw` makes a mask of their instance rows, which the draw does not apply yet.

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
  frame. `TakeLandClipResult` writes the result into bits 0 and 1, and a change of bit 1 raises the land flag that
  `RebuildDue` reads. A block without its cells keeps its bits, and so does one the land clip gives no answer for. In
  the game the second case cannot happen: the cull gives only steps 0 to 2, the seam pass only bits 0 to 15, and the
  frame always has its clamp.
- **A land load.** At the map clear the list is emptied and the blocks start again from their state at the load
  (`InitialBlockState`, [A land load](#a-land-load)): no visibility bit, not partly outside, every level-of-detail field
  0, the distance the file's. The last rebuild's turn is kept, and the map clear's RebuildCount of 1 gives the first
  rebuild. At that first frame the lines are placed as LH3DIsland::Create places them (`LodLinesAtLandCreation`). The
  detail index is `DetailIndexOf(EngineConfig::detailLevel)`: openblack's levels 5 and 6 are taken as 4, because the
  original's start-up turns a stored index of 5 or more into 4.
- **Stage B.** The block arrays live beside the map cells (`MapCellsSystem`, `draw_list::GameBlocks`), written at the
  map cells' insert and remove points. The listed mark is the component `DrawListed` on the entity: it goes when the
  entity is destroyed, so an index made again (a new entt version) is not taken as listed. DontDraw is
  `components::DontDraw`, which SET_HIGH_GRAPHICS_DETAIL writes. An object is available while its entity exists. The
  RebuildCount writers publish `events::DrawListRebuildRequested`.
- **Stage C.** The full and the still passes; a destroyed entity's entry is nulled when a pass meets it, which stands for
  EndTurn's nulling.
- **Stage D and the highlight's Draw** (`script_highlight::Draw`). The three early exits return 1 and step nothing.
  Otherwise the mesh's on-screen test (`draw_list::OnScreen`) with `field_of_view::ObjectOnScreen`'s centre and radius,
  then the glints' step ([particles.md](particles.md#the-glints-on-a-target-er_glintsontarget)). The flag is the test's
  result, or 1 when the glow branch (an active scroll that is not a did-you-know) or the render particle branch (row
  3) runs. The gold scroll's render particle itself is not ported, but the original makes it at creation for every
  highlight ([intro.md](intro.md)), so ours takes that branch for every gold scroll.
- **Still to do in openblack:**
  - the highlight's pose (its spin, the did-you-know's facing, the distance scale, the Transform) is still set every
    frame by `script_highlight::UpdateFrame`; the original sets it inside Draw only. Gating it waits for the renderer to
    draw only what the list draws: on its own it would show a stale pose where the original draws nothing;
  - the seed graphic's holder effect (with its glints) is stepped every frame by the orb and the icons, not from the
    list;
  - the vortex's over-land effects are still stepped every frame
    ([vortex.md](vortex.md#the-z-sorted-part-landscapevortexdraw-0x5ffdc0-from-the-object-draw-list));
  - the renderer still draws the objects the list does not list;
  - the block's highest altitude is the land file's `highestAltitude`; the original recomputes `+0x924` at the load
    (fn_007FEA60), and the two were not compared;
  - the writers that are not hooked: the tug, Reward::Create and Process, the tutorial key, GScript::Load's release and
    the task-stop camera release.
- **The turn at a land load (a separate fix to check).** The original sets the turn to 0 in GGame::ClearMap
  (GData::Reset `0x510750` through ResetState `0x5557A9`, from `0x552E62`), before the map script
  (GSetup::LoadMapFeatures) and Town::AsssignTownFeature run. openblack's `Game::LoadMap` calls `game_clock::SetTurn(0)`
  (src/Game.cpp:2869) after `script.Load` (:2820) and `AssignTownFeatures` (:2832), so on a second or later land the
  previous land's turn is still in place while the map script runs. The value at the end is the same; whether anything
  reads the turn in between was not checked. The last-rebuild turn `[0xD2019C]` is never reset (its one write is
  `0x5E5D90`): ClearMap's RebuildCount = 1 forces the first rebuild instead.
