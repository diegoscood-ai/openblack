# Objects and resources

Pots and piles of food and wood, taking in batches, the store, the static objects and the rocks, the fields and the
sounds (picking up, LHAudio and QMixer, channels, hand in the water, ambience). Everything **faithful** except what is marked **(inferred)**.
The trees are in [trees.md](trees.md) and the map loading in [map-loading.md](map-loading.md).

- [Pots and piles](#pots-and-piles-pot--pileresource)
- [Taking in batches](#taking-in-batches-multi-pick-up)
- [Store](#store-storagepit)
- [Static objects](#static-objects-mobilestatic-rocks)
- [Fields](#fields-field-report-documentacionfieldfield_notestxt)
- [Sounds](#sounds-report-documentacionsoundnotestxt)
- [Moved to other pages](#moved-to-other-pages)
- [Pending](#pending) · [Test hooks](#test-hooks) · [Sources](#sources)

## Pots and piles (Pot / PileResource)

- `Pot::Create` (0x66CF10): potType 0 = simple Pot, 1 = PileFood, 2 = PileWood. The hand pots (HandWood 11,
  HandFood 12) are also piles.
- **Simple pot**: scale `min(5, cantidad/scaleEvery + 0.25)` (`Pot::GetScaleFromAmount` 0x66D4A0). `scaleEvery` is not
  used for anything else.
- **Pile**: it does not scale; it sinks. `PileResource::SetSize` (0x66E900): target `(GetProportionRaised − 1)·altura`,
  animated over 1 s with the Zoomer; it is drawn at `GetAltitude(pos) + desplazamiento` and is not visible if it is fully buried.
- `GetProportionRaised` (0x66F1B0 wood / 0x66EB60 food): x = amount/maxInPot in [0.1];
  p = x > 0 ? 0.05 + 0.95x : 0; food: 1 − (1 − p)². A single copy, `ecs::object::GetProportionRaised`
  ([engine-math.md](engine-math.md#object-size)); the 2D radius of a food pile is
  `GetProportionRaised × Object::Get2DRadius` (0x66F180), so when empty it measures 0.
- When created, every pile starts buried (`−altura`) and rises over 1 s (`CallVirtualFunctionsForCreation` 0x66E300).
- Creation scales: **MagicFood 0.3**, **MagicWood 0.7** (constructors 0x5FA9F0 / 0x600E20); the rest 1.
- `PileFood::Draw` (0x51BF80): the store's food pile (info 2) and the magic food (info 10) offset the
  texture in V by `0.25·(1 − clamp(desplazamiento/altura + 1))` (LH3DObject vfunc 0xE8, **inferred** as a UV
  offset). The grain seems still and the pile "shrinks".
- `FoodPile` (info 8, MSH_B_WORSHIPGRAIN) is the pile of the worship sites; its mesh is offset.

## Taking in batches (multi pick-up)

- Press on a pile: 25 immediately into a hand pot. Every 0.1 s turn: `(int)(8 + 62·t²)`, t = n/60,
  up to **20000** per batch (`maxAmountCanBePickedUp`). Verified: 413 at 3 s, 1746 at 6 s.
- The hand stays fixed in x,z over the pile, y = ground + height of the pile.
- Releasing the button leaves the pot in the hand; another press + release puts it down or throws it.
- Putting down: it joins a nearby pile or store of the same resource (approximate radius 15 m) or creates MagicWood/MagicFood.
- Particles (`SF_MultiPickUpWood/Food`, `ER_MultiPickup::ModifyAtomCollection` 0x6A77C0): 8 per second, each one goes
  in a straight line over 1 s from the ground under the hand to the current position of the hand; they are destroyed when picking up stops.
  Wood = mesh MSH_I_OFFERING_WOOD at 0.35 with `AppearanceRuleTumble`; food = grains from S_SpriteSheet1 (32 frames,
  20 fps); fish = S_Spangle_A (not implemented).

## Store (StoragePit)

- A single total for 5 piles of wood + 1 of food (`StoragePit::AddResource` 0x732F60 /
  `RemoveResource` 0x7332A0).
- Adding wood: piles 1→5, each up to 5000 except the last one (no cap). Taking out: **5→1**, regardless of which
  pile it is taken from. A pile at 0 stays buried and is not visible.
- `PotStructure::GetResource` (0x66EF00): a store pile reports the store's total.

## Static objects (MobileStatic, rocks)

- Position `GetAltitude(pos) + altitud del script`, rotation `SetYXZMatrixOnly(y, x, z)`, uniform scale
  (`Game3DObject::SetPosition` 0x63B680, `MobileStatic::GetWorldMatrix` 0x608DE0).
- The original does **not** settle them on the ground: `GetAltitudeFondation` is only used for buildings.

## Trees

Moved to [trees.md](trees.md) (uprooting, dropping, forests, growth, drawing).

## Fields (Field, report `documentacion\field\field_notes.txt`)

- The 6 GFieldTypeInfo are identical: ageGrowth 80, ageRecolt 1200 (ripe), timesToSow 30, foodValueTakenWithHand 25,
  totalFoodInField 350, maxFarmerInFarm 10, sun 0.5/1.5, rain 1.5/1.5, ratioBeforeRipe 0.2. The `IsUnripe` symbol
  (0x5298D0) returns **ripe** (growth ≥ 1200).
- `Process` 0x529020 every 10 turns (+ an offset 0..9), with the 30 crops sown and not ripe:
  d = 2·(0.5·alignment + 1)·(0.5 growing | 1.5 ripening or with rain); growth += d, food += d·350/1200.
- `RemoveFood(n)` 0x5295A0: 0 without food or unsown; cost = n ripe, (int)(1.2·n) unripe; if it is not enough:
  unripe it is emptied and gives (int)(0.2·n); ripe it gives what is left and is deleted entirely (it has to be sown again).
- Hand: action button over the field (locked selection; needs growth > 0 and food > 1). It starts with
  (int)min(25, food), **half if it is ripe**, removed from the field; per turn (int)min(8 + 62t², food), t =
  min(turns/60, 1), ≤ 20000 − what is in the hand, half if ripe; the hand receives n (oddity). Grain particles
  (`SF_MultiPickUpFood`), sound G_PICKUPFOOD. It cannot be given back.
- Drawing `Draw` 0x528570: one mesh (MSH_T_WHEAT); only with growth ≥ 20 and food ≥ 25; it sinks v = food/350 − 1
  over 1 s (y += 2·v·scale·height) and fades out below v = −0.8; colour from olive to light green while growing, to
  white while ripening; the ripe ones sway with the trees' wind.
- openblack: `ecs/Fields`, `HandFish.cpp` (`TryPickUpField`, `UpdateFieldPickUp`), hook `OPENBLACK_HAND_TEST_FIELD=1`.
  **Differences**: the engine is faithful (the field is born empty and only the farmers sow it, and there are no villager
  jobs yet, so the fields stay empty); the **`world.crops`** mod replaces them: they start sown and ripe,
  are sown again when emptied and grow `speed` times faster. Since the hand leaves the last unit
  of food of a ripe field forever (its halved and truncated amounts reach 0, and `RemoveFood` only deletes it if it is
  asked for more than it has), with the mod a ripe field with less than 25 (what it needs to be drawn) counts as
  empty and is deleted. The alignment/rain in the growth is missing.
- **Colour and sway of the mesh** (`Field::Draw` 0x528570, details in `documentacion\field\draw_colour_sway_notes.txt`):
  `BlendColor` 0x5284C0 (k = 0 gives a, 255 gives b, `(a(255−k) + b·k)/255` truncated): growing, olive (121,145,25) →
  light green (170,212,67) with k = 255·(1 − food/350); ripening, olive → white with k = 255·(growth − 80)/1120;
  ripe, white. It multiplies byte by byte the object's terrain light, `(c·tinte) >> 8` (fn_0080BF10), before the
  haze and the N·L: in openblack it goes in the x of the instance's fifth column, negative
  (`−1 − r·65536 − g·256 − b`, `lh3d_colour::PackInstanceTint`, [rendering-objects.md](rendering-objects.md#the-object-colour-fields-in-the-instance)), only if the world.foliage mod does not set its `MeshTint`. The
  ripe ones sway: column 1 (up axis) is sheared in world z with `1,75 × escala × T0[i]`, `T0 = −0,03·cos(fase)`
  of 16 phases (`Tree::PreDraw` 0x74A7C0: speed Random(1, 2) every 2 s, phase += ms·speed·0.00106061; the wind
  angle is always 0), `i` fixed per field (in the original, bits of its address); only the drawn matrix.
  `ecs::FieldDrawColour`, `ecs::WindSway`. The trees use the same table ([trees.md](trees.md#drawing)). With the world.foliage mod
  (`fields = wheat`) the field is drawn with plants that grow in stages instead of the mesh (mod-library.md).

## Sounds (report `documentacion\sound\notes.txt`)

- LHAudio's pitch is a **percentage** of the wav's frequency (100 = normal). On start (0x1001278B, unsigned
  integers): d = deviation·p/100, p = p − d + rand·2d/32767 (0 → 100), frequency = rate·p/100 (integer division; the same
  in `LHSampleSetPitch` 0x10013520, which does nothing if the channel already has that p). The pitch, the volume, the loops
  (+0x248) and the .sad mode only count if their bit is in the flags at +0x244 (0x1, 0x20, 0x40, 0x400) and the
  caller has not set them (mask +0x1C of the options); otherwise, 100, 127, 0 and 3. openblack passed the raw number.
- **Volume** (verified with Unicorn, `documentacion\agua\re\emu_qmixer.py`): LHaudio sends to `QSWaveMixSetVolume`
  floor(master·v/127)·258 (0x100133C1; master = BWSetup's `AudioSampleMasterVolume` = 127 → v·258, 0..32766) and
  QMixer stores it as vol/32767 (0x18007AE5) and **multiplies** it by the distance gain (0x1800AE20):
  linear gain v·258/32767 (`sample_play::QMixerGain`, `Sound::volume`). The .sad's "user param"
  (`LHSampleGetUserParam` 0x10014230) is the high half of that same u32 (+0x25C >> 16).
- Picking up from a pile, field or fish farm: **a single looping channel** (G_PICKUPWOOD 98 for wood; G_PICKUPFOOD 44
  for the rest) whose pitch rises to ftol(60 + 180·t²) % per turn; it stops on release or when it runs out. Putting down on a pile:
  G_PileFood/Wood(Small) depending on the amount (< 200 small ones). openblack: `HandSystem::UpdatePickupSound` with
  `audio::PlaySoundEffect` (.sad mode 2, owner 0, `audio::SetPitch`, `audio::StopSoundEffect`); traced with `OPENBLACK_HAND_TEST_FISH=1`: a single start and
  pitch 0.60 → 1.02 over the hook's 3 s. The original makes it 3D (+0x0C 0, it does not follow anyone) at the point +0xC8 of the
  interface state, which is **the hand**: `GInterface::Process` → `fn_005D2250` sends in packet 0x15 the
  hand's position (`CHand`+0x78, `Morphable::position`, 0x5D2350); `GPacket` 0x63CA9E → 0x5DBFB0 stores it at
  +0xA4 (and the camera at +0xB0/+0xBC); `GInterfaceStatus::Process` 0x5DC4E7 → `fn_005DBC60` computes the hand's velocity
  with +0xA4 − +0xC8 and copies +0xA4 to +0xC8 (0x5DBF1F) **before** `ProcessInInteract` (0x5DC574), which reaches
  `UpdateMultiPickup`. So it sounds where the hand is at the start (mode 2 does not move it afterwards) and does not start with the
  camera more than 180 from the hand. openblack: the position of the hand's `Transform`.
- The player **no longer sets AL_PITCH = 1 every frame** (`AudioPlayer::UpdateSource` did so and erased the .sad's pitch
  and that of `SetEmitterPitch`): the pitch is set when the source is created and with `SetSourcePitch`.
- **Distances** (`QSWaveMixSetDistanceMapping {min, max, escala}`, LHaudiodllR 0x10012159): .sad +0x268 / +0x26C /
  +0x270 if the flags 0x80 / 0x100 / 0x200 are set; otherwise, 1 / 9999 / 0.3 (`LH_SamplePlayOptions` 0x10010E90).
  QMixer (0x1800ACDF, 0x1802CE50; channel flags 0x103/0x111 from 0x10012065: neither 0x800 "cap at max" nor 0x1000
  "linear"; verified with Unicorn): gain 1 up to min (or with scale 0), `min / (min + escala·(d − min))` up to max
  (min/d with scale 1), **0 beyond max** (the channel keeps playing, muted). The scale is per channel because LHaudio does not
  use the hardware mixer (option `UseHardware` of `HKCU\Software\Lionhead Studios Ltd\Audio\Override`, which does not
  exist; with it it would call `QSWaveMixSetListenerRolloff(4)`). In openblack: `AL_INVERSE_DISTANCE_CLAMPED` with reference = min and rolloff =
  scale (`AudioPlayer::SetSourceDistance`), and the channel beyond max is muted in `AlSampleOutput` (the 16
  channels of `audio::sample_play`; since phase B5 of the audio there are no `AudioEmitter` emitters). `Sound` also stores `cloneGroup` (+0x118), `playMode` (+0x274 with 0x400, otherwise 3),
  `atmosGroup` (+0x11A) and `atmosFrequency` (i32 at +0x27C, −1 = not an atmosphere one; checked with `offsetof`).
- `GAudio::PlaySoundEffect` 0x429E30 with a position: **does not start** if the camera is farther than the .sad's max
  (raw +0x26C, `LHSampleGetMaxDistance` 0x10014170; the mapping's max if it is 0) → `audio::PlaySoundEffect` (`Audio.h`; formerly `AudioManager::PlayAt`, removed in phase B5 of the audio).
  In addition (0x429F36..0x429FD9, and the same in `SamplePlayAnimEffect` 0x42A4B0): with the widescreen (panoramic) view set by a script
  (+0x45E8 and +0x45EC) no sample with user param 1 plays (InGame has 60, editor 82, e.g. `G_WaterFlow`);
  inside the citadel (`g_game+0x205A28 == 1`) only those with user param 2; after `SET_GAME_SOUND false` (GScript+0x90,
  0x7100B0, which also does `LHSampleStopAll`) only the dialogue banks HelpSprites and Villagers. The collisions of
  physics go through `SamplePlayAnimEffect` → `LHSamplePlayAnimEffect` 0x100146F0: they do not start beyond the .sad's max nor beyond
  800 (0x426E6B) and the channel's owner is the object. All in `Audio/SamplePlay` (`PlaySoundEffect`, `PlayAnimEffect`).
- **Channels and modes** (`LHSamplePlay` 0x100113B0: allocation 0x10011020, start 0x10011420; `Audio/SamplePlay`): 16
  channels (+0xCC, constructor 0x1001535E). Each channel remembers bank, owner (+0x20 of the options: 0 none, −1 the
  ambience's, or the object), sample, clone group (.sad +0x118) and priority (.sad +0x240). Mode 1: a free channel.
  Mode 2: **nothing** (not even a new position) if a channel of the same bank and owner plays the same sample or one of the same group
  (> 0); otherwise, a free one. Mode 3: the channel of the same bank, owner and sample (it restarts), otherwise that of the same group
  (> 0), otherwise a free one. With no free one: the one with the lowest priority if it is lower than the sample's (it restarts); otherwise, it does not play.
  `G_BigSplash` (mode 2) does not play again while the same sample of the same object is playing; the ten `G_HandInWater`
  (group 4, mode 3, no owner) restart one another; the ambience one-shots (mode 3, owner −1) restart their
  channel if they come up again while playing. Only the samples that go through `sample_play` count towards the 16: the
  physics hits, the hand's water and dust, the pick-up loop, the ambience, the `SoundTags` (waterfall) and those
  of the boat. Still on their own (topics of other sessions): `AnimationSounds`, the rocks, the camera whistle,
  `G_RockPast` and the hand's pick-up/plant/break `PlaySample` calls.
- **Tracking and listener, once per turn** (`fn_004270D0` from `ProcessAudioGameTurn`): `LHSampleUpdate3DChannels`
  0x10014310 moves each 3D channel with +0x0C (1 by default) and without AtmosInfo to its owner's position
  (`Get3DSoundPos`, game function 0x427200): without an owner, **the camera**; owner −1 or no longer present, it stops; beyond
  its max (+0x6C) from the camera, it stops. The collisions set +0x0C = (code A of the table ≠ 0x16, 0x64689F); the hand in
  the water and the pick-up one, 0. Then `LHListenerUpdate`: the QMixer listener = the camera (position, forward and up,
  velocity 0) **only once per turn** (and not while paused nor in the first 5 turns); openblack the same
  (`AudioManager::UpdateListener`).
- The .sad files are RIFF wavs (`QSWaveMixOpenWaveEx`); openblack used to try MPEG first and dr_mp3 found frames inside
  some of them (`G_BigSplash_03` lasted a fraction of a second): now a RIFF goes straight to the wav reader.
- **Listener axes**: openblack is left-handed and OpenAL right-handed; positions and velocities already went with x ↔ z, the
  orientation (at, up) did not, and left and right came out reversed. Now it goes through the same swap.
- **Hand in the water / grabbing land** (`StartLandscapeGrip` fn_005D1AB0): a single branch chosen by the **water bit
  of the cell** (`InBounds && IsLand` 0x5D1F94; outside the map = water), not by the height. Water: ring,
  `G_HandInWater_01..10` = InGame 99 + counter (0xD18228, 0..9, advances even if discarded), **3D at (x; 0.2; z)**,
  vol 127, pitch 100 ±5 %, min 40 / max 150 / scale 4, and the scare of the fish. Land: dust and
  `G_HandGrabLand_01..06` = InGame 4 + LocalRand(6) (0x5D1FC4, `SoundTag` without an object, 2D: is3D 0, vol 10, pitch
  60 ±15 %). The ten water ones are in **clone group 4** and are played in mode 3 without an object: `LHSamplePlay`
  (0x10011146..0x100111BC) restarts the previous one's channel → **one at a time** (`sample_play`, HandFish.cpp).
  Trace: `OPENBLACK_AUDIO_TRACE=1`.
  - Conditions (resolved): the land branch (0x5D1FA8) makes no dust nor sound if `g_game+0x25005C` (the
    **HelpSystem**) has the **widescreen** set (+0x45E8) **by a script** (+0x45EC = the script's task number,
    `GScript::SetWideScreen` 0x6F7BF0 → `HelpSystem::SetWideScreen` 0x5C6AD0; the videos and the playback
    pass 0); the water one (0x5D1FF0) does nothing with the **game paused** (`g_game+0x14` bit 4, toggled by
    `PauseGame` 0x54AE20) nor if the hand's 3D object (`CHand+0x482C`) draws something held (+0x8C, set by its
    `SetHeldG3D` vt+0x234 = 0x816830; empty also with a `SpellSeed` that is not drawn in the hand). In openblack:
    `ScreenFade::IsWideScreenOn` (only `SET_WIDESCREEN`), `Game::IsPaused` and `!_held` (`HandPlacement.cpp`).
- **Ambience (atmos)** (done, 2026-09-30; `Audio/SoundMap`, `Audio/AtmosBanks`; report `documentacion\agua\audio.md` §1-3):
  - Cell zone: `Terrain::GetAtmosType` 0x7352B0 = `(flags >> 2) & 0xF` (bits 2..5 of byte 7; **1 = SEA** outside
    the map or without a block). Table 0x9CB048 of 14 types {name, bank, daytime}: 1 SEA `ocean.sad`, 2 STILL_FRESH_WATER
    `lake.sad`, 3 COASTAL `shore.sad`, 4 JUNGLE*, 5 ARCTIC, 6 DESERT*, 7 COUNTRYSIDE*, 8 SWAMP*, 9 RUNNING_WATER
    `stream.sad` (no base map uses it), 10 STRATOSPHERE `high.sad`, 11 NIGHT* `night.sad`, 12 RAIN, 13 WIND
    (* = daytime). The codes of Daniels118's editor are `tipo << 1` (bit 1 is the water cell that is not drawn).
  - `GSoundMap::Update` 0x71D6F0, every turn from `GGame::EndTurn` (before `ProcessSoundTags`): receiver = camera;
    11 × 11 cells around (radius 50); per type, number of cells and the nearest corner. Volume = radial (1 up to
    20, 0 at 50) × fade by the camera's height above the ground at that corner (1 below 120, 0 at 250); the daytime ones
    also × weatherFade × (1 − night), night = max(0, sky type − 1). The coast is computed without that and the **sea =
    min(radial, 1 − coast)**: within 20 of a COASTAL cell the sea goes quiet (the point `1464, 2016` of Land1 gives SEA 0,
    COASTAL 1; `1300, 2016` gives SEA 1). STRATOSPHERE with the absolute height: 0 below 200, rises up to 1500, falls up to
    44444. NIGHT = fade(camera) × (1 − fraction of sea cells) × weatherFade × night. RAIN = rain/70, WIND =
    (|wind| − 15)/30. `CameraWeather()` reads the weather with `ecs::weather::atmos::GetWeatherSmooth(cámara, true)`
    (`LH3DAtmos::GetWeatherSmooth` 0x835180; byte 3 is the cloudiness).
  - `GAudio`: targets = the map's volumes, **all 0 inside the citadel**: the symbol
    `HelpSystem::GetWideScreenControl` 0x4282F0 is misnamed, it is `g_game+0x205A28 == 1` (`GoInsideCitadel` 0x554004,
    2 during the falling spell video); the widescreen does not touch the ambience (without a citadel interior: never). The
    `fn_00429100` copies 15 floats, so the 15th (the camera's x) falls into `current[0]` (NONE, without a bank: only visible
    in the trace). `ProcessAtmosBanks` 0x428FE0: step 0.04 between 0.1 and 0.8 and 0.02 outside (0 → 1 in 33 turns, 3.3 s),
    group 1 if GAudio+0x190 > −0.6 (double at 0x8C4A08) otherwise 2, `LHAtmosSetBankVolume(trunc(cur·127))`.
    GAudio+0x190 is set by `fn_005E2240(a)` = 2a − 1 with a = clamp(a, 0, 1), from `fn_0064AC30` in
    `GPlayer::ProcessPlayers` every turn: a = (alignment of the **player with the most influence at the camera's
    position**, `MapCoords::CalculateMostInfluentialPlayer` + `GPlayer::GetAlignmentValue`, + 1) / 2; `GAudio::Reset` (on
    clearing the map) leaves it at 0. openblack: `atmos_banks::Alignment()` reads `GameQueries::cameraAlignment`, which
    `ecs::audio_queries` takes from `ecs::effects::alignment::GetInterfaceAlignment()` (Milagros' fn_0064AC30, once
    per turn) with the formula of fn_005E2240; the same x as the target of the sky alignment, so the
    sky test hook and the debug slider also reach it (audio.md, "Fase C: C2").
    Only from turn 6 and not paused (`g_game+0x14 & 4`); otherwise, `AtmosProcess(0)`: the loops stop and **only the
    channels with AtmosInfo** (0x10001EBF: loops = 1, one-shots = their entry), not the other samples. With a video
    (`g_game+0x250188`) `LHAtmosProcess(1)` does not run (openblack has no in-game videos). On changing map
    (`GAudio::Reset`) ambience and samples stop, but the bank volumes are kept.
  - The DLL's mixer (`fn_10001610`, `LHAtmosProcess` 0x100018B0): samples with +0x27C = 0 are 2D **loops** (vol of the
    .sad or 127, fade-in +5 per turn without a cap until reaching its volume, gain bank·fade/127; they are cut
    abruptly when the bank reaches 0); +0x27C = f > 0 are **one-shots** in **a single queue** for all the banks,
    next = counter + 4f + rand·12f/32767, and when registering a bank counter = head − 20. **At most one per
    turn** (the head, if due): it plays if its bank is not at 0, relative to the listener in QMixer (x, y, 0) with x, y =
    2 − rand·4/32767; if |x| + |y| ≤ 1 they are multiplied by 4 and if they end up at (0, 0) they go to 5·(a, b), with a, b = ±1 that
    rotate (a' = −a, b' = −a·b). It is put back in the queue even if it has not played. The one-shots of a group other than
    their bank's go down 5 per turn. In Land1 there are 15 loops and 400 one-shots.
  - **Axes of the relative mode** (verified with Unicorn, `emu_polar.py`): LHaudio converts the relative position (+0x14) to
    polar (0x10012269): azimuth = atan2(x, y) in degrees (with π taken as 1/0.318471 and |ftol| of the negatives),
    elevation = atan(z/|xy|), range |xyz|; QMixer (0x1800AA85) goes back to right = r·cos(el)·sin(az), up =
    r·sin(el), forward = r·cos(el)·cos(az). That is, **x right, y forward, z up**: the one-shots (x, y, 0) lie in the
    horizontal plane around the listener, at 2..7.1 (e.g. (2, 2) ahead to the right, (0, −4) behind), attenuated with
    their mapping (min 1, scale 2 or 4: at (2, 2) with scale 2, 0.215). In openblack: `sample_play::PolarRelative` and the relative
    emitter in OpenAL's listener space (right, up, −forward). Traces: `OPENBLACK_AUDIO_TRACE=1` (channels)
    and `OPENBLACK_ATMOS_TRACE=<n>`. Checked in Land1 (camera at 2120, 40, 2400): the lake waves (lake.sad, pitch
    160 of the .sad) play at AL_PITCH 1.600 at (−1, 1) and (1, 2).

## Moved to other pages

- Trees: rules, fire and sacrifice → [trees.md](trees.md) ([tug](trees.md#tug-handstatetug-enter-0x5b7df0--update-0x5b8070-in-handtreescpp),
  [pick-up rules](trees.md#pick-up-rules-and-bigforest), [fire](trees.md#fire), [sacrifice](trees.md#sacrifice)).
- Creation from CHL → [map-loading.md](map-loading.md#creation-from-chl-create-27--create_with_angle_and_scale-252).
- Map mist (CREATE_MIST) → [map-loading.md](map-loading.md#map-mist-create_mist).
- Animals and herds → [map-loading.md](map-loading.md#animals-and-flocks-create_flock-create_new_animal).
- Map simulation data → [map-loading.md](map-loading.md#map-simulation-data-data-only-nothing-is-drawn).
- Fish farms → [map-loading.md](map-loading.md#fish-farms-create_fish_farm--create_town_fish_farm).
- The missionaries' boat (`PetitNavire`) → [water.md](water.md#the-missionaries-boat-petitnavire).
- Construction percentage of a Feature →
  [map-loading.md](map-loading.md#build-percentage-of-a-feature-built_percentage-chl-property-22).
- Fish puzzle: the script side → [water.md](water.md#fish-puzzle).
- Map script objects →
  [map-loading.md](map-loading.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates).

## Pending

- Taking in batches: the fish particles (`S_Spangle_A`).
- `PileFood::Draw`: confirm that LH3DObject's vfunc 0xE8 is the UV offset (**inferred**).
- Fields: the alignment and the rain in the growth; the villager jobs that sow them (without them the fields
  stay empty except with the `world.crops` mod).
- Sounds: since audio's B4 all of them (picking up, uprooting, planting, crushing, rocks, whistles, piles) go through the 16
  channels as in the original, 2D or 3D according to their place ([audio.md](audio.md#b4-the-worlds-callers-on-the-channels)).

## Test hooks

- `OPENBLACK_HAND_TEST_FIELD=1`: taking from a field.
- `OPENBLACK_HAND_TEST_FISH=1`: taking from a fish farm; traces the pick-up loop (one start, pitch 0.60 → 1.02).
- `OPENBLACK_AUDIO_TRACE=1`: `sample_play` channels (hand in the water, collisions, ambience).
- `OPENBLACK_ATMOS_TRACE=<n>`: the ambience (sound map and banks).
- Trees and map loading: the hooks of [trees.md](trees.md#test-hooks) and
  [map-loading.md](map-loading.md#test-hooks).

## Sources

- `C:\Users\diewgarc\dev\documentacion\field\field_notes.txt` and `draw_colour_sway_notes.txt`: fields.
- `C:\Users\diewgarc\dev\documentacion\sound\notes.txt`: pick-up sounds and LHAudio.
- `C:\Users\diewgarc\dev\documentacion\agua\re\emu_qmixer.py` and `emu_polar.py`: QMixer volume, distances and axes (Unicorn).
- `C:\Users\diewgarc\dev\documentacion\agua\audio.md` §1-3: the ambience.
- `C:\Users\diewgarc\dev\decomp_pickup`: picking up, holding and dropping (interface, CHand, objects).
