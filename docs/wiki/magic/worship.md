# Worship

## Summary

Miracles come from worship. Each town holds some kinds of magic, and its village centre shows one icon for each
miracle it holds. The player's temple has one worship site per tribe, with an icon for every miracle that its towns
hold. When the village totem is raised, villagers walk to the site and dance there. Their dance fills the site's
battery of prayer power, and each dancer pays for it with a little of its life. Tapping an icon starts it charging
from that power; when it is full, the miracle's seed appears in the hand, ready to cast. Separately, dispensers make
one-off miracle orbs every few minutes, and on Land 1 fireflies sleeping on objects give a random orb when the object
is picked up.

openblack ports the sites, their icons and seeds, the altar's need signs, the battery, the charge, the worship
percentage and the villagers' worship states (with the meal from the site's pot, the altar rest and the supply
errands), the footpath from each town's storage pit, the powering up of the site's artefacts, the dispensers and the
fireflies. A site the citadel asks for starts unbuilt, with a building site in each of its towns; that the villagers
build it as the original's is not checked. The worshippers dance their site's own dance, from the game's dance files.

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
| the worship site's `b_worship.l3d` | **16**: 7 the altar's rest point, 8 dance centre and totem, 9 arrival, **10..15 the six icon slots** |
| the village centre (e.g. 179 `BuildingNorseVillageCentre`) | **14**: **0..5 the six icon slots** (all at y = 2.781, in a ring) and 6 the totem (y = 4.613, in the centre) |
| the icon, mesh 203 | 1: the point where the `SpellSeedGraphic` floats (+1 in y; fn_00726140 0x726140, where `SpellIcon::Create3DSpellObject` puts the graphic) |

`fn_0077CE70` = point 7, `GetRestPos` (the altar rest's goal, `Villager::GotoAltarForRest`); `GetArrivePos` 0x77CED0 =
point 9. The villagers beyond `maxDancersVisible` do not wait at a special point: they hide at the site's door (see
below);
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
- The altar is a `CitadelPart` of its own: its constructor 0x780840 passes percent 0 and the site's under-construction
  bit, so it starts 0 % built. `WorshipSite::BuildBy` 0x77DC50 is `MultiMapFixed::BuildBy` and then, while the altar is
  available, the altar's `BuildBy` by the same amount; `WorshipTotem::Draw` 0x519360 draws nothing at all until the
  altar is built (no partly built draw, no magic system). A percent set on the site alone (`SetPercentBuilt`) leaves
  the altar as it is.
- The villagers at the site: +0xD4 / +0xD8 list them, newest first (`fn_0077D040` adds, `fn_0077D110` finds one);
  `WorshipSite::RemoveVillagerFromWorshipCount` 0x77D0A0 takes one off the list (if it is there) and the count +0xC8
  one down, when there is a count.
- The go-home queue, +0x120 / +0x124: `fn_0077E0C0` puts a villager in, sorted by its desire for life (the highest
  first), sets its +0x118 and sends another villager (`AdjustWorshipersWorshipping(1, 0, 1)`);
  `WorshipSite::RemoveVillagerRequestingToGoHome` 0x77E1D0 takes it out; `fn_0077E0A0` tells whether a villager is
  first in it (an empty queue counts as first). `GetNumVillagersRequestingToGoHome` 0x77E260 counts it. Only
  `CheckRequestGoHome` 0x76C8D0 calls `fn_0077E0C0` (0x76C90B), and nothing reaches `CheckRequestGoHome`: the index
  has no call or jump to it, it is in no vtable, and its address appears nowhere in the exe's bytes (bw1-decomp's
  `ProcessInWorship` does not call it either). So **the queue stays empty** in the original: nobody asks to go home,
  `GetWorshipersNeeded` adds 0 for the askers, and every worshipper counts as first.
- The towns: `Town::SetWorshipSite` 0x73D030. `WorshipSite::AddTown` 0x77C800 also plans a footpath from the town's
  storage pit (`GetStoragePitEvenIfPlanned` 0x7412A0: the town's available pit, else the first `StoragePit` of its
  abode list) to the site: the site gets an empty footpath link if it has none (0x77C831..0x77C864), and unless
  `GetNearestPathToQuick`(pit's arrive point, the site's, 0.001) finds one of its footpaths already ending there
  (0x77C89D) a `GFootpathFinder`(pit, site) starts (0x77C8B5, [routes](../beings/routes.md)). openblack:
  `site::AddTown` → `footpaths::StartFootpathFinder`. `fn_0077C950` removes a town.
  `Villager::GetWorshipSite` 0x76C340 is its town's site (vt +0x48, then the town's vt +0x30C); the villager's worship
  flags are Villager +0xE0. With no town it falls back to `Citadel::FindTribeWorshipSite` 0x463190 (the citadel's six
  sites, +0x34, matched on the tribe at +0x8C) of `GetCitadel` (vt +0x114), but `Villager::GetCitadel` 0x751DB0 is
  the town's player's citadel (town +0x2C, player +0xA48) and null without a town, so the fallback is never reached:
  a villager with no town has no worship site. openblack's `SiteOf` gives none, the same.
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
  - It pops the mana, then the thing. It takes a thing that `IsWorshipSite` (vt +0x304; only `WorshipSite`
    0x55DCA0 says yes) or `IsSpellIcon` (vt +0x3D0; `SpellIcon` 0x55D450 for `SpellIcon`, `TownSpellIcon`,
    `TownCentreSpellIcon` and `WorshipSpellIcon`); an icon gives its site through `GetWorshipSite` (vt +0x30C): a
    `WorshipSpellIcon` its +0x118 (0x77F0A0), a town or village-centre icon its town's site (0x748F00), a plain
    `SpellIcon` none (0x55D3A0). Anything else, a town or a totem too: "Thing not valid!" (0xC0DA54) and
    nothing (0x6FE853..0x6FE8B9). `ClearAllSpellIconsChantStores` 0x77B060 walks the site's icons (+0xE0, next
    +0x110) and zeroes each one's store (+0x134). Nothing is pushed.
  - `GET_MANA` 0x6FE8C0 pops the thing: a worship site (vt +0x304) pushes its battery +0xF0 as a float (type 2);
    anything else "Thing not valid!" and 0.0. openblack: `magic::script::GameSetMana` / `GetMana`
    (`src/Magic/Script/CHLWorship.cpp`); test
    `WorshipTest.AScriptSetsASitesPrayerPowerThroughTheSiteOrItsIconAndEmptiesItsIconsCharges`.
- **Visual strain** `fn_0077B3B0` (per frame, through `fn_00463980`): `phase = fmod(phase + (5 +
  5·clamp(strain,0.1))·dt, 2π)`, `pulse = (cos phase + 1)/2`.

#### The food pot and what a site takes

- `WorshipSite::CreateFoodPot` 0x77AD90: pot info 2 (StoragePitFoodPile, 0xD4C8E8) at `GetFoodPosAndYAngle` 0x77CDB0 =
  the site's local point (9, 0, −38), at the site's angle + 1.5 (0x99C45C), scale 0.7 (0x99C460).
- `WorshipSite::GetResource` 0x77BD80 (vt +0x98): FOOD with a pot (+0xB4) → the pot's `JustGetResource(FOOD, 0, 0)`
  (vt +0x94, `Pot` 0x66D390: its amount when the type is its own); any other type, or no pot, 0 ✅.
- `WorshipSite::RemoveResource` 0x77C670 (vt +0xA0): with a building site (+0x74) and WOOD or ANY, every argument goes
  to the site's `RemoveResource` (0x77C67B..0x77C69A); else FOOD with a pot → the pot's `JustRemoveResource(FOOD, n,
  0)` (vt +0x90, `PotStructure` 0x66D9B0; 0x77C6A4..0x77C6BD); any other type, or no pot, 0. No pot is made, no
  `DoResourceRemoving` runs and the status is not used on the food path ✅.
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

#### A sacrifice at the altar

A villager given to a site's altar (`WorshipTotem`) by the hand: `Villager::ApplyThisToObject` 0x752C40, in this
order (checked in the disassembly):
- its sacrifice value (vt +0x5C0), × 1.25 for a child (vt +0xAF8, `fmul` 1.25 0x752CD3), added to the site's battery
  (+0xF0) and to what was sacrificed there (+0xF4), the site being the totem's +0x100;
- the deed for the creature to copy (`ConsiderMakingCreatureMimicPlayer`, action 0x28), when the status has a player;
- `VillagerDead` 0x7506C0 with reason 7 (SACRIFICE);
- **the light**: the totem's MapCoords (+0x14) with its altitude plus the totem's `GetHeight` (vt +0x42C), then
  `CreateSpotVisual` 0x63E540 (OBJECT_APPEAR 0x1B, 1.0, no owner) 0x752D86; if made, the visual is given the site's
  player (vt +0x20 with the site's `GetPlayer`, 0x752D94..0x752DA9). No interface status is asked for;
- with an interface status, the sounds (see [the world's sounds](../audio/world-sounds.md));
- for the local interface's status only, the prayer power as a `ValueSpinner` number (0x752EB5..0x752F90): `"%3.0f"`
  of the value, red (bytes 0, 0, 0xFF, 0xFF), at the interface's MapCoords +0x3B8: x and z × 1/6553.6 (0x8AA3A4), y the
  land's altitude there plus +0x3C0. `Living` 0x5ED7AB and `Tree` 0x74BF67 make the same number; `DeadTree` none.
The light is the villager's only: `Living`, `Tree` and `DeadTree` make none.

An animal, a tree and a dead tree (read in the v1.42 bytes):
- **an animal**, `Animal::ApplyThisToObject` 0x41B360, given to a `WorshipTotem`: no test of the site being built
  and none of the animal being indestructible. When the status has a player (vt +0x1C), the deed for the creature
  (`ConsiderMakingCreatureMimicPlayer`, action 0x28), then that player's sacrifice count, `GameStats` +0x1124, one
  more (the `inc` 0x41B3E4..0x41B3F4), then `GAlignment::Update(player, animal, 7)` 0x41B3FE (which halves the change
  for an animal, × 0.5 at 0x8AA3B4, 0x4143FA), then the animal's death reason +0x10C = 7 (0x41B403); then
  `Living::ApplyThisToObject` 0x5ED5C0 takes it;
- **its value**: `Living::ApplyThisToObject` 0x5ED5C0 adds vt +0x5C0 × vt +0x11C (0x5ED5F5, 0x5ED609, 0x5ED60F), the
  sacrifice value times the life, to the site's battery and sacrificed total, then the sound and the number (0x5ED7AB);
- **a tree**, `Tree::ApplyThisToObject` 0x74BDA0: refused when +0x25 & 0x40 is set (0x74BDD4); the same value
  (0x74BDE2, 0x74BDF6), its ghost (`GoolooGooloo` 0x74BE21), `ToBeDeleted`, the sound and the number (0x74BF67);
- **a dead tree**, `DeadTree::ApplyThisToObject` 0x510EE0: the same value (0x510F17, 0x510F2B), the sound (0x510FCF)
  and the ghost (0x510FDF), with no indestructible test and no number.
Only the animal and the villager (`VillagerDead` 0x75091D) count in the player's sacrifices; a tree counts none.

#### Deleting a site

`WorshipSite::ToBeDeleted` 0x77AA60, in order (before the CitadelPart 0x469540 and MultiMapFixed 0x52E2B0 parts):

1. Each icon in +0xE0 is deleted (`WorshipSpellIcon::ToBeDeleted` 0x77F230 → `RemoveSpellIcon` 0x77C450).
2. The dance (+0xA0) is deleted: `Dance::ToBeDeleted` 0x50B970 runs `RemoveFromDance(1)` (vt +0xB08 0x751510) and
   `SetStateAfterFinishingDance` (vt +0xB0C 0x759B80) on every member that is still available.
3. The altar's need signs (+0x90, the `ShowNeeds`, [below](#the-altars-need-signs)) are deleted with the same
   `now`; the link is not cleared.
4. If the totem (+0xDC) is available, its +0x100 is set to 0 and it is deleted. +0xDC itself is not cleared.
5. Every town of +0xA4 gets +0x98C (its worship site) = 0.
6. `fn_004637C0` clears the citadel's sites[+0x110].
7. The lists +0xA4 and +0xAC are emptied.
8. The food pot (+0xB4, with no availability test) gets `SetMultiMapFixed(0)` (vt +0x868) and is deleted
   (`Pot::ToBeDeleted` 0x66D110, which runs `Pot::RemoveReaction` 0x66D6A0 first); then +0xB4 = 0.
9. The mana path sprite at +0xB8 is deleted (`CreateManaPathSprite` 0x77B320 stores it; cleared here at 0x77ABEB,
   by `Process` 0x77B283 and by `SetToZero` 0x77AE6E).

<a id="the-altars-need-signs"></a>
### The altar's need signs

Three signs stand over every site's altar, rising with how much its dancers want food, how much they want rest, and
how many of its people are hiding. They are the same kind of object as the workshop's wood sign
([buildings](../world/buildings.md)) with the `GShowNeedsInfo` rows 0, 1 and 2. Read in the exe ✅ unless marked.

- **Made with the altar.** `WorshipSite::Create` 0x77AD53 makes the altar (`WorshipTotem::Create` 0x780930). Inside
  the altar's constructor 0x780840, after its `CitadelPart` (whose `Object` constructor takes the altar's creation
  index N) and after +0x100 = the site, `ShowNeeds::Create(altar)` 0x7808B0 is stored in the site's +0x90; only then
  the altar's `CallVirtualFunctionsForCreation` (0x780963) and `WorshipSite::Init` 0x77AD6C (the dance and the food
  pot). `ShowNeeds` (0x719AB0) is a `GameThing`: no creation index, no position. It makes, in order, one
  `ShowNeedsVisuals` for each of `GShowNeedsInfo::Infos` 0xD99738 + i × 0x114, i = 0, 1, 2 (the loop ends at +0x33C),
  at the altar's MapCoords (+0x14), owned (+0x60) by the `ShowNeeds`, stored at +0x18 / +0x1C / +0x20. Each is an
  `Object`, so the three take **N + 1, N + 2, N + 3**, before the dance and the pot. A sign starts with its desire
  (+0x5C) and Zoomer (+0x64..+0x90) at 0 (0x719D60); its own `CallVirtualFunctionsForCreation` 0x719E00 makes its 3D
  object with the row's mesh, +0x54 = 0 and scale 1.0, and does not call `Object`'s: no map cells, no other list. No
  random draw anywhere.
- **The rows** (`Scripts/info.dat`, type 25 ShowNeed, anim −1, MaxNeedValue 1.0 in all four):

  | Row | Name | Mesh | ShowNeedGreater | MaxHieght |
  |---|---|---|---|---|
  | 0 | Show Needs Hunger | 96 BuildingCitadelDesireHungry | 0.25 | 4.843 |
  | 1 | Show Needs Life | 97 BuildingCitadelDesireSleep | 0.25 | 4.843 |
  | 2 | Show Needs Hiding | 102 BuildingCitadelPopCounterBead | 0.1 | 4.81 |
  | 3 | Show Needs Workshop Wood | 175 BuildingDesireFlagWood | 0.0 | 3.9 |

- **Every turn.** `Citadel::Process` 0x462D70 runs every citadel part's `Process` (vt +0x5FC), its list +0x4C first
  (0x463068..0x463081), then +0x64; every `CitadelPart` constructor pushes itself at the head of +0x4C (0x469470), so
  the newest part comes first, and a citadel with no heart model goes straight there (0x462D7E). `WorshipSite::Process`
  0x77B1D0 ends with `fn_00719B90` on +0x90 (0x77B2B1), with no built test. There: an altar that is null or not
  available (vt +0x2C) deletes the `ShowNeeds` (`ToBeDeleted(0)`, its three signs too) and returns; otherwise each
  sign's desire (+0x5C, `fn_00719E80`) is set, in order:
  - row 0, `CalculateDesireForFood` 0x77C310: 1 − min((food + 0.0001) / (`CalculateFoodNeededByDancers` + 0.0001),
    1);
  - row 1, `CalculateDesireForRest` 0x77C390: no dance 0; else `Dance::CalculateAverageLifeDesiredByDancers`
    0x50BEB0, the sum over the dance's groups and their members of `CalculateLifeDesire` (vt +0xB10,
    `Villager` 0x75BAE0: 1.0 − its life), a float after every member, over the groups' member counts (0 with none);
    above 1 it gives 1 (`test ah, 0x41`: at 1 or below, or unordered, the average is computed again and returned);
  - row 2, `CalculatePeopleHidingIndicator` 0x77C3D0: no dance 0; else (the site's people +0xC8 − the dance's
    members +0x90) as an unsigned 32-bit count (`fild qword`), × 0.01, at most 1 (more, or unordered, gives 1).
  The original's own results for the rest and hiding desires were taken with the oracle (the functions run on fake
  sites and dances) and pinned in `test/test_altar_needs.cpp`.
- **Drawn.** `WorshipSite::Draw` 0x5193D0: a site not built (`WorshipSite::IsBuilt` 0x77BDD0: flag +0x58 & 2 clear
  and `GetPercentBuilt` ≥ 1) draws its construction and no signs; a built one calls `ShowNeeds::Draw` 0x5193FA
  **before** it looks at its own fire, so the signs show on a burning site. No distance or influence test of its own.
  `ShowNeedsVisuals::Draw` 0x719E90 is reached only from there and from `Workshop::Draw` (0x51CC67), and the sign's
  Zoomer steps (`SetDestinationWithSpeedAndTime`, then the advance by `g_game_time_inc` × 0.001) only inside it: the
  sign moves only on the frames its building's Draw runs. A building's Draw (vt +0x610) is called by the landscape's
  object draw list (`fn_005E5CD0`): on a rebuild frame (0x5E5D32..0x5E5D6A) the list `DrawObjects` 0xD1D28C is filled
  from the `GameBlock` object lists (0xD189D8, `GameBlock::Insert`) of the land blocks `LH3DIsland::PreDraw` chose
  (0xFA92D8, 0x7FF468) within the draw distance [0xC37200], plus the list at 0xD199E8; every listed object's Draw runs
  (0x5E6086) and the ones left on screen (`g_b_last_on_screen`) are marked active (0x5E6094); on the other frames only
  the active ones' Draw runs (0x5E60F3..0x5E610F). The other callers of vt +0x610 (`GLandscape::Draw` 0x5E4BC2, the
  super villagers; `Town::DrawDesireFlags` 0x73D8B5; the list at g_game +0x205CD4, 0x5E6165) are not a site's or a
  workshop's. So ✅ a site's signs step only on the frames the site is in the object draw list and drawn. The
  [object draw list](../rendering/object-draw-list.md) is ported (`ObjectDrawListSystem`).
  `ShowNeeds::Draw` 0x719C10 draws the signs 0, 1, 2 with `ShowNeedsVisuals::Draw(i)` 0x719E90, the workshop's
  function ([building sites](../world/building-sites.md)): the Zoomer, the height (value × MaxHieght), turned to face
  the camera, the small scale below (MaxNeedValue − ShowNeedGreater) × 0.2, the land's colour and fog, no alpha. Its
  point is the owner's `GetShowNeedsPos(i)`: `ShowNeeds` 0x719D30 → the altar's +0x100 → `WorshipSite::GetShowNeedsPos`
  0x77CD70 → `WorshipSite::GetSpecialPos(i)` 0x77CC90, the site mesh's **special point i** (only its x and z; the
  altitude is the sign's height). The workshop's sign stands at its point 9 instead (0x77A630).
- **Deleted** with the site ([Deleting a site](#deleting-a-site), step 3): `ShowNeeds::ToBeDeleted` 0x719B20 deletes
  the signs 0, 1, 2 with the same `now`, then itself; each sign frees its 3D object (0x719DD0).
- **Under the hand.** A sign is an `Object` (not a `MultiMapFixed`), available, never locked and never put in the
  hand: the hand is **Over Object (18)**, in the influence or out of it. Its tooltip comes in `CalculateCanSelectLock`
  0x5D72EB..0x5D735D after the leash's and before the citadel heart's and the town's, so before the influence test:
  0xE73 «Recoger» (pick up) by default; row 3 0xEFB «Madera Necesaria: %3.0f»; row 0 0xECE «Deseo Comida:
  %3.0f%%»; row 1 0xEFF «Deseo Descanso: %3.0f» (no per cent sign); row 2 keeps 0xE73 (the frozen game's
  `Scripts/InfoScript2.txt`, `HELP_TEXT_TOOLTIP_01`, `_137`, `_92`, `_141`; the id is 0xE73 + n − 1). Event 0, no button (the action the
  leash test left), align 0, not forced. The numbers (`CalculateOverObject`): 0xECE and 0xEFF the sign's Zoomer value
  (+0x64) × 100.0 (0x5CA5C6..0x5CA5FA, which also sets [0xD17BC4] = 1); 0xEFB the workshop's wood wanted
  (`fn_00779B20`, as an unsigned 64-bit number, 0x5CA1B6).
- **The leash** tied to one of these signs is on [the leash's page](../creature/leash.md#the-leash-tied-to-a-needs-sign).

### The icons and the charge

#### The worship site's icons

- **Creating one**: `WorshipSpellIcon::Create` 0x77F2B0 places mesh 203 in slot 10..15 with the site's scale and
  angle, and its `SpellSeedGraphic` above it (`SpellIcon::Create3DSpellObject` 0x726210). The `WorshipSpellIcon` is
  0x140 bytes (`SetToZero` 0x77F1F0); ctor 0x77F140 = SpellIcon(pos, info, seed, the site's scale, the site's angle);
  `SpellIcon::Create3DObject` 0x7261A0 = Game3DObject::Create(pos, 0, mesh 203, angle, scale);
  `WorshipSite::AddSpellIcon` 0x77C430 puts it at the head of the site's list; `CallVirtualFunctionsForCreation`
  0x77F290.
- **An icon of a site not built** ✅: `WorshipSpellIcon::IsBuilt` 0x77FEE0 is its site's `IsBuilt` (1 with no site).
  `SpellIcon::Draw` 0x519650 of an icon not built only calls `DrawBuilding`, at `GetPercentForDrawBuilding`, which a
  worship icon answers 0.0 (0x77F0C0): nothing is drawn, and neither the seed graphic (`DrawSpellSeedGraphic`, which
  also steps it) nor the magic system (the charge's ring) runs.
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

On every frame a built icon is drawn on screen (`SpellIcon::Draw` 0x519650 when `g_b_last_on_screen`, and
`TownCentre::Draw` for the slot icons, bw1-decomp `DrawingObject.cpp`), `SpellIcon::DrawMagicSystem` 0x726D20 →
`TChargingData::Draw` 0x7267A0, charging or not; it asks vt +0x914 `GetWorshipSpellIcon` (a worship icon itself
0x77F090; a town icon its site's icon of its seed, `TownSpellIcon::GetWorshipSpellIcon` 0x748F30) and stops at 0. The
ring's mesh is loaded by `SpellIcon::TChargingData::Init` 0x7266E0 (mesh 0x231, 561). The visual has four parts
(constants at 0x981A0C..0x981A4C: 0.1, 1, 3, 0, 1, 0, 255, 6, 1.5, 1, 0, 0.1, 0.1, 0.6, 0.2, 8, 1.5; [0x981AA0] =
205, [0x92A540] = 4π; peeked with the oracle):

1. **The fraction f** (0x7267F3..0x726871): 1 with a held seed, else `GetPower` 0x77FE80 when the store +0x134 is
   not 0 (`fcomp`, `test ah, 0x40`: a NaN store counts as 0). It is "full" when a seed is held or, with a store,
   `GetChantNeededForSpellSeed` 0x77FE40 is not above 0 (`test ah, 0x41`). While charging, f = (0.2 + f) / (0.2 + 1).
2. **The icon's tint**, icon +0x10C `SpecularColor` (0x726879..0x726A3F), set to 0 first: the player's colour
   (`GPlayer::GetPlayerColour` of vt +0x1C, the owner; the neutral player's black 0xFF000000 too, alpha kept) with each
   RGB byte × b >> 8 (the masks 0xFF0000FF / 0xFF0000 / 0xFF00, then >> 8). Full: b = round((0.1 + (0.6 − 0.1)·(sin(4π·phase)
   + 1)·0.5)·255) (`fistp`: to nearest), with phase the PSys global phase [0xD4EBF8] (`fn_0068F710`), which goes 0..1
   over 3.33 s; b runs 26..153 at twice the phase's rate. Else, while charging: s = (`GameTurn` g_game +0x205A40 −
   `GetTurnChargingStarted` 0x77F670 + `TurnFraction` +0x205D64) × `g_msPerTurn` [0xD01A38] × 0.001; for s < 1.5, b
   = round(clamp((1 − cos 8s)·0.5·(1 − s/1.5), 0, 1)·205) + 50 (a byte sum; a NaN goes to 0), two flashes; for s ≥
   1.5, b = 50 (0x7269EE: ×25 ×2). Neither: +0x10C stays 0. So the 0.1..0.6 pulse is the tint's brightness, not an
   alpha. **It is drawn one frame late**: `SpellIcon::Draw` reads +0x10C before `DrawMagicSystem` sets it; with it not
   0 the icon is placed with `SetPositionOnMap` and drawn with `CombineColorFromPos(0xFFFFFFFF, +0x10C)` (the land
   light × 255 / 256, the land's specular + the tint held to 255 a channel, then the haze), else `SetPositionOnLand`
   (the land light alone; the same altitude, both add the land's to Pos's). An icon not built (`DrawBuilding`) or off
   screen does not run it, so its +0x10C stays.
   - **Oracle** (`tools/oracle/scratch/icon_charge_draw.py`, the function itself with fake objects and its callees
     stubbed): red 0xFFFF4646 full at phase 0, 1/16, 1/8, 3/8 gives 0xFF581818, 0xFF852424, 0xFF982929, 0xFF190707;
     charging at 0, 1, 2, 3, 8, 10 turns gives 0xFF310D0D, 0xFF4E1515, 0xFF8C2626, 0xFFBF3434, 0xFF310D0D,
     0xFF581818, then 0xFF310D0D from turn 15. A sweep of 256 phases and 160 charge times matches openblack's
     float formula bit for bit (`icon_charge_port_tint.py`). f: 0.75 charging gives 0x3F4AAAAA (0.7917), an empty store
     0x3E2AAAAA (1/6).
   - **The running original** (a run of the worship lesson's demo, which taps the FOOD icon, with the icon's fields
     peeked, 2026-10-30): +0x120 Charging = 1, +0x138 = the start turn 17601, and at turn 17625 (2.4 s) +0x10C =
     0xFF310D0D, +0xC4 (the sheet's f) = 0x3E2AAAAA; with the store at 5250 of 7000, +0xC4 = 0x3F4AAAAA.
3. **A `LightSheet`** at `TChargingData` +4 (the icon's +0x8C; 0x726A45..0x726BA0), charging or not. Once (flag
   `TChargingData` +0): `Init(10)` ([the light sheet](hand-casting.md#the-light-sheet-as-read-from-the-exe): its shift
   period 0.03 s and slide speed 1), then point i (0..9) at the angle a = float(i) × 6.2831855 × 0.11111111 (i·2π/9,
   so point 9 is point 0 again) = (sin a)·row2 + 0·row1 + (cos a)·row0 + the position of the icon's 3D object matrix
   (+0x14..+0x40), each product and sum a float in that order (radius [0x981A30] = 1), then the sheet's spread +0x10 =
   1.5 ([0x981A2C]) and its height +0x14 = 6.0 ([0x981A28]) (written at `TChargingData` +0x14 / +0x18; the earlier
   "+0x18 = 6.0, +0x14 = 1.5" here read them as the sheet's own offsets). Every frame: colour +0x2C = the player's
   colour & 0xFFFFFF, the fed strength +0x38 = f, `Update(float(g_game_time_inc) × 0.001)` (no pause test: the frame's
   game ms is 0 in a pause), `GlobalTextures::GetLightSheetMaterial` into +0x58, and with `g_started_frame` one
   `LH3DZSorter::NewZObject` (callback 0x4246E0) at its middle's squared distance to the camera (the middle the last
   draw left). With f = 0 the sheet stands at no height and its light is 0, so an idle icon's sheet adds nothing.
   - **Oracle** (the same script, the flag cleared, `Init` stubbed): the points of the identity, of a matrix turned
     (0.8, 0, −0.6) at (1000.5, 20.25, 2000.75) and of the same at size 2 at (1234, 55.5, 987.25), and spread 1.5,
     height 6.0; openblack's sum matches them bit for bit (`icon_charge_port_sheet.py`). The running original's icons
     all hold 1.5, 6.0, 0.03, 1.0 at the sheet's +0x10..+0x1C and their player's colour 0x00FF4646 at +0x2C, and their
     sheets' time (+0x30) grows from the start, charging or not.
4. **The pulse ring**, while charging (mesh 561 `MSH_S_PULSE_IN`, 0x726BA5..0x726D11): t = fn_0068F720(0.1) / 1.0,
   where fn_0068F720(x) = (((phase > x) ? phase : phase + 1) − x) × 3.33; drawn for 0 ≤ t ≤ 1 on the icon's matrix,
   raised (0·(1 − t) + 0.1, [0x981A38]) × twice the mesh's +0x28 along its second row, scaled 3(1 − t), in the
   player's RGB, with **alpha 255·t** (0x726CDD..0x726D07: (0 − 255)(1 − t) + 255).
   In detail: fn_0068F720 adds 1 when the phase is not above 0.1 (`test ah, 0x41`), each step a float; the ring is
   skipped for t < 0 and t > 1. With u = 1 − t (a float): the matrix of the icon's 3D object copied into the ring's
   (`rep movsd`, 12 dwords from +0x14), its position += r × row1 with r = (u·0 + 0.1) × (h + h), h the icon mesh's
   +0x28 (vt +0xF8 `GetMesh`; the half height of its box, unscaled), then its nine cells × s, s = ((1 − 0)·u + 0) × 3;
   the ring's colour +0x4C = (the player's colour & 0xFFFFFF) | (`__ftol`((0 − 255)·u + 255) & 0xFF) << 24 (cut, not
   rounded); its specular is the object's own, 0 from `LH3DObject`'s constructor; then its vt +0x100 draw.
   - **Oracle** (the same script; an icon turned (0.8, 0, −0.6) at (1000.5, 20.25, 2000.75), mesh +0x28 = 2.5): drawn
     at the phases 0.10001 to 0.4003 (t = 0.99998: alpha 254), not at 0, 0.05, 0.1, 0.45, 0.9; at 0.25 the cells are
     row0 (0x3F99C0EB, 0, 0xBF66A161), the position (1000.5, 20.75, 2000.75) and the colour 0x7FFF4646. openblack's
     `charge_visual::Ring` / `RingModel` match all of them bit for bit (`icon_charge_port_ring.py`).

#### The player's view of its icons

GPlayer functions over all its icons: `fn_0064BB10` = the largest charge fraction of the icons charging for the player
(per icon `fn_0077F690`); `fn_0064BDE0` = some icon is `ValidForRequestSpell(status, −1, 1)`; `fn_0064BDB0` = packet
0x25 → 0x5DABA0, which calls the best icon's `fn_0077FB40(status, −1, 1)`; `fn_0064BD90` / `fn_0064BD50` (packet 0x26
0x5DABF0) = the same with the last seed type; packet 0x6A 0x5DAC30 → `fn_0077FC30`;
`GPlayer::CancelAllSpellsCharging` 0x64BC60 (CLEAR_PLAYER_SPELL_CHARGING: at that player's temple, every icon
`IsCharging(status)` with this computer's `MyInterfaceStatus` (0x70CD8A) is cancelled for it, so only the local
hand's charges stop; openblack passes the local player, `worship::player::CancelAllSpellsCharging(player, local)`); `IsAtLeastOneSpellBeginToBeCharged`
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
- **The gesture icons** (sub_68B7E0): one graphic per magic type drawn at once with the icons' own camera
  (`DrawSpellGraphic` with draw now set): [the gesture icons](gestures.md#the-gesture-icons).

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
  accepts those flagged 0x200, the disciples: it calls `IsAvailableForWorshipSite(allowDisciple)` 0x752820 with 1;
  nothing else differs between the passes):
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
- **Never read** ✅: the town info's `maxDistanceForVillagersToGoToTheWorshipsite` (+0x11C, 1000) and
  `minLifeForVillagersToGoToTheWorshipsite` (+0x120, 0.4). No instruction refers to `GTownInfo::Definitions` + 0x11C or
  + 0x120, `xref.py field` finds no read of them through a town's info in Town, Villager, WorshipSite, Citadel,
  CitadelHeart or BuildingSite, and of every function's C only other infos' +0x11C / +0x120 are read. The distance
  read is +0x148 `maxDistanceThatVillagersWillGoToWorship` (below), the life the villager info's
  `damageThresholdToGoHome` (+0x35C, above). openblack reads neither field either.
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
  place (state 60); `Villager::StartHidingAtWorshipSite` 0x76C550 sends it to the site's **door**: vt +0x864 is
  `GetDoorPos`, which `WorshipSite` does not override, so it is `MultiMapFixed::GetDoorPos` 0x52E370 (the mesh's door
  point through `Game3DObject::GetDoorPosition` 0x63AFE0, else, with none or an x or z of 0, the site's position).
  It walks there along the footpath (`SetupMoveToOnFootpath`, state 213) and is added to the site only if the walk
  was set up. `HidingAtWorshipSite` 0x76C5E0 reads the same door: within 1 of it (`AreWeThere`, vt +0x85C with
  `push 0x3F800000` at 0x76C621: the squared whole-unit distance strictly below (speed + 1)², so its speed plus one
  map unit, 1/6553.6 m) it worships (`ProcessInWorship`), else `SetupMoveToWithHug(door, 213)`. openblack does the
  same (`villager_worship::IsAtHidingPlace`), with the door as the target.
- Eating at the site: `Villager::CheckAllowedToRestAtWorshipSite` 0x76C9A0 (`ProcessInWorship` asks it with the walk,
  the altar rest's 62 and 63 without) compares the belly (+0xE8, `fld`) with the info's `hungryForFood` (+0x2C0)
  strictly: `fcomp`, then C0 (`test ah, 1`) at 0x76C9B8..0x76C9C5, so a belly at the level is not hungry (the needs'
  `IsHungry` is `<=`). The meal (`GetAmountOfFoodRequiredForMeal` 0x75BC00) is compared with the site's food
  (`GetFood` 0x77BDB0, 0 without a site) as signed numbers (`cmp`, `jl`, 0x76C9DE). With the walk:
  `SetupMoveToWithHug(site->GetResourceNearestEdge(FOOD, villager, 1), 241)`: the food pot's nearest edge
  (`WorshipSite::GetResourceNearestEdge`, vt +0x8D4), or `GetFoodPos` without a pot.
- State 241 `GetFoodAtWorshipSite` 0x76CA30: at the edge (`AreWeThere`, 0) the meal from the site (`GetResourceFrom`
  0x753390) when it is not 0, `EatFoodHeld`, then `PlayAnimThenSetState(65)`; else the walk to the edge again; no site:
  `SetTopState(163)`. State 65 `RestartWorshippingAtWorshipSite`: `StartWorshippingAtWorshipSite`, else 163.
- Their exit `ExitGetFoodAtWorship` 0x76C280 (rows 65 and 241): for a state with another exit (vt +0x96C), it reads
  the state table's record at +0xE8 (`Infos` + state × 0x114, 0x76C2A6); the records sit 0x10 behind the file's
  (`IsReactiveState` reads +0xC8 for the file's 0xB8 the same way), so this is the file's 0xD8, set for 58, 59, 60, 213
  and 241 and clear for 61-65 in the shipped info.dat. Clear: `RemoveVillagerFromWorshipSite`.
- The altar rest (`GotoAltarForRest`, 61: along the footpath to `GetRestPos`, point 7, then 62; 62 and 63 rest on while
  `CheckAllowedToRestAtWorshipSite(0)`, then `AtAltarFinishedRest` 64: back into the dance, else `DecideWhatToDo`).
  Nothing in the game sends a villager to 61.
- The supply errands (42-46, exit `ExitMoveToWorshipSiteWithSupplies`: the site's food on the way, `ResourcesOnTheWay`,
  goes down by the food held, 0x77C4D0): `GotoStoragePitForWorshipSupplies`, the satisfier of the town's supply desire,
  wants a functional storage pit, a site and `Town::GetFoodForWorshipSiteIfEnough` (the site's
  `CalculateFoodNeededFromTown`: the dancers' need, cut to a whole number, less the pot and the food on the way, at
  least 0; none when `CalculateFoodSpareForWorship` 0x747A40, the pit's food less the town stats' food for dinner
  (+0x6F4, `fild`, `fsub`, `__ftol`), is below it). `GotoWorshipSiteWithSupplies` (44) returns 0, so no supplies set
  off; 45 returns 1; 46 puts what it holds in the site through `AtStructureAddResource` and takes it off the food on
  the way (0x76C2D0).
- The walk part of `GotoWorshipSiteForWorship` 0x76BCC0 sets off only when the town has a site and the site's vt
  +0x890 answers yes (0x76BCEA): in `WorshipSite`'s vtable (0x8F2840 + 0x890) that slot is `WorshipSite::IsBuilt`
  0x77BDD0, the `CitadelPart` test (flag 2 at +0x58, under construction, clear and `GetPercentBuilt` +0x5C ≥ 1.0,
  `test ah, 1`), then a walk of a list at +0xE0 that changes nothing, then 1. Not built: nobody on the way, the state
  is not set, and the disciple part below still runs. openblack sent villagers to a site still being built until
  2026-10-20 (ours, from the exe; `abode_queries::IsBuilt`).
- A worship disciple: `GotoWorshipSiteForWorship` 0x76BCC0, after the walk part and whether or not it set off
  (0x76BD67), with disciple type 11 calls `SetVillagerDisciple(0, NONE, 0)` and, with a town and the walk set off,
  `Town::IncrementWorshipPercentage` 0x73C650: `GetWorshipersNeeded(1, 1)` > 0 → nothing; the people (+0x618 + +0x61C,
  `fild` qword) 0 → nothing; else `fild` qword of the town's villagers on the way (+0x5CC), `fiadd` of its
  worshippers (+0x5C4), `fild` of the needed count, `fadd` 1.4f, `faddp`, `fdivrp` by the people, below 1 stored, else
  1, then `SetWorshipPercentage`.

#### Every turn at the site

`Villager::ProcessInWorship` 0x76C890 every turn:

- `Villager::CheckVillagerGoBackToTownFromWorship` 0x76BEC0: the site gone or no longer the town's player's: state 163;
  fewer villagers needed and first in the go-home queue: state 248, through `SetTopState(0xF8)` (vt +0x8E8) at
  0x76BF59..0x76BF6D, which counts as left when it returns 1, a pause first (239 with FINAL 248,
  `Villager::SetTopState` 0x7520A2) included.
- Not called: `CheckRequestGoHome` 0x76C8D0 (with life < `damageThresholdToGoHome` 0.3 it would sign up in the queue,
  sorted by the life desire `GetLifeDesireFromLife` 0x75BBC0) is not among the three steps (0x76C890..0x76C8C9) and has
  no caller (see the go-home queue above).
- `ReduceVillagerLifeByChant` 0x76C800: `life -= chantDamage × chantLifeRate`, 5e-6; on reaching 0 it dies with reason
  4 and is counted by `GET_TOWN_WORSHIP_DEATHS`. The rate is the villager's own info row's (`[esi+0x28]` +0x28C at
  0x76C817..0x76C81C, multiplied after the site's +0x104); info.dat has the same 5e-6 in every row.

#### Leaving the site's states

- `Villager::ExitMoveToWorshipSite` 0x76C170, when it leaves for a state with another exit (vt 0x96C) that is not a
  teleport reaction (exit `ExitReactToTeleport` 0x766390, states 201, 202, 251, tested at 0x76C197), takes it off the
  town's list of villagers on the way and clears its flag 0x10.
- `Villager::ExitAtWorshipSite` 0x76C1F0, when it leaves for a state with another exit, takes it off the go-home queue.
  For a state that is not reactive (the state table's +0xB8, `IsReactiveState` 0x7525B0) and not 241 (eating at the
  site) it calls `RemoveVillagerFromWorshipSite`; else it only comes off the site's count (0x77D0A0) and out of the
  dance (vt 0x978 `IsDancing`, vt 0xB08 `RemoveFromDance(1)`), and stays counted in the town's worshippers (+0xE0 bit
  0x2). The site's count comes off only with a site (`test edi, edi` at 0x76C24C), but the dance test at 0x76C258
  does not depend on it: `Living::IsDancing` 0x5ECC10 reads the villager's own dance (+0xD8), so a dancer whose town's
  site is gone still leaves the dance. `RemoveVillagerFromWorshipSite` 0x76C440 does the same with a town (0x76C488)
  and, with no town (0x76C44A), only clears the flag 0x2 and +0x118. openblack's `site::LeaveDance` takes the villager out of whichever site's dance
  holds it.

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
     0.001), it waits; otherwise +0xCC = +0xC4 = 0 (0x722A78..0x722ABF). The touch (read 2026-10-24): `IsTouching` is
     `GetDistanceFromObject` (vt +0x6C4, `Object::GetDistanceFromObject` 0x637FB0 for the dispenser, an Abode) ≤ 0.001
     (`test ah, 0x41`), which is `GUtils::GetDistanceInMetres` of the two map positions (+0x14) less the orb's and the
     dispenser's `Get2DRadius` (vt +0x64), so the orb stays while it is within both radii of the dispenser's centre
     across the land, whatever its height. openblack: `magic::OrbTouches` from `OrbStillOnDispenser`
     (`src/Worship/SpellDispenser.cpp`, with `ecs::IsAvailable`); test
     `DispenserRules.TheOrbFloatsAboveItsDispenserAndStaysOnItWhileTheyTouch`. When the period has passed and no orb
     could be made, the count is left as it is (no reset after `CreateOneOffSpellSeed` 0x722B0F), so the next pass asks
     again.
  2. It also needs IsActive (vt 0x40C), a magic (+0xD4), IsBuilt (vt 0x890) and IsRepaired (vt 0x88C)
     (0x722AC0..0x722AF2). The dispenser's slots are the abode's (read 2026-10-24): `IsActive` 0x7226C0 (+0xD0),
     `Abode::IsBuilt` 0x4016C0 (not flagged under construction, +0x58 & 2, and `GetPercentBuilt` +0x5C ≥ 1) and
     `Abode::IsRepaired` 0x4016A0 (`GetPercentRepaired`, vt +0x884 → vt +0x11C, ≥ 1). openblack: `abodes::IsBuilt`
     and `abodes::IsRepaired` gate `magic::StepDispenser` (`src/Worship/SpellDispenser.cpp`); test
     `DispenserRules.ADispenserCountsOnlyWhileItsBuildingIsBuiltAndRepaired`.
  3. Then ++ +0xC4, and at ≥ +0xC8 (the period) `CreateOneOffSpellSeed` 0x722B80 (0x722AF4..0x722B0F) →
     `OneOffSpellSeed::Create` at its position + 1.2 × its height, site visual 9.
- **The period**: the default is `timeEachMobileObjectTakesToProduce` = **300** turns; `SET_MAGIC_PROPERTIES` 0x70CC30
  and `SET_TIMER_TIME` 0x711280 change it in seconds (× turns per second). With `SET_MAGIC_PROPERTIES` a period of 0
  disables it; `SET_TIMER_TIME` keeps a period of 0 out (see [the script](#script)).
- **Activating**: `fn_00723030` sets the dispenser active; activating it makes an orb at once.
  `CREATE_SPELL_DISPENSER` writes +0xD4 = magic, `fn_00723030(1)`, +0xC8 = ftol(period) in turns; with 0 it calls
  `fn_00723030(0)`.
  The handler is 0x716881: the abode by name (`GAbodeInfo::GetInfoFromText` 0x405A70), `GetScriptPos`, the magic by
  name (0x5FB3B0, not checked: 42 when none), `FindTownWithID`, else the nearest town (0x552FF0), then
  `SpellDispenser::Create` 0x7228D0 (pos, abode, town, the line's angle and scale floats, 1.0 and 0), written as the last created object,
  null too (0x716926..0x71692C); none stops there. Then +0xD4 = magic, `fn_00723030(1)` (+0xD0 = 1 and, being
  on, `CreateOneOffSpellSeed` 0x722B80: the orb at once), +0xC8 = `__ftol`(period) (0x71694B, truncated), and with
  0 `fn_00723030(0)` (+0xD0 = 0 only; the orb stays). openblack: tests
  `LandScriptMagicObjectsTest.CreateSpellDispenserGivesItsMiracleAnOrbAtOnceAndItsPeriod` and
  `LandScriptMagicObjectsTest.CreateSpellDispenserWithNoPeriodMakesItsOrbAndIsSwitchedOff`.
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
  - The handlers (the callback's table 0x717EBC, indexed by the command; frozen v1.42 exe): 10 and 12 at 0x71594A
    take the seed by name (0x72B170: `_stricmp` with each seed's name, 30 when none, not checked), then
    `FindTownWithID` (none: nothing), `Town::AddMagicTypesHeld` of the seed's base magic (+0x124) and, when the town
    has its village centre (+0x9A4), `TownCentre::AddSpell` 0x744050 of the seed, also when the town held the magic
    already. 11 at 0x7159C0: `FindTownWithID` first, then `GMagicInfo::GetInfoFromText` 0x5FB3B0 (`_stricmp` with
    each magic's effect name, 42 when none); a magic of 0 or less or from 42 on does nothing; then the base of
    `GetFirstSpellSeedForMagicType` 0x72B090 (-1 when none, not checked; `GetMagicTypeFromPULevel(-1)` 0x72AFC0),
    `AddMagicTypesHeld(magic)`, and `AddMagicTypesHeld(base)` only when `IsMagicTypeHeld(base)` 0x73D630 is false.
    14 at 0x7158FB: the seed by name, `FindTownWithID`, `AddMagicTypesHeld` of the seed's base magic, nothing else.
    13 is 0x717E8A, the common "return 0". None of them writes the last created object. openblack: tests
    `LandScriptTownSpellCommandsTest.*` (`test/test_land_script_town_commands.cpp`).
- **`CREATE_PLANNED_WORSHIP_SITE` does nothing** ✅ (frozen v1.42 exe). It is entry 23 (0x17) of the land script's
  command table at 0xC20F00 (16 bytes an entry: the name 0xC21C58, the arguments "ANLLNN"; entry 22 is
  `CREATE_WORSHIP_SITE`). The command handler 0x715180 first formats the line's debug echo from its arguments
  (0x7151E1..0x71535E, `sprintf` only, nothing drawn or looked up), then switches on the command (`cmp eax, 0x68`
  0x715365, table 0x717EBC). Entry 23 of the table (0x717F18) is 0x717E8A, the handler's common "return 0" exit, the
  same one every failed check of the other cases jumps to; entry 22 is 0x7160C8. So the line makes no object, calls
  no `GetScriptPos`, draws no random number and leaves the last created object [0xD99380] / [0xD99384] as the
  previous command left it; the handler returns 0, as `CREATE_WORSHIP_SITE` does (0x716142). Nothing else names the command:
  `WorshipSite::SaveObject` writes only `CREATE_WORSHIP_SITE`, for a built site that is not under construction, and
  the only maker of a `PlannedWorshipSite` (ctor 0x77BEA0) is `WorshipSite::ConvertToPlanned` (0x77C04B). No shipped
  land script uses the command (`Scripts/Land1`–`Land5`, `LandT`, `demo2`, `comp`, `dance` and the playgrounds). A
  site still to be built comes only from a citadel's request (`Citadel::RequestANewWorshipSite`, see
  [the temple and citadel](temple-and-citadel.md)).
- **CHL natives**: 330 `IS_SPELL_CHARGING` 0x70CB80, 331 `IS_THAT_SPELL_CHARGING` 0x70CBD0, 355 `GAME_SET_MANA`
  0x6FE800, 356 `SET_MAGIC_PROPERTIES` 0x70CC30, 376 `SET_CAN_BUILD_WORSHIPSITE` 0x6FEC40, 386 `SET_MAGIC_IN_OBJECT`
  0x6FF0B0, 410 `GET_TOWN_WORSHIP_DEATHS` 0x6FF640, 422 `GET_MANA` 0x6FE8C0, 423 `CLEAR_PLAYER_SPELL_CHARGING` 0x70CD80
  and 453 `GET_SPELL_ICON_IN_TEMPLE` 0x6F3590; plus the dispenser branches of `SET_ACTIVE` (255) and `SET_TIMER_TIME`
  (145) and the `CREATE` types 30 `ONE_SHOT_SPELL`, 31 `ONE_SHOT_SPELL_IN_HAND` and 36 `SPELL_DISPENSER` (`GScript`
  0x6F1010).
- **`SET_ACTIVE` and `SET_TIMER_TIME` on a dispenser**: `SET_ACTIVE` 0x6FD720 acts on a dispenser through
  `fn_00723030`. `SET_TIMER_TIME` 0x711280 does not: on a dispenser (vt +0x1F4, 0x71132A) it computes ftol(1000 /
  [0xD01A38] × seconds) inline (0x711338..0x711359) and writes the period +0xC8 only when that is above 0 (unsigned,
  `jbe` 0x711360..0x711362). A shorter time leaves the period as it was, and the active flag is never touched. The
  turns per second are an unsigned division (`div` at 0x71133F), loaded as a 64-bit integer (`fild qword`) and
  multiplied by the float seconds; a negative time gives a negative `ftol`, which the unsigned test takes as a large
  period (−1 s at 100 ms a turn: 0xFFFFFFF6 turns). `SET_MAGIC_PROPERTIES` 0x70CC30 does the same sum (0x70CCD7..
  0x70CCF8) for a time above 0 and otherwise takes the building's `timeEachMobileObjectTakesToProduce` (info +0x1AC,
  0x70CD18); it writes the period whatever it is and turns the dispenser off (`fn_00723030(0)`) only for a period of
  0, never on (read 2026-10-24). openblack: `dispenser::SetTimerTime` / `SetMagicProperties`
  (`src/Worship/SpellDispenser.cpp`); tests `MagicSystemTest.AScriptTurnsOnlyADispenserOffAndSetsItsTurnsUnlessNone`
  and the `SET_MAGIC_PROPERTIES` rows of `test/test_miracle_core_rows.cpp`.
- `GET_SPELL_ICON_IN_TEMPLE` → `fn_00463AD0`: the site icon of that magic, in any of the sites.
  - ✅ The miracle (the second POP, 0x6F35BB) goes to `fn_00463AD0` unchecked, through
    `WorshipSite::GetSpellIconFromMagicType` 0x77B1B0 to `GSpellSeedInfo::GetFirstSpellSeedForMagicType` 0x72B090,
    which compares the whole dword with each seed's four miracle slots (`SpellSeedIsOfMagicType` 0x72B060, +0x124 and
    +0x128..0x130). Nought (`MAGIC_TYPE_NONE`) finds the first seed with an empty slot, and that seed's icon is given
    when a site has one; a value no seed lists (42 or more, a negative one) finds no seed (-1) and gives 0. openblack
    the same since 2026-10-26 (before, a miracle of 0 or less or of 42 or more gave 0 with "Invalid Temple"); test
    `ScriptQueryNatives.GetSpellIconInTempleLooksUpAnyMiracleValue`. The shipped script's one call passes a real
    miracle.
  - ✅ An icon found is added to the scripts' table as a found thing (`AddScriptGameThing(icon, 0)`,
    0x6F3617..0x6F361A) and its id pushed as an object (type 4, 0x6F3628); none pushes 0 and adds nothing. openblack
    the same since 2026-10-26 (`ScriptObjects().Register(icon, false)`; before, the icon was given without a place in
    the table); test `ScriptQueryNatives.GetSpellIconInTempleIsTheIconOfTheMiraclesSeedInAnyOfTheSites`. The one
    shipped call is in Land 2's Impress Village gold scroll (Khazar's wood
    miracle).
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
- **`GET_HELP`** ✅: 248 (`GScript::GetThingHelp` 0x6FCF50, frozen v1.42 exe) POPs the object and looks it up the
  same way; none is "Object no longer valid" (0xC0D428) and PUSH 0.0 (type 2). A thing that is not a reward chest
  (vt +0x3F4 `IsReward`) first gets the message "Not reward!" (0xC0D930, 0x6FCFAD) and the code goes on. A reward
  chest gives its info `[+0x28]` +0x120, `GRewardInfo::helpTextEnum` (+0x110 after the `GBaseInfo`), loaded
  unsigned and PUSHed as a float (0x6FCFD2..0x6FCFFC). Anything else then takes GET_FIRST_HELP's two branches,
  byte for byte the same: a dispenser (vt +0x1F4) its miracle's effect +0xF8 (0x6FD006..0x6FD049), any other object
  (vt +0x460) its info +0x50 (0x6FD053..0x6FD08C), a thing that is not an object 0.0 with no message (0x6FD05F →
  0x6FCF80). Its only callers are `Reward.txt`'s `displayrewardhelp` (L23, ip 3112) and `FirstReward` (L181,
  ip 3395), once the player has clicked a chest (`GAME_THING_CLICKED`), on the chest; the text goes to the dialogue.

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

### The sacrifice at the altar

`ecs::altar_offering::Offer` (`src/ECS/AltarOffering.cpp`), with the rules of `ecs::altar_sacrifice` (taken from
raffclar's #551, read in the v1.42 bytes for every kind, [above](#a-sacrifice-at-the-altar)): the value, the site's
battery and sacrificed total, the deed, an animal's count in its giver's sacrifices (`game_stats::Sacrificed`, before
its alignment; test `VillagerDeathTest.AnAnimalSacrificedAtTheAltarCountsOneSacrificeForItsGiverAndATreeNone`; the
Temple debug window's Statistics shows the count), the death, and,
for a villager, the light from the top of the altar (`RaiseLight`: OBJECT_APPEAR at `MapCoordsOf(altar)` raised by
`GetHeight`, magnitude 1, for the site's player through `SetSpotVisualPlayer`), then the sounds at the hand, then,
given by the local hand, the prayer power floating up in red (`FloatPrayerUp`, `FloatingNumberArchetype`, "%3.0f") from
the land under the pointer, read as the believers gained read it (`GetHoverLandPoint` at the land's height: the
interface's MapCoords +0x3B8 with its altitude +0x3C0 taken as on the ground, **🔍 inferred**). A tree or a dead tree
leaves its ghost (`object_ghosts::Add`, the original's `GoolooGooloo` 0x5E6540, called by `Tree::ApplyThisToObject`
after the prayer power and before the tree goes, 0x74BE21, and by `DeadTree::ApplyThisToObject` 0x510FDF) where the
hand still draws it.

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
  functions (capacity, battery size, draws, strain, icon share) are still ours, the same expressions as his.
- **The site's artefacts**: at the end of the site's turn, every thousand turns, `ArtefactSystem::PowerUpSite` raises the
  worth of the artefacts put down in the site by how many dance at it (the pure `artefact_rules::SitePowerUp`, tested
  in `test/test_artefact_rules.cpp`; the site's turn by
  `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople`); the rest of the
  artefacts' rules are the town's.
- **The dance** (ours, from the exe, 2026-10-29): `site::CreateDance` makes the site's dance with its altar, as
  `WorshipSite::Init` 0x77AEE0 does (only once its totem is made, `WorshipSite::Create`): the row `GDanceInfo[19 + slot]` (CitadelDance1..6, the files NewWorship2,
  NewWorship1, NewWorship, again), about point 8 (`GetDancePos`), the site its target, no duration, no footpath
  ([dances](../beings/dances.md)), facing the site's angle (`Dance::GetAngle` 0x50C310 → `GetAngleFromCitadel` 0x77BD60,
  the heart's angle + slot × 2π/7, the site's `yAngle`); then `SetWorshipSpeed(0.5)`. `site::SetDanceIntensity` is
  `WorshipSite::SetWorshipSpeed` (start above 0, stop at 0, `dance_rules::SetWorshipSpeed`); the battery reads the
  dance's speed (`GetWorshipSpeed`, 0 without a dance). `StartWorshippingAtWorshipSite` 0x76C4C0 joins it through
  `FindDanceGroup` (`dances::AddDancer`; no group, no worship) and walks to its place in its group's shape
  (`CalculateDancePosition` 0x597F20, `dances::DancerTurnOf`); state 60 (0x76C680) takes one off the dance's
  `NumDancersExpected` on its first turn (raised by `GotoWorshipSiteForWorship`), then, when `ProcessInWorship` returns
  1, `PerformDance` 0x5EF9F0 with the group's own move (`villager_dance::PerformDance`: the facing, the walk back to
  its place through state 154), returning its result. The clips are the dance's (`villager_dance::DanceClip`). The
  dancer count, the food and rest the dancers want walk the dance's groups (`Dance::NumDancers`,
  `CalculateFoodNeededByDancers` 0x50BF20, `CalculateAverageLifeDesiredByDancers` 0x50BEB0). The site's deletion
  deletes its dance (`Dance::ToBeDeleted` 0x50B970: each available dancer leaves it and goes to 163). A loaded game's
  site dance is linked back to its site by the dance's target. Tests `WorshipTest.ASitesDanceIsTheCitadelDanceOfItsSlotAboutItsCentreAndGoesAsHardAsItsDancersChant`,
  `WorshipTest.ASitesDeletionEndsItsDanceAndItsDancersFinishDancing`,
  `WorshipperTest.AWorshipperDancesInItsGroupOfTheSitesDanceAndWalksBackToItsPlace`. The debug bar's Magic window,
  under Worshippers, shows each site's dance (its row, state, speed, rate, clock, groups, dancers and those on the
  way).
- **The resources a site takes**: `worship::site::AddResource` (the pure `AddResourceRouteOf`), reached from
  `object_resources::AddResource`, so an object a worship site takes (a thrown animal or pot of food, an object given)
  fills its food pot; the food pot's add is `pot_resource::AddToPotDirect` (the pile sound, the cap, the poison, the
  size). Taking is `worship::site::RemoveResource` (the same route: the building site's take, or the food pot's
  `object_resources::RemoveFromPotDirect`), reached from `object_resources::RemoveResource`; reading is
  `GetFoodResource`, reached from `object_resources::GetResource` (test `TakeResourceTest.AWorshipSiteGivesTheFoodInItsPot`).
  The food pot is part of the site: emptied, it stays, and what it cannot give the site is asked for
  ([objects](../world/objects.md#worship-food-pot)).
- **The altar's build** ✅: the altar takes its own `CitadelPartBuild` as it is made (`CreateTotem`, 0 % built, under
  construction when its site is), `abodes::BuildBy` builds it after its site by the same amount while it is available,
  and `abodes::RedrawConstruction` keeps it `NotDrawn` until it is built; a loaded altar takes its saved percent and
  flags. Test `WorshipSiteBuildTest.TheAltarIsBuiltWithItsSiteAndIsDrawnOnlyOnceBuilt`. The Temple window's Altar need
  signs section shows each site's percent, its building site and builders, and its altar's percent and whether it is
  drawn. The altar is also part of a temple for the creature's kept area, as the original (`IsCitadelPart` 0x464B00
  answers 1 and is the altar's too, vtable slot +0x35C).
- **The footpath to the site**: when a town joins a site, the site gets a footpath link and, unless one of its
  footpaths already ends at the town's storage pit, a footpath from the pit is planned over a few turns, as the
  original (`WorshipSite.cpp`, through `ecs::footpaths`); the path-finding debug window shows how many are being
  planned.

### The altar's need signs in openblack

✅ The three signs, as [the original](#the-altars-need-signs):
- **Made** by `CreateTotem` (`Worship/WorshipSite.cpp`) right after the altar, with `ecs::show_needs::Create` (rows
  0, 1, 2, owner the altar, at the altar's position): each takes the next creation index after the altar's, before the
  dance and the food pot. The site keeps them in `WorshipSite::needsSigns` (the original's +0x90).
- **Every turn**, `worship::citadel::Process` ends with the citadel's sites, newest first (by creation index),
  and `worship::site::ProcessNeedsSigns` sets the three desires: `CalculateDesireForFood`, `CalculateDesireForRest`
  and `CalculatePeopleHidingIndicator`, whose rules are the pure `worship::altar_needs` (`Worship/AltarNeeds`). An
  altar no longer available deletes them (openblack then forgets the links; the original keeps them).
- **Drawn** from the object draw list, as the original: when the list runs a workshop's or a worship site's draw
  (the draw list's consumer, `Game.cpp`), `show_needs::DrawFor` (`ECS/ShowNeeds.cpp`, the workshop's sign's code)
  steps that building's signs' Zoomers, turns them to the camera and marks them drawn; a built site's three stand at
  its special points of their rows (`show_needs::NeedsPoint`), on fire or not. `show_needs::EndDrawnFrame`, after
  the list, gives the drawn ones their Mesh and takes it from the rest, so a sign whose building is not drawn neither
  shows nor moves.
- **Deleted** by `worship::site::ToBeDeleted`, after the dance and before the altar.
- **The hand** over a sign is Over Object (`HandSystem::InterfaceHandState` takes a sign under the cursor as the
  interface's object) and shows `hand_tooltips::NeedsSignToolTip`, the workshop's sign's 0xEFB too.
- **The debug window**: Temple, Altar need signs (closed by default) shows each sign's desire, how full it shows and
  whether it is drawn. The testbed's `worship.altar_needs` shows the hunger sign rising over the dancers' site.

The creation indices: every site made now takes three more after its altar, so everything made after a site has an
index three higher than before (a mover of the state hash wherever a site is made).

### The icons and the charge in openblack

- An icon is under the hand through its seed graphic's sphere, twice the graphic's size, as the original's
  ([the pick in openblack](../hand/picking.md#the-pick-in-openblack)).
- An icon of a site not built yet draws nothing, as [the original](#the-worship-sites-icons): `worship::Update` calls
  `icon::UpdateBuiltDraw` for each site icon every frame, which marks the icon, its seed graphic and bands `NotDrawn` and
  takes the holder effect out of the draw while `icon::IsBuilt` is false; `seed_graphic::UpdateIconGraphics` neither
  steps nor draws such an icon's graphic, and `UpdateChargingVisual` draws it no ring. Tests
  `WorshipIconBuilt.AnIconDrawsAsBuiltOnlyWithItsSiteBuilt`,
  `WorshipSiteBuildTest.ASitesIconAndItsSeedGraphicAreNotDrawnUntilTheSiteIsBuilt`.
- Before, openblack painted the icon's seed half transparent: corrected. The game flag 0x2000 is
  `OPENBLACK_INFLUENCE_EVERYWHERE`. The worship icon's tap sound is in `Worship/WorshipSpellIcon.cpp`.
- The icon rings step 15 m up to 30 m as the original's `GetSpellIconPos` (0x77B100 / 0x77B10A) (`k_IconRingStep`
  and `FindIconPosition`, `WorshipSite.cpp`), and the push is in metres: ✅ faithful.
- `icon::UpdateChargingVisual` (`WorshipSpellIcon.cpp`), every frame for every worship icon:
  - ✅ **the icon's tint**: `icon::ChargeDrawingOf` (`charge_visual::Level`, `ChargeSeconds`, `Tint` in
    `Worship/IconChargeVisual.cpp`) sets `SpellIcon::chargeTint`, and the icon is drawn with the one set on the frame
    before (`SpellIcon::specularColour`), as the original's draw reads it before setting it; an icon not built keeps
    its colour. The renderer draws a worship icon whose colour is not 0 with the white tint and that colour added to
    the land's specular (`DrawColoursOf`, `RenderingSystem.cpp`). Tests `IconChargeVisual.*`
    (`test/test_icon_charge_visual.cpp`), pinned to the oracle; readout: the Magic window's "Icon charges".
  - ✅ **the sheet of light**: `SpellIcon::chargeSheet`, a `particles::LightSheet` stood on its first frame built on
    `charge_visual::SheetPoints` of the matrix the icon is drawn with (`object_placement::Cells`), height 6, spread 1.5, shift 0.03 s; then every
    frame its player's colour, fed the charge's fraction and moved on by the frame's game time, before the particle
    service draws it (`AddOwnedLightSheet`: drawn, never moved on there, so the strength goes in before the step as
    in the original). Tests `IconChargeVisual.TheSheet*`; readout: "Icon charges" shows what it is fed and its
    highest rise.
  - ✅ **the pulse ring**: `charge_visual::Ring` (t, the scale 3(1 − t), the raise 0.1 × twice the icon mesh's half
    height, the alpha 255 t cut to a whole number) and `RingModel` on the matrix the icon is drawn with
    (`object_placement::Cells`); the ring's Transform takes the icon's sized axes and the ring's scale, so its drawn
    cells are the icon's times the scale as in the original, its alpha is that byte over 255, and its colour the
    player's in place of the land's light (`ObjectColour`, specular 0). It draws for t < 1 (the original ≤ 1, the same
    picture: scale 0 at 1). Tests `IconChargeVisual.TheRing*`; readout: "Icon charges" shows its scale, alpha and
    raise.

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
  has no `Mesh` (Pending). The holder's target, player and creation matrix match the original (`seed_graphic::Create`
  in `SpellSeedGraphic.cpp`; the glint matrix is updated only after the holder's step and only with `useMesh != 0`, in
  `DrawSpellGraphic`).
- **Not ported**: the per-object envmap (FOOD, the creature vials, BEAM_EXPLOSION's properties) and the `CreatureRoom`
  / `WorldRoom` branch.

### The worship percentage and the villagers in openblack

- `Worship/WorshipPercentage.cpp`, `VillagerWorship.cpp`: openblack passed the arguments of `GetDistanceModifier` the
  other way round and sent the farthest ones first; corrected.
- The villager states are in the table in `LivingActionSystem.cpp`. The walks to the gate (59) and to the door (213)
  take the site's footpaths, the walk to the dance place (60) is a plain one, each with its state to follow
  ([routes](../beings/routes.md#pending), W4); 58 is entered only when a reaction pops back to the walk. The `VillagerStates` enum matches the original and is what indexes the table.
- In `VillagerWorship.cpp`: `SiteOf` (the town's site), `AddVillagerToWorshipSite` (also counts the town's
  worshipper), `RemoveVillagerFromWorshipCount`, `RemoveGoHomeRequest` (the go-home queue, which nothing joins, as in
  the original),
  `CanIGetToTheWorshipSite` (with the teleport stone that shortens the trip), `StartHidingAtWorshipSite` and
  `HidingPlace` (the site's door) with `IsAtHidingPlace` (the arrival there, on whole units),
  `CheckAllowedToRestAtWorshipSite`, the meal (241, 65), the altar rest (61-64), the
  supply errands (42-46), and the exits for `VillagerStates::WorshippingAtWorshipSite`,
  `VillagerStates::HidingAtWorshipSite`, the meal and the errands; going home is `VillagerStates::GoHomeFromWorship`.
- The arithmetic is raffclar's pure rules (`src/ECS/WorshipperRules.cpp`, PR #308): the share's head count, the
  candidates' order and key, the reach, the arrival, the chant, the meal, the going home, the site's food want and the
  disciple's share (in the original's x87 order).
- **Checked against v1.42 (2026-10-20)**, PR #308's port and the stack's own commits, rule by rule:
  `GetWorshipersNeeded` 0x73C860 (population as a u32 into a float, `pop × share + 0.5` at 24 bits and truncated, the
  `share ≤ 0` guard by `test ah, 0x41`, `extra > 0 && have ≥ needed` signed); `GetWorshipDistanceKey` 0x73C590 (town
  distance + 100.0f, then `((life × life) × life) × modifier`); the candidates' list insert (strict `<`, stable);
  `IsAvailableForWorshipSite` 0x752820 (flag 0x200, the disciples, only on the second pass); the arrival 0x76BE00
  (`< 10.0f`, dancers against `maxDancersVisible` unsigned, `jae` at 0x76BE78); `CheckAllowedToRestAtWorshipSite`
  0x76C9A0; the food the town can spare 0x747A40 (pit food as u64 less the float at Town +0x6F4, truncated, at least
  0); the site's want 0x77B920 (`jns`); `FoodForSiteIfEnough` 0x73C950; `CanIGoHome` 0x77E0A0 (no side effects);
  `GotoWorshipSiteForWorship` 0x76BCC0 (disciple type 0xB handed over, `IncrementWorshipPercentage` 0x73C650 only
  once set off; the oracle gives the share bits the tests pin: (0, 2, 1, 10) → 0x3EE147AE, (−1, 2, 1, 10) →
  0x3EAE147B, (−3, 7, 0, 23) → 0x3E706ADA, (0, 0, 1, 1) → 1.0); the food point (9, −38) of `GetFoodPosAndYAngle`
  0x77CDB0; the meal states 65 / 241 and `ExitGetFoodAtWorship` (the row flag at +0xE8, openblack's `field0xd8`);
  `GetResourceNearestEdge` 0x77C6D0; the altar rest 61-64 and the supply rows 42-46 (0x76BFA0, 0x76C080 with
  `ArrivesAtStoragePitForResource(0, n, 0, 0xA3)`, 0x76C2D0); `GetRestPos` 0x77CE70 (special point 7); the door
  `MultiMapFixed::GetDoorPos` 0x52E370; `CheckRequestGoHome` 0x76C8D0 with no caller. All match. Four differences of
  older code were found and fixed beside them: the hiding arrival, the chant rate, the site being built, and leaving
  the dance without a site (above).
- The worshippers' readout: the Magic window's Worshippers section (closed by default), with each site's dance.
- Components: `WorshipSite::infiniteChants`, `goHomeRequests` and `villagers`; `WorshipVillager` (the villager's link
  to its site, with `requestedGoHome`); the icon's `removeTimer` (vestigial, as in the original) and `bandSpin`.
- Tests (`test_worship`): `ShippedNorseSiteInfo` builds the shipped Norse site row, and the fixture fills
  `MagicType::Fireball`'s row in `magicGeneral`, since the GMagicInfo rows live in per-class sections (types 0..9). The
  nearest-first test puts the villager's distance to the site's centre against the town's + 100, about 128.9 m, as the
  original's `fn_0073C590` does.

### Dispensers and fireflies in openblack

- **Dispensers** (`Worship/SpellDispenser.cpp`): the turn's timer and the orb's place are the pure rules of
  `Magic/DispenserRules` (raffclar's names: `magic::StepDispenser` over a `DispenserTimer`, `OrbPosition`,
  `OrbTouches`), called by `dispenser::Process` and `dispenser::CreateOneOffSpellSeed`. The counting is ours, which
  differs from his in two places (see Pending).
- The orb's seed is `magic::GetSpellSeedOfMagicInfo` (`Magic/MagicTables.cpp`), used by
  `dispenser::CreateOneOffSpellSeed`.
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
outside the table gives 0 there. `GET_HELP` (`GetHelp`) gives a reward chest's row of `InfoConstants::reward`
(`helpTextEnum`, read unsigned; a kind outside the table gives 0) and hands any other thing, after the "Not reward!"
message, to the same `PushHelpTextOfThing` as `GET_FIRST_HELP` (`test/test_script_query_natives.cpp`, `GetHelp*`).
The land script's `CREATE_PLANNED_WORSHIP_SITE` (`FeatureScriptCommands::CreatePlannedWorshipSite`) does nothing, as
the original: no object, no random draw, and the last created object stays the previous command's
(`test/test_footpaths.cpp`, `FootpathsTest.APlannedWorshipSiteMakesNothingAndKeepsTheLastCreatedObject`).

### Differences from the original and what is missing

- `CREATE_PLANNED_WORSHIP_SITE` does nothing, as in the original (see [Script](#script)). Whether openblack's
  worship sites are built as the original's are is a matter of the citadel's request and the villagers' build work,
  not of this command; that a planned citadel gets its six slots is not checked here.
- **The hand grips, drags and lets go of the totem**, as read in
  [the village totem](village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go): the grip's four tests, the
  held share, the drag on every drawn frame, its packet 0x29 at the end, the empathy at the release, and the guidance's
  raise sound, the see-through second totem and the 0xEEB tooltip. Shown by the testbed's
  `objects.village_totem_drag`. What the totem still misses is listed on [its page](village-totem.md#pending) (an older
  list here also named the reaction at the grip, the facing, the icon, the hand's pose, `HEIGHT` and
  `GET_TOTEM_STATUE`, which have since been ported).
- Not ported: the
  reward chests and the stealing of spells by the creature (with the creature). The mana path sprite
  (`CreateManaPathSprite` 0x77B2C0) is ported: [the mana path motes](spell-core.md#the-mana-path-motes).
- `GET_FIRST_HELP` / `GET_LAST_HELP` (and `GET_HELP` on what is not a reward chest) on an object that is not a spell
  dispenser: the original gives its info row's
  `HelpStartEnum` (+0x50); openblack still gives 0, because it does not look up every kind of object's info row by
  thing. Telling apart a thing that is not an object (0 with no message) is still to do as well.

### Where the code is

- `Worship/AltarNeeds`: `AverageLifeDesired`, `DesireForRest`, `PeopleHidingIndicator` (the altar's need signs).
- `Worship/Citadel`: `CitadelWorship::sites` (indexed by the slot), `StrainSoundFractionAtMostOne`,
  `GetSpellIcon` (fn_00463AD0), `WorshipSiteBuilt`,
  `SetInterfaceCitadel`, `EntranceValidToTap`.
- The chant music's choice of site (fn_004639A0) is `ecs::audio_queries::PickChantSite` (ECS/AudioQueries).
- `Worship/FireFlyReward`: `SetRewardProbability` (fn_0052B630), `OnPlacedInMagicHand` (fn_0052B600).
- `Worship/GestureIconProvider`: `InterfacePlayer`; `Worship/InterfaceStatus`: `HasInterface`, `HeldSpellSeed`
  (fn_005DC830).
- `Worship/PlayerSpellIcons`: `AnyRequestableIcon` (fn_0064BDE0), `SetLastSeedType` / `LastSeedType` (fn_005DCA20 /
  fn_005DCA40), `SetChargingPowerUp` (packet 0x6A), `CancelAllSpellsCharging`, `OnMagicTypesChanged` (the tail of
  `SetMagicTypeEnabled`).
- `Worship/IconChargeVisual`: the charge visual's pure functions, `charge_visual::Level`, `ChargeSeconds`,
  `Brightness`, `Tint`, `SheetPoints`, `Ring`, `RingModel`.
- `Worship/SpellDispenser`: `TownIdFor` (fn_00723010), `ApplySeed` (a seed given to a dispenser); `dispenser::Process`
  skips a dispenser with `MagicType::None`.
- `Worship/SpellSeedGraphic`: `LoadBandMesh`, `SetBandColour`, `UpdateBands`, `ReplacedMesh`, `StepEffect`,
  `BandRotation`; `seed_graphic::Create` makes no holder for `ParticleType::None`.
- `Worship/TownCentreSpellIcon`: `CreateIcon`, `AddSpell`, `RemoveSpell`, `SpellCount`.
- `Worship/TownMagic`: `AddMagicTypesHeld`, `TakeSeedMagic` and `GiveSeedMagic` (creature theft), `CheckAddWorshipSite`;
  a player owns a magic type while one of its towns holds it (`SetMagicTypeEnabled` counts its holders, as the original's `GPlayer::MagicRemainder`,
  `Magic/Core/Players.h`).
- `Worship/Worship`: `ReturnSeedToItsSite`, `OnSeedOutOfHand`.
- `Worship/WorshipPercentage`: `SetTotemPercentage`, `AdvanceRise`, `AddVillagerOnWay`, `ProcessTotem`, `TotemOf`,
  `SetStatuePercentage`.
- `Worship/WorshipSite`: `TribeOfTown`, `TownHasSpellIcon`, `SiteTransform` (🔍 inferred: the snap to the land's height
  is openblack's), `FindNearestFreeSlot`, `IconPositionFromSlot`, `FindIconPosition`, `CreateTotem`, `RemoveTown`,
  `AddTownSpells`, `DancerCount`, `GetFoodResource`, `TotalChantsAvailable`, `SetMana`, `AddDancer`,
  `VillagersRequestingToGoHome`.
- `Worship/WorshipSpellIcon`: `PlacementIndexOf`, `WorshipIconOf`, `SeedTypeOf`, the icon's `MagicTypeOf`,
  `GetChantRequired`, `ChargeFraction`, `ValidForPutFullyChargedSeedInHand`, `PowerUpValid`, `AddSeed`,
  `ChargeDrawingOf`, `UpdateChargingVisual`.

### Test hooks

The hooks are in [debug environment variables](../project/debugging.md#debug-environment-variables).

#### Checks in the game

Land2 with `-s Land2.txt`, Land1 with `-s Land1.txt`:

- PLAYER_TWO's citadel with its two worship sites (the Norse one from `CREATE_WORSHIP_SITE` in slot 5 and the Greek
  one that `PostLoadCleanup` adds for town 2 in slot 0), both at the citadel's origin and rotated to their slot, with
  their food cauldron and their altar. The `b_worship` mesh wears its temple's texture, as the original.
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

- **What `GAME_SET_MANA` takes** (found 2026-10-25, [using the chants](#using-measuring-and-setting-the-chants)):
  the original takes a worship site or a spell icon, a village-centre icon giving its town's site; openblack's
  `WorshipSiteOf` (`src/Magic/Script/CHLWorship.cpp`) also takes a town or a totem for their site, which the
  original refuses, and refuses a village-centre icon.
- **A seed name no seed has** in `CREATE_TOWN_SPELL`, `CREATE_TOWN_CENTRE_SPELL_ICON` and
  `CREATE_PLANNED_SPELL_ICON`: the original takes seed 30 unchecked and reads its "base magic" past the seed table
  (0xD9D678 + 30 x 400 + 0x124 = 0xDA067C, inside `GMapShieldInfo::Infos`), gives the town that value as a magic
  and, for 10 and 12, asks the village centre for seed 30; openblack does nothing. No shipped script names a missing
  seed. Likewise `CREATE_NEW_TOWN_SPELL` of a magic no seed casts reads seed -1's base (0xD9D60C); with the game's
  info.dat every such magic (31, 32, 37..40) is named `NONE`, which resolves to 0 first, so it cannot be named.
- **Not tested**: the village centre's icon that `CREATE_TOWN_SPELL` adds (it needs the centre's mesh points and the
  seed graphics); the tests cover a town with no centre.
- (The icon's +0x114 countdown is dead in the original: [the worship site's icons](#the-worship-sites-icons).)
- **The dispenser's help text** for a magic type outside the 42-entry table: what the original reads is not known
  (the index is not checked); openblack gives 0.
- **The seed graphic's glints**: a seed without a mesh (useMesh 0) has no `Mesh` in openblack, so its holder's glints
  would find no points. The original walks `info.mesh` (after ReplaceMeshGivenSeedType) through the frozen creation
  matrix. This is unreachable with the game's data, because no glint file is the holder of such a seed. The holder is
  also stepped every frame, not only when the object draw list draws the orb or the icon, and its first step is a
  single one ([the glints on a target](../physics/particle-rules.md#the-glints-on-a-target)).
- **The dispenser's counting** differs from raffclar's in two places (see the port decisions page).
- **The charge visual** ([the charge visual](#the-charge-visual)): a town centre's slot icons run the same draw with
  their site's icon of their seed, so they show that icon's charge too: not ported for them (openblack draws them with the white tint and
  no added colour, and no sheet or ring). openblack does not know which icons are off screen, so an icon that comes back on screen takes
  this frame's tint where the original's first frame shows the one it kept, and its sheet of light was moved on all
  along (the original's stands still off screen).
- **Not identified**: the town centre icon's +0xA & 1 flag. Verify with `xref.py writers` on that offset. (+0xB8 is
  the mana path sprite, `CreateManaPathSprite` 0x77B320; +0x90 is the altar's need signs, set by the altar's
  constructor at 0x7808B8, [the altar's need signs](#the-altars-need-signs).)
- **The altar's need signs**, as the workshop's sign:
  - **Not identified** 🔍: the two fields the draw sets on the sign's 3D object, `LH3DObject` +0x5C and +0x60 (bw1-decomp's
    `field_0x5c` / `field_0x60`, after `SnowLevel` and before `DrawAsFence`), both = 0x2D on every draw (0x719EA2..0x719EAD).
    A scan of the whole `.text` for a store of 0x2D to +0x5C / +0x60 finds only this draw and `TownDesireFlags::Draw`
    (0x746A41..0x746A4C, the same two stores); nothing that reads them was found. And the flag [0xD17BC4] the sign's
    tooltip sets: written by `InitStaticsValues` 0x54A882, `ToolTips::SubmitToolTips` 0x5C9B47 / 0x5C9B4F and
    `CalculateOverObject` (0x5CA454, 0x5CA49D, 0x5CA4B6, 0x5CA576, 0x5CA5E1, 0x5CA6C2: set to 1 by the texts that carry
    a number), read only by `fn_005C9D00` 0x5C9EBA, where a non-zero flag skips the block at 0x5C9EC2..0x5C9F33 (it
    builds something with the colour bytes 0xFF, size 0x80). What it shows is not known; openblack has no counterpart.
