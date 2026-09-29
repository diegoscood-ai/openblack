# Original B&W (runblack.exe W120, D3D7, LH3D): one in-game frame

"(inf)" = inference. Scratch dumps: render\frame_*.txt; frame_A_modes.py/txt; frame_B_*.txt; frame_C_*.

## 0. Frame driver
GGame::Loop 0x54CF20 (decomp src/Black/Game.cpp) per iteration:
ProcessGraphicsEngine 0x54D850 → [mouse, camera->Update (sets FOV + dynamic near), GInterface::PreDrawProcess 0x5CE9E0 (hand collide, disciple icon), **Process3dEngine 0x54DA80**, debug BMan/camera editor, GInterface::PostDrawProcess (leash/collide update only, no drawing), HelpSystem::PostDrawProcess (flag)] → ScriptedScreenShot → **GGame::FlipScreen 0x54D800 → LHScreen::Flip 0x7DE090** (timing text, software LHMouse::Draw, LHFlip 0x7DE580, then the frame *clear* fn_0082EE70, see §3).

Process3dEngine: LH3DRender::StartFrame 0x82F0E0 → video (Bink) → switch field_0x205a28 (0 = world, 1 = citadel/temple, 2 = falling-spell video) → fade / help / influence → **LH3DRender::FinishFrame 0x82F460** (Z-sort flush + callbacks + EndScene) → debug 2D (CreatureMentalEditor, leash info, DisplayHowImpressed, computer players, countdown text) → LH3DAtmos::Render2D (debug weather map via Lock, inf).

StartFrame: fninit/FPU control, delta time (g_delta_time 0xC38134, smoothed fps 0xEC7FC0), **BeginScene** (vt+0x14), g_frame++, g_started_frame=1, **zsorter reset fn_0083F3B0**, fn_00813770, fn_0085BF00, SetLight/SetProjMatrix only if HW TnL (never: [0xC386E4]=1 forced by start_system), fn_00821270.

## 1. Draw stages in order (case 0 = world)
| # | Stage | Function | What / states | Sort / cull |
|---|---|---|---|---|
| 1 | landscape.PreDraw | GLandscape::PreDraw 0x5E3F60 → LH3DIsland::PreDraw 0x7FF2D0 → fn_00877210 | builds visible block list (32×32 blocks of 160), per-block fog class +0x940 | frustum+near cull per block bbox; list 0xFAA?/0xFA92D8 sorted by distance +0x9BC **front to back** |
| 2 | shadow textures | TemporaryShadow::UpdateAll 0x825190 (and fn_00874850 per SuperVillager) | CPU-rasterised silhouettes into per-object shadow textures (fn_008801D0, inf), list 0xFAA7E0 | — |
| 3 | prep | LH3DCreature::PrepareForDrawing 0x4ED320 per creature, CHand::PrepareForDrawing 0x46C550, PSysLightMaps::AddDrawing 0x6CA6E0, LH3DLandscape::TextureUpdateThread 0x871F00 (block textures: footprints/decals fn_008721A0/fn_00872FA0), LH3DAtmos::Update3D | no draws | — |
| 4 | GLandscape::Draw 0x5E42E0 | (details below 4a–4q) | | |
| 4a | setup | 3D cursor (Get3DPointFromScreen), fn_00802550, Windmill::PreDraw, Tree::PreDraw (wind sway), fn_008296D0 (8 s fade timer, mode 0xF), fn_005E5830 (hand pos; night hand light fn_00823460/fn_0086D360; clouds fn_005E25C0 if Clouds key) | | |
| 4b | **sky** | GLandAlignement::DrawSky 0x5E2160 (skipped in wireframe) → fn_0086A330 → light table fn_00869850 + fog params, fn_0086B7F0 (sky_{good,ntrl,evil}_{day,dusk,night}.555 → 3 textures, mode 2) → fn_0086B010 | **moon** moon.l3d + additive glow quad 500 (AdditiveMaterial mode 13), also a Y-mirrored copy (inf reflection); **sky dome** sky.l3d with ZFUNC ALWAYS, 2 passes (base alignment texture α255, then 2nd alignment with global alpha (alignment−1)·255), darkened by storm, lerp to white on lightning; **sun** sun.l3d fn_0086C140 colour 0x957C63, visible 6–18 h, ÷(1+8·cloud). **No stars.** | sky covers whole screen (colour buffer not cleared) |
| 4c | **reflected land** (LandRef key) | fn_007FF4F0, ZWRITEENABLE off around it (0x5E48B3–0x5E4900) | heights ×−1, light table ×0.5, small bump forced off | mirrored block list |
| 4d | underwater parts | PetitNavire::PreDraw 0x5DFF20 (boats); hand DrawUnderWater vt+0x118 with specular 0x65A0A0A0 via **alt mode table 0xC387C8**; creature hand object; fn_00646FE0 (physics objects), fn_00775120 (?), fn_00824B90 (FishFarm fish), fn_005DFCE0 (footprint UV2, inf) | drawn before sea (inf: parts below sea level) | |
| 4e | SuperVillagers | list 0xEB9A08: vt+0x610 Draw + fn_00874850 shadow; swim villagers spawn water rings | | |
| 4f | night hand glow | fn_005E3F70: additive ground quad ±60, ZFUNC ALWAYS | | |
| 4g | **sea** | fn_00879930 (skip if [0xECA664] or wireframe): sky.raw/skya.raw mode 5, 2-px rows, ZFUNC ALWAYS, no Z write, colour = light table[255], alpha 255→80 at 7000–14000, P = 2000−1800·WaterTiling | | |
| 4h | vortex | fn_005FF310 → fn_005FFBB0 (LandscapeVortex, inf) | | |
| 4i | **landscape** | fn_007FF610: per visible block front→back, transition blocks fn_00877D20, block fn_00874AA0 / SSE fn_007A1800 (mode 14 blocks, light table per vertex, software haze, small bump 2nd pass mode 14), then **projected dynamic shadows** per block fn_00878350 (shadow material mode 6) | front→back |
| 4j | debug | fn_0081F820 script field-of-view triangles | | |
| 4k | citadel heart | fn_00467360 → CitadelHeart::DrawNow 0x4670D0 | | |
| 4l | water rings | fn_005E5100 over 1024×0x38 at 0xEAB7C8 (GWater sprite circle, LH3DSprite::Draw direct) | | |
| 4m | **models** | fn_005E5CD0 object draw list (≤3000, GLandscape::DrawObjects 0xD1D28C) from visible blocks (dist < VanishObjectDist 100000) + global list; rebuilt on DrawListRebuildCount/10 turns/land change; camera-still frames only redraw last-on-screen objects. Object Draw (e.g. MobileObject::Draw 0x518150): lighting fn_00801C90 (other agent), fog fn_007FEB30, Game3DObject::AddForDrawing 0x63B5D0 → LH3DObject::AddDrawing fn_00815A70: frustum CheckRegionOnScreen 0x868C80, **LOD** by distance (23.3f/66.7f/86.7f → LOD 1/2/4, fade 86.7f–173.3f, cull beyond if IsDisappear; far humans → sprite impostor), IsGlowing → glow sprite, **NeedSorting (alpha mesh, flag 0x200) → Z-sorter, else Draw immediately** | opaque: block order, unsorted; alpha: Z-sorter |
| 4n | extra in 5E5CD0 | SuperVillager list 0xEB9A10 (OverrideMaterial with alpha-ref override 0xA), game list vt+0x610, bookmarks, Reward sprites, PetitNavire::PostDraw, hand_intro, sprite emitters fn_008274A0/fn_00823570/fn_00827B90/fn_00828E50 | | |
| 4o | tail | storage pit / LandFeature::DrawWorm | | |
| 5 | creature fight sparkles | LH3DCreature::DrawFightSparkles 0x48DD70 | | |
| 6 | leashes | GInterface::DrawAllLeashes 0x5D9310 (fn_008491B0; leash material mode 15) | | |
| 7 | physics objects | PhysicsObject::DrawAll 0x646DE0 (AddForDrawing, DrawOutOfMap) | as models | |
| 8 | hand | CHand::UpdateHeldObject; **CHand::AddDrawing 0x46D100 → Z-sorter** (lighting ×1.5); other players' held objects DrawInHand | Z-sorted | |
| 9 | interface | GInterface::Draw 0x518640 → fn_005FAF80 (magic hand), status vt+0x500 (gesture trail etc., mode 13 gestures) | | |
| 10 | liquid particles | DrawLiquidParticles 0x845C50 → Z-sorter | Z-sorted | |
| 11 | debug overlay | GGame::Draw 0x5533B0 (only if g_game->field_0x14 & 0x4000: alignment/belief bars) | | |
| 12 | misc | CreatureLessonChooser::UpdateDraw, EditorIconBase::DrawMouseOver | | |
| 13 | particles | PSysGlobal::DrawLoop 0x68F5E0 (PSys managers → PSysManager::AddDrawing 0x6797D0 → Z-sorter), FireFly::DrawAll, Spell::DrawSpells 0x7203F0, GParticleContainer::DrawParticleContainers, GPlayer::DrawPlayers (player sparkles blobs.raw mode 13), TownCentre::DrawAll (DrawPSys) | Z-sorted | |
| 14 | value spinners | ValueSpinner::Update/AddDrawing (not during help cutscene) | Z-sorted (inf) | |
| 15 | debug | LH3DStorm::DebugDrawAll, LH3DAtmos::DrawWindField | | |
| 16 | **weather** | LH3DAtmos::Render3D 0x836250: per storm (list 0xFA92D8… storm dist < 560) per 80×80 tile, intensity from GetWeather at 4 corners, if >5: rain fn_008341B0 → Z-sorter callback 0x833F80 (streaks, AtmosMaterial mode 6 atmos.raw, splashes g_water_drop_cb, count RainSplash key); snow fn_00834290 → callback 0x834120 (snow.raw mode 9). Lightning: LightSheet::DoTheDrawing 0x83E8C0 (mode 13) via GLightSheet::Draw → Z-sorter. Rain colour = (light-table base/2 & 0x7f7f7f)+0x7f7f7f. | Z-sorted by tile distance² | |
| 17 | camera force field | CameraModeNew3 ForceField (only when flagged) | | |
| 18 | villager names | VillagerName::AddDrawing → Z-sorter | | |
| 19 | fade | GScript::ProcessFade 0x6EB9D0 or Temple::UpdateFade 0x794280 → sets screen fade colour [0xFA51D8] (drawn in FinishFrame) | | |
| 20 | help 3D | HelpSystem::Draw3D 0x5C59A0 (help dude / arrows) | | |
| 21 | ClearLight 0x5E57B0 | removes the hand light (fn_00822F90/fn_00823780/fn_0086D460) | | |
| 22 | power spins | PowerSpinRunner → Z-sorter (PowerSpin::Draw) | | |
| 23 | influence ring | InfluenceCircle::Draw 0x826C90 (in world only if cam height>100: alpha 0→120 at 100–200 height; mode 6, cull none, wrap, UV scroll 0.0001/−0.0002 per ms) Draw3DWorldTriangle **immediate** | | |
| 24 | **FinishFrame 0x82F460** | (a) **Z-sorter flush fn_0082F280** back→front (key = dist² to camera, max 2048 entries); (b) "before" callbacks (prio ascending, 0xEC8130): FallingSpell (0x526480), HelpDude 0x5C2E30 (100), CameraModeNew3 0x4562E0 (1000); (c) fn_0086BB60 sky post (sun/lens-glare, inf); (d) **Z reset quad**: full-screen FVF 0x1C4 quad z=1, rhw=0, colour 0, material [0xEDD494] mode 1, ZFUNC ALWAYS → Z=1 everywhere; (e) letterbox bars if [0xEB9950] (queued 2D rects fn_0081E590, height fn_0081E8B0); (f) "after" callbacks: HelpDude 0x5C2E10 (100), **2D rect queue flush 0x81E7D0→fn_0081E3C0 (10000; mode 1, ZFUNC ALWAYS, no Z write)**, HelpText 0x5CD020 (20000: help text boxes), LHVideoPlayer::thedraw 0x844E30 (0x8000), start_system 0x6424E0 (0xA0000); (g) fn_00836200 debug weather; (h) **screen fade fn_0086FEE0**: full-screen quad colour [0xFA51D8] mode 1 ZFUNC ALWAYS if alpha≠0, then letterbox rects, fade reset; (i) EndScene (vt+0x18) | | |

Case 1 (citadel/temple): Update3D, LH3DSky::g_b_we_are_inside_citadel=1, TemporaryShadow::UpdateAll, DrawSky (no sun), Temple::Draw 0x794370 (own lights via fn_0081E1F0 SetLight, rooms; Citadel* detail keys), Temple::Update, liquid particles. Case 2: FallingSpell video + liquid particles. Video: LHVideoPlayer::DrawToScreen (letterboxed 16:9, alpha fade; FallingSpell 80).

HUD/2D: B&W has no classic HUD; 2D = help text boxes (callback), tooltips/text via GatheringText (fonts mode 6), queued rects, fade, letterbox, software cursor in Flip (inf: cursor usually the 3D hand).

## 2. Render modes (table 0xC38728, 19 entries {fn, flag}; L3D material type == mode index)
Common: colour = TEXTURE×DIFFUSE; ALPHAFUNC GREATEREQUAL set once (0x82CBA6); ALPHAREF = mat+4 (or override [0xECA65C] if [0xECA658]); material +5 bit0 cull NONE else CCW, bit2/g_b_need_tilling → WRAP else CLAMP; mode cache [0xC38718] (reset 0x14 each frame). No mode touches fog/specular/lighting/stage 1.
| # | fn | states | L3D type / users |
|---|---|---|---|
| 0 | 82D470 | untextured, opaque, Z write | Smooth |
| 1 | 82D5C0 | untextured, SA/ISA, Z write, stage 0 untouched | SmoothAlpha; 2D rects, fade, Z-reset quad |
| 2 | 82D820 | textured opaque, α=tex | Textured; sky domes |
| 3 | 82D920 | SA/ISA, α=tex×diff, Z write | TexturedAlpha |
| 4 | 82DC20 | SA/ISA, α=tex, Z write | AlphaTextured (buildings, hand) |
| 5 | 82DD90 | SA/ISA, α=tex×diff, Z write | AlphaTexturedAlpha; sea |
| 6 | 82DF10 | as 5, no Z write | …AlphaNz; fonts, influence, human_shadow, dynamic shadows, atmos rain, video |
| 7 | 82D6F0 | untextured SA/ISA, no Z write | SmoothAlphaNz |
| 8 | 82DAA0 | as 6 | TexturedAlphaNz |
| 9 | 82E080 | SA/ISA **+ alpha test**, α=tex, Z write | TexturedChroma (trees 0x96), snow |
| 10 | 82E830 | additive SA/ONE + alpha test, α=tex×diff, Z write | …AdditiveChroma |
| 11 | 82E9C0 | as 10, no Z write | …AdditiveChromaNz |
| 12 | 82EB50 | additive SA/ONE, Z write | …Additive |
| 13 | 82ECD0 | additive SA/ONE, no Z write | …AdditiveNz: gestures, lightning, moon glow, sparkles, fire, smoke |
| 14 | 82DD90 | = 5 (flag 0) | terrain blocks, small bump |
| 15 | 82E470 | SA/ISA + alpha test, α=tex×diff, Z write | TexturedChromaAlpha; leash |
| 16 | 82E6A0 | as 15, no Z write | text cache |
| 17 | 82D820 | = 2 | — |
| 18 | 82E2A0 | Z only (ZERO/ONE) + alpha test | ChromaJustZ; vortex, fizz |
Alt table 0xC387C8 (object fade / DrawWithGlobalAlpha, underwater hand): 0,1→D5C0; 2,3,17→D920; 4,5→DD90; 9→E470 (ref scaled by object alpha); rest same. Flag word: no reader found.

## 3. Global state
- No HW T&L; CPU transform to XYZRHW. Horizontal FOV [0xEA1DD0] default 70° (1.22173), aspect w/h [0xE839EC]; sz = 1−near/z, rhw = near/z, **no far plane**. Near [0xE839E0] dynamic from camera height above ground: 0.3 + 0.16·h clamped [0.3, 3.5] (0.1 path cams, 0.2 citadel with fov 90°).
- Fog: no D3D fog. Software haze (Fog key [0xC37204]): start/end from sky type (noon/midnight 400→900, dusk 100→800; storm →15/350), colour = light-table base/3, per-vertex darken diffuse toward "dark" and add fog RGB to specular (inf); landscape per block +0x940, objects fn_007FEB30.
- Clear: in Flip after present, only every ~2000 ms (2 frames) in play, black 0xFF000000, z=1; Z reset each frame by FinishFrame quad. Colour buffer relies on sky.
- Gamma: none. LightBoost (custom only) changes light-table divisor.
- Filtering bilinear, no mips, no AA, dither on (rendering.md).
- Detail: fn_00823AD0 from start_system 0x643026, level = registry detailidx (<5) else **4**; 5 = custom (fn_008237B0 reads each key). Table 0x9A3704 (L0..L6): LevelOfDetail (dead), UseSmallBump 1 all, Clouds/CloudShadows 0001111, WaterTiling 0/.2/.4/.6/**.8**/.5/1 (level 4 → sea period **560**), LandRef 0001111, CitadelReflections 0000111, CitadelLightmaps 0111111, CitadelGlows/People 0011111, CitadelVolumeLight 0001111, RainSplash 0,0,3,5,8,8,8, LightBoost 0, Fog 0001111; startup-only: Weather 0001111, Light 0011111 (object dynamic light flag 0x20), ShadowsOnObjects 0001111 (flag 0x40), UseHighTexture 0000111 (256 vs 128 px land textures), UseMultiLayerOnLandscape inverted (1 only at L0), FixeLand. HardwareTnL/MaxObjectDistance read but ignored. Landscape detail distances 0xE9C508 depend on VRAM texture count. VanishObjectDist 100000 (script).

## 4. Parity checklist (openblack Renderer::DrawScene/DrawPass: footprint, reflection(sky, land), main(sky, water, island, models, blended, sprites, debug))
- original: colour buffer not cleared, sky dome covers screen with ZFUNC ALWAYS → openblack: clears to 0x274659 — approx (harmless).
- original: horizontal FOV 70°, infinite far, near 0.3–3.5 by camera height → openblack: check Camera (approx/unknown).
- original: sky 3 alignment textures × day/dusk/night blended by 2 passes, storm darkening, lightning white → openblack: sky shader with type/alignment — approx (no storm/lightning).
- original: sun (sun.l3d, 6–18 h) and moon (moon.l3d + additive glow, mirrored copy) → openblack: missing.
- original: no stars → openblack: n/a.
- original: clouds + cloud shadows (detail ≥3) → openblack: missing.
- original: reflected land only (no models), Z write off, half light, no small bump → openblack: has.
- original: underwater parts of hand/boats/physics objects drawn before the sea (alt table) → openblack: missing (inf).
- original: sea mode 5, period 560 at default detail (200 only at L6), wind scroll, row wobble, alpha 255→80 → openblack: approx (P=200, no wind).
- original: landscape blocks front→back, mode 14, light table, software haze (Fog), small bump → openblack: has, **haze missing**.
- original: dynamic projected shadows (CPU silhouettes, mode 6) on terrain and objects (ShadowsOnObjects) → openblack: missing.
- original: footprints/decals baked into block textures → openblack: has (Footprint pass) approx.
- original: models: opaque immediate unsorted; alpha meshes Z-sorted back→front; LOD 1/2/4 by distance with fade 86.7f–173.3f and human sprite impostors → openblack: opaque + MainBlended (no back-to-front sort), LOD TODO — approx.
- original: TexturedChroma = alpha test ≥0x96 **plus** SA/ISA blending → openblack: alpha test only — approx.
- original: model lighting fn_00801C90 + per-vertex (see objlight_ notes) → openblack: see other agent.
- original: software haze on models (fn_007FEB30) → openblack: missing.
- original: water rings for swimming villagers, fish farm fish, boats → openblack: missing/unknown.
- original: hand Z-sorted with lighting ×1.5, AlphaTextured wrist fade → openblack: has (approx).
- original: particles (PSys), spells, player sparkles, town centre PSys, fireflies, liquid particles — all Z-sorted → openblack: missing.
- original: rain/snow per storm tile (atmos.raw mode 6 / snow.raw mode 9), splashes, lightning sheets (mode 13) → openblack: missing.
- original: leashes (mode 15), gestures (mode 13), influence ring (mode 6, >100 height) → openblack: missing.
- original: villager names, value spinners, help 3D, help text boxes (finish-frame callback) → openblack: missing (debug GUI only).
- original: sprites/billboards through LH3DSprite Z-sorted → openblack: sprites pass (unsorted) — approx.
- original: screen fade colour quad + letterbox bars in FinishFrame → openblack: missing.
- original: Bink video overlay/letterbox → openblack: missing.
- original: citadel/temple interior path (own lights, Citadel* keys) → openblack: TempleInterior — approx.
- original: no gamma, no post effects, no D3D fog → openblack: none — has.
