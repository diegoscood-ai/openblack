# The Land 1 intro

## Summary

The game opens on Land 1 with a scripted cinematic: a family on the beach, a son taken by the sharks, the intro
film, the rescue by the player's hand, the welcome crowd and the two advisors, and then the hand-over, after which
the player follows the family to the citadel and the tutorial teaches the camera. All of it is the challenge script
`FollowUs` and the natives it calls: dialogue texts in a strip above the bottom bar, the script's look-ups of the
things it acts on (CALL and CALL_NEAR), and a few hard-coded "JC specials" (the falling light, the intro hand that
carries the son).

openblack runs the cinematic from the original script up to the hand-over (about 4 minutes in), draws the dialogue
texts and the advisors, and has the family in high detail and the JC specials ported and wired. What comes after
the hand-over (the citadel as a plan, the villagers' focus and the second cinematic) is pending.

## The original

### Flow

`LandControlAll` (challenge dump `rt_chl_code.txt` 171106) runs `LandControl1` (74956), which runs `SetupLand1` and,
when the tutorial is not skipped (`IsSkippingToCreatureSelect == 0`), `FollowUs` (49528..53196):

1. 49619..50103 the family is created (mother 4/49, father 4/53, son 4/52), `SET_GAME_TIME 7.3`.
2. 50104..50134 the cinema starts: `START_CAMERA_CONTROL`, `START_DIALOGUE`, `START_GAME_SPEED`, `SET_WIDESCREEN 1`,
   `START_MUSIC 54`, fade in.
3. 50141..51024 the kiss, the son and the sharks, then `SET_AVI_SEQUENCE(1, 1)`: `INTRO.bik`
   ([Film playback](../video/playback.md)).
4. 51027..52153 the rescue, the welcome crowd, the texts and the advisors (4431..4437, 5189..5192).
5. 52154 `RUN Drag` (12627..12722): `PLAY_HAND_DEMO(279, 1, 0)` with `Data\HandDemo\drag.hnd`, three
   `HAND_DEMO_TRIGGER` waits and `IS_PLAYING_HAND_DEMO` (it pushes `!IsPlayBack(0)` 0x6FDB9A: wait for the end).
6. 52172..52175 the hand-over: `SET_WIDESCREEN 0`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`.
7. After it: the player follows the family, the citadel (`BUILD_BUILDING` 52474, `CALL_NEAR(Citadel)` 52490), the
   second cinematic from 52520, then `TunnelOfUltimateDoom`, `TeachRotate` / `TeachPitch` / `TeachZoom`, `GiveFood`,
   `CheckCitadel`, and `CitadelGuide` (33817).

`SetupLand1` sets the skip from `CAN_SKIP_TUTORIAL` (25397..25403), that is from the answer to the tutorial requester
([Skipping the tutorial](land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)).

#### The script's game speed and its other switches

The cinema takes the game speed for one script task at a time (CHL 128 START_GAME_SPEED, 129 END_GAME_SPEED,
066 SET_GAMESPEED). GScript +0xAC holds the task that controls it; `GScript::Reset` 0x6EB2D0 does not write it (0
from the constructor, 🔍 inferred).

| Command | What it does |
|---|---|
| START_GAME_SPEED 0x6FA9E0 | when nobody or this task controls the speed, `GCamera::SetScriptSlomoControl(1)` 0x441F40 (GCamera+0x70 = 1) and the task takes it |
| END_GAME_SPEED 0x6FAAB0 | when nobody or this task controls it, `ActualEndGameSpeed` 0x6FAA60: SetScriptSlomoControl(0), `GGame::SetSpeed(1.0)` 0x5537F0, +0xAC = 0 |
| SET_GAMESPEED 0x6FAAE0 | `GGame::SetSpeed` only for the task that controls the speed (an owner of 0 matches no running task) |

- **The speed indicator.** Besides Save / Load, the on-screen speed indicator fn_005CBF10 reads GCamera+0x70
  (`GetCamera()` 0x5CC01A, `+0x70` 0x5CC01F..0x5CC028): while a script controls the speed and the game is not
  paused, the words are reset and not drawn (the other operand is the indicator's state; see
  [The game clock](../engine/game-clock.md#game-clock)).
- **A stopped task** gives the game speed back (fn_006FAA40 from the task-stop callback 0x6EC71E) and the wide screen
  too (fn_005C78C0 at 0x6EC6FA: SetWideScreen(0, 0) when that task owns it).
- **The release of the script camera** (fn_006ECD70) also Release()s every SuperVillager and empties the list
  (0x6ECEA6..0x6ECECA), and sets `GLandscape::DrawListRebuildCount` = 1 (0x6ECEC4).
- **The other switches.** GScript +0x78 (draw leashes) is written by SET_DRAW_LEASH 0x708C9D and read by
  `GInterface::DrawAllLeashes` 0x5D9382. +0x80 (draw highlights) is written by SET_DRAW_HIGHLIGHT 0x708CCD and read
  by `ScriptHighlight::Draw` 0x709C9D, which draws a did-you-know sign anyway. GScript +0x7C is written only by
  fn_006ECD70 (0x6ECEDB) and GScript::Reset; no reader found.

#### The tutorial requester's layout

The SkipBox's Init, its callback and `FrontEnd::Init` are in
[Skipping the tutorial](land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial). Its layout:

**The box.** `FrontEnd::Init` calls `SkipBox::Init(400, 290, 0x544480)` at 0x53BB95..0x53BBAC, through
`DialogBoxBase::Init` 0x513400. The box is centred at (400, 300), 400 × 290 (0x513525 / 0x51352C). Its controls are
placed in dialog space through `SetupThing::adjust` 0x411B40. `DialogBoxBase::Init` 0x5134C6..0x513507 starts two
Zoomers, both over 0.5 s ([0x3F000000]): the one at +0x34 goes to 0 and the one at +4 goes to +0xC4 (1.0).

**The texts**, read at Init. The question is entry 4 of `InfoScriptPatch2` [0xD17CA0] and the four labels are
entries 5..8; an entry past the database's count gives entry 0 (0x544214 / 0x5442D8..0x544321). The button's text
is HELP_TEXT_REQUESTER_BOXES_04 (0x5442A0: [0xD17CA8] + 0x79B0 = entry 0xA24 "Aceptar"), falling back to entry 0
when the count is 0xA24 or less.

**The controls:**

| Control | Made with | Notes |
|---|---|---|
| The question | `SetupControl(999, 150, 190, 500, 70)` (0x544231..0x54425A) | +0x23C = 4 (wrapped, centred) |
| Label i | at (290, 225 + 44 i, 300, 40) (0x5443A7..0x5443D4) | +0x23C = 3 (wrapped, left) |
| Check box i | `SetupCheckBox(60 + i, 250, 225 + 44 i, 1, i == +0x20, L"", 24)` (0x54435A..0x54437D) | the selected answer starts checked |
| The button | `SetupBigButton(11, 220, 400, main[0xA24], 32, 0, 2)` (0x5442A9..0x5442BD) | size 32, side 0, look 2 |

The selection +0x20 is set to 0 at 0x544206 and again at 0x5442C2. A click on a check box sets +0x20 = id % 10
(0x5444D1). In the callback, event 1 (a click) on button 11 gives the answer; `PauseGame(0)` follows at 0x54458B and
`DialogBoxBase::Hide` at 0x544593.

**The draw.**

- The background is `DrawBg(400 − w / 2, 300 − h / 2, 400 + w / 2, 300 + h / 2, 0xFFFFFF, 0, −1)`. It is see-through
  and drawn on every edge when +0x94 == 1 and +0x98 == 0 (fn_00408340 0x4085F4..0x4086B5).
- `SetupBigButton::Draw` 0x40CEB0 with side +0x244 == 0 (the ctor's 6th argument, 0x40D2BA) puts the label to the
  right of the 32 × 32 button. That label is left-justified at x1 + 2, its shadow ([0xC4CCDC]) is drawn first
  (0x40D10E..0x40D21A), and its size is the box's +0xAC (GetMidTextSize). The button sets no +0x20.
  [0xC4CCE4] / [0xC4CCF8] are the label's text and hover colours (🔍 inferred).
- `SetupStaticText::Draw` 0x409430 takes its size at +0x240 from +0x20 and shrinks it by one while the wrapped text
  is higher than its rect and the size above 10 (0x4094BA..0x409558). The shadow pass at (+2, +2) in [0xC4CCDC] and
  then the text in [0xC4CCE8] are drawn at 0x4095A8..0x409709.

### Dialogue texts

The script's lines appear in a strip above the bottom bar, the newest sliding in while the older ones shrink and fade.
In short (the details are in the sections below):

- **The stack.** HelpSystem+0x14 is a HelpText: a ring of 6 texts, the newest sliding in over 333 ms (+0xA8 += dt ×
  0.003, fn_005CC760), older ones shrinking (×0.86, then ×0.93) and dimming (alpha − 20). Line height H / 30.
- **The box.** A full-width strip above the bottom bar (fn_005C57B0: bottom = H − margin − bar − 1, top = bottom −
  trunc(0.126667 H)), black at alpha 0x80 (fn_005CCE60), no texture.
- **Fonts by narrator** (fn_005CCEA0): 2 good spirit → f1 in RGB(235,235,183), 3 evil spirit → f3 in
  RGB(255,180,180), others j0 white.
- **Lifetime.** Texts never expire: only GAME_CLEAR_DIALOGUE / GAME_CLOSE_DIALOGUE / END_DIALOGUE or a single-line
  RUN_TEXT clear them; the reading time only drives TEXT_READ.
- **The click cue.** The "Continuar" cue is a KMIcon (Draw3D 0x5C59D0;
  [KMIcon](../interface/tooltips.md#kmicon-the-button-or-key-drawing)).
- One splitter serves the display and the word count (fn_005CB590). All layout steps are float (the FPU runs at 24
  bits, fn_007DEE00; [Floating point](../engine/floating-point.md)).

#### Drawing order

- HelpText's callback 0x5CD020 is registered with priority 20000 (RegisterFinishFrameCallback 0x5CAD74): after the
  bars, before the film and the fade; it draws the box (fn_005CCE60), the dialogue texts (fn_005CC760) and the click
  cue (KMIcon of Draw3D 0x5C59D0..0x5C5ACE).
- The frame of that callback is fn_005CCAB0; it is drawn only if g_game+0x250188 (a front-end state) allows it.
- The script's fade colour and the bars of fn_005C5780 are drawn in `FinishFrame` 0x82F460 (e) / (h)
  ([The frame and its draw stages](../rendering/frame.md)).

#### The text database

`GSetup::LoadTextScripts` 0x719280:

- Texts come from `Scripts\InfoScript2.txt`; each ADD_TEXT goes through the callback 0x7192E0 → fn_005CCF80 →
  HelpTextData fn_005CAD00. GetHelpText gives entry 0 for an id out of range (0x5C5FAD..0x5C5FCA); count at
  HelpTextDatabase +4 [0xD17CAC].
- A second database, `Scripts\InfoScriptPatch2.txt` [0xD17CA0] (callback 0x7193F0, its own ids in file order, count
  [0xD17CA4]), is read by SkipBox::Init 0x54420F.
- fn_007191F0 also turns " ~" and "~ " (the space and the tilde together) into one 0xF8FE (0x719200 / 0x71921A).
- The language g_game+0x250080 (start_system 0x6432F9..0x6433A4): the first two characters of the game folder's
  `Country.txt` (fopen "rt" 0x643313), matched case-insensitively against the table 0xBEA970 (uk 0, us, fr, de, se,
  es 5, jp 6, nl, br, it, sc 10, tc 11, pl, kr 13, th 14); 0 without the file or a match. Read once. No file
  gives 0 at 0x643321; the two bytes are read at 0x643337, the table is walked at 0x64334C..0x643368, a match
  returns its index at 0x643399 and no match gives 0 at 0x64336A.
- `NeedsBiggerText` 0x4079C0: true for the languages 6, 10, 11, 13 and 14 (0x4079D2..0x4079E9).
- When the options close, the help's reading speed (READ_SPEED) comes from the slider; with no value in the profile,
  `ReadRegistrySettings` [0x514742..0x514755] uses fn_005C6CA0 (+0x4610).

#### The box and the stack

HelpText, 0xBC bytes, `HelpText::Create` 0x5CB090:

- Line height (+0xA0): H / 30 [0x8CF3F8], or H / 28 [0x92A53C] when NeedsBiggerText; computed once in the ctor
  (fn_005CADC0 0x5CADE2).
- The box's margin = trunc((H − 2 × bar) × 0.025) [0x92A440]; its height 0.126667 H is 19/150 [0x92A43C].
  fn_005C6BB0 recomputes the region every frame.
- The slide-in (+0xA8) only advances while the texts are drawn (the TEXT_DRAW gate fn_005C6E60 and not hidden), and
  in the citadel with g_delta_time instead of g_game_time_inc.
- Six entries are laid out while the slide-in runs, four once it reaches 0.9999 [0x8CF368] (0x5CC90B..0x5CC926).
- The newest enters from a scale of 1 / 0.93 (0x3F89A269, 0x5CC88E); each slot lerps scale and y from the previous
  slot; alpha = ftol(scale × 255), times the slide-in for the newest (0x5CC9AB..0x5CC9F9).
- Profile value TEXT_TOPTOBOTTOM ([0xD16180], 0 without a profile): the first slot at y0 = trunc(lineH × −1/3)
  instead of +1/3 (0x5CC799..0x5CC7C9) and each entry bottom-aligned.
- Profile value TEXT_DRAW (HelpSystem +0x4604, 1 without a profile; fn_005C6E60): 0 never draws, 1 draws the current
  text if its HelpTextData +4 is set, any other value always.
- A single-line text is centred with no zoom (fade only) (0x5CC8A1..0x5CC8C6). A second text clears the single-line
  flag (0x5CCF2C).
- `HelpText::Reset` 0x5CB020: with "all" the ring is emptied (text and narrator; number and colour stay); always font
  +0x8, hidden +0xAC, single line +0xB0 and box shown +0xB4 = 0.
- fn_005CB010 (GAME_CLOSE_DIALOGUE, ClearDialogueControl): box hidden (+0xB4 = 0) and fn_005CB000 (+0xAC = 1: the
  texts are not drawn).
- Data\f1 is "Footlight MT", Data\f3 "Orange LET" (table 0xECCD08); a font that did not load ([0xECCD0C] /
  [0xECCD14]) is replaced by j0 (0x5CAE3F..0x5CAE6E).

#### Word layout

fn_005CB750 measures, fn_005CB960 draws:

- The wrapped block is centred horizontally (x0 = trunc((W − widest) / 2)) and every line starts at x0: ragged right
  (0x5CBA1D..0x5CBA37). The widest line keeps its trailing space in the measure (0x5CB7F3, then the wrap at
  0x5CB8DD).
- Max lines = ftol((bottom − top + 1) / lineH); the words past the last line are dropped, with no ellipsis
  (0x5CBC5C).
- $D / $P print the entry's number with "%3.3f" (0xBF1A68) / "%3.3f%%" (0xBF1A74); the draw puts a space after it but
  the measure does not (0x5CBB89).
- The measure walks the codes too, so the draw starts with the font and colour the measure ended with (fn_005CCF50
  runs once, before fn_005CB750).
- Each word is one `GatheringText::DrawTextRaw` call at z = near × 1.2, clipped to the box's top and bottom
  (0x5CBDFE..0x5CBE7F).
- vAlign 2 bottom-aligns the block, any other non-zero value centres it (not truncated).

#### The missionaries' song karaoke

Inside the word draw fn_005CB960; read in the v1.42 exe and run in the oracle:

- The gate, once per draw (0x5CBA3C..0x5CBA62): the script music type, GAudio +0x28 (GGlobal::Global's first dword
  [0xCD3B20] is the GAudio), is 0x33..0x35, the three `MissionariesVerse1..3.sad` (MUSIC_TYPE 51..53,
  [Music](../audio/music.md)).
- Per word (0x5CBB91..0x5CBBCB), after the $D / $P number: the word is the sung one when
  `GAudio::IsUsingBouncingBall` 0x426C40, the byte [0xD17CB0] and the gate are all set and the word index (the
  counter +1 at the top of each splitter step, 0x5CBA9F, codes and empty pieces included, so the first piece is 1)
  equals GScript +0x9C, the music beat that the verse's markers count (`L<n>` sets 1, `P` / `W` add 1:
  [GScript switches](../audio/script-audio.md#gscript-switches)).
- IsUsingBouncingBall: language g_game+0x250080 ≤ 12 through the byte table 0x426C70 {0,0,0,0,0,0,0,0,1,0,1,1,0}
  (0 = yes): every language but br (8), sc (10), tc (11) and those past 12 (oracle, languages 0..15 and −1).
- [0xD17CB0] has one writer, the stack draw fn_005CC760 (0x5CCA0A): `i == 0`, the newest entry, the same value it
  passes as the draw's icon flag.
- The box (0x5CBC9C..0x5CBDFB), only for a non-empty word that was placed (after the wrap test, so on the line it
  lands on): its width again (GetStringWidth), then three Zoomers at 0xD17BE8 (x), 0xD17C18 (y) and 0xD17C48 (width),
  each `SetDestinationWithSpeedAndTime(target, 0, 0.18)` (0x3E3851EC) toward float(left + pen), (top + yAcc) +
  yOffset and the width, then `Update(GetAlexTimeInc() × 0.001)` (fn_005557E0: the frame's ms, clamped to 0..500) on
  x, y, width. Every frame re-aims them from where they are, so the box glides; a frame of 0.18 s or more lands it
  ([The Zoomer](../engine/matrices-and-zoomer.md)).
- Then fn_00447BA0(ftol(x), ftol(y), ftol(width + x), ftol(y + line height +0x9C), colour): the help's glow panel
  (the 9-slice of atmos.raw in AdditiveMaterial, [Tooltips](../interface/tooltips.md)), in the word's colour (entry
  +0x40.. and the alpha +0xA4) with the alpha byte shifted right once (0x5CBDA9), drawn right before the word.
- The Zoomers are statics made by the HelpText constructor fn_005CADC0 (0x5CAE86..0x5CAFE0): all zero but the y one's
  value, destination and start value, the screen height (TheSystem +0x1A). No other code writes or reads them.

#### The splitter

fn_005CB590, shared with the word count fn_005CBEC0:

- Blanks: space, tab, CR, LF, 0xF8FE (fn_005CB0F0); escapes '$' and '\' (fn_005CB190). A word stops at a blank, an
  escape, the end or after 47 (0x2F) characters (0x5CB6E3).
- "$$" / "\\" (any two escapes) is the second character as a literal.
- Codes (fn_005CB1D0 / fn_005CB220): a digit or one of C D F M N P (either case, byte table 0x5CB26C). $C<n> colour,
  $F<n> font (fn_005CCEA0), $M<n> the control icon, $D number, $P percent, $1 stop (fn_005CB4C0); unknown codes ($I,
  $s, $g…) only lose the escape (0x5CB54C). After $C<digits> one more character is skipped (0x5CB53A).
- $C<n> (fn_005CB400): n read as 0xBBGGRR; $C0 goes back to the narrator default (2 → RGB(235,235,183), 3 →
  RGB(255,180,180), else white).
- $M<n> only acts in the draw (flag of fn_005CB590): fn_005CB3E0 → HelpSystem fn_005C5B50(n), the control-icon
  KMIcon at the right of the box (HelpSystem +0x28).
- The blanks after a code belong to its empty piece; a word followed by blanks has type 3, by 0xF8FE type 6, after a
  line feed type 1 (fn_005CB120).

#### The click-to-continue cue

HelpSystem::Draw3D 0x5C59D0..0x5C5ACE, KMIcon +0x24:

- Made once while a text waits for the click: fn_005C78F0(1) resolves the action (no cue when it fails, 0x5C5A4F);
  animType 1 (the button blinks); text 0xE79 (HELP_TEXT_TOOLTIP_07; entry 0 when [0xD17CAC] <= 0xE79); fade 1.0 s
  [0x92A444]; c1 yellow.
- Size trunc((bottom − top + 1) / 3); x = (W & 0xFFFF) − 4 (0x5C5AA0); y = bottom − trunc(size / 2) + 1
  (fn_005C5970), moved there every frame (+0x124, 0x5C5AD0).
- Deleted by AddText (0x5C615E..0x5C6196), ClearTextDisplayed (0x5C54F1 / 0x5C550B), the used click (0x5C6A56) and,
  once a turn, fn_005C5E50 when HelpText's newest entry is empty.

#### The text functions

- TEMP_TEXT 0x6F7E40 / TEMP_TEXT_WITH_NUMBER 0x6F7F50 show the string prefixed with "*" (UNICODE_sprintf "*%s"
  0xC0D420, 0x6F7E8D), narrator 1, text id 0, and report "Development text being used in game!" (0xC0D3F8) as a
  script warning.
- AddText fn_005C6100: withInteraction 1 = wait for the click (+0x57C), 2 = no click (+0x580); the text id becomes
  current (+0x584, 0x5C615C).
- ShownLongEnough fn_005C68C0: 1 s [0x92A444] while the text waits for the click, else 0.5 s [0x915D1C]; in ms inside
  the citadel (0x5C68ED..0x5C6949), else in turns.
- ProcessInterface returns 0x14 when the click was taken by the text (0x5C6A25, 0x5C6ABA); during a hand demo
  (IsPlayBack(0), 0x5C69ED) the click does nothing.
- ClearTextDisplayed 0x5C54E0: the KMIcons +0x24 / +0x28, then +0x580, +0x57C, +0x45DC and +0x45E4 = 0.
- HelpSystem::Reset 0x5C5580 (from GScript::Reset 0x6EB340) also clears the sets' sent turns +0x78 and counts +0x2FC
  (0x98 dwords each), the banter count +0x45D4, the history (+0x45C8 / +0x45C4) and ResetIcons; it does not touch the
  dialogue owner +0x45CC.
- `HelpSystem::SetWideScreen` 0x5C6AD0, when +0x45E8 changes, also calls `DialogBoxBase::HideAll` (on) and sets the
  bars' timer +0x45F0 = wideScreenTime 0xD16174 × 1000 × the part already done (0x5C6B3F..0x5C6B4E).

#### The help system's turn

HelpSystem::Process 0x5C8FE0, in order: fn_005D7E40 (the hand state, 0x5C8FF0); ProcessIcons fn_005C5E50
(0x5C9001); the spirits fn_005C9060 (0x5C9008: HelpSpirit::Process of +8, the evil one, then of +0xC, the good one;
[The advisor spirits](../interface/advisors-and-help.md#the-advisor-spirits)); the tooltips fn_005C9D00 (0x5C900F);
the did-you-know bubble fn_005C82F0 (0x5C9020); with +0x45F8 && +0x45F4, ProcessConditions and ProcessBanter
(0x5C9025..0x5C904B).

#### Help message sets

HELP_SYSTEM_MESSAGE_SET:

- The table 0xBF0D60: 0x98 sets of 20 bytes {first, last, mode, category, script}.
- `HelpSystem::ProcessConditions` 0x5C86B0: +0x30, 18 dwords, cleared every turn, [1] = 1; nothing more while a
  script holds the wide screen; [2] = 1 when HelpProfile's AllInterface (43) was last used more than 120 s ago
  ([0x92A49C]).
- `ProcessBanter` 0x5C81C0: when condition 2 is set, RunMessageSet(GetRandomBanterSet()).
- `GetRandomBanterSet` 0x5C81E0: the n-th set of 1..25 not sent yet, n = LocalRand(25 − count) (the local stream,
  not the synced one; [Random numbers](../engine/random.md#random-numbers)); once all 25 are sent, their sent counts
  and +0x45D4 are cleared and set 1 is given.
- `SetSetSent` 0x5C8A80: +0x78[set] = turn, TriggerCategory(category), +0x2FC[set] += 1, +0x55C = set, +0x560 =
  turn, HelpProfile event HelpQuery (42).
- `RunMessageSet` 0x5C8B80: refused when StopRunningScripts refuses; mode 3 runs the set's script (RunMessage
  0x5C8C90); else GetStartAndEndTextForSet 0x5C8AE0: mode 0 the table's first..last, mode 1 one of them
  (GetRandomTextFromSet 0x5C8C10: first + LocalRand(last − first + 1)), mode 2 the thing's
  GetQueryFirstEnumText / GetQueryLastEnumText (vt +0x4D8 / +0x4DC), 0 and 0 without a thing.

### Script look-ups: CALL and CALL_NEAR

A script finds the things it acts on by type, sub-type and place. CALL 026 finds the thing at a point, CALL_NEAR 051
(0x6F0EB0) the nearest one within a radius. The type table 0xC0C728 picks the finder: the cells'
`FindNearForScript` with radius 1.0 for CALL, `GetNearestTown` (10.0 or the radius) for towns, the creature list for
creatures; marker, dance, flock, influence ring and weather thing have none. An object's script (type, subtype) is
vt +0x4E8 / fn_006F6D00: subtype 5000 means any; the citadel heart is type 18. A script's marker is type 1
(ScriptMarker 0x70D960, `mov eax, 1`, the +0x4E8 slot of its table 0x8E10B0). The 13 look-ups of the intro resolve.

- **The pops.** `CALL` = `GScript::Call` 0x6F0D60: pops excludingScripted, the point (z, y, x), the sub-type, then
  the type (0x6F0D75..0x6F0DDF). `CALL_NEAR` pops excludingScripted, the radius, the point, the sub-type, the type
  (0x6F0EC5..0x6F0F41).
- Type 1..41 only (`test esi, esi; jle` / `cmp esi, 0x2A; jl`), else "Invalid type=%d" (0xC0CB48) and 0. The table
  0xC0C728 has 24 bytes a type: +0 the AtPos finder (CALL), +4 the NearPos one (CALL_NEAR); NULL → "No find function
  for type=%d" (0xC0CB18) and 0. Found: `AddScriptGameThing(thing, 0)` 0x70D0F0; else "Thing not found" (0xC0CB38)
  and 0. `PUSH(id, 4)` at 0x6F0E94 / 0x6F0FFD.
- `FindTownAtPos` 0x6F7340 = `FindTownNearPos(…, 10.0 [0x41200000])`; `FindTownNearPos` 0x6F7370 =
  `MapCoords::GetNearestTown(r)` 0x6020E0. The type, sub-type, filter and excludingScripted are not used.
- `FindAtPos` 0x6F7220 / `FindNearPos` 0x6F7280: GScript +0x14 = the point, +0x20 = 1.0 / r, then
  `FindNearForScript(filter, type, subtype, 1.0 / r)` 0x604370: the nearest accepted thing of the square's cells, not
  cut at r ([Object lists per cell](../engine/object-size-and-map-cells.md#object-lists-per-cell)). The filter is
  0x6F6FA0 (type and sub-type) for CALL, 0x6F7070 for CALL_NEAR (also `GetDistanceInMetres(point, the thing or its
  totem) <= r`, `test ah, 0x41`), wrapped by 0x6F79F0 / 0x6F7A20 (IsInScript vt +0x448 rejects) when
  excludingScripted.
- `FindCreatureAtPos` 0x6F7380 / `FindCreatureNearPos` 0x6F73C0: the first creature of `Creature::CreatureList`
  0xC5FCF8 with `GetDistanceInMetres(point, its MapCoords) <= 1.0 [0x8AA390]` (AtPos: no type or sub-type test) /
  `<= r` and info +0x1F4 == subtype (NearPos).
- `GET_NEAREST_TOWN_OF_PLAYER` (`GScript::GetNearestTownOfPlayer` 0x6F2A20): five POPs (0x6F2A2F..0x6F2A83): the
  radius, the player (a float, `__ftol` 0x6F2A96), then z, y, x; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0
  (0x6F2A9C), `GetPlayer` 0x5509B0 (0x6F2AC0), `MapCoords(LHPoint)` (0x6F2AD7). None: "Did not find town" (0x6F2AEA)
  and 0; else `AddScriptGameThing(town, 0)` (0x6F2B0D) and the town.

#### The script object types

**`GameThing::GetScriptObjectType`** (vt +0x4E8) per class: ScriptMarker 0x70D960 = 1, Abode 0x406810 = 2 (every abode class:
Field, StoragePit, TownCentre, Totem, Creche, Graveyard, Windmill, Workshop, Wonder, Football, PuzzleTotem), Feature
0x5276C0 = 3 (also AnimatedStatic, the chess pieces, Flowers, WorshipSiteUpgrade), Villager 0x753020 = IsAChild ? 5 :
4, Animal 0x41B200 = 6, Reward 0x6E5CA0 = 7, MobileStatic 0x609330 = 8 (Bonfire 0x439A70, Fragment, MagicTeleport,
GStreetLantern 0x734D40 too), Town 0x73E200 = 9, Dance 0x50C3C0 = 10, Flock 0x530490 = 11, Creature 0x47C8B0 = 12,
DeadTree 0x5115B0 = 13 (FelledTree too), InfluenceRing 0x5CDC50 = 14, WeatherThing 0x774360 = 15, Pot 0x66F530 = 16,
ScriptTimer 0x711600 = 17, CitadelHeart 0x4680B0 = 18, WorshipSite 0x77D2E0 = 19, MobileObject 0x607B60 = 20 (Whale,
OneOffSpellSeed, crops, creeds), Dove 0x41EAA0 = 21 (Bat, Crow, Dove, Pigeon, Seagull, SpellBat, SpellDove, Swallow,
Vulture: the vtable sweep), Tree 0x74C130 = 22 (MagicTree too), LandscapeVortex 0x5FFFF0 = 23, SpellSeed 0x729C90 =
+0x72 ? 30 : 24 (`neg byte +0x72; sbb; and 6; add 0x18`), Poo 0x6083C0 = 25 (`fn_00607000` for GMobileObjectInfo
5), Ball 0x436100 = 28, Mist 0x606910 = 29, Rock 0x6E79E0 = 33, SpellDispenser 0x722FB0 = 36, `ScriptHighlight::GetScriptObjectType`
0x70AE30 = 37, GComputerPlayer 0x6587B0 = 38, Scaffold 0x6EAB60 = 39, TotemStatue 0x738EB0 = 40; every other class 0
(`GameThingWithPos` 0x570200: `xor eax, eax`: the Citadel, PuzzleGame, BigForest, FishFarm, MapShield, the spell
icons, GBaseOnly 0x609340...).

- `Villager::IsAChild` (vt +0x458, 0x55CB00): IsChild vt +0xAF8 == 1, flags +0xE0 bit 3.
- The class factory `fn_00419E00` has no case for the Vulture, CitadelDove and CitadelBat (0x419FCE), so none is
  ever made.

#### The sub-type

`fn_006F6D00`, byte table 0x6F6F78, jump table 0x6F6F28, types 2..40; `(type − 2)` unsigned above 0x26 at 0x6F6D09
→ "Unknown type for search" 0xC0D368 and 9999 = 0x270F. It is the info record's index (Object +0x28 − array) / size:

- Abode (0x6F6DB1) 0xC3C690 / 456; Feature (0x6F6DC4) 0xCC99A0 / 292; Villager (0x6F6D2A) 0xDA6BE8 / 932; Animal
  (0x6F6D4A) 0xC4D030 / 716; Reward (0x6F6DE4) 0xD50BF8 / 304; MobileStatic (0x6F6E02) 0xD3A6D8 / 300; Creature
  (0x6F6D6A) info +0x1F4; Store (0x6F6D75) 0xD4C660 / 324; WorshipSite (0x6F6EB1) (+0x8C − 0xDA57A8) / 28;
  MobileObject (0x6F6E20) 0xD38448 / 276; Tree (0x6F6D93) 0xDA3AD8 / 320; Vortex (0x6F6E3E) +0xE0; SpellSeed
  (0x6F6E6D) (+0x28 − 0xD9D678) / 400; OneShotSpell (0x6F6E5A) `CastOneOffSpellSeed` (vt +0xAC) +0x68, else as
  SpellSeed; PuzzleGame (0x6F6E8B) +0x48 (never reached: its type is 0); Field (0x6F6E90) (+0x120 − 0xCCF070) / 340
  (never reached: its type is 2); Highlight (0x6F6ED4) 0xD96390 / 272; TotemStatue (0x6F6EF2) 0xDA1D18 / 292;
  DEAD_TREE (0x6F6E46) "Not implemented" (0xC0D380); the default is byte 19 of the table (0x6F6F12).
- No sub-type (9999) for TOWN, DANCE, FLOCK, INFLUENCE_RING, WEATHER_THING, TIMER, CITADEL, BALL, MIST,
  ONE_SHOT_SPELL_IN_HAND, TOTEM, COMPUTER_PLAYER, SCAFFOLD, below 2 or above 40.
- Fixed sub-types: a Fragment (0x76E9D0) has &MSInfo 0xD3A930 = 2; a OneOffSpellSeed (ctor 0x72A3A0) has 0xD39F3C =
  row 25. Both SpellSeed ctors store the same info at +0x28 (Object ctor) and +0x58 (0x7280A0: ebx at 0x7280AB and
  0x7280F0; 0x727FF0: the icon's +0x80 at 0x728003 and 0x728052).
- The filter 0x6F6FA0(thing, type, subtype): type equal (0x6F6FB0), then sub-type 5000 (0x1388, 0x6F6FBA) accepts any
  without asking it, else own == subtype and own ≠ 9999 (0x6F6FCB..0x6F6FD6).
- (🔍 inferred) The WorshipSite's +0x8C is its tribe info: the 28-byte records of 0xDA57A8 by tribe.

### JC specials and the confirmation sounds

A few moments of the intro are hard-coded in the engine rather than written in the script: the "JC specials". The
script starts them with PLAY_JC_SPECIAL 326:

| Special | What it does |
|---|---|
| 0 | the light falling from the sky on the son (20 sprites from 4000 to 10 units at 0.45 u/ms) |
| 1 / 2 | the engine's debug camera following the light, and given back |
| 4 / 5 | the intro hand (hand_intro.l3d) and the son in its grip |
| 6 | the missionaries' boat ([The missionaries' boat](../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire)) |
| 14 / 15 | bookmarks on / off |
| 7..13, 18 and above 15 | nothing (0x708F70; `FollowUs` passes 18 at L52177) |

START_ANGLE_SOUND 285 / 348 are GConfirmation (yes / better samples of HelpSprites). The family drawn in high detail
(SET_HIGH_GRAPHICS_DETAIL, THING_JC_SPECIAL) is in
[The family in high detail](../beings/animation.md#the-family-in-high-detail-supervillager).

**The bookmarks switch.** `PLAY_JC_SPECIAL` 14 (0x708F5A) turns the bookmarks on ([0x9CD384] = 1), 15 (0x708F66)
off; only `FollowUs` uses 15 (L50058). [0x9CD384] is 1 from InitStaticsValues 0x54A81E and
CleanGameForScriptReboot 0x6EBD04 and is saved by Bookmark::SaveAll 0x43A49C; the bookmark keys
(GGame::ProcessKey 0x63F46B) and the bookmarks' drawing (fn_00439B90 from fn_005E5CD0 0x5E6180) only work while it
is set.

#### The intro hand and the light

JCMisc.cpp's `Intro`. PLAY_JC_SPECIAL 0, 1, 2, 4 and 5 go to fn_005DF9C0. The per-frame update is fn_005DF640 (from
fn_005E5CD0 0x5E6241). The hand's grip is fn_005DFCE0 (GLandscape::Draw 0x5E4B35). `Intro::ReleaseAll` is
0x5DFC40.

##### The sequence

1. 0 makes the light. 1 puts the drawn camera behind it (LH3DTech's debug camera mode 2,
   [Shake](../camera/script-camera.md#shake)). 2 gives the camera back, 4.5 s later in FollowUs.
2. When the light lands, the intro hand appears at the Son's spot and plays Data\MISC\hand_intro2.anm (0xBF3298;
   "Hand_Pick_Up_Swimmer", 766 ms, one shot).
3. Then 4 puts the hand at the boat with Data\MISC\hand_intro.anm (0xBF32D0, set with vt+0x180;
   "Hand_Put_Down_Swimmer", 3066 ms, looping).
4. 5 plays that clip with the Son in its grip (feature 7). The second 5 jumps to 2379 ms (0x5DFBCE) and lets the Son
   go (0x5DFBC6). At the end of the clip the hand goes.

##### The globals and the hand's placement

| Global | What it holds |
|---|---|
| [0xD19C88] | the light |
| [0xD19C8C] | the material |
| [0xD19C90] | the debug camera follows the Son's spot |
| [0xD19C94] | set when the pick-up clip wraps (0x5DF807); never cleared |
| [0xD19C98] | the hand object |
| [0xD19C9C] | the hand mesh |
| [0xD19CA0] | the clip (fn_00839900, 0 when none) |
| [0xD19CA4] | playing |
| [0xD19CA8] | the pick-up yaw (never written, so 0) |
| [0xD19CAC] | cleared at 0x5DF680, never read |
| [0xD19CB0] | the pick-up wait |
| [0xD19C34] | follow the Son |
| [0xEB9A78] | the light timer |
| [0xBF2AFC] | the state (initially −1) |

- **The hand** is `LH3DObject::Create(2)` 0x80B4D0 (an animated object), then vt+0x58(1), then
  `LH3DMesh::CreateFromHD("Data\\MISC\\hand_intro.l3d", 0)` 0x8067F0 (string 0xBF32B4), then SetMesh vt+0xF4
  (0x5DFB00..0x5DFB3B / 0x5DF67B..0x5DF6C1). It is freed through vt+0x4 (0x5DF8DE / 0x5DFC70), fn_00839970 and
  LH3DMesh::Release 0x806D00.
- **Its places.** The Son's spot is [0xD19A28] (initialiser 0x5DF620). The put-down hand is at [0xD19A38]
  (initialiser 0x5DF5F0). The pick-up hand in state 12 stands at [0xD19A18], 1.5 below the Son's spot, set once
  (fn_005DF640 0x5DF75B..0x5DF76F). Its yaw is π [0xBF2B00] (0x5DF830).
- **Scale and yaw.** The hand's scale is 0.008 [0xBF2AF4] (also written to [0xD19A14] at 0x5DF83B). The put-down
  yaw is −π/4 [0x8C79E4] − 15 [0xBF2B04] × π/180 [0x92B20C], each step a float (fn_005DF9C0 0x5DF9C3..0x5DF9DD).

##### What each special does

- **Special 0** (0x5DF9EE) makes CreateMaterial(13, [0xEA1A90]) once. It clears [0xD19C34] at 0x5DFA5C (0x5DFA72
  when the new fails). The light's direction is set before it is normalised at 0x5DFA3F..0x5DFA4F.
- **Special 1** (0x5DFA7D) runs only in state 0. It sets the debug camera focus 0xEA1B68 = [0xD19A28], sets
  [0xD19C90] = 1 and puts the debug camera's position [0xEA1B58] at the light's start (0x828760).
- **Special 2** (0x5DFAD5) runs only in state 0.
- **Special 4** (0x5DFAF5) frees an old clip first (fn_00839970, 0x5DFB44). The clip time is set to 0 with
  vt+0x188(0) (0x5DFB81) and the position with vt+0x20 (0x5DFBA0).
- **Special 5** (0x5DFBB2) runs only in state 4 and sets playing at 0x5DFBE4..0x5DFBEE.

##### The update, per frame

- **State 0** (0x5DF92D): the light falls. When it is done it is deleted (fn_008282E0 + delete, 0x5DF937). When the
  light has landed (hold, 0x5DF968) the pick-up hand comes. The camera follow is at 0x5DF978..0x5DF99C.
- **State 12** (0x5DF66F) is the pick-up hand: [0xD19CB0] and [0xD19CAC] are cleared at 0x5DF680. Once the light
  is gone (0x5DF711), [0xD19CB0] = 1000 (0x5DF731) and the clip may run (0x5DF7B0, time at 0x5DF81B).
- **State 4** (0x5DF865) is the put-down hand. When its clip wraps everything goes except [0xD19C34]
  (0x5DF8DC..0x5DF910). The clip time is written at 0x5DF921.
- **The clip time** is `Object::AdvanceAnimTime`'s rule written inline (0x5DF7EC..0x5DF803 / 0x5DF8A8..0x5DF8D2):
  n = time + ms. A looping clip (+0x50 & 0x100) gives n % duration (signed idiv), otherwise min(n, duration − 1).
  It wraps when n < time (0x5DF805 / 0x5DF8DA jge).

##### The grip

- The grip (fn_005DFCE0 0x5DFD03..0x5DFEB2) needs a hand, [0xD19CA4] set, its time ≤ 1817 ms ([0xBF2B08],
  0x5DFD05..0x5DFD1E) and its mesh with an EBone block (flag 0x200000). The check is at 0x5DFCE0..0x5DFD9F.
- The hand is posed with GetPose 0x839980(bones [0xC37D9C], mesh, time, +0x14) at 0x5DFDA1..0x5DFDD2. Then
  bones[EBone +0x304] times the EBone's matrix 0 (fn_007FAE60) gives the EBone point through the bone
  (0x5DFE65..0x5DFE8A). Its y is lowered by 0.65 [0xBF2B0C] (0x5DFE93..0x5DFEB2).
- GLandscape::Draw 0x5E4B30..0x5E4BEE draws the feature-7 SuperVillagers at the grip while [0xD19C34] is set: from
  the first special 5 to the second, or from special 0. That read happens before the update, so it sees the hand as
  the last frame left it (0x5E4BCE).
- When fn_005DFCE0 writes nothing, the original reads a stale stack slot.

##### The release

`Intro::ReleaseAll` 0x5DFC40 is called by THING_JC_SPECIAL 18 (0x70910E) and CleanGameForScriptReboot 0x6EBBFB. It
frees the light (0x5DFC41..0x5DFC5E), the hand object, mesh and clip (0x5DFC64..0x5DFC9D) and the material
(0x5DFCA3..0x5DFCBE). It sets [0xD19CA4] = 0, turns the debug camera off ([0xEA9EC8] = 0) and sets the state to −1.
It keeps [0xD19C34], [0xD19C90], [0xD19C94], [0xD19CB0] and the light timer.

#### The light of special 0

fn_00827F20(target, direction, material) 0x827F20..0x8282CF, 0x34 bytes in [0xD19C88].

##### Making it

- It is 20 LH3DSprites (`Create(20, 1)` 0x8404A0: SetToZero 0x8404F0, then +0x28 |= 0x80 for the sprite's own
  material; 0x828000..0x8281E3).
- The direction is normalised as (x² + y²) + z² with InverseSquareRoot 0x841170 (0x827F41..0x827F87).
- start = target + direction × −4000 [0x9A3950], written to both +0x04 and +0x10 (0x827F9A..0x827FF8). Sprite i is
  placed at start + direction × (−i × 6) (0x82804D..0x8280A3).
- Sprites above 5 use cell 50 at double size (0x82806D, 0x8280AE..0x8280B7, fadd 0x8280DD). The others use cell 48
  (0x8280F0..0x8280F9).
- The size is float(15 (19 − i)) × 0.05 + 3, at least 1e-4 (0x828108..0x82812E), set with `LH3DSprite::SetSize`
  inlined (0x828134..0x82814E: the origin scaled by new / old, then the size; the light's origins stay 0).
- Each sprite gets the angle Random(0, 2π) (0x828136, 0x828151) and the colour white with alpha (19 − i) × 255 / 20,
  truncated (0x82815C..0x828186).
- Sprite 18 is the glow at the head, 80 [0x8D060C] wide with alpha 0x28 (0x828183, 0x8281AB).
- Sprite 19 (0x8281E9..0x8282B9) is a thin streak 350 [0x9A3948] × 17.5 [0x9A3944] (half sizes, 0.05 at 0x8282A9),
  6 [0x9A394C] behind the start. The streak is turned −π/8 (0x828257), coloured 0x0EFFFFFF (0x828269) and uses cell
  49 (0x828280..0x828286).
- Creation ends with +0x28, +0x2C and +0x30 = 0 (0x8282BC..0x8282C4).

##### The fall

- The head (fn_00828350): d = elapsed × 0.45 [0x9A3958]. Past 3990 [0x9A3954], d = 3990 and +0x30 = 1 (0x828369).
- 0x828760 (special 1) copies the head into +0x04 and into the debug camera's position.
- The callback, falling state (0x828500): sprites 18 and 19 stay at the head (0x828556) and the others follow at −i ×
  6 (0x828572..0x8285CE). Sprite 19 turns by 0.00015708 rad [0x9A395C] per ms of g_game_time_inc (0x82852D).
- When the game time stands (0 ms), there are no Random draws (0x8285D6). Otherwise every sprite gets a new angle
  Random(0, 2π) (0x8285E1). The five front sprites flicker with float(15 (19 − i)) × 0.05 + Random(−2, 2) + 3
  (0x8285E6, 0x8285F1..0x828649).
- After 8237 ms of fall, ZFUNC ALWAYS (8) is set around the 20 draws (0x828656..0x8286CE, 0x828659). LESSEQUAL (4)
  comes back after.
- Then elapsed += ms (0x8286D4). The debug camera's position is put 150 [0x8CC7E8] back up the beam and 30 above
  (0x8286D7..0x828746).

##### Hold and fade

- On arrival the light holds for 500 ms [0xC383E0] (0x82850E, 0x828519; hold code 0x8283F0..0x828418). The light
  timer is [0xEB9A78].
- Then it fades over 1000 ms [0xC383E4] (0x828412): alpha = ftol((timer / 1000) × (255 − 50) + 50), with 50
  [0xC383E8] as the floor (0x82841E..0x828470).
- In hold and fade (0x828473..0x8284FB), sprite 0 is drawn three times with ZFUNC ALWAYS and a new Random(0, 2π)
  angle after the first and the second.

##### Drawing it

- It is one Z object (fn_00828300 from fn_005DF640, fn_005E5CD0 0x5E6241), key |head − g_camera|² summed (x² + y²) +
  z² (0x828300..0x828336); its callback 0x8283D0 draws LH3DSprite::Draw sprites in [0xD19C8C] (mode 13, CULLMODE 3
  from +5 = 0 at 0x840C08..0x840C27), ZFUNC 8 while depthAlways (0x828483 / 0x828673), else 4
  ([Transparency and render modes](../rendering/transparency-and-render-modes.md)).
- The sprites are drawn with `LH3DSprite::Draw` 0x840530 in mode A: in the plane of the screen, nothing at or before
  the near plane.
- The callback's CRT Random draws are at 0x8284A8, 0x8284C0, 0x8285E1 and 0x8285FB. They run when the Z queue is
  drained ([The C runtime](../engine/random.md#the-c-runtime)).

### Addresses

The original's addresses and names that openblack's code used to quote, next to the openblack symbol each one
corresponds to (the family in high detail, `super_villager`):

| Address or name | What it is | openblack |
|---|---|---|
| `[0xC383D8]` | the SuperVillager clip cross-fade length, 300 ms | `super_villager::k_FadeMs` |
| `[0x9A392C], fn_00825530 0x8255E1` | the SuperVillager drawn-yaw turn rate (float 0x407B53D2 = 5 pi / 4 rad/s), read by the draw path | `super_villager::k_TurnRate` |
| `SuperVillager +0x14 / +0x18 / +0x1C / +0x20..+0x28` | the smoothed yaw (set from obj+0x48 at 0x825FBC, Villager::Draw's +0x108), the thing, its Eyes, the three saved meshes | `Entry / components::SuperVillager` |
| `[0xBF3584]` | the swim ring period, 1000 ms | `super_villager::k_SwimRingEveryMs` |
| `[0xC37D9C] +0x114 / +0x11C` | bone 5's x and z in the bone buffer (5 x 0x30 + 0x24 / + 0x2C), where the swim ring is made | `super_villager::k_SwimRingBone` |
| `LH3DObject +0x98, GetDetailMesh(2, 1, 0)` | the high detail slot SET_HIGH_GRAPHICS_DETAIL compares (set by Villager::SetAge 0x7528C0) | `HighMesh` |
| `fn_00825E70: obj+0x88..+0x94` | the release clears the drawing object's cross-fade state | `ClearSmoothing` |
| `fn_00825F20 0x825F49` | making a SuperVillager returns early when the thing is one already (the HD mesh leaks) | `Create` |
| `fn_00825E40` | links a new SuperVillager at the head of g_first [0xEB9A08] | `Create / testing::Adopt` |
| `0x825F85..0x825FB5` | TemporaryShadow creation: ShadowInfo holder with si+0xC = 1 (land only, fn_008745A0), holder+4 = 1 fixed sun (0x825FB5); fn_00825090 calls vt +0xC4(0) (IsHumanShadowed off) and vt +0x98(0) = fn_007F98E0 (IsDisappear off) | `Create (DynamicShadow)` |
| `0x708DE7..0x708DEC` | SET_HIGH_GRAPHICS_DETAIL stores the thing (+0x18), sets the shadow object's +0x10 = 0 so TemporaryShadow::UpdateAll 0x825190 skips it, DrawListRebuildCount = 1 | `SetHighGraphicsDetail` |
| `0x825FC5..0x825FF8, 0x825C8A` | the three meshes saved at +0x20..+0x28, then vt+0xF4(hd, 0, 0) with an HD mesh; the draw uses the high slot | `Create` |
| `0x825FFB..0x82615D` | Eyes made when the high slot's mesh has an EBone block and the eye type is not -1 | `Create` |
| `0x826183..0x8261A1` | SuperVillager::Release gives the three meshes back | `Restore` |
| `fn_00825E70 0x825E79 -> fn_00825130 -> fn_008250F0, fn_008745E0` | the TemporaryShadow's holder freed and the saved flags given back (vt +0xC4(+8) IsHumanShadowed, vt +0x98(+0xC) IsDisappear) | `Restore` |
| `fn_00825E70 0x825EDF..0x825EF6` | deletes the Eyes whatever the thing's state | `ReleaseEntry` |
| `fn_00825400 0x825403..0x82541D` | on-screen test: CheckRegionOnScreen 0x868C80 (path [0xEA9EB4] == 0) of mesh box +0x14; centre box +4, radius obj +0x44 x box +0x1C, origin matrix +0x38; side effects g_b_last_on_screen, g_last_selected_box, g_last_distance, vt+0xA0 and the fn_007ACC60 path not ported | `RegionOnScreen` |
| `GLandscape::Draw 0x5E4C7D..0x5E4D4B, [0xEA9EC0]` | swim ring: clock += g_game_time_inc, past 1000 ms (jle) reset to 0; ring +0x18 growth 1, +0x1C kept, +0x20 angle, +0x24 1, +0x28 aspect 1, +0x2C rate 1, +0x30 cell 0x30, +0x34 0xFFFFFFFF; body posed later at fn_008254A0 0x5E6255 | `SwimRing` |
| `0x708E02..0x708E1A` | SET_HIGH_GRAPHICS_DETAIL off: vt+0x70(0) (normal draw back), DrawListRebuildCount = 1, fn_00825440 | `SetHighGraphicsDetail` |
| `0x708D2D, LH3DObject::AddDrawing 0x815A7C` | SET_HIGH_GRAPHICS_DETAIL on: vt+0x70(1) sets obj+8 \|= 0x10, which AddDrawing skips | `SetHighGraphicsDetail` |
| `0x708D40..0x708DC5` | the high slot compared against MeshPack[501], [498], [439], [420] | `SetHighGraphicsDetail` |
| `0xC20574 / 0xC20554 / 0xC20534 / 0xC20520` | the HD mesh path strings nors_man / nors_woman / nors_boy / sable | `SetHighGraphicsDetail` |
| `0x70905B..0x709085, jump table 0x709130` | THING_JC_SPECIAL: the thing must be a SuperVillager except for feature 19; jump on feature - 7 (0..12) | `ThingJcSpecial` |
| `0x70909E / 0x7090AE / 0x7090C2 / 0x7090D0 / 0x7090E2 / 0x70910E` | THING_JC_SPECIAL features 7 (follow grip), 8 (snap), 9 (fchs yaw mirror), 16 (+pi/2 [0x8C78D8]), 17 (-pi/2), 18 (Intro::ReleaseAll 0x5DFC40) | `ThingJcSpecial` |
| `0x5E4B3A..0x5E4B93, HelpSystem +0x45E8 / +0x45EC` | every SuperVillager released each frame without a script's wide screen | `Update` |
| `0x5E4BA5..0x5E4BB7 / 0x5E4E3C, IsAvailable vt+0x2C` | a SuperVillager whose thing is no longer available is released | `Update` |
| `fn_008254A0 0x8254DD..0x825504, [0xEB9A0C], [0xEA9F30]` | dead SetPosition to (0, 0, 0) under [0xEB9A0C] != 0, a bss flag with one reference (the read 0x8254E3) | `FollowHand` |
| `fn_00825530 0x825E1F..0x825E28` | the eyes updated after the body | `Draw` |

### Sources

- The challenge script dump (`rt_chl_code.txt`, line numbers above), the v1.42 exe through the xref index, and the
  oracle for the karaoke gate and box.

## openblack

> **Code rules.** Intro and help state lives in ECS components (highlights, timers, SuperVillagers) or Locator
> services (the help system, the advisors), never in globals; the `.hd`, `.l3d`, `.anm` and text files load through
> the resource caches; resources are owned with RAII and standard types; pure logic (text layout, timers, spirit
> motion) is tested with fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or
> addresses (those belong here). See [Refactor conventions](../project/refactor-conventions.md).

### State in openblack

| Part | State |
|---|---|
| Steps 1..6 of the flow | ported and checked in game: the hand-over comes about 4 min after the start |
| Dialogue texts | drawn (below) |
| Advisors | drawn and moving; voice tags not fed (pending) |
| Villagers' focus, override animations, flocks, dance, CALL_IN | pending |
| Citadel as a plan, INSIDE_TEMPLE, SET_INTERFACE_CITADEL | pending |
| Family in high detail, JC specials | ported and wired: SET_HIGH_GRAPHICS_DETAIL, THING_JC_SPECIAL and PLAY_JC_SPECIAL in `CHLApi.cpp`, updated and drawn every frame from `Game.cpp` (`ECS/SuperVillager*` and the intro system, below) |

- **The dialogue texts**: `Help/HelpTextDisplay` (the ring, the box and the stack), one splitter for the display and
  the word count (`Help/TextSplitter`), drawn by `Renderer::DrawHelpText` between the bars and the film, from the
  pre-draw `graphics::OverlayFrame`. The click cue is drawn with `help::input_prompt` (was `help::kmicon`).
- **The karaoke** (#93): `help::karaoke` (`Help/Karaoke`) holds the gate (`UsesBouncingBall`, `IsVerseMusic`,
  `IsSungWord`) and the box (`Highlight::Follow`), pinned to the oracle's runs of the word draw frame by frame
  (test_karaoke). `HelpTextDisplay::Layout` marks the sung word (`TextRun::sung`, its measured width kept) from
  `SongInputs` (the language, the script music type, the music beat), which join the cached layout's inputs;
  `FollowSong` steps the one `karaoke::Highlight` once a frame the texts are drawn, by `game_clock::ClampedFrameMs`;
  the renderer draws the box as the glow panel (`AddKeyOrMousePanel`, additive) between the words before the sung one
  and the rest. The testbed's `dialogue.song_karaoke` shows it.
- **The help system's turn**: one function, `help::Process` (`Help/HelpSystem.cpp`), with the six parts in the
  original's order; the game's turn calls it after the scripts and the script fade, and the temple's paused turn
  after the room's.
- **CALL and CALL_NEAR**: an object's script type and sub-type is `ecs::script_type`; the cell search is
  `map_cells::FindNearForScript`. A script's marker gets type 1 from the `ScriptMarker` tag that
  `ScriptMarkerArchetype` puts on it (`script_type::TypeOf`; before, 0). Test: `test_script_delete`
  `AMarkerIsTheMarkerType`.
- **The JC specials**: the intro system (`Locator::introSystem`, its rules in `ECS/IntroRules`; updated and drawn
  every frame from `Game.cpp`); the confirmation sounds are `audio::confirmation`, fed by the world camera's turn and
  tilt.

#### Where it is in the code

- `src/ECS/ScriptTypes.{h,cpp}` (`ecs::script_type`; `ScriptType` is `script::ObjectType`):
  - `script_type::TypeOf` gives each component the class's type: `ScriptType::Abode`, `ScriptType::Feature`,
    `ScriptType::Villager` / `ScriptType::VillagerChild` (`villager::IsChild`), `ScriptType::Animal` (or the bird type
    for the dove classes: `animal_ai::IsDoveClass`, the flying species plus `AnimalInfo::Vulture`),
    `ScriptType::MobileStatic` (a `MobileStaticInfo::SingingStoneBase` is a base only, so none), `ScriptType::Town`,
    `ScriptType::Flock`, `ScriptType::Creature`, `ScriptType::DeadTree`, `ScriptType::InfluenceRing`,
    `ScriptType::WeatherThing`, `ScriptType::Store`, `ScriptType::Citadel` (openblack's Temple is the citadel heart),
    `ScriptType::WorshipSite`, `ScriptType::MobileObject` (`ScriptType::Poo` for a `MobileObjectInfo::LumpOfPoo`),
    `ScriptType::Tree`, `ScriptType::SpellSeed` / `ScriptType::OneShotSpell`, `ScriptType::Mist`,
    `ScriptType::SpellDispenser`, `ScriptType::Highlight` and `ScriptType::TotemStatue`.
  - A class openblack does not have gives `ObjectType::None` (`ScriptType::None`), as the original's base class does.
  - `SubtypeOf`: the info record's index by type, with the same cases as the original (also `ScriptType::Reward`,
    `ScriptType::Vortex`, `ScriptType::PuzzleGame` and `ScriptType::Field`, never reached); a fragment's sub-type is
    `MobileStaticInfo::Rock`.
- `src/ECS/IntroRules.{h,cpp}` (`intro_rules`): the Son's spot `k_BoySpot` is pinned to the original's bits
  (`0x44B10AC9`, 0, `0x4500580C`: 1416.34, 0, 2053.50); `light::Step` runs the light's states (`State::Falling`,
  `Holding`, `Fading`, `Gone`); the sprite size goes through a private `SetSize` (`LH3DSprite::SetSize`'s rule).
- `src/ECS/Systems/Implementations/IntroSystem.cpp`: `CreateHand` makes the hand entity with no mesh yet (the update
  gives it one on the frames the hand is drawn), `FreeHand` destroys it (⚠️ approximate: the mesh and the clip stay
  in their caches), `QueueLight` keys the light's Z object at its head as it is now.
- `src/Graphics/OverlayFrame.h`: what the end of the frame draws over the scene (the fade, the bars, the help texts,
  the advisors), filled once a frame before the draw and read by the renderer through `DrawSceneDesc::overlay`; its
  `IntroLightOverlay` carries the light of special 0, submitted in the main view's sorted queue with a
  `zsort::Key` in `SumOrder::XYZ`.
- `src/ECS/Systems/CameraBookmarkSystemInterface.h`: `SetEnabled` is the bookmarks' switch of PLAY_JC_SPECIAL 14 / 15
  (on at start-up and after a script reboot).
- `Game::ProcessEvents` takes the bookmark keys only while `Locator::cameraBookmarkSystem::value().IsEnabled()`
  (0x63F46B..0x63F472, PLAY_JC_SPECIAL 14 / 15); `Game::LoadMap` turns the bookmarks on when
  `Locator::cameraBookmarkSystem::has_value()`, as CleanGameForScriptReboot 0x6EBD04 does.
- `src/Gui/SkipBox.{h,cpp}`: the tutorial requester `SkipBox`, drawn through raffclar's `DialogPainter` with
  `graphics::GameFont`. `SkipBox::SkipBox` builds the texts, the selection 0 and the two half-second fades;
  `SkipBox::MouseMove` and `SkipBox::Click` hit-test the check boxes and a 32 × 32 button square (🔍 inferred: the
  original's hit tests are not read); `SkipBox::Draw` draws the box, the controls and the label with
  `DialogPainter`'s equivalents of the original's control draws.

### Test hooks

- `OPENBLACK_TEST_TEXT_CLICK=1` clicks the texts that wait for a click; `OPENBLACK_TEXT_TRACE=1` logs the texts;
  `OPENBLACK_HAND_DEMO_TRACE=1` the hand demo.
- `OPENBLACK_TEST_TEXT_SHOT="id,path[,ms]"`: a screenshot `ms` (1000 by default) after text `id` is shown, for
  example `4431` ("Saludos.", both advisors on screen).

## Pending

### The tutorial requester

- **The controls' own draws and hit tests are not read**; openblack uses `DialogPainter`'s equivalents. Not read:
  the button's hit test `SetupBigButton::HitTest` 0x40D310 (the button's own extent, DrawBigButton's size 32; here a
  🔍 inferred 32 x 32 button square), `SetupCheckBox::HitTest` 0x410F90 / `Click` 0x411020 (here the check box
  squares), and the arrow's look 2 (+0x248) through `DrawBigButton` 0x412150. Verify with xref
  (`xref.py show 0x40D310`).
- **SetupBox's own draw** (vt 0x8D85F0) is pending: the background DrawBg 0x413960 over k_Box and the controls in
  their list order; the controls' draws: SetupControl's text (DrawText 0x4119B0 with +0x23C as the justification),
  SetupBigButton::Draw 0x40CEB0, SetupCheckBox::Draw 0x410B80 (DrawBigButton(x, y, +0x23C != 0, hovered, w, +0x248
  != 0, 1, -0xA000, 0xA000)).
- **⚠️ approximate** the labels' vertical place, as BigButton's labels: the centre less half the size (the exe's - 5
  / the text height, 0x40D153, not re-read).
- 🔍 inferred: k_Box spans (200, 155)..(600, 445) (to confirm in SetupBox's draw); "look 2" is read as the
  left-arrow look from GameMenu's call sites, not from a documented table.

### Dialogue texts and the help system

- **The line height after a resolution change**: ReInitialiseText 0x5C7A80 is not read.
- **⚠️ approximate** The did-you-know bubble's place in the frame: the original queues it as a Z object in the world
  (`HelpSystem::Draw3D` 0x5C5B38 → fn_005C83D0 → 0x5767D0), drawn with the world's sorted objects, so before
  HelpText's callback (priority 20000) and under the dialogue texts; openblack draws it after `DrawHelpText`, over
  them (`Renderer.cpp`). Where in the sorted queue it falls (its key) is not read.
- **`GatheringText::GetFrac`** (Bubble.cpp, 0x831550..0x8315A0): the a == b branch at 0x831589 is not read; b == a
  gives 1 in openblack.
- **`RunMessageSet` mode 2**: the thing variant 0x5C8D90 is not read, so openblack always takes the "no thing" case
  (0 and 0) instead of the thing's GetQueryFirstEnumText / GetQueryLastEnumText (vt +0x4D8 / +0x4DC).
- **did_you_know_read::MarkRead** (was dyk_read): fn_0078CD20 is not read in detail (🔍 inferred an append when the
  list has room), and where the original saves the read signs (the help profile) is pending.
- **`HelpProfile` `processBlocked`**: the meaning of [0xC4CCEE] is pending: GGame clears it at 0x54B1C5 before
  GoInsideCitadel, and the hand is not drawn while it is set.
- **`InterfaceActive`**: read from the offsets as a help-system flag copied from the interface plus an interface
  state bit cleared; its other effects are pending (unknown beyond that).

### The advisors

See [The advisor spirits](../interface/advisors-and-help.md#the-advisor-spirits).

- **The anim sounds** ([The anim sounds](../interface/advisors-and-help.md#the-anim-sounds)): which points an anim's
  phase passed (`CrossedSounds`, as PlaySoundFX 0x5C2800) is ported, nothing plays them yet. PlaySoundFX's callers:
  ApplyWingsHover 0x5BCB05 / 0x5BCB75, ApplyLipSync 0x5BD052, Update1 0x5BED2B / 0x5BEE47 / 0x5BF11F. With no bank
  passed it uses GAudio +0x3AC (`GetSoundFXBank` 0x5BC7C0); for each event crossed it calls
  `GAudio::SamplePlayAnimEffect` 0x42A4B0 with the dude as owner, the distance from g_camera to the dude's +0x3374
  point, the event's +0 and the flag (event +4 == 0) (0x5C293B..0x5C29DE); the rest of its argument record is not
  read yet.
- **The voice tags** are not fed.
- **Where the land's light is sampled** for an advisor in the world: 0x5BE9B6 (fn_00801C90) seems to run in Update1
  after the matrix and before the base pose and ApplyAnim's root moves, so before the anims move the advisor
  (🔍 inferred); ours samples at its position after them. Kept until it is read in the x86.
- **The trail's alpha test**: whether the original's hardware compares the modulated alpha rounded or truncated to a
  byte (it matters only for an alpha between 4.5 and 5 of 255); and the normal table and no forced ALPHAREF at the
  callback are read from the writers, not checked while the game runs.
- **`HelpDudeFile` `value35C0`**: +0x35C0 (0x5C2374) is pending, 0 in both.
- **`AdvisorSpirit::UpdateMotion`** (was `HelpDude::Update1`), step 6, the zones: fn_005BDD40's four records +0x3404
  are pending (nothing reads them in openblack).

### Script highlights and timers

See [Script highlights](../interface/script-highlights.md#script-highlights).

- **ScriptTimer**: GameThing::SetScriptNameOfCreate 0x56FA70 (fn_007115A0's call on CREATE) is not ported.
- **ScriptHighlight `hidden`** +0x5C: no writer found besides fn_00709910 (= 0) and Load.
- **ScriptHighlight::Create**, pending in the renderer: 0x709AB7 fn_0080B440(0) on the 3D object; 0x709ACE vt
  +0x98(0) on it for a scroll; the sprite +0x64 (0x709ADE) and the glow object +0x68 (0x709AF7: mesh k_GlowMesh,
  SetMaterialProperties {1, 0, 0, 1, 1} 0x709B4B) are ExtrasOf's; 0x709B5A vt +0x80 on the 3D object
  (UseFootpathIfNecessary in its vtable, not read). Also 0x709BC7: unless +0xA & 1 (🔍 inferred clear for a new
  object).
- **ScriptHighlight::SetActivated**, pending in the audio: the options' mapping to audio::PlaySoundEffect(PlayOptions)
  is not checked (sample 0x86).
- **Still to do in openblack** (from the object draw list,
  [The object draw list](../rendering/object-draw-list.md#the-object-draw-list-stage-4m)):
  - the highlight's mesh is still taken away and given back every frame (`script_highlight::SyncMeshes`, openblack's
    stand-in for the hidden and SET_DRAW_HIGHLIGHT 0 early exits);
  - SET_PROPERTY YPOS: the original moves the highlight at the next turn and its next drawn frame; openblack's
    `SetYPos` moves the Transform at once (queued fix);
  - the seed graphic's holder effect (with its glints): the original steps it from the list; openblack steps it
    every frame from the orb and the icons (queued fix);
  - the vortex's over-land effects (the Post step and the light-map alpha): the original steps them only for a
    vortex the land's object draw list reaches; openblack steps them every frame
    ([The Z-sorted part](../world/vortex.md#the-z-sorted-part)) (queued fix);
  - the block's highest altitude is the land file's `highestAltitude`; the original recomputes `+0x924` at the load
    (fn_007FEA60), and the two are not yet compared;
  - the RebuildCount writers not hooked: in the original the tug, Reward::Create and Process, the tutorial key,
    GScript::Load's release and the task-stop camera release each ask for a rebuild; openblack publishes no
    `DrawListRebuildRequested` from them, one fix per writer (queued fix).

### The intro hand, the family and the JC specials

- **⚠️ approximate** The intro hand's grip when the original's grip lookup writes nothing: the original reads a stale
  stack slot there, which openblack does not reproduce (🧪 verify with xref: `xref.py show fn_005DFCE0`). A bone
  outside the pose (the original reads past the buffer) gives nothing in openblack (pending).
- **The intro hand**: LH3DObject::Create(2) (an animated object) in [0xD19C98] with vt+0x58(1); obj+4 bit 0x20 when
  [0xC38224] is pending.
- **The Eyes ctor**: LH3DObject::Create(0), vt+0x98(0) and vt+0x58(0) (obj+4 bits 8 and 0x20 off): no fade with the
  distance, and unlit ([The family in high detail](../beings/animation.md#the-family-in-high-detail-supervillager)).
- **`HandDemo` `Record::trigger`**: "the trigger key state" is 🔍 inferred from the space-key note.
- **The countdown display**: that the arguments of CreatureMentalEditor::DrawTextA 0x4DF310 are r, g, b is
  🔍 inferred.

### After the hand-over

- The camera feature mask's effects, the angle / pitch sound feeds (the player's camera is not CameraModeNew3).
- The second cinematic's villager opcodes, the citadel plan.
- The villagers' focus, override animations, flocks, dance and CALL_IN; INSIDE_TEMPLE and SET_INTERFACE_CITADEL.
