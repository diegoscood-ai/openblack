# info.dat

## Summary

`Scripts\info.dat` holds the game's tables: one record per species, spell, tree, villager state, vortex type and so
on. The game loads it once and reads each field from fixed offsets. In memory every record sits 0x10 bytes behind
its place in the file (a vtable and a header come first), so the code reads a field at its file offset + 0x10.

openblack reads the same file into the structures of `src/InfoConstants.h`, field for field, and gives names to the
fields whose readers are known. This page holds the layout of the tables that several systems read; what each system
does with its fields is on that system's page.

## The original

Every table below is read from the frozen `Scripts\info.dat`. The rule "memory = file offset + 0x10" holds for each
of them; the offsets on this page are the file's unless they say "in memory".

#### The creature tables in info.dat

- **`GCreatureInfo`** (one row per species, 0x384 bytes): the 416 bytes that used to be one opaque array are named.
  The species row is at +0x1E4, `startEnergy` +0x1F4, `comfortTemperature` +0x1FC, `growUpMinutes` +0x204,
  `energyDrain` +0x220, `sleepRecover` +0x230, `slowSpeed` / `walkSpeed` / `runSpeed` from +0x240, `runAwayDistance`
  +0x2D8, `sleepLength` +0x364, `pooPerEnergy` +0x370; also `desireFloor` +0x264, `chantsPerEnergy` +0x284,
  `spellEnergyFloor` +0x288, `totemIcon` +0x334 and `alignmentChangePerTurn` +0x338. These are the file's offsets: the
  exe reads each at its offset + 0x10 (checked field by field against info.dat). `lookSweepAngle` is +0x2DC (read by
  fn_004D0F30 at 0x4D0F51 and 0x4D0F72, the look sweep's bound: mind+0x1D3DC += +0x1D3E0 each call and the step is
  negated once the value leaves ±[info+0x2EC]) and `alignmentChangeScale` +0x374 (bw1-decomp `AlignmentChangeScale`,
  memory +0x384; read by `GAlignment::Update(Creature&)` 0x414220 at 0x414275 / 0x4142D9 and
  `CreatureDesires::GetAlignmentChange` 0x4DCFB0 at 0x4DCFDD: the creature's alignment change is the desire's
  `AlignmentChange` × this scale) [V]. +0x234 and +0x24C have no reader through the creature's info pointer
  (Object+0x28).
- **The species' speeds, body rates and casting** (read from the frozen info.dat, 2026-10-26): the 17 rows start at
  file offset 0xBEEC (0xBEC0 past the 0x2C header, the row number at each +0x1E4), 0x384 apart. Besides the fields
  above: `secondsPerAgeTick` +0x200 (an integer, 3600 in every row), `lowEnergyThreshold` +0x20C, `exhaustionRate`
  +0x210, `secondsToDehydrate` +0x214, `strengthDecay` +0x218, `carryStrengthMinutes` +0x21C, `fatBurn` +0x224,
  `overeatFatFactor` +0x228, `sleepHeal` +0x22C. The rates from +0x20C to +0x230 and the casting pair (36000, 0.005) are
  the same in every row; the speeds, `comfortTemperature` and `growUpMinutes` differ by species (Giant Ape 0.5 / 0.7 /
  0.9, 15, 240; Cow 0.4 / 0.6 / 0.8, 11, 200; Tiger 0.5 / 0.8 / 0.95, 15, 260; the Wolf's comfort 0; rows 10 to 16,
  Polar Bear to Gorilla, alike: 0.5 / 0.7 / 0.92, −5, 240 but the Chimp's 220). openblack: tests
  `CreatureInfo.realInfoDatSpeciesSpeedsBodyRatesAndCasting` (skips without the game's data, every row bit for bit) and
  its synthetic twin `CreatureInfo.ASpeciesRowsSpeedsBodyRatesAndCastingAreReadFromWhereTheFileKeepsThem`.
- **`CreatureActionInfo`**: `strengthGain` / `energyCost` / `exhaustionCost` at +0x0 / +0x4 / +0x8, `desire` +0x98,
  `desireMultiplier` +0xC0. `field0xfc` exists only in patch 1.20 (openblack issue #684).
- **`CreatureInitialDesireInfo`**: 0x1C0 bytes in memory, 0x1B0 in the file, which leaves out the 0x10-byte header;
  openblack's `field0xN` is therefore the original's +0x10+N. Read from `info.dat`:
  - +0x10..+0x2C: the 8 sources of the desire;
  - +0x38 (openblack's `field0x28`): learnable;
  - +0x4C (`field0x3c`): holds 0.3 to 3 in the data (hunger and tiredness 2, healing 3); bw1-decomp once called it
    `DesireDecay`; it is the desire's **cap**;
  - +0x50 / +0x54 (`field0x40` / `field0x44`): hold 0.999 / 1 in nearly every row (0.9 / 1 in two); bw1-decomp once
    called them `InitialValueMin` / `InitialValueMax` (its current C: `DecayRateMin` / `DecayRateMax`); one number is
    drawn from this range per desire, and it is the desire's **per-turn decay**
    ([Random streams](../engine/random.md#random-streams));
  - +0x78 (`field0x68`): the weight (bw1-decomp: `DesireGrowthRate`);
  - +0x80 / +0xC0 / +0x100 / +0x140 / +0x180: the name and the desire, short, trying and doing texts.

  bw1-decomp marks the names of +0x4C..+0x54 as fabricated, and the older ones are wrong. The reading "cap = +0x4C,
  decay drawn in [+0x50, +0x54], value 0" is the original's, **read in the exe**:
  - `CreatureDesires::Initialise` 0x4DC100, for each desire i in order: activated (+0x8) from the game flag 0x2000,
    the suppression counter (+0xA8) and the value (+0x148) to 0; then `fld [+0x54]; fsub [+0x50]; fstp` (the range,
    rounded to a float), `GameFloatRand(range)` (call 0x4DC14E), `fadd [+0x50]` and the store to the desires'
    +0x288 + 4i (0x4DC163; the loop's register points 8 bytes into the desires, so the instruction reads
    `fstp [esi + 0x280]`): the drawn number is stored as a float, no `__ftol` (bw1-decomp types the array
    `uint32_t`). Then the increase time (+0x1E8) from the per-species table, `RandomiseIncreaseTime` 0x4DC310 (which
    draws only when [0xD00DEC] is set; it is 1, see [Random streams](../engine/random.md#random-streams)),
    `+0x468 = [+0x4C]` (0x4DC188) and `+0x508 = [+0x78]` (0x4DC191).
  - `CreatureDesires::UpdateDesires` 0x4DC430 shows what they are. For an activated desire, the suppression counter
    goes down by 1 if above 0; then with an increment from the sources above 0 and the counter at 0, `value +=
    increment` (0x4DC495); otherwise `value *= [+0x288]` (0x4DC499): **+0x288 is the per-turn decay**. Then `value <
    [creature info +0x274]` gives that floor (0x4DC4A8), else `value > [+0x468]` gives that cap (0x4DC4C1): **+0x468,
    from +0x4C, is the cap**. The floor is the `GCreatureInfo` field openblack calls `desireFloor` (file offset 0x264):
    0.0 in all 17 species rows of `Scripts\info.dat` and in all 19 of `CreatureIsle\Scripts\info.dat`.
  - `FindWeakestDesire` 0x4DC7B0 compares +0x148 (the value), not +0xA8 as an earlier note said.
  - **Feedback** moves the decay: `ModifyDesireAfterFeedback` 0x4DCA60 does `+0x288 += feedback × 0.001` (0x4DCC41),
    then clamps it to [+0x50, +0x54] of the row (0x4DCC55, 0x4DCC73), the low bound tested first.
  - **A mind file does not carry the decay.** The reader fn_004E84A0 (from `LoadFullyQualified` 0x4E8040 and
    `LoadMindIntoPreexistingCreature` 0x4E8440, both after `Creature::Create`, whose constructor has already run
    `Initialise`) reads per desire: activated (+0x8, 0x4E861D), value (+0x148, 0x4E8625), cap (+0x468, 0x4E8633),
    increase time (+0x1E8, 0x4E8641), the dropped float of versions below 7 and the one of versions 6 to 9, and from
    version 9 the sources (rebuilt, 0x4E870D) and, below 15, a dropped int. It writes nothing to +0x288 or +0x508: the
    decay drawn when the creature was made, and the weight, stay.

#### Where the creature's mind tables sit

The desire tables lie past the end of `.data`'s raw data, so they are zero in the exe and filled at start-up by
`load_variables` 0x42B460 from the file's "Info" block (file 0x2C):

| Table | In memory | Row | File offset |
|---|---|---|---|
| The desires (`DETAIL_CREATURE_DESIRE_TABLE`, loaded at 0x42D1B3) | `0xC67E90` | 0x1C0 (0x1B0 in the file) | `0x64BA4` |
| The desire sources (`DETAIL_CREATURE_DESIRE_SOURCE_TABLE`, 0x42D1CB) | `0xC8AD60` | 0xA4, 61 rows | `0x68F24` |
| `InitialValue` of the sources (0x42D1E6) | `0xC89958` | 17 floats + 0x10 header, 61 rows | `0x6B268` |
| `InitialThreshold` of the sources (0x42D1FE) | `0xC87E98` | as above | `0x6C29C` |
| The increase times per species (`CreatureDesireForType::LoadBinary` 0x42DFA0) | `0xC84FB0` | 0x44, 40 rows | `0x100CC` |

- The desire dependency table (`g_CreatureDesireDependency` 0xC85CD0, 40 rows of 0xB0) is at block offset `0x63278`.
  Only four entries are negative (Compassion and Anger both ways, Sadness to Compassion and to Play); what it does
  when a plan is chosen is on
  [the fizzle shown](../creature/casting.md#the-fizzle-shown) (`SuppressDesiresAfterDecidingOnActivity` 0x4DC210).
- The creature action rows (`CreatureActionInfo`) start at file offset `0x4BD40`, 0x100 bytes apart, row 0
  "Undefined" first; row 22 is "FollowPlayer" and row 23 "CommunicateState".

<a id="infodat-tables-srcmagicmagictables"></a>
### info.dat tables

The magic tables.

- `GMagicInfo*` per MAGIC_TYPE at `0xD37D10` (42), `GMagicEffectInfo[42]` at `0xCC6630` (0x11C in memory),
  `GSpellSeedInfo[30]` at `0xD9D678` (0x190). Each record sits 0x10 bytes behind the file (vtable and header): exe
  offset = file offset + 0x10.
- The loader creates one object per record, one class per section, in MAGIC_TYPE order. The sections in file order
  give exactly 0..41: general 10 (0-9), heal 2, teleport 1, forest 1, food 2, storm/tornado 3, shield 2, wood 1,
  water 2, flock flying 1, flock ground 1, creature 16. Checked against the real info.dat: the `magicType` field of
  each record matches its position.
- `GMagicEffectInfo` in the file: 0x10C-byte rows from file `0x1130` (block offset 0x1104), up to `0x3d28`, where the
  30 `GSpellSeedInfo` rows of 0x180 bytes start. In the row: `costToCreate` +0x68, `useTribalPowerMultiplier` +0xBC
  (9 × u32), the aggressive and defensive flags +0xE0 / +0xE4, `aggressiveAttackValue` +0x104 and
  `computerCastDuration` +0x108. Which spells carry the flags is on
  [the fight's cast](../creature/casting.md#the-fights-cast).
- The GMagicInfo rows live in per-class sections: types 0..9 are magicGeneral (`SlotOf` 0x5FB700).
- The record is read with its class: `GMagicResourceInfo` for food and wood, `GMagicRadiusSpellInfo` for storm and
  shield.
- `timerWhen{OneShot,PlayerCasting,CreatureCasting,ComputerPlayerCasting}` are **float** (seconds, -1 = no limit; the
  getters 0x5FB7A0..0x5FB7D0 do `fld`). MAGIC_TYPE 0 carries the integer 10 there (garbage). Storm: 40 s.
- `GWorshipSiteInfo::chantsToReserveForMaintaining` is a float in the code but the file holds an integer: it is read as
  ~7e-43 (a bug in the original, kept).
- The tail of `GSpellSeedInfo` (file 0xF0..0x17C; the memory offsets below are +0x10): `selectionGesture` (1 SPIRAL
  / 2 INVERSE_SPIRAL), `gesture`, `gestureStage2` (0), `sizingGesture` (file +0xFC, memory +0x10C, which fn_00729AC0 reads: 4 CIRCLE on STORM, SHIELD,
  PHYSICAL_SHIELD; read by fn_00729AC0), `castType` (SPELL_CAST_TYPE, +0x100: `SpellSeed::IsSpellCastInHand`
  0x729820, `ApplyOnlyAfterRecSystem` 0x7286B0), `isKeptInHand` (+0x104: `IsSpellKeptInHand` 0x729840),
  `castOnObject`, `seedFollowsSpell`, `magicTypes[4]` (no PU, PU 0, 1, 2), `powerUpGestures[3]` (+0x124: the gesture
  that powers the seed up to PU 0, 1, 2, 0 = none; `SetupPowerUpGestures` 0x5CEE30, which calls `TrySetupPowerUpGestures` 0x5CEE80), `mesh` (+0x130:
  `SpellSeed::GetMesh` 0x729850), `scale`, `holdLoweringMultiplier` (+0x13C, read at 0x728660), `holdRadius`,
  `holdYRotate` (+0x144, 0x728670), `holdType`, `attachInHandEffectToBone`, `deleteSeedOnceCast` (+0x158:
  `IsThisSeedDeletedOnceCast` 0x7281F0), `holderParticle`, `exists` (+0x16C: 1 = a real spell, 0 = an unused slot;
  fn_004E1E20), `iconIndex`, `tooltip`. No meaning yet: 0x10C, 0x138 (equal to the scale except on the flocks),
  0x150/0x154, 0x15C (0.1), 0x160 (1), 0x168, 0x178, 0x17C (1 on FIRE, LIGHTNING_BOLT, HEAL, WEAK, STRONG).

### Wood and the GTreeInfo table

- Over a store, a tree's wood is `woodValue·scale·GLandBalance[5]` (`Tree::GetDefaultResource` 0x74B7A0, × life). A
  **dead tree** gives less: `DeadTree::GetDefaultResource` 0x511330 = `woodValue·scale` without life or land balance.
- The GTreeInfo table (stride 0x140): wood 800 Oak, 700 Beech/Cedar/Copse, 500 Birch/Olive, 400 Cypress, 350
  Conifer/Pine, 300 palm trees, 100 hedges, 15 bushes; weight 1000 (bushes 20, hedges 100); heat capacity 1000
  (bushes 100, hedges 200); sacrifice 400/500/1000 (Oak)/250/350/100/200/110; combustion temperature 110 for all.
- The GTreeInfo array: base 0xDA3AD8, stride 0x140, filled at 0x749D30; Pine is index 12 (0xDA49D8).
- `BigForest::GetCarriedTreeType` 0x438DE0 reads its GBigForestInfo (+0x28) +0x124; the four GBigForestInfo rows of
  info.dat all have carriedType 3 (file offsets 0x8614C, 0x86264, 0x8637C, 0x86494).
- `BigForest::GetWoodValue` 0x4390B0 = life × +0x84.
- In `DeadTree::Create` (0x510C30) the code does `and word [esi+0x98], 0xFFEF`: it clears bit 0x10 of +0x98 (a word
  of DeadTree's own, bw1-decomp `DeadTree.h` `DeadTreeFlags`), "make the roots". `Tree::EndPhysics` 0x74B830 sets it
  (`ebx = 0x10` at 0x74B846, `or word [esi+0x98], bx` at 0x74BC10, its only setter) on the dead tree it makes through
  fn_00510B70 (0x74BBE2) when a thrown tree lands, and the dead tree's first `DeadTree::Draw` 0x5107F0 (`test byte
  [ebx+0x98], 0x10` at 0x5107F3) then makes a TreeRoots mesh (0x250 = 592) at its position with fn_00826280, links it
  into the list [0xEB9A10] and clears the bit (0x510821). `DeadTree::CallVirtualFunctionsForCreation` 0x510A95 clears
  the low five bits (`and 0xFFE0`) [V].

<a id="infodat-state-table-villagerstateinfoh"></a>
### info.dat state table

The villager states: `GVillagerStateTableInfo::Infos` 0xDB9E68, 0x114 bytes per state; the disassembly reads each
field at the file offset + 0x10. By file offset: the clip (0x00), the served desire and its amount (0x04 / 0x08, read
by AdjustTownModifier), is-final (0x0C), not-stored-as-previous (0x10), is-moving (0x14), the resume state (0x20), the
speed group (0x24), can-pause-for-a-second (0xC4), no-go-home-when-hurt (0xD8), do-periodic-checks (0xE4),
go-home-when-hurt (0xE8), no-out-of-clip (0xF0), goes-back-after-landing (0xF4, see
[Soft drop and landing](../beings/villager-hand-and-death.md#soft-drop-and-landing)), the life drain per turn (0xF8),
and more.

Fields whose readers are known (file offset, with the memory address the code reads):
- 0x18 IsScriptState (0xDB9E90, vt +0x960, read by ExitInScript 0x5ED9E7); 0x1C script-interruptable (0xDB9E94, vt
  +0x964, read by ExitNoChangeState).
- 0xA8 AvailableState, values 1 / 3 / 4: GetVillagerAvailableState 0x751F40 reads it from GetFinalState's row, and
  IsVillagerAvailable tests `& 1`.
- 0xC8 food interest (0xDB9F40) and 0xCC wood interest (0xDB9F44): the state factor of IsInterestedInFoodObject /
  IsInterestedInWoodObject.
- The carried-object column read by SetStateCarriedObject is at 0xDB9F54. The reaction-availability column read by
  IsAvailableForReaction is at 0xDB9F64: memory +0xEC (file 0xDC) and memory +0xFC (file 0xEC) of the row.
- No-go-home-when-hurt (0xD8) is also read by ExitGetFoodAtWorship.

<a id="the-gvortexinfo-rows-infodat-0x8889c-stride-0x30-in-memory-0xd38248--type--0x40-fields-from-0x10"></a>
#### The GVortexInfo rows

The rows start at file offset 0x8889C, stride 0x30; in memory they are at 0xD38248 + type × 0x40, with the fields
from +0x10.

| Type | PreLandscape +0x10 | PostLandscape +0x14 | ObjectMover +0x18 | LightMap +0x1C | initial state +0x20 | fadeWhenDeleted +0x24 | baseScale +0x28 |
|---|---|---|---|---|---|---|---|
| 0 In | 78 SF_LandscapeVortexInBefore | 79 SF_LandscapeVortexInAfter | 76 SF_LandscapeVortexObjectMover | 77 SF_LandscapeVortexLightMap | 2 FadeIn | 0 | 0.28 |
| 1 Out | 80 -> SF_LandscapeVortexInBefore | 81 SF_LandscapeVortexOutAfter | 0 (none) | 77 SF_LandscapeVortexLightMap | 1 Active | 1 | 0.28 |
| 2 Volcano | 82 SF_LandscapeVolcanoBefore | 83 SF_LandscapeVolcanoAfter | 0 (none) | 84 SF_LandscapeVolcanoLightMap | 1 Active | 0 | 0.75 |

- The particle type → file table is filled in fn_0068EA00 (0x68ECDF..0x68ED5A, table [0xD4EBB0] + 4 × type). **80 is
  InBefore again** (the same register, 0x68ED1F): there is no "OutBefore" file.
- SF_LandscapeVolcanoAfter and SF_LandscapeVolcanoLightMap have `InitiallyCreated` all 0, so they make no atom and draw
  nothing.

### Sources

- bw1-decomp: `src/Black/CreatureMentalDesire.cpp` (`CreatureDesires::Initialise`); the rest read in the exe with the
  xref index and checked against the frozen `Scripts\info.dat` and `CreatureIsle\Scripts\info.dat`.

## openblack

**Done**: the whole file is read. `src/InfoConstants.{h,cpp}` holds the structures, field for field; fields whose
reader is unknown keep their `field0x..` names (other code uses them).

- The creature tables: `GCreatureInfo`, `CreatureActionInfo` and `CreatureInitialDesireInfo` as above. The desires
  take the original's reading (cap +0x4C, decay drawn in [+0x50, +0x54], value 0).
- The magic tables: `src/Magic/MagicTables.{h,cpp}`, filled by `load_variables`. `GetMagicInfoAs<T>` returns the
  record with its class. `test_magic_tables` (`MagicTables.recordsInMagicTypeOrder`, on a synthetic info block whose
  records carry their `magicType` as the file does) checks that each record is found at its type's place. The `GSpellSeedInfo` tail carries the names above;
  `SpellSeedArchetype::Create` gives a seed the mesh of its info, as `SpellSeed::GetMesh` 0x729850 does.
- The villager state table: `src/ECS/Villager/VillagerStateInfo.h`, whose `state_info::` accessors name the fields
  (`Clip`, `ServedDesire` / `ServedDesireAmount`, `IsFinal`, `NotStoredAsPrevious`, `IsMoving`, `ResumeState`,
  `SpeedGroup`, `CanPauseForASecond`, `NoGoHomeWhenHurt`, `DoPeriodicChecks`, `GoHomeWhenHurt`, `NoOutOfClip`,
  `GoesBackAfterLanding`, `LifeDrainPerTurn` and the others in the file).
- `GWorshipSiteInfo::chantsToReserveForMaintaining` keeps the original's misread (~7e-43).
- `GCreatureInfo` +0x374 is `float alignmentChangeScale` (`src/InfoConstants.h`, with its offset check). The
  creature's own alignment change per turn (`GAlignment::Update(Creature&)`) is `alignment::ProcessCreatures`
  (`ECS/Effects/Alignment.cpp`, from the creature loop); it reads the balance row's scale, since the load's copy of
  it over the creature row's own is not ported.
- The dead tree's roots: felling (`ecs::FellTree` in `src/ECS/Trees.cpp`) and the script's CREATE
  (`FeatureScriptCommands::CreateDeadTree`) clear the bit as the original; the comment in `FellTree` can name it. Not ported: the thrown tree that lands as a dead tree and grows its roots at the first draw, a port
  row.

### Test hooks

None of its own: the tables are data. The cross-page list is in [Debugging](../project/debugging.md).

## Pending

- **The initial desires**: the meaning of the fields is read (decay drawn in [+0x50, +0x54], cap +0x4C, above). The
  original has no draw-free set-up: a mind file is read into a creature whose desires were already drawn, and keeps
  their decays. Whether the 40 draws fall at the same place on the synchronised stream is followed in
  [Random streams](../engine/random.md#random-streams).
- The unnamed `GCreatureInfo` fields +0x234 and +0x24C: no reader through the creature's info pointer (the
  0x41xxxx hits are Animal code, another info class); an access through another path is not excluded. Still open.
- The `GSpellSeedInfo` fields 0x10C, 0x138, 0x150/0x154, 0x15C, 0x160, 0x168, 0x178 and 0x17C: no meaning yet.
- Not ported: the load's copy of the balance row's alignment change scale over the creature row's (+0x374, above)
  and the thrown tree's roots (above), port rows.
