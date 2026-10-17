# Screen, text and read-outs

## Summary

What the original draws on the screen outside the 3D world and the tooltips: the row of little people a knock on a
building brings up (the population read-out), the one-press keys that fly the camera to the temple or over the realm
and show the villagers' names, the interface interaction levels the scripts set, the script's screen fade and the
cinema bars, and the game's own fonts with the hand's message. openblack draws all of them as the original does,
with the differences listed under Pending.

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change these systems.

### The population read-out

A knock on a building brings up, for 8 s, a row of little people over each drawn building of its town: one gold person
per adult living there and one green person per free place.

- **The knock.** `Abode::InterfaceTap` 0x406830 first stores `GetTown()` (vt +0x48, `Abode::GetTown` 0x401730, which may
  be null) in [0xC4CC6C] (0x40683F), then calls `HowManyPeople::KnockKnock` 0x829690 (0x406844), for every abode
  class and before the inhabitants. No ownership test: any player's town takes it.
- **The state** is four globals, never reset at a land change: [0xEB9AA8] the ms left (signed, bss: 0); [0xC383F0]
  the steps (255 in the data); [0xC383F4] the scale (1.0 in the data); [0xEA9EE0..E8] the camera's right as the last
  update left it, (1, 0, 0) from the static initialiser fn_00829660 → 0x829670.
- **`KnockKnock`** 0x829690: 0 → 8000; above 7000 (signed `jg`) unchanged; below 1000 → 8000 − t; otherwise 7000.
- **The update** `fn_008296D0` is called once per drawn frame by `GLandscape::Draw` (0x5E4889, its only caller):
  - t == 0: return; the steps, the scale and the right vector stay;
  - t −= `g_game_time_inc` [0xEA9EC0] (game time: 0 while paused, the game speed applied); negative → 0 (`jns`);
  - steps: t ≤ 1000: `ftol(t × 0.255f)` (fild, `fmul [0x9A2BA8]`); t > 7000: `ftol(255 − (t − 7000) × 0.255f)`
    (`fsubr [0x8AB270]`); otherwise 255 (0x829766);
  - scale = steps × 0.0039215689f ([0x900058], 1/255 as a float) (0x82971E), stored as a float;
  - the right vector copied from [0xEA1CF8..0xEA1D00], row 0 of the camera's inverse world to camera matrix:
    `LH3DTech::UpdateCamera` ends with `SetInverse` 0x7FB290 of the world to camera matrix 0xEA1D28 into 0xEA1CF8
    (0x819F21..0x819F2B); the falling spell's camera fn_00819F50 does the same (0x81A73F).
  The FPU runs at 24 bits ([Floating point](../engine/floating-point.md#the-x87-arithmetic)), so each product rounds to a float before
  `ftol` truncates it. At t = 200, 400, 600, 800 and 1000 the steps are 51, 102, 153, 204 and 255 (the scale 1.0 at
  1000); worked out at a higher precision they would be 50, 101, 152, 203 and 254. The growing side gives the same
  steps either way.
- **`Abode::Draw`** 0x515F70 ends (0x5162E5..0x516311) with: steps != 0 and `GetTown()` == [0xC4CC6C] →
  `DrawPercentFull(flag)`; otherwise flag != 0 → `DrawPercentFull(flag)`; the flag is the abode's +0x7C bit 0x10 (the
  held villager's home, below). A null tapped town matches every townless abode. It runs only for the abodes the
  object list draws. `Abode::Draw` has no return before that end; it is the Draw (vt +0x610) of Abode, Creche, Wonder
  and Graveyard, and Windmill 0x516320, TownCentre 0x516450, StoragePit 0x519350 and Workshop 0x51CBF0 call it. Field
  0x528570, Football 0x531610 (MultiMapFixed's), Totem 0x51ABA0 and SpellDispenser 0x722940 do not: no row over them.
- **`DrawPercentFull`** 0x4070F0: the point = the 3D object's position (G3D +0x40, +0x38..+0x40) with
  y = ((2 × [mesh +0x28]) + y) + 1.5 (the mesh's unscaled half height); places = GAbodeInfo +0x174
  MaxVillagersInAbode; adults = the byte +0xB4 (`GetNumAdultsInAbode` 0x4070D0). With the flag: places 1, adults −1.
  Then `HowManyPeople::Draw(places, adults, &point)` 0x829780.
- **`HowManyPeople::Draw`** 0x829780:
  - t == 0 and adults != −1: nothing (0x82978E..0x829797);
  - the sprites: when places exceeds the count made ([0xEB9AA4]), `LH3DSprite::Create(places, 1)` 0x8404A0 again
    (SetToZero, flags |= 0x80: the sprite's own material at +0x2C); the material [0xEB9AAC], made once,
    `CreateMaterial(15, misc0.raw [0xEA1A90])` with +5 |= 1 (two-sided) and +4 = 5 (ALPHAREF); flags &= ~6, |= 0x39
    (cell 57 = column 1 of row 7, mode A: in the plane of the screen); base size 0.9 (0x3F666666);
  - the row's start = point + right × (−places × 0.5), each component a float product and sum (0x829869..0x8298EB);
  - marker i: k = i / 1.3f ([0xC383F8], 1.3f, never written), at start + right × k (0x8298FB..0x8299FC);
  - colour: i < adults (signed) gold 0xFFF8D235, else green 0xFF5C822F; size (the half size +0xC) = scale / 1.3f, at
    least 0.0001 ([0x8BF518]); the stretch stays 1 and the origin 0 (+0x18 / +0x1C are 0 and only scaled);
  - adults −1: teal 0xFF4995A9 at 1 / 1.3f, the scale not applied;
  - each marker is drawn at once with `LH3DSprite::Draw` 0x840530 (0x8299FF), during the abode's own draw. There is
    no `LH3DSprite::AddDrawing` (0x840C70, the Z-sorter's `NewZObject` at 0x840CB3) in `HowManyPeople::Draw`, and
    `LH3DSprite::Draw` makes its triangles with `LH3DRender::DrawTriangle` 0x82F810 (0x840C57).
- **Where the rows go in the frame.** In `Abode::Draw` the building is drawn first (`MultiMapFixed::Draw` 0x516132:
  at once, or queued by its mesh's 0x200 flag like every model), then its chimney smoke is queued
  (`LH3DSmoke::AddDrawing` 0x7F8D30 at 0x5162E0), then the row (0x516311), the last call before its `ret`. So each row
  is drawn at once in the object pass (stage 4m of [The frame and its draw stages](../rendering/frame.md)), right after its building
  and before the next object of the list, before everything the Z-sorter drains at the end of the frame, a queued
  building's own draw included. Its material, mode 15, blends and writes Z.
- **The held villager's home**. `Villager::InHand` 0x76ADE0, the IN_HAND state's function, run
  every turn, sets `GetAbode()` (0x752160) +0x7C |= 0x10 (0x76ADF9) when the villager has a home, before its
  `IsInACreaturesHand` test (0x76AE0F), so in a creature's hand too. `Villager::ExitInHand` 0x76B000 clears it
  (&= 0xEF, 0x76B020) once `Living::ExitInHand` 0x5ED500 lets the villager go (into 10, 11 or a death state 13..17;
  [Villagers: data and states](../beings/villager-states.md), "What can refuse"). They are the only `or byte [reg + 0x7C], 0x10` and
  `and byte [reg + 0x7C], 0xEF` in the exe's code (a byte search).
  `Abode::Draw` then calls `DrawPercentFull(1)`: one teal person at full size over the home whenever it is drawn,
  read-out or not, and in place of the home's own row while the read-out shows. Its row starts half a unit along the
  right vector the read-out's update last kept, which a frame without a read-out does not refresh ((1, 0, 0) until
  the first knock).
- **(not ported)** holding a villager near a house: `Villager::InHand` → `FindCloseObjectsForInteract` 0x756E80 → the
  nearest object's `DiscipleInHandNear` (vt +0x7F4, 0x76AE5B); an Abode's (0x407420), with places > 0, the villager
  within `Get2DRadius`, its town inhabited and the holding status's player the town's, stores the town in
  [0xC4CC6C] and calls `KnockKnock`, so the read-out comes up and is held while the villager is held there. Blocked
  by the disciples' port (the same call is the disciples' preview) (queued fix F-16).

- fn_005CB750(region, text, &width, &height): the widest line and the height of the wrapped text, with the font, colour and line height of the state (the codes it walks change the state, as in the original: the draw that follows starts with the font and colour the measure ended with). The font +0x8 is never null here (0x5CB779 returns without writing the outputs when it is). (in the code: `MeasureText`)

#### The one-press keys: temple, realm, villager names

**The action table.** `ControlMap::LoadDefaults` 0x46F890 fills the 33 bindable actions (stride 0x30C: name, key,
mouse). Slot *n* is openblack's `BindableActionMap` bit *n*, and every default key and mouse button is openblack's
(`Input/KeyBindings.h`). The slots read here:

| Slot | Name (help text, in 0x46F890) | Default key |
|---|---|---|
| 17 | ZOOMTO_CITADEL (inline text 0xA6C, 0x46FFF5..0x470018) | Space (0x39) |
| 19 | ZOOMTO_REALM (0xA6E, `lea [ebx+0x39E4]` 0x470103) | F3 (0x3D) |
| 27 | SHOWVILLAGERNAME (0xA77, `lea [ebx+0x5244]` 0x470532) | N (0x31, `push 0x31` 0x47053F) |
| 28 | SHOWVILLAGERDETAILS (0xA7E, `lea [ebx+0x5550]`) | S (0x1F) |

The text ids are the `GetTextL` indices, 4 below `HELP_TEXT_ACTIONS_*` in bw1-decomp's `HelpTextEnums.h` for every slot
(0xA5E is HELP 2658 for F1 in slot 0, 0xA76 is TALK 2682 for T in slot 5).

**How a key reaches its action.** `GameWindowProc` → `LHKeyboard::ProcessKeyboard` 0x7DCAF0 buffers every key-down
(`SetKeyValue` 0x7DCDA0; the repeat bit 30 is not tested, only passed on to a callback) → `GGame::ProcessBufferedKeys`
→ `GGame::ProcessKey` 0x63EF20. Inside the citadel (g_game+0x205A28 == 1, 0x63F259) the key goes to the citadel's
handler fn_00794830, which takes only actions 20..25 (the room keys, fn_00794900) and the room's own keys, so none of
these three acts there. Outside, `ProcessBindableKeys` 0x63F830 maps the key and its one modifier to an action with
fn_00470A00 (which also asks fn_0046F750, the ControlMap switches: slots 17..19 are blocked when either switch is off)
and calls `GGame::DoAction` 0x63F940. `DoAction` does nothing at all while a script holds the wide screen (HelpSystem
+0x45E8 && +0x45EC, 0x63F952..0x63F965), then jumps on the action (byte table 0x63FF58, targets 0x63FF0C):

- **17, the temple key** (0x63F99B): the current player's fn_0064A7A0(0);
- **19, the realm key** (0x63FB5D): the same with 1;
- **27, the names key** (0x63FC6E): toggles bit 0x20000 of g_game+0x14 (0x63FC82..0x63FC94). The frame
  (0x54E19D..0x54E211) fades g_game+0x59B0 towards that bit at 0.003 [0x8DF89C] × `g_game_time_inc`, and then draws
  the `VillagerName` list (`First` 0xDB9E28) unless a script holds the wide screen (0x54E217..0x54E233);
  `SpecialVillager::DrawVillagerInfo` reads the fade (0x71F279, 0x71F2D8);
- **28, the details key** (0x63FCA3): toggles bit 0x200 of the hovered `Living`'s +0xB4 (interface +0x3C8, 0x63FCB7..),
  or bit 0x200000 of g_game+0x14 when nothing living is hovered (0x63FD08). `Villager::DrawVillagerInfo` 0x51B510
  draws when either is set (0x51B51B, 0x51B52A).

**fn_0064A7A0(realm)**: makes the camera mode a `CameraModeNew3` if it is not one (0x64A7C4..0x64A815), then calls
`CameraModeNew3::ZoomToCitadel(x, z, distance, pitch, tapAllowed)` 0x457B60:

- realm: the point fn_005E5C90 returns (the global 0xD1A360, (2560, 0, 2560) before a land loads, 0x5E3F30), 2200
  (0x45098000), π/8 (0x3EC90FDB), tap not allowed (0x64A829..0x64A854);
- temple, without a citadel (player +0xA48 == 0): the same point and pitch, tap allowed (0x64A856..0x64A883);
- temple: the citadel's MapCoords (+0x14, × 10/65536 [0x8AA3A4]), `CitadelDistance` 130 [0x9CE694], `CitadelPitch`
  0x3F490FD8 [0x9CE698] (three float steps below π/4), tap allowed (0x64A885..0x64A8D3).

`GLandscape::Open` 0x5E52E0 sets the realm point when a land loads (0x5E5340..0x5E545B): x and z each halfway between the
smallest and the largest *start* coordinate (index × 160) of the land blocks that exist, y 0.

**ZoomToCitadel** (now = `GetTickCount`, +0x2F8 the last tap, +0x08 "flown to a place", +0x24 / +0x30 the view to fly
back to):

- *Single tap* (now − last > 500 and the tap allowed, 0x457BB7..0x457BD0): nothing but last = now while the mode
  watches a fight (+0x44, 0x457BD6 → 0x457D83). Otherwise the distance and pitch become 50 [0x9CE6A0] and 0x3F060A90
  [0x9CE6A4] (two float steps below π/6); `UpdateClickParams(origin, focus, 1)` 0x459610 is called, and the target is
  origin + unit(focus − origin) × +0x1C0 (0x457C06..0x457CDB), +0x1C0 being the harmonic mean of the land hits of 16
  rays down the middle of the screen, blended with +0x2E4 by the pitch (0x459ACE..0x459BCC); the focus is that point's
  land height + 3 [0x9CE618]; the heading is fn_00442860(origin, focus); `SetPointFromPointDistanceHeadingAndPitch`
  0x442810 (0x457D69) and `FlyToPosFoc` with t = 0 (0x457D80); then last = now.
- *Zoom* (a double tap, or the realm key): if +0x08, and the target's (x, z) is within 100 [0x9CE69C] of the current
  focus (dist² ≤ 100², 0x457D9E..0x457DCF), fly back to +0x24 / +0x30 with t = 0.4 (0x457DEA) and clear +0x08.
  Otherwise +0x08 is cleared, the current view saved, and the camera flown to the target's land height + 3 from the
  distance and pitch, keeping the heading, t = 0.4 (0x457EB7, 0x457ED2), +0x08 = 1. Both ways end with last = 0
  (0x457EE3) and `CameraHelpCallback(0x307)` (0x457F01).

- 0x63F430 / 0x63F43E (in the code: `KeyShortcutsEnabled`)
- fn_0046F750(action), from ControlMap::IsActionPerformed 0x470AF9, fn_00470A00 and fn_00470A60: true = the bindable action is blocked. Switch 1 off: actions 3..19 but 5 (byte table 0x46F7A4); switch 1 on and switch 2 off: 17..19. 🔍 inferred with openblack's BindableActionMap bit order (Input/GameActionMapInterface.h) that is every camera move but TALK, and ZOOM_TO_TEMPLE / ZOOM_TO_CREATURE / ZOOM_TO_REALM. (pending) GameActionMap does not ask it yet (in the code: `IsActionBlocked`)

### Interface interaction levels

SET_INTERFACE_INTERACTION 063 (0x70B220, 16 levels): GInterface+0x28's limit bits (`interface_active`), CameraHelp's
feature mask (`camera_help`, storage only: the camera does not read it yet, pending; the mask is in
[Player camera features (CameraHelp)](../camera/player-camera.md#player-camera-features-camerahelp)), the hand's reach (fn_0046BF20 with 75 in
JUST_GRAB, 0x42960000 at 0x70B30D, and 1800 in the other levels that set it: the hand's `SetHandReach`; JUST_HAND_MOVE 8
(0x70B4BC), JUST_HAND_INTERACTION 12 (0x70B520, which only sets bit 2) and the invalid levels (9 and > 15, 0x70B785)
leave it), the two key switches; level 9 logs "Unexpected interaction".
GInterface::SetActive (0x5CEDC0): the script's wide screen makes the interface inactive (0x5C6AF4), a hand demo active.

- `SET_INTERFACE_INTERACTION` = `GScript::SetInterfaceInteraction` 0x70B200: POP the level (0x6F6BC0) → 0x70B220.
- `CleanGameForScriptReboot` 0x6EB330 ends a hand demo that is still running. Also [0x6EBC77..0x6EBD10]: GInterface+0x28
  = 0, hand reach 1800, camera traits 0x1BF, ControlMap switches 1 / 1, and then `SetInterfaceInteraction(0)`, which
  writes them all again. fn_005D1260 [0x6EBC67] is not ported.
- `INSIDE_TEMPLE` 0x6FF5A0: `PUSH(g_game +0x205A28 == 1, VMType 6)` (0x6FF5B4).
- `FADE_FINISHED` 0x6FCE40: no fade in progress.

- JUST_GRAB: 0x42960000 (0x70B30D) (in the code: `LevelHandReach`)
- [0x8CBEAC] (in the code: `LevelHandReach`)
- GScript::SetInterfaceInteraction(SCRIPT_INTERFACE_LEVEL) 0x70B220 (runblack.exe; CHL 063 SET_INTERFACE_INTERACTION through 0x70B200): how much of the interface the player keeps while a tutorial script teaches one gesture. Each level writes, through the jump table 0x70B7A8: - GScript+0xA4 = the level (always, also for an invalid one); - GInterface+0x28 (Input/InterfaceActive.h): = 0 (0, 6, 7: the interface is active again), |= 6 or |= 4; - CameraHelp::EnableCameraFeatures(F, -1) (Camera/CameraHelp.h), and for 1, 2 and 10 the auto-pitch fn_004473F0(0.448799, 15.0, 1); - the hand's reach fn_0046BF20(R) on GInterface+0x3A0 (CHand +0x4838 = min(R, 1800)): the hand keeps it (owner Mano, HandSystemInterface::SetHandReach), this only asks for it (LevelHandReach); - the two ControlMap switches (g_game+0x250300)->+0x652C / +0x6530. Research: dev\docs\collected\documentacion\intro\spec_misc_ops.md §063 and spec_demo_mode.md (the level table).
- [0x8CBEAC]: the hand's largest reach (CHand ctor 0x46BC3E, fn_0046BF20's bound, SetDistanceFromView's 1800). The hand owns the reach and its bound; these are the values the levels pass (in the code: `k_MaxHandReach`)
- fn_004473F0's arguments for levels 1, 2 and 10 (0x3EE5C8FA, 0x41700000) (in the code: `k_TutorialAutoPitchParam1`)
- 0x70B220. Above 15 (unsigned, `cmp eax, 0xF; ja`) and 9: "Unexpected interaction = %d" (0xC20648), only +0xA4 written (in the code: `Set`)
- The R a level passes to fn_0046BF20 (CHand +0x4838 = R <= 1800 ? R : 1800, `test ah, 0x41`): 75 for JUST_GRAB (0x70B30D), 1800 for the other levels that set it, nullopt for JUST_HAND_MOVE (8), JUST_HAND_INTERACTION (12) and the invalid levels, which leave the reach as it is. Set hands it to HandSystemInterface::SetHandReach, whose readers are the hand's: GInterface fn_005D1AB0 (5 calls through fn_0046BF50), CHand fn_0046DF60 (0x46E00F, 0x46E123) and CHand::SetDistanceFromView 0x46C0D0 (0x46C0E4: the distance is clamped to [2, reach]) (in the code: `LevelHandReach`)
- ControlMap +0x652C (1 from the ControlMap ctor 0x46F719) (in the code: `GetControlSwitch1`)
- ControlMap +0x6530 (1 from the ControlMap ctor 0x46F724) (in the code: `GetControlSwitch2`)

### Screen fade and cinema bars

**Faithful** (done, with the black of `OnNewGame`).
- GScript state (g_game+0x250090): +0xB0 alpha step per turn, +0xB4 alpha, +0xB8 ARGB colour.
  - `SET_FADE(r, g, b, t)` (0x6FCD70 → `SetupScreenFadeTo` 0x6EBA90; all truncated, t as a char): t ≤ 0 → A = 255
    instantly; otherwise, alpha 0 and step 255/(10t) (the A byte does not change until the next turn).
  - `SET_FADE_IN(t)` (0x6FCE00 → 0x6EBB00): t ≤ 0 → A = 0; otherwise, alpha 255 and step −255/(10t).
  - `FADE_FINISHED` = step == 0. `ProcessFade` 0x6EB9D0 once per game turn (100 ms): it does not advance while paused.
- Drawing `fn_0086FEE0` (FinishFrame, right before EndScene, after the help and the 2D rectangles): if A ≠ 0, colour
  quad x 0..W−1, y h'..H−1−h' (h' = h − 1 with bars), mode 1, ZFUNC ALWAYS; then the bars again on top; the colour
  is set to 0 every frame.
- Bars `[0xEB9950]` = f: height `trunc((H − 0.5625·W)·f)/2` (16:9 with f = 1; nothing on wider screens),
  black. `SET_WIDESCREEN` (32, `HelpSystem::SetWideScreen` 0x5C6AD0) slides f linearly over
  `HelpSystemInfo.wideScreenTime` = 2 s of game time, continuing from wherever it is; `WIDESCREEN_TRANSISTION_FINISHED` (132).
- Other sources: `OnNewGame` 0x5538E0 sets black instantly (`SetupScreenFadeTo(0, 0, 0, 0)` 0x55397F), and then
  current = target = 0 for `Temple::UpdateFade` (0x553984 / 0x55398E), only when `LandControlAll` started on Land 1
  **and** the game is not paused after the start-of-game question (`test [g_game+0x14], 4; jne` 0x55396B..0x55396F).
  The first is bl, cleared at 0x553932 and set to 1 only at 0x553959 (tested at 0x553961), after four checks in a row:
  the game's GScript exists (g_game+0x250090 != 0, 0x553929..0x553936), the challenge scripts are loaded ([0xD95C0C]
  != 0, 0x553938..0x553940; `GScript::Reset` 0x6EB2DF clears it, and `GScript::LoadBinary` 0x6EB65E sets it once
  the challenge binary has loaded, as the load of `GlobalChallenges.txt` does at 0x6EB5F5), the land number is 1
  (`cmp [g_game+0x205A08], 1` 0x553942, the value `SET_LAND_NUMBER` writes, [The land script reader](../scripts/map-script-reader.md)) and
  `StartScript("LandControlAll")` 0x553950 returned non-zero. On any other start land neither the script nor the black
  is started. The question runs between the two
  (0x55395B) and always calls `PauseGame(1)` (0x54CC0D, [Skipping the tutorial (SkipBox and CAN_SKIP_TUTORIAL)](../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)),
  which sets that bit at once (0x54AE5A..0x54AE68) in a single-player game (`IsSinglePlayer` 0x54AE46): a single-player
  new game therefore never gets this black; the `FollowUs` intro's own `SET_FADE_IN(12)` fades its picture in from
  black over 12 seconds. `Temple::UpdateFade` 0x794280 in the citadel (real time, 1/s), not ported.
- `FADE_FINISHED` is `fn_006EBB50`; `WIDESCREEN_TRANSISTION_FINISHED` is 0x6FAC20.
- Each bar's height is computed by `fn_005C5780`.
- Snapping the bars (fn_005C6C40, as a hand demo does) sets the slide's timer to −FLT_MAX: `fld [0x915D18]`, `fchs`
  (0x5C6C40..0x5C6C48).
- Reversing `SET_WIDESCREEN` in the middle of a slide: `GetWideScreenPercentage` 0x5C6AE3 is read before +0x45E8
  changes, then +0x45F0 = (on ? p : 1 − p)·wideScreenTime·1000 (0x5C6B1F..0x5C6B4E).
- `Temple::UpdateFade` writes the colour every frame it runs through `fn_0053CE60` ([0xFA51D8], 0x794361), leaving
  ProcessFade's step and alpha as they are.
- `LH3DRender::FinishFrame` 0x82F460 order: the bars (0x82F652..0x82F6DD), then the callbacks with bit 0x80000000
  (0x82F6E5..0x82F718, among them the video), and the fade `fn_0086FEE0` last (0x82F753), over the film.

- fn_005C57B0: H, W = [0xE8505A] / [0xE85058]; bar = fn_005C5780 (at `const int margin = Ftol(static_cast<float>(height - 2 * barPixels) * k_BoxMarginFraction); // 0x5C57DB..0x5C58`) (in the code: `ComputeTextRegion`)
- 0x5CB020 (in the code: `HelpTextDisplay::Reset`)

### Text: the original's fonts and the hand message

**Faithful** (done; the offset relative to the hand is **⚠️ approximate**).
- Fonts `Data\j0` ("Ocean Sans MM", the one for the messages), `f1`, `f3`: `.met` = u32 cell height 80, wchar[128]
  name, u32 count, 28-byte records {u16 code, u16 bitmap width, s16, u16, f32 left, f32 width,
  f32 right, u32 offset and u32 size in the `.fnt`}. `.fnt` (`CachePage::RenderChar` 0x830C10): lengths of
  1-bit runs alternating 0/1 starting from 0; one byte, or 0xFF + u16; bitmap of width × 80 by rows.
- Glyph cache: white ARGB4444, alpha by 2×2 blocks with the table 0x9A3990 {0, 4, 8, 12, 15}/15, one transparent
  texel on each side; rows 40..59 at a quarter (sizes < 26). `DrawTextRaw` 0x832C60: s = size/80,
  X0 = x + advance + left·s, X1 = X0 + (width + 2)·s, height = size; advance += (left + width + right)·s; no
  kerning; mode 16 (SRCALPHA/INVSRCALPHA, alpha test ≥ 5, no Z).
- Message with the amount in the hand: `ToolTips::ForceToolTips(0xEEA, amount)` every turn of a locked selection
  (piles, fields, fish farms) and 12 turns afterwards; text 0xEEA from `Scripts\InfoScript2.txt` ("Cantidad:
  %3.0f"; the id is the order of the ADD_TEXT lines). `CameraHelp::DrawKeyOrMouse` 0x447EA0: next to the hand on
  screen, box of H/25 (text at 2/3), yellow text with two black copies at ±1 px; it moves to the other side of the hand
  past 2/3 of the screen. The user checked in the original that **there is no background** (the code's additive box is
  not seen) and that the amount **is shown while the hand holds it**, until it is released.
- `GatheringText::GetStringWidth` 0x831130 = Σ (left + width + right)·size/80.
- `SetupThing::DrawTextWrap` 0x411750 wraps the menus' texts by a rule of its own, not read yet.
- A message box's question (`SetupBox::MessageBoxA` 0x411246..0x4112BB) starts from the given size and takes one
  size smaller while the wrapped text is 125 or more high and the size is above `GetSmallTextSize`, instead of the
  generic fit.
- `GetMidTextSize` 0x407A00..0x407A0B (22) and `GetSmallTextSize` 0x407A10..0x407A1B (20) return the menus' mid and
  small text sizes, one bigger when `NeedsBiggerText` 0x4079C0; `GetBigTextSize` 0x407A20 is always 35. The `SetupBigButton` constructor sets the mid size only for look 2
  (0x40D2B7); with another look the exe keeps the size its caller passes.
- `DrawTextRaw` clips the height at 0x832DAB..0x832E20: above the top the glyph is cut (y = top, h −= cut), past the
  bottom h = bottom − y, nothing when h ≤ 0; the v range is cut in the same ratio of the unclipped size
  (0x832E07..0x832E20). HelpText passes its box's top and bottom (0x5CBE04..0x5CBE7F).
- The glyph material's mode 16 is made by `CachePage::Init` 0x830244 (+4 = 5).
- **The glyph cache (`FontCache`) and its start-up shuffle** (**verified** unless marked):
  - `GatheringText::SetupGameFonts` 0x830F60 (called from `start_system` at 0x6433C7, right after the atmosphere)
    calls `FontCache::Init` 0x830750 at 0x830F6F. Init fills the 256-byte table at **0xECCD1C** with 0..255
    (0x830755), then shuffles it with 255 `rand()` (0x83076C): for i = 0..254, r = `rand() % (256 − i)` and
    T[i] ↔ T[r] (0x830771..0x83078D). The draw ranges are 256, 255, ..., 2. It is **not** Fisher-Yates: r is drawn
    over 0..255 − i, not over i..255, so the permutation is biased (a valid permutation all the same). The table
    has no symbol and no struct: a file-scope global of LH3DText.cpp, written only by Init.
  - Init then zeroes the frame stamp 0xECCD18 (u8), runs `CachePage::Init` on the 4 pages at 0xED0E20 (0x128 bytes
    each) and zeroes +0x104..+0x10C of the 4 `FontFile`s at 0xECB898 (0x11C bytes each; +0x104 glyph count,
    +0x108 the 28-byte records).
  - **The table's only use is a hash permutation.** Its 10 reads (refs on 0xECCD1C, besides Init's own 4) are all
    the same inlined lookup: `FontCache::AddToCache` 0x8313CF / 0x8314E0, `FontCache::Find` 0x832C1A,
    `GetStringWidth` 0x8311DF / 0x831250, `DrawText2Texture` 0x831FB4 / 0x832025, `DrawTextRawOriented` 0x832586,
    `DrawTextRaw` 0x832F2D / 0x832FA3. With c the UTF-16 code and f the font index: a = rol16(c, 5);
    b = c ^ a; low byte of b ^= f; b = rol16(b, 3); h = (~a) ^ b; h = ((h >> 11) ^ ~h) & 0x7FF; the first probe is
    (h & 0x700) | T[h & 0xFF] (`mov al, [T + (h & 0xFF)]` keeps h's bits 8..10 in ah), into the 2048-entry table of
    8-byte `CacheEntry`s at 0xECCE20..0xED0E20, probed linearly with wrap-around, at most 0x800 steps.
  - A `CacheEntry`: +0 u16 character (0 = empty), +2 u16 glyph record index (written at 0x83086E from a binary
    search of the font's records), +4 u8 font, +5 page (low nibble) and line (high nibble), +6 u8 x in the line,
    +7 u8 frame stamp. A per-font presence bitmap at 0xED12C0 (4 × 0x2000 bytes, one bit per UTF-16 code) says
    whether a glyph is cached; a set bit makes the lookup scan the whole table without stopping at empty slots
    (0x832C29..0x832C49), a clear bit inserts at the first empty or matching slot (0x83141A..0x831455). So an entry
    is always found whatever slot it got, and no character is ever cached twice.
  - **Where a glyph is drawn from does not use the hash slot.** `AddToCacheWork` 0x8307E0 places the glyph on a
    page line chosen by two round-robin cursors (line 0xED92D4, page 0xED92D8): 4 pages × 4 lines (`CacheLine`,
    0x44 bytes at page+0x18: +0 width used of 256, +1 count, +4 up to 16 `CacheEntry*`). The line keeps the
    entry's **pointer**; its page/line/x go into +5/+6 (0x830B16..0x830B38) and `CachePage::RenderChar` 0x830C10
    draws its bitmap there. Under pressure, a line's entries whose stamp is 4 or more frames old
    (`[0xECCD18] − [+7] ≥ 4`, 0x830987..0x830993) are evicted in the line's own order (bit cleared, slot emptied,
    pointers moved up by `memmove`, count 0xED92C8 + 1) and `CacheLine::Rebuild` 0x830B80 repacks and redraws the
    rest. Each failed pass of the 4 bumps the stamp and adds 1000 to 0xED92C8; after the 4th the glyph is dropped
    and the caller retries with '?' (0x831475, 0x8312EE). The stamp also goes up once per frame in
    `LH3DRender::FinishFrame` (0x82F460 / 0x82F478), which flushes the whole cache when 0xED92C8 > 1000
    (0x82F758..0x82F7C5).
  - So the slot an entry lands in decides only its address in the hash table: no reader walks the table in slot
    order to choose, evict or draw anything (the whole-table walks are the clears in `SetupGameFonts` 0x83101F,
    `FontCache::Close` 0x8306A0 and `FinishFrame` 0x82F764). The live entries are at most 4 × 4 × 16 = 256 of
    2048, so the probe never runs out (**likely**: the bound comes from the line structure, not from a check).
    **Nothing drawn depends on the shuffled order**; only the 255 CRT draws it spends do (every later `rand()`
    user, see [Random numbers](../engine/random.md#random-numbers)).
- HelpText's fonts (ctor `fn_005CADC0`, table 0xECCD08): +0xC j0, +0x14 f1, +0x10 f3; f1 and f3 fall back to j0 when
  they are missing. The original makes them when HelpText is made, so before the first text is drawn; openblack
  preloads j0, f1 and f3 at the end of each land load (`Renderer::PreloadForLand`), so the first frame that draws
  text no longer creates their atlases. A texture is made without a `bgfx::frame()` of its own: its bytes are copied
  to bgfx (`bgfx::copy`), which needs no frames to stay alive (cine audit #9).

- UNICODE_sprintf 0x5CB88E / 0x5CBB80 of the entry's number with "%3.3f%%" (0xBF1A74) or "%3.3f" (0xBF1A68) (in the code: `FormatNumber`)
- fn_005CB960(region, text, vAlign, yOffset, flag): measure, place and draw the words of one entry; returns the measured height. lineHeight = +0x9C, alpha = +0xA4 (its low byte is the colour alpha, 0x5CBC8E). (in the code: `DrawEntryText`)
- (pending) the missionaries song karaoke (0x5CBA3C..0x5CBA62, 0x5CBB91..0x5CBDFB): music type GGlobal+0x28 in 0x33..0x35, fn_00426C40, [0xD17CB0] (newest) and the word index == GScript+0x9C get a highlight box fn_00447BA0 driven by the Zoomers 0xD17BE8/0xD17C18/0xD17C48; not drawn. The word counter [esp+0x3C] is kept for it. (in the code: `DrawEntryText`)
- fn_005CADC0 0x5CADE2..0x5CAE26: +0x9C = +0xA0 = H * (1/28 or 1/30); +0xA4 = 0xFF (0x5CAE2C, rewritten per entry) (in the code: `HelpTextDisplay::HelpTextDisplay`)
- fn_005CB010 (in the code: `HelpTextDisplay::Close`)
- The display side of the script dialogue texts: HelpText (runblack.exe, HelpSystem+0x14, 0xBC bytes), its ring of six entries, the box region, the stack of texts with its slide-in animation and the word layout. Pure logic: no bgfx, no Locator, no game globals; the renderer draws the TextFrame that Layout returns (the box with fn_0081E360, each run with GatheringText::DrawTextRaw 0x832C60). Sources: dev\docs\collected\documentacion\intro\spec_text.md and the disassembly of 0x5CAD40..0x5CAFF2, 0x5CB000..0x5CB750 (splitter), 0x5CB750..0x5CB953 (measure), 0x5CB960..0x5CBEBB (draw), 0x5CC760..0x5CCAA4 (stack), 0x5CCE60..0x5CCF72 (box, fonts, AddText), 0x5C57B0, 0x5C5970, 0x5C6E60 (bwdis.py).
- GatheringText::GetStringWidth 0x831130 of a font: the width of the text at that font size (in the code: `WidthFn`)
- ctor 0x5CAD40 / fn_005CADC0: base line height +0xA0 = screenHeight * (1/30) (0x8CF3F8), or * (1/28) (0x92A53C) when NeedsBiggerText 0x4079C0; computed once. Ends with Reset(1) (0x5CAFEA). (in the code: `HelpTextDisplay`)
- Whether Data\f1 / Data\f3 were loaded ([0xECCD0C] / [0xECCD14]); a missing one is replaced by j0 (0x5CAE3F..0x5CAE6E) (in the code: `SetFontsLoaded`)
- The GatheringText fonts of HelpText (+0xC/+0x10/+0x14, table 0xECCD08), chosen by fn_005CCEA0 (in the code: `TextFont`)

### The in-game menu's options and pause

- The menu pauses the game while it is open; Continue (0x53FF00) hides it and calls `PauseGame(0)`, a plain flag. Its
  close event (event 7, 0x5400F2) stores `timeGetTime` in [0xBEA294].
- `MiniDialogBoxOptions::InitControls` 0x515690 sets the volume sliders to the master volumes / 127, and the detail to
  the running level (`DialogBoxOptions::ReadRegistrySettings` 0x51481A, [0xC381EC]). Without a profile value
  `DialogBoxOptions::ReadRegistrySettings` takes the help's read speed from fn_005C6CA0 (+0x4610, 0x514742..0x514755).
- `DialogBoxOptions::ControlCallback` 0x5145A3: a volume slider sets its master volume at once, `ftol(value × 127)`
  (fn_00428600 for the effects, fn_00428660 0x428660 for the music;
  [the masters](../audio/banks-and-channels.md)).
- The detail level takes effect only at the next start ("detailidx", saved at 0x514969, read by `start_system`
  0x642FCB). Auto save (AutoGuardado) is g_game +0x14 bit 16 (0x514550); the push scroll (Scroll de Empuje) is the
  profile's [0xD4C550] (0x51458E).
- Closing the options (fn_005148B0, 0x514916 → fn_005C6CF0 0x5C6DC6) sets the help's READ_SPEED from the slider.
- The SkipBox's answer bits and its unpause are on
  [Skipping the tutorial](../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial).

### The dialogue text box

The box the scripts' texts (RUN_TEXT and the like) are shown in: HelpText's ring of entries, the slide-in of the
stack and the word layout.

- [0x8AB26C] (and -1/3 [0x92A550]) (in the code: `k_SlotThird`)
- [0x92A548] (in the code: `k_SecondScale`)
- [0x92A54C] (in the code: `k_OlderScale`)
- 0x5CB7F3..0x5CB82A: GetStringWidth(" ", 1, lineH) (0x9D00CC) (in the code: `MeasureText`)
- 0x5CB8CE..0x5CB8DB (in the code: `MeasureText`)
- 0x5C5974..0x5C5985 (signed, toward zero) (in the code: `ClickCueHeight`)
- 0x5C5987..0x5C599B (in the code: `ClickCueY`)
- 0x5CCED0..0x5CCEEE (+0x98 too) (in the code: `HelpTextDisplay::Add`)
- fn_005C6E60: 0 -> never (0x5C6EA4); 1 -> the current text's HelpTextData+4 (0x5C6E75..0x5C6EA3; no current text -> 1); other -> always (0x5C6E6F) (in the code: `HelpTextDisplay::IsTextDrawn`)
- fn_005CC760 0x5CC785 / 0x5CC793 return before the animation (in the code: `HelpTextDisplay::Advance`)

- The gate of fn_005CC760 (0x5CC77E..0x5CC793): fn_005C6E60 (TEXT_DRAW profile value) and not hidden (+0xAC) (in the code: `IsTextDrawn`)
- fn_005C5E50 0x5C5E53..0x5C5E66: HelpText's newest entry has a text ([[+0x14] + [+0x94] * 16 + 0x34] != 0) (in the code: `HasNewestText`)

**Splitting a text into lines.**

- 0x5CCEA4 (in the code: `DrawState::SetFont`)
- 0x5CCEAC (at `font = TextFont::J0;`) (in the code: `DrawState::SetFont`)
- 0x5CB490 (in the code: `DrawState::SetColour`)
- 0x5CCF63 (in the code: `DrawState::SetEntry`)
- 0x5CB5A1 (in the code: `Splitter::Next`)
- 0x5CB13D (in the code: `Splitter::SkipBlanks`)
- 0x5CB2CC: unknown codes ($I, $s, $g...) only lose the escape (fn_005CB4E0 0x5CB54C) (in the code: `Splitter::Code`)
- 0x5CB338..0x5CB34F: with the flag, fn_005CB3E0 -> HelpSystem fn_005C5B50(n), the control icon KMIcon at the right of the box. (pending) not ported: only recorded in DrawState::controlIcon (in the code: `Splitter::Code`)
- 0x5CB501 (in the code: `Splitter::SkipCode`)
- fn_005CB200 0x5CB507 (in the code: `Splitter::SkipCode`)

- fn_005CB0F0 (in the code: `IsBlank`)
- fn_005CB190 (in the code: `IsEscape`)
- fn_005CB1B0 (in the code: `IsDigit`)
- fn_005CB1D0: a digit, or fn_005CB220: C/c (fn_005CB200) or one of DFMNPdfmnp (byte table 0x5CB26C over 'D'..'p') (in the code: `IsCode`)
- fn_005CCF50: font and colour defaults of the entry being drawn (in the code: `SetEntry`)
- _wtoi 0x7C87D8 from position i (approximate: the CRT white space set) (in the code: `Wtoi`)
- fn_005CB120: past the blanks; 1 when it went past a line feed (stops right after it), 3 otherwise (in the code: `SkipBlanks`)
- fn_005CB4E0: past the escape, its code and the code's digits (and one more character after C<digits>) (in the code: `SkipCode`)

## openblack

Status: the population read-out, the held villager's home, the one-press keys for the temple, the realm and the names,
the script fade, the cinema bars and the hand message are **done**; the S key, the citadel's own fade and the other
hand messages are not ported (see Pending).

- **The knock read-out.** `AbodeKnockSystem` (raffclar's `abodeKnockSystem` slot and names) keeps the town, the ms
  left, the steps, the scale and the right vector, with the original's starting values. `Tap` stores the town
  (`abode_villagers::TownOf`, null for none) and calls `abode_knock::Knock` before the inhabitants. `Update` runs in
  `Game::Update` where the landscape is drawn (not in the temple, under a full screen film or the falling spell's
  film), before the object list, with `game_clock::FrameGameMs` and the camera's right as the original takes it: row
  0 of `affine::Inverse(affine::WorldToCamera(origin, focus))`, the ports of `SetInverse` and of the camera's world to
  camera matrix. The steps and the scale are
  `abode_knock::ReadoutSteps` / `StepsScale` with float products. `PeopleOver` gives the markers over one building
  (`abode_knock::DrawsReadout`, `ShowsRow`, `Readout`, `RowPoint` with `ecs::object::MeshHalfHeight`, `RowStart`,
  `MarkerPoint`). `Game::LogicFrame` asks it for each abode whose Draw the object list called this frame
  (`DrawnThisFrame`) and lays the sprites out (`billboard::SpriteQuad`, cell 57) into
  `OverlayFrame::knockReadout`, one `OverlayFrame::knockRows` entry per building. `Renderer::DrawPass` draws each
  building's row (`Renderer::DrawKnockRow`) in the main view's object pass at the end of that building's draw, where
  the shadows over it are drawn: after the shadows over an object drawn at once, and at once in the object pass for a
  queued one, when it is queued. A building drawn with its damage mesh gets its row after the damage meshes, where
  openblack draws them. Since openblack draws the models mesh by mesh, a row follows its building's whole batch, as
  the shadows over objects do, not its own building alone in the list's order ([Pending](../rendering/model-lighting.md#pending)). The
  material is misc0.raw in mode 15, ALPHAREF 5, two-sided
  (`render_modes::materials::k_KnockReadout`).
  - The tapped town is kept as an entity across a land change, as the original keeps its pointer; a town of the new
    land that takes the same entity would match while the read-out has time left.
- **The held villager's home.** The flag is `components::Abode::heldVillagersHome`, a field (no component pool changes). IN_HAND's
  state function `VillagerInHand` (LivingActionSystem.cpp) calls `villager::MarkHomeOfHeld` every turn, and its
  exit `VillagerExitInHand` calls `villager::UnmarkHomeOfHeld` only when it lets the villager go
  (`villager_animation::HandLetsGoInto`). The changes that skip the exits run it too when they leave the hand (the
  gentle put-down's LANDED), so they clear the mark as the original's do. `AbodeKnockSystem::PeopleOver` gives the
  teal person (`abode_knock::k_HeldVillagersHome`, `RowDrawn`).
- **The one-press keys** (`Input/ShortcutKeys`, `Camera/ZoomToPlaces`): the three keys on their default bindings; none
  acts while a script holds the wide screen (`HelpSystem::IsScriptWideScreen`) or inside the temple; the temple and realm
  keys ask `help::interface_interaction::IsActionBlocked` with their slot; N toggles `showVillagerNames`. The temple key
  times its taps by the engine's frame clock (`zoom_to::TapTime`), with the original's 500, 50 and π/6's float, 130 and
  π/4's float, 2200 and π/8, 3 and 100; a single tap while a fight is watched only starts the double tap's time; flying
  back with either key ends a tap; the realm point is halfway between the first and the last blocks' corners. Not as the
  original (see [Pending](../hand/animation-and-placement.md#pending)): the single tap's point, the flights' times, the key repeat, the camera mode switch,
  the camera help's report, the S key.
- **The fade and the bars**: `Gui/ScriptFade` and `Gui/CinemaBars` (`cinematicDirectorSystem`), view `RenderPass::ScreenOverlay`, `Renderer::DrawScreenOverlay`.
  `ScriptFade`'s `IsFinished` answers `FADE_FINISHED` (no fade in progress);
  `CinematicDirectorSystem`'s `IsWideScreenTransitionFinished` answers `WIDESCREEN_TRANSISTION_FINISHED`, and
  `CinematicDirectorSystem::SnapWideScreen` (`CinemaBars::Snap`) is the bars' snap. The black of `OnNewGame`
  is `CinematicDirectorSystem::StartStory`, given by `Game::LoadStartLand` under the original's conditions
  (`new_game_choice::OpensOnBlack(started, landNumber, held)`): `LandControlAll` started, the start land's number
  (`MapScriptGlobals::landNumber`, what `SET_LAND_NUMBER` writes) is 1, and the game not held paused for the question.
  The script's existence and the loaded challenge scripts need no test of their own: `LoadStartLand` reaches the
  black only after the VM is set up and `challenge.chl` has loaded. The rule is therefore: only a new game on Land 1
  can open on black. The question with its pause is shown at every new game that can show the box, so only a Land 1
  game that shows no box (no window, or no in-game interface) opens on black; an answer given with `--skip-opening`
  counts as the question answered while the game waited.
- **The glyph cache's shuffle**: `graphics::GameFont::MakeHashPermutation` is Init's shuffle (the same swaps over
  the raw `crt::Rand`), made once right after `weather::StartAtmos` in `InitializeEngine`, whether or not a font
  is loaded, so the CRT stream after start-up is the original's. The table is kept by the resources next to the
  font cache (`ResourcesInterface::Get/SetFontHashPermutation`, set by `InitializeGame` when it makes the
  resources) for a hashed glyph cache; nothing reads it yet.
- **The in-game menu's actions** (`Game::HandleInterfaceAction`): the SkipBox's answer bits and the unpause; the
  options' sliders written into the configuration as `ftol(value × 127)` (Audio applies the configuration every
  frame); the help's read speed from the slider when the options close; the pause while the menu is open (not inside
  the citadel) and Continue's unpause; the close time (`SDL_GetTicks`) for Escape's debounce. `Game::Initialize` sets
  the sliders from `Locator::config::value` as `MiniDialogBoxOptions::InitControls` does, and the read speed as
  `DialogBoxOptions::ReadRegistrySettings` does.
- **The menus' text sizes and wrapping**: `DialogPainter::MidTextSize` and `DialogPainter::SmallTextSize` are
  `GetMidTextSize` and `GetSmallTextSize` (`MidTextSize`, `SmallTextSize`, and `k_BigTextSize` = 35); `BigButton`'s label starts at the mid size for every look
  (⚠️ approximate, the exe sets it only for look 2). `StaticText::messageBox` marks a message box's question, sized as
  `SetupBox::MessageBoxA` does. `GameInterface::ShowSkipBox` opens the
  skip box sized with `DialogPainter::MidTextSize()` (the original's `GetMidTextSize` 0x407A00). `gui::GameFont`'s `GetWidth` is `GetStringWidth`; `gui::GameFont::Wrap` breaks lines by
  raffclar's rule (after spaces and hyphens, at line breaks, a word too long broken where it ends) on our widths, not
  by `SetupThing::DrawTextWrap`'s own rule (Pending).
- **The text**: `Graphics/GameFont` (R8 atlas with the same rasterisation; `GameFont::AddText` clips the height as
  `DrawTextRaw` does), `Common/HelpText`, `Renderer::DrawKeyOrMouse`
  (view `ScreenOverlay`), `fs_text`. Hook `OPENBLACK_TEST_TOOLTIP=<n>`. The offset of the text relative to the hand
  (half a box) is an estimate.
  The other messages are missing (when hovering over piles and stores, "Pick up"...).

raffclar's tree, for comparison:
- **raffclar's** `ReadoutScale` works in double, so his steps at t = 200, 400, 600, 800 and 1000 are a step down and
  his test pins 254/255 at 1000; his read-out runs on real time and over every building of the town, drawn or not.

### Test hooks

- `OPENBLACK_TEST_TOOLTIP=<n>`: shows the hand message with text `n`.

## Pending

- **The one-press keys** ([The one-press keys](#the-one-press-keys-temple-realm-villager-names)), not as the original:
  - the single tap's point: the original's is along the view at the click parameters' harmonic-mean distance
    (`UpdateClickParams`, +0x1C0); openblack's is the land under the camera's focus. The camera model's port of it
    (`DefaultWorldCameraModel::UpdateFocusPointInteractionParameters`) is private and runs on the target view;
  - the flights' times (0 for the tap, 0.4 for a zoom and the way back): openblack's `SetFlight` has no time;
  - the taps are timed by the engine's frame clock where the original reads `GetTickCount`;
  - the key repeat: the original acts on every key-down the system sends, its auto-repeat too, so holding Space taps
    again and again; openblack acts once per press;
  - a temple key and a realm key in one frame: the original acts on both, in the order the keys came; openblack on
    the temple key only;
  - the switch to `CameraModeNew3` from another camera mode, and `CameraHelpCallback(0x307)` after a zoom or a way
    back, are not reproduced; nor is the map coordinates' rounding of the temple and of the points whose height is
    read (openblack reads the height at the float point);
  - the names fade in and out at 0.003 per ms in the original; whether openblack's labels fade has not been compared;
  - the S key (the details of the hovered villager, or of all) is not ported (queued fix F-94).

- Text, the glyph cache ([above](#text-the-originals-fonts-and-the-hand-message)): openblack's `graphics::GameFont`
  lays every glyph of a font out once, at load, in a static atlas, so it has no hash table (the shuffled permutation
  is made but not read), no eviction and no '?' under pressure. Not measured: whether the original's text ever fills the 4 pages so that a glyph is evicted while still on screen, or
  dropped to '?'; openblack never does either.

- Flights to a place, not ported:
  - `SetFlight` does not set the flight clock `_flightSeconds` to 0, as `FlyToPosFoc` sets +0x78;
  - the other callers of `FlyToPosFoc` have no openblack flight: fn_0043AA60's town view (0.3), `Restart` (0.1) and
    `ZoomToCitadel` (0 / 0.4; openblack's temple and realm keys, `Camera/ZoomToPlaces`, fly with `SetFlight`, which has
    no time; see [the one-press keys](#the-one-press-keys-temple-realm-villager-names));
  - the double click's flight takes the camera's heading and pitch from `EulerFromPoints` (glm's `atan`, straight up
    when both x and z are within 0.1), where the original's are `GetHeadingAndPitchFromPoints`' (in openblack
    `script_camera::HeadingAndPitchFromPoints`, within 0.01, a double); whether openblack's target origin and focus at
    the click are the original's frame origin [esp+0x14] and focus [esp+0x3C] at that point of the frame is not checked;

- **The menus' line breaking**: `SetupThing::DrawTextWrap` 0x411750's own rule has not been read; openblack's
  `gui::GameFont::Wrap` uses raffclar's rule on our widths.
- **The in-game options not kept**: the detail level only takes effect at the next start, and openblack has no
  settings file to keep it in; auto save (0x514550) and the push scroll ([0xD4C550], 0x51458E) are not in openblack.
