# Random numbers and streams

## Summary

The original has one random state for the whole game, drawn through four streams: the **synchronised** GRand (the
game logic; in a network game every machine draws the same numbers), the **local** GRand (sounds, help, effects and
drawing that may differ between machines), the C runtime's `rand()` (**CRT**, used by some 3D and weather code), and
the particle systems' **PSys** selection, which points each effect at the synchronised or the local GRand while it
steps. The local, CRT and PSys streams are shared by the game turn and the drawn frame on the one game thread.

openblack reproduces the generators bit for bit (formulas, resolution, seeds and their resets) in one module,
`game_random`, and every draw in the code goes through it on the stream the original uses at that site. The full
sequence of a game does not match the original's yet, because not every call is made in the original's order; the
creature's creation draws and its blink do.

## The original

### Random numbers

The generator and the four streams, as read in the v1.42 `runblack.exe` (with the 1.20 symbols; the patch did not
touch them).

#### The generator

- **`_LHRand` 0x7DB600**: `s = ror32(s·9377 + 0x24DF, 13)`, stored rotated (0x7DB629); it returns `s % n`, an
  **unsigned** remainder (`div` at 0x7DB62B). With n = 0 the original would divide by 0: every caller checks n first.

#### The synchronised and local GRand

The two seeds belong to `GData` (g_game +0x205A30): the synchronised one at +8, the local one at +0xC (g_game
+0x205A3C).

- `GameRand(n)` 0x6DE510 → `GData::Rand` 0x510650: 0 for n = 0 without drawing (0x510693); otherwise LHRand on the
  **synchronised** seed.
- `GameFloatRand(x)` 0x6DE530 → `GData::FloatRand` 0x5106B0: 0 for ±0 and for **NaN**, without drawing (`fcomp 0;
  test ah,0x40` at 0x6DE53C / 0x5106FE); otherwise `(u·x)·k` with u = LHRand(0xFFFF) and k = [0x8D6050] =
  **0x37800080** (about 1/65535). The result has the sign of x and |r| ≤ 65534/65535·|x|.
- `LocalRand(n)` 0x6DE570 (a long, unsigned division) and `LocalFloatRand(x)` 0x6DE590: the same on the **local**
  seed.
- `GameFloatRange(a, b)` 0x5E1CE0 (the callback [0xEEA380] of `GLandAlignement::Open`): `GameFloatRand(b − a) + a`,
  with b − a rounded to a float first (`fstp`).
- The script commands: CHL `RANDOM` (`GScript::Random` 0x6F8DA0) is `ftol(min + GameFloatRand(max − min + 1))`, pushed
  back as a float; `RANDOM_ULONG` (0x6F8E20) is `GameRand(max − min + 1) + min`. Both are on the synchronised stream.

#### The seeds

- `GGame::Init` 0x54F4AF sets both seeds to **0x88F89F** (the local one at 0x54F4B4, the synchronised one at
  0x54F4BA).
- `GData::Reset` 0x510750 sets both to **0**. It runs from `ResetState` 0x5557A0 ← `ClearMap` 0x552BB0 (at 0x552E62)
  ← `GGame::LoadMap` 0x552F40 (at 0x552F4F). `GGame::LoadMap` is called by `GScript::LoadMap` 0x6FB36A (the call; every
  LOAD_MAP after the first land, see [Changing land](../scripts/land-change-and-saving.md#changing-land-load_map)),
  by `GGame::StartPlaygroundGame` 0x555990 (at 0x5559A7: the skirmish game, from `ResetAndStartPlaygroundGame`
  0x54F759) and by two unnamed functions (0x54C990 in sub_54C930, 0x641752 in sub_641570). The old notes called
  0x552F40 "StartPlaygroundGame"; the symbols (xref) name it `GGame::LoadMap`, and `StartPlaygroundGame` is its
  caller. Only the first land of a new campaign starts from 0x88F89F: `Init` chooses by the start-up mode at GGame
  +0x25017C (table 0x54FF60): 1 = new campaign → `GSetup::LoadMapScript` 0x54F7AB without `ClearMap`; 4 = skirmish
  → the playground; 0 = autosave; 2 = load a game.
- A saved game keeps both: `WriteSafe(GData&)` 0x563440 writes them (+8 at 0x56345C, +0xC at 0x563494) and
  `ReadSafe` 0x563620 reads them in the same order.

#### The PSys selection

- `PSysFloatRand` 0x6729B0 and `PSysRand` 0x6729E0 call through the pointers [0xD4E0C0] / [0xD4E0BC].
- Outside an effect's step the pointers hold 0x672990 / 0x6729A0, which return **0** without drawing (set by the
  initialiser 0x672A80).
- `fn_00673340`, the step of each effect, points them by the effect's +0xAC at the synchronised stream (0x672AB0 /
  0x672AF0, through `GameRand`) or the local one (0x672B10 / 0x672B40, through `LocalRand`), and when the step ends
  puts back the 0 functions (0x67349B), **not** the previous ones: the selection does not nest.
- +0xAC is `NET_GAME_TYPE == 1`, the sixth argument of `GJPSysInterface::Create` 0x68F2F0 (`sete` at 0x68F3AE). An
  effect that belongs to a spell (`Spell::InitWithPos`, push 1 at 0x71FF63) and the one-off visuals
  (`GParticleContainer::Create` → fn_0063E410, push 1 at 0x63E436) are synchronised. The others read are local: the
  hand (fn_0046E7B0), MagicTeleport, the flock, the storm, the dispenser, the physical shield (fn_0072CD40, push 0 at
  0x72CDA1), the utilities, the object explosion (fn_006718E0), the town centre (0x69BC31), and more.
- The PSys float is `(u·k)·x` (0x672AD7 / 0x672ADD), multiplied **in another order** than GRand's: from 0x88F89F with
  x = 2π, `GameFloatRand` gives 4.314674377441406 and the PSys one 4.3146748542785645. It has no "0 for 0": with
  x = 0 it still draws.
- `PSysRand(n)` is `s % n`, not `floor(PSysFloatRand(n))`: the first draw from 0x88F89F with n = 10 is 8, where the
  float would give 6.
- `FloatRand(a, b)` 0x6729C0 is `FloatRand(b − a) + a`. `RandR3` 0x6729F0 (`PSysManager::PSysRandR3`) draws x, y, z =
  `FloatRand(2) − 1` in that order, again while `(z² + y²) + x² > 1`; outside a step it would never end (it would get
  −1, −1, −1 for ever), a case the original never reaches.
- Where the PSys runs in the turn and the frame:
  [The PSys in the world](../physics/particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules).

#### The C runtime

- `rand()` 0x7C8837: `s = s·0x343FD + 0x269EC3`, returns `(s >> 16) & 0x7FFF`; `srand` 0x7C882A. `Random(a, b)`
  0x81D180 is `((rand()·k)·(b − a)) + a` with k = [0x9A3700] = **0x38000100**.
- The seed is kept per thread and starts at **1** (`__initptd` 0x7D2323). `srand(time)` is called only in
  fn_005776E0 (when `creature.lhp` is saved), never at start-up.
- The draws spent before the first land: `start_system` → `fn_00835AD0` spends 7168 (snow) + 1792 (rain) CRT draws
  when Weather is on ([The drawn snow](../world/weather.md#the-drawn-snow)), the first of the process; then
  `SetupGameFonts` (0x6433C7) → `FontCache::Init` 0x830750 spends 255 `rand()` (0x83076C): it swaps T[i] with
  T[`rand() % (256 − i)`] for i = 0..254 over the 256-byte table 0xECCD1C. That table is only the glyph cache's hash
  permutation: every read of it is the hash's first probe, so nothing drawn depends on the order (**verified**, see
  [the fonts](../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message)). These are the
  "`rand()` calls spent before" the first sky.
- Call sites: `rand` 48, `Random` 167.

#### Arithmetic

Everything is float, one operation per statement: the game's FPU runs at 24 bits (fn_007DEE00, `and 0xFCFF` at
0x7DEE0D), so every x87 `fmul` / `fadd` rounds as a float operation does
([Floating point](floating-point.md#the-x87-arithmetic)).

#### The PSys noise grid

The grid at 0xD066D8 (fn_00590DF0, from `GGame::InitOneTimeOnly` 0x54F0F4, before `Init`) is 256 values of
`1 − GameFloatRand(2)` drawn from the seed g_game starts with, 0 (the game object is zero-filled by its allocator,
0x4368C4, and nothing draws or sets the seed before; oracle: grid[0] = 0.69552147, grid[255] = 0.25722134): the same
values in every game [V]. GGame is made by `Base::operator new` 0x4366F0 (`start_system` 0x6432B8..0x6432CB), whose
allocator fn_00436870 zero-fills the block (`rep stosd` 0x4368C4, `rep stosb` 0x4368CB). The synced seed is
g+0x205A38: its only earlier accesses are the GData constructor 0x54B284 (other words of the GRand) and `LoadFiles`
0x5503FB; `GGame::Init` sets the 0x88F89F seeds later (0x54F4B4/0x54F4BA). The first four values are 0x3F320DB2,
0x3E90EA90, 0xBE016480, 0xBF420FC2 and the seed after the grid is 0x5B130400 (oracle on 0x590DF0).

### Random streams between turn and frame

Which side of the game draws from which stream:

| Stream | Turn side | Frame / draw side |
|---|---|---|
| Synchronised GRand (`GameRand` 0x6DE510) | the game logic | none found in the draw |
| Local GRand (`LocalRand` 0x6DE570: 115 call sites; `LocalFloatRand` 0x6DE590: 49) | `PhysicsObject::AttemptToAddSoundEvent` 0x6467D1, GGuidance, GSpookyVoices, the PSys local stream fn_00672AA0, `ParticleMistCreator` 0x6AA5E0 | `Tree::Draw` 0x74B135, `Abode::Draw` 0x51615A, the light maps fn_006CA7D0, `ParticleLightMap::DrawAt` 0x67B26A, the HelpSystem's draw, the fire's flames fn_00731AB0 |
| CRT (`rand` 0x7C8837: 48; `Random` 0x81D180: 167) | `SmokyStuff::Create` 0x823D39, LH3DMist, the creation of Reward and TempleLeash (0x464783..0x4647BC: four `Random` per post, 12 per temple) | `GWeather::DrawClouds` 0x83FD05, `Tree::PreDraw` 0x74A805, the camera shake fn_008210C0, the fish fn_00824DA0, `PetitNavire::PostDraw` |
| PSys selection ([0xD4E0C0] / [0xD4E0BC], set per effect step by fn_00673340) | effects stepped in the turn | effects stepped in the draw |

So every stream but the synchronised one is shared by the turn and the draw on the one game thread: running turns
and frames at the same time would change their sequences, and race on the PSys selection
([The engine loop](loop.md#the-original-loop)).

### Random streams

The creature's draws, the most studied user of the streams.

#### The desires at creation

`CreatureDesires::Initialise` 0x4DC100 (M119 0x125EE30; bw1-decomp `src/Black/CreatureMentalDesire.cpp:63`, now
lines 113–139): for each of the 40 desires, in desire order, the **decay rate** is
`DecayRateMin + GameFloatRand(DecayRateMax − DecayRateMin)` (the call at 0x4DC14E, stored in the desires' +0x288
array (`fstp [esi + 0x280]` at 0x4DC163, esi = desires + 8 + 4i; `lea esi, [ebx + 8]` 0x4DC114, `add esi, 4` 0x4DC1B3;
`UpdateDesires` reads the same array, `fld [esi + 0x140]` 0x4DC499 with esi = +0x148, 0x4DC447; bw1-decomp
`IncreaseTime[40]` at 0x1E8 then `DecayRates[40]`) [V]): **40 draws on the synchronised stream**. The level starts at 0 without a draw; the increase time
(the species' value from `CreatureDesireForType::InitialIncreaseTime`, 0x4DC171) is then jittered by
`RandomiseIncreaseTime`. The table fields are in
[The creature tables in info.dat](../formats/info-dat.md#the-creature-tables-in-infodat).

`CreatureDesires::RandomiseIncreaseTime` 0x4DC310 jitters a desire's increase time t: h = (float)(0.2 t) ([0x8AA3AC]
= 0.2), r = `GameFloatRand(h + h)` (0x4DC346), t = (r − h) + t. It runs only when [0xD00DEC] is set, which `GGlobal`'s
constructor does for the whole run
([A creature with no plan](../creature/plans-and-needs.md#a-creature-with-no-plan)); a t of 0 draws nothing.

#### Where the creation draws fall

📼 1.20 only. All on the synchronised stream, in this order, from `Creature::Create` 0x474A20 → the
constructor 0x473B90 → `Creature::Initialise` 0x474350 (single player, the frozen `Scripts\info.dat`, whose creature
rows are 0x384 bytes from file offset 0xBEEC):

1. The Living constructor: `GameRand(startAge >> 1)` (0x5EBF89), age = r + startAge − (startAge >> 2). The start age
   (row +0x124) is **23** for all 17 species, so `GameRand(11)` is drawn: 1 draw.
2. The `CreaturePhysical` constructor 0x4EF2C0 (called at 0x474381): strength (+0xC) = (`GameFloatRand(0.2)`
   (0x4EF3FA) + the species' strength, info +0x12C) − 0.1; fatness, shown and previous (+0x14, +0x18) =
   (`GameFloatRand(0.2)` (0x4EF426) + the start fatness, info +0x200, which is 0.5 for every species) − 0.1: 2 draws,
   always.
3. The `CreatureMental` constructor 0x4D22F0 (0x4743D2) → `CreatureDesires` 0x4DBFF0 → `Initialise` 0x4DC100: for
   each desire in order, its decay `GameFloatRand((float)(max − min)) + min` (0x4DC14E), then
   `RandomiseIncreaseTime` (0x4DC346). Every range and increase time in the data is non-zero: 80 draws.
4. Then `InitialiseSources` 0x4DE270 (called at 0x4DC1E4): for each desire, its 8 source slots in order (id 0x3D,
   no source, is passed over, not a stop), each through `CreatureDesireSource`'s constructor 0x4DE150: value =
   (`GameFloatRand(2a)` (0x4DE1B2) − a) + value, clamped to [0, 1]; threshold = (`GameFloatRand(2b)` (0x4DE21B) − b)
   + threshold, below 0 → 0 (and +8 = 0), above 1 → 1. a and b are the source table's +0xC / +0x10: 0.1 and 0.2 for
   every source but 22 (TirednessFromExhaustion), which has 0.1 and 0.1. 58 sources (8, 10 and 11 belong to no
   desire; Anger's are 6, 9, 7 in that order): 116 draws.
5. `fn_004E2E70` (called at 0x4D23E2): the 9 innate personality values from the row's +0x340..+0x360 (kindness,
   aggression, lethargy, friendliness, communicativeness, then four not named). Each is a clamp to [0, 1] of
   (r + v) − d that draws again at every test it makes: x1; if x1 > 0, x2, and 1 if x2 ≥ 1; otherwise x3, and 0 if
   x3 ≤ 0; otherwise x4 is the value. d = 0.1 (`GameFloatRand(0.2)`, the spread `[0x8AB22C]`) for the first five,
   0.3 (`GameFloatRand(0.6)`, `[0x8AB23C]`) for the sixth, 0.4 (`GameFloatRand(0.8)`, `[0x8C7A44]`) for the last
   three; the draws are made from 28 call sites, 0x4E2E93..0x4E33F6. 2, 3 or 4 draws each, 18 to 36 in all; always
   4 when v ≥ d, so fields 6 to 8 always draw 4 (they are 0.5 for every species).
6. `fn_004F1FF0` (called at 0x4D2408), the `CreaturePreviousActions` constructor: `GameRand(120)` (0x4F201F) for
   each of its 0x148 = 328 entries, one per creature action.
7. `SpreadReaction` 0x6E3F18 draws nothing at creation (the reaction's +0x20 is 0). The receivers of the
   creation's `ReactToCreature` reaction (radius 55, `ReactionInfo[6]`, made at 0x47448F) draw only through their
   states and clips: a villager's setup, priority and turn counts draw nothing, its states draw as they run (146
   GameRand(30), 148's next look GameRand(70), 159 GameRand(10)) and their clips as each is entered
   (`InspectCreatureAnimation` one to three draws, `RespectCreatureAnimation` one); see
   [Villager reactions](../beings/villager-reactions.md#a-creature-reaction-6). The handlers of another creature in
   range can reach `GameRand` at 0x4F1909; the animals' are not read.

Steps 3 to 6 are gated by [0xD00DEC], which is 1. `Create` itself makes 545 to 563 draws (562 when every personality
value draws 4). A creature read from a mind file makes 2 more per saved source while the file is read
(`fn_004E84A0` builds each source again with its constructor at 0x4E870D, file version ≥ 9), before the file's
values replace them.

#### The creature's blink

`LH3DCreature::UpdateTime` 0x481FF1..0x482058 📼 1.20 only: the timer +0x5468 goes down by +0x48B4, which is `UpdateTime`'s
own argument stored as it is at 0x481E33 (the ms the creature lives this turn, `ftol(physical+0x48 × ms per turn)`;
not the size-scaled step AnimTimeInc +0x48BC). At 0 or below (`jg` at 0x482009) the state +0x5470 goes 0 → 1
(200 ms), 1 → 2 (200 ms), and 2 → 0 with `LocalRand(+0x546C) + (+0x546C sar 1)` (0x482028, on the local stream,
g_game +0x205A3C): the one draw of a blink. `LH3DCreature::Init` 0x4800F3..0x480107 starts the timer at 1000 with
the interval 5000 and the state 0. The interval's other writers are the mind's: 5000 at 0x475CCA (fn_00475CC0), 2500
in `LookJustWokenUp` 0x4D1119 and 1250 in `LookAtObjectFlutteringEyelids` 0x4D1220 (+0x546C = 0x4E2 at 0x4D1228,
then `LookAtObjectArgument` 0x4D16D0); the timer and the state are read by `DrawNow` (0x48EDE4..0x48EE22). The
fluttering look has no direct caller: it is the look function of the sub-actions of the creature's social plans
(smile, kiss, talk, dance, sit, joke ... with a friend, wave at an object, be silly with the player), 48 sites in the
`ConstructSubActionsFor` builders (SmileAtFriend 0x4A21F9, 0x4A2284, and sub_509530 0x509618 among them) [V].

It runs once a creature turn, at the end of `Creature::ProcessState` (0x473939), after the mind. Inside `UpdateTime`
it comes after `AddEvilGoodSparkles` (0x481FE5, 11 `LocalRand`, see
[Drawing in the frame](../creature/drawing.md#drawing-in-the-frame)) and `UpdateBlood`, and before the sway spring,
`UpdateLook`, `OperateFace` (no draw) and `StateAction`, whose static, individual and fight actions draw `LocalRand`
too. `ProcessBreath` 0x4907C0, the first call, draws nothing. Oracle, 2026-10-16: `LocalRand(5000)` from 0x88F89F
gives 1878, then 4348 (seeds 0xE6F4C8D6, 0x5BAD6184).

#### The streams in the creature's code

In 0x472000..0x512000 📼 1.20 only: `GameRand` / `GameFloatRand` in the mind, the plans, the sub-actions, mimicry, the
reactions, `BeCutAndScarredByEffect` and `ForceActOnMapCoord`; `LocalRand` only in the 3D body
(`AddEvilGoodSparkles` 11, `fn_004ED250` 3, `DrawNow` 3, `UpdateTime` 1, `StartIndividualAction` 1, and the cast's
`fn_00484310` 1, which also has one `GameRand`).

#### Neighbouring draws the creature must respect

- **The high-detail villager's eye blink is on the CRT stream.** The eyes blink every `Random(1000, 5000)` ms with
  a hold of `Random(100, 200)` ms, a glance target of `Random(-0.25, 0.25)` and a squint of `Random(0, 0.3)`
  (0x88358D..0x8836E2, the blink's draw at 0x88363F;
  [The family in high detail](../beings/animation.md#the-family-in-high-detail-supervillager)). The creature's eyes
  are another object (`LH3DCreature`), on the local stream (above).
- **The mist's frame counter is on the CRT stream.** `LH3DMist`'s constructor 0x7F9560 sets its counter (+0x84) to
  `Random(0, 16) & 15` ([Map mist](../world/haze-and-mist.md#map-mist-create_mist)).
- **The temple's leash draws from the CRT stream when it is created**: four `Random` per post, Random(0, 1),
  Random(0, 2π), Random(0, 2π), Random(0, 15), post by post, 12 per temple
  ([The temple's leash posts](../creature/temple-leash-posts.md#the-temples-leash-posts)).

#### Other draws at start-up and creation

- **The dances' RandomScatter shape** makes 512 synchronised draws once per run at start-up, before the seeds are
  set, so they never reach a land's stream ([Shapes](../beings/dances.md#shapes)) (verified with xref):
  `GData::InitialiseShapes` 0x50F960 has one caller, `LoadFiles` 0x550401, whose one caller is `InitOneTimeOnly`
  0x54F091, called once from `pc_main` 0x641DAA; `GGame::Init` 0x54F3B0 runs only from `StartGame` 0x54C1A4 and sets
  both seeds to 0x88F89F (0x54F4AF..0x54F4BA).
- **An animal's making** draws its age, the Living constructor's age draw and its reaction's spread, in that order
  ([Creation: mesh and scale](../beings/animal-life.md#creation-mesh-and-scale)) (verified with xref): the age is
  `GameRand(40)` + 5 at 0x419C34 (alone) or `GameRand(20)` + 5 at 0x419D47 (a flock), then `fn_00419E00` runs the
  constructors (the Living's `GameRand(startAge >> 1)`).

### Sources

- bw1-decomp: `src/Black/CreatureMentalDesire.cpp` (`CreatureDesires::Initialise`); the rest read in the exe with
  the xref index and, for the blink's numbers, the oracle.

## openblack

`src/Common/GameRandom.{h,cpp}`, namespace `openblack::game_random`, reached through `Locator::gameRandom`. One state
for the whole game, as in the original: the two GRand seeds, the active PSys stream and the CRT seed. Everything is
✅ faithful to the original except what is marked.

- **GRand**: `GameRand`, `GameFloatRand`, `LocalRand`, `LocalFloatRand` and `GameFloatRange` as above; the constants
  are written with `std::bit_cast` of their bits, one float operation per statement.
- **Seeds**: `Init` / `Reset` / `Save` / `Load`. `Game::LoadMap` calls `Init()` on the first load and `Reset()` on
  the others (member `_firstMapLoaded`). openblack has no start-up mode: a script from the Playgrounds folder acts as
  case 4 and gets `Reset()` on its first load too 🔍 inferred. openblack has no saved games, so `Save` / `Load` have
  no caller.
- **PSys** (`game_random::particles`): `particles::StepScope`, opened in `Effect::Step`, does not nest, like the original.
  `particles::Effect(…, NetGameType)` and `manager::Start` / `StartForSpell(…, NetGameType)`, Local by default. `RandR3`
  outside a step returns (−1, −1, −1) once and warns, where the original would loop for ever (approximate; not
  reachable in the original).
- **CRT** (`game_random::crt`): one seed, the game thread's. openblack makes the atmosphere's 8960 draws at the same
  point as the original (`weather::StartAtmos`, from `InitializeEngine`) and, since 2026-10-09, the font cache's 255
  right after (`graphics::GameFont::MakeHashPermutation` over the raw `crt::Rand`, the same swaps, whether or not a
  font is loaded). `InitializeEngine` hands the table to `InitializeGame`, which keeps it with the resources next to
  the font cache (`ResourcesInterface::Get/SetFontHashPermutation`); nothing reads it yet (openblack has no hashed
  glyph cache).
- **The noise grid**: `Particles/Noise.cpp`, from seed 0, as the original (`src/Particles/Noise.cpp:44`); its test
  can pin grid[0..3] and grid[255] above.
- **The fluttering eyelids**: the friend plans' steps carry the fluttering look (`flutterEyelids`,
  `src/Creature/CreatureFriendAgendas.cpp`), which sets the 1250 ms blink interval (`k_FlutterBlinkIntervalMs`,
  `src/Creature/CreatureIdleMind.h:805`, applied in `src/Creature/CreatureIdleMind.cpp:1154-1159`).
- **The animal's making**: the same order; the age (`RandomAge`, `src/ECS/Archetypes/AnimalArchetype.cpp:88-91`) is
  drawn before `MakeAnimal` and `ConstructorAgeDraw` after it (:97). openblack does not draw RandomScatter at start-up,
  which changes no land's stream (a queued fix on the dances page).
- **The script commands**: CHL `RANDOM` and `RANDOM_ULONG` go through the synchronised GRand (`CHLApi.cpp`).
- **Every draw goes through `game_random`** since 2026-10-03, on the stream the original uses at each site (animals,
  trees, fish, fireflies, fields, rocks, fragments, dust, sound and help, clouds, smoke, villagers, the script
  commands…). The first pass migrated Magic (SpellFlock, SpellForest, SpellWater), Worship (FireFlyReward),
  ECS/Weather (Climate, Storms, WeatherThing, StormClouds, Rain), VillagerFire, ECS/Fire/FireGraphic and PSys (Effect,
  Mist, LightMap, Mesh, Gesture, Lightning, Storm, TownBelief, Noise); `villager::GameRand/GameFloatRand/SetRandForTests`
  forward to the module. The old `graphics::lh3d::Random` / `grand_local::*` (src/3D/LH3DRandom) is **removed**: its
  users (CameraShake, MistArchetype, the villagers…) call `game_random` directly (`crt::Random` for `lh3d::Random`,
  `LocalRand` / `LocalFloatRand` for `grand_local::*`). `Locator::rng` (`RandomNumberManagerInterface`) remains only
  for the tests' `TestRng` (test_villager_*, test_camera); no game code uses it.
- **Threads**: no mutex; in Debug an `assert` checks that only the thread that called `Init` / `Reset` draws.
- **The creature's creation**: `creature_creation::Make`, called by `CreatureArchetype::Create` and so by every
  maker, makes steps 2 to 6 above in this order as the creature is made (in the script native that makes it, the
  original's turn), after the Living constructor's age draw (`living::ConstructorAgeDraw`, step 1). The creature takes
  the drawn strength and fatness unless its maker gives it a body (a mind file's, the debug spawner's, a testbed
  scenario's, the editor's copy), and its mind the drawn desires; the personality and previous-action values are kept
  in the mind, but nothing reads them yet.
- **The creature's blink**: `CreatureAnimationSystem::PoseTurn` does the same at the end of the creature turn, after
  the minds and before the sway and the pose, on `game_random::LocalRand` and `creature_turn_pose::LivedMs`; the
  room's copy of the creature blinks the same way. Its numbers are the original's: the first blink after 1000 ms, the
  two closed states 200 ms each, then a wait of `random(5000) + 2500` ms on the local stream.
- **The creature's puke**: its 12 drops are scattered by a private seeded generator of the physiology system
  (`GetPukeDrops` reads them), not by a game stream, so they stay off the references.
- **The creature rule** (decided 2026-10-07): each draw follows the original's stream; a private seeded engine only
  for a draw with no counterpart in the original (visual extras, debug tools); no `random_device`. The state hash
  covers the synchronised, local and CRT seeds, so only a private engine is off the references.

### Porting raffclar's creature draws

In the ported code every one of these draws is passed in by the caller. "His draw" is the call site in his systems at
`ff1e79d1`, not ported yet; "our stream" is what the port must call. In every row his engine is a private
`std::mt19937` seeded from `std::random_device` (his `CreatureAnimationSystem.h:34`, `CreatureMindSystem.h:87`,
`CreatureLocomotionSystem.h:55`, `CreaturePhysiologySystem.h:47`, `CreatureFightSystem.h:93`,
`CreatureSpawner.h:182`), except the cave, which uses `Locator::rng`. None survives the port.

| His draw | Original counterpart | Original stream | Our stream to use | Confidence |
|---|---|---|---|---|
| `CreatureAnimationSystem.cpp:410` eye blink: next open = `random(5000) + 2500` ms (in our `CreatureEyes`: `random(interval) + interval / 2`, interval 5000), one draw per blink, on the frame clock | `LH3DCreature::UpdateTime` 0x482028, once a creature turn by the ms it lives | local (`LocalRand`) | `game_random::LocalRand`, at the end of the creature turn (`PoseTurn`) | **read in the exe** |
| `CreatureMindSystem.cpp:705` `uniform(low, high)` → `creature_desires::Create`, one float per desire (40) | `CreatureDesires::Initialise` 0x4DC100 | synchronised (`GameFloatRand`) | `game_random::GameFloatRange(min, max)` | **checked in bw1-decomp** |
| `CreatureMindLearning.cpp:1117` the same `uniform` in `ClearLearning` | the same function | synchronised | `game_random::GameFloatRange` | **checked in bw1-decomp** |
| `CreatureMindSystem.cpp:702` `random(range)` → `creature_mind::Think`: the idle activity lot, the face, the hang-around direction and distance, the sleep, poo and throw-about places, the need and object-activity choice | not decompiled; every decompiled Living decision draws `GameRand` (`Villager.cpp:1016, 1019`, `VillagerReaction.cpp:180, 259, 326, 805`, `VillagerStates.cpp:737`) | synchronised (turn logic) | `game_random::GameRand(n)` | inferred (strong) |
| `CreatureMindSystem.cpp:914` `ShowFeeling` → `creature_mind::PullFace` | the sub-action's face in `ProcessSubAction` 0x4FF024..0x4FF096: `SetFaceForActionIdle` 0x4C8870's `GameRand(10)`, the only face that draws, then the variety's `GameRand(7)` after every face ([The faces a creature pulls](../creature/pose-and-animation.md#the-faces-a-creature-pulls)) | synchronised (`GameRand`) | `GameRand(n)` | **read in the exe** |
| `CreatureMindSystem.cpp:942` `SitDown` → `creature_mind::SitDown` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1080` `Sleep` → `creature_mind::Sleep` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1117` `Poo` → `creature_mind::Poo` (where it goes) | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:872` `bernoulli(0.5)` in `PlayAction`: whether an action plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureMindLearning.cpp:368` `Random(range)`, used by `creature_plan_actions::Agenda` and `creature_watching::StepMimicry` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindLearning.cpp:373` `Chance()` 0..1, used by `creature_watching::StartMimicry`: drawn at the mimic site, during the turn, only for the player's own creature ([Mimicry](../creature/mimicry.md#mimicry-and-the-town-hooks)) | not decompiled (the entry point 0x4EA900 is read, the draw inside `Creature::MimicPlayer` is not) | synchronised (logic) | `GameFloatRand(1.0f)` | inferred |
| `CreatureLocomotionSystem.cpp:667` run-away distance `random(40) + species runAway` | `Creature::RunAwayFromObjectReaction` named, not decompiled; the villagers' flee draws are `GameRand` (`VillagerReaction.cpp:900, 903`) | synchronised (logic) | `GameRand(40)` | inferred |
| `CreatureLocomotionSystem.cpp:844` fidget choice `random(3)` | not decompiled | synchronised (logic) | `GameRand(3)` | inferred |
| `CreatureLocomotionSystem.cpp:857` `bernoulli(0.5)`: whether the confused fidget (the third choice above) plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreaturePhysiologySystem.cpp:324` the poo's yaw, `uniform_real(0, 2π)`, for the `LumpOfPoo` object | not decompiled; objects created by the turn's logic belong to the synchronised stream | synchronised (logic) | `GameFloatRand(2π)` | inferred |
| `CreatureMindSystem.cpp` `Fart` → `creature_fart::PuffVelocities`: the fart's 15 puffs, three draws each bounded by the cloud's size, z then y then x ([The poo desire's fart](../creature/plans-and-needs.md#the-poo-desires-fart)) | `LH3DCreature::DoStaticAction` 0x4876E4 / 0x4876FC / 0x487714 | synchronised (logic) | `GameFloatRand(s)` | ✅ oracle |
| `CreaturePhysiologySystem.cpp:347-355` the 12 puke drops: two `unit(-1, 1)` and one `green(0.4, 0.9)` each (36 draws) | not decompiled; in the original the puke is not an ECS sprite | unknown | `game_random::particles::FloatRand` inside a `StepScope(Local)` if ported as a particle effect, else a private seeded engine | no counterpart |
| `CreatureFightSystem.cpp:626` `bernoulli(0.5)`: whether the faint, lying and getting-up sequence plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureFightSystem.cpp:894` `bernoulli(0.5)`: a taunt, or (his reading of the game) an action past the end of the list that plays nothing, before a fight's Ready stage | not decompiled | synchronised (logic) | `GameRand(2)` | inferred |
| `CreatureFightSystem.cpp:1073` → `fight::ChooseMove`, `fight::ComputerEndsBlock` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureFightSystem.cpp:1421-1431` an unblocked blow's wound on the skin atlas: u, then v (`:1426`, `:1427`, each `uniform_int(16, 239)` from the texel distribution of `:1424`), then the kind in `fight::WoundKind` (`random(2)`, and `random(3)` more only for a wound of blow type 1), then the column (`random(8)`): 4 or 5 draws per wound, in the order of the designated initialisers | not decompiled | synchronised (kept on the creature) | `GameRand(n)`, the same order and ranges | inferred |
| `CreatureCaveEffects.cpp:202` a cave mist's frame counter, `int(Random(0, 16)) & 0xF` | `LH3DMist` constructor 0x7F9560 | CRT | `game_random::crt::Random(0, 16)` | **checked** |
| `CreatureCaveEffects` the cave's flames, smokes, spray and mists: at set-up 16 CRT draws for the flames, 20 for each of the 18 smokes' particles, 3 for each fire smoke's place, 6 local for each spray, 3 local for each mist; each frame 3 local for each spray's place and 3 CRT for each spray particle starting over | `CreatureRoom::InitEngine` 0x7878E0, `LH3DSmoke::Create` 0x7F8B60, `CreatureRoom::Draw` 0x786908..0x786984, the smoke's step fn_007F8E00 0x7F9086..0x7F90C9 (read 2026-10-22, [the creature room's effects](../interface/temple-interior.md#the-creature-rooms-waterfall-and-effects)) | CRT and local GRand | `game_random::crt::Random`, `game_random::LocalFloatRand` (`CreatureCaveEffects::GameDraws`) | **checked** |
| `CreatureSpawner.cpp:936` the debug spawner's facing | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |
| `CreatureSpawnerAppearance.cpp:163, 171` the debug mark place and wound kind | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |

His desires (`CreatureMindTables.cpp:73-76`) read the table's fields one slot off (`field0x3c` as the maximum,
`field0x40` / `field0x44` as the range): the port takes the original's decay range, as above. His
`CreatureMindLearning.cpp:425` also builds the desires at the middle of the range without a draw; the original always
draws 40 times. His creature cave's mist uses the original's formula exactly; no leash code of his draws at all.

### Test hooks

- `OPENBLACK_TRACE_GAME_RAND`: traces every draw with its call site, taken with `std::source_location` (the original
  logs the stack with `GetCurrentStackString`).

## Pending

- **The order of every draw.** The formulas, resolution and seed cycle are faithful; the sequence of a whole game is
  not, because not every call is made in the original's order.
- **The creation draws**: still missing, each queued as a fix: the draws of the creation reaction's receivers
  (`ReactToCreature`, step 7), the 2 per saved source when a mind file is read, and the readers of the personality and
  previous-action values.
- **The creature's other draws** (mind, plans, mimicry, fidgets, fight, poo): no decompiled function yet, so
  "synchronised" in the table is a rule, not a reading (kept on [the creature's Pending](../creature/overview.md#pending)); each is confirmed as its class is decompiled or read in the
  exe (`xref.py show` on each caller of `GameRand` in 0x472000..0x512000).
- **The puke**: whether it is a particle effect in the original, and with which `NET_GAME_TYPE`. (The cave's smoke,
  spray and mists are not: they are the creature room's own objects, drawn from the C library's and the local streams,
  read 2026-10-22.)
- **The temple leash's CRT draws at creation** (12 per temple): made by no code of ours yet.
- **The start-up mode**: openblack treats a playground script as the skirmish case 🔍 inferred; the campaign's first
  land is the only one that starts from 0x88F89F.
- **Sources of this page that disagree**: an older draft put the creature's eye blink on the CRT stream and the
  desires' draw on their initial value; the exe (the local stream at 0x482028) and bw1-decomp's current C (the decay
  rate, stored at +0x288) settle both, as written above.
