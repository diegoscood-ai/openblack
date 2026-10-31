# The creature

## Summary

The player's creature is the game's centre: a giant animal with a body that grows and changes shape with what it
does, a mind that learns from the player, and plans, spells, fights and a leash. This page holds what the creature is
made of in openblack, the constants that still stand in for unread values, the status panel, the C key and the order
of a creature's turn; each part has its own page (below).

openblack's creature code comes from raffclar's tree (`stack/81-creature-mode` at `ff1e79d1`), ported first as pure
groundwork (formulas, state machines and file formats, unit tested with fakes) and since then wired into the game: the
creature systems are emplaced in `Locator.cpp` and run every turn and every frame through `ecs::creature_loop`
(`ProcessTurn`, `UpdateFrame`, `UpdateLeash`, called from `Game.cpp`), and the player's creature exists on Land 1, made
by the land script's `LOAD_MY_CREATURE` from the default `--creature-file`
([The player's creature](players-creature.md#the-players-creature)). Much of it is still his model rather than a reading of the
original: the constants with no source are in [Stand-ins](#stand-ins-and-unverified-constants) and
[Pending](#pending).

Progress: [creature](../project/progress/INDEX.md) (what our tree does of it, row by row).

The other creature pages: [the body](body.md), [drawing](drawing.md), [pose and animation](pose-and-animation.md),
[the mind](mind.md), [plans and needs](plans-and-needs.md), [casting](casting.md),
[script control](script-control.md), [the player's creature](players-creature.md), [mimicry](mimicry.md),
[fights](fights.md), [hits and the catch](hits-and-catch.md), [the leash](leash.md), [the rope leash](leash-rope.md),
[the temple's leash posts](temple-leash-posts.md) and [tattoos](tattoos.md). The creature's random draws (creation,
blink, the stream of each site) are on [Random streams](../engine/random.md#random-streams).

## The original

### What is ported

Most of the creature classes of `runblack.exe` (`Creature`, `CreatureMental`, `CreatureAction`, `CreatureAgenda`,
`CreatureLook`, `CreatureFace`, `CreatureMimic`, `CreatureRoom`, `LH3DCreature`) have headers in bw1-decomp but no
bodies; what is known of them is read in the exe, function by function, on the pages listed above. The species rows
are in [The creature tables in info.dat](../formats/info-dat.md#the-creature-tables-in-infodat).

#### Audio, status panel and the follow camera

- **Sounds.** A creature's sounds sit on moments of its animations and are looked up by size (the size it is drawn at,
  the pen's in its pen), species, ground and action. `SET_CREATURE_SOUND` is 1 after a reset, and 0 means only the
  local creature's voice is heard ([Voices](../audio/voices.md)). The event modes 1 and 2 are a stop and a release (no
  shipped animation uses them).
- **The status panel**: damage, hunger and tiredness bars, and the hand's reward. The original draws it with
  fn_00517080, from fn_005178D0 in the help text's frame callback right after the fight's panel (0x5CD03C), with values
  set by two writers each frame [V]:
  - `Creature::Draw` 0x517910 (0x51791D..0x5179D7): while the creature's Flags +0x24 & 0x10 is set and it is the local
    interface's interact object (`GetInteractObject` 0x5CED10). The flag is set and cleared only by the locked select's
    start and end packets (fn_005DA950 at 0x5DA99C, fn_005DAA10 at 0x5DAA5A, from `GPacket::ProcessPacket`), so it is
    the hand pressed on the creature and held, through the let-go: not hover, not the right button, not the leash (📼 1.20 only
    for the reading in our hand terms). Damage = 1 − life, hunger = 1 − energy (physical +0x1C), tiredness = exhaustion
    (+0x30), and the reward row, always drawn on this path, from the creature's player's hand's HandStateCreature +0xD8
    while the hand is in that state (else the last value stays).
  - `CameraModeFollow::Update` (0x44C1E5..0x44C23F): any follow camera on a creature, the three rows without the reward
    row, near the top of the screen (y0 H/40, three quarters of the height).
  - HandStateCreature +0xD8 is 0 at `Enter` (0x5B1431), + 0.1f a stroke up to 1 (0x5B2243), − 0.1f × k a slap down to
    −1 with k 1 or 2, doubled while above 0.25 (0x5B1ED4..0x5B1F19), and kept after `Exit` until the next `Enter`. It
    is also the creature's feedback the hand sends (packet 0x59).
  - Hidden in the sequence mode, under an active SetupBox, and while the help system's widescreen is on with its second
    argument (+0x45E8 and +0x45EC, 0x517099..0x5170B1), the one that also turns the interface off (`SetWideScreen`
    0x5C6AD0).
- **The follow camera**: distance 2..1500, least pitch 0.241661, height × 8, pace 2 s easing to 1 s, as in
  [Following](../camera/script-camera.md#following). Headings run from 0 to 2π.
- **The C key and the script's interface level** [V]: the C key is bindable slot 18 (`ZOOMTO_CREATURE`, bit 18).
  Outside the citadel `GGame::ProcessKey` hands the key to `GGame::ProcessBindableKeys` 0x63F830, which maps it to a
  slot with fn_00470A00 (0x63F852). fn_00470A00 walks the 33 slots (stride 0x30C, 0x470A15..0x470A2C) and, for the
  first slot the key matches, asks fn_0046F750 (0x470A3E), the ControlMap's two switches set by
  SET_INTERFACE_INTERACTION. A blocked slot returns -1 (0x470A45 → 0x470A2E), so `DoAction` is never called.
  fn_0046F750 blocks slot 18 in both cases: with switch +0x652C off, slots 3..19 through the byte table 0x46F7A4
  (entry 15, slot 18, is 0 → 0x46F78D, return 1; only entry 2, slot 5, is 1 → 0x46F795); with +0x652C on and +0x6530
  off, slots 17..19 (0x46F783..0x46F78B). Not blocked, `GGame::DoAction` 0x63F940 jumps on slot 18 (byte table
  0x63FF58 → 3, target 0x63F9C7): with a creature (player +0xA64), a `CameraModeFollow` on it is popped
  (`PopViewMode`, 0x63FA3D) and, unless the top mode is already a `CameraModeNew3`, a new `CameraModeNew3` is pushed
  (0x63FA5C..0x63FAAA); or else a new `CameraModeFollow(camera, creature, 1.0, 0, distance < 80)` is pushed
  (0x63FB4B). So while a level turns the camera moves or the realm zooms off (levels 1..8 and 10..15), C neither locks
  on nor lets go. The block is asked only by the control map (fn_0046F750's three callers: IsActionPerformed 0x470AF9
  and the scans 0x470A3E / 0x470A93).

#### The order of a creature's turn

`Creature::ProcessState` 0x472DC0 runs, for each creature in turn:

1. the pen's place follows the temple (0x472E29..0x472E9F);
2. the spells step: the spell state machine `fn_004F48C0` at 0x472EB4, which alone calls the table at 0xBE0380 (wait,
   ease in, hold, wear off). `ProcessSpells` at 0x472EBB only moves the objects held at the hand and mirror bones;
3. the pen's shrink from the size the spells left (`ShrinkDownIfNearCitadel` 0x472F72 → `SetSize` 0x480530, which
   gives every creature's body its size there, kept within 0.05..4 as +0x90, and (size × 15) / rest height as +0x94;
   the walking speeds go by SetSize's own argument). A script's `Creature::SetScale` 0x47B160 calls `SetSize` at once;
4. the autoscale step (`fn_0047DC50` at 0x472FB7);
5. `UpdateLeash` (0x472FC7) and the walk back inside the confinement (0x4730DC..0x473295), then the needs and the
   growth (`UpdateAttributesAsTimePasses` 0x4EF510 at 0x473337), the mind (Look 0x4733A9, the plans, `UpdateDesires`
   0x47364D), the walk, the object actions and the fight (`ProcessSubAction` 0x4737B0), and last the pose
   (`UpdateTime` 0x473939).

The growth step writes only the creature's own size (+0x6C); the drawn size is the one SetSize last got
([The creature's radius](body.md#the-creatures-radius)). The creatures are stepped inside `Living::ProcessLiving`
([villagers.md](../beings/villager-states.md)).

#### Spell reversion

`GScript::CreatureSpellReversion` 0x6F4DD0 (`CREATURE_SPELL_REVERSION`, native 233) POPs the object
(`GetScriptGameThing`), then the flag; "Thing not found!" returns; "Thing not creature!" via `IsCreature` (vtable
+0x34), the write itself guarded by `IsCreature` again; it stores the raw flag at Creature +0x10BC. The flag is set to
1 at creation (`fn_00474130` 0x474239) and read once, in the per-turn spell step `fn_004F48C0` at 0x4F49FB; it is not
in `Creature::Save` 0x4E4EA0 nor `Load` 0x4E5FF0. With the flag 0, when the holding phase is over
(0x4F49FB..0x4F4A1C): phase 0, the miracle's vtable +0x530 called and the pointer cleared; no finish hook, no event
0x1B, no call to the queue release 0x4F4E60 (called only from the normal finish at 0x4F4AA2). So the creature keeps
the size, strength or look the spell gave it, and only a spell that wears off releases the spells waiting for its
kind. No Land 1 or Land 2 script calls the native.

### Stand-ins and unverified constants

Values openblack uses where the original's are not known. Each is a stand-in until it is read in the executable.

- **The idle mind** (`CreatureIdleMind.h`), his constants that stand in for what the original's planner decides (his
  idle choice of the next agenda and its constants were removed on 2026-10-22: a creature with no plan chooses nothing
  of its own, see [A creature with no plan](plans-and-needs.md#a-creature-with-no-plan)):
  `k_MinDesireShown` 0.2 (a desire is shown only when it is over all others), `k_FaintSeconds` 10 (how long a fainted creature lies out cold). His other idle timings
  (`k_ShowDesireSeconds` 60, `k_SitSeconds` 10 + 5, `k_HangAroundDistance` 20, …) have no wiki backing either.
  `k_FaceRepeatSeconds` 4.2 and `k_FaceVarietyStep` 7 are the original's (42 turns between faces and `GameRand(7)`,
  [The faces a creature pulls](pose-and-animation.md#the-faces-a-creature-pulls)).
- **The mind:** his `MiracleMultiplier` table (17 species) and the planner's and learning's constants.
- **The eyes:** first blink 1000 ms, blink 200 ms, wait `random(5000) + 2500` ms: the original's numbers
  ([The creature's blink](../engine/random.md#the-creatures-blink)), on its local stream and the ms the creature lives
  each turn.
- **Locomotion:** no stand-in left: walk 8 and run 20 per (0.875 × size + 0.25) and acceleration 12 (`SetSize`, the
  size SetSize last got: the pen's in the pen), top-speed margin 1.1 (`SetRequiredSpeed` 0x47FA68), slope 0.6
  (0x4912E7) within [0.3, 1.1] (0x491300..0x491319), corner π/6 (0x8CF164 at 0x4913D5).
- **Physiology:** growth slope 8 (at most 2), fat burnt below energy 0.5, youth exhaustion 4 − 0.15 × age, warmth
  threshold 0.6 at 0.025 per degree. (Growth ×3 and the 50 turns of sleep are read:
  [Asleep, sleeping and sitting](plans-and-needs.md#asleep-sleeping-and-sitting).)
- **Spells:** the frozen colour is 0x354F8D, read: `Creature::ProcessState` writes it to the 3D body's +0x98 with the
  ice material +0x90 and the frozen amount +0x94 (0x4738DB..0x473911); how the body is drawn with it is in
  [The frozen look](drawing.md#the-frozen-look). (The #8CC8FF openblack had before was not the exe's.) (Big ×1.8 and
  small ×0.5555556 are read: they apply only with a joint-activity partner, to the partner's drawn size, and are
  ported; see [The size spells](body.md#the-size-spells-the-start-delay-and-the-targets).)
- **Fight:** arena 15 × 2.6 per size, at most 60 (read: fn_00424FB0, on the drawn size), damage 0.05 / 0.03, AI
  chances 16 / 8 / 1, a block ends 1 in 8, a counter 1 in 5.
- **Throw:** gravity 9.81 (to compare with our physics' gravity).
- **Leash:** tied 1.5× within 180..360; pull 0.8; rope 40 nodes at 200 Hz. The hand slack 0.7 × 15 × s + 22 and at
  most 3 × 15 × s + 32 are read (`fn_005E6980` 0x5E69A2, 0x92B360..0x92B36C), s the size SetSize last got.
- **The hand on the creature** (`Creature/CreatureFeedback.h`, `CreatureHandSystem`): the capsules' radius share 0.12
  is openblack's. The slap's height shares `k_FeetBelow` 0.4, `k_WaistBelow` 0.7, `k_SlapAbove` 1.1 and its speeds
  `k_SlapSpeed` 5 and `k_HardSlapSpeed` 9 heights a second are the original's (`HandStateCreature::Update` 0x5B17B0,
  [The creature state and its packets](../hand/states.md#the-creature-state-and-its-packets)), on the drawn
  height (15 × the size it is drawn at, the pen's in its pen, the original's +0x90 × 15); how the speed and the gates
  are measured differs (Pending).
- **Follow camera:** the clear-view search, 32 headings × 8 samples; his key, wheel and clear-view constants.
- **Start scale without the tables:** `CreatureArchetype::StartScale` gives 0.22 when `info.dat` is not loaded (his
  `k_UnknownStartScale`); with the tables it is the species row's `startScale`.
- **Height:** 15 per unit of size, as [engine-math.md](../engine/coordinates-and-angles.md) gives it (`k_CreatureHeightPerScale`); his
  copies are pinned to ours by tests.

### Addresses

The ported code's comments describe behaviour in plain words; the original's names and offsets behind them are kept
here.

| Address or name | What it is | openblack |
|---|---|---|
| `GCreatureInfo` +0x1E4..+0x370 (size 0x384) | the creature species row; offsets in [The creature tables](../formats/info-dat.md#the-creature-tables-in-infodat) | `GCreatureInfo` named fields, `static_assert`s in `InfoConstants.h` |
| `CreatureActionInfo` +0x0 / +0x4 / +0x8 / +0x98 / +0xC0 | strength gain, energy cost, exhaustion cost, desire, desire multiplier | `CreatureActionInfo` named fields |
| `CreatureInitialDesireInfo` +0x4C / +0x50 / +0x54 | `DesireDecay`, `InitialValueMin` / `Max` in bw1-decomp | `creature_mind` tables (`initialMax`, decay range) |
| `CreatureDesires::Initialise` 0x4DC100 | the 40 initial draws | `creature_creation::Make` (draw injected) |
| `CreatureDesires::FindWeakestDesire` 0x4DC7B0 | reads the desire's value array (set to 0 at start) | `creature_desires` |
| `CreatureDesires::RandomiseIncreaseTime` 0x4DC310 | the increase time's jitter under [0xD00DEC] | `creature_creation::Make` |
| `CreatureDesireSource` constructor 0x4DE150 | a new source's value and threshold jitter | `creature_creation::Make` |
| `fn_004E2E70` | the 9 innate personality values, a clamp that draws at each test | `creature_creation::Make` (kept, not yet read) |
| `fn_004F1FF0` | the `CreaturePreviousActions` constructor, 328 × `GameRand(120)` | `creature_creation::Make` (kept, not yet read) |
| `GCreatureInfo` +0x340..+0x360 | the innate personality values | `GCreatureInfo::innateKindness` .. `field0x360` |
| MSVC `rand` LCG 214013 / 2531011, seed 0x913 | the mind file's name cipher | `creaturemind::MindFile` |
| `LH3DMist` constructor 0x7F9560, +0x84 | the mist's frame counter, `Random(0, 16) & 15` | `CreatureCaveEffects` (the cave's four mist domes, `frame_anim::MistStartCounter` on the CRT stream, [Random](../engine/random.md#random-streams)) |
| `LH3DCreature` | the creature's drawn body and eyes | `CreatureBody`, `CreatureEyes` |
| `LH3DCreature` +0x8C / +0x5224 / +0x5228, `fn_0048F5B0`, `SetSize` 0x480530 | the rest height, the bones' reach and the radius ([The creature's radius](body.md#the-creatures-radius)) | `CreatureBodyMetrics`, `creature_cast_moves::BoneReach`, `creature_morph::Reach` / `Radius` |
| `LH3DPrimitive::Create` 0x84AB50 | turns a primitive's blend offset (+0x2C, count +0x28) into a pointer when the count is not 0 | `l3d::L3DFile::GetBlendSpan` |
| `fn_0084AC20` (32-byte vertices), `fn_0084AE00` (40-byte) | the blend loop: `[0xC392A8]` set, then for each blend v[a] += (v[b] − v[a])·w on +0x0, +0x4, +0xC (0x84AC40..0x84ACB2, 0x84AE1C..0x84AE8B), then the clip codes and the projection | `vertex_blend::Towards`, `vs_object` (`USE_MORPH`) |
| `fn_0084A6D0` 0x84A886, `fn_0084C3C0` 0x84C718, `fn_0084D2D0` 0x84D7A5, `fn_0084DAA0` 0x84DF34 | the boned primitive draws that call it, after the per-group bone loop and the light (0x84C3F0..0x84C706) | `Renderer::BindBlendSources` |
| `[0xC392A8]` | the blend switch, 1 at start; `fn_00810720` (vt+0x15C) saves it, sets 0 when its first float argument is above 0 (0x81088F) and restores it (0x810C33) | always on |
| `GScript::CreatureSpellReversion` 0x6F4DD0 | the reversion native ([Spell reversion](#spell-reversion)) | `CreatureSpellReversion` (CHLApi.cpp), `spell_creature::SetReversion` |
| Creature +0x10BC | the reversion flag | `creature_spells::Spells::reversion` |
| `fn_004F48C0` 0x4F49FB..0x4F4A1C | the holding phase over with the flag 0 | `creature_spells::Step`, `Phase::Holding` |

### Sources

- bw1-decomp's creature headers (no bodies) and the v1.42 `runblack.exe` through the xref index.
- raffclar's creature notes and statuses: [creature progress](../project/progress/INDEX.md).

## openblack

Port decisions for the creature, row by row: [Port decisions: creature](../project/port-decisions-creature.md).

**Code rules.** The creature's pure logic stays free of global state and draws no random number itself: every draw is
passed in by its caller, so the systems choose the stream ([Random streams](../engine/random.md#random-streams)). No `random_device`,
clock or file I/O in it; mind files are bytes in and bytes out, and the loader goes through the resource caches. See
[openblack-internals.md](../project/internals.md).

### The files

- `src/InfoConstants.h`: the named creature tables. `src/3D/CreatureBody.{h,cpp}`: mesh names (`ParseMeshName` and
  `GetIdFromMeshName` return `std::optional`), loaded once by the creature-mesh loop in `Game.cpp`.
- `src/3D/SkeletalAnimation.{h,cpp}` (`openblack::skeletal_animation`, beside the unrelated
  `ecs::components::SkeletalAnimation`), with `FromMorph` for the morph parser's animations.
- `components/morph`: his parser, with the header's named fields (`duration`, `looping`, stride, displacement, hair
  groups, sound events) and the creature block that follows the animations in a `.cbn` (action points, leash bone,
  the right eye's bone, eyes, tattoo sites, voice bank, then the numbers the block ends with). The block is read only
  when the header's first field is not 0: in `Data/CTR/hh.HBN` it is 0 (the hand's file), in the `.cbn` files 0x15, so
  the hand reads the same clips as before. `components/rawimage`: headerless `.raw` images.
- **Loaders** (`src/Resources/Loaders.cpp`): the mind loader reads through `Locator::filesystem` and never throws (a
  file that cannot be read leaves `ErrCantOpen`). `LoadCreatureRigs` walks `Data/CTR` at start-up, reads each `.cbn`'s
  `Creature` block into `CreatureRig` (`GetCreatureRigs()`, by `creature::GetRigId`) and preloads the skin meshes each
  rig names into `GetL3DFiles()` under `creature/skins/<name>`; nothing is read later. `Game.cpp` also loads
  `Data/Eyeball.l3d`, `Data/Eyelid.l3d`, `Data/C_Ape_Hair(a).raw` and the skin art, which the creature pass
  (`Graphics/RendererCreature.cpp`) draws.
- **Components and archetype**: the creature's components in `src/ECS/Components/` (`CreatureLocomotion` holds a
  `route_planner::RouteFollower` behind a pointer; `CreatureDrawPose` is ours, the per-frame drawn pose).
  `CreatureArchetype::Create` keeps our rotation (`affine::AngleY`) and creation index, puts the components on, draws
  the body at `DrawnScale` (size × 15 / rest height; the size alone without the mesh), measures its body once
  (`BodyMetrics`, [The creature's radius](body.md#the-creatures-radius)) and publishes `Teleported`. `SnapTurnStart`
  sets a creature's `fromPosition` / `toPosition` and drawn pose, looked up through the const registry. `ObjectMetrics`
  reads a creature's scale from `Creature::size`.
- `src/Creature/`: the pure modules (109 headers on 2026-10-22: body, mind, plans, movement, fight, leash, audio, panel,
  mode and the rest), with the `CreatureMind.h` placeholder the loaders use. `components/creaturemind/`: the mind file library, linked into
  `openblack_lib`. `src/Audio/AnimEffectKeys.h`. `src/Camera/CreatureFollow.{h,cpp}`.
- Tests: one group `test_creature`, 151 files (84 in `test/creature/`, the rest `test/test_creature_*.cpp` and their
  neighbours), about 2,400 tests on 2026-10-22; one integration test (`test_creature_mind_file.cpp`) reads saved minds
  from `OPENBLACK_CREATURE_SAVES` and skips without them, and a few read the game's data (the info tables, the rigs)
  and skip without it.

### The systems

- **Systems** (`src/ECS/Systems/`, one `<X>SystemInterface.h` and one `Implementations/<X>System` each, all reached
  through `Locator`): animation, skin, hair, footprints, physiology (made with the game) and locomotion (made with each
  land). `ecs::creature_loop` calls them in the turn and the frame; no registry signal is connected.
- **Draw-only data and walkable land**: the puke's 12 drops are kept by the physiology system as draw-only data
  (`GetPukeDrops`), no entity, scattered by a private seeded generator. Locomotion tests a destination with the
  `land_avoid` mask (`IsPosValid` at 7.1, and 7.05 while walking).
- **The other systems**: fight, leash, creature hand, mind (with its learning) and object actions, made with the game;
  `ecs::creature_loop` runs all but the creature hand, which the hand and the debug spawner reach. How each differs
  from raffclar's (the turn length from `game_clock`, the draws on `GameRand` / `GameFloatRand`, the walk on our
  `RouteFollower`, the fight stepped by the turn, the leash's keys and packets, the object actions by the turn) is in
  [Port decisions: creature](../project/port-decisions-creature.md).
- **The order of a creature's turn** (`ecs::creature_loop::ProcessTurn`), as the original's (above): the pen follows
  the temple (`FollowTemplePens`); the spells (`spell_creature::ProcessTurn`); the pen's shrink (`ShrinkInPens`); the
  autoscale (`Autoscale`); then the needs and the growth (`CreaturePhysiologySystem::ProcessTurn`), the leash, the
  mind, the walk, the object actions and the fight, and last the pose. At step 3 every creature's body is given its
  size (`player_creature::GiveBodySize`, the pure `creature_morph::ApplySize`, into `components::CreatureDrawPose`): the
  size given (the pen's ramp, else `Creature::size`, after the spells), kept within 0.05..4, and its scale
  (size x 15) / rest height. A script's `SetScale` gives it at once; a creature is given its size as it is made.
  `Transform::scale` stays the creature's own size's scale, so the state hash keeps it as before;
  `creature_pose::DrawnSize` and `DrawnScale` read the body's. So the size the pose and the animation step read is the
  spells' and the pen's of this turn, without this turn's autoscale or growth, which reach it the turn after, and a size
  spell's 2.4 is drawn although the growth keeps `Creature::size` at 2 or below; a freeze pauses the mind and stops the
  walk the turn it starts, and the mood spells' desires are set before the mind's update. Not the original's yet: the
  leash runs after the needs and the growth, and ours goes system by system rather than creature by creature (two
  creatures only); see [Pending](#pending).
- **Creature audio** (`CreatureAudio` with `src/Audio/AnimEffectKeys.h`; `CreatureAudioSystem`, made with the game,
  updated each frame by `ecs::creature_loop`): its animations' moments play through `Locator::audio`'s
  `PlayAnimationEffect` with the creature as a tracked owner, from the creature bank (`SfxBank::Creature`) or the
  species' own (`CreatureBank` of `VoiceBankStem`, which builds `audio/sfx/creature/{}.sad`), at the distance from the
  listener; the ground is `ecs::sea_cells::GetSurfaceType`, not his surface rules. The event modes 1 and 2 map to our
  Stop and Release. The bank's own filters stand for his temple and wide-screen gates. Whether other players'
  creatures speak is the script's `creatureSound`. Each sound kept on the creature notes its bank and the channel it
  started on (none when filtered out or not started). The footsteps still leave prints.
- **The status panel** (`CreatureStatusPanel`): `creature_panel_choice::Choose` picks the creature the hand holds
  (`CreatureHandSystemInterface::GetCreature`) with the reward, else the one Creature Mode follows without it, and
  `Game::UpdateHandInterface` hands `creature_panel::FromNeeds` to the interface, not in the temple nor while a
  script's wide screen bars are on (`creature_panel_choice::HiddenByWideScreen` on the help system's wide screen and
  its owner, the original's gate, [fights.md](fights.md); it used to test the interface's active switch, which hand
  playback and dialogue boxes also change). The sequence mode and an active SetupBox, which hide it too in the
  original, are not tested yet. The `k_Trigger = Hover` rule of
  `CreatureStatusPanel.h` is not used. `GetLastFeedbackSum` is the hand's reward value (HandStateCreature +0xD8).
- **Creature Mode** (`CreatureMode`, `src/Camera/CreatureFollow`): C locks onto the player's creature; the double
  click is our `DOUBLE_CLICK`. The follow camera is built on the script-camera follow functions
  (`HeadingAndPitchFromPoints`, `PointFromDistanceHeadingAndPitch`, `FollowSeconds`, `ClampFollowDistance`,
  `FollowPitch`, `ThingViewingDistance`). The C key is `BindableActionMap::ZOOM_TO_CREATURE`; the interface level's
  block is applied once, in the action map: `help::interface_interaction::BlockedActions()` is handed to it, and
  `CreatureModeSystem::ReadKeys` reads C as not held. A test holds C back under levels 13
  (JUST_GRAB_DOUBLE_CLICK_AND_ROTATE) and 8 (JUST_HAND_MOVE), both before and after the lock-on, and lets it through
  again under level 0.
- **Creature spells** (`src/Magic/Spells/SpellCreature.{h,cpp}`): the creature miracles (MAGIC_TYPE 26..41) are our
  spell class tree's `SpellClass::Creature`, registered last. Its operations are the plain spell's but for two: cast on
  an object it is the plain cast, then, on a creature, `spell_creature::Receive` (held for the spell's time times the
  caster's tribal power, then the spell runs with no time limit until the creature lets it go; a spell of the same kind
  it replaces is closed down); its close-down is the plain one, then no creature's spell points at it any more. A
  creature miracle cast at a place does exactly what it did before as a plain spell; only the "class not ported"
  warning is gone. `spell_creature::ProcessTurn` moves each creature's spells on by a turn (creatures in entity order,
  through the const registry), applies each event through the pure `creature_spells::Apply` (the body value pulled,
  the freeze and fizz, the mind paused, the desire made dominant then least) and plays the sound action from the
  creature bank. It runs first in each creature turn. His `MagicSystem` creature methods are not taken.
- **Spell reversion** (`CREATURE_SPELL_REVERSION`): `creature_spells::Spells::reversion` (true unless a script turns it
  off; his field and name) and `spell_creature::SetReversion`, called by the native, which POPs the creature, then the
  flag; no thing logs "Thing not found!", anything but a creature "Thing not creature!" and nothing changes (a
  creature with no spell on it yet is given its spells' component to keep the flag). Without reversion a spell that has
  held its time goes straight to off, as the original. Unlike his version, such a spell makes no way for the spells
  waiting for its kind. Dormant on Land 1 and Land 2.
- **The creature as a spell target**: `fire::traits::IsCreature` is true for a creature that is still there (the
  mourners pass on fire starters that may be gone); `PhysicsObjects::ObjectInfo` gives a creature its species' row, so
  it has the row's burn multiplier, defence multipliers and weight like any object. Out of a fight a spell's damage
  and a fire's burn come off its body's life, and a kill faints a creature that can die (`effects::ApplyEffect`'s
  creature case in `src/ECS/Effects/EffectValues.cpp`, d111caf72; the fire's common `ReduceLife` in
  `src/ECS/Fire/FireObjectTraits.cpp`); the fight's part, its cuts and scars and the fight health lost to a fire wait
  (Pending). Explosions and tornadoes already leave creatures alone (`CanBeDestroyedBySpell`).
- **Throws**: what a creature lets go of goes into our physics objects, never his own flight. Thrown at a target, it is
  `PhysicsObjects::AddObject` with the creature as the thrower; put down, dropped, tossed aside or let go of by a
  creature that is gone, it starts as the hand's objects do (`from_hand::InitialisePhysicsFromHand`), but is thrown
  above a speed across the ground of 1 rather than 2 (`hand_grab::IsThrow`, [physics.md](../physics/physics.md)). An object no
  body can be made for is put down where it is. `ecs::creature_physics` gives the creature its body when the physics
  wants one (a ball as tall as the creature, six points and eight faces, mass 1000, not moved by a hit) and its weight;
  the game's physics class hooks (`ecs::PhysicsGameHooks`) call it, set from `Locator.cpp` with the other game
  services; the same hooks offer a thing starting to fly to the creatures to catch
  ([The creature's catch](hits-and-catch.md#the-creatures-catch)).
- **Mimicry and the town hooks** (`src/Creature/CreatureDeeds.h`, `src/ECS/CreatureMimic.{h,cpp}`,
  `src/ECS/Events/CreatureMimicEvents.h`): the deed sites publish the player's deed, which the mind system takes
  through `PlayerDid` (`PlayerCastDeed` for a miracle), for the player's own creatures only; three villager sites
  publish the town need a creature may share, which the mind system takes through `EmpathiseWithTownDesire`
  ([Mimicry and the town hooks](mimicry.md#mimicry-and-the-town-hooks)).
- Ported since: the creature and leash passes of the renderer (`Graphics/RendererCreature.cpp`,
  `RendererLeash.cpp`, `RendererLeashPosts.cpp`), the debug spawner (`src/Debug/CreatureSpawner*.cpp`), Creature
  Mode's system (`CreatureModeSystemInterface`) and the Creature Cave (`CreatureCaveSystemInterface`). The mind
  interface's casting methods (`TryMiracle`, `TellCast`) do nothing only as the interface's defaults, for the tests'
  fakes; the mind system carries them out (`CreatureMindCasting.cpp`, [casting](casting.md)). His fight interface's
  additions are in [Fights: miracles, a tornado and fainting](fights.md#fights-miracles-a-tornado-and-fainting):
  `ForceFaint` is called by the tornado (`TornadoSystem::CatchCreature`) and by an effect's kill
  (`src/ECS/Effects/EffectValues.cpp`); `IsBlocking` and `Recoil` wait for the miracles' side.

### Test hooks

- The debug windows: the selected creature's body panel ([the body's test hooks](body.md#test-hooks)), the spells and
  the debug spawner; the leash's `Shake` is for the debug windows and the hand demos only.
- `OPENBLACK_CREATURE_SAVES`: the folder of saved minds the integration test reads.

## Pending

**The turn and the body**

- **The order of a creature's turn**: the original's `UpdateLeash` 0x472FC7 and the walk back (0x4730DC..0x473295)
  come before its physical turn 0x473337; openblack runs the leash after the needs and the growth. Queued as a fix.
- **The creature in the turn**: the original runs it inside `Living::ProcessLiving` per creature
  ([villagers.md](../beings/villager-states.md)); ours as one block after the living list. Queued as a fix.
- **The alignment and strength the body shows**: the original writes the targets once a turn in
  `Update3DCreatureFromAttributes` and at once in `SetStrength` 0x4F0100 and `ModifyStrength` 0x4F0170, and steps the
  alignment in `GAlignment::Process` 0x414140; queued as a fix. (The script's ALIGNMENT SET, 0x70EF22 → 0x618C40, is
  followed: [GET_PROPERTY / SET_PROPERTY on a creature](players-creature.md).)
- **The shown fatness at load**: the mind file's phys+0x18 (previous fatness) is where the original's shown fatness
  starts; queued as a fix.
- **The size at load and the drawn radius**: whether the original clamps a creature file's size to 0.05..4 as it reads
  it (ours does, `CreatureMindFileBody.cpp`), and whether the radius scale by own size over drawn size
  (`ObjectMetrics.cpp`) is the original's now that a creature can be made above 4, are not read.
- **How the original picks a species' body meshes**: it does not walk `Data/CreatureMesh` as we do; our skip of names
  whose last part is not an appearance is a port decision.
- **The creature's row in the map cells' type** (`map_cells::TypeOf`) is now the species' row, inferred.
- **The creature info's offsets that the data cannot settle**: `startWarmth` (+0x1F8) and `desireFloor` (+0x264)
  hold 0 in every row of `info.dat`, so only the exe's reads place them, not the values. The info tables with no exe
  read cited yet (the climate, help and dance infos) are not checked against the file+0x10 rule.
- **Every constant in [Stand-ins](#stand-ins-and-unverified-constants)**, and the four unread blocks of `GCreatureInfo`.

**Random draws**

- **The mind, plans, mimicry, fidgets, fight and poo draws**: no decompiled function, so "synchronised" is a rule, not a
  reading. In the original's creature code `GameRand` and `GameFloatRand` serve the mind, plans, sub-actions, mimicry
  and reactions, and `LocalRand` only the 3D body; each of our draws is still to be matched to its site and confirmed
  as the classes are decompiled.
- **The puke and the cave's smoke, spray and mist dome**: no puke or cave particle routine is named in the exe (the only
  "Puke" string is an action name); whether they are particle effects in the original, and with which
  `NET_GAME_TYPE`, is not known.

**Walking, water and things**

- **Route planning and walkable land**: his A* lattice is replaced by our route follower and `land_avoid` mask; his
  heading convention against the original's (3D angle + π/2, [engine-math.md](../engine/coordinates-and-angles.md)) is not traced.
- **The creature's route** (`RouteFollower::SetDest`): the two distances are the original's, the arrival ring's inner
  radius then its outer one (`StartMovingToPoint`, [the walk's two distances](pose-and-animation.md#the-original);
  `CreatureLocomotionSystem::StartMove`, e91776111; before, they were swapped). The rest is not known: the port passes
  the creature's radius, (distance + 1) × 5 as the longest route (the footpaths' factor), with 64 search turns a game turn and no obstacle hook.
  Ours now has the original's ([routes](../beings/routes.md#the-creatures-walk-on-the-follower)); his later #988 shares
  the original's 32 search turns a turn among the searching creatures, taken into ours.
  When the route ends with a re-plan in flight, or the destination is inside an obstacle, the creature stands.
- **Water for a creature**: one search, the drinking action's (`ConstructSubActionsForDrinkFromTheSea` 0x4A15F0:
  `Creature::FindNearbyWaterPoint` 0x47D860 within 1000, else the exploration map's coast and sea), which the
  planner, the forced plans and the older needs path all take ([plans and needs](plans-and-needs.md)).
  `GUtils::FindNearestDrinkingWater` 0x74E3A0 serves only the abodes and the tiger's lair.
- **Surface rules**: his snow at depth 27 and material type 0 extras, kept out of the shared `GetSurfaceType`.
- **Footprints on April Fools' day**: when the original reads the date is not read; his code read it at every print,
  the port once per land.
- **What a creature does to things** ⚠️ approximate: the force of a creature's blow on a home (his 3.75 × size is
  dropped for the thrown-object damage), whether an eaten villager dies with a reason the town counts, and where the
  original takes a creature's palm. A dead tree it walks round only while it burns and no script controls it.
- **What a creature carries**: the carried thing's weight is `Object::GetWeight` 0x638480 (scale³ × the info's
  weight, every object, `DeadTree` and `Reward` their own); ours is the info weight alone and nothing for a rock. The
  carry's strength step divides by minutes × the constant 600 (0x8AB5F4), as ours does (`k_CarryTurnsPerMinute`).
  The weight is queued as a fix.
- **What a creature throws**: the thrower's player for the flying-object reaction (the hand's start gives the hand's)
  is not read.
- **A creature's body in the physics**: the original's is its bounding sphere with one point for each bone, its mass
  read from the body; the port's is a ball as tall as the creature, mass 1000; queued as a fix in the physics. Whether
  a creature's put-down goes through the hand's start of physics is not read.

**Spells and fights**

- **Spells on a creature**: where the turn calls the creature spells (from the creature's turn, approximate); a
  creature's own damage: `BeCutAndScarredByEffect` 0x478890 and the casting creature's opinion
  (`UpdateHowNiceIsCreature` every 600 turns) are queued as fixes; `Creature::ApplyEffect` in a fight is ported
  ([fights](fights.md#miracles-blocking-and-fainting));
  a tornado carrying a creature, the three flames on a burning
  creature (their draw stream not read) and the town's magic stolen by a creature wait on the miracles' side.
- **Spell reversion**: without reversion, a spell waiting for its kind stays in the queue until some other spell wears
  off; whether a later path releases it is not traced.
- **The fight by the turn**: a blow's clock moves 100 ms at a time, as in his code; the original's fight states step by
  AnimTimeInc +0x48BC, and how often `UpdateTime` runs for a fighter is the open fade gate ([0xD00DE4]).
- **Fights**: the wound type per blow (his guess) and the tier-1 AI gap he notes.

**The hand, the leash and the camera**

- **The hand on a creature**: allies (`IsAllied` 0x64D5D0) are ported (a72d124d9: the hand takes hold of an allied
  player's creature as of its own, `creature_hand::IsFriendlyTo` over `ecs::alliances`); Flags bit 0x400 and mind +0xF60 / +0xFB4
  in `ValidForLockedSelectProcess` (openblack's asleep and frozen stand for the mind ones; the bit is the script's
  control); `GoIntoStateOfFinishingAction` (0x477B20), `ReconnectToGame` (0x480730) and
  `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476ED0) wait on the 3D creature's states. The click threshold (his
  1000 ms against our 225 ms grab press); the 0xE85 interact tooltip's owner rule (his: own awake creature only;
  check 0x5D7406).
- **The leash's grab-start tap on a thing that can't go into the hand**: with the leash on and untied the original
  sends such a press to `StartGrab` 0x5D1740, so a quick release taps it (the exact branch not traced); queued as a fix.
- **The rope leash forgotten**: whether forgetting the rope leash takes a worn leash off in the original (ours
  `SetKnown(Rope, false)` does) is not read; the writers of +0x1110 are 0x474317, 0x558B41, 0x564D34, 0x63DA72,
  0x65BBB1 and 0x6FC1EA.
- **The playground flag**: the leash picker's square wave also needs g_game +0x205A0C clear (not a playground game);
  openblack has no such flag and passes false.
- **Double click on a creature (the lock-on) and the camera help.** In `CameraModeNew3::Update` 0x45A960 the lock-on is
  part of the double-click flight: the branch needs camera feature 0x10 (0x45DDA3) and clears the interface's
  double-click flag (0x45DDE9). With a thing under the hand (`GInterface+0x3C8`) it first reports
  `CameraHelpCallback(0x306 DoubleClickObject)` (0x45DE3A, help event 31), then asks `IsCreature` (vt+0x34, 0x45DE4B),
  and after the flight's set-up builds `CameraModeFollow(camera, thing, 1.0, 0, 0)` (ctor 0x44B800, at 0x45E34B); a
  fight arena in range starts `StartFight` 0x45A4D0 instead (0x45E07E clears the follow). openblack's Creature Mode
  locks on only with feature 0x10, as the original. The world camera's double click flies to the thing under the hand
  and reports 0x306 (event 31), else 0x305 (event 30) on the land
  ([Player camera features](../camera/player-camera.md#player-camera-features-camerahelp)); on a creature it does not yet go on
  to follow it. A fight arena under the point starts watching that fight, as the original
  (`fight_orbit::ArenaAtPoint` in `DefaultWorldCameraModel`).

**Minds, towns and scripts**

- **The town hooks**: the mind's side of the empathy (0x4C80F0) has its clamped add and its ×0.9995 decay per turn
  (`creature_perceived_desires::IncreaseTown` and `Fade`, tests `PerceivedDesires.*`), and
  `Town::UpdateAttitudeToCreature` 0x7437F0 is step 16 of the town's turn (`town_creature_attitude::UpdateAttitudes`,
  tests `TownCreatureAttitude.*`); the even-turn history is not kept (it is read only by the mind's save). Not read: who reads the
  town's attitude beyond the readers listed, the worship-site calls (states 171, 9 and 7) and the spell's
  (`fn_00721730`), the help-sample groups 0xD99D08 and 0xD99DA0 of the aggressor guidance, and readers of the
  town-desire empathy array through registers or pointers (only the immediates 0x18C80, 0x18D20, 0x18DDC and 0x1A1DC
  were scanned).
- **`CREATURE_FORCE_FRIENDS`**: (a) one reader in ours: a creature's anger is never about a creature in its list or one
  that lists it (`AreFriends` in `CreatureMindLearning.cpp`, through the desire a creature most wants about another,
  which the friendliness test of two creatures reads); its readers in the original (a friend cannot be attacked,
  `CanBeAttackedByCreature`; the activity choice in `GetActivityObjectUsefulness`) are not ported, and what the list at
  +0x12A0 that `CanBeAttackedByCreature` tests first holds is not read. (b) The original drops a deleted friend in
  `RemoveBeliefsAboutDeletedObjects`; ours keeps the entity of a destroyed creature in the list (the anger test never
  meets it). (c) Whether the list is saved with the game or the creature is not read; ours saves it nowhere. (a) and (b)
  are queued as fixes.
- **Saving a mind as the creature itself** (`SaveMind`): the curse rule's size is
  `OriginalSizeOfMyCreature × 0.06666667` ([0x8C9D38]) on the x87, ours a division by 15, which can differ in the last
  bit; queued as a fix. openblack saves the player's creature's mind at a land change and as the creature is
  removed (`player_creature::SaveCreature`), so the rule is reached when the curse script runs then.
- **The Creature Cave without a creature.** openblack's creature room shows its four scrolls blank and opens no cave
  screen while the player has no creature (as raffclar's port); what the original's cave shows in that state has not
  been checked.
- **A land load from a script while inside the temple**: inside, the citadel's pause runs only the temple help and
  temple special scripts, and no shipped `LOAD_MAP` is in one; only a `LOAD_MAP` in the very turn the entrance is
  tapped can run with the player inside, and what the original does then is not read
  ([land change](../scripts/land-change-and-saving.md#load-map-inside-the-temple)); openblack leaves the temple first
  whatever the path (`Game::PrepareNewLand`).
- **Creature in temple**: the script native `SET_CREATURE_IN_TEMPLE` (0x6FF8D1, the only writer of
  `CreatureRoom::CreatureInTemple` [0xD96270] besides the start's 1 and the save/load) sets
  `CreatureCaveSystem::SetCreatureShown`, read where the creature room makes its copy (raffclar #349, unverified
  against v1.42); not saved with the game yet.

**Code the original lacks**

- The removal of the creature behaviour the original does not have
  ([not in the original](../project/not-in-the-original.md)), each its own measured commit. Done: the old idle
  stand-ins and their debug switch (e51b27ed1), planning every desire at once (7a52bd21f), the leash's tug
  (ea7895309); Watch the telly is now a forced-only poo, as the original's (764e131fa), and Look down a cliff fails
  as the original's, with no executor (da83d404f). Still to do: the four mood faces, the pen tables with calling the
  creature home, and the show-desire timer (`showDesireSeconds`, `k_MinDesireShown`, still read in
  `CreatureMindLearning.cpp`: the original weighs showing its state like any desire).
