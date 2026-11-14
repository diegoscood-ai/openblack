# Forest

## Summary

The forest miracle (NATURE) plants a whole forest at once when its seed lands: up to 18 magic trees on a spiral
2 to 11 m around the cast point, whose species depend on the ground under the cast point. While the spell is paid for
the trees grow; when it closes they all shrink and disappear. The hand's seed stays on the ground at the centre,
rising with the tallest tree, and can be picked up and cast again when it came from a temple icon. Over the scene a
camera take, butterflies (or bats), sparkles, a vortex and a light map play.

openblack ports the spell, its trees and forest, the seed's drawing and pick-up, the camera take and the butterflies
faithfully, inside `ECS/Trees` (there is no forest entity of its own). The forest's particle rules are on [Forest rules](../physics/particle-rules.md#forest-rules).

## The original

### Forest

What follows has been read in the exe.

#### The spell

- **SpellForest** (MAGIC_TYPE 13 NATURE; `GMagicForestInfo::AllocSpell` 0x5FAD90, 0xF8 bytes): +0xEC the Forest,
  +0xF0 "forest created" (fn_007254F0 sets it to 0), +0xF4 the maximum number of trees (`SetMaxObjectsToCreate`
  0x7256C0: -1 → `finalNoTrees` 18). `InitWithPos` 0x725540 is Spell's.
  - `SpellForest::GetMagicInfo` 0x725A20. The radius 11 is `SpellForest::GetForestRadius` 0x725560 (float 0x9819F4),
    and `fn_00725570` reads 0x9819F8.
  - `CanCast` 0x5FAE80 (vt 0x30): inside the map, land, `fn_005FADF0` (no Abode of **the cell** of the point,
    `FindType(0)`, has `Get2DRadius > distance` to its fire centre) and `ValidPlaceForTree` 0x725C50 (inside,
    land and not `MapCoords::IsFixed`).
  - The radius of `fn_005FADF0` is each object's vt 0x64, and **`Field::Get2DRadius` 0x528E80 is the constant 5 m**
    ([0x8AB6E4]): it is the only class that overrides the slot (the vtables of Object, Abode, Field, Tree and Pot
    give `Object::Get2DRadius` 0x638180 except Field's). But `FindType(0)` (`FindTypeOnMap` 0x6015E0) compares the
    type of the info (+0x10) with 0 ABODE, and that of a field is 18 FIELD (info.dat `fieldType[].type`, checked by
    `test_map_cells`): **a field never prevents the forest**.
  - **`IsFixed` 0x603790 → `MapCell::IsFixed` 0x601EA0 only looks at the first fixed object of the cell** (MapCell
    +4, where `Fixed::InsertMapObjectToCell` 0x52DEA0 puts the newest with `SetFirstObjectFixed`) and its bit +0x24 &
    2, which only the ctor of `MultiMapFixed` 0x52E1F0 sets (`or byte [esi+0x24], 2` at 0x52E207). That is: `IsFixed`
    = "the newest fixed object of the cell is a MultiMapFixed". The classes that inherit from it, according to
    bw1-decomp (`src/Black/*.h`): Abode (and with it Field and StoragePit), BigForest, CitadelPart (CitadelHeart,
    WorshipSite, CreaturePen, WorshipTotem), Feature (and AnimatedStatic), FishFarm, MobileStatic (and with it
    MagicTeleport and the street lamps), PFootball, PrayerSite, SpellIcon and TotemStatue; `SingleMapFixed` (Tree,
    MapShield, ScriptHighlight, PrayerIcon) does not set it, nor do GFootpath or BuildingSite (they are GameThing). A
    tree is alone in its cell: a cell whose last fixed object is a tree is not "occupied", even if it has a building
    underneath. Each tree of the event enters at the head of the cell's list when created (`InsertMapObject`,
    SingleMapFixed 0x52E620).
  - `SpellEvent` 0x725830: nothing with type 1 or if there is already a Forest; `ApplyDefaultSpellEffect` (pays
    costPerEvent 1; EffectValues of NATURE: alignment 1; reaction 21) and, if it applies, **the whole forest at
    once**: N = `fn_00725790` = round(+0xF4 × (strength > 0)); step = N > 1 ? 1/(N − 1) : 1; turns = N × **17/13**
    (the float 0x9819FC = 1.3076923) × 2π; for i < N: f = i·step, r = 2 + 9·√(1 − (1 − f)²), a = f·turns, point =
    castPos (+0xC0) + (r cos a, r sin a) truncated to MapCoords (ftol × 6553.6); if `ValidPlaceForTree` and the
    forest has fewer than N (fn_00725800), `fn_00725600(point, GetRandomTreeInfo(castPos))`.
  - **No shield test, no harm** (read 2026-10-24). The landing event comes from the seed's `LandscapeCollide`
    (`ModifyAtomCore` 0x67D6E0, with `SendEvent` +0x20): `SpellEventInfo {3, the atom's global point, its last global
    movement, strength 1.0, checkShields 0 (byte +0x20 = 0 at 0x67D7C4), no target}`, so `ApplyDefaultSpellEffect`
    never looks for a shield (0x720D19) and a shield over the cast point does not stop the planting. The NATURE effect
    row of info.dat has no burn, crush, hit, heal, fly away or belief, and the plain alignment modifier 1, so the
    event's `ApplyEffectToMapPos` harms nothing. openblack: `LandscapeCollide` (`src/Particles/ParticleEffect.cpp`)
    sends no shield test; tests `ForestMiracleEffects.TheSeedsSoundStartsWithItAndIsCutWhenItLandsOnTheFortyFirstTurn`
    (the landing asks for no shield test), `SpellForest.realInfoDat` (the row).
  - `GetRandomTreeInfo` 0x725580: `Terrain::GetMaterialInfo` 0x735330 of the cell of the **cast point** (not of
    each tree) and `magicTreeTypes[GameRand(4)]` (one GameRand per tree). `GetTerrainMaterial` 0x735380: 27 (snow) if
    `GClimate::GetSnow ≥ 27` at the corner of the cell; otherwise, the type of the second material of the cell's
    altitude in its country (fn_00804CF0 → fn_00873770, like `GSoundMap`); **a 0 (off the map or without a block)
    becomes 1**, which is DEEP_WATER: palm trees; `> 43` → 0. Grass (18) and almost all: beech, birch, cedar, cedar;
    sand and water: palm trees; snow and ice: conifers.
  - `fn_00725600`: with the first tree it creates the Forest (`new 0x58` → fn_005399E0(pos, creator): the creator's
    player, id = counter 0xBEA238++, at the start of the list g_game +0x205BB4) and marks +0xF0. Then
    `fn_005FD000(pos, spell, info, forest, GameFloatRand(2π), 0, woodValueMultiplier 0.25 × tribal power)` and the
    target scale +0x64 = `fn_007255C0` = **1 − 0.5 × distance/11**: 0.91 for the first (at 2 m) and 0.5 at the edge
    (the plan said 1.0 at the centre: there is no tree at the centre).
  - `Process` 0x7259C0 = `CoreProcess` + `ProcessTrees` 0x725A30 (its argument is not used): if the forest was
    deleted (+0xA **bit 0**, the ToBeDeleted one; not bit 1) → +0xEC = 0 and CloseDown; without a forest it returns 1
    while there is a PSys and 5 (the spell is deleted) when there is not; with a forest it compares N with the trees
    (+0x4C + +0x54, unsigned): N < trees → `fn_0053A490`: **all** shrink `decaySpeed` 0.05 (fn_0074A3A0: scale − d;
    ≤ 0 → ToBeDeleted); otherwise → `fn_0053A520`: **all** `Tree::Grow(growSpeed 0.01, 0, 0)`. The number both
    receive is not used. Returns 1. The forest is reached through wrappers: shrink `fn_00725B00` → `fn_0053A490`, grow
    `fn_00725AD0` → `fn_0053A520`.
  - `CalculateCostToMaintain` 0x7259E0 = costPerGameTurn (5) + trees × costPerEvent (1): 23 with 18.
  - `CloseDown` 0x55D1E0 = CoreCloseDown + creator = NULL. **The trees do not stay**: without a creator
    `GetSpellStrength` 0x720750 gives 0, so N = 0 and the whole forest shrinks 0.05 per turn until it disappears; with
    the last tree the Forest is deleted and on the next turn the spell (closed spells keep being processed:
    `ProcessSpells` only skips those that are not `IsAvailable` 0x401810, i.e. the deleted ones). A forest of a player
    (`timerWhenPlayerCasting` -1) lasts as long as it is paid for; that of a one-off miracle (120 s) or that of a
    script with a duration goes away when it closes. Real trace (duration 8 s): closes at turn 81, shrinks from ~0.4
    and at 88 none is left; the spell is deleted at 89.
  - `ToBeDeleted` 0x725500: deletes the forest (if it is not already being deleted) with its trees.
  - `GetMaxObjectsToCreate` 0x7256F0 = min(+0xF4, forest ? its trees : (+0xF0 ? 0 : 18)); stored by the seed
    (`StoreChantsAndAgeFromSpell`, vt 0x550: `SpellOps::maxObjectsToCreate`). `HasEnoughChantsAndLifeForRecast`
    0x725730 = that > 0. `AdjustSpellSeedPos` 0x725750 (vt 0x540): altitude = max(altitude, the tallest tree,
    fn_0053A740 with vt 0x42C `GetHeight`), −5 without a forest (+0xEC == 0). **It does have a caller** (what was said
    before was false): the seed drawing 0x729020, `call [edx+0x540]` at **0x72906E** (see the hand seed, below). It is
    the only class that overrides it: the pointer 0x725750 appears only once in the image (vtable 0x8F4FE4 + 0x540)
    and the other twelve spell vtables have `Spell::AdjustSpellSeedPos` 0x55CE60 (`ret 4`).
  - `fn_00725B20` / `fn_00725BF0` (a tree at a random point of the 2..11 m ring) have no caller.

#### The hand seed over the forest

**Faithful.** There are two seeds: the `Seed.L3D` atom of the PSys (falls spinning and is deleted on touching land,
see below) and the `I_Forest` SpellSeed of the hand, which is the one that stays and is picked up.

- On casting (HAND_POSITION) the SpellSeed leaves the hand (0x16) but stays tied to the spell (`deleteSeedOnceCast`
  0) and to the icon (+0x5C: only deleted by `ClearSpellIconLink` 0x7281D0 from `ToBeDeleted` 0x728280).
- Each frame `GGame::Process3dEngine` 0x54E023 → `Spell::DrawSpells` 0x7203F0 → `Spell::Draw` 0x720430 (vt 0x50C)
  → `DrawSpellSeed` 0x721360 (vt 0x508; with +0xAC it jumps to) **0x729020**: if `fn_00728FC0` (not on the map, not
  IN_HAND nor stored in the hand, `seedFollowsSpell` +0x120, spell open, **icon** +0x5C) and +0x60, altitude =
  `fn_006022C0(seed, 1)` (the top `GetTopPos` 0x638160 = altitude above the ground +0x1C + `GetHeight` of the objects
  of its cell, list +4 and then +0, which are not living and do not move and whose circle overlaps: d² < r_obj² +
  r_seed², 0 if none), vt 0x540, and `LHMatrix::Translation` to (x, ground + altitude, z), +0x44 = 1, +0x48 = 0 and
  `AddForDrawing(seed)` 0x63B5D0, which sends the draw collision: the hand sees it.
- **The ground seed does not spin.** `LHMatrix::Translation` 0x403530 rewrites the whole matrix: identity rows
  (0x403532..0x403558) and then the point. +0x44 and +0x48 are the `scale` and the `y_angle` of LH3DObject
  (bw1-decomp `LH3DObject.h`; `Game3DObject::SetPosition` 0x63B740 writes them the same way), not "alpha and flags".
  So every frame the `I_Forest` is drawn upright, without rotating and at scale 1, whatever rotation it brought from
  the hand (the four seeds that get here, STORM, NATURE, SHIELD and PHYSICAL_SHIELD, have scale 1 in
  `GSpellSeedInfo`). Nothing else moves that seed: `SpellSeed::Draw` 0x518710 is a `ret`; `DrawOutOfMap` 0x5190A0
  only draws it in the hand. The scale of the Object (+0x50, `GetScale` 0x402520) is another one.
- **What falls and spins is the atom** `Seed.L3D` of the PSys (`SF_Forest`, group 0): born 9.435 m above the point,
  `UpdateRuleGravity_Seed` (gravity 1.6, `MaxSpeed` 3.34513, undamped: about 4 s of fall) and
  `UpdateRuleRotatePrincipalAxis_Seed` (`AngularVel` 12.2611 rad/s ≈ 1.95 turns/s around its Y, `AxisChosen` 1 →
  0x6A1218; the angle is dt × AngularVel, `fld [0xD4E0EC]; fmul [+0x24]`, without tying it to anything else).
  0x6A1150 also rotates the fourth row of the atom's matrix (+0x68), but `SetRotationMatrix` 0x674120 leaves it at 0
  and the atom's position is +0x80: there is no orbit, the "spiral" is the mesh's off-centre wing. Its only event is
  `LandscapeCollide_Seed` (type 3): the trees all come out on touching land, not while it falls. `Seed.L3D`'s one
  sub-mesh is saved with flags 0, which `LH3DSubMesh::Create` 0x87FA00 makes every level at load (0x87FA06..0x87FA1F,
  |= 0xE0000800), so the original draws it. The same is true of `Vortex.L3D` (the forest's and a created magic
  object's vortex) and `S_food_chicken.l3d` (the storm's push); `mist.l3d` has its own draw that never reads the
  levels.
- **The user's recollection:** "the forest seed falls spinning at the same speed at which the trees grow; from the
  temple it stays on the ground while the forest lasts and can be picked up and cast again; from an orb it neither
  stays nor can be picked up". What the exe gives: the spin and the fall belong to the atom (before there are trees);
  what goes "at the speed of the trees" is the `I_Forest`, whose height is that of the tallest tree (fn_0053A740) and
  that is why it **rises** exactly at the pace of the growth (0.01 of scale per turn × rain × alignment). Nothing was
  found that makes it go down while the forest lives (when the spell closes, `fn_00728FC0` fails and it stops being
  drawn). The rest of the recollection matches.
- The ground is not counted twice: the altitude of a MapCoords is above the ground (`GetLHPoint` 0x605C40 =
  `GetAltitude` + y), so `GetTopPos` is too.
- Result: −5 m (underground) while the atom falls; when the forest comes out, on the ground at the centre; then it
  rises with the tallest tree (trace `seed … drawn over spell …`, altitude −5 → 0.18 → … → 16 m).
- Picking up: `SpellSeed::ValidForPlaceInHand` 0x728580 (neutral with g_game +0x14 & 0x2000, or of the hand's
  player) → 225 ms grab `GenericPickup` 0x5D2800 (inside the influence, vt 0x714 = 1) → `PlaceObjectInMagicHand`
  0x5DA6F0 → `InterfaceSetInMagicHand` 0x728810 (`StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200:
  the spell closes and the forest shrinks 0.05 per turn). With stored trees it can be cast somewhere else.
- Also in `Spell::DrawSpells`: `GPlayer::DrawSpellIcons` 0x64AF20 (fn_0064AF20, static; per player and neutral,
  `GetNextPlayerAndNeutral` 0x550980, with a citadel +0xA48: `Citadel::DrawSpellIcons` 0x463980, which calls each
  worship site's `WorshipSite::DrawSpellIcons` 0x77B3B0 on the six slots +0x34. An overloaded site (Overloaded > 0)
  moves its overload pulse phase by lerp(OverloadPulseMinSpeed, OverloadPulseMaxSpeed, clamp(overload, 0, 1)) × game
  ms × 0.001, fmod 2π, pulse = (cos + 1)/2; then the mana path PSys is drawn with alpha pulse × max, or max), fn_00682950 (the invisible collision of the fireballs, list g_game +0x205C9C,
  fn_00682F30) and the vt 0x108(1) of the object +0xB0 in `Spell::Draw`. fn_00725FE0 is a `ret`; fn_0072BF50 are the
  shields.
- **Temple versus orb.** There is no flag of the orb's own: `seedFollowsSpell` (+0x120) = 1 and
  `deleteSeedOnceCast` (+0x168) = 0 are valid for both (NATURE row of `seed_table.md`). The difference is the icon
  +0x5C: `OneOffSpellSeed::CreateSpellIntoHand` 0x72A730 asks `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 (the
  icon of that seed in the player's worship sites that passes `WorshipSpellIcon::ValidForRequestSpell` 0x77FBA0, the
  one with the most available chants fn_0077CBC0) and with an icon creates the seed with fn_007282A0 → fn_00727FF0
  (+0x5C = icon, scale = icon's scale × the seed's); without an icon, fn_00728300 → fn_007280A0 (+0x5C = 0). Without
  an icon `fn_00728FC0` fails: 0x729020 neither draws it nor sends the draw collision, so the hand does not see it
  (`ValidForPlaceInHand` 0x728580 never gets asked). The seed stays alive, invisible and tied to the spell, until
  `Spell::ToBeDeleted` 0x71FD90 deletes it (vt 0xC of +0xAC at 0x71FE16) along with the spell. **Faithful nuance:**
  an orb touched by a player who already has the forest icon in their temple gives a seed tied to that icon, which
  does stay and can be picked up.
- **When it goes away.** With the spell closed (on picking it up, through `ClearSpellLink` 0x728200; or through
  `ProcessFromSpell` 0x728F70 if it leaves its player's influence) it stops being drawn at once. The forest shrinks
  0.05 per turn; when the Forest is deleted (`ProcessTrees` 0x725A30: flag +0xA & 1 → +0xEC = 0 and `CloseDown`)
  and the PSys has finished, `ProcessTrees` returns 5 and the spell is deleted with its seed (0x71FE16).

#### MagicTree

- **MagicTree** (0x74 bytes, vtable 0x92CA30; `MagicTree::MagicTree` 0x5FCF50, created by fn_005FD000): `Tree(pos, info, forest,
  maxScale 1.0, angle, scale 0)`, magic flag +0x5C | 2, +0x70 = wood multiplier, +0x6C = the spell's player (without
  a spell, that of g_game +0x205A5B) and `CreateReaction(this, REACT_TO_MAGIC_TREE 8, GetPlayer, 0)`.
  - +0x6C = `GetPlayerNumber` 0x64A790 of the spell's `GetPlayer` (vt 0x1C, spell +0xA4) (0x5FCF91..0x5FCFC8); the
    tree's own `GetPlayer` 0x5FD060 returns +0x6C. A
    spell with a NULL +0xA4 would make the original read +0xB5 of NULL; no caller does that.
  - `ToBeDeleted` 0x5FD070: its reactions, `Tree::ToBeDeleted` and, if the forest is still available and has run out
    of trees, the forest.
  - `StartOnFire` 0x5FD0D0 removes reaction 8; `EndOnFire` 0x5FD0E0 creates it again unless the g_game flag +0x14 &
    0x8000 is set (0x5FD0EB `test ch, 0x80` skips the `CreateReaction`). **That flag is "I am inside `GGame::ClearMap`
    0x552BB0":** it is set at the start and cleared at the end (bw1-decomp `src/Black/Game.cpp`, `GGame::ClearMap`), and
    that is why the objects destroyed when changing land do not create reactions nor notify their town (`Abode.cpp` 92
    and 105) nor anything similar.
  - `GetWoodValueMultiplier` (vt 0x868) = +0x70; Tree's 0x74B810 gives 1. `Tree::GetWoodValue` 0x74B7B0 = life × that
    × woodValue × scale × [0xD1A294]. `GetImpressiveType` 14. The MagicTree's wood multiplier (0.25 × tribal power,
    +0x70, 0x5FD0C0) is copied by the DeadTree (+0x9C, 0x5108F7), which also keeps the mesh and the fire (fn_00730960
    in its ctor 0x510880) and stops being a MagicTree.

#### Tree

- The ctor 0x749E00 receives (pos, info, forest, maxScale → +0x64, angle, scale); if maxScale ≠ scale it marks
  "growing" (+0x5E bit 0) and the count +0x60 (int16) = GameRand(growsAfterNumGameTurns); then `Forest::AddTree` and
  +0x5E | 2.
- `Tree::Grow` 0x74A3F0 (amount, setScale, raiseMax): with raiseMax, maxScale = max(maxScale, scale + amount);
  nothing if the scale is already maxScale; otherwise, min(scale + amount, maxScale) with `SetScale` (vt 0x124) or
  with `SetJustScale` (vt 0x51C) and the 3D matrix rebuilt with the scale, the Y angle and the position. Returns how
  much it grew.
- `Tree::Process` 0x74A290 (vt 0x5FC, list of those growing): lowers the count; at 0 it goes back to
  growsAfterNumGameTurns and, if it grows below maxScale, `Grow(amount, 0, 0)`; returns 1 if it is still growing.
  The amount is built at 0x74A2D7..0x74A33E: `growthAmount` (info +0x11C) × (1 + 0.01 [0x8C5840] ×
  `rainingAcceleratorMultiplier` (info +0x130) × `GClimate::GetMaxRainingOrSnowing` 0x771600) × (1 + 0.5 [0x8AA3B4]
  × `MapCoords::GetAlignment` 0x6057B0). The two queries are made at `MapCoords::GetLHPoint` 0x605C40 of the tree
  itself (the ground plus its y).
- **`MapCoords::GetAlignment` 0x6057B0 (the "alignment of the place") belongs to the terrain, not to a player:** it
  starts at 0 and for each player that exists (order of `GGame::GetNextPlayer` 0x5508A0) adds
  `Influence::CalculatePlayerInfluence(pos, player, 0, type 0, allies 1)` 0x5CD170 × `GPlayer::GetAlignmentValue`
  0x64D6A0, and clips to −1..1 (0x60580F and 0x60582C). That is, the good or evil of whoever rules there, weighted by
  their influence. (`fn_00605850` then passes it through `GetDiscreteAlignmentValue` 0x414730, which is not used
  here.)
- `Tree::ToBeDeleted` 0x74A210: leaves its forest (fn_0053A220) and the global tree list g_game +0x205CDC.

#### Forest

0x58 bytes: id +0x40, next +0x44, `Trees0` +0x48/+0x4C the grown ones, `Trees1` +0x50/+0x54 the growing ones.

- `AddTree` 0x53A310: tree +0x68 = forest; to `Trees1` if it grows below maxScale, otherwise to `Trees0`; each list
  ordered by `DistanceToForest` 0x53A890 (distance to the forest, 0 without a forest): the new one goes before the
  first one that is farther away. `fn_0053A220` takes it out.
- `Forest::ProcessForests` 0x539D70 (slot 5 of the turn) → `Forest::Process` 0x539DA0: a forest with no big forest
  (+0x38) and both tree lists empty (+0x4C, +0x54) (0x539DA7..0x539DB9) sets +0x34 = 2000 and lowers it each turn;
  below 2 it is deleted. Otherwise, and when not scenic (+0x3C ≠ 1, 0x539DEF), `Tree::Process`
  of each growing tree; those that return 0 move to `Trees0`. Then the new natural trees (every 2000 +
  GameFloatRand(1000) × 0.05 × grown, next to a random one, fn_00539FD0 / fn_0053A010, and
  `GPlayer::FUN_0064da80(14, 1)` 0x539FB7 to the most influential player of the new tree,
  `CalculateMostInfluentialPlayer` 0x603830 at 0x539F9B, if not neutral). They are made in the miracle's forests too:
  a new tree in one of them makes there be more trees than N and the whole forest shrinks. +0x38 is the BigForest that
  holds the forest (`BigForestObject`: set by `BigForest::BigForest` 0x438D55 / 0x438D66, cleared by
  `BigForest::ToBeDeleted` 0x438EA7); +0x3C is `IsScenic` (0 in the ctor, `Forest::Forest` 0x539C01 / 0x539C07, kept in
  saves, `Forest::Load` 0x53B092). Both are also read by `GetNearestEdgeToPos`, `Save`, `GetWoodValue` and
  `SaveObject`.
- `ToBeDeleted` 0x539C60: `ToBeDeleted` of each tree of the two lists and leaves the list.
- **There is no merging of forests**: the miracle always makes a new Forest.
- **The Forest's player is not needed** (it is not for the alignment of the new trees): fn_005399E0 stores it
  (0x5399F4 `GetPlayer` of the creator → the GameThing ctor fn_0046B8A0), but `Forest::Process` 0x539DA0 never reads
  it; statistic 14 of a new tree (0x539FB7) goes to the most influential player of that tree and
  `GPlayer::FUN_0064da80` exits immediately outside a multiplayer game (`IsMultiplayerGame` at 0x64DA90).
  🔍 inferred no other reader was found.
- **Where the miracle forest grows, and who holds it.** The trees grow and wither only inside the spell's own turn:
  `SpellForest::Process` 0x7259C0 = `CoreProcess` + `ProcessTrees` 0x725A30 (shrink fn_00725B00 → fn_0053A490, grow
  fn_00725AD0 → fn_0053A520), run by `ProcessSpells` in the order of the spell list. There is no separate
  forest-miracle stage: `Forest::ProcessForests` 0x539D70 (slot 5 of the turn) is every forest's natural growth.
  There is no miracle forest class: the first tree makes an ordinary Forest (fn_005399E0, `new 0x58`, the id from the
  counter 0xBEA238, at the head of the forest list g_game +0x205BB4) that the spell holds at +0xEC, and
  `MagicTree::MagicTree` 0x5FCF50 takes that Forest. The Mac symbols agree (`SpellForest::ProcessTrees`,
  `GrowAllTrees`, `DecayTrees` are spell members; `MagicTree::MagicTree(…, SpellForest*, GTreeInfo*, Forest*, …)`).

#### What is seen: the SF_Forest effect

- **Without the scene**: the seed `Seed.L3D` (scale 0.207) falls from 9.4 m with gravity 1.6 (max. 3.35 m/s)
  spinning, with SOUND_SPELL_FOREST_1 and a disc of blue blotches; on touching land (turn 41, 4.1 s) the 18 trees
  come out. **Everything else hangs from the camera atom** (group 2, `ParticleAnimWithCameraCreator`:
  `Tree_Goddess_test.anm` + `Forest.cam`, pause 4 s, alpha from 0 to 252 between 4.1 and 9 s and back to 0 between
  22.1 and 23.5 s, lives 25 s): at 3.5 s `AddSubCollectionsToAtom` adds the sparkles, the vortex (`Vortex.L3D`, 6)
  and the sparkle root (SOUND_SPELL_FOREST_4); at 5.5 s the butterflies (SOUND_SPELL_FOREST_3; `UR_ForestPath` +
  `UR_Flocking` + `ParticleGoodEvilCreator` → `ParticleAnimCreator` S_Butterfly_Flap.anm, or bats M_Bat_Flap.anm);
  and at 3.5 s the light map `Forest_LMap.raw` with permanent sparkles. What is seen is the seed, the blotches, the
  sparkles, the vortex, the butterflies and the light map, and the camera take below.
- **There is no goddess on screen.** The camera atom's creator has `MeshFileName NULL_STRING` and no `MeshEnum` (the
  ctor 0x6A9180 sets MeshEnum −1 at 0x6A93A0, and fn_006A95E0 then loads the null name), so its object has no mesh:
  `Tree_Goddess_test.anm` plays on nothing. The "goddess" is the invisible atom that carries the camera.
  `Tree_Goddess_Boned.L3D`, `Tree_Goddess.cam` (the same size as Forest.cam, other bytes), `TEACHER.l3d` and
  `Teacher_Cam.cam` in `Data\Spells\Anims` are named by no spell file, and runblack.exe has no "goddess" string: they
  are leftovers of the data.
- **The camera take** (`ParticleAnimWithCameraCreator`; detail in
  [the particle meshes](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome) and
  [placed paths](../camera/script-camera.md#placed-paths-cameramodepath)). At the camera atom's first step the
  caster's camera (the local player's: an effect with no spell counts too) is given a `CameraModePath` placed at the
  atom (Forest.cam's points × 0.140486 around the forest), unless a script holds the camera, which refuses it for
  good. The camera glides onto the path's start over 4.3 s (the pause and 0.3 s), then from about 4.1 s of age the
  path plays with the atom's animation, the camera following it 0.3 s behind, for the clip's 6633 ms slowed by 0.6
  (about 11 s). At the animation's frame 999 (about 15 s) the camera is let go where it is; a movement key that would
  move it, or a land grip, gives it back to the player before that. The near plane is 0.1 meanwhile and the hand
  stays shown. No random number is drawn.
- **The butterflies and the bats flap:** `ParticleAnimCreator_Butterfly` / `ParticleAnimCreator_Bats` (SF_Forest.txt
  lines 726 and 756) bring `AnimFileName` `.\Data\SPELLS\Anims\S_Butterfly_Flap.anm` / `M_Bat_Flap.anm` with
  `PlayAnim 1`, `LoopAnim 1`, `RandomiseInitFrame 1`, `SpeedUpFactor 1`, `InitialScale 4` / 1 and the meshes
  `S_Butterfly.l3d` / `MSH_A_BAT_1`. Each atom plays the clip (one cycle every 366 ms the butterfly and every 800 ms
  the bat) from a random frame, with its bones (`Particle3DAnim::DrawAt` 0x67A8E0, `GetCycleTimeFromFrame`
  0x6C85F0). Detail and what remains in
  [the particle meshes](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome).
#### More addresses from the code

- The spiral's turns per tree, 17/13, is the float 0x3FA76276; the −5 of `AdjustSpellSeedPos` without a forest is the
  float 0xC0A00000.
- `MagicTree.cpp` spans 0x5FCF00..0x5FD1C0.

## openblack

Status: **done** for the spell, the trees, the seed and the scene (the butterflies and the bats are there).
Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **The spell**: `Magic/Spells/SpellForest`, with `spell_forest::AdjustSpellSeedPos`. `SpellForest.cpp` `Process`
  returns `psys != 0 ? 1 : 5`; `Spell.cpp` `base::ToBeDeleted` deletes the seed with the spell.
  `SpellForest::ToBeDeleted` deletes the forest with `DeleteForest`.
- **The cast test**: `NoAbodeCovers` walks `ecs::map_cells::FindType(cell, ABODE, previous)` and does not look at
  fields. `IsFixedCell` is `ecs::map_cells::IsFixed`: the real head of the ordered
  fixed list ([object lists per cell](../engine/object-size-and-map-cells.md#object-lists-per-cell)); openblack tests
  the list of MultiMapFixed components (`ecs::fire::traits::IsMultiCellStatic`, the common
  trait). Each tree of the event enters at the head when created (`map_cells::InsertMapObject` in `CreateTree`).
- **A creature's forest**: a creature casts a forest at a place (the place of a villager), and its try asks this same
  check alone, as the original's `CastSpellAtPos` asks the class's rule (`cast_rules::CanCreatureCastAtPlace`;
  [the spell core](spell-core.md#cast-rules), [the creature's cast at a place](../creature/casting.md#casting-miracles-what-openblack-does)).
- **The trees and the forest**: `ECS/Trees`, `components::Tree`; `TreeArchetype::Create(forestId, pos, info,
  nonScenic, angle, maxSize, size)` does what the Tree ctor does, and `MagicTree` calls it with its forest's id,
  maxSize 1 and size 0. The forest is an id of `ECS/Trees` (`CreateForest`, `IsInForest`, `GrowAllTrees`,
  `ShrinkAllTrees`, `TallestTreeHeight`); the trees store the id in `forestId` and there are no lists. The miracle
  always makes a new forest (`CreateForest(0, the first tree's point)`); openblack keeps `CreateForest(0, …)` because
  the Forest's player is not needed. The spell's `SpellForestData::forestId` names an `ECS/Trees` forest, and
  `ForestSystemInterface` has no `ProcessTurn` and no forest entity of its own. The natural trees of `ECS/Trees`
  also grow in the miracle's forests, as the original's do.
- **Deleting**: `ShrinkAllTrees` deletes with `DeleteTree` (Tree::ToBeDeleted 0x74A210) and notifies the listeners;
  the miracles one (`MagicTree.cpp`, `AddTreeDeletedListener`) removes their reactions (0x6E4750), releases the hand
  and notes the forest of a deleted MagicTree. `SpellForest::Process` then deletes that forest if it has run out of
  trees (`DeleteForest` = Forest::ToBeDeleted 0x539C60; the last part of MagicTree::ToBeDeleted 0x5FD070) and closes
  on the next turn, like the original. The listener receives how the tree goes away: `Removed` (the entity is
  destroyed: its fire also goes, FireEffect::ToBeDeleted 0x72EBE0, and the hand releases it) or `BecameDeadTree`
  (`FellTree` or the hand's dead tree). The wood multiplier goes in `Tree::woodValueMultiplier` and the DeadTree
  copies it; `HandSystem::DepositInStore` already multiplies the wood value by it.
- **The fire guard**: openblack only calls `EndOnFire` from the fire system (`ECS/Fire/FireEffect.cpp`, when a fire
  goes out), never while clearing a land: the condition always holds and there is no guard to port.
- **Growth**: `ecs::TreeGrowthAmount` (`ECS/Trees.h`), the rain with `weather::GetMaxRainingOrSnowingAt` and the
  alignment with `effects::alignment::LandAlignmentAt` (in `ECS/Effects/Alignment`). It also reaches the miracle's
  trees, as in the original (their forest is one more in the list).
- **The seed**: `magic::seed::DrawSpells` (`Magic/Core/SpellSeed.cpp`, from `magic::Update`), `FollowsSpell`,
  `ValidForPlaceInHand`; the hand: `HandSystem::FindObjectUnderHand` (`HandPlacement.cpp`) and `SendSeedOrStonePickup`
  (`HandApplyToObject.cpp`); the orb's seed tied to a temple icon: `one_off::CreateSpellIntoHand`. On the ground seed
  `DrawFromSpell` sets the identity rotation and scale 1. Only a seed
  with an icon is drawn and picked up: that of a one-off orb or `OPENBLACK_TEST_SEED` (without icon) is neither drawn
  nor picked up, like the original. `OPENBLACK_TEST_SPELL` (script) does not create a seed. The shields' part of
  `Spell::DrawSpells` is `map_shield::DrawShields`.
- **`Seed.L3D`'s levels**: openblack drew nothing of it until its loader made every level of a sub-mesh saved with
  flags 0 (`l3d::LoadedFlags`, applied in `L3DSubMesh::Load`, 2026-10-15): the renderer leaves out a sub-mesh
  without the first level. The same fixed `Vortex.L3D` and `S_food_chicken.l3d`.
- **The scene**: the camera take in `Particles/Rules/Forest.cpp`; `UR_ForestPath` and `ParticleGoodEvilCreator` in
  `Particles/Rules/Forest.cpp`, `UR_Flocking` in `Particles/Rules/Flock.cpp` and `ParticleAnimCreator` in
  `Particles/Creators/Mesh.cpp`.
- **Approximate**: `IsMoving` vt 0x174 of a fixed object is taken as "is in physics" (openblack does not store the
  previous turn's position); the cell lists are those of `ecs::map_cells`. What the other owners without a hook
  create or move (a tree replanted with the hand) enters the cell lists in the next turn's `Sync`, in creation order.
- **Inferred**: the atom's spiral while falling could not be seen in a still screenshot; it is taken to be the same
  as the original from the code of `RotateAxis` and the off-centre `Seed.L3D`.
#### Where the code is

- `ForestInfoOf` (`SpellForest::GetMagicInfo`); `OnTreeDeleted` in `Magic/Objects/MagicTree.cpp`, the rest of
  `Tree::ToBeDeleted` for a tree `ECS/Trees` takes away.

### Test hooks

- `OPENBLACK_TEST_SPELL=NATURE,x,z` (or `13`), `OPENBLACK_TEST_MAGIC_TURN=<n>` and `OPENBLACK_TEST_FOREST_SHOT`
  ([debug environment variables](../project/debugging.md#debug-environment-variables)).
- `OPENBLACK_SPELL_TRACE=1`: each turn writes `SpellForest trees <n> wanted <N>: grow|decay <sum>` and the event
  `SpellForest event 3 ...: N trees wanted, M made ..., material m`.
- `OPENBLACK_TREE_TRACE=1`: each growth turn writes `growth <a> = <growthAmount> x rain <r> (mult <m>) x alignment
  <al>`.
- Tests: `test_spell_forest` (the spiral with the exact constants, the quantisation to MapCoords, the target scale,
  N, GetMaxObjectsToCreate, the cost and AdjustSpellSeedPos; `seedFollowsSpell`; `treeGrowthAmount`; with
  `OPENBLACK_GAME_PATH`, the NATURE row: 18, 0.1, 0.01, 0.05, 0.25, 10000 initial chants, 120/-1 s, and the
  `magicTreeTypes`); `test_map_cells` (the field's type).

## Pending

- **Not ported in `Spell::DrawSpells`**: fn_00682950 (the invisible collision of the fireballs) and the vt 0x108(1) of
  the object +0xB0 in `Spell::Draw`. `GPlayer::DrawSpellIcons` 0x64AF20 (the sites' overload pulse and the mana path's
  alpha) is `site::UpdateStrainVisual` every frame: [the mana path motes](spell-core.md#the-mana-path-motes).
- **The Forest's player**: no reader other than the store was found 🔍 inferred; verify with `xref.py readers` on the
  GameThing player field of a Forest.
- **The atom's spiral** (inferred from the code): confirm with a sequence of shots of the falling seed.
