# Screen, text and read-outs

## Summary

What the original draws on the screen outside the 3D world and the tooltips: the row of little people a knock on a
building brings up (the population read-out), the one-press keys that fly the camera to the temple or over the realm
and show the villagers' names, the interface interaction levels the scripts set, the script's screen fade and the
cinema bars, and the game's own fonts with the hand's message. openblack draws all of them as the original does,
with the differences listed under Pending. The in-game menu's Game Statistics box is drawn too, from raffclar's
port (unverified against v1.42), all but the players' creatures on its Creature tab.

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
  [0xC4CC6C] and calls `KnockKnock`, so the read-out comes up and is held while the villager is held there. The
  same call is the disciples' preview, which is ported (`villager::DiscipleInHand`); the abode's knock from it is not.

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

**The Controls page's box.** ✅ Read 2026-10-31 (`DialogBoxKeyBinding::Init` 0x5129D0, `InitControls` 0x512BD0, the
item's draw 0x512830, `SetupList::Draw` 0x40A5C0). Its title (static text 999 at 100, 60, 600 by 100, help text
0xD7B, wrapped and centred, the big size); the list (300 at 100, 120, 600 by 320, its text halfway between the big and
the mid size, centred, ignoring keys); Load Defaults (button 0x12D at 260, 530, 280 by 40, text 0xD7C, the mid size)
and Back (big button 0x12E at 30, 530, text 0xD7A, the mid size); the options tabs (`AddOptionsTabs(4)`). Each of the
33 items is `ControlMap::GetText` 0x470910: the action's name, " : ", its key ("%s"), " " and its mouse button as
`ControlMouse::GetDisplayText` 0x46F180 writes it, "     (%s)" (five spaces, then the name in brackets). An item with a
mouse button gets its picture from the item's draw, called after the list draws its text (the list's +0x23C set),
with the list's left (+0x8), top, the box's right, bottom, and the item's top and bottom (a selected item's cut to the
list): nothing unless the item is wholly inside the list (0x512851..0x512867); the picture's top is the item's top
(the text is at the top + 2), its side the item's height or the list's text size if that is less (0x512894..0x5128A6);
its left is (left − w + right) / 2, cut towards zero, plus the width of the text up to six characters before the
first "(" (w the whole text's width and that part's, each cut to the box's width; with no "(", the whole text's), at
the list's text size (0x5128D0..0x51297E), drawn by fn_00447450 with the help profile's mouse type, white at the draw
alpha. openblack: `GameMenu::BuildControls` lays the page out the same, and `List::Draw` places the picture by
`gui::list_mouse::PlaceOf` (test `GameMenu.AControlsMousePictureStandsAtItsItemsTopByTheTextBeforeItsGap`; the
list's texts `GameMenu.ListsTheControls`). 🔍 Not read: the original adjusts the box's edges and the item's top and
bottom to the screen (`SetupThing::adjust`, `adjusty`) but measures the widths at the list's own text size; whether
`GatheringText::GetStringWidth` 0x831130 scales that size to the screen decides whether the picture keeps to the gap
at other screen sizes than the setup screens' own. openblack measures and places everything in the setup screens'
size and scales the whole.

**Load Defaults on the Controls page.** ✅ Read 2026-10-30 (`DialogBoxKeyBinding::ControlCallback` 0x512CC0, no 1.20
C). The button (control 0x12D) puts up a Yes/No message box (`SetupBox::MessageBoxA` style 2, answer id 0x130) whose
text is help text 0xD7C, `HELP_TEXT_DIALOG_LOADDEFAULTS` itself (its index among InfoScript2.txt's `ADD_TEXT` lines,
as the delete question's 0xD81 is `HELP_TEXT_DIALOG_AERYOUSUREDELETE`). A yes (event 3, answer 0x130, button 10000)
runs `ControlMap::LoadDefaults` and the page's `InitControls`, which lists the bindings again; a no does nothing.
The bindings are written only as the page is left (Back 0x12E or event 7, `SaveKeyBindings`). openblack: the button
asks with the same text (`GameMenu::BuildControls`, `Action::LoadDefaultControls`), and a yes calls
`GameActionMap::ResetKeyBindings` (`Game::HandleInterfaceAction`); the page lists the default bindings, so it shows
the same. Tests `GameMenu.LoadDefaultsAsksFirstWithItsOwnLabel`, `GameActionMap.RebindingMovesTheAction`. The
bindings are not kept with the profile yet ([key bindings](../project/progress/interface/key_bindings.md)).

**How a key reaches its action.** `GameWindowProc` → `LHKeyboard::ProcessKeyboard` 0x7DCAF0 buffers every key-down
(`SetKeyValue` 0x7DCDA0; the repeat bit 30 is not tested, only passed on to a callback) → `GGame::ProcessBufferedKeys`
→ `GGame::ProcessKey` 0x63EF20. Inside the citadel (g_game+0x205A28 == 1, 0x63F259) the key goes to the citadel's
handler fn_00794830, which takes only actions 20..25 (the room keys, fn_00794900: 20..25 → rooms 0, 1, 2, 5, 4, 6, the main room, the creature's, the challenge's, save
game, options and credits; confirmed: xref immediates and an oracle peek of its jump table 0x794950) and the room's own keys, so none of
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

#### The villagers' names

The names a named villager shows over its head while the names key's display is up (bit 0x20000 of g_game+0x14;
`GGame::ClearVariables` 0x54BF20 sets the flags to 0x20 when the game is made, so the display starts off and a land
change keeps it). ✅ read from the exe; the numbers are the exe's float constants as `xref.py brief` lists them.

- **The display's size** g_game+0x59B0, in `GGame::Process3dEngine` 0x54DA80, only in the world view of a frame that
  is not a full screen film (the citadel and the falling spell's film never reach it, 0x54E19D..0x54E211): the step is
  the frame's `g_game_time_inc` (an int, 0 while paused) × 0.003 [0x8DF89C]; below the switch's 0 or 1 it grows by the
  step and stops at it, otherwise it shrinks by the step and stops at it. The names are added while the land is drawn,
  earlier in the frame, so a frame's names have the size from before its step.
- **Who adds a name**: `Villager::Draw` 0x51B940 calls `DrawVillagerInfo` (vt +0xB44) after drawing the villager,
  even one at home or in a state that draws nothing, but returns at once for the villager the hand holds; only the
  object draw list calls `Draw`, so the names come in the order the list draws the villagers.
  `SpecialVillager::DrawVillagerInfo` 0x71F240: nothing while this villager speaks in its bubble ([0xD9C368] and
  [0xD9C36C], set only by `MakeHimSpeak` 0x71EF70, reached by the gathering box's e-mail messages) and has a 3D
  object; nothing when `Villager::DrawVillagerInfo` 0x51B510 drew its details (the details key, below); then a name
  when the display is past 0.01 [0x8C5840] or its status has bit 0x100 (byte +0xB5 & 1, 0x71F28C; no writer of that
  bit found), every villager's details are off (g_game+0x14 & 0x200000), its own details are off (+0xB5 & 2) and
  `CanShowName` 0x71F470 allows it: not in a script (vt +0x448) and not hidden (byte +0xA & 0x40, set by
  `GScript::AddScriptGameThing` 0x70D0F0 for every thing a script makes, and never cleared). The point is the 3D
  object's position (+0x40 → +0x38..+0x40) or, with none, its map coords (× 1/6553.6 [0x8AA3A4], the land's height
  plus its altitude), raised by 1.75 [0x8CF11C]; the colour white, or `GPlayer::GetPlayer3DColor` 0x64B590 of the
  player of its town (vt +0x48, then +0x1C); the size the display's.
- **`VillagerName::Add`** 0x7629E0(size, point, text, colour): nothing while the help system's wide screen is on
  (+0x45E8), nor below a size of 0.01; the size is held to 1; nothing unless `LH3DTech::ProjectPoint` 0x819390 gives
  2 (in front of the near clip and on the screen), its pixel and depth kept. The colour: when all of its red, green and
  blue are below 10 it is white; each channel then goes half way to white, rounding down (c + (255 − c) / 2), alpha
  0xFF. The text is cut at 255 characters and into lines at its line breaks, 8 lines at most (the eighth keeps the
  rest). The line height is 16 [0x8AB6E8] × the size; the box is as wide as its widest line (`GetStringWidth` of font
  j0 at the line height) + 6 [0x8AB35C] × the size, or half the screen's width (whole pixels) when that is wider than
  the screen; its left is the pixel's x less half its width, at least 0, and when its right passes the screen's width it
  is moved back to end there; its bottom is 50 [0x8C6CA4] above the pixel, its top 4 [0x8AB418] × the size + the lines'
  heights above that; its words are set in by twice the size. Then, from the leftmost label placed this frame, while
  the label it looks at starts left of its right: when that one covers it (its right past its left, its top at or
  above its bottom and its bottom at or below its top) it moves up to stand one pixel above it and looks again from
  the leftmost, stopping when the label it moved over is itself off the top; otherwise it goes on to the next. A label
  whose top is above the screen's is dropped; the others are kept from left to right, before the first that starts
  as far left.
- **Its draw** (0x54E217..0x54E2A4): while a script holds the wide screen (+0x45E8 and +0x45EC) the list is deleted
  undrawn; otherwise each label, from left to right, is queued in the Z-sorter by `VillagerName::AddDrawing` 0x7628A0
  at its point's squared distance from the camera, ((x² + y²) + z²). `VillagerName::Draw` 0x762DC0, at its place in
  the drain: its alpha ftol(size × 120 [0x8CF338]); its depth `fn_005766E0` (the did-you-know bubble's draw depth,
  [the scripts' highlights](script-highlights.md)) of its projected depth; `GatheringText::DrawBubble` 0x8334C0, a
  black box with that alpha at its top and bottom, corners a quarter of its height, no tail; then, with the bubble
  texture's material [0xED92DC] (gatheringtext.raw in mode 6, made by `SetupGameFonts` 0x830F60) and
  `SetupThing::DrawAlpha` the box's alpha, two `SetupThing::DrawBox` 0x412980 in black, each corner cut towards zero,
  kept at the depth and tested against it (its bool 1, so the frame's ZFUNC stays): the stalk (x − 3, bottom) to
  (x + 3, y − 16) with uv (0.53125, 0.05)–(0.59375, 0.05), and the head (x − 6, y − 16) to (x + 6, y) with uv (0.5,
  0.05)–(0.625, 0.125), whatever the label's size; then the lines in font j0 (`GatheringText::DrawTextRaw` 0x832C60,
  its int 1 keeping the frame's ZFUNC) at (left + inset, top + inset + line × line height) and the same depth, in the
  colour with alpha ftol(size × 255); then the label is taken off the list. A label the full queue drops stays in the
  list for the next frame.

### Interface interaction levels

SET_INTERFACE_INTERACTION 063 (0x70B220, 16 levels): GInterface+0x28's limit bits (`interface_active`; read by the
tooltips and by the hand's action press, 0x5D1350 and 0x5D14C0: [the action press](../hand/states.md#the-action-press)), CameraHelp's
feature mask (`camera_help`, read by the player's camera, `DefaultWorldCameraModel`'s `Allows`; the mask is in
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
- GScript::SetInterfaceInteraction(SCRIPT_INTERFACE_LEVEL) 0x70B220 (runblack.exe; CHL 063 SET_INTERFACE_INTERACTION through 0x70B200): how much of the interface the player keeps while a tutorial script teaches one gesture. Each level writes, through the jump table 0x70B7A8: - GScript+0xA4 = the level (always, also for an invalid one); - GInterface+0x28 (Input/InterfaceActive.h): = 0 (0, 6, 7: the interface is active again), |= 6 or |= 4; - CameraHelp::EnableCameraFeatures(F, -1) (Camera/CameraHelp.h), and for 1, 2 and 10 the auto-pitch fn_004473F0(0.448799, 15.0, 1); - the hand's reach fn_0046BF20(R) on GInterface+0x3A0 (CHand +0x4838 = min(R, 1800)): the hand keeps it (HandSystemInterface::SetHandReach), this only asks for it (LevelHandReach); - the two ControlMap switches (g_game+0x250300)->+0x652C / +0x6530. Research: dev\docs\collected\documentacion\intro\spec_misc_ops.md §063 and spec_demo_mode.md (the level table).
- [0x8CBEAC]: the hand's largest reach (CHand ctor 0x46BC3E, fn_0046BF20's bound, SetDistanceFromView's 1800). The hand owns the reach and its bound; these are the values the levels pass (in the code: `k_MaxHandReach`)
- fn_004473F0's arguments for levels 1, 2 and 10 (0x3EE5C8FA, 0x41700000) (in the code: `k_TutorialAutoPitchAngle`, `k_TutorialAutoPitchDistance`)
- 0x70B220. Above 15 (unsigned, `cmp eax, 0xF; ja`) and 9: "Unexpected interaction = %d" (0xC20648), only +0xA4 written (in the code: `Set`)
- The R a level passes to fn_0046BF20 (CHand +0x4838 = R <= 1800 ? R : 1800, `test ah, 0x41`): 75 for JUST_GRAB (0x70B30D), 1800 for the other levels that set it, nullopt for JUST_HAND_MOVE (8), JUST_HAND_INTERACTION (12) and the invalid levels, which leave the reach as it is. Set hands it to HandSystemInterface::SetHandReach, whose readers are the hand's: GInterface fn_005D1AB0 (5 calls through fn_0046BF50), CHand fn_0046DF60 (0x46E00F, 0x46E123) and CHand::SetDistanceFromView 0x46C0D0 (0x46C0E4: the distance is clamped to [2, reach]) (in the code: `LevelHandReach`)
- ControlMap +0x652C (1 from the ControlMap ctor 0x46F719) (in the code: `CameraMovesAllowed`)
- ControlMap +0x6530 (1 from the ControlMap ctor 0x46F724) (in the code: `RealmZoomsAllowed`)

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
  black over 12 seconds. `Temple::UpdateFade` 0x794280 in the citadel (real time, 1/s); openblack: `gui::ScreenFade`.
- `FADE_FINISHED` is `fn_006EBB50`; `WIDESCREEN_TRANSISTION_FINISHED` is 0x6FAC20.
- **What clears the fade:** only `GScript::Reset` 0x6EB2D0: the colour bytes +0xB8..+0xBB = 0 (0x6EB3D2..0x6EB3E4) and the step +0xB0 and alpha +0xB4 = 0 (0x6EB433 / 0x6EB439); the next `ProcessFade` then writes the colour 0 to [0xFA51D8] (the step is 0, 0x6EBA78). Its callers are `GGame::Init`, `GGame::Close`, `GGame::StartPlaygroundGame`, `GameOSFile::LoadAllGame`, `GScript::LoadBinary`, the script library's creation, the land script's script-load command (no story land uses it), `Config::ProcessOneGameTurn` and sub_641570. `LOAD_MAP` (`GScript::LoadMap` 0x6FB320, `GGame::LoadMap`, `ClearMap`) does not call it, and neither does the script reboot key (`ScriptRebootRequested` [0xD019A9], set only by `GGame::ProcessKey` 0x63F089, read at 0x54E8AC: `CleanGameForScriptReboot` 0x6EBBC0, which snaps the bars out with [0xEB9950] = 0 and clears the close clipping [0xD1A2F8], but leaves the fade). So a fade lasts across a land change. openblack: the fade starts clear with the game, a land change keeps it, and the scripts' restart (a new game, a playground's start, the testbed) puts it back to its start, no fade going on and the colour's alpha 0, at the end of `Game::ResetScripts` after the help system's reset (`gui::ScriptFade::Reset` through `CinematicDirectorSystem::OnScriptsRestart`, which turns the close clipping off too; tests `ScriptFade.TheScriptsRestartStopsTheFadeAndClearsThePicture`, `CinematicDirector.TheScriptsRestartClearsTheCloseClippingAndTheFade`).
- Each bar's height is computed by `fn_005C5780`.
- Snapping the bars (fn_005C6C40, as a hand demo does) sets the slide's timer to −FLT_MAX: `fld [0x915D18]`, `fchs`
  (0x5C6C40..0x5C6C48). Its callers: `PlayFullScreenMovie` (0x54D9EF), `HelpSystem::Reset` (0x5C55DD, right after its
  `SetWideScreen(0, 0)` at 0x5C55D6), `sub_5DACCB` (0x5DAD0D) and `GInterface::StartPlayBack` (0x5DAE29). With the
  wide screen off, `GetWideScreenPercentage` 0x5C6B60 gives 1 − |timer · 0.001 / wideScreenTime| clamped to 0, so the
  help system's reset (the scripts' restart) takes the bars away at once, never in a slide; the frame's
  `fn_005C6BB0` (timer += the frame's game ms, or `g_delta_time` in the citadel while paused, 0x5C6BBB..0x5C6BE8,
  then [0xEB9950] = the percentage at 0x5C6C00) keeps them at 0. The restart's own write of 0 to [0xEB9950]
  (0x6EBBCF) is only until that frame. openblack: `HelpSystem::Reset` calls `Hooks::snapWideScreen`
  (`CinemaBars::Snap`) after its `SetWideScreen(0, 0)`; its bars' fraction is read only after the frame's update, so the
  restart's 0 needs no write of its own. A new game too: `GGame::Init` makes the help system (`HelpSystem::Create` at
  0x54F4FE, kept at g_game +0x25005C) before `GScript::Reset` (0x54F53A), so the game opens with no bars at once,
  where openblack's bars, as made, slid out from full over the first 2 s. Tests: `CinemaBars.TakenAwayAndSnappedTheyGoAtOnce`,
  `HelpSystem.ResetTakesTheWideScreenAwayThenSnapsTheBars`.
- Reversing `SET_WIDESCREEN` in the middle of a slide: `GetWideScreenPercentage` 0x5C6AE3 is read before +0x45E8
  changes, then +0x45F0 = (on ? p : 1 − p)·wideScreenTime·1000 (0x5C6B1F..0x5C6B4E).
- `Temple::UpdateFade` writes the colour every frame it runs through `fn_0053CE60` ([0xFA51D8], 0x794361), leaving
  ProcessFade's step and alpha as they are.
- `LH3DRender::FinishFrame` 0x82F460 order: the bars (0x82F652..0x82F6DD), then the callbacks with bit 0x80000000
  (0x82F6E5..0x82F718, among them the video), and the fade `fn_0086FEE0` last (0x82F753), over the film.

- fn_005C57B0: H, W = [0xE8505A] / [0xE85058]; bar = fn_005C5780 (at `const int margin = TruncateToInt(static_cast<float>(height - 2 * barPixels) * k_BoxMarginFraction);`) (in the code: `ComputeTextRegion`)
- 0x5CB020 (in the code: `HelpTextDisplay::Reset`)

### The scripts' screen texts

Read in the v1.42 exe 2026-10-28 (raffclar's reading in his #363, checked here):
- **GAME_DRAW_TEXT** 433 (`GScript::DrawTextA` 0x709170) pops fade, size, height, width, down, across, then the text
  id: the help text database's entry, entry 0 for 0 or an id at or past the count; **GAME_DRAW_TEMP_TEXT** 434
  (`GScript::DrawTempText` 0x709290) the same with the script's string, through `CHAR2WCHAR` 0x8300A0
  (`MultiByteToWideChar`) and `UNICODE_sprintf(L"%s")` into a buffer. Each makes a 0x828-byte item (alpha +0x814,
  target +0x818, rate +0x81C all 0) at the head of the list [0xCADB68] (next +0x824), then `fn_0050ACA0`: fade 0 →
  alpha and target 1, rate 0; else target 1, rate 1 / fade (alpha stays 0); x, y, w, h = across·W, down·H, width·W,
  height·H and the line size size·H, from the screen's size then; the words `wcsncpy` 1023 and ended; the colour
  +0x820 = [0xCADB5C].
- **SET_DRAW_TEXT_COLOUR** 436 (0x7093B0) pops blue, green, red: [0xCADB5C] = 0xFF ‹ftol(red)› ‹ftol(green)›
  ‹ftol(blue)›, each the low byte; written nowhere else, so it is 0 (black) until a script sets it and lasts over
  lands and the scripts' restart. **FADE_ALL_DRAW_TEXT** 435 (0x709430) pops the time and gives each item `Reset(t)`
  0x50AC50: t 0 → alpha, target, rate 0; else target 0, rate 1 / t.
- Each frame, in HelpText's after-callback right after the fight's panel (`fn_0050B2B0`, [the frame](../rendering/frame.md)):
  the clipping window's boxes, then each item from the head (`fn_0050AD70`): alpha towards the target by
  `g_game_time_inc × 0.001 × rate`, held at it, up then down; above 0.001 its alpha byte (`ftol(alpha × 255)` held
  to 0..255) goes into the colour's and it is drawn through `GatheringText::DrawText` 0x8315B0 (the box, the size,
  centred, the line breaking of [the highlights](script-highlights.md)), first a pixel down and right with the
  colour's alpha cleared, which shows nothing; otherwise the item is unlinked and freed. `DrawText` draws nothing
  when the box is narrower than half a line or lower than a line (0x8315BF..0x8315F9).
- `GScript::Reset` 0x6EB2D0 gives every item `Reset(0)` (0x6EB414..0x6EB431) and the window `ResetClip(0)`.
- Shipped callers: the GameOver script's "Game Over" (refs/chl/GameOver.txt:52) and Land 5's credits by the villagers'
  wave (refs/chl/landcontrol5.txt:240..).

### The scripts' clipping window

Read in the v1.42 exe 2026-10-28:
- **SET_CLIPPING_WINDOW** 437 (`GScript::SetClippingWindow` 0x709470) pops time, height, width, down, across and calls
  `CreditTextItem::SetClip(across, down, width, height, time)` 0x50AFA0: the target [0xCADB44] = 1; time 0 → the
  alpha [0xCADB48] = 1 and the rate [0xCADB40] = 0, else the rate = 1 / time; then, from the screen's size then,
  x [0xCADB58] = W·across, y [0xCADB54] = H·down, w [0xCADB50] = W·width, h [0xCADB4C] = H·height, each a float
  product. **CLEAR_CLIPPING_WINDOW** 438 (0x709510) pops the time: `ResetClip(time)` 0x50B040, the target 0; time 0 →
  the alpha and the rate 0 too, else the rate 1 / time; the box stays.
- Each frame, first in `fn_0050B2B0` (before the texts), `fn_0050B080`: the alpha moves up towards the target, or else
  down to it, by `rate × (g_game_time_inc × 0.001)`, held at it; above 0.001 `SetupThing::DrawAlpha` = `ftol(alpha ×
  255)` held to 0..255 and four `SetupThing::DrawBox` 0x4132C0 calls, black 0xFF000000 at each corner, use_alpha 0:
  (−1, −1)–(W, ftol(y)), (−1, ftol(y + h))–(W, H), (−1, ftol(y))–(ftol(x), ftol(y + h)) and (ftol(x + w), ftol(y))–
  (W, ftol(y + h)) (0x50B179..0x50B290); `DrawQuad` 0x412EB0 gives each corner the alpha (0xFF × DrawAlpha) >> 8 and
  puts it at the corner + 0.5, so (inferred, the rasterisation of the time's pixel centres) each box covers the pixels
  after its first corner up to its second: the window shows ftol(x) + 1 .. ftol(x + w) across.
- `GScript::Reset` gives it `ResetClip(0)` (0x6EB40F). Its only shipped user is Land 5's credits window, opened left or
  right (refs/chl/landcontrol5.txt:100..115).
- openblack: `gui::script_text::ClipWindow` (`OpenClip`, `CloseClip`, `StepClip`, `ClipAlphaByte`, `ClipBoxes`), kept
  by `CinematicDirectorSystem` (`SetClipWindow`, `ClearClipWindow`, stepped by `UpdateTexts` before the texts,
  cleared by `OnScriptsRestart`) and drawn by `GameInterface::DrawScriptTextLayer` before the texts; tests
  `ScriptClipWindow.*`.

### Text: the original's fonts and the hand message

**Faithful** (done; the offset relative to the hand is **⚠️ approximate**).
- Fonts `Data\j0` ("Ocean Sans MM", the one for the messages), `f1` ("Footlight MT", the good advisor's) and `f3`
  ("Orange LET", the evil advisor's): ✅ the names read from each `.met`'s name field in `_gamedata\frozen\Data`
  (first given by the openblack project wiki's Resource Files page). `.met` = u32 cell height 80, wchar[128]
  name, u32 count, 28-byte records {u16 code, u16 bitmap width, s16, u16, f32 left, f32 width,
  f32 right, u32 offset and u32 size in the `.fnt`}. `.fnt` (`CachePage::RenderChar` 0x830C10): lengths of
  1-bit runs alternating 0/1 starting from 0; one byte, or 0xFF + u16; bitmap of width × 80 by rows.
- Glyph cache: white ARGB4444, alpha by 2×2 blocks with the table 0x9A3990 {0, 4, 8, 12, 15}/15, one transparent
  texel on each side; rows 40..59 at a quarter (sizes < 26). `DrawTextRaw` 0x832C60: s = size/80,
  X0 = x + advance + left·s, X1 = X0 + (width + 2)·s, height = size; advance += (left + width + right)·s; no
  kerning; mode 16 (SRCALPHA/INVSRCALPHA, alpha test ≥ 5, no Z).
  Its y and size offsets [0xED92C0] / [0xED92C4] are never written (always 0), and the positions go unrounded to
  `Draw3DScreenTriangle` 0x81C5C0 as Direct3D 7 pre-transformed vertices, whose pixel centres are at whole
  coordinates. openblack's backends put pixel centres half a pixel in, so its screen texts map each vertex with
  that half pixel (`screen_point::PretransformedToClip`, used by `Renderer::SubmitScreenText`); without it every
  glyph sampled half a pixel down and right, and the newest dialogue line came out a pixel higher than the
  original's (measured on two shot pairs, 2026-10-27). The text colours are the exe's (`fn_005CB400`); the few
  units' difference in a glyph's anti-aliased edge is the rasteriser's, not a colour. Test
  `ScreenPoint.APretransformedVertexKeepsItsPixelInAHalfPixelInClipSpace`.
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
- ✅ The fade-in, measured 2026-10-27 on the original (Land 1's opening, breakpoints at AddText 0x5CCED0 and before each
  entry's draw in DrawStack 0x5CCA01, eax = alpha, edi = slot, [esp+0x20] = scale, with the frame's game ms
  [0xEA9EC0]): the slide-in advances by game ms × 0.003 a frame, clamped to 1; a single line with nothing before it
  keeps scale 1 (alpha = ftol(255 × s): 12, 23, 35, 48 … at 16, 15, 16, 16 ms); in the stack it shrinks in from
  1/0.93 (alpha 26, 38, 51, 75 … at 32, 15, 16, 31 ms), and the line before it slides to scale 0.86 and alpha 199
  (233, 229, 228, 224 … 199). openblack's `HelpTextDisplay` gives the same alphas frame for frame (test
  `HelpTextDisplay.ALineFadesInFrameByFrameAsTheOriginalDrewIt`). There is no fade-out: older lines step down the stack.

### The in-game menu's options and pause

- The menu pauses the game while it is open; Continue (0x53FF00) hides it and calls `PauseGame(0)`, a plain flag. Its
  close event (event 7, 0x5400F2) stores `timeGetTime` in [0xBEA294].
- `MiniDialogBoxOptions::InitControls` 0x515690 sets the volume sliders to the master volumes times the float nearest
  1/127, and the detail to the running level (`DialogBoxOptions::ReadRegistrySettings` 0x51481A, [0xC381EC]). ✅ Read
  2026-10-23: `fild` of the volume, `fmul dword [0x8D8690]` (f32 0x3C010204), `fstp` to the slider (0x5156EE..0x5156F8
  for the effects, 0x515710..0x51571A for the music; `Init` 0x514F50 the same at 0x5153D8 / 0x5153FA). The product is
  exact in the x87 and rounds once to float, which is one float step below the volume / 127 for 8 of the 128 volumes
  (9, 13, 18, 26, 36, 52, 72 and 104); a slider sent back without moving gives those one lower, `ftol(value × 127)`.
  openblack: `gui::SliderOfVolume` / `gui::VolumeOfSlider` (`src/Gui/GameMenu.h`), used by `Game::CreateInterface`
  and `Game::HandleInterfaceAction`; test `GameMenu.AVolumeSliderIsTheVolumeTimesTheFloatNearestOneOver127`, the 8
  bit patterns. Without a profile value
  `DialogBoxOptions::ReadRegistrySettings` takes the help's read speed from fn_005C6CA0 (+0x4610, 0x514742..0x514755).
- The sliders are set again each time either options box shows (the Options page `MiniDialogBoxOptions` or the
  Advanced page `DialogBoxOptions`, its own tab included): event 6 (0x51425D) runs `DialogBoxOptions::InitControls`
  0x513DC0, which calls the Options box's InitControls (0x513DFA) and so the same `fmul`. After a drag the knob shows
  next time at trunc(x × 127) × 1/127, up to one step (270/127 px) from where it was left. openblack:
  `GameMenu::ShowOptionsBox` from `GameMenu::ShowPage` for the Options and Advanced pages, the volumes from
  `GameMenu::SetVolumeSource` (the configuration's, set by `Game::CreateInterface`); test
  `GameMenu.TheVolumeSlidersAreSetFromTheMainVolumesEachTimeAnOptionsPageShows`.
- `DialogBoxOptions::ControlCallback` 0x5145A3: a volume slider sets its master volume at once, `ftol(value × 127)`
  (fn_00428600 for the effects, fn_00428660 0x428660 for the music;
  [the masters](../audio/banks-and-channels.md)).
- The detail level takes effect only at the next start ("detailidx", saved at 0x514969, read by `start_system`
  0x642FCB). Auto save (AutoGuardado) is g_game +0x14 bit 16 (0x514550); the push scroll (Scroll de Empuje) is the
  profile's [0xD4C550] (0x51458E).
- Leaving either options box (fn_005148B0, on code 7 below: READ_SPEED written from the slider +0x40, 0x514906..0x514916,
  then fn_005C6CF0 → fn_005C6C90, 0x5C6DE1, 0.5 when the key is missing) sets the help's READ_SPEED, by Back, a tab,
  Escape or the menu closing; not when the whole menu closes later. openblack: in the same `TakeOptionsLeft` step
  (`Game::ApplyPlayerSettings` with the read speed); test `GameMenu.TheHelpTextSpeedIsTakenAsTheOptionsBoxIsLeft`.
- **When the Advanced choices take effect** ✅ read 2026-10-23 (xref notes #1291, #1293..#1299). The Advanced page is
  `DialogBoxOptions` ([g_game+0x250304], its selectors and boxes made by `Init` 0x513860), the Options page
  `MiniDialogBoxOptions` ([...]+0x60). Both share one callback, `DialogBoxOptions::ControlCallback` 0x5140C0
  (`DialogBoxOptions::Init` pushes it at 0x513D49 and the Options box stores it at box+0xB0, 0x513577).
  - A click (code 1, 0x514318..) on a selector only steps the box's own index (help level +0x50, story text +0x4C,
    tooltips +0x48); the check boxes tick themselves, with no handler.
  - Code 7, the box left (jump table 0x5145F8 → 0x5142E0), runs fn_005148B0: TOOLTIP_LEVEL, TEXT_DRAW,
    CREATURE_HELP, HELP_LEVEL, READ_SPEED and TEXT_TOPTOBOTTOM written to the current profile (keys table
    0x8D85F4), the left-handed box to `Profile+0x81C`, AUTOSAVE and the detail; then, with the help system made,
    `GGame::ReadRegistrySettings` 0x5550F0 and fn_005C6CF0, which take the profile's values into the help system
    (tooltips +0x45FC, text from the bottom 0xD16180, story text +0x4604, creature help +0x4608, help level through
    fn_005C79C0, read speed through fn_005C6C90); then `PlayerProfile::WriteBackToRegistry` (0x5149BF).
  - `SetupBox::SetCurrentActiveBox` 0x407A30 sends code 7 to the box being left whenever the shown box changes
    (0x407B15): Back (Hide, 0x5141FC), a tab (fn_0053F3E0, HideAll then Show, the page's own tab too), Escape
    (`GGame::ProcessKey` → Hide 0x63F202), the answer to VIDEOCHANGE and the menu closing (HideAll). A question
    (`SetOnHold`) is not a box change. So leaving the Options page applies the Advanced box's choices too.
  - openblack: `GameMenu::TakeOptionsLeft`, set by `GameMenu::ShowPage` when the Options or the Advanced page is left
    (its own tab included) and by `GameMenu::Close` over either; `Game::HandleInterfaceAction` then keeps the menu's
    choices with the profile and applies them (`Game::ApplyPlayerSettings`, through `help::ApplyPlayerChoices`, a free function of the profile's settings and the
    help system, dialogue and creature help it reaches, each passed over when not made yet; tests
    `GameMenu.TheAdvancedChoicesReachTheHelpAsTheOptionsBoxIsLeft` and, for the help level's field of view,
    `HelpSystem.SettingTheHelpLevelSendsTheAdvisorsOutHomeUnlessAScriptHoldsTheDialogue`). The
    selectors and boxes only change the menu's own choices. Tests `GameMenu.TheOptionsBoxIsLeftByBackATabEscapeOrTheMenuClosing`,
    `GameMenu.ThePlayersAndControlsPagesAreNotTheOptionsBox`. ✅ Read 2026-10-24: fn_005148B0 writes the selectors'
    places as they are (the getters fn_005140B0 +0x48 tooltips, fn_00514010 +0x4C story text, fn_00513EA0 +0x50 help
    level are plain loads), the creature help and text-from-the-bottom boxes' +0x248 and the slider's float as a
    double, and the help system takes them back unchanged; it does so only with g_game +0x250538 set (0x51497F), and
    also calls fn_0046E630 on the local hand (0x5149B5), which is an empty function (a lone `ret`). openblack takes
    each part that is made; test `GameMenu.TheAdvancedChoicesReachTheHelpAsTheOptionsBoxIsLeft` follows each Advanced
    choice from the menu to the help.
  - **The left-handed box and the hand** ✅ read 2026-10-24 (xref note #1426). The box ticks itself with no handler;
    the leave writes it to `Profile+0x81C` (0x514925, its only writer) and to the registry (0x5149BF). The hand takes
    it only in `PlayerProfile::SetCurrentProfile` 0x66B7A0, its only reader (0x66B87F): the profile read again from
    the registry, then, when the local hand's +0x484C equals `Profile+0x81C`, packet 0x64 with (`Profile+0x81C` == 0)
    (fn_00550BE0, 0x66B89F), whose handler toggles the hand (`ToggleLeftRight` 0x46C260) when its +0x484C differs
    from the packet's value (0x63CA31..0x63CA3C); so +0x484C ends as the box's opposite. `SetCurrentProfile` runs as
    either options box shows (`ReadRegistrySettings` 0x514881) and as the first page shows (its show, code 6 of
    sub_540040, calls `MainMenu::InitControls` through [0xCD0B60] vt+0x20, 0x53FBB3), which a tab shows first outside
    the citadel, and from the Players page, the game's start and its loop (17 callers). So the side changes as the
    next page shows after the leave, not at the tick, and not as the menu closes over the Advanced page.
    openblack (2026-10-24): `GameMenu::TakeProfileShown` (every page shown and the menu's opening) and
    `Game::HandleInterfaceAction` setting `EngineConfig::rightHandedHand` from the profile played (the menu's choice
    without one) after the leave is kept; test `GameMenu.TheLeftHandedBoxReachesTheHandOnlyAsAPageShowsAfterTheLeave`.
    Not read: whether the Players and Controls pages' own shows inside the citadel run `SetCurrentProfile` (openblack
    takes every page), and which of the Players page's actions do; the mirror's direction is still the hand's own
    question ([the hand](../hand/animation-and-placement.md)).
  - Code 6, either box shown (0x51425D), runs `DialogBoxOptions::InitControls` 0x513DC0 →
    `DialogBoxOptions::ReadRegistrySettings` 0x514620, which takes the box's choices again from the current profile:
    TOOLTIP_LEVEL, TEXT_DRAW, CREATURE_HELP, HELP_LEVEL, READ_SPEED, HAND_ORIENTATION, TEXT_TOPTOBOTTOM, pushscroll
    and AUTOSAVE, and the detail from the registry. A missing key gives the help system's live value (tooltips,
    story text, creature help, help level, read speed), or off (the hand, text from the bottom, push scroll), or on
    (auto save). The leave before it (code 7) has already written the box to the profile. So a script's
    SET_CREATURE_HELP, or a save loaded, is shown and put back to the profile's value at the next leave, even with
    nothing clicked.
  - openblack: `GameMenu::ShowOptionsBox` takes the profile's choices (`GameMenu::SetProfileSource`, the profile
    played, given by `Game::CreateInterface`) each time the Options or the Advanced page shows, and sets the boxes,
    selectors and slider to them. A leave not yet taken writes these same choices first, so they stay. The left-handed
    box stays as the hand is drawn and the detail as the game runs; without a profile the menu keeps its own choices,
    the creature help box following the switch while the menu is closed. Test
    `GameMenu.TheOptionsPagesTakeTheProfilesChoicesEachTimeEitherShows`.
  - ✅ Read 2026-10-23: picking a profile applies none of its help choices. The help system takes the help level
    (fn_005C79C0, writing +0x45F4 at 0x5C79C4), the read speed (fn_005C6C90, the only writer of +0x4610, 0x5C6C94)
    and the rest only through fn_005C6CF0, whose callers are the options' leave (fn_005148B0, 0x51499F) and
    `HelpSystem::Create` (0x5C5749 / 0x5C5756, from `GGame::Init` 0x54F4FE); the Players page's callback
    (sub_53E740) and the other `PlayerProfile::SetCurrentProfile` callers reach neither. So the picked profile's
    choices, its read speed included, take effect when the options' box is next left, which shows them first
    (code 6 reads the profile). openblack: `Game::PlayerProfileChanged` shows the profile on the menu and applies
    nothing; test `GameMenu.APickedProfilesChoicesWaitForTheOptionsBoxToBeLeft`.
  - ✅ Read 2026-10-23. The tooltip level is never clamped: TOOLTIP_LEVEL goes into the help system (+0x45FC) as the
    profile keeps it, at start, from a save and at the box's leave, and its readers compare it with 0, 1, 2 and 3
    (0x5C9AA1, 0x5C9D0B, 0x5C9DBD, 0x5C9E02, 0x5C9EC2, 0x456467), so a level out of the four shows tooltips with no
    minimum's filter, no intelligent level's stop or fade-out, and fades them in. Only the box wraps its own index
    (fn_00514020): |v, 400 up when negative| & 3; the help level (v, 500 up when negative) % 5 (fn_00513E10), the story
    text |v, 300 up when negative| % 3 (fn_00513F90). The next leave writes and applies that index. openblack:
    `Game::ApplyPlayerSettings` sets the level as it is, and `gui::TakeProfileSettings` takes the tooltips' index
    through `gui::OptionsIndex`; tests `GameMenu.AToolTipLevelOutOfTheSelectorsFourIsShownAtItsPlaceOnIt`,
    `HelpToolTipsTest.ALevelOutOfTheFourShowsALowTextWithoutTheIntelligentLevelsFadeOut`. The story text's and the
    help level's places are wrapped the same way (`gui::OptionsIndex` with 3 / 300 and 5 / 500; test
    `GameMenu.AStoryTextOrHelpLevelOutOfItsSelectorIsShownAtItsPlaceOnIt`).
  - ✅ Read 2026-10-23, **undefined in the original**: the help level's wrap (fn_00513E10) has no absolute value.
    0x513E17..0x513E2A: `test eax, eax; jge`, `add eax, 0x1F4`, `cdq; idiv 5`, the remainder (edx) the place. Below
    -500 the remainder is -4..-1, and 0x513E5C `mov eax, [esp + esi*4 + 8]` reads the text number from the stack
    below its five-entry table (0xD6B..0xD6F at esp+8..esp+0x18): whatever that slot holds. Out of the text
    database's range (0x513E60..0x513E66, `cmp eax, [0xD17CAC]; jae` / `test eax, eax; ja`) it falls back to the
    first text ([0xD17CA8]+8, 0x513E68), else that text is shown. The negative place is stored (+0x50, 0x513E8D),
    stepped from (code 1) and written at the leave. The story text (fn_00513F90, `cdq; xor; sub` before its
    `idiv 3`) and the tooltips (fn_00514020) take the absolute value first and are always in range. openblack does
    not reproduce the stack read: the help level's place is wrapped like the others', so the selector always shows
    one of its five; this only arises from a profile edited by hand to a help level under -500.
- The SkipBox's answer bits and its unpause are on
  [Skipping the tutorial](../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial).

#### How the menu's boxes fade into one another

✅ Read in the exe 2026-10-23 (no 1.20 C). Every page of the menu is a box of its own: the main box, the Options page
(`MiniDialogBoxOptions`), Players (`ProfileEditor`), Advanced (`DialogBoxOptions`), Controls (`DialogBoxKeyBinding`)
and the Game Statistics box (`StatsBox`). One becomes the shown box only through `SetupBox::SetCurrentActiveBox`
0x407A30, reached by `DialogBoxBase::Show` 0x5135F0 (the current box pushed on a stack of 16), `Hide` 0x513640 (back
to the box before) and `HideAll` 0x5136E0 (the stack emptied, no box).
- The box shown: its fade (+4) zeroed and sent to 1 over 0.5 s (0x407C14 / 0x407C1E), its message dim (+0x34) zeroed.
- The box left becomes `CurrentFadeBox` (written even when it is null, 0x407B25): its fade and dim go to 0 over 0.2 s
  from where they are (0x407B30, 0x407B6D). Every frame (0x64254F..0x6425B4) it is drawn first, taking no clicks, then
  the shown box: a cross-fade. `DrawAll` 0x408340 drops it once its time is up and its fade is 0 (0x408545..0x40856A).
- A box's alpha is clamp((fade − 0.75 × dim) × scale × 255, 0, 255) (0x408592..0x4085CB), in real seconds (the frame's
  `g_delta_time` × 0.001).
- The switch also starts a static 0.6 s zoomer (`SetupBox::FadeIn` 0xC4CC88, 0x407A42) that nothing reads.
- **Cross-fade:** the Options button (Show, 0x540000); every page's Back arrow (Hide: Options 0x5155C0, Players
  0x53ED69, Advanced 0x5141FC, Controls 0x5130E9 after saving the keys); the Statistics box's Back and first tab (Hide,
  0x56DAB1 / 0x56DAA7); Escape on a box that allows it (`GGame::ProcessKey` 0x63F1DD..0x63F202, Hide). The main box
  does not: ✅ read 2026-10-24, `SetupBox::UpdateWantKey` 0x407EF0 sets `CanEscape` from the last shown box's
  `CanESCOut` (vt +0x1C) while it wants the keys (vt +0x14, `WantsKeyControl` = shown), and `MainMenu::CanESCOut`
  0x53BD50 returns 0, so Escape on the first page goes to `SetupBox::Key` 0x408F80 instead: the focused control's
  `KeyDown` (a button's passes it to its box's `ClickKeyDown` 0x409170, a lone `ret`) and the box's callback with
  code 2, which on the first page (sub_540040, 0x540149) acts only on a pending question's answer 0x2712; then
  `ProcessKey` returns (0x63F2A4, `ebx` set at 0x63F1F0), so the menu is not opened or closed either. The first page
  stays up until its Continue. openblack: `GameMenu::Escape` gives `Action::None` there; test
 `GameMenu.EscapeBacksOut`. While a question is up Escape goes to the box
  instead.
- **Replace:** a tab (fn_0053F3E0 on the options pages, fn_0053F740 on the main box: HideAll, then the main box Shown,
  then the page). HideAll makes the old box the fading one, but the main box's Show overwrites `CurrentFadeBox` with
  the null active box, so the old box goes at once and the page fades in from 0; the tab of the page already shown
  fades it in again. The Statistics box's other tabs change its own controls (fn_0056D270), with no fade.
- openblack: `gui::BoxFades` (`src/Gui/BoxFades.h`), the page shown and the one leaving, used by `GameMenu`; tests
  `BoxFades.*`, `GameMenu.FadesInAndOut`; the tab of the page shown acts as any other (`gui::Dialog`'s tabs, test
  `GameMenu.TheTabOfThePageShownShowsItAgainFromNothing`). The temple's tattoo editor and story history boxes fade in from nothing each
  time they are shown too (`TattooEditorDialog::Open`, `StoryHistoryBox::SetShown`; test
  `TattooEditorDialog.ShownAgainItFadesInFromNothing`), so put away by the bars and shown again in the same frame they
  blink and fade in, as the original's. A question's box is drawn at its own dim alone, and its answer puts the box
  back at once whole (`BoxFades::SnapShown`, test `GameMenu.AnAnsweredQuestionGivesTheBoxBackWhole`), as
  `SetOnHold` 0x4081A0 / `SetOffHold` 0x408240 do (the question's alpha ftol(dim × 255), 0x40871B..0x408729; the
  box's fade set to 1, 0x408272..0x40829A).

### The dialogue text box

The box the scripts' texts (RUN_TEXT and the like) are shown in: HelpText's ring of entries, the slide-in of the
stack and the word layout.

- [0x8AB26C] (and -1/3 [0x92A550]) (in the code: `k_SlotThird`)
- [0x92A548] (in the code: `k_SecondScale`)
- [0x92A54C] (in the code: `k_OlderScale`)
- 0x5CB7F3..0x5CB82A: GetStringWidth(" ", 1, lineH) (0x9D00CC) (in the code: `MeasureText`)
- 0x5CB8CE..0x5CB8DB (in the code: `MeasureText`)
- 0x5C5974..0x5C5985 (signed, toward zero) (in the code: `click_cue::CueSize`)
- 0x5C5987..0x5C599B (in the code: `click_cue::MiddleRow`)
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
- 0x5CB338..0x5CB34F: with the flag, fn_005CB3E0 -> HelpSystem fn_005C5B50(n), the control icon KMIcon at the right of the box: every code is kept in order in DrawState::controlIcons and drawn by DialogueText::ShowControlIcons ([the control icon itself](../scripts/land1-intro.md)) (in the code: `Splitter::Code`)
- 0x5CB501 (in the code: `Splitter::SkipCode`)
- fn_005CB200 0x5CB507 (in the code: `Splitter::SkipCode`)

- fn_005CB0F0 (in the code: `IsBlank`)
- fn_005CB190 (in the code: `IsEscape`)
- fn_005CB1B0 (in the code: `IsDigit`)
- fn_005CB1D0: a digit, or fn_005CB220: C/c (fn_005CB200) or one of DFMNPdfmnp (byte table 0x5CB26C over 'D'..'p') (in the code: `IsCode`)
- fn_005CCF50: font and colour defaults of the entry being drawn (in the code: `SetEntry`)
- _wtoi 0x7C87D8 from position i (approximate: the CRT white space set) (in the code: `Splitter::ParseNumberAt`)
- fn_005CB120: past the blanks; 1 when it went past a line feed (stops right after it), 3 otherwise (in the code: `SkipBlanks`)
- fn_005CB4E0: past the escape, its code and the code's digits (and one more character after C<digits>) (in the code: `SkipCode`)

## openblack

Status: the population read-out, the held villager's home, the one-press keys for the temple, the realm and the names,
the script fade, the cinema bars and the hand message are **done**; the S key is not
ported (see Pending). The hand's other messages are the tooltips, on [their own page](tooltips.md).

- **The scripts' screen texts** ([the original](#the-scripts-screen-texts)): raffclar's `gui::script_text`
  (`src/Gui/ScriptTexts.h`, his #363) kept by `CinematicDirectorSystem` (`WriteText`, `SetTextColour`, `FadeAllTexts`,
  `UpdateTexts`, `GetTexts`, the newest first; `OnScriptsRestart` fades them at once and keeps the colour); the box is
  kept as shares and placed on the screen it is drawn on. `Game` steps them once a frame by the frame's game ms after
  the bars, not while a film plays (the help text draws nothing then); the renderer's `scriptTexts` layer, right after
  the fight's panel, draws them through `GameInterface::DrawScriptTextLayer`, broken into lines by
  `help::WrapGatheringText` (the original's) and laid out by `script_text::Lay`; no shadow is drawn. Tests
  `ScriptTexts.*`, `CinematicDirectorTexts.*`.

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
  keys ask `help::interface_interaction::IsActionBlocked` with their slot; N toggles `showVillagerNames`, the names
  display ([the villagers' names in openblack](#the-villagers-names-in-openblack)). The temple key
  times its taps by the engine's frame clock (`zoom_to::TapTime`), with the original's 500, 50 and π/6's float, 130 and
  π/4's float, 2200 and π/8, 3 and 100; a single tap while a fight is watched only starts the double tap's time; flying
  back with either key ends a tap; the realm point is halfway between the first and the last blocks' corners. Not as the
  original (see [Pending](#pending)): the single tap's point, the flights' times, the key repeat, the camera mode switch,
  the camera help's report, the S key.
- **The interface interaction levels** (`help::interface_interaction`): as in the original, only the scripts'
  restart sets the level back to normal (`interface_interaction::Set(0)` in `Game::ResetScripts`: a new game, a
  playground's start, the testbed), so a land change keeps a tutorial's level, the limit bits, the hand's reach and
  the control switches; every land load calls `camera_help::Reset` as the map's clear does (the features back to all
  but the self-tilting camera, its angle and height to their start). The Land Island window's "Kept by a land
  change" readout shows the level. See [what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets).
- **The fade and the bars**: `Gui/ScriptFade` and `Gui/CinemaBars` (`cinematicDirectorSystem`), view `RenderPass::ScreenOverlay`, `Renderer::DrawScreenOverlay`.
  `ScriptFade`'s `IsFinished` answers `FADE_FINISHED` (no fade in progress);
  `CinematicDirectorSystem`'s `IsWideScreenTransitionFinished` answers `WIDESCREEN_TRANSISTION_FINISHED`, and
  `CinematicDirectorSystem::SnapWideScreen` (`CinemaBars::Snap`) is the bars' snap, which the help system's reset
  calls right after it takes the wide screen away (`HelpSystem::Hooks::snapWideScreen`), so the scripts' restart
  takes the bars away at once. The scripts' restart (`Game::ResetScripts`: a new game, a playground's start, the
  testbed) also clears the fade (`CinematicDirectorSystem::OnScriptsRestart`); a land change keeps the fade, the bars and
  the task holding them, as the original's map clear does. The Land Island debug window's "Kept by a land change"
  readout shows the bars' drawn fraction, their task and the fade. The black of `OnNewGame`
  is `CinematicDirectorSystem::StartStory`, given by `Game::LoadStartLand` under the original's conditions
  (`new_game_choice::OpensOnBlack(started, landNumber, held)`): `LandControlAll` started, the start land's number
  (`MapScriptGlobals::landNumber`, what `SET_LAND_NUMBER` writes) is 1, and the game not held paused for the question.
  The script's existence and the loaded challenge scripts need no test of their own: `LoadStartLand` reaches the
  black only after the VM is set up and `challenge.chl` has loaded. The rule is therefore: only a new game on Land 1
  can open on black. The question with its pause is shown at every new game that can show the box, so only a Land 1
  game that shows no box (no window, or no in-game interface) opens on black; an answer given with `--skip-opening`
  counts as the question answered while the game waited.
- **The glyph cache's shuffle**: `graphics::GameFont::MakeHashPermutation` is Init's shuffle (the same swaps over
  the raw `crt::Rand`), made once right after `weather::StartAtmos` in `InitializeEngine` (`src/Locator.cpp`), whether or not a font
  is loaded, so the CRT stream after start-up is the original's. The table is kept by the resources next to the
  font cache (`ResourcesInterface::Get/SetFontHashPermutation`, set by `InitializeSimulation` when it makes the
  resources) for a hashed glyph cache; nothing reads it yet.
- **The in-game menu's actions** (`Game::HandleInterfaceAction`): the SkipBox's answer bits and the unpause; the
  options' sliders written into the configuration as `ftol(value × 127)` (Audio applies the configuration every
  frame); the options' box's choices kept with the profile and applied as the box is left, the read speed too; the pause while the menu is open (not inside
  the citadel) and Continue's unpause (`gui::StepMenuPause`), the close time (`SDL_GetTicks`) for Escape's debounce. `Game::Initialize` sets
  the sliders from `Locator::config::value` as `MiniDialogBoxOptions::InitControls` does, and the read speed as
  `DialogBoxOptions::ReadRegistrySettings` does. Auto save (0x514550) is the profile's `Settings::autoSave`, read at
  each turn's end by `save_triggers::EndTurn` (`src/ECS/SaveTriggers.cpp`; test `test_save_triggers.cpp`).
- **The Game Statistics box** (raffclar's PR #525, unverified against v1.42): the in-game menu's Statistics tab opens
  `gui::StatisticsBox` (`src/Gui/StatisticsBox.cpp`, rules in `StatisticsBoxRules`) over the menu
  (`GameMenu::OpenStatistics`): six tabs (back to the menu, then Player, Creature, Town, Miracle and Tech Stats), the
  rows of figures for up to four players as numbers or bars, three bar-graph boxes, and the influence and population
  histories as two line graphs. The figures are taken once as it opens (`player_statistics::Box()`), which steps the
  lines of code once more. On the Creature tab the box asks `RendererInterface::ShowInterfaceCreatures` to draw the
  players' creatures, which draws nothing yet (Pending). What is counted, and where, is in
  [Game statistics in openblack](../engine/loop.md#game-statistics-in-openblack); the tracker rows are in
  [statistics.md](../project/progress/interface/statistics.md).
  - **Which players it shows** ✅ read in the exe 2026-10-23 (xref note #1298). `GameStats::GetNumPlayersInGame`
    0x566850 counts the players 0..7 whose type (+0x8E0) is 1 (the human) or 2 (a computer god) and whose number
    (+0xB5) is under 4; `GetPackage` 0x566890 takes min(count, 4) and fills the figures of players 0 up to that, and
    the box shows that many slots from player 0. A player's type becomes 2 only when `TOGGLE_COMPUTER_PLAYER` turns it
    on (`GPlayer::ToggleComputerPlayer` 0x64C400). So a land turning on only the third or the fourth player (the
    Indian, Japanese, Demon and Tibetan God playgrounds) shows players one and two, the second empty, and The
    Ultimate Sandbox Map's toggle off shows the human alone. openblack: `statistics::PlayersShown`
    (`src/ECS/StatisticsRules.h`) on each `Player` entity's place and `PlayerMagic::playerType`, then the figures of
    players one up from `player_statistics::Box()`, a place without a god giving its empty figures; test
    `StatisticsRules.TheBoxCountsTheHumanAndComputerGodsUnderFourAndShowsThatManyFromPlayerOne`.
  - ✅ Read in the exe 2026-10-23. `StatsBox::Init` 0x56D370: six `SetupTabButton`s (ids 0x8FC..0x901, small text,
    the second selected), their texts by number 6778 or 6670 (Game Over, when box+0x40 is set), 6801, 6736, 6828, 6777
    and 6814 (jump table 0x56D93C), which the text script names HELP_TEXT_DIALOG_ADDITION_88, HELP_TEXT_GAME_OVER,
    ADDITION_111, 46, 138, 87 and 124; the box's own text size set to the mid size (box+4 → +0xAC), which the heading
    (`SetupStaticText` 40, 60, 720 × 28, text 6751 ADDITION_61) takes; Back a left-arrow `SetupBigButton` at 20, 530,
    40 (text 3417 TATOOBACK); the three `SetupVBarGraph`s and the four row bars (xref note #969); `SetupHLineGraph`s at
    40, 100 and 40, 230, 470 × 100 (6502 PAUSE_STATS_05, shares; 6503 PAUSE_STATS_06); a `SetupList` at 40, 365,
    720 × 150 in small text; `SetupCheckBox`es at 290, 530 (6504, ticked) and 490, 530 (6505), both of the kind a
    click only ticks (`SetupCheckBox::Click` 0x411020, +0x24C). The box's answer 0x56DA00 unticks the other check box
    (0x56DAB6..0x56DAD9). The row draw 0x56A960 writes each value in its player's colour (+0x14) when there is more
    than one, then the total in `NormalColour` (0xFFD0D0D0) unless a percent row, and "Total" (text 6500,
    PAUSE_STATS_03) once a frame (0xD06064) over a one-value row. `SetupHLineGraph::Draw` 0x40DAB0: its name in
    `ShadowColour` (black) one down and right, then in `HighlightColour` (0xFFC08020) while hovered or `DefaultColor`
    (white); the readout "%d%%" of the share × 100, or "%0.2f" up to a top of 10 and "%d" above (0x40E3C3..0x40E41C).
    The Player tab gives each line the copied graph's index as its count (fn_0056B4B0, 0x56D0E6 / 0x56D14B).
    Shown as values (+0x254 clear, 0x40DE89..0x40E188), each player's line in turn is drawn in two loops: first every
    stretch with `SetupThing::DrawLine` (x1, y1, x2, y2, the colour, adjust 1, 0.0, 100.0: one D3D line list, so one
    pixel wide, its alpha `DrawAlpha`), then for each stretch two `DrawQuad`s, from y-2 down to the line and from
    y+2 up to it, clear at the outer edge and solid at the line. The points start at left+2 and step by
    (width-4)/(count-1) as a float running sum (x += step), each place cut with `ftol`; y is
    (bottom-2) - (v-lowest)/range × (height-4), cut and held to [top+4, bottom-4]. openblack draws the same pieces in
    the same order from `statistics_box::ValueLinePieces`, pinned in
    `StatisticsBoxLayout.AValueLineIsDrawnAsWholeUnitStretchesThenTheirSoftEdges` (xref note #1357).
    Shown as shares (+0x254 set, 0x40DBF0..0x40DE84), the fewest points of any line are drawn (a count of 0 is
    replaced by the next line's, 0x40DC0B..0x40DC38). For each stretch i: the inverses of the players' sums at i and
    i+1 (1/sum, 0 for a sum of 0); x1 = ftol(x) and x2 = ftol(x + step) of the same float running sum as the values;
    then for each player its running shares (+= value × inverse), the tops (bottom-2) - (height-4) × share cut, and
    one `DrawQuad` (x1, low A) (x2, low B) (x2, high B) (x1, high A), clear, clear, solid, solid, whose triangles
    (index table 0x9C8100: 0,1,3 and 1,2,3) are the ones `DialogPainter::DrawShape` makes from the corners starting
    at the top left; the tops become the next band's feet. openblack draws `statistics_box::ShareBands`, pinned in
    `StatisticsBoxLayout.ShareBandsStackAtWholeUnitPlacesOfARunningSum`.
    openblack pins these in `StatisticsBox.*`, `StatisticsBoxRows.NumbersAreEachPlayersInTheirColumnThenTheTotal`,
    `StatisticsBoxRows.OnePlayerWritesOnlyTheTotalUnderItsHeading`, `StatisticsBoxLayout.AGraphsNameIsUnderItWithAShadowAndLightsUp`,
    `StatisticsBoxLayout.ALineGraphWritesTheValueAtThePointer` and `GameStats.TheBoxDrawsOnlyTheFinishedPoints`.
  - **A click on a row** ✅ read in the exe 2026-10-24: the box's answer 0x56DA00, for the list (+0x34)
    (0x56DAE9..0x56DB0F), takes the list's picked row (+0x248); a row under 0 (none picked) or the row the graph
    boxes last took (+0x68, which every row-to-box copy fn_0056ACA0 writes, 0x56ACBC) does nothing, any other row is
    copied into the default box (+0x4C, the right hand one). openblack: `statistics_box::ShowsPickedRow` from
    `StatisticsBox::PickRow`; test `StatisticsBoxPages.AClickShowsARowUnlessNoneIsPickedOrItIsShownAlready`.
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
  The other messages (over piles and stores, «Recoger»...) are the tooltips: [Tooltips](tooltips.md).

raffclar's tree, for comparison:
- **raffclar's** `ReadoutScale` works in double, so his steps at t = 200, 400, 600, 800 and 1000 are a step down and
  his test pins 254/255 at 1000; his read-out runs on real time and over every building of the town, drawn or not.

### The villagers' names in openblack

As [the original](#the-villagers-names) does them:

- `gui::villager_labels` (`src/Gui/VillagerLabels.h`, pure): the display's step (`StepNameFade`, and
  `StepNameFadeFrame` for the size a frame draws with and whether the land is drawn), who shows a name (`NameSize`),
  its colour (`NameColour`), the layout and the stacking (`Board::Add`) and the pointer (`PointerShape`); tests
  `VillagerLabels.*` (`test/test_villager_labels.cpp`).
- `ecs::villager_label_source::Gather` (`src/ECS/VillagerLabelSource.h`): the named villagers the object draw list
  drew this frame, in its order, without the one in the hand, each at its drawn position (`ecs::DrawnPosition`, the
  game's pose) + 1.75, projected as the game projects a point (`creature_leash::ProjectRopePoint`); none while the help
  system's wide screen is on. The hidden mark is `components::NameHidden`, which `script_held::AddScriptThing` gives
  every thing a script makes (test `ReleaseTest.AThingAScriptMadeHidesItsNameForGood`).
- `Game::PlaceVillagerNames`, in the frame's object draw list step: the names placed with the display's size from
  before the step, then the step on a frame that draws the land; a script's wide screen drops them. `FillVillagerNames`
  lays each out for the draw (`graphics::VillagerNameOverlay`: `help::BubbleShape`, `help::DrawDepth`).
- `Renderer::DrawVillagerName`: one Z object a name in the world view's transparency queue (`ZObject::villagerName`),
  queued after the gesture icons and before the did-you-know bubble, drawn in the world at its depth through the view's
  camera and tested against the world's depth: the box and the pointer in gatheringtext.raw's mode 6 material, the words
  in font j0.
- The switch: N, as the original's key (`input::ShortcutKeys`), `EngineConfig::showVillagerNames`; the debug bar's
  Villager Names > Names Display (N) shows it too, off by default. The debug overlay over every villager is its own
  switch now (Villager Names > Overlay, `showVillagerOverlay`).
- Not ported: the details (the S key), so the names never give way to them; a villager's always-show mark (no
  writer found); a named villager's speech bubble (only the e-mail box made it speak); the hidden mark is not saved
  with the world; a label the full queue drops is not kept for the next frame.

### Test hooks

- `OPENBLACK_TEST_TOOLTIP=<n>`: forces the hand's amount-in-hand message (0xEEA) with the number `n` every turn.

## Pending

- **The read-out while a villager is held near a house** ([the population read-out](#the-population-read-out)):
  the abode's knock from the held villager's disciple test is not ported.
- **The Game Statistics box against a shot of the original:** its graphs' pieces, their order and their whole-unit
  places are read from the exe and pinned by tests (`statistics_box::ValueLinePieces`, `ShareBands`), but no shot of the
  original's box has been compared with openblack's (no check opens the box).
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
  - the S key (the details of the hovered villager, or of all) is not ported (queued fix F-94): the key is bound and
    reaches the action map, but nothing acts on it, so its row in `Input/KeyBindings.h` is marked not implemented
    (2026-10-28). Porting it needs the details' text in the names' list (`Villager::DrawVillagerInfo` 0x51B510 adds
    it to the same `VillagerName` list, which openblack now draws: `gui::villager_labels::Board`), the hovered living
    thing's switch and the game's switch.
- **The villagers' names** ([in openblack](#the-villagers-names-in-openblack)): no game run has shown them against
  the original yet; the always-show mark, the speech bubble, the saved hidden mark and the dropped label's carry-over
  are not ported.

- Text, the glyph cache ([above](#text-the-originals-fonts-and-the-hand-message)): openblack's `graphics::GameFont`
  lays every glyph of a font out once, at load, in a static atlas, so it has no hash table (the shuffled permutation
  is made but not read), no eviction and no '?' under pressure. Not measured: whether the original's text ever fills the 4 pages so that a glyph is evicted while still on screen, or
  dropped to '?'; openblack never does either.

- Flights to a place, not ported:
  - the other callers of `FlyToPosFoc` have no openblack flight: fn_0043AA60's town view (0.3), `Restart` (0.1) and
    `ZoomToCitadel` (0 / 0.4; openblack's temple and realm keys, `Camera/ZoomToPlaces`, fly with `SetFlight`, which has
    no time; see [the one-press keys](#the-one-press-keys-temple-realm-villager-names));
  - the double click's flight takes the camera's heading and pitch from `EulerFromPoints` (glm's `atan`, straight up
    when both x and z are within 0.1), where the original's are `GetHeadingAndPitchFromPoints`' (in openblack
    `script_camera::HeadingAndPitchFromPoints`, within 0.01, a double); whether openblack's target origin and focus at
    the click are the original's frame origin [esp+0x14] and focus [esp+0x3C] at that point of the frame is not checked;

- **The Game Statistics box**: the players' creatures on its Creature tab are not drawn
  (`ShowInterfaceCreatures` draws nothing yet), and the box is not checked against v1.42.
- **The menus' line breaking**: `SetupThing::DrawTextWrap` 0x411750's own rule has not been read; openblack's
  `gui::GameFont::Wrap` uses raffclar's rule on our widths.
- **The in-game options not kept**: the detail level is not kept for the next start (openblack has no settings for
  it; it takes it as the Options page is left, a chosen difference); the push scroll ([0xD4C550], 0x51458E) is kept
  with the profile (`Settings::pushScrolling`, `src/Common/PlayerProfiles.h`) but nothing reads it.
