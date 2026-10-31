# Storm and tornado

## Summary

The storm miracles gather a ring of dark clouds that spiral in over the cast point and drift with the wind: STORM
(16) rains, the electric storm (STORM_PU1, 17) adds lightning that sets things on fire, and the tornado (STORM_PU2,
18) brings down a funnel that wanders over the ground, picking up objects and people one at a time and destroying what
it carries. A small whirlwind marks the cast. Each storm also registers a weather storm that darkens and wets the
land around it; weather storms (from the climate and from scripts) share that code, with their own cloud domes,
lightning and thunder.

openblack ports the weather storms, the spell, the clouds and their glow, the tornado, the whirlwind, the flash and
the cloud shadows faithfully, with a few approximations listed under openblack. This page owns storms and the
tornado's rule and cast; what the tornado carries and flings is on
[the vortex](../world/vortex.md#the-objects-a-particle-system-carries-tornado-and-vortex), the particle rules of the storm
on [Storm rules](../physics/particle-rules.md#storm-rules), and the weather page keeps one paragraph on weather storms.

## The original

### Storms: LH3DStorm and GWeather

The LH3DStorm descriptor (0x50 bytes, constructor `fn_0083F3F0`) comes by default with inner radius 100, outer 300,
fade 10 s, life 100 s, strength 1, 8 clouds, blackness 0.5, elevation 160, fall speed 1, no lightning, and as weather
10 degrees / rain 100 / overcast 100 / wind (10, 0). The GWeather object (0x3C0, list 0xEEA37C, the newest first)
copies it and adds age, destination, speed, deletion counter, lightning timers, drawing position, radii and fade.

- `GWeather::Update` 0x83F900 (from `fn_0083F840`, once per turn with 0.1 s):
  - when `lifeTime` passes it is marked for deletion;
  - the **fade** rises from 0 to `strength` in `fadeInTime` and goes down the same way at the end, and the **inner
    radius** follows it (the outer one does not);
  - it moves towards its destination at `speed` m/s in x,z;
  - it only moves when the distance to its target is above 0.001 ([0x8AA3B0], 0x83FA11);
  - the lightning, once faded in: `forkTimer` and `sheetTimer` count down and on reaching 0 they are reloaded with
    `min + rand(max − min)`. The fork one queues a strike point (0xEEA384; the atoms are made later by
    `UR_LightningStrike`, see [the weather's fork strike](fireball-and-lightning.md#the-weathers-fork-strike)) and the
    sheet one plays the thunder (0xEEA388). The flash (`fn_00837290`) is in
    [the flash and the clouds of the registered storms](#the-flash-and-the-clouds-of-the-registered-storms).
- The lightning callbacks are function pointers set by the engine: fork 0xEEA384 = 0x68E8F0 (from
  `PSysGlobal::InitializeOneTimeOnly`; it only queues the point, through 0x693A80), sheet 0xEEA388 = 0x429CE0 (GAudio's
  constructor fn_00429CB0 sets it: the thunder). Both get the storm, the point (storm x, z, land height + elevation)
  and the outer radius (pushed radius, &point, storm at 0x83FC6B..0x83FC75; `ret 0xC`).
- The thunder, 0x429CE0 (checked in the disassembly): `GetTickCount() % 11` (unsigned `div`) + 2 is the sample, then
  fn_0071E920(&point, sample, track 0, mode 2, loops 0, +0x40 0, is3D 1, ATMOS_TYPE 12 = rain, delay 1): a point sound
  tag in the rain's ambient bank (`rain.sad`, samples 2..12) at the storm's point, that waits until the sound has
  travelled to the camera (see [the audio engine](../audio/engine.md), the point tag). No volume of its own and no
  random draw: the clap comes from the wall clock.
- The lightning flash bitmap 0xED92F0 is built once for all flashes with a refcount at 0xEDD3A8 (values capped at 255
  inside d < 32).
- A new storm: fn_0083F6F0 → fn_0083F590 puts a new GWeather at the head of the list with speed 1 and target = its
  own position. The GWeather ctor fn_0083F4E0 leaves the speed (+0x68, metres per second towards the target) at 0.
- fn_0083F8D0 answers whether a storm is still in the list and not marked for deletion (the original keeps a
  GWeather pointer and asks it); the deleting destructor is vt 0x10, after fn_0083F630 unlinks it (and deletes its
  cloud mists).
- **Deletion over two turns**: `fn_0083F7B0` only marks (+0x94 = 1); the counter rises on each `UpdateAll` and the
  storm is destroyed when it goes past 2. While it is marked it no longer contributes anything to the grid.
  `GClimate::ToBeDeleted` 0x7713E0 does destroy them instantly.
- `GWeather::CalcAtmos` 0x8400E0: if the point is in the box and the circle of the **outer** radius, the weight is 1
  inside the inner one and goes down linearly to 0 at the outer one; `w = weight × fade × 256`. With `w == 0` it does
  nothing. The temperature **tends** towards the storm's (`LerpByte`) and rain, snow, overcast and wind are **added**
  (`ClampAdd(x, (value × w) >> 8)`).
- `fn_0083F750` (`KILL_STORMS_IN_AREA`) marks the storms whose 2D distance to the point is less than radius + their
  outer radius.

### Storm, electric storm and tornado

Read in `runblack.exe`. The three MAGIC_TYPEs (16 STORM, 17 STORM_PU1, 18 STORM_PU2) are the same class and cast the
same `SF_LightningStormPush`; what changes is the upgrade level their rules read (−1, 0, 1: derived from the seed,
unverified) and the effect rows and those of `GMagicStormAndTornadoInfo`.

#### The spell

`SpellStormAndTornado`, 0xF8 bytes, vtable 0x9847DC.

- Constructor 0x72D9C0: `Spell(type, creator)`, +0xEC (the whirlwind's PSys) and +0xF0 (the water reaction) to 0
  (`fn_0072DA10` zeroes +0xEC and +0xF0), and it enters **first** in the list 0xDA07F8 (count 0xDA07FC).
- `InitWithPos` 0x72DAA0: the radius (castData +0) is clipped to [minRadius 20, maxRadius 1000] and written back
  (`min(r, max)` and then `max(r, min)`); `Spell::InitWithPos` (the main PSys, magnitude = radius); then a second PSys
  **without a spell**, `GJPSysInterface::Create(NULL, 106 SF_StormCast, the LHPoint of the position, PSysProcessInfo
  +0x24, 1.0, 0)`, whose magnitude becomes the spell's (vt 0x11C).
- `Process` 0x72DB90: forgets the reaction +0xF0 if it is no longer there; if there is a whirlwind, a
  `PSysProcessInfo` of zeros with strength 1 and active which the creator fills in (`UpdateSpellInfo`, vt 0x5C),
  strength 1 again, and a step of the whirlwind; without a creator, or when the whirlwind ends (5), it is deleted.
  Then `Spell::Process`.
- `CalculateCostToMaintain` 0x72DB50 = that of `Spell` × (radius / radiusForNormalCost 40)² (`radiusForNormalCost`
  is info +0x60, `fdiv [info +0x60]`). `GetRadius`/`Get2DRadius` (vt 0x60/0x64) = the magnitude. `CloseDown` is that
  of `Spell`. `ToBeDeleted` 0x72DA20: leaves the list, `Spell::ToBeDeleted` and deletes the whirlwind.
- fn_0072DCC0, from the fire's rain cooling (fn_0072EFB0 0x72F2D9, with the position +0x14 of the burning object):
  the first storm spell without a water reaction whose radius exceeds the distance in x, z to its +0x14 receives
  `REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE` (34) from its player, stamped.
- Rows (info.dat): `rainAmount` 50 / 100 / **0**: the tornado **does not rain**. Cost per turn 20/25/30 × (r/40)²,
  per event 0/2/10, timers 40 s.

#### The cores and the clouds

`SF_LightningStormPush` creates 5 invisible cores (group 0, `CreateRuleSphere`), each with its collection of clouds
(group 2).

- **`UR_CloudMoverNew`** 0x6D41C0 (ctor 0x6D4150: DelayBeforeMove 0, WindDamping 0.06, WindMagnification 60): the
  first step places the cores at the PSys origin; until `DelayBeforeMove` (5 s in the file) it does nothing;
  afterwards, per core: the wind `fn_00771B10(p, smoothed)` (0 if the script casts), `T = wind × WindMagnification ×
  0.1`, `v += WindDamping × (T − v) × dt`, `p += v × dt`. The spell's position (+0x14) follows the core. With level 1
  the cores bounce off the shields (`DoAnyShieldDeflections`). 0x6D452C..0x6D4601 measure the slope and scale a
  **copy** of the velocity that nobody reads: in the exe the slope does nothing. CollectionData 0x24 bytes (ctor
  0x560C10).
- **`UR_CloudGather`** 0x6D4A70 (ctor 0x6D4700; `TornadoGroup` is not in the file: 10):
  - AtomData 0x38 (ctor 0x560B60) and CollectionData 0x58 (ctor 0x560AF0, dtor fn_006D4900: a detached lightning
    collection is deleted and fn_006D4950 marks the storm). The ctor 0x6D471B sets flags |= 6; it does nothing unless
    its three float providers (Radius, Scale, CloudHeight) exist (0x6D4A7A). fn_006D4970 creates NumAtoms clouds at
    once, each at a random age up to TimeToForm. fn_006D56F0 reads the storm spell's `rainAmount`
    (GMagicStormAndTornadoInfo, file +0x58); < 0 without a spell.
  - First step: lightning flashes if the level ≠ −1; rain yes; the heading = `GetCurrentHeading` 0x673660 =
    `(cameraForward.x, 0, cameraForward.z)` **unnormalised**; only the collection of the **first core** (index 0 in
    its collection, fn_00673CA0) registers the storm, and with level 1 it adds the tornado group to the core. Rate
    `NumAtoms / TimeToForm`; spin ±1 at random; **the first lightning** at `rand(0.5; 1) × SwitchLife +
    LightningDelay` (3 and 8 s in the file: between 9.5 and 11 s). The delay is not an interval: it is the other way
    round.
  - It emits while `count < due` (the first cloud comes out on the first step) and there are fewer than `NumAtoms`.
  - Each cloud (fn_006D4880): radius `(rand(0.7; 1) + 0.7) × R` (1.4..1.7 R, R = the provider Radius = 1.2 ×
    magnitude), random angle, age 0, extra height `rand(HeightVaryAmount)`. After `TimeToForm` it starts again: the
    clouds **spiral towards the centre** in `TimeToForm` s (`r = (1 − f) × radius × (1 + 0.3 cos(age × 0.1 + θ)) × the
    collection's scale`, with `f = age / TimeToForm`), with angular speed `±MaxAngularSpeed × (1 − (2f − 1)²)`.
  - Their look: grow = `f / FracToMaxSize` and then it goes down; grey colour `MaxColor + (MinColor − MaxColor) f`,
    alpha `MinAlpha + (MaxAlpha − MinAlpha) grow`, scale `(MinScale + (MaxScale − MinScale) grow) × CloudScale`, and
    the ratio `MaxCloudRatio + (MinCloudRatio − MaxCloudRatio) f` goes to the atom's stretch, which the mist creator
    uses as its k (`TakeRatioFromMatrix`). The collection, in its first 10 s, multiplies the ratio and the radius by
    `c + (1 − c) × smooth(t)` (`CloudRatioMaxCollection` 1, `CollectionRadiusInitialScale` 2).
  - Past half formed, a cloud calls fn_006D56B0(0.2, 2 − 2f) (0x6D4F8E) and discards the result.
  - Height: `baseScale × extra height × scale + CloudHeight`, plus the average ground height on a 3 × 3 grid of
    points at 0.33 R from the core (with the tornado, that of the PSys origin, which is the base of the tornado).
  - They are drawn as `LH3DMist` with the mist creator, the k taking the stretch and the colour multiplied by the base
    colour of the light table [0xFA26A4]. The shadow map `S_SMClouds16` **is stamped** on the ground
    (0x67A7BE..0x67A8C1 → fn_006CA280 → fn_0086CFF0 mode 2 → fn_00878C70) at `(x + 10, 0, z + 10)`, centred, atom
    alpha / 255. `RenderParticleMist::DrawAt` 0x67A7BE..0x67A8C1 puts one record per cloud into the list 0xD4EDB8
    (position `(x, 0, z)`, frame 0, alpha = alpha byte of the DrawData × 1/255 clipped to [0, 1]);
    `PSysLightMaps::AddDrawing` 0x6CA6E0 → fn_006CA280 stamps it with fn_0086CFF0 in mode 2 (shadow: bpp 1 because
    `IsShadowMap`, `ParticleMistCreator::GetBitmap` 0x6AA540), 16 × 16 cells of 10 m; fn_0086D360 → fn_0086D060 →
    **fn_00878C70**: bilinear, `v = 255 − (255 − texel) × strength / 255` with floor 0x30, and `min(v, byte +3)` in
    the per-vertex colour of the LandBlock's cells (bit 4 of +0x920; ClearLight fn_0086D460); the lightning's light is
    mode 1 (fn_00878780).
  - Each cloud's atlas counter is the +0x84 of its `LH3DMist`, which the ctor 0x7F9560 starts at `ftol(Random(0, 16))
    & 15` (0x7F95DC..0x7F95FB), the `Random` 0x81D180 being the CRT's `rand()`. The original only advances those that
    `LH3DMist::AddDrawing` 0x7FA7F0 sees.
- **The registered storm** (fn_006D5730, on the values of fn_0083F3F0): inner `max(R, 60)`, outer `max(2.5 R, inner
  + 20, 80)` (the three `fcomp; test ah, 0x41; je` keep the value only if it is greater); fade `0.5 × TimeToForm`
  (4 s), life 1e9 s, strength 1, **0 clouds** (`DrawClouds` draws nothing for the miracle), elevation = the provider
  CloudHeight (1.5 × magnitude); 20 degrees, rain `min(ftol(strength × rainAmount) as a byte, 100)` (so `strength 3 ×
  100 = 300 → 44`: 300 & 0xFF = 44 is what meets the `cmp al, 0x64` of 0x6D5915), overcast 80, snow 0; wind =
  heading × `strength × lerp(WindMinSpeed, WindMaxSpeed, clamp((mag − MagMin) / (MagMax − MagMin)))`, each component
  clipped to ±128 and rounded to a byte (128 gives −128). The position is **not** set when it is created:
  `fn_006D5950` copies it from the core each step, and since the destination of the `GWeather` stays at (0, 0, 0),
  `GWeather::Update` moves it 0.1 m per turn towards the map origin before it is set again (faithful, negligible). On
  closing it is marked for deletion (fn_006D4950); if someone deletes it (a `KILL_STORMS_IN_AREA`), the next step
  registers another one. Read 2026-10-24: `KILL_STORMS_IN_AREA` (fn_0083F750) only marks (fn_0083F7B0: +0x94 = 1
  when 0), and the gather's lookup each step (fn_0083F8D0 at 0x6D566A) answers the storm only while it is in the list
  with +0x94 = 0, so a marked storm reads as gone at once and +0x54 is cleared (0x6D5674). The registration test
  comes first in the step (+0x54 = 0 and the registered flag +0x47, 0x6D561F..0x6D562B), so the new storm is made on
  the step after, from a fresh `StormInfo` (0x6D5634): it fades in again over its 4 s. openblack: the same order in
  `UR_CloudGather` (`src/Particles/Rules/Storm.cpp`, `weather::storms::Find` gives no marked storm); test
  `Storm.aStormAScriptEndsComesBackOnTheNextStepAndFadesInAgain`.
- **Lightning** (0x6D52A0): with lightning flashes and half-formed clouds (those with `f > 0.5`, the static list
  0xD4EE88, which each collection empties when starting), the next one in `rand(0.5; 1) × SwitchLife / max(tribal
  power, 1)`; it comes out of a random cloud: the first time `AddSubCollection(LightningGroup)` on it; the following
  ones the same collection **moves** to the new cloud (fn_00674A30) and, after `rand(0.5; 1) × LightningLife`, it
  detaches from the cloud and stays without updating until the next one. The lightning is the `UR_Lightning` of the
  lightning bolt in its parent mode (radius 1 × magnitude): type 3 events with the storm's row (STORM_PU1: burn
  10000, hit 0.0045 → sets on fire at the first strike). The thunder is `SoundLightning` at the cloud, with a random
  size (`PSysFloatRand(1)` against the floats 0.33 and 0.66, strict: < 0.33 → 3, < 0.66 → 2, otherwise 1;
  0x6D5429..0x6D545F) and flags |= 0x22 (0x6D5472: delayed by distance / 347, on the ground).
- **The cloud's glow** ✅ faithful: during `SpecLife` (0.5 s) the lightning's cloud receives in its +0x90 a bluish
  specular (0x6D5205..0x6D5283: `v = ftol((1 − t / SpecLife) × 255)`, ARGB `(255v, 200v, 200v, 255v) >> 8`; after
  `SpecLife`, and in fn_006D4880 0x6D48E5, `0xFF000000`). **It is drawn**: `RenderParticle` copies +0x90 to DrawData
  +0xC (0x679BF4) → `RenderParticleMist::DrawAt` 0x67A6C4/0x67A6D6 pushes it to `SetColour` 0x7F9770 (+0x4C colour,
  **+0x50 specular**) → the effect branch of fn_007FA300 (0x7FA3B1..0x7FA5AF) does not touch +0x50 (the normal one
  overwrites it with the haze at 0x7FA6DD) → fn_0080DB30 0x80DEF5 puts it in [0xE9FE2C] → the specular of each vertex
  (e.g. fn_0084D2D0 0x84D645: vertex +0x14). `D3DRS_SPECULARENABLE` (0x1D) is set once when the device is opened
  (fn_0082C8F0 0x82CC1E..0x82CC5F): 1, except on a Voodoo ([0xEA9E9C], `CheckDescForVoodoo`); no other code changes
  it (search for `push 0x1d` and all calls to `SetRenderState` 0x412940), and mode 6 (fn_0082DF10) does not either.
  So the cloud brightens towards bluish white (colour = texture × diffuse + specular; the alpha does not change) and
  fades out in 0.5 s. The creator's `SpecColorR/G/B` (`ParticleCreator::DefineProperties` 0x6B3562..0x6B359B,
  +0x24/+0x28/+0x2C, 0 in the ctor 0x6A91C4..0x6A91CA) goes into +0x90 like fn_006A85E0 0x6A8748..0x6A875B: `(R <<
  16) | (G << 8) | B`, alpha 0; it is 0 in all the dumped files.
  - **The specular's alpha is not fog**: in D3D7 the specular alpha of a TL vertex is the vertex fog factor only with
    `D3DRS_FOGENABLE` (0x1C). The game never sets it: there is no `push 0x1c` before `SetRenderState` 0x412940 nor
    before `IDirect3DDevice7::SetRenderState` (vt +0x50) anywhere in `.text` (byte sweep; nor `FOGTABLEMODE` 0x23 or
    `FOGVERTEXMODE` 0x8C), and the D3D7 default is FALSE. The engine's haze is in software (light and specular of each
    vertex, fn_007FEB30). So the alpha 0xFF / `(255v) >> 8` of +0x90 changes nothing in the drawing. The effect branch
    of fn_007FA300 only changes the light (fn_0081E1F0 = the device's `SetLight(0)`, vt +0x48, with the position (0,
    500000, 0) and back, 0x7FA4F2..0x7FA590) and [0xC39264] (0xD2 → 0x5A).

#### The tornado

`UR_Tornado` 0x6D18B0 (ctor 0x6D1680). Only with level 1 (STORM_PU2): group 10 under the first core, and under it
group 11 with the rule.

- `UpdateBaseAndTopPoints` 0x6D1C60: the closing fade `1 − (t − close) / FadeOutTime`; the collections' alpha
  `min(age / FadeInTime, fade)`; the top follows the parent (the root of the tornado, which `UR_FollowParent` leaves
  at the core) after `DelayBeforeMove`; **the base wanders around the top** `(n(t) + n(2t)/2, 0, n(1.3t) + n(2.6t)/2)
  × TornadoScale × TopMoveAmp` with `t = age × TopMoveFreq` (`VSNoise1To1`), on the ground; the PSys origin becomes
  the base; the top at `TornadoScale × TopHeight` above it; velocities per step. **Each step** while it is not
  closing: `SpellEvent` type 2 at the base with its velocity (row 18: hit 0.001, push 0.001, radius 1).
- The funnel: radius `(BaseRadius + (TopRadius − BaseRadius) h²) × TornadoScale` (fn_006D2790; the exponent is 2);
  its axis at height h, `lerp(base, top)` with Schlick's gain `gain(FunnelBendParameter, h)` in x, z (fn_006D2910;
  `bias(b, x) = x^(ln b / ln 0.5)`, 0xD4EEA0 = 1 / ln 0.5) plus a wiggle of `TornadoScale × WiggleAmplitude` with
  phase `gain × WiggleCount × π`: the gain, still on the stack after the two lerps, `fimul` WiggleCount (+0x30),
  `fmul` the float π (0x8C36A0); the amplitude TornadoScale (+0x60) × WiggleAmplitude (+0x34) stored as a float; then
  `fld st0; fcos; fmul amplitude; fadd x; fstp x` and `fsin; fmul amplitude; fadd z; fstp z`, the cosine and the sine
  unrounded (0x6D29F9..0x6D2A27).
  - The FPU's order. The bias (fn_006D2710 for the spin, fn_006D2910 twice for the gain): `fldln2; fxch; fyl2x` of b
    (its natural logarithm, unrounded), `fmul` the float 1 / ln 0.5 = 0xBFB8AA3B, then `__CIpow` 0x7C7AE0 of x. The
    float is made at start-up by `crt_xc_fn_PSysTornado_006D1660` (`fldln2; fld [0x8AB260] (0.5); fyl2x; fdivr 1.0;
    fstp`), the same at 24 and 53 bits (oracle). The spin: ThetaBias kept between 0 and 1 (not above 0, or unordered,
    gives 0) and written back to +0x64; (TopThetaDot − BaseThetaDot) × the bias + BaseThetaDot, × the atom's speed
    (+0x28), × the strength (+0x64 of the tornado's data 0xD4EEC0), left on the stack and stored by the caller. The
    gain: h kept as above; 2h by `fadd st0, st0`; below 0.5, the bias of 1 − FunnelBendParameter at 2h × 0.5; else
    1 − the bias at 2 − 2h × 0.5 (`fsubr`); the lerps of x and z take it from the stack, unstored (0x6D29DF..0x6D29F6).
- The flying atoms (`UpdateFlyingAtoms` 0x6D31F0): spin at `lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) ×
  own speed (0.5..1.5)` (more slowly outside the wall: × `r / (ρ + 0.1)`), the radius approaches the wall with
  `dρ/dt = −0.5 (ρ − r)` integrated with the midpoint method (fn_006D28B0), they rise towards `lerp(base, top, mix)`
  with k 0.1 (real objects) or 0.3, and the funnel's velocity at that height is added. The real objects (state 0) and
  the decorative ones (1) go to group 12 (`GroupToMoveToOnceDone`, +0x98: gravity 10, collide with the ground, 15 s)
  when they pass 0.9 of the height (`h > 0.7 && h > 0.9`, 0x6D33AF..0x6D33CF), and on closing to
  `GroupToMoveToOnCloseDown` (+0x9c, 0x6D333F..0x6D334F; `DefineProperties` 0x6AD718 / 0x6AD72F); either one at −1
  deletes the atom. The decorative ones start with gravity and blend into the vortex in `PretendBlendTime`.
- **Picking up** (fn_006D21B0): once every three game turns, after `FadeInTime`, **one object at a time**; reach
  `(r(0) + r(1)) × clamp(tribal power, 1, 5)`; spiral of `(ceil(R/10) + 2)²` cells (GUtils::Spiral 0x74D7E0 with
  direction 1 and count 1, 0x6D22E9: −x, −z, +x, +x...); each object with a 3D object whose own cell is that one,
  **available** (vt +0x2C `IsAvailable`, 0x6D237A: not being deleted, and for a villager not dying, as in
  `SearchAround`) and within its 2D radius + reach; one that is not available is passed over before any branch. If it
  fits entirely (`CanBecomeAPhysicsObject`, `2 r(0) > radius`, `r(1) > radius`; for pots and piles, the flag of their
  `GPotInfo`, Pot 0x66E8F0) the spell is asked (`SpellEvent` 7, `CanBeDestroyedBySpell`); if it is a pile
  (`IsPileResource`), `lerp(150, 650, clamp(TornadoScale))` (rounded) is taken from it as **a new pile of the hand's
  type** (vt 0x870 `GetHandPotInfoType`: HAND_FOOD 12 / HAND_WOOD 11; from a store's pile it is taken from the store),
  scaled `rand(0.7; 1.2) × clamp(TornadoScale, 0.2, 1)`, and that one flies. A creature: fn_00477060, a jump to
  `Creature::Faint` (0x6D2464): it faints where it is, nothing is taken up and the search goes on. fn_006D2140 (one caller, the
  search fn_006D21B0 at 0x6D23B1) is the take-up test: with h = the object's vt +0x64 (0x6D214C), it answers 1 only
  when the object's vt +0x7B0 is not 0, bit 0x10 of Object +0x25 is clear, the object has a 3D object (+0x40),
  2 × fn_006D2790(0) > h and fn_006D2790(1.0) > h (the tornado's radius at 0, doubled, and at 1; 0x6D2161..0x6D2199).
  - A pile split by the tornado is created by `Pot::Create` with the hand's pot type (PileFood 0x66EC30 = 12
    HAND_FOOD, PileWood 0x66EC40 = 11 HAND_WOOD) and enters its map cell at once (`MobileObject` 0x607150 + 0xA9,
    CallVirtualFunctionsForCreation).
  - A carried object rises to rand(0.7) of the funnel's height (0x6D257A..0x6D269D).
  - A thrown debris or pretend object: baseVelocity + tornadoScale × speed × (cos θ sin φ, cos φ, sin θ sin φ), φ =
    rand(spread), θ = rand(2π), speed = rand(0.33, 0.66) × the rule's.
- What it carries (`RenderParticleGameObject`, 0x6C9E60): the atom copies the object's matrix (fn_00674150) and the
  object draws the atom's (`DrawAt` 0x67B170). When the atom dies (hits the ground in group 12, or at 15 s), the
  destructor 0x6C9FC0: **a living thing** is left where it is and dies (`DestroyedByEffect`: the villager dies),
  **anything else is deleted** (`ToBeDeleted`): the tornado destroys what it carries. The scale of the carried object
  is uniform (that of the Y axis of its matrix).
  - Release (fn_006C9FC0): an object no longer available (vt +0x2C, a dying villager too) is only let go, not killed
    or deleted (0x6C9FD8..0x6CA02F); otherwise fn_006C9EF0 puts it back on the map where the atom is (angles from the
    matrix, altitude 0), `EndPhysics(NULL, 1)`, clears +0xA & 0x10, then `DestroyedByEffect(player, 1.0)` (0x6CA01A;
    `Villager::DestroyedByEffect` 0x7502D0 → VillagerDead(2 SPELL)).
- Dust (`UpdateDebrisAtoms` 0x6D2AF0): `DebrisEmitRate` per second, at most 50, of the dust colour of the ground
  material (`GTerrainMaterialInfo.tornadoDustColorRGB` of `Terrain::GetMaterialInfo` 0x735330 at the cell of the
  tornado's base, 0x6D2BB2: snow, 27, where `GClimate::GetSnow` (the weather's snow cover byte, which the
  atmosphere's cell takes from the lying snow 0xEDC344 through fn_00834DD0) is at least 27 at the cell's corner, else the second
  material of the altitude in its country, a 0 becoming 1 and a material above 43 row 0, `GetTerrainMaterial`
  0x735380; openblack's `particles::storm::TornadoDustColour` through `ecs::terrain_material::AtCell` since
  2026-10-25, test `Storm.theTornadosDustTakesTheTerrainMaterialOfItsCell`);
  decorations (fn_006D2E70): `PretendEmitRate × TornadoScale` per second, only on dry land and with the tornado fully
  visible (alpha > 250): chickens and bushes. The funnel meshes (`S_TornadoNonFade.l3d`, fn_006D2A40) at the base,
  spinning `(1 + 0.13 i) × MeshThetaDot`.
  - The dust colour: each channel of the creator's colour × the ground dust's >> 8, alpha kept (0x6D2C21..0x6D2CC5);
    the dust cell is the high words of MapCoords(LHPoint) of the tornado position, passed on unchecked
    (0x6D2B9B..0x6D2BB2).
- `UR_Tornado` data: CollectionData 0x74 (ctor 0x5608A0; the one being processed is kept in 0xD4EEC0 and its
  collection in 0xD4EEC4); FlyingAtomData 0x2C (ctor 0x5609F0, a new one in state 2); DebrisCollectionData and
  FlyingCollectionData 0x28 (ctors 0x560950 / 0x5609A0: +0x20 made, +0x24 to make). Defaults and inputs:
  DebrisSpreadAngle 1.0367 (+0xC8); the TornadoScale provider is 1 without one, strength 1 (0x6D1924).
- fn_006D2860: the funnel sprites' scale (BaseScale + (TopScale − BaseScale)·h²) × tornadoScale (the sprites are
  unused in the data); fn_006D2710 writes ThetaBias clamped to 0..1 back into the rule.
- Unused: fn_006D1AD0, the funnel sprites (`NumAtomsToCreate` = 0 in the only file). Read by no code in the exe:
  `PretendHeight`, `MaxSearchDistance*`, `LocalSearchDistance`, `PauseBeforeAffectsGameObjects`,
  `UseTornadoStrength`, `K1_Accn`, `ScaleWhipUp`.

#### The whirlwind

`UR_StormCast` 0x6D59B0, SF_StormCast. 30 atoms in a ring around the parent (group 0 has a hierarchy and scale = the
magnitude): a radius that shrinks from `MaxRadius` to `MinRadius` at `RadiusDot` and, after `DispersalAge`, grows and
fades in `FadeOutTime`; spin `lerp(ThetaDotMinRadius, ThetaDotMaxRadius, normalised radius) × (1 ± ThetaDotSpread)`;
at `InitHeight × scale` above the ground; the parent advances along the heading **scaled to 20** (+0x64 of the ctor,
no property), accelerating from 0 to 1 between `AccnStartTime` and `AccnEndTime`. Data: CollectionData 0x40
(fn_006D5F40), AtomData 0x2C (fn_006D5F70). Alone, the whirlwind ends at 4.4 s.

#### The flash and the clouds of the registered storms

- **The flash** (the 0x24-byte object at GWeather +0x70): `fn_00837290(point, outer radius, intensity)` turns it on
  in the lightning of `GWeather::Update` (0.5 the fork ones 0x83FBF0, 1.0 the sheet ones 0x83FC5D); `fn_008372D0`
  ages it each turn and turns it off after 0.8 s; `fn_00837200` each frame (from `LH3DAtmos::Update3D`, only the
  unmarked storms): `s = 1 − age`, f1 = s, f3 = s³, both 0.1 between 0.2 and 0.5 s, × intensity. The terrain light
  stamp `fn_0086CFF0(f3)` (the 64 × 64 radial map 0xED92F0, `(32 − d) × 9`, made by fn_00837110). The flash at the
  camera is [0xFA2768] of `Update3D` 0x83587C..0x835903: the nearest storm in x, z (the unmarked ones; the first of
  equal ones), "inside" if `d² < ((inner + outer)/2)²` of its descriptor, and `ftol(clamp(f1, 0, 1) × 255)`.
  **Faithful quirk**: the inside flag is not cleared when another, closer one appears from which one is outside,
  which then gives its flash.
- **The clouds** (`GWeather::DrawClouds` 0x83FC90, for all the storms, marked or not, from fn_0083F8B0 each frame):
  up to 16 domes (confirmed: xref immediates; `numClouds` is clipped in the descriptor itself), one new one per frame: k `Random(2.5; 5)`, size
  `(outer + inner) × 2 Random(0.01; 0.015)`, at `(±1, Random(−10, 10) + elevation, ±1)`; every 400 **frames** a new
  destination `(±1, Random(0, 20) + elevation, ±1)` towards which they go in steps of 0.0025; position `(outer +
  inner)/2 × (x, z)` (confirmed: xref constants) from the drawn position, and the height above the ground; colour = the base of the light table
  with each byte × (1 − blackness/2), alpha `fade × 0.75 × base alpha`, and the distance haze of fn_007FEB30; they are
  drawn above alpha 5 (the effect branch). The storm's shadow on the ground (`fn_0086CFF0` mode 2 with the map
  0xEE9D3C, strength `(blackness + 0.7) × fade`); with 0 clouds (the miracle) `DrawClouds` exits earlier (0x83FC9E
  `jle 0x8400CB`), so the miracle's storm does not put that shadow, only that of its particle clouds. `Random`
  0x81D180 is the CRT's `rand()` (seed 1 at startup). The miracle does not have these clouds (0); the climate ones and
  those of the weather objects do (8 by default).
- **Overcast at the camera** (map-level): `GCamera::Update` 0x4426BA, the overcast byte of
  `LH3DAtmos::GetWeatherSmooth(camera, 1)` × 0.01 → [0xD1A26C], which `DrawSky` 0x5E2215 copies to [0xFA2754] for the
  light table. It can exceed 1 (a byte of up to 127). For the miracle: the overcast 80 of fn_006D5730 (+0x4B) reaches
  the camera through `CalcAtmos` (full inside the inner radius, 72 m with radius 60) and the light table applies it:
  cap of the base colour `ftol(255 − 96 × 0.8)` = 178 (0x869ADB) and the haze towards the storm one (colour `(c >> 3)
  + 32`, k 48, near 15, far 350; 0x869DB1..0x869F37). In the Land 1 afternoon the change is barely noticeable: what
  darkens most in the original is the clouds' shadow.

The sky alignment (`fn_0064AC30`), which also changes the clouds, is in
[the sky alignment](influence-and-alignment.md#the-sky-alignment).
#### More addresses from the code

- The cost is the square times `Spell::CalculateCostToMaintain` 0x720810; the radius's clamp is
  `SpellStormAndTornado::InitWithPos` 0x72DAA0 (0x72DAB2..0x72DADC).

## openblack

Status: **done**, with the approximations below. Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Weather storms**: `Storms.cpp`; the lightning callbacks are `SetForkCallback` / `SetSheetCallback`. The sheet one
  is set when the audio starts (`audio::Init`) and cleared when it shuts down. The fork one is set where the game's
  services start (`Locator.cpp`, right after the weather system, the particle system being already there) and cleared
  before the particle system goes: it queues the point for the strike queue that `UR_LightningStrike` empties
  (`particles::lightning_strike::Queue`). The flash is `Storm::flash`. The thunder is `audio::tags::Thunder` with
  `lightning::ThunderClap` and `audio::TickCount()` (the fixed clock under `OPENBLACK_FIXED_FRAME_MS`).
- **The spell**: `Magic/Spells/SpellStormAndTornado`; the water reaction is connected in `ECS/Fire/FireEffect.cpp`.
  The villagers it reaches are only impressed by it, once per their standard wait, as by the other reactions that only
  impress ([reactions that only impress](../beings/villager-reactions.md#reactions-that-only-impress-34-39-and-40)).
- **The wind magic a script asks for** (`IS_WIND_MAGIC_AT_POS`, [the weather natives](../scripts/script-commands.md)):
  `weather::storms::IsWindMagicAt` answers yes where a storm with no clouds (a tornado's or a storm miracle's wind),
  marked for deletion or not, has the point within its outer radius plus the tiny float the original adds (since
  2026-10-21; test `WeatherTest.WindMagicIsACloudlessStormOverThePoint`; the debug Weather window says whether there
  is wind magic at the camera).
- **The cores' drift**: `UR_CloudMoverNew`'s step towards the climate's smoothed wind × WindMagnification × 0.1, by
  the damping × the difference × the step, is `storm::CloudWindVelocity`, in the original's order (test
  `Storm.theCloudCoresDriftWithTheWind`, 2026-10-21).
- **The clouds**: `Particles/Rules/Storm`; drawn by the mist creator (`Particles/Creators/Mist.cpp`), corrected so
  that the k takes the stretch and the colour is multiplied by the light table's base
  (`land_light::CurrentTable().GetLandColour()`, that of the previous frame). The cloud shadows: `mist_atoms::SubmitFrame`
  calls `land_light::AddStamp`.
  `MistArchetype::Create` starts each cloud's atlas counter from `game_random::crt::Random(0, 16)`, the CRT stream,
  as the `LH3DMist` constructor does.
- **The glow**: `Atom::specular` (+0x90) and `DrawAtom::specular` (`Particles/ParticleEffect.h`), written by `CloudGather`
  (`Storm.cpp`), passed by `mist_atoms::SubmitFrame` to `MistDesc::specular` (`Graphics/Mists.h`) and added in the
  effect branch of `Renderer::DrawMist` (`u_cloudSpecular`, `fs_cloud.sc`); the port only uses the RGB, which is
  faithful. The creator's `SpecColorR/G/B` is read (`particles::ReadCreatorProperties`) and `Effect::NewAtom` puts it in
  +0x90.
- **The tornado**: the available test is `storm::PickUpBranch`, gated by `particle_carried_objects::IsAvailable`. A
  creature is `TornadoSystem::CatchCreature`, the fight system's `ForceFaint`, once (a creature out cold is not caught
  again until it has come round). openblack checks the island side of the dust cell. `Effect::UpdateCollection` walks
  by index.
- **The funnel's numbers**: `storm::ThetaDot`, `storm::Bias`, `storm::Gain` and `storm::FunnelCentre` take the
  FPU's logarithm and the runtime's power (`gutils::x87::Log`, `Pow`, see
  [Floating point](../engine/floating-point.md#the-runtimes-arctangent-arccosine-power-and-logarithm)) in the
  original's order: 10 000 of 10 000 spins equal fn_006D2710 and 10 000 of 10 000 gains equal fn_006D2910 (a line
  from (0, 0, 0) to (1, 1, 1) with no wiggle), each run whole in the oracle (`test_storm_x87`). The wiggle takes the
  gain for its phase and the FPU's cosine and sine (`gutils::x87::SinCos`): 10 000 of 10 000 centres equal
  fn_006D2910 run whole with wiggles of 0 to 8 half turns. A flying atom's next point round the funnel is
  `storm::OrbitPoint`, in the order of `UpdateFlyingAtoms` 0x6D34AE..0x6D3526: the FPU's arctangent of dz over dx
  (`fpatan`, unrounded) plus the slowed spin × dt (one rounding), `fcos` and `fsin` of it, each × ρ₂ rounded once and
  stored, then added to the centre; y is vy × dt + dy + the centre's y, unstored until it is used. 10 000 of 10 000
  points equal the original's own instructions run in place in the oracle (the stretch is entered with its frame
  set up and left at 0x6D352A, since the whole function needs a live atom list).
- **The flash and the registered clouds**: `ECS/Weather/LightningFlash` and `ECS/Weather/StormClouds`
  (`LightningFlash`, `StormClouds`).
  `weather::LightningFlashAtCamera(camera)` is [0xFA2768]. The terrain stamps are made with `land_light::Stamp` /
  `land_light::AddStamp` (`LightningFlash.cpp`, `StormClouds.cpp`, `Mist.cpp`). The clouds go to `mists::Submit`; the
  CRT draws are `game_random::crt` ✅ faithful. The overcast at the camera is `Clouds::WeatherOvercastAtCamera`, through
  `storms::CalcAtmos` and `LandLightTable::Build` (trace `overcast 80`).
- **Checked in the game** (by day): the ground under the storm is darker than without it, with dark blotches with sharp
  edges that move with the clouds, and each lightning lights up the ground (and the trees, which take the light of
  their cell) in a bluish patch. Without the original side by side it cannot be stated whether the intensity is the
  same (the 0x30 floor leaves almost black blotches).
- **Approximate**:
  - the place where a released living thing dies: where its atom was last seen (the turn or the frame);
  - taking resources from a loose pile: the amount and the sinking (like the hand); the empty pile is not deleted.
    Piles are searched for in the map's cells, as every other object (a pot joins its cells when it is made,
    `PotArchetype::Create`);
  - the tornado's collection is updated in the same step it is created (the original links it at the head of the
    list and it enters on the next step);
  - the clouds of the registered storms count frames like the original, but openblack draws more frames per second;
    their atlas counter always advances (not only on screen); the base of the light table is that of the previous
    frame;
  - each cloud's atlas counter (`Atom::mistCounter`) starts like the original's with `game_random::crt`, and advances
    on every frame even if the cloud is not on screen. Before it was a global counter for all: they no longer pulse in
    unison.
- **Inferred**: what `InitialisePhysics` does with a villager or animal carried by the tornado: here it leaves physics
  and goes to the hand state (IN_HAND, `animal_ai::PlaceInHand`). Bit 0x10 of Object +0x25 that fn_006D2140 looks at
  is not checked. The `GetDistanceInMetres` distance is taken in x, z.
#### Where the code is

- `StormInfoOf` (`SpellStormAndTornado::GetMagicInfo`) and `ClampStormRadius` (the radius's clamp) in
  `SpellStormAndTornado`.

### Test hooks

- `OPENBLACK_TEST_SPELL=STORM,x,z,60` (and `STORM_PU1`, `STORM_PU2`), `OPENBLACK_TEST_STORM_SHOT`,
  `OPENBLACK_TEST_STORM_STRIKE_SHOT`, `OPENBLACK_TEST_STORM_PILE`, `OPENBLACK_TEST_STORM_CLOUDS` and
  `OPENBLACK_STORM_TRACE` in [debug environment variables](../project/debugging.md#debug-environment-variables).
- `test_storm`: the spell's clipping and cost, the fn_006D5730 descriptor (radii, wind, rain and their quirks), the
  cloud ramps, the funnel (radius, bias/gain, RK2), a cloud collection that registers its storm and marks it on
  closing, the flash (curve, map, camera and the flag that stays), the colour of the registered clouds, the interface
  alignment and, with the game, `SF_LightningStormPush` / `SF_StormCast` (their registered classes; the whirlwind
  alone, which ends at 4.4 s); `cloudMistAtoms` (each cloud's own counter and the specular in the drawing atoms) and
  `creatorSpecColour` (the creator's `SpecColorR/G/B`).

## Pending

- **Unverified**: the upgrade level from the seed: −1 / 0 / 1. Verify with the oracle by running `UR_CloudGather`'s
  first step for each of the three seeds and reading the level it stores.
- **Inferred**: what `InitialisePhysics` does with a carried villager or animal, and what bit 0x10 of Object +0x25
  means (see openblack). fn_006D2140 is now read (the take-up test, [above](#the-original)); the bit's meaning is still
  open.
- **Terrain, not storm**: the row/column order of the texel in `land_light::ApplyStamp` against fn_00878C70.
- **Not ported**: fn_006D1AD0 (unused in the data).
- **The intensity of the stamps** against the original: needs a side-by-side capture.
