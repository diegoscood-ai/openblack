# Tooltips

## Summary

When the player moves the hand over the land, the game shows one short hint next to it: what pressing a button would
do there («Recoger», «Lanzar», «Entrar en Templo»...), with a small drawing of the mouse button or key that does it
(the KMIcon). Some hints carry a number, such as the food in a store or the believers a town still needs.

Every game turn the hand works out its state (what it holds, what is under it, whether it is in the player's
influence), the state picks the text, and the help system decides whether the new text takes over the one shown, by
priority, display time and forcing. The icon fades in over as many seconds as the text has been shown before.

openblack reproduces the hand's states, the texts of the states it has ported, the help system's rules and the
drawing of the icon with the original's atlas cuts, the leash's texts, the camera's hints over the land (Pitch,
Rotate, Zoom Out) and the building texts; the few states and texts still missing (the
leash's hand state 32) are listed under [Pending](#pending). The help event each builder passes is never read by the
original's Submit (0x5C9A70), so none is raised ([not in the original](../project/not-in-the-original.md)).

## The original

<a id="tooltips"></a>
### How a tooltip is made

The original shows a single tooltip next to the hand, and the hand's state picks it every game turn. The addresses on
this page are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the tooltips. What openblack does
at each step is under [Counterparts in the code](#counterparts-in-the-code).

Each turn, in this order:

1. the hand works out its state ([The hand's state](#the-hands-state));
2. the state's builder submits a text and a button ([The text of each state](#the-text-of-each-state));
3. the help system keeps or replaces the shown text, and the constructor builds the KMIcon
   ([The help system's part](#the-help-systems-part));
4. every frame the KMIcon fades in and is drawn next to the hand
   ([KMIcon](#kmicon-the-button-or-key-drawing)).

### The hand's state

`fn_005D7E40` works out GInterface +0x3AC from the action state (table 0xD18278) and, when the table gives nothing,
from `fn_005D7F20`. `GET_HAND_STATE` 413 is this number (`HandSystemInterface::GetInterfaceHandState`; opcode
function 0x6FF730: GInterface +0x3AC, the hand state of the last turn).

In short:

- gripping the land: 20, whatever the hand holds;
- something in the hand: 24, or 5 for a spell seed;
- a tug: the object branch, so 9 🔍 inferred;
- scooping: 14;
- IN THROW: 23;
- nothing under the hand: 3;
- an object under the hand, in this order: 3 when it is not interactable; 18 out of the influence when interaction
  with it needs the influence; 13 for a locked select (piles, fields, a village totem the player may take hold of);
  9 for one that can be picked up; 18 for the rest.

#### From the action state (fn_005D7E40)

- An inactive interface (IsActive vt 0x40C) gives 25 at 0x5D7E54; inside the citadel (g_game +0x205A28 == 1) 30
  (0x5D7E73); otherwise the row of the action state.
- Row 17 (BUBBLE GRIP) of table 0xD18278 is [0xD184A8] = 0x1D Drag Bubble (0x5D7CEF). The bubble's own test
  (0x5772E0) is GInterface +0x3AC == 0x1D, the hand state of the last turn (not 0x1E in the citadel).
- **LANDSCAPE LOCK** (StartLandscapeGrip 0x5D1F81): 0x5D8090 is
  `(+0x38 & 1 && +0x39 & 1) ? ((+0x40 & 1 && EnabledFeatures & 4) ? 22 : 20) : 0`, 20 Grip Landscape or 22 Zoom
  Landscape with both buttons; without the grab and action bits of GInterface +0x38 / +0x39, fn_005D7F20. State 22
  is only one of three ways the camera takes both buttons (with the raw both-buttons bits, and the grip with the
  right button); the camera handles it in `CameraModeNew3::Update` 0x45B784
  ([Player camera: the land grip](../camera/player-camera.md#player-camera-the-land-grip)).
- **Action state 4 IN LOCKED SELECT**: 0x5D80C0 with an object gives 14 Select Lock; with a creature 19 Creature
  Interaction, or 14 when the creature's +0x10AC is not 0 (not named). States 3, 5 and 6 give 18
  ([The locked select on a creature](../hand/states.md#the-locked-select-on-a-creature)).
- **A tug** is action state 13 WAIT FOR PLUCK with nothing placed in the hand yet (CHand::PickUp 0x46DC30 only writes
  +0x4904 / +0x4908), so fn_005D7F20's object branch. 🔍 inferred: the tree stays the collide object (+0x400) during
  it.
- 3 on the frame the grab-land button is pressed (GInterface byte +0x38 and +0x39 & 1, 0x5D7FD3).

#### From the object under the hand (fn_005D7F20)

fn_005D7F20 reads the object under the hand at GInterface +0x400. With none but a screen object (+0x408) it gives 28
Over Bubble (0x5D8051..0x5D8069, plus fn_00459210(0xFA)). Otherwise the object's steps (0x5D7F6D..0x5D8070), in
order:

1. `IsInteractable` (vt 0x190) false → 3 (`Abode::IsInteractable` 0x407200 for Abode, Field, StoragePit needs vt
   0x880).
2. Out of the influence (GInterface +0x48 == 0) and `InterfaceMustBeInInfluenceForInteraction` (vt 0x714: Object
   0x4028A0 = 1, only ScriptHighlight 0x709840 = 0) → 18.
3. `ValidForLockedSelectProcess` (vt 0x6CC) → 13: a pile that is not the hand's own type (0x66E4F0), a field with
   growth and food (0x5299E0), a totem statue the player may grip
   (`TotemStatue::ValidForLockedSelectProcess` 0x738500: the status's player is the totem's, `IsFunctional`, the
   player's citadel heart built, the town's worship site built). It is tested after the influence and before `ValidForPlaceInHand`, so any other totem goes on and
   ends at 18 (Object 0x402870 = 0).
4. `ValidForLockedApplyProcess` (vt 0x72C, Object 0x4028F0 = 0) → 0x10 / 0xF.
5. `ValidForPlaceInHand` (vt 0x6FC) and not `IsCannotBePickedUp` (vt 0x180) → 9.
6. Else 18.

**(not ported)**: the leash (+0x410) 32; the bubble's (+0x408) extra help call; the vt 0x72C step (15 / 16): which
classes override it with an empty hand, and which of the two states they give, is not read.

#### Where the hand submits

The hand's part of the tooltip constructor (the builder 0x5C9FC0) is fn_005D78D0 with the interface's hand state. It
is called once per turn, before the lifetime, and submits what the hand shows.

### The text of each state

The texts of the states come from the table at `0xBF1C10`, one row per hand state; the builders below fill it.

| State | Original | Texts |
|---|---|---|
| 3 Normal | fn_005D6980 | the camera's hints 0xE76 «Inclinar» (up and down arrows) then 0xE77 «Rotar» (four arrows), each with its camera feature; over a fish in the water and in the influence 0xE73 «Recoger»; otherwise 0xE7E «Mover» (left button, four arrows) |
| 5 Has Magic | fn_005D6C10 | 0xE81 «Lanzar» or 0xEF1 at a target the seed can take; otherwise the land texts |
| 9 Can Pick up | fn_005D6D70 | a flying object 0xE80 «Atrapar»; otherwise 0xE73 «Recoger», plus 0xEF7 «Golpear para Romper» (rocks) or 0xE7A «Golpear» when it can also be tapped; a one-shot orb only its own text, forced, with no button |
| 13 Can Select Lock | fn_005D77C0 | piles 0xEFD / 0xEFE «Cantidad Comida / Madera: N»; fields 0xE73 |
| 14 Select Lock | constant | 0xE85 «Interactuar» (up and down arrows) |
| 18 Over Object | 0x5D7190 | first a building that is not whole, the local player's: 0xEFB «Madera Necesaria» then 0xEF4 «Construcción Completada»; a creature: 0xE85 (the player's own, or another's in the influence); in the influence a spell icon its seed's tooltip, forced, with its charge after it, and a built workshop 0xEF3 with how far its scaffold has got; my built temple's entrance 0xECB «Entrar en Templo»; a script highlight (in or out of the influence) 0xEA0 «Recordatorio» once lit, else 0xE83 «Activar»; tappable and not pickable: 0xEF7 (rocks) or 0xE7A (abodes, a temple's leash post); otherwise the land texts |
| 19 Creature Interaction | constant | action 2, 0xE85 «Interactuar», arrows 0xF00 (all four) |
| 22 Zoom Landscape | constant | action 6 (the zoom control), 0xEFA «Zoom», arrows 0x300 (up and down) |
| 28 Over Bubble, 29 Bubble Grip | constant | action 2, 0xE84 «Pergamino», arrows 0x300 (up and down); rows 28 / 29 of 0xBF1C10 (stride 0x18: builder, action, text, align, name, event) |
| 24 Object In Hand | fn_005D6F40 | a scaffold over a scaffold it may join: 0xEA5 «Combinar Andamios» (`HELP_TEXT_TOOLTIP_51`) (over one it may not join only the drop and throw texts); a creature that takes it as a gift: 0xE87 «Dar»; a target that takes it: 0xE8E; otherwise the held object's own drop text (0xEEF «Plantar» for a tree, a scaffold's plan's text or 0xEF0) or 0xEEE «Soltar», then 0xE74 «Lanzar» |

#### State 3: Normal (the land texts)

- **The leash first** (`CalculateLeashToolTip` 0x5D67F0, ✅ in the v1.42 exe: called by the builders of states 3 / 20
  (0x5D699B), 9 (0x5D6E1A, after the flying and cannot-be-picked-up tests), 13 (0x5D77D8) and 18 (0x5D71A8); it was
  marked "1.20 only" by mistake). Only while the hand's leash status is on (status +0x12C → +0x14); the object is the
  interface's (+0x400), the creature the local player's. It always returns 0 (`xor eax, eax` at 0x5D687A and
  0x5D697B), so its callers' early exits (`test eax, eax` / `jne` at 0x5D69A3, 0x5D71B0) are never taken: every
  builder goes on with its own texts after it, and the help system's rules keep whichever took over. With the land
  tooltips as a fallback a builder can ask it twice in a turn, and so trigger 0x2F twice:
  - tied and over the tied object: 0xEA3 and help event 41 DetachLeash (0x5D6855); tied over anything else or
    nothing: nothing;
  - untied over a thing that is not the creature, with creature +0x1268 ≥ 4, vt+0x6F0 == 1 and not a
    OneOffSpellSeed or SpellIcon: 0xEA4 and event 40 AttachLeash (0x5D691E);
  - untied over nothing: 0xEA2 and event 47 FocusLeash, with `HelpProfile::Trigger(0x2F)` (0x5D695C);
  - none of the 33 `Trigger` calls pushes 0x28 / 0x29, so 40 and 41 are only the tooltip's events.
- **The camera's tricons**: 0xE76 «Inclinar» (Tricon +0x90 & 2) and 0xE77 «Rotar» (& 1) are set by
  `CameraModeNew3::UpdateTricons` 0x459230 from the cursor near the screen's **edges**, not its centre: |x| > 0.45,
  y > 0.43 / 0.49, y < −0.49 / −0.4. In a window (LHScreen +0x64 `windowed`, [0xE850B4]) the 0.49 is 0.45 (0x459280;
  [Player camera: the mouse](../camera/player-camera.md#player-camera-the-mouse)). fn_005D6980 presets the action to 1
  (0x5D69AB) and reads +0x90 only while the camera's mode is CameraModeNew3 (the dynamic cast at 0x5D69DA, else 0):
  & 2 with EnabledFeatures & 1 gives 0xE76, align 0x300, help event 0x1C (0x5D6A0E..0x5D6A2E); else & 1 with
  EnabledFeatures & 2 gives 0xE77, align 0xF00, event 0x19 (0x5D6A42..0x5D6A62). Both come before the fish.
- **Over the water**: a shown fish within 2 units (fn_00824B10 on the FishFarm list) gives 0xE73.
- **Zoom Out** (0x5D6B2F..0x5D6BF7): with the camera's mode CameraModeNew3 and its +0xC8 set (the screen's middle
  pixel meets the land), `HelpProfile::Trigger(0x2C)` (0x5D6B7F), then +0x2E4 < [0x92AB64] 15.0 (`test ah, 1`: a NaN
  passes), `CalculatePitch` > [0x92AB68] 0.55 (`test ah, 0x41`, strict) and EnabledFeatures & 4: 0xE8B «Alejar»,
  **action 4** (the zoom-in control), event 0x1D, the align left as the caller's. The caller's align is always 0: the
  turn's builder fn_005C9D00 zeroes it (0x5C9D11, 0x5C9D71..0x5C9D7D), fn_005D78D0 calls a row's function with the
  caller's pointers (the row's own action, text and align are read only for a row with no function, 0x5D790B), and none
  of the five fallback callers writes it first. So Zoom Out shows no arrows.

#### State 5: Has Magic

fn_005D6C10 (0x5D6C10..0x5D6D60):

- Out of the influence (GInterface+0x48 == 0): the land tooltips (0x5D6C26 → 0x5D6D40).
- The held seed is `GetFirstObjectInCurrentHand` 0x5DC130 with `IsSpellSeed` vt 0x4C4; no seed, or no seed info,
  gives no tooltip (0x5D6C4B / 0x5D6C56 → 0x5D6D5C).
- With an object under the hand (+0x400): `ValidToApplyThisToObject` vt 0x71C (SpellSeed 0x7286D0: the seed ready,
  the target not script-highlighted, `CanCast(obj, player)` 0x729190) gives action 2, 0xE81 «Lanzar», align 0, not
  forced, help event 9 (0x5D6C9D..0x5D6CBF); otherwise the land tooltips, with no try of the land under it.
- With no object: `ValidToApplyThisToMapCoord` vt 0x724 (0x728720: the seed ready, `CanCast(coords)` 0x729150) at the
  hand's point (&GInterface+0x3F0) gives action 2 with 0xEF1 when the magic is 0x10..0x14 (the storms, the tornado,
  the shields) and 0xE81 otherwise (0x5D6CE6..0x5D6D1D); otherwise the land tooltips.
- The magic is `GSpellSeedInfo::GetMagicType(status+0x178)` 0x72AF10 of the current power-up gesture: gesture 0 gives
  magicTypes[0] (+0x124), the gesture found at +0x134[i] gives +0x128[i], any other gives 0.

#### State 9: Can Pick up

- First of all, the interface's flags (GInterface +0x28, the byte SET_INTERFACE_INTERACTION writes) holding 2 send
  the state to the land tooltips fn_005D6980 (0x5D6D90..0x5D6D99 → 0x5D6F14), before the object is looked at.
- A flying object is Flags +0x24 & 0x40 IN_PHYSICS and altitude (Pos +0x1C) >= 0 (0x5D6DA3..0x5D6DB7: fcomp 0.0,
  `test ah, 1`, so a NaN fails too) → 0xE80 on action 2, not forced; IsCannotBePickedUp gives nothing (0x5D6DC6).
- openblack: `hand_tooltips::CanPickUpFirst` takes `interface_active::GetFlags()`, InPhysics and the transform's
  height, in this order (test
  `HandToolTips.CanPickUpShowsTheLandUnderTheScriptsLimitsThenCatchOverAThingInThePhysicsAboveNothing`); the debug
  bar's Camera window shows the interface flags under "The hand's tooltip over the land".
- **A one-shot orb** (RTTI OneOffSpellSeed 0xBECA20, 0x5D6E2B), after the flying test, IsCannotBePickedUp and the
  leash: Submit(vt 0x184, action −1, align 0, **forced**, event 0) and nothing else (no 0xE73); the forced submit is at
  0x5D6E61. vt 0x184 is `OneOffSpellSeed::GetOverwritePickUpToolTip` 0x72AC50 (vtable 0x8F3774):
  - `GetMagicInfo` 0x72A610 = `GSpellSeedInfo::Infos[orb+0x68].GetMagicInfoFromPULevel(orb+0x78)` 0x72AFE0 (that
    level's magic, or magicTypes[0], in the table [0xD37D10]);
  - when it is not null, `GMagicInfo::GetMagicEffectInfo` 0x5FB680 (table 0xCC6630, stride 0x11C, by the magic
    info's +0x10) gives the effect's +0x110 `toolTipsEnum`;
  - when it is null, the seed info's `tooltip` (0xD9D678 + 0x184, stride 0x190; the first info's is 0xD9D7FC);
  - a text of 0 is dropped by SubmitToolTips (0x5C9A7C..0x5C9A90). The table is filled from info.dat at load.
- **The tap text** is `GetOverwriteTapToolTip` (vt 0x19C) or 0xE7A; Rock's is 0x6E7A60 (0xEF7); the abodes keep
  0xE7A. `SpellIcon::GetOverwriteTapToolTip` 0x726420 is `[this+0x80 seed info]+0x184`, the icon's
  `GSpellSeedInfo::tooltip`, but a spell icon never reaches the tap (state 18 below).

#### State 13: Can Select Lock

- A TownCentre or a TotemStatue gives 0xECC / 0xEFC (the believers, the population) first, the same test as state
  18's (0x5D7801..0x5D7866); a select locked by someone (Flags & 0x10, 0x5D7867) gives the land tooltips.
- A pile is `Pot` 0x66F540: IsPileFood (vt 0x480) ? 0xEFD : 0xEFE. The number (`CalculateOverObject`
  0x5CA8FA..0x5CA9C5): a dynamic_cast to Pot, then GetResource (vt 0x98) FOOD for 0xEFD, else WOOD, unsigned 32 bits
  widened straight to a double. A store's pile is a `PotStructure` (0x66EF00): it answers its structure's
  GetResource, the store's whole total, so the hand over any of a store's piles shows all the store holds (openblack
  `object_resources::GetResource`; it showed the pile's own amount until 2026-10-25; test
  `HeldApplyTest.TheHandOverAStoresWoodPileShowsTheStoresWholeWood`). A field is
  0x52A000 (0xE73).
- State 13's own text is `GetOverwriteInteractableToolTip` (vt 0x194, 0x5D787E, its only reader) or 0xE85.
  LeashObj's (0x464580: 0xEC8 / 0xECA / 0xEC9 for posts 0 / 1 / 2) is never shown: a post is no locked select
  (`ValidForLockedSelectProcess` Object 0x419330 = 0), so the hand over it is in 18, which gives 0xE7A.

#### State 18: Over Object

fn 0x5D7190, its tests in order:

1. **The leash** first (0x5D71A8).
2. **A building that is not whole** (0x5D71B8..0x5D72EA), before everything else below: a spell icon (`CastSpellIcon`
   vt 0xB8) whose `GetWorshipSpellIcon` (vt 0x914) is set skips it; else a `MultiMapFixed` (the dynamic cast at
   0x5D720F: every Abode, a field too, the citadel parts, a spell icon, a totem statue; a mobile static, a forest, a
   fish farm and the rest keep MultiMapFixed's `IsBuilt` 0x438D80 and `IsRepaired` 0x438D70, both 1) that is not both
   `IsBuilt` (vt 0x890) and `IsRepaired` (vt 0x88C), not `IsFeature` (vt 0x484), and whose `GetPlayer` (vt 0x1C) is
   none or the local player gets action −1 and, only with EnabledFeatures & 1, Submit 0xEFB «Madera Necesaria» then
   0xEF4 «Construcción Completada», align 0, not forced, event 0, and returns (0x5D72AB..0x5D72E2). Without the
   feature it goes on with the action at −1. `Abode::GetPlayer` 0x405F70 is its town's player, else
   `GameThing::GetPlayer` 0x570130, the neutral player; `Field::GetPlayer` 0x528940 its town's (+0x118) or none;
   `CitadelPart::GetPlayer` 0x469750 its citadel's. The numbers (`CalculateOverObject` 0x5CA126..0x5CA2FA): 0xEF4 is
   (IsBuilt ? `GetPercentRepaired` vt 0x884 : `GetPercentBuilt` vt 0x880) × 100.0 (0x8AB41C); 0xEFB with a building
   site (MultiMapFixed +0x74) is `GetWoodNeededToBuild` 0x43C5F0, 0 when it is not above 0 (NaN too, `test ah, 0x41`),
   and with no site it is written with 0xEF4's words and percent (0x5CA27F..0x5CA2E6).
   The totem and the spell icons: `TotemStatue::IsBuilt` 0x5610B0 and `SpellIcon::IsBuilt` 0x55D3D0 (TownSpellIcon,
   TownCentreSpellIcon) are +0x58 bit 1 clear and `GetPercentBuilt` (0x4014F0, +0x5C) >= 1.0; `WorshipSpellIcon::IsBuilt`
   0x77FEE0 is its site's (1 with none). `TotemStatue::IsRepaired` 0x561090 and `SpellIcon::IsRepaired` 0x55D3B0 (all
   four icon classes) are `GetPercentRepaired` (0x401500, vt 0x11C, `Object::GetLife` 0x402600) >= 1.0.
   `TotemStatue::GetPlayer` 0x738490 is its town's (vt 0x48, `GetTown` 0x738480) player, else the neutral player;
   `SpellIcon::GetPlayer` 0x726540 is its worship site's (vt 0x30C; `SpellIcon::GetWorshipSite` 0x55D3A0 is 0), else
   the neutral; `TownSpellIcon::GetPlayer` 0x748B90 is its town's, else none. The skip: `GetWorshipSpellIcon` is
   `this` for a WorshipSpellIcon (0x77F090), so a worship site's icon never shows them; for a TownSpellIcon and a
   TownCentreSpellIcon (0x748F30) it is its town's worship site's (vt 0x30C, 0x748F00) icon of the same seed info
   (+0x80; `WorshipSite::GetSpellIconFromSpellSeedInfo` 0x77C2B0, the list at +0xE0 through +0x110), so only a town
   centre's icon whose seed its site has no icon of can show them.
3. **A town centre or a totem**: IsTownCentre (vt 0x1E0, TownCentre 0x55DB70, 0x5D73A2) or IsTotemStatue (vt 0x1E4,
   TotemStatue 0x561150, 0x5D73B0) gives 0xECC then 0xEFC (0x5D73BE / 0x5D73DC), before the creature and influence
   tests, so out of the influence too. The builder takes the same test (0x5CA6DB, 0x5CA792), and
   `TotemStatue::GetTown` 0x738480 is `+0x7C ? centre->GetTown() : NULL`, so a totem shows its town centre's town's
   numbers. Both leave the action as `CalculateLeashToolTip` 0x5D67F0 left it, which is −1 (no button) when there is
   no leash text: SubmitToolTips(text, action, align, 0, event), pushed at 0x5D73C4..0x5D73F5. The totem's icon is
   not an object of its own: `TotemStatue::Draw` (0x738E4D, 0x738E86) registers the plinth's and the icon's hits both
   as the totem, so either gives the totem's state and texts. A totem always has its town centre (set at its making,
   0x737B78; the centre deletes its totem with it, 0x743B54..0x743B65).
   - The believers number (fn_00740EA0, 0x5CA723): (GetBeliefInPlayer(me) − GetMaxBeliefMeNotIncluded) × 1000
     (0x5CA728); below 0 it is × −1 and the text is 0xEE0 «%3.0f Creyentes Necesarios» (0x5CA73F..0x5CA74E).
   - «Población: %3.0f» is Town +0x618 + +0x61C (adults + children), then "%s/%d" with +0x644 + +0x650 (the adult
     and child places) (0x5CA7B6..0x5CA824).
4. **A creature**: after the town centre test (0x5D73B8 → 0x5D7402), a creature (vt 0x34) with Flags bit 0x400 clear
   and a player, when it is the local player's or the hand is in the influence (+0x48), gives 0xE85 with the town
   centre's action and arrows (0x5D7406), so action −1 and align 0; any other creature the land tooltips
   (fn_005D6980). There is no asleep test.
5. **Out of the influence** (+0x48 and vt 0x714, 0x5D7485..0x5D74A3): the land tooltips. The test is
   `+0x48 == 0 && InterfaceMustBeInInfluenceForInteraction()`, and only ScriptHighlight's 0x709840 returns 0, so a
   script highlight out of the influence goes on to its own text (12 below).
6. **A spell icon**: `CastSpellIcon` vt 0xB8 is `this` (0x55D370) for SpellIcon, TownSpellIcon, TownCentreSpellIcon
   and WorshipSpellIcon, and 0 for everything else (ArenaSpellIcon 0x401900 too). It gives Submit(vt 0x19C, the
   action untouched (−1), align 0, **forced**, event 0) and returns (0x5D74EA..0x5D74FF → 0x5D7689), with no tap test
   and no 0xE7A; a tooltip of 0 shows nothing, not even the land. ✅ The text builder (0x5CA3C6..0x5CA4A8, read
   2026-10-23) writes the charge after the name: the icon (a town centre's: its site's icon of the same seed, vt
   0x914; none, the name alone) gives store (+0x134) / `GetChantRequiredForSpellSeed` (`fdivr`, 24-bit), × the f32
   100.0f (0x8AB41C); `fcom` with 0 and `test ah, 0x40` take both zero and not-a-number to the name alone
   (`UNICODE_sprintf(dst, name)`); otherwise the exe's own `_swprintf` 0x7C647F with `L"%s %3.0f%%"` (0xBF19F8),
   whose exact halves round away from zero (12.5 → " 13", 0.5 → "  1"; a charge over a need of 0 is +inf, "  1").
   Run on the exe's code with the oracle: 1/8 → 12.5 → "13", 1/3 → 33.333336 → "33", 2/3 → 66.66667 → "67", 99.5/100
   → "100". (openblack: `hand_tooltips::SpellIconChargePercent`, `help::tooltips::Numbers::percentAfter` written with
   `fixed_format::WithNumber`.)
7. **A workshop**: a dynamic_cast<Workshop> (0xBF1A10) with `Abode::IsBuilt` vt 0x890 gives 0xEF3, action −1, align
   0, not forced (0x5D7504..0x5D7524 → 0x5D7686). ✅ The text builder (0x5CA2FD..0x5CA3C1, read 2026-10-23) fills it
 for a built and repaired workshop: the turns left × 100 in 32 bits
   (unsigned), `fild`, divided by the info's time a scaffold takes (f32, info+0x1AC: 200 for every tribe's workshop),
   taken from 100.0f, `__ftol` (cut), `cdq; idiv 100` (the signed remainder), written as a double into its "%3.0f%%"
   (`UNICODE_sprintf`; whole numbers, so no rounding). Run on the exe's own code with the oracle: turns left 0 → 0
   (idle), 1 → 99, 50 → 75, 100 → 50, 133 → 33, 199 → 0, 200 → 0. A damaged workshop's text shows as written. A
   workshop being built goes on to the entrance, the storage pit and the tap. (openblack:
   `hand_tooltips::WorkshopProductionPercent` and `WorkshopToolTip`.)
8. **A CitadelEntrance** (RTDynamicCast at 0x5D74B8): with its heart (+0x54) `CitadelPart::IsBuilt` (vt 0x890,
   0x464AD0: +0x58 bit 1 «under construction» clear and GetPercentBuilt >= 1), the heart's GetPlayer against the local
   player (0x5D7544..0x5D7570): mine gives action 2, 0xECB «Entrar en Templo» (HELP_TEXT_TOOLTIP_89), the row's
   align, no help event, not forced (0x5D7576..0x5D7589 → 0x5D7686); another player's returns with no tooltip at all
   (0x5D7570 → 0x5D76B4). SET_INTERFACE_CITADEL is not read there: a locked entrance still says «Entrar en Templo». A
   heart that is not built goes on to the other tests, so the tap's 0xE7A while the entrance is valid to tap
   (CitadelEntrance keeps GameThingWithPos's GetOverwriteTapToolTip 0x5705C0 = 0). The entrance is state 18, not 9: it
   keeps `Object::ValidForPlaceInHand` 0x402870 = 0 (vt 0x6FC).
9. **A storage pit**: `IsStoragePit(0)` (vt 0x300, 0x5D7594) gives action −1 and 0xEF9 «Comida Almacenada: %3.0f
   Madera: %3.0f», GetResource(FOOD) then GetResource(WOOD) (0x5CA878 / 0x5CA894).
   `StoragePit::IsStoragePit` 0x4E4990 is `return IsBuilt()` (vt 0x890, `Abode::IsBuilt` 0x4016C0: +0x58 bit 1 clear and GetPercentBuilt >= 1.0),
   and every other class returns 0, so only a **built** pit shows it; one being built goes on to the altar, the flag,
   the highlight, the tap and the land. The food (GetResource(0) vt 0x98, unsigned 32 bits) is rounded to a float and
   then widened to double; the wood (GetResource(1)) goes from int straight to double (0x5CA83E..0x5CA8F7).
10. **A worship site's altar**: `IsSacrificeAltar` (vt 0x4B0, true only for `WorshipTotem` 0x7807E0) gives 0xEE9
   «Prayer Energy: %3.0f», event 0, the action and align as the leash test left them (−1 and 0), not forced
   (0x5D75B6..0x5D75D0). It comes after the pit and before the town desire flag, and after the influence test, so
   only in the influence. The number (`CalculateOverObject` 0x5CA562..0x5CA5C1): a dynamic_cast to `WorshipTotem`,
   its site +0x100, the site's battery +0xF0 loaded as a float and printed with the text's "%3.0f"; a totem with no
   site returns without writing the text (never met: the totem is made with its site, ctor 0x780840 at 0x7808AA).
   The totem is a `CitadelPart` and a MultiMapFixed (`WorshipTotem::CallVirtualFunctionsForCreation` 0x780AB0), so
   the building test of state 18 (step 2) comes first for a totem that is not built
   (`CitadelPart::IsBuilt` 0x464AD0).
11. **A temple** ✅: IsCitadelHeart 0xEE8 «Salud Templo» (vt 0x220, 0x5D7366..0x5D739D), which comes after the need
   signs and before the town centre, so in or out of the influence: its `GetLife` (vt 0x11C, `Object::GetLife`
   0x402600, `fld [ecx+0x48]`) × 100, action −1, align 0 (openblack: `hand_tooltips::TempleHealthToolTip` of
   `life::LifeOf`; the heart under the cursor is the hand's object through `hand_tooltips::IsInterfaceObject`). The town desire flag 0xE88 (vt 0x1E8, 0x5D75D5), between the altar and the
   highlight, is ported.
   **A need sign** ✅ (`ShowNeedsVisuals`: a worship site's altar's three, or a workshop's) is tested between the leash
   and the town (test 1 and 2 above), right after the building test (0x5D72EB..0x5D735D), so out of the influence too:
   0xE73 «Recoger» by default; info row 3 (the workshop's) 0xEFB «Madera Necesaria: %3.0f»; row 0 (hunger) 0xECE «Deseo
   Comida: %3.0f%%»; row 1 (rest) 0xEFF «Deseo Descanso: %3.0f», with no per cent sign; row 2 (people hiding) keeps
   0xE73 (`Scripts/InfoScript2.txt`: `HELP_TEXT_TOOLTIP_01`, `_137`, `_92`, `_141`). Event 0, the action and align as
   the leash test left them (−1 and 0), not forced. Its number (`CalculateOverObject`): 0xECE and 0xEFF the sign's
   Zoomer value (+0x64) × 100.0 as a float, widened to double (0x5CA5C6..0x5CA5FA, which also sets [0xD17BC4] = 1, 🔍 not
   identified: [the altar's need signs, Pending](../magic/worship.md#pending)); 0xEFB the workshop's wood wanted
   (`fn_00779B20`, read as an unsigned 64-bit number, 0x5CA1B6). The hand over a sign is in state 18, in the influence
   or out of it ([the altar's need signs](../magic/worship.md#the-altars-need-signs)). openblack:
   `hand_tooltips::NeedsSignToolTip`, with `HandSystem::InterfaceHandState` taking a sign under the cursor as the
   interface's object.
<a id="tooltips-script-highlight"></a>
12. **A script highlight** ✅ (a challenge scroll or a did-you-know sign): `IsHighlight` (vt 0x48C, 0x5D75FF) gives
   action 2 and, by its +0x60 active flag (lit), 0xEA0 «Recordatorio» (HELP_TEXT_TOOLTIP_46) with help event 0x22,
   else 0xE83 «Activar» (HELP_TEXT_TOOLTIP_17) with help event 0x23, the align as the leash test left it (0), not
   forced (0x5D7609..0x5D7631 → 0x5D7686). It comes before the tap, so a highlight never shows 0xE7A, and it does
   not ask whether the highlight can be tapped (a scroll not active yet says «Activar» too).
   `CalculateOverObject` 0x5C9FC0 has no case for either text, so they show as written. The hand over a highlight
   is in state 18: it is no locked select and does not go into the hand, and out of the influence step 2 of
   [the hand's state](#from-the-object-under-the-hand-fn_005d7f20) gives 18 too.
13. **The tap**: tappable and not IsCannotBePickedUp (0x5D7649 / 0x5D7657) gives GetOverwriteTapToolTip or 0xE7A.

#### State 24: Object In Hand

fn_005D6F40 (0x5D6F40..0x5D7180), in order: a query icon in the hand (vt 0x1FC) gives 0xE7C (event 0x2A); out of
the influence the land texts (0x5D6FB7 → 0x5D6FCD); then the action is 2 (0x5D6FF0) and, with an object under the
hand (+0x400) and one in it: both scaffolds (`IsScaffold` vt 0x49C, the target's first, 0x5D7009 / 0x5D7017) with the
held's `ValidToApplyThisToObject` (vt 0x71C, 0x5D7038) give 0xEA5 «Combinar Andamios» (`HELP_TEXT_TOOLTIP_51`), action 2, event 0
(0x5D704E..0x5D7061) ✅; two scaffolds that may not join go straight to the drop and throw texts (0x5D7040 →
0x5D7110), with no gift and no apply test. Else the target's `InterfaceValidToGiveObject` (vt 0x748; only
`Creature::InterfaceValidToGiveObject` 0x47A330 overrides Object's 0) == 1 gives **0xE87 «Dar»**, event 5
(0x5D707D..0x5D709C); else the held valid to apply (vt 0x71C, 0x5D70B5) gives 0xE8D on a sacrifice altar (vt 0x4B0,
event 7) or 0xE8E (event 6). The creature's test also asks its help to say it cannot pick the thing up
(`SendCandidateHelpScript(0x25, 7)`) when it cannot (not ported from the tooltip; the hand's giving sends it each
frame).

The drop text is 0xEEE «Soltar» (0x5D7117) unless the held object's `GetOverwriteDropToolTip` (vt 0x198,
0x5D7123) is not 0: Tree 0x74B790 gives 0xEEF «Plantar», a Scaffold 0x6E9860 its plan's
ToolTipsForBuild or 0xEF0 without a plan ([building sites](../world/building-sites.md)); action 2, align 0, event 0.
Then 0xE74 «Lanzar», action 2, align 0xF00, event 4 (0x5D7155..0x5D7174). openblack: `hand_tooltips::HeldDropText`
(the `Tree` component, which a magic tree also has; `scaffolds::GetOverwriteDropToolTip`) and
`hand_tooltips::DropToolTips` ✅.

openblack: the whole order is the pure `hand_tooltips::HeldToolTips` (`HeldFacts`, tests `HandToolTips.TwoScaffolds*`,
`WithAThingInTheHand*`), read by `HandSystem::SubmitToolTips` in the same order: two scaffolds ask only
`scaffolds::ValidToApplyThisToObject`, else `HandSystem::GiveVerdict` and, only without a gift,
`HandSystem::HeldValidToApplyTo`. The query icon is left out: every vtable's slot 0x1FC is
`GameThingWithPos::IsQueryIcon` 0x401B50, `xor eax, eax; ret`, so the branch is never taken ✅. Over a sacrifice altar
(`IsSacrificeAltar`, the `WorshipTotem`) the apply shows 0xE8D «Sacrificar» (`HeldFacts::overSacrificeAltar`, 2026-10-21).

### The help system's part

#### Submitting a text

- **SubmitToolTips** 0x5C9A70(text, action, align, force); `action` is the BINDABLE_ACTION whose button or key the
  icon shows (−1: none):
  - the texts 0xE73..0xF1C, with {priority, display, afterFocus} from info.dat;
  - a higher priority keeps the tooltip;
  - while the display timer runs (2.5 × display × 10 turns), only a forced text takes over, so «Soltar» and «Lanzar»
    alternate every 25 turns;
  - the same text again only takes over again if it is forced;
  - 0xD16668[cur] += 1 is read by nobody;
  - it resolves the action with fn_005C78F0 (0x5C9BC4); if that fails, animType 0, clickType 0 and row fn_005C4800()
    (0x5C9BD0..0x5C9BE0).
- **ForceToolTips** 0x5C9C60: the amount in the hand, 0xEEA (priority 0.925); it is Submit(text, −1, 0, 1) plus the
  value in [0xD17BBC].

#### Every turn (fn_005C9D00)

- The hand submits.
- A text nobody submits stays afterFocus × 25 turns.
- At TOOLTIP_LEVEL 2 a priority below 0.9 fades out in 1 s once its display time is over: fn_005C9C80 returns 0 with
  no icon, with the display used up and priority < 0.9 (0x5C9CC3); then no icon is created (0x5C9E1A).
- [0xD17BDC] "sent this turn" is set to 0 by HelpSystem::Process after fn_005C9D00 (0x5C9016).

#### The globals

| Global | What it holds |
|---|---|
| [0xBF19C4] | index of the shown text (text − 0xE73) |
| [0xD17BC8] | display turns (while > 0 only a forced text gets in) |
| [0xD17BCC] | afterFocus turns |
| [0xD17BC4] | forced |
| [0xD17BD0] | align |
| [0xBF19C8] | action |
| [0xD17BD4] / [0xD17BD8] / [0xBF19CC] | the stored animType, clickType and row |
| [0xD17BDC] | "sent this turn" |
| [0xD17BBC] | the ForceToolTips value |
| [0xD17BB8] | part of the pause test; only ever written to 0 (0x54A86C) |
| 0xD163C0 | times each text took over, capped at 80 (0x50, 0x5C9C32); they are the seconds of the fade-in |

#### The constructor (the builder 0x5C9FC0)

- **The text's number**: the forced value for 0xEEA / 0xEE1 / 0xEA2 / 0xEEB; for the other texts with a number, the
  object's number (helptext::Format: UNICODE_sprintf of the first conversion); the text as it is otherwise.
  - 0xEF9 carries two numbers in order (food, then wood);
  - 0xEFC appends "%s/%d" ([0xBF19EC], 0x5CA81E..0x5CA824) with the places;
  - 0xECC becomes 0xEE0 if the believers are negative (0x5CA745).
  - **0xEF9, 0xECC and 0xEFC are read again every turn** from this turn's object under the hand (GInterface+0x400,
    0x5CA116), whoever submitted them, while the text is alive (submitted this turn, or in its time after the focus,
    0x5C9FF0..0x5CA01F): 0xECC / 0xEFC from a town centre or a village totem (vt 0x1E0 / 0x1E4, 0x5CA6DB..0x5CA7AC)
    with its town, 0xEF9 from a storage pit (vt 0x300, 0x5CA856), built or not. With no object, or one that does not
    pass the text's test (0x5CA120, 0x5CA6F5, 0x5CA7AC, 0x5CA856), the raw template shows, with its "%3.0f"
    (0x5CA9D6..0x5CAA05). openblack ✅ (2026-10-21, fork issue #157): `help::tooltips::SetObjectNumbers`, which the
    hand registers (`HandSystem::ToolTipNumbers`, its tooltips' object `ToolTipObject`); test
    `HelpToolTipsTest.ATownsPopulationIsReadAgainEveryTurnFromTheObjectUnderTheHand`. A changed number makes a new
    icon, as the text compare does.
- **The action**: it resolves it again on its own (0x5CA07C): −1 sets animType 0 and row 3 (0x5CA06C / 0x5CA072); a
  failure gives clickType 0, row 3 and animType 0 (0x5CA088..0x5CA090), that is, no picture; −1 is such a failure
  (`ConvertActionToKMIcon` returns 0; confirmed: oracle).
- **Keeping the icon**: the icon is kept while clickType (+0x18), row (+0x1C), animType (+0x14), align | 0x16 (+0x12C)
  and the text (wcscmp +0x20) are equal (0x5C9E57..0x5C9E91). A key's name is not compared: its row is the pointer
  [0xD16110].
- **Making the KMIcon** (0x5C9F83): fn_004489D0(start 0, target 1, duration = the fade: the times it was shown, in
  seconds, or at once when forced **or at TOOLTIP_LEVEL 3**; the builder's animType, clickType, row; the text; x 0,
  y 0 as offsets from the hand (align | 0x16 anchors it there); S = H/25 (0x5C9D6C..0x5C9D8D: imul 0x51EB851F, sar 3,
  from [0xE8505A]); align | 0x16; c1 yellow {b0, gFF, rFF, aFF}, c2 white, alpha8 0x80).
- **The icon** fades in, in real time, over as many seconds as the text has been shown (forced: at once).
- ✅ **The builder's own forced flag** (read 2026-10-23): `CalculateOverObject` writes the forced flag [0xD17BC4] = 1
  as it builds some texts, before the icon is made (0x5C9EBA → 0x5C9F33), so they show at once whoever submitted them:
  0xEE8 (0x5CA4B6), 0xEE9 (0x5CA576) and 0xECE / 0xEFF (0x5CA5E1) by their id, before their object is cast; a spell
  icon's text (0x5CA454 with its charge, 0x5CA49D the name alone, already submitted forced); and through the shared
  tail 0x5CA6B4..0x5CA6C2 a desire flag's 0xE88 and a built, repaired workshop's 0xEF3 with their number. (openblack:
  `help::tooltips::BuilderShowsAtOnce`, test `HelpToolTipsTest.TheBuildersObjectTextsShowAtOnce`.)

#### How a number is written

"%3.0f" and the like go through the exe's `sprintf` 0x7C57D2 and `_cftof` 0x7CCBFC:

1. `_fltout2` 0x7D8E9F widens the double to 80 bits (`__dtold` 0x7D8EFB, where a zero loses its sign).
2. `$I10_OUTPUT` 0x7DA9B2 finds 17 significant digits: it scales the number under 1 with the 12-byte power tables
   0xC30CB0 / 0xC30E10 (`__multtenpow12` 0x7DB0A5, `__ld12mul` 0x7DAE85, which leaves out the low partial products),
   makes 18 digits by ×10 and rounds on the 18th half up, so an exact half there goes up for some numbers and down
   for others with the scaling's error.
3. `__fptostr` 0x7D8E28 then rounds those digits, zeros after them, half up at the precision.

So 2.675 (held as 2.67499999999999982…) is `2.67`, 0.15 is `0.1`, 0.49999999999999994 is `0`, 12.5 is `13`, −0.0 is
`0` but −0.4 is `-0`; an infinity or a NaN is written from the words `1#INF`, `1#IND`, `1#QNAN` as if they were
digits (`1.#J` with two decimals). Checked against the exe's own sprintf through the oracle on 220,000 cases
(precisions 0 to 9, widths, halves, exact halves in the 18th digit, subnormals, infinities, NaNs): no difference.

#### Pause

- In the help turn, right after the widescreen test, with g_game +0x14 & 4 (PauseGame 0x54AE20) and outside the
  citadel (g_game+0x205A28 != 1), the icon is deleted and the turn ends before the hand submits
  (0x5C9D44..0x5C9D5E → 0x5C9FA0); inside the citadel it stays. The condition includes [0xD17BB8] <= 0, which is only
  ever written to 0 (0x54A86C).
- `HelpSystem::Process` 0x5C8FE0 has one call site, `GGame::ProcessTurn` (0x54E69E), which is skipped while paused
  (`ProcessGameCode` 0x54D828..0x54D830); the other way in is `Temple::ProcessGameTurn` 0x794A30, only while paused
  inside the citadel (0x54CCFF, 0x5556A0).
- So the delete only happens when a pause begins within a turn, before the help step; between turns the icon is not
  touched, and `HelpSystem::Draw3D` is called every frame (0x54E2ED), so it stays drawn, frozen, while paused, unless
  an active box skips the draw (0x448B72..0x448B88; the Escape and F2 boxes, shown with `PauseGame(1)`, hide it this
  way, not the pause).

#### Resets and a new land

- `HelpSystem::ResetIcons` 0x5C5610 deletes the KMIcons +0x24, +0x2C (the tooltip's, 0x5C5632..0x5C5649: ??_GKMIcon
  0x448C00 of +0x2C, +0x2C = 0) and +0x28; it is called by Load 0x5C73E6 and `HelpDudeControl::Uninit` 0x5C56ED.
- **A new land**: `GGame::LoadMap` 0x552F40 calls `ClearMap` 0x552BB0 (0x552F4F), which calls `InitStaticsValues`
  0x54A780 (0x552F18; also called by GGame::Init). It puts every tooltip global back (0x54A86C..0x54A8BA): [0xD17BB8]
  0 and the ForceToolTips value [0xD17BBC] 0; forced [0xD17BC4], display [0xD17BC8] and afterFocus [0xD17BCC] 0; the
  shown index [0xBF19C4] and the action [0xBF19C8] −1; the align [0xD17BD0] 0; the stored animType [0xD17BD4] 0,
  clickType [0xD17BD8] 0 and row [0xBF19CC] 3; "sent" [0xD17BDC] 0. No other writer of these was found in .text
  outside the tooltip functions.
- The show counts 0xD163C0 are never reset: only Submit writes them (0x5C9C34..0x5C9C48).
- The icon goes with `GScript::Reset` 0x6EB2D0, `HelpSystem::Reset` 0x5C5580 and `ResetIcons`.
- **A land change leaves the icon drawn** (read 2026-10-22, #206): no callee of `ClearMap` touches the help system
  (its calls are listed in [what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets);
  `InitStaticsValues` writes only the globals above), and the icon +0x2C is deleted only by `ResetIcons` and the
  tooltip's turn fn_005C9D00, whose every exit but two deletes it (0x5C9F99..0x5C9FB0): it is kept when the turn
  makes it again (0x5C9F8C) or when the builder's clickType, row, animType, align and text equal the icon's
  (0x5C9E57..0x5C9E91, 0x5C9EB1). So the icon stays drawn through the load, and on the new land's first turn it
  goes, unless the same text is submitted again, when it is kept as it was, with no new fade-in, though the text
  counts as shown once more.

### KMIcon (the button or key drawing)

A mouse button or key picture with a text, faded in real time and drawn every frame by CameraHelp::DrawKeyOrMouse
0x447EA0. Research: dev\docs\collected\documentacion\hand\kmicon\README.md.

#### The object

- Object of 0x140 bytes (camerahelp.cpp, `new` at line 0x183). Ctor fn_004489D0 with 14 arguments (start, target,
  duration, animType, clickType, row, text, x, y, S, align, c1*, c2*, alpha8). fn_00448BE0 links it at the head of
  the list [0xC5AFE0] (newest first, next at +0x13C); the dtor ??_GKMIcon 0x448C00 unlinks it.
- Fields:
  - +0x00 start; +0x10 elapsed;
  - +0x14 animType (−1 key; 0..3 button blink, fn_00447450);
  - +0x18 clickType; +0x1C row (0, 1, 2; 3 = no icon) or key code;
  - +0x20 wchar[0x80] of the text (L"" if null, 0x448A75); the original points to the shared buffer [0xD16110] (or
    [0xD16184]), which every key resolution rewrites (0x5C797D / 0x5C799F);
  - +0x124 y; +0x12C align;
  - +0x130 c1 (white if null); +0x134 c2 (c1 if null).

#### Who makes them

- The tooltip (fn_005C9D00, 0x5C9F83), the click prompt of `HelpSystem::Draw3D` (0x5C59D0..0x5C5ACE, HelpSystem
  +0x24) and the $M control icon (fn_005C5B50, HelpSystem +0x24 / +0x28).
- The «Did you know?» bubble's $m icons go through fn_005760C0 0x57636B..0x5763CC.
- The KMIcons +0x24 / +0x28 are deleted when HelpText has no text (fn_005C5E50, once per turn).

#### The frame loop and the update

- **The loop** fn_00447850 (0x447850), with g_delta_time [0xC38134] × 0.001 (0x8AA3B0):
  - first (0x447859..0x4478D2) it draws a stand-alone mouse picture from the globals 0xC5AFBC..0xC5AFDC, which nobody
    writes, so with their zeros it draws nothing (🔍 inferred; not ported);
  - then fn_00448AC0(dt) on each icon from the head (0x4478DB..0x447904);
  - it runs **twice per frame** with the same dt read at 0x4478D5 both times: #1 at the end of
    `HelpSystem::Draw3D` (0x5C5B31, from GGame::Process3dEngine 0x54E2ED), under FinishFrame's overlays; #2 at the end of HelpText's
    FinishFrame callback fn_005CCAB0 (0x5CCE41, also on its early exit 0x5CCAD4; the callback 0x5CD020); so the fades
    last half their nominal time (audit_kmicon.md section 1).
- **The update** (fn_00448AC0, 0x448AC0..0x448B6C):
  - elapsed += dt, clamped to [0, duration];
  - current = start + (target − start)·elapsed/duration, or target if the duration is 0;
  - a = clamp(current, 0, 1), factor for the text, the icon and the panels;
  - c1.a = c2.a = ftol(a·255), panels at ftol(alpha8 × a) (0x448B57..0x448B6C).
- **Hidden under a box**: the draw is skipped while there is an active box (`SetupBox::CurrentActiveBox` [0xC4CD20],
  the box that takes the input, through `SetupBox::GetCurrentActiveBox` 0x407ED0) whose `BackgroundStyle` (+0x94,
  checked by `SetupBox::DrawAll` 0x4085F4 / 0x40861E) is not 0 (0x448B72..0x448B88). The update above runs first, so
  the fade goes on under the box. Every box sets it to 1 (`DialogBoxBase::Init` 0x51354F, the game's message box
  0x54C83D, the network boxes) or 2 (`SetupTabButton`); only `HistoryBox::Init` 0x545252 and
  `GatheringBox::Init` 0x570FA1 write 0, and neither exists in openblack. The box is active from its Show until its Hide (0x513623,
  0x513666 / 0x513671); a box fading out is `CurrentFadeBox` [0xC4CD24], not the active one, so the icon is drawn
  again as soon as the box is hidden. Every caller of the loop reaches the icon through this update, so both draws of
  a frame are skipped.

#### Drawing (CameraHelp::DrawKeyOrMouse 0x447EA0)

In short: S = H/25, at the hand on screen; the text at int(2S/3); black copies at (−1, −1) and (+1, +1), then yellow;
S/2 to the left of the hand, or ending S/2 to its right past 2/3 of the screen (hysteresis back below 1/3). In detail:

- **Place**: the hand point is ProjectPoint 0x819390 of [[GInterface +0x3A0] +0x482C] +0x38 (0x447EB9..0x447F01). Y
  clamped to [0, H − S] then X to [0, W − S] (0x447EB9..0x447F85); without the hand (align & 6 not 6) y is the box's
  vertical centre (0x447F87..0x447FA0), the hand's branch jumping past it to 0x447FA7; a null c1 is white, a null c2
  is c1, nothing when both alphas are under 4
  (0x447FA7..0x44800B).
- **Widths**: a key's text (0x448011..0x448108, "%s + %s" 0x9CDFA8, key names fn_0046EE60) and width fn_00447910 =
  max(1, GetStringWidth(text, 0.4S) + int(2S/3)) (0.4 at 0x8C7664); a mouse button is S wide, none for row 3
  (0x448111..0x448128); + S/2 per arrow stub 0x400 / 0x800 (0x448130..0x448183). Text size int(2S/3) (0x448187;
  int(4S/5) with NeedsBiggerText 0x4079C0); total = textW + keyW' + 2.0 (0x8AB478).
- **Layout** 0x448214..0x448353: align & 1 right-aligns, align & 2 the side with hysteresis [0xC5AFE4] (1 past 2W/3,
  0 below W/3); align & 0x10 text first, & 0x20 the side flag instead (0x4482E5..0x448301).
- **Panels**, only with alpha8 ≠ 0 (0x448357..0x448541), fn_00447BA0 in AdditiveMaterial [0xEDC364]: behind the icon
  c2's rgb at alpha8 (0x448366..0x448399), the rest c1's rgb at alpha8/3 (0x4483A8..0x4483CD), behind the text with
  o = (S − textSize)·0.5 (0x4483CF..0x44841E); stubs 0x400 / 0x800 / 0x100 / 0x200 (0x448426..0x44853C).
- **The panel's 9-slice** fn_00447BA0: the soft blob (0, 0.25)–(0.24609375, 0.49609375) of atmos.raw, DrawAlpha =
  argb >> 24 (0x447C03), b = (y1 − y0)/4 (0x447BAD..0x447BBB), corners 2b squares (0x447BD6..0x447C2B), breaks u 0 /
  0.078125 / 0.16796875 / 0.24609375, v 0.25 / 0.328125 / 0.41796875 / 0.49609375 (0x447C38..0x447E83). Confirmed
  against the disassembly: the four edges are integers, b is a signed division that rounds toward zero (cdq / and 3 /
  add / sar 2), so each corner is a 2b square centred on the box's corner, half inside and half outside; the UV
  breaks are exact binary floats (0x3DA00000, 0x3E2C0000, 0x3E7C0000; 0x3E800000, 0x3EA80000, 0x3ED60000,
  0x3EFE0000). Its DrawBox 0x412980 (adjust flag off) only orders the corners and clips at ±0xA000, which changes
  nothing on screen; its last argument is 100.0.
- **Key cap** fn_00447990 (0x448554..0x44857B): a 3-slice in AtmosMaterial [0xEDC368], white with DrawAlpha = c2.a
  (0x4479D4), v 0.001953125..0.248046875, u 0.501953125 / 0.564453125 / 0.685546875 / 0.748046875 (0x447A1C,
  0x447A83, 0x447ADE); its text (0x447AF2..0x447B93) at (x0 + h/5, y0 + h/7), size 0.4h, black with the cap's alpha.
- **The mouse picture** (fn_00447450, called at 0x44859E), below.
- **Arrows** (0x4485A6..0x44888D): AtmosMaterial, colour c2, grown by g = S × 0.1111111 (0x8C7668); left
  0x4485D3..0x44866B, right 0x44867F..0x44871E, up 0x448736..0x4487D3, down 0x4487EB..0x44888D.
- **The text** (0x448895..0x4489BC): DrawTextRaw three times in j0, black at (−1, −1) (0x448913) and (+1, +1)
  (0x448968), then c1 (0x4489BC); y offset o is a float.

#### The mouse picture (mousehelp.raw)

fn_00447450:

- an S×S quad of mousehelp.raw (256×256, 4×4 cells of 64 px) with mousehelpa.raw, material mode 5 [0xC5A318] made on
  the first draw (0x44757B..0x447591);
- a mouse with a wheel uses the third row; the left button is the right one mirrored; the wheel turns on the fourth
  row;
- lit state from GetTickCount (0x447461..0x4474EA): animType 1 (T/100) % 5 < 2, animType 2 (T/80) % 12 in {0, 1, 4,
  5}, else always;
- the cell by clickType (jump tables 0x447838 / 0x447818, 0x4474EC..0x44756E; 1 and others = the right button's cell
  mirrored). ✅ Read 2026-10-24 from the exe's bytes: the byte table over click types 0..16 is 0, 1, 2, 3, 4, 7, 7, 7,
  5, 7, 7, 7, 7, 7, 7, 7, 6 into the targets 0x44754F (0: cell 4r), 0x447553 (1 and the rest, and above 16 by the
  unsigned `ja` at 0x4474F3, so a negative type too: lit ? 1 : 0, + 4r, mirrored), 0x44752A / 0x447539 / 0x447546
  (2 / 3 / 4: lit ? 1 / 2 / 3 : 0, + 4r), 0x447504 (8: d ≤ 3 ? 15 − d : 11) and 0x447519 (16: d ≤ 3 ? 12 + d : 11),
  d the (T/100) % 5 of the first `div`; openblack: `key_or_mouse::MousePictureCell`, test
  `InputPromptIcons.TheMousePictureLightsTheButtonToPressInItsRow`. Nothing for a negative cell or colour 0
  (0x4475A9..0x4475B7); u = (c & 3)·0.25, v = (c >> 2)·0.25,
  mirrored u + 0.25 → u (0x4475BD..0x44761F);
- mode 5, tiling off (0x447709), ZFUNC ALWAYS (0x44776E), ZWRITEENABLE 0 (0x447795) around DrawAndClip2D 0x4477BB;
- the wheel's cells (clickType 8 / 0x10) turn over one picture every tenth of a second, the four frames one way for up
  and the other for down, resting on cell 11 one tenth in five. With animType 2 the wheel's frame index would be
  (T/80) % 12 (edx after the animType 2 branch, 0x4474AF..0x4474DB); animType 3 recomputes (T/100) % 5 and stays
  lit (0x447494..0x4474AD), the same as any other type.
- **animType 2 and 3 are never passed** (read 2026-10-24, xref callers): fn_00447450 has three callers. sub_512830
  passes 0 (0x51299D) and fn_00447850 0 or 1 (`setne` of [0xC5AFDC], 0x4478BD..0x4478CA). The third,
  `DrawKeyOrMouse` 0x447EA0, passes its own first argument (0x448548, 0x448594), and its three callers pass:
  `TriconDraw` the result of `ConvertActionToKMIcon` (0x4571AD, 0 on failure, 0x4571B9); fn_005760C0 the same
  (0x57622C); fn_00448AC0 the icon's +0x14. The icon's +0x14 is written only by its maker fn_004489D0 (0x448A23,
  its fourth argument) and by fn_005C5B50 (0x5C5D8E, from `ConvertActionToKMIcon`). fn_004489D0's four callers pass
  −1 (0x5C5C57), `ConvertActionToKMIcon`'s result (0x5C5E37), the same with 0 made 1 (`HelpSystem::Draw3D`,
  0x5C5A55..0x5C5A62), and `CalculateOverObject`'s out value (fn_005C9D00, 0x5C9F83), which is
  `ConvertActionToKMIcon`'s or 0 (0x5CA06C..0x5CA090). `ConvertActionToKMIcon` writes only 0 (a mouse binding,
  0x5C7920) or −1 (a key, 0x5C7971). So the picture is only ever drawn with animType −1, 0 or 1: kind 2's blink and
  its wheel frames are never seen. openblack passes −1, 0 and 1 too; kind 2's blink branch,
  which nothing reached, is removed from `Renderer::DrawKeyOrMouseMouse`
  ([not in the original](../project/not-in-the-original.md#interfacetooltips)).

#### Resolving an action's picture (fn_005C78F0)

- −1 and 0x21 have no picture (README.md section 1, "Signature");
- mouse binding (fn_00471940): animType 0, GetClickTypeForAction 0x471620, row fn_005C4800;
- key binding (fn_00471850): animType −1, clickType 0 and the name written to [0xD16110] by fn_004712D0 (Windows name
  via fn_0046EE60, 🔍 inferred).
- **GetClickTypeForAction** 0x471620 (0x47168C..0x47173A), the mouse code of a binding as DrawKeyOrMouse's click type:
  code 1 LMB → 1, 2 MMB → 4, 3 wheel up → 0x10, 4 wheel down → 8, 5 RMB → 2, other → −1. An action with code 0
  that is 3, 4 or 6 (fn_00471910 0x471910..0x471937) takes both buttons (0x47164E..0x471689): fn_00471760 gives 3
  when action 2's code (+0x820) is 1 and action 1's (+0x514) is 5, or +0x820 is 5 and +0x514 is 1, else −1
  (0x471780..0x471840). `IsMouseButtonAssignedToAction` 0x471940 calls such an action a mouse binding when that is
  not −1 (0x471963..0x4719D3), so it is resolved as the mouse before its key ✅.
- **Mouse row** (fn_005C4800): 2 with a wheel ([0xE85248]), 1 with more than 2 buttons ([0xE8524C] > 2), else 0.
- **The wheel flag** [0xE85248] is the system mouse's +0x44 (TheSystem's mouse at +0x1C4): the mouse's constructor
  fills it from `GetSystemMetrics(SM_MOUSEWHEELPRESENT)` (0x7E4F74..0x7E4F7B; +0x48 takes `SM_CMOUSEBUTTONS`, so
  Ghidra's and bw1-decomp's `NumButtons` / `IsMultiFrame` names at these two offsets are one field off), and
  `UpdateDeltaPos` sets it to 1 once the wheel turns (0x7E4890). The script's `HAS_MOUSE_WHEEL` (CHL 206,
  `GScript::HasMouseWheel` 0x6FC520) pushes it as a boolean, read in the v1.42 exe 2026-10-28; the tutorial's zoom
  lesson asks it before the spirit's wheel line (refs/chl/LandControlT.txt:1088). openblack: one constant,
  `help::input_prompt::k_MouseHasWheel` (a wheel mouse assumed: openblack can't ask the system the same), read by
  the mouse row, the zoom pictures and `HasMouseWheel` in `src/CHLApi.cpp`; test `MouseWheelNative.*`.

#### The default bindings

ControlMap (g_game +0x250300), `LoadDefaults` 0x46F890:

- action 6 (zoom on, +0x1248 / +0x1348 / +0x1450, text 0xA65) → key 0x9D (the right Ctrl) and mouse 0
  (0x46FCB3..0x46FD1B), so its picture is both buttons (click type 3), see GetClickTypeForAction above;
- action 1 → mouse 1 (LMB, +0x30C / +0x514, text 0xA5C);
- action 2 → mouse 5 (RMB, +0x618 / +0x820, text 0xA5D);
- action 0 → key 0x3B (F1);
- action 4 (zoom in, +0xC30 / +0xE38, text 0xA63) → mouse 3 (the wheel up) on a mouse with a wheel ([0xE85248]), else
  0 (0x46FC63..0x46FC94); action 3 (zoom out, +0x924 / +0xB2C, text 0xA64) → mouse 4 (the wheel down) with a wheel,
  else 0 (0x46FBCE..0x46FBFF). So «Alejar»'s icon (action 4) is the wheel-up frames (click type 0x10). With no wheel,
  `GetClickTypeForAction` gives actions 3 / 4 / 6 (fn_00471910) fn_00471760's click type: 3 (both buttons) when
  action 2 is the right button and action 1 the left, or the other way round, else −1 (0x471760..0x47183A), and
  `IsMouseButtonAssignedToAction` 0x471940 takes such an action as a mouse one only when that is not −1
  (0x47199E..0x4719B7). The click types of the mouse codes 1..5 are the jump table at 0x471740: 1, 4, 0x10, 8, 2;
- the rest not decoded.

### The pause menu's boxes around the help

The menu's options and its message boxes are described in
[the in-game menu's options and pause](../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause); the
layout read with them:

- `AddMainMenuTabs` 0x53F7B0: tab 0 HELP_TEXT_DIALOG_ADDITION_88, 1 HELP_TEXT_PAUSE_STATS_151, 2 the multiplayer
  database's entry 0x13 HELP_TEXT_MULTIPLAYER_MAIN_TAB (0x53F80D..0x53F820, in every game, kept in [0xCD0698]), 3 and
  4 without a label.
- `DialogBoxOptions::Init`: each selector is SetupButton(id, 300, top, 200, 30) between two SetupBigButton(id, 260 /
  508, top − 1, L"", 32, side 0, look 2 / 3) (0x513933..0x513B3A); the help text speed is SetupSlider(0x78, 250, 365,
  300, 30, 0, HELP_TEXT_DIALOG_HELPTEXTSPEED) (0x513B4D..0x513B90).
- `DialogBoxOptions::InitControls` copies +0x54 to +0x5C each time the options open (0x513DF5).
- Leaving the options with a changed detail tells HELP_TEXT_DIALOG_VIDEOCHANGE first (0x5144D7..0x514526); the tell
  clears [0xCC62B8] (0x51451C), so its answer hides the box and shows the main page. Event 6 also sets [0xCC62B8] to 0
  (0x514285).
- `SetupBox::MessageBoxA` style 3 (0x411400..0x4114C0): SetupBigButton(10002, 200, 338, 0xA21 «Sí.», 32, side 0,
  look 2) and SetupBigButton(10003, 568, 338, 0xA22 «No.», 32, side 1, look 3); 10002 is the quit (0x540152,
  0x514237). The single-answer box has SetupButton(10000, 325, 338, 150, 32, 0xA24 «Aceptar») (0x411603..0x411639).

### Differences

- **(not ported)**: the land texts' own `HelpProfile::Trigger(0x2C)` before the zoom test.
- The amount 0xEEA is forced every turn of the scooping, from ProcessInInteract (Pile 0x66E6E4, Field 0x52989F,
  FishFarm 0x52DAD6), and once when it ends (Pile 0x66E8DA, Field 0x529AD9, FishFarm 0x52D92A). It then stays about
  13 turns, until «Soltar» takes over. The help system's turn (`HelpSystem::Process` 0x5C8FE0) is
  `Game::GameLogicLoop`'s step 0x54E69E.
- Out of the influence a second press with something in the hand does nothing (ActionPressedHolding 0x5D16BE).
- The tooltips still missing in openblack are in [Pending](#pending).

## openblack

Status: **partial**. The help system's rules, the hand's states and the drawing are done; the items marked
**(not ported)** above are missing (gathered under [Pending](#pending)).

### Files

- `src/Help/ToolTips.{h,cpp}` (`help::tooltips`): the help system's part (Submit, Force, the turn, `Reset` at
  `Game::PrepareNewLand`'s map clear, the pause test).
- `src/ECS/Systems/HandToolTipRules.{h,cpp}` (`hand_tooltips::*`): the hand's state (`ObjectHandState`) and the text of each state
  (`SeedToolTip`, `OrbPickUpToolTip`, `SpellIconToolTip`, `WorkshopToolTip`, `TownToolTips`,
  `CreatureInteractToolTip`, `StoragePitToolTip`, `BubbleToolTip`), read by `HandSystem::SubmitToolTips` (`HandToolTips.cpp`);
  `HandSystemInterface::GetInterfaceHandState` answers `GET_HAND_STATE`.
- `Renderer::DrawKeyOrMouse` and `Renderer::AddKeyOrMousePanel` draw the text, the icon and the glow panels;
  `Renderer::DrawKeyOrMouseMouse` (`DrawKeyOrMouseMouse`) draws the icon's mouse picture, an S × S cell of
  `mousehelp.raw` with `mousehelpa.raw`, lit by the frame's tick count, mirrored for the left button, the wheel's
  frames for clickType 8 / 0x10, nothing for row 3 or a colour of 0. `input_prompt::Frame` runs the icon's two
  updates a frame and `input_prompt::Snapshot` leaves the frame's copy empty while `GameInterface::IsBoxActive`.
- `Common/FixedFormat` (`fixed_format::Fixed`): the numbers written as the exe's `sprintf` writes them.

### Where it is in the code

- `HandToolTips.cpp`: `HandSystem::InterfaceHandState` works out the hand state of the turn (25 with the interface
  inactive, 30 inside the citadel, 0x1D for a bubble grip, 20 for the land grip (22 with the action button held and feature 4), 14 for a locked select, 28 with only
  a screen object, else the object's steps; the object is the one under the hand or, when none, the drawn object
  under the cursor of the kinds `hand_tooltips::IsInterfaceObject` takes: a village totem's part, a thing with a tap, a
  creature, a need sign, a town's desire flag, a worship site, its altar and a temple's heart); `HandSystem::SubmitLandToolTips` gives the land texts (`hand_tooltips::LandToolTip`: the camera's hints from
  `CameraModel::GetHandCues`, none while a script's camera or a camera path has the view, with
  `camera_help::GetEnabledFeatures`; Zoom Out from the help profile's view of the player's camera,
  `help_profile::PlayerCameraView`, which `Game::Initialize` gives through `help_profile::SetQueries` (none while a
  script's camera or a camera path has the view); a fish in the
  water in the influence: 0xE73).
- `Graphics/Renderer.h`: `KeyOrMouseRow` is DrawKeyOrMouse's row (a mouse row, a key code or a key's name);
  `DrawKeyOrMouseIn` draws one for a caller outside the icon passes (the «Did you know?» bubble's $m) with the frame
  copy's hand point and tick count; `_keyOrMouseSide` is the side hysteresis, shared as the original's one global.
  `Renderer::DrawInputPrompts` is the draw half of the frame loop, twice a frame with each pass's alpha, from
  `OverlayFrame::inputPrompts`; `Renderer::DrawDidYouKnowBubble` draws the bubble from `OverlayFrame::didYouKnow`.
- `Renderer::GameFontAt` (`GameFontAt`): HelpText's fonts j0, f1 and f3 from Data, loaded on first use, f1 / f3
  falling back to j0. `Renderer::MeasureText` measures a text in the font HelpText would draw it with.
  `Renderer::SubmitScreenText` (`SubmitScreenText`) draws `GameFont::Vertex` glyph quads in the screen overlay, mode
  16, ZFUNC ALWAYS. `Renderer::AddScreenQuad` (`AddScreenQuad`) makes the two triangles of a pixel rectangle in clip
  space (⚠️ approximate: the original box draw's clip at ±0xA000 and its 100.0 are left out). `DrawSpirits` draws
  the advisor spirits ([advisors and help](../interface/advisors-and-help.md)).
- `Gui/GameMenu.cpp` (the pause menu's boxes above): `GameMenu::OptionsTabs` makes the `Dialog::Tab` list (the third
  tab is the Players page, `GameMenu::BuildPlayers`); `GameMenu::BuildOptions` and `GameMenu::SelectOptionsTab` tell the video change before going
  to `Page::Main` or leaving `Page::Options`; `GameMenu::ShowPage` keeps the detail shown each time the options open;
  `GameMenu::BuildAdvanced` adds the help text speed slider; `GameMenu::Ask` and `GameMenu::Tell` lay out the message
  box (`Layout::Wrapped` big text, Yes with `LabelSide::Right` and `Look::LeftArrow`, or one OK button);
  `GameMenu::Update` turns the controls page's wheel pictures.

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The hand's state**

- `hand_tooltips::ObjectHandState`; `HandSystem::Interactable` for `IsInteractable` (available, an abode partly
  built).
- `VillageTotemSystem::ValidForLockedSelect` with the local player.
- The land grip: 20, or 22 with the action button held (the last frame's, `HandSystem::_actionHeld`) and the camera's
  feature 4 (`hand_tooltips::LandGripHandState`), whatever the hand holds (since 2026-10-26: the holding hand grips the
  land too, world test `HandInputWorld.GrippingTheLandWhileHoldingIsTheLandGripAllTheSame`), so `GET_HAND_STATE`, the
  tooltip and the camera path's take-back see the grip with something held; the grip lasts the whole hold of the grab, as the original's
  m_Buttons keeps its press bit while the landscape lock's low bit is set (StartLandscapeGrip `or byte [edi+0x38]`
  0x5D1F7E). Its tooltip is `hand_tooltips::ZoomLandscapeToolTip`. The camera's both buttons stay read from the raw
  buttons, and the path camera's take-back keeps reading only 20, as the original's `HAND_STATE_GRIP_LANDSCAPE`.
- The hand's submit: `SetStateSubmitter`.

**The text of each state**

- `hand_tooltips::SeedToolTip`, fed by the press's tests (`hand_detail::SeedValidToApplyToObject` / `SeedValidToApplyToMapCoord`, at
  `_cursorObject` and at the action point, (0, 0, 0) when it is cleared, as the press) and
  `magic::MagicTypeForGesture` of `gestures::State().currentPowerUpGesture`. The help event has no counterpart in
  openblack's Submit.
- `magic::OneOffPickUpToolTip` (every magic type has a record there, so the seed info's text is never reached) and
  `hand_tooltips::OrbPickUpToolTip`.
- Every entity with `SpellIcon` (the worship sites' and the town centres' icons; openblack makes no arena icon),
  `hand_tooltips::SpellIconToolTip` of the seed info's `tooltip`.
- `hand_tooltips::WorkshopToolTip` with `workshops::IsWorkshop` and `abodes::IsBuilt`; for a repaired one
  `hand_tooltips::WorkshopProductionPercent` fills the text's number (tests
  `HandToolTips.ABuiltWorkshopShowsHowFarItsScaffoldHasGot`,
  `HandToolTips.AWorkshopsProgressIsCutAndTakenModuloAHundredAsTheOriginals`).
- `hand_tooltips::TownCentreForToolTips` (a `VillageTotem` gives its `townCentre`) and `hand_tooltips::TownToolTips`,
  action −1; states 13 and 18 use the same helper. openblack's plinth and icon entities are both taken as their totem
  (`VillageTotemSystemInterface::TotemOf`) for the hand's interface state and tooltips.
- `hand_tooltips::CreatureInteractToolTip`.
- `hand_tooltips::BubbleToolTip` for the hand states 0x1C and 0x1D (a help bubble under the hand, or dragged).
- The leash: `SubmitLeashToolTip` (HandToolTips.cpp) at the head of `SubmitLandToolTips` and of the states 9 (after
  the flying and cannot-be-picked-up tests), 13 and 18, as the original, from the leash service (the player's
  creature, its worn leash as "on in this hand", what it is tied to) and the creature's development phase
  (`CreatureMindState`; a creature with none counts as grown up, as the leash takes it); the choice is the pure
  `hand_tooltips::LeashToolTipOf` / `LeashToolTip`, and «Bring Creature» triggers
  `help_profile::Event::FocusLeash`.
- The building texts: `hand_tooltips::BuildingToolTips`, fed by `BuildingFactsOf` in `HandToolTips.cpp` for an abode, a
  field, the temple's heart, a worship site, a village totem (its town centre's town, its own `percentBuilt`, repaired
  with its life whole) and a town centre's spell icon (`hand_tooltips::SpellIconOwnOrNoPlayer`, with
  `worship::icon::WorshipIconOf` for the skip; repaired with its life whole). openblack's spell icons keep no percent
  built, so they are always built.
- `worship::citadel::EntranceToolTipFor`, read by `HandSystem::SubmitToolTips`.
- `hand_tooltips::StoragePitToolTip`, with `abodes::IsBuilt`.
- `hand_tooltips::PrayerEnergyToolTip` over a `WorshipTotem`, with its site's `WorshipSite::battery`; a totem whose
  site is gone shows nothing of its own and the hand goes on to the flag, the tap and the land. Our totem keeps no
  building state of its own, so the building texts never come first over it (a port row). The debug bar's Magic
  window, Prayer power, "What each altar shows" reads it for every altar.
- A script highlight: `hand_tooltips::ScriptHighlightToolTip` of the highlight's `active`, after the town desire flag
  and before the tap; `hand_tooltips::OverObjectShowsLand` is the influence test with the highlight's exception. The
  help events 0x22 / 0x23 it passes are never read, in the original too.

**The help system's part**

- The text's number: `BuildText`; the action's picture: `ResolveAction`.
- `fixed_format::Fixed` (`Common/FixedFormat`) does the same steps as the exe's number writing.
- The pause: the same test in `help::tooltips::ProcessTurn`, with `game_clock::IsPaused` and
  `game_clock::IsInsideCitadel`; nothing deletes the icon between turns, and `GameInterface::IsBoxActive` skips its
  draw (KMIcon below).
- Making the icon (`ProcessTurn`): S is 0 before the renderer exists (at `const int32_t height =
  Locator::rendererInterface::has_value() ? Locator::rendererInterface::value().GetResolu`).
- `help::tooltips::Reset`, called at `Game::PrepareNewLand`'s map clear, puts the fields back and leaves the icon
  (#206), so the same text on the new land's first turn keeps its icon; test
  `HelpToolTipsTest.ANewLandKeepsTheIconDrawnAndTheSameTextKeepsItsIcon`. ResetIcons is `ResetIcon`. The temple's
  exit (`gui::ToolTips::Clear`) still does both.

**KMIcon (the button or key drawing)**

- The frame loop is `Frame`: call it with pass 0, then pass 1; each records passAlpha. Its draw half is
  `Renderer::DrawInputPrompts` over the frame's copy (Snapshot). The tooltip's icons are made by help::tooltips; the drawing
  is Renderer::DrawKeyOrMouse.
- `GameInterface::IsBoxActive` (the menu open, not fading out, or the SkipBox shown, the same test as the fight and
  creature panels and the input) makes `input_prompt::Snapshot` leave the frame's copy empty, while
  `input_prompt::Frame` still runs both updates.
- The glow panels: `Renderer::AddKeyOrMousePanel` follows the 9-slice rule, with the exact breaks, and the help's
  click cue (`input_prompt`, from `HelpSystem::Draw3D`) draws its glow through the same panel, so the cue's glow is
  the original's: the mouse box at alpha8 and the text box at alpha8 / 3.
- The click type: `ClickTypeOfMouseCode`, through `input_prompt::MouseClickTypeOf`, which gives the zoom controls (3, 4
  and 6) with no mouse binding (action 6 always, and 3 and 4 on a mouse with no wheel) both buttons, 3, as
  fn_00471760 does (tests `InputPromptIcons.TheZoomOnControlShowsBothButtons`,
  `HandToolTips.OnAMouseWithNoWheelTheZoomControlsShowBothButtons`); `k_Defaults` has action 6 (2026-10-18). The debug
  Key Bindings window's Picture column reads out each action's picture. The mouse row: `MouseRow` (not verified):
  openblack cannot ask DirectInput's mouse caps, so a wheel mouse is assumed, for the defaults too (`k_MouseHasWheel`).
- The stand-alone mouse picture of the frame loop is not ported (it draws nothing).

### Test hooks

None of its own; the debug bar's hand readouts are listed in [debugging](../project/debugging.md). The stats overlay
shows the interface's hand state of the last turn and the tooltip text shown (`help::tooltips::ShownText`).

## Pending

### Missing texts

- **The steps marked (not ported) above**: the leash's hand
  state 32 and the bubble's extra help call in the object's state; the empty hand's vt 0x72C step (15 / 16); and the
  land texts' own `HelpProfile::Trigger(0x2C)` before the Zoom Out test (openblack's `help_profile::Event::LookAtLand`
  is not counted there).

### Drawing

- The glow panels of DrawKeyOrMouse (fn_00447BA0): the corners measure is **done** (by code comparison against the
  disassembly; see [the drawing in detail](#kmicon-the-button-or-key-drawing)), and pinned by
  `InputPromptIcons.TheGlowPanelIsANineSliceWithCornersOfHalfTheBoxsHeight` (`graphics::key_or_mouse::PanelSlices`).
  The half pixel is read and ported: `SetupThing::DrawBox` 0x412980 writes every corner with 0.5 added ([0x8AA3B4] =
  0.5, the `fadd` at 0x412ADF and its three neighbours), as `SetupThing::DrawQuad` 0x412EB0 (0x412FC6..0x41309E) and
  `SetupThing::DrawLine` 0x4125A0 (0x4125DF..0x412654) do; its last argument (100.0) is only the vertices' 1/w. The
  other 2D draws (the texts, the bubbles, the hand's tricons, the mouse picture fn_00447450 through `DrawAndClip2D`
  0x82A5B0) add nothing of their own; where they use 0.5 it halves a size. Direct3D 7 puts pixel centres on whole
  pixels, so openblack takes every pre-transformed vertex half a pixel in (`screen_point::PretransformedToClip`) and
  the box draws' corners half a pixel further (`screen_point::BoxDrawCorner`, `Renderer::AddBoxDrawQuad`): the glow
  panels, the key cap and the arrows of the input prompt icons and the help bubble's scroll arrows cover the pixels one
  right and one down of the corners they are given, as the original's; the screen quads and rectangles, the bubbles,
  the villagers' names and the tricons take the half pixel (2026-10-31; tests
  `ScreenPoint.TheBoxDrawsCornersAreHalfAPixelOn`, `ScreenPoint.APretransformedVertexKeepsItsPixelInAHalfPixelInClipSpace`;
  the View menu's "2D half pixel" switch, on by default, puts them back at their pixels for comparison).
  The interface's own canvas (`gui::Canvas`: the setup screens, the loading screen, the in-game panels) takes the
  half pixel for every vertex too, and its box draws their corners' half pixel (`Canvas::DrawBoxQuad`, `DrawBoxShape`,
  `DrawBoxLine`), each by the original it ports: the setup screens' boxes, bevels, backgrounds, tabs, arrows, symbols,
  coloured shapes and lines (`SetupThing::DrawBox` 0x412980 / 0x4132C0, `DrawQuad`, `DrawLine`, `DrawBevBox` 0x413C20,
  `DrawBg` 0x413960, `DrawBigButton` 0x412150, the graphs' draws 0x40D3C0 / 0x40E8B0 / 0x40DAB0, all ending in the
  three); the front end's pointer (fn_0053C0F0's `DrawBox`, its texture 1/512 in and 31/256 across, as
  openblack's); the creature's and the fight's panels and their bars (`DrawCreatureStats`, `DrawCreatureFightStats`,
  `DrawStatBar` 0x516B00: `DrawBox`, `DrawBevBox`); the scripts' clipping window (four `DrawBox` 0x4132C0); and the
  loading screen's pictures, frame, bar and lines (`DrawLoading`: `DrawBox`, `DrawBevBox`, `DrawLine`). With no
  offset of their own: the texts (`GatheringText`), the mouse picture, and the front end's swirling backdrop
  (`Plasma::Render` 0x53C840 through `Draw3DScreenTriangle`). 🔍 Not read: the full-screen black fades of the front end
  and the menu, which take only the half pixel (untextured and whole-screen, so the pixels they cover are the same
  either way). The big buttons' symbol styles (11..20) are 1/256 + 1/512 into their cells and 0.2421875 −
  1/256 across (`DrawBigButton`); openblack has no button of those styles (its big buttons are the square, the two
  arrows and the rotation). The player's symbol is the original's `SetupPicture` (the tattoo editor has one too)
  (`NewProfileBox::Init` makes one; `SetupPicture::Draw` 0x40FA20: (index mod n) / n + 1/512 in, 1/n − 1/256 across),
  which `DialogPainter::DrawSymbol` takes as they are (n = 4).

### Tests

- The builders' land fallback is not unit-tested (for example the leash asked twice in a turn, and its focus-leash
  event counted twice, when a builder falls back to the land tooltips): `HandSystem::SubmitToolTips` needs the hand's
  data to be built. A pure fallback plan of each builder would make it testable.

### Not traced

- **The temple's exit**: `tooltips::Reset` is also called there (`gui::ToolTips::Clear`); whether the original resets
  the tooltip there was not traced.
- **The pause delete**: whether any `PauseGame` caller runs inside `GGame::ProcessTurn`, before the help step (the
  only way the tooltip's pause delete at 0x5C9D3C is reached), was not traced.
