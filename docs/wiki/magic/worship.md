# Worship

## Summary

Miracles come from worship. Each town holds some kinds of magic, and its village centre shows one icon for each
miracle it holds. The player's temple has one worship site per tribe, with an icon for every miracle that its towns
hold. When the village totem is raised, villagers walk to the site and dance there. Their dance fills the site's
battery of prayer power, and each dancer pays for it with a little of its life. Tapping an icon starts it charging
from that power; when it is full, the miracle's seed appears in the hand, ready to cast. Separately, dispensers make
one-off miracle orbs every few minutes, and on Land 1 fireflies sleeping on objects give a random orb when the object
is picked up.

openblack ports the sites, their icons and seeds, the battery, the charge, the worship percentage and the villagers'
worship states, the dispensers and the fireflies. The sites are born built, the dance files' real dance is not used at
the site, and the artifacts and the supplies are not ported.

## The original

The addresses below are those of the v1.42 exe, read with the 1.20 symbols.

<a id="worship-where-miracles-come-from"></a>
### How worship fits together

The original's chain is: a **town** stores magic types → its **village centre** shows one icon per seed → the player's
**citadel** has one **worship site** per tribe, with one icon per seed → the **villagers** dance there and fill its
**battery** of prayer power → when an icon is tapped it **charges** and the seed appears in the hand. Separately there
are the one-off miracle **dispensers** and the **fireflies**. The temple and its six slots are on
[the temple and the citadel](temple-and-citadel.md#the-citadel-and-its-six-slots).

The code lives in these files:

| Name | Address | What |
|---|---|---|
| `Worship.cpp` | 0x77A960..0x77E870 | the worship site: the dance ground of one tribe, its battery and its icons |
| `WorshipSpellIcon.cpp` | 0x77F140..0x780640 | charging from the site's prayer power, the seed into the hand, cancelling, the chant store |
| `SpellIcon.cpp` | 0x725FF0..0x726D30 | the icon base the site's and the town centre's icons share |
| `TownCentre.cpp`, `TownSpellIcon.cpp` | 0x743F60..0x744320, 0x748A70..0x748F70 | the town centre's icons, at most 6 |
| `SpellDispenser.cpp` | 0x7226C0..0x723070 | the miracle dispensers |
| `FireFly.cpp` | 0x52B5A0..0x52B790 | the firefly rewards |

<a id="structure-and-data"></a>
### The data and the site object

#### The tribe and icon tables

- `GWorshipSiteInfo[9]`, one per tribe (`GTribeInfo.worshipSiteInfo`): `chantsPerVillager` 3 (Celtic 4, Tibetan 5),
  `maxDancersVisible` 20, `chantsToFillBattery` 9000, `eachVillagerAddToFillBattery` 300, `prayerSiteDistance` 44,
  `radiusFromCitadel` 37.5, `artifactPowerupMultiplier` 1e-5, and the altar mesh per tribe (101
  `BuildingCitadelNorseAltar`, 93 Indian, 94 Aztec, 95 Celtic, 98 African/Egyptian, 99 Greek, 100 Japanese, 103
  Tibetan). The altar mesh is `GWorshipSiteInfo` +0x124 (file +0x114 `meshType`), read by `WorshipTotem::GetMesh`
  0x780A70.
  - **Bug kept:** `chantsToReserveForMaintaining` is in the file as the integer **500** and the executable reads it
    with `fld` (`fn_0077A950`), so it is ~7e-43 ≈ 0: the reserve for maintaining spells does not exist in practice.
- `GSpellIconInfo[2]` (`GSpellIconInfo`): [0] "Spell Icon" (worship site, at 0xD9D3E8), [1] "TownSpell Icon" (village
  centre, at 0xD9D514). Both use mesh **203** `BuildingVillageCentreSpellHand` and `gatheringChantAddPerGameTurn` 61.
- Charge cost = `GMagicEffectInfo.costToCreate` (FIRE 3500 / PU1 7000 / PU2 10000, LIGHTNING 5000/7500/10000, HEAL
  6000/9000, FOOD 7000/10000, WOOD 7000, WATER 5000/7000, NATURE 13000 (its `initialChants` is 10000), LIGHTNING BEAM
  16000/32000/60000...).
- The town belief constants: `GBeliefInfo` 0xC58640 +0x14..+0x24, with GPlayerInfo +0x48 and GTownInfo +0xB8.

#### Special points

The special points are the L3D's extra metrics; `Game3DObject::GetSpecialPos` 0x63B040 / 0x63B0B0 gives the metric's
matrix times the object's.

| Mesh | Points |
|---|---|
| the worship site's `b_worship.l3d` | **16**: 7 hiding place, 8 dance centre and totem, 9 arrival, **10..15 the six icon slots** |
| the village centre (e.g. 179 `BuildingNorseVillageCentre`) | **14**: **0..5 the six icon slots** (all at y = 2.781, in a ring) and 6 the totem (y = 4.613, in the centre) |
| the icon, mesh 203 | 1: the point where the `SpellSeedGraphic` floats (+1 in y; fn_00726140 0x726140, where `SpellIcon::Create3DSpellObject` puts the graphic) |

`fn_0077CE70` = point 7 (where the villagers beyond `maxDancersVisible` wait); `GetArrivePos` 0x77CED0 = point 9;
`WorshipSite::GetSpecialPos` 0x77CC90 (world point).

#### The site's mesh and texture

- `B_WORSHIP.l3d` (Data\Citadel\OutsideMeshes) is loaded by `fn_008829C0` into the heart's 3D object +0xAC. The site's
  `CallVirtualFunctionsForCreation` 0x77B9D0 gives it to the site (vt 0xF4). One mesh per heart 3D object, shared by
  its sites (`CallVirtualFunctionsForCreation` 0x77B9D0 reads the citadel's heart (+0x30), its 3D object (+0x40) and
  its +0xAC at 0x77BA08..0x77BA14).
- **The site's texture** (✅ faithful). `B_WORSHIP.l3d` carries no skin (skin count 0); the three `Textured` materials
  of its drawn sub-mesh name skin **1**, and the `Smooth` one of its physics sub-mesh none (0xFFFFFFFF). `fn_008829C0`
  is called once, from `InitTemple` (0x8829A6, just before the first `ProcessAlignement` at 0x8829AF). It loads the
  file with `CreateFromHD(path, 1)` (0x8829DB..0x8829E2, "don't care about textures") when +0xAC is still null, takes
  the heart's mesh (`ResolveLoad` vt+0xF8, the morphed one) and its first skin (`LH3DMesh` +0x3C `skins`, element 0:
  0x8829C6..0x8829D7), and writes that texture into `LH3DMaterial::texture` (+8) of **every primitive of every
  sub-mesh** (0x8829F0..0x882A2C), the physics one included. So the sites wear the temple's own texture,
  `B_FIRST_TEMPLE`'s skin 0xFC0B7D34, which the alignment blend rewrites in place (see
  [the temple's outside](temple-and-citadel.md#the-temples-outside-alignment-and-size)): a site follows its temple
  from evil to good, and each player's set.
- The site's own collide shape is `CreateCollideData` 0x77E490.

#### The site object, its lists and its links

- The site object: a `CitadelPart` of 0x128 bytes (ctor `fn_0077A960`; its chant fields zeroed by `fn_0077AE60`). Its
  citadel is `CitadelPart` +0x80 (vt 0x114 `GetCitadel` 0x464A80). +0x108 is the infinite-chants cheat
  (`fn_0077BC70`). +0x90 is written only by the constructor (0x77A997; its meaning is not settled).
- Its altar: `WorshipTotem::Create` 0x780930 (ctor 0x780840) places the tribe's altar at special point 8, at the
  site's angle and scale.
- The villagers at the site: +0xD4 / +0xD8 list them, newest first (`fn_0077D040` adds, `fn_0077D110` finds one);
  `WorshipSite::RemoveVillagerFromWorshipCount` 0x77D0A0 takes one off the list (if it is there) and the count +0xC8
  one down, when there is a count.
- The go-home queue, +0x120 / +0x124: `fn_0077E0C0` puts a villager in, sorted by its desire for life (the highest
  first), sets its +0x118 and sends another villager (`AdjustWorshipersWorshipping(1, 0, 1)`);
  `WorshipSite::RemoveVillagerRequestingToGoHome` 0x77E1D0 takes it out; `fn_0077E0A0` tells whether a villager is
  first in it (an empty queue counts as first). `GetNumVillagersRequestingToGoHome` 0x77E260 counts it.
- The towns: `Town::SetWorshipSite` 0x73D030. `WorshipSite::AddTown` 0x77C800 also makes the town's footpath link to
  the site (`fn_007412A0`, `GFootpathLink::GetNearestPathToQuick`). `fn_0077C950` removes a town.
  `Villager::GetWorshipSite` 0x76C340 is its town's site; the villager's worship flags are Villager +0xE0.
- The icons: `GetSpellIconFromSeedType` 0x77B170 / `GetSpellIconFromMagicType` 0x77B1B0.
- The dance: a villager joins it in `StartWorshippingAtWorshipSite` 0x76C4C0 (GroupBehaviour, Dance +0x90).
  `Dance::SetState` 0x50BAF0. The real **dance** comes from its `.DAN` (`GDanceInfo[19 + slot]`,
  `GroupBehaviour::CalculateDancePosition` 0x597F20). The dance's shapes, places and moves are read in
  [the dances](../beings/dances.md).
- Finding a site from a point: `MapCoords::FindWorshipSite` 0x602460 (off the map: null) does a single
  `FindTypeOnMap(8 CITADEL, 0)` on the cell's fixed list. A WorshipSite answers itself, a WorshipSpellIcon answers its
  site (vt +0x30C), and anything else (the heart, a town centre icon) gives null.
- `Citadel::FindOrCreateWorshipSite` 0x463220 is called from `CreateBuiltWorshipSite` 0x465123.

### The battery and the site's turn

#### Each turn

`WorshipSite::ProcessSpellIcons` 0x77B4D0, once per turn from `Citadel::ProcessSpellIcons` 0x463920 (which comes from
`GPlayer::ProcessSpellIcons` 0x64AEE0, inside `Spell::ProcessSpells`):

1. **Strain** (+0x114) = `(requested − capacity) / capacity`, with capacity = `N × chantsPerVillager × tribal
   power[2]` (`fn_0077E060`). Without capacity, 1 if something was requested and 0 if not.
2. If the strain is **not** positive, the icons that are being charged share out what is left over: `min(available,
   needed) / how many` to each one (`fn_0077CBC0` subtracts the maintenance reserve, ~0 because of the bug above). What
   each icon accepts is charged to the site.
3. `WorshipSpellIcon::Process` of each icon.
4. **End of turn** `fn_0077B6A0`: `k = min(1, used/capacity + boost)` with `boost = max(0.2, 0.5 − battery/max × 0.5)`
   (0 if it comes out ≤ 0); produced = `capacity × k`; `chantDamage` = produced / N (what it costs each dancer in
   life); `battery -= used − produced` (never below 0); `available = battery + capacity`. That `k` is also the
   intensity of the dance (`fn_0077B8D0` → `fn_0050C340`).
   - Every 1000 turns the site's artifacts (+0xAC) get [0xD1A298] × artifactPowerupMultiplier × N / the player's
     `fn_0064D0E0` (`fn_00426A80`).

#### Using, measuring and setting the chants

- `UseChants` 0x77BBB0 records what was requested, charges at most what is available and adds to the player's
  statistic. `MaintainSpell` 0x77BC50 and `fn_0077CC50` are the variants for the cheats (infinite chants, free
  maintenance).
- `MaxBattery` (`fn_0077E780`) = `chantsToFillBattery + N × eachVillagerAddToFillBattery` (9000 without dancers).
- Available `fn_0077CC10` = 1e6 with the infinite cheat, otherwise available − used;
  `WorshipSite::GetTotalChantsAvailable` 0x77CC30 = 1e6 with the cheat, otherwise +0xF8.
- `GAME_SET_MANA` 0x6FE800: `fn_0077B060` empties every icon's store, then the battery (+0xF0) is set.
- **Visual strain** `fn_0077B3B0` (per frame, through `fn_00463980`): `phase = fmod(phase + (5 +
  5·clamp(strain,0.1))·dt, 2π)`, `pulse = (cos phase + 1)/2`.

#### The food pot and what a site takes

- `WorshipSite::CreateFoodPot` 0x77AD90: pot info 2 (StoragePitFoodPile, 0xD4C8E8) at `GetFoodPosAndYAngle` 0x77CDB0 =
  the site's local point (9, 0, −38), at the site's angle + 1.5 (0x99C45C), scale 0.7 (0x99C460).
- `WorshipSite::GetResource` 0x77BD80 (vt +0x98) gives 0 when the site has no pot.
- `Dance::CalculateFoodNeededByDancers` adds nothing for members that are not villagers (dynamic_cast 0x50BF51).
  `foodReqiredForDinner` is read as an integer (`fimul`).
- `WorshipSite::DeleteObjectAndTakeResource` 0x77E7B0 (vt +0x684, returns 1, makes no reaction, unlike the storage
  pit). If the local hand threw the object (Object +0x24 & 0x40, `SearchForPhysicsObject` 0x646950, its +0x24 ==
  MyInterfaceStatus): `HelpProfile::Trigger(6)` 0x5C46E0. Then `DoDeleteObjectAndTakeResource` 0x63A940 →
  `WorshipSite::AddResource` 0x77C5F0 (type, n, IS, poisoned, pos, k):
  - with a building site (+0x74) and WOOD (1) or ANY (−2), every argument goes to the site's `AddResource` (vt +0x9C,
    0x77C5FE..0x77C625);
  - else FOOD (0) only: no food pot (+0xB4) → `CreateFoodPot` 0x77AD90 (0x77C638), then the pot's
    `JustAddResource(FOOD, n, poisoned)` (vt +0x8C, 0x77C63D..0x77C651) and its result returned. The status, the
    position and the last argument are not used on the food path;
  - any other type 0 (0x77C65B).

#### Deleting a site

`WorshipSite::ToBeDeleted` 0x77AA60, in order (before the CitadelPart 0x469540 and MultiMapFixed 0x52E2B0 parts):

1. Each icon in +0xE0 is deleted (`WorshipSpellIcon::ToBeDeleted` 0x77F230 → `RemoveSpellIcon` 0x77C450).
2. The dance (+0xA0) is deleted: `Dance::ToBeDeleted` 0x50B970 runs `RemoveFromDance(1)` (vt +0xB08 0x751510) and
   `SetStateAfterFinishingDance` (vt +0xB0C 0x759B80) on every member that is still available.
3. The object at +0x90 is deleted.
4. If the totem (+0xDC) is available, its +0x100 is set to 0 and it is deleted. +0xDC itself is not cleared.
5. Every town of +0xA4 gets +0x98C (its worship site) = 0.
6. `fn_004637C0` clears the citadel's sites[+0x110].
7. The lists +0xA4 and +0xAC are emptied.
8. The food pot (+0xB4, with no availability test) gets `SetMultiMapFixed(0)` (vt +0x868) and is deleted
   (`Pot::ToBeDeleted` 0x66D110, which runs `Pot::RemoveReaction` 0x66D6A0 first); then +0xB4 = 0.
9. The mana path sprite at +0xB8 is deleted (`CreateManaPathSprite` 0x77B320 stores it; cleared here at 0x77ABEB,
   by `Process` 0x77B283 and by `SetToZero` 0x77AE6E).

### The icons and the charge

#### The worship site's icons

- **Creating one**: `WorshipSpellIcon::Create` 0x77F2B0 places mesh 203 in slot 10..15 with the site's scale and
  angle, and its `SpellSeedGraphic` above it (`SpellIcon::Create3DSpellObject` 0x726210). The `WorshipSpellIcon` is
  0x140 bytes (`SetToZero` 0x77F1F0); ctor 0x77F140 = SpellIcon(pos, info, seed, the site's scale, the site's angle);
  `SpellIcon::Create3DObject` 0x7261A0 = Game3DObject::Create(pos, 0, mesh 203, angle, scale);
  `WorshipSite::AddSpellIcon` 0x77C430 puts it at the head of the site's list; `CallVirtualFunctionsForCreation`
  0x77F290.
- **Its place**: `WorshipSite::GetSpellIconPos` 0x77B080: for the rings 0, 15 and 30 m (step 15 at 0x77B100, cap at
  0x77B10A), the first slot 10..15 whose candidate is not within 1.0 of an icon of the site
  (`MapCoords::IsCloseToEqual`); slot −1 when there is no room. `GetSpellIconPosFromSlot` 0x77AFC0: with ring > 0 the
  point is put on the ground (altitude 0) and pushed `ring` metres outward from the site's origin
  (0x77B00A..0x77B02F); with ring 0 the special point stays as it is.
- **Its levels**: `UpdateGraphicsWithPULevels` 0x77F320 shows the highest upgrade level the player has enabled and
  sets +0x58 = 0.5. **+0x58 is not an alpha**: it is only read by `DrawSpellGraphic` 0x51A712 as the band size (0.2 ×
  +0x58 × scale). The icon's seed is painted opaque (the icon passes alpha 0xFF). The bands' angle (+0x44) turns by
  10.3 rad a second ([0xBE8E94], 0x51A2EA).
- **The removal countdown** (dead code): its +0x114 (a word) is a removal countdown. `StartRemoveFromPlayer`
  fn_0077FF10 sets 1000 when it is 0 (0x77FF2B), `WorshipSpellIcon::Process` 0x77F395..0x77F3B5 lowers it and deletes
  the icon at 0 (vt +0xC(0), returns 3), and `StopRemoveFromPlayer` (`WorshipSpellIcon::StopRemoveFromPlayer` 0x77FF40, from
  `WorshipSite::AddSpellIconIfNecessary` 0x77CA8D, when the icon was fading) clears it. `StartRemoveFromPlayer` has no
  caller and is in no vtable, so the countdown never starts.
- **Validity**: `WorshipSpellIcon::ValidForRequestSpell` 0x77FBA6 returns 0 when `IsFunctional` (vt +0xD4,
  MultiMapFixed 0x52EF70) is not 1. In the vtable 0x99D878, `IsBuilt` (vt +0x890) is
  `WorshipSpellIcon::IsBuilt` 0x77FEE0 = the site's `IsBuilt`, or 1 without a site.
- **SpellIcon virtuals**: vt 0x910 `GetSpellIconPlacementIndex` (WorshipSpellIcon 0x77FC90 = slot − 10;
  TownCentreSpellIcon 0x748E90 = its index, `fn_00743FF0`); vt 0x914 `GetWorshipSpellIcon` (a town icon asks its
  town's site, `fn_0077C2B0`); `SpellIcon::GetPlayer` 0x726540 (the site's player); `GetSpellSeedType`
  0x726360; `IsSpellSeed` 0x726310; `SpellIcon::GetMagicInfoFromPULevel` 0x7262A0.

#### The village centre's icons

- `TownCentre::AddSpell` 0x744050 creates one icon per seed in the first free slot 0..5 of the village centre, and
  does nothing if an icon of that seed exists. Otherwise it calls `fn_00748CB0(pos, &spellIcon[1], seed, TC, the
  point's angle, TC scale, 1, 0)` → `fn_00748BF0` → the TownSpellIcon ctor `fn_00748A70` (which calls `fn_0073D1C0`),
  then `TownCentreSpellIcon::CallVirtualFunctionsForCreation` 0x748D20. The slot comes from `fn_00743F60` (the first
  free slot, `GetSpecialPos` 0x63B0B0).
  - **Bug kept (TownCentre::AddSpell):** when the first free slot has no special point, the loop goes on (0x7440A5),
    but `fn_00743F60` always tries the first free slot again (0x743F68). Every later try fails too and AddSpell returns
    0 (0x7440AE).
- `TownCentre::MakeFunctional` 0x743E80 does it for all the magic the town already had and then calls
  `WorshipSite::AddTownSpells`.
- Each village icon asks the worship site for an icon of its seed (`fn_0073D1C0` →
  `WorshipSite::AddSpellIconIfNecessary` 0x77C9E0); when it is removed, the site's one only disappears if no other
  town of the site has that seed (`fn_0077CAA0`).
- The other town centre icon functions: `FindSpellIcon` 0x743FA0; `fn_00744120` counts the icons
  (`CanTownCentreHoldMoreSpells` 0x73D5F0: < 6); `fn_007442C0` removes a seed's icons
  (`TownSpellIcon::ToBeDeleted` 0x748AE0 → `Town::RemoveSpellIcon` 0x73D220 → the site's `fn_0077CAA0`);
  `TownCentre::AddPowerUp` 0x744010 / `fn_00744030` → `SetPULevel` 0x748EB0;
  `UpdateGraphicWithPULevels` 0x748ED0 shows the highest level the town holds;
  `SpellIcon::ToBeDeleted` 0x7260A0 takes the icon out of the map (vt +0x548, MultiMapFixed 0x52E7B0), then the
  graphic.
- `TownCentre::DeleteDependancys` 0x743BE0 (0x743C4E..0x743C80), for each of the 6 slots +0xD0: the slot is set to 0,
  the icon's +0x118 (its town centre) to 0, and the icon is deleted (`ToBeDeleted(0)`) unless its +0xA & 1 is set (flag
  not identified).

#### A town's magic, and the creature's theft

- `Town::AddMagicTypesHeld` 0x73D380 (if not held yet: held, the owner's `SetMagicTypeEnabled(1)`, and
  `TownCentre::AddSpell` for a base magic or `AddPowerUp` for a power-up; returns 1 if newly added);
  `RemoveMagicTypesHeld` 0x73D450 (the mirror); `IsMagicTypeHeld` 0x73D630;
  `Town::GetNextSpellIcon` 0x73D360; `fn_0073D2E0` (the town has a TownSpellIcon of that seed).
- `GPlayer::IsMagicTypeEnabled` 0x64C220 (the cheat, or a holder enables it). `GPlayer::SetMagicTypeEnabled` 0x64C300:
  on → one more holder and "ever enabled"; off → one less (never below 0); then every icon of the player's six sites
  redraws its levels (`fn_0077B8A0`). `SetMagicTypeEverBeenEnabled` 0x64C250 / `HasMagicTypeEverBeenEnabled` 0x64C260.
- Creature theft: `fn_0073D500` takes the seed's base magic and its held power-ups from a town (recording which ones);
  `fn_0073D5A0` gives them back or to another town.

#### Tapping an icon

- `SpellIcon::InterfaceTap` 0x726430 → `WorshipSpellIcon::ActualInterfaceTap` 0x77F880: if it is already full, the
  seed goes to the hand; if it is being charged, it is cancelled; otherwise, it starts charging. A village centre icon
  forwards the tap to the worship site icon of its same seed (`TownSpellIcon::GetWorshipSpellIcon` 0x748F30).
- The tap sound is `G_ClickOnSpell_01`, bank InGame, sample 0x2A, not 3D, at a pitch of {100, 115, 130, 145, 155, 175}
  % depending on the slot (`fn_00726490`), with the pitch at +0x48 (`GAudio::PlaySoundEffect` 0x429E30 at 0x726528).
  The pitch table on the stack of `fn_00726490` has 7 entries {100, 115, 130, 145, 155, 175, 190}, but the index is
  clamped to 0..5, so 190 is never read. Sample 42's .sad flags (0x402) have no pitch bit, so the options' pitch is
  kept.

#### Charging and filling

- **Starting**: `StartCharge` 0x77FA00 / `ValidForStartCharge` 0x77FAB0 / `fn_0077FB40` (packet 0x25). `StartCharge`
  records the game turn (`GGame::GetGameTurn`; the symbol given is the setter 0x77F680).
- **Every turn**: `WorshipSpellIcon::Process` 0x77F390 runs the removal countdown, the neutral self-charge, and while
  charging puts the chants into the held seed. It returns 3 when the icon removed itself. With the game flag 0x2000 the
  neutral icons charge by themselves at `gatheringChantAddPerGameTurn` (61) per turn.
- **Bug kept in `AddToChantStore` 0x77FDA0:** below the requirement it returns what it put in; above it, it leaves the
  store at the requirement and returns the **excess** `x − (requirement − store)`, and it is that excess that is
  charged to the worship site.
- **Full**: when it fills (`GetChantNeeded` ≤ 0), if there is already a seed in the hand its upgrade level is raised;
  otherwise `PutFullyChargedPowerUpSeedInHand` 0x77F8F0 puts it in the hand **already ready** (`fn_00729900(1)`). The
  miracle's voice plays (`PlayFullyChargedSoundFX` 0x77F4E0, bank `SpellDialogue.sad`) and the charge ends. The voice
  (0x77F520): a seed type above 0x1D gives 8 (0x77F5D6); `GAudio::PlaySoundEffect` 0x429D60(NULL, voice, mode 2, loops
  0, 0, is3D 0, bank 9) at 0x77F5EE.
- **Accessors**: `GetHeldSpellSeed` 0x77F490 (a seed of this icon in a hand of the icon's player);
  `GetChantRequiredForSpellSeed` 0x77F840 (the held seed's need at the charged level, otherwise costToCreate of that
  level); `GetChantNeededForSpellSeed` 0x77FE40; the charge fraction `fn_0077FE80`; `IsCharging` `fn_0077F6D0`;
  `RemoveFromChantStore` 0x77FE10 (takes out min(x, store)); `UseCreateChants` 0x77FCE0 (the store into the seed, at
  most its need), from `fn_0077FCD0`; `ValidForPutFullyChargedPowerUpSeedInHand` 0x77F950; `fn_0077FBF0` (a seed of
  this icon is held and that level's magic is enabled); `fn_0077F780` / `fn_0077F7D0` (a seed made from the icon joins
  / leaves its list, and bit 0 of the seed's flag).

#### Cancelling and returning the seed

- `WorshipSpellIcon::CancelCharge` 0x77F9A0 and `ReturnAllChantsToWorshipSite` 0x77FD60 return the store to the
  battery.
- `SpellSeed::ApplyToWorshipSite` 0x7289C0 / 0x728B30 / 0x729A80 returns the seed's chants to the site of its icon
  (dropping it on the site's ground, giving it to the totem or to an icon, or shaking it off the hand). If it is given
  to an icon **of another seed** of the same player, that icon hands over its charged seed (the exchange).
- A dispenser, a `WorshipTotem` and any icon are "return points" (`IsSpellSeedReturnPoint`), so
  `SpellSeed::CanCast(object)` 0x729190 lets the seed be given to them even if the magic cannot be cast on objects.
- `SpellSeed::RemoveFromHand` 0x728F00 → `MapCoords::FindWorshipSite` → `ApplyToWorshipSite` 0x7289C0 (returns 3),
  otherwise 0x17 (0x728F1C). A forced throw (`SpellSeed::ThrowObjectFromHand` 0x72ACD0) uses `ApplyToWorshipSite`
  0x729A80, and the seed goes in every case.
- `SpellSeed::InterfaceSetOutMagicHand` 0x728940: the seed's icon stops charging for that hand (`CancelCharge`), and
  the interface remembers the seed type (`fn_005DCA20`).

#### The charge visual

Per frame, `SpellIcon::DrawMagicSystem` 0x726D20 → `TChargingData::Draw` 0x7267A0, while charging. The ring's mesh is
loaded by `SpellIcon::TChargingData::Init` 0x7266E0 (mesh 0x231, 561). The visual has four parts (constants at
0x981A0C..0x981A4C, [0x981AA0] = 205, [0x92A540] = 4π, read with the oracle):

1. **The fraction f** (0x7267F3..0x726871): 1 with a held seed, else `GetPower` when the store +0x134 is not 0. It is
   "full" when a seed is held or `GetChantNeededForSpellSeed` ≤ 0. While charging, f = (0.2 + f) / 1.2.
2. **The icon's tint**, icon +0x10C (0x726879..0x726A3F): the player's colour with each RGB byte × b / 256 (alpha
   kept). Full: b = int((0.1 + 0.5·(sin(4π·phase) + 1)·0.5)·255), with phase the PSys global phase [0xD4EBF8], which
   goes 0..1 over 3.33 s. Else, while charging: s = (turns since the charge started + g_game +0x205D64) × ms per turn ×
   0.001; for s < 1.5, b = int(clamp((1 − cos 8s)·0.5·(1 − s/1.5), 0, 1)·205) + 50; for s ≥ 1.5, b = 50. Neither:
   +0x10C = 0. So the 0.1..0.6 pulse is the tint's brightness, not an alpha.
3. **A `LightSheet`** (Init(10) once: 10 points at i·2π/9 on a radius-1 circle through the icon's matrix, +0x18 = 6.0,
   +0x14 = 1.5), in the player's RGB, fed the fraction f at +0x38, updated with game ms × 0.001 and z-sorted by its
   squared distance to the camera (0x726A45..0x726BA0).
4. **The pulse ring**, while charging (mesh 561 `MSH_S_PULSE_IN`, 0x726BA5..0x726D11): t = fn_0068F720(0.1) / 1.0,
   where fn_0068F720(x) = (((phase > x) ? phase : phase + 1) − x) × 3.33; drawn for 0 ≤ t ≤ 1 on the icon's matrix,
   raised (0·(1 − t) + 0.1, [0x981A38]) × twice the mesh's +0x28 along its second row, scaled 3(1 − t), in the
   player's RGB, with **alpha 255·t** (0x726CDD..0x726D07: (0 − 255)(1 − t) + 255).

#### The player's view of its icons

GPlayer functions over all its icons: `fn_0064BB10` = the largest charge fraction of the icons charging for the player
(per icon `fn_0077F690`); `fn_0064BDE0` = some icon is `ValidForRequestSpell(status, −1, 1)`; `fn_0064BDB0` = packet
0x25 → 0x5DABA0, which calls the best icon's `fn_0077FB40(status, −1, 1)`; `fn_0064BD90` / `fn_0064BD50` (packet 0x26
0x5DABF0) = the same with the last seed type; packet 0x6A 0x5DAC30 → `fn_0077FC30`;
`GPlayer::CancelAllSpellsCharging` 0x64BC60 (CLEAR_PLAYER_SPELL_CHARGING); `IsAtLeastOneSpellBeginToBeCharged`
0x64BB90 / `IsSpellBeginToBeCharged` 0x64BBF0 (an icon of that magic, via `fn_00726380` → `fn_0072B230`, whose store
is above 0); `GetNextInterfaceStatus` 0x64AAC0.

### SpellSeedGraphic: the seed that floats in the orb and in the icons

**Faithful except where marked.** The seed seen floating in a one-off orb and over each icon.

#### The object and its creation

- An object from `SpellIcon.cpp` (it is not an `Object`; list 0xD9D3D0). Fields: +0x14 MapCoords of the mesh, +0x2C
  the mesh (Game3DObject), +0x30 the band, +0x34/+0x38 phases of the vials, +0x3C y angle, +0x40/+0x44 band angles,
  +0x48 seed, +0x50 holder PSys, +0x54 scale, +0x58 band size, +0x5C auto-update, +0x60 PU, +0x64 the given point, +0x70
  the band's alpha. Seed row = 0xD9D678 + type × 0x190 (memory offsets = file + 0x10). `load_variables` 0x42C28E fills
  the GSpellSeedInfo table.
- The creation path: `Create` 0x726F60 → `CreateGraphic` 0x726F00 → the ctor `fn_00726E70` (not an Object: no
  creation index; `fn_00726F10` zeroes the angles and sets +0x58 = +0x54 = 1) → `fn_00727190`.
- `fn_00727190` sets the mesh `GSpellSeedInfo.mesh` (+0x130 of the file) and calls
  `ReplaceMeshGivenSeedType` 0x728450 (table 0x72854C per seed − 3): FLYING_FLOCK sets mesh 1 (AnimalBat1)
  if the player's alignment (GPlayer+0x60 → +8) < `alignmentSwitch` (fn_00723140), otherwise 11 (AnimalSpellDove), and
  fn_00727440 redoes it every 30 turns (`g_game +0x205A40 % 0x1E` in fn_00727350). FOOD and the creature vials carry
  envmap 0 (`envmap.raw`) and BEAM_EXPLOSION properties {1,0,1,1,0}. The holder PSys (+0x164 of the file) is created at
  the point + `unknown0x154` × scale with magnitude = scale; the band (`CreatePUBand` 0x727080) if pu ≠ −1.
- fn_007270E0: +0x64 = point, mesh at point + `unknown0x150` × scale (−1.5 almost always: the I_* meshes have their
  origin at the bottom and are ~3 m tall, so this centres them), effect at point + `unknown0x154` × scale.
- `ToBeDeleted` 0x726FE0 takes it out of the list and deletes its 3D objects and effect. `SetAutoUpdate` 0x727680. `SpellSeedGraphic::SetPowerUpType` 0x727060 sets the level.

#### Drawn in the orb and in the icons

- **The orb** (`OneOffSpellSeed::Draw` 0x518E90), every frame it is visible: `GetSpellGraphicPos` 0x72A840 = the drawn
  matrix applied to the mesh point `ResolveLoad()+0x18` (the box centre, 🔍 inferred because it is the point
  fn_00518720 rotates about) and scale = scale of the 3D object × 0.6 ([0x8C7BDC]); `DrawUpdateAtPos` 0x727630 (+0x54
  = scale, fn_007270E0, fn_007274D0: PSys to its point, magnitude = scale, `Process_` with the info zeroed, power 1,
  active) and `DrawSpellGraphic(orb, 0, 1, 0x95)`.
- **The icons** (`SpellIcon::Draw` 0x5198D2, `TownCentre::Draw` 0x5164D4 → `DrawSpellSeedGraphic` 0x726D30):
  `SpellSeedGraphic::UpdateOnly` 0x727590 (the holder PSys stepped by milliseconds) and `DrawSpellGraphic(icon, 0, 1,
  0xFF)` (both tint the icon with 0xFFFFFFFF). The seed stays where `Create3DSpellObject` created it (special point 0 +
  1, scale 1).

#### DrawSpellGraphic

`DrawSpellGraphic` 0x519AD0:

- **The mesh**, only if `useMesh` (+0x168 of the file, fn_00727690) is 1. **STORM, FIRE, LIGHTNING_BOLT, WATER and
  TELEPORT have 0**: in the orb and in the icon only their holder effect is seen (LIGHTNING_STORM / FIREBALL /
  LIGHTNING_BOLT / WATER / TELEPORT_ON_HOLDER). With useMesh = 0, 0x519B0D jumps past the whole mesh part, so the y
  angle +0x3C does not advance either.
- **Its size and turn**: size = `GSpellSeedInfo.scale` (+0x134) × +0x54; angle +0x3C += 2 rad/s × dt ([0x8D8700]),
  fmod 2π (double [0x8D45D8]); `SetPosition` 0x423140: rows X = (cos, 0, sin), Z = (−sin, 0, cos).
- **No bounce or pulse** for the player's seeds: `AsMagicCreatureSpellInfo` (`GMagicInfo::AsMagicCreatureSpellInfo`, vt 0x38) of its base magic is
  NULL (it is not NULL only for a creature spell phial, 0x519B5C..0x519B73) and it jumps to 0x51A0B3. The bounce (+0x38
  at 0.35/0.5 per s, `0.5(1 + sin 2π f)`), the 8×4 UV frames at -15 frames a second (+0x34) and the squashes
  0.7/0.8/1.5 of the switch 0x519D76 (by GMagicCreatureSpellInfo+0x58) belong to the vials 12..27; the UV frames are
  0x519B79..0x519C1B (see [frame-animated textures](../rendering/textures.md#frame-animated-textures)). A creature
  spell phial calls `SetAnimatedUV_2(1)` (0x519B83) before its 8 × 4 UV frames.
- **Colour and light**: diffuse alpha = the owner's (0x51A0B3..0x51A0E1) and `SetGlobalAlpha(alpha ≠ 0xFF)`
  (0x51A0EB). But with arg 2 = 0 (all the world calls) `GetAltitudeAndSetColorSpecular` 0x803340 (0x51A187, at +0x14)
  puts the cell's light on the mesh, with no haze afterwards: it rewrites all of +0x4C with table[brightness]
  (0x803409..0x803413) or table[255] (0x803365 / 0x8033DA), with alpha 0xFF (all of `palette.raw` has alpha 0xFF). So in
  the orb the seed goes through the 0xC387C8 table with alpha 0xFF ([0xC37D8C], 0x80DEF8), **opaque** (not 0x95). The
  creature vials go through fn_00801C90 + fn_007FEB30 (0x519D90 / 0x519D9E), the model light.
- **The holder effect** receives the alpha: `GJPSysInterface::SetAlpha` 0x55ED50 (vt 0x12C) writes byte +0x6C of the
  manager; fn_00679860 0x679875 copies it into [0xC0215C] and fn_00679920 0x679BC2..0x679BDF does atom alpha × it >> 8
  if it is not 0xFF. The holder seen in an orb or an icon is additive (FIRE's SF_FireBallOnHolder...): in the orb
  (0x95) it adds 149/256 of its light, on an icon (0xFF) all of it. Then it is painted as the last step left it. With
  arg 2 = 0 (an orb or an icon) it queues the whole holder effect as a single Z object (`AddDrawing(t, GetOrigin)`
  0x51A2CA); the `CreatureRoom` / `WorldRoom` branch is `Draw_(t, 0)` 0x51A291.

#### The power-up band

- **Its turn**, if pu ≠ −1: +0x44 += 10.3 × dt ([0xBE8E94]), +0x40 += dt ([0xBE8E90] = 1 rad/s, 0x51A305). With +0x60
  = −1, no band is drawn (the band object stays: `SetPowerUpType` 0x727060), and the band angles +0x40 / +0x44 do not
  advance (0x51A2D0 / 0x51A2E1 jump past 0x51A2EA..0x51A318).
- **Its shape**: pu + 1 draws at +0x64 with size 0.2 × +0x58 × +0x54 (the 0.2 is [0xBE8E9C], 0x51A70C), rows:
  identity with row 1 and row 2 swapped (the old 1 negated), rotation (x, z) by base + +0x44, (x, y) by 0.3, (x, z) by
  k, (x, y) by 0.2; base, k = 0, −1 for the first and 0.5, 1 for the others (band constants [0xBE8E98] 0.3, [0xBE8EAC]
  0.2, [0xBE8EA8] 0.5, [0xBE8EA4] 1). Afterwards fn_0051A830 turns it towards the camera ([0xBE8E8E] = 1; see
  [objects that face the camera](../rendering/meshes-and-lod.md#objects-that-face-the-camera-billboards)).
- **Its colour** (`SetColour` 0x7F9770 at 0x51A3BE: edx → +0x4C, the argument → +0x50): +0x4C = `GetPlayerColour`
  0x64D800 (table 0xBFF0B8 by `GetRemapedPlayer`) of the owner (vt 0x1C), or of the local player (g_game +0x205A59)
  if the owner is the neutral one (g_game +0x205A5B) (0x51A322..0x51A36D); its rgb with alpha (+0x70 × the caller's
  alpha) >> 8 (0x51A397..0x51A3B9); +0x70 = 0x3C (fn_00726F10 0x726F4E, only writer), so in an icon (alpha 0xFF) the
  alpha is 59 and in the orb (0x95) 34. +0x50 (specular) = 0x141414 (byte [0xBE8EA0] = 20). Red for player 1.
- **Its material**: `CreatePUBand` 0x727097..0x7270B8 loads it with `GJUtils::GetSharedMesh` and MaterialProperties
  {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so `SetMaterialProperties` 0x57E120 turns the band into mode 13
  (SRCALPHA / ONE, no Z write, fn_0082ECD0).
- **Each level is drawn twice** with the same matrix and colour: 0x51A780 vt+0x104 and then 0x51A7A3 vt+0x104 or, on
  the last level with arg 1 = 0 (all calls: icons and orbs), 0x51A796 vt+0x100. The object is an `LH3DStaticObject`
  (LH3DObject::Create(0) 0x80B4F8, vtable 0x9A2974).
  - vt+0x104 = fn_00815980: screen test (CheckRegionOnScreen 0x868C80) and distance test, then it draws right away
    (vt+0x108 = fn_0080DB30).
  - vt+0x100 = fn_00815A70: the same test, LOD by distance (vt+0x1D0), records g_last_distance /
    g_last_selected_box and, if the object has bit 0x10 of +4 (vt+0x44 = fn_007F97C0), puts it in the Z-sorter
    (`NewZObject` 0x83F310 with fn_007FA980 → vt+0x108, key = distance² to the camera, 0x815F0F..0x815F53); otherwise,
    it draws right away. That bit is set by `SetMesh` (vt+0xF4 = fn_007F9E10 → vt+0x40 = fn_007F97A0) when the mesh
    has bit 0x200 in its flags (fn_007F9D40), and `Power_Up_Band.L3d` has it (flags 0xA2200).
  - So all draws are immediate except the second one of the last level, which goes sorted with the transparent ones
    (with the object's state when the sorter is flushed, which is that of the last level: nothing changes it
    afterwards). Same material and same face mode in both passes (both end in fn_0080DB30): there is no back-face pass
    and no half band. Additive, so each band adds its light twice.

#### The holder PSys and the 3D object its glints read

- **The holder PSys** is created inside `fn_00727190` (0x7272C9..0x72732C); `fn_007276E0` is only the getter of
  `holderParticle` (row +0x164 of the file, read as [row+0x174]). When that is not 0: `PSysInterface::Create(NULL,
  type, mesh MapCoords, effect point, +0x54, NET 0)` (0x7272FF), stored at +0x50; then `psys->vt+0x114(this)`,
  AddTarget, at 0x727313; then `psys->vt+0x20(this->GetPlayer())`, SetPlayer, at 0x727329; then `CreatePUBand` if pu
  ≠ −1.
- The graphic owns it and steps it itself: every drawn frame (DrawUpdateAtPos / UpdateOnly), and also every turn when
  auto-updated (`fn_00727350` → `fn_007273A0`, vt 0x100 with the zeroed info). It is drawn as it was last stepped (time
  multiplier 1, 0x51A27E).
- **The 3D object +0x2C** is what the holder's glints read (`ER_GlintsOnTarget`,
  [the glints on a target](../physics/particle-rules.md#the-glints-on-a-target); vertex walk 0x7277B0 / 0x727800 on
  the `ResolveLoad()` mesh, with no useMesh test):
  - `fn_00727190` deletes the old +0x2C and calls `fn_007270E0` (+0x14 = the point with y + meshHeight × +0x54, the
    effect point with y + holderHeight × +0x54), then creates it at 0x727202 with `Game3DObject::Create(+0x14,
    Get3DType(info.mesh), info.mesh, 0, 1.0)`. There is **no useMesh test** on this path, so the object exists for
    every seed (null only if Create fails, which skips to 0x7272C9).
  - Its matrix at creation (0x727248..0x7272BE), with P the mesh point (x/6553.6, altitude(+0x14) + the y offset,
    z/6553.6): if +0x54 ≠ 1, `SetScale(+0x54)` (0x519320: the matrix zeroed, the scale on the diagonal) then
    `PostTranslation(P)`; otherwise `Translation(P)` (0x403530: identity rows plus P). +0x54 is the Create scale
    argument (ctor 0x726ED9); `GSpellSeedInfo.scale` is not applied and the angle is 0.
  - Only `DrawSpellGraphic`'s useMesh branch rewrites it afterwards (`SetPosition`: the mesh point from +0x14, angle
    +0x3C, size info.scale × +0x54); 0x519B0D jumps past that branch when useMesh is 0. DrawUpdateAtPos / fn_007274D0
    and fn_00727440 (which only calls SetMesh) do not touch it. So with useMesh 0 the matrix stays the creation one for
    ever, even when an orb moves the graphic. Freeze (receive type 0, info.dat file 0x870+0x48) takes the jump-table
    case 0x519DE4, which only sets the light and draws: the phial squashes (receive types 5 and 6) do not reach its
    matrix.
  - Every frame the holder effect is stepped **before** `DrawSpellGraphic`: an icon calls UpdateOnly (0x726D50) then
    DrawSpellGraphic (0x726D62), an orb DrawUpdateAtPos (0x51906F) then DrawSpellGraphic (0x519086). So the glints
    always read the previous draw's matrix, and at the first step the creation matrix (the opposite of the script
    highlight, which draws before it steps).
  - `IsAvailable` (vt +0x2C) is `GameThing::IsAvailable` 0x401810, !(+0xA & 1): the glints' parent atom lives while
    the graphic exists and is not being deleted.
- **Which seeds have glints**: holder 92 (`SF_CreatureSpellFreezeOnHolder`) is the seed CREATURE_SPELL_FREEZE (seed
  12: useMesh 1, mesh 539 `SpellPhileFreeze`, scale 0.8, holderHeight 0); seeds 17, 18 and 22 to 26 also give holder 92
  with useMesh 1 but exists 0. The useMesh 0 seeds' holders (58, 47, 60, 18, 74) are not glint files.

### The worship percentage and the villagers

#### Setting the percentage: the totem

- `Town::SetWorshipPercentage` 0x73C060 runs when the hand lets the totem go (packet 0x29, once per release: see
  [the village totem](village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go)). Without a worship site it
  gives 0 (only +0x5C0 = 0, the totem is not touched). Otherwise the percentage is stored, passed to the totem
  (`TotemStatue::SetWorshipPercentage` 0x738270, which raises it 8 m with a *Zoomer* of |Δ|·5200 ms, which is in ms:
  [the Zoomer](../engine/matrices-and-zoomer.md#zoomer-lh3dlib)) and sent to the villagers that are missing.
- `Town::GetTotemStatue` 0x73E1D0: the town's centre (+0x9A4), then its +0xCC; called by `Town::SetWorshipPercentage`,
  `Villager::CheckInteractWithWorshipSite` and fn_004B7265.
- **The rise** is advanced in two places, one float step at a time, with the engine ms since +0xD0: inline in
  `TotemStatue::Draw` 0x7389C9..0x738A78 (per frame) and in `TotemStatue::Process` 0x737FAF..0x738061 (vt +0x5FC, from
  `TownCentre::Process` 0x743E05). `Draw` 0x738960 stores +0xD0 every frame from the creation on.
- **The totem's sounds**: `SetWorshipPercentage` sets +0xCC = 1, `ReleaseLoopOnSoundEffect(this, 0x1E, bank 1)`
  0x42A330 and `SoundTag::Create(this, 0xB, track 0, mode 2, loops 0, 0, is3D 1, bank 1, delay 0)` 0x71E840 (the rising
  loop, 0x738434..0x73845B). In `Process`, when |value − destination| < 0.005 ([0x999A90], qword) while +0xCC is set:
  +0xCC = 0, `SoundTag::Remove(this, 0xB, bank 1, stop 1)` 0x71EC30, and the stop sound 0x1E at the totem's point
  (0x73808C..0x73810B).

#### Choosing who goes and who comes back

- `Town::GetWorshipersNeeded` 0x73C860: `target = pct > 0 ? max(1, int(population × pct + 0.5)) : 0`; `result =
  target − (worshipping + on the way) + those asking to go home`.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0 (n, skipLifeCheck, requireReachable) makes two passes (the second also
  accepts those flagged 0x200):
  - to send, the available villagers **closest** to the dance centre first (`fn_0073C590` =
    `GetDistanceModifier(distance; distance from the centre to the town + 100) × life³`). Villagers sent to worship need
    a life above `damageThresholdToGoHome` (each villager's own GVillagerInfo +0x35C, 0x73C2DE), unless skipLifeCheck
    is set;
  - to withdraw, those who are at or going to the site, the **farthest** first (state 163).
- `GUtils::GetDistanceModifier` 0x74F290 (called at 0x73C620) is `SigmoidThreshold(0.5; 1 − min(d, max)/max)`
  (0x74F170), with the threshold in the **first** argument (`push 0x3F000000` at 0x74F2B7): it **decreases** with
  distance, from 0.99996 at d = 0 to 3.6e-5 at d ≥ max (see
  [the distances](../engine/coordinates-and-angles.md#gutils-distances)).
- It is **life³**, not life²: after `GetLife` (0x73C630) the loop 0x73C63A..0x73C644 (`mov eax, 2`, and two rounds of
  `dec eax; fmul life; jne`) multiplies the life twice more, and the modifier comes in at the end (0x73C646).
- Available means `Villager::IsAvailableForWorshipSite` 0x752820 (and not `Villager::IsAtOrOnTheWayToWorshipSite`
  0x752860); leaving goes through `Villager::RemoveVillagerFromWorshipSite` 0x76C440
  ([Towns](../beings/towns.md#townprocess-step-by-step)).
- The town's counts: `Town::AddVillagerOnWayToWorshipSite` 0x73E300 / `RemoveVillagerOnWayToWorshipSite` 0x73E360;
  `fn_0073E3E0` / `fn_0073E3F0` (the town's count of villagers at the site).

#### Going to the site

- `Villager::CheckNeededForWorship` 0x76BA60 enters from `DECIDE_WHAT_TO_DO`. At 0x76BB83 a villager that
  `CheckNeededForWorshipSiteBuilding` 0x76C930 needs is not sent to worship: its site is the villager's
  (`GetWorshipSite`, never null there, so the citadel's `FindOrCreateWorshipSite` 0x76C95A is not reached), 0x76C96D
  asks `Town::GetBuildingSiteInList(site)` (none: 0), 0x76C981 sets the top state 163 and
  `Villager::SetupBuildingObject` 0x7584B0 returning 1 gives 1.
- `Villager::CanIGetToTheWorshipSite` 0x76BC20: 1 when the site is within the town info's
  `maxDistanceThatVillagersWillGoToWorship` (+0x148, 500; flat `GetDistanceInMetres` 0x74CD70, `<=` at
  0x76BC5E..0x76BC69). Farther, the villager's player looks for a teleport stone that shortens the trip
  (`fn_0064D6B0`, called at 0x76BC84): 1 with that stone, 0 without (0x76BCA3). With no player, 1 and no stone
  (0x76BC74).
- The villager states:

  | State | What |
  |---|---|
  | 58 | the walk along the path (`SetupMoveToOnFootpath`) |
  | **59** | arrives at the site (0x76BE00; within 10 m of point 9 it joins the dance if `N < maxDancersVisible`, otherwise the hiding place) |
  | **60** | dancing (0x76C680) |
  | **213** | hidden (0x76C5E0) |
  | 241 | eating at the site |
  | 42-46 | carrying supplies |
  | **248** | goes back home (0x761B70) |

- `Villager::StartWorshippingAtWorshipSite` 0x76C4C0 puts the villager in the dance group and sends it to its dance
  place (state 60); `Villager::StartHidingAtWorshipSite` 0x76C550 sends it to the site's hiding point (vt 0x864, state
  213).
- Eating at the site (state 241) needs the villager's stomach: `Villager::CheckAllowedToRestAtWorshipSite` 0x76C9A0
  sends a hungry villager to the site's food pot, `GetFoodAtWorshipSite`.

#### Every turn at the site

`Villager::ProcessInWorship` 0x76C890 every turn:

- `Villager::CheckVillagerGoBackToTownFromWorship` 0x76BEC0: the site gone or no longer the town's player's: state 163;
  fewer villagers needed and first in the go-home queue: state 248, through `SetTopState(0xF8)` (vt +0x8E8) at
  0x76BF59..0x76BF6D, which counts as left when it returns 1, a pause first (239 with FINAL 248,
  `Villager::SetTopState` 0x7520A2) included.
- `CheckRequestGoHome` 0x76C8D0: with life < `damageThresholdToGoHome` 0.3 it signs up in the queue, sorted by the life
  desire `GetLifeDesireFromLife` 0x75BBC0.
- `ReduceVillagerLifeByChant` 0x76C800: `life -= chantDamage × chantLifeRate`, 5e-6; on reaching 0 it dies with reason
  4 and is counted by `GET_TOWN_WORSHIP_DEATHS`.

#### Leaving the site's states

- `Villager::ExitMoveToWorshipSite` 0x76C170, when it leaves for a state with another exit (vt 0x96C) that is not a
  teleport reaction (exit `ExitReactToTeleport` 0x766390, states 201, 202, 251, tested at 0x76C197), takes it off the
  town's list of villagers on the way and clears its flag 0x10.
- `Villager::ExitAtWorshipSite` 0x76C1F0, when it leaves for a state with another exit, takes it off the go-home queue.
  For a state that is not reactive (the state table's +0xB8, `IsReactiveState` 0x7525B0) and not 241 (eating at the
  site) it calls `RemoveVillagerFromWorshipSite`; else it only comes off the site's count (0x77D0A0) and out of the
  dance (vt 0x978 `IsDancing`, vt 0xB08 `RemoveFromDance(1)`), and stays counted in the town's worshippers (+0xE0 bit
  0x2).

### Dispensers and fireflies

#### The dispenser and its turn

- `SpellDispenser` is an Abode with its magic and its period. `SpellDispenser::Create` 0x7228D0 (pos, abode info,
  town, y angle, scale); `fn_007227B0` zeroes its fields. `CallVirtualFunctionsForCreation` 0x7227D0 creates its
  effect, particle type 0x90, on the land under it.
- With no town, `fn_00723010` takes the head of `GetPlayer(0)`'s town list (+0xA50: PLAYER_ONE's oldest town, filled
  at the tail by `fn_0064C090`). A script town number goes through `GGame::FindTownWithID` 0x552FA0.
- **Its turn**: `SpellDispenser::Process` 0x722A70 (vt +0x5FC, after `Abode::Process`, 0x722A73) runs from the town's
  abode pass `fn_00747600` every processAbodeEvery turns, so the period counts those calls.
  1. The orb is at +0xCC: while it is available and touching the dispenser (vt +0x6B8 `Object::IsTouching` 0x637E00,
     0.001), it waits; otherwise +0xCC = +0xC4 = 0 (0x722A78..0x722ABF).
  2. It also needs IsActive (vt 0x40C), a magic (+0xD4), IsBuilt (vt 0x890) and IsRepaired (vt 0x88C)
     (0x722AC0..0x722AF2).
  3. Then ++ +0xC4, and at ≥ +0xC8 (the period) `CreateOneOffSpellSeed` 0x722B80 (0x722AF4..0x722B0F) →
     `OneOffSpellSeed::Create` at its position + 1.2 × its height, site visual 9.
- **The period**: the default is `timeEachMobileObjectTakesToProduce` = **300** turns; `SET_MAGIC_PROPERTIES` 0x70CC30
  and `SET_TIMER_TIME` 0x711280 change it in seconds (× turns per second). With `SET_MAGIC_PROPERTIES` a period of 0
  disables it; `SET_TIMER_TIME` keeps a period of 0 out (see [the script](#script)).
- **Activating**: `fn_00723030` sets the dispenser active; activating it makes an orb at once.
  `CREATE_SPELL_DISPENSER` writes +0xD4 = magic, `fn_00723030(1)`, +0xC8 = ftol(period) in turns; with 0 it calls
  `fn_00723030(0)`.
- **Giving it a seed**: an uncast seed (+0x98 == 0) given to it turns into an orb there, losing its chants
  (`fn_00728C50` / `fn_00728C80`, visual 0x1B).
- **Which lands have them.** `CREATE_SPELL_DISPENSER` only appears in Land3 (`WOOD`, 600 turns), Land5 (`FIRE_PU2`, 600
  turns) and the playgrounds. The challenge script's `GiveSpellDispenserReward`
  (`CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER)`, `SET_MAGIC_PROPERTIES`, `SET_ACTIVE`, `SET_TIMER_TIME`) is the only
  `SPELL_DISPENSER` it creates, and it is run only by `SpellDispensors` (nine dispensers in a row, magic types 1..9,
  250 s), which only `LandControlT`, the test land, runs. So **Land1 and Land2 have no dispenser** in a normal game.

#### The orb's seed, and the magic table's filled fields

- `CreateOneOffSpellSeed` 0x722B80 takes its magic's `GMagicInfo` (`fn_00722B20`: the pointer table 0xD37D10 indexed
  by the magic type), its seed field +0x28 (`fn_005FB400`: 0xD9D678 + 400 × seed, back to the index by `fn_0072B200`)
  and the level of that magic in that seed (`fn_0072B010`).
- info.dat leaves the field at −1 in every row (+0x28 is the record's +0x18, after the 0x10-byte `GBaseInfo`), but it
  is **filled at load**: the end of `load_variables` calls `fn_0042B400` (0x42D547), which for magic types 1..41 writes
  +0x28 = `GSpellSeedInfo::GetFirstSpellSeedForMagicType(type)` when that is not −1 (0x42B40E..0x42B421), and +0x2C /
  +0x30 the gesture and level of `fn_0072B100` (0x42B432..0x42B44D) when that gesture is not 0. Type 0 is not touched
  (the loop runs 0xD37D14..0xD37DB8).
- `fn_0072B100` 0x72B100 is `fn_0072B010` 0x72B010 on the first seed with the type (or gesture 0 and level −1 without
  one); `fn_0072B010` gives, for a type in `magicTypes[1..3]` at slot k, `powerUpGestures[k]` (+0x134 + 4k) and level
  k, and for the base type `magicTypes[0]` (+0x124) gesture 0 and level −1. So only the power-up types get a gesture
  and a level (FIRE_PU1 INVERSE_SPIRAL / 0, FIRE_PU2 SPIRAL / 1, …); the base types keep 0 and −1.
- So a dispenser's orb is the first seed that has its magic type (Land3's WOOD → the WOOD seed, Land5's FIRE_PU2 → the
  FIRE seed), the same choice the firefly reward makes through the same field (`fn_0052B6F0`, 0x52B744).
- **The other readers of the filled fields.** `SpellSeed::DoPreCastThings` 0x729460 tests the filled +0x28 of the
  magic being cast (0x729502: `== 2`, FIRE → magnitude 1; the magic is the seed's last cast one, +0x98, or its magic at
  its level, `Cast` 0x72953E / 0x729549). `SetupPowerUpGestures` 0x5CEE30 sets the interface's current power-up
  gesture (+0x178) to the filled +0x2C of the held seed's magic at its level (`GetMagicInfoFromPULevel` 0x72AFE0 with
  seed +0x6C, 0x5CEE57..0x5CEE6C). `PSysManager::GetPowerUpLevel` 0x673510 reads the filled +0x30.

#### Fireflies

- **The fireflies themselves** are on [Night lights](../world/day-and-night.md#night-lights): once a turn
  `FireFly::ProcessAll` 0x52B7A0 brings up to 50 out at trees and rocks in the evening and flies one a turn to an abode
  or a street lantern, and back to a tree or rock in the morning; each frame `FireFly::Draw` 0x52AA90 / `fn_0052ABC0`
  draws its orbit and fade (S_SpriteSheet3 frame 37, additive).
- **The reward** (`FireFly.cpp` 0x52B5A0..0x52B790): when the hand picks up an object on which a firefly was sleeping
  (`fn_0052B600`, from `GInterface::PlaceObjectInMagicHand` 0x5DA6F0), a one-off miracle is drawn with the
  probabilities of `FIRE_FLY_SPELL_REWARD_PROB`, which **only Land1.txt uses** (HEAL 20; FIRE, LIGHTNING, NATURE,
  FOOD, WOOD and WATER 1 each).
  - The probabilities are at 0xCCFBAC and their running sums at 0xCCFB04 (the total at 0xCCFBA8), set by `fn_0052B630`
    (map command 88).
  - In `fn_0052B600`, after the object's `InterfaceSetInMagicHand`, the firefly at exactly its MapCoords goes
    (`fn_0052B5A0`), then the reward is drawn.
  - `fn_0052B6F0` draws `r = GameFloatRand(total)` (`GameFloatRand` 0x6DE530); nothing for a total of 0; the
    first magic whose running sum is ≥ r (a probability of 0 is never picked); its first seed and level through
    `fn_005FB400` / `fn_0072B200`, and an orb if that seed exists (`GSpellSeedInfo.exists`).
  - **Bug kept:** `FireFly::OnClearMap` 0x52A1E0 clears only the probabilities. The running sums, which are what
    `fn_0052B6F0` reads, keep the last land's values until the next `FIRE_FLY_SPELL_REWARD_PROB`.

### Script

- **Map commands**: `CREATE_TOWN_SPELL` / `CREATE_TOWN_CENTRE_SPELL_ICON` (10 and 12, the same handler),
  `CREATE_NEW_TOWN_SPELL` (11), `CREATE_SPELL_ICON` (13, does nothing, not even in the original),
  `CREATE_PLANNED_SPELL_ICON` (14, only the town's magic type), `CREATE_WORSHIP_SITE` (19, handler at 0x7160C8),
  `FIRE_FLY_SPELL_REWARD_PROB` (88) and `CREATE_SPELL_DISPENSER` (90: town, pos, abode, magic, y angle, scale,
  period).
- **CHL natives**: 330 `IS_SPELL_CHARGING` 0x70CB80, 331 `IS_THAT_SPELL_CHARGING` 0x70CBD0, 355 `GAME_SET_MANA`
  0x6FE800, 356 `SET_MAGIC_PROPERTIES` 0x70CC30, 376 `SET_CAN_BUILD_WORSHIPSITE` 0x6FEC40, 386 `SET_MAGIC_IN_OBJECT`
  0x6FF0B0, 410 `GET_TOWN_WORSHIP_DEATHS` 0x6FF640, 422 `GET_MANA` 0x6FE8C0, 423 `CLEAR_PLAYER_SPELL_CHARGING` 0x70CD80
  and 453 `GET_SPELL_ICON_IN_TEMPLE` 0x6F3590; plus the dispenser branches of `SET_ACTIVE` (255) and `SET_TIMER_TIME`
  (145) and the `CREATE` types 30 `ONE_SHOT_SPELL`, 31 `ONE_SHOT_SPELL_IN_HAND` and 36 `SPELL_DISPENSER` (`GScript`
  0x6F1010).
- **`SET_ACTIVE` and `SET_TIMER_TIME` on a dispenser**: `SET_ACTIVE` 0x6FD720 acts on a dispenser through
  `fn_00723030`. `SET_TIMER_TIME` 0x711280 does not: on a dispenser (vt +0x1F4, 0x71132A) it computes ftol(1000 /
  [0xD01A38] × seconds) inline (0x711338..0x711359) and writes the period +0xC8 only when that is above 0 (unsigned,
  `jbe` 0x711360..0x711362). A shorter time leaves the period as it was, and the active flag is never touched.
- `GET_SPELL_ICON_IN_TEMPLE` → `fn_00463AD0`: the site icon of that magic, in any of the sites.
- `GET_TOWN_WORSHIP_DEATHS` → `Town::GetDeathsFromWorshipping` 0x740D60 → `GetDeaths` 0x740D70: TownStats +0x7C[4
  CHANT], written by `Villager::VillagerDead`.
- **`GET_FIRST_HELP` and `GET_LAST_HELP`**: 359 `GET_FIRST_HELP` (`GScript::GetFirstHelp` 0x6FD0A0) and 360
  `GET_LAST_HELP` (`GScript::GetLastHelp` 0x6FD180) are the same code byte for byte.
  - Each POPs the object (`ScriptDLL::POP` 0x6F6BC0) and looks it up (`GScript::GetScriptGameThing` 0x70D220). None:
    `ScriptErrorMessage` 0x6F62B0 with "Object no longer valid" (0xC0D428) and PUSH 0.0 (type 2, float).
  - A spell dispenser (vt +0x1F4 `IsSpellDispenser`, 0x6FD0F3): its magic type +0xD4 → the `GMagicInfo*` table
    0xD37D10 (`fn_00722B20` 0x722B20) → `GMagicInfo::GetMagicEffectInfo` 0x5FB680 (its +0x10 effect type into the
    `GMagicEffectInfo` array 0xCC6630, 0x11C bytes each) → the effect's +0xF8 `helpStartEnum` (+0xE8 after the
    0x10-byte `GBaseInfo`), loaded as an unsigned 32-bit number (`fild qword` with a zero high half) and PUSHed as a
    float (0x6FD10B..0x6FD132). Neither table index is checked.
  - Any other object (vt +0x460 `IsObject`, 0x6FD140): its info `[+0x28]` → `GObjectInfo::HelpStartEnum` +0x50, the
    same way (0x6FD14A..0x6FD171). A thing that is not an object: PUSH 0.0 with no message (0x6FD148 → 0x6FD0D0).
  - So both opcodes give the **first** text; `HelpEndEnum` is never read, and the per-class `GetQueryFirstEnumText` /
    `GetQueryLastEnumText` are not called. The one base-game caller is challenge.chl's `GiveSpellDispenserReward`, on
    its dispenser, when `dont_tell` is 0 (both results go to `HelpJustTalkMaybeWithSpirits`); it runs only from
    `SpellDispensors`, so the normal lands never reach it.

### Addresses

The class-qualified names of functions named in the sections above:

| Name | Address | What |
|---|---|---|
| `WorshipSite::RemoveSpellIcon` / `WorshipSite::GetSpellIconFromSeedType` / `WorshipSite::GetNumVillagersRequestingToGoHome` | 0x77C450 / 0x77B170 / 0x77E260 | |
| `WorshipSpellIcon::CallVirtualFunctionsForCreation` | 0x77F290 | |
| `WorshipSpellIcon::GetHeldSpellSeed` / `WorshipSpellIcon::CancelCharge` | 0x77F490 / 0x77F9A0 | |
| `SpellIcon::GetSpellSeedType` / `SpellIcon::IsSpellSeed` / `SpellIcon::GetMagicInfoFromPULevel` | 0x726360 / 0x726310 / 0x7262A0 | |
| `SpellSeedGraphic::ToBeDeleted` / `SpellSeedGraphic::SetPowerUpType` / `SpellSeedGraphic::SetAutoUpdate` | 0x726FE0 / 0x727060 / 0x727680 | |
| `SpellSeed::ReplaceMeshGivenSeedType` | 0x728450 | the flock seed's mesh by the alignment |
| `TownCentreSpellIcon::CallVirtualFunctionsForCreation` / `TownCentreSpellIcon::SetPULevel` / `TownCentreSpellIcon::UpdateGraphicWithPULevels` | 0x748D20 / 0x748EB0 / 0x748ED0 | |
| `TownCentre::FindSpellIcon` / `TownCentre::CanTownCentreHoldMoreSpells` | 0x743FA0 / 0x73D5F0 | |
| `Town::RemoveMagicTypesHeld` / `Town::IsMagicTypeHeld` | 0x73D450 / 0x73D630 | |
| `GRand::GameFloatRand` | 0x6DE530 | the fireflies' draw |
| `GPlayer::IsAtLeastOneSpellBeginToBeCharged` / `GPlayer::SetMagicTypeEnabled` | 0x64BB90 / 0x64C300 | |

## openblack

Status: **done** for the sites and their texture, the icons, the seed graphic, the battery, the charge, the worship
percentage and the villagers' states, the dispensers, the fireflies and the script; **different by design or not
ported**: listed under [Differences from the original](#differences-from-the-original-and-what-is-missing). Port
decisions (the battery rules, the dispenser rules): [port decisions: magic](../project/port-decisions-magic.md).

### The sites, the battery and what a site takes

- **The structure**: `src/Worship`, `ECS/Systems/Implementations/VillagerWorship`; the special points in
  `src/Worship/SpecialPoints.cpp`.
- **The site's texture** (`WorshipSite.cpp` `CitadelSiteMesh`): one copy of `B_WORSHIP` per heart mesh in the mesh
  cache (`temple/B_WORSHIP_l3d/<heart mesh id>`), made with the citadel's first site; its skin source is the heart's
  mesh and `L3DMesh::SetSkin` gives every primitive the heart mesh's first skin (`GetFirstSkin`, in the file's order).
  The partly built site (and temple) keeps the skins too: `PartialBuild::BuildMesh` sets the intact mesh as the
  generated one's skin source. Before (bug): skin 1 was looked up in the texture manager, where it is the first
  texture of `AllMeshes.g3d`, so the site was drawn with an unrelated pack texture (black without one).
- **The battery**: the rules are in `Magic/WorshipBattery` (taken from raffclar's tree, the same rules as ours term for
  term); `Worship/WorshipSite.cpp` applies its end of the turn to the site's component. The site's other battery
  functions (capacity, battery size, draws, strain, icon share) are still ours, the same expressions as his. openblack
  has no artifacts.
- **The dancers** are spread over a 6 m ring around point 8, at 256/N each (the ring part of `CalculateDancePosition`),
  not the dance file's places (queued fix F-51, after F-50); the dance itself is ported for the scripts' dances
  (afa59370f).
- **The resources a site takes**: `worship::site::AddResource` (the pure `AddResourceRouteOf`), reached from
  `object_resources::AddResource`, so an object a worship site takes (a thrown animal or pot of food, an object given)
  fills its food pot; the food pot's add is `pot_resource::AddToPotDirect` (the pile sound, the cap, the poison, the
  size). The town's footpath link to the site is not ported, because the villagers use openblack's pathfinding.

### The icons and the charge in openblack

- Before, openblack painted the icon's seed half transparent: corrected. The game flag 0x2000 is
  `OPENBLACK_INFLUENCE_EVERYWHERE`. The worship icon's tap sound is in `Worship/WorshipSpellIcon.cpp`.
- The icon rings step 15 m up to 30 m as the original's `GetSpellIconPos` (0x77B100 / 0x77B10A) (`k_IconRingStep`,
  `WorshipSite.cpp:75-76`), and the push is in metres (:227-247): ✅ faithful.
- `UpdateChargingVisual` (`WorshipSpellIcon.cpp:683-725`) draws the pulse ring with alpha t, as the original (its
  comment at :687, "alpha 255 (1 - t)", is wrong); it draws for t < 1 (the original ≤ 1, the same picture, scale 0 at
  1). Not ported: the ring's raise and colour (:720), the icon's tint (+0x10C) and the light sheet of
  [the charge visual](#the-charge-visual).

### The seed graphic

`Worship/SpellSeedGraphic.cpp`:

- **Updating**: `seed_graphic::DrawUpdateAtPos` / `UpdateOnly` / `DrawSpellGraphic` / `UpdateIconGraphics`;
  `one_off::UpdateFrames` (orb) and `worship::Update` (icons) call them every frame (🔍 inferred: also when they are not
  on screen). The flock seed's mesh is redone every 30 turns with `Game::GetTurn` (🔍 inferred that this field is the
  turn counter).
- **The mesh**: openblack painted the useMesh 0 seeds' meshes (I_Lightning2, I_Blast, I_Lightning, the horn for water
  and the shield for teleport): those were the "wrong icons". Of the vials' look only the UV frames are ported
  (`frame_anim::SpellIconFrame`); the bounce and the squashes are not.
- **Colour and light**: the seed in the orb is `components::Alpha` = 1. The cell's light is the
  `land_light::ObjectMode::Cell` mode of `SpellIcon::Draw` (`SpellSeedGraphic::landCellLight`, `LandLightOf` from
  `RenderingSystem.cpp`). Without the alpha given to the PSys (before) the centre of the bubble came out burnt white
  and covered the icon.
- **The band** faces the camera with `billboard::BandToEye`; its colour is `components::ObjectColour` (new) + `Alpha`
  (⚠️ approximate: the specular is not painted, as the colour path of vs_object does not have it, and the model light
  (90 + 166 N·L) is that of the PSys mesh atoms; 🔍 inferred: the local player is PLAYER_ONE). Each level's two draws
  are two entities per level (`k_DrawsPerBand`, `extraBands` = 2 (pu + 1) − 1) (⚠️ approximate: the order relative to
  the bubble, immediate ones before and the Z-sorter one among the transparent ones, is not reproduced: the 2 (pu + 1)
  go in openblack's translucent pass).
- **The holder**: `seed_graphic::Create` gives it the graphic as its target, then the graphic's player, in the same
  order. The glints' matrix is `SpellSeedGraphic::glintModel`: the creation one at creation
  (`seed_graphic::CreationGlintModel`), and `DrawSpellGraphic` sets it inside its useMesh branch, after the Transform,
  to the draw's `SetPosition` matrix (`seed_graphic::DrawnGlintModel`, `affine::PlacementMatrix`). The orb and the
  icons step the holder before they call `DrawSpellGraphic`, as the original does, so the glints read the previous
  draw's matrix. A seed without a mesh keeps the creation one, but it gives no glint points in openblack, because it
  has no `Mesh` (Pending). The holder's target, player and creation matrix match the original
  (`SpellSeedGraphic.cpp:268-293`; the glint matrix is updated only after the holder's step and only with
  `useMesh != 0`, :459-464).
- **Not ported**: the per-object envmap (FOOD, the creature vials, BEAM_EXPLOSION's properties) and the `CreatureRoom`
  / `WorldRoom` branch.

### The worship percentage and the villagers in openblack

- `Worship/WorshipPercentage.cpp`, `VillagerWorship.cpp`: openblack passed the arguments of `GetDistanceModifier` the
  other way round and sent the farthest ones first; corrected.
- The villager states are in the table in `LivingActionSystem.cpp`. openblack walks with the WallHug inside 59, so 58
  is not used. **Watch out:** openblack's `k_VillagerStateStrings` is wrong at indices 248..254 (it says
  `RESTART_MEETING`...); the `VillagerStates` enum does match the original and is what indexes the table.
- In `VillagerWorship.cpp`: `SiteOf` (the town's site), `AddVillagerToWorshipSite` (also counts the town's
  worshipper), `RemoveVillagerFromWorshipCount`, `RequestGoHome` and `RemoveGoHomeRequest` (the go-home queue),
  `CanIGetToTheWorshipSite` (with the teleport stone that shortens the trip), `StartHidingAtWorshipSite`,
  `CheckAllowedToRestAtWorshipSite` (never: openblack's villagers have no belly yet), and the exits for
  `VillagerStates::WorshippingAtWorshipSite` and `VillagerStates::HidingAtWorshipSite`; going home is
  `VillagerStates::GoHomeFromWorship`.
- Components: `WorshipSite::infiniteChants`, `goHomeRequests` and `villagers`; `WorshipVillager` (the villager's link
  to its site, with `requestedGoHome`); the icon's `removeTimer` (vestigial, as in the original) and `bandSpin`.
- Tests (`test_worship`): `ShippedNorseSiteInfo` builds the shipped Norse site row, and the fixture fills
  `MagicType::Fireball`'s row in `magicGeneral`, since the GMagicInfo rows live in per-class sections (types 0..9). The
  nearest-first test puts the villager's distance to the site's centre against the town's + 100, about 128.9 m, as the
  original's `fn_0073C590` does.

### Dispensers and fireflies in openblack

- **Dispensers** (`Worship/SpellDispenser.cpp`): the turn's timer and the orb's place are the pure rules of
  `Magic/DispenserRules` (raffclar's names: `magic::StepDispenser` over a `DispenserTimer`, `OrbPosition`,
  `OrbStillThere`), called by `dispenser::Process` and `dispenser::CreateOneOffSpellSeed`. The counting is ours, which
  differs from his in two places (see Pending).
- The orb's seed is `magic::GetSpellSeedOfMagicInfo` (`Magic/MagicTables.cpp`), used by
  `dispenser::CreateOneOffSpellSeed`; until 2026-10-08 openblack read the raw −1 and its dispensers made no orb at all.
  The filled fields' other readers: `GetSpellSeedOfMagicInfo` in `PrepareCast` (`SpellSeed.cpp`),
  `GetGestureOfMagicInfo` in `gestures::SetupPowerUpGestures` (`PowerUpSystem.cpp`), `GetPowerUpLevelOfMagicInfo` in
  the spell's PSys sink (`Spell.cpp`).
- `SET_TIMER_TIME` is `dispenser::SetTimerTime`, which the miracles' service's `SetDispenserPeriod` calls.
- **Fireflies**: `Worship/FireFlyReward.cpp` (the reward); the fireflies are `src/ECS/FireFlies`:
  `ProcessFireFliesTurn` (once a game turn, from `Game.cpp`), `UpdateFireFlies` (every frame, in game seconds, 0 while
  paused) and `TakeFireFlyAt` (a firefly on exactly that point goes when its object is placed in the hand; true when
  there was one).

### The script in openblack

`Magic/Script/CHLWorship.cpp` and the worship part of `MapScriptMagic.cpp`. `GET_FIRST_HELP` / `GET_LAST_HELP` are
`magic::GetDispenserHelpText` (`Magic/MagicTables.cpp`), called by `PushFirstHelpText` in `CHLApi.cpp`; a magic type
outside the table gives 0 there.

### Differences from the original and what is missing

- openblack's worship sites are born **built**: there is no construction work and no `BuildingSite`, so
  `CREATE_PLANNED_WORSHIP_SITE` does nothing and a planned citadel gets its six slots anyway.
- **The hand grips, drags and lets go of the totem**, as read in
  [the village totem](village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go): the grip's four tests, the
  held share, the drag on every drawn frame, its packet 0x29 at the end, the empathy at the release, and the guidance's
  raise sound, the see-through second totem and the 0xEEB tooltip. Shown by the testbed's
  `objects.village_totem_drag`. What the totem still misses is listed on [its page](village-totem.md#pending) (an older
  list here also named the reaction at the grip, the facing, the icon, the hand's pose, `HEIGHT` and
  `GET_TOTEM_STATUE`, which have since been ported).
- Not ported: the real dance of the dance files at the site (queued fix F-50), the site's artifacts, the mana path
  sprite (`CreateManaPathSprite` 0x77B2C0, on the casting side), eating at the site, the supplies to the site, the
  reward chests and the stealing of spells by the creature (with the creature).
- `GET_FIRST_HELP` / `GET_LAST_HELP` on an object that is not a spell dispenser: the original gives its info row's
  `HelpStartEnum` (+0x50); openblack still gives 0, because it does not look up every kind of object's info row by
  thing. Telling apart a thing that is not an object (0 with no message) is still to do as well.

### Where the code is

- `Worship/Citadel`: `CitadelWorship::sites` (indexed by the slot), `StrainSoundFractionAtMostOne`,
  `FindNearestWorshipSite` (fn_004639A0, for the chant music), `GetSpellIcon` (fn_00463AD0), `WorshipSiteBuilt`,
  `SetInterfaceCitadel`, `EntranceValidToTap`.
- `Worship/FireFlyReward`: `SetRewardProbability` (fn_0052B630), `OnPlacedInMagicHand` (fn_0052B600).
- `Worship/GestureIconProvider`: `InterfacePlayer`; `Worship/InterfaceStatus`: `HasInterface`, `HeldSpellSeed`
  (fn_005DC830).
- `Worship/PlayerSpellIcons`: `AnyRequestableIcon` (fn_0064BDE0), `SetLastSeedType` / `LastSeedType` (fn_005DCA20 /
  fn_005DCA40), `SetChargingPowerUp` (packet 0x6A), `CancelAllSpellsCharging`, `OnMagicTypesChanged` (the tail of
  `SetMagicTypeEnabled`).
- `Worship/SpellDispenser`: `TownIdFor` (fn_00723010), `ApplySeed` (a seed given to a dispenser); `dispenser::Process`
  skips a dispenser with `MagicType::None`.
- `Worship/SpellSeedGraphic`: `LoadBandMesh`, `SetBandColour`, `UpdateBands`, `ReplacedMesh`, `StepEffect`,
  `BandRotation`; `seed_graphic::Create` makes no holder for `ParticleType::None`.
- `Worship/TownCentreSpellIcon`: `CreateIcon`, `AddSpell`, `RemoveSpell`, `SpellCount`.
- `Worship/TownMagic`: `AddMagicTypesHeld`, `TakeSeedMagic` and `GiveSeedMagic` (creature theft), `CheckAddWorshipSite`;
  a player owns a magic type while one of its towns holds it (`GPlayer::MagicRemainder`, `Magic/Core/Players.h`).
- `Worship/Worship`: `ReturnSeedToItsSite`, `OnSeedOutOfHand`.
- `Worship/WorshipPercentage`: `SetTotemPercentage`, `AdvanceRise`, `AddVillagerOnWay`, `ProcessTotem`, `TotemTown`.
- `Worship/WorshipSite`: `TribeOfTown`, `TownHasSpellIcon`, `SiteTransform` (🔍 inferred: the snap to the land's height
  is openblack's), `FindNearestFreeSlot`, `IconPositionFromSlot`, `FindIconPosition`, `CreateTotem`, `RemoveTown`,
  `AddTownSpells`, `DancerCount`, `GetFoodResource`, `TotalChantsAvailable`, `SetMana`, `AddDancer`,
  `VillagersRequestingToGoHome`.
- `Worship/WorshipSpellIcon`: `PlacementIndexOf`, `WorshipIconOf`, `SeedTypeOf`, the icon's `MagicTypeOf`,
  `GetChantRequired`, `ChargeFraction`, `ValidForPutFullyChargedSeedInHand`, `PowerUpValid`, `AddSeed`,
  `UpdateChargingVisual`.

### Test hooks

The hooks are in [debug environment variables](../project/debugging.md#debug-environment-variables).

#### Checks in the game

Land2 with `-s Land2.txt`, Land1 with `-s Land1.txt`:

- PLAYER_TWO's citadel with its two worship sites (the Norse one from `CREATE_WORSHIP_SITE` in slot 5 and the Greek
  one that `PostLoadCleanup` adds for town 2 in slot 0), both at the citadel's origin and rotated to their slot, with
  their food cauldron and their altar. The `b_worship` mesh wears its temple's texture, as the original (it used to
  come out with a wrong texture).
- The site's four spell icons (FIRE, NATURE, FOOD, WOOD from town 1) at points 10..13 of the `b_worship` mesh, with
  their `SpellSeedGraphic` and their effect above.
- With `OPENBLACK_TEST_WORSHIP="1.0.5"`: 11 of the 22 villagers of town 1 at the worship site. The log gives `site 149
  icons 4 N 11 C 33.0 k 0.223 strain -1.000 battery 6824 / 12300 available 6857 damage 0.67`: capacity 11 × 3, maximum
  9000 + 11 × 300 and the damage per dancer exactly as in the original. Town 1's totem is raised on its plinth (8 × 0.5
  = 4 m), with the village centre's icons around it.
- With `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`, `OPENBLACK_TEST_TOWN_SPELL="0,FIRE;..."`,
  `OPENBLACK_TEST_MANA=40`, `OPENBLACK_TEST_TAP_ICON="FIRE,5"`: the charge ring lit over the FIRE icon while it fills
  at 40 chants per turn.
- With `OPENBLACK_TEST_MANA=20000`: with the battery full the icon fills in one turn and `Worship: seed 3093 of icon
  3086 in the hand with 7000 chants` (FOOD costs 7000).
- With `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826.2670,WOOD"`,
  `OPENBLACK_TEST_FIREFLY_REWARD="1846.2670.3"`: Land1's miracle dispenser with its WOOD orb and three firefly rewards.
  The total of the probabilities is 26 (HEAL 20 + six of 1), as in Land1.txt, and HEAL, WATER and FOOD came out.

## Pending

- **Unverified**: the villager's bit 0x200 that the second pass of `AdjustWorshipersWorshipping` accepts;
  `maxDistanceForVillagersToGoToTheWorshipsite` (1000) and `minLifeForVillagersToGoToTheWorshipsite` (0.4), which are
  not read in the ported functions. Verify each with `xref.py readers` on the field. (The icon's +0x114 countdown is
  dead in the original: [the worship site's icons](#the-worship-sites-icons).)
- **The dispenser's help text** for a magic type outside the 42-entry table: what the original reads is not known
  (the index is not checked); openblack gives 0.
- **The seed graphic's glints**: a seed without a mesh (useMesh 0) has no `Mesh` in openblack, so its holder's glints
  would find no points. The original walks `info.mesh` (after ReplaceMeshGivenSeedType) through the frozen creation
  matrix. This is unreachable with the game's data, because no glint file is the holder of such a seed. The holder is
  also stepped every frame, not only when the object draw list draws the orb or the icon, and its first step is a
  single one ([the glints on a target](../physics/particle-rules.md#the-glints-on-a-target)).
- **The dispenser's counting** differs from raffclar's in two places (see the port decisions page).
- **Not identified**: the site's +0x90 (set only by the constructor, 0x77A997) and the town centre icon's +0xA & 1
  flag. Verify with `xref.py writers` on those offsets. (+0xB8 is the mana path sprite, `CreateManaPathSprite`
  0x77B320.)
