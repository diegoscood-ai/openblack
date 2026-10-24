# Teleport

## Summary

Each teleport cast leaves an invisible stone with a swirling pool drawn over it. A player's stones work together: a
moving villager (or animal, or the creature) that passes one of them, or is dropped on it with the hand, comes out
at the player's other stone that brings it closest to where it is going. Villagers also use the stones on their way
to the worship site. A useful jump gives chants back to the spell; only a forced backward jump costs. The hand can
pick a pool up again as its seed, which closes the spell and removes the stone.

openblack ports the spell, the stone, who uses it, the cost, the hand's use of the stones, the villagers' route to
the worship site and the pool's mesh faithfully. The pool is drawn without lighting, and the creature has no path
through the stones yet. The pool's mesh is one of the [surfaces of revolution](../physics/particle-rules.md#surfaces-of-revolution).

## The original

### Teleport

TELEPORT is MAGIC_TYPE 12; its class is `SpellTeleport` (a `SpellWithObjects`, 0xF4). Each cast leaves an **invisible
teleport stone** (`MagicTeleport`, a MobileStatic → MultiMapFixed) with a particle pool (ParticleType 73
`SF_TeleportVortex`). A player's stones form a list (GPlayer +0xA58 head, +0xA5C count, newest first). A living thing
that moves and enters or is dropped on a stone comes out through the stone of the same player that brings it closest
to where it is going.

#### The spell and the stone

- `SpellTeleport::InitWithPos` 0x5FBEB0: `MagicTeleport::Create(pos, this)`, the stone enters the spell's object
  list (+0xEC/+0xF0) and then `Spell::InitWithPos`. The spell **has no particle type** (the effect belongs to the
  stone), so InitWithPos sends `SpellEvent 11` (`particleType == 0`) and creates no PSys of its own.
- `GMagicTeleportInfo` vt+0x30 (CanCast at pos) 0x5FBE50: fails if there is a MultiMapFixed (building, field,
  another stone...) less than `fn_005FCCA0` = **6 m** away (`fn_00604C30` with the predicate 0x5FBE90,
  `Object::AsMultiMapFixed`, vt +0x678, and no excluded object); otherwise, `GMagicInfo::CanCast` = 1. fn_00604C30
  walks a spiral of max(3, ceil(2r/10))² cells, with `FindType(−1)` in each, `d < r` and cutoff `1.5·best + 10`.
  Since `SPELL_AT_POS` checks nothing (neutral creator, flag at 0), the script plants the stone anyway.
- `MagicTeleport::Create` 0x5FC1F0: `new MagicTeleport(pos, spell)` (0xA4 bytes, vtable 0x92C110; ctor 0x5FC130: `MobileStatic(pos,
  0xD3B614,...)`), `CallVirtualFunctionsForCreation` 0x5FC260 (creates PSys 73 at the world pos and `SetPlayer`),
  and `SetScale(GetScale()·0.01)`. The stone **draws no mesh**; `Draw` 0x5FCCC0 only sends an invisible hand
  collision of radius 3 while the spell has a seed (+0xAC; see "The hand and the stones"), and advances and draws
  the PSys with the frame time. The stone enters its map cells as a MultiMapFixed (0x52E890+0x184).
- The stone is `operator new(0xA4)`; its `GMobileStaticInfo` 0xD3B614 comes from `fn_005FC420`.
  `MagicTeleport::GetPlayer` 0x5FC430.
- Cost and timers (effect 12): costToCreate 5000, initialChants 2000, costPerGameTurn 1, costPerEvent 1,
  `divideCostsByTribalPower = 1`, `costPerKilometer = 200` (GMagicTeleportInfo +0x58). Player timer **-1** (the
  stones persist), creature/CP 25 s, one-off 120 s.
- **How long a pool lives (it is not a bug):** having no timer, it is closed by the chants. A dispenser orb seed
  gives it `initialChants` 2000 and the spell spends 1 per turn, i.e. about **200 s** (trace: 2000 → 1814
  in 186 turns), unless a player keeps paying (`CostToMaintain`). Each useful jump **gives back** chants (`PayFor` with
  negative cost), so a pool that is used lasts longer.
- `MagicTeleport::ToBeDeleted` 0x5FC310: removes reactions, deletes the PSys, unlinks itself from the player's list
  and frees the list of travellers. `SpellWithObjects::CloseDown` 0x721300 sets each object to die (`SetDying`
  0x4027A0 = `ToBeDeleted(0)`), so closing the spell deletes the stone.

#### Who uses a stone

- `MagicTeleport::ShouldLivingThingReact` 0x5FC590: the living thing moves (IsMoving vt 0x174) and there is another
  stone T of the same player with `1.2 · (|living−this| + |T−dest|) < |living−dest|` (0x8C6C98 = 1.2), with `dest =
  GetFinalDestPos` (vt 0x884) and `FastDistance` 0x74CE10 (fixed point, 6553.6 per metre: `max + min/2`).
  - `Object::IsMoving` 0x402710: `Pos.x != coords.x || Pos.z != coords.z` (`GameThingWithPos::Pos` +0x14 / +0x18
    against `Object::coords` +0x2C / +0x30), each a 32-bit integer compare of the 16.16 MapCoords
    (0x402710..0x40271E); 0 at 0x402720, 1 at 0x402723. The altitude is not compared. Vtable +0x174 is this function
    for `Object`, `MobileObject`, `MobileWallHug`, `Living`, `Villager` and `Animal` (vtables 0x930AA0, 0x8EED1C,
    0x8CDED8, 0x8CD394, 0x8F7960, 0x8AB7CC); `Creature` has its own (0x479E40, its LH3DCreature's),
    `GameThingWithPos` 0x4052B0. `coords` is the start of the turn: `Living::ProcessLiving` 0x5EC810 copies the
    whole MapCoords (x, z, altitude) of each living into it (0x5EC8AE..0x5EC8C1) **before** `ProcessReaction`
    0x5F1270 (0x5EC8C6) and its ProcessState (vt 0x620, 0x5EC8CF); `GScript::SetPosition` 0x6F8A00 (SET_POSITION)
    copies it again after the move (0x6F8C40..0x6F8C53, after vt 0x55C `MoveMapObject` at 0x6F8B3B), so a thing a
    script has just put down is not moving. So it is **whether it moved in its last turn**, in any state.
    `ShouldLivingThingReact` calls it at 0x5FC5C0 and returns 0 when it is 0. `Living::MoveByTeleport` 0x5EC340 is
    the two sounds and `MoveMapObject` only.
  - `Villager::GetFinalDestPos` 0x756AD0 → `Living::GetFinalDestPos` 0x5EC1E0: if it has a footpath and node
    (+0xC8/+0xCC), the last non-hidden node of the footpath (`GFootpath::GetEndNonHiddenNode` 0x535120 with the flag
    (+0xB4 >> 3) & 1); otherwise, `MobileWallHug::GetDestPos` (vt 0x860, 0x416F70) = **the goal +0x80, whether it
    moves or not**. The +0x80 of a villager that never walked is (0, 0) after `SetToZero`.
- `Villager::ReactToTeleportPriority` 0x766200 = `(ShouldLivingThingReact ? 0xFF : 0) & the priority of reaction
  20` (the REACT_TO_TELEPORT row of `ReactionInfo`). `SetupReactToTeleport` 0x766250 registers the destination in the
  stone (`GetFinalDestPos` vt 0x884 at 0x766297 and `fn_005FC6A0` at 0x7662A5), sets +0xBC = the stone and enters
  `GO_TOWARDS_TELEPORT_REACTION` (201) or its fast twin 251 (0x766380 is a `jmp` to the 201 one, so only the state
  table row changes: animation and speed index). **The condition (0x7662B2..0x7662CE):** `0xC9 + 0x32 · (speed >
  threshold)`, with the villager's own speed (`MobileWallHug` +0x5A, the u16 that `GetSpeedInMetres` 0x60C070
  converts to metres) and the threshold `GMobileWallHugInfo` +0x10C, which is `speedGroup.speed2` of info.dat (the
  same field that `Living::FleeFromPredatorPriority` reads at 0x5F15ED); `setle` on the u16, so 251 only if it is
  **strictly** faster. `GoToTeleportReaction` 0x7662F0: on arrival (`AreWeThere`) it moves to `TELEPORT_REACTION`
  (202), otherwise `SetupMoveToWithHug(stone)`. `TeleportReaction` 0x7663F0 calls `DoTeleport(living, false)` and
  `StopReactingAndSetState`.
- The reaction is spread when the stone is created (`Reaction::CreateReaction` with flag 0), and **again whenever the
  turn's walk over the reaction list comes to it**: `ProcessReactions` offers one reaction a turn, newest to oldest,
  so with N reactions the stone's is offered again every N turns (its gate 0xD00DD4 is set to 1 at start-up; see
  [the animals' reactions](../beings/animal-life.md#reactions); before 2026-10-17 this page said it was never set).
  The earlier observation (the villagers wandering around the town react and jump) was read as "only those already
  moving when the stone is made"; with the re-spread, villagers who walk into its reach later are offered it too.
  🧪 to re-check in game with the walk ported.
- Dropping with the hand: `fn_005FC4B0` requires that the villager's player be the stone's and `teleportCount != 1`.
  `fn_005FC4F0`, in its exact order: `SetTopState(FLYING 10)` 0x5FC4FD, `fn_005DA0C0` (the interface leaves it at
  the stone's MapCoords) 0x5FC517, `SetTopState(LANDED 11)` 0x5FC51E, `DecideWhatToDo` (vt 0x8C8) 0x5FC52C,
  `GetFinalDestPos` (vt 0x884) 0x5FC549 → `fn_005FC6A0`, and `DoTeleport(forced)`; returns 1 if it jumped,
  otherwise 0x17.

#### The hand and the stones

**Faithful.**

- **The hand sees the stone.** `MagicTeleport::Draw` 0x5FCCC0: while the spell (+0x9C) has a seed (+0xAC,
  0x5FCD03), `SendInvisibleDrawCollision(stone, (x, GetAltitude + y, z), 3.0)` (0x5FCD18). 0x519960 projects the
  centre (`ProjectPoint` 0x819390; nothing behind the near plane 0xE839E0), measures the on-screen radius of a point
  at 3 m and, if the mouse falls inside the circle, `SendObjectDrawCollision(stone, depth)`. A script stone has no
  seed and the hand does not see it.
- **Pressing with a villager in the hand over the stone = applying it.** `ActionPressedHolding` 0x5D1560: target =
  `GetCreatureToGiveTo` (no creature) or the object under the hand; outside the influence (`[this+0x48]` == 0) and
  `InterfaceMustBeInInfluenceForInteraction` (vt 0x714, 0x4028A0 = 1) it goes to the no-target branch
  (0x5D15D6..0x5D15E9); then `ValidToApplyThisToObject` (vt 0x71C, 0x5D1607). For a villager
  `ApplyOnlyAfterRecSystem` and `ValidForLockedApplyProcess` are 0, so `SendApplyToObject` 0x5D30D0 (0x5D1684):
  checks that the hand is holding something (status +0x90 = the hand's counter, **not** the influence as an earlier
  verification said), repeats the validity (0x5D3126), packet 0x11 (0x5D32A7) and state 0x12. The packet (0x5DA1A0)
  repeats the checks and calls vt 0x720 (0x5DA251): `Villager::ApplyThisToObject` 0x752C40 → stone (0x752FC2) →
  `ValidToApplyVillagerDirectlyToTeleport` 0x752FEA → `fn_005FC4F0` 0x752FF8, which takes the villager out of the
  hand with fn_005DA0C0 (`RemoveFirstFromHand` fn_005CED60 and the stone's MapCoords) and jumps.
  - `HandleApplyResult` fn_005DA100: 5 → nothing; 3 → consumed; if it is not 1 and the object is no longer in the
    hand, it is returned as is (the villager's case, 1 or 0x17); 0x16 → `RemoveFirstFromHand`; 0x17 → fn_005DA0C0 at
    the target's position; 0x18 → fn_005DC1E0.
  - `fn_006E47C0(obj, v)` sets +0x30 = v (the "available" flag) on every reaction the object started (+0x14, list
    g_game +0x205BDC, next +0x40); `HandleApplyResult` calls it with 1 (0x5DA136). `fn_006E47F0` (from the
    put-in-hand `fn_005DC330`, 0x5DC38C) sets +0x30 = 0 on the object's reactions whose info asks
    `whetherReactionFinishesIfInitiatorInHand`.
  - The totem branch of `Villager::ApplyThisToObject` (`WorshipTotem`, the sacrifice at 0x752C40..0x752FB0) and the
    `HelpProfile` 6/7 help are part of the same function.
- **Picking up the pool.** `MagicTeleport::ValidForPlaceInHand` 0x5FC440 = the vt 0x6FC of the spell's seed (0
  without a seed); `InterfaceSetInMagicHand` 0x5FC470 = `GInterfaceStatus::PlaceObjectInMagicHand(seed)` 0x5DC870
  and returns 0. The seed goes back to the hand (`StoreChantsAndAgeFromSpell`, `ClearSpellLink`: the spell closes and
  `SpellWithObjects::CloseDown` 0x721300 deletes the stone), with the 225 ms grab and the influence of
  `GenericPickup` 0x5D2800. Like every object that is not a tree or a forest, it plays `SoundTag::Create(MapCoords
  +0x14, 10 G_PickUpObject, mode 3, 3D, InGame)` 0x71EB60 (0x5D2881..0x5D28B7).
- **What is needed to use it:** two stones of the player (`teleportCount != 1`) and villagers of one of their towns
  (`fn_005FC4B0`), inside their influence. With a single stone it cannot be applied.

#### `DoTeleport` and the cost

`MagicTeleport::DoTeleport` 0x5FC790. `costPerKilometer` is read through `fn_005FBF20` (the GMagicTeleportInfo).
Among the player's other stones it picks the one with the largest saving `s = dist(dest, living) − dist(dest, T)`
(metres, `GetDistance` 0x74CD50); threshold 0 (or −1e6 if forced). If there is a spell: `SpellEvent{2, the stone's
position}` and **`PayFor(−s·costPerKilometer·0.001, true)`** (`fn_005FBF10`). `PayFor` 0x720990 → `fn_00720FC0` does
`chants −= cost` without clipping, divided by `max(tribalPower,1)` with `divideCostsByTribalPower`. **Literal
reading: a useful jump (s > 0) gives chants to the spell; only a forced backward jump (s < 0) costs.** Checked: a
forced jump of −30 m charges 6 (=30·200·0.001); the natural jumps of +50..60 m have a positive saving. Then
`CreateSpotVisual(SPOT_VISUAL 14 VILLAGER_TELEPORT = SF_TeleportVillager, 1.0)` at both ends
(`GParticleContainer::CreateSpotVisual` 0x63E540(pos, 14, 1.0, NULL); the plain `CreateSpotVisual` passes its float
to 0x63E4B0 together with the entry's own duration, entry +0x44, so the 1.0 is not a duration) and
`Living::MoveByTeleport` 0x5EC340: sound `G_SpellTeleportEnergiseGo` (InGame 39) where it was, `..Arrive` (InGame
38) where it arrives, and `MoveMapObject`. Both are `SoundTag::Create(MapCoords, sample, track 0, mode 2, loops 0,
+0x40 0, 3D 1, AUDIO_SFX_BANK_TYPE **1 = InGame**, delay 0)` 0x71EB60 (0x5EC342..0x5EC372; the bank is 1, not 2 as an
old comment said).

- `GPlayer::Process` 0x6496BC → `fn_005FCC70`/`fn_005FCBA0`: each turn, for each available stone, those that no
  longer exist or no longer follow the stone's reaction drop off the list of travellers: `fn_005FCBA0` drops
  travellers that are not available (vt 0x2C) or whose reaction (`Living::GetReaction` 0x5ECA60) is no longer the
  stone's.

#### The villagers go to the worship site through the stones

**Faithful.** `Villager::CheckWorshipActivity` 0x76BAE0 → `Villager::CanIGetToTheWorshipSite` 0x76BC20 →
`GPlayer::fn_0064D6B0`, all re-read:

- `CanIGetToTheWorshipSite(MagicTeleport*& out)`: without a town (vt 0x48) or without a worship site (vt 0x30C) it
  returns 1; if `GetDistanceInMetres` 0x74CD70 (flat: `GetDistance` 0x74CCB0 only uses the first two dwords of the
  MapCoords) from the villager to the site is `<=` `maxDistanceThatVillagersWillGoToWorship` (the town's info +0x148,
  500 m in Land 1) it returns 1 without a stone (0x76BC5E..0x76BC69); if it is greater, it asks for the player (vt
  0x1C; without a player also 1) and calls `fn_0064D6B0(the villager's position, the site's position, maxDist)`: 0 →
  it cannot go (0x76BCA3), otherwise `out` = the stone and 1.
- `fn_0064D6B0` walks the player's stone list (+0xA58, the newest first) and stores **separately** the minimum
  distance to the origin (d1, with the stone that gives it) and to the destination (d2), both starting at `maxDist`
  (0x64D6C5/0x64D6C9); at the end it returns that stone if `d1 + d2 < maxDist` (0x64D720..0x64D731, strictly). A
  single stone can serve both ends (the two minima are kept apart, 0x64D6D5..0x64D71C); the stone it gives back is
  always the one nearest `from` (0x64D6EF). The route through the stones is taken only when it is strictly shorter
  (0x64D72E): 30 + 70 against 100 is not enough.
- With a stone, `CheckWorshipActivity` (0x76BB99..0x76BC07) does: `SetReactionDoneWhen(REACT_TO_TELEPORT)` 0x6E44A0
  (the reaction's record is stamped with the turn; if it does not have one, it **adds** it at the end and, with 3
  already, removes the oldest, 0x6E4507..0x6E4540; without the 1800-turn forgetting of `fn_006E4340`), the villager
  leaves the list of reactors of the reaction it was following (0x76BBAF..0x76BBF2) and `StartReacting(
  REACT_TO_TELEPORT, stone, the stone's reaction +0x94)` (vt 0x994 = `Living::StartReacting` 0x6E4590, which
  dispatches by type to `Villager::SetupReactToTeleport`). That is: first `GotoWorshipSiteForWorship` sets state 59
  and the goal (the site's arrival point), and on top of it 201/251 towards the stone is pushed; on jumping,
  `StopReactingAndSetState` → `PopFromPrevious` returns state 58 (`GOTO_WORSHIP_SITE_FOR_WORSHIP`, the resume state
  of 59 in info.dat), whose **own state function is 0x76BCC0 = `GotoWorshipSiteForWorship`**, which sets 59 again and
  walks: the villager continues towards the site from the stone it came out of.
- The jump's saving is positive (it goes towards the site), so **the jump gives chants back to the spell**.

#### A creature and the stones

- `MagicTeleport::DoTeleport` 0x5FC790 calls vt +0xAF0 (0x5FC949) after the two spot visuals (0x5FC90D, 0x5FC923).
  vt +0xAF0 is `Living::MoveByTeleport` 0x5EC340 for villagers and animals, and `Creature::MoveByTeleport` 0x479F70
  for the creature (the Creature vtable 0x8CD300 at +0xAF0).
- `Creature::MoveByTeleport` 0x479F70 does not move the creature itself; it goes through its mind:
  `ForceActivityAndForceAction` (0x479FA7) with a `CreaturePlan` of desire 0x18 `CREATURE_DESIRE_TO_OBEY_PLAYER` and
  action 1 `CREATURE_MOVE_TO_POS` on its belief about itself; mind +0x1C14 = 1 (0x479FB2) and fn_004FF5C0 on the
  sub-action agenda (0x479FC8); then three sub-actions: 0x5D with a `SubArgumentPoint` at the target (0x47A07F), 8
  with a `SubArgumentPointAndFloat` of the destination kept at +0x11CC and **5.0** (0x47A0FA), with `LookAtPosition`
  0x4D1460 as its callback (0x47A13D), and 0x7B with no argument (0x47A180); last, mind +0x1C10 = 1 (0x47A18F).
- `Creature::SetupReactToTeleport` 0x4F3A60 keeps `GetDestPos` (vt +0x860) at +0x11CC (0x4F3A91..0x4F3AAC),
  registers it with the stone (fn_005FC6A0, 0x4F3ABB, as the villager's setup at 0x7662A5), adds a belief about the
  stone (0x4F3AC8), forces desire 6 `CREATURE_DESIRE_CURIOSITY` with action 0x86 `CREATURE_GO_TO_TELEPORT`
  (0x4F3ADF), sets mind +0x1C14 = 1 and +0x1C10 = 1, then calls vt +0x990 with the reaction.
- `Creature::ReactToTeleportPriority` 0x4F3A10: `ShouldLivingThingReact` (0x4F3A40), giving REACT_TO_TELEPORT's
  priority from the reaction info ([0xD4FE90] = `GInfoArrays::ReactionInfos` +0x7E0, entry 20's priority, info.dat
  data with no code writer; also read by `Villager::ReactToTeleportPriority` 0x766235) when it should react, else 0
  (0x4F3A45..0x4F3A51, `neg al; sbb al, al; and eax, ecx`).

#### SF_TeleportVortex and ZR_SurfRevol

- The file `SF_TeleportVortex` (decompressed from `Data\Spells\ZSpellFiles\SF_TeleportVortex_txt.zzz`, verified):
  `ZR_SurfRevol` FunctionIndex 0 (flat disc), texture `S_TileLandscape.raw` (256×256, with alpha
  `S_TileLandscapeA.raw`), NumU 12 × NumV 5, SpeedU 0 / SpeedV 0.213717, `AlphaFadeIn`/`Out` 0.4, `FadeAlphas` 1,
  `ChangeSpecColor` 1, `MaxUVChange` 1, `MaxVertexChange` 0.35, `HeightAboveLandscape` 0.5, `DoRaiseAboveLandscape`
  1, `RaiseAboveLandscapeRadius` 30, `MaterialUpdateZBuffer` **0**, `MaterialUseTextureAlpha` 1, `UseAdditiveAlpha`
  0, `MaterialSetDoubleSided` **0** and `UseLighting` **1**; scale 5 through `UR_ChangeScale` on the parent atom
  (`InitialScale` 2); sound loop `SOUND_SPELL_TELEPORT_POOL` in the `SoundOfCreate` of group 0's `CreateRuleAnAtom`
  (LOOPING 1, SOFTRELEASE 1).
- The culling: fn_0081C780 sets CULLMODE ((~mat+5)&1)·2+1 (0x81CC5E..0x81CC6E). The discs do **not** go through the
  plain world triangles: with as many speculars (+0x30, count +0x38) as colours DrawAt takes fn_0081C780 (0x67CAEE),
  Draw3DWorldTriangle with per-vertex specular (0x81C9B9..0x81C9C4), and ZR_SurfRevol sizes both to NumU×NumV
  (0x685A0E..0x685A3D). `UseLighting` (+0x88) requires the mesh's normals (`fn_006C9340`) and a D3D light.
- `ZR_SurfRevol::ModifyAtomCollection` 0x686370 (props 0x6B2E80): creates a `RenderParticleGJMeshRotatingUV` (ctor
  0x6C8B60) with a surface-of-revolution mesh built in `fn_006858F0`. The profile comes from FunctionIndex (table
  0x6868CC): 0 `TestDisk` (r=t, y=0), 1 `TestFunnel` (r=t, y=3(√t−1)), 2 `TestFunnelSpout` (r=1.5t, y=3(√2t−1)), 3
  `TestFunnelParab` (r=t, y=3(t²−1)); any other → disc. Vertex (i,j): `u=i/(NumU−1)`, `t=j/(NumV−1)`,
  `(r(t)cos2πu, y(t), r(t)sin2πu)`, uv `(u,t)`. With `FadeAlphas`, the row with `t<AlphaFadeIn` goes in RGB
  `255·t/fadeIn` (alpha 255) and with `t>1−AlphaFadeOut` in alpha that goes down to 0 (RGB 255); with
  `ChangeSpecColor` the specular is the player's colour × `(1 − the fade factor)` (`GetPlayer3DColor` 0x64B590 →
  table 0xBFF0B8 with `GetRemapedPlayer`; the neutral is 0xFF000000). `fn_00685F00` scales the UVs by
  `TextureWidth/256, TextureHeight/256`; `fn_00685F40` twists the UVs `u += (1−t)²·MaxUVChange`; `fn_00685FC0` rotates
  each row `t·MaxVertexChange` about Y; with `DoRaiseAboveLandscape` (`fn_00686980`) the mesh is cut along the
  terrain's cells and diagonals (`fn_00686D90`) and each vertex rises to the terrain below it minus that of the
  centre, once when it is created ([meshes stuck to the ground](../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph)).
  Without `DoRaiseAboveLandscape` the twists breathe with sin(fmod(collection age × +0x68, 2π)); +0x68 has no
  property (1.0 from the ctor 0x686200). `RenderParticleGJMeshRotatingUV::GameUpdate` 0x6C8BC0 shifts the UVs
  (SpeedU/V) within the tile; `DrawAt` 0x67CBA0 draws in mode 6 (colour = texture×diffuse + specular, alpha =
  texture×diffuse).
- **Size (faithful, verified):** the mesh has radius 1 and only goes through the atom's drawn matrix
  (`RenderParticleGJMesh::DrawAt` 0x67C150 multiplies each vertex by `DrawData+4`, the PSR of atom+0xD0). That PSR
  already carries the hierarchy: `PostUpdateAtoms` fn_00673EA0 does `cur = the parent's PSR × local` (fn_00673DB0,
  fn_007FAE60) and `scale = +0x74 × +0x78 × the parent's`. The disc atom is born with +0x74 = 1 (ctor `AtomCore`
  0x673830) × `Scale` 1 (0x68649E) and is a child (Hierarchies[2] = 1) of the group 2 atom. Radius = 1 ×
  `InitialScale` of group 2 × `UR_ChangeScale`: **6 m in `SF_SpellDispenserVortex`** (InitialScale 6, without
  UR_ChangeScale; magnitude 1.0 in `SpellDispenser::CallVirtualFunctionsForCreation` 0x722840) and **10 m in
  `SF_TeleportVortex`** (2 × 5). `fn_00686980` (DoRaiseAboveLandscape) takes the mesh to world space with that same
  matrix (fn_00673E40) and returns it with its inverse (0x686D6C), so it does not change the size.
- `SF_TeleportInHand` and `SF_TeleportOnHolder` (sprites) belong to the hand effects. The villager flash SV 14
  `SF_TeleportVillager` is the spot visual above.
#### More addresses from the code

- `MagicTeleport.cpp` spans 0x5FBF50..0x5FCE00. The forced jump's threshold −1e6 is the float 0xC9742400.

## openblack

Status: **done** for the spell, the stone, the villagers, the hand, the cost, the worship route and the pool's mesh;
**partial** for the pool's drawing (no lighting); **not ported** for the creature. Port decisions:
[port decisions: magic](../project/port-decisions-magic.md).

- **The stone**: `src/Magic/Objects/MagicTeleport`; a player's stones are kept by `TeleportStone::serial`, newest
  first. The cast test: `cast_rules::CanCastAt` asks `Locator::teleportSystem` (`CanPlaceStone`), which calls
  `teleport::AnyMultiCellStaticNear` (was `AnyMultiMapFixedNear`), which is
  `ecs::map_cells::FindNearestInSpiral(pos, IsMultiCellStaticClass, 6) != null`; the stones are in their cells (hook
  `InsertMapObject` in `teleport::Create`), so they are no longer looked up in the players' lists. The script still
  plants a stone where the test fails (the hook confirms it: "CanCastAt(A) now false" but the stone is created).
  No effect reaches a stone: `MagicTeleport::IsEffectReceiver` 0x5FCE00 says no, and `effects::IsEffectReceiver` skips
  a `TeleportStone` (since 2026-10-22; before, a blast shattered and removed it; test
  `RepairsTest.NoEffectReachesATeleportStoneASeedATotemOrAnEmptyPot`, [the blast](lightning-explosion.md#openblack)).
- **The final destination** (`villager_teleport::FinalDestination`): on a footpath walk (`components::LivingFootpath`
  with a footpath and a node) the footpath's end non-hidden node in the walk's direction (`footpaths::GetEndNonHiddenNode`
  with `reverse`), else, or with every node hidden, the wall hug's goal (#74 W3b). Test:
  `test_villager_reactions`.
- **The close-down**: in the code the function is called `TeleportCloseDown`: with the name `CloseDown`, inside
  `magic::RegisterTeleportSpell` it resolved to `magic::CloseDown` (the dispatch) and the game hung when the spell
  closed (checked with `OPENBLACK_TEST_SPELL=TELEPORT,…,3`: it closes and is deleted at turn 31).
- **The villagers**: `src/ECS/Systems/Implementations/VillagerTeleport` (`CurrentReaction` is `Living::GetReaction`
  0x5ECA60 as far as the stones care: the teleport reaction a living follows, 0 for none). `villager_teleport::MovedSinceTurnStart` /
  `IsMoving` compare `map_coords::ToFixed` of x and z of the Transform against `DrawPosition::turnStart`, which
  `BeginLivingTurn` writes for each living before its reaction and state (`living_turn::ProcessLiving`) and
  `SnapTurnStart` writes on a script's SET_POSITION (`NotifyTeleported`). Before a living's first turn it has no turn
  start yet and counts as not moving (openblack's own). Before this the test stood in a walk state with a speed for
  the position; that test is kept only for `villager_teleport::OnMoved`'s restart of a walk after a jump, which is
  openblack's own. openblack always uses the WallHug goal for the destination ✅ faithful, so a forced jump on a
  villager who has never walked is computed against (0, 0), like the original.
  `villager_teleport::SetupReactToTeleport` has the 201/251 rule (a villager who was already running to the worship
  site leaves with 251). `villager_teleport::LandAt` goes through FLYING and LANDED (both states exist in openblack's
  table: 10 `VillagerCarried`, 11 `VillagerLanded`). **Approximate**: openblack's `DecideWhatToDo` only changes the
  state, so the goal registered afterwards is the one the villager already had.
- **The hand**: `HandSystem::PickObjectAlongRay` (`HandPlacement.cpp`) tests a 3 m sphere on each stone of
  `teleport::HandCollisionStones()`: the ray passes less than 3 m from the centre in the plane of its depth
  (⚠️ approximate: the distance it competes with is the length of the ray up to that plane, like openblack's meshes,
  not the projected w). A script stone (`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_TELEPORT`) has no seed and the hand
  does not see it. The apply is in `HandApplyToObject.cpp`, `HandSystem::HeldActionPressedOnObject`; picking up the
  pool is `HandSystem::PickUpSeedOrStone` (⚠️ approximate for the seed: openblack only stores the drawn point, not its
  own MapCoords that 0x729020 does not move). Not ported: `fn_006E47C0`, the totem branch (with a totem the press
  still sets up the drop) and the `HelpProfile` 6/7 help.
- **The cost and the sounds**: the jump's sounds go through `audio::tags::CreateAtMapCoords`, not through a direct
  emitter. The travellers' list is kept by `teleport::ProcessPlayers`, through `Locator::teleportSystem`
  `ProcessTurn`, slot 3 of the turn.
- **The worship route**: `teleport::FindRouteStone`, which openblack reaches through `Locator::teleportSystem`
  `RouteStoneFor`. `SetReactionDoneWhen` is ported separately in `VillagerWorship.cpp` (the `RefreshRecord` of
  `StopReacting` only updates and was of no use). openblack does not keep the reactors' list in a `Reaction`, and
  `SetupReactToTeleport` changes its +0x94 / +0xBC anyway. Row 58 had no function in openblack (the villager stood
  still): now it is `villager_worship::GotoWorshipSiteForWorshipState` (`VillagerStateFunctions.h`).
- **The pool**: the `ZR_SurfRevol` rule (`src/Particles/Rules/SurfRevol`) stores the mesh in a `SurfRevolCreator`
  (type `Other`) per atom; the radius is `atom.scale` (which `Effect::PostUpdate` already multiplies by the parent in
  a hierarchy). An error was corrected: the port multiplied again by the parent's scale (36 m radius for the
  dispenser, 100 m for the teleport stone). `src/Graphics/RendererRevolvedSurface.cpp` draws it with the
  `WorldTextured` program in two passes (the blended texture and the specular added with a 1×1 white texture), with
  `render_modes::CullFor(surface.doubleSided, false)`; that bgfx's Ccw is the D3DCULL_CCW of these vertices is
  🔍 inferred (vertices in world space, without mirroring, like the models; the pool is still visible from above).
  The vortex is advanced per frame (`SetPerFrame`). The dispensers' discs share it.
- **The hand effects**: `UR_FollowLocalHand` is ported (`src/Particles/Rules/HandFollow.cpp`, registered in
  `ParticleRegistry.h`: the old note that it was missing was false), although its effect in the hand has not been checked
  in game. The villager flash works (`particleSystem` `StartSpotVisual`).
- raffclar's walk-and-fade model of a creature through the stones is not taken: see
  [port decisions: magic](../project/port-decisions-magic.md#teleport-tornado-and-flocks).
#### Where the code is

- `Magic/Objects/MagicTeleport`: `ProcessTravellers` (fn_005FCBA0 for one stone) and `ReactionOf` (the stone's own
  REACT_TO_TELEPORT, +0x94, 0 when there is none).

### Test hooks

- `test_teleport`: `FastDistance`, the detour rule (1.2), `ChooseTarget`, the sign of the cost (500 useful m give
  back 100 chants, −20 forced m cost 4) with the real `Spell::PayFor`, `FindRouteStone` (two tests: the basic one and
  `RouteStoneBothEndsAreLookedForApart`, the two minima separately and the strict `<`), the four profiles of
  `ZR_SurfRevol` and the vortex mesh (12×5, fades, triangles, UV and vertex twist, player colours).
- `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,player[,mode]]"` (`OPENBLACK_TEST_TELEPORT_TURN=<n>`,
  `OPENBLACK_TELEPORT_TRACE=1`): plants two stones (like `SPELL_AT_POS`) and, in `walk` mode, makes the nearest
  villager walk towards B; in `drop`, drops it forced on A; `none` only the stones;
  - `hand`, the real hand path (`OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1,0,hand"`): only plants B (script); A is cast
    with the hand (`OPENBLACK_TEST_SEED=TELEPORT`, `OPENBLACK_TEST_MAGIC_TURN=200` and a press of
    `OPENBLACK_TEST_CAST` with the mouse at x0,z0); ten turns later it puts in the hand the villager of PLAYER_ONE's
    town nearest to A, and the following presses of `OPENBLACK_TEST_CAST` (real, through `HandSystem::Update`) apply
    it and then pick up the pool. Every 10 turns it writes the stones, their seed and what is in the hand;
  - `worship` (`OPENBLACK_TEST_TELEPORT="x0,z0,0,0,0,worship"` with `OPENBLACK_TEST_WORSHIP_SITE="NORSE"`): puts
    stone A far from the site (x0,z0, or a land point at 1.15 × maxDist that it looks for itself), stone B 20 m from
    the site, moves the PLAYER_ONE villager nearest 80 m beyond A (so it ends up out of reach) and calls
    `CheckWorshipActivity`.

## Pending

- **The creature and the stones**: `Creature::MoveByTeleport` 0x479F70, `SetupReactToTeleport` 0x4F3A60 and
  `ReactToTeleportPriority` 0x4F3A10 are read; openblack has no creature path through the stones. It waits on the
  mind's forced plans (`ForceActivityAndForceAction`) and the sub-action agenda (sub-actions 93 and 123,
  [tying the leash with the hand](../creature/leash.md#tying-the-leash-with-the-hand)).
- **The pool's lighting** (`UseLighting 1`): openblack draws the pool without lighting, so it looks dimmer than in
  the original. It needs the mesh's normals and a light that the `WorldTextured` program does not have; it is work for
  the particle renderer, and doing it halfway would leave the pool invisible from above. The installed
  `S_TileLandscape.raw` is dated 2021 (replaced by a patch), so the tone is not comparable either. The specular
  distribution of fn_0081C780 is also pending; FragMesh's two-sided lighting uses its per-vertex specular
  (`world_triangles::Vertex::specular`, Color1), and the pieces and the SurfRevol discs pass 0.
- **The hand**: `fn_006E47C0` (the reactions' "available" flag), the totem branch and the `HelpProfile` 6/7 help are
  not ported.
- **The hand effects** `SF_TeleportInHand` / `SF_TeleportOnHolder` have not been checked in game.
