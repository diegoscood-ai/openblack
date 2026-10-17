# Influence and alignment

## Summary

A god can only act inside its influence: the area its temple, its towns and its influence rings cover, shown on the
land as a coloured curtain round each temple and town. The hand also carries a waning "virtual" influence a few
metres past the border. Each player has an alignment from −1 (evil) to +1 (good), moved by its acts (uprooting and
planting trees, deaths, the effects of miracles) at a capped rate per turn; the alignment of whoever rules a place
colours the sky and speeds the growth of trees. Reactions are how living things notice events around them, and every
object has a life from 0 to 1.

openblack ports the influence query, the rings, the border and its ripple, the virtual influence's strength, the
alignment and its acts, the reactions' spread and the object life faithfully; the allies, the virtual influence's hum
and sparks, and the alignment history are not ported.

## The original

### Influence

All distances are in x,z (`GetDistanceInMetres` 0x74CD70).

- **Query.** `Influence::CalculatePlayerInfluence(pos, player, 0, type, allies)` 0x5CD170 returns -1 to 1; "in the
  influence" is `> 0`.
  - Without a player it gives 0.
  - With the game flag 0x2000 (the registry value "GatheringFlag", `start_system` 0x6433B1) it gives 1 everywhere.
  - Then it looks at `GPlayer+0x934` (only ever 0) and the virtual influence of each of the player's interface
    statuses (`fn_0076D330`, [virtual influence](#virtual-influence); it is the only one that reads `type`), and
    afterwards `CalculatePlayerRawInfluence`.
  - If that gives ≤ 0 and allies are requested, it returns that of the first ally with influence (`fn_005CD400`,
    `GPlayer::IsAllied` 0x64D5D0, +0x950[ally] > 0.1). If there is no ally, 0.
  - Who calls and with which arguments:
    - cast rules 2 and 3 (fn_005FB5D0): allies = 1;
    - the hand, `m_InInfluence` (GInterface+0x48, fn_005D1120): with the hand's position, type 1 and allies = 1;
    - `GInterfaceStatus::Process` 0x5DC558: drops the locked object (picking up in batches) outside the influence.
- **`CalculatePlayerRawInfluence`** 0x5CD230 (`Influence::IsInPlayerRawInfluence` 0x5CD460 asks it):
  - it sums the citadel, the player's towns (GPlayer+0xA50) and the rings, and clamps it between -1 and 1;
  - an anti ring of the same player covering the point returns 0;
  - a ring attached to an object that is in the hand does not count ("in the hand" is `fn_005CDBD0` plus the
    `obj+0x24 & 4` test);
  - `CameraExclusion::InsideInclusion` is always true in a normal game (it is only used by the camera force field of a
    saved game).
- **Citadel and towns: all or nothing.** They contribute their radius if the point is inside, so the sum goes past 1
  and stays at 1. For the citadel, inside is `r > d` (fn_004630F0); for the town, `d < r` (fn_007479E0).
  - **Citadel.** `Citadel::GetInfluence` 0x464090 = `playerInfluenceMultiplier × Citadel+0x6C`.
    - +0x6C is set once, when the first CitadelHeart is created (0x4649B0): `M2 × (land ? storyInfluence[land-1] :
      influence)` from GCitadelHeartInfo, that is 125, or 750/450/250/450/450 on lands 1 to 5.
    - M2 is 1 with `CREATE_CITADEL` and the blueprint's scale with the planned citadel. In Land1 it is built by the
      challenge script: `BUILD_BUILDING(1915.05, 2508.89, 1.0)` → `ForceBuildingOfPlannedAtPos` →
      `CreatePlannedNoFixedCheck`.
    - Result: **750 m in Land1**, 450 in Land2 and 250 in Land3 (which is why in Land3 you start with so little).
    - The temple of `CREATE_PLANNED_CITADEL` is a town's plan: its heart, its influence and its circle are made when
      Land 1's script builds it (`BUILD_BUILDING`, a few seconds in), and its border shows only once it is finished
      (the latch below).
  - **Town.** `Town::Process` 0x747380 recalculates the radius (+0x5C8) every turn:
    - the base is `Town::GetBaseInfluence` 0x73FD40: GTownInfo's `influence` (25), or its `storyInfluence[land-1]`
      (25/25/25/50/25); the single `GTownInfo` is at 0xDA2780;
    - to that is added, every `processAbodeEvery` (1) turns, the `GetInfluence` of each building in the town, except
      if Town+0x5F8 (last argument of the constructor, 0 in `CREATE_TOWN`; it is not `SET_TOWN_UNINHABITABLE`, which
      writes +0x5F4);
    - the total is multiplied by `townInfluenceMultiplier`, only when the town has a player;
    - only the player's own towns count: a NEUTRAL one only counts for the neutral player.
  - **Building.** `Abode::GetInfluence` 0x4072A0 = `% built × scale × life × GAbodeInfo::influence × (adults +0xB4 +
    children +0xB7 + 1)` (`MultiMapFixed::GetInfluence` 0x52ECA0 times that factor).
    - `influence` values: houses 5, totem and village centre 90, store 45, workshop and dispenser 25, graveyard 30,
      crèche and football pitch 20, wonder 150, field 5.
    - Fields are also Abodes.
  - **Land globals.**
    - The multipliers are 1 at the game's start (the GGame constructor and GGame::Init, once) and a land's load keeps
      the last land's ([the map script reader](../scripts/map-script-reader.md)). They are changed by
      `SET_TOWN_INFLUENCE_MULTIPLIER` (case 96: Land3 0.5, Land4 0.8, Land5 0.6) and `SET_PLAYER_INFLUENCE_MULTIPLIER`
      (case 97).
    - `SET_LAND_NUMBER` writes g_game+0x205A08.
    - Land 6 reads the float that comes after the story array.
- **Rings** (`InfluenceRing`, 0x44 bytes; list g_game+0x205C4C, newest first):
  - Fields: position, followed object (+0x28), player (+0x34), radius (+0x38) and anti (+0x3C).
  - They contribute `Influence::CalculateInfluenceOnRange(d, r)` 0x5CD560, with `GInfluenceInfo` (0xD17CC0) 0.4 / 0.2 /
    0.2:
    - 1 up to 0.4·r;
    - from 0.8 to 0 up to 0.6·r;
    - from **0.2** to 0 up to r. The 0.2 is a double at 0x8C7C68, and the jump from 0 to 0.2 at 0.6·r is kept.
    - The int third argument of `CalculateInfluenceOnRange` is not read.
  - `ProcessRings` 0x5CDB90 (per ring `fn_005CDC00`): the ring follows its object, and if the object disappears it is
    deleted with it.
  - `IsInAntiInfluence` 0x5CD490: the point is inside an anti ring of that player (`d ≤ r`; the anti test
    `IsInInfluence` 0x5CDA60).
  - `InfluenceRing::Create` 0x5CD9D0 → ctor 0x5CD760 (at the head of the list); a ring that follows an object is
    `fn_005CD990` → `fn_005CD800` (it takes the object's position and keeps a weak link); `InfluenceRing::ToBeDeleted`
    0x5CD8A0; a ring's contribution `InfluenceRing::CalculateInfluence` 0x5CD900. The shields' anti rings are made with
    flag 1.
- **Scripts.**
  - `CREATE_INFLUENCE_RING(pos, player, radius, anti)` (case 59).
  - CHL `INFLUENCE_OBJECT` (60, `GScript::InfluenceObject` 0x6F9AA0: pushes the ring, via AddScriptGameThing, or 0) and
    `INFLUENCE_POSITION` (61, 0x6F9B60): on the stack go anti, player (game index, not converted), radius and object
    or position; they return the ring. `GGame::GetPlayer` 0x5509B0 maps the game index 0..7 and returns NULL from 8 on.
  - CHL `GET_INFLUENCE` (62, 0x6F9C60): on the stack go position, `raw` and player (script one: 0 = the local one, n =
    n − 1). Allies = `raw == 0`.
  - LandT's script opens a 1000 m ring at (2185.6, 2409.5).
- The player's influence power +0x8C (`GPlayer::CalculateInfluencePower` 0x64AD00) and `fn_0064B700` (the ratio) are in
  [the audio engine](../audio/engine.md) (heartbeat, `beliefShare`).
- `CalculateMostInfluentialPlayer` and its helpers 0x5CD4F0 / 0x5CD600 / 0x5CD6C0; the per-land colour remap
  `GetRemapedPlayer` 0x64D790.

#### The border

`InfluenceCircle`.

- **The list.** `GGame::Update3DInfluence` 0x555280 (from `GGame::ProcessTurn` 0x54E738) rebuilds it only when the
  dirty byte g_game+0x250174 is set **and** `GameTurn % 10 == 0`: one circle per citadel and per town with influence,
  in the owner's colour (the rings, anti rings and shields are never drawn). The byte is set by fn_00555240 when a
  radius moved by more than 0.01 since the last rebuild (`Citadel::Process` 0x4630C6 against citadel +0x78,
  `Town::Process` 0x74759E against town +0xF24) and by `ForceNeedUpdateInfluence` 0x555270 (a deleted citadel or
  town, a town changing owner). `ForceNeedUpdateInfluence` is called from `Citadel::ToBeDeleted` 0x462C9B, a town
  changing owner (`fn_00649810` 0x6499E2), `Town::ToBeDeleted` 0x739993 and `TownCentre::Process` 0x743E63.
- `Update3DInfluence` starts with `InfluenceCircle::Reset` 0x826C50 and walks the players with `GetNextPlayer`
  0x5508A0, which stops at the neutral one (0x5508C6): the neutral player gets no circle. A citadel circle needs
  `Citadel::GetInfluence` ≠ 0 (0x55530E), a town circle +0x5C8 ≠ 0 (0x555354); the dirty byte is cleared at 0x555384.
  The dirty byte (g_game offset 0x250174) is 0 after `GGame::ClearVariables` 0x54BF22 (`xor eax, eax` at 0x54BF20);
  a citadel's or a town's turn sets it (fn_00555240 0x555257, from `Citadel::Process` 0x4630C6 / `Town::Process`
  0x74759E), as does `ForceNeedUpdateInfluence` 0x555270, and `Update3DInfluence` reads and clears it (0x555286 /
  0x555384); nothing else touches it. So a land's first circles appear only when a radius first moves by more than 0.01 from the 0 of Citadel
  +0x78 / Town +0xF24.
- The circle list [0xEB9A14] is newest first (the ctor links at the head, 0x826608..0x82661B); each circle is
  new(0x38) at 0x826FB6. Fields: +0x18 the hidden flags ((3N + 3) × 4 bytes allocated, N + 1 used), +0x2C the colour
  index & 7 (0x826632), +0x30 dead, +0x34 the alpha cache. `InfluenceCircle::Add` 0x826FA0 = the ctor `fn_008265F0`,
  then every dead circle is deleted (0x826FFA..0x82701F).
- **Overlaps** (`fn_00827040`, at each `Add`, for each older circle, newest first): nothing if either circle is dead or
  their players differ. d is the 3D distance between the centres: d + r < r_older kills the new one, else d + r_older
  < r kills the older one, else each hides its columns inside the other (fn_00827110), the new one first
  (0x8270F9..0x827104). Quirks: a hidden closing column turns white (0x00FFFFFF), so its two segments fade towards
  white; a circle killed later in the same ctor may already have hidden columns of others.
- Point inside a circle: `fn_00827210`, dx² + dz² < r² (strict, 0x82723C).
- **The curtain** (fn_008265F0): 40 units high, three rows at H, H + 20 and H + 40; only the middle row gets an alpha,
  so it is a soft band that peaks 20 above the land. `burn.raw` / `burna.raw`, scrolled +0.0001 u and −0.0002 v per
  game ms (global clock [0xEB9A40]), material [0xEB9A18] (mode 6, two-sided, tiled), colours `g_players_color`
  [0xEA9EFC] (alpha 0; blue and white differ from the generic table 0xBFF0B8). `g_players_color` is written by
  `fn_00826510` (0x826520..0x8265E5); colour indices 5 and 7 are the ones that differ from the generic table (there
  0x4777FF and black).
- `SetCurtainAlpha` (0x826F0D..0x826F57): only when the alpha differs from +0x34 and the player's border is shown; only
  the middle vertex (3j + 1) of each column that is not hidden takes the alpha.
- **When.** `InfluenceCircle::Draw(1)` 0x826C90 runs **every frame in the world view** (`GGame::Process3dEngine`
  0x54E3D2..0x54E3DE): there is no option, no hand-proximity test and no fade timer. `WorldRoom::ShowInfluence`
  [0xC2A478] only gates `Draw(0)`, the map in the temple's world room (0x54E3E0..0x54E412). Nothing is drawn while
  g_camera.y ≤ 100; the middle alpha is 120 from y = 200 up and ftol((y − 100) · 0.01 · 120) below. It is drawn at
  once, after everything else drawn at once and before the Z-sorter drain, so every Z-sorted blended thing is drawn
  over it; it writes no Z.
- **The latch** [0xEB9A1C + 4p]: cleared on every land load (fn_00828A50 from `LH3DIsland::Create` 0x803E85); set by
  the heart's 3D object when it is drawn whole: `LH3DCitadel::SetPercent` 0x883120 (vt +0x200) clamps the percent to
  0..1 (below 0 → 0 at 0x883134, above 1 → 1 at 0x88314F), stores it at +0x9C (0x88319A) and, when it is exactly 1,
  sets the latch of its player +0xA8 (0x8831A7..0x8831AD). `InfluenceCircle::Draw` only reads it (0x826F15). It is
  never cleared on that land, whatever the percent does after. Its callers give it the heart's percent built:
  `InitTemple` at the heart's creation (0x8827C1) and `Citadel::Process` every turn (0x462E33 / 0x462E4F). So a temple
  made built (CREATE_CITADEL) shows its border at once, and a planned one started by `BUILD_BUILDING` (Land 1) only
  once it is finished. Until then the player's circles are drawn with alpha 0 and crossing them makes no ripple and
  no sound: on Land 1, no crossing sounds until the temple is finished.
- **The ripple** (fn_00827250..fn_00827500): crossing a border with the hand (fn_00827820) makes 7 growing rings of
  `smoke.raw` cell 63 in the player's colour, standing in the curtain's plane at the crossing point (bisection
  fn_00827670), for 2 s, Z-sorted; and sound 52.
  - `fn_00827250` (new(0x68)), life 2000 ms at +0x0C (0x827273), colour at +0x34. 7 sprites from
    `LH3DSprite::Create(7, 1)` 0x8404A0 (stride 0x34), smoke material [0xEA1ABC], cell 63, size max(2i, 0.0001),
    angle `Random(0, 2π)` 0x81D180. Matrix RotX(π/2), then the yaw. The ripple list [0xEB9A44] is newest first (linked
    through +0x10).
  - The ripple's yaw is `GetYAngle(centre, point)` 0x841260 + π/2 ([0x8C78D8]). The inside point is the previous hand
    point if it is inside, otherwise the current one, both at y = 0; `fn_00827670` finds the edge between them; y =
    max(GetAltitude(point), hand.y).
  - Per frame, `fn_008274A0` on each ripple (from `fn_005E5CD0` 0x5E6264..0x5E628D): life −= g_game_time_inc; below
    0, `fn_00827450` unlinks it and frees its sprites. The Z callback `fn_00827500`: every sprite grows by
    g_game_time_inc × 0.001 × 10, wraps past 14, alpha ftol((1 − s/14) × fade × 255), with fade = life × 0.001 under
    1000 ms, otherwise 1. The sizes are kept, so the next draw grows them again.
  - **Quirk:** the ripple ctor never writes ripple +0..+8, so its Z-sorter key reads whatever the heap held there.
  - The hand-crossing statics [0xEB9A48] (inside on the previous frame), [0xEB9A68] (filled) and [0xEA9EF0]
    (previous point) are never cleared in the original; `InfluenceCircle::Reset` only empties the list.

#### Virtual influence

What the hand keeps of its player's influence past the border: a strength 0..1 that lets the land within 10 m of the
hand count as in the influence, wanes away from the last place the hand was in it, and comes back while the hand is
inside.

- **The object.** `GVirtualInfluence` (0x60 bytes) belongs to an interface status (+0x130). `GInterfaceStatus::Init`
  0x5DD1A0 makes it only when +0x130 is NULL (0x5DD1D0..0x5DD1FA) and `SetToZero` 0x5DBA00 does not touch it, so it is
  kept from one land to the next. The ctor 0x76CCA0: everything 0, +0x34 radius per strength 10.0, +0x30 strength 1.0,
  +0x40 the hum's start strength 1.0, +0x38 the status. Fields: +0x08 the last place in influence (MapCoords), +0x14
  the mana path's start (MapCoords), +0x20 and +0x48 the hand's place at the turn (copies of status +0x14, read by
  nothing else), +0x2C the turn last inside, +0x3C hum started (byte), +0x44 switched off, +0x58 the last time scale
  (written only). Status +0x14 is the status's synced MapCoords.
- **`SET_VIRTUAL_INFLUENCE`** `GScript::SetVirtualInfluence` 0x6FD6C0: pops the player (float, `__ftol`,
  `ConvertScriptPlayerToGamePlayer`), then the switch (its raw value: any but 0 is on) →
  `GPlayer::SetVirtualInfluence` 0x64C3B0: for every interface status of the player (`GetNextInterfaceStatus`) with a
  virtual influence, +0x44 = (on == 0); switched off, +0x30 = 0. Its other caller is `Citadel::ToBeDeleted` 0x462C90
  (in a multiplayer game only, with the 0 it clears the player's citadel with). The land scripts: `SetupLand1` switches
  player 1's off (its ninth statement, from `LandControl1`'s first), `SetupLand2`, `SetupLand3` and `SetupLand5` switch
  it on. Since it is kept between lands, Land 2 starts with the strength Land 1's switch left: 0. Land 1's first turns,
  before `SetupLand1` switches it off, have it on at full strength.
- **The turn** fn_0076CD40 (named `FindOtherCreatureAndReturnHowAggressiveAndNear` in the symbols; it is in the class's
  range): from `GInterfaceStatus::Process` 0x5DC4F2, right after the movement fn_005DBC60, and from fn_005DA400
  0x5DA46B, the interface's apply-to-map-coord with something in the hand, before the held object is asked whether it
  is available. Switched off (+0x44), it returns **before** the hand's place is noted. Then +0x20 = +0x48 = status
  +0x14; under an anti-influence ring of the player (`Influence::IsInAntiInfluence` 0x5CD490) +0x30 = 0; in its raw
  influence (`IsInPlayerRawInfluence` 0x5CD460) +0x08 = the place, +0x14 = 0, +0x2C = the game turn (g_game +0x205A40)
  and the strength + 0.1 ([0x8AB22C]), at most 1. Outside: d = `MapCoords::GetDistance`(+0x08, the place), t = the
  turns since +0x2C (as an unsigned 64-bit integer), the chants fn_0076D2A0, s = 1 / (chants /
  `virtualInfluenceChantsToDouble` [0xC5E238] + 1); P = player +0x8C (the influence power) × 0.2 ([0x8AA3AC]); strength
  −= 1 − `GetDistanceModifier`(s × t, P + `virtualInfluenceMaxGameTicks` [0xC5E234]) × `GetDistanceModifier`(d, P +
  `virtualInfluenceMaxDistance` [0xC5E230]); under 0.1 it is 0.
- **The chants** fn_0076D2A0: over the six worship sites of the player's citadel (player +0xA48, +0x34), each site's
  spare chants (`WorshipSite::GetChantsAvailableForVirtualInfluence` fn_0077CB20: 0 when `GetChantsAvailable` 0x77CC10
  is 0 (0x77CB2F..0x77CB3A), else that less the site info's chants reserved for maintaining (+0x158,
  `GetChantsToReserveForMaintaining` 0x77A950), held to `WorshipSite::GetChantsVillagerCanSupply` 0x77E060 (villagers
  worshipping `GetNumberVillagersWorshipping` 0x77B960 × info +0x144 ChantsPerVillager × player +0x70, the tribal
  power, 0x77E069..0x77E095) less the site's used +0xFC (0x77CB4E..0x77CB71), not under 0, divided by the player's
  number of interfaces `GetNumberOfInterfaces` 0x64AA60) are
  **used** (`WorshipSite::UseChants` 0x77BBB0) and added up. Only a turn with the hand outside does this.
- **The grant** fn_0076D330 (place, type), from `CalculatePlayerInfluence` 0x5CD1C1 for each interface status of the
  player; the first that is not 0 is the answer, before the raw influence. The place under an anti-influence ring of
  the player gives 0. It is measured from status +0x14, except for the local status (`MyInterfaceStatus`) when the type
  is the interface's (1), or the type is 0 and the game is not paused (g_game +0x59A4 == 0): then the place is moved by
  (status +0x14 − GInterface +0x3F0), so the distance is from the interface's action point. Inside the strength (at
  most 1) × 10 m ([+0x34], strictly) it gives that strength, else 0.
- **The draw** `GVirtualInfluence::Draw` 0x76D000, from `CHand::Draw` 0x46D2A7, only for the status of
  `MyInterface`: the hand's place is the CHand (interface +0x3A0) +0x78 position × 6553.6 ([0x8AC400]) with altitude
  0. Nothing (and a started hum is stopped, fn_0076CF60: `StopPlayingSoundEffect` 0x81) unless the strength is not 0,
  the last place in has an x that is not 0, it is switched on, the last place in is outside every anti-influence ring
  and inside the raw influence, and the hand is outside both. Then: when the hum is not started, +0x40 = the strength
  and fn_0076CED0 plays sound 0x81 (`G_VirtualInfluence_04`, 129, a 2D loop) at the strength × 100 ([0x8AB41C]) as its
  pitch; when the mana path's start has x 0 it is worked out, fn_005903A0(hand, last place in, 0.5): x and z = the
  hand's + the difference × 0.5 rounded to the nearest (a half to the even one), the altitude = the hand's + (the
  other's − the hand's) × 0.5, so **half the last place's altitude** (a loop around it repeats the same call four times
  with influence tests whose answers are not used); `PSysGlobal::AddManaPathSprite` 0x68FB20(start, hand, the player's
  colour, the strength at most 1); last `SetSoundFraction` 0x76CF90 with the smaller of the strength and +0x40, which
  starts the hum again when fn_0042A280 says it is not playing (inferred: it has played out) and sets its pitch to
  that × 100.
- **The sparks** `AddManaPathSprite`: [0xD4EC24] += the frame's game time (the int [0xEA9EC0]) × the strength; only
  past 10 ([0xC02A0C], strictly) it is set to 0 and a `SpellPointInf` sprite (`GetOrCreateSpellPointPSys`) is made
  between the start and the hand with the colour's four channels × ftol(strength × 255) / 256. [0xD4EC24] is a single
  global: **one count** for the whole game, whichever hand lets the sparks out.

### Player alignment

GPlayer +0x60.

- Value from −1 (evil) to +1 (good) at +0x08 and a pending change at +0x0C. New game: 0 (`GGame::Init` 0x54FEA0 takes
  the one from the profile, 0 without one; from esi+0xF4 → +0x1C). `GPlayer::LoadPlayerAlignment` 0x64D355 reads the
  value saved in the registry, clamped to −1..1. It lives with the player, not with the land (it is not cleared when
  loading a map).
- `GAlignment::CrudeSet` 0x4146F0 (+8 = the value clamped to −1..1); `CrudeUpdate` 0x4146B0 (+8 += change, clamped at
  once; used by SET_ALIGNMENT and the network packets).
- **Acts** (`GAlignment::Update` 0x4145A0 for trees): ±`GPlayerInfo::treePullPutAlignmentChange` (0.005), weighted by
  the current alignment (fn_00414660): towards where it already leans it counts `v·(1 − |a|/2)`, against it `v·(1 +
  |a|/2)`; it is added to the pending change. Uprooting with the hand (`Tree::InterfaceSetInMagicHand` 0x74B730) is
  evil; replanting (`Tree::EndPhysics` 0x74BBB6) and the tree that water plants (`Tree::ApplyWaterSpell`) are good.
- **Every turn** (`GPlayer::Process` → `ProcessForPlayer` 0x4141A0 → `Process` 0x414140): the pending change, clamped
  to −1..1, times `maxAlignmentChangePerGameTurn` (0.0019444 = 0.7 per game hour) is added (`CrudeUpdate`, clamped to
  −1..1) and the pending change goes back to 0. In other words, the pending change is a **fraction of the maximum
  rate** of that turn: an uprooted tree moves the alignment by about 10⁻⁵ (−0.005 × 0.0019444). That is what the code
  says; other acts (effects, miracles, deaths) contribute much more.
- Script: `GET_ALIGNMENT(player)` (`GScript::GetAlignment` 0x6F9A60) returns the value (`GPlayer::GetAlignmentValue`
  0x64D6A0); `SET_ALIGNMENT(player, v)` **adds** v (`CrudeUpdate`, despite the name) and outside −1..1 gives the error
  "Alignment out of range" without doing anything (`GScript::SetAlignment` 0x6F99C0).
- **The history** (`CAlignmentHistory`, one global at 0xC4CD40; `AddTotal` 0x415480 and the `Add*` wrappers
  0x414D40..0x4153C0, e.g. `Add(GPlayer*, Tree*, float)` 0x415260). **Retail never reads it**: its only reader is the
  debug overlay 0x414840, which has no caller, and its node lists are never recorded (byte +0x85 is always 0). The
  advisors (`GGuidance::HelpSpritesAlignmentProcess` 0x71CEB0) read the raw per-turn change straight from
  `GAlignment::ProcessForPlayer` 0x4141A0, not the history.
- The **terrain** alignment (`MapCoords::GetAlignment`, the one for growth and the fields) is something else, from the
  influence of each cell: every player's influence at the point times their alignment, clamped to −1..1
  ([the tree's growth](forest.md#tree)).
- **Deaths** (`GAlignment::Update(GPlayer*, Object*, DEATH_REASON)` 0x4143B0, from `Villager::VillagerDead` 0x750818 on
  the owner's GAlignment). The change is v = GPlayerInfo +0x20 + 4r (`dealthReason[r]`); a child (vt +0x458) doubles
  it (`fadd st0, st0`); an animal (vt +0x454) halves it (× 0.5). There is no ScaleChange and no clamp: the pending
  change += v (0x414400), and `ProcessForPlayer` folds it in.
- `GAlignmentInfo` at runtime is at 0xC4CE30 (stride 0x48), rows 0 burn .. 4 fly away. The column comes from
  `GObjectInfo::GetAlignmentType` 0x4012A0 (vt 0x34 of the info at +0x28).

#### The player system's list of player entities

- The original has no list of player objects to keep: its players are the game's fixed table of `GPlayer`
  (`GGame::GetPlayer` by index, no range check), all there from the game's start. A lookup always gives the player as
  it is on the land being played.
- `TOGGLE_COMPUTER_PLAYER(player, on)` (feature-script case 77, 0x717842) creates nothing: unless the GSetup flag
  [0xC20D5C] is 0 (1 at start; named `CreateMultiplayerCreatures` in the symbols), it takes
  `GPlayer::GetPlayerFromText` 0x64B5E0 and calls fn_0064C400(on) on that player (0x717860): with a computer-player
  object at +0x944, its +0x1B8 = on, and with on != 0 the player's +0x8E0 = 2. Land 2 uses it for PLAYER_TWO and
  PLAYER_THREE, Land 3, Land 5 and comp.txt for PLAYER_TWO; Land 1 never does.
  - The arguments: the first is the player's name (the parameter block's text at +0), the second the on or off
    (+0x6004, an integer). The switch reaches case 77 with ebx = 0 (0x7152FB), so the command is skipped only while
    the flag is 0. The flag is 0 only while `GGame` loads a network map (0x54F828, 0x54F8B8, set back to 1 at
    0x54F83A and 0x54F957, each followed by `GGame::SetUnusedCitadelComputerPlayers`); a story land always runs the
    command.
  - fn_0064C400 0x64C400..0x64C422 is the whole rule: `if (+0x944) { +0x944->+0x1B8 = on; if (on) +0x8E0 = 2; }`
    (the write at 0x64C410). Off writes only the computer player's +0x1B8 = 0: the player's +0x8E0 stays as it was (2
    after an on). On twice is the same as once. +0x944 is always set: `GPlayer::Init` 0x649190 writes +0x8E0 = its
    PLAYER_TYPE argument (0x64919C) and, when +0x944 is null, allocates the 0x1FC-byte `GComputerPlayer` (fn_00656D40
    with the player, 90.0, 30.0 and 0x32, 0x6491CA..0x6491FD). `GPlayer::SetToZero` clears it (0x649372, 0x649409)
    and the player's release deletes it (0x6492C2..0x6492C8). `GComputerPlayer`'s `Init` is 0x656E60, `InGameInit`
    0x657060, `NewProcess` 0x657420.
  - The other callers of fn_0064C400: `GGame::SetUnusedCitadelComputerPlayers` 0x555AF1 (on = 1 for every player of
    type neither 1 nor 2 that has a citadel at +0xA48, after a network map loads; it also makes that player a creature
    at the citadel and moves it through the 13 development phases), fn_00555B90 0x555BC6 (from
    `GPacket::ProcessPacket` 0x63C48B) and `GNetwork::ProcessOnePacket` 0x634CDF: network paths only.
  - The kind goes back at the end of the land: `GPlayer::OnEndOfClearMap` 0x64CD00 calls fn_006588D0 0x6588D0 on its
    computer player, which is fn_006572C0 0x6572C0 (the computer player's +0x1B8 = 0 and, when its player +0x28 has
    +0x8E0 == 2, that +0x8E0 = 0, 0x6572D9..0x6572E2; then its lists are emptied) and then `GComputerPlayer::Init`
    0x656E60. So a computer player of one land is a player not playing (0) on the next until a script switches it on.
  - The last writer, 0x64C3A0..0x64C3AA (between `SetMagicTypeEnabled` and `SetVirtualInfluence`, no symbol), is a
    plain setter `+0x8E0 = argument`; no call to it and no pointer to it is in the exe, so nothing runs it.

#### Spell effects

- `GAlignment::Update` 0x414410: nothing if the life did not change. `K = |Δlife| +
  GPlayerInfo.applyEffectAlignmentChangeAddition`: player 0 stores at +0x64 the pointer to `GPlayerInfo` 0xD47988, and
  +0x1C in memory is file +0x0C. For crush, hit, heal and push, `pending += f(v × GAlignmentInfo[i][col] × K)`; for
  burn, the same with `ConvertTemperatureToDamage`. **fn_00414660 compares the sign of the change with that of A** (0
  counts as positive): same sign `v(1 − |A|/2)`, opposite sign `v(1 + |A|/2)` (read again at 0x414660..0x4146AD: `je
  0x414696` if A ≥ 0; in each branch `jne` if v < 0; corrected on 2026-09-30, the first reading said it only looked at
  the sign of v).
- **Whose alignment** (`Object::ApplyEffect` 0x637B82..0x637C05, its last step): an applier (+0x28) that `IsCreature`
  (vt +0x34) moves its own GAlignment (Creature +0x168). Otherwise p = `EffectValues::GetPlayer` 0x5254C0: with an
  applier, a tail jump to the applier's own `GetPlayer` (vt +0x1C, 0x5254CB), which may be none; with none, +0x3C,
  else the neutral player (g_game's player at its +0x205A5B index, 0x5254D5..0x5254ED). With p: the alignment is p
  +0x60 (every GPlayer has one, the neutral one too, GPlayer ctor 0x648E54); if the object has an owner (vt 0x1C),
  owner +0x94[4 × p.+0xB5] += the harm (0x637BC5..0x637BEA); then `GAlignment::Update(this, ev, life0)`. With no p
  nothing moves. A physics impact's applier is what struck it: a rock's own player (the neutral one unless it was made
  with another), a villager its town's owner (none without a town), and with nothing striking it the thrower's player,
  else the neutral player ([physics](../physics/physics.md)).

#### The sky alignment

`fn_0064AC30`, once per turn at the end of `GPlayer::ProcessPlayers` (0x64A697): the player with the most influence
(`Influence::CalculateMostInfluentialPlayer` 0x5CD630: the first, in player order, whose influence exceeds that of the
previous ones and 0; if none, the neutral one) at the interface's position, **GInterfaceStatus +0xB0 = the camera's
position** (as shown by `UpdateSpellInfo` 0x5DC948, which computes the camera's front as +0xBC − +0xB0), and `x =
clamp((alignment + 1)/2, 0, 1)` is what `fn_005E2240` receives. It starts at 0.5; `DoCitadelMultiplayer` fixes it at
0.5 (there is no multiplayer).

### Reactions

- `CreateReaction` 0x6E3D70 creates the 0x44-byte object (radius of the ctor 0x6E39D0: 1 if the reaction grows,
  otherwise `maxReactionDistance`) and spreads it once (`SpreadReaction` 0x6E3E10, the spiral from
  [the animals' reactions](../beings/animal-life.md#reactions)): each living thing in the cell, in the cell's order,
  goes to the handler of its class. The records (+0x98), the score fn_006E4620 and the switch rule are common to the
  living. In info.dat REACT_TO_FIRE does not grow: 35 m.
- `SpreadReaction` 0x6E3E10 walks (max(1, trunc(radius × 0.2)))² × GetReactionPower cells. `GetReactionPower` (vt
  +0x4F4) is 1.0 in `GameThingWithPos` 0x4024D0; `Spell` 0x55CF10 overrides it with `GetSpellStrength` 0x720750, and
  `Tree` 0x55D8D0 with its life.
- The stealth branch of 0x6E3E10: when +0x20 is set and the cell's first object is another Living, `GameRand(1000) >
  stealthRandomChance` skips the cell; otherwise the stealth flag is cleared. No creator sets +0x20.
- In `ApplyReactionToLivingObjectsAtSquare`, the shield test (0x6E4031, `fn_0072B990`) comes after the class's vt
  +0x984 test and before the distance; it applies to villagers and animals alike.
- **Nothing turns the reaction's source away**: `ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 (its only caller is
  `SpreadReaction`, 0x6E3F4B, which passes the reaction's thing as the source) never compares the living with the
  source (0x6E3F90..0x6E4334), nor do the class tests it calls (`Creature::IsAvailableForReaction` 0x4F2820 and
  `Living::IsAvailableForReaction` 0x5F11F0 take only the type). `SpreadReaction`'s one compare with the source
  (0x6E3F05) is inside the stealth branch, which no creator enables. So the living that started a reaction is offered
  it like every other living in reach, at distance 0:
  - a burning creature scores its own fire: `Creature::ReactToFirePriority` 0x4F2EE0 tests only the distance against
    the two radii and the heights, which its own fire passes, and returns the row's priority (the byte at 0xD4FAA8,
    210); its setup is `SetupReactToFire` 0x4F2F70;
  - a burning animal likewise: `Animal::ReactToFirePriority` 0x4203A0 returns that byte with no test at all (setup
    `SetupReactToFire` 0x420460);
  - a burning villager starts no REACT_TO_FIRE: the fire's update fn_0072EFB0 creates it (0x72F75B) only when
    `IsVillager` (vt +0x2C8, 0x72F745; 1 in `Villager` 0x55CAB0, 0 in `GameThingWithPos` 0x401D10) is 0, and outside
    the hand and flight (+0x24 & 0x44, 0x72F729);
  - a dying villager's own REACT_TO_DEATH scores 0 by its priority (`I am the initiator -> 0`) and its death states
    take no reactions.

  The only initiator test seen is `Creature::UpdateHowImpressed` 0x4F2780 (nothing for its own reaction), which is
  about impressiveness, not reacting.
- `Reaction::GetInfo` 0x6E4700 = 0xD4F6B0 + 100 × the type, + 0x24. The info's fields start at +0x10: the ctor
  0x6E39D0 reads `whetherReactionGrows` at +0x34 and `maxReactionDistance` at +0x2C;
  `whetherReactionFinishesIfInitiatorInHand` is at +0x28.
- Turns to react: `Living::StandardNumGameTurnsToReactFunction` 0x5F18C0 = ftol((1 + 0.5 × (R − d)/R ×
  howImportantIsDistance) × numGameTurnsForNormalThingsToReact), R = 10 + maxReactionDistance, each step in float, with
  `fimul` for the turns (info +0x14, 0xD4F6C4). `StandardNumGameTurnsBeforeReactingAgainFunction` 0x5F1920 is the same
  with numGameTurnsForNormalThingsBeforeReactingAgain (info +0x18, 0xD4F6C8).
- Field +0x04 of the reaction type table 0xC09CC0 + 0xC0 × t (read at 0x6E41BE): a Living may switch to another
  reaction of the same type. It is 1 only for 10 REACT_TO_FIRE, 28 and 35 (initialiser 0x6E0E80).
- `fn_006E47C0(obj, v)` sets +0x30 = v (the "available" flag) on every reaction the object started (+0x14, list g_game
  +0x205BDC, next +0x40); `HandleApplyResult` calls it with 1 (0x5DA136). `fn_006E47F0` (from the put-in-hand
  `fn_005DC330`, 0x5DC38C) sets +0x30 = 0 on the object's reactions whose info asks
  `whetherReactionFinishesIfInitiatorInHand`.
- `fn_006E4830` moves a reaction to another initiator (from `FireEffect` `fn_00730960`: a tree's fire passes to its
  DeadTree). `Reaction::GetReactionInitiatedByObject` 0x6E4870 returns the first one's id. `fn_005F0FB0` returns a
  record's turn.

### Object life

- Life in 0..1 (Object+0x48). A creature keeps the Object life, +0x48 (**proven** from the executable). Its vtable
  keeps `Object::GetLife` 0x402600 (`fld [ecx+0x48]`) at vt 0x11C and `Object::SetLife` 0x63A140 at vt 0x5B0, and
  overrides only `ReduceLife` (vt 0x5B8, 0x47DD00), `IncreaseLife` (vt 0x5BC, 0x47DE20), `ApplyEffect`, the defence and
  `DestroyedByEffect`; there is no other life field. Every creature reader and writer goes through vt 0x11C / 0x5B0:
  the sleep heal (sub-state 0x5086D0, `SetLife(clamp01(GetLife() + sleepHeal))` at 0x508706 and 0x508793), the
  fight's end (fn_004845F0, `GetLife() − (1 − fight health) × 0.25` at 0x48467F..0x4846A3), the fight's start (about
  0x49BD6A, `(life + 1) × 0.5`), the RestoreHealthFromLife desire source (getter 0x4DE9C0 = `1 − GetLife()`, setter
  0x4DE9F0 = `SetLife(1 − v)`, registered at 0x4DDAD1 / 0x4DDAE4), `DisplayStatistics` 0x4765E5 (a developer text dump
  of the first creature's values, not the status panel, see [the creature](../creature/overview.md)), Draw 0x517958,
  `UpdateOnCompletion` 0x475E52 (+0.5, not clamped) and 0x475EB3 (= 1), `DestroyedByEffect` 0x476F8E (= 1), 0x5076C2
  (= 0.4), 0x5052FA (= 1) and the script's GET / SET of the life.
- `Living::Living` 0x5EBEC0 starts with `SetLife(GLivingInfo::life)` (for a creature, from the creature's row);
  `Object::Object` 0x636520 with 1.0.
- `Object::ReduceLife` 0x637810: if the life is less than the amount, 0; otherwise life − amount. Returns the new one.
  Villagers and animals do not override it.
- `Object::IncreaseLife` 0x637870 (and `Villager::IncreaseLife` 0x753460, which calls it): up to 1.
- `Villager::SetLife` 0x756B40 counts in the town (Town+0x714) the villagers below 0.7 life (fn_00756BC0 /
  fn_00756BD0; see [injured villagers](../beings/villager-hand-and-death.md#injured-villagers-the-towns-count)).
  `Object::SetLife` 0x63A140 has no clamp; it refuses a value at or below 0.01 when Object+0x24 has 0x4000
  (INDESTRUCTIBLE) set (`dh & 0x40`), or 0x200 (`dh & 2`) while `g_game+0x25005C → +0x45E8` and `+0x45EC` are set (an
  indestructible creature would not faint from a miracle). The callers keep the range: the sleep to 0..1,
  `IncreaseLife` at most 1, `ReduceLife` at least 0; `UpdateOnCompletion`'s +0.5 can pass 1.
#### More addresses from the code

- `GPlayer::CalculateInfluencePower` 0x64AD00: +0x8C = `Citadel::GetInfluence` of the player's citadel (+0xA48, with
  its heart +0x30), + each town's +0x5C8 (fn_0073FBC0, 0x73FBC0, the town list +0xA50), + the radius +0x38 of every
  influence ring of the player (g_game+0x205C4C, anti rings too), each a float sum; stored and returned. The rest of
  the function (+0x90 = (+0x90 + +0x8C) × 0.5 and the GameStats history +0xAC) is not ported (no reader).
- The curtain's alpha constants: 120 (0x78, 0x826CA2) from y = 200 ([0x8C7B34]) up, below that ftol((y − 100) × 0.01
  ([0x8C4B10]) × 120 ([0x8CF338])) (0x826CC7..0x826CF3); nothing at all while g_camera.y [0xEA1DBC] <= 100 ([0x8AB41C],
  0x826CA9..0x826CC1).
- The ripple's sprites wrap past 14 ([0x99C9EC]) and 1 / 14 [0x9A3934], the alpha's 255 [0x8AB270].
- [0xEA9EF0..0xEA9EF8]: the hand's point of the previous crossing call (x and z; y is written 0, 0x82786A /
  0x827A1F), which every call overwrites with the current one (0x827A12..0x827A2A), the first included.
- `Living::SetReactionDoneWhen` 0x6E44A0 (the switch of `ApplyReactionToLivingObjectsAtSquare` 0x6E428D): the record
  of the type gets the turn; without one a new record {type, now} is made and, when the list already holds 3, its
  head (the oldest) is dropped first (0x6E4524..0x6E453B). No "again" test and no 1800-turn pruning (unlike the
  records' fn_006E4340). The new record goes at the tail (0x6E4568..0x6E457F).
- `Living::StandardNumGameTurnsBeforeReactingAgainFunction` 0x5F1920 (table +0x40) is the turns-to-react rule with
  numGameTurnsForNormalThingsBeforeReactingAgain.
- `GAlignment::CrudeUpdate` 0x4146B0 adds the change, clamped at once.
- The alignment history: `CAlignmentHistory` 0xC4CD40 and fn_00414E10; fn_004153C0 is the only reader of the
  alignment type kept with a record.
- `Spell::GetSpellStrength` 0x720750 is `Spell`'s `GetReactionPower`.

## openblack

Status: **done** for the influence query, the rings, the border, the virtual influence's strength and grant, the
alignment, the reactions' spread and the life; not ported: the allies, the multiplayer rule, the temple's world room
border, the colour remap, `CalculateMostInfluentialPlayer`, the virtual influence's draw (hum and sparks), the
alignment history and the computer god. Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Influence** (`src/ECS/Influence`): the GatheringFlag is `OPENBLACK_INFLUENCE_EVERYWHERE`. `GPlayer+0x934` is not
  ported (always 0). The batch drop outside the influence is in `HandResources.cpp`. openblack's `GAbodeInfo::Find`
  would return the tribeless records at the end (ark 1, totem 120), so the record is looked up by the mesh.
- **The border** (`src/ECS/Influence/InfluenceCircles.cpp`, `src/Graphics/RendererInfluence.cpp`): the curtain is
  `land_morph::InfluenceCurtain`, scrolled by `frame_anim::InfluenceScroll`, material `materials::k_InfluenceCircle`,
  colours `influence::k_CircleColours`. The latch is per player (`influence::BoundaryShown`), set as the original sets
  it: `worship::citadel::SetHeartDrawPercent` sets `influence::ShowBoundary` at 1. The crossing ripple is made by
  `influence::ProcessHandCrossing` and drawn with the border. Every openblack user of the player colours takes the
  identity remap. openblack has no temple world room, so `Draw(0)` is not ported.
- **The virtual influence**: `virtual_influence` (`src/Common/VirtualInfluence.*`, pure), its state the local player
  entity's `components::VirtualInfluence` (the player system keeps it between lands, with what else the players keep:
  `PlayerSystem::KeepForNextLand` takes it off the last land's entity, `TakeUpKept` gives it to the next land's first
  local player entity), `SET_VIRTUAL_INFLUENCE` in `CHLApi.cpp`. Status +0x14 is the hand system's synced map coords
  (the spell info's interface position). The grant is `Grant` and `MeasuresFromActionPoint`, asked by
  `influence::CalculatePlayerInfluence` for the local player with the hand system's `GetActionPoint` (GInterface
  +0x3F0) and `GetSyncedPlace` (status +0x14); the pause is `game_clock::IsPaused`. The turn: `HandSystem::ProcessTurn`
  after `UpdateTurnMovement` and the ApplyToMapCoord packet's handler after its empty-hand test call
  `InfluenceSystemInterface::ProcessVirtualInfluence` with the synced place. The sparks are `EmitManaPath` with one
  `ManaPathClock`, not called yet. openblack has no multiplayer, so the citadel's caller is not ported.
- **The alignment** (`src/ECS/Effects/Alignment.*`): one `components::Alignment` per `PlayerNames` outside the land's
  registry (`Magic/Core/Players`, `AlignmentOf`), the same one the miracles use. The turn's step is in slot 3 of
  `Magic/MagicLoop.cpp`, `GPlayer::ProcessPlayers`. The advisors are `audio::guidance::UpdateAlignmentRemarks`. The
  terrain alignment is `alignment::LandAlignmentAt` (`src/ECS/Effects/Alignment.cpp`), read by the fields, the trees
  and the building sites. Whose alignment: `effects::EffectPlayer`, used by the physics impacts; the miracles set
  `EffectValues::player` to the spell's player directly.
- **The player entities** (2026-10-14): `FeatureScriptCommands::ToggleComputerPlayer` takes the land's entity of that
  player (`magic::players::EntityOf`) and makes it only when there is none (the original's players all exist; on twice
  no longer makes a second entity). The type is `magic::players::TypeAfterComputerToggle(type, on)`: on gives 2
  (`PlayerMagic::k_ComputerPlayerType`), off leaves the type. The flag at 0xC20D5C is not kept, as openblack loads no
  network maps. Every land's players are new entities of type 0 (PLAYER_ONE 1), which is what `OnEndOfClearMap`
  leaves. openblack makes a player entity per land (LOAD_LANDSCAPE makes PLAYER_ONE, TOGGLE_COMPUTER_PLAYER the
  computer players) and `PlayerSystem` lists them by name (`AddPlayer`, `RegisterPlayers`, `GetPlayer`). Since
  2026-10-08 the list is emptied on every land load (`magic::players::Reset`, before the registry is reset), and the
  computer players a script makes are listed as they are made. Before, the list was never emptied and only kept the
  first entity listed for a name, so from the second land on it named the last land's destroyed entities, and the
  script's computer players were missing. Nothing in the game reads the list yet, so neither change moves a measured
  check.
- **The sky alignment** (`alignment::GetInterfaceAlignment`): slot 3 of the turn, after `alignment::ProcessPlayers`.
  `Clouds::InfluentialPlayerAlignment` (map one) returns `2x − 1`, except with the `OPENBLACK_TEST_SKY_ALIGNMENT` hook
  or the debug slider moved off 0.
- **Reactions** (`ECS/Effects/Reactions`, the only module, merged with the animals' one): the handlers are set with
  `SetLivingReactionHandler` (animals in `ECS/AnimalFlee.cpp`, villagers in `VillagerReactions.cpp`, which dispatches
  fire and teleport); the records are `components::ReactionRecords`. Before, fire always spread with
  `maxReactionDistance` (inf) and sorted the cell's villagers by entity; now it uses the ctor's radius and the cell's
  order, like the animals (⚠️ approximate: that of openblack's grid, which is rebuilt once per turn and before an
  out-of-turn spread, not the original's lists). There is one clock, the game turn fixed at the start of the turn
  (`BeginTurn`, which also removes the reactions whose initiator no longer exists); the animals' records also use it.
  openblack's spread and the villagers' handlers (slot, fire, shield, teleport) no longer skip the initiator. Nothing
  ported scores its own reaction yet: the creatures and the animals take no fire reaction, a predator's own
  FLEE_FROM_PREDATOR scores 0 (not stronger than itself) and a thrown thing is out of the cells while it flies.
- **Life** (`src/ECS/Life`): villagers now store it as a float (`Villager::life`, before an integer percentage that
  lost small changes from fire or chanting); rocks and animals in `components::Life`. A creature's is its body's
  (`CreatureNeeds::needs.life`), so `life::LifeOf` / `SetLife` read and write it there; the sleep heal is
  `creature_physiology::SleepTurn`, the fight's end `fight::LifeAfterFight`, the desire source `1 − needs.life`. The
  town's count is `Town::injured`, kept by `life::SetLife`. `ReduceLife` does not kill: the death states are not
  ported, so the physical impact (`HurtByImpact`) kills on reaching 0 as before.
#### Where the code is

- Rings (`src/ECS/Influence/InfluenceRings.cpp`): `CreateRing` (`InfluenceRing::Create`), `CreateRingOnObject`
  (fn_005CD990), `DeleteRing` (`InfluenceRing::ToBeDeleted`); the debug hook `LogPoints`
  (`InfluenceDebugHooks.cpp`) prints `GET_INFLUENCE` for `PlayerNames::PLAYER_ONE`, the local player, with allies on.
- `Influence.h`: `CalculateInfluencePower`, `CalculateInfluencePowers` (every player and the neutral one, after its
  alignment: the magic loop's slot 3), `BoundaryShown` (the latch), `CurtainAlpha`, `UpdateRipples` (once a frame),
  `DrawRipple` (the Z callback, `Ripple::k_Sprites` sprites), drawn through `Renderer::CollectInfluenceRipples`;
  `ResetHandCrossing` is for the tests only (nothing in the original clears the crossing's statics).
  `IsAttachedObjectInHand` is ⚠️ approximate: any hand in the original, only openblack's local hand here.
- `InfluenceSources.cpp`: `BaseInfluence` (`Town::GetBaseInfluence`), `ProcessTowns` (the influence part of
  `Town::Process`), `ProcessCitadels` (the latch from the heart's 3D object; 🔍 inferred that the value is the temple's
  appearance fade), `InfluencePowerRatio` (fn_0064B700).
- `InfluenceCircles.cpp`: `HideContact` (fn_00827110, the quirk of the white closing column kept), `MakeRipple`
  (fn_00827250), `CrossingRipple` (the ripple of a crossing).
- Reactions (`ECS/Effects/Reactions`): `SetAvailable` (fn_006E47C0), `SetUnavailableInHand` (fn_006E47F0),
  `GetReactionInitiatedBy` (`Reaction::GetReactionInitiatedByObject`), `RecordTurn` (fn_005F0FB0),
  `StandardTurnsToReact`, `StandardTurnsBeforeReactingAgain` and their common `StandardTurns` (every step a float at
  the game's 24 bits), `InMap` (`MapCoords::InBounds`).
- Alignment (`ECS/Effects/Alignment`): `CrudeSet`, `UpdateForTree`, `UpdateForResource` (`GAlignment::Update(Abode*,
  type, n, delta)`), `DeathAlignmentChange`, `UpdateForDeath`, `LandAlignmentAt`, `AlignmentFactor` (the
  `GAlignmentInfo` row and column); an object with no info takes `AlignmentType::Unimportant` 🔍 inferred. The advisors
  run for `PlayerNames::PLAYER_ONE` (inferred: openblack's local player).

### Test hooks

- `OPENBLACK_INFLUENCE_EVERYWHERE`: the GatheringFlag (influence 1 everywhere).
- `OPENBLACK_ALIGNMENT_TRACE=1`: the alignment's trace.
- `OPENBLACK_TEST_SKY_ALIGNMENT`: fixes the sky alignment.
- The testbed's `miracles.burning_cow` sets out a burning living thing offered its own reaction.

## Pending

- **Influence**: the allies (openblack has no alliances), the multiplayer rule (without a citadel, 0), the border in
  the temple's world room (`Draw(0)`, `WorldRoom::ShowInfluence`), the per-land colour remap `GetRemapedPlayer`
  0x64D790 and `CalculateMostInfluentialPlayer` with its helpers.
- **The virtual influence**: the chants its waning reads and uses up (fn_0076D2A0 over
  `GetChantsAvailableForVirtualInfluence` 0x77CB20, identified [above](#the-original)) are passed as 0, and no site's
  chants are used; the hum (sound 129) and the mana path (one `ManaPathClock` for the game, `SpellPointInf` sprites)
  are neither heard nor drawn: not ported, a port row.
- **`TOGGLE_COMPUTER_PLAYER`**: the computer god's on flag (`GComputerPlayer` +0x1B8) and everything that reads it;
  openblack has no `GComputerPlayer`, so only the player's type is set (with the computer god's port row).
- **Alignment**: the history (`CAlignmentHistory`) only feeds a dead debug overlay in retail (not ported). `EffectValues::GetPlayer`
  0x5254C0 returns +0x28's `GetPlayer` (vt +0x1C), else +0x3C, else the player g_game +0x18 + 0xA60·[+0x205A5B]
  (0x5254C2..0x5254ED; see [whose alignment](#the-original)). Still open: whether each miracle sets +0x28 (9 call
  sites in 3 functions, not read), so whether the miracles' `EffectValues::player` is always their creator's own
  `GetPlayer`.
- **Reactions**: the creatures' REACT_TO_FIRE (`Creature::ReactToFirePriority` 0x4F2EE0, `SetupReactToFire` 0x4F2F70)
  is queued as a fix; the animals' (`Animal::ReactToFirePriority` 0x4203A0, `SetupReactToFire` 0x420460) is not
  ported. Once they are, a burning one takes up its own fire, as nothing turns the source away. The reaction switch
  (field +0x04) is not ported.
- **Life**: the INDESTRUCTIBLE floor of `Object::SetLife` 0x63A140 (0x4000, and 0x200 under a game setting) is not
  ported, nor `UpdateOnCompletion`'s +0.5. The creature's starting life: `Living::Living` 0x5EBEC0 calls
  `SetLife(GLivingInfo::life)` from the creature's row, ours starts at 1 (`creature_physiology::Start`); queued as a
  fix.
- **Sounds of the interface and the hand with no place in openblack yet** (the logo, the arena and leash spell
  clicks, a town's conquest, the creature's acknowledgement, the virtual influence's hum, chests and scrolls, the menu
  click): on [Creature and interface sounds](../audio/creature-and-interface-sounds.md#interface-and-hand-sounds-with-no-place-in-openblack-yet).
