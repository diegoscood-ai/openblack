# Magic: the core of the miracles

Core of the original's magic system and of its port: the info.dat tables, the spell life cycle, the chants, events and
effects, the cast rules, seeds and one-off miracles, casting from the hand and gestures, worship and prayer power,
influence, alignment, reactions, life, the fire model, the order within the turn and the hooks. Each miracle has its own
section in [miracles.md](miracles.md); the particle engine (PSys) is in [particles.md](particles.md), and time and weather
in [day-night-weather.md](day-night-weather.md#weather-and-climate-m6a-srcecsweather).

Plan and reports: `dev\documentacion\miracles\` (`PLAN.md` and the reports it cites: `core.md`, `casting.md`, `sources.md`,
`destructive.md`, `resources.md`, `protect_creature.md`, `visuals_sound.md`). W120 addresses. This page collects what was
verified in `runblack.exe` while porting it, by milestone.

- [info.dat tables](#infodat-tables-m0-srcmagicmagictables)
- [Spell core](#spell-core-m1-srcmagiccore-srcmagicspells-srcecseffects)
- [Casting from the hand, gestures and hand effects](#casting-from-the-hand-gestures-and-hand-effects-m2-srcmagicgestures-srcmagichand-handspellseedcpp)
- [Worship: where miracles come from](#worship-where-miracles-come-from-m7-srcworship-ecssystemsimplementationsvillagerworship)
- [Influence](#influence-m1i-srcecsinfluence)
- [Player alignment](#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsplayeralignment)
- [Reactions](#reactions-ecseffectsreactions)
- [Object life](#object-life-m0-srcecslife)
- [Fire](#fire-m5-srcecsfire)
- [Time and weather](#time-and-weather)
- [Miracles one by one](#miracles-one-by-one)
- [Wave 2 review](#wave-2-review-lane-review2-m2-m3-m5-m6a-m7-together)
- [Audited assumptions](#audited-assumptions-2026-10-01)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## info.dat tables (M0, `src/Magic/MagicTables`)

- `GMagicInfo*` per MAGIC_TYPE at `0xD37D10` (42), `GMagicEffectInfo[42]` at `0xCC6630` (0x11C in memory),
  `GSpellSeedInfo[30]` at `0xD9D678` (0x190). In memory each record sits 0x10 bytes behind the file (vtable and
  header): exe offset = file offset + 0x10.
- `load_variables` creates one object per record, one class per section, in MAGIC_TYPE order. The sections in file
  order give exactly 0..41: general 10 (0-9), heal 2, teleport 1, forest 1, food 2, storm/tornado 3, shield 2, wood 1,
  water 2, flock flying 1, flock ground 1, creature 16. Checked against the real info.dat: the `magicType` field of each
  record matches its position (`test_magic_tables`, `realInfoDat`).
- `GetMagicInfoAs<T>` returns the record with its class (`GMagicResourceInfo` works for food and wood,
  `GMagicRadiusSpellInfo` for storm and shield).
- `timerWhen{OneShot,PlayerCasting,CreatureCasting,ComputerPlayerCasting}` are **float** (seconds, -1 = no limit; the
  getters 0x5FB7A0..0x5FB7D0 do `fld`). MAGIC_TYPE 0 carries the integer 10 there (garbage). Storm: 40 s.
- `GWorshipSiteInfo::chantsToReserveForMaintaining` is a float in the code but the file holds an integer: it is read as
  ~7e-43 (bug in the original, kept).
- Names of the tail of `GSpellSeedInfo` (file 0xF0..0x17C): `selectionGesture` (1 SPIRAL / 2 INVERSE_SPIRAL),
  `gesture`, `gestureStage2` (0), `sizingGesture` (4 CIRCLE on STORM, SHIELD, PHYSICAL_SHIELD), `castType`
  (SPELL_CAST_TYPE), `isKeptInHand`, `castOnObject`, `seedFollowsSpell`, `magicTypes[4]` (no PU, PU 0, 1, 2),
  `powerUpGestures[3]`, `mesh`, `scale`, `holdLoweringMultiplier`, `holdRadius`, `holdYRotate`, `holdType`,
  `attachInHandEffectToBone`, `deleteSeedOnceCast`, `holderParticle`, `exists`, `iconIndex`, `tooltip`. No meaning
  yet: 0x10C, 0x138 (equal to the scale except on the flocks), 0x150/0x154, 0x15C (0.1), 0x160 (1), 0x168, 0x178,
  0x17C (1 on FIRE, LIGHTNING_BOLT, HEAL, WEAK, STRONG).

### Ported helpers

| Function | Address | Detail |
|---|---|---|
| `GMagicInfo::GetInfoFromText` | 0x5FB3B0 | stricmp against the effect's `debugString` ("STORM_PU2"); 42 = not found |
| `IsMaintainedSpell` | 0x5FB810 | FOREST (13), SHIELD, PHYSICAL_SHIELD (19, 20) |
| `GetChantsRequiredToCreate` | 0x5FB830 | `costToCreate` (FIRE 3500, STORM 8000); `GScript::GetManaForSpell` uses the same (0x5FB800) |
| `IsCreatureCastFromAbove` | 0x5FB7E0 | `== 1` |
| `IsInAggressiveRange` | 0x5FB840 | 1.0 if min ≤ d ≤ max, otherwise 0.0 |
| `GMagicEffectInfo::GetTribalPower` | 0x5FB6A0 | product of the player's `TribalPower[t]` (GPlayer+0x68) over the flagged tribes; <0 → 0.5, >100 → 100, ≤0.5 → 0.5; no player 1 |
| `GetTribalPowerTribe` | 0x5FB710 | the first flagged tribe with power > 1, otherwise -1 |
| `GSpellSeedInfo::GetPowerUpFromMagicType` | 0x72AF70 | -1 for `magicTypes[0]`, 0/1/2 for `[1..3]`, -1 if not there. Oddity: with `[3] = 0`, MAGIC_TYPE NONE gives 2 |
| `fn_0072AFA0` | | levels = 1 + non-null `powerUpGestures` |
| `GetMagicTypeFromPULevel` | 0x72AFC0 | -1 → `[0]`, pu → `[pu+1]` (no bounds check) |
| `GetMagicInfoFromPULevel` | 0x72AFE0 | the one for that level; if its type is 0, the base one |
| `fn_0072B010` | | the power-up gesture of the type and its level (the base type: 0 and -1) |
| `SpellSeedIsOfMagicType` | 0x72B060 | any of `magicTypes[0..3]` (NONE matches the first seed with a 0: STORM) |
| `GetFirstSpellSeedForMagicType` | 0x72B090 | -1 if none (not 30) |
| `fn_0072B100` | | `fn_0072B010` on the first seed of the type |
| `fn_0072B0D0` | | the first seed with `exists` and an equal `iconIndex`; otherwise -1 |
| `fn_0072B170` | | seed by name (stricmp against the `debugString`: "HEAL"); 30 = none |
| `fn_0072B1C0` | | the first seed of the type; 30 = none |

- `GMagicInfo::powerupType` is -1 in every record and nobody writes it. `MagicTables::GetPowerUpLevel` gives the
  level derived from the seed (`GetPowerUpFromMagicType` on the first seed of the type: TORNADO 1, storm lightning
  0).
  **Unverified** (PLAN R3).

## Spell core (M1, `src/Magic/Core`, `src/Magic/Spells`, `src/ECS/Effects`)

Each spell is an entity with `components::Spell` (the 0xEC-byte `Spell`); the original's virtual functions are a
`SpellOps` table per class (`SpellClass`, the class of the `GMagicInfo` that allocates it with vt 0x34), each class in
its own file under `Spells/`. An unregistered class runs as a plain `Spell` (M1 registers General and Heal).

### Life cycle (Spell.cpp 0x71FB40..)

- Constructor 0x71FB40: without a creator it sets neither creator nor player; with one, `player = creator->GetPlayer()`
  (or the neutral player), it goes **at the start** of the list (`g_game+0x205BC4`), `+0x48 = IsCreature`, `+0x4C` = the
  player of the creator (or of the SpellIcon) has +0x8E0 == 1.
- `GMagicInfo::CastAtPos` fn_005FB490: without a creator it uses the neutral player; `AllocSpell`, `InitWithPos`; if it
  does not return 1 the spell is deleted. fn_005FB520 does the same with `InitWithObject` if the `castOnObject` flag it
  reads is 1. It reads that flag at `GetSpellSeedInfo(spellSeedType = -1) + 0x118` = 0xD9D600, inside GSpellIconInfo[1]
  (R2 unresolved); here the `castOnObject` of the first seed of that magic type is used.
- `Spell::InitWithPos` 0x71FE50, in order: creature desires (fn_00721730, M8), `originalCastPos`, player statistic,
  `SetChants(castData.chants)` (+0x38 = +0x3C), `maxObjects`, `duration`, pos and castPos, copy of the
  PSysProcessInfo, `dir = info+0x24`, magnitude (`castData.magnitude`, 40 without castData), `PSysInterface::Create` at
  `(x, altura del suelo + pos.y, z)`, the player record +0xDC {castPos, magic (+0xC), turn (+0x10)}. With a PSys:
  `psys->SetPlayer`. **Without a PSys and with `particleType != 0` it returns 0**: the spell is not cast, and there is
  no reaction either. Without a PSys and with `particleType == 0`: `SpellEvent{11}`. Afterwards, the reaction
  `createReactionOnCast`.
- `Spell::ProcessSpells` 0x720300, once per turn: decay of the spell grid, worship sites (fn_0072BF80, M6/M7), icons
  (fn_00727350, M7), `GPlayer::ProcessSpellIcons` (M7), **first all the `ProcessMaintainRequest`**, and then, per
  spell, `ProcessSpellSeed` (vt 0x500) and `Process` (vt 0x528); a 5 deletes it.
- `ProcessMaintainRequest` 0x7204D0 (always 1): age += 0.1 s; `edad > duración` (with duration ≥ 0) → CloseDown; a
  creator that is not functional → CloseDown and creator = NULL; `enabled = 1`; `creator->UpdateSpellInfo`; hand spells
  (castType IN_HAND) move castPos to the hand. If it is open, **the strength is read before paying for the turn**
  (`psInfo.power` is the previous strength), it pays for the turn and marks the grid. If it is closed: `enabled = 0`,
  `power = 0`.
- `CoreProcess` 0x720660: if it is open, `Recharge` and, with `power <= 0`, CloseDown. Then one PSys step with the
  PSysProcessInfo; if it returns 5, its reactions and the PSys go away. `Process` 0x720710 returns 5 when there is no
  PSys left.
- `CoreCloseDown` 0x720160: `closedDown = 1` and CloseDown of the PSys (vt 0x118).
- `ToBeDeleted` 0x71FD90: leaves the list, deletes the PSys and the reactions, **also deletes the linked seed** (vt 0xC
  on +0xAC) and then CloseDown.

### Chants (`Magic/Core/Chants`, 0x720750..0x720A90)

Verified instruction by instruction:
- Safety level 0x720880: the maintained ones (FOREST, SHIELD, PHYSICAL_SHIELD) → `initialChants`; the others
  `max(min(coste/turno × (1000/ms por turno) × 5, initialChants), costPerEvent)`.
- Strength 0x720750: without a creator, 0. With `S > 0`, `chants/S` clamped to 0..1; with `S <= 0`, 1 if there are
  chants left. Then × `GetTribalPower` (0x7216F0, of the spell's player) × the seed's `+0x8C` × `+0xE4`.
- `PayFor(coste, forzado)` 0x720990: without a creator, 0; free (+0x5C), 1. With `divideCostsByTribalPower == 1`,
  cost / max(tribal power, 1). Subtracts the cost; if it ends up below the level and `isSpellRecharged`, the creator
  refills the whole deficit (forced) or at most the cost. Returns the strength.
- fn_00720830 (pay for the turn): **with cost 0 it returns 1 without calling PayFor**. `PayForOneEvent` 0x720A90 pays
  `costPerEvent` (without a creator it returns 0 without creating the mana path point). `Recharge` fn_00720910 refills
  the full deficit.
- Who pays (`MaintainSpell`, vt 0x58): `GPlayer` 0x64C430 gives everything **only if it is the neutral player**;
  otherwise 0, so the spells of a seed without an icon live off their initial chants. `GameThing` 0x56FED0 gives
  everything. The worship icon and the creature are M7 and M8.
- Lightning cast by a player (real trace): 5000 chants, −50 per turn, strength 1 until it drops below 2500 (turn 50),
  0.98 on turn 51 and 0.8 on turn 60. It closes on turn 61 (the age is a sum of 0.1 floats and on turn 60 it still does
  not exceed 6.0) and the PSys ends on that same turn.

### Events and effects (`SpellEvent`, `ECS/Effects`)

- `Spell::SpellEvent` 0x720F40 ignores types 1 and 11. `ApplyDefaultSpellEffect` 0x720C30:
  - if it is closed, nothing;
  - the spell moves to the event (and 0);
  - EffectValues of the effect × the strength returned by `PayForOneEvent` (with 0 it is not applied and returns 0),
    × tribal power × `event.strength`;
  - type 4: `SpellHitSpell` with the target; if the other one does not fall, it returns 0;
  - type 7 (after paying for the event): with a target, `CanBeDestroyedBySpell == 1`, no reaction and no direction;
    **without a target it applies nothing but still goes on to the reaction and returns 1** (0x720DB4 jumps to 0x720EBC);
  - type 5 with a target: if it accepts it, `ApplyEffect` (and the heal one removes poison);
  - the others (and 5 without a target): `ApplyEffectToMapPos` at the spell's position;
  - at the end, the reaction `createReactionOnEvent` and `+0x2C = event.velocity`.
- With `checkShields` it looks for a shield (fn_006D0BC0) and sends **itself** a type 4 event with itself as the
  target (0x720D84). Pending with the shields (M6).
- `SpellHitSpell` fn_00720B70: cost = own strength × `costPerShieldCollide`. If the other one has strength 0 → 1. The
  other one pays forced and this one pays one event. If the other one is left without strength and this one with
  strength → `SetUpDestroyedReaction` and 1; otherwise `UpdateStruckReaction` and 0.
- `EffectValues` (0x40 bytes): +0x08 the 7 numbers (burn, crush, hit, heal, push, alignment, belief), +0x24 the radius,
  +0x28 who applies it (the spell's creator), +0x3C the player. `*=` (0x525720) only scales the 7 numbers.
  `IsDestructive` 0x5258C0: burn, crush, hit or push > 0.
- `ApplyEffectToMapPos` 0x525100: cells of pos ± R; every available object that accepts the effect, with
  `dist(pos, centro de fuego) ≤ R + radio de fuego` and `|alt(pos) + pos.y − (alt(obj) + obj.y)| ≤ altura + R`. No
  attenuation. In `Object` the fire centre is the position (0x639AA0) and the radius is `Get2DRadius` (0x639AC0 → vt
  0x64).
- `Object::ApplyEffect` 0x637980 (villagers do not override it):
  - damage = positive crush and hit × the defence multipliers (0x637D00; before that it passes the heat to the fire,
    M5); healing = heal × its multiplier;
  - heal → `IncreaseLife`, damage → `ReduceLife`; if life drops to 0 → `DestroyedByEffect` (a villager dies);
  - crush > 0.01 on something that can be crushed and without its own reaction → REACT_TO_OBJECT_CRUSHED (18), started
    by whoever applies it (or the object itself) and with the player **of the object** (vt 0x1C; for a villager, the
    owner of its town);
  - returns `(1 − vida0)/curación + vida0/daño`.
- `FireEffect::ConvertTemperatureToDamage` 0x72EEC0: 0 below Tc; otherwise `(T − Tc)/Tc ×
  defenceMultiplierBurn × 0,1`.
- `GAlignment::Update` 0x414410, the alignment change left by the effects: in
  [Spell effects](magic.md#spell-effects-galignmentupdate-0x414410).
- The reactions (`CreateReaction` 0x6E3D70, `SpreadReaction` 0x6E3E10): in [Reactions](magic.md#reactions-ecseffectsreactions).

### Cast rules (`Magic/CastRules`)

- In the `GMagicInfo` vtable the symbols have swapped names:
  - **vt 0x30 is the check at a position**: base 0x5FB420 = 1; heal 0x5FBD20 = `FindTargets`; resources
    0x5FBA00 = land; creature 0x5FA7E0 = 0; forest 0x5FAE80; teleport 0x5FBE50;
  - **vt 0x2C is the one for an object**: base 0x5FB430 = vt 0x30 at its position; resources 0x5FAC00; forest
    0x42D8E0 = 0; creature 0x5FA7F0.
- The rule fn_005FB5D0: inside the map (10 m cell < size) and, depending on `castRuleType`: 0 always, 1 land, 2
  influence `> 0`, 3 both.
- `GMagicHealInfo::FindTargets` 0x5FBB00: R = `dummyVar` (10 / 35) and maximum `maxToHeal` (20 / 100), both × the
  tribal power if there is a spell. It walks `ceil(2R/10)²` cells in a spiral (GUtils::Spiral 0x74D7E0, table +x, +z,
  −x, −z) and counts their living mobile objects that accept the effect and `CanBeHealedByHealSpell`, closer than R.
  **It does not check whether they are missing life**: any living one counts. With a spell, each one becomes a target
  of its PSys.
- **`SPELL_AT_POS` checks nothing**: the creator is the neutral one and the check flag goes to 0. Moreover
  `SpellHeal::InitWithPos` 0x72D870 does not look at how many it found. So a script heal is cast even if there is
  nobody (the PLAN expected it to fail). Only the hand asks (`SpellSeed::CanCast` 0x729150: the rule and then vt 0x30).

### Seeds and one-off miracles (`SpellSeed`, `OneOffSpellSeed`)

- **fn_00729900 is the reverse of what PLAN §4.1.1 says**: with 0 → `+0x90 = 1` (ready); with another value →
  `+0x90 = 0, +0x94 = 0`. The worship icons pass 1 (the seed waits `delayBeforeSeedActive` = 1.5 s) and
  `CreateSpellIntoHand` passes 0: **a one-off seed is ready as soon as it reaches the hand**. Moreover
  `InterfaceSetInMagicHand` 0x728810 already sets +0x90 = 1.
- `CreateSpellIntoHand` 0x72A730: with the hand free, it looks for the player's worship icon for that seed
  (`GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40, which asks for an icon from which the spell can be requested **with
  chants available** at its site) and, if there is one, creates that icon's seed (fn_007282A0: its creator is the icon,
  `worship::icon::CreateSeed`); otherwise, the loose seed (fn_00728300). It marks it "ever enabled"; `+0x72 = 1`; it
  charges it for free with its whole cost; it puts it in the hand, ready. In Land1 there are no icons, so it always comes
  out loose.
- `InterfaceSetInMagicHand` 0x728810: `SetPowerUp` of the current level. Without chants to recast (or with bit 1 of
  +0x54) the seed is deleted (3); otherwise it clears +0x98, +0x70 and +0x94 and is left ready.
- `ProcessInHand` 0x729930 (every turn in the hand): +0x94++; ready when `turnos × 0,1 > 1,5`; if its spell has closed,
  the seed is deleted.
- Also ported: `StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200 (if the spell is still tied to this
  seed, CloseDown of the spell; otherwise, only of its PSys), `ProcessFromSpell` 0x728F70 (always 1) and `Cast` 0x729520.
  Casting from the hand is M2.
- `OneOffSpellSeed::Create` 0x72A2F0: seed 0..29; `MobileObject(pos, info 0xD39F3C, 0, 0, escala 1)`. The orb is always
  drawn at scale 1 and +0x6C keeps the scale that will be passed to the seed.
  - Shared mesh `.\data\spells\meshes\O_Bibble_up.l3d`: a dome from 0 to 4.5 m above the ground, with UVs in 0..0.25
    (a 4×4 atlas).
  - `UpdateFrame` 0x72A570: `fase = fmod(fase + ms × 18 × 0,001, 16)`, frame = int(phase), offset
    `u = (cuadro % 4)/4`, `v = (cuadro / 4)/4` (vt 0xE8 receives (u, v)). In openblack the offset goes in
    `UvScroll {u, v}` and the shader adds `u` in quarters.
  - **The orb is additive (faithful, corrected on 2026-10-01 with the capture of the original).** The mesh has a
    physics submesh (`Smooth`, not drawn) and the visible one, the cap, an `AlphaTextured` primitive (type 4, byte
    +5 = 5: two-sided and repeat) whose skin 0xF49809BD ARGB4444 is a dark turquoise orb (51, 119, 136) with a white
    highlight at the top left, almost opaque (alpha 13-15 of 15, or 0 outside).
    - But the file is not what decides: `CallVirtualFunctionsForCreation` 0x72A450 loads it with
      `GJUtils::GetSharedMesh` 0x57DFB0 and `MaterialProperties` {1, 1, 0, 1, 1} (bytes at 0x72A474..0x72A485). Byte
      +3 = 1 makes `PGetSharedMesh` (0x57DF18) call fn_0057E1D0, which applies `GJUtils::SetMaterialProperties`
      0x57E120 to all the primitives when the mesh is loaded:
      - type 4 → 6; if +4 = 0 → 3; if +0 (additive) = 1 → 13; if +1 (writes Z) = 1: 6→5, 13→12, 8→3, 16→9; otherwise:
        5→6, 12→13, 2 or 3→8, 9→16;
      - +2 (two-sided) sets or clears bit 0 of byte +5.
      - For the orb: **mode 12** (`fn_0082EB50`: `SRCALPHA / ONE`, colour and alpha = texture × diffuse, writes Z) and
        **single-sided** (byte +5 = 4).
    - `Draw` 0x518E90 tints the object with `0x96FFFFFF` (byte from [0xBE8E8C]; fn_0080BF10 multiplies the diffuse:
      alpha 0xFF × 0x96 >> 8 = 0x95) and calls `SetGlobalAlpha(1)` (LH3DObject vt 0x48, bit 0x80 of the flags), which
      switches to the alternative mode table 0xC387C8. That table **leaves unchanged** the additive modes 10-13 (read
      from the executable).
    - Result: the orb **adds** its texture × light × (0.58 × texture alpha) to whatever is behind it. Over the sand by
      day it comes out almost white and pearly: the turquoise texture turns sky blue and the highlight, saturated
      white. The background shows through with green and pink tones.
    - Before, openblack blended it as mode 5 (`SRCALPHA / INVSRCALPHA`, two-sided). That covered half the background
      with the dark turquoise: a dark greenish orb. The earlier investigation (N·L light, ambient 90/256, alpha 0x95)
      was correct, but it missed this material change at load time.
    - openblack:
      - `graphics::MaterialProperties` and `L3DSubMesh::SetMaterialProperties` (the type change of 0x57E120, with the
        type stored in `Primitive::materialType`) and `L3DMesh::SetMaterialProperties` (fn_0057E1D0), in
        `src/3D/L3DSubMesh.*` and `L3DMesh.h`;
      - `Game.cpp` applies it to `O_Bibble_up` when loading it;
      - `Renderer::DrawSubMesh`: an object with `components::Alpha` (the 0xC387C8 table) keeps the additive blending
        of its additive primitives, and those in modes 11 and 13 without writing Z.
    - Captures: `dev\_audit\magic\orbref_a.png` (before), `orbref_b.png` and `orbref_c.png` (after), and the comparison
      `dev\documentacion\audit_magic\ref\orb_compare.png` with the user's capture of the original (`ref\dispenser_original.png`).
    - Differences remaining against that capture, **pending**:
      - in the original the orb floats higher above the dispenser and looks bigger;
      - in openblack the effect of the FIRE seed is seen as a yellow core inside the orb, and in the original it is not
        seen (at its centre there is a sky-blue blotch);
      - the original's sand is lighter, and since the orb is additive the background changes its look a lot.
    - To sort it in the Z-sorter, `Draw` moves its position forward towards the camera by its radius (vt 0x60) and
      then restores it. That way the orb is painted after the seed inside. `DrawSpellGraphic` receives the high byte
      of the diffuse (0x95) as alpha. openblack: `components::Alpha` = 149/255 in `OneOffSpellSeedArchetype` (pass
      `MainBlended`). The forward shift by the radius is there (lane "seed"): `one_off::UpdateFrames` stores
      `OneOffSpellSeed::sortPoint` = box centre + normalize(camera − centre) × radius (`Get2DRadius`: largest half
      extent in x/z × scale, 2.3 m) and `RenderingSystem` / `Renderer` sort the orb by that point
      (`RenderContext::sortPoints`).
  - **The orb always faces the camera.** `Draw` calls fn_00518720 every frame, active while the byte [0xBE8E8D] is 1
    (it is). This rotates the object's 3D matrix around the centre `c` of the mesh's box
    (`LH3DMesh::ComputeBoundingBox` 0x8081B0 at load time, all submeshes; here (0; 2.23; 0)):
    - `D = normalize(centro − cámara)` and `U = normalize(Y − (Y·D)·D)` (Gram-Schmidt with (0, 1, 0), static 0xCC62D0);
    - it builds the matrix with rows (U×D, −D, U), inverts it (fn_007FB3F0), scales it by +0x44 and sets the position
      to `centro − M·c`. That way the mesh's +Y points at the camera.
    - The visible submesh is only the top cap (y from 2.18 to 4.46, radius 2.28), so a round bubble is seen from any
      side. The physics one is a whole sphere from 0 to 4.37 and does not change when rotating.
    - Only the 3D object's matrix moves, not the object's position. The 4×4 animation does not depend on the rotation.
    - `Draw` draws nothing if +0x70 (the SpellSeedGraphic) is 0. `CallVirtualFunctionsForCreation` 0x72A450 creates it
      (except with object flag 0x100, which a new orb does not have): `SpellSeedGraphic::Create(pos, semilla, el
      jugador local, 1, pu)` and `SetAutoUpdate(0)`; `ToBeDeleted` deletes it. In openblack this is done by
      `OneOffSpellSeedArchetype` and `one_off::InterfaceTap` (capture `review2_orb_graphic.png`: the FIRE seed and its
      effect inside the dispenser's orb).
    - openblack: `one_off::UpdateFrames` computes `OneOffSpellSeed::facing` and `facingOffset`, and `RenderingSystem`
      draws the orb with them. `Transform` does not change: the dispenser compares its position and the physics uses
      the sphere. Captures `orb_face_low.png` (from the side) and `orb_face_top.png` (from above).
- `InterfaceTap` 0x72A640: `CreateSpellIntoHand`, immersion 0xE, sample 0x6D (`G_SpellBubblePop_04`) and the orb is
  deleted (3).
- **With the real hand** (faithful, lane "grab", `HandSystem.cpp` / `HandPlacement.cpp`):
  - The object under the cursor (`SendObjectDrawCollision` 0x5D56C0, exact triangle) reaches `ActionPressed`
    fn_005D1330 → `StartGrab` 0x5D1740 if `ValidForPlaceInHand` (vt 0x6FC) or `InterfaceValidToTap` (vt 0x740). The orb
    has both: it is a `MobileObject` (`Mobile::ValidForPlaceInHand` 0x425B00 = 1) and `InterfaceValidToTap` 0x72A630 = 1.
  - Pressing on it starts the grab (state 13). If it is released before 225 ms (`State_Grab` 0x5D5250, 0xE1) it is a
    **tap**: `Tap` 0x5D3930 → 0x5D38A0 → packet 0x20 → 0x5DA650 → `InterfaceTap`, and the charged seed goes to the hand.
  - If it is held down, **the orb itself is picked up**: `GenericPickup` 0x5D2800 (packet 0x13) → `PlaceObjectInMagicHand`
    → `InterfaceSetInMagicHand` 0x72A530 (it only marks the magic as enabled). It is carried as a `MobileObject`
    (`GetHoldType` 0x607120 = 6, `Object::GetHoldRadius` 0x638C00) and is dropped or thrown with physics: constants 9
    (`GetPhysicsConstantsType` 0x72A920) and the `GMobileObjectInfo` info 25 (0xD39F3C; **(inferred)** that it is 25,
    from the 0x114 step from WHALE's). The dispenser no longer sees it in its place and makes another one when
    recharging.
  - The tap and the grab require the hand to be inside the player's influence
    (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1; `m_InInfluence` from fn_005D1120, type 1). Outside it
    nothing happens.
  - Selection volume: the mesh as it is drawn, rotated towards the camera (fn_00518720), so the dome that is seen from
    any side counts. **(inferred)**: if the ray hits the seed inside (`SpellSeedGraphic`, which is not an `Object`), it
    counts as if it hit its orb or icon.
  - The icons of the worship sites and of the town centres (`Object::ValidForPlaceInHand` 0x402870 = 0) are tapped on
    press (`StartGrab` → `Tap` immediately), with the same influence rule.
  - **Pending**: the hover tooltip text (fn_005D6D70: on an orb, `GetOverwritePickUpToolTip` 0x72AC50 = the text of
    its magic +0x110; the tap one is 0xEF7). Neither `GInterface::StartImmersion(0xE)` nor the `GameThingClicked`
    record of fn_005D36D0 are there.
  - Hook: `OPENBLACK_MOUSE_AT=0.5,0.5 OPENBLACK_CAMERA_LOCK=1948,40,2550,1939.2,33,2537.7` with
    `--mod test.miracle-dispensers` (the FIRE orb in the centre). Tap: `OPENBLACK_TEST_CAST="press@5,release@5.1"`.
    Pick up the orb: `"press@5,release@5.6"`. Drop it: add `",press@7,release@7.2"`. Captures `grab_tap.png`,
    `grab_hold.png` and `grab_drop.png` in `dev\_audit\magic`.
- Map script (fn_00715150):
  - case 83, `CREATE_ONE_SHOT_SPELL(pos, semilla)` → Create(pos, the seed by name, −1, 1);
  - case 84, `CREATE_ONE_SHOT_SPELL_PU(pos, magia)` → the first seed of that magic and its level
    (`GetPowerUpFromMagicType`).

### CHL script (`Magic/Script/CHLSpells.cpp`)

- `SPELL_AT_POS` 0x70C190 pops curl, duration, radius, from, to and magic. `CastSpellAtPos` 0x70BD60 builds castData
  {radius, initialChants, duration, −1} and the PSysProcessInfo {+0x0C from, +0x18 to − from, +0x24 dir (0),
  power 1, +0x34 curl, active}. `SPELL_AT_THING` 0x70BFA0: with an Object, cast on the object.
- `SPELL_AT_POINT` 0x70C560 casts nothing: it returns the first spell of that magic closer than the radius
  (fn_007217A0). For the shields it calls fn_0072BA00 (M6).
- `SET_PLAYER_MAGIC` 0x70C6C0 (player, magic, enable) → `SetMagicTypeEnabled` (the counter of who has it).
  `HAS_PLAYER_MAGIC` 0x70C750 → "ever enabled", and **1 if the player does not exist**. The script's players are
  n − 1 (0 = the neutral one; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0).
- `PLAYER_SPELL_CAST_TIME` 0x70C9A0: seconds since the last cast (FLT_MAX without a player).
  `PLAYER_SPELL_LAST_CAST` 0x70CA50: its magic. `GET_LAST_SPELL_CAST_POS` 0x70CAB0: its point. `GET_MANA_FOR_SPELL`
  0x70CD40: `costToCreate`.

### PSys linked to the spell (`PSys/SpellLink.h`)

How the spell owns its effect and advances it (`StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, event 3 of
`LandscapeCollide`): in [PSys linked to the spell](particles.md#psys-linked-to-the-spell-psysspelllinkh).

### Spell grid (`SpellGrid`)

`u8[64][64]` at 0xD9C370 (80 m cells). `MarkSpellGrid` fn_00721570 sets 0xFF where there is an open spell;
fn_007215C0 lowers it by 0x20 per turn when `g_game+0x205A28 == 1` (unidentified flag; here always).

### Order within the turn (`Magic/MagicLoop.cpp`)

`GGame::ProcessTurn` 0x54E5C0 calls, in this order: atmosphere (1), influence rings (2), players (3), dances (4),
forests (5), the living, fire (6), reactions (7), `Spell::ProcessSpells` (8), the particle containers (9), physics
(10), the PSys sounds (11), `GScript::Process`, weather (12), `CHand::GameTurnUpdate` (13) and the rewards (14). In
openblack `Game::GameLogicLoop` calls `magic::ProcessTurn` (1..8) after `livingActionSystem`, then
`psys::manager::ProcessTurn` (9) and the fireflies, the physics, `magic::ProcessPSysGameLoopEnd` (11), the scripts,
the weather things and the climate (12), `magic::ProcessHandTurn` (13), in the original's order
([engine-loop.md](engine-loop.md) §2); the rewards (14) are not ported. The spells' PSys are not advanced by the manager:
they are advanced by their spell in 8.

### Hooks and traces

`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_SEED`, `OPENBLACK_TEST_ONESHOT` and `OPENBLACK_SPELL_TRACE` are in
[openblack-internals.md](openblack-internals.md#debug-environment-variables). Captures and logs in
`dev\_audit\magic\`:
- `m1_oneshot_orb.png` and `m1_oneshot_orb_close.png`: the orb next to Land1's store;
- `orb_translucent_open.png` (orb at 1790, 2625) and `orb_translucent.png` (at the store): the translucent orb;
- `m1_heal_trace.log`: heal at the store; 0 targets, it is cast anyway, closes after 20 s and is deleted on turn 200;
- `m1_lightning_player.log`: the lightning table above;
- `m1_seed_hand.log` and `m1_oneshot_tap.log`: a FIRE seed in the hand with 3500 chants, ready.

## Casting from the hand, gestures and hand effects (M2, `src/Magic/Gestures`, `src/Magic/Hand`, `HandSpellSeed.cpp`)

Reports: `casting.md` (§2-5) and `visuals_sound.md` (§1.4, §4.13). What is below was read in the exe; what was not is
stated.

### Gestures: the buffer and the recogniser (`GestureBuffer`, `GestureMatch`, `GestureTemplates`)

- **Input** (`GestureInput.cpp`): the sample is given by the mouse **with no button**. It is CMouse message 0, every
  28 ms of mouse events (`fn_005CEAD0`), with the terrain point under the cursor, or that of the last sample if it is
  outside. There are no samples while paused nor during the 0.4 s following a recognition. If the camera changed
  position in the frame (`GCamera::IsMoving`), **the buffer is cleared** on every `ProcessPowerUpSystem`.
- `GestureSystem::AddSample` 0x57BBC0: 80 samples in a ring.
  - A still sample is compared with the one from **two messages before** (0x57BC3A: head − 2, because the head has not
    advanced yet). 70 in a row like this clear the buffer, and that sample starts it again: it is the 72nd still
    sample.
  - `ProcessNewSample` 0x57C3F0 finds the corners on the fly:
    - corner = turn ≥ π/8·¾ (`FindCorner` 0x57BFE0);
    - merge or rejection by length (`MergeOrReject` 0x57C200; `LongEnough` fn_0057C630: 12 px, or between 4 and 12 if
      the recent box measures less than 50 px);
    - heading and octant of the exit (`UpdateHeading` 0x57C710). The octant rounds an exact .5 down (fn_00578700).
- `Gestures.jty` (`GestureSystemDataList::Load` 0x579AF0): 81 templates of 0x65C bytes.
- `MatchGesture` 0x579F10 → `Match` 0x57A050: first `MatchForward` 0x57A1A0 and, if the template allows it,
  `MatchMirror` 0x57A3E0 (negated turns, error without wrapping). Only three things are compared:
  - the sequence of turns (a turn smaller than T1 = 21π/128 can be absorbed; maximum error T2 = 3π/16);
  - the first direction;
  - the aspect class (0.15 / 4, fn_00579FA0).
- Packet (fn_0057A5E0), in mode 2 (the one of all the templates):
  - the point is the terrain point under the centre of the box of the matched samples;
  - the size is 1.05 × the half width of that box in the world, at that distance.

  It is the `size` of the circle of the storm and of the shields (the magnitude of the cast).
- The player's miracle gestures (SPIRAL selection) are **14**, not 12: FORK_DOWN, CYRILLIC_L, VERTICAL_SCRIBBLE,
  S_SHAPE, FORK_RIGHT, FORK_LEFT, FORK_UP, HEART, THREE, W_SHAPE, SQUARE_SPIRAL, INVERSE_SQUARE_SPIRAL, HOUSE and STAR.
  Each one recognises its stroke and none of the other 13 (`test_gestures`, `realData`).

### What is looked for and when (`PowerUpSystem.cpp`, `GInterface::ProcessPowerUpSystem` 0x5CF300)

- It runs at the end of each `InterfaceActionProcess`: once per frame (`ProcessFrameInputs`) and again per turn
  (`GInterface::Process`, with the time of the last frame). That resolves R6. Still not found: who sets bit 0x02 of
  m_Buttons and who reads the 40 s cap of the repeat.
- Order of each call:
  1. the clearing by the camera, the 0.4 s wait and the expiry of the pending circle (5 s);
  2. **the circle**: with the action pressed (m_Buttons 0x200) and a seed with `sizingGesture` (CIRCLE: storm,
     shield and physical shield), the circle stores position and size;
  3. with an **icon** seed that is charging (`HoldingChargingSeed` fn_005CEF50), the seed's power-up gestures
     (fn_005D0000); if it already has a power-up, SCRIBBLE removes it (packet 0x6A). If there is no such seed, the
     stage of the open selection (`SelectionStage` 0x5CFAE0, cap `selectionSystemTimeOut` = 30 s);
  4. **SCRIBBLE cancels**:
     - it shakes off whatever is in the hand, if it is in the influence and is `ValidToShakeFromHand`
       (`DoRemoveFromHandVisual` + `ForceDropHeld`; a seed goes back to its worship site or is deleted);
     - or, with the hand empty, it cancels the charge of the most charged icon (packet 0x1E);
  5. with the hand free, SPIRAL / INVERSE_SPIRAL open the selection if there is a requestable icon of that category
     (`OpenSelection` 0x5CF010);
  6. R_SHAPE repeats the last miracle (packet 0x26), if the player can.
- **One-off seeds** have no icon, so with them there are no power-up gestures (only with those of a worship icon, M7).
  SCRIBBLE does shake them off.
- **API for M7** (`PowerUpSystem.h`): `gestures::SetIconProvider(IconProvider*)`. Without a provider the selection
  never opens; `Worship/GestureIconProvider.cpp` registers its own. The provider answers:
  - `AnyRequestableIconOfCategory` (fn_0064BE40), `ForEachRequestableIcon` (the walk of OpenSelection) and
    `IconValidForRequest` (fn_0064BEC0);
  - `RequestSpell` (packet 0x25), `CanRepeat` / `RepeatLast` (0x26) and `CancelMostChargedIcon` (0x1E);
  - `AnyIconChargingForHand` / `MaxChargeFraction` (the PHandFX charge bands);
  - `PowerUpAvailable` / `SetPowerUpCharge` (0x6A).
- Not ported:
  - the help (`HelpProfile::Trigger` 0xE..0x17): with `OPENBLACK_GESTURE_TRACE=1` its events go to the log;
  - the immersion (force feedback 3, 8, 9, 10);
  - the HUD gesture icons (`DisplayGesture` fn_0068ABA0, `S_Gesture0/1.raw`, R17 not read). The `LookingFor` table is
    filled in.

### Casting from the hand (`HandSpellSeed.cpp`)

- `ActionPressedHolding` 0x5D1560 with a seed: over a valid object (in the influence) it applies to the object;
  otherwise, to the ground under the hand, which must be in the player's influence. Depending on the `castType`:
  - **HAND_GESTURE** (storm, fire, shields, flocks): it arms on press if it can be cast there
    (`ValidToApplyThisToMapCoord` 0x728720 = ready and `CanCast`); otherwise, `FailApply`. Arming
    (`BeginApplyOnRelease` fn_005D2730) resets the buffer with the current sample and starts the IN_GAME 3 loop
    `G_HandGesture_02`. It casts on release (states 8/9, 0x5D48D0).
  - **HAND_POSITION** (forest, heal, teleport, destroying lightning): it casts on press (`DropOnMapCoord` fn_005D1850).
  - **IN_HAND** (food, wood, water, lightning): state 10/11. While it is held, one apply per turn
    (0x5D4C10 / 0x5D4D00); on release, `ApplyUnlockProcess` 0x728EB0.
- `SendApplyToMapCoord` 0x5D3340:
  - one packet per turn (`m_ApplySentTurn`);
  - with a pending circle, the point and the gesture are those of the circle;
  - **fn_00729AF0: a seed with `sizingGesture` needs that gesture in the packet**; otherwise, `FailApply`;
  - a power-up level being charged goes with the cast;
  - then `SpellSeed::ApplyThisToMapCoord` 0x728E20 (the magnitude is the size of the gesture) and the result
    (fn_005DA100): the seed stays in the hand if the spell is kept in it; otherwise it leaves (0x16) with the visual
    SUCEED_CAST (3).
- `FailApply` fn_005D18F0: visual 4 (`SF_FailedApply`) at the point and `G_SpellCastFailure`.
- Hold parameters (0x728640..0x728680): MAGIC until the seed is ready (`Cwiggle` at half length) and then its
  `holdType`; radius `holdRadius × escala`, plus `holdLoweringMultiplier`. The seed's mesh is only drawn in the hand
  with `isSpellSeedDrawnInHand`: fire, lightning, heal and storm are only their effect in the hand.

### The hand (`HandMagicFX.cpp`: PHandFX and the effect in the hand)

- **Effect in the hand** (CHand fn_0046E7B0 / `DrawSpellInHand` 0x46E680):
  - it is the level's `particleTypeInHand`, and `SetPowerUp` creates it again;
  - it is advanced every frame by `max(1, g_game_time_inc)` ms, strength = that of the seed's PSys, magnitude = the
    hand's scale, and only with the seed ready;
  - `UR_FollowLocalHand` 0x69A6A0 and `UR_FollowCastPosn` 0x69FE30 (`Rules/HandFollow.cpp`) carry it to the hand;
  - effects that advance per frame are drawn where the last step left them (`manager::SetPerFrame`), without
    per-turn interpolation.
- **PHandFX** (ctor 0x68CB10, `Draw` 0x68D0C0, `Band::Draw` 0x68D6D0): `Power_Up_Band.L3d` bands at scale 10 on the
  root bone, at 10 + 40·index, spinning at (1 + 0.2·index)·12 rad/s.
  - **Matrix** (`Band::Draw` 0x68D8BB..0x68D9EA): the local one is 10·I with the translation (0, 0, +0x18 + index·+0x1C)
    (0x68D900..0x68D909), that is, along the **root bone's own Z axis** (the forearm). Only once it has arrived
    (f ≥ 1, 0x68D90D) each row rotates its (x, y) by the angle +0x20 around that Z (0x68D922..0x68D9DB:
    (x, y) → (c x + s y, c y − s x), c stored as a float at 0x68D929, s on the stack; `lh_matrix::TurnRows(2)`).
    Afterwards fn_007FAFF0 0x68D9EA = local × bone (rows; in glm bone · local). The bone is the first 0x30 bytes of
    the matrix pointed to by CHand +0x47F0 (copied at 0x68D0F2..0x68D100; `PrepareForDrawing` 0x46CAE5 copies the
    same one into the matrix of the hand object). Result: a bracelet that goes around the wrist and spins about the
    forearm's axis. openblack had it along Y and spinning about Y (the ring hung below the hand and turned edge-on);
    corrected (`DrawBand`, captures `documentacion/audit_magic/wristring_{before,after}_1500{0,1}.png`).
  - Permanent: `SetPULevel(pu + 1, 1)` from `SpellSeed::SetPowerUp` 0x729BFC..0x729BFE (pu = POWER_UP_TYPE: −1 no
    power-up, 0 = PU1, 1 = PU2), so 0 / 1 / 2 rings (maximum 5); they start at 2.4 s; alpha 20→130 in 0.85 s, with
    matrix lerp. They fly from in front of the camera to the root bone of the hand (the wrist).
  - Colour (`Band::Draw` 0x68D849..0x68D8B1, on every draw): +0x4C = `GetPlayerColour` 0x64D800 of the local player
    (g_game +0x205A59) with the band's alpha; +0x50 = per-channel lerp of the ctor's colours +0x34 / +0x38
    (fn_0068CA30, args 8 and 9), 0 in all callers. A single draw per band (0x68DD46 vt+0x104). User's recollection:
    a translucent red ring reaches the wrist when picking up a miracle (the exe confirms it: player 1's red).
    openblack: `components::ObjectColour`.
  - Temporary: 5 on gaining a level, 0.1 s between them; alpha 20→120, with slerp.
  - Charge ones: duration lerp(3.5; 1; c), one every lerp(6; 0.3; c) s.
  - They arrive flying from 4 m in front of the camera, at half scale (the matrix 0xEA1CF8 is the camera's, inf).
  - `AddSpellToHandVisuals` plays `G_SpellPowerUpBand`; the shake, `G_ShakeHand_01` and a band that leaves.
- **The hand glow** (a second pass with additive `S_Hand_Flow`, player colour, alpha 0.8, 8×4 atlas at
  −20 frames/s) is computed (`hand_fx::GetGlow`) but **not drawn**: it needs a skinned mesh shader with two textures
  (colour and `S_Hand_Flowa`).

### Utility effects (`PSys/Utility.cpp`, PSysUtilityPSys 0xD4E0E8)

- **The trail** (PT 48 `SF_GestureChain`) is active when the game is waiting for a gesture: icon seed charging, seed
  with m_Held & 8, selection open with the hand free, or seed with a circle. It goes in the hand, with magnitude
  `escala de la mano × f(distancia)` ({0, 50, 500, 1500} → {0.2; 1; 1; 1.5}). Its rules `ZR_ChainGesture` 0x68A080
  (emission fn_0068A330) and `CreateRuleMakeChain` 0x69FD10 are in `PSys/Rules/Gesture.cpp`, and the ribbon is drawn
  with M5's `ParticleChainCreator` (`Graphics/RendererChain.cpp`). Colour: fn_00671110 creates it with
  `PSysInterface::Create` and does `SetPlayer` (vt 0x20) on it with the local player (g_game +0x205A59,
  0x671172..0x671197); `ParticleChainCreator0` of `SF_GestureChain` has `UsePlayerColor 1` (white 255 × the player's
  colour, alpha 10), so the trail comes out in the player's colour (red for player 1). fn_00671260 does the same with
  PT 35 (0x6712CD..0x6712EA); the selection (fn_006711D0) gets no player.
- **The selection** (PT 28 `SF_SpellSelection`), while it is open.
- **The recognised gesture** (`fn_00689790` from `Success(1)`; PT 35 `SF_Gesture`; `UR_GesturingRecognised`
  0x6884F0 / 0x688910):
  - The record (0x48 bytes, list 0xD4EB10) carries the stroke (the terrain points of the whole buffer) and the ideal
    shape of the gesture (`PathSymbol<n>.cam`, or the circle's) placed over the pixel box of what was matched:
    - the box keeps the centre and divides its half sizes by those of the shape (fn_0068C140);
    - each point goes to the terrain under its pixel, at its altitude, or at 400 m along the ray (fn_00689F20);
    - if on the ground the shape comes out **more than twice as deep as it is wide** (camera axes in the horizontal),
      it is squashed vertically ×0.75 and this is repeated, 15 times at most;
    - the ideal one is resampled to as many points as the stroke has.
  - The rule takes one record per step: one atom (PCreator) and **IN_GAME 36 `G_SpellGestureRecognise`**. In its
    subcollection it places `NumAtoms` (234) sprites in the player's colour, with scale × (length of the ideal / 100):
    - the ideal approaches the camera until that scale rises (at most to half distance);
    - each sprite goes from the stroke to the ideal (t over `TimeToIdeal`, blended with smoothstep by `InterpGain`)
      and lights up from the ends (alpha `t × MaxAlpha`);
    - it jitters with shuffled-phase value noise, which fades out after `DispersalTime`. The noise is
      `Noise::VSNoise1To1` 0x590BB0: Ebert's lattice, permutation table 0xBEFDBC and Catmull-Rom spline 0x590010
      (`PSys/Noise.cpp`);
    - the collection pulses from `CollectionAlphaPulse` to 0 between 2.4 and 4.5 s, and the atom dies at `DieAge`
      (7 s).
  - Not ported:
    - the drawing of LH3D's `LightSheet` (50 points on the ideal, height scale × 9, alpha 1 − (2f − 1)²); the data
      are there;
    - the colour pulse of the hand (vt 0x2C of the hand object, unidentified).
  - The noise lattice: 256 × `1 − GameFloatRand(2)` (fn_00590DF0) from seed 0 (inferred: before `GGame::Init`), the
    same values in every game (`PSys/Noise.cpp`, game_random).
- Not ported: feeding a fireball in flight with a fire seed in the hand (the start of `ProcessPowerUpSystem`); it needs
  the cursor to be able to point at the MagicFireBall.

### Hooks, tests and captures

- Hooks, in [openblack-internals.md](openblack-internals.md#debug-environment-variables):
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_CAST_PATH`, `OPENBLACK_TEST_THROW_VEL`, `OPENBLACK_TEST_SHOT_PATH`,
  `OPENBLACK_TEST_GESTURE` and `OPENBLACK_GESTURE_TRACE`.
- **For the other lanes**:
  - `OPENBLACK_TEST_SEED` + `OPENBLACK_TEST_CAST` casts through the real hand path;
  - `_CAST_PATH` drags the hand during the first press (food, wood, water);
  - `_THROW_VEL` is the hand velocity the spell receives (the fireball).
- `test_gestures`:
  - octants and rounding of .5; corners of a square; the clearing after the still samples;
  - synthetic templates (it recognises its own and not the others; the mirror, only with `allowReverse`);
  - the selection with a fake icon: SPIRAL opens and FORK_RIGHT requests seed 4;
  - with `OPENBLACK_GAME_PATH`, `Gestures.jty` (81 × 1628), the player's 14 gestures, and CIRCLE and STAR mirrored with
    `reversed`.

## Worship: where miracles come from (M7, `src/Worship`, `ECS/Systems/Implementations/VillagerWorship`)

Research: `dev\documentacion\miracles\sources.md` (§1-§8). The original's chain is: a **town** stores magic types → its
**village centre** shows one icon per seed → the player's **citadel** has one **worship site** per tribe, with one icon
per seed → the **villagers** dance there and fill its **battery** of prayer power → when an icon is tapped it
**charges** and the seed appears in the hand. Separately there are the one-off miracle **dispensers** and the
**fireflies**.

### Structure and data

- `GWorshipSiteInfo[9]`, one per tribe (`GTribeInfo.worshipSiteInfo`): `chantsPerVillager` 3 (Celtic 4, Tibetan 5),
  `maxDancersVisible` 20, `chantsToFillBattery` 9000, `eachVillagerAddToFillBattery` 300, `prayerSiteDistance` 44,
  `radiusFromCitadel` 37.5, `artifactPowerupMultiplier` 1e-5, and the altar mesh per tribe (101
  `BuildingCitadelNorseAltar`, 93 Indian, 94 Aztec, 95 Celtic, 98 African/Egyptian, 99 Greek, 100 Japanese, 103
  Tibetan).
  - **Bug kept:** `chantsToReserveForMaintaining` is in the file as the integer **500** and the executable reads it
    with `fld` (`fn_0077A950`), so it is ~7e-43 ≈ 0: the reserve for maintaining spells does not exist in practice.
- `GSpellIconInfo[2]`: [0] "Spell Icon" (worship site), [1] "TownSpell Icon" (village centre). Both use mesh
  **203** `BuildingVillageCentreSpellHand` and `gatheringChantAddPerGameTurn` 61.
- **Special points** (the L3D's extra metrics; `Game3DObject::GetSpecialPos` 0x63B040 / 0x63B0B0 = the metric's
  matrix times the object's; `src/Worship/SpecialPoints.cpp`):
  - the worship site's mesh `b_worship.l3d` has **16**: 7 hiding place, 8 dance centre and totem, 9 arrival,
    **10..15 the six icon slots**;
  - the village centre's mesh (e.g. 179 `BuildingNorseVillageCentre`) has **14**: **0..5 the six icon slots** (all at
    y = 2.781, in a ring) and 6 the totem (y = 4.613, in the centre);
  - the icon's mesh 203 has 1: the point where the `SpellSeedGraphic` floats (+1 in y).
- Charge cost = `GMagicEffectInfo.costToCreate` (FIRE 3500 / PU1 7000 / PU2 10000, LIGHTNING 5000/7500/10000,
  HEAL 6000/9000, FOOD 7000/10000, WOOD 7000, WATER 5000/7000, NATURE 13000, LIGHTNING BEAM 16000/32000/60000...).

### The citadel and its six slots (`Worship/Citadel.cpp`)

`CitadelWorship` goes in the temple's entity (`components::Temple`), which openblack creates in `CitadelArchetype`. Six
slots (`sites[6]`); the angle of slot *n* is **the heart's angle + n × 2π/7** (`Citadel::GetWorshipSiteAngle`
0x463610) and the site is placed at `radiusFromCitadel` from the citadel's origin.

- `Citadel::AddTown` 0x463130 → `FindOrCreateWorshipSite` 0x4631D0 / 0x463220 → `FindTribeWorshipSite` 0x463190 or
  `RequestANewWorshipSite` 0x4633F0 (the free slot closest to the nearest town of that tribe, otherwise to the
  citadel).
- `CitadelHeart::CreateBuiltWorshipSite` 0x465110 is the script's `CREATE_WORSHIP_SITE`: it creates the site of that
  tribe **without checking the town** and adds the player's towns of that tribe to it. The position and site number
  that the script brings **are not used**.
- `GPlayer::PostLoadCleanup` 0x64AB90 (right after the land's script; in openblack, on the first turn): for each player
  with a citadel, each of its towns without a worship site → `Citadel::AddTown`.
- `Town::IsAllowedToCreateWorshipSite` 0x740BB0: **never on land 1**, nor if the script forbids it
  (`SET_CAN_BUILD_WORSHIPSITE`), nor without population. That is why in Land1 there are only dispensers and fireflies.
- **What the audio reads** (`GGuidance::CheckWorshipSiteDesiresSFX` 0x71B270). It walks `GPlayer+0xA48` →
  `Citadel+0x34..+0x48` in slot order (`citadel::WorshipSitesOf`). It skips the sites without dancers: fn_0077B960
  jumps to 0x77CFB0, which gives `Dance+0x90` or 0 without a dance (`site::DancerCount`). Of the others it keeps the
  one closest to the camera, closer than 200 m (0x980130). Then it asks for its `CalculateDesireForFood` (vt+0x420 of
  `??_7WorshipSite` 0x8F2840 = 0x77C310; `site::CalculateDesireForFood`), which is
  `1 − min((comida + 0,0001) / (necesaria + 0,0001), 1)`.
  - The food is that of the site's pot (+0xB4, `Pot::JustGetResource` 0x66D390).
  - The needed amount comes from `Dance::CalculateFoodNeededByDancers` 0x50BF20: the sum, per dancer, of
    `(1 − comida en la barriga +0xE8) × foodReqiredForDinner` (+0x2D8).
  - It also reads `Citadel+0x70`, the fraction of the worship strain sound, limited to 1 at 0x71B31C
    (`citadel::StrainSoundFractionAtMostOne`). Only `SetWorshipStrainSoundFrac` 0x463850 writes it (from
    `ProcessSpellIcons` 0x46396C) and it is saved and loaded with the game (0x463D6A / 0x463FB9).
  - **(approximate)** openblack sums the dancers in the order in which they joined, not group by group; only the
    rounding changes.
  - **(inferred)** The initial value of +0x70 is 0: Citadel's constructor has not been read.

### The battery and the site's turn (`Worship/WorshipSite.cpp`)

`WorshipSite::ProcessSpellIcons` 0x77B4D0, once per turn from `Citadel::ProcessSpellIcons` 0x463920 (which comes from
`GPlayer::ProcessSpellIcons` 0x64AEE0, inside `Spell::ProcessSpells`):

1. **Strain** (+0x114) = `(pedido − capacidad) / capacidad`, with capacity = `N × chantsPerVillager × poder tribal[2]`
   (`fn_0077E060`). Without capacity, 1 if something was requested and 0 if not.
2. If the strain is **not** positive, the icons that are being charged share out what is left over:
   `min(disponible, necesitado) / cuántos` to each one (`fn_0077CBC0` subtracts the maintenance reserve, ~0 because of the
   bug above). What each icon accepts is charged to the site.
3. `WorshipSpellIcon::Process` of each icon.
4. **End of turn** `fn_0077B6A0`: `k = min(1, usado/capacidad + empuje)` with
   `empuje = max(0,2; 0,5 − batería/máximo × 0,5)` (0 if it comes out ≤ 0); produced = `capacidad × k`;
   `chantDamage` = produced / N (what it costs each dancer in life); `batería -= usado − producido` (never below 0);
   `disponible = batería + capacidad`. That `k` is also the intensity of the dance (`fn_0077B8D0` →
   `fn_0050C340`).
   - Every 1000 turns the site's artifacts would give an extra; openblack has no artifacts (report R12).
- `UseChants` 0x77BBB0 records what was requested, charges at most what is available and adds to the player's
  statistic. `MaintainSpell` 0x77BC50 and `fn_0077CC50` are the variants for the cheats (infinite chants, free
  maintenance).
- `MaxBattery` = `chantsToFillBattery + N × eachVillagerAddToFillBattery` (9000 without dancers).
- **Visual strain** `fn_0077B3B0` (per frame): `fase = fmod(fase + (5 + 5·clamp(tensión,0,1))·dt, 2π)`,
  `pulso = (cos fase + 1)/2`.
- The real **dance** comes from its `.DAN` (`GDanceInfo[19 + hueco]`, `GroupBehaviour::CalculateDancePosition` 0x597F20).
  It is not ported: the dancers are spread over a 6 m ring around point 8, at 256/N each (the ring part of that
  function). **UNVERIFIED**: the exact shape of the dance.

### The icons and the charge (`Worship/WorshipSpellIcon.cpp`, `Worship/TownCentreSpellIcon.cpp`)

- `WorshipSpellIcon::Create` 0x77F2B0 places mesh 203 in slot 10..15 with the site's scale and angle, and its
  `SpellSeedGraphic` above it (`SpellIcon::Create3DSpellObject` 0x726210). `UpdateGraphicsWithPULevels` 0x77F320 shows
  the highest upgrade level the player has enabled and sets +0x58 = 0.5. **+0x58 is not an alpha**: it is only read by
  `DrawSpellGraphic` 0x51A712 as the band size (0.2 × +0x58 × scale). The icon's seed is painted opaque (the icon
  passes alpha 0xFF). Before, openblack painted it half transparent: corrected.

### SpellSeedGraphic: the seed that floats in the orb and in the icons (`Worship/SpellSeedGraphic.cpp`, faithful except where marked)

Object from `SpellIcon.cpp` (it is not an `Object`; list 0xD9D3D0). Fields: +0x14 MapCoords of the mesh, +0x2C the mesh
(Game3DObject), +0x30 the band, +0x34/+0x38 phases of the vials, +0x3C y angle, +0x40/+0x44 band angles, +0x48 seed,
+0x50 holder PSys, +0x54 scale, +0x58 band size, +0x5C auto-update, +0x60 PU, +0x64 the given point. Seed row =
0xD9D678 + type × 0x190 (memory offsets = file + 0x10).

- `Create` 0x726F60 → fn_00727190: the mesh `GSpellSeedInfo.mesh` (+0x130 of the file) and `ReplaceMeshGivenSeedType`
  0x728450 (table 0x72854C per seed − 3): FLYING_FLOCK sets mesh 1 (AnimalBat1) if the player's alignment
  (GPlayer+0x60 → +8) < `alignmentSwitch` (fn_00723140), otherwise 11 (AnimalSpellDove), and fn_00727440 redoes it
  every 30 turns (`g_game +0x205A40 % 0x1E` in fn_00727350; openblack uses `Game::GetTurn`, **(inferred)** that this
  field is the turn counter); FOOD and the creature vials carry envmap 0 (`envmap.raw`) and BEAM_EXPLOSION properties
  {1,0,1,1,0}: **not ported** (openblack has no per-object envmap). The holder PSys (+0x164 of the file) is created at
  the point + `unknown0x154` × scale with magnitude = scale; the band (`CreatePUBand` 0x727080) if pu ≠ −1.
- fn_007270E0: +0x64 = point, mesh at point + `unknown0x150` × scale (−1.5 almost always: the I_* meshes have their
  origin at the bottom and are ~3 m tall, so this centres them), effect at point + `unknown0x154` × scale.
- The orb (`OneOffSpellSeed::Draw` 0x518E90), every frame it is visible: `GetSpellGraphicPos` 0x72A840 = the drawn
  matrix applied to the mesh point `ResolveLoad()+0x18` (the box centre, **(inferred)** because it is the point
  fn_00518720 rotates about) and scale = scale of the 3D object × 0.6 ([0x8C7BDC]); `DrawUpdateAtPos` 0x727630 (+0x54 =
  scale, fn_007270E0, fn_007274D0: PSys to its point, magnitude = scale, `Process_` with the info zeroed, power 1,
  active) and `DrawSpellGraphic(bola, 0, 1, 0x95)`.
- The icons (`SpellIcon::Draw` 0x5198D2, `TownCentre::Draw` 0x5164D4 → `DrawSpellSeedGraphic` 0x726D30):
  `UpdateOnly(ms)` and `DrawSpellGraphic(icono, 0, 1, 0xFF)` (both tint the icon with 0xFFFFFFFF). The seed stays where
  `Create3DSpellObject` created it (special point 0 + 1, scale 1).
- `DrawSpellGraphic` 0x519AD0 (read in full in the part for the player's seeds):
  - only if `useMesh` (+0x168 of the file, fn_00727690) is 1. **STORM, FIRE, LIGHTNING_BOLT, WATER and TELEPORT have
    0**: in the orb and in the icon only their holder effect is seen (LIGHTNING_STORM / FIREBALL / LIGHTNING_BOLT / WATER /
    TELEPORT_ON_HOLDER). openblack painted their meshes (I_Lightning2, I_Blast, I_Lightning, the horn for water and
    the shield for teleport): those were the "wrong icons".
  - size = `GSpellSeedInfo.scale` (+0x134) × +0x54; angle +0x3C += 2 rad/s × dt ([0x8D8700]), fmod 2π (double
    [0x8D45D8]); `SetPosition` 0x423140: rows X = (cos, 0, sin), Z = (−sin, 0, cos). **No bounce or pulse** for the
    player's seeds: `AsMagicCreatureSpellInfo` (vt 0x38) of its base magic is NULL and it jumps to 0x51A0B3. The bounce
    (+0x38 at 0.35/0.5 per s, `0,5(1 + sin 2π f)`), the 8×4 UV frames at −15 per s (+0x34) and the squashes
    0.7/0.8/1.5 of the switch 0x519D76 (by GMagicCreatureSpellInfo+0x58) belong to the vials 12..27. Only the UV
    frames are ported (0x519B79..0x519C1B, `frame_anim::SpellIconFrame`, see
    [rendering-objects.md](rendering-objects.md#frame-animated-textures)); the bounce and the squashes are not.
  - diffuse alpha = the owner's (0x51A0B3..0x51A0E1) and `SetGlobalAlpha(alfa ≠ 0xFF)` (0x51A0EB), but with arg 2 = 0
    `GetAltitudeAndSetColorSpecular` (0x51A187) rewrites all of +0x4C with table[brightness] (0x803409..0x803413) or
    table[255] (0x803365 / 0x8033DA), with alpha 0xFF (all of `palette.raw` has alpha 0xFF): in the orb the seed goes
    through the 0xC387C8 table with alpha 0xFF ([0xC37D8C], 0x80DEF8), **opaque** (not 0x95). openblack:
    `components::Alpha` = 1.
  - with arg 2 = 0 (all the world calls) `GetAltitudeAndSetColorSpecular` 0x803340 (0x51A187, at +0x14) puts the
    cell's light on the mesh, with no haze afterwards: the `land_light::ObjectMode::Cell` mode of `SpellIcon::Draw`
    (`SpellSeedGraphic::landCellLight`, `LandLightOf` from `RenderingSystem.cpp`). The creature vials go through
    fn_00801C90 + fn_007FEB30 (0x519D90 / 0x519D9E), the model light.
  - the PSys receives the alpha: `GJPSysInterface::SetAlpha` 0x55ED50 (vt 0x12C) writes byte +0x6C of the manager;
    fn_00679860 0x679875 copies it into [0xC0215C] and fn_00679920 0x679BC2..0x679BDF does atom alpha × it >> 8 if it
    is not 0xFF. In the orb (0x95) the seed's additive effect adds 149/256 of its light: without that (before) the
    centre of the bubble came out burnt white and covered the icon (`orbcolour_compare.png`). Then it is painted as the
    last step left it.
  - the band if pu ≠ −1: +0x44 += 10.3 × dt ([0xBE8E94]), +0x40 += dt; pu + 1 draws at +0x64 with size
    0.2 × +0x58 × +0x54, rows: identity with row 1 and row 2 swapped (the old 1 negated), rotation (x, z) by base
    + +0x44, (x, y) by 0.3, (x, z) by k, (x, y) by 0.2; base, k = 0, −1 for the first and 0.5, 1 for the others.
    Afterwards fn_0051A830 turns it towards the camera ([0xBE8E8E] = 1; `billboard::BandToEye`, see
    [rendering-objects.md](rendering-objects.md#objects-that-face-the-camera-billboards)).
  - **Band colour** (`SetColour` 0x7F9770 at 0x51A3BE: edx → +0x4C, the argument → +0x50): +0x4C =
    `GetPlayerColour` 0x64D800 (table 0xBFF0B8 by `GetRemapedPlayer`) of the owner (vt 0x1C), or of the local player
    (g_game +0x205A59) if the owner is the neutral one (g_game +0x205A5B) (0x51A322..0x51A36D); its rgb with alpha
    (+0x70 × the caller's alpha) >> 8 (0x51A397..0x51A3B9); +0x70 = 0x3C (fn_00726F10 0x726F4E, only writer), so in
    an icon (alpha 0xFF) the alpha is 59 and in the orb (0x95) 34. +0x50 (specular) = 0x141414 (byte [0xBE8EA0] = 20).
    Red for player 1. openblack: `components::ObjectColour` (new) + `Alpha`; **(approximate)**: the specular is not
    painted (the colour path of vs_object does not have it) and the model light (90 + 166 N·L) is that of the PSys
    mesh atoms. **(inferred)**: the local player is PLAYER_ONE.
  - **Each level is drawn twice** with the same matrix and colour: 0x51A780 vt+0x104 and then 0x51A7A3 vt+0x104 or, on
    the last level with arg 1 = 0 (all calls: icons and orbs), 0x51A796 vt+0x100. The object is an
    `LH3DStaticObject` (LH3DObject::Create(0) 0x80B4F8, vtable 0x9A2974). vt+0x104 = fn_00815980: screen test
    (CheckRegionOnScreen 0x868C80) and distance test, then it draws right away (vt+0x108 = fn_0080DB30). vt+0x100 =
    fn_00815A70: the same test, LOD by distance (vt+0x1D0), records g_last_distance / g_last_selected_box and, if the
    object has bit 0x10 of +4 (vt+0x44 = fn_007F97C0), puts it in the Z-sorter (`NewZObject` 0x83F310 with fn_007FA980
    → vt+0x108, key = distance² to the camera, 0x815F0F..0x815F53); otherwise, it draws right away. That bit is set by
    `SetMesh` (vt+0xF4 = fn_007F9E10 → vt+0x40 = fn_007F97A0) when the mesh has bit 0x200 in its flags (fn_007F9D40),
    and `Power_Up_Band.L3d` has it (flags 0xA2200). So: all draws are immediate except the second one of the last
    level, which goes sorted with the transparent ones (with the object's state when the sorter is flushed, which is
    that of the last level: nothing changes it afterwards). Same material and same face mode in both passes (both end
    in fn_0080DB30): there is no back-face pass and no half band. Additive, so each band adds its light twice.
    openblack: two entities per level (`k_DrawsPerBand`, `extraBands` = 2 (pu + 1) − 1). **(approximate)**: the order
    relative to the bubble (immediate ones before, the Z-sorter one among the transparent ones) is not reproduced: the
    2 (pu + 1) go in openblack's translucent pass.
- openblack: `seed_graphic::DrawUpdateAtPos` / `UpdateOnly` / `DrawSpellGraphic` / `UpdateIconGraphics`;
  `one_off::UpdateFrames` (orb) and `worship::Update` (icons) call them every frame. **(inferred)**: also when they are
  not on screen.
- Captures (`dev\_audit\magic\`, `--mod test.miracle-dispensers` with `level=all`): `seed_<semilla>_a/_b.png` (two
  frames, 10 frames apart) and `seed_grid1.png` / `seed_grid2.png` (brightened crops), and
  `seed_land2_icons_650/660.png` (icons of the Land 2 worship site).
- `TownCentre::AddSpell` 0x744050 creates one icon per seed in the first free slot 0..5 of the village centre;
  `TownCentre::MakeFunctional` 0x743E80 does it for all the magic the town already had and then calls
  `WorshipSite::AddTownSpells`. Each village icon asks the worship site for an icon of its seed
  (`fn_0073D1C0` → `WorshipSite::AddSpellIconIfNecessary` 0x77C9E0); when it is removed, the site's one only
  disappears if no other town of the site has that seed (`fn_0077CAA0`).
- **Tapping** (`SpellIcon::InterfaceTap` 0x726430 → `WorshipSpellIcon::ActualInterfaceTap` 0x77F880): if it is already
  full, the seed goes to the hand; if it is being charged, it is cancelled; otherwise, it starts charging. A village
  centre icon forwards the tap to the worship site icon of its same seed (`TownSpellIcon::GetWorshipSpellIcon`
  0x748F30). The tap sound is `G_ClickOnSpell_01` at a pitch of {100, 115, 130, 145, 155, 175} % depending on the slot
  (`fn_00726490`).
- **Charge** `StartCharge` 0x77FA00 / `ValidForStartCharge` 0x77FAB0 / `fn_0077FB40` (packet 0x25). When it fills
  (`GetChantNeeded` ≤ 0): if there is already a seed in the hand its upgrade level is raised; otherwise,
  `PutFullyChargedPowerUpSeedInHand` 0x77F8F0 puts it in the hand **already ready** (`fn_00729900(1)`, the M1
  correction) and the miracle's voice plays (`PlayFullyChargedSoundFX` 0x77F4E0, bank `SpellDialogue.sad`).
- **Bug kept in `AddToChantStore` 0x77FDA0:** below the requirement it returns what it put in; above it, it leaves the
  store at the requirement and returns the **excess** `x − (requisito − almacén)`, and it is that excess that is
  charged to the worship site.
- Returning the seed: `CancelCharge` 0x77F9A0 and `ReturnAllChantsToWorshipSite` 0x77FD60 return the store to the
  battery; `SpellSeed::ApplyToWorshipSite` 0x7289C0 / 0x728B30 / 0x729A80 returns the seed's chants to the site of its
  icon (dropping it on the site's ground, giving it to the totem or to an icon, or shaking it off the hand). If it is
  given to an icon **of another seed** of the same player, that icon hands over its charged seed (the exchange). A
  dispenser, a `WorshipTotem` and any icon are "return points" (`IsSpellSeedReturnPoint`), so
  `SpellSeed::CanCast(objeto)` 0x729190 lets the seed be given to them even if the magic cannot be cast on objects.
- With the game flag 0x2000 (`OPENBLACK_INFLUENCE_EVERYWHERE`) the neutral icons charge by themselves at
  `gatheringChantAddPerGameTurn` (61) per turn.
- The **charge ring** (mesh 561 `MSH_S_PULSE_IN`, `TChargingData::Draw` 0x7267A0) uses the fraction
  `almacén/requisito` (1 with a seed in the hand), shown as `(f+0,2)/1,2`, and when it fills it pulses with
  `alfa = 255·(0,1 + 0,5·(sin(4π t)+1)/2)`.

### The worship percentage and the villagers (`Worship/WorshipPercentage.cpp`, `VillagerWorship.cpp`)

- `Town::SetWorshipPercentage` 0x73C060 (dragging the totem, `TotemStatue::NetworkUnfriendlyLockedSelect` 0x7386A0:
  `pct = clamp(pct + dy × 0,1; 0; 1)`): 0 without a worship site; otherwise it is stored, passed to the totem
  (`TotemStatue::SetWorshipPercentage` 0x738270, which raises it 8 m with a *Zoomer* of |Δ|·5200 ms, which is in ms:
  [engine-math.md](engine-math.md#zoomer-lh3dlib)) and sent to the villagers
  that are missing.
- `Town::GetWorshipersNeeded` 0x73C860: `objetivo = pct > 0 ? max(1; int(población × pct + 0,5)) : 0`;
  `resultado = objetivo − (adorando + en camino) + los que piden volver a casa`.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0: two passes (the second also accepts those flagged 0x200); to send, the
  available villagers **closest** to the dance centre first
  (`fn_0073C590` = `GetDistanceModifier(distancia; distancia del centro a la ciudad + 100) × vida³`); to withdraw,
  those who are at or going to the site, the **farthest** first (state 163).
  - `GetDistanceModifier` 0x74F290 is `SigmoidThreshold(0,5; 1 − min(d; max)/max)`, with the threshold in the **first**
    argument (`push 0x3F000000` at 0x74F2B7): it **decreases** with distance, from 0.99996 at d = 0 to 3.6e-5 at
    d ≥ max (see [engine-math.md](engine-math.md#gutils-distances)). openblack passed them the other way round and
    sent the farthest ones first; corrected in the "sistemas2" session.
  - It is **life³**, not life²: after `GetLife` (0x73C630) the loop 0x73C63A..0x73C644 (`mov eax, 2`, and two rounds
    of `dec eax; fmul vida; jne`) multiplies the life twice more, and the modifier comes in at the end (0x73C646).
- Villager states (table in `LivingActionSystem.cpp`): **59** arrives at the site (0x76BE00; within 10 m of point 9 it
  joins the dance if `N < maxDancersVisible`, otherwise the hiding place), **60** dancing (0x76C680), **213** hidden
  (0x76C5E0) and **248** goes back home (0x761B70). Exits `ExitMoveToWorshipSite` 0x76C170 and `ExitAtWorshipSite`
  0x76C1F0. The original's 58 is the walk along the path (`SetupMoveToOnFootpath`); openblack walks with the WallHug
  inside 59, so 58 is not used. `Villager::CheckNeededForWorship` 0x76BA60 enters from `DECIDE_WHAT_TO_DO`.
  - **Watch out:** openblack's `k_VillagerStateStrings` is wrong at indices 248..254 (it says `RESTART_MEETING`...);
    the `VillagerStates` enum does match the original and is what indexes the table.
- `Villager::ProcessInWorship` 0x76C890 every turn: `CheckVillagerGoBackToTownFromWorship` 0x76BEC0,
  `CheckRequestGoHome` 0x76C8D0 (with life < `damageThresholdToGoHome` 0.3 it signs up in the queue, sorted by the
  life desire `GetLifeDesireFromLife` 0x75BBC0) and `ReduceVillagerLifeByChant` 0x76C800
  (`vida -= chantDamage × chantLifeRate`, 5e-6; on reaching 0 it dies with reason 4 and is counted by
  `GET_TOWN_WORSHIP_DEATHS`).
- `Villager::CanIGetToTheWorshipSite` 0x76BC20: within `maxDistanceThatVillagersWillGoToWorship` (500).
- Not ported: eating at the site (state 241, needs the villager's stomach) and carrying supplies (states 42-46).

### Dispensers and fireflies (`Worship/SpellDispenser.cpp`, `Worship/FireFlyReward.cpp`)

- `SpellDispenser` is an Abode with its magic and its period. `SpellDispenser::Process` 0x722A70: while its orb still
  exists and touches it, it waits; otherwise, every `periodo` turns it creates another (`CreateOneOffSpellSeed` 0x722B80
  → `OneOffSpellSeed::Create` at its position + 1.2 × its height, site visual 9). The default period is
  `timeEachMobileObjectTakesToProduce` = **300** turns; `SET_MAGIC_PROPERTIES` 0x70CC30 and `SET_TIMER_TIME` 0x711280
  change it in seconds (× turns per second) and a period of 0 disables it. Giving it an uncast seed turns it into an
  orb there, losing its chants (`fn_00728C50`, visual 0x1B).
- In **Land1** the dispensers do not come from the land's script, but from the challenge script
  (`GiveSpellDispenserReward`: `CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER)`, `SET_MAGIC_PROPERTIES`, `SET_ACTIVE`,
  `SET_TIMER_TIME`). `CREATE_SPELL_DISPENSER` only appears in Land3, Land5 and the playgrounds.
- **Fireflies** (`FireFly.cpp` 0x52B5A0..0x52B790): when picking up with the hand an object on which a firefly was
  sleeping (`fn_0052B600`, from `GInterface::PlaceObjectInMagicHand` 0x5DA6F0) a one-off miracle is drawn with the
  probabilities of `FIRE_FLY_SPELL_REWARD_PROB`, which **only Land1.txt uses** (HEAL 20; FIRE, LIGHTNING, NATURE,
  FOOD, WOOD and WATER 1 each): `r = GameFloatRand(total)`, the first miracle whose cumulative sum reaches `r`, its
  first seed and level, and an orb if that seed exists (`GSpellSeedInfo.exists`).

### Script (`Magic/Script/CHLWorship.cpp`, the worship part of `MapScriptMagic.cpp`)

- Map commands: `CREATE_TOWN_SPELL` / `CREATE_TOWN_CENTRE_SPELL_ICON` (10 and 12, the same handler),
  `CREATE_NEW_TOWN_SPELL` (11), `CREATE_SPELL_ICON` (13, does nothing, not even in the original),
  `CREATE_PLANNED_SPELL_ICON` (14, only the town's magic type), `CREATE_WORSHIP_SITE` (19),
  `FIRE_FLY_SPELL_REWARD_PROB` (88) and `CREATE_SPELL_DISPENSER` (90).
- CHL natives: 330 `IS_SPELL_CHARGING` 0x70CB80, 331 `IS_THAT_SPELL_CHARGING` 0x70CBD0, 355 `GAME_SET_MANA` 0x6FE800,
  356 `SET_MAGIC_PROPERTIES` 0x70CC30, 376 `SET_CAN_BUILD_WORSHIPSITE` 0x6FEC40, 386 `SET_MAGIC_IN_OBJECT` 0x6FF0B0,
  410 `GET_TOWN_WORSHIP_DEATHS` 0x6FF640, 422 `GET_MANA` 0x6FE8C0, 423 `CLEAR_PLAYER_SPELL_CHARGING` 0x70CD80 and
  453 `GET_SPELL_ICON_IN_TEMPLE` 0x6F3590; plus the dispenser branches of `SET_ACTIVE` (255) and `SET_TIMER_TIME` (145)
  and the `CREATE` types 30 `ONE_SHOT_SPELL`, 31 `ONE_SHOT_SPELL_IN_HAND` and 36 `SPELL_DISPENSER` (`GScript`
  0x6F1010).

### Selection by gesture

The icons seen by M2's selection system are registered with `gestures::SetIconProvider`
(`Worship/GestureIconProvider.cpp`): those of the interface's player, in the order of the lists of its six sites
(`GPlayer` 0x64BAB0..0x64BF40). `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 chooses, among the valid icons of that
seed, the one of the site with the most chants available.

### Captures

In `dev\_audit\magic\` (Land2 with `-s Land2.txt`, Land1 with `-s Land1.txt`; the hooks are in
[openblack-internals.md](openblack-internals.md#debug-environment-variables)):

- `m7_land2_site.png`: PLAYER_TWO's citadel with its two worship sites (the Norse one from `CREATE_WORSHIP_SITE` in
  slot 5 and the Greek one that `PostLoadCleanup` adds for town 2 in slot 0), both at the citadel's origin and rotated
  to their slot, with their food cauldron and their altar.
- `m7_land2_icons.png`: the site's four spell icons (FIRE, NATURE, FOOD, WOOD from town 1) at points 10..13 of the
  `b_worship` mesh, with their `SpellSeedGraphic` and their effect above.
- `m7_land2_dance.png` (`OPENBLACK_TEST_WORSHIP="1,0.5"`): 11 of the 22 villagers of town 1 at the worship site. The
  log gives `site 149 icons 4 N 11 C 33.0 k 0.223 strain -1.000 battery 6824 / 12300 available 6857 damage 0.67`:
  capacity 11 × 3, maximum 9000 + 11 × 300 and the damage per dancer exactly as in the original.
- `m7_land2_totem.png` (the same): town 1's totem raised on its plinth (8 × 0.5 = 4 m) and the village centre's icons
  around it.
- `m7_land2_charge.png` (`OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE;..."`, `OPENBLACK_TEST_MANA=40`, `OPENBLACK_TEST_TAP_ICON="FIRE,5"`): the charge
  ring lit over the FIRE icon while it fills at 40 chants per turn.
- `m7_land2_seed_hand.png` / its log (with `OPENBLACK_TEST_MANA=20000`): with the battery full the icon fills in one
  turn and `Worship: seed 3093 of icon 3086 in the hand with 7000 chants` (FOOD costs 7000).
- `m7_land1_dispenser.png` (`OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826,2670,WOOD"`,
  `OPENBLACK_TEST_FIREFLY_REWARD="1846,2670,3"`): Land1's miracle dispenser with its WOOD orb and three firefly
  rewards. The total of the probabilities is 26 (HEAL 20 + six of 1), as in Land1.txt, and HEAL, WATER and FOOD came
  out.

Known drawing bug: the worship site's `b_worship` mesh (a `ContainsLandscapeFeature`) comes out **black**, because
openblack does not give it the terrain texture it uses. It is not part of the worship system.

### Differences from the original and what is missing

- openblack's worship sites are born **built**: there is no construction work and no `BuildingSite`, so
  `CREATE_PLANNED_WORSHIP_SITE` does nothing and a planned citadel gets its six slots anyway.
- **Dragging the totem with the hand is not wired up** (`percentage::TotemTown` is ready for it): the percentage is
  tested with `OPENBLACK_TEST_WORSHIP`.
- Not ported: the real dance of the `.DAN` files, the site's artifacts, the mana path sprite
  (`CreateManaPathSprite` 0x77B2C0, on the casting side), the supplies to the site, the reward chests (M7b) and the
  stealing of spells by the creature (with M8).
- **Unverified:** player type 3 that cannot have a worship site; the villager's bit 0x200 that the second pass of
  `AdjustWorshipersWorshipping` accepts; `maxDistanceForVillagersToGoToTheWorshipsite` (1000) and
  `minLifeForVillagersToGoToTheWorshipsite` (0.4), which are not read in the ported functions; the counter
  `WorshipSpellIcon +0x114` (nobody activates it in the executable).

## Influence (M1i, `src/ECS/Influence`)

Full research in `dev\documentacion\miracles\influence.md`. All distances are in x,z
(`GetDistanceInMetres` 0x74CD70).

- **Query.** `Influence::CalculatePlayerInfluence(pos, jugador, 0, tipo, aliados)` 0x5CD170 returns -1 to 1; "in the
  influence" is `> 0`.
  - Without a player it gives 0.
  - With the game flag 0x2000 (the registry value "GatheringFlag", `start_system` 0x6433B1) it gives 1 everywhere. In
    openblack it is `OPENBLACK_INFLUENCE_EVERYWHERE`.
  - Then it looks at the virtual influence (`SET_VIRTUAL_INFLUENCE`, not ported; it is the only one that reads `tipo`)
    and afterwards `CalculatePlayerRawInfluence`.
  - If that gives ≤ 0 and allies are requested, it returns that of the first ally with influence (`IsAllied` and
    +0x950 > 0.1). If there is no ally, 0.
  - Who calls and with which arguments:
    - cast rules 2 and 3 (fn_005FB5D0): allies = 1;
    - the hand, `m_InInfluence` (GInterface+0x48, fn_005D1120): with the hand's position, type 1 and allies = 1;
    - `GInterfaceStatus::Process` 0x5DC558: drops the locked object (picking up in batches) outside the influence. It
      is already in `HandResources.cpp`.
- **`CalculatePlayerRawInfluence`** 0x5CD230:
  - it sums the citadel, the player's towns (GPlayer+0xA50) and the rings, and clamps it between -1 and 1;
  - an anti ring of the same player covering the point returns 0;
  - a ring attached to an object that is in the hand does not count;
  - `CameraExclusion::InsideInclusion` is always true in a normal game (it is only used by the camera force field of a
    saved game).
- **Citadel and towns: all or nothing.** They contribute their radius if the point is inside, so the sum goes past 1
  and stays at 1. For the citadel, inside is `r > d` (fn_004630F0); for the town, `d < r` (fn_007479E0).
  - **Citadel.** `Citadel::GetInfluence` 0x464090 = `playerInfluenceMultiplier × Citadel+0x6C`.
    - +0x6C is set once, when the first CitadelHeart is created (0x4649B0): `M2 × (tierra ? storyInfluence[tierra-1] :
      influence)` from GCitadelHeartInfo, that is 125, or 750/450/250/450/450 on lands 1 to 5.
    - M2 is 1 with `CREATE_CITADEL` and the blueprint's scale with the planned citadel. In Land1 it is built by the
      challenge script: `BUILD_BUILDING(1915.05, 2508.89, 1.0)` → `ForceBuildingOfPlannedAtPos` →
      `CreatePlannedNoFixedCheck`.
    - Result: **750 m in Land1**, 450 in Land2 and 250 in Land3 (which is why in Land3 you start with so little).
  - **Town.** `Town::Process` 0x747380 recalculates the radius (+0x5C8) every turn:
    - the base is `Town::GetBaseInfluence` 0x73FD40: GTownInfo's `influence` (25), or its `storyInfluence[tierra-1]`
      (25/25/25/50/25);
    - to that is added, every `processAbodeEvery` (1) turns, the `GetInfluence` of each building in the town, except
      if Town+0x5F8 (last argument of the constructor, 0 in `CREATE_TOWN`; it is not `SET_TOWN_UNINHABITABLE`, which
      writes +0x5F4);
    - the total is multiplied by `townInfluenceMultiplier`;
    - only the player's own towns count: a NEUTRAL one only counts for the neutral player.
  - **Building.** `Abode::GetInfluence` 0x4072A0 =
    `% construido × escala × vida × GAbodeInfo::influence × (adultos +0xB4 + niños +0xB7 + 1)`
    (`MultiMapFixed::GetInfluence` 0x52ECA0 times that factor).
    - `influence` values: houses 5, totem and village centre 90, store 45, workshop and dispenser 25, graveyard
      30, crèche and football pitch 20, wonder 150, field 5.
    - Fields are also Abodes.
    - openblack's `GAbodeInfo::Find` would return the tribeless records at the end (ark 1, totem 120), so the record
      is looked up by the mesh.
  - **Land globals.**
    - The multipliers are 1 by default (GGame::Init). They are changed by `SET_TOWN_INFLUENCE_MULTIPLIER` (case 96:
      Land3 0.5, Land4 0.8, Land5 0.6) and `SET_PLAYER_INFLUENCE_MULTIPLIER` (case 97).
    - `SET_LAND_NUMBER` writes g_game+0x205A08.
    - Land 6 reads the float that comes after the story array.
- **Rings** (`InfluenceRing`, 0x44 bytes; list g_game+0x205C4C, newest first):
  - Fields: position, followed object (+0x28), player (+0x34), radius (+0x38) and anti (+0x3C).
  - They contribute `Influence::CalculateInfluenceOnRange(d, r)` 0x5CD560, with GInfluenceInfo 0.4 / 0.2 / 0.2:
    - 1 up to 0.4·r;
    - from 0.8 to 0 up to 0.6·r;
    - from **0.2** to 0 up to r. The 0.2 is a double at 0x8C7C68, and the jump from 0 to 0.2 at 0.6·r is kept.
  - `ProcessRings` 0x5CDB90: the ring follows its object, and if the object disappears it is deleted with it.
  - `IsInAntiInfluence` 0x5CD490: the point is inside an anti ring of that player (`d ≤ r`).
- **Scripts.**
  - `CREATE_INFLUENCE_RING(pos, jugador, radio, anti)` (case 59).
  - CHL `INFLUENCE_OBJECT` (60) and `INFLUENCE_POSITION` (61): on the stack go anti, player (game index, not
    converted), radius and object or position; they return the ring.
  - CHL `GET_INFLUENCE` (62): on the stack go position, `raw` and player (script one: 0 = the local one, n = n − 1).
    Allies = `raw == 0`.
  - LandT's script opens a 1000 m ring at (2185.6, 2409.5).
- **Drawing: the border** (`InfluenceCircle`, `src/ECS/Influence/InfluenceCircles.cpp` and
  `src/Graphics/RendererInfluence.cpp`; spec `dev\_scratch\coordinador\spec_influence_circle.md`):
  - **The list.** `GGame::Update3DInfluence` 0x555280 (from `GGame::ProcessTurn` 0x54E738) rebuilds it only when the
    dirty byte g_game+0x250174 is set **and** `GameTurn % 10 == 0`: one circle per citadel and per town with influence,
    in the owner's colour (the rings, anti rings and shields are never drawn). The byte is set by fn_00555240 when a
    radius moved by more than 0.01 since the last rebuild (`Citadel::Process` 0x4630C6 against citadel +0x78,
    `Town::Process` 0x74759E against town +0xF24) and by `ForceNeedUpdateInfluence` 0x555270 (a deleted citadel or
    town, a town changing owner).
  - **Overlaps** (fn_00827040, at each `Add`): a circle fully inside another one **of the same player** (3D distance of
    the centres) is deleted; where two of them cross, the columns inside the other go transparent (fn_00827110). Quirk
    kept: a hidden closing column turns white (0x00FFFFFF), so its two segments fade towards white.
  - **The curtain** (`land_morph::InfluenceCurtain`, fn_008265F0): 40 units high, three rows at H, H + 20 and H + 40;
    only the middle row gets an alpha, so it is a soft band that peaks 20 above the land. `burn.raw` / `burna.raw`,
    scrolled +0.0001 u and −0.0002 v per game ms (global clock [0xEB9A40], `frame_anim::InfluenceScroll`), material
    [0xEB9A18] (mode 6, two-sided, tiled: `materials::k_InfluenceCircle`), colours `g_players_color` [0xEA9EFC]
    (`influence::k_CircleColours`, alpha 0; blue and white differ from the generic table 0xBFF0B8).
  - **When.** `InfluenceCircle::Draw(1)` 0x826C90 runs **every frame in the world view** (`GGame::Process3dEngine`
    0x54E3D2..0x54E3DE): there is no option, no hand-proximity test and no fade timer. `WorldRoom::ShowInfluence`
    [0xC2A478] only gates `Draw(0)`, the map in the temple's world room (0x54E3E0..0x54E412). Nothing is drawn while
    g_camera.y ≤ 100; the middle alpha is 120 from y = 200 up and ftol((y − 100) · 0.01 · 120) below. It is drawn at
    once, after everything else drawn at once and before the Z-sorter drain, so every Z-sorted blended thing is drawn
    over it; it writes no Z.
  - **The latch** [0xEB9A1C + 4p]: cleared on every land load (fn_00828A50 from `LH3DIsland::Create`); set by the
    citadel's 3D object when its fade reaches 1 (fn_00883120 0x8831AD). Until then the player's circles are drawn with
    alpha 0 and crossing them makes no ripple and no sound. (inferred) openblack has no temple fade, so it is set as
    soon as the player has a temple (`influence::ProcessCitadels`).
  - **The ripple** (fn_00827250..fn_00827500): crossing a border with the hand (fn_00827820) makes 7 growing rings of
    `smoke.raw` cell 63 in the player's colour, standing in the curtain's plane at the crossing point (bisection
    fn_00827670), for 2 s, Z-sorted; and sound 52.
- **Not ported:**
  - the virtual influence;
  - the allies (openblack has no alliances);
  - the multiplayer rule (without a citadel, 0);
  - the border in the temple's world room (`Draw(0)`, `WorldRoom::ShowInfluence`; openblack has no temple world room)
    and the per-land colour remap `GetRemapedPlayer` 0x64D790 (every openblack user of the player colours takes the
    identity);
  - `CalculateMostInfluentialPlayer` and its helpers 0x5CD4F0 / 0x5CD600 / 0x5CD6C0.
- **Inherited difference.** openblack creates the temple of `CREATE_PLANNED_CITADEL` already built. The original gives
  it the influence when Land1's script builds it, a few seconds in.

## Player alignment (`GAlignment`, GPlayer +0x60; `src/ECS/Effects/Alignment.*`, `components::PlayerAlignment`)

- Value from −1 (evil) to +1 (good) at +0x08 and a pending change at +0x0C. New game: 0 (`GGame::Init` 0x54FEA0
  takes the one from the profile, 0 without one). It lives with the player, not with the land (it is not cleared when
  loading a map): in openblack, one `components::PlayerAlignment` per `PlayerNames` outside the land's registry
  (`Magic/Core/Players`, `AlignmentOf`), the same one the miracles use with `GAlignment::Update` 0x414410.
- **Acts** (`GAlignment::Update` 0x4145A0 for trees): ±`GPlayerInfo::treePullPutAlignmentChange` (0.005), weighted by
  the current alignment (fn_00414660): towards where it already leans it counts `v·(1 − |a|/2)`, against it
  `v·(1 + |a|/2)`; it is added to the pending change. Uprooting with the hand (`Tree::InterfaceSetInMagicHand`) is
  evil; replanting (`Tree::EndPhysics`) and the tree that water plants (`Tree::ApplyWaterSpell`) are good.
- **Every turn** (`GPlayer::Process` → `ProcessForPlayer` 0x4141A0 → `Process` 0x414140; in openblack slot 3 of
  `Magic/MagicLoop.cpp`, `GPlayer::ProcessPlayers`): the pending change, clamped to −1..1,
  times `maxAlignmentChangePerGameTurn` (0.0019444 = 0.7 per game hour) is added (`CrudeUpdate`, clamped to −1..1) and
  the pending change goes back to 0. In other words, the pending change is a **fraction of the maximum rate** of that
  turn: an uprooted tree moves the alignment by about 10⁻⁵ (−0.005 × 0.0019444). That is what the code says; other acts
  (effects, miracles, deaths) contribute much more.
- Script: `GET_ALIGNMENT(jugador)` returns the value; `SET_ALIGNMENT(jugador, v)` **adds** v (`CrudeUpdate`, despite the
  name) and outside −1..1 gives the error "Alignment out of range" without doing anything (`GScript::SetAlignment`
  0x6F99C0).
- Not ported: the history (`CAlignmentHistory`, one global at 0xC4CD40; `AddTotal` 0x415480 and the `Add*` wrappers
  0x414D40..0x4153C0, e.g. `Add(GPlayer*, Tree*, float)` 0x415260). **Retail never reads it**: its only reader is the
  debug overlay 0x414840, which has no caller, and its node lists are never recorded (byte +0x85 is always 0). The
  advisors (`GGuidance::HelpSpritesAlignmentProcess` 0x71CEB0, ported as `audio::guidance::HelpSpritesAlignmentProcess`)
  read the raw per-turn change straight from `GAlignment::ProcessForPlayer` 0x4141A0, not the history (spec
  `dev\_scratch\coordinador\spec_alignment_history.md`).
  The **terrain** alignment (`MapCoords::GetAlignment`, the one for growth
  and the fields) is something else, from the influence of each cell, and is still not ported. Trace:
  `OPENBLACK_ALIGNMENT_TRACE=1`.

### Spell effects (`GAlignment::Update` 0x414410)

- `GAlignment::Update` 0x414410 (R7 resolved): nothing if the life did not change. `K = |Δvida| +
  GPlayerInfo.applyEffectAlignmentChangeAddition`: player 0 stores at +0x64 the pointer to `GPlayerInfo` 0xD47988,
  and +0x1C in memory is file +0x0C. For crush, hit, heal and push, `pendiente += f(v ×
  GAlignmentInfo[i][col] × K)`; for burn, the same with `ConvertTemperatureToDamage`. **fn_00414660 compares the sign
  of the change with that of A** (0 counts as positive): same sign `v(1 − |A|/2)`, opposite sign `v(1 + |A|/2)`
  (read again at 0x414660..0x4146AD: `je 0x414696` if A ≥ 0; in each branch `jne` if v < 0; corrected on
  2026-09-30, the first M1 reading said it only looked at the sign of v).

### The sky alignment (`alignment::GetInterfaceAlignment`)

`fn_0064AC30`, once per turn at the end of `GPlayer::ProcessPlayers` (0x64A697; here in slot 3 of the turn, after
`alignment::ProcessPlayers`): the player with the most influence (`Influence::CalculateMostInfluentialPlayer` 0x5CD630:
the first, in player order, whose influence exceeds that of the previous ones and 0; if none, the neutral one) at the
interface's position, **GInterfaceStatus +0xB0 = the camera's position** (as shown by `UpdateSpellInfo` 0x5DC948,
which computes the camera's front as +0xBC − +0xB0), and `x = clamp((alineación + 1)/2, 0, 1)` is what
`fn_005E2240` receives. It starts at 0.5; `DoCitadelMultiplayer` fixes it at 0.5 (there is no multiplayer).
`Clouds::InfluentialPlayerAlignment` (map one) returns `2x − 1`, except with the `OPENBLACK_TEST_SKY_ALIGNMENT` hook
or the debug slider moved off 0.

## Reactions (`ECS/Effects/Reactions`)

- Reactions (`ECS/Effects/Reactions`, the only module, merged with the animals' one): `CreateReaction` 0x6E3D70 creates
  the 0x44-byte object (radius of the ctor 0x6E39D0: 1 if the reaction grows, otherwise `maxReactionDistance`) and
  spreads it once (`SpreadReaction` 0x6E3E10, the spiral from [animals.md](animals.md#reactions)): each living thing
  in the cell, in the cell's order, goes to the handler of its class (`SetLivingReactionHandler`: animals in
  `ECS/AnimalFlee.cpp`, villagers in `VillagerReactions.cpp`, which dispatches fire and teleport). What is common to the
  living is also there: the records (+0x98, `components::ReactionRecords`), the score fn_006E4620 and the switch rule.
  Before, fire always spread with `maxReactionDistance` (inf) and sorted the cell's villagers by entity; now it uses the
  ctor's radius (in info.dat REACT_TO_FIRE does not grow: 35 m, the same) and the cell's order, like the animals
  (approximate: that of openblack's grid, which is rebuilt once per turn and before an out-of-turn spread, not the
  original's lists). There is one clock, the game turn fixed at the start of the turn (`BeginTurn`, which also removes
  the reactions whose initiator no longer exists); the animals' records also use it.

## Object life (M0, `src/ECS/Life`)

- Life in 0..1 (Object+0x48). Villagers now store it as a float (`Villager::life`, before an integer percentage that
  lost small changes from fire or chanting); rocks and animals in `components::Life`.
- `Living::Living` 0x5EBEC0 starts with `SetLife(GLivingInfo::life)`; `Object::Object` 0x636520 with 1.0.
- `Object::ReduceLife` 0x637810: if the life is less than the amount, 0; otherwise life − amount. Returns the new one.
  Villagers and animals do not override it. It does not kill: the death states are not ported, so the physical impact
  (`HurtByImpact`) kills on reaching 0 as before.
- `Object::IncreaseLife` 0x637870 (and `Villager::IncreaseLife` 0x753460, which calls it): up to 1.
- `Villager::SetLife` 0x756B40 counts in the town (Town+0x714) the villagers below 0.7 life
  (fn_00756BC0 / fn_00756BD0); openblack's `Town` does not have that count yet. `Object::SetLife` 0x63A140 does not let
  objects with flag 0x40 (or 0x200 in a certain interface state) drop below 0.01: which ones is unverified, not ported.

## Fire (M5, `src/ECS/Fire`)

Reports: `destructive.md` §2-4 and §7, `visuals_sound.md` §4.1-4.2, `psys/part_render.md` §8 and §10. Everything below
was read in the exe (W120) except what is marked UNVERIFIED or "(inf)".

The fireball and the lightning, which are what start most fires, are in
[Fireball and lightning](miracles.md#fireball-and-lightning-m5-magicobjectsmagicfireball-psysrulesfireballlightning).

### The heat model (`FireEffect`, `SpreadEffect.cpp` 0x72E940-0x7310F0)

Every object hotter than the air carries a `FireEffect` (0x50 bytes, save type 0x29). There is a global list, newest
first, and `FireEffect::ProcessList` 0x730760 walks it once per turn (0.1 s; slot 6 of `GGame::ProcessTurn`).

- **Object values** (`GObjectInfo` +0xB0 heatCapacity, +0xB4 combustionTemperature, +0x80.. defence multipliers;
  `FireObjectTraits.cpp`): `Tc = max(combustionTemperature, 40)` (fn_00730180), `Tmax = 2·Tc` (fn_007301B0),
  capacity `max(heatCapacity, 1)` (fn_007301D0), ambient `MapCoords::GetTemperature` 0x605CC0 = **24.7 across the whole
  map** (fld 0x930080).
- It **burns** when `T >= Tc` (`IsOnFire` 0x730360); it reacts with `T >= 100` or `T >= Tc`
  (`IsAboveReactionTemperature` 0x730380). The fire fraction (0x7303E0) is `(T - 0,8·Tc)/(2·Tc - 0,8·Tc)` limited to
  `2·vida` and to 0..1; the fire radius is `1,25 ·` the object's radius `·` the fraction (0x72FF10) and the flame height
  `1,25 · altura · (T - Tamb)/(2·Tc - Tamb)` (fn_0072FF70).
- **Per turn** (`fn_0072F5B0`, inside ProcessList):
  - in water it cools 50 times faster; with rain or snow the multiplier is `rainCoolingMultiplier·lluvia + 1`;
  - if it was given heat this turn: `T += 0,1·T/(2·Tc)` with a ceiling of `2·Tc`;
  - otherwise: `T -= (T + 10 - Tamb)·(4·altura·radio)·0,1·multiplicador/capacidad` (the "area" 4·H·r);
  - while burning: damage `(T - Tc)/(2·Tc - Tc) · defenceMultiplierBurn · 0,1` to the life (0x72EEC0), and **charring**
    +0.04 per turn while life < 0.6, capped at `(0,6 - vida)/0,6`; when cooling it drops 0.02 per turn;
  - on death: `DestroyedByEffect` (a creature is not destroyed);
  - **spreading**: spiral of 10 m cells while the cell is within `radio del fuego + 10 m` of the fire centre; each object in
    those cells receives heat (`fn_0072F980`). The wind (fn_00771B10) **is computed and discarded**: it does not move
    the search. An object in the hand only spreads inside the influence of whoever holds it (0x730860);
  - the reaction `REACT_TO_FIRE` (10) is created when passing the reaction temperature and deleted when dropping below
    it; in the hand it is `REACT_TO_BURNING_OBJECT_IN_HAND` (33), `FireEffect::StartedMoving` 0x730A60;
  - **groups**: each fire is born as the root of its group (+0x40 root, +0x44 next); `AddToMyFireGroup` 0x72FBE0 chains
    the new fire right behind the one that lit it, and a root that stops burning passes the group to the first member
    that is burning. The firefighters list (+0x48) is kept only by the root.
- **Sound** (`FireSound.cpp`): only the **2** fires closest to the camera with fraction > 0.1 make sound (2-slot table
  0xDA09CC), a looping `G_Fire` on the object.
- **Visual** (`FireGraphic.cpp`, `PSysBase` 0xD0, fn_00731160/1560/2200): `S_Fire.raw` flames in mode 13 orange
  0xFF713C with cell `int(fmod(-25·edad, 32) + 32)`, additive white steam and grey smoke `S_SpriteSheet3` (cell
  `int(fmod(25·edad, 32))`) in bursts of 30 turns; tint of the burning tree (fn_0074B3A0: grey 50, or
  `max(50, 255 − (1 − vida)·2550)` with life > 0.9, **capped unsigned at the frame's tree brightness
  [0xC22FA0]** (0x74B47B, `ecs::TreeBrightness`, the same one that multiplies a tree without fire at 0x74B077; before,
  the port used 255, and at night the burnt tree came out lighter), grey of the charring (fn_00730570:
  `k = ftol(c·255) & 0xFF` and each channel `((unsigned)(−175k) >> 8) − 1` (0x730585..0x7305D7) =
  **`255 − ceil(175k/256)`**: 255 with k = 0, **80** with k = 255; before, the port truncated and gave 81; `test_fire`
  compares it with the exe's integer code for all 256 k) and brightness `GetFireEffectCharingColor` 0x730480. The light
  map `S_LMFireBall` of the burning object (bit 4 of +0xB5, sistemas session U5) **only exists under the
  MultiMapFixed objects**: the flag `Object +0x24 & 2` (0x7312D5) is set only by the `MultiMapFixed` ctor (0x52E207:
  houses, BigForest, Feature...), not by a standalone tree.
  - **Pending (render)**: while it draws the burning tree, 0x74B4D6..0x74B51E set `OverrideMaterial` [0xECA658] = 1
    and `OverrideRenderMode` [0xECA65C] = `ftol(min(254, 230 + calor·25/255))` (heat 255 if T > 1.5·Tc, otherwise
    `ftol((T − Tc)·255/(0,5·Tc))`; cap 254 [0x99A17C]), and remove them after `AddForDrawing` (0x74B5D8). It is not a
    glow material: the mode functions 0x82E080.. read it as the **ALPHAREF** of the alpha-tested primitives
    (`render_modes::AlphaRef` `forced`), so the burning tree's foliage gets cut out (only the almost opaque texels pass).
    Not ported: the trees are instanced and `fs_object` takes the ALPHAREF per draw (`u_skyAlphaThreshold.y`), so a
    separate draw is needed for each burning tree (`FireGraphic.cpp`, TODO).
- **Villagers** (`VillagerFire.cpp`, `VillagerFireman.cpp` 0x75A3D0-0x75B460 and `ReactToFire` 0x765870): states 215
  `REACT_TO_FIRE`, 216 `PUT_OUT_FIRE_BY_BEATING`, 219 `ON_FIRE` and 220 `MOVE_AROUND_FIRE`. The water ones (217, 218)
  **in W120 give up on the spot** (`DECIDE_WHAT_TO_DO`), so nobody carries water. A villager who is putting out a fire
  receives no heat (fn_0072F980). `SetupOnFire` 0x75B170 stores the previous state and destination and switches to
  `ON_FIRE` with the fire that heats it. **R10** (the decision at 0x765870) is read in `ReactToFire`: the villager looks
  for the fire of the group closest to it that is above the reaction temperature (`fn_00730070`).
  - Firefighter's spot (`GetFireFightingPos` 0x75AA90): on the line from the fire to the villager, at `max(radio seguro, radio
    del objeto)` (0x75AAF2..0x75AB16; before, the port took the minimum, and the villager went back and forth
    216 ⇄ 220 every turn) + the villager's radius (0x75AB23) + `GameFloatRand(1)`. The arrival of `MOVE_AROUND_FIRE` and
    of `GO_TOWARDS_TELEPORT` is `MobileWallHug::AreWeThere` 0x60AD60: strict `d² < (paso +0x5A + extra)²`, the step of
    `RebuildMoveByStep` 0x609D10 = `WallHug::speed` (before, 1 m). Checked: Land1,
    `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, `OPENBLACK_VILLAGER_TRACE=1` (`dev\_audit\magic\fix_firemen.log`):
    13 changes 216 → 220 and 25 220 → 216 over the whole life of the fire (before, 3213 in 650 turns), each villager
    dozens of turns in each state.
  - `Villager::ReactionValidate` 0x756A00 (`villager_reactions::ReactionValidate`): the "validate" column (+0x80) of the
    state table 0xD09198 in the reaction rows (201, 202, 251, 215-218, 220, 6-30, 140-146), which
    `Villager::ProcessState` 0x74FF91/0x74FFD9 runs every turn for the top state (+0x8C) and the saved one (+0x8D)
    before the state: `PopFromPrevious` 0x751E50 if the reaction's object (+0xBC) does not exist or is not available
    (`GameThing::IsAvailable` 0x401810, vt 0x2C), or if the `ReactionInfo` row (0xD4F6B0, `Reaction::GetInfo`
    0x6E4709) asks for `whetherReactionFinishesIfInitiatorInHand` (+0x28) and the object is in the hand (+0x24 & 4).
    `ReactToFire` 0x765870 and `GoToTeleportReaction` 0x7662F0 check nothing more (the first only returns 0 if the
    object is not an `Object` or has no fire, without changing state). Wired up (2026-10-01, V2 merge):
    `LivingActionSystem::VillagerCallValidate` calls it on every row without its own validate whose original validate
    is 0x756A00 (`VillagerOriginalFns.h`); the custom (inferred) exits of `ReactToFire` and `GoToTeleportReaction` are
    gone (details in [villagers.md](villagers.md)).
  - **What takes the villager out of 215 when the object stops burning** (2026-10-02): it is not the state. The fire
    removes its `REACT_TO_FIRE` when dropping below the reaction temperature (fn_0072EFB0 0x72F781), when being deleted
    (`FireEffect::ToBeDeleted` 0x72EC4C) or when moving, with `RemoveAllReactionsOfTypeInitiatedByObject` 0x6E4780,
    which calls `Reaction::ShutDown` 0x6E4720 on each one: +0x34 = 1 and, while there is any follower left (+0x1C),
    `StopReactingAndSetState` (vt +0x99C, 0x5F11C0: `ResetStateAfterReacting` 0x751E10 = `PopFromPrevious` and
    `DECIDE_WHAT_TO_DO` if the final state is a reaction one; then `StopReacting`) of the first in list +0x18
    (0x6E4731..0x6E4743). That way the villager goes back to what it was doing in the same turn, whether it is in 215,
    fleeing towards 215 or putting out the fire. Ported: `villager_fire::ShutDownReaction`, called by `RemoveReactions`
    in `FireEffect.cpp` before removing the reaction (the order of the followers is (inferred): by entity). For a
    `REACT_TO_FIRE` removed some other way (`Pot::RemoveReaction` 0x66D6A0 removes all those of an object; openblack's
    reactions do not keep the list of followers), `ReactToFire` does the same `StopReactingAndSetState` when it sees
    that its reaction is gone (approximate: one turn later). Still not ported: `Living::ProcessReaction` 0x5F1270
    (every turn: reaction not available → `StopReacting`; object +0xBC null or not available, or past the turns of table
    0xC09CF0 for its type → `StopReactingAndSetState`), `TODO` in `VillagerCore.cpp` (mapas session).

### CHL natives (`Magic/Script/CHLFire.cpp`)

170 `IS_ON_FIRE` 0x6FB4C0, 171 `IS_FIRE_NEAR` 0x6F7910 (`FindNearForScript` with the predicate 0x6F7100; a fireball is
not in the cells, so it does not find it), 174 `SET_TEMPERATURE` 0x6FB840, 175 `SET_ON_FIRE` 0x6FB780, 321
`SET_HURT_BY_FIRE` 0x6FDF40 and 426 `SET_SET_ON_FIRE` 0x6FDEE0 (the last two, bits 2 and 3 of Object +0x0A).

### Capture

- Captures in `dev\_audit\magic\`:
  - `m5_tree_fire.png` (`OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"`): the tree burning, charred and with
    flames, and villagers around it; the log shows the spread to villager 30, who flees in state 219 and dies, and from
    there to object 52. `m5_gfx.log` has the trace of the graphic (2 live flames out of the 2 the tree allows, scale
    0.25).

## Time and weather

It is in [day-night-weather.md](day-night-weather.md#weather-and-climate-m6a-srcecsweather) (LH3DAtmos, GClimate, storms, rain and `Weather.h` queries).

## Miracles one by one

Each miracle has its own section in [miracles.md](miracles.md):

- [Food and wood](miracles.md#food-and-wood-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)
- [Water](miracles.md#water-m4a-magicspellsspellwater-psyscreatorsmist)
- [Heal](miracles.md#heal-m4-m4h-magicspellsspellhealcpp-psysruleshealcpp)
- [Forest](miracles.md#forest-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)
- [Flocks](miracles.md#flocks-m4c-magicspellsspellflock-psysrulesflockcpp)
- [Fireball and lightning](miracles.md#fireball-and-lightning-m5-magicobjectsmagicfireball-psysrulesfireballlightning)
- [Shields](miracles.md#shields-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)
- [Teleport](miracles.md#teleport-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)
- [Storm, lightning storm and tornado](miracles.md#storm-electric-storm-and-tornado-m6-storm-magicspellsspellstormandtornado-psysrulesstorm-ecsweatherlightningflashstormclouds)
- [Lightning explosion and missing PSys classes](miracles.md#lightning-explosion-and-missing-psys-classes-m6b-psysrulesexplosionkeypointsorientforestcpp)
- [Creature miracles](miracles.md#creature-miracles-m8-pending)

The particle engine (particle types, class registry, creators, sound and rule index) is in
[particles.md](particles.md).

## Wave 2 review (lane "review2": M2, M3, M5, M6a, M7 together)

Checked against the executable (`dev\documentacion\miracles\impl\review2\`) and with the complete chains in the game.

### Formulas re-read in the exe (they match)

- Gestures: `MatchForward` 0x57A1A0 (turns aligned from any starting point, absorption of small corners, error wrapped
  with fn_0057A150, the two constants of `crt_xc` 0x579DC0/0x579DF0 = 3π/32 × 7/4 and × 2).
- Casting: `SpellSeed::ApplyThisToMapCoord` 0x728E20, `Cast` 0x729520, `DoPreCastThings` 0x729460 (the branch "type 2
  seed → magnitude 1" looks at `GMagicInfo +0x28`, which is −1 in every row: dead) and `SendApplyToMapCoord` 0x5D3340.
- `Pot::AddResourceToPos` 0x66F270: the 9-cell spiral, first list +4 and then +0, `IsCloseToEqual` with
  `Get2DRadius × GetRadiusMultiplierForApplyingPotToPos`, poisoned in arg5 and acceleration in arg6.
- Heat: fn_0072F980 (the firefighter immune, radius, the height check only if either of the two is 3 m or more above
  the ground, `min(10·ΔT, 0,5·calor de la fuente)`, the source loses heat if it is not burning, the group,
  `SetupOnFire` if the villager is not in state 219).
- `UpdateRuleGravityWithFloor` 0x6A1880 (gravity in the air `clamp(v.y + MaxSpeed, 0, 1) × g × gravedad del átomo
  × dt` and the return to the ground).
- `GWeather::CalcAtmos` 0x8400E0 (box, radius², falloff between the two radii, `ftol(f × fundido × 256)`, temperature with
  a wrapping byte add and the other five bytes with saturation).
- The worship site's battery (fn_0077B6A0: intensity `usado/capacidad + max(0,2; 0,5 − batería/máx × 0,5)` up to
  1, `batería − (usado − producido)` with no upper cap, available = battery + capacity) and the tap on an icon
  (`SpellIcon::InterfaceTap` 0x726430 → `ActualInterfaceTap` 0x77F880).

### Fixed in the review

- `Pot::AddResourceToPos` returns `cantidad − lo que quedó` on every path (0x66F511), also when it makes a new pile;
  before it returned the whole amount. Only the hand's log used it.
- `MapCoords::IsWater` 0x6035B0 answers **1** outside the map and where there is no land block (0x603617); the copy in
  `ECS/PotResource.cpp` answered 0, so food or wood dropped on open sea without a block made a pile.
- The one-off seed is tied to the player's best icon (`CreateSpellIntoHand` 0x72A730 → fn_007282A0), as above.
- The one-off orb has its `SpellSeedGraphic` inside (0x72A450) and is deleted with it.

### The size of the fireball cast with the hand (inferred, user's recollection)

`Spell::InitWithPos` 0x71FE50 gives the PSys the magnitude `SpellCastData[0]` without checking whether it is 0
(`PSysInterface::Create` 0x68E910 → `GJPSysInterface::Create` 0x68F3DA stores it in the manager +0xA0, which
`MagnitudeFloatProvider` 0x69DA90 reads). In `SpellSeed::Cast` 0x729520 that value comes from the gesture packet (+0x14,
fn_0071FA10), which is `GInterface` +0x1B8 copied whole into packet 0x12 (`SendApplyToMapCoord` 0x5D362D → fn_00550E90 →
format 15 of `SendPacketCompressed`, a 0x18-byte block, not quantised) and which **only the circle writes** (0x5CF57A
and 0x5D33BA; the `GInterface` is born zeroed). But right afterwards, `SpellSeed::DoPreCastThings` 0x729460 does
`if (magicInfo.spellSeedType == FIRE) castData.magnitude = 1.0` (0x729502..0x72950B): the programmers fixed the
fireball at magnitude 1 whatever the gesture. Read literally, that branch is dead: info.dat leaves `GMagicInfo` +0x28
(`spellSeedType`) at −1 in every row and nothing writes it in the game, so the fireball would come out with the size of
the last circle, or 0 → 0.01 (4 cm, it barely heats) if one was never drawn. The user remembers (2026-10-01) that a
fireball cast from the hand **always came out big**, with any gesture: their recollection rules, and the port applies
the branch with the type of the seed itself (`GSpellSeedInfo`, seed +0x6C) when the row leaves the field at −1
(**inferred**, `SpellSeed.cpp` `DoPreCastThings`). Result: atom scale 1 × 4.0168 of the root sprite, the fireball is
visible in flight and sets fire to the house and the tree where it lands (`fix_fireball_flight.png`,
`fix_fireball_hut.png`). With `SPELL_AT_POS` the magnitude is still the script's radius (10 in `m5_fireball.png`),
because it does not go through the seed.

### Chains tested in the game (captures in `dev\_audit\magic\`)

- Land1, dispenser → orb → seed → cast: `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1812,2652,1"`,
  `OPENBLACK_TEST_TAP="1812,2652,200"`, `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"`,
  `OPENBLACK_TEST_THROW_VEL`: the orb gives the FIRE seed ready (3500 chants), it arms (state 8) and on release the
  spell comes out with its `MagicFireBall` (T 6000). With the earlier 0.01 fireball the barn did not catch fire
  (`review2_disp_fireball.log`); with the FIRE seed's magnitude 1 the house, a tree and the villagers next to it burn
  (`fix_fireball_hut.png`, with `OPENBLACK_CAMERA_FLY=1800,75,2600,1826,30,2641`, `OPENBLACK_MOUSE_AT=0.5,0.55` and
  `OPENBLACK_TEST_THROW_VEL=0,2,6`).
- Land1, food next to the store: the same dispenser with `FOOD`: inside the store's radius (18.5 m) everything goes into
  it (`review2_disp_food.log`); a little further away (`review2_disp_food_pour.png`) it makes a `MagicFood` of 200 that
  grows 18 per grain, with the hand raised 16 m and the 4 s stream.
- Land2, icon → charge → seed → cast: `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE"`, `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="FIRE,200"` and
  `OPENBLACK_TEST_CAST`: `seed 3120 of icon 3082 in the hand with 3500 chants`, armed and cast
  (`review2_land2_cast.png`, `review2_land2_seed.log`). With 3000 chants the icon stays charging with the battery at 0
  (`review2_land2_charge.log`). A one-off orb tapped with chants at the site comes out tied to icon 3081
  (`review2_land2_oneshot_icon.log`).

## Audited assumptions (2026-10-01)

TEAM_GUIDELINES §1.7 audit of everything local/magic adds: 245 findings, 41 corrected to match the original, 52 with the
source added, 139 marked in the code and 13 unchanged (already faithful or from another session). Table per file:
`dev\documentacion\audit_magic\assumptions_audit.md`. What remains marked, by topic:

- **Corrected to match the original:**
  - Spells: a spell without a PSys is cast anyway and ends on the next turn (0x71FE50, step 8).
  - Seeds and casters: `ProcessSpellSeed` always returns 1 (0x721370); a creator without an object is not functional
    (0x405240); the miracle selection sets the power-up gesture to 0 (0x5CF010).
  - Hand: the glow frame is rounded (`fistp` 0x68D323, not `__ftol`) and the wrap is "> 64" (0x68D0C0).
  - Teleport: the SPOT_VISUAL 14 flashes last as long as their entry.
  - Script player: the byte g_game+0x205A5B is the slot of the **neutral player** (7; GGame::SetupPlayers 0x550458,
    GPlayer::IsNeutral 0x64AC00). That is why the script's player 0 and an ownerless magic pile are neutral.
  - Fireball: the fireball bounces off the shields (DoAnyShieldDeflections 0x6A1FA0 from GravityWithFloor 0x6A1F48);
    the non-human cast is resolved again if v² > **0.01** (the double [0x8C7620] of `fcomp qword` at 0x69EC60; read as
    a float it looked like 89129, corrected on 2026-10-02) and the climb exceeds 30° (the double [0x9375F0] =
    0.52370351552963257, `fptan` 0x69EC77).
  - Lightning: the modes go in order (hand, manager, parent; 0x690F88) and the parent's uses a circle, without a cone;
    the forks are only updated with the effect active.
  - UR_WillowWisp: the age of each atom is fraction·dt (0x6A70CC).
  - Fire:
    - The fire reaction does not come out either in the hand or in flight (+0x24 & 0x44, 0x72F729).
    - A burnt Feature is deleted (0x6378E0); a burnt field sets T = 0 and deletes its fire (0x52A010).
    - StartOnFire includes field, mobile static, animated static and fragment (0x52EC60).
    - Bit 0 of the fire graphic is IsMorphWithLand.
    - The rain is cut off only below 0 (fn_008341B0).
  - Villagers:
    - The exit functions receive the next state (ExitPutOutFire 0x752530, ExitReaction 0x7527A0,
      ExitMoveToWorshipSite, ExitAtWorshipSite 0x76C1F0).
    - The shield blocks the fire reaction (fn_0072B990).
    - A villager going to worship does not fight the fire (0x765A6A).
    - Worshippers are no longer counted twice: the worship site keeps the list of its villagers (+0xD4, fn_0077D040).
    - `SetupMoveToWithHug` (0x5F2890) sets TOP and then FINAL (0x752440) in a single shared function
      (`VillagerMove.cpp`).
  - Worship: the seed goes into the hand only if `InterfaceSetInMagicHand` returns 1 (0x5DA77C); the citadel
    influences with factor 1 (0x463240).
- **(approximate):**
  - Reactions: openblack's cell grid and its order; `InBounds` uses the land's extent, not
    MapCoords::InBounds 0x6042C0.
  - EffectValues: Object's `ReduceLife` for all classes.
  - Lightning: the second fork instead of the tree fn_00691F30.
  - Light maps: alpha = RGB maximum, without the ×190 level.
  - Fire graphic: the charring noise is made of two sines (not VLNoise 0x590C30); all fires are updated.
  - Random numbers: storms, rain, fire and the miracles now go through `game_random` (GRand, the PSys and the
    original's CRT); the fireflies and other systems still use openblack's (phase B).
  - Villagers: MOVE_AROUND_FIRE goes straight (GetViaPoint 0x75A440 not ported); the decision to fight the fire
    (0x765870: formula read, without a random term) takes fn_00730290 / fn_007302E0 untraced; FLYING / LANDED are not
    run when landing after a teleport.
  - Gestures: frames with the mouse act as mouse messages.
  - Worship: the count of those going back home (vt 0x8C8 unidentified).
- **(inferred):**
  - Reactions: GetReactionPower = 1 for all (Spell 0x55CF10 and Tree 0x55D8D0 not ported).
  - EffectValues: one hit per object in ApplyEffectToMapPos 0x525100.
  - Seeds: the right hand for the one-off seed; the local player to check the influence of what is carried in the
    hand.
  - Default values of the PSys rules whose ctor was not read (Gravity 10, turns, trail, mesh, gesture 5 s,
    chain of 0.5·scale).
  - Shield: the default castData (40).
  - Worship: the 6 m dance ring and the fallback icon point.
  - Many of openblack's defensive values: out-of-range indices, caps, 0.0001.
- **Pending (TODO with an address in the code):**
  - Lightning: the single-target branch (0x691CF5), the cut-off by shield fn_006D0BC0 and the per-state sound.
  - Reactions: the stealthy branch of 0x6E3E10.
  - Creature: death counter and alignment (+0x11C0, +0x168).
  - Seeds: the MagicFireBall target (0x728A20).
  - Worship: the path along the footpath (58).
  - Fire: the route around the fire.
  - Spell class not ported (creature, M8), which runs as a plain Spell (storm, water and flocks already have theirs,
    wave 4).

### Wave 4 (water, flocks, storm, lightning explosion; lane audit4)

25 findings (table in `dev\documentacion\audit_magic\assumptions_audit.md`, section "Wave 4"): 4 corrected, 1 comment, 7
marked, 11 checked against the disassembly and 2 unchanged.

- **Corrected to match the original:**
  - Flocks: at the end of `SpellFlock::Process` the leader's position goes to the **flock** (+0x14, the centre of the
    domain), not to the spell (0x7234F2..0x723519).
  - Tornado: the pick-up spiral is GUtils::Spiral 0x74D7E0 started with direction 1 (0x6D22E9); the port walked the
    mirror-image one.
  - Storm: the fire-extinguishing reaction is forgotten when it is not available (vt 0x2C, 0x72DBA2), not only when it
    disappears.
  - Loop: fn_0064AC30 (sky alignment) goes after the teleport travellers, at the end of
    GPlayer::ProcessPlayers (0x64A697).
- **(approximate):** the subcollection added mid-step is updated in that step (PSys.cpp); `MoveToBaseGroup` without a
  root collection deletes the atom; the tornado's per-cell vessels come from the registry; the base colour of the mists
  is that of the previous frame; the output of `UR_ForestPath` without keys is 0.
- **(inferred):** the +0x80 of `EventConditionAtomNearVillagers` in metres; the wolf leader's destination before its
  first turn.
- **Unchanged:** `FixedObjectsInMapCell` walks the whole registry per cell (slow with many fixed objects);
  `SetDeathCallback` for animals has a single slot (only the flocks use it).

## Pending

What is missing is in each topic, at the end of its section:

- Worship: where miracles come from: [Differences from the original and what is missing](#differences-from-the-original-and-what-is-missing)
- Influence: the virtual influence, the allies, the multiplayer rule, the border in the temple's world room, the colour remap and `CalculateMostInfluentialPlayer` ([Influence](magic.md#influence-m1i-srcecsinfluence)).
- Casting from the hand: the help, the immersion, the HUD gesture icons, the hand glow and feeding a fireball in flight ([Casting from the hand, gestures and hand effects](magic.md#casting-from-the-hand-gestures-and-hand-effects-m2-srcmagicgestures-srcmagichand-handspellseedcpp)).
- Alignment: the terrain alignment; the history (`CAlignmentHistory`) only feeds a dead debug overlay in retail ([Player alignment](magic.md#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsplayeralignment)).
- Life: the town's count of injured villagers (Town+0x714) and the 0x40 flag of `Object::SetLife` 0x63A140 ([Object life](magic.md#object-life-m0-srcecslife)).
- Dispenser orb: the user accepts the size of the seeds and the height of the bubble (2026-10-01). The reference capture of the original (`dev\documentacion\audit_magic\ref\dispenser_original.png`) is a WATER orb, not a fire one: its sky-blue blotch is the effect of the water seed. What remains (approximate) is that the terrain light and the haze are taken at `posición + facingOffset` and not at the point moved forward towards the camera (Draw 0x518FCD..0x518FF2) ([Seeds and one-off miracles](#seeds-and-one-off-miracles-spellseed-oneoffspellseed)).
- Dispenser broken by a thrown rock: openblack breaks it into pieces like a house; the original draws it with `MultiMapFixed::Draw` (`SpellDispenser::Draw` 0x722940 -> 0x518090). `Abode::ReactToPhysicsImpact` 0x406240 and what happens to its orb still have to be read.
- FOOD and BEAM_EXPLOSION seeds: they are also loaded with material properties (`{1,0,1,1,0}`); apply `L3DMesh::SetMaterialProperties` as for the bubble.
- Vortex between lands (`MagicVortex`, CREATE VORTEX): not ported; on release, fn_005FE3B0 marks `thing+0x25 |= 0x40` at 0x5FE5DD (`script_held::SetCannotBeEaten`).
- Rain in the growth of the trees (`GrowTree`, formula in [trees.md](trees.md)): the weather belongs to Milagros.
- Burning tree: the forced ALPHAREF 230..254 (`OverrideRenderMode`, 0x74B4D6..0x74B51E) needs a separate draw
  per tree ([Fire](magic.md#fire-m5-srcecsfire)). The villagers' `Living::ProcessReaction` 0x5F1270 (mapas).
- `OPENBLACK_TIME_OF_DAY` is no longer applied in Land 1 (the script controls the clock).
- Handover for a new miracles session: `Desktop\B&W\Prompts y detalles.md`, section MILAGROS.
- Fire: the light map `S_LMFireBall` of the burning object, only under the MultiMapFixed objects (sistemas U5,
  [Fire](magic.md#fire-m5-srcecsfire)).
- What the audit marked in the code and the (inferred) size of the fireball: [Audited assumptions](magic.md#audited-assumptions-2026-10-01), [The size of the fireball cast with the hand](magic.md#the-size-of-the-fireball-cast-with-the-hand-inferred-users-recollection).

What is pending for each miracle is in [Pending](miracles.md#pending).

## Test hooks

All the `OPENBLACK_*` are in [openblack-internals.md](openblack-internals.md#debug-environment-variables).
By topic:

- Spell core: [Hooks and traces](#hooks-and-traces)
- Casting from the hand, gestures and hand effects: [Hooks, tests and captures](#hooks-tests-and-captures)
- Worship: where miracles come from: [Captures](#captures)
- Fire: [Capture](#capture)

## Sources

- `dev\documentacion\miracles\`: `PLAN.md`, `core.md`, `casting.md`, `sources.md`, `influence.md`, `destructive.md`,
  `resources.md`, `protect_creature.md`, `visuals_sound.md`, and `impl\review2\` (wave 2 review).
- `dev\_audit\magic\`: the captures and logs cited, and `assumptions_audit.md` (the assumptions audit).
