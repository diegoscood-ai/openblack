# The temple and the citadel

## Summary

Each god's temple (the citadel) has a heart and up to six worship sites round it, one per tribe whose towns worship
there. The temple's outside is a blend of fifteen meshes, three sizes by five stages from evil to good: it grows with
the player's share of the influence and turns with the player's alignment, and doves or bats circle it in numbers set
by how good or evil the player is. When the heart's life is brought to 0 the temple is destroyed in a 22-second
sequence of beams, a glow and an explosion, and the game is over. Until then, blows to the heart are passed on to the
player's buildings and villagers.

openblack ports the six slots, the outside's blend, the flock, the destruction's timeline, the heart's blows and the
shell drawn over the heart as it is destroyed, and the heart's own colours and fade; the destruction's beams are not drawn yet. The temple's interior is on
[the temple interior](../interface/temple-interior.md); the temple in the villagers' route plan is on
[routes](../beings/routes.md).

## The original

The addresses below are those of the v1.42 exe, read with the 1.20 symbols (bw1-decomp has the header
`src/Black/CitadelHeart.h`, no bodies for these).

### The citadel and its six slots

Six slots; the angle of slot *n* is **the heart's angle + n × 2π/7** (`Citadel::GetWorshipSiteAngle` 0x463610; the
slot angle 2π/7 is the float 0x8C836C) and the site is placed at `radiusFromCitadel` from the citadel's origin.

- `Citadel::AddTown` 0x463130 → `FindOrCreateWorshipSite` 0x4631D0 / 0x463220 → `FindTribeWorshipSite` 0x463190 or
  `RequestANewWorshipSite` 0x4633F0 (the free slot closest to the nearest town of that tribe, otherwise to the
  citadel).
- `Citadel::FindNearestWorshipSitePosAngleAndSlotToPos` 0x463540: of the free slots, the one whose ring point is
  nearest (x, z) to the target point; best starts at 1e6 (0x497423F0); −1 when all six are taken. `RequestANewWorshipSite` 0x4633F0
  gets that point from `Town::GetNearestTownToPos` 0x73B170 (the citadel's MapCoords, the tribe, 0x7FFF = any abode
  type, FLT_MAX [0x8C7E1C]) at 0x46345C: that town's MapCoords, otherwise the citadel's.
- `WorshipSite::Create` 0x77AC50 is called with (angle 0, scale 1.0, percent 0, underConstruction 0). The MultiMapFixed
  ctor sets +0x58 bit 3 and +0x5C = 0 (0x52E22D..0x52E234), so `IsBuilt` 0x77BDD0 (+0x5C ≥ 1) is false until the site
  is built. `WorshipSite::Draw` 0x5193D0 → `DrawBuilding` 0x517F90 then draws nothing at 0 %: **a site that the citadel
  requests is invisible until it is built.**
- The site: `fn_0077A960` = CitadelPart(pos = the citadel's origin, info, citadel, slot, tribe, angle, scale 1);
  `fn_00463770` stores it in sites[slot]. `WorshipSite::AssignTownsToWorshipSite` 0x77AF70 adds every town of the
  player's list whose `Town::GetTribe` 0x73C840 (Town +0x5B8) is the site's, oldest first.
- **A site takes its slot when the citadel asks for it**, at 0 % built: the site's constructor calls
  `Citadel::AddWorshipSite` 0x463770 (at 0x77AA1B), the only caller. The villagers then build it through the building
  site `CitadelHeart::AddAllWorshipSitesNeeded` 0x464F50 gives each town; building it does not move the slot. openblack
  does the same (`site::CreateInSlot`).
- **Land 2's opening, in order** (`GScript::SetBuildWorshipSite` 0x6FEC40): the heart's switch off sets the citadel's
  +0x74, so `Citadel::FindOrCreateWorshipSite(town)` 0x4631D0 answers none and the finished temple opens no site; the
  town's switch on clears +0x5F0 and +0x74 and finds (or asks for) the site, without joining it or giving a building
  site; the heart's switch on runs `AddAllWorshipSitesNeeded(citadel, 0)`, so the town joins the site and gets its
  building site. Test `WorshipSiteBuildTest.TheTownFindsItsSiteAndTheHeartGivesTheTownABuildingSiteOfIt`.
- Only when it has a totem (0x77AD5D): `fn_0077AEE0` makes the Dance of `GDanceInfo[19 + slot]` (0xCC4B80 + (19 +
  slot) × 0xB0) at point 8, `fn_0077B8D0(0.5)` and `CreateFoodPot`. `AddResource` 0x77C638 makes the pot again when it
  is gone.
- `CitadelHeart::CreateBuiltWorshipSite` 0x465110 is the script's `CREATE_WORSHIP_SITE`: it creates the site of that
  tribe **without checking the town** and adds the player's towns of that tribe to it. The position and site number
  that the script brings **are not used**. No site → returns 0. For the first town of the player of that tribe (Town
  +0x5B8), `AddTown` 0x77C800 if it is missing; if that town has a building site of the worship site, `BuildBy(1.0)`
  (vt +0x900, WorshipSite 0x77DC50) and `Town::RemoveBuildingSite` 0x73BA20. With no town of that tribe it still does
  `BuildBy(1.0)` and returns 0 (0x465163..0x465174).
- `GPlayer::PostLoadCleanup` 0x64AB90 (called from `GSetup::LoadMapFeatures` 0x7180B0, right after the land's script):
  for each player with a citadel, each of its towns without a worship site → `Citadel::AddTown`.
- `Town::IsAllowedToCreateWorshipSite` 0x740BB0: **never on land 1**, nor if the script forbids it
  (`SET_CAN_BUILD_WORSHIPSITE`, the flag at Town +0x5F0), nor without population (Town +0x618 + +0x61C, the TownStats
  adults and children, `TownStats::Add` 0x7492E0). That is why in Land1 there are only dispensers and fireflies: Land 1
  never makes a worship site, so nothing of the sites' building reaches Land 1's runs or its land view shots.
  `Town::CheckAddWorshipSite` 0x740BF0 requires that the town is allowed (`IsAllowedToCreateWorshipSite`), that the player is not type 3, and that the
  player has a citadel → `FindOrCreateWorshipSite` and `AddTown`.
- **What the audio reads** (`GGuidance::CheckWorshipSiteDesiresSFX` 0x71B270). It walks `GPlayer+0xA48` →
  `Citadel+0x34..+0x48` in slot order. It skips the sites without dancers: fn_0077B960 jumps to 0x77CFB0, which gives
  `Dance+0x90` or 0 without a dance. Of the others it keeps the one closest to the camera, closer than 200 m
  (0x980130). Then it asks for its `CalculateDesireForFood` (vt+0x420 of `??_7WorshipSite` 0x8F2840 = 0x77C310), which
  is `1 − min((food + 0.0001) / (needed + 0.0001), 1)`.
  - The food is that of the site's pot (+0xB4, `Pot::JustGetResource` 0x66D390).
  - The needed amount comes from `Dance::CalculateFoodNeededByDancers` 0x50BF20: the sum, per dancer, of `(1 − the
    food in its belly +0xE8) × foodReqiredForDinner` (+0x2D8).
  - It also reads `Citadel+0x70`, the fraction of the worship strain sound, limited to 1 at 0x71B31C. Only
    `SetWorshipStrainSoundFrac` 0x463850 writes it (from `ProcessSpellIcons` 0x46396C) and it is saved and loaded with
    the game (0x463D6A / 0x463FB9). +0x70 starts at 0: `Citadel::Citadel` 0x4629F0 calls fn_00462D40 (0x462A3C),
    which zeroes +0x5C, +0x60, the six sites +0x34..+0x48, +0x6C, +0x70 (0x462D5C), +0x74, +0x78 and +0x7C.
- `Citadel::Process` 0x462D70 (from `GPlayer::Process` 0x649525, before the player's towns). The heart's 3D object takes
  `GetPercentBuilt` (vt +0x880) through `LH3DCitadel::SetPercent` 0x883120. When it reaches 1, the heart leaves its map
  cells and enters them again (RemoveMapObject vt +0x548 / InsertMapObject vt +0x544). Before and after that it gives
  the 3D object its two targets (0x462D8F..0x462DE4 and 0x462E55..0x462E65): the alignment target, from
  `GetAlignmentValue` 0x64D6A0, through `SetNasty` vt +0x218, and the size target, from the player's share of the
  influence, through `SetStage` vt +0x20C (an earlier note called them the alignment colour). Then it steps both and
  blends the outside again when it has moved far enough (0x462E6B..0x46304A): see
  [the temple's outside](#the-temples-outside-alignment-and-size). Last, `CitadelHeart::SetAlignmentFlock` 0x465270
  (0x463050..0x463068): see [the temple's doves or bats](#the-temples-doves-or-bats).
- `CitadelHeart::Built` (0x46501B..0x4650F1): GetPlayer (0x468020: +0x80 ? the citadel's player : the town's),
  SetLife(1.0), `fn_00464F50(citadel, 0)`. `fn_00464F50` opens the worship sites: for every town not yet in a site,
  `AddTown`, then `Town::GetBuildingSiteInList` 0x73CE40 or else `Town::AddBuildingSite` 0x73B8E0, with the desire boost
  at +0x63C (0x464FCE). Then, only for the local player and outside a script's wide screen:
  `GAudio::StartScriptMusic(0x3D)` 0x428230 if no script music is playing and the land is not 1 (g_game +0x205A08 !=
  1); and `SaveGameRoom::InstantSaveGame(0x14)` 0x792FB0 if the game is neither a playground (g_game +0x205A0C) nor
  multiplayer.
- `WorshipSite::Built` 0x77AC10 (after `MultiMapFixed::Built`): the site's building site is removed from every town of
  its player (each town of `GetPlayer()`, `CitadelPart` 0x469750, the citadel's).
- The worship strain's frame time is read every turn as `mov eax, [0xD01A38]; fild qword; fmul [0x8C7E30]` = 0.001f
  (0x4638A4..0x4638B9).
- `Citadel::ToBeDeleted` sets the player's +0xA48 to 0 when the citadel is marked (0x462C5E..0x462C66), so a marked
  citadel belongs to nobody.
- `SET_INTERFACE_CITADEL` (414, 0x70B9A0) writes GScript +0xA0 = the popped value (0x70B9BD); `GScript::Reset` 0x6EB312
  sets it to 1, only when the scripts restart: a land change keeps it
  ([what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps)). `CitadelEntrance::InterfaceValidToTap` 0x468F50 = IsMultiplayerGame ? 1 : GScript +0xA0 != 0.

#### The scripts' temple position and entrance

Read in the frozen v1.42 exe ✅.

- **GET_TEMPLE_POSITION** 451 (`GScript::GetTemplePosition` 0x6FFBA0) pops the script player (a float, `__ftol`
  0x6FFBBC), converts it (`ConvertScriptPlayerToGamePlayer` 0x6EB9A0, `GetPlayer` 0x5509B0) and takes the player's
  citadel (+0xA48). No player or no citadel: three 0.0 (0x6FFC57). Else the citadel's MapCoords (+0x14 x, +0x18 z,
  +0x1C altitude): x and z × 10/65536 ([0x8AA3A4]) and y = `LH3DIsland::GetAltitude`(its MapCoords) 0x803090 + its
  altitude (0x6FFBE4..0x6FFC1C), pushed x, y, z as vector parts (type 3).
- **GET_TEMPLE_ENTRANCE_POSITION** 457 (`GScript::GetTempleEntrancePosition` 0x6FFCB0) pops the height, the radius,
  then the player (0x6FFCC0..0x6FFCF5), and finds the citadel the same way (none: three 0.0, 0x6FFE9A). It takes the
  same point P of the citadel (0x6FFD27..0x6FFD60). A citadel without its heart (+0x30) gives P, with neither the radius
  nor the height (0x6FFD64 → 0x6FFE54). Else the heart's 3D object (`Object` +0x40) gives its special point 15, the
  pen point (vt +0x1CC with edx = 15, 0x6FFD7D; read as the leash posts read theirs:
  [the point](../creature/temple-leash-posts.md#the-posts-and-what-they-hold)) and d = that point − P. Unless d.x,
  d.y and d.z all compare equal to 0 (`fcom`, C3: 0x6FFDA3..0x6FFDD0), d is made a unit vector:
  1 / sqrt((dz² + dy²) + dx²) ([0x8AA390] 1.0, `fdivr`) times each part (0x6FFDD2..0x6FFE0A). The point is then
  d × radius + P, with y = (d.y × radius + P.y) + height (0x6FFE0C..0x6FFE4E), each step a float.
- Callers: `GameOver` (L10..L12, ip 22313, 22323, 22333: the camera's start at radius 90, height 40, and its zoom at
  radius 50, height 10, from the temple) and Land 2's `LearnGestures` (L88, ip 100137: the citadel heart within 10 of
  the temple position, `CALL_NEAR` type 18). `GameOver` starts only when the local player's heart is being destroyed,
  so the heart is still there; `LearnGestures` starts 20 minutes into Land 2. No standard check reaches either.

### The heart's and the worship site's collide shapes

Both build their map collide circles themselves, each child circle with its own radius (all f32, constants with no
writers).

- `CitadelHeart::CreateCollideData` 0x468FB0 (0x8C8448 = 21.5, 0x8C844C = 1.4; the radius at 0x468FBD):
  - 1 circle at the heart's point (vt +0x63C) with radius 21.5 × 0.7 = **15.05 m**;
  - then 7 spokes at angle a = `GetYAngle` (vt +0x508) + 3.83 (0x9CE95C), stepping 2π/7 (0x8C836C = 0.8975979, bits 0x3F65C8FA). On each
    spoke the distance d starts at 21.5 × 0.74 = 15.91, and every child circle has radius 1.4 × (21.5 × 0.11) =
    **3.311 m** (3.3109999);
  - spokes 0 and 1 have 6 circles: 3 at P + d(cos a, sin a) with d += 21.5 × 0.2 = 4.3 after each; one at a ± 0.06,
    then d += 2.15; one at a ± 0.16, then d += 1.505; one at a ± 0.27 (+ for spoke 0, − for spoke 1);
  - spokes 2..6 have 4 circles each at P + d(cos a, sin a), with d += 4.3;
  - that makes 1 + 12 + 20 = **33 circles**: a 15.05 m circle at the centre and 32 circles of 3.311 m.
- `WorshipSite::CreateCollideData` 0x77E490 (0x77E499): **10 circles** from a static table at 0xDCBC98 {x, y, z},
  built once (guard 0xDCBD10). Each local point is transformed by the site's matrix (vt +0x63C), and the radius is
  1.3 (0xC24CE8) × y. The rows are (2.448, 1.4, −34.171), (3.348, 1.4, −36.551), (24.71, 1.4, −24.133),
  (26.359, 1.4, −25.951), (3.93, 2, −39.084), (27.452, 2, −28.503), (7.405, 3, −40.707), (12.317, 3, −39.988),
  (22.537, 3, −35.073), (26.905, 3, −33.063): radii 4 × 1.82 m, 2 × 2.6 m, 4 × 3.9 m, and no centre circle.

### The temple's outside: alignment and size

The temple's outside is a blend of the fifteen meshes `B_TEMPLE<size><stage>` of Data\Citadel\OutsideMeshes, three
sizes by five stages from evil to good, with its texture blended from `Evil<set>.16B` to `Neutral<set>.16B` to
`Good<set>.16B`. The original keeps the look in the heart's 3D object (`LH3DCitadel`):

| Field | Last blended | Current | Target |
|---|---|---|---|
| Size, 0..1 | +0x84 | +0x88 | +0x8C |
| Alignment, 0..1 | +0x90 | +0x94 | +0x98 |

Also +0x9C the draw percent, +0xA4 a dirty flag and +0xA8 the player (& 7).

- **The start** (`InitTemple` 0x882730, from `CallVirtualFunctionsForCreation` 0x4675A0):
  - player & 7 into +0xA8 (0x88273F..0x882744);
  - alignment 0.5 for last, current and target (0x882789..0x88279C); size 0 for the three and the percent 0
    (0x8827A9..0x8827BB);
  - `SetDrawPercent` vt+0x200 with the percent built (0x8827C1), dirty = 1 (0x8827D0);
  - the land flattened (0x8827C7..0x882962; [internals](../project/internals.md#render));
  - `ComputeBoundingBox` 0x8081B0 (0x882999), then `ProcessAlignement` vt+0x208 (0x8829AF): **the first blend is at
    the heart's creation**, due to the dirty flag, before the first turn. That is CREATE_CITADEL in the land script,
    and the plan's conversion for a planned citadel (Land 1: the CHL `BUILD_BUILDING`; the later lands: the AI).
- **The heart enters the map cells after the first blend** (InsertMapObject vt +0x544 at 0x46776C, after `InitTemple`
  0x46767F and the entrance). The heart's vtable 0x8C8D00 gives `MultiMapFixed::InsertMapObject` 0x52E650, which asks
  `CreatureMustAvoid(0)` (vt +0x7BC, 0x52F490, 1 with no creature) and then sends the footpaths round the heart,
  `SendFootpathsAroundObsticle` at 0x52E744, with `Object::Get2DRadius` 0x638180 of the 3D object's mesh (vt +0xF8):
  the blended one, whose box the blend has just made again (0x882E1F..0x882E27). On Land 1 that is B_TEMPLE02's
  36.94 m, not B_FIRST_TEMPLE's 25.58 m, so the footpaths bend round a wider circle from the heart's creation on. Each
  later blend, between RemoveMapObject and InsertMapObject in `Citadel::Process`, sends them round again with the new
  box. `CitadelHeart::ShouldFootpathsGoRound` 0x464B70 is not on this path. The heart and each worship site build
  their collide data with their own `CreateCollideData` (`CitadelHeart` 0x468FBD, `WorshipSite` 0x77E499), with a
  radius per child circle, which the walkers go round
  ([Walking round obstacles](../beings/wall-hugging.md#walking-round-obstacles-mobilewallhug); the fix row for openblack's one radius is in its Pending).
- **Every turn**, in `Citadel::Process` 0x462D70, first thing in `GPlayer::Process`. A citadel without a heart, or a
  heart without its 3D object, returns at 0x462D79..0x462D89: **a plan has nothing to morph**. The percent built
  (+0x9C) is never read by what follows, so **the look moves from 0 % built**, built or not.
  - Alignment target = (`GPlayer::GetAlignmentValue` 0x64D6A0 + 1) × 0.5 ([0x8AA390] 1.0, [0x8AA3B4] 0.5), given to
    `SetNasty` vt+0x218 = 0x80BB50: below 0 → 0; from 1 up → 0.9999 (0x3F7FF972); else as it is.
  - Size target = 2 ([0x8C7E2C]) × `fn_0064ACC0`, given to `SetStage` vt+0x20C = 0x80BAA0: below 0 → 0; above 1 → 1.
  - `fn_0064ACC0`, the player's share of the influence: 0.01 ([0x8C5840]) when the land number g_game+0x205A08 is 1;
    else the sum `fn_0064AEA0`, and 0.01 when it is 0 ([0x8AA398] 0.0); else the player's influence power +0x8C over
    the sum (0x64ACC0..0x64ACF6).
  - `fn_0064AEA0` adds +0x8C over the player slots from the first, power + sum in slot order. Its walk 0x550980 goes
    from g_game+0x18 by 0xA60 up to g_game+0x5318: **all eight slots, with no active test, so the neutral player
    counts**. `fn_0064B700` (the heartbeat's ratio, [the audio engine](../audio/engine.md)) walks with 0x550930
    instead, which skips the inactive slots.
  - Both targets are set twice, at 0x462DCF / 0x462DDE and at 0x462E5A / 0x462E65, around the percent part
    (`GetPercentBuilt` vt+0x880 at 0x462DF3, `SetDrawPercent` vt+0x200, and the map-cell remove and insert when it
    reaches 1).
  - The steps (alignment 0x462E6B..0x462F11, size 0x462F17..0x462FBD): |current − target| ≤ 0.001 ([0x8C7CB0], a
    double) → current = target; else current ± 0.016 ([0x8C8368]) toward the target, and the target when that would go
    past it.
  - `IsChangedSize` vt+0x1FC = 0x882AD0: |last − current| > 0.03 ([0x99A168], a double), for the alignment or the
    size. Only then (0x462FC7..0x46304A): RemoveMapObject vt+0x548, `ProcessAlignement` vt+0x208, then, when it
    returned non-zero and the 3D object's +0xE0 is set, the temple's camera exclusion (3D object +0xE0) is moved and
    resized (`CameraExclusion::Adjust` fn_00454AA0: its position, radius 15, height twice the mesh's +0x28, each held to
    0..10000 by 0x8C58A4; the call at 0x463011..0x46303D, the temple object from fn_004630E0, which returns the
    citadel's +0x30), and InsertMapObject vt+0x544. It is not a visual blend: the camera's exclusion dome over the
    temple follows the temple's size. While a value moves a step a turn, that is a
    blend about every second turn.
  - Then, every turn, `CitadelHeart::SetAlignmentFlock` 0x465270 with the alignment and whether it changed
    (0x463050..0x463063).
- **The blend** (`ProcessAlignement` 0x882B10..0x88309A) returns 0 unless `IsChangedSize` or the dirty flag, and
  clears the flag.
  - The size index is vt+0x214(size) (ftol(2 × min(size, 0.9999))), clamped to 0..2, and the next one; the lower one's
    weight is clamp(next − 2 × size, 0, 1) (0x882B4C..0x882BB6).
  - The stage index is ftol(4 × alignment) ([0x8AB418] 4.0), clamped to 0..4, and the next one; the lower one's
    weight is next − 4 × alignment, not clamped (0x882BBE..0x882C0A).
  - `fn_008830E0(size, stage)` gives the mesh `B_TEMPLE<size><stage>` (sprintf and `LH3DMesh::CreateFromHD(name, 1)`:
    read from the disk afresh each blend, no cache, released after). `fn_00807970(dst, a, b, w)` blends two meshes into
    `dst` with `LH3DPrimitive::Blend` 0x855260 on every vertex of every primitive: keep = 1 − w and rest = 1 − keep,
    each a float (0x855269..0x85528B), and each of the eight floats (position, texture coordinates, normal)
    keep × a + rest × b; at the game's 24-bit x87 precision each product and the sum round to a float
    (0x8552A6..0x855322). The normal is not made unit again. With the extra-metrics flag (0x100000) it also blends the
    mesh's 0x30-byte extra metrics as (b − a) × w + a (`fn_00679C30`, 0x807B11..0x807B55); the sixteen B_TEMPLE /
    B_FIRST_TEMPLE files' extra metrics are byte for byte the same, so that blend gives them back. The order
    (0x882C10..0x882CCF): 1 − ws (ws the lower size's weight) stored as a float (0x882C37), the heart's mesh =
    blend(B_TEMPLE<low><stage low>, B_TEMPLE<high><stage low>, 1 − ws); only when the stages differ,
    C = blend(B_TEMPLE<low><stage high>, B_TEMPLE<high><stage high>, 1 − ws) in place (0x882C91), then 1 − wa stored as
    a float (0x882CA5) and the heart's mesh = blend(itself, C, 1 − wa) (0x882CC1). Checked on 0x855260 with the oracle,
    run three times as the temple runs it (7 cases, bit for bit).
  - The bake (0x882CD4..0x882E15), every vertex of every primitive of every submesh, with the 3D object's matrix (rows
    at +0x14, +0x20 and +0x2C, position at +0x38): world x = ((z·[+0x2C] + y·[+0x20]) + x·[+0x14]) + [+0x38] and world
    z = ((x·[+0x1C] + z·[+0x34]) + y·[+0x28]) + [+0x40], the melting's order; H = `GetAltitude` 0x803090 at ftol(x ×
    65536 × 0.1) and ftol(z × 65536 × 0.1) ([0x8AC408], [0x8AC404]); and **v.y = v.y − ([+0x3C] − H)**
    (0x882DC3..0x882DD7), with no 1 / scale. It runs at the creation (from 0x8829AF) and at every blend (from
    0x462FE2).
  - `ComputeBoundingBox` 0x8081B0 (0x882E27); then last size = size and last alignment = alignment
    (0x882E2C..0x882E42).
  - The texture (0x882E48..0x883081), into the mesh's first material's texture (locked by 0x838AF0, unlocked by
    0x838EB0). Alignment above 0.5: Neutral → Good with weight ftol((a − 0.5) × 510) ([0x9377F4] 510.0); else Evil →
    Neutral with ftol(a × 510); clamped to 0..255. The images come from the name tables 0xC39B28 (evil), 0xC39BA8
    (neutral) and 0xC39C28 (good), 0x180 bytes a set, set = +0xA8 & 3, read by `fn_00822190`. Each 4444 texel: for
    each of the three colour nibbles ((A & m) × (255 − w) + (B & m) × w) / 255 (the multiply by 0x80808081), & m; the
    alpha nibble is the first image's. It returns 1.
- **The draw.** The finished temple uses the static draw 0x80DB30: no land deltas when it is drawn, as its mesh is
  baked. While it is being built, `Draw` 0x882A40 uses `DrawPartialyBuilt` 0x816AD0 on the same morphed mesh.

### The temple's doves or bats

`CitadelHeart::SetAlignmentFlock(alignment, changed)` 0x465270, called every turn at the end of `Citadel::Process`
with the player's `GetAlignmentValue` and the `IsChangedSize` result (ebx, 0x462FCD -> 0x463057).

- Only on a turn with `g_game+0x205A40 % 100 == 0` (0x465270..0x46528F); else nothing, not even the height.
- alignment < 0 (`fcom` with 0, so NaN counts as evil) -> kind 16 Bat and |a|; else kind 12 Dove
  (0x46529A..0x4652B1). The table rows are `Dove` and `Bat`, not `CitadelDove` / `CitadelBat`.
- count = ftol(`maxFlockCount` (heart info +0x13C, an integer loaded with `fild`) x |a|), at the game's 24-bit
  precision a float product, truncated (0x4652B6..0x4652CD); 0 while `GetPercentBuilt` (vt +0x880) < 1
  (0x4652DC..0x4652EF).
- No flock yet (+0x98 == 0): `fn_004651D0(&heart pos, GetPlayer())` makes `Flock(pos, GFlockInfo 0xC5E624, player,
  0xABA52)` (Flock::Flock 0x52F780) and sets +0x50 = 30 (domain radius) and +0x52 = 15 (flock distance)
  (0x4651D0..0x465212); flock +0x38 = the heart (0x465315). The heart starts without one: its constructor's
  `SetToZero` 0x464DA0 clears +0x98 (0x464DAE), and its creation makes none.
- Every member whose info is not the kind's row, from the head of the list (fn_00530CA0 / fn_00530CB0: the cursor
  +0x44 from +0x3C along each node's next): `RemoveLivingFromFlock(m, 0)` then `m->ToBeDeleted(0)`
  (0x46534A..0x46537E).
- Then count > members (+0x48): one more by `fn_00419D10(&heart pos, row, 0, flock, 0)` (an animal of that kind, no
  town, that flock, age 0); count < members: the head member (+0x3C -> +8) removed and `ToBeDeleted(0)`
  (0x465380..0x4653D5). Either way "changed" is set (0x4653D8), even when the creation failed.
- When the argument or "changed" is set: flock +0x54 (`Flock::altitude`) = (`GetScale` x the mesh's +0x28, its half
  height) x 2 + 10 (0x4653EB..0x465419). At 24 bits that equals `scale x full height + 10`: the doubling is exact.
- The heart's deletion: `CitadelHeart::ToBeDeleted` 0x464C50 calls 0x465230 (0x464D5A) after the entrance, the camera
  exclusion and the hearts' list, before `CitadelPart::ToBeDeleted` (0x464D62): flock +0x38 = 0, `fn_00530140` (every
  member from the head `ToBeDeleted(0)`, each leaving the flock as it goes), the flock's `ToBeDeleted(0)`, +0x98 = 0.
  Always 0, whatever the heart's own `now`.

### The temple's destruction

The script's call is in bw1-decomp's `src/Black/ScriptFunctions.cpp`.

#### Its two triggers

- `CitadelHeart::DestroyedByEffect` 0x468DA0 (vt +0x5F8) only calls `DestructionSequenceStart` 0x465AB0 and returns 1:
  the heart stays. Its callers are `Object::ApplyEffect` 0x637A8A (a life brought to 0 by any effect), the fire's
  `fn_0072EFB0` 0x72F510 and `fn_006C9FC0` 0x6CA01A (from `RenderParticleGameObject`'s destructor).
- `GScript::DeleteObject` 0x6F947A, OBJECT_DELETE mode 3 (`SCRIPT_DELETE_MODE_TEMPLE_EXPLODE`) on a citadel heart: a
  heart with +0xB8 set has its mesh exploded (`PSysGlobal::ExplodeObjectMesh(heart, its point, 80.0, 3.0, 0)`) and
  stays; any other gets `DestructionSequenceStart`. These are the only two callers of 0x465AB0 (xref).

#### The start

0x465AB0: +0xB8 = 1, +0xBC = 0, then every part of the heart's citadel (`GetCitadel` vt +0x114, its `PartList` +0x4C,
next +0x84) but the heart gets `ToBeDeleted(0)` (vt +0xC), from the head again after each: `CitadelPart::ToBeDeleted`
0x469540 takes the part out of the list. The list is newest first (the `CitadelPart` constructor 0x4693F0 puts a part
at its head).

#### The turn

`Citadel::Process` 0x462D70 ends with each part's `Process` (vt +0x5FC, 0x463073), the heart's `CitadelHeart::Process`
0x4665A0: fn_00465C70 (the destruction), fn_004688F0 and `ProcessTransferedDamageEffect` 0x468940 (the hurt heart).
fn_00465C70, in order:

1. The beams' source (+0xC0) and the glow (+0xCC) are let go once their container is no longer available (vt +0x2C);
   every turn.
2. Only with +0xB8 set and a citadel: clock += ms per turn (`fild`) x 0.001 [0x8AA3B0].
3. The heart's player (vt +0x1C) is the local player: the guidance's heartbeat override (fn_0071C3F0, guidance +0xB8)
   = the table 0x9CE910 drawn straight along the clock: points 0, 19, 25 s (0x9CE91C) with paces 0.63661975,
   1.2732395, 3.9788735 (0x9CE910); before the first and from the last on, their pace.
4. +0xA4 (the specular colour) = alpha 255 - 255 x clamp((clock - 14) / 4) << 24 | 0xFFFFFF.
5. The moments, each `fn_00466560(before, moment, after)` = before <= moment < after:
   - 14 s ([0xC5E244] = [0x8C83C4]): a shell, `LH3DObject::Create`, mesh 525 of the mesh pack, through fn_0083EFE0
     (+0xC4, +0xC8); 22 s ([0xC5E240] = 0.5 + 21.5): +0xC8 released (fn_0083F050);
   - 0 s: `SoundTag::Create(citadel +0x80, 171, 0, mode 2, loops -1, 0, 3D, InGame, 0)` and +0xD0 = clock;
   - while 0 <= clock < 14: as long as +0xD0 < clock, share = clamp(clock / 14), +0xD0 += (0.2 - 0.4) x share + 0.4;
     the source made if missing (`CreateSpotVisualWithSpecifiedDuration(heart point, 21 MAGIC_BEAM_ON_CITADEL, 1.0,
     ftol(turns a second x 14), 0)`, `SetPlayer`), two random points of the heart's model facing no more than 0.1 down
     (fn_00465C30 → fn_005906E0 on the 3D object's mesh, the local random), then with a source a plasma beam
     (`PSysBase::new(0x50)`, vtable 0x8C9B34) between them: life (0.7 - 3.0) x share + 3.0, speed (1.5 - 1.0) x share
     + 1.0, alpha ftol((200 - 50) x share + 50), leaving along the first normal and arriving against the second (x -1
     [0x8AB678]); fn_0063E3A0 hands it to the source's effect (vt +0x114);
   - 10 s: with a citadel, spot visual 19 MAGIC_FX_ON_CITADEL for 6 s (+0xCC), `SetPlayer`, and each part of the
     citadel given to its effect; then every turn while it is there and the heart is the local player's, fn_0063E3E0
     sets the container's +0x34, the power its effect is stepped with (`GParticleContainer::Process` 0x63E2C1 copies
     it into the process info +0x30), to (1 - 0.25) x guidance +0xAC (the heartbeat's pulse, fn_0071C450) + 0.25.
     openblack: `PowerGlowByHeartbeat` sets the spot visual's strength (`SetSpotVisualStrength`) to
     `temple_destruction::GlowStrength` of the pulse as the turn left it (`guidance::GetState().heartBeatPulse`, not
     the one drawn between turns), and the container gives it to its effect every turn (test
     `TempleDestruction.TheLocalPlayersGlowIsAsStrongAsTheirPulseFromAQuarterToWhole`);
   - 21.5 s: `SoundTag::Remove(citadel, 171, InGame)`, `SoundTag::Create(citadel, 168, 0, 2, 0, 0, 3D, InGame, 0)`,
     spot visual 32 EXPLOSION_CITADEL for 15 s with `SetPlayer`;
   - 22 s ([0xC5E24C]): spot visual 26 EVIL_SMOKE for ftol(turns a second x ((GameFloatRand(0.4) + 0.8) x 7.5))
     turns, no player, `SetScale(10)` (vt +0x124, the effect's magnitude);
   - 22 s ([0xC5E248]): +0xB8 = 0, +0xBC = 0; the local player's heartbeat override 0; `SetSpecularColor(0)`, +0xA4 =
     0xFFFFFFFF; the shell's object and its fade released; the heart's `ToBeDeleted(0)`.

   All of it at the game's 24-bit precision, one rounding a step.

The 21.8 s are two floats each set at start-up to 21.5 + 0.3 ([0xC5E254] by 0x465B30, [0xC5E250] by 0x465B60). Past
[0xC5E254], `CitadelHeart::DrawNow` (0x46726D) skips the rest of the heart's draw: its model (vt+0x108, 0x467288),
its fire effect (`Object::DrawFireEffect` 0x467297, only with a fire at +0x44), the help's view of it
(`HelpSystem::SendFOVObject` 0x4672E1), the collide-only object at +0xDC (`Game3DObject::AddJustForCollide`
0x46731B), the hand's pick of it (`GInterface::SendObjectDrawCollision` 0x467336), the leash posts (fn_00466730),
fn_00466BB0 and `CitadelHeart::DrawLantern`, so the entrance lanterns are no longer drawn (they are not deleted). The
colour and fade before the test (0x4670E3..0x46725B) and `DrawDestruction` (0x46725D) still run. [0xC5E250] is read
inside fn_00466BB0 (0x466D63), which draws the object at +0xA8, the temple's column of light ([the heart's
draw](#the-hearts-draw)), only while the clock is below it. The shell goes on past the test: `DrawDestruction` is called at 0x46725D, before it, and draws while the
shell's object (+0xC4) is there, with no test of the clock of its own (0x466DAF..0x466DB9), so from 21.8 s until the
shell's release at 22 s the shell is drawn over a heart that no longer is. Ours: from a little after the explosion
the heart is tagged `NotDrawn` by the destruction's turn (`temple_destruction::HeartDrawn`,
`TempleDestructionSystem::Step`), so its model is out of every pass and the cursor no longer finds it, until it goes
at 22 s; the heart restarted or ended is drawn again. Its shell goes on being drawn
([the shell over the heart](#openblack)). The entrance lanterns follow it (`temple_destruction::HeartDrawn` in
`CitadelLanterns`). 🔍 Still drawn past 21.8 s in ours: the leash posts and fn_00466BB0's object; a fire on the heart, the help's view and the +0xDC collide object have no counterpart drawn per heart in ours.

#### The heart's draw

Every citadel heart is drawn each frame by `fn_00467360` (0x467380), which walks the hearts' list ([0xC5E3D4], next
+0xD8) and calls `CitadelHeart::DrawNow` 0x4670D0 for each (`CitadelHeart::Draw`, vt +0x610 0x464B90, is an empty
`ret`). In order:

- **The colours.** `LH3DObject::CombineColorFromPos` 0x80BEC0 on the heart's 3D object (+0x40), with the diffuse
  edx = the heart's +0xA4 and the specular = `GetSpecularColor` (vt +0x5A4, 0x464B30: the heart's +0xA0): the land's
  light at the object's position into its +0x4C / +0x50 (`GetColorAndSpecular` 0x801C90), then
  `CombineColorWithCurrent` 0x80BF10: +0x4C = (land × diffuse) >> 8 in each of the four channels, +0x50 = land
  specular + specular, each channel held to 255; then the haze on +0x50 (`GetFogValue`). Oracle (0x80BF10): land
  0xFF808080 × 0xFFFFFFFF gives 0xFE7F7F7F; land 0xFFC08040, specular 0xFFF0F0F0, with 0xF1FFFFFF and 0x0A203040,
  gives 0xF0BF7F3F and 0xFFFFFFFF. The heart's constructor sets +0xA4 = 0xFFFFFFFF and +0xA0 = 0 (bw1-decomp
  `CitadelHeart.cpp`), so a whole heart is drawn in its land light times white (each channel × 255 / 256) and the
  land's own specular. Its fire does not change these (`DrawFireEffect` 0x467297 only adds the flames).
- [0xEB9960] would then draw each channel of +0x4C towards 255 by [0xEB9964]; nothing writes [0xEB9960] (xref: two
  readers, 0x4670F3 and 0x829610, no writer; it is in the zeroed part of .data), so it never does.
- vt +0x34 (`fn_0080B480` for a static object; bw1-decomp `CitadelHeart.cpp` `SetSnowlevel(matrix.GetPos())`)
  sets the object's snow amount +0x58 from the snow cover under it, at every draw [V]: c = the bilinear cover at the
  object's point ([0xEDC344], `fn_0086CB80`), `ftol(max(c − 20, 0))` ([0x8C7658] 20, `test ah, 1` takes 0 when below
  or unordered), × +0x60 `sar 8`, then at most +0x5C (0x80B4B3..0x80B4C6): `snow_cover::ObjectLevel`. It is set
  again from the cover each draw, so the fade's share below never builds up.
- **The fade** (0x4671D9..0x467258). A = the drawn colour's alpha (+0x4C >> 24): the land's alpha is whole (0xFF,
  `GetColorAndSpecular` 0x801F7B), so A = (0xFF × +0xA4's alpha) >> 8, 0xFE for a whole heart. When A ≤ 0xF0 and the
  heart is being destroyed (+0xB8): `GJUtils::SetMaterialProperties` 0x57E1D0 on its mesh (vt +0xF8) with {additive
  0, Z write 0, two-sided 0, change 1, alpha 1} (the bytes at 0x4671FF..0x467211): every primitive of the model in
  its mode without Z write, back faces culled, for good (the mesh is changed in place; again at each such draw, which
  changes nothing more); `SetGlobalAlpha(1)` (vt +0x48, bit 0x80; nothing clears it), so its draw at once takes the
  global alpha table at A ([the single transparent queue](../rendering/transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter):
  the static object's draw never queues by that bit); and its snow amount +0x58 = ((A − 0x80) × +0x58) >> 7 held to
  0..255 (vt +0x38, `fn_007F9790`, a plain store) [V]: p = `imul` of (A − 0x80) and +0x58 (0x46722E..0x467231); 0
  when `p & ~0x7F` ≤ 0 (p below 128, a negative one too), 255 when it is ≥ 0x7F80, else `p sar 7`
  (0x467235..0x467250). The static draw (0x80DB30) then shows the snow only when +0x58 ≥ 5 (0x80E423, `jl`) through
  the snow pass vt +0x124 (`fn_0080E940` in the citadel's table 0x9A2BFC, 0x80E443): alpha reference 255 − the
  level, and a depth test of `EQUAL` when the object's own draw left alpha blending on (0x80EBD0..0x80EBDA). +0xA4's alpha is 255 − 255 × clamp((clock − 14) / 4) (see [the turn](#the-turn)),
  so the heart turns see-through once its alpha is 0xF1 or less (A 0xF0), at 14.3 s with turns of 100 ms, and is
  unseen from 18 s.
- **Its specular colour** is written by `DrawDestruction` (below) after this draw's colours were taken, so each draw
  adds the specular colour the last one left.
- Then the shell ([the shell's draw](#the-shells-draw), `DrawDestruction` 0x46725D), and the heart's own draw, vt
  +0x108 (0x467288, the static object's `fn_008156B0`: at once, never through the transparent queue), skipped while
  it is being destroyed (+0xB8) with the clock past 21.8 s ([0xC5E254], 0x46726D..0x46727E: `fcomp`, `test ah, 0x41`,
  `je`, so at exactly 21.8 s it is still drawn); then its fire, the help's view, the hand's collision and the
  lanterns.
- **Which hearts, and in what order.** `GLandscape::Draw` calls `fn_00467360` (0x5E4EA0, stage 4k of
  [the frame](../rendering/frame.md), before the water rings and the models), which walks the hearts' list from its
  head [0xC5E3D4] along +0xD8 and calls `DrawNow` for every heart (0x467380), with no screen test and not through the
  object draw list (`CitadelHeart::Draw`, the list's vt +0x610, is an empty `ret`). A heart joins the list at its head
  when it is made (`CallVirtualFunctionsForCreation` 0x4677B5..0x4677C0) and leaves it in `ToBeDeleted`
  (0x464D1B..0x464D44), so the hearts, and their shells, are drawn newest first. Its draw runs at any alpha: from 18 s
  the heart is drawn at an alpha of 0, its shell before it.
- **The column of light** (fn_00466BB0, called from `DrawNow` 0x467344). Its object, +0xA8, is a static
  `LH3DObject` made in `CallVirtualFunctionsForCreation` (0x4677E1..0x46780C) with mesh [0xC02978] = 530 of the mesh
  pack (static data, no writer). Each draw rebuilds its whole matrix (0x466C55..0x466CA4, twelve dwords copied to
  +0x14): θ = atan2 of the eye minus the heart's point, + π/2 ([0x8C78D8]), rows (cos θ, 0, sin θ), (0, 3, 0),
  (−sin θ, 0, cos θ) and the heart's point. Then it writes 3.0 to the object's `scale` (+0x44, 0x466CAC). That field
  is not applied to the vertices: `LH3DObject::SetPosition` folds a scale into the matrix and only records it
  there, and `LH3DP3::CheckRegionOnScreen` takes the world centre from the matrix alone and the field only for its
  radius (box diagonal × scale). So the column is drawn as wide as its mesh (5.8) and three times as tall (2400),
  and its screen test is a sphere three times its box's diagonal. Its colour: the owner's `GetPlayerColour` with
  alpha ftol((clamp(beat, 0, 1) + 1) × 55) ([0x8C9D3C]), the beat from the owner's first interface status +0x30
  (`fn_0071C430`), set by vt +0x2C, then vt +0x48 (1) and `AddDrawing` (vt +0x100). A village centre's column
  (`TownCentre::DrawPSys`, its object at +0xC4) is built the same way (0x69BDF7..0x69BEC2: the same rows, `scale`
  3.0) at its model's box centre, with alpha 0x14. openblack: `light_column::Model` (`src/Graphics/LightColumn.cpp`),
  rotation and the upright stretch of three only (#203, 2026-10-22; before it, every axis was also multiplied by
  three, a column three times too wide).

#### The shell's draw

The shell made at 14 s is the scrolling glow shell of the creed glows (the same class and mesh 525, see
[the Creed glows](../creature/drawing.md#the-creed-glows)), made with `fn_0057D540(6)`: mode 13 with
`S_TileLandscape.raw`. `CitadelHeart::DrawNow` 0x4670D0 calls `CitadelHeart::DrawDestruction` 0x466D90 (0x46725D) after the
heart's colours and before the heart's own draw (vt +0x108, 0x467288; vt +0x38 at 0x467258 only sets the snow
amount +0x58), each frame the heart is drawn, also past 21.8 s when the heart itself no longer is:

- The pulse: `fn_0071C430` on the guidance of the heart's player's interface status (`GetTempFirstInterfaceStatus`
  +0x30): (+0xAC − +0xB0) × the turn fraction (g_game +0x205D64) + +0xB0, the heartbeat's pulse between turns.
- With the object (+0xC4): t = the clock +0xBC (`fn_00465B10`). s = A(t) × 0.3 [0x8C840C], A drawn straight between
  the moments {0, 1, 20, 22} [0x9CE948..0x9CE954] with the values {0, 1, 1, 2} [0x9CE938..0x9CE944]: t ≤ 0 (or not a
  number) gives the first, t ≥ 22 the last, else (t − Tᵢ) / (Tᵢ₊₁ − Tᵢ) × (Vᵢ₊₁ − Vᵢ) + Vᵢ for the first Tᵢ₊₁ above
  t, every step a float. The object's +0x44 = s and its matrix s on the diagonal, every other term 0, at the heart's
  3D object's position (+0x40 → +0x38): unturned.
- With the shell (+0xC8): `fn_0083F0E0(GetPlayerColour of the heart's player, 0x141414)`: the colour and a specular
  grey of 20; +0xC = 0.6 [0x8C8410] and +0x10 = 0.2 [0x8C8414] (the slide speeds), +0x14 = 0.7 [0x8C8418] and +0x18 =
  0.1 [0x8C841C] (the texture fractions); +8 (the zoom) = B(t) × the pulse, B the same moments with the values {0, 5,
  5, 10} [0x9CE928..0x9CE934]; then `fn_0083F270(g_game_time_inc × 0.001)` (game time: it stands still while paused)
  and `fn_0083F210` directly, so the shell is drawn at once, after the heart, outside the transparent queue and with
  no screen or distance test.
- The grey: `fn_0083F210` hands the shell's +0x28 to the object's vt +0x138 (0x80CE10), which stores it in
  `g_specular` [0xE9FE2C] (0x80CEEB). `LH3DPrimitive::DrawVolLightGJ` writes `g_specular` into each inner corner's
  specular (vertex +0x14, 0x84FAFC..0x84FB02 and 0x84FE5D..0x84FE62) and 0 into each outer corner's (0x84FBF3,
  0x84FBF6). The device's default states turn `D3DRENDERSTATE_SPECULARENABLE` on (`fn_0082C8F0` 0x82CC54, see
  [the frame](../rendering/frame.md)), so the vertices' specular is added after the texture. The heart's shell gets
  0x141414, an added grey of 20 at its inner edge that fades to none at its outer. The creed glows and the particle
  effects' shells give 0 (`fn_0083F070` and `fn_0083F0E0` with 0).
- Then, while +0xB8 and t > 3.5 [0x8C83CC] (`fcomp`, `test ah, 0x41`: not for t ≤ 3.5 or not a number), the
  heart's specular colour (vt +0x5A0, `CitadelHeart::SetSpecularColor` 0x464B20: +0xA0), 0x466FA0..0x4670BA: f =
  (t − 3.5) / 3 [0x8C83D8], held to 0..1 (f ≤ 0 or not a number gives 0, f ≥ 1 gives 1); v = `ftol`((f × the pulse)
  × (60 − 10) + 10) ([0x8C83D4], [0x8C83D0]), its low byte; then each of the heart's player's colour's red, green
  and blue (`GetPlayerColour`) × v >> 8, its alpha kept. So from 3.5 s it rises from 10/256 of the god's colour to
  10 + 50 × the pulse at 6.5 s. The turn sets it back to 0 at 22 s (0x466493). The heal chakra's two rules are the
  only other callers of vt +0x5A0 (0x6A0A2A, 0x6A0EFF).
- **Another player's pulse** is 0 (traced, xref): every player has its own `GInterface` and interface status
  (`GPlayer::Init`, bw1-decomp `Player.cpp`), each with its own guidance (+0x30), but `GInterfaceStatus::Process`
  0x5DC4E0 runs `ProcessHeartBeatSFX` 0x71C190 only for `MyInterface`'s status (0x5DC502..0x5DC50D), and the guidance's
  +0xAC / +0xB0 are written only by `GGuidance::Init` 0x71AC70 (0, 0x71AD8D / 0x71AD93) and the heartbeat's step
  `fn_0071C460` 0x71C532 / 0x71C546, called only from `ProcessHeartBeatSFX` (0x71C3C1). So another player's heart's
  shell reaches nowhere and its specular stays at 10/256 of its colour.

#### The game's end

`GGame::EndTurn` 0x54EA56..0x54EAE4: not over (+0x205A10), not a playground game (+0x205A0C), not multiplayer, the
local player processed this map (+0x205A14, set by `GPlayer::Process` 0x649515 for the local player before its
citadel's process, cleared with the map), the local player's citadel heart with +0xB8 set: +0x205A10 = 1 and
`StartScript("GameOver")` (single-player script types 0x7F, `GScript::StartScript` 0x6EB710).

#### The cycle

Land 2's `LeaveThroughVortexL2` deletes Lethys' citadel with the temple's explosion, once player 3 has no towns and the
citadel's life is at most 0.1 (`LandControl2`): the land cycle's 400 frames of Land 2 do not get there.

### The heart's blows: what it passes on and its hurt beams

No bodies in bw1-decomp.

#### The heart in the physics

`CitadelHeart::InteractsWithPhysicsObjects` 0x467B40: `GetPercentBuilt` (vt +0x880) > 0.1 (double [0x8C9D40]);
`ChecksVerticesVObjects` 0x467B30 = 0; `GetPhysicsConstantsType` 0x467B60 = 0; `SetUpPhysOb` 0x467B70:
`SetUpConstants(10000.0, the constants of type 0, 0)` and `BuildFromVertices`; it keeps `Object::GetWeight` (scale^3 x
the info's weight) as its weight as an object.

#### `CitadelHeart::ReactToPhysicsImpact`

0x467BB0 (vt +0x7AC, from `PhysicsObject::GameTurnUpdate` 0x64635D):

1. No body that struck it (the entry's +0x20): nothing (0x467BC2).
2. The thing that struck it (`GetGameObjectWhoHitMe` 0x644F00) and its hand's player (`GetInterfaceStatusWhoHitMe`
   0x644F10, vt +0x1C); breaks = the thing and its `PhysicallyDestroysAbodes` (vt +0x7A8).
3. target = `GetActualObjectToEffect(player, breaks)` (vt +0x5D8); a target other than the heart gets
   `ReactToPhysicsImpact(the heart's entry, 1)` (vt +0x7AC) and that is all: `Abode::ReactToPhysicsImpact` 0x406240
   reads the struck body from the entry it is given, so the building takes the blow as if struck itself;
   `Living::ReactToPhysicsImpact` 0x5ED3E0 with 1: a villager not in the physics (+0x24 & 0x40) gets
   `InitialisePhysics` (vt +0x784) with the velocity (0, 30, 0), the spin (1, 0, 1) and no thrower.
4. Else, a thing still there (vt +0x2C) that breaks buildings: heart +0x8C = the game turn (0x467CCF; its one
   reader in play is the computer players' `ReactToPlayerThrowingStuff` score 0x664FA0, see
   [the computer players' minds](../hand/computer-minds.md#playerinteract-and-reacttoplayer)), then `EffectValues(2 hit,
   harm, the hand's player, radius 1.0, 0)` (0x525040) and `ApplyEffect(values, 0)` (vt +0x5CC): harm = sqrt((z² + y²)
   + x²) of the body's velocity (+0x104) × its mass (+0x15C) × 0.000005 [0x8C8424], at most 0.2 [0x8AB244]; a float a
   step. A life brought to 0 is `DestroyedByEffect`: the destruction.

#### `CitadelHeart::GetActualObjectToEffect`

0x468C30 (player, beams): over the heart's citadel's player's towns (`GPlayer::GetNextTown` 0x64D260), each town's
buildings (+0x754, next +0x9C): the first available (vt +0x2C), with life (vt +0x11C) > 0, built (vt +0x880) > 0, not a
field (vt +0x210) nor a football pitch (vt +0x464), with life > 0.25 [0x8AB3D4] is taken at once; else after every town
the first such building; else the first available homeless villager of any town (+0x768, next +0xE4;
`Villager::IsAvailable` 0x751D50: not dying); else the heart. With beams, a building or villager gets fn_004686B0, the
heart fn_004684F0:

- fn_004686B0 (the target's beam): a new target forgets the last turn; due once the turn is more than ftol(turns a
  second × 2.0 [0x8C842C]) after it (unsigned); then +0xB4 = the turn, the source made if missing (fn_004684A0: spot
  visual 21 for ever, `SetPlayer`), and with a source the spark sound (206 + the counter [0xC5E3E4] of 5, the options at
  0x468815: in-game bank, 3D, not tracked, owned by the heart, at its point) and a beam from the heart's point raised by
  its height (vt +0x42C) to the target's so raised, tangents (0, 1, 0) and (0, -1, 0), life 2.0, speed 1.5 [0x8C8430],
  alpha 70 [0x8C8434];
- fn_004684F0 (its own): forgets the target, the same wait, then to a random upward point of its model, arriving
  against its normal; no sound. The point is drawn even without a source.

Other callers of `GetActualObjectToEffect` are the fire (0x730689) and the lightning (0x67EB67), and the worship
site's 0x4697A6; a worship site struck (`CitadelPart::ReactToPhysicsImpact` 0x4697F0) passes the blow to the heart.
`CitadelPart::ReactToPhysicsImpact` 0x4697F0 (the worship site's, its altar's and the creature pen's, vt +0x7AC): with a
citadel whose heart (+0x30) is set, it calls the heart's `ReactToPhysicsImpact` 0x467BB0 with the same physics object
and true (which the heart's does not read), and takes nothing itself; else `Object::ReactToPhysicsImpact`. openblack:
`PhysicsGameHooks::ReactToImpact` hands a worship site's or its altar's blow to its temple's heart
(`worship::citadel::PartHeart`, through the physics hooks' `CitadelPartHeart`), which passes it on or takes it as a
blow of its own (`StrikeHeart`); test `CitadelPlanTest.AWorshipSiteAndItsAltarHandTheirBlowsToTheirTemplesHeart`. The
creature pen has no openblack form.

#### `CitadelHeart::Process`

0x4665A0, after the destruction: fn_004688F0 lets go of the source once it is no longer available and of the target
once it is no longer available (+0xB0 = +0xB4 = 0); then `ProcessTransferedDamageEffect` 0x468940: life ≥ 1 nothing; ≤
0 taken as 0; only when `GetActualObjectToEffect(the heart's player, 0)` is the heart; due when the turn is more than
ftol(((3.0 - 0.4) × life + 0.4) × turns a second) after +0xB4, which takes the turn; the source made if missing; two
random upward points; with a source, at life ≤ 0.6 [0x8C7BDC] the spark sound (206 + the counter [0xC5E3E8] of 5) and a
beam between them: life the wait, speed 1.5, alpha ftol((20 - 100) × life + 100).

Heart +0x8C (the turn of the last blow it took itself): in the heart's own functions (0x464000..0x46A000) it is only
written (0x464A58, 0x464DA8, 0x467CCF), saved and loaded; a reader elsewhere was not looked for.

## openblack

Status: **done** for the slots, the outside, the flock, the destruction's timeline and the blows; **partial** for the
destruction's and the blows' drawing (the shell and the heart's colours and fade are drawn; the beams and the glow's
power are kept as state, not drawn). Port decisions (his temple destruction, taken with three changes): [port decisions: magic](../project/port-decisions-magic.md).

- **The slots** (`Worship/Citadel.cpp`): `CitadelWorship` goes in the temple's entity (`components::Temple`; a part's
  `citadel` field is the original's `CitadelPart::citadel` +0x80, `GetCitadel` 0x464A80), which
  openblack creates in `CitadelArchetype`; six slots (`sites[6]`); `site::Create`. `PostLoadCleanup` runs in `Game::LoadMap`
  right after the land's script and its town features, as the original's, and works out each such player's influence
  power there too ([influence](influence-and-alignment.md#where-the-code-is)). The audio's readers are `citadel::WorshipSitesOf`, `site::DancerCount`, `site::CalculateDesireForFood` and
  `citadel::StrainSoundFractionAtMostOne`. The dancers are summed group by group of the site's dance, as the
  original's. Save games are not ported. The
  interface citadel is reset by `worship::citadel::ResetInterfaceCitadel` in `Game::ResetScripts`, only when the scripts
  restart (a new game, a playground's start); a land change keeps it, as the original's (2026-10-22, #206; from
  57e7572d to then every land load reset it, on the unchecked belief that the original's land load restarts the
  scripts).
- **The scripts' temple position and entrance** (`GetTemplePosition`, `GetTempleEntrancePosition` in `CHLApi.cpp`):
  the player's citadel is `worship::citadel::Of`, its place `map_coords::ToWorld` of its MapCoords
  (`ecs::object::MapCoordsOf`), the pen point the heart's special point 15 read as the leash posts read theirs
  (`worship::temple_leash::SpecialPointOnLand`), and the entrance `worship::citadel::EntrancePoint`, the original's
  float steps (`test/test_script_query_natives.cpp`: `GetTemple*`, `TempleEntrance.*`).
- **The outside** (faithful, except where marked: `ECS/Systems/Implementations/TempleExteriorSystem.cpp`,
  `3D/TempleExteriorMorph.cpp`, `components::TempleExterior`): `CitadelArchetype::CreateHeart` calls
  `TempleExteriorSystem::Create` after the flattening (the start and the first blend), and `citadel::Process` takes the
  targets, steps them (`Step`) and, when `IsChangedSize` says so, takes the heart out of the map cells around `Blend`.
  The meshes and the images are read once through the byte cache; the first temple's file gives the own mesh's shape,
  skins and footprint. The own mesh is `temple/exterior/<the entity's index>` in the mesh cache, with skins that can
  take new texels. The bake is `land_morph::BakeAgainstY`. After a blend the partly built draw is cut again from the
  new mesh. `MorphWithTerrain` stays off. In openblack the heart's map cells also come from the blended box
  (⚠️ approximate: the original's own collide shape for the heart is not ported).
  - The blend (2026-10-22): `TempleExteriorMorph::PlaceOf`, `Mix` and `BlendVertices` blend two meshes at a time in
    the original's order and rounding, every step its own float; test
    `TempleExteriorMorph.TheNestedBlendMatchesTheOriginalsBits` (the oracle's bits). Every mesh of the place is read, as
    the original reads all four (the upper stage's two only when the stages differ). The extra metrics stay the first
    temple's, the same with the shipped files.
  - **Approximate**: the influence powers are the last turn's: openblack works them all out after the citadels; the
    original works out each player's after its own citadel.
- **The flock**: `ProcessAlignmentFlock` in `Worship/Citadel.cpp`, the rules in `Animals/BirdRules` (raffclar's
  names), the flock on `components::CitadelHeart::flock` (the heart's +0x98); the turn gate is `game_clock::Turn()`;
  the new member is `AnimalArchetype::Create(pos, kind, no town, flock, age 0)`; the head member is openblack's
  `members.back()`. The heart's deletion is `CitadelHeartToBeDeleted` in `ECS/Abodes.cpp`.
- **The destruction**: `TempleDestructionSystem` (raffclar's names, his pure timeline `Temple/TempleDestruction` and
  model points `3D/ModelSurface`), its game world `GameTempleDestructionWorld`, the state on `components::Temple`
  (`destroying` = heart +0xB8, `destructionClock` +0xBC, the beams' source +0xC0, the glow +0xCC, the beams' clock
  +0xD0). The triggers: `DestroyedByEffect` in `ECS/Effects/EffectValues.cpp` and `fire::traits::DestroyedByEffect`;
  the `ScriptDelete::CitadelHeart` case of `ObjectDelete` (`CHLApi.cpp`), `TempleDestructionSystem::DeleteByScript`;
  the explosion through `particles::explode_object::QueueObject`. openblack's parts are the citadel's worship sites (by
  their creation index) and the heart. openblack calls `ProcessHeart` at the end of `worship::citadel::Process`. The
  heartbeat override is `audio::guidance::SetHeartBeatOverride`, its pace `temple_destruction::HeartbeatPace`; the
  float steps are in `TempleDestruction.cpp`. The game's end: the `gameOver` flag in the registry's context (cleared
  with the map), the call after `audio::ProcessTurn` in the game's turn; the players always take their turn before the
  end of it, so +0x205A14 is always set by then. openblack plays no playground game, so that test is always false.
- **The blows**: `StrikeHeart` in `ECS/PhysicsGameHooks.cpp` (where raffclar has his),
  `TempleDestructionSystem::ActualObjectToEffect` and its hurt heart's turn, the pure rules in `Physics/TempleHeart`
  (his names: `Choose`, `Harm`, the beams' waits, his master's damage rules), the state on `components::Temple`
  (`beamSource` = heart +0xAC, `beamTarget` +0xB0, `beamTurn` +0xB4). The heart in the physics:
  `k_NoObjectCollision` (as a building's), `DynamicsSystem::InteractsWithPhysicsObjects`, `ConstantsType` and
  `AddProxy`, and the body's mass in the class hook `PhysicsGameHooks::BodyMassOf`; the body is made from the heart's
  drawn mesh, as a building's. The outcome is `temple_heart::Outcome`; the building's part is
  `BuildingDamageSystem::ReactToImpact` with the heart's entry; the villager is flung by `AddObject` with the spin in
  the body's axes. The heart that ends its destruction this turn goes after the hurt heart's turn (openblack defers its
  `ToBeDeleted` to the end of its turn: the original's heart is only marked by then). Heart +0x8C is
  `components::CitadelHeart::breakingBlowTurn`, set before the harm by `temple_heart::BreakingBlowTurn` (test
  `TempleHeartStrike.TheHeartKeepsTheTurnABlowThatBreaksBuildingsHarmsIt`); openblack saves no heart, so it is not
  saved.
- openblack differs: the heart and the worship site take one child radius from the mesh box (`map_collide::Shape`
  and `map_collide::FromMesh`, `ECS/MapCollide.h`) instead of [their own circles](#the-hearts-and-the-worship-sites-collide-shapes),
  a fix row.
- Not ported: the resize of the temple's camera exclusion (`CameraExclusion::Adjust` 0x454AA0); openblack's
  exclusions are loaded data only (`camera_zones::Exclusion` in `Camera/CameraZones.h`, read from the file in `CameraZoneSystem.cpp`), a port row.

- **The heart's colours** ([the heart's draw](#the-hearts-draw)): `RenderingSystem::WriteEntityRow` gives the row of
  a heart (an entity with `components::Temple`) its own pair, whatever burns on it: its land light times
  `heart_fade::Tint` (white with the heart's alpha, `src/Temple/HeartFade.h`) and the land's specular plus its own.
  The pair is in the heart's row, so the hearts' pass draws it with the model (`drawHeartModel` in
  `Renderer::DrawPass`). As the original takes the pair (`CombineColorFromPos` 0x4670EE) before its shell's draw
  writes the specular the next draw adds, the row takes the specular the last frame left (`_heartSpecularsDrawn`) and
  the frame then works out the next (`_heartSpeculars`). Test: `HeartFade.*` in `test_temple_destruction`.
- **The heart's fade** ([the heart's draw](#the-hearts-draw)): `GameTempleDestructionWorld::FadeHeart` keeps the turn's
  alpha in `components::Temple::fadeAlpha`, which the tint takes. The first time `heart_fade::TurnsSeeThrough` (the
  drawn alpha at most 0xF0 while it is destroyed) it changes the heart's mesh's materials
  (`heart_fade::k_SeeThroughMaterial`, `L3DMesh::SetMaterialProperties`) and gives the heart `components::Alpha` at
  the drawn alpha, so it is drawn at once through the global alpha table; from then on each fade only moves that
  alpha. Its specular colour: `RenderingSystem::BeginPrepareDraw` gives each row of a heart being destroyed the
  specular the last frame left (`_heartSpecularsDrawn`), then this frame's (`heart_fade::Specular` of the clock, the
  owner's pulse at the turn fraction and `surf_revol::PlayerColour`, at the owner's colour index on the land) for the next; a heart
  no longer being destroyed has none. Tests: `HeartFade.*`. 🔍 inferred a frame that writes no rows (nothing to draw
  changed) does not step the specular, where the original's draw would; the pulse and the clock only move while the
  game runs. The snow's share of the fade: while `heart_fade::TurnsSeeThrough`, the heart's row carries its drawn
  alpha (`argb_colour::PackInstanceSnowAlpha`, below zero in the window's float), and `vs_object` takes the snow
  under it (`SnowObjectLevel`) through `SnowLevelSeeThroughHeart` before the shown test of 5 (`SnowShown`), as
  `heart_fade::SnowLevel`; test `HeartFade.TheSnowOnASeeThroughHeartThinsWithItsAlpha`; the Temple window's alignment
  part shows the snow on the heart and what is drawn of it. Render only: no state moves. 🔍 inferred it shows only on
  the heart's primitives that still write depth after the see-through change (`partial_build_cap::TakesSnow`), where
  the original's snow pass follows the depth test the last drawn primitive left (`EQUAL` after a blended one); the
  heart mesh's primitive modes were not listed, nor was the cover's rate +0x60 / cap +0x5C of the citadel's 3D
  object read (255 and 255 taken, as for every object). The material change is made on
  the heart's `Mesh`, not a partly built model's (`DrawMesh`). 🔍 inferred with `components::Alpha` the heart casts no baked shadow
  (the rule of every fading object), where the original's baked shadow was not checked for a fading heart.
- **The hearts' pass** ([the heart's draw](#the-hearts-draw), which hearts and in what order): in the world view,
  `Renderer::DrawPass` draws the citadel hearts in a pass of their own before the water rings and the models, newest
  first as the hearts' list (`heart_shell::DrawOrder` of `components::Temple::creationStamp`, given as the heart is
  made; test `HeartShell.TheHeartsAndTheirShellsAreDrawnNewestFirst`), each heart's shell then its model
  (`heart_shell::Pass`; test `HeartShell.EachHeartIsDrawnRightAfterItsOwnShellNewestFirst`), so that with two hearts
  destroyed at once each shell comes after the other heart, not before it. The heart's model is drawn by the models'
  own draw code, so in the same way, with the same row, programs and colours as among the models: at once in its
  range's way (or queued, by its mesh's Z-sort flag), a fading one through the alternate mode table, then the shadows
  over it and its read-out row; the models' draw leaves the hearts' rows out. Not in the pass yet: the heart's
  fire, its lanterns and leash posts, which are drawn where they were; the reflection view, which draws the hearts
  among its models; and 🔍 inferred the object draw list still decides whether a heart's model is drawn, where the
  original draws every heart, which differs only for a heart the list leaves out (off the screen).
- **The shell over the heart** ([the shell's draw](#the-shells-draw)): `GameTempleDestructionWorld::ShowShell` keeps
  `components::Temple::shellShown` (from 14 s until 22 s, as the timeline shows and takes it away). Each frame
  `Renderer::CollectHeartShells` (`RendererParticles.cpp`) takes every shown heart that has an instance this frame,
  whether the object draw list reaches it, it is off the screen or faded out, at its row's point; and, from a little
  after the explosion, when the heart itself is no longer drawn and so has no instance, at the heart's drawn point
  (`heart_shell::PlacementOf`, `ecs::DrawnPosition`; test `HeartShell.OnceTheHeartIsNoLongerDrawnItsShellIsDrawnAlone`),
  until the shell goes at 22 s. `Renderer::DrawPass` draws them in the hearts' own pass (below), each right before
  its heart.
  `DrawHeartShell` slides its texture by the frame's game ms (`glow_shell::Slide`). It then draws the creed model's
  glowing shell (`DrawGlowShellAt`, the artefact aura's `VolumeGlowShell`) at once. The model is
  `heart_shell::Model` at the heart's drawn point, sized `heart_shell::Size`. The shell reaches `heart_shell::Reach`
  × the model's half diagonal out. Its pulse is `HeartbeatSystemInterface::Pulse` of the owner, which is 0 for
  any player but the local one, as the original's ([another player's pulse](#the-shells-draw)). The
  shell is drawn in the owner's colour (`surf_revol::PlayerColour`, at its colour index on the land) with
  `S_TileLandscape`, texture fractions 0.7 and 0.1. Its slide starts at 0 each time the shell is shown. The grey
  `heart_shell::k_Added` is added after the texture at the inner edge, fading to none at the outer: the shell's
  programs (`vs_volume_glow`, `fs_particle`) take it as `u_glowAdded`, which only this shell sets and which is set
  back to 0 after it. Tests: `HeartShell.*` in `test_temple_destruction`.

### Test hooks

- None of the page's own; the destruction and the blows are covered by the tests of `Temple/TempleDestruction` and
  `Physics/TempleHeart`.

## Pending

- **The temple's outside**: the resize of the temple's camera exclusion (`CameraExclusion::Adjust` 0x454AA0) is not
  ported, a port row. Open: whether `DrawPartialyBuilt` 0x816AD0 also adds the `UpdateMelting` deltas
  while the temple is built, which would count the land offset twice over the baked mesh; the player's alignment on
  the first turn of each land.
- **The heart's fade**: the snow's share is not ported; the snow map exists (`Locator::snowSystem`) but the fade does
  not read it ([the heart's fade](#openblack)).
- **The temple's destruction**:
  - the plasma beams are handed to `ParticleSystemInterface::AddPlasma`, which draws nothing yet;
  - the random points are drawn on the heart's drawn model (its blended outside); the original's is the mesh of the
    citadel's 3D object (`LH3DObject` vt +0xF8): not checked that the two are the same;
  - a citadel with a second heart: only the citadel's heart takes its turn; `RenderParticleGameObject`'s
    `DestroyedByEffect` call (0x6CA01A) is not ported.
- **The heart's blows**: the beams at what takes a blow and the hurt beams are handed to `AddPlasma`, which draws
  nothing; the other effects the heart passes on (`GetActualObjectToEffect`, from the fire 0x730689 and the lightning 0x67EB67,
  the worship site's 0x4697A6) are not wired; a town's spell dispenser is not among its buildings in openblack.
- openblack differs: the heart and the worship site have one child radius from the mesh box (`map_collide::Shape`, `ECS/MapCollide.h`),
  a fix row.
