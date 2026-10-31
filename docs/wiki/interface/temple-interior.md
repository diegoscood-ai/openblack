# The temple interior

## Summary

Inside the temple (the citadel) the original draws its own rooms in place of the land: the main room with its
reflection and pool, the challenge room with its list of scrolls, the creature cave and the save-game room, each
with a camera that flies between fixed views, and its own sounds (doors, scroll squeaks, wooshes, buttons, the
heart's sparks and the worship strain). The game runs a 100 ms temple turn there even while paused. openblack draws
the rooms on its own renderer, keeps the challenge records the scripts write with their pictures, runs a challenge's
reminder on the second tap of its scroll, draws those pictures on the challenge room's walls, mirrors the main room
and the five picture rooms under their floors, and plays the interior's sounds; the hand's placement inside is
ported only for its distance and scale (not its up and clip), and the challenge room's pictures and controls are raffclar's, unverified against v1.42. Everything in the rooms
fades with the temple's depth fog, and the models are lit from a light of the temple's own.
[The depth fog and the temple's light](#the-depth-fog-and-the-temples-light) has both.
The temple's outside and its slots are on [the temple and the citadel](../magic/temple-and-citadel.md).

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the interior.

### The temple interior

The original draws the citadel in case 1 of `GGame::Process3dEngine` (`DoCitadelDraw` 0x555410; the frame's order is in [Other cases: temple, video and 2D](../rendering/frame.md#other-cases-temple-video-and-2d)).

- **No sun and no glare inside ✅ faithful.** `LH3DSky::g_b_we_are_inside_citadel` [0xFA2760] is 1 only around `GLandAlignement::DrawSky` (0x555439..0x55544D; the same in `Process3dEngine`). DrawSky → fn_0086A330 → fn_0086B010 still draws the dome and the moon, which do not read the flag, but the sun (fn_0086C140, called at 0x86B618) returns at once while it is set (0x86C141..0x86C148). The "sun drawn" flag [0xFA275C] is only set after the sun is drawn (0x86C20D), and the glare (fn_0086BB60, from `LH3DRender::FinishFrame` 0x82F4C9) runs only if it is set (0x86BB78..0x86BB7F) and clears it (0x86BB92). So inside the citadel there is no glare at all, and its eased visibility [0xFA2778] keeps its value through the visit (the easing at 0x86BBA1 is skipped). `GScript::SetSunDraw` also writes [0xFA2760] (0x708EC1): the raw value the script passes, so any value but 0 hides the sun outside the temple until the next frame drawn inside it clears the flag ([the sun and its glare](../world/sky.md#the-sun-and-its-glare)).
- **The main room's reflection, under its floor and its pool (faithful, relative to the room).** `WorldRoom::Draw`
  0x79E280: with CitadelReflections [0xC381E0] (written at 0x8238E9) and CitadelLightmaps [0xC381E4] (0x82390B),
  fn_00795310(1, …) (0x79E2B5) draws the room mirrored, then its vt+4(1) and fn_007954A0(1), then the floor
  (fn_00795430(0xFF), 0x79E2D9). The mirror multiplies row 1 of the room object's matrix (+0x18: +0x20..+0x28, its
  local y axis) by −1 [0x8AB678] (0x795338..0x795364) and undoes it after the draw (0x7953A7..0x7953CE); the position
  is not touched. fn_007954A0 does the same around fn_0083D8D0 (0x7954C1..0x795525). The room is reflected in its own
  plane y = 0, through its position. openblack's rooms stand at the temple's position (0, 0, 0), turned only about y,
  so the world's y = 0 of `Camera::Reflect` (`DrawTempleReflection`) is that plane, and the pool's reflection
  (`DrawTemplePool`) shows that same mirrored room. Where the original puts the interior in the world has not been
  read ([The game camera](../camera/game-camera.md)); the plane relative to the room does not depend on it.
- **The main room's pool ✅ read** (2026-10-22). `WorldRoom::Draw` 0x79E280 draws it after the mirrored room and the
  floor (0x79E2D9) and before `MiniMap::DrawMap` (0x79E4BE) and the room itself (`TempleRoom::Draw` 0x799FE0, whose
  first act is `InnerRoom::Draw(false)` 0x799FF7). A function-static clock [0xE3A16C], never reset, moves on only while
  the main room is drawn (also through `WorldRoom::DrawDoors` once the door's swing is past 0.01): `fild g_delta_time`
  [0xC38134] (whole milliseconds, at least 1), × 0.001f [0x8AA3B0], added, all in floats (the game runs the x87 at
  single precision: `LHResetFPU` 0x7DEE00 clears its precision bits), so not ms / 1000 (0x79E2DE..0x79E2F0). Its alpha
  a = ftol(32.0 × sin(t) + 128.0), truncated (0x79E2F6..0x79E304). The water's colour is each byte of
  `Temple::CitadelColour` [0xE05FE8] × 208 >> 8 (0x79E309..0x79E35D); its specular is `Temple::CitadelSpecular`
  [0xE05FE4] as it is. One static object (`mainwater.l3d`, `mainwaterlo.l3d` without CitadelLightmaps), global alpha
  on, clipping on, is drawn twice: at (0, 0, 0), turned 0, alpha a, UV offset (−0.01t, 0.007t); then at (0, 0.05, 0),
  turned 0.7853982, alpha 255 − a (0x79E444), UV offset (0.01t, 0.005t). Its draw (vt+0x108, fn_0080DB30) is
  `LH3DPrimitive::DrawLit`, lit by the model light, times the colour, plus the specular, once the object's dynamic
  lighting is on: `SetDynamicLighting(true)` at `WorldRoom::InitEngine` (0x79D353), which takes effect with the "Light"
  detail setting [0xC38224] (detail levels 2 to 6, table 0x9A388C). It never reaches the rooms' mesh draw fn_00809E50,
  so it has no lightmap and no depth fog. openblack: `temple_pool::Advance`, `Layers` and `WaterColour`
  (`3D/TemplePool.h`), drawn by `Renderer::DrawTemplePool` shaded by the model light, unfogged; tests `TemplePool.*`;
  the debug window's "Temple fog" node shows the pool's clock and its layers' alpha. 🔍 The original draws the pool and
  the map before the room's own meshes, openblack after them; whether that shows depends on whether the water writes
  depth and whether anything of `main.l3d` lies under it (not read).
- `PictureRoomBase::Draw` 0x78F1F0 is the draw of five rooms: `ChallengeRoom` 0x781D3A, `CreditsRoom` 0x78B2D4,
  `GameOptionsRoom` 0x78D810, `SaveGameRoom` 0x79024C and `UniverseRoom` (the multiplayer room) 0x79CFA0. With its
  +0x124 set, and the same two flags, it mirrors its room the same way (0x78F21F), then the room's vt+4(1) and
  `DrawGlow(1)`, then draws the floor at alpha 0xD0 (0x78F23A) instead of 0xFF; without the flags, the floor alone at
  0xFF. +0x124 is the room's floor file, which each constructor always sets (`challengefloor.l3d`, `creditsfloor.l3d`,
  `optionsfloor.l3d`, `savegamefloor.l3d`, `multifloor.l3d`, or their `lo` copies without CitadelLightmaps), so only
  the two flags decide it. `InnerRoom::DrawFloor` 0x795430 gives the floor the colour (alpha << 24) | CitadelColour
  and turns its global alpha on (vt+0x48), so the floor's texture alpha is multiplied by 0xD0 / 0xFF over the room's
  reflection. In openblack the five rooms are mirrored and their floors drawn over the reflection at that opacity
  (`temple_draw::PlanParts`, `MeshPlan::floorOpacity`, the reflective floor programs: with a lightmap, and without one
  (`ObjectReflectiveInstanced`) for the challenge, credits, options and multiplayer floors, which have none, and for any
  submesh of a floor that has none; test
  `TempleDrawPlan.MirrorsThePictureRoomsUnderTheirFloorsAtTheirFloorsOpacity`). Not yet: the rooms' own mirrored
  vt+4(1) draw (what each room adds, not read) and their glows in the reflection.

#### The options inside the temple

Read 2026-10-22. The options room (`GameOptionsRoom::Update` 0x78D860) does nothing while any dialog is up
(`SetupBox::GetCurrentActiveBox` 0x407ED0 = [0xC4CD20], which `DialogBoxBase::Show` sets, 0x513623) or until its camera
has arrived (+0xC4's +0x124 = 1). Then, if it has not opened them (+0x160 = 0) and it is the room the player is in
(Temple +0x1C), it turns the cursor on (`FrontEnd::SetCursorOn`, CursorOn [0xCD0620]), shows the Options tab page
(`MiniDialogBoxOptions`, [g_game +0x250304] +0x60, vt+0xC) and marks itself (+0x160 = 1). Otherwise, unless the
player's page waits to restart the game ([0xCD0B04]: set where the profile editor's Back button is relabelled Restart,
0x53E6A0, cleared by 0x53E6F0 and by its restart, 0x53F299 / 0x53F2DF; `ProfileEditor::CanESCOut` 0x53BD30 refuses
Escape while it is set), it turns the cursor off, goes back to the main room (`GoToRoom(0)`) and clears the mark, so the
options open again at every later visit; setting the room up clears it too (0x78D7E8). `Temple::Update` also updates
the room seen through the main room's door (Temple +0x20, 0x794229), which for the options room runs the going back
with no effect in a single-player game. Leaving the temple hides no dialog (`GGame::LeaveInsideCitadel` 0x553A70,
fn_00793D00 and the rooms' `CloseEngine`; none of `DialogBoxBase::Hide` / `HideAll`'s callers is on the way out), so
options a script takes the player out of stay up on the land; what the land then does with them and with the cursor
left on (`GInterface::InterfaceActionProcessFirstGUIFeatures` 0x5D2B44 reads it) is not read. The tabs
(`AddOptionsTabs` 0x53F540, the list at 0x53F71C, ids 0x8FC + i): Main Menu (HELP_TEXT_DIALOG_ADDITION_88), Options,
Players, Advanced (`DialogBoxOptions`), Controls, and a sixth, Immersion (HELP_TEXT_PAUSE_STATS_150), only with an
immersion device ([0xD18588]). The tab handler fn_0053F3E0: inside the citadel the first tab reads
HELP_TEXT_ROOM_WORLD_TITLE (0x53F4A3..0x53F51E; outside 0x1A7A, Main Menu); a click on a tab (0x8FC..0x901) hides the
dialogs (`DialogBoxBase::HideAll`) and, outside the citadel only, shows the main menu (0x53F41F), then the tab's page;
the first tab has none, so inside it closes the options and the room then goes back to the main room. A pending video
change shows its notice first. openblack: `TempleInterior::UpdateOptionsAndFutureRooms` through
`temple::options_room::Update` (`src/Temple/OptionsRoomRules.h`, the restart waited for as `GameMenu::IsRestartPending`;
tests `OptionsRoom.*`), `gui::GameMenu::SetInsideTemple`
and `OptionsTabs`; no Immersion tab (no force-feedback device); tests `GameMenu.InsideTheTempleTheFirstTabClosesTheOptionsWithoutTheMainMenu`,
`GameMenu.OptionsOpenTheOptionsTabs`.

#### The future room

Read 2026-10-22 (`UniverseRoom`, the temple's room 3; bw1-decomp has no body for it).
- **Its words.** `Update` 0x79D000 clears the words' time (+0x168) while the room's camera comes in (+0xC4's +0x124 =
  0) and, with no dialog up, the camera arrived (state 1), in the room the player is in (Temple +0x1C) and outside
  the multiplayer citadel ([0xE05FE0]), marks the words started (+0x164 = 1). The mark lasts the visit to the temple:
  setting the room up clears it (0x79CE90), as does the update through the main room's door with the room's camera
  left at 1 (the room is then not the current one: cursor off, `GoToRoom(0)`, +0x160 and +0x164 cleared).
  `DrawAdditional` 0x79CED0, outside the mirrored pass and once started, adds the frame's whole milliseconds
  (`g_delta_time`, at least 1) × 0.001 to the time, takes alpha = ftol(time × 64) held to 128 into
  `SetupThing::DrawAlpha`, and while the camera has arrived draws text 0x1AA3 (HELP_TEXT_DIALOG_ADDITION_129) with
  `DrawTextWrap(0, 0, 800, 600, 0, true, …, 60, white)`.
- **Its turn.** Every picture room's camera is a `ChallengeRoomCamera` (`PictureRoomBase::InitEngine` 0x78F4A6);
  its `UpdateMain` 0x7859D0 turns the room by itself under a dialog (0x786174): s = clamp(+0x3D0, 0, 1), +0x3D0 +=
  dt, and, but in the library (+0x548), yaw +0x4FC += s × dt × 0.25 (0x8AB3D4). With no dialog it turns the same way
  when the room is the future room (rooms[3]) and its words have started (0x78600A, the library check not made);
  otherwise the arrow keys (actions 7 to 10, 0x786079) turn and lean it. +0x3D0 grows only while the room turns by
  itself and starts at 0 each time the camera comes in (0x79661B). So in a single-player game the future room turns by
  itself once its words show and its arrow keys do nothing; in the multiplayer citadel the words never start (0x79D05E).

openblack: `temple::future_room::Step` (`src/Temple/FutureRoomRules.h`, raffclar's, kept for the whole visit as the
original's mark), from `TempleInterior::UpdateOptionsAndFutureRooms` with `game_clock::FrameRealMs`; the turn is
`TempleCameraModel::PictureTurnByItself` (raffclar's); tests `FutureRoom.*`,
`TempleCamera.APictureRoomTurnsByItselfUnderADialogSpeedingUpOverASecond`, `TheLibraryKeepsStillUnderADialog`,
`TheFutureRoomTurnsByItselfWhileItShowsItsWords`. Not read: whether the room's camera, left by its way back, keeps
state 1 so that a look through the main room's door clears the words' mark; with a mark kept from an earlier coming
in the original's yaw already creeps during the walk in (`InnerCamera::Update` calls `UpdateMain` at 0x796DA8), where
openblack turns only once the player has the camera.

#### The rooms' tooltips

Read 2026-10-22. No room overrides `TempleRoom::CalculateTooltipsInsideCitadel` 0x78D9A0 (slot +0x30 of all nine room
vtables): it is the help system's "Inside Citadel" row (table 0xBF1ED8, fn_005D67D0 → fn_00794A00), and it submits the
three statics ToolTipAction [0xC2A1C0], ToolTipText [0xC2A1C4] and ToolTipAlign [0xE36140], not forced, or nothing
while a setup box is up. `Temple::Draw` sets them for the current room each frame (0x79463D..0x79466E):
`PreToolTipProcess` (vt+0x34), the room's draw (its controls' Get callbacks), then `PostToolTipProcess` 0x799F50 (none
while the camera's state +0x124 is 0 or 3, on its way in or through a door). They start at 1, 0xE77, 0 (fn_0079A720).
- The world room, Pre 0x79F440: Move 0xE7E with the four arrows; on the pool (camera +0x3F8 = 1) 0xEED with the four
  arrows when the press began on it and the zoom +0x450 is 1, else 0xE75 while not zoomed; over a door (camera +0x12C
  up to 7) **with no press held** (the room's +0x7C = 0) the table 0x79F5B0: 0xE95, 0xE99, 0xE98, 0xE9A, 0xE97, 0xE96,
  none for the scrolls' wall, 0xE94. The buttons 0xE91, 0xE8F, 0xE93, 0xE92, 0xE90 (0x79EE20..0x79F073); the scroll
  (`WorldScrollGet`) 0xEE3 or 0xEE2 with the up and down arrows when hovered, 0xE8B when looked at close up.
- The challenge room: Pre 0x781CC0 (Move 0xE7E with the four arrows, [0xDD0C00] = 0; its way back, door 7, 0xE9B
  with no press held); its pictures (fn_00782260, read 2026-10-22): none past the records; hovered with the blend
  +0x450 at 0, 0xE75 (Zoom In); hovered with the blend at 1 on the slot looked at [0xC28604] and no replay button
  hovered before it this frame ([0xDD0C00] = 0), nothing once won (success exactly 1) or 0xE9F (Replay); not hovered,
  the blend at 1 on that slot and no button hovered, 0xE8B (Zoom Out); in between, nothing. Its buttons
  (fn_00782490), shown while not won (a success neither exactly 1 nor unordered, 0x7824D5): hovered, 0xE9F, setting
  [0xDD0C00]. Then its scroll (fn_00784210). The compares test C3, so an unordered blend counts as equal. The
  callbacks run in the order of the controls' submesh numbers: `LoadOptionData` 0x798B30 finds each control's submesh
  by name ignoring case (fn_00799110, `__stricmp` 0x79913A), bubble-sorts the entries by it (0x79905F..0x799099), and
  `PreDraw` 0x799F80 calls them in turn, hovered = (the room's +0xC8 == the submesh). As openblack's; tests
  `TempleChallengeRoom.ToolTips`, `ToolTipsInTheOrderOfTheRoomsSubMeshes`.
- The creature room, Pre 0x7866C0, keeps the arrows as they were; its way back door 0; its targets and scrolls as
  openblack's.
- The library (credits, 0x78B280, door 4) and the save game room (0x7901E0, door 5): Move and the way back; the save
  game room's scroll (fn_00791F90) as the challenge room's, its zoom out held back by a picture's zoom in [0xE05FD4];
  its pictures (fn_00790C00: 0xE75, or 0xEE6 with the save's name cut to 16 as 13 + "...", from afar; close up 0xE9D
  with a save, else 0xE9C for the slots below 14; 0xE8B off it) and its buttons (fn_00791050 0xE9C, fn_00791190 0xE9D
  with the arrows kept, fn_00791280 0xE9E).
- The options and the future rooms have the empty base Pre (0x799F40): the tooltip stays as Post left it.
- The room's +0x7C is its press (`TempleRoom::UpdateMouse` 0x79A3A0): 1 for a press on a control (0x79A4D4), 2 for a
  press on none (0x79A553), 0 when it is let go (0x79A562), under a setup box (0x79A577) and at a change of room
  (fn_007946D0, 0x7946DD / 0x794777).

**Where the tooltip is drawn inside the temple**, as on the land: `HelpSystem::Draw3D` (called at 0x54E2ED after
`Temple::Draw`, the citadel's branch 0x54DE07..0x54DE55) draws the tooltip's icon (x = y = 0, align | 0x16) with
`CameraHelp::DrawKeyOrMouse` 0x447EA0, which projects the hand's own 3D object [[GInterface +0x3A0] +0x482C] +0x38
(0x447EB9..0x447F01), the object `TempleRoom::DrawHand` 0x799ED0 draws through `CHand::Draw`, with the camera the
room's draw set; nothing when the point does not project. Not the mouse, nor the room's hit point.

openblack: the input prompts' anchor is the hand entity's projected position in the temple too
(`help::input_prompt::HandAnchor`, test `InputPromptIcons.TheAnchorIsTheHandsOwnPoint`), drawn by
`Renderer::DrawKeyOrMouse`. `UpdateTempleToolTip` (`src/3D/TempleToolTips.cpp`) follows these rules; the doors and the way back wait for
no press at all (`RoomPressHeld`, kept by `TempleInterior::HoldControl`); the save game room's pictures and buttons wait
for the room's slots (saved games); tests `TempleToolTips.*`.

#### The world's statistics scroll

Read 2026-10-22 in the v1.42 bytes. `WorldRoom::MakeScrollText` 0x79F250 adds through `AddText` 0x79A910 (a switch on
the text's id that reads each figure, fills its `$I` through fn_0079C550 and starts a new line), in order: WORLD_TITLE
(a blank line after); TOTAL_POPULATION, the world's villagers not counted out [g_game +0x205A54]; the believers,
ftol(`GetProportionOfWorldPopulationWhoBelieveInMe` 0x64B680 × 100), the men and women of the player's towns over the
world's people; the men, ftol(`GetPercentageOfMalesInTheWorld` 0x64B7C0 × 100), **counted from the people in each of
the player's towns' homes (+0x754, their people +0xA0) and its homeless (+0x768), anyone not a woman counting as a
man**, none with nobody (a blank line after); the deaths and births (the game statistics +0x4C / +0x48); "%s: %i" of
PAUSE_STATS_108 and the sacrifices (+0x1124) (a blank line after); `GameStats::GetTotalBuildings` 0x564F60 (the running
homes and civic total) and `GetTotalWonders` 0x564FC0 (a blank line after), both worked out once a game turn
(`IsValid` 0x564DD0); the disciples (`GetTotalNumberOfDisciples` 0x64B890: every kind but the missionaries in the
player's towns, and the player's missionaries in the other towns); the builders, breeders, fishermen, farmers and
foresters (`GetNumberOfDisciples` 0x64BA70 with 4, 5, 3, 1 and 2); the missionaries (`GetTotalNumberOfMisionaries`
0x64BA20: in every town, the missionary controls (+0x99C) whose reaction's player is the player); the craftsmen and
traders. The protectors (0x4D4) are left off.

The drag (`SubOptionEntryScroll::UpdateMouse` 0x78E320): a press keeps the mouse's y; every frame while held (message
7 from `CMouse::ProcessButtons` 0x61A3F2) the amount is y − the last y; letting go sets it to 0. `WorldScrollGet`
takes it from the scroll's offset and `FindProperTextOffset` 0x79A6A0 keeps it from 0 to the lines × 10 − 240: a texel
a pixel, no easing.

The pictures (`TempleRoom::FormatTextureForScroll` 0x79989F..0x799DA8): a line starting with U+F8FF (a gesture) or
U+F900 (a control) is rebuilt as "$g%d" (0xC2A2D4) or "$m%d" (0xC2A2E0), n = the character − 0xF8FE, and drawn only
with the scroll's submesh given (the camera close) and the line in sight (−10 ≤ y ≤ 256); the line counter moves on
either way. In the submesh's frame (made unit, its y and z rows turned round), across = |Δz| × 237 / 65536 and down =
|Δy| × 255 / 65536; the line's corner P0 = the centre + (X − 128) × across along z + (y − 128) × down along y, X being
the x of the last line of text read (0 before any, the lines passed over too), and P1 = P0 + 10 × down; both must
project on the screen (fn_00819490 gives 2) and the texture's corners (0, 0) and (256, 256) in front. h = |ftol(the
screen's y of P1 − P0)|; `fn_005760C0` 0x5760C0 draws "$g" (0 < n < 24) as a shadow (2 pixels right and down, depth −
1, black) then the symbol (depth − 2, white), each `fn_005763E0` 0x5763E0: a square 2h across centred between the
corners' x, its picture cell n % 16 of S_Gesture⟨n / 16⟩, cut at the top and the bottom with its picture's edges moved
as far; and "$m" (when `ConvertActionToKMIcon(n)` has one) a line lower as `DrawKeyOrMouse`, 2h across, only within
the corners. The "$m" picture (fn_005760C0 0x57623C..0x5763CC, read 2026-10-22; it needs the help system,
g_game +0x25005C, and `ConvertActionToKMIcon(n)`): side = 2h, the y passed in (P0's screen y + 3h) less the side;
drawn only while that is under the scroll's top on the screen and it plus the side over its bottom (both strictly);
`DrawKeyOrMouse(anim, key, mouse, L"", ftol(side / 2 + (the corners' x) / 2), ftol(((y + side) + y) / 2 − side × 0.2),
ftol(side), align 1, white at 255, no second colour, no panels)`; the clip and fade arguments the scroll passes are its
own corners, so the fades come out at 255. Oracle (fn_005760C0 with its callees stubbed): h 17, corners (300.25,
100.5)..(700.75, 650.125), y 351.3 → (517, 327), 34; h 12, (−20.6, 40.4)..(610.2, 500.9), y 233.7 → (306, 216), 24;
y 500.8 → (306, 484); y 64.4 and 500.9 → not drawn. fn_00819490 0x819490 projects through `g_world_to_clipping`: 0
when the clip w is under the near clip [0xE839E0] (0.2 inside the temple, `InnerCamera::PreDraw` 0x79693C), else
x = (x / w + 1) × the screen's half width, y = (1 − y / w) × its half height, 2 when 0 ≤ x < the width and 0 ≤ y <
the height. The only texts with "$g" are HELP_TEXT_DYK_15 and DYK_16, which `challenge.chl` never places and the exe
never names, and no dialogue line has one, so no scroll shows a gesture's picture
([not in the original](../project/not-in-the-original.md#templemain_room)). openblack: `TempleScrollTexture::LayOut`
gives each control's line with the place across of the text line before it; `TempleScrolls::AppendFocusedText` keeps
those in sight while the camera is close (`TempleInterior::GetScrollPictures`); `FillOverlayFrame` projects them as
fn_00819490 with the frame's camera and the temple's place (`temple_scroll_pictures::ControlPicture`, `PlaceAt`) and
`help::input_prompt::ResolveAction` gives the picture; `Renderer::DrawTempleScrollIcons` draws them before the input
prompts' first draw; tests `TempleScrollPictures.*`, `TempleScrollTexture.TheLinesPassedOverGiveTheirPlaceToAControlsPicture`;
debug: the temple panel's "Scroll pictures". The near clip is the camera's pixel near clip; whether the key and mouse
draw is depth-tested inside the room's draw is not read.

openblack: `TempleScrolls::Write` (the world scroll), `player_statistics::World` (the people housed and homeless, the
missionaries by their reaction's player) and `statistics::World`; the drag `TempleScrolls::Hold` with
`TempleScrollTexture::ClampPosition`. The once-a-turn totals: inside the temple no game turn runs, so they stand as
worked out (🔍 inferred from the temple's pause). Tests `StatisticsWorld.*`.

#### The save game room

Read 2026-10-22 in the v1.42 exe (bw1-decomp has only `SaveGameRoom.h`).
- **The scroll.** `SaveGameRoom::MakeScrollText` 0x792110 writes HELP_TEXT_ROOM_SAVEGAME_TITLE (0x4C2) and a new
  line, the time played (HELP_TEXT_DIALOG_ADDITION_136 "%.2d:%.2d:%.2d": the hours t / 3600 not wrapped, (t / 60) %
  60, t % 60), DIALOG_ADDITION_119 with the profile's `numberofsaves` (+0x80C), DIALOG_ADDITION_84 with its
  `numberofloads` (+0x810), a new line, then for each saved game of the room's list ([0xE05F18], its count [0xE05F1C])
  its date (+0x104) and its name (+0) or HELP_TEXT_NONAME_SAVED_GAME_01 (0x12EE) for none, each through `AddRawText`,
  and a new line; `EndText`, and the scroll's submesh [0xE05FCC] when asked. The list is built once a visit by
  `InitEngine` 0x78FD10 from the save folder's slot folders whose `%d.sav` and `SaveGame.inf` both open (fn_00792A20:
  the slot +0x100, compared % 100, the date, the name), so with no saved games it is empty. `Draw` 0x790240 writes the
  scroll again every frame while the room's blend is over 0.5 ([0xE05FD0]), then once more as it falls under.
- **The time played** = `PlayerProfile.timestarted` (+0x808) + `time()` − `GGame +0x59AC`, whole seconds of the
  system clock as a 32-bit number. +0x59AC is set by `time()` at the end of `GGame::Init` (0x54FD15), once a run,
  after the first land is loaded; `GGame::Close` 0x54ECB8..0x54ECC7 adds the run's seconds to the profile and writes
  it back (`WriteBackToRegistry`). `GetProfileByName` reads "timestarted" (0xC01F54), "numberofsaves" (0xC01F44) and
  "numberofloads" (0xC01F34), each 0 when missing (0x66BBC9..0x66BC14), so a new profile shows 0:00:00 from the land's
  start and no saves nor loads. A save in the room adds one to the saves after `SaveAllGame` (0x792F5D..0x792F66), a
  load from the room one to the loads (0x793332..0x79333B); the autosaves, the quick save and load, the scripts' saves
  and the temple's save as it is built count nothing.
- **The controls** (`TempleRoom::LoadOptionData(savegame[lo].l3d, 0x55, …)` from the constructor 0x78F960): 21
  pictures `LH_PICTURE_savegame_01..21` (click fn_00790B30; shown always, fn_00790C00, `Data\Textures\PictureTexture.raw`
  at alpha 0xFA for a slot without a save, the save's own picture otherwise, fn_00792780, half tinted by the player's
  alignment), 21 `LH_BUTTON_SAVE00..20` (fn_00790F80; shown for slots 0..13 only, with g_game +0x205A10 at 0, [0xBEC284]
  7, the room's blend exactly 1 and the camera close, [0xE01EE8]; tooltip HELP_TEXT_TOOLTIP_42), 21 `LH_BUTTON_LOAD`
  (fn_00791100; shown for a slot with a save, likewise; TOOLTIP_43), 21 `LH_BUTTON_DELETE` (fn_00791260; the same;
  TOOLTIP_44) and `LH_Scroll_Savegame`. Slots 14 to 20 are the land's own: 14 the previous land (TOOLTIP_157), 15 to 17
  the land's autosaves (TOOLTIP_159 "Land Autosave: %3.0f"), 18 the next land (TOOLTIP_158), 19 the quick save
  (TOOLTIP_155), 20 the autosave on exit (TOOLTIP_160), each label shown while an empty slot is looked at (`Draw`
  0x790640..0x79085F, the table 0xC2864C).
- **Saving** into an empty slot goes straight on; into one with a save it asks HELP_TEXT_ROOM_CITADEL_SAVEGAME_ROOM_
  SAVEREQUEST (`ComfirmForSaveGame` 0x792BD0, `MessageBoxA` style 3), then saves (0x792CA0: `CreateSaveGameFiles`,
  `UpdateDateAndTime`, `SaveGameInfo`, `SaveAllGame`). **Loading** asks LOADREQUEST (`ComfirmForLoadGame` 0x792B70);
  loading slot 14 first saves the game into 18, and 18 into 14 (0x7936D4..0x793796). **Deleting** asks DELETEREQUEST
  (sub_792C30, its answer sub_7937B0) and on yes removes the slot's folder (sub_7934C0, through sub_78EC00:
  `FindFirstFileA`, `DeleteFileA`, `RemoveDirectoryA`), drops it from the list and writes the scroll again.
  `SaveGameRoom::InstantSaveGame` saves without asking: the quick save (slot 19, packet 0x61 or 0x62 from
  `GGame::DoAction`, outside a network game, g_game +0x205A10 at 0 and more than three turns since the last; its
  packet's case is inferred), `GScript::InstantSaveForScript` 0x6FF8E0 (SAVE_GAME_IN_SLOT, the slot popped as a float
  and truncated) and the temple built (`CitadelHeart::Built` 0x4650EF, slot 20, outside a network game).

openblack: `TempleScrolls::Write` (SaveGame) writes the scroll's lines as the original; there are no saved games yet,
so the list is empty, as the original's with an empty save folder; test
`TempleScrolls.WithoutSavedGamesTheSaveRoomWritesItsTitleTimeAndCounts`. The time played and the counts are the
profile's (`player_profiles::Statistics`, kept as a "statistics" line of the profiles' file, raffclar's), the time
`player_profiles::TimePlayed` of the seconds kept and those since the first land was loaded
(`PlayerProfileSystem::StartPlaying` / `StopPlaying` from `Game`), read on the system clock's seconds as the
original's `time()` (`machine_clock::WallTime`, the time system's `GetWallTime`: the wall clock in play, and with the
fixed step a fixed date moved on by the steps, so the checks, whose profile is never kept, read the same; see
[the system clock's seconds](../engine/game-clock.md#the-system-clocks-seconds)); tests
`PlayerProfiles.TheTimePlayedIsTheTimeKeptAndTheTimeSinceTheGameStarted`, `TheStatisticsAreKeptWithTheProfile`,
`PlayerProfileSystem.TheTimePlayedIsKeptAsTheGameEnds`. Saving, loading, deleting, their counts, the pictures and the
slots wait for the saved games (../project/progress/engine/saving_and_loading.md).

#### The map in the pool

Read 2026-10-22 (`MiniMap`, the world room's +0x154).
- **Built on each entry.** `GoInsideCitadel` → `Temple::InitEngine` 0x793C60 → `WorldRoom::InitEngine` 0x79D280 →
  `MiniMap::Init` 0x797530 (the cells zeroed, a 256² 16-bit texture, 128² in low resolution) and `BuildMapTex`
  0x797F10, its only caller (0x79D3B7); then `CreateWorldMapColours` and `CreateWorldMapSpells`. Leaving
  (`LeaveInsideCitadel` → fn_00793D00) closes every room, and `WorldRoom::CloseEngine` 0x79D6C0 deletes the map.
- **BuildMapTex** copies each land block's page (LandBlock +0x9C0, its offsets +0x9CC / +0x9D0 in 256ths) texel for
  texel, nearest, every 8th texel, transposed, 8 × 8 texels a block (4 × 4 in low resolution); no block or no page
  bits gives 0. It frames the map: the offset = the blocks' min + max + 1, the scale = 11 [0x92B6A8] / √(dx² + dz²).
- **DrawMap** 0x7977A0, every frame the world room is drawn (from `WorldRoom::Draw` 0x79E8BA): 65 × 65 vertices at
  the cells (x × 8, z × 8); y = altitude × 0.67 × scale / 80; uv = (x, z) / 64; the grey (255 × brightness) >> 8 (the
  white map colours × the brightness, see the fog above); alpha 0, 0x55, 0xAA for altitudes 0 to 2 (table 0x99EE50)
  and 0xFF above; where a block is, the triangles (x, z)(x + 1, z + 1)(x + 1, z) then (x + 1, z + 1)(x, z)(x, z + 1),
  culled CCW (the material's two-sided bit cleared at `Init`). The per-cell map colours are never set:
  `DrawWorldMapColours` 0x79DCC0 is a bare `ret` and `CreateWorldMapColours` 0x79DCD0 only clears them.
  `MiniMap::CalcPoint` 0x797590 interpolates the four vertex altitudes.
- **The markers** (`WorldRoom::DrawAdditional`). Pass 0 draws the glows with no depth test: 0.65 (doubled by
  `DrawGlowAtPoint` 0x83D860 to 1.3), the colour bytes 0x61, 0x6E, 0x7C in `LH3DColor`'s blue, green, red order, so
  red 0x7C, green 0x6E, blue 0x61 (0x79D8EC..0x79D8F6, the same in `DrawCreature` 0x79DB60 and `DrawChallenges`
  0x79E0B3), a tenth above the marker. Pass 1 draws the spells' seeds (`DrawWorldMapSpells` 0x79DF90: scale 0.1, half
  a unit up, made by `CreateWorldMapSpells` 0x79DF50 on entry), then every player's temple (`DrawCitadel` 0x79D830) and
  every creature (`DrawCreature` 0x79DAB0) at its cell × 10, then the challenges. `MiniMap::DrawMarker` 0x79DA40:
  scale 0.05, turned by `MarkerAngle`, which grows by the frame's ms × 0.001 each drawn frame, then
  `ApplyCitadelColoring` 0x7867B0 (each channel × the temple's light, the specular the temple's colour), set on the
  marker's object with `SetColorSpecular` and drawn through `AddDrawing` (WorldRoom.cpp:331), the object draw every
  model of the room takes, so the marker is shaded by the model light over its own normals. openblack (2026-10-27)
  draws them shaded as the room's other models (`Renderer::DrawTempleMapMarkers`); before, they were drawn unshaded, in
  one flat colour, so their shape did not show. The colour is
  `GetPlayer3DColor` 0x64B590 → `GetPlayerColour` 0x64D800 = the player colours [0xBFF0B8] at `GetRemapedPlayer`
  0x64D790's place, which by the land's number gives lands 1, 4 and 5's second player the seventh's colour, land 2's
  second and third the fifth's and sixth's, and land 3's second the sixth's; black becomes white, then each channel
  goes a quarter of the way to white (c + (255 − c) / 4, rounded down); a creature with no player is white.
- **A double click on the map** (`WorldRoomCamera::UpdateMain` 0x7A064D..0x7A07CE) is taken when the point is within
  10 of the middle or the land there is above 1; it sets the way out and the fade, and starts the force-feedback
  effect COMMAND_SUCCESS (`StartImmersion(44, 0x80000000)`, 0x7A07BF), which does nothing without the force-feedback
  mouse; no hand animation or sound.

openblack: `TempleMap` frames the map at `TempleInterior::Activate` (each visit) and builds the relief each frame the
main room is drawn; `Renderer::DrawTempleMapPass` draws the land from above into a 256² target once a visit, shown from
the next frame, every block with its own texture (texture level 0 whatever the world's pass left, so the far blocks'
tiles don't reach it). The original makes the map from each block's low-resolution tile instead (`BuildMapTex`,
above), which openblack doesn't do yet (Pending); the markers come from
`TempleInterior::UpdateMapMarkers` and `Renderer::DrawTempleMapMarkers`, in `TempleMap::MarkerColour` (the land's
players' colours, `gui::statistics_box::PlayerColour`), their glows red 0x7C, green 0x6E, blue 0x61; tests `TempleMap.*`.

#### The main room's doors

Read 2026-10-22 in the v1.42 bytes (the decompilation has no C for them). The temple keeps one doorway at a time: its
leaf [0xC2A18C] (−1 for none), its swing `Temple::DoorPosition` [0xE0602C], from 0 to 2, and a rate a second
[0xE06030]. Their only writers are the four functions below, and nothing resets them, so they carry over from one visit
to the next (the temple is made once a game, in `GGame::Init`).
- `Temple::Update` (0x793EE0) first reacts to the active room's camera as it stood at the start of the frame
  (0x793F66..0x79404B): walking through a door (state 3) at its stage 1, Open(the leaf of the room ahead, −0.5, 0.4)
  from the main room, or Open(the leaf of this room, 0, 2.0) from another; at stage 0, FastCloseDoor. The stages
  move on in `InnerCamera::UpdateMain`, 0 to 1 past 1.5 s (0x795D4C) and 1 to 2 past 5.0 s (0x795E21).
- Then the room updates (vt+0x14, 0x79420E): its camera calls Close(−1, 1.6) once its way in has run 0.4 s
  (0x796222, `InnerCamera::UpdateMain`'s state 0), and FastCloseDoor from `CalcDoorHit` 0x796A60 while in state 1.
- Then the doorway steps (fn_00794D30, from 0x794251) by `g_delta_time` × 0.001: with no leaf the swing is 0; below 1
  it moves on by the rate, plays G_CitadelDoorOpen_01 (0x794DAE) as it crosses 0, and holds at 1; past 1 it moves on
  and past 2 it forgets the leaf. `InnerRoom::SetDoorMatrices` 0x794E30 then turns the leaf's joint by (1 −
  cos(swing × π)) × 0.6981317 (the double at 0x99EE60) while 0 < swing < 2, and the next joint the other way; every
  other joint of the 32 is left straight.
- Open (fn_00794F30): the rate first; then, when past 1 or for another leaf, the swing = from, and the leaf taken if
  it is one. Close (fn_00794FB0): with a doorway open no further than 1, G_CitadelDoorClose_02, the rate, and the swing
  turned round to 2 − swing, plus 0.01 if that is still no more than 1. FastCloseDoor 0x794F70: with a doorway, the
  close sound, the swing 2, the rate 2. `Temple::GoToRoom` calls FastCloseDoor (0x793EBC) only past its first returns
  (the room already the active one, or not open).
- The leaves (0xC2A154, by room): the creature room 5, the challenge room 7, the multiplayer room 13, the options 3,
  the save game room 9, the credits 11; the main room −1. The way out is room 7, one past the table: it reads the
  bytes after it ([0xC2A170], the text "C:\dev\MP\Black\Temple.cpp"), a leaf of 0x645C3A43, past the 32 joints. So
  walking out the doorway swings from −0.5 at 0.4 a second and plays its open sound, but no leaf turns; it is left
  about 0.9 open, and the next visit's way in turns it round to close, with its creak.

openblack: `TempleDoors` (`src/3D/TempleDoors.cpp`) steps the doorway; `TempleCameraModel::FollowTemple` makes the
calls; the way out's doorway is `TempleDoors::k_ExitLeaf`, one past the joints; the doorway is kept from one visit
to the next (`TempleInterior` makes it once); going straight to a room shuts it only for another room than the one the
player is in (`TempleCameraModel::ChangesRoom`); tests `TempleDoors.*`, `TempleCamera.GoingToTheRoomThePlayerIsInDoesNothing`.
⚠️ approximate: openblack's doorway reacts to the camera's state of the same frame (`FollowTemple` runs after the
camera's step), the original's to the state at the frame's start, so the doorway's calls (and their sounds) can come
one frame early; the fix belongs to the temple camera's frame order (../camera/temple_camera.md). The debug
window's Temple page shows the doorway's leaf and swing.

#### The signs over the doors

The main room labels its seven "SIGN …" submeshes with the rooms' titles (`WorldRoom::InitEngine` names them):
`TempleRoom::DrawNameScrolls` 0x798430, called from `WorldRoom::Draw` 0x79E5CC with size 1 and stretch 1, the creature
room's with (4 signs, none highlighted, size 1.3) and the library's (credits) with (7, none, no colours, 0.8).
- **The library's signs ✅ faithful** (read 2026-10-22). `CreditsRoom::InitEngine` 0x78CA9E names its seven
  (`InitNameScrolls` 0x798350, each submesh found by name ignoring case, `__stricmp` 0x7983E5, −1 when missing): SIGN
  CONTROL, SIGN CREATURE, SIGN CREDITS, SIGN DIDYOUKNOW, SIGN HISTORY, SIGN MIRACLES, SIGN VILLAGE. Its first draw
  reads their texts in that order: TEMPLE_SCROLLS_02, 03, 01, 06, 07, 05, 04 (0x1A16, 0x1A17, 0x1A15, 0x1A1A, 0x1A1B,
  0x1A19, 0x1A18). `CreditsRoom::Draw` 0x78B3CE draws them with (7, 0x3FC, none highlighted, no colours → (200, 200,
  50), size 0.8, stretch 1), with [0xE36120] = 1 around the call so the text runs along the sign's x axis (as the
  creature room's); the main room leaves it 0. So the library's letters are 0.8 × 2.1 high, stretched 1 × 0.8.
- Each label is centred on its sign's box, in the sign's own frame (the submesh's matrix made unit, its second axis
  turned round, its origin at the box's middle).
- The letters are size × 2.1 high and stretched × 0.8 along the text (0x8C4A04 = 0.8): x = −½ × the text's width at
  that size × stretch × 0.8, y = −½ × the size.
- A black shadow 0.21 × size lower at depth −0.4, then the label at −0.6, opaque, in its room's colour (the creature
  room (236, 198, 100), options (229, 212, 65), exit (222, 227, 239), multiplayer (111, 146, 235), credits
  (212, 107, 251), save (156, 209, 64), challenge (235, 146, 111)), or (200, 200, 50) with no colours.
- The highlighted label is drawn in a grey that pulses with the real clock: ftol(sin((ms & 1023) × 2π / 1024) × 24 +
  230), the same at the game's single precision as openblack's float sum for all 1024 values.
- The highlighted sign is the main room camera's `HighlightedDoor` (door 7, the challenge room's, lights sign 6), which
  `WorldRoomCamera::UpdateMain` sets every frame from `CalcDoorHit(room, cursor, 45, true)` (0x79FE21) before the
  camera's state switch, so in every state of that camera, and which keeps its value while another room is active;
  `InnerCamera::Init` sets −1 (0x797196).

openblack: `TempleSigns` (`src/3D/TempleSigns.cpp`, `k_LabelSize`, `k_LabelStretch`, `HighlightGrey`,
`MainRoomSignOfDoor`, the rooms' layouts `LayoutOf`; each sign found by its submesh's name ignoring case, as the
original's); the lit door is `TempleCameraModel::GetHighlightedDoor`, `NextHighlightedDoor` every frame from
the door under the cursor while the main room's camera runs, kept in the other rooms, none on coming in (🔍 inferred:
that `InnerCamera::Init` runs at each entry, as `Temple::InitEngine` does); tests `TempleSigns.*`,
`TempleCamera.TheMainRoomLightsTheDoorUnderTheCursorInEveryState`; the library's layout,
`TempleSigns.TheLibraryLabelsItsSevenScrollsAsTheOriginalLaysThemOut`.

#### The library's scrolls

Read 2026-10-22 in the v1.42 bytes (bw1-decomp has no body for `CreditsRoom`).
- **The credits ✅ faithful.** The first scroll is written by fn_0078BDA0 (0x78BDA0..0x78C040; callers
  `CreditsRoom::InitEngine` 0x78CA65 with 0, `CreditsRoom::Draw` 0x78B41A with 1 when the camera is close to scroll 1
  and 0x78B4BD with 0 when it leaves): `ClearText`, 36 names hard-coded in the exe as UTF-16 (0xC279DC..0xC28038), each
  through `AddRawText` 0x79A8D0 (dropped once the text would pass 7000 characters, else line-wrapped, then a new line),
  `EndText`, then `FormatTextureForScroll` with the submesh "LH_SCROLL_LIB01" when the argument is 1. No title, no text
  id. openblack: `k_Staff` in `src/3D/TempleScrolls.cpp`, the same names in the same order; test
  `TempleScrolls.TheCreditsListThePeopleWhoMadeTheGameInTheOriginalsOrder`.
- **The history ✅ faithful.** `CreditsRoom::MakeHistoryText` 0x78C450: `ClearText`, `AddText(0x195F
  LIBRARY_ROOM_07)`, `AddNewLine`, then for i = 0..4 the dialogue's history line start + i (fn_005C5F50: none unless
  0 ≤ n < the count +0x45C8, else entry[(head +0x45C4 − n + 0x3FF) & 0x3FF] of a ring of 1024, n = 0 the newest);
  none goes to the next i; when start + i > 0, `AddRawText` of DIALOG_ADDITION_06 0x1A28 with start + i; then
  `AddRawText` of the narrator's caption (+0xC; the table 0x78C7A4: 2 → ADDITION_131, 3 → 128, 4 → 18, 5 → 17,
  6 → 133, 7 → 79, 8 → 82, 9 → 95, 10 → 16, 11 → 15, 12 → 134, any other → 135), `AddNewLine`, `AddText` of the
  line's text (+0), `AddNewLine` twice; `EndText`, the submesh "LH_SCROLL_LIB07". The buttons (`HistoryBox::Init`
  0x545240): Previous Line 0x1A10 at (60, 500), Latest 0x1A73 at (320, 500), 160 wide, Next Line 0x1A0F at (700, 500);
  their handler sub_5453D0: Latest sets the start to 0, Previous adds one, Next takes one, then the start is held to the
  count − 1, then to 0. `CreditsRoom::Draw` 0x78B485 holds it the same way every frame while the camera is closer than
  0.5 to scroll 7, shows the box and rewrites the scroll; further off it rewrites all seven and hides the box;
  `TriggerIntroCamera` 0x78B254 and `InitEngine` 0x78C7F4 set the start to 0. openblack: `TempleScrolls::Write`
  (LibraryHistory), `help::StoryCaption`, `DialogueText::GetHistory`, `TempleScrolls::TurnHistory`, `ClampStoryStart`,
  `gui::StoryHistoryBox`; tests `TempleScrolls.TheStoryHistoryShowsFiveLinesFromItsFirstLine`, `StoryHistory.*`.
- **The numbered texts** (`TempleRoom::AddText` 0x79A910, read 2026-10-22). Text 0 writes nothing at all, not even a
  new line (`jbe` at 0x79A926 to the exit); a number at or past the text count takes text 0's (HELP_TEXT_NONE,
  0x79C2C7); any other is copied, cut up (`SeeIfAnyCuttingUpOfTheTextNeedsDoing` 0x79C770) and ends its line; past
  7000 characters nothing is written. Its own cases are 0x481..0x4D7 (the figures), 0x108B (the profile's name) and
  0x19CD (the sacrifices). The library's five help scrolls (title and tips), the challenge scroll (0x480..0x482 and
  each record's title) and the history's lines go through it; the creature room's miracle scroll skips a miracle whose
  text is 0 altogether (0x789227) and adds its percent through `AddRawText`. openblack: `helpText` and `addHelp` in
  `TempleScrolls::Write`; test `TempleScrolls.TheTextNumberedZeroWritesNothingAndOnePastTheTextsIsTheFirstText`.
  Whether a history line can be one of AddText's own cases (a dialogue line numbered 0x481..0x4D7, 0x108B or
  0x19CD) is not read.
- **A control's words** (`SeeIfAnyCuttingUpOfTheTextNeedsDoing` 0x79C770, 0x79C833..0x79C9E5, read 2026-10-22). A
  "$m<n>" ("$", "/" or "\" then m or M) cuts the text at the "$" and reads n with `_wtoi`; for 0 ≤ n < 33 the text goes
  on there, over what followed, with "(" (0xC2A350), the key's words (fn_0046F260 on the control map g_game +0x250300,
  row n of 0x30C bytes, its key at +0x100: `ControlKey::GetDisplayText` "%s", or "%s + %s" with its modifier first),
  " " (0x9CEE0C) when there are both, the mouse's name (fn_0046F2B0, `ControlMouse::GetDisplayText` 0x46F180 "     (%s)"
  stripped of its spaces and brackets; fn_0046F050's table 0x46F150: 1 LEFTMOUSEBUTTON, 2 MIDDLE, 3 WHEEL_UP,
  4 WHEEL_DOWN, 5 RIGHT), then ")" (0xC2A34C); the scan goes on over the changed text up to its first length, and only
  then are the lines broken. The keys' names are Windows' `GetKeyNameText`, as the Controls page shows them. Of the
  navigation tips (category 0: DYK_10, 18, 22, 44, 46, 67, 74, 88, 89, 91, 93, 108, from `challenge.chl`), DYK_18,
  22, 44, 46 and 67 name a control; DYK_20 (creature) and DYK_31 (miscellaneous) do too, and so do lines of the story
  (the citadel guide's), which the history writes the same way. openblack: `TempleScrollText::ControlWords` /
  `SetControlWords` (raffclar's), the words from `gui::control_bindings` (the default bindings, as the Controls page
  shows them; openblack does not rebind yet); tests `TempleScrollText.AControlEndsTheTextWithItsKeyAndMouseInput`,
  `AControlOutsideTheGamesControlsHasNoWords`, `AControlsWordsAreItsKeyAndMouseInputInBrackets`,
  `TempleScrolls.TheLibrarysNavigationTipNamesItsControlsButton`. What the exe names for the zoom's two buttons (the
  Controls page's "both", ZOOMON) is not read; openblack writes no mouse there, as its Controls page.
- **What is kept.** `DYKSave` 0x78CE70 and `DYKLoad` 0x78D150 have one caller each, `GGame::Save` 0x5547ED and
  `GGame::Load` 0x55503A, reached only through the whole game's save (`GameOSFile::SaveAllGame` 0x558160) and load
  (`GameOSFile::LoadInstance` 0x55C310); `CreditsRoom::Clear` 0x78D420 runs only from its constructor. So the lists of
  tips read are kept only in a saved game.
  - **The load** (`DYKLoad`, read 2026-10-26), category 0 to 4 in turn (`AddDidYouKnow` 0x78CD20's jump table: 0 the
    list at 0xDF9D1C with its count at 0xE01E6C, then 0xDF5C58 / 0xE01E70, 0xDF1B94 / 0xE01E74, 0xDEDAD0 / 0xE01E78,
    0xDE9A0C / 0xE01E7C): a count into a local, that many texts over the list's start, then a word straight into the
    list's count. Nothing clears the list first and the count is not bound to 48, so a count past the texts keeps
    what the list held there. A failed read stops the rest (`ReadEnabled`). `DYKSave` writes the count, the texts and
    the same count again, so in a save the game writes both counts agree. `AddDidYouKnow` refuses a text when the
    count is 48 or more, else when the list holds it, else appends it.
  - **openblack:** `saved_game_load::StateOf` takes the five lists and counts from the game object's tail
    (`GameTail::didYouKnow` / `didYouKnowWords`), and `Apply` gives them to `did_you_know_read::Load`, after the snow
    lying (the order in `GGame::Load`: the snow, then the hand and buttons, then these lists); tests
    `SavedGameLoad.TheDidYouKnowTextsReadComeFromTheGameTailCategoryByCategory`,
    `ScriptHighlight.ASavedGamesDidYouKnowListIsWrittenOverTheListAndHoldsItsSavedCount`. The save half waits for
    openblack writing saved games.

#### The challenge list

The challenge room's pictures, frames and buttons, the world room's challenge markers and the challenge scroll all read one list that only three script functions fill: SNAPSHOT (057), UPDATE_SNAPSHOT (238) and UPDATE_SNAPSHOT_PICTURE (351). Read in the v1.42 exe; bw1-decomp's `ScriptFunctions.cpp` has the same three bodies.

**The pops ✅ faithful.** Every value is a raw 32-bit `ScriptDLL::POP` (0x6F6BC0), its type kept. Top of the stack
first:

| Function | Pops, in order |
|---|---|
| SNAPSHOT `GScript::SnapShot` 0x6F97B0 | challenge, count n, n values with their types (the first popped kept first), the reminder script (a string offset, `STRING` 0x6F6BD0), title text, alignment, success, focus z, y, x, position z, y, x, quest (data +0x2C) |
| UPDATE_SNAPSHOT `GScript::UpdateSnapShot` 0x6F9640 | challenge, count n, n values, reminder script, title, alignment, success |
| UPDATE_SNAPSHOT_PICTURE `GScript::UpdateSnapShotDetails` 0x6F94A0 | challenge, take picture, title, alignment, success, focus z, y, x, position z, y, x (11, its fixed count) |

- The alignment is clamped to -1..1 and the success to 0..1, each with one `fcomp` against the low bound then one
  against 1 (0x6F9861..0x6F98DF): a NaN takes the low bound (C0 is set for unordered), so NaN alignment = -1, NaN
  success = 0.
- n above 12 (SNAPSHOT, `jbe` 0x6F97EF) or 12 and above (UPDATE_SNAPSHOT, `jb` 0x6F9688) prints "Paul - number of
  stack objects is too big." and still pops all n into a 12-slot array (an overflow in the original). The shipped
  `challenge.chl` passes n = 0, 1 or 2 at every one of its 247 SNAPSHOT and UPDATE_SNAPSHOT calls.
- SNAPSHOT passes take picture = 1, UPDATE_SNAPSHOT 0, UPDATE_SNAPSHOT_PICTURE what it popped
  (`Temple::UpdateChallenge` 0x794970 forwards to the room). UPDATE_SNAPSHOT_PICTURE passes no reminder script (NULL)
  and n = 0. Fields a function does not pop are left as the stack held them: UPDATE_SNAPSHOT's camera and quest, and
  UPDATE_SNAPSHOT_PICTURE's quest.

**The list ✅ faithful.** A singly linked list, head [0xDD0BD8], count [0xDD0BDC], of 0xD8-byte records
(`TempleChallenge`). Record: +0 challenge (u16), +2 reminder script (66 chars, `strcpy`), +0x44 title, +0x48
success, +0x4C alignment, +0x50 position, +0x5C focus, +0x68 turn it started, +0x6C turn it was done, +0x70 n,
+0x74 the values, +0xA4 their types, +0xD4 quest. No limit on the count (the room shows the first 21).

`ChallengeRoom::UpdateChallenge` 0x784780, in this order:
1. Look for the record whose u16 challenge equals the given one's low 16 bits (0x7847AB), from the head.
2. **None:** the picture of slot = count is taken (fn_00784690, 0x7847BE, whatever take picture says); a record is
   made (fn_00781230): challenge, then the common write below, +0x68 = the game turn [g_game+0x205A40], +0xD4 = 1, at
   once replaced by the given quest (0x784925); it is appended at the tail and the count goes up. The alignment is not
   touched.
3. **Found (slot k):** the old values of type 4 (object) lose their script reference (fn_00784750 →
   `GScript::DecrementScriptReference`); the given alignment, clamped again, goes to
   `GAlignment::ScriptUpdate(alignment, the local player [g_game+0x205A59], challenge)` (0x78488C); if the record's
   success is already ≥ 1 (`test ah,1` 0x7848A0; NaN is not), the new success is forced to 1. With take picture: the
   picture of slot k is taken and the common write gets the given camera; without, it keeps the record's position and
   focus (`GetPosition` 0x7813B0, fn_007813D0). The quest is not touched.
4. The common write (fn_007812A0): the reminder script (NULL → empty), title, success, alignment, position, focus, n,
   the values and types; each value of type 4 takes a script reference (fn_00784720 →
   `GScript::IncrementScriptReference`); if the success is exactly 1 (fn_00781440, `fcomp qword 1.0`), +0x6C = this
   turn, so a done record's turn moves on with each later snapshot.

So an UPDATE_SNAPSHOT_PICTURE of a known challenge empties its reminder script and its values.

**The pictures** (read 2026-10-22; bw1-decomp has only `ScriptedScreenShot.h`). `fn_00784690(k)` builds a request:
the low-detail file `screenshots_lores_<k >> 3>_map_0.raw` (0xC25104), the high-detail one
`screenshots_hires_<k >> 1>_map_0.raw` (0xC250C4), under `PathCreator::GetCurrentGamePath`, with k & 1 and k & 7. If k
is the slot looked at [0xC28604] it makes that texture again first (`fn_00784640`, 0x7846FD). Then `fn_00828E90`
copies the request to 0xEA9F70 and runs `ScriptedScreenShot::Process` 0x821750 at once (0x828EE7), inside the script's
native. Process locks the front buffer (`LHScreen.PPrimarySurface` 0xE85080, flags 0x811), so the picture is of the
frame on the screen when the script runs, and two snapshots of one turn read the same frame. Row r (0..127) and column
c (0..255) read the screen at `ftol(r × H × 0.0078125)` and `ftol(c × W × 0.00390625)`, W × H the whole mode, each pixel
the mean with the one picked before it in its row (the first with itself): at 32 bits (a + b) >> 1; at 16 bits (565
when the green mask is 0x7E0, else 555) each channel's top four bits, (a4 + b4) << 3. The 256 × 128 picture goes into
half k & 1 of the high-detail file's image (read first, `fn_0082AB40`) and is softened in place over rows 0..126 and
columns 0..254: L(L(p00, p01), L(p10, p11)), L(a, b) = (128 a + 127 b) / 255 (`fn_00821D50`; oracle: (255, 0) → 128,
(0, 255) → 127, (200, 100) → 150), the last row and column as picked. The 128 × 64 low-detail one takes rows 0, 2 …
126, each pixel (left + right) >> 1, into its eighth of the low-detail image (row ((k & 7) >> 1) × 64, column (k & 1)
× 128). Both images are written as 256 × 256 16-bit surfaces (`fn_0082A8E0`): 1555 with the top bit set, or 565 with
green's lowest bit cleared ([0xEDD46C]), five bits a colour either way. In a window the front buffer is the whole
desktop **🔍 inferred**.

**Their textures.** Entering the room (`OnEnterRoom` 0x784F00) makes the three low-detail textures from `lores_0..2`
(`fn_00784070`, materials of mode 3, byte +4 = 0xFA, [0xE01EC8]), drops the high-detail one and sets [0xC28604] = −1;
a picture's click (`fn_007821F0`) makes the one high-detail texture from `hires_<k >> 1>` (`fn_00784640`). So the
low-detail textures are read again only at the next entry. `fn_00784570`, called only by the pictures' draw (k ≤ 20),
gives the high-detail material with uv (0, (k & 1) / 2)..(1, + 1/2) for the slot looked at, else low-detail [k >> 3]
with ((k & 1) / 2, ((k & 7) >> 1) / 4)..(+ 1/2, + 1/4). `CopyChallengeFiles` 0x784A30 copies `hires_0..10` and
`lores_0..2`, exactly the 21 slots' files: records past the 21st write files nothing draws or copies.

openblack: `challenge_pictures::Take` / `Shrink` (the 32-bit path, exact; the 4:3 middle of other screen shapes is
raffclar's presentation deviation), `TakeRequests` / `Collect` (pictures asked together share one frame), the frame
already on the screen as the script runs, as the original's (`graphics::ShownFrameTargets`, `ShownFrames`: while a
running task's script can take a picture, the frame is drawn aside, copied to the window, and read back only when a
snapshot asks), the textures
by `TempleChallengePictures` in the game's 16-bit form, five bits a colour (`challenge_pictures::Texels`, the 1555 form
the land's textures use; the device-dependent 565 form is not followed), a slot's low-detail texture written again
only as the player comes into the room (`Reread`, `temple_challenge_pictures::StepsFor`); tests `ChallengePictures.*`,
`TempleChallengePictures.*`. One difference is left: openblack's picture is of the next frame drawn after the script's turn, the original's of the frame already on the screen as the script runs ([Pending](#pending)). What a slot new in the stay shows in the original until the next coming in (the low-detail
file's earlier contents) cannot be told; openblack writes it at once.

`GAlignment::ScriptUpdate` 0x414360: change = `GetUpdatedChangeThisTurn`(alignment × GPlayerInfo +0x14, the script scale) (`ScaleChange` in openblack: ×(1 − |A|/2) the way the player leans, ×(1 + |A|/2) the other way), the alignment history gets it (`CAlignmentHistory::Add` 0x415110), and `CrudeUpdate` 0x4146B0 adds it to the alignment at once, clamped to -1..1, not through the turn's pending change. At the game's 24-bit x87 precision every step rounds to float (oracle, 9 cases in `test_challenge_records_original`).

**Who reads it.** The room's draw callbacks (fn_00782260, fn_00782490, fn_00782530, fn_00782560, fn_007825D0) and its
choice (fn_00782410) walk to slot k; `ChallengeRoom::MakeScrollText` 0x784159; `WorldRoom::DrawChallenges` 0x79E03B
through `GetNumOfChallenges` 0x784F30 and `GetChallengeAtPosition` 0x784F40; sub_7849A0 counts the records whose
success is exactly 1 and sub_7849D0 gives slot k's position (both through the temple, fn_007949B0 and sub_7949C0);
`ChallengeRoom::StartScript(challenge)` 0x784C10, from `ScriptHighlight::InterfaceTap` 0x70ADAD through
`Temple::StartScript` 0x794A20, runs a record's reminder with `TempleChallenge::StartScript(0)` 0x781460 (its values
pushed from the last to the first, then the script by name). The save and load (`ChallengeSave` 0x784C70,
`ChallengeLoad` 0x784D10) keep it, and `ChallengeClear` 0x784DE0 (from `GGame::ClearMap` 0x552DD2 and
`GScript::CleanGameForScriptReboot` 0x6EBC00) lets go of every record's references and frees them.

**Which lands call them.** All 260 calls are in `Scripts\Quests\challenge.chl` (137 SNAPSHOT, 110 UPDATE_SNAPSHOT, 13
UPDATE_SNAPSHOT_PICTURE, for 49 challenges; every challenge an update names also has a SNAPSHOT);
Land 1's `Introduction` takes challenge 12's snapshot after its last camera arrival (instruction 88064, n = 0). None
runs inside the 900-frame Land 1 check or the cycle: openblack logs the first call of every native not written yet,
and the reference runs (`obs2_fixed.log`, `cyc900_obs2.log`) and the hand demos log none.

### The creature room's waterfall and effects

Read 2026-10-22 in the v1.42 bytes (bw1-decomp has no body for `CreatureRoom`). The game thread's x87 runs at single
precision here, so each step below is a float step and `__ftol` truncates.
- **The waterfall ✅ faithful.** `CreatureRoom::Draw` 0x786840 slides the waterfall object [room +0x474] on each frame
  the room is drawn (0x78688C..0x7868D1): the static [0xDE0F70] (its only writer 0x7868AF, never reset, so kept from
  one visit to the next) += `g_delta_time` (the frame's whole milliseconds, at least 1) × [0x99EB78] = 2.1e-5, minus
  its truncation; then `SetAnimatedUV_1(0, s × −10)` ([0x8C7670]) before the object is drawn. openblack:
  `temple_waterfall::Advance` / `Offset` (`src/3D/TempleWaterfall.h`), stepped in `TempleInterior::UpdateCreatureCave`
  on the frames the room is drawn by `game_clock::FrameRealMs`; tests `TempleWaterfall.*`; debug: the Creature room
  header's "Waterfall slide".
- **The effects ✅ faithful in their draws.** They are not particle effects but the room's own objects, made by
  `CreatureRoom::InitEngine` 0x7878E0 (run by the room's first update or draw of a visit, the set-up flag +0xE0
  cleared by `CloseEngine` 0x798B03) and stepped by `CreatureRoom::Draw`; every random number comes from the C
  library's `Random` 0x81D180 (CRT) or `GRand::LocalFloatRand` 0x6DE590 (local), drawn in this order:
  - 4 flames (`LH3DSprite::Create(4, 1)`, 0x787999..0x787AA2): CRT `Random(−1, 1)` to the depth, `(1, 2)` to the
    height, `(−1, 1)` across, then the size max((`Random(0.8, 1.2)` + 0.5 i) + 2, 1e-4) ([0x8BF518]); colour
    0x80FF8040, render mode 13; the frame 0x1F − (((GetTickCount() >> 5) + 8 i) & 0x1F) each frame (0x7889A5).
  - 16 sprays (`LH3DSmoke` at +0x480): each smoke's constructor 0x7F8B60 gives its 10 particles age ftol(90 i), height
    0.5 i, the angle CRT `Random(0, π)` and the spin ftol(`Random(1, 100)`) & 1; then local 1, 18, 1 for a first place
    ((x + 115) − 0.5, −25 − y, (z − 123) − 0.5) ([0x99EBC0] 115, [0x99EBC4] −25, [0x99EBC8] 123), the first draw to the
    depth, and local 1, 1, 1 for the drift (x − 0.5, y, z − 0.5), the first draw to the depth; no wind (+0x108), rising
    (+0x109), size 4.
  - 2 fire smokes (+0x4C0): the constructor's 20 CRT draws, colour 0x404040, the place fire + (CRT `Random(−1, 1)`
    across, then up, then in depth), (fire.y + the draw) + 2 high; no wind, not rising, size 2.
  - 4 mists (`LH3DObject::Create(7)`, +0x4C8): the constructor's CRT frame counter, then local 1, 8, 1 for
    ((x + 160) − 0.5, −45 − y, (z − 30) − 0.5), the first draw across; +0x44 = (i + 10) / 2, which the mist's draw does
    not read (fn_007FA300 and fn_0080DB30 scale only type 4 by it); colour `ApplyCitadelColoring(0x204080C0)`
    (0x788234..0x78826C): each byte, the alpha too, times the citadel colour over 256, so 0x1F3F7FBF in full light.
  - Each frame drawn (`g_game_time_inc` set to `g_delta_time` at 0x786868, put back at 0x7870DF): each spray's place
    ((x + 160) − 2.5, −30 − y, (z − 16) − 0.5) from local 1, 23, 5, the first draw to the depth (0x786908..0x786984),
    and its step; then the fire smokes' steps; then the mists, drawn at once (vt+0x108). The smoke's step
    fn_007F8E00: no wind gives no pull, rising pulls the height down by 3 ([0x8C2C50], 0x7F8F17); a particle past
    its 900 starts over with the velocity CRT `Random(−4, 4)` in depth, `(3, 4)` up, `(−4, 4)` across
    (0x7F9086..0x7F90C9).
  - A mist's draw (fn_007FA300 0x7FA6A4..0x7FA75F) lights it by the land at its point: the cave's points lie off the
    map (z × 0.1 < 0), so it takes the default light (table[255], [0xEDDD08]) and the specular 0xFF000000 (0x8020F8),
    then the haze. Whether openblack's haze inside the temple is the island fog `GetFogValue` reads there is not read.
  openblack: `CreatureCaveEffects` (`FlameOf`, `FireSmokePlace`, `SprayFirstPlace`, `SprayDrift`, `SprayPlace`,
  `MistPlace`, `MistColour`, the streams `GameDraws`), the mists drawn by `Renderer::DrawMist` through
  `land_light::At` at the dome's point; tests `CreatureCaveEffects.*`; debug: the Creature room header's mist colours.
  The smokes' and mists' ages move on by the whole steps of the frame's real whole milliseconds, the fraction
  dropped each frame (`frame_anim::SmokeAgeStep`, `MistAdvance`), as the original truncates each frame's step
  (0x7F8FE0, 0x7FA608).

### The depth fog and the temple's light

- **The temple's light ✅ read.** `Temple::Draw` 0x794370 saves the engine's one model light [0xEA9E90]
  (0x794373..0x79439B), sets it with `SetSun` 0x81E1F0 to the literal (−1000, 1000, −500) (0xC47A0000, 0x447A0000,
  0xC3FA0000 at 0x79439F..0x7943B7) and puts the saved one back with `SetSun` at its end (0x7946C3). `SetSun` stores
  the point as it is (0x81E1F6..0x81E20D), so it is a point of the space the rooms are drawn in. Everything the
  temple draws in between is lit from there: its rooms, what is in them, and the hand, which `TempleRoom::DrawHand`
  0x799ED0 draws inside the room's draw, once a frame (the flag [0xE3611C], set by `Temple::Draw` at 0x794621 and
  cleared once the hand is drawn at 0x799F2A). The creature's room moves the light to its fire for the copy and puts
  it back after ([the copy's colour](../creature/tattoos.md#how-the-copy-is-posed-and-drawn)). **🔍 inferred**: that the
  rooms' space is openblack's, with the temple's origin at the world's (0, 0, 0): the room's fire, a point read from
  `movement.l3d`'s own matrices, is given to `SetSun` the same way, and openblack's rooms stand at the origin.
- **The fog's table.** After the light's colour, `Temple::Draw` calls `LH3DPrimitive::CalcFogTable` 0x84A5B0
  (0x79461C, its only caller) with the red, green and blue bytes of `Temple::CitadelColour` [0xE05FE8] each times the
  float 0.4 [0x8C7664] (0x7945B2..0x794613), 100, and the room's near and far (`Temple` +0x1C's +0xE4 / +0xE8; 30 and
  80 with no room). It keeps near and far ([0xF03150], [0xF0314C]) and 1024 / (far − near) as a float [0xF03154], and
  fills 1024 pairs [0xF03158]: for step i, t = i × (1 / 1024), the first the grey 0xFF000000 plus three times
  (255 v) >> 8 with v = 256 − ftol(156 t), the second 0xFF000000 plus the three colours each `fistp`(t × colour), to
  the nearest, halves to even (0x84A604..0x84A6BB).
- **Once at a point** (`ApplyFog` 0x84A4B0): the depth is the point through the third column of `g_current_matrix`,
  stored as a float; nearer than near (or no number) takes step 0, farther than far step 1023, else
  ftol((d − near) × [0xF03154]) held to 0..1023; the colour's red, green and blue each times the step's grey byte,
  >> 8, and it returns the step's colour (0x84A557..0x84A5A0). Its callers:
  - the mesh draw fn_00809E50 (0x80A8E7..0x80A916), for each submesh drawn without a lightmap in its second pass: the
    point is the first vertex of the submesh's first primitive, as the file has it, through `g_world_to_clipping`
    (the vertex is taken as a point of the world). The model's colour [0xC37D8C] is faded, and the step's colour is
    added to its specular [0xE9FE2C] by fn_00809DE0, each channel held at 255;
  - `CreatureRoom::Draw` 0x786B43..0x786B76, the belts and medals: the colour in the temple's light
    (`ApplyCitadelColoring`), faded at the object's position (+0x38), and the step's colour **in place of** the
    temple's specular (`SetColorSpecular`'s pushed argument, 0x786B71);
  - `CreatureRoom::DrawAdditional` (0x78883D), the copy of the creature, which drops the step's colour;
  - fn_0046A680 (0x46A7BC), not read.
- **At each vertex** (`LH3DPrimitive::DrawLightMap` 0x857B20, 0x857BCD..0x857D82): a lightmapped wall's vertex's depth
  through `g_current_matrix` (0x857B74, the local-to-camera matrix, the vertex in its model's space), summed in
  another order than `ApplyFog`'s: (m[8]·x + m[0x20]·z) + m[0x14]·y, then + m[0x2C], each a float (0x857BB2..0x857BC7);
  not farther than near (or no number) keeps the model's colour and specular; not nearer
  than far takes step 1023's two colours alone; between them the step is as above and each byte of the colour and of
  the specular moves towards the step's grey and colour by (step / 4) of 256: own + ((step's − own) × (step / 4)) >> 8,
  rounded down, the colour's alpha kept (0x857D89).
- **What is not fogged ✅ read** (2026-10-22). The fog's table and range are read only by `ApplyFog`, `DrawLightMap`
  and `GetFogFactor` 0x84A430, and no hardware fog state is set. The pool's two layers and the map's markers are drawn
  through the object draw (vt+0x108, fn_0080DB30; the markers through `AddDrawing`), which never reaches fn_00809E50:
  not fogged.
- **The map's fog, which changes nothing.** `MiniMap::DrawMap` 0x7977A0 takes once a frame k = ftol(255 × (1 −
  `GetFogFactor` of the rooms' origin through `g_world_to_clipping`)) (0x797868..0x797898; GetFogFactor: 0 nearer than
  near, 1 beyond far, else (d − near) × the steps per unit / 1024; the table is not used). Each vertex's colour byte
  goes to 255 − ⌈(255 − c) × k / 256⌉ (towards white), then × the cell's brightness >> 8, and its specular × ((brightness ×
  k) >> 8) >> 8. The map's cells are white with no specular (`MiniMap::Init` 0x797530, `WorldRoom::DestroyWorldMapColours`
  0x79DCE0; `DrawWorldMapColours` is empty), so the map stays grey (255 × brightness) >> 8, as openblack draws it (test
  `TempleMap.DrawsTwoTrianglesAQuadOfEachBlock`).

### The challenge room's woosh, the heart's sparks and the worship strain

Read in the v1.42 exe (bw1-decomp has no bodies for these: `ChallengeRoom.cpp` is empty), with xref and
the oracle (each function run with stubbed callees, its globals and the arguments of its sound call captured).

**The challenge room's woosh** (fn_00782410, 0x782410..0x78248B). `ChallengeRoom`'s constructor 0x781A60 loads its
controls with `TempleRoom::LoadOptionData(file, 0x6A, 0xC25168, 0xC25310, 0xC254B8)` (0x781AA6): 106 callbacks. Of
them, fn_00782630..fn_007828B0 (step 0x20, 21 of them) call fn_007821F0(data, k) and fn_00783B30..fn_00783DB0 (21)
call fn_00782410(data, k) straight, k = 0..20.
- fn_007821F0 (the click): the challenge room camera's blend, `[[[g_game+0x59B4]+0x1C]+0xC4]+0x450` (`fld dword`,
  `fcomp qword 1.0`, `test ah, 0x40`: exactly 1 or unordered), `ChallengeRoomCamera::Init` 0x785332 sets it to 0 and
  `UpdateMain` 0x785CA4..0x785E21 moves it toward 1. At 1 (or NaN) it calls fn_00782410(data, k) and returns.
  Otherwise `Temple::SetCameraToLookAtSubMesh(data+4, 0, 0, 25.0)` 0x78223B, [0xC28604] = k, fn_00784640(k),
  [0xE01EE8] = 1, and no sound of its own (oracle: blend 1.0 and NaN choose; 0.5, 0 and 0x3F7FFFFF turn the camera,
  with submesh `data+4`, 0, 0, 25.0).
- fn_00782410 (the choice): the challenge is node k of the list [0xDD0BD8] (count [0xDD0BDC], node +0 next, +4 the
  `TempleChallenge`); k ≤ 0 gives the first node (oracle: −1 → the first); k ≥ count, an empty list or a null list
  give a null challenge, whose `GetSuccess` (0x7813F0, `fld [ecx+0x48]`) would then read address 0x48 **(not settled:
  no click reaches it unless a control exists for a slot without a challenge)**. [0xDD0BF0] = the challenge in every
  case. If `GetSuccess` is exactly 1.0 or unordered (`fcomp qword 1.0`, `test ah, 0x40`, `jne`) nothing more.
  Otherwise [0xDD0BEC] = 1, fn_00799190 (the temple fade: [0xE06024] = 0xFFFFFF, [0xE06020] = 1.0, [0xC2A150] = 0,
  [0xE06028] = 0), then `PlaySoundEffect(0, 46 + (GetTickCount() & 3), 3, 0, 0, 0, InGame)` 0x782486, the same call
  as `InnerCamera::FocusOnSubMesh` 0x7957A6 (both through 0x429D60, bank [GAudio + 0x3AC]). No random draw: the clock
  picks the woosh. Oracle: success 0.5 → chosen, fade globals as above, woosh 0x31 (tick 0x12345677); 1.0 and NaN →
  nothing; 0 and 0x3F7FFFFF → chosen.
- `ChallengeRoom::Update` 0x782170 then, once [0xDD0BEC] and the fade's end [0xE06028] are both set:
  `LeaveInsideCitadel` 0x78218D, [0xDD0BEC] = 0 and `TempleChallenge::StartScript([0xDD0BF0], 1)` 0x7821A4.

**The heart's sparks.** Both use the heart's beam state (+0xB0 the last target, +0xB4 the last turn) and their own
cyclic counter, and play through the options variant 0x429E30: bank InGame (GAudio + 0x3AC), is3D 1, track 0,
owner the heart, sample 206 + c, the point the heart's `MapCoords::GetLHPoint` (x and z from 16.16 times 10/65536,
y the land's altitude plus its height above the land), everything else the `LH_SamplePlayOptions` defaults (volume
127, pitch 100, loops 0, mode 3). The oracle captured the options for both (offsets +0x04, +0x08, +0x0C, +0x20,
+0x24, +0x30..+0x38).
- fn_004686B0 (the beam at what takes a blow; the only callers are `CitadelHeart::GetActualObjectToEffect` 0x468D1C
  and 0x468D90): nothing without its flag (second argument, a byte). A new target sets +0xB0 and +0xB4 = 0; a null
  target then does nothing. Due when turn > +0xB4 + ftol(1000 / [0xD01A38] × 2.0) (20 turns), +0xB4 = turn; the
  spot visual of +0xAC is made if missing (fn_004684A0, `CreateSpotVisualWithSpecifiedDuration`, type 0x15); without
  it nothing more; then the spark 206 + [0xC5E3E4] (0..4, 0x468815) and a `UR_PlasmaInf` from the heart to the target.
  Oracle: new target at turn 20 no, 21 yes; same target from 100: 120 no, 121 yes; flag 0 or a null target: nothing.
  This is `physics::temple_heart::BeamAtTargetDue` (and `BeamInterval`); `BeamAtItselfDue` is fn_004684F0, which has
  no sound.
- `CitadelHeart::ProcessTransferedDamageEffect` 0x468940 (from `CitadelHeart::Process` 0x4665B1, every turn, after
  fn_00465C70 and fn_004688F0, which forgets an unavailable spot visual and target): life = `GetLife` (vt +0x11C).
  Life ≤ 0 or unordered → share 0; life ≥ 1 → nothing; otherwise share = life. Then nothing unless
  `GetActualObjectToEffect(GetPlayer(), false)` is the heart itself (no building or homeless villager takes its
  blows; with the flag false neither spark function does anything). Due when turn > +0xB4 + ftol(((3.0 − 0.4) × share +
  0.4) × (1000 / [0xD01A38])), each step rounded to a float (24-bit FPU); +0xB4 = turn (the target is kept); the spot
  visual as above, none → nothing more; the spark 206 + [0xC5E3E8] (0..4, 0x468B32) only when share ≤ 0.6 (`fcomp
  0.6`, `test ah, 0x41`, je skips it above); then a `UR_PlasmaInf` between two points of fn_00465C30 whatever the share.
  Oracle (36 lives, 10 at the edges where float and double steps differ by a turn): 0 / −1 / NaN → 4 turns, 0.25 → 10,
  0.5 → 17, 0.6 → 19 with the sound, 0x3F19999B → 19 without, 0x3F7FFFFF → 29; the table is pinned in
  `TempleHeartWounds.TheOriginalsWaitAndSoundForEachLife`.
- No game-random draw in either: the two points of the hurt heart's beam come from fn_00465C30 → fn_005906E0, which draw
  with the local random only (`GRand::LocalRand` once, `GRand::LocalFloatRand` twice, xref), so the game's sequence
  never moves; openblack draws them with its local random too. The pitch deviation of the .sad (if any) is the audio
  library's own `rand`.

**The worship strain.** `Citadel::SetWorshipStrainSoundFrac` 0x463850 plays nothing itself: from
`Citadel::ProcessSpellIcons` 0x46396C with the largest site strain (+0x114, `test ah, 0x41`: strictly larger), only for
the local player's citadel, it moves Citadel +0x70 by [0xD01A38] × 0.001 toward it and clamps. The sound is the
guidance's worship site remark, which reads +0x70 (see Guidance above). In openblack this is already ported and
running: `worship::citadel::ProcessSpellIcons` (`src/Worship/Citadel.cpp`), `audio_queries::WorshipSites`
(`src/ECS/AudioQueries.cpp`) and `CheckWorshipSiteDesires` (`src/Audio/Services/Guidance.cpp`). Re-read against the
exe: identical (the NaN branches differ but cannot be reached: the target starts at 0 and only takes larger values).

### The temple's help scripts

- `Temple::StartTempleScript` (0x794A80) starts a script by name unless the game is multiplayer or a skirmish.
- `TempleRoom::OnEnterRoom` (0x79A800, the rooms' vtable slot 0x3C; ChallengeRoom and SaveGameRoom call it from their
  own): `GScript::StopHelpScripts` (types 0x4A); then, with a help system, the temple's `HelpSuppressed` (+0x44) clear
  and the help system's two on flags (+0x45F4, +0x45F8) set, the room's `HelpScriptName` (+0x3C) starts. The names are
  set by the rooms' constructors: WorldRoom and GameOptionsRoom "CitadelWorldRoomHelp", CreatureRoom
  "CitadelCreatureRoomHelp", ChallengeRoom "CitadelChallengeRoomHelp", SaveGameRoom "CitadelSaveGameRoomHelp",
  CreditsRoom "CitadelCreditsRoomHelp"; the multiplayer room has none. `Temple::InitEngine` calls it on the way in.
- The scrolls' Set callbacks look at the scroll's sub-mesh, then, unless `HelpSuppressed`, start the scroll's help:
  WorldScrollSet (0x79F0C9) "CitadelWorldRoomScrollHelp", ChallengeScrollSet (0x7843C9)
  "CitadelChallengeRoomScrollHelp", SaveGameScrollSet "CitadelSaveGameRoomScrollHelp", CreatureScrollCreatureSet
  "CitadelCreatureRoomAttributesScroll", CreatureScrollFightSet "CitadelCreatureRoomActionsLearntScroll",
  CreatureScrollLikesSet "CitadelCreatureRoomLikesScroll", CreatureScrollMagicSet "CitadelCreatureRoomMagicScroll". Their
  help does not wait for the help system's flags.
- The room cameras' `UpdateMain` stop their scroll helps by name (fn_006F0A80) on the player's input:
  ChallengeRoomCamera stops both the challenge and the save game ones (0x785E9D, 0x785EB3), CreatureRoomCamera its four.
- The creature room's zoom (fn_00789FA0): unless `HelpSuppressed`, place 2 (the belts) starts
  "CitadelCreatureRoomAttackDummies" and place 3 (the medals) "CitadelCreatureRoomMagicPlinths" (0x78A077..0x78A09F).
- `GGame::LeaveInsideCitadel` calls `StopHelpScripts` (0x553B66), under the same `HelpSuppressed`.
- `Temple::HelpSuppressed` (+0x44) has one writer (read 2026-10-22, every function that loads the temple
  [g_game +0x59B4] and every `Temple` method scanned for +0x44): `Temple::InitEngine` 0x793C63, = 0 on every entry; the
  third argument `GoInsideCitadel` passes `InitEngine` (the help system's +0x45E8 && +0x45EC, 0x554033..0x554043) is
  not read (`InitEngine` reads only its first two, `ret 0xC`). So every reader sees 0, and no help of the temple ever
  waits: `OnEnterRoom` 0x79A81E, the seven scrolls' Set callbacks, fn_00789FA0 0x78A082, `WorldRoomCamera::UpdateMain`
  0x7A1315 (`CitadelWorldRoomNotAvailable`), `SaveGameRoom::LoadGame` 0x792F84 (its "CitadelSaveGameRoomLoadHelp",
  0x792F8B) and `LeaveInsideCitadel` 0x553B5C. openblack keeps no such flag.
- **What the hand holds going in** (read 2026-10-22): `GoInsideCitadel` 0x553F10..0x553F5C sends packet 0x5B when the
  local player has a creature whose leash is on the player's interface (`GetInterfaceStatusLeashOn`); its handler
  (jump table 0x63DDCC, 0x63D3CE) calls `GLeashStatus::SetOn(creature, 0)`. The hand's held object (`CHand` +0x4904)
  is not touched on the way in (its writers are the constructor, fn_0046DC30, `ThrowObject`, fn_0046DE40 and
  `OnClearMap`); inside, `CHand::GetRequiredState` is the citadel's (0x46CD1C) whatever is held, and `CHand::Draw`
  skips the hand's effects, the virtual influence and `DrawSpellInHand` (0x46D287). openblack: `leash_tie::OnGoInsideCitadel`,
  `HandSystem::RequiredHandState`.
- **The world's dialogue is put away inside** (read 2026-10-22). `GoInsideCitadel` 0x553F97 calls
  `HelpSystem::SaveTextOnTempleEntry` 0x5C79E0 with a help system: `StopHelpScripts`, `SpiritHome(2, 0)` then
  `SpiritHome(1, 0)`; when the dialogue is held (`IsDialogueControlled`) by a task whose script type has neither
  TempleHelp nor TempleSpecial (`GetScriptType` & 0x18 = 0), it keeps the owner (+0x45CC into +0x59C), the six text ids
  (into +0x5A0..) and the two click waits (+0x57C / +0x580 into +0x5B8 / +0x5BC), sets +0x5C0 = 1 and calls
  `ClearDialogueControl`; otherwise +0x5C0 = 0. `LeaveInsideCitadel` 0x553B3F calls fn_005C7B00 before `PauseGame`
  and `StopHelpScripts`: with +0x5C0 set and nobody holding the dialogue, `DialogueControlRequest(owner)` and the
  texts and click waits come back; then +0x5C0 = 0 and `ReInitialiseText` 0x5C7A80, always: fn_005CB000 on the text
  display, then each of the six texts that is set, the oldest first, added again with no number
  (fn_005CCED0), and the newest's reading time started again (fn_005C61B0). `HelpSystem::ResolveLoad` 0x5C78B0 ends
  in fn_005C7B00 too. fn_005CB000 only sets the display's hidden flag (+0xAC = 1). openblack:
  `temple_help::PutAwayDialogue` from `TempleInterior::Activate` before the room's help starts, and
  `BringBackDialogue` from `Deactivate` before the help stops (raffclar's #468, refitted onto our dialogue control
  and `help::DialogueText::Hold` / `Restore` / `ShowAgain`); tests `TempleHelp.AScriptsDialogueIsKeptWhileInsideAndGivenBackOnLeaving`,
  `ATempleScriptsDialogueAndNoDialogueAreNotKept`, `TheCinemaBarsAloneAreKeptWithNoOwner`,
  `TheDialogueTakenInsideIsNotGivenBack`, `DialogueText.TheTextsTheTempleKeptAreShownAgain`; the debug window's Temple
  page shows the task whose dialogue is kept. `ResolveLoad`'s call waits for saved games.

### The challenge room's controls and its camera's blend

Read in the v1.42 exe (bw1-decomp has headers only for `ChallengeRoom`, its camera, `TempleRoom` and
`InnerCamera`), with xref and the oracle.

**The controls.** `ChallengeRoom`'s 106 controls (`LoadOptionData`, the table at 0xC25168 / 0xC25310 / 0xC254B8):
- 0..20 `LH_Picture_Challenge_01..21` (pictures): the click is fn_007821F0(data, k), the draw state fn_00782260, shown
  for k < count.
- 21..41 `LH_FRAME_CHALLENGE_01..21` (bronze frames): shown for k ≥ count (fn_00782530).
- 42..62 `LH_FRAME_CHALLENGE_SILVER_01..21`: shown for k < count and the challenge's +0xD4 (its type as found) 0
  (fn_00782560); 63..83 `LH_FRAME_CHALLENGE_GOLD_01..21`: k < count and +0xD4 ≠ 0 (fn_007825D0).
- 84..104 `LH_Button_Challenge_01..21`: the click is the choice fn_00782410(data, k) straight; shown for k < count and
  a success that is neither exactly 1 nor unordered (fn_00782490; the compare is unsigned, 0x78249A; C3 hides it,
  0x7824D5).
- 105 `LH_Scroll_Challenge`, the room's scroll.
- count is [0xDD0BDC], the list's length; slot k is the (k + 1)th challenge found (a new one is appended at the tail,
  `UpdateChallenge` 0x78494E). The room's mesh has the names in other cases (`LH_PICTURE_challenge_01`,
  `LH_BUTTON_challenge_01`, `LH_SCROLL_CHALLENGE`), and the lookup ignores case (read: fn_00799110, `__stricmp`
  0x79913A).
- Pictures and buttons act when the press is let go over them (`TempleRoom::UpdateMouse` 0x79A3A0; the picture's
  vt+4 0x78E190, the button's 0x78DC40), as the main room's buttons do.

**The camera's blend** (`InnerCamera::ZoomProgress`, `ChallengeRoomCamera` +0x450). `Init` 0x785332 sets 0. Each
frame `TempleRoom::UpdateMouse` passes dt = `g_delta_time` × 0.001f (0x79A507..0x79A519) through
`ChallengeRoomCamera::Update` 0x7854D0 and `InnerCamera::Update` 0x796B60 to `UpdateMain` 0x7859D0 (vt+0x1C at
0x796DA8), unscaled. There, with the camera's state +0x124:
- not 4 (0x785CA4..0x785CE4): over 1 it becomes 1 (strictly over: an unordered one is kept), then blend − dt × 0.7f
  (0x8AB238); not over 0 (or unordered) it becomes 0;
- 4, looking at a submesh (0x785DE8..0x785E21): blend + dt × 0.75f (0x8AB274); over 1 it becomes 1 (an unordered one
  stays unordered).
- The game's floating point runs at 24 bits (control word 0x007F from `LHResetFPU` 0x7DEE00; `LH3DRender::StartFrame`
  sets 53 bits only for its own timing, 0x82F12A..0x82F144, and resets at 0x82F275), so each step is single float
  arithmetic. The oracle ran the two blocks' bytes for blends 0, 0.5, 0.99, 1 and 1.2 at dt 0.016f, 0.033f and 0, and a
  random sweep of 4000 cases, all matching single float arithmetic (53-bit arithmetic differs in 7.5 % of them).
- What the camera looks at does not matter: a look at the scroll raises it too, so a click on any picture after it
  chooses at once.

**The replay** (`TempleChallenge::StartScript` 0x781460, read with xref for raffclar's #381). It pushes the record's
values from the last to the first (`GScript::PUSH`, count +0x70, values from +0x74, types +0x30 past each); then, only
when its argument is set **and** both ControlMap switches `[[g_game+0x250300]+0x652C]` and `+0x6530` are on (the
player's camera and interface keys, `help::interface_interaction::KeyShortcutsEnabled`), `GCamera::SetPositionAndFocus`
with the record's camera position +0x50 and focus +0x5C; then `GScript::StartScript` by the record's name (+2). The
room's `Update` calls it with 1 after `LeaveInsideCitadel`; a started scroll's second tap with 0.

**The leave turn** (re-read 2026-10-18 for the landed #381). `ChallengeRoom::Update` 0x782170 leaves when the pending
replay flag `[0xDD0BEC]` is set and the temple fade's arrival counter `[0xE06028]` is above 0; the counter is cleared
when the fade starts and raised by `Temple::UpdateFade` 0x794280 when the fade reaches its target (white). In
`GGame::Process3dEngine` the temple's `Update` (call at 0x54DE50) runs before `Temple::UpdateFade` (call at 0x54E2DE),
so the room leaves on the frame after the one the screen turns white. openblack is the same: `TempleInterior::Update`
checks `_replay` and `ScreenFade::GetTurns() > 0` (`done`, cleared by `FadeThrough`, raised when the fade passes its
target), and `Game.cpp` runs the temple's update before `FallingSpellVideo::ProcessFrame` steps the fade. ✅ No change.

**The looked-at picture.** `OnEnterRoom` 0x784F14 resets the high-detail slot `[0xC28604] = -1` and "a picture looked
at" `[0xE01EE8] = 0`; fn_007821F0 sets both (above); `ChallengeScrollSet` 0x7843D1 clears `[0xE01EE8]` only.

**The close look** (`ChallengeRoom::Draw` 0x781D30, read 2026-10-22): after `PictureRoomBase::Draw`, only while the
blend +0x450 is not 0 (an unordered one counts as 0) and a picture is looked at ([0xE01EE8]), the record of slot
[0xC28604] (`fn_00784F70`) is written in `TempleRoom::ScrollFont` (`GatheringText::gamefont`, `InitCommon` 0x79A680)
at the size `fn_0078F100` gives, 30, or 33 when the language needs bigger text (`NeedsBiggerText` 0x4079C0: languages
6, 10, 11, 13, 14), its alpha ftol(blend × 255), in black, then in yellow two pixels right and down. The title (+0x44;
a number at or past the texts, or 0, gives text 0), copied into 64 characters, at x = (W − ftol(its width)) >> 1 as an
unsigned number (W the screen's width [0xE85058], so a line wider than the screen goes off it), black at (x, 10),
yellow at (x + 2, 12); then "%s %.2d:%.2d:%.2d  %s %i%%" (0xC2506C; texts 0x141A and 0x141B, ftol(success × 100))
centred the same on its own width at (x, size + 10) and (x + 2, size + 12). The time is (the done turn when the
success is exactly 1, else the game's turn) − the start turn, over 1000 / the milliseconds a turn, both whole numbers.
What `DrawTextRaw`'s fifth (10.0) and eleventh (4096.0) arguments do is not read. openblack:
`TempleInterior::UpdateChallengeCaption`, `GameInterface::DrawCaption` (`temple_challenge_room::TextSize`,
`TitleText`, `LineLeft`, `TimeTaken`, `DetailsLine`, `TextAlpha`), scaled past 800x600 (raffclar's dev#52); tests
`TempleChallengeRoom.TellsTheTimeTakenAndTheSuccess`, `TheCloseLooksLinesAsTheOriginals`.

**The map's challenge markers** (`WorldRoom::DrawChallenges` 0x79E030). For every challenge whose success is under 1
(an unordered one counts, `fcomp` 1.0, `test ah, 1`), the glow pass (argument set, 0x79E081) draws a glow of 0.65 in
the bytes 0x61, 0x6E, 0x7C (blue, green, red: red 0x7C, green 0x6E, blue 0x61) at the record's camera position (`TempleChallenge::GetPosition` 0x7813B0) plus 0.1 up, in world
units, never through `MiniMap::CalcPoint`; the marker pass (0x79E115) places a white marker with `CalcPoint` of the
position's x and z as they are (not from their cell, as the temples' and creatures' callers pass). So the glow is drawn
far outside the room (raffclar's vb#43, kept).

**The way to a close look and back** (read 2026-10-24 in the v1.42 exe with xref and the oracle; bw1-decomp has the
headers only). Every room of pictures' camera is a `ChallengeRoomCamera`, so this holds for the challenge, save game,
options, future and library rooms:
- **What sets the look.** `TempleRoom::SetCameraToLookAtSubMesh` 0x79A5D0 (from `Temple::SetCameraToLookAtSubMesh`
  0x7949E0: a picture's click fn_007821F0, the rooms' scroll callbacks, `SaveGameScrollSet`, `WorldScrollSet`, the
  library's fn_0078B560..fn_0078BC70) calls the camera's `FocusOnSubMesh` (vt+0x24, `InnerCamera::FocusOnSubMesh`
  0x795570 for every temple camera): the submesh index +0x44C, the look's origin +0x454 (the frame × (middle +
  offset)) and focus +0x460 (the frame × middle), and the woosh unless the state +0x124 is 4 already.
- **The states.** In state 1 (the player's), `UpdateMain` 0x7859D0 asks for state 4 when +0x44C ≥ 0 (0x785F4C); in
  state 4 for state 1 when +0x44C < 0 (0x785DBE). `UpdateState` 0x785500, after `UpdateMain`, takes the asked state:
  into 1 it clears the turn-by-itself time +0x3D0 and +0x44C (-1), and from 4 it also sets the height target +0x504
  to 6.0 (0x78553D). So the look starts the frame after the one it is asked in.
- **The aim** (fn_00785580, `UpdateMain`'s three calls 0x785D76, 0x785DE3, 0x7861F9): the room's view from the turn
  +0x46C, lean +0x49C and height +0x4CC zoomers; then, only with the blend +0x450 over 0 (an unordered one or one at
  or under 0 is stored as 0, 0x785986), one over 1 stored as 1 (0x7858FE), along = (1 − cos(blend × π)) × 0.5
  (`fmul` float π 0x40490FDB, `fcos`, `fsubr` qword 1.0, `fmul` qword 0.5, `fstp` float, 0x785908..0x78592F) and
  each point + (the look's point − it) × along (`LHPoint` `-`, `*`, `Add`, 0x785941..0x78597E). At 24-bit precision
  the cosine stays extended and only 1 − cos is rounded: the oracle ran the function over 3000 random blends and
  points, bit for bit as float arithmetic with the cosine taken in double.
- **The library turns about its path's end** (read 2026-10-26). The aim's eye and look point are turned about the room
  centre passed in, unless the camera's byte +0x548 is set: then about the point +0x54C, with its y forced to 0
  (0x785825 writes 0 into +0x550; fn_00785580 0x7857D3..0x785873). `ChallengeRoomCamera::Init` fills +0x54C
  (0x785319..0x78536F) with the first output, the eye, of the path evaluator fn_0086D4E0 on the room's path [+0x43C]
  at the time [path+4]: that function clamps a time past [path+4] to [path+4] − 1 and takes the key
  ([path+8] − 1) × t / [path+4] (keys of 0x18 bytes from +0xC), so [path+4] is the path's length and the point is the
  eye where the path ends. `PictureRoomBase::InitEngine` copies +0x548 from the room's byte +0x12C (0x78F587..0x78F58D),
  which `PictureRoomBase`'s constructor clears (0x78F1C3) and only `CreditsRoom`'s constructor sets, to 1 (0x78B1BE;
  every write of a +0x12C in the exe scanned, the other writers are cameras and unrelated classes). So only the
  library turns about its path's end, on the floor. openblack: `k_CreditsRoom.aroundPathEnd` in
  `TempleCameraModel::PictureOrbitPose`, with the path end's eye `PathEnd(room).origin`; test
  `TempleCamera.TheLibraryTurnsAboutItsPathsEndOnTheFloor`.
- **The zoomers.** `InnerCamera::Update` 0x796B60 sets the times +0x3EC and +0x3F0 to 0.5 and +0x3F4 to 1 every frame
  before `UpdateMain` (0x796BEA..0x796BF6); after `UpdateMain` and `UpdateState` it gives the position zoomers (+0x4)
  the aim's origin +0x3D4 with +0x3EC and the focus zoomers (+0x94) its focus +0x3E0 with +0x3F0, each with
  `Zoomer::SetDestinationWithSpeedAndTime(dest, 0, T)` (the focus's z inline), then steps all six by dt
  (0x796DD4..0x797103). The drawn camera is the zoomers' value: the camera chases its aim, so it lags behind the
  cosine and, on the way back, first runs on past the look before turning back.
- **State 4** (0x785DBE..0x785F49): the aim (+0x3D4/+0x3E0) with the blend as it was, then the lean target +0x500 =
  0.5 and the height target +0x504 = the aim origin's y (0x785DF2..0x785E02), then the blend + dt × 0.75. The times
  stay 0.5. A press (the argument [esp+0x78]) or one of actions 7 to 10 then stops the challenge and save game help
  scripts, asks for state 1, sets +0x558, the turn target +0x4FC = `atan360`(focus.x − origin.x, focus.z − origin.z)
  + π/2 of the aim (the same as the room's starting turn: `atan360(x, z)` is atan2(z, x), oracle), the height target
  the room's lowest +0x508 (then 6.0 from `UpdateState`) and the lean target 0.2 (0x785E91..0x785F39). `UpdateMain`
  returns there: none of the room's own controls run in state 4.
- **State 1** (0x785F4C..0x786224): the blend falls first (0x785CA4, all states but 4), then the room's controls (a
  drag only while +0x558 is clear: a press with it set waits to be let go, which clears it, 0x785F6C/0x785FF6), then
  the aim with the fallen blend and the times × the blend (0x7861FE..0x786224): 0.5 × blend, so none once the blend
  is 0, where `SetDestinationWithSpeedAndTime` puts the zoomers on the aim at once (T < 0.001).
- The oracle: fn_00785580 for blends 0.012 to 0.99, 1.2, −0.1 and NaN from two views toward two looks, and 95 frames
  of a look and back at 30 frames a second through the exe's own 0x407D60 and 0x442720 (the camera's x); pinned in
  `TempleCamera.APictureRoomsCameraAimsAlongHalfACosineTowardACloseLook` and
  `APictureRoomsCameraEasesToACloseLookAndBackAsTheOriginals`.
- The other rooms: `CreatureRoomCamera` mixes its own aim toward +0x454/+0x460 with the same cosine (0x78A26E..
  0x78A396, not read further). `WorldRoomCamera` (the main room) never reads +0x44C, +0x454 or +0x460, and no temple
  camera but these two enters state 4 (the only stores of 4 to +0x124 or +0x438 in 0x780000..0x7A2000 are 0x785518,
  0x785F56 and the creature room's 0x789E61): the main room's scroll
  sets a look its camera does not follow (🔍 inferred from the reads; not run in the game).

### Which rooms are open

- `WorldRoom::RoomsAvailable` [0xE3A1B0] is 0 from `WorldRoom::ResetStatics` 0x79F5D0 (0x79F62D, called by the
  WorldRoom constructor 0x79D1E7) and is set to 1 by every `WorldRoom::Draw` 0x79E280 (0x79E295, `ebx = 1`): once the
  main room has been drawn in a game, it stays set.
- Each room's `IsAvailable` reads it, with [g_game+0x205A0C] (`SkirmishGame`, bw1-decomp `Game.h`):
  - challenge room (0x784B60): 0 in a skirmish or multiplayer game; else the network session's +0x94 must be 1 and
    the flag set;
  - creature room (0x789B30): the local player's creature (g_game+0xA64 + the player index [g_game+0x205A59] × 0xA60)
    and, outside a skirmish or multiplayer game, the flag;
  - library (0x78CBE0): 1 in a skirmish or multiplayer game, else the flag;
  - save-game room (0x7937E0): 1 while [0xBEC284] == 5 (the room the game keeps the player in), else not in a
    multiplayer game and the flag;
  - multiplayer room (0x79D0A0): 1 in a skirmish game, else the flag, else [0xBEC284] == 3.
- A click on a closed room's door (`WorldRoomCamera::UpdateMain` 0x7A12D8..0x7A132B): the room's `IsAvailable` (its
  vtable +0) false, the camera keeps its pose (+0x438 = +0x124) and, unless the temple's `HelpSuppressed` (+0x44, bw1-
  decomp `Temple.h`) is set, `GScript::StopHelpScripts` 0x6EC780 (help, temple help and multiplayer help) then
  `Temple::StartTempleScript("CitadelWorldRoomNotAvailable")` 0x794A80, which starts nothing in a multiplayer or
  skirmish game.

## openblack

Status: **partial**. The draw, the challenge records, the sounds and the temple turn are done; what is missing is in
Pending. The drawing is raffclar's, re-applied on our renderer (`Renderer::DrawTemplePass`,
`Graphics/RendererTemple.cpp`).

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The temple interior**

- `Renderer::DrawTemplePass` (`Graphics/RendererTemple.cpp`), raffclar's drawing re-applied on our renderer.
- the temple pass draws the dome and the moon, and neither the sun nor its glare, as the original.

**The depth fog and the temple's light** (raffclar's, from PR #349, on our temple renderer; its arithmetic checked
against the v1.42 exe on 2026-10-20, ✅ faithful but for the wall vertex's depth below; no shot of the original
compared)

- **Checked against the exe** (2026-10-20, read and run with the oracle): the ranges (`TempleRoom`'s constructor 60 / 200
  at +0xE4 / +0xE8, 0x798971 / 0x79897B; `CreatureRoom::Draw` 40 / 150 as its first instructions, 0x786849 / 0x786853,
  on every draw; `Temple::Draw` reads the current room's before any room is drawn, 0x794582..0x794636, so the creature
  room's first frame takes 60 / 200; no room 30 [0x8BF51C] / 80 [0x8D060C]); the steps (`CalcFogTable` 0x84A5B0,
  1024 [0x8D167C] / (far − near), `ApplyFog` 0x84A4B0: 3002 depths × 3 ranges against `Step` / `ShareOfStep`, no
  difference); the share (0x84A604..0x84A6AC, 256 − ftol(156 × i / 1024), grey (255·v) >> 8); the added colour
  (`CitadelColour` [0xE05FE8] × 0.4f [0x8C7664], `fistp` halves to even, 0x7945B2..0x794613); the walls' quarter
  step, the once-only points and the belts' added colour in place of the specular. ⚠️ approximate: a lightmapped wall
  vertex's depth is summed x, z, y then the offset on the CPU in the original, and in `mul(u_view, world).z` on the GPU
  in ours: at most a float's last bit apart, so a vertex can only change step exactly at a step's edge; the GPU can't
  be made to round that way. 🔍 inferred: `Once` places a submesh's first vertex at the temple's origin plus the file's
  vertex, where the original takes the file's vertex as it is; the two agree while the origin is (0, 0, 0).
- The light: `model_light::k_Temple` (−1000, 1000, −500) and `model_light::InsideTemple(origin)`
  (`Graphics/ModelLight.h`), set by a `ScopedLight` for the whole of `DrawTemplePass`, the hand's draw included, so
  the copy's own `ScopedLight(fire)` nests inside it. Test `ModelLight.TheTemplesLightInsideTheTemple`.
- The fog: `temple_fade` (`3D/TempleLight.h`) over the frame's `TempleLight::fade`, which keeps our ranges (the
  creature room's 40 / 150 only once it has been drawn). `Step`, `ShareOfStep` (our `Share` is made of the two, the
  same values), `EndColour` (the light's bytes × 0.4f), `AddedAt` (rounded halves to even), `Once` (a point) and
  `AtVertex` (a lightmapped wall's vertex). `Renderer::TempleFogOf` gives a pass its fog, from its camera's view.
  - The rooms' meshes (`drawInstances`): lightmapped submeshes at each vertex in `vs_object_temple_instanced`
    (`assets/shaders/temple_fog.sh`, the GPU twin of `AtVertex`); the others once on the CPU (`Once`) at their first
    primitive's first vertex from the temple's origin, with the light's colour (or a control's glow) kept and the
    fog's added to it; the vertex shader then shades the faded colour, in whole bytes, with the model light.
  - The belts and medals: once on the CPU where each stands, the fog's colour in place of the light's.
  - The copy keeps its own fade without the added colour (unchanged); the hand, the pool, the map and its markers,
    the glows, the beams and the mists are drawn as before (Pending).
  - In the main room's reflection the points fogged once are mirrored through y = 0 with the reflection's camera, so
    their depth is the one the room's camera sees, as the original's untouched `g_world_to_clipping`.
  - The debug Citadel window's "Temple fog" (`EngineConfig::drawTempleFog`, on) turns it off and shows the frame's
    range and colour. Off, the rooms are drawn exactly as before.
  - Tests `TempleFade.AStepIsTheWayFromTheStartInEvenStepsHeldAtTheEnd`, `TheColourAddedIsTwoFifthsOfTheTemplesLight`,
    `TheColourAddedAtAStepRoundsHalvesToEven`, `AModelIsFoggedOnceAtItsDepth`,
    `AWallsVertexMovesTowardsTheFogByAQuarterOfTheStep` (`test_temple_light`), from raffclar's `test_temple_fog`
    values.

**The challenge list**

- raffclar's `script::challenge_snapshots` (`ScriptHeaders/ScriptChallengeSnapshots`, the pops) and `ecs::challenge_records` (`ECS/ChallengeRecords`, the list, records as `components::ChallengeRecord` entities), pinned to the exe by `test_challenge_records_original` (oracle runs of `ChallengeRoom::UpdateChallenge` 0x784780 and `GAlignment::ScriptUpdate` 0x414360). The three natives fill it (`KeepChallengeRecord` in `CHLApi.cpp`): they pop every value, keep the record, hold its things through the script objects service and move the local player's alignment at once (`effects::alignment::AddClamped`). The scripts' restart empties it as `ChallengeClear` does, after the intro's things are released (`GScript::CleanGameForScriptReboot` 0x6EBC00; openblack: `challenge_records::Clear` in `Game::PrepareNewLand`).
  - **The picture** (raffclar's PR #378 and #455, **unverified against v1.42**). A new record always asks for one, and
    later a SNAPSHOT, or an UPDATE_SNAPSHOT_PICTURE whose "take picture" is set (`Outcome::takesPicture`). The record
    gets a `components::ChallengePicture` and `challenge_pictures::Ask` names the picture: the frame already on the
    screen as the script runs, the one drawn before the snapshot's turn, as the original reads the front buffer inside
    the native ([the pictures](#the-challenge-list)), so two snapshots of one turn share it. bgfx reads only the frame
    being made, so the frame on the screen is kept aside as it is drawn:
    - When the scripts' program is read (`Game::FindSnapshotScripts`), `challenge_pictures::ScriptsThatSnapshot` finds
      the scripts whose code calls SNAPSHOT, UPDATE_SNAPSHOT or UPDATE_SNAPSHOT_PICTURE, or starts a script that does
      (each start is a task of its own). While any running task runs one of them (`challenge_pictures::KeepsShownFrames`,
      checked every frame, `FrameSnapshot::keepShownFrame`, `RendererInterface::KeepShownFrame`), the frame is kept.
    - A kept frame's screen passes (`ShownFrameTargets::k_Passes`: Main to Interface, the game's whole frame) draw into
      one of two targets of the window's size, 8-bit BGRA colour and D24S8 depth, in turn (`graphics::ShownFrames`,
      numbered by the renderer's frames handed over). A full-screen pass (`RenderPass::ShownFrameCopy`, the existing
      `Interface` program, point-sampled, white vertices, no blending) copies the target to the window, and the debug
      windows (ImGui, where the frame counter is), the pointer and the alpha pass draw over it as before. So the frame
      kept has no debug windows and no frame counter.
    - The pointer stays out of the frame kept and is drawn over the debug windows as before. It is drawn only with the
      front end's boxes and pages (`GameInterface`), which pause the game, so no script turn runs while it shows. In
      play the pointer is the 3D hand, which the Main pass draws.
    - A snapshot asks for the picture of the frame on the screen, the last one handed over
      (`RendererInterface::AskShownFramePicture`, `ShownFrames::Ask`, `OnScreen`). As the next frame begins (which
      draws into the other target), that target is blitted into a read-back texture and read back once
      (`ShownFrameTargets::ReadBack`, as `OffscreenScreenshots` does), and `challenge_pictures::Take` makes the
      256 × 128 picture when the pixels are back. Two snapshots of one turn share that read-back.
    - Cost while kept: one full-screen copy a frame, plus one read-back for each frame a snapshot reads (measured
      2026-10-29 on Land 1 at 1536 × 864: 3.6 to 3.9 ms a frame against 3.6 to 4.7 ms without, within the noise). Not kept (nothing running can take a picture, the multisampling debug switch on, a renderer
      without texture blits and read-backs or without those formats as targets, or the window drawn aside while
      minimised): the frame is drawn to the window as before.
    - A check's screenshot of the window stays bit-identical. The passes run the same draws into a target of the
      window's size and formats (8-bit unsigned normalised colour; D24S8 depth, the format bgfx gives the window's
      depth on Direct3D 12 and Vulkan), with the same clears. The copy gives each pixel its own texel unchanged: point
      sampling at the pixel's centre, a vertex colour of 1, and an 8-bit value read and written back to 8 bits is the
      same value. Nothing after the copy tests depth (the debug windows, the pointer's canvas, the alpha pass).
    - A picture taken while the frame on the screen is not kept (a native called from outside a script, as the testbed
      does, or a renderer that can't keep frames) falls back to the next frame drawn and logs it once. The frame's logic
      hands the names not yet sent to its draw (`challenge_pictures::TakeRequests`, `FrameSnapshot::screenPictures`),
      which asks the renderer for an in-memory picture of the frame (`RendererInterface::RequestScreenPicture`,
      `graphics::ScreenPictures`) and leaves the debug windows out of that frame.

    The renderer makes the picture on its
    own thread when it reads the frame (`challenge_pictures::Take`): 256 by 128 RGB picked evenly over the screen, each
    pixel the mean of itself and the pixel picked before it in its row, then softened by blending each pixel with the
    ones to its right, below and below to the right (keeping 128/255 of each, the last row and column as picked); the
    128 by 64 low-detail copy averages the pixel pairs of every other row (`Shrink`). `Game::FinishFrame` collects the
    pictures that are back (`challenge_pictures::Collect`); one the renderer gave up leaves the old picture. The
    challenge room draws them on its walls (`TempleChallengePictures`, [below](#the-challenge-rooms-controls-and-its-cameras-blend)). Tests `test_challenge_pictures`,
    `ScreenshotCapture.APictureIsMadeFromTheFrameAndTakenOnce`, `AForgottenPictureIsNotMade`. Debug: the LHVM
    viewer's Challenges tab shows each record's pictures taken and waiting, and whether the frame on the screen is kept
    with the last frame kept. At debug level (`-L game=debug,graphics=debug`) the log says how many scripts can take a
    picture when the program is read, each time the keeping starts (with the task that starts it) or stops, when the
    targets are made, and once per reason why a frame asked for is not kept.
  - **The reminder** (**unverified against v1.42**). A tap on a challenge's scroll (`script_highlight::InterfaceTap`,
    any player's, as the original's has no check there) calls `CHLApi::StartChallengeReminder`: a help script holding
    the dialogue is stopped first, as in `InterfaceTap` before `Temple::StartScript`; with nobody holding it and a
    record, its values are pushed from the last to the first (`ReminderArguments`) and its reminder script started
    with the types 0x7F (`script_control::StartScriptWithArguments`, which `StartHelpScript` also uses). Before the
    challenge's script has taken its SNAPSHOT there is no record and nothing starts, so in practice the second tap
    replays it. Debug: a Remind button per record in the LHVM viewer's Challenges tab.

**The challenge room's woosh, the heart's sparks and the worship strain**

- **openblack.** `temple_challenge_pick` (`src/3D/TempleChallengePick.h`: `ClickOn`, `CanChoose`, `ChosenWoosh`), with its test (`test_temple_sounds`), pinned to the oracle, and connected in the challenge room (`TempleInterior::ClickChallenge`, below). The heart's sparks play from the temple destruction system: its beam at what takes a blow (`BeamAtTargetDue`) and its beam across itself while hurt (`DamageBeamSounds`, from 0.6 of its life down) each play the next of the five through `TempleDestructionWorld::PlaySpark`, with a counter of their own (`audio::Counter::CitadelSparkEffect` and `CitadelSparkDamage`), in 3D at the heart, owned by it and not following it; `test_temple_destruction_system` records them with a fake, and `test_temple_heart` pins the heart's rules (`src/Physics/TempleHeart.h`).

**The challenge room's controls and its camera's blend**

- **openblack.** `temple_challenge_room` (`src/3D/TempleChallengeRoom.h`: `ControlOf`, `Visible`, `BlendStep`), pinned to the oracle's answers (`test_temple_challenge_room`). The blend is the room camera's zoom toward the submesh it looks at (`TempleCameraModel`, which the scrolls already use: the same rates and the same clamps, so the same values; it steps through `BlendStep`). `TempleChallengeControls` shows the controls of the challenges found (`ecs::challenge_records::InOrder`, a quest's frame gold), hides the others from the draw and the pick, and turns a let-go over the same picture or button into its click; a picture's click before the blend is 1 turns the camera to it (`FocusOnSubMesh` 0x795570: position = frame × (middle + (0, 0, 25)), look-at = frame × middle, in the room mesh's space; the scrolls pass (−30, 0, 0) and a save-game slot (0, 0, 20)).
- **The way to a close look and back** ✅ ([the original](#the-challenge-rooms-controls-and-its-cameras-blend)): in the rooms of
  pictures `TempleCameraModel::UpdatePictureLook` aims the camera at `TowardCloseLook` (the room's view toward the look
  by `CloseLookAlong`, with the blend before it rises), heads the lean for 0.5 and the height for the aim's, and gives
  the zoomers half a second; taken away by the player it turns the room to face the look's way (`PictureStartYaw` of
  the aim), leans it to 0.2 and holds the drag until the press is let go (`_pressLeftLook`); back with the room the
  height heads for 6 and the turn by itself starts afresh. `UpdatePictureOrbit` aims at the view toward the look with
  the fallen blend and eases in `PictureEaseTime` (0.5 × blend). The cosine is not applied to the drawn camera in
  these rooms; the main and the creature's rooms apply it. Debug: the Camera window's "Temple close look" line (blend, along, the ease time).
  Tests `TempleCamera.APictureRoomsCameraAimsAlongHalfACosineTowardACloseLook`,
  `ACloseLooksAimIsTheRoomsViewWithNoBlendAndTheLookPastAFullOne`, `APictureRoomsCameraEasesToACloseLookAndBackAsTheOriginals`.
- **The choice and the replay** (raffclar's #381, refitted; **unverified against v1.42**). `TempleInterior::ClickChallenge`: a button's click, or a picture's once `ClickOn` says Choose, chooses the slot's challenge if `CanChoose`: the record (`InOrder`) is kept in `_replay`, the temple fade goes to white and `ChosenWoosh` plays. A picture's turn marks its slot looked at and in high detail (`TempleChallengeControls::LookAt`); entering the room (`SetCurrentRoom`, `Activate`) forgets both (`Enter`), and the room's scroll the look (`LookedAtScroll`). Once the fade is over, `TempleInterior::Update` leaves (`LeaveToReplayChallenge`: `Deactivate`, then `CHLApi::StartChallengeScript(challenge, true)`), which pushes `ReminderArguments`, puts the camera at `challenge_records::ReplayView` (both views kept and `KeyShortcutsEnabled`) and starts the reminder with the single-player script types. A started scroll's tap (`CHLApi::StartChallengeReminder`, raffclar's #378) goes through the same start, without moving the camera, after its dialogue check (`help::script_control::FreeDialogueForScript`); a record with no reminder name moves the camera on a replay but starts nothing.
- **The pictures** (his #381): `TempleChallengePictures` writes each started slot's record picture (`components::ChallengePicture`, #378) to a 128x64 texture, and the slot looked at (`GetHighDetailSlot`) to a 256x128 one, again whenever the record's count of pictures taken changes (`temple_challenge_pictures::StepsFor`); the textures go to the room's picture submeshes through `GetScrollTextures`, and are dropped as the player leaves. The debug panel lists each record's pictures taken.
- **The tooltips** (his #381): `UpdateTempleToolTip`'s challenge room branch walks the pictures, buttons and scroll in the order of their submeshes (`TempleChallengeControls::GetToolTipControls`): Zoom In from afar, Replay close up (nothing once won), Zoom Out off the picture, Replay on a button; the scroll says nothing close up while a picture is looked at.
- **The close look** (his #381): `TempleInterior::UpdateChallengeCaption` builds a `gui::GameInterface::Caption` from the looked-at record (`temple_challenge_room::TimeTaken`, `DetailsLine`, `TextAlpha`, the turn length `game_clock::MsPerTurn`), drawn by `GameInterface::DrawCaption` with the sizes × `gui::ReferenceScale` (his dev#52, a presentation deviation taken; exact at 800x600) and the plain glyphs.
- **The map** (his #381): `UpdateMapMarkers` marks the challenges not done in white, placed with `TempleMap::ToMap` of the view's x and z, their glow at the world position + 0.1 (`TempleMapMarker::glow`, drawn in `RendererTemple.cpp`; vb#43 kept).
- The scroll's counts and titles come from `challenge_records::List` (`GatherScrollFacts`).
- Testbed: `temple.challenge_room` (four snapshots with their views, two clicks) and `temple.challenge_markers`; the temple debug panel's challenge list shows which slots have their replay button.

### The citadel interior in openblack

`g_game+0x205A28` (0x4282F0, the symbol says `HelpSystem::GetWideScreenControl`) is set to 1 by `GGame::GoInsideCitadel`
0x554004 and to 0 by `GGame::LeaveInsideCitadel` 0x553B1F. In openblack the value is `game_clock::SequenceMode`: the
temple interior (`Locator::temple`, opened and closed by `ENTER_EXIT_CITADEL` and the debug window) sets it to
`k_SequenceModeCitadel` when the player goes in and back to `k_SequenceModeNone` on leaving. `Game.cpp` registers
`GameQueries::insideCitadel` as `game_clock::IsInsideCitadel`, which `StartCameraControl` (CHLApi.cpp) also reads.
What the audio does inside (all disassembled again):

- **Filters**: `GAudio::PlaySoundEffect` 0x429F6D and `SamplePlayAnimEffect`
  0x42A554 only let the userParam 2 samples sound (`cmp di/bp, 2`); the 3D cutoff is measured from
  `LH3DTech::g_camera` (0x429EB1), which in openblack is the same camera (the temple one inside). The ambience is turned off
  (fn_00429100, `atmos_banks::SetTargets`) and the advisor does not interrupt (0x5C3810).
- **Music** (`GameMusic::ProcessCitadelMusic`, `ProcessCitadelMusic` 0x427B60, first branch of `ProcessMusic`
  0x427E2C): inside, on the first turn `LHSampleStopAll` (`sample_play::StopAll`, the 16 channels; the latch is the
  global [0xC56164], which `GAudio::Reset` does not touch and which goes back to 0 on leaving, 0x427C7F); type = 44 + fn_00426C80(
  `GetDiscreteAlignmentValue` 0x414730 of `GPlayer::GetAlignmentValue` 0x64D6A0 of the local player g_game+0x205A59),
  CITADEL_EVIL / NEUTRAL / GOOD, all three in `citadel.sad` (group 4); without a bank it returns 0 and `ProcessMusic` continues
  (0x427BDF). Options: bank, volume 127, start `pos[group − 1]` read (0x427BF6) **before** `fn_004281C0` saves the positions (0x427C28; the alignment saves first), sync 1, fade 1, 2D, pitch 100;
  `LHMusicPlay` **every turn** (the engine re-triggers the same bank, 0x1000DFD4), "Music Playing=%s" and
  returns 1 → `ProcessMusic` sets "Music Playing=NONE", +0x180 = 0, +0x1C = −1 (on leaving the alignment music starts
  again). The alignment is the new query `GameQueries::localPlayerAlignment` (`ecs::audio_queries`:
  `ecs::effects::alignment::Get(PLAYER_ONE)`; 0 without it), not GAudio+0x190.
- **A land loaded with the temple open**: the original never does it (the one `GGame::LoadMap` caller that can run
  inside, the setup box's tutorial land, calls `LeaveInsideCitadel` first); openblack's `Game::PrepareNewLand` calls
  `TempleInterior::Deactivate` first when the player is inside, so the player leaves the rooms as on every way out
  before anything of the land goes. Only the leave is ported, not the tutorial land's save.
- **Leaving** (`audio::LeaveCitadel`, from `TempleInterior::Deactivate`): `LeaveInsideCitadel` 0x553B25 → Temple
  `fn_00793D00` (if its engine was started, Temple+0x24): `StopPlayingSoundEffect(2 G_Fire_01, owner 0, InGame)`
  0x793D48 and `(12 G_WaterFlow, 0, InGame)` 0x793D59. Entering has no audio call of its own (0x553E10..0x55405E;
  `Temple::InitEngine` 0x793C60 does not touch the audio).
- **fn_00427200** (the game's 3D function that LHaudio calls at 0x1001438C, 0x1001487B and 0x10014B91): each coordinate
  with |v| > 5000 becomes 0 (`fabs; fcomp qword 5000.0` 0x8C49E0, `test ah, 0x41`: equal, less and NaN stay), in the
  channel's stored point (+0x50, 0x427222..0x42726D, after copying it for the default case) and in the point
  it returns (0x427349..0x42738E); the distance it returns (0x427399..0x427400) is that of the point **without** the cap plus
  the offset to `g_camera`. `audio::GuardSoundPoint`, in `sample_play::UpdateChannels` (LHSampleUpdate3DChannels
  0x10014310: it stops the channel if that distance is not less than the maximum +0x6C, 0x100143AF; otherwise,
  LHSampleSet3DPosition with the capped point) and in `audio::OwnerSoundPoint` (the start of the anim-effects).
- **HelpSystem** `ReadSpeedFactor` fn_005C6CB0: with the game thread's FPU at 24 bits (fn_007DEE00) each step
  rounds to float (the qword constants 0.5, 1, 0.80000000000000004 and 0.20000000000000001 whole, the dwords 4 and 3);
  it returns float; a NaN goes to the first branch (`fcom`, `test ah, 0x41`). E.g.: 0.1 → 2.5999999 (not 2.59999999404);
  0.7 → 0.68000001.

**API**: `audio::LeaveCitadel()` and `audio::ProcessCitadelTurn()` (Audio.h), `audio::GuardSoundPoint(p)` (AudioSystem.h),
`GameQueries::insideCitadel`, `GameQueries::localPlayerAlignment`; `GameMusic::GetCitadelSamplesStopped()` ([0xC56164], for the tests).

**Tests**: `GameMusicTest.CitadelMusicInsideTheCitadel`, `GameMusicTest.CitadelMusicWithoutItsBank`,
`GameSfx.GuardSoundPoint`, `SamplePlayTest.TrackedPointGuardedAt5000`, `HelpSystem.ReadSpeedFactor` (values at 24 bits),
`AudioManager.EachTurnReachesTheServiceAsItsOwnCall`, `PausedTurnTimer.*` (test_game_clock).

**The world pauses inside the citadel.** In the original:
- Entering (`GoInsideCitadel`), the pause flag is kept (g_game+0x2502C4 = bit 2 of g_game+0x14, 0x553F61..0x553F72) and,
  in a single player game, `PauseGame(1)` (0x553F83). Leaving (`LeaveInsideCitadel`), after the sequence mode 0
  (0x553B1F) and fn_00793D00, `PauseGame(g_game+0x2502C4)` (0x553B44..0x553B4B) gives the kept flag back. `PauseGame`
  stops the game timer and starts it again from now, so the world goes on from the turn it stopped at, without catching
  up.
- While paused, the world's turns wait (`LocalTimerSaysDoATurn` says no). In sequence mode 1 (or with the paused
  cinematic), `ProcessNetworkPackets` (0x54CC66..0x54CCFF) has a turn every 100 ms of `GetTickCount`: the static
  [0xCD3AE8] is set to now the first time (0x54CC98), then a turn is due once now − it > 100 (0x54CCA9, `jbe`), which
  moves it on by 100 (0x54CCB0), or to now when it is still more than 200 behind (0x54CCC1..0x54CCCA). It is never
  reset. In sequence mode 1 that turn is `Temple::ProcessGameTurn` 0x794A30 (0x54CCFF).
- `Temple::ProcessGameTurn`, only while paused (0x794A35): `GDebug::ClearMessages(-1)` (0x794A42),
  `ScriptDLL::LookIn(0x18)` (TempleHelp | TempleSpecial, 0x794A4F), `ProcessAudioGameTurn` (0x794A5A) and
  `HelpSystem::Process` (0x794A6F: the hand state, the icons, the spirits, the tooltips, the bubble, the conditions and
  the banter), only with a help system (0x794A6B). The paused end of turn (`AtmosProcess(0)`) goes on in between.
  openblack: `temple_turn::Process` (`src/Temple/TempleTurn.h`) from `Game::ProcessTempleTurn`, in that order, the
  room's tooltip (ours) before the help's turn (where the original submits the room's tooltip against
  `HelpSystem::Process` is not read); tests `TempleTurn.*`.
- **Escape** (the temple's key handler fn_00794830, from `GGame::ProcessKey` 0x63F295, read 2026-10-22): Escape with
  no modifier leaves from the save game room while the game keeps the player in a room ([0xBEC284] not 7), else goes
  to the main room from any other room, else leaves; other keys, and Escape with a modifier, go to the key bindings
  (fn_00470A00, fn_00794900), then the room's vt+0x1C. [0xBEC284] is the room the game keeps the player in: 7 none
  (`InitStaticsValues` 0x54A926, `LeaveInsideCitadel` 0x553AA0), 3 the multiplayer citadel (`DoCitadelMultiplayer`
  0x555580), 5 after the game is over (the game-over requester's callback sub_54B1A0, its button 0x2710, then
  `GoInsideCitadel(5, 0)`). While it is set, `GoToRoom` to another room sets `Temple::LeaveCitadel` (0x793DA9) and
  `LeaveInsideCitadel` sets g_game +0x250180 = 6 (0x553A90), which ends `pc_main`'s loop. openblack:
  `temple::OnEscape` (`src/Temple/TempleKeys.h`), with no kept room (the game-over requester is not ported); tests
  `TempleKeys.*`. ⚠️ openblack takes Escape with Shift or Ctrl too (`Game.cpp`), where the original sends it to the
  key bindings.
- The tooltip's icon is kept while paused inside the citadel (fn_005C9D00 0x5C9D4A, `cmp g_game+0x205A28, 1`), and the
  helpers' frame step (fn_005557E0 0x5557E5) is the real frame time in sequence mode 1.

openblack:
- `TempleInterior::Activate` keeps `game_clock::IsPaused()` and calls `game_clock::Pause(true)` before the sequence mode
  1; `Deactivate` gives the kept flag back after the sequence mode 0. `game_clock::Pause` is `PauseGame`'s: the timer
  stops and starts again from now, the turn number stays, and there is no burst of turns on leaving.
- `Game::Update`, while paused in sequence mode 1: `game_clock::PausedTurnTimer` (the rule of [0xCD3AE8], on
  `game_clock::TickCount()`, so the fixed step makes it deterministic), then `Game::ProcessTempleTurn`:
  `LookIn(TempleHelp | TempleSpecial)`, `audio::ProcessCitadelTurn()` (ProcessAudioGameTurn alone: no sound map, no
  sound tags, no turn 5 gate), the room's tooltip (`TempleInteriorInterface::ProcessTurn`, its Submit), `help::Process`
  and the `OPENBLACK_AUDIO_TEST_CITADEL` hook. The tooltips age once a temple turn, and `help::Process` does not run
  from the world's turns inside.
  Not ported: `GDebug::ClearMessages` and the paused cinematic. Entering from the frame's key pass ends no turn
  paused, so the ambience is not cut there; inside, only the temple's own turn drives the audio.
- `help::tooltips::ProcessTurn` keeps the icon while paused inside the citadel; the spirits' frame step reads
  `game_clock::IsInsideCitadel()` (the sequence mode) rather than the temple being active.
- **✅ read 2026-10-24** The escape menu, opened inside the temple by its options room, does not pause or unpause the
  game: the temple holds the pause. In the original the menu's pause is the main menu's: its callback sub_540040 runs
  `PauseGame(1)` as it shows (code 6, 0x5400C1, single player only) and Continue `PauseGame(0)` (0x53FF07). Inside
  the citadel the main menu never shows: the options room shows the Options page itself (above), a tab shows the main
  menu only outside the citadel (fn_0053F3E0, `cmp g_game+0x205A28, 1` at 0x53F41F), and Escape goes to the temple's
  own handler (0x63F295); no options page's callback (`DialogBoxOptions::ControlCallback` 0x5140C0 and the leave
  fn_005148B0) calls `PauseGame` (xref callers of 0x54AE20). openblack: `gui::StepMenuPause` (pausing as the menu
  opens unless inside the citadel, giving back only its own pause); test
  `GameMenu.TheMenuPausesTheGameAsItOpensAndContinueUnpausesItButNotInsideTheTemple`.
- What is audible: the citadel's music and filters on the temple's 100 ms turns, and nothing of the world, which is
  paused (no world turn: no sound map, tags, villagers or animals), as in the original.

#### Audit: citadel interior

Reviewed against the disassembly: the whole of fn_00427200 (0x427209..0x42726D the uncapped copy and the cap of +0x50,
0x4272AD / 0x4272E1 the flag 0, 0x427321..0x42738E the returned capped point, 0x427399..0x427400 the uncapped
distance + offset), LHSampleUpdate3DChannels 0x10014310 (flag 0 → LHSampleStop and +0x18 = 0; `fcomp` +0x6C,
`test ah, 1`: a NaN does not stop; LHSampleSet3DPosition with the capped point), LHSampleSet3DPosition 0x10013AC0 (the QMixer
source at point + offset 0x10013E76..0x10013E99, +0x50 = the point 0x10013EBA), LHSamplePlayAnimEffect
0x10014B91 (the returned point goes into the options), ProcessCitadelMusic 0x427B60..0x427C8F (options +0x00/+0x04/+0x14/
+0x1C/+0x20/+0x24/+0x28; the bank it checks, GAudio+0xDC + 4·index, is that of type 44 + index), 0x4282F0
(`== 1`), ProcessMusic 0x427DF0..0x427EBB, 0x426C80, 0x64D6A0, 0x429F6D, 0x42A554, Temple fn_00793D00
(0x793D3C..0x793D59), StopPlayingSoundEffect 0x42A210, LeaveInsideCitadel 0x553B1F / 0x553B25, GoInsideCitadel
0x553E10..0x55405E (its calls: none for audio; fn_00463A50 → fn_00469E70 is for the citadel, not for sound),
fn_005C6CB0 and fn_005C61B0. Everything matches the code; nothing to fix.

- `ReadSpeedFactor`: the team's rule (step in double, result to float) could round twice in the qword `fmul`
  and `fadd`; checked **exhaustively** for the 2^23 floats of the second branch (b = 2 − 2r is exact; the
  double rounding only differs if the double falls on a float midpoint: none does) and the first branch is a
  single rounding (3 − 4r is exact in double): ✅ faithful for every READ_SPEED.
- No new AL sources or buffers, no ECS dependencies in `src/Audio`, no new music thread (`LHMusicPlay`
  from the game turn, like the other branches); the latch [0xC56164] is global as in the original.
- In game, measured before the world paused inside the citadel (to measure again: inside, the world's samples no longer
  start at all, and the hook's 80 turns inside are the temple's) (Land 1, `OPENBLACK_AUDIO_TEST_CITADEL="100,180"`, traces AUDIO/SFX/MUSIC/ATMOS): on entering `LHSampleStopAll` stops the
  tree creaks on channels 0, 2 and 4; inside no sample starts (all `filtered (inside the citadel…)`),
  citadel.sad (CITADEL_NEUTRAL) rises 4 → 127 while intro.sad goes down; on leaving `SFX: stop InGame.sad/2` and `/12`
  owner 0, `MUSIC_TYPE_SCRIPT_INTRO` comes back from chunk 1 abruptly (fn_00427CA0 passes fade 0, 0x427D41) and
  citadel.sad fades out, the ambience loops start again and the creaks sound again.

#### The temple interior's sounds

All in InGame.sad, all user parameter 2 (they pass the citadel's filter), all 2D and without an owner but where said.
The code: `3D/TempleSounds.h`, `TempleDoors`, `TempleToggles::ClickOf`, `TempleScrolls`, `TempleInterior::HoldControl`.

| Event | Sample | Call | Mode, loops | Where |
|---|---|---|---|---|
| A doorway starts to swing open (its swing goes from ≤ 0 to > 0) | 60 G_CitadelDoorOpen_01 | `PlaySoundEffect(0, 60, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794DAE (fn_00794D30) |
| A doorway turns round to close / is shut at once | 61 G_CitadelDoorClose_02 | `PlaySoundEffect(0, 61, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794FED (fn_00794FB0), 0x794F8D (`InnerRoom::FastCloseDoor`) |
| A scroll starts to turn | 54 + GetTickCount() % 6, G_ScrollSqueak_01..06 | `PlaySoundEffect(0, 54 + t % 6, 2, 0, 0, 0, InGame)` | 2, 0 | 0x78429E (challenge), 0x7894C3 (creature), 0x78B64C, 0x79201E (save game) |
| The camera starts to look at a scroll (InnerCamera state ≠ 4) | 46 + (GetTickCount() & 3), G_Woosh_01..04 | `PlaySoundEffect(0, 46 + (t & 3), 3, 0, 0, 0, InGame)` | 3, 0 | 0x7957A6 (`InnerCamera::FocusOnSubMesh`) |
| A map button turned off / on | 63 G_CitadelButtonDown_01 / 62 G_CitadelButtonUp_01 | options: bank InGame, is3D 0, track 1 (the default), owner 0, mode 3, pitch below | 3, 0 | the ten callbacks at 0xC2A7A8 (0x79E940..0x79EDA0) |

- The scrolls' squeak: the flag (0xDD0BE8 in the challenge room, 0xDE0FA4 in the creature room) is cleared when the mouse
  does not move **and** when the clamped position does not change (0x7842AB), set after a squeak; a squeak only while
  it is clear. The pick is the clock, not a random stream: no random draw.
- The buttons, by their flag and tooltip: Temples (0xC2A4EC, 0xE91) 100 / 100; Creatures (0xC2A4F8, 0xE8F) down 110,
  up: 110 is written to the options' **owner** (+0x20) and the pitch stays 100 (0x79EAD5), an original slip, kept
  (`Owner::Key(110)`); Miracle activity (0xC2A4F0, 0xE93) 95 / 95; Influence (0xC2A4F4, 0xE92) 105 / 105; both its
  callbacks set `WorldRoom::ShowInfluence` (off 0x79EC77, on 0x79ECEB), which gates the circles over the map
  ([inside the citadel](../magic/influence-and-alignment.md#the-original)), and the on one calls fn_0079DCD0;
  Challenges (0xC2A4FC, 0xE90) 108 / 108. The
  .sad has no pitch override for 62/63, so the caller's pitch plays (with deviation 0).
- From the .sad: 54..59 deviation 10 and mode 2 (0x400), 60/61 mode 2, 62/63 mode 3, 46..49 pitch 90 (0x1) deviation 10.
- The doors, buttons, scrolls and the scroll woosh play as in the table above. The creature room plays too (`CreatureRoom::DrawAdditional` 0x78869A 175
  G_FireCreatureCave at the fire's point [0xC5E454] and 0x7886E0 177 G_WaterCreatureCave 3D at (160, −45, −30), both
  options with track 0 and owner 0, every frame the room is drawn; its vfunc 11 0x7871D5 / 0x7871EC stops them): in
  openblack `TempleInterior::UpdateCreatureCave` plays both every frame the room is drawn and stops them once it is
  not. The challenge room's woosh (`ChallengeRoom` 0x782486, fn_00782410, a challenge chosen, 46 + tick & 3, mode 3,
  2D) plays through `ChosenWoosh` as a challenge is chosen, and the heart's sparks 206 + c (0x468815, 0x468B32) play
  from the temple destruction system's two beams ([above](#counterparts-in-the-code)). The worship strain (`Citadel::SetWorshipStrainSoundFrac` 0x463850) is not
  silent: it plays no sound itself and is already ported and connected (worship site remarks). Who plays 2 / 12 with owner 0 inside the temple (what fn_00793D00
  stops) is not in the inventory **(inferred: nobody in the original; the stop stays the same)**.

### Which rooms are open in openblack

- `temple::IsRoomAvailable` (`src/Temple/TempleRoomAccess.h`, raffclar's, tested in `test_temple_room_access`): the
  single player rules above. `TempleInterior::_roomsOpened` is set while the main room is drawn (`IsRoomDrawn(Main)` in
  `Update`), per game. `GoToRoom` and `Activate(room)` refuse a closed room; the door click in
  `TempleCameraModel::UpdateOrbit` refuses it and calls `CHLApi::TellRoomNotAvailable`
  (`help::script_control::RoomNotAvailable`), which nothing holds back, as the rooms' help. The
  temple debug window shows the flag and can set it, and the testbed's "open the temple's rooms" command
  (`Kind::OpenTempleRooms`) calls the same `TempleInteriorInterface::OpenRooms` for the scenarios that go straight into
  a room on the first visit (unverified against v1.42).

### Test hooks

- `OPENBLACK_AUDIO_TEST_CITADEL="<enter>[,<leave>]"`: enters the temple interior at hook turn `<enter>` and leaves
  it at `<leave>`, like `ENTER_EXIT_CITADEL(1)` / `(0)`; listed with the other audio hooks in
  [the audio engine's test hooks](../audio/engine.md#test-hooks).

## Pending

- The close look ([the way to a close look and back](#the-challenge-rooms-controls-and-its-cameras-blend)): in the
  original the look starts the frame after it is asked in (through the asked state); openblack starts it the next
  time the camera steps, and the order of the temple's update and the camera's in a frame is not compared. What ends
  it is the argument [esp+0x78] of `UpdateMain` (a press, from `TempleRoom::UpdateMouse`, whether held or new not
  read) or actions 7 to 10; openblack ends it on a new press or the arrow keys. The main room's camera
  (`WorldRoomCamera`) is not seen to follow its scroll's look, where openblack moves the drawn camera toward it along
  the cosine; the creature room's camera mixes its own aim (0x78A26E), where openblack mixes the drawn camera; neither
  is read through yet.
- The map in the pool from the land's pages: the original copies each block's low-resolution tile, every 8th
  texel, transposed, 8 × 8 a block (`BuildMapTex` 0x797F10, [the map in the pool](#the-map-in-the-pool)); openblack draws
  the blocks' own textures from above (`Renderer::DrawTempleMapPass`). Best after the prints are baked into the
  tiles (#222), when the tile holds what the original's does.
- The temple's depth fog: whether the hand (drawn through `CHand::Draw` inside the room's draw) goes through the mesh
  draw's first-vertex fog has not been read, and openblack leaves it unfogged (the pool, the map and its markers are
  read: [what is not fogged](#the-depth-fog-and-the-temples-light)); only the second of the mesh draw's two passes fogs at the first vertex (0x80A8D1, its counter
  [esp+0x24]), and which submeshes the first pass draws is not read; a submesh of type 2 drawn over another (0x80A84C)
  may be fogged twice; fn_0046A680's `ApplyFog` is not read. The fog's per-vertex arithmetic runs in the vertex shader
  in 32-bit floats, its twin `temple_fade::AtVertex` is tested on the CPU.
- When a script ends its camera while the player is inside the temple, raffclar gives the script camera the player
  came in from a camera of the player's own (his `ReleaseOutsideScriptCamera`, #226). Ours keeps the script camera
  as a mode (`script_camera::End`), not a model the temple stores; the exe's camera modes inside the temple are not
  read. Not ported.
- A scroll looked at through the testbed's `LookAtScroll` (not a click) starts no help, as only the scrolls' click
  callbacks start it in the exe.
- Which rooms are open: the room the game keeps the player in once it is over ([0xBEC284] 3 and 5) is not kept; the challenge room's network
  session check (+0x94 == 1) is taken as true; whether the keys and the direct entry are refused by the exe as by
  `GoToRoom` / `Activate(room)` is not read (raffclar's rule); `_roomsOpened` is not saved.

- The flag g+0x14 bit 0x400000 («always do a turn», **🔍 inferred**), the second `ProcessNetworkPackets` after drawing
  if g+0x205D58 (0x54D3C9) and the cinematic while paused (0x54CCCF..0x54CCEB): they do not exist in openblack. The
  temple's own 100 ms turn while paused (0x54CC88..0x54CCFF) is `game_clock::PausedTurnTimer` and
  `Game::ProcessTempleTurn` ([The citadel interior in openblack](#the-citadel-interior-in-openblack)).

- [The temple interior](#the-temple-interior), what is missing:
  - The hand's placement inside the temple is ported for its distance and scale (2026-10-24,
    [the temple state's distance](../hand/states.md#the-temple-states-distance)); its up and clip are not. In the
    original, `TempleRoom::Draw` 0x799FE0 casts the cursor into the room (`InnerCamera::RayCast` 0x795980, called at
    0x79A199) and on a hit writes the point to [0xE360C8..0xE360D0] and InnerCamera+0x440 to [0xE360B8..0xE360C0],
    then copies [0xE360C8] to [0xE360D8] (0x79A1EB..0x79A208) **(inferred: the hand's point)**; the hand's state
    there is `HandStateCitadel` (`Enter` 0x5B0C10, `Update` 0x5B0D00), read up to its place.
  - CitadelReflections and CitadelLightmaps: the original draws the main room's reflection only with both options
    on; openblack always draws it, which is the same only at the default settings (queued fix).
  - [The challenge list](#the-challenge-list): an UPDATE_SNAPSHOT or UPDATE_SNAPSHOT_PICTURE of a challenge with no
    record makes one whose unpopped fields (UPDATE_SNAPSHOT's camera, both functions' quest) are whatever the native's
    stack frame held: not determinable. openblack's record then has no camera and is no quest; the shipped scripts
    always take a SNAPSHOT of a challenge they update, so only the order at run time could reach it. The alignment
    history (`CAlignmentHistory::Add`) is not kept, as elsewhere. The room's pictures (fn_00784690, the
    `screenshots_lores/hires` files) are kept in memory and drawn by the challenge room (raffclar's #381), not written
    to files.
  - The challenges' pictures and reminder (raffclar's PR #378) are **unverified against v1.42**. His notes on them:
    - Deviation (his deviations list, row 50, presentation, taken): the original only ran at 4:3
      (`ScriptedScreenShot::Process` 0x821750); on any other screen shape the picture is sampled from the largest 4:3
      part of the middle of the screen (`challenge_pictures::PictureRegion`), so it looks as it did at 4:3. Exact at
      4:3, which is what every check runs.
    - Vanilla bug (his row 40, kept): UPDATE_SNAPSHOT_PICTURE passes no reminder script and no values, and the
      update copies them in, so a picture update empties the reminder; a later tap on its scroll starts a script with
      no name. openblack starts nothing then (the VM would log an error for the empty name at every tap).
    - Vanilla bug (his row 41, kept as silver): a record first made by UPDATE_SNAPSHOT_PICTURE takes its quest flag
      from a stack word the native never sets; ours is no quest.
    - His row 42 (a vanilla bug, open in his list) is not reached in the original (read 2026-10-22): records past the
      21st write their pictures to `hires_10/11…` and `lores_2/3…`, but nothing draws them (only 21 picture controls;
      `fn_00784570`'s one caller has k ≤ 20 and `fn_00784640` runs only for the slot looked at, set by a click on one
      of them) and `CopyChallengeFiles` 0x784A30 copies exactly the 21 slots' files, so no material is indexed past
      its end. openblack makes textures only for the records, drawing the first 21.
    - bgfx keeps one screenshot request per frame buffer and frame: a next-frame challenge's picture asked in the
      same frame as a check's own screenshot would take the place of one of them. No check runs a snapshot, so it
      never happens there; a kept frame is read back from its own target, not the window. Two snapshots in one script turn share the
      frame on the screen (`challenge_pictures::Ask` with `ShownFramePicture`), as the original's, which reads the
      screen at once for each ([the pictures](#the-challenge-list)).
  - [The challenge room](#the-challenge-rooms-controls-and-its-cameras-blend), from raffclar's #381, **unverified
    against v1.42**: the textures' upload is his reading of the exe, not re-read (the close look and the tooltips are
    read, 2026-10-22); text scaled past 800x600 uses the small glyphs (his sharp glyphs are
    a port row of their own); a replayed record with no reminder name (vb#40) moves the camera but starts nothing, as
    the scroll's tap does.
  - The Creature Cave's mist domes are lit by the land at their point, off the map, as the original's
    ([the creature room's effects](#the-creature-rooms-waterfall-and-effects)); whether openblack's haze inside the
    temple is the original's fog there is not read.
