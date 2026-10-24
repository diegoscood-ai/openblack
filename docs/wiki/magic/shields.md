# Shields

## Summary

The shield miracles put a dome over the place they are cast: SHIELD (19) a magic, see-through dome that stops
miracles and creatures, PHYSICAL_SHIELD (20) a solid, spinning shell that also stops thrown objects. The size comes
from the circle gesture drawn with the hand; the cost grows with the square of the size. Each hit costs the shield
chants; when it runs out it breaks. Villagers of a town that has been attacked come to stand under it, amazed.

openblack ports the spell, both objects, the villagers' reactions, the creature's avoidance and the dome's particles
faithfully. The strike and destruction reactions have no listener yet, and the shields are not in the map's object
grid. The domes' particle rules are on [Shield rules](../physics/particle-rules.md#shield-rules).

## The original

### Shields

Verified in `runblack.exe` while porting it. MAGIC_TYPE SHIELD (19) and PHYSICAL_SHIELD (20) use the same class
`SpellShield` (SpellWithObjects, 0x10C bytes), which makes a `MapShield` object: `MagicShield` (vtable 0x9836EC; invisible; the
dome is the spell's PSys, SF_DefenseSphere) or `PhysicalShield` (vtable 0x983F64; the solid mesh MSH_S_SOLID_SHIELD
554). The dome's meshes are
on [the particle meshes and the shield dome](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome).

#### The spell

- `InitWithPos` 0x72B5F0, in order: the radius (castData +0) is clipped **first from above and then from below**
  (`maxRadius` 1000 if it is not smaller, then `minRadius` 5 if it is not smaller) and rewritten into castData;
  `Spell::InitWithPos` (the magnitude stays in the radius); the REACT_TO_MAGIC_SHIELD (13) reaction of the spell's
  player with radius r + 30 (reaction +0x3C); the nearest town less than 500 m away (0x43FA0000, `push` at 0x72B683;
  `MapCoords::GetNearestTown` 0x6020E0, all the towns of all the players and of the neutral one); **an anti ring with
  radius = the magnitude for each other active player** (`GGame::GetNextActivePlayer` 0x5508D0: the seven slots with
  +0x8E0 ≠ 0); and `MapShield::Create` 0x72BE20 in the object list. If `Spell::InitWithPos` fails, the original does
  everything else anyway (and then deletes it).
- `SpellShield::GetMagicInfo` 0x72B820. The ctor 0x72B4B0 → `fn_0072B3C0` puts the spell in the list and sets +0xF4
  / +0xF8 / +0xFC = 0. The list: head 0xDA07F0, linked through +0x100, count 0xDA07F4, newest first. The curl is read
  through `fn_007202D0` (Spell +0x98).
- `Process` 0x72B750: releases the strike reaction (+0xF4) when it is no longer there, and
  `SpellWithObjects::Process`.
- Cost 0x72B7F0: `costPerGameTurn × (magnitude / radiusForNormalCost)²` (20 and 22 × (r/30)²; with r = 40, 35.6 and
  39.1 per turn); `divideCostsByTribalPower` = 1 divides it in `PayFor`. Timers: player -1 (lasts while it is paid
  for), creature and computer 20 s, one-off 40 s.
- `CloseDown` 0x72B840 = `SpellWithObjects::CloseDown` 0x721300: `CoreCloseDown` and, since
  `GetSetObjectsDyingOnCloseDown` (0x55CF50) gives 1, `SetDying` (vt 0x6A4) on each object. `ToBeDeleted` 0x72B500:
  out of the list 0xDA07F0, its reactions go, +0xF8 = 0, `SpellWithObjects::ToBeDeleted` 0x720FD0 (CloseDown and the
  empty list) and the `ToBeDeleted` of each ring.
- `UpdateStruckReaction` 0x72B780 (vt 0x51C): reaction 35 the first time; afterwards it only refreshes its turn
  (reaction +0x2C). `SetUpDestroyedReaction` 0x72B7C0 (vt 0x520): removes the 13s it started, +0xF8 = 0 and creates
  the 36. In the `Spell` base both are empty (0x55CE10 / 0x55CE20). `SpellHitSpell` (fn_00720B70) calls them on the
  struck spell.
- `IsUnder` 0x72BD20: `dist(p, castPos) < GetRadius() − margin` (x, z). `fn_0072BA00` (for `SPELL_AT_POINT`): the
  first available shield whose magnitude covers the point; its mask is tested as `(bit | mask) != 0`, so it does not
  filter.
  - `fn_0072BA00` (reached from `GScript::SpellAtPoint` 0x70C5F0 / `fn_007217A0` with mask 3) measures
    `GetDistanceInMetres` from **originalCastPos (+0xC0)**, not castPos (+0xCC), and needs Get2DRadius (the
    magnitude) strictly greater than that distance (0x72BA43..0x72BA5B). The type bits are 19 → 2 and 20 → 1.

#### The object

- List `g_game +0x205CA4`, the newest first, count +0x205CA8, linked through +0x5C; `MapShield` ctor 0x72C070
  (`FixedObject(pos, info, 0, 1)`).
- The mesh of both shields is 0x22A (554): `GMapShieldInfo` 0xDA05D0 / 0xDA06D8 (`MapShield::Create` gives the plain
  shield, 0x68 bytes, the first row at 0x72BE53, and the physical shield, 0x100 bytes, the second at 0x72BEA8; so a
  shield's weight and collision sound are its own row's), `MapShield::GetMesh` 0x72C1B0. The
  constants of `ProcessShield` / `DrawShield` are at 0x9828B0..0x9828D0 (the die factor 1.5 at 0x9828C8).
- `ToBeDeleted`: `PhysicalShield` 0x72CC50 deletes its PSys, then `MapShield::ToBeDeleted` 0x72C0F0 takes it out of
  the list.
- **MagicShield** (fn_0072C250): `SetScale(0.017 × r)`; `Draw`, `DrawShield` and `ProcessShield` empty; `SetDying`
  0x72C320 deletes it immediately (3); `IsEffectReceiver` 0; does not interact with physics.
  `MagicShield::ProcessShield` 0x72C2E0 is empty; `InteractsWithPhysicsObjects`: PhysicalShield 0x72D600 = 1,
  MagicShield 0. The magic shield is static (`MagicShield::Get3DType` 0x72C340 → `Object::Get3DType` 0x6364F0).
- **PhysicalShield** (fn_0072C9F0 after the zeros and ones of fn_0072CB70):
  - `creationTurn` = the turn; **`SetScale(1.0)` and the height `shieldHeight + raiseWithScale × 1.0` with the 1.0
    that `finalScale` still has**; `startSpin = clamp(Spell +0x98, ±3)` — **Spell +0x98 is the `curl` of the spell's
    PSysProcessInfo** (+0x64 + 0x34; the curl of the script or of the interface); `endSpin = ±0.15` with the sign of
    the initial one (0 counts as +); `startScale = 0.017 r × 0.01`; `finalScale = 0.017 r` and
    `SetScale(finalScale)` (`finalScale` is set through `fn_0072D5E0`).
  - **The physical shield follows the land:** `PhysicalShield::Get3DType` 0x72CE50 = 1 (a morphable 3D object).
    `UpdateMelting` runs at creation (`CallVirtualFunctionsForCreation` 0x72CD23, `SetUpPhysOb` 0x72CEB8) and again
    on every `DrawShield` after the interpolation (0x72D01E), so it follows the land as it grows.
  - fn_0072CD40: the current and previous matrix and scale, **two `ProcessShield` to prime it** and the effect
    SF_PhysicalShieldFX (PARTICLE_TYPE 0x43) at `(x, ground + height, z)` with magnitude 1, then `SetPlayer`,
    `AddTarget(the shield)` and `SetMagnitude(radius)`. The shield advances it itself each turn with a
    PSysProcessInfo of zeros, strength 1 and active (vt 0x100 in `ProcessShield`).
  - `ProcessShield` 0x72D190 (from `Spell::ProcessSpells` via fn_0072BF80, before the maintenance):
    - `t` = (turn − creation) × 0.1 s. Before 0.5 s, **A = 1 − t/0.5: the shield is born at full size and shrinks
      while it is not drawn**, and the spin does not advance. Afterwards, `u = t − 0.5`: A = x + x² − x³ with x =
      u/1.5 (1 after 1.5 s) and the spin `startSpin + (endSpin − startSpin) × B`, B = y + y² − y³ with y = u/6.
    - Dying: `dieTime += dt`; after 1.5 × 1.5 = 2.25 s it is deleted; otherwise, A × (1 − clamp(dieTime/1.5)).
    - scale = start + (final − start) × A; `bob = fmod(bob + 1.3 dt, 2π)`.
    - Height (+0x1C) = `shieldHeight + raiseWithScale × finalScale + (sin(bob) + 1) × bobMagnitude × scale × 0.5`:
      **the bobbing goes with the new scale** (not A); with the data 0, -2 and 3 the shield is between -1.36 and
      +0.68 m relative to the ground at full size.
    - `SetScale(scale)` only if it changed from the previous turn and **differs by more than 0.3 (double 0x900C70)
      from `GetScale`**: the collision scale goes in jumps behind the drawn one (0.68 → 0.276 when shrinking, then
      0.597 when growing, for r = 40).
    - The matrix: identity × scale rotated by the angle in Y (rows 0 and 2), at `(x, ground + height, z)`:
      diag(scale) (0x72D438..0x72D4BA), turned about Y by the angle at +0xEC inline, as `RotateY` 0x5198F0 does
      (0x72D4DB..0x72D558: r0' = c r0 + s r2, r2' = c r2 − s r0).
  - `DrawShield` 0x72CED0 (each frame, via fn_0072BF50): scale and matrix interpolated between the previous turn and
    this one by the turn fraction; alpha (+0x70) = `max(40, min(1, strength) × 255)`; **it is only drawn after
    0.5 s**, with `SetGlobalAlpha(1)` and the white tint (the alpha of its own texture is seen). The PSys alpha (+0x14
    → +0x6C) is the shield's and, when dying, × the clip of dieTime/1.5 **which the code does backwards**: 1 until
    1.5 s and 0 afterwards.
  - **Mesh material** (`PhysicalShield::CallVirtualFunctionsForCreation` 0x72CCB0): `fn_0057E220(obj3D vt+0xF8, 5,
    0xD)` (0x72CCCD) and `(…, 4, 0xD)` (0x72CCE5); vt+0xF8 of the vtable 0x9A32A0 is fn_007F9E70 = `[this+0x7C]`, the
    LH3DMesh. fn_0057E220 walks **all** the submeshes (+0xC / +0x10) and their primitives (+4 / +8) and changes only
    the dword +0 (type) if it equals `from` (0x57E252..0x57E256): neither byte +5 (sides, WRAP) nor ALPHAREF. In
    MSH_S_SOLID_SHIELD (554) submesh 1 (inner layer, AlphaTextured 4) becomes AlphaTexturedAlphaAdditiveNz 13 = mode
    fn_0082ECD0 (SRCALPHA / ONE, no alpha test, **no Z write**); no primitive is of type 5. Submesh 0 (outer,
    TexturedChroma 9, ALPHAREF 200) does not change: with the table 0xC387C8 it goes to mode 15 (fn_0082E470, SA/ISA,
    alpha ≥ 200, writes Z). It changes the shared mesh, for all physical shields. The alpha test of the outer layer
    scales ALPHAREF by the object's alpha (mode 15, 0x82E579..0x82E5AE: `al = material+4`, `fild`, `fimul` by the
    alpha byte, `fmul 1/255`, `fsub 5`). The same function also calls the base
    `SingleMapFixed::CallVirtualFunctionsForCreation` 0x52E880 and makes the path links (vt 0x78 / 0x80 / 0x88 / 0x98
    / 0x1E8 on [esi+0x40], fn_00644DF0 if (obj+0xA & 1) == 0).
  - `SetDying` 0x72D170: dying and without a spell; it fades by itself (shrinks in 1.5 s and is deleted at 2.25 s).
  - Physics: `InteractsWithPhysicsObjects` 1, `GetAlwaysRemainsInPhysicsInternalSystem` 1 (it stays in the system
    even if nothing moves nearby: `PhysicsObjects` adds it at each `BeginTurn`), constants of row 10
    (`GetPhysicsConstantsType` 0x72D7E0), weight `GMapShieldInfo.weight` = 50000 × scale³ (`Object::GetWeight`). The
    collision mesh is the physics submesh of 554 (28 vertices), at the object's scale. The physics model is 20
    substeps of 5 ms per turn (`GameTurnUpdate` 0x646046), per-vertex contact against the faces of the other body
    (`CollideVertices` fn_007FDA60 with the centre→vertex segment, fn_007FBF80) and springs from
    `PhysicsConstants.txt`; the mesh branch of fn_007FDD60 is for the other body with +0x168 (fn_008683C0).
  - `ReactToPhysicsImpact` 0x72D610: if whoever hit it (`GetGameObjectWhoHitMe` 0x644F00, the striker) is available,
    `PhysicallyDestroysAbodes` (vt 0x7A8) and the spell has strength: `SpellEvent 5` at its MapCoords (x, z, its
    height above the ground) without a target, `PayFor(|v| × mass × chantCostPerImpactMomentum (25) × 0.0001,
    forced)` with the speed and mass of the striker (the payment is `fn_0072B830`), `Town::UpdateAggressor` and the
    strike or destruction reaction. When the thrown object has no player, `Town::UpdateAggressor` takes the hitter's
    own `GetPlayer` and, with none, the **neutral** player (g_game +0x205A5B is `NeutralPlayerIndex`, 0x73C9C7; an
    earlier reading of it as the interface's player was wrong). See [town aggression](../beings/towns.md#town-aggression).
  - The physical shield accepts non-burning effects (`IsEffectReceiver` 0x72CC80); the magic one none.
- `IsPointDefinietlyWithinShieldVolume` (vt 0x870): MagicShield 0x72B850 is a 3D sphere with radius the spell's
  magnitude; PhysicalShield 0x72B8E0 a cone of its `Get2DRadius` R: `d² < R²` (x, z) and **the height of the point
  above the ground (MapCoords +8, relative)** less than `H (1 − d/R)`, with H = `GetHeight` 0x638120 (mesh +0x28 ×
  scale × 2). `fn_0072B990` (`Reaction::ApplyReactionToLivingObjectsAtSquare`): a living thing within the 2D radius
  of a shield ignores a reaction whose origin is not inside it.
- **The mesh's box**: `Get2DRadius` / `GetHeight` come from the mesh's box. `LH3DMesh::ComputeBoundingBox` 0x8081B0
  walks all the submeshes (+0xC / +0x10, the physics one of 554 included) on load (the header brings it zeroed) with
  `LH3DSubMesh::ComputeBoundingBox` 0x87FB20 → `LH3DPrimitive::ComputeBoundingBox` 0x807F30 (the first point comes
  out as the maximum and the second as the minimum, 0x807F97..), and +0x24 / +0x28 / +0x2C end up being the **half
  extent**: (max − min) and then × 0.5 (0x80833A / 0x808349 / 0x808355; +0x30 is its magnitude). That is why
  `Object::Get2DRadius` 0x638180 = scale × max(half X, half Z) and `GetHeight` 0x638120 = scale × half Y × 2 (`fadd
  st, st` 0x63813D) = the whole height.

#### The villagers' reactions

Fully read in the exe (`VillagerReaction.cpp`, 0x765BB0..0x766010). Reaction 13 is created by the spell when cast and spread then (`SpreadReaction`),
and again whenever the turn's walk over the reaction list comes to it (one reaction a turn,
[the animals' reactions](../beings/animal-life.md#reactions)), so the villagers who are nearby at either moment get a
chance. The initiator of reactions 13 / 35 / 36 is the
**spell itself**, whose position is what `Reaction::GetPos` 0x6E45C0 returns (the spell's +0x14).

- `Villager::ReactToMagicShieldPriority` 0x765BB0: 0 if the initiator is not a `SpellShield` (0x765BCD) or is not
  available (vt 0x2C, 0x765BDD); **without a town it already returns the priority** of ReactionInfo[13] (+0x10,
  0xD4FBD4, 0x765C08 → 0x765C48); with a town, 0 if `TownDesire::GetDesireSignificanceToVillager(town +0x34,
  TOWN_DESIRE_FOR_PROTECTION 3)` 0x746660 is 0 (`test ah, 0x40`) or if `turn − town +0xEB0` (the turn of the last
  aggressor) exceeds `GVillagerInfo` +0x364 (`GVillagerInfo::numGameTurnsAfterAggressionInterestedInShield`, matched by name;
  the offset was not recounted), compared unsigned (0x765C21..0x765C46). The distance it
  computes at 0x765BEF **is discarded** (`fstp st(0)`): it has no influence.
- `Villager::SetupReactToMagicShield` 0x765C60: `AddReaction(reaction, 168)` (vt +0x990 → `Living::AddReaction`
  0x5F0F30: stores the reaction and pushes the final state; `Villager::GetFinalState` 0x751DD0 is the top state when
  it is a final one, table +0x0C, else the destination state) and +0xBC = the spell; if it is **not** already under the
  shield (`SpellShield::IsUnder(its position, 0.2 R)` 0x72BD20, 0x765CC4) it walks with `SetupMoveToWithHug` to
  `centre + GetPosFromAngle(heading centre→it ± π/8, 0.8 R (1 − random³))` with final state 168, that is, it goes
  underneath, biased towards the edge; and in both cases it leaves the look-at point (+0x10C, `JustWholeMapXZ`) **1 m
  from itself in that same heading**, i.e. looking out of the dome (0x765D8B..0x765DEF).
- `Villager::AmazedByMagicShieldReaction` 0x765E00 (row 168 of the state table; its exit is the thunk 0x5B0100 =
  `Villager::ExitReaction` 0x7527A0): with a town, the spell still available and the protection desire > 0 (`test
  ah, 0x41`), `LookAtPos(+0x10C, 0)` one step per turn and, once it is looking, 1 time in 4 (`GameRand(4)`) a new
  point; the state's clip (`Villager::AmazedByShieldAnimation` 0x4240C0) is kept for `(GameFloatRand(10) + 20) × 1000
  / duration` loops (`IsReadyForNewAnimation` 0x5EC960) and then drawn again, unless it is 286 INTO_POINTING, which
  after one loop moves to 395 TALKING_AND_POINTING (0x765F55..0x765F8D). If the town, the shield or the desire is
  missing: `SetupWaitForCounter(ftol(GameRand(60) + 20), 163)` 0x76B060 (0x765FC0), with row 57 WAIT_FOR_COUNTER
  (`Living::WaitForCounter` 0x5EC310).
- `Town::UpdateAggressor` 0x73C9B0 with its per-player slots: the physical shield's impact calls it
  (0x72D74F..0x72D788: `EffectValues(2, 0, the hitter, 1.0, PhysicsObject::GetPlayer 0x647460)` and value 0). It also
  plays the `TownAttackSFX` 0x71B7C0 and gives the help spirit's guidance about a creature or a town attack
  (0x73CAAA..0x73CB29: not a creature mimicry, see [the emergency's creature part](../creature/mimicry.md#the-emergencys-creature-part)).
- `MapShield::CreatureMustAvoid` 0x72C170 (vt 0x614): not controlled by script, +0x24 & 0x400, and player different
  from the shield's; if the shield no longer has a spell, +0x60 = 0 at 0x72C155, its player is that of
  `GameThing::GetPlayer` 0x570130 = the interface's, g_game +0x205A5B (not NULL). The creature's path that uses it is
  at 0x54AF60.
- The impression, `GetImpressiveValue` ([belief from the miracles villagers react to](../beings/villager-reactions.md#belief-from-the-miracles-villagers-react-to)):
  `SpellShield::GetImpressiveValue` 0x72BA80 gives 0 for a villager whose town has the shield's player as aggressor
  (+0xEAC) if the reaction is 13 or 35, and otherwise `Spell::GetImpressiveValue` 0x721630, × 4 when it is 36
  (0x72BAED); `PhysicalShield::GetImpressiveValue` 0x72D7F0 is the same with 13 and with base
  `Object::GetImpressiveValue` 0x639860, without the × 4.
- The spreading replaces the reaction a villager is already following with another that scored higher (0x6E4134).
- The town's protection desire rises when someone other than the town's own player harms it (the town aggression),
  and the aggressor's turn comes from every attack; so a villager with a town reacts only once its town has been
  attacked. `TownDesire::GetDesireSignificanceToVillager` 0x746660 reads three desire tables, TownDesire +0x90 /
  +0xD4 / +0x118: it gives ([+0x118] + [+0xD4]) + [+0x90] − info +0x18, and 0 when that is ≤ 0 or NaN (`test ah,0x41`).
- `PhysicalShield::GetImpressiveValue` 0x72D7F0 in full: when the villager has a town (vt +0x2C8 and vt +0x48), the
  town's +0xEAC is the shield's player (vt +0x1C) and the reaction (+0x24) is 13, it gives 0 ([0x8AA398]); otherwise
  `Object::GetImpressiveValue` 0x639860, with no × 4.

#### The particles

- **`DefensiveShield` registry** (list 0xD4EE48, `PSysShield.cpp`): `UR_AddDefensiveSphere` 0x6A2A60 creates the
  first time a `DefensiveSphere` (fn_006D0B20) at `GetCurrentParentPos` (the effect's origin, in SF_DefenseSphere)
  with the provider's radius (magnitude × 1.11062); afterwards it only follows the radius. It is deleted with its
  collection.
- `DefensiveShield` (PSysBase, ctor 0x6D0A60: +0x14 prev, +0x18 next, +0x1C the owning PSysManager) and its only kind
  `DefensiveSphere` (0x30 bytes, fn_006D0D20): TSphere {radius +0x20, centre +0x24}. It is removed with fn_006D0B70
  from UR_AddDefensiveSphere's CollectionData dtor fn_006A2A30.
- The sphere vtable: vt 0xFC `FindIntersect` 0x6D0D50 (→ `GJUtils::FindIntersect`), vt 0x100 `DeflectOffShield`
  0x6D0D80, vt 0x104 `HasCrossedIntoShield` 0x6D0DC0 (`to` inside, `from` not), vt 0x108 `IsPointInShield` 0x6D0DA0
  (→ `GJUtils::PointIsInSphere` 0x57C9F0); the list helpers fn_006D0BC0 / fn_006D0BE0 (first sphere containing a
  point) and fn_006D0C20 / fn_006D0C40 (first sphere a move crossed into).
- `UR_AddDefensiveSphere` (+0x20 SphereRadius, IsMagical): the first run is marked in CollectionData +0x20; the
  centre never moves.
- **The deflection**: `UpdateRuleGravityWithFloor::DoAnyShieldDeflections` 0x6A1FA0. The original calls it **at the
  end of each atom's update** (0x6A1F48) only with `CheckShieldDeflections` (+0x72), with `oldGlobal` = the atom's
  global position **at the start of its update** (0x6A1903). Margin = 1.25 × baseScale × ruleScale; if the atom
  crossed into a sphere: the intersection (`FindIntersect` 0x57CCD0, the radius + margin), a spark
  (`AddImpactTarget`, fn_006D0AF0), and a `SpellEvent 4 {impact, the last global movement, 1, target = the shield's
  spell}` to the atom's own effect; with 0 the atom stays at the impact (`GlobalToLocal`) and its velocity bounced
  (`v −= 2(v·n)n`, 0x57CFD0) and it returns true; with 1 it passes. In the spell, `ApplyDefaultSpellEffect` →
  `SpellHitSpell`: the shield pays strength × `costPerShieldCollide` (FIRE 600/800/1000) and the ball an event; if
  the shield still has strength, strike reaction and 0 (it bounces). `UR_CloudMoverNew` (0x6D46D9) also calls the
  function.
- `ApplyDefaultSpellEffect` with `event +0x20` (`checkShields`, 0x720D19): with a sphere that contains the point
  (margin = the effect's radius) it sends itself an event 4 with itself as target; with 0 the event is cut, otherwise
  a spark at the point (the intention is not clear).
- `SpellTargets` also stores **points** (12 bytes, +0x14): fn_00670F00 takes a point first and otherwise an object,
  its position.
- `UpdateRuleShieldSpark` 0x6A2BF0 (an AtomCreateRule): for each impact, while there are fewer than
  `MaxNumAtomsForCollection`, an atom at the impact with its `SoundSpark`, three random angles and a number of waves
  2..5; those older than `SparkLife` are deleted. `ModifySubCollection` 0x6A2F90 draws with its N atoms **an arc from
  the centre of the sphere to the impact**: atom i (from N to 1) at u = i/N at u R along the direction of the impact,
  deviated R × WiggleAmpl × sin(k u π) sin(4t + a3) and sin((k+1) u π) sin(2t + a3); scale 0.04 R (1.5 + 0.4 (1 +
  sin(a3 − 6t))), alpha (1 − t/SparkLife) × 510 between 0 and 255.
  - Properties: +0x20 NextGroups, +0x28 PCreator, +0x2C SphereRadius, +0x30 MaxNumAtomsForCollection, +0x34
    SparkLife, +0x38 WiggleAmpl, +0x3C SoundSpark. AtomData: three PSysFloatRand(2π) at +0x20..+0x28
    (0x6A2CF7..0x6A2D16), the wave count +0x2C = ftol(PSysFloatRand(4) + 2) (a float draw, not PSysRand;
    0x6A2D1E..0x6A2D31), the orientation +0x30 rebuilt from the position each step (the spark atom never moves).
  - Defaults (ctor, 0x6A2B97 / 0x6A2B9E): SparkLife 3.0, WiggleAmpl 0.08. SF_DefenseSphere gives SparkLife 3 and no
    WiggleAmpl, so its arcs wiggle by 0.08 R. openblack's rule (`Particles/Rules/Shield.cpp`) has the same defaults;
    test `ShieldMiracleParticles.ASparkLivesThreeSecondsAndItsArcWigglesByAnEighthOfATenthOfTheRadius`.
  - The orientation fn_006A2D34 / fn_006A3AA7 give an atom from a point about the sphere's centre: the identity
    turned by the colatitude π/2 − atan2(y, |xz|), then by the yaw atan2(z, x); the Y row ends up along the point's
    direction.
- `UR_InitialSpin` 0x69E490: when the effect has a player, spin = curl (manager +0x58 = PSysProcessInfo +0x34) ×
  ScaleAngularVelocity between ±MaxAngularVelocity, which fades in TimeToFade; rotates the atom about its Y. Ctor
  0x6BF450: ScaleAngularVelocity 1.0 and MaxAngularVelocity 1.0 (0x6BF47B..0x6BF483), TimeToFade 2.0 (0x6BF48C); the
  editor range 0..1 of the three properties is not applied; SF_DefenseSphere.txt gives 1, 5 and 5.
- `UR_VapourEndEffect` 0x6A39E0: the surface patch is placed on its parent (the sphere's tracer), oriented the first
  time with its Y outwards and afterwards rotated by how much the parent moved (axis last × current, angle acos);
  scale = ScaleFactor, **alpha = age × 30** (full at 8.5 s).
- **The spinning dome**: the original interpolates each frame **the whole 3×4 matrix** of the atom (fn_00679C30,
  from fn_00679920 0x6799F9), rotation included. What is seen when spinning: 15 additive caps (mode 13, no Z write, Z
  test only against the world, two-sided, per-patch ordering which in additive mode changes nothing) that overlap 2-3
  times, with a curvature that does not match the sphere and 4-bit DXT3 alpha rings; when they move the sums change
  where they overlap. The per-vertex light is fn_00858BA0; the original's fraction goes from 0 to 5
  (0x6799A8..0x6799DF).
- `SetCollectionAlpha` 0x6A2720: collection alpha = clamp(provider, 0, 255) (in the dome, strength × 255, 40..255).
- `UR_AtomsAtEPTarget` 0x69A960: takes an object target the first time and makes one atom per extra point of its
  mesh (5 if it has none; MSH_S_SOLID_SHIELD has none, so the five go to the object's position); if the object goes
  away, it deletes its atoms.
- `CheckShieldDeflections` 0x6A2570 (atom rule, no file uses it), with its `MoveToBaseGroup`.
- **`UR_SphereSurfaceTracer` 0x6A32B0**: the radius property is `ScaleSphereRadius` (not `SphereRadiusFP`, which does
  not exist), the angles with `fmod 2π`, the alpha `Alpha × ScaleAlpha` and, outside a hierarchy, + the parent's
  position.
- **The tracer's `OrientToSurface`** (+0x44, 0x6A351D..0x6A35C4; only SF_DefenseSphereInHand and
  SF_DefenseSphereOnHolder set it to 1, the SF_DefenseSphere dome has it at 0). Each patch is rotated outwards from
  the sphere at its (θ, φ): `fn_006743A0(π/2 − φ)` (rows (cos a, −sin a, 0), (sin a, cos a, 0), (0, 0, 1) and the
  fourth row at 0) and then in each row (x, z) → (c x − s z, c z + s x) with c, s = cos θ, sin θ; the patch's local Y
  axis ends up on the normal. The in-hand effect (`particleTypeInHand` 65 of `GMagicShieldInfo`[0],
  `SF_DefenseSphereInHand`: a root atom that follows the hand with `SetScale` = `RenderHandScale` × 3.5 and
  `CreateRuleSphere` of 15 MSH_S_SPELLBALLSURFACE02 patches in group 1) forms **a single ball** that spins in the
  hand.
#### More addresses from the code

- `MapShield::GetPlayer` 0x72C150 is the shield's player that `CreatureMustAvoid` compares (0x72C184..0x72C193, as
  pointers: the same player, two null ones included, gives 0).

## openblack

Status: **done** for the spell, the objects, the villagers' reactions, the creature's avoidance and the particles;
not ported: the strike reaction's listeners, the reaction switch, the physical shield's impression
and the shields in the map's object grid. Port decisions:
[port decisions: magic](../project/port-decisions-magic.md).

- **Files**: `Magic/Spells/SpellShield`, `Magic/Objects/MapShield`, `Particles/Rules/Shield`,
  `ECS/Systems/Implementations/VillagerShield`. The miracles' turn, frame and land steps and the reaction spread reach
  the shields through `Locator::magicShieldSystem` (`MagicShieldSystemInterface`); the order and the lists are the
  same as before.
- **The spell**: the reaction's radius is `reactions::SetRadius`, the struck reaction's refresh `reactions::Stamp`.
  `SpellOps` has `updateStruckReaction` / `setUpDestroyedReaction`. A warning for the other classes: in a
  `Spells/*.cpp`, a function of the anonymous namespace called `CloseDown` and registered as `ops.closeDown =
  CloseDown` inside `openblack::magic::Register...()` **resolves to `magic::CloseDown`** (the dispatch), which calls
  itself endlessly: the game hangs when the spell closes. Here it is called `ShieldCloseDown`. The players that get an
  anti ring are those the land created.
- **The object**: in openblack the `Alpha` component = 1, or 0 before 0.5 s; the PSys alpha is
  `Effect::SetGlobalAlpha`. The material change is
  `L3DMesh::ReplaceMaterialType` / `L3DSubMesh::ReplaceMaterialType` (type and fields of `k_MaterialTypeLut[to]`;
  `twoSided`, `wrap`, `uvOffset` and `alphaCutoutThreshold` remain) from `map_shield::Create`. Before, the inner layer
  was SA/ISA and also wrote Z with alpha 0: murky orange holes, a double net and pieces of the back wall missing with
  straight triangle cuts. Now the holes add their pinkish glow and the back wall looks the same in all of them. The
  drawing order of the submeshes within the object (submesh 0 first) is 🔍 inferred; with the inner one additive and
  without Z it hardly matters. The outer layer's alpha test is faithful: the shader compares the unscaled texture
  alpha against the threshold − 5/255, which is the same. `Get2DRadius` / `GetHeight` are exactly what openblack does
  with `GetBoundingBox().Size()` (× 0.5 for the radius, as is for the height).
- **The reaction blocking**: `map_shield::IsReactionBlockedByShield(living, origin)`, for the reaction spreads of the
  villagers (the fire one has its `TODO` in `VillagerFire.cpp`).
- **The spreading fix**: in openblack a `Spell` entity has no `Transform`, so `reactions::SpreadReaction` cut off
  before starting and **no shield reaction reached anyone**. `Reactions.cpp` now reads the initiator's position with
  `MapPosOf`: the `Transform` if it has one and, otherwise, `components::Spell::position`.
- **The villagers**: the clip is `ClipChoice::AmazedByShield`; row 57 WAIT_FOR_COUNTER is ported with it. In
  `VillagerShield.{h,cpp}`:
  - `ReactToMagicShieldPriority`: the reaction's priority when its initiator is an available shield spell and the
    villager has no town, or its town wants protection and was attacked lately; else 0.
  - `IsAvailableShieldSpell`: a live shield spell (a `Spell` of the shield class, not marked by `ToBeDeleted`).
  - `ProtectionDesireSignificance`: the town's protection desire through `ecs::town_desire`;
    `AttackedRecently`: the turns since the town's last aggressor against the villager info's limit, unsigned.
  - `ApplyShieldReaction`: the shield part of the reaction spread for one villager, as the fire's and the
    teleport's (no reaction of its own, a score above 0, not reacted to a shield lately; the reaction's turn stamped
    by `reactions::MarkStarted`).
  - `LookAtPoint`: 1 m from the villager along the bearing from the dome's centre, jittered by ±π/8, so it faces out.
  - `UpdateAmazedClip`: the clip held `GameFloatRand(10) + 20` seconds (drawn every turn, before openblack's clip
    guards), INTO_POINTING (286) followed once by TALKING_AND_POINTING (395); `FinalState`, the original's
    `GetFinalState`.
  - The town's protection input is `DesireInputs::protection` (`ECS/TownDesire.h`).
  `Town::UpdateAggressor` is ported with its per-player slots ([town aggression](../beings/towns.md#town-aggression);
  `components::Town::aggression`), with the `TownAttackSFX` and the help spirit's guidance. The town's protection
  desire is read (`town_desire::GetDesireSignificanceToVillager`, `ECS/TownDesire.cpp:1106-1112`: desire + boost +
  boostA − the trigger, the same sum and order as the original's, used by `VillagerShield.cpp:157-163`, :324 and :391).
  openblack differs: `PhysicalShield::GetImpressiveValue` 0x72D7F0 is not ported (`MiracleImpressiveValue`,
  `VillagerReactions.cpp:411-441`, has no physical-shield branch at :432; also `magic::ShieldImpressiveValue`,
  `Magic/Impressiveness.cpp:67`), a fix row if openblack's physical shield makes reaction 13. A minor approximation:
  `record.lastTurn != 0` (`VillagerReactions.cpp:436-439`) stands in for the original's null test of the town's
  aggressor (+0xEAC), so an attack made on turn 0 reads differently. To see the reaction in the game without an attack
  there is the hook `OPENBLACK_TEST_SHIELD_REACTION=1`, which treats both conditions as satisfied. Not ported from the
  spreading: the switch to a higher-scoring reaction (`reactions::MaySwitch`), the same as in fire and teleport.
- **The creature's avoidance**: `MapShield::CreatureMustAvoid`'s condition is ported, with openblack's PLAYER_ONE as
  the interface's player. Its player comes in as a parameter: the creature's route follower calls it through
  `route_plan_world::CreatureMustAvoid` (`src/ECS/RoutePlanWorld.cpp`) with the creature's owner, so a creature walks
  round another player's shield, as the original's creature path does.
- **A creature's shields**: a creature casts the shield, its power-up and the physical shield at anything, and a
  town's shield (CastShieldAroundTown) at a built storage pit; a shield for a plan about nothing makes it idle (since
  2026-10-21, [the creature's casting](../creature/casting.md#the-attack-shield-and-impress-constructs)).
- **The particles**: `AddDefensiveSphere` (fn_006D0B20: a new sphere at the head of the list, its id) and
  `RemoveDefensiveSphere` (fn_006D0B70) keep the dome's spheres. The API for the fireballs is in
  `Particles/Rules/Shield.h`, namespace `particles::shields`:
  `DoAnyShieldDeflections(effect, atom, oldGlobal)`, `FindShieldContainingPoint`, `FindShieldCrossedInto`,
  `IsPointInShield` (strict `< (r + m)²`), `HasCrossedIntoShield` and `SpellOf`. The fireball calls
  `DoAnyShieldDeflections` (`Particles/Rules/Fireball.cpp`; corrected). `Effect::AddTargetPoint` / `TakeTargetPoint`
  store the target points. `ApplyDefaultSpellEffect`'s `checkShields` is ported as is. `CheckShieldDeflections` is
  ported except `MoveToBaseGroup`. `UR_SphereSurfaceTracer` was corrected in ParticleEffect.cpp (with `SphereRadiusFP` the dome
  came out at 1 m). The tracer's `OrientToSurface` uses `affine::TurnRows(axis 1)` (was `lh_matrix`); without it the 15
  patches of the in-hand effect all had the same orientation and looked like many pieces spinning in a ring.
- **The spinning dome**: openblack drew the interpolated position with the new turn's rotation and every 0.1 s the
  caps twisted abruptly (the spring). It is now interpolated (`Effect::CollectCollection`, ParticleEffect.cpp). Checked with
  `OPENBLACK_TEST_SHIELD_FRAMES` (SHIELD r 40, curl 5, 2.5 s): from a fraction 0.94 to 0.07 of the next turn only the
  alpha rings slide; within the turn (0.07 → 0.94) the dome moves as a whole and smoothly. The per-vertex light is
  ⚠️ approximate, and openblack's fraction goes 0..1 against the original's 0..5 (it only matters if a frame takes
  longer than a turn).
- The correction of the PSys hierarchies that the dome needs is in
  [the fix in the PSys core: the hierarchies](../physics/particles.md#fix-in-the-psys-core-the-hierarchies).
#### Where the code is

- `ShieldInfoOf` (`SpellShield::GetMagicInfo`) and `DataOf` (the ctor's list entry) in `SpellShield.cpp`;
  `ProcessPhysical` (the physical shield's turn and matrix) in `MapShield.cpp`; the mesh is `MeshId::SpellSolidShield`;
  the interface's player is `PlayerNames::PLAYER_ONE`.

### Test hooks

- `OPENBLACK_TEST_SPELL` accepts `curl` (the seventh value); `OPENBLACK_TEST_SHIELD_SHOT` captures by turns
  ([debug environment variables](../project/debugging.md#debug-environment-variables)).
- `OPENBLACK_TEST_SHIELD_REACTION=1`: the villagers' reaction without a town attack.
- `test_shield`: clipping, cost, curves, sphere helpers, registry and deflection with a real effect, hierarchy frame
  and, with `OPENBLACK_GAME_PATH`, the real rows (5 / 1000 / 30, 25, 0 / -2 / 3, 20 and 22, weight 50000) and
  `physicalShieldMaterialTypes` (loads 554 from AllMeshes.g3d with bgfx Noop: after (5, 13) and (4, 13) submesh 1 is
  type 13 additive without Z, two-sided and WRAP; submesh 0 stays type 9 with threshold 200/255 and Z).
- `OPENBLACK_TEST_SHIELD_FRAMES="<turns>,<n>,<prefix>[@<slow>]"`: n screenshots in consecutive frames from those
  turns after the first shield (`<prefix>_<i>.png`; the log gives the turn and its fraction). Each screenshot stops
  the frame ~0.45 s at 1600×900: with `-W 640 -H 360` two come out per turn (fraction ~0.07 and ~0.94). `@<slow>`
  multiplies the turn duration; the PSys interpolates with the game's fraction (`game_clock::TurnFraction`, g_game
  +0x205D64), so it also goes slowly. MapShield's own fraction still uses the wall clock
  ([the game clock, Pending](../engine/game-clock.md#pending)).

## Pending

- **Code layout**: the shield list is still a vector, newest first, in the magic objects service. Making it an order
  key on the `MapShield` component waits for the fireballs' list, which shares the counter.
- **Still out**: reaction 35 (strike), which nobody attends to, and the reaction switch (`reactions::MaySwitch`).
  Reaction 36 (destruction) is taken up by the villagers since 2026-10-17: they hide in a building of their town or
  panic ([the villagers' reactions](../beings/villager-reactions.md); ported from raffclar's tree, unverified against
  v1.42). The creature's path that uses `CreatureMustAvoid` (0x54AF60) is ported (see openblack).
- openblack differs: the physical shield's impression (`PhysicalShield::GetImpressiveValue` 0x72D7F0) is missing
  (`VillagerReactions.cpp:432`), a fix row; and the turn-0 aggressor test (`VillagerReactions.cpp:436-439`), a minor
  fix row.
- **The map's object grid**: the shields are not in it, so `ApplyEffectToMapPos` does not reach them (the physical
  one would accept non-burning effects, `IsEffectReceiver` 0x72CC80; the magic one none).
- **`CallVirtualFunctionsForCreation` 0x72CCB0**: the base `SingleMapFixed::CallVirtualFunctionsForCreation`
  0x52E880 and the path links are not ported; the material change `fn_0057E220` is.
- **The meshes' extra points** are not loaded (`UR_AtomsAtEPTarget` uses the object's position, exact for 554).
- **Equal sort keys**: the physical shield and its SF_PhysicalShieldFX at the same point are sorted with a non-stable
  `std::sort`: what the Z-sorter does with ties is still unread (0x82F280 turned out to be the callback loop of
  `FinishFrame`, not the ordering; 0x82F460 must be read). It belongs to the common rendering.
- **The fast rock** (33 m/s) that passes through the physical shield's shell **is not proven to be a deviation**: the
  original uses the same model that openblack ports, and the mesh branch of fn_007FDD60 is not used by any openblack
  body. The strike **is detected** (it pays chants), but the spring does not brake 33 m/s in the two substeps the
  contact lasts. A screenshot of the original is missing to compare; nothing in Physics has been touched.
- **Objects with `Alpha`** (`Renderer.cpp`): a Standard primitive with `depthWrite` = false (types 6, 7, 8) still
  writes Z and does not switch mode table (0xC387C8) per primitive; 554 is no longer affected.
- **`Primitive::modulateAlpha`** (α = texture × diffuse of modes 3, 5, 6, 8, 10..13, 15, 16) **is read by nobody**:
  `fs_object.sc` always multiplies the alpha by the object's. After the material change the inner layer of 554 is of
  type 13, which is × diffuse, so for the shield it matches the original.
