# The spell core

## Summary

Every miracle is a spell: an object made from its info.dat row when it is cast, which owns a particle effect, pays
for itself with chants every turn and every event, closes when its time or its chants run out, and is deleted when
its effect has finished. Its effects (burn, crush, hit, heal, push, alignment, belief) reach the objects around it
through a common path that also starts reactions, blames the attacker of a town and moves the player's alignment.
Seeds are what the hand holds before a cast; one-off seeds float in orbs made by dispensers and fireflies. This page
holds that common core, the order of a turn's magic work, and the reviews of the whole chains; each miracle has its
own page.

openblack ports the spell life cycle, the chants, the events and effects (with a creature taking them, in part), the
cast rules, the seeds and orbs, the script commands, the spell grid and the turn order faithfully, as one entity per
spell with a table of class functions.

## The original

The addresses on these pages are those of the v1.42 `runblack.exe`, read with the 1.20 symbols (bw1-decomp is built
from 1.20; the patch did not change these parts). They collect what was verified in the executable while porting it.

Where the rest of the magic is:

- [info.dat tables](../formats/info-dat.md#infodat-tables)
- [Casting from the hand, gestures and hand effects](hand-casting.md#casting-from-the-hand-gestures-and-hand-effects)
  and [the gestures](gestures.md#gestures-the-buffer-and-the-recogniser)
- [Worship: where miracles come from](worship.md#worship-where-miracles-come-from),
  [the temple and the citadel](temple-and-citadel.md#the-citadel-and-its-six-slots) and
  [the village totem](village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go)
- [Influence](influence-and-alignment.md#influence), [player alignment](influence-and-alignment.md#player-alignment),
  [reactions](influence-and-alignment.md#reactions) and [object life](influence-and-alignment.md#object-life)
- [Fire](../world/fire.md#fire); the particle engine (PSys) in [the particles](../physics/particles.md)

#### Ported helpers

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

- `GMagicInfo` +0x28 `spellSeedType`, +0x2C `gestureType` and +0x30 `powerupType` are −1, 0 (none) and −1 in every
  record of info.dat, but the game **fills them at load** (`fn_0042B400`, see
  [dispensers and fireflies](worship.md#dispensers-and-fireflies)): for each power-up type, the gesture and level that
  power its first seed up to it (TORNADO 1, storm lightning 0); the base types keep −1.
- `GetMagicInfoText` 0x5FB3F0: the effect info of table[i]'s own magicType, + 0x34 (its `debugString`);
  `GetInfoFromText` 0x5FB3B0 compares against it.
- `GMagicInfo::GetMagicEffectInfo` 0x5FB680.
- The power-up level that the PSys rules read is `PSysManager::GetPowerUpLevel` 0x673510 (storm, fireball rows): the
  filled +0x30 of its spell's `GMagicInfo` (0x67351F), −1 without a spell.
- `GSpellSeedInfo.magicTypes[0]` is at file offset 0x114 (memory +0x124): `fn_0072AF10(0)` reads it (0x72AF1C), the
  same field as `GetMagicTypeFromPULevel(-1)` (0x72AFC9); `fn_0072AF50(0)` gives its `GMagicInfo` [0xD37D10 + 4 ×
  type]; `fn_00727700` gives the seed row (0xD9D678 + type × 0x190).

### Spell core

The 0xEC-byte `Spell`; the original's virtual functions differ per class (the class of the `GMagicInfo` that
allocates it with vt 0x34).

#### Life cycle

`Spell.cpp` 0x71FB40..

- `Spell` is 0xEC bytes (vtable 0x9805B0); its positions are MapCoords as metres (x and z on the map, y the
  altitude above the land). Constructor 0x71FB40 (its fields zeroed by `fn_0071FCF0`): without a creator it sets neither creator nor player; with one, `player =
  creator->GetPlayer()` (or the neutral player), it goes **at the start** of the list (`g_game+0x205BC4`), `+0x48 =
  IsCreature`, `+0x4C` = the player of the creator (or of the SpellIcon) has +0x8E0 == 1.
- `GMagicInfo::CastAtPos` fn_005FB490: without a creator it uses the neutral player; `AllocSpell`, `InitWithPos`; if it
  does not return 1 the spell is deleted. fn_005FB520 does the same with `InitWithObject` if the `castOnObject` flag
  it reads is 1. It reads that flag at `GetSpellSeedInfo(spellSeedType = -1) + 0x118` = 0xD9D600, inside
  GSpellIconInfo[1]: `GMagicInfo::GetSpellSeedInfo` 0x5FB400 = Infos [0xD9D678] + 0x190 × (the magic info's +0x28, its
  seed type), so type −1 gives 0xD9D4E8 and +0x118 is 0xD9D600, the TownSpell Icon info (GSpellIconInfo[1] at
  0xD9D514, stride 0x12C) +0xEC (see Pending).
- `Spell::InitWithPos` 0x71FE50, in order: creature desires (fn_00721730, creature), `originalCastPos`, player
  statistic, `SetChants(castData.chants)` (+0x38 = +0x3C), `maxObjects`, `duration`, pos and castPos, copy of the
  PSysProcessInfo, `dir = info+0x24`, magnitude (`castData.magnitude`, 40 without castData), `PSysInterface::Create`
  at `(x, the ground's height + pos.y, z)`, the player record +0xDC {castPos, magic (+0xC), turn (+0x10)}. With a
  PSys: `psys->SetPlayer`. **Without a PSys and with `particleType != 0` it returns 0**: the spell is not cast, and
  there is no reaction either. Without a PSys and with `particleType == 0`: `SpellEvent{11}`. Afterwards, the reaction
  `createReactionOnCast`.
  - Statistic in `InitWithPos`: player +0xA44, `fn_0056A4D0` (a jump table of `inc`): one more spell of that type.
  - `InitWithPos` also adds a minimap blip when g_game +0x205A28 == 1 (`fn_0079DDF0`); `ToBeDeleted` removes it
    (0x79DD20).
  - `InitWithObject` 0x7200E0: `InitWithPos(creator, the object's +0x14 ...)` and, if it worked,
    `psys->AddTarget(object)`.
- `Spell::ProcessSpells` 0x720300, once per turn: decay of the spell grid, worship sites (fn_0072BF80), icons
  (fn_00727350), `GPlayer::ProcessSpellIcons`, **first all the `ProcessMaintainRequest`** (the loop is
  `Spell::ProcessMaintainRequests` 0x720390), and then, per spell, `ProcessSpellSeed` (vt 0x500) and `Process` (vt
  0x528); a 5 deletes it.
- `ProcessMaintainRequest` 0x7204D0 (always 1): age += 0.1 s; `age > duration` (with duration ≥ 0) → CloseDown; a
  creator that is not functional → CloseDown and creator = NULL; `enabled = 1`; `creator->UpdateSpellInfo`; hand
  spells (castType IN_HAND) move castPos to the hand, quantised to MapCoords (ftol(x × 6553.6)), with altitude 0
  (0x72056B..0x720595). If it is open, **the strength is read before paying for the turn** (`psInfo.power` is the
  previous strength), it pays for the turn and marks the grid. If it is closed: `enabled = 0`, `power = 0`. For a human
  caster, `fn_00720460` / `fn_005D1260` give the interface's "can't cast here" feedback.
- `CoreProcess` 0x720660: if it is open, `Recharge` and, with `power <= 0`, CloseDown. Then one PSys step with the
  PSysProcessInfo; if it returns 5, its reactions and the PSys go away. `Process` 0x720710 returns 5 when there is no
  PSys left.
- `CoreCloseDown` 0x720160: `closedDown = 1` and CloseDown of the PSys (vt 0x118). In `Spell::CloseDown` 0x55CE40 →
  `CoreCloseDown`: the first close-down of a spell cast by this computer's interface stops the hand's grain raise
  (`fn_005B2F10` → `HandStateGrain` `fn_005B2F40`).
- `ToBeDeleted` 0x71FD90: leaves the list, deletes the PSys and the reactions, **also deletes the linked seed** (vt 0xC
  on +0xAC; then `fn_007213B0` clears the link) and then CloseDown.
- Accessors: `Spell::GetMagicInfo` 0x7201D0, `GMagicEffectInfo::GetInfo` 0x720730, `GetParticleType` 0x720130,
  `Spell::IsCastFromHand` 0x721510 (the seed's castType is IN_HAND), `Spell::IsScriptCasting` 0x720270 (the creator is
  the player at g_game +0x205A5B, the neutral one).
- The magic type of a spell is at spell +0xB4 (`SpellStormAndTornado::GetMagicInfo` 0x72DB80 = 0xD37D10[spell
  +0xB4]).
- Plain `Spell` (class General, MAGIC_TYPE 0-9): vtable 0x9805B0, `GMagicInfo::AllocSpell` 0x5FB450. Base virtuals:
  `InitWithObject` vt 0x538 = 0x7200E0, `CalculateCostToMaintain` 0x720810 (= costPerGameTurn), `CloseDown` vt 0x530 =
  0x55CE40 (→ `CoreCloseDown` 0x720160), `HasEnoughChantsAndLifeForRecast` vt 0x548 = 0x55CE00 (`mov al, 1`: always 1).
- `Spell::SetInterfaceStatus` 0x7201F0 (`fn_007201F0`) stores the casting `GInterfaceStatus*` at +0x44. "My interface" means iface ==
  `MyInterfaceStatus`; the check is `NetUnsafeIsMyInterfaceCasting` 0x7201E0.
- Spell class sizes and vtables: SpellHeal 0xEC bytes, vtable 0x8F4A8C, `GMagicHealInfo::AllocSpell` 0x5FBD40 (no
  state of its own). SpellResource 0xF0 bytes, vtable 0x8F6AD8 (AllocSpell 0x5FAC20). SpellShield vtable 0x9828D8,
  `GMagicShieldInfo::AllocSpell` 0x5FBA70. SpellStormAndTornado save type 0x10, `GMagicStormAndTornadoInfo::AllocSpell`
  0x5FBAB0. SpellTeleport vtable 0x8F3FD4, `GMagicTeleportInfo::AllocSpell` 0x5FBDF0.
- Base functions of `SpellWithObjects`: `Process` 0x721290, `CloseDown` 0x721300, `ToBeDeleted` 0x720FD0,
  `ProcessSpellSeed` 0x7212F0; `Spell::SpellEvent` 0x720F40.

#### Chants

0x720750..0x720A90, verified instruction by instruction:

- Safety level 0x720880: the maintained ones (FOREST, SHIELD, PHYSICAL_SHIELD) → `initialChants`; the others
  `max(min(cost per turn × (1000/ms per turn) × 5, initialChants), costPerEvent)`.
- Strength 0x720750: without a creator, 0. With `S > 0`, `chants/S` clamped to 0..1; with `S <= 0`, 1 if there are
  chants left. Then × `GetTribalPower` (0x7216F0, of the spell's player) × the seed's `+0x8C` × `+0xE4`.
- `PayFor(cost, forced)` 0x720990: without a creator, 0; free (+0x5C, set by 0x720820, which nothing calls), 1. With `divideCostsByTribalPower == 1`
  (`fn_00720970` tests it), cost / max(tribal power, 1). Subtracts the cost; if it ends up below the level and
  `isSpellRecharged`, the creator refills the whole deficit (forced) or at most the cost (`fn_00720FB0` adds what the
  creator's `MaintainSpell` gives). Returns the strength.
- fn_00720830 (pay for the turn): **with cost 0 it returns 1 without calling PayFor**. `PayForOneEvent` 0x720A90 pays
  `costPerEvent` (without a creator it returns 0 without creating the mana path point). `Recharge` fn_00720910 refills
  the full deficit.
- Who pays (`MaintainSpell`, vt 0x58): `GPlayer` 0x64C430 gives everything **only if it is the neutral player**;
  otherwise 0, so the spells of a seed without an icon live off their initial chants. `GameThing` 0x56FED0 gives
  everything. `WorshipSpellIcon::MaintainSpell` 0x77F6F0 → `WorshipSite::MaintainSpell` 0x77BC50 / `UseChants`
  0x77BBB0 (the site pays; an icon without a site pays from its own store). `Creature::MaintainSpell` 0x4F8350 (the
  creature's physical energy).
- Lightning cast by a player (real trace): 5000 chants, −50 per turn, strength 1 until it drops below 2500 (turn 50),
  0.98 on turn 51 and 0.8 on turn 60. It closes on turn 61 (the age is a sum of 0.1 floats and on turn 60 it still does
  not exceed 6.0) and the PSys ends on that same turn.
- `SetChants` 0x720FA0: +0x38 = +0x3C = chants.
- `CreateSpellPoint(chants, perTurn)` 0x7213D0 makes the mana path sprites. Only spells whose creator is a worship
  site get them.
- `UpdateSpellInfo` (vt 0x5C): `GPlayer::UpdateSpellInfo` 0x64C470 does nothing for the neutral player. It forwards
  only for a spell that has an interface status (a hand cast) → `GInterfaceStatus::UpdateSpellInfo` 0x5DC8F0, which
  fills +0x00 the status's position, +0x0C the hand, +0x18 the camera's forward and +0x24 the hand's velocity.
  `WorshipSpellIcon::UpdateSpellInfo` 0x77F750 uses the icon's player's when +0x8E0 == 1. `Creature::UpdateSpellInfo`
  0x4F8750 ([casting a miracle](../creature/casting.md#casting-a-miracle-the-originals-steps)).

#### Events and effects

`SpellEvent`.

- `Spell::SpellEvent` 0x720F40 ignores types 1 and 11. `ApplyDefaultSpellEffect` 0x720C30:
  - if it is closed, nothing;
  - the spell moves to the event (and 0);
  - EffectValues of the effect × the strength returned by `PayForOneEvent` (with 0 it is not applied and returns 0),
    × tribal power × `event.strength`. `fn_00720AE0` builds the spell's EffectValues (effect info, applied by the
    creator, the creator's player); `fn_00720B20` scales them by `PayForOneEvent`'s strength and fails when that
    strength is 0;
  - type 4: `SpellHitSpell` with the target; if the other one does not fall, it returns 0;
  - type 7 (after paying for the event): with a target, `CanBeDestroyedBySpell == 1`, no reaction and no direction;
    **without a target it applies nothing but still goes on to the reaction and returns 1** (0x720DB4 jumps to
    0x720EBC);
  - type 5 with a target: if it accepts it, `ApplyEffect` (and the heal one removes poison);
  - the others (and 5 without a target): `ApplyEffectToMapPos` at the spell's position;
  - at the end, the reaction `createReactionOnEvent` and `+0x2C = event.velocity`.
- With `checkShields` it looks for a shield (fn_006D0BC0) and sends **itself** a type 4 event with itself as the
  target (0x720D84); see [the shields' particles](shields.md#the-particles).
- `SpellHitSpell` fn_00720B70: cost = own strength × `costPerShieldCollide`. If the other one has strength 0 → 1. The
  other one pays forced and this one pays one event. If the other one is left without strength and this one with
  strength → `SetUpDestroyedReaction` and 1; otherwise `UpdateStruckReaction` and 0.
- After a hit by a player's spell that is not a creature cast: `GPlayer::ConsiderMakingCreatureMimicPlayer` 0x4EA900
  with the action from `fn_004E9DF0` (creature).
- `EffectValues` (0x40 bytes; Effect.cpp spans 0x524EF0..0x525910): +0x08 the 7 numbers (burn, crush, hit, heal,
  push, alignment, belief), +0x24 the radius, +0x28 who applies it (the spell's creator), +0x3C the player. `*=`
  (0x525720, reached through `fn_00525670`, which is skipped for a factor of 1) only scales the 7 numbers.
  `IsDestructive` 0x5258C0: burn, crush, hit or push > 0. `fn_005250A0` → `fn_005250D0` fill the 7 numbers and the
  radius from a `GEffectInfo` (the base of `GMagicEffectInfo`).
- `EffectValues::GetPlayer` 0x5254C0 looks at AppliedBy (+0x28) first; `GetCausedPlayer` 0x525910 looks at +0x3C
  first, otherwise AppliedBy's player. They only differ when `Abode::ApplyEffectsDueToPhysicalDestruction` sets +0x3C
  to the hand's player while AppliedBy is the hitter. `Town::UpdateAggressor` reads `GetCausedPlayer` (0x73C9BC).
- `ApplyEffectToMapPos` 0x525100: cells of pos ± R; every available object that accepts the effect, with `dist(pos,
  the fire centre) ≤ R + the fire radius` and `|alt(pos) + pos.y − (alt(obj) + obj.y)| ≤ height + R`. No attenuation.
  In `Object` the fire centre is the position (0x639AA0) and the radius is `Get2DRadius` (0x639AC0 → vt 0x64).
- `Object::ApplyEffect` 0x637980 (villagers and animals do not override it), in this order:
  - life0 = `GetLife` (0x63798A); damage = positive crush and hit × the defence multipliers (0x637D00; before that it
    passes the heat to the fire); healing = heal × its multiplier;
  - heal → `IncreaseLife` (0x6379EA), then damage → `ReduceLife` with `EffectValues::GetPlayer` (0x637A24);
  - a life that is now 0 from a life0 that was not (0x637A30..0x637A52): an applier (+0x28) that `CastCreature`s gets
    its +0x11C0 count of objects destroyed raised by 1 (0x637A54..0x637A73), then `DestroyedByEffect(GetPlayer,
    life0)` (vt +0x5F8, 0x637A8A; a villager dies). This comes **before** the crushed reaction, the town attack and
    the alignment, which all see the object as its destruction left it;
  - crush > 0.01 (`test ah, 0x41`, strict, 0x637A9F), `CanBeCrushed` (vt +0x62C) and no reaction of its own
    (`GetReactionInitiatedByObject` 0x6E4870) → REACT_TO_OBJECT_CRUSHED (18), started by whoever applies it as a
    GameThingWithPos (or the object itself) and with the player **of the object** (vt 0x1C, read after the kill; for a
    villager, the owner of its town), stamped (`CreateReaction` 0x6E3D70 with 1). `Living::CanBeCrushed` 0x4172B0 is
    1, so every living thing (villagers, animals, creatures) can be crushed; Fixed and Object answer 0;
  - the town attack (0x637AFE..0x637B7D: with a destructive effect (`IsDestructive`), a town (GetTown vt +0x48), an
    AppliedBy and `ConvertTemperatureToDamage(burn) + damage != 0` → `Town::UpdateAggressor` 0x73C9B0 with
    `GetCausedPlayer`), then the alignment (0x637B82..0x637C05, in
    [spell effects](influence-and-alignment.md#spell-effects));
  - returns `(1 − life0)/healing + life0/damage`.
- `FireEffect::ConvertTemperatureToDamage` 0x72EEC0: 0 below Tc; otherwise `(T − Tc)/Tc × defenceMultiplierBurn ×
  0.1`.
- The reactions (`CreateReaction` 0x6E3D70, `SpreadReaction` 0x6E3E10): in
  [reactions](influence-and-alignment.md#reactions).
- Life reduction is vt 0x5B8 with the effect's player: `Abode::ReduceLife` 0x405D90, `Field::ReduceLife` 0x52A0A0 (no
  change, no building site), `Object::ReduceLife` 0x637810 for the rest, `Creature` 0x47DD00
  ([a creature taking an effect](#a-creature-taking-an-effect)).
- `Object::DestroyedByEffect` is vt 0x5F8: `Object` 0x6378E0 = ToBeDeleted (features too; `CleanupWhenDeleted`
  0x6377F0 does RemoveMapObject vt +0x548); `Villager` 0x7502D0 → `VillagerDead(2 SPELL)`; `Animal` 0x41B1B0 →
  `Living::SetDying`; `Field` 0x52A010: fn_0052A030 sets crops, growth and food to 0 and `SetTemperature(0)`, then
  deletes its fire (vt 0xC, 0x52A024); `Abode::DestroyedByEffect` 0x403F80 (every Abode class: the store, the
  dispenser, the totems and the workshop share the slot) first calls `GoolooGooloo(this)` (0x403F86, the ghost of its
  model), then the script branch (+0x74 site, `Town::AddBuildingSite`, `SetLife(1)` when not built, the +0x44
  dependant `ToBeDeleted(0)`) or the scaffold rebuild / `ToBeDeleted(0)`.
- `Object::FillInEffectDefenceMultiplier` (vt 0x5C8) takes the info's defenceMultiplier* (+0x90..); creatures override
  it (0x478C00). `GetDamageEffect` 0x637D00 first passes the burn to `FireEffect::ApplyEffectToFireEffectIfNecessary`
  0x730670.
- `IsEffectReceiver` (vt 0x774): `Object` 0x4029E0 = 1. `Villager` 0x751D70: with an EffectValues whose +0x14 (the
  heal) is above 0, not dead (vt +0xAF4, 0x751D8A..0x751D94); otherwise vt 0x530 `Villager::IsReachable` 0x756460:
  available (vt +0x2C), not at home (+0xE0 & 4), not in the hand (+0x24 & 4) and top state +0x8C not 236
  GO_AND_HIDE_IN_NEARBY_BUILDING (0x756465..0x75649A). Other classes with their own: TownDesireFlags 0x55DAB0,
  TotemStatue 0x561130, Totem 0x561520, MagicTeleport 0x5FCE00, LandscapeVortex 0x5FFFA0, ScriptHighlight 0x7097E0,
  SpellSeed 0x728F50, OneOffSpellSeed 0x72A520, MagicShield 0x72C310, PhysicalShield 0x72CC80, WorshipSpellIcon
  0x77F0B0, WorshipTotem 0x7807A0, ArenaSpellIcon 0x4256D0. `Pot` 0x66D650 receives a burn only with something in
  it (+0x70); `Field` 0x528900 only while ValidForPlaceInHand (vt 0x6FC; inferred: while it has food to give).
- A villager's `GetPlayer` (vt 0x1C) is its town's owner (`Villager::GetPlayer` 0x7502F0).

#### A creature taking an effect

- **`Creature::ApplyEffect`** 0x478C80 (vt, 0x39A bytes):
  1. `GetDamageEffect` (vt +0x5D0, which first passes the burn to the fire). A harmless effect (damage not above 0)
     whose applier (+0x28) is not the creature, while it fights, from anything but its own player (vt 0x1C) → 0
     (0x478CA9..0x478CD2). A harmful one applied by the creature itself → 0 (0x478CD5).
  2. **Fighting** (0x478CE8), unless the bytes [0xC6414C] or [0xC6414D] (not named) are set: blocking (fn_0048A580) →
     the effect's numbers × 0.1 in place (`EffectNumbers::operator*=`, 0x478D24); the reel fn_00484790(0xB8); a heal →
     `TimeWarpHeal(ftol(heal × 9600))` (0x478D7D) and the fight health (`LH3DCreature` +0x4AA8) += heal, kept to 0..1;
     harm → the fight health −= it; at 0 or below, with an opponent (+0x528C): its plan (desire 0x18, action 0x27, or
     0x13 when its +0x9C is below −0.5), mind +0x1C14 = 1, its wins +0x17C up by 1, its +0x10B0 = 1; then the
     creature's own +0x10B0 = 1 and `Faint` 0x476FA0 (with no opponent: +0x10B0 and `Faint`).
  3. **Not fighting**: `BeCutAndScarredByEffect` 0x478890.
  4. Always: a heal (`GetHealEffect`, vt +0x5D4) above 0 → `TimeWarpHeal(ftol(heal × 9600))` (0x478F2D, 0x478F42): the
     marks on its skin mend; in a fight a second time.
  5. An applier that is a `Creature` (`RTDynamicCast` 0x478F5D): at most every 600 turns (the target's mind +0x18C3C
     holds the turn) `UpdateHowNiceIsCreature(applier, the positive numbers 0..3 × the table at 0xC4CE38, 0x48
     apart)`.
  6. Fighting → returns 1.0 with no change to its life (0x47900B). Not fighting → `Object::ApplyEffect` 0x637980,
     which calls `GetDamageEffect` again, the class's `ReduceLife` / `IncreaseLife`, `DestroyedByEffect`, the
     alignment and the town's aggressor.
- **`Creature::FillInEffectDefenceMultiplier`** 0x478C00: the seven multipliers of the species row (+0x90..); burn,
  crush, hit and fly away (i ≤ 2 or i == 4) are divided by `clamp(LH3DCreature +0x90, 0.001, 2) × 1.5 + 1` (a size
  below 0.001 or unordered is 0.001, above 2 is 2).
- **`Creature::ReduceLife`** 0x47DD00: with an opponent (+0x528C) the fight health takes it too (at 0 or below the
  same knock-out as above), then always `Object::ReduceLife` 0x637810. **`Creature::IncreaseLife`** 0x47DE20: with an
  opponent the fight health + the amount (at most 1), then `Object::IncreaseLife` 0x637870.
- **In a fight** (`IsFighting` 0x47B1C0) is "the 3D body has an opponent", `LH3DCreature` +0x528C. It is set by fn
  0x484410 (0x48447C), which the fight's start sub-state 0x5036C0 calls as the duel's first body action begins (the
  fight start animations 0x7A..0x7C), and cleared by fn_004845F0 (0x4845FC) and `StopWhatIAmDoing` (0x475B58).
- **`Creature::DestroyedByEffect`** 0x476F70: when +0x1158 is set, `Faint`; else `SetLife(1.0)` (vt +0x5B0). It
  returns 1.
- **`Creature::CanBeHealedByHealSpell`** 0x47DF40 = 1. `CanBeDestroyedBySpell` 0x47B1E0 = 0. `ApplySingleEffect`
  0x479020 is the Object's.
- `BeCutAndScarredByEffect` 0x478890: damage above 0 (and a test against 1), an applier whose `CastPlayer` is not its
  `GetPlayer` (the player test 0x4788DA..0x4788F6 guards only these) → `ChangeSource` twice (fear and anger from the
  damage, 0x12 and 9), then marks on the skin (0x47891E on) with `GameRand` (0x478A3D, 0x478AA0, 0x478AD7, 0x478B9B)
  and `GameFloatRand` (0x478AF6, 0x478B16, 0x478B32) draws and fn_00486390, which paints into the skin's buffer
  (`GetSafeBuffer` 0x4842B0). The marks run for any harm out of a fight but the creature's own (its own player's
  fireball included) and also depend on where it is hit. Not read to the end.
- `Creature::Faint` 0x476FA0 makes no draw of its own, but it reaches `GameRand` through
  `ForceActivityAndForceAction` → `FinishActionUnsuccessfully` → `FinishActionGeneric` → `StopWhatIAmDoing` (in
  `StateSet` 0x484F4D and `GetNthActionForPlan` 0x4F1909), each behind conditions, and through virtual calls not
  followed.

#### Cast rules

- In the `GMagicInfo` vtable the symbols have swapped names:
  - **vt 0x30 is the check at a position**: base 0x5FB420 = 1; heal 0x5FBD20 = `FindTargets`; resources 0x5FBA00 =
    land; creature 0x5FA7E0 = 0; forest 0x5FAE80; teleport 0x5FBE50;
  - **vt 0x2C is the one for an object**: base 0x5FB430 = vt 0x30 at its position; resources 0x5FAC00; forest
    0x42D8E0 = 0; creature 0x5FA7F0.
- The rule fn_005FB5D0: inside the map (10 m cell < size) and, depending on `castRuleType`: 0 always, 1 land, 2
  influence `> 0`, 3 both.
- `GMagicHealInfo::FindTargets` 0x5FBB00: R = `dummyVar` (10 / 35) and maximum `maxToHeal` (20 / 100), both × the
  tribal power if there is a spell. It walks `ceil(2R/10)²` cells in a spiral (GUtils::Spiral 0x74D7E0, table +x, +z,
  −x, −z) and counts their living mobile objects that accept the effect and `CanBeHealedByHealSpell`, closer than R.
  **It does not check whether they are missing life**: any living one counts. With a spell, each one becomes a target
  of its PSys (through `fn_00720140`, psys->AddTarget, vt 0x114).
- **`SPELL_AT_POS` checks nothing**: the creator is the neutral one and the check flag goes to 0. Moreover
  `SpellHeal::InitWithPos` 0x72D870 does not look at how many it found. So a script heal is cast even if there is
  nobody. Only the hand asks (`SpellSeed::CanCast` 0x729150, entered at `fn_00729120`: the cast rule for the seed's
  player, then vt 0x30 with the seed's creator).
- `GMagicCreatureSpellInfo` vt 0x2C 0x5FA7F0 (on an object) accepts a `Creature` (`RTDynamicCast`) for which
  fn_004F5230 on its `CreatureReceiveSpell` (+0x370) is false: fn_004F5230 is `queue count (+0x18C) > 5`, the queue
  (+0x184, empty from the constructor 0x4F5240, growing 10 entries at a time, +0x190) of spells waiting for another of
  their kind to finish (fn_004F50E0 pushes one, from fn_004F4DC0 and fn_004F4F30; fn_004F4E60 starts and removes
  them). It is **not a test of the creature's mind**. At a position (vt 0x30 0x5FA7E0) it is always 0. The 1.20
  symbols of the two `CanCast` overloads are swapped: the one named for a `MapCoords` is 0x5FA7F0, the object's.
- **The class rules of the miracles a creature casts from above** [V]: FOOD, FOOD_PU1 and WOOD are
  `GMagicResourceInfo`, whose vt 0x2C `CanCast(Object*)` 0x5FAC00 is `GMagicObjectInfo::CanCast(MapCoords)` 0x5FBA00
  on the object's +0x14, `MapCoords::IsLand` 0x603720; WATER keeps the base `GMagicInfo::CanCast(Object*)` 0x5FB430,
  which is its vt 0x30, `GMagicInfo::CanCast(MapCoords)` 0x5FB420 = 1.
- **A creature's cast** (`Creature::CastSpellOnObject` 0x4F7970,
  [casting miracles](../creature/casting.md#casting-miracles-what-openblack-does)): the class's rule on the object
  (vt 0x2C); `Creature::DestroySpell` 0x4F81F0: a held spell with a duration below 0 closes down (`Spell::CloseDown`
  vt +0x530). The original's release callers: the two cast functions, the cast sub-action's Finish 0x500E1C,
  `StopWhatIAmDoing` 0x475AAE, a fight blow landing 0x48B239 (the opponent's), the fight's Cast state 0x4855B6 and the
  hand's start of a locked select on the creature (`NetworkFriendlyStartLockedSelect` 0x5DA950, at 0x5DA9EC).

#### Seeds and one-off miracles

`SpellSeed`, `OneOffSpellSeed`. `SpellSeed` is an Object of 0xA0 bytes (vtable 0x981FC0), made from a worship icon
(0x727FF0) or from a seed info (0x7280A0), both through `fn_00728140`. A creature spell is `SpellCreature` 0x5FA790.

- **fn_00729900 works the other way round from its first reading**: with 0 → `+0x90 = 1` (ready); with another value
  → `+0x90 = 0, +0x94 = 0`. The worship icons pass 1 (the seed waits `delayBeforeSeedActive` = 1.5 s) and
  `CreateSpellIntoHand` passes 0: **a one-off seed is ready as soon as it reaches the hand**. Moreover
  `InterfaceSetInMagicHand` 0x728810 already sets +0x90 = 1.
- `CreateSpellIntoHand` 0x72A730: with the hand free, it looks for the player's worship icon for that seed
  (`GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40, which asks for an icon from which the spell can be requested
  **with chants available** at its site) and, if there is one, creates that icon's seed (fn_007282A0: its creator is
  the icon); otherwise, the loose seed (fn_00728300). It marks it "ever enabled"; `+0x72 = 1`; it charges it for free
  with its whole cost; it puts it in the hand, ready. In Land1 there are no icons, so it always comes out loose.
  `CreateSpellIntoHand` and the fireball's catch require `GInterfaceStatus::IsHandReadyForObject` 0x5DC890
  (`IsSpaceInHands`: nothing held; the bit 0x20 of +0x24 that it also tests is not identified). `fn_005DC830` returns
  the seed in the player's hand.
- `InterfaceSetInMagicHand` 0x728810: `SetPowerUp` of the current level. Without chants to recast (or with bit 1 of
  +0x54) the seed is deleted (3); otherwise it clears +0x98, +0x70 and +0x94 and is left ready. For my interface
  (0x72881D..0x728859): HelpProfile StopSpell (13) when the seed still has its spell (+0x60), otherwise GetSpell (12).
  `fn_005DCA20` stores the interface's last seed type, which the R gesture repeats; `fn_005DCA40` reads it. The turn it
  also stores (status +0x38) has no known game reader ([the hand](../hand/animation-and-placement.md)).
- `ProcessInHand` 0x729930 (every turn in the hand): +0x94++; ready when `turns × 0.1 > 1.5`; if its spell has closed,
  the seed is deleted. On the seed's first turn in the local interface's hand (`IsInterfacePowerUpWhenInHand` = 1):
  `SetupPowerUpGestures` `fn_005CEE30` (0x72993F..0x72997B, also needing vt +0x734 ≠ 0). It also calls
  `Object::ProcessInHand` 0x639AD0. While not ready (+0x90 = 0), when turns × `GGameInfo` +0x40 (ms per turn, `fimul`)
  × 0.001 > `GSpellSystemInfo` +0x24 (delayBeforeSeedActive) (0x72999B..0x7299C6) it sets +0x90 = 1 and, when the
  seed's interface status is the local one (`MyInterface` +0x39C, 0x7299CF..0x7299E9), calls fn_005D1260
  (`GInterface::EndAction`) on that interface (0x7299C8..0x7299FB): once, on the turn it becomes ready.
- `StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200 (if the spell is still tied to this seed,
  CloseDown of the spell; otherwise, only of its PSys, `fn_00720190`), `ProcessFromSpell` 0x728F70 (always 1) and
  `Cast` 0x729520. Casting from the hand is on [its page](hand-casting.md#casting-from-the-hand).
- Seed accessors: `SpellSeed::GetMagicInfo` 0x7290F0 (the magic at its power-up level, the base type for an empty
  level); `GetChantNeededForSpellSeed` 0x729860 = costToCreate(pu) − the store; `SetChantStore` 0x729A50 (symbol
  "GetChantStore": +0x74 = +0x78); `AddToChantStore` 0x729A30; `SpellSeed::GetMesh` 0x729850; `IsSpellCastInHand`
  0x729820; kept in hand 0x729840; `DoPostCastThings` 0x729260 (`fn_007281A0` links the spell).
- `SpellSeed::GetPower` 0x7298B0 = min(store / costToCreate, 1). It divides with no test: store / 0 is inf or NaN, and
  the min against 1.0 [0x8AA390] then gives 1.
- Seed init: the common init `fn_00728140` zeroes +0x74/+0x78/+0x7C/+0x80/+0x84. The ctor 0x7280A0 then stores 0 in
  +0x7C/+0x84/+0x74/+0x78 and −1 in +0x80. Only `StoreChantsAndAgeFromSpell` 0x728780 writes +0x7C = −1 ("none").
- `SpellSeed::SetPowerUp` 0x729B30: a charge above the new level's cost goes back to the worship site. For the local
  interface, `fn_00729C40` plays SpellDialogue sample 10 / 11 / 12 for PU 0 / 1 / 2 (jump table 0x729C80; none for −1,
  0x729C48): `GAudio::PlaySoundEffect` 0x429D60(NULL, sample, mode 2, loops 0, 0, is3D 0, AUDIO_SFX_BANK_TYPE 9).
- `DoPostCastThings` 0x729385..0x7293E6: if the seed is my interface's, `HelpProfile::Trigger` CastCreatureSpell (10)
  for a `GMagicCreatureSpellInfo` (vt +0x38 non-null), otherwise CastSpell (9).
- `fn_00728FC0` (the seed follows its spell): `SpellSeed::InsertMapObject` 0x728F30 is empty, so the "not in the map"
  test (vt 0x178) always passes. A NULL spell (+0x60) passes the "spell open" test (0x728FF6).
- `SpellSeed::GetPlayer` 0x729800 is the player of the interface status (+0x64), NULL without one.
- `ClearSpellIconLink` 0x7281D0 → `RemoveFromSpellIcon` 0x7281C0 (`fn_0077F7D0`).
- The seed's 2D radius is `Object::Get2DRadius` 0x638180 (vt +0x64, 0x6022D9) of the info's mesh (`GetMesh`
  0x729850). The Game3DObject +0x40 keeps that mesh even while the seed is not drawn.
- `OneOffSpellSeed::Create` 0x72A2F0: seed 0..29 (`cmp -1`, `cmp 0x1E` at 0x72A2F8 / 0x72A301); `fn_0072A3A0` makes
  the `MobileObject(pos, info 0xD39F3C, 0, 0, scale 1)` and `fn_0072A400` takes the Create arguments; then
  `CallVirtualFunctionsForCreation` 0x72A450 sets the shared mesh and, at 0x72A4C4 (unless the object flag 0x100,
  never set on a new orb), makes the seed graphic inside, `SpellSeedGraphic::Create` 0x726F60(pos, seed, the local
  player g_game +0x205A5B, 1.0, the power-up +0x78), kept at +0x70 with `SetAutoUpdate(0)`. The orb is always
  drawn at scale 1 and +0x6C keeps the scale that will be passed to the seed.
  - Shared mesh `.\data\spells\meshes\O_Bibble_up.l3d`: a dome from 0 to 4.5 m above the ground, with UVs in 0..0.25
    (a 4×4 atlas).
  - `UpdateFrame` 0x72A570: `phase = fmod(phase + ms × 18 × 0.001, 16)`, frame = int(phase), offset `u = (frame %
    4)/4`, `v = (frame / 4)/4` (vt 0xE8 receives (u, v)).
  - When it is created, the orb enters its map cell at once: MobileObject 0x607150+0xA9 → `Object::InsertMapObject`
    0x636740. Its info (GMobileObjectInfo 25, 0xD39F3C) is type 20 MOBILE_OBJECT, so the orb goes at the head of the
    mobile list (0x636830).
  - `OneOffSpellSeed::ToBeDeleted` 0x72A420 deletes the `SpellSeedGraphic` inside.
- **The orb is additive** (faithful, corrected on 2026-10-01 with the capture of the original). The mesh has a physics
  submesh (`Smooth`, not drawn) and the visible one, the cap, an `AlphaTextured` primitive (type 4, byte +5 = 5:
  two-sided and repeat) whose skin 0xF49809BD ARGB4444 is a dark turquoise orb (51, 119, 136) with a white highlight
  at the top left, almost opaque (alpha 13-15 of 15, or 0 outside).
  - But the file is not what decides: `CallVirtualFunctionsForCreation` 0x72A450 loads it with
    `GJUtils::GetSharedMesh` 0x57DFB0 and `MaterialProperties` {1, 1, 0, 1, 1} (bytes at 0x72A474..0x72A485). Byte +3 =
    1 makes `PGetSharedMesh` (0x57DF18) call fn_0057E1D0, which applies `GJUtils::SetMaterialProperties` 0x57E120 to
    all the primitives when the mesh is loaded:
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
    day it comes out almost white and pearly: the turquoise texture turns sky blue and the highlight, saturated white.
    The background shows through with green and pink tones.
  - To sort it in the Z-sorter, `Draw` moves its position forward towards the camera by its radius (vt 0x60) and then
    restores it. That way the orb is painted after the seed inside. `DrawSpellGraphic` receives the high byte of the
    diffuse (0x95) as alpha. The terrain light and the haze are taken at the point moved forward (Draw
    0x518FCD..0x518FF2).
- **The orb always faces the camera.** `Draw` calls fn_00518720 every frame, active while the byte [0xBE8E8D] is 1 (it
  is). This rotates the object's 3D matrix around the centre `c` of the mesh's box (`LH3DMesh::ComputeBoundingBox`
  0x8081B0 at load time, all submeshes; here (0; 2.23; 0)):
  - `D = normalize(centre − camera)` and `U = normalize(Y − (Y·D)·D)` (Gram-Schmidt with (0, 1, 0), static
    0xCC62D0);
  - it builds the matrix with rows (U×D, −D, U), inverts it (fn_007FB3F0), scales it by +0x44 and sets the position to
    `centre − M·c`. That way the mesh's +Y points at the camera.
  - The visible submesh is only the top cap (y from 2.18 to 4.46, radius 2.28), so a round bubble is seen from any
    side. The physics one is a whole sphere from 0 to 4.37 and does not change when rotating.
  - Only the 3D object's matrix moves, not the object's position. The 4×4 animation does not depend on the rotation.
  - `Draw` draws nothing if +0x70 (the SpellSeedGraphic) is 0. `CallVirtualFunctionsForCreation` 0x72A450 creates it
    (except with object flag 0x100, which a new orb does not have): `SpellSeedGraphic::Create(pos, seed, the local
    player, 1, pu)` and `SetAutoUpdate(0)`; `ToBeDeleted` deletes it.
  - **The pivot is the box centre through the matrix the orb is drawn with.** fn_00518720 takes `W = M·c` with the 3D
    object's current matrix, rotation included (0x518733..0x5187B8), not the position plus `scale × c`. While the orb
    flies that matrix is the body's pose written every frame by fn_00646FE0 ([physics](../physics/physics.md), "Drawn
    between turns"), which tumbles; in the hand it is the matrix the hand draws it with (`CHand::DrawTheHeldObject`
    0x46DF00, then `DrawOutOfMap` 0x518C50, which calls fn_00518720 as `Draw` does, 0x518C7B). The new translation is
    `W − R·s·c` (0x518B5C..0x518B87), so the bubble's centre stays at W. The seed inside is placed by
    `GetSpellGraphicPos` 0x72A840 from that new matrix, `R·c + T` = W, size +0x44 × 0.6 (0x72A904): the seed is always
    at the bubble's centre and they fly together.
- `InterfaceTap` 0x72A640: `CreateSpellIntoHand`, immersion 0xE, sample 0x6D (`G_SpellBubblePop_04`) and the orb is
  deleted (3). The sound (0x72A6A5..0x72A6F4): bank InGame (GGlobal +0x3AC), owner the orb, sample 0x6D, is3D 1, track
  0, at the interface status's +0xC8 (the hand's position), `GAudio::PlaySoundEffect` 0x429E30.
- **With the real hand** ✅ faithful:
  - The object under the cursor (`SendObjectDrawCollision` 0x5D56C0, exact triangle) reaches `ActionPressed`
    fn_005D1330 → `StartGrab` 0x5D1740 if `ValidForPlaceInHand` (vt 0x6FC) or `InterfaceValidToTap` (vt 0x740). The
    orb has both: it is a `MobileObject` (`Mobile::ValidForPlaceInHand` 0x425B00 = 1) and `InterfaceValidToTap`
    0x72A630 = 1.
  - Pressing on it starts the grab (state 13). If it is released before 225 ms (`State_Grab` 0x5D5250, 0xE1) it is a
    **tap**: `Tap` 0x5D3930 → 0x5D38A0 → packet 0x20 → 0x5DA650 → `InterfaceTap`, and the charged seed goes to the
    hand.
  - If it is held down, **the orb itself is picked up**: `GenericPickup` 0x5D2800 (packet 0x13) →
    `PlaceObjectInMagicHand` → `InterfaceSetInMagicHand` 0x72A530 (it only marks the magic as enabled). It is carried
    as a `MobileObject` (`GetHoldType` 0x607120 = 6, `Object::GetHoldRadius` 0x638C00) and is dropped or thrown with
    physics: constants 9 (`GetPhysicsConstantsType` 0x72A920) and the `GMobileObjectInfo` info 25 (0xD39F3C). The
    dispenser no longer sees it in its place and makes another one when recharging.
  - The tap and the grab require the hand to be inside the player's influence
    (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1; `m_InInfluence` from fn_005D1120, type 1). Outside it
    nothing happens.
  - Selection volume: the mesh as it is drawn, rotated towards the camera (fn_00518720), so the dome that is seen from
    any side counts.
  - The icons of the worship sites and of the town centres (`Object::ValidForPlaceInHand` 0x402870 = 0) are tapped on
    press (`StartGrab` → `Tap` immediately), with the same influence rule.
  - The hover tooltip text (fn_005D6D70: on an orb, `GetOverwritePickUpToolTip` 0x72AC50 = the text of its magic
    +0x110; the tap one is 0xEF7) and the `GameThingClicked` record of fn_005D36D0.
- Map script (fn_00715150):
  - case 83, `CREATE_ONE_SHOT_SPELL(pos, seed)` → Create(pos, the seed by name, −1, 1);
  - case 84, `CREATE_ONE_SHOT_SPELL_PU(pos, magic)` → the first seed of that magic and its level
    (`GetPowerUpFromMagicType`).

#### CHL script

- `SPELL_AT_POS` 0x70C190 pops curl, duration, radius, from, to and magic. `CastSpellAtPos` 0x70BD60 builds castData
  {radius, initialChants, duration, −1} and the PSysProcessInfo {+0x0C from, +0x18 to − from, +0x24 dir (0), power 1,
  +0x34 curl, active}. `SPELL_AT_THING` 0x70BFA0: with an Object, cast on the object.
- `SPELL_AT_POINT` 0x70C560 casts nothing: it returns the first spell of that magic closer than the radius
  (fn_007217A0). For the shields it calls fn_0072BA00.
- `SET_PLAYER_MAGIC` 0x70C6C0 (player, magic, enable) → `SetMagicTypeEnabled` (the counter of who has it).
  `HAS_PLAYER_MAGIC` 0x70C750 → "ever enabled", and **1 if the player does not exist**. The script's players are n − 1
  (0 = the neutral one; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0).
- `PLAYER_SPELL_CAST_TIME` 0x70C9A0: seconds since the last cast (FLT_MAX without a player). `PLAYER_SPELL_LAST_CAST`
  0x70CA50: its magic. `GET_LAST_SPELL_CAST_POS` 0x70CAB0: its point. `GET_MANA_FOR_SPELL` 0x70CD40: `costToCreate`.

#### PSys linked to the spell

How the spell owns its effect and advances it (`StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, event 3 of
`LandscapeCollide`): in [PSys linked to the spell](../physics/particles.md#psys-linked-to-the-spell).

#### Spell grid

`SpellGrid`: `u8[64][64]` at 0xD9C370 (80 m cells). `MarkSpellGrid` fn_00721570 sets 0xFF where there is an open
spell; fn_007215C0 lowers it by 0x20 per turn when `g_game+0x205A28 == 1` (unidentified flag; here always). The decay
step 0x20 is stored at 0xC22570 (u32).

#### Order within the turn

`GGame::ProcessTurn` 0x54E5C0 calls, in this order: atmosphere (1), influence rings (2), players (3), dances (4),
forests (5), the living, fire (6), reactions (7), `Spell::ProcessSpells` (8), the particle containers (9), physics
(10), the PSys sounds (11), `GScript::Process`, weather (12), `CHand::GameTurnUpdate` (13) and the rewards (14) (see
[the engine loop](../engine/loop.md)). The spells' PSys are not advanced by the manager: they are advanced by their
spell in 8.

- Slot 11, `PSysGlobal::GameLoopEnd` 0x68F5B0: first the vortices (`fn_005FF330`, 0x68F5B9), then `fn_0069D340`
  (0x68F5BE, not identified).
- Fire is drawn per frame: `FireEffect::Draw` 0x730330 → `fn_00731560` with g_game_time_inc.
- The falling spell is drawn from `GGame::Process3dEngine` case 2 (0x54DDE0), with g_delta_time: `FallingSpell::Draw`,
  then its finish frame callback 0x526480 ([the falling spell](falling-spell.md#the-falling-spell-what-fallingspell-draws)).

### The icons' charge, the phase and the heart's ring points

- **The cancel of packet 0x1E** is `GPlayer::CancelSpellCharging` fn_0064BCC0 (0x64BCC0): it cancels the charge that started last
  ([the gestures](gestures.md#the-original), step 4).
- **The PSys global phase** [0xD4EBF8]: `DrawOffsetDecay::ProcessList` 0x68F680 (called only from
  `PSysGlobal::DrawLoop` 0x68F620) adds ms × 0.001 / 3.33 ([0x936C4C] = 3.33f), with ms = fn_0068F6E0 =
  `g_game_time_inc`, or 0 while g_game +0x14 bit 2 is set; while phase > 1 it takes 1 off and adds 1 to the wrap
  counter [0xD4EBFC] (read only by the uncalled getter 0x68F700). Its only writers are it (0x68F6A6, 0x68F6D3) and a
  reset to 0 (0x68F660). The oracle pins the formula: 3000 cases (phase 0, random, 0.99, 0.999999, 1; ms 0..20000)
  equal to openblack's.
- **An icon's charge fraction** is `WorshipSpellIcon::GetPower` 0x77FE80: with a held seed,
  `GetChantNeededForSpellSeed(icon +0x124)` / `GetChantRequiredForSpellSeed(icon +0x124)`, else the chant store +0x134
  / `GetChantRequiredForSpellSeed()` 0x77F840, with no zero test (`fdivr` at 0x77FEB0, 0x77FEC9).
  `SpellSeed::GetChantRequiredForSpellSeed(level)` fn_00729890 is the seed's info `GetMagicInfoFromPULevel(level)` →
  `GMagicInfo::GetChantsRequiredToCreate` 0x5FB830 (effect info +0x78).
- **The charge visual** `TChargingData::Draw` 0x7267A0 is the fraction, the icon's tint, the light sheet and the pulse
  ring, whose alpha is 255·t ([the icons and the charge](worship.md#the-icons-and-the-charge)).
- **The heart's ring point** fn_00467890 (thiscall, ret 0xC; index, angle, MapCoords* out; this = citadel +0x30, the
  CitadelHeart): it reads the extra-metrics block of the heart's own mesh (3D object +0x40 → mesh +0xAC,
  0x4678A0..0x4678E1; count at +4, entries of 0x30 bytes), takes entry[index] +0x24..+0x2C (0x467906..0x467934), turns
  it about y by (angle − the heart's vt +0x508 angle) (0x467938..0x4679A0), post-multiplies by the heart's matrix
  (`PostMultiply` 0x4679A4, so the heart's scale counts) and writes (ftol(x × 6553.6), ftol(z × 6553.6), 0)
  (0x467A95..0x467ABD, [0x8AC400] = 6553.6), returning 1. Its two `GetAltitude` calls (0x467A83, 0x467A8E) are dead.
  With no metrics block or index ≥ count it gives the heart's position and returns 0 (0x467AD0). Callers:
  `Citadel::FindNearestWorshipSitePosAngleAndSlotToPos` 0x463587 / 0x4635E8 (index 9) and fn_00468050 0x468083. So the
  radius is the heart mesh's point 9, not `radiusFromCitadel`.
- **The flock leader's event** carries the leader's movement, which is zero ([flocks](flocks.md#flocks), step 6).
- **The fireball's short horizontal direction** at 0x69EA5C / 0x69EA6E / 0x69EA81 (0x8AB22C = 0.1) is the origin
  fallback of the non-human cast ([fireball and lightning](fireball-and-lightning.md#the-original)).
- **A dispenser's timer** (0x71132A..0x711362): the period is written only when above 0, so a time shorter than a turn
  leaves it unchanged, and the active flag is not touched; a period of 0 disables a dispenser only through
  `SET_MAGIC_PROPERTIES` and `CREATE_SPELL_DISPENSER`.

### Time and weather

It is in [weather and climate](../world/weather.md#weather-and-climate) (LH3DAtmos, GClimate, storms, rain and the
weather queries).

### Miracles one by one

One page per player miracle: what the spell does, its object, its particles and what is missing.

- [Food and wood](food-wood-water-heal.md#food-and-wood)
- [Water](food-wood-water-heal.md#water)
- [Heal](food-wood-water-heal.md#heal)
- [Forest](forest.md#forest)
- [Flocks](flocks.md#flocks)
- [Fireball and lightning](fireball-and-lightning.md#fireball-and-lightning)
- [Shields](shields.md#shields)
- [Teleport](teleport.md#teleport)
- [Storm, electric storm and tornado](storm-and-tornado.md#storm-electric-storm-and-tornado)
- [Lightning explosion and missing PSys classes](lightning-explosion.md#lightning-explosion-and-missing-psys-classes)
- [The falling spell](falling-spell.md#the-falling-spell-what-fallingspell-draws)
- [Creature miracles](../creature/casting.md#creature-miracles-pending)

The particle engine (particle types, class registry, creators, sound and rule index) is in
[the particles](../physics/particles.md).

### Review of the whole chains

Checked against the executable and with the complete chains in the game (hand casting, food and wood, fire, weather
and worship together).

#### Formulas re-read in the exe (they match)

- Gestures: `MatchForward` 0x57A1A0 (turns aligned from any starting point, absorption of small corners, error wrapped
  with fn_0057A150, the two constants of `crt_xc` 0x579DC0/0x579DF0 = 3π/32 × 7/4 and × 2).
- Casting: `SpellSeed::ApplyThisToMapCoord` 0x728E20, `Cast` 0x729520, `DoPreCastThings` 0x729460 (the branch "type 2
  seed → magnitude 1" looks at `GMagicInfo +0x28`, −1 in every row of the file but filled at load: live for the
  fireball and its power-ups; [the size of the fireball](fireball-and-lightning.md#the-size-of-the-fireball-cast-with-the-hand))
  and `SendApplyToMapCoord` 0x5D3340.
- `Pot::AddResourceToPos` 0x66F270: the 9-cell spiral, first list +4 and then +0, `IsCloseToEqual` with `Get2DRadius ×
  GetRadiusMultiplierForApplyingPotToPos`, poisoned in arg5 and acceleration in arg6.
- Heat: fn_0072F980 (the firefighter immune, radius, the height check only if either of the two is 3 m or more above
  the ground, `min(10·ΔT, 0.5·the source's heat)`, the source loses heat if it is not burning, the group,
  `SetupOnFire` if the villager is not in state 219).
- `UpdateRuleGravityWithFloor` 0x6A1880 (gravity in the air `clamp(v.y + MaxSpeed, 0, 1) × g × the atom's gravity ×
  dt` and the return to the ground).
- `GWeather::CalcAtmos` 0x8400E0 (box, radius², falloff between the two radii, `ftol(f × fade × 256)`, temperature
  with a wrapping byte add and the other five bytes with saturation).
- The worship site's battery (fn_0077B6A0: intensity `used/capacity + max(0.2, 0.5 − battery/max × 0.5)` up to 1,
  `battery − (used − produced)` with no upper cap, available = battery + capacity) and the tap on an icon
  (`SpellIcon::InterfaceTap` 0x726430 → `ActualInterfaceTap` 0x77F880).

#### Fixed in the review

- `Pot::AddResourceToPos` returns `amount − what was left` on every path (0x66F511), also when it makes a new pile;
  before it returned the whole amount. Only the hand's log used it.
- `MapCoords::IsWater` 0x6035B0 answers **1** outside the map and where there is no land block (0x603617); the copy in
  `ECS/PotResource.cpp` answered 0, so food or wood dropped on open sea without a block made a pile.
- The one-off seed is tied to the player's best icon (`CreateSpellIntoHand` 0x72A730 → fn_007282A0), as above.
- The one-off orb has its `SpellSeedGraphic` inside (0x72A450) and is deleted with it.

#### Chains tested in the game

- Land1, dispenser → orb → seed → cast: `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1812.2652.1"`,
  `OPENBLACK_TEST_TAP="1812.2652.200"`, `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"`,
  `OPENBLACK_TEST_THROW_VEL`: the orb gives the FIRE seed ready (3500 chants), it arms (state 8) and on release the
  spell comes out with its `MagicFireBall` (T 6000). With the earlier 0.01 fireball the barn did not catch fire; with
  the FIRE seed's magnitude 1 the house, a tree and the villagers next to it burn (with
  `OPENBLACK_CAMERA_FLY=1800.75.2600.1826.30.2641`, `OPENBLACK_MOUSE_AT=0.5.0.55` and
  `OPENBLACK_TEST_THROW_VEL=0.2,6`).
- Land1, food next to the store: the same dispenser with `FOOD`: inside the store's radius (18.5 m) everything goes
  into it; a little further away it makes a `MagicFood` of 200 that grows 18 per grain, with the hand raised 16 m and
  the 4 s stream.
- Land2, icon → charge → seed → cast: `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE"`, `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="FIRE,200"` and
  `OPENBLACK_TEST_CAST`: `seed 3120 of icon 3082 in the hand with 3500 chants`, armed and cast. With 3000 chants the
  icon stays charging with the battery at 0. A one-off orb tapped with chants at the site comes out tied to icon 3081.

### Audited assumptions (2026-10-01)

Audit of every assumption the magic code adds: 245 findings, 41 corrected to match the original, 52 with the source
added, 139 marked in the code and 13 unchanged (already faithful or from another part of the code). What remains
marked, by topic:

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
- **Approximate:**
  - Reactions: openblack's cell grid and its order; `InBounds` uses the land's extent, not MapCoords::InBounds
    0x6042C0.
  - EffectValues: Object's `ReduceLife` for all classes.
  - Light maps: alpha = RGB maximum, without the ×190 level.
  - Fire graphic: the charring noise is made of two sines (not VLNoise 0x590C30); all fires are updated.
  - Random numbers: storms, rain, fire and the miracles now go through `game_random` (GRand, the PSys and the
    original's CRT); the fireflies and other systems still use openblack's.
  - Villagers: MOVE_AROUND_FIRE works on map units converted from openblack's float metres (the saved destination
    round-trips through metres); the decision to fight the fire (0x765870: formula read, without a random term) takes
    fn_00730290 / fn_007302E0 untraced; FLYING / LANDED are not run when landing after a teleport.
  - Gestures: frames with the mouse act as mouse messages.
  - Worship: the count of those going back home (vt 0x8C8 unidentified).
- **Inferred:**
  - Reactions: GetReactionPower = 1 for all (Spell 0x55CF10 and Tree 0x55D8D0 not ported).
  - EffectValues: one hit per object in ApplyEffectToMapPos 0x525100.
  - Seeds: the right hand for the one-off seed; the local player to check the influence of what is carried in the
    hand.
  - Default values of the PSys rules whose ctor was not read (Gravity 10, turns, trail, mesh, gesture 5 s, chain of
    0.5·scale).
  - Shield: the default castData (40).
  - Worship: the 6 m dance ring (queued fix F-51) and the fallback icon point.
  - Many of openblack's defensive values: out-of-range indices, caps, 0.0001.
- **Pending (TODO with an address in the code):**
  - Lightning: the single-target branch (0x691CF5) and the per-state sound. (The fork tree fn_00691F30 and the cut-off
    by shield fn_006D0BC0 are ported: `Particles/Rules/Lightning.cpp`, Fork and the shield test.)
  - Reactions: the stealthy branch of 0x6E3E10.
  - Creature: death counter and alignment (+0x11C0, +0x168).
  - Seeds: the MagicFireBall target (0x728A20).
  - Worship: the path along the footpath (58).
  - Fire: the route around the fire.
  - Spell class not ported (creature), which runs as a plain Spell (storm, water and flocks already have theirs).

#### Second audit (water, flocks, storm, lightning explosion)

25 findings: 4 corrected, 1 comment, 7 marked, 11 checked against the disassembly and 2 unchanged.

- **Corrected to match the original:**
  - Flocks: at the end of `SpellFlock::Process` the leader's position goes to the **flock** (+0x14, the centre of the
    domain), not to the spell (0x7234F2..0x723519).
  - Tornado: the pick-up spiral is GUtils::Spiral 0x74D7E0 started with direction 1 (0x6D22E9); the port walked the
    mirror-image one.
  - Storm: the fire-extinguishing reaction is forgotten when it is not available (vt 0x2C, 0x72DBA2), not only when it
    disappears.
  - Loop: fn_0064AC30 (sky alignment) goes after the teleport travellers, at the end of GPlayer::ProcessPlayers
    (0x64A697).
- **Approximate:** the subcollection added mid-step is updated in that step (PSys.cpp); `MoveToBaseGroup` without a
  root collection deletes the atom; the tornado's per-cell vessels come from the registry; the base colour of the
  mists is that of the previous frame; the output of `UR_ForestPath` without keys is 0.
- **Inferred:** the +0x80 of `EventConditionAtomNearVillagers` in metres; the wolf leader's destination before its
  first turn.
- **Unchanged:** `SetDeathCallback` for animals has a single slot (only the flocks use it).

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to
quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x671879` | the explode effect's per-turn process call with zeroed process info (power 1.0) | `explode_object::GameLoopEnd` |
| `0x67188E` | the finished explode effect (result 5) deleted; remade next turn | `explode_object::GameLoopEnd` |
| `0x6718ED..0x671932` | creation of the always-on EXPLODE_OBJECT effect (type 0x17, zero position/direction, power 1.0) when none | `explode_object::GameLoopEnd` |
| `0x67C175..0x67C184` | mesh piece draw: land light looked up under the matrix translation | `mesh_pieces::AppendPiece / explode_object::LitColour` |
| `0x67C189..0x67C1F6` | mesh piece draw: DrawData colour multiplied by the land light byte by byte | `explode_object::LitColour` |
| `0x67C47F..0x67C4C4` | mesh piece draw: vertices without own colours take the base colour | `mesh_pieces::AppendPiece` |
| `0x67C4CE..0x67C4E4` | mesh piece draw: model light only with the lighting switch on and a normal per vertex | `mesh_pieces::AppendPiece (lit)` |
| `0x67C508..0x67C5E4` | mesh piece draw: light brought into mesh space through the inverse drawn matrix, normalised | `model_light::LightInMeshSpace` |
| `0x67C602..0x67C62C` | mesh piece draw: intensity = fistp(255 * dot(light, normal)) | `model_light::Intensity` |
| `0x67C631..0x67C6A3` | mesh piece draw: RGB scaled by the intensity factor >> 8, alpha kept | `model_light::Apply` |
| `0x67C635 / 0x67C63D` | mesh piece draw: reads of the global ambient [0xC39264] | `model_light::Ambient` |
| `0x67E01F` | edge build: jne on sQ <= sP (C0 or C3) puts the second point first | `MakeEdge` |
| `0x67E060..0x67E084` | edge build: squared length (dz dz + dy dy) + dx dx | `MakeEdge` |
| `0x67E87E` | the rocks' call of fn_006812B0 (queue flag 0) | `explode_object::QueueMesh` |
| `0x6807E2 / 0x6807F1` | ExplodeMesh sub-mesh test: LOD 0 flag 0x20000000 set, status bits 0x3F0 clear | `k_SubMeshLod0 / k_SubMeshStatus` |
| `0x68088B..0x6808E2` | ExplodeMesh: three edges per triangle (fn_0067DF00 -> fn_0067DFD0) into the edge array 0xD4E338 | `explode_object::SplitPrimitive` |
| `0x6808F8` | ExplodeMesh: qsort of the edges by length with comparator 0x682550 | `explode_object::SplitPrimitive (CrtQsort)` |
| `0x68091A..0x68099D` | ExplodeMesh: of a run of equal neighbouring edges the last one kept | `explode_object::SplitPrimitive` |
| `0x68093E / 0x68094E` | ExplodeMesh: the two fn_00460640 calls comparing an edge's two ends | `SameEdge` |
| `0x6809E0..0x680AF0` | ExplodeMesh: per unique edge the triangles that use it | `explode_object::SplitPrimitive (users)` |
| `0x680AF6..0x680D6C` | ExplodeMesh: the piece walk over shared edges up to MaxTrigsPerFrag | `explode_object::SplitPrimitive` |
| `0x680B9B` | ExplodeMesh sets the RenderParticleGJMesh land-light flag (+0x21 = 1) | `mesh_pieces::AppendPiece (LitColour)` |
| `0x680D47..0x680D68` | ExplodeMesh: next start = first unused triangle | `explode_object::SplitPrimitive` |
| `0x680D93..0x680E15` | ExplodeMesh: vertices to world through the matrix, summed | `ExplodeMesh (centre)` |
| `0x680E26..0x680E54` | ExplodeMesh: centroid = sum x (1 / n) | `ExplodeMesh (centre)` |
| `0x680E8B..0x680F31` | ExplodeMesh: atom rotation identity (SetRotationMatrix), position +0x80 = centroid | `ExplodeMesh (atom.rotation/position)` |
| `0x680F8A..0x680FCD` | piece velocity: direction scaled to speed / sqrt((x x + z z) + y y) | `explode_object::PieceVelocity` |
| `0x680FD9..0x681001` | piece velocity: random part length | `d` |
| `0x681068` | piece velocity: random part's Y halved ([0x8AA3B4] = 0.5) | `k_RandomYFactor` |
| `0x681073..0x681093` | piece velocity: random part added to d | `explode_object::PieceVelocity` |
| `0x681253` | a caller of fn_00681260 (queue flag 0) | `explode_object::QueueObject` |
| `0x6812C5` | fn_006812B0 returns when the 3D object's mesh (vt 0xF8) is NULL | `explode_object::QueueMesh` |
| `0x6814F5..0x68153F` | UR_ExplodeObject::ModifyAtomCollection loop emptying the queue from its last entry | `ExplodeObject::ModifyCollection` |
| `0x6825A9..0x682628` | fn_00682580: linear fallback walk when bsearch misses | `FindEdge` |
| `0x6BED00` | UR_ExplodeObject2 ctor: RandomFactor +0x2C = 0.3, MaxDepth +0x30 = 4 | `k_DefaultRandomFactor / k_DefaultMaxDepth` |
| `0x7C7EAF` | CRT qsort: insertion (short sort) for size <= 8 | `CrtQsort` |
| `0x7C7EE3` | CRT qsort: middle element as pivot | `CrtQsort` |
| `0x7C8141` | CRT bsearch (MSVC 6) | `CrtBsearch` |
| `0x801F83..0x8020E2` | land light lookup: per-channel lerp a + ((b - a) w >> 8), imul/shr/mask | `explode_object::LandLight (lerp)` |
| `0x8020E9` | land light lookup: alpha forced to 0xFF | `explode_object::LandLight` |
| `0x938ABC` | UR_ExplodeObject2 vtable | `ExplodeObject2` |
| `0xD4E2A0` | mesh piece draw: transformed vertex buffer | `mesh_pieces::AppendPiece (vertices)` |
| `0xD4E310` | count of the second explode queue, zeroed by fn_006811D0 on map clear | `explode_object::Clear` |
| `0xD4E328` | count of the first explode queue, zeroed by fn_00681200 on map clear | `explode_object::Clear` |
| `0xD4E338` | ExplodeMesh's edge array (0x1C-byte edges) | `explode_object::SplitPrimitive (edges)` |
| `0xD4E350` | ExplodeMesh's unique edge array (0x30 bytes: the edge and a GJArray of triangles) | `explode_object::SplitPrimitive (unique/users)` |
#### More addresses from the code

| Name | Address | What |
|---|---|---|
| `GetTimerWhenOneShot` | 0x5FB7C0 | a one-off miracle's timer from its row |
| `GetTimerWhenCreatureCasting` | 0x5FB7B0 | the timer when a creature casts |
| `Spell.cpp` | 0x71FB40..0x722070 | the spell core's range |
| `Object::GetDefenseMultiplier` | 0x637930 | the seven numbers of the object's info at +0x90 (EffectNumbers), 1 each without an info (the same table ApplyEffect multiplies by); `Abode::ApplyEffectsDueToPhysicalDestruction` 0x4067C3 divides by it |
| `Spell::HasEnoughChantsAndLifeForRecast` | 0x55CE00 | vt 0x548, `mov al, 1` |
| `GPlayer::SetMagicTypeEnabled` | 0x64C300 | on: one more holder and ever enabled; off: one less (not under 0) |

- The heal's search (`GMagicHealInfo::FindTargets`): both tests measure from the spiral's MapCoords (the one its
  `operator+=` moves), not from the cast position: `GetDistanceInMetres` 0x74CD70 (coords, the object's +0x14) at
  0x5FBBEE < R, available, IsEffectReceiver, a Living that can be healed, and then dx² + dz² < R² with dx = coords −
  object in metres (fild; fmul 10 [0x92C100]; fmul 2^-16 [0x8AC41C], 0x5FBC54..0x5FBCA3; `test ah, 0x41`: an exact
  square, not the first test again).
- `GMagicInfo::powerupType` (+0x30) is −1 in every row of the file; the load-time fill above writes it.
- The hand's turn in the magic loop is the hold's update (0x46E505..0x46E557).

### Sources

- runblack.exe (the addresses throughout this page) and, where cited, the bw1-decomp headers.

## openblack

Status: **done** for the life cycle, the chants, the events and effects, the cast rules, the seeds and orbs, the
script, the spell grid and the turn order; **partial** for a creature taking an effect (the fight's part and the
marks are not ported). Progress: [miracle effects and reactions](../project/progress/miracles/miracle_core.md),
[gesture](../project/progress/gesture/), [worship](../project/progress/worship/), [miracles](../project/progress/miracles/)
(what our tree does of it, row by row). Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

**Code rules** ([the conventions](../project/refactor-conventions.md)): spells, seeds, icons and fires are ECS
entities with their data in components, and the magic services are reached through the Locator, never through
globals; info.dat, the meshes and the effects load through the resource caches; pure logic (the chants, the gesture
recogniser, the alignment, the curves, costs and spirals) is unit tested in `test/` with fakes; comments describe
behaviour in plain English, with no decompiled names or addresses (those belong here).

- **The spell core** (`src/Magic/Core`, `src/Magic/Spells`, `src/ECS/Effects`): each spell is an entity with
  `components::Spell`; the original's virtual functions are a `SpellOps` table per class (`SpellClass`), each class in
  its own file under `Spells/`. An unregistered class runs as a plain `Spell`. The filled `GMagicInfo` fields are
  `MagicTables::GetSpellSeedOfMagicInfo`, `GetGestureOfMagicInfo`, `GetPowerUpLevelOfMagicInfo` (the last in
  `Spell.cpp`; before 2026-10-08 it was computed as `GetPowerUpFromMagicType` on the first seed, the same for every type
  but NONE: 2 there; the fill leaves NONE at −1). `castOnObject` is taken from the first seed of that magic type (the
  original's read inside GSpellIconInfo[1] is unresolved). Not ported: the creature desires at the cast and the
  minimap blip.
- **Chants**: the rules are in `Magic/SpellChants` (taken from raffclar's tree, the same rules as ours term for term);
  `Magic/Core/Chants` applies them to the Spell component. The chant rules report every payment to the caster
  (`SpellCasterInterface::OnChantsSpent`); nothing draws the mana path yet. The creature's `UpdateSpellInfo` is
  `magic::creator::UpdateSpellInfo` for `Kind::Creature` (the hands from the turn's pose). `Creature::MaintainSpell`
  is not ported.
- **Effects**: `effects::ApplyEffect` (`ECS/Effects/EffectValues.cpp`) keeps the original's order. It does not keep
  the creature's count of objects destroyed, and it stops after the kill for a thing its destruction deleted at once.
  The Abode's ghost is `abodes::DestroyedByEffect` → `ecs::object_ghosts::Add`; the scaffold rebuild /
  `ToBeDeleted(0)` is ported, the script branch is not. `Villager::IsReachable` is taken as 1 (unverified). The
  creature's mimicry after a hit is not ported.
- **A creature taking an effect**: a creature's life is its body's (`life::LifeOf` / `SetLife`); its defence
  multipliers are its species row's divided by its size as above (`effects::GetDefenseMultiplier`,
  `CreatureDefenceDivisor`; `Creature::size` stands for the 3D body's size, +0x90, until the pen-size series switches
  it to the drawn size, `creature_pose::DrawnSize`). A heal can reach it (the cast rules' `CanBeHealedByHealSpell` is
  true for a creature) and mends its marks by `heal × 9600` counts, truncated (`CreatureSkinSystem::Heal`,
  `creature_marks::k_HealEffectCounts`), before the object's effect raises its life. `effects::ApplyEffect` on a
  creature takes the damage's first pass (the burn to the fire), returns 0 for harm it applies itself and, in a fight,
  for a harmless effect from anything but itself or its own player (the applier, +0x28, is the spell's creator: a
  player for a player's spell, `EffectValues::appliedByPlayer`; an object of its player, such as an icon, or a spell
  whose caster has gone, is not), mends the marks, returns 1 in a fight and otherwise goes on to the object's effect,
  where the damage comes off its body's life and a kill is its `DestroyedByEffect`: `ForceFaint` when it can die
  (`Creature::canDie`, +0x1158), else all its life back. In a fight is a `CreatureFighting` at the `Duel` stage whose
  end is not paid for yet (`fight::State::Start` in `BeginDuel`, `EndFightFor`); not while it walks up, taunts or
  waits, nor while a winner shows off.
- **Cast rules** (`Magic/CastRules`): `cast_rules::CanCastOn` (a creature whose `creature_spells::Spells::waiting`
  holds no more than 5; a resource on land, the rest by `CanCastAt`, true for water: it matches the original). A
  creature's cast: the class's rule is `MagicSystemInterface::CanCreatureCastAt(type, target)`, which the creature's
  mind asks before the cast so that it can tell that refusal (and the energy test's) from the others: only those two
  make it hungry. `CastByCreature` casts (hands, beams, the spell at +0x1078, the body paying `costToCreate`),
  `ReleaseCreatureCast` lets go (the beams close down and are kept until their effects are done,
  `ParticleSystemInterface::CloseDownSpotVisual`). The hand's locked-select release is not yet in openblack.
- **Seeds**: the icon's seed is `worship::icon::CreateSeed`. The orb's frame offset goes in `UvScroll {u, v}` and the
  shader adds `u` in quarters. openblack does not keep the status's +0x38 turn. `Object::ProcessInHand` is not
  ported.
- **The orb's material**: before, openblack blended the orb as mode 5 (`SRCALPHA / INVSRCALPHA`, two-sided). That
  covered half the background with the dark turquoise: a dark greenish orb. The earlier investigation (N·L light,
  ambient 90/256, alpha 0x95) was correct, but it missed this material change at load time. Now:
  `graphics::MaterialProperties` and `L3DSubMesh::SetMaterialProperties` (the type change of 0x57E120, with the type
  stored in `Primitive::materialType`) and `L3DMesh::SetMaterialProperties` (fn_0057E1D0), in `src/3D/L3DSubMesh.*` and
  `L3DMesh.h`; `Game.cpp` applies it to `O_Bibble_up` when loading it; `Renderer::DrawSubMesh`: an object with
  `components::Alpha` (the 0xC387C8 table) keeps the additive blending of its additive primitives, and those in modes
  11 and 13 without writing Z. The orb is `components::Alpha` = 149/255 in `OneOffSpellSeedArchetype` (pass
  `MainBlended`). The forward shift by the radius: `one_off::UpdateFrames` stores `OneOffSpellSeed::sortPoint` = box
  centre + normalize(camera − centre) × radius (`Get2DRadius`: largest half extent in x/z × scale, 2.3 m) and
  `RenderingSystem` / `Renderer` sort the orb by that point (`RenderContext::sortPoints`).
- **The orb's facing**: done by `OneOffSpellSeedArchetype` and `one_off::InterfaceTap`; `one_off::UpdateFrames`
  computes `OneOffSpellSeed::facing` and `facingOffset`, and `RenderingSystem` draws the orb with them. `Transform`
  does not change: the dispenser compares its position and the physics uses the sphere. The pivot (faithful since
  2026-10-09, issue #133): openblack took the drawn position plus `scale × c`, without the drawn rotation, while the
  seed went through the drawn matrix: a thrown orb's bubble circled its seed as the body turned.
  `billboard::LookAtCentre` now takes the drawn matrix's rotation and scale; an orb that was never moved (identity
  rotation, scale 1) is drawn to the bit as before.
- **With the real hand**: `HandSystem.cpp` / `HandPlacement.cpp`. **Inferred**: if the ray hits the seed inside
  (`SpellSeedGraphic`, which is not an `Object`), it counts as if it hit its orb or icon. Not there: the hover tooltip,
  `GInterface::StartImmersion(0xE)` and the `GameThingClicked` record.
- **CHL script**: `Magic/Script/CHLSpells.cpp`.
- **The PSys linked to the spell**: `Particles/SpellLink.h`.
- **The spell grid**: `SpellGrid`, decayed every turn (the flag taken as always set).
- **The turn order** (`Magic/MagicLoop.cpp`): `Game::GameLogicLoop` calls `magic::ProcessTurn` (1..8, through
  `Locator::magicSystem`) after `livingActionSystem`, then the particle system's `ProcessTurn` (9) and the fireflies,
  the physics, `magic::ProcessSpellParticlesEndOfLoop` (11, was `ProcessPSysGameLoopEnd`), the scripts, the weather
  things and the climate (12), `magic::ProcessHandTurn` (13), in the original's order; the rewards (14) are not
  ported, nor `fn_0069D340`.
#### Where the code is

- `cast_rules::CanCastOn` (`CanCastOn`: vt 0x2C, the class's check on an object); `GetTimerWhenOneShot` and
  `GetTimerWhenCreatureCasting` in `Magic/MagicTables.h`. **Approximate**: the heal's search takes an object's
  MapCoords from its float position.
- `MagicInfoOf` (`Spell::GetMagicInfo`), `EffectInfoOf` (`GMagicEffectInfo::GetInfo`), `IsCastFromHand`
  (`Spell::IsCastFromHand`) in `Magic/Core/Spell.h`; `IsMagicTypeEnabled` in `Magic/Core/Players.h`;
  `GetPaidEffectValues` (fn_00720AE0 + fn_00720B20) in `Magic/Core/SpellEvent.h`; the seed's `MagicTypeOf`
  (`SpellSeed::GetMagicInfo`) and `SeedRadius` (`Object::Get2DRadius` of the info's mesh, not the `Mesh` component,
  which goes while the seed is hidden) in `Magic/Core/SpellSeed`; `creator::UpdateSpellInfo` tests `Kind::WorshipSpellIcon`
  for the icon's case.
- `DefenceMultipliers` (`Object::FillInEffectDefenceMultiplier`, 1 without an info, inferred), `DamageEffect`
  (`Object::GetDamageEffect`), `EffectValues::FromEffectInfo` (`FromEffectInfo`) and `GetDefenseMultiplier` (an
  `std::array` of `EffectValues::_COUNT`) in `ECS/Effects/EffectValues`.
- The hand's turn is `HandSystem::GameTurnUpdate` (`HandTurn.cpp`).
- The code's notes, now settled against the original: `player::CancelMostRecentCharge` / `CancelMostRecentCharge`
  (`Worship/PlayerSpellIcons.cpp:224-248`, `turn > latestTurn` from 0) is the original's rule (its "🔍 inferred" comment
  at :226-227 is stale); `UpdatePhase` (`SpellSeedGraphic.cpp:561-569`) matches the phase formula (the bit-2 gate
  belongs to its caller); `icon::ChargeFraction` (`WorshipSpellIcon.cpp:359-372`) matches except its guards
  (`cost != 0`, `required != 0`: 0 where the original divides by 0, only with a 0 cost in the data) and that it takes
  the cost through `MagicTypeOf(icon, powerUp)`, the original through the seed's own info; `seed::ProcessInHand`
  (`Magic/Core/SpellSeed.cpp:431-441`) matches (its first-turn setup at :427 lacks the vt +0x734 test);
  `icon::UpdateChargingVisual` draws the pulse ring with alpha t, right; `LeaderEvent` (`SpellFlock.cpp:275-283`) and
  `SpeedFromHand` (`Particles/Rules/Fireball.cpp`) match.
- openblack differs: `effects::IsEffectReceiver` (`ECS/Effects/EffectValues.cpp:240-243`) takes any available villager;
  the original takes a heal only when not dead and any other effect only when `Villager::IsReachable` (ported as
  `ecs::villager::IsReachable`, `ECS/Villager/VillagerHome.cpp:240`), so in openblack a villager at home, in the hand
  or hiding (236) takes fire and lightning, a fix row.
- openblack differs: `HeartRingPoint` (`Worship/WorshipSite.cpp:183-196`) reads B_WORSHIP's point 9 (`k_SiteMesh`, :71)
  on the citadel's origin at scale 1, as a float; the original reads the heart's own mesh point 9 through the heart's
  matrix, truncated to map units, a fix row (to measure; first check that the heart mesh has 10 or more extra points,
  else the original gives the heart's position for every slot).
- `OneOffSpellSeedArchetype::Create` (`ECS/Archetypes/OneOffSpellSeedArchetype`) makes the orb: `entt::null` for a
  seed type outside 0..29, else the `OneOffSpellSeed` and its seed graphic (`worship::seed_graphic::Create`, the
  local player, 1.0). `components::Spell` (`ECS/Components/Spell.h`) keeps `isMyInterfaceCasting` (+0x44) and `free`
  (+0x5C). `test_spell_forest` sets the cast types `SpellCastType::SpellCastInHand` (cast in hand, 0x729820) and
  `SpellCastType::SpellCastHandPosition` (kept in hand, 0x729840).

### Test hooks

- `OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_SEED`, `OPENBLACK_TEST_ONESHOT` and `OPENBLACK_SPELL_TRACE` are in
  [debug environment variables](../project/debugging.md#debug-environment-variables), with all the other
  `OPENBLACK_*`. Checked with them: a heal at Land1's store with 0 targets is cast anyway, closes after 20 s and is
  deleted on turn 200; a FIRE seed reaches the hand with 3500 chants, ready.
- By topic: [casting from the hand](hand-casting.md#test-hooks), [worship's checks in the game](worship.md#checks-in-the-game).
- Per miracle: [water and heal](food-wood-water-heal.md#test-hooks), [flocks](flocks.md#test-hooks),
  [fireball and lightning](fireball-and-lightning.md#test-hooks), [shields](shields.md#test-hooks),
  [teleport](teleport.md#test-hooks), [storm, electric storm and tornado](storm-and-tornado.md#test-hooks),
  [lightning explosion](lightning-explosion.md#test-hooks), [forest](forest.md#test-hooks).

## Pending

What is missing for each miracle is on its page. The core's:

- **The worship battery**: `Worship/WorshipSite.cpp`'s capacity, battery size, draws, strain and icon share to call
  the `Magic/WorshipBattery` rules as the end of the turn does (the file's owner to agree); until then those rules, and
  `UseWorshipChantsIfNotInfinite` / `MaintainSpellFromWorship`, are only reached by the tests.
- **Chants**: the mana path sprites of a worship-site creator (`CreateSpellPoint` 0x7213D0) are not drawn.
- **The leash type's packet 0x65** wipes the local player's gesture buffer (0x5E6B59..0x5E6B81); queued as a fix
  ([gestures](gestures.md#pending)).
- **An effect's kill** (`Object::ApplyEffect` 0x637A54..0x637A73): an applier that is a creature counts one more object
  destroyed (Creature +0x11C0, a float); queued as a fix. What reads the count is not traced. The crushed reaction's
  player for an animal or a creature is still the neutral one in openblack (`PlayerOf` answers for villagers only);
  their own `GetPlayer` (vt 0x1C) is not read.
- **Whose alignment a miracle moves**: `EffectValues::GetPlayer` 0x5254C0 takes the applier's own `GetPlayer` first,
  and a miracle's applier is its creator. openblack's miracles put the spell's player in `EffectValues::player`, which
  is the same only if every creator answers the caster; the creators' `GetPlayer` are not read. A creature applier's
  own alignment (Creature +0x168) is queued as a fix with the kill count.
- **A creature taking an effect**: `BeCutAndScarredByEffect` 0x478890 is queued as a fix: out of a fight, its own
  `GetDamageEffect` pass (a third one: the port makes two), the fear and anger `ChangeSource`, and the marks with their
  draws. So the random trace will differ from the original's once a creature takes harm out of a fight from anything
  but itself. A kill by a miracle faints it through `ForceFaint`, whose faint draws `GameRand(2)` for a mirrored fall
  (inferred, [the creature](../creature/overview.md)); whether, and how often, the original draws on such a faint is
  not settled. In a fight: the block (the numbers × 0.1 in place), the reel, the fight health, the knock-out with the
  opponent's plan, and the marks mended a second time (0x478D7D as well as 0x478F42) are not ported; the two bytes
  [0xC6414C] / [0xC6414D] that skip it are not named. The fight health a creature with an opponent loses in
  `ReduceLife` / gains in `IncreaseLife` is not ported. The opinion of a casting creature (`UpdateHowNiceIsCreature`,
  every 600 turns) and the indestructible creature's life floor (`Object::SetLife`) are queued as fixes.
- **The dispenser orb**: the user accepts the size of the seeds and the height of the bubble (user, 2026-10-01). The
  reference capture of the original is a WATER orb, not a fire one: its sky-blue blotch is the effect of the water
  seed. What remains ⚠️ approximate is that the terrain light and the haze are taken at `position + facingOffset` and
  not at the point moved forward towards the camera (Draw 0x518FCD..0x518FF2). Against the user's capture: in the
  original the orb floats higher above the dispenser and looks bigger; in openblack the effect of the FIRE seed is
  seen as a yellow core inside the orb; the original's sand is lighter, and since the orb is additive the background
  changes its look a lot.
- **The orb's pivot**, not verified: (1) that fn_00646FE0 (from `GLandscape::Draw` 0x5E49DC) runs before the orb's
  `Draw` in the same frame; if it runs after, the original's bubble lags one frame behind the body. (2) An orb at rest
  keeps the last turned matrix, whose `M·c` is W again, so W does not drift while it lies still; what writes its
  matrix when something else moves it on the land is not read. openblack takes the `Transform` each frame.
- **The one-shot orb in the reflection and in the temple**: whether the original draws the orb's bubble, the seed
  inside it and its bands in the sea's reflection (the physics objects are reflected, [the frame](../rendering/frame.md)
  step 4d; whether a carried or thrown orb is among them is not read), and whether it draws them while the temple's
  inside is shown, is not traced. openblack draws the bubble and the bands, see-through models, in the world view
  only; the renderer's globe list (`Renderer::DrawGlobes`, empty today) is read in the world view only until this is
  known.
- **A dispenser broken by a thrown rock**: openblack breaks it into pieces like a house; the original draws it with
  `MultiMapFixed::Draw` (`SpellDispenser::Draw` 0x722940 -> 0x518090). `Abode::ReactToPhysicsImpact` 0x406240 and what
  happens to its orb still have to be read.
- **The dispenser rules against raffclar's** (`Magic/DispenserRules`, 2026-10-08): his names were taken over our
  counting. Ours keeps two differences from his: the orb counts as still there up to 0.5 m across the land plus the
  touch tolerance (0.001), his up to 0.5 m; and when the count reaches the period ours leaves it as it is until an orb
  is actually made (with no seed for the magic it asks again every call), his puts it back to 0 at once. His
  `PeriodTurns` is not used: our periods go through `game_clock::TicksForSeconds`, the original's whole turns per
  second times the seconds. Both wait on reading `Object::IsTouching` 0x637E00 and what `CreateOneOffSpellSeed`
  0x722B80 does with the count when the one-off seed refuses its seed.
- **The miracles' service's `CreateDispenser`** (2026-10-10): nothing in the game calls it yet. It makes the building
  the debug Magic window's Running tab starts with (the Norse dispenser, else the first in the tables), for the nearest
  town, with that building's own period, and floats its first bubble at once, as the map script's
  `CREATE_SPELL_DISPENSER` does; raffclar's waits a period first. Which building, town and period the original's
  caller gives is to be read once such a caller is ported; no tribe rule is assumed.
- **The miracles' service's `GetSpells`** (2026-10-10), for the debug window: every class's upkeep was read for side
  effects. None draws a random number; the base, shield and storm upkeeps only read the tables and the spell. The
  forest's counts its trees and the flocks' their animals, and each first makes the spell's record of them when it has
  none (`SpellForestData`, `SpellObjects`). Such a spell is reported with an upkeep and a strength of 0 until it has
  that record, so the window changes nothing. `DiscardHeldSeed` goes through the hand's forced drop, so the seed goes
  back to its site a turn later.
- **The miracles' service's `SummonSeed` / `SummonSeedAtSite`** (2026-10-15): the original has no summon besides the
  worship's request. `SummonSeed` is the request packet 0x25 (handler 0x5DABA0): `GPlayer::ValidForRequestSpell`
  0x64BEC0, then `GPlayer::RequestSpell` 0x64BDB0, which asks the best icon (`FindBestSpellIconForSpellSeed` 0x64BF40)
  for `WorshipSpellIcon::RequestSpell` 0x77FB40 with power-up −1 and chants required 1 (pushed at 0x64BDC9/0x64BDCB);
  a seed type of −1 gives 0. `WorshipSpellIcon::RequestSpell` 0x77FB40 has that one caller (xref), so nothing in the
  original asks the worship for a power-up level: the service refuses any level but −1. 0x77FB40 puts a full icon's
  seed into the hand when the hand is ready (`PutFullyChargedPowerUpSeedInHand` 0x77F8F0), else starts the charge
  (`StartCharge` 0x77FA00), whose seed comes when the icon fills (`WorshipSpellIcon::Process` 0x77F390). The service
  returns the seed only when it came at once. `SummonSeedAtSite` asks that site's icon of the seed in the same way, for
  the site's player; the original asks one chosen icon only through the hand's tap (`ActualInterfaceTap` 0x77F880),
  which cancels a charge instead of asking again. raffclar's version (charged at once from a player's store, in the
  hand before it is full) is not the original's and was not taken. The packet's one sender (a scan of the direct
  SetPacket calls 0x550900..0x551800) is fn_005CFAE0 at 0x5CFD68 (SetPacket 0x550C20(0x25, seed, 0), after
  HelpProfile::Trigger(0x11)), called only by `GInterface::ProcessPowerUpSystem` (0x5CF66F): the miracle selection by
  gestures, not the icon tap. openblack's selection (`Magic/Gestures/PowerUpSystem.cpp`) asks the worship's icon
  provider, whose request is `SummonSeed`; the original sends the packet and runs the request when the packet is
  processed, openblack runs it at once.
- **The FOOD and BEAM_EXPLOSION seeds** are also loaded with material properties (`{1.0,1.1,0}`); apply
  `L3DMesh::SetMaterialProperties` as for the bubble.
- **The vortex between lands** (`MagicVortex`, CREATE VORTEX): not ported; on release, fn_005FE3B0 marks `thing+0x25
  |= 0x40` at 0x5FE5DD (`script_held::SetCannotBeEaten`).
- **Rain in the growth of the trees** (`GrowTree`, formula in [the trees](../world/trees.md)).
- `OPENBLACK_TIME_OF_DAY` is no longer applied in Land 1 (the script controls the clock).
- **The fireball's `DrawOffset`** that draws the locally cast ball from the hand (fn_006C7840):
  [fireball](fireball-and-lightning.md#pending).
- **Physics**: the pair skip with +0x1A4 == 1 and flag 2 (0x64583E..0x645866) in `Substep`. ⚠️ approximate the box
  centre turns only on xz; 🔍 inferred the off-map delete when the object that stays is another one (Tree → DeadTree);
  not tested in game: the off-map delete, the heartbeat and the chants.
- openblack differs: `effects::IsEffectReceiver` takes any available villager instead of a heal for the not dead and
  any other effect for the reachable only (`ECS/Effects/EffectValues.cpp:240-243`), a fix row.
- openblack differs: `HeartRingPoint` reads B_WORSHIP's point 9 at scale 1 instead of the heart mesh's point 9 through
  the heart's matrix (`Worship/WorshipSite.cpp:183-196`), a fix row.
- **The `castOnObject` read** at 0xD9D600 (fn_005FB520): known, it is the TownSpell Icon info's +0xEC
  (GSpellIconInfo[1], [above](#spell-core)). Still open: which magic infos have seed type −1 (the creature's
  `Creature::CastSpellOnObject` 0x4F7DBF is one caller) and what that field holds in info.dat.
