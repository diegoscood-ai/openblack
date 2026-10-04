# The Land 1 intro and the tutorial's script side

How the original runs the Land 1 intro (the challenge script `FollowUs` and what it calls) and what openblack has of
it. Session Intro, 2026-10-03/04. The research, with every address, is in `dev\documentacion\intro\spec_*.md`; this page
keeps the state and the facts others need. Marks: **(inferred)**, **(approximate)**, **(pending)**, **(not ported)**.

- [Flow](#flow)
- [State in openblack](#state-in-openblack)
- [Dialogue texts](#dialogue-texts)
- [The advisor spirits](#the-advisor-spirits)
- [Script look-ups: CALL and CALL_NEAR](#script-look-ups-call-and-call_near)
- [Interface interaction levels](#interface-interaction-levels)
- [Script highlights](#script-highlights)
- [Timers, help events and field of view](#timers-help-events-and-field-of-view)
- [The family in high detail (SuperVillager)](#the-family-in-high-detail-supervillager)
- [JC specials and the confirmation sounds](#jc-specials-and-the-confirmation-sounds)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## Flow

`LandControlAll` (challenge dump `rt_chl_code.txt` 171106) runs `LandControl1` (74956), which runs `SetupLand1` and,
when the tutorial is not skipped (`IsSkippingToCreatureSelect == 0`), `FollowUs` (49528..53196):

1. 49619..50103 the family is created (mother 4/49, father 4/53, son 4/52), `SET_GAME_TIME 7.3`.
2. 50104..50134 the cinema starts: `START_CAMERA_CONTROL`, `START_DIALOGUE`, `START_GAME_SPEED`, `SET_WIDESCREEN 1`,
   `START_MUSIC 54`, fade in.
3. 50141..51024 the kiss, the son and the sharks, then `SET_AVI_SEQUENCE(1, 1)`: `INTRO.bik` (video.md).
4. 51027..52153 the rescue, the welcome crowd, the texts and the advisors (4431..4437, 5189..5192).
5. 52154 `RUN Drag` (12627..12722): `PLAY_HAND_DEMO(279, 1, 0)` with `Data\HandDemo\drag.hnd`, three
   `HAND_DEMO_TRIGGER` waits and `IS_PLAYING_HAND_DEMO` (it pushes `!IsPlayBack(0)` 0x6FDB9A: wait for the end).
6. 52172..52175 the hand-over: `SET_WIDESCREEN 0`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`.
7. After it: the player follows the family, the citadel (`BUILD_BUILDING` 52474, `CALL_NEAR(Citadel)` 52490), the
   second cinematic from 52520, then `TunnelOfUltimateDoom`, `TeachRotate` / `TeachPitch` / `TeachZoom`, `GiveFood`,
   `CheckCitadel`, and `CitadelGuide` (33817). Full tree: `spec_second_cinematic.md`.

With the mod `game.skip-intro` on (the default) `FollowUs` never runs: `SetupLand1` sets the skip from
`CAN_SKIP_TUTORIAL` (25397..25403) and the mod's answers are 1..3 (mod-library.md).

## State in openblack

| Part | State |
|---|---|
| Steps 1..6 above | ported and checked in game (`--mod game.skip-intro=off`): the hand-over comes about 4 min after the start (d5dae62e) |
| Dialogue texts | drawn (below) |
| Advisors | drawn and moving (ae5b5287); voice tags not fed (pending) |
| Villagers' focus, override animations, flocks, dance, CALL_IN | session Personas (pending) |
| Citadel as a plan, INSIDE_TEMPLE, SET_INTERFACE_CITADEL | session Edificios |
| Family in high detail, JC specials | Hito 3 (drafted) |

## Dialogue texts

HelpSystem+0x14 is a HelpText (`Help/HelpTextDisplay`): a ring of 6 texts, the newest sliding in over 333 ms
(+0xA8 += dt × 0.003, fn_005CC760), older ones shrinking (×0.86, then ×0.93) and dimming (alpha − 20). The box is a
full-width strip above the bottom bar (fn_005C57B0: bottom = H − margin − bar − 1, top = bottom − trunc(0.126667 H)),
black at alpha 0x80 (fn_005CCE60), no texture. Fonts by narrator (fn_005CCEA0): 2 good spirit → f1 in RGB(235,235,183),
3 evil spirit → f3 in RGB(255,180,180), others j0 white. Line height H / 30. One splitter for the display and the word
count (`Help/TextSplitter`, fn_005CB590). Drawn by `Renderer::DrawHelpText` between the bars and the film (callback
0x5CD020, priority 20000), from the pre-draw `graphics::OverlayFrame`. Texts never expire: only GAME_CLEAR_DIALOGUE /
GAME_CLOSE_DIALOGUE / END_DIALOGUE or a single-line RUN_TEXT clear them; the reading time only drives TEXT_READ. The
"Continuar" click cue is a KMIcon (Draw3D 0x5C59D0; session Mano's `help::kmicon` replaces openblack's text-only cue).
All layout steps are float (the FPU runs at 24 bits, fn_007DEE00).

## The advisor spirits

Two `HelpDude`s (0x5B8F00..0x5C2800) driven by `HelpDudeControl` (HelpSystem+0x10): dude 0 the good one
(`Data\HelpSprite\markgood.hd`, 97 bones), dude 1 the evil one (`markevil.hd`, 73 bones). A `.hd` holds a plain L3D0
mesh and 80 clips in CAnim format (not `.anm`: rotations are YXZ Euler angles, layers additive; checked against the
exe's functions in an emulator). Ported in `Help/HelpDudeFile`, `Help/CAnim`, `Help/Spirits`, `Help/SpiritsRuntime`,
`Graphics/RendererSpirits`.

- They float 7..9 units in front of the camera in "hover" coordinates (x and y over half the screen width, clamped
  ±0.75 / ±0.6), moving on cubic splines in time (1.0 s for eject, home and fly) and a potential field (rest zones at
  ±0.66, the partner, the mouse, the screen centre).
- Drawn after the scene in their own view (`RenderPass::FinishFrame3D`, depth cleared) while their in-world blend
  +0x35DC < 0.5, else in the world; alpha fades +3/s in, −2/s out; halo (good only) and puff from `smoke.raw`, the
  rainbow trail.
- No random idle gestures exist: gestures and emotions come from the voice samples' cue labels (pending: not read).
- The mouth: vowels 7/8/9 from `audio::advisor`'s lip-sync key.
- CHL 7, 8, 9, 10, 100, 137, 138, 139, 140, 165, 166, 167, 300, 301, 418 (spec_spirits.md §1.4).

## Script look-ups: CALL and CALL_NEAR

CALL 026 / CALL_NEAR 051 (0x6F0EB0) pop the flag, (the radius), the point, the subtype and the type; the type table
0xC0C728 picks the finder: `map_cells::FindNearForScript` with radius 1.0 for CALL, `GetNearestTown` (10.0 or the
radius) for towns, the creature list for creatures; marker, dance, flock, influence ring and weather thing have none.
An object's script (type, subtype) is `ecs::script_type` (vt +0x4E8 / fn_006F6D00): subtype 5000 means any; the citadel
heart is type 18. The 13 look-ups of the intro resolve (audit_call_interaction.md).

## Interface interaction levels

SET_INTERFACE_INTERACTION 063 (0x70B220, 16 levels): GInterface+0x28's limit bits (`interface_active`), CameraHelp's
feature mask (`camera_help`, storage only: the camera does not read it yet, pending), the hand's reach (75 for
JUST_GRAB, else 1800: the hand's `SetHandReach`), the two key switches; level 9 logs "Unexpected interaction".
GInterface::SetActive (0x5CEDC0): the script's wide screen makes the interface inactive (0x5C6AF4), a hand demo active.

## Script highlights

`ecs::script_highlight` (CREATE_HIGHLIGHT 272, HIGHLIGHT_PROPERTIES, SET_DRAW_HIGHLIGHT, ProcessHighlights 0x70A460 at
turn step 0x54E6D5): the bronze / silver / gold scrolls and the Did You Know sign (info rows at 0xD96390 in the W120
order 0 bronze, 1 sign, 2 silver, 3 gold: the CHL enum and openblack's Enums.h order are wrong for BW1), script type
37. GAME_THING_CLICKED 016 compares GInterface+0x45C, the hand's tap memory (15 s). A script-created highlight is
deleted when its last reference goes, unless RELEASE_FROM_SCRIPT released it (0x70F600..0x70F670).

## Timers, help events and field of view

- Script timers 145..148: a ScriptTimer (script type 0x11) keeps the turn it was set and its length in turns; the time
  left is computed when asked and never below 0. It is the only class deleted when no script variable holds it
  (0x561300). The countdown (084/087/092/099/103/144) ticks once a turn (0x6EB6BA).
- GET_TOTAL_EVENTS 237: HelpProfile's 49 counters (g_game+0x250060), each at most once a turn, not while paused or
  while a script holds the wide screen; reset by HelpProfile::Process (0x5C4660, after HelpSystem::Process). Writers:
  the camera 25..30 (the player's camera model), casting 9..22, the hand 1..8 (session Mano), 5/6 (session Edificios),
  the creature (pending). API `help_profile::Trigger(Event)`.
- GAME_THING_FIELD_OF_VIEW 011 / POS_FIELD_OF_VIEW 012: `Graphics/RegionOnScreen` (sphere: LH3DBoundingBox::
  CheckRegionOnScreen 0x868C80; point: fn_0081F1D0); both false inside the temple.

## The family in high detail (SuperVillager)

SET_HIGH_GRAPHICS_DETAIL 290 (0x708CE0) makes a villager a SuperVillager (fn_00825F20): its mesh is swapped for the
high-detail one — `Data\MISC\Intro\nors_man.l3d` (501, father), `nors_woman.l3d` (498, mother), `nors_boy.l3d` (439,
son and every Celtic / Norse male child), `Data\MISC\sable.l3d` (420, the creature trainer, info.dat row 77) — with
22 bones identical to the villager meshes', so the villagers' own clips drive them unchanged. Its own draw path
(fn_008254A0 → fn_00825530): a 300 ms clip cross-fade, a second yaw smoothing at 5π/4 rad/s (turned on a local copy of
the drawn matrix, 0x8255AB), the default sun as light, no haze, and the eyes: one `eye_ball.l3d` drawn twice and four
lids on bone 8, blinking every Random(1000..5000) ms (the CRT stream) with a shared glance. Released every frame unless
a script holds the wide screen (0x5E4B3A). THING_JC_SPECIAL 349 gives the cinematic's orders (7 follow the intro
hand's grip, 8 snap, 9 mirror the yaw, 16/17 ±π/2). Drafted for Hito 3 (`ECS/SuperVillager*`, spec_hd_models.md).

## JC specials and the confirmation sounds

PLAY_JC_SPECIAL 326 in the intro: 0 the light falling from the sky on the son (20 sprites from 4000 to 10 units at
0.45 u/ms), 1 / 2 the engine's debug camera following it, 4 / 5 the intro hand (hand_intro.l3d) and the son in its
grip, 14 / 15 bookmarks on / off, 18 nothing, 6 the missionaries' boat (water.md). START_ANGLE_SOUND 285 / 348 are
GConfirmation (yes / better samples of HelpSprites); silent until openblack's camera feeds them (pending). Drafted for
Hito 3 (spec_jc_specials.md).

## Pending

- The advisors' voice-tag gestures (WAV cue labels), the pupils, the eye-bone scales, the anim sounds.
- The camera feature mask's effects, the angle / pitch sound feeds (the player's camera is not CameraModeNew3).
- The second cinematic's villager opcodes (session Personas), the citadel plan (session Edificios).
- HelpSystem::Process 0x5C8FE0 as one function (today its parts sit at its turn step: tooltips, icons).

## Test hooks

- `--mod game.skip-intro=off` runs FollowUs; `OPENBLACK_TEST_TEXT_CLICK=1` clicks the texts that wait for a click;
  `OPENBLACK_TEXT_TRACE=1` logs the texts; `OPENBLACK_HAND_DEMO_TRACE=1` the hand demo.
- `OPENBLACK_TEST_TEXT_SHOT="id,path[,ms]"`: a screenshot `ms` (1000 by default) after text `id` is shown, e.g.
  `4431` ("Saludos.", both advisors on screen).

## Sources

`dev\documentacion\intro\`: spec_text, spec_spirits, spec_spirits_motion, spec_misc_ops, spec_demo_mode,
spec_second_cinematic, spec_timers_events, spec_highlight, spec_hd_models, spec_jc_specials; audits audit_spirits,
audit_call_interaction, audit_supervillager, audit_jc_specials. Challenge dump `dev\documentacion\mapa\rt_chl_code.txt`.
