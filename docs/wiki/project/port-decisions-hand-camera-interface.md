# Port decisions: hand, camera and interface

## Summary

How raffclar's code for the hand, the camera, the input, the interface and the help system was brought into our tree:
for each piece, what of his was taken as he wrote it, what of ours stays behind his names and interfaces, and what was
left out and why. The rule is the one of [the refactor conventions](refactor-conventions.md): where both trees do the
same job his version is taken only once it is shown to behave as the original, bit for bit; otherwise ours stays,
moved behind his names. Every "not taken" below is either a change of behaviour waiting for its own measured commit,
or something the original does otherwise.

## The original

This page records porting decisions, not the original's behaviour. Each decision points at the page that holds the
original's rules it was checked against: [the hand](../hand/grab-hold-and-throw.md), [the player
camera](../camera/player-camera.md), [the script camera](../camera/script-camera.md), [advisors and
help](../interface/advisors-and-help.md) and [the screen and its read-outs](../interface/screen-and-readouts.md).

## openblack

### The interface and help

#### Gui, the files that stay ours

His versions would change what is drawn or when.

- `GameFont` adapts the exact port in `graphics::GameFont`, read through the font cache. His parses the font files
  himself, with other glyph metrics and line breaks, so every line of text would move.
- `ToolTips` is a handle over the help system's tooltips (`Help/ToolTips`). His is a separate state machine whose
  timing and priority differ, which would change the tooltips and the hand demos.
- `GameInterface` keeps the SkipBox, which the deterministic runs answer, and loads its textures through the
  resource cache. His newer panels come as behaviour changes: the creature status and fight panels are his since M18
  ([the creature's port decisions](port-decisions-creature.md#the-hand-the-catch-and-the-fight)); his tooltip glow and
  arrows are to come later (the cinema bars are his `Gui/CinemaBars`, behind his cinematic director, since M13); his reads files directly, outside the loaders. It takes his pointer
  canvas: the game's pointer is drawn in its own view, `RenderPass::Cursor`, right after the ImGui view, so it shows
  over the debug windows. It is drawn where it was before (over the start-up requester and an open menu), and since
  M21 also over a debug window, as his (`GameInterface::Draw` is told so by `IsMouseOverWindow`, never under ignored
  real input); his single pointer draw after the boxes is not taken.
- `GameMenu` keeps the escape menu's states and the pure functions behind them (`EscapeState`, `EscapeOverQuestion`,
  `PageAfterAnswer`, the video change prompt, the help speed slider), all covered by tests. His later edits are to be
  compared against it one by one.

#### The controls' text sizes

`Controls` is his text with our text sizes put back ([the files taken from his
tree](port-decisions-engine-world.md#gui-the-files-taken-from-his-tree)), each one the original's:
- `LabelSize()`: the big buttons' and the sliders' labels use the mid size (`DialogPainter::MidTextSize`, the
  original's `GetMidTextSize` 0x407A00). The original's big button sets the mid size only for look 2
  (0x40D2B7) and keeps the caller's size with another look; ours uses the mid size for every look (Pending).
- `EditBox::Draw`: the edit box's text uses the small size (`DialogPainter::SmallTextSize`, the original's
  `GetSmallTextSize` 0x407A10), which the original's edit box keeps from its constructor (0x40C481) and reads in its
  draw, its character input (0x40BA45) and its key handler (0x40BF03).

#### Help text fonts

The comments on `help::TextFont` and the text splitter are his plain-English text (each advisor's lines in a font of
its own), pointing at the intro research notes, where our findings on the fonts are. His note on the advisor fonts in
`GameInterface.cpp` is not taken: our dialogs load only j0 there.

#### Click cue

The "Continue" cue's numbers are behind his `help::click_cue` names (`Help/ClickCue`): his public `k_LabelText` and
`k_FadeSeconds`; his internal `CueSize`, `k_RightInset` and `k_GlowAlpha`, which are public in ours (his keep them
inside `ClickCue.cpp`, so a port of his `Layout` takes ours rather than adding a second `CueSize`); plus our own
`MiddleRow`. `k_LabelText` replaces our tooltip id for the same text, which had no other user. His `ButtonLit`,
`LabelSize`, `Layout`, `Box`, `Cue` and `k_WheelMouseRow` are his text, on our `CueSize` and `MiddleRow`: their
arithmetic is the input prompt icon's for the cue (size, middle row, right inset, the 2 pixel gap, text first, the text
two thirds of the size, the glows at `k_GlowAlpha` and a third of it, the left button as the right one mirrored on the
wheel mouse row), and `test_click_cue` pins them to the same numbers. The input prompt draw's first blink calls
`ButtonLit`. Ours still draws the cue as one of the help system's input prompt icons, through the shared key-or-mouse
draw, which works from the icon's size rather than the box's rows; so `LabelSize` and `Layout` are not called by the
game, and the shared draw keeps its own two-thirds size (four fifths for the languages that need bigger text is still
not done).

#### Help profile

His `helpProfileSystem` slot and `HelpProfileSystemInterface` (`Trigger`, `ProcessTurn`, `Get`) front our
`help_profile` module, so our `HelpProfileSystem` keeps nothing: its `Get` is the module's profile. His
`help::profile::HelpProfile` and `EventCount` are taken as he wrote them, with his test: their counting is ours and the
original's, bit for bit (once a turn, the latest 64 times, the same smoothed rate, and the same groups: 14 to 23 also
count as 24, 1 to 42 as 43, 9 to 11 as 11 and every event as 48). Our module keeps that profile in the script state and
adds what his does not have: nothing is counted while the game is paused or a script holds the wide screen (his only
stops in a script's cut scene), the looks at the land and the sky, and the camera help's own 12 counts. The turn's
count goes through the slot; the other callers still call `help_profile::Trigger`, and the natives 235 to 237 read our
module, with his numbers and an error that names the native. Tapping a scroll or a sign is event 34 with a script and
35 without, in both trees.

#### Advisors

His `advisorSystem` slot and `AdvisorSystemInterface` (`Load`, `IsLoaded`, `Update` with his `Frame`, `ProcessTurn`,
`GetController`) front our advisor spirits, whose state stays in the script state, so our `AdvisorSystem` keeps
nothing. Ours adds `FillOverlay` (the frame's spirits and trails for the draw) and `GetFile` (an advisor's .hd for the
debug window). Our advisors read the clock, the cursor and the camera themselves, so the `Frame` is passed empty. His
`Reset`, `GetModel`, `GetDraws` and `GetTrails`, with his models through the resource caches and his advisors'
renderer, are not taken yet.

- His advisors' pure pieces are taken as he wrote them: `ParseAudioTags` reading every bracket of a label, `TagLabel`
  and `BuildSentenceTags` (one bad label drops the sentence's tags), `CrossedSounds` (an anim's sounds whose points its
  phase passed), and `AdvisorSpirit::FlushTags(before)` behind `AdvisorSpiritController::StopSentence(dude, before)`,
  in his later form that fires only the tags before the stop. The tag parser's word buffer, ours in the script state at
  first, is his `AdvisorVoices::_tagWord` since the cue labels' row (N2-M-5), so `ParseAudioTags` and
  `BuildSentenceTags` take his last word ([voices and help](port-decisions-audio-video-scripts.md#voices-and-help)).
  His `helpdude::SoundEvent` is our `HelpDudeAnimEvent`. Our `DudeData` takes his `sounds`, `faceBones`, `pupilCentre`
  and `pupilScale`, and the .hd's word after the face bones is named as his (`unusedWord`, `pupilCentreU`,
  `pupilCentreV`, `pupilScale`). The tags and the sounds are called now: a line's tags come from its wave's cue labels
  (N2-M-5), and the anims' sounds play as the original sends them (#93).
- His advisors' constants are taken: 1 / 0.9 as 1.1111110f (the depth spacing), 1 / 0.78 as 1.2820513f (the cling
  edge) and the good advisor's base pose rate 0.99981135f, the bit patterns the original holds. The spacing's squared
  distance is compared with neither tree's 0.81f but with the original's 0x3F4F5C2A (0.81000006).
- His depth spacing is taken: the advisor's own depth goes to its closeness to the other, at least zMin, where ours
  took the y difference.
- His `RootMoveByRows` (an anim's root move summed in the game's order) and his head angles zeroed before they are
  looked for are taken; only the two angles are zeroed, the third is left, as the original does.
- His `k_DefaultFaceRecord` is taken, in our `HelpDudeFile.h`, as the record an advisor starts with. Our `DudeData`
  keeps zeroed records when there is no file (his fills them with the default); both files have their eight.
- His `ScaleEyes` layer is taken: the face pushes it before the lids with rec[3] and rec[4] (his `blend` and
  `blendB`), and our pose evaluation in the runtime applies it to the local matrices through a pure
  `spirits::ScaleEyes`, since our pose is the runtime's rather than his `SpiritPose`.
- His trail is taken into the logic: his `Trail`, `TrailVertex`, `TrailUv`, `TrailIndices` and
  `AdvisorSpirit::ResetTrail`, `UpdateTrail` and `TrailStrip`, fed every frame for both advisors by
  `AdvisorSpiritController::Update` between the in-world and the near-screen draws, with his `FrameInput::trailDt`.
  The strip's length is summed as the game sums it, from z (his `glm::length` sums from x). Our runtime keeps only the
  turn into the world for the draw (`Runtime::TrailTriangles`, its `WorldTrailVertex`), as his advisor system's
  `MakeTrails` does. The trail starts as neither tree had it, as the original's: its clock at 5 and spark i at the age
  i (his start with 0 and ages 0 is not taken). His sparks are taken in `UpdateTrail`: they draw from the C runtime's
  random numbers through the controller's `Random`, as his do.
- His point weight is taken: the pointing arm's angle is scaled by the arm clip's phase in the intro and the outro as
  in the hold.
- His `AnimLayer::fill` is taken: only the stand pose's Set fills the bones a clip lacks, the base pose's Sets and
  blends do not.

#### The lip sync in the spirits' update

Behaviour change (progress row N2-M-4). As his, `Queries::lipSync(dude, dt)` calls `AdvisorVoices::ApplyLipSync` from
`AdvisorSpirit::UpdateAnimStack`, with the controller's dt, so the mouth and the tags read this frame's result.
`AdvisorVoices::Update(dt)` and `LipSyncThisFrame` are gone, with the game's late call in its audio section: every
advisor's line is already looked at once a frame by `AdvisorSpiritController::Process` (the original's dispatch: out,
at home not hovering, at home hovering with the line only and no lip sync). His `Update()` without arguments is not
taken: the original has no such second pass. The voices' tests start a delayed line with `UpdateSaySentence`.

#### The advisors' body light

Behaviour change (progress row N-M-10). His `vs_advisor` rule, min((N·L < 0 ? 0 : N·L × 1.3) + 0.4, 1) × 255
truncated, is the original's for animated objects and is taken into our advisor variant of the object program
(`USE_ANIMATED_OBJECT_LIGHT`), in place of the ambient-90 rule; CPU twin `model_light::AnimatedIntensity`. Not taken:
his `vs_advisor` / `fs_advisor` programs (our object program already has his pupils, the in-world colour and the render
modes' alpha and tests, measured) and `fs_advisor_quad` (our `WorldTextured` gives the same halo, smoke and trail,
with the alpha compared as a byte as `fs_object` does).

#### The song's karaoke box

Ours only (issue #93): his tree has none, so ours is written to his layout: a pure `help::karaoke` in `Help/Karaoke`
(the gate and a `Highlight` of three `Zoomer`s that the text display owns, as the original's statics belong to its
text display; the missionaries' song shows its box behind the sung word, #93), tested by `test_karaoke` against the
original's numbers.

#### Script control of the dialogue, the camera and the game speed

His `dialogueControlSystem` slot, taken as he wrote it, holds the dialogue's owner, which leaves our help system. The
help system is told through his hooks: `taken` clears the text, `released` closes the display, sends both advisors
home, takes the wide screen away, stops both advisors and sends them home again, in our release's order, then clears
the text, and `sendSpiritsHome` sends both home. As in his tree, both hooks clear the text through the
`helpTextSystem` slot's `ClearAllText` (our `ClearDialogue`, the same call the help system made itself), so the help
system keeps only the advisors' part of the release; his `InterruptAdvisors` between the two sends home is not taken
(our advisors are stopped by the help system's own hook, in our order), nor is his wide screen's removal by the
caller. Whether a task is a help script is asked by the caller, with the same query our release asked.

His `scriptControlSystem` slot holds the owners of the camera and of the game speed over our script camera: the
camera is taken with `script_camera::BeginFrom` and given back with `script_camera::End`. His `GetScriptCamera` is
left out of the interface until his `ScriptCameraModel` is measured against our script camera and taken (our script
camera is a mode of its own, with the dual cameras and the field of view, not a camera model), and his ground height
is passed but not used, as our script camera reads the ground itself. The switches that follow a change of control
are his `ScriptSwitchChanges` (`CameraTaken`, `CameraReleased`, `DialogueEnded`), written into our `CameraControl` and
the script audio state rather than his leash system and `CHLApi` fields; the end of a dialogue also sets the music
beat back to 0, which his switches do not cover. The help scripts start through his `CHLApi::StartHelpScript`, over
our `script_control::StartHelpScript`. Both slots are made with the game's state and reset after the VM, whose
task-stop callback is their only other user.

Not taken, as neither is the original's (read 2026-10-28):

- the reset of both slots as a new land opens. Ours keeps the owners, as the original keeps them. The camera's task
  (GScript +0xA8) is written only by `START_CAMERA_CONTROL` (`GScript::StartCameraControl` 0x6ECCA0, at 0x6ECCDF
  outside the citadel and 0x6ECD5B inside it) and by the release (0x6ECE59, reached from `GScript::EndCameraControl`
  0x6ECEF0), besides the save and load (0x6EC1C6 / 0x6EC55A) (`xref.py field GScript 0xa8`). A new land restarts no
  scripts (`LOAD_MAP`, [the land change](../scripts/land-change-and-saving.md)), and `GScript::Reset` 0x6EB2D0 (a new
  game, a load, the script reload) writes neither this task nor the game speed's +0xAC
  ([the Land 1 intro](../scripts/land1-intro.md));
- leaving Creature Mode before `START_CAMERA_CONTROL` takes the camera. The original's handler does not touch the
  follow mode that Creature Mode is: outside the citadel it only calls `CameraModeScript::Create` 0x461140 (at
  0x6ECCBA), which refuses only while the current mode cannot be left (`GCamera::CantExitCurrentMode` 0x441B70), and
  the follow can be (`CameraMode::CanExit` 0x44A2E0, [which camera gives way](../hand/states.md)), so the script mode
  goes on top of it. openblack ends Creature Mode on the script camera's first frame instead (`Step::Camera` in
  `src/Game.cpp`); what the follow left under the script mode does in the original, during the control and after
  `END_CAMERA_CONTROL` puts a new player's mode on top, has not been read.

The wide screen's owner stays the help system's until his cinematic director holds it. The script
camera itself: [The script mode](../camera/script-camera.md#the-script-mode).

#### --skip-opening

His launch option, as he wrote it (his help text and `ParseNewGameStart`; an unknown word is an incorrect argument).
With an answer the SkipBox is not shown and the game not paused, and the flags are set at once with `SkipFor`; `ask`
is what our game does anyway, as it shows the box at every new game. Without the option nothing changes, and the
checks keep `OPENBLACK_TEST_SKIP_ANSWER`, which answers the box only while it is shown. With an answer, the cinematic
director's `StartStory` is skipped, as his skips it (see "The story's black opening" in
[the audio, video and scripts port decisions](port-decisions-audio-video-scripts.md)).

#### Help texts

His `helpTextSystem` slot and `HelpTextSystemInterface` front our help system's texts (`Help/HelpSystem`), which stays
in the script state; `HelpTextSystem` holds its dialogue (see [Dialogue owner](#dialogue-owner)) and the advisors' voices
(`GetVoices`, `InitVoices`: [voices and help](port-decisions-audio-video-scripts.md#voices-and-help)). Taken: `IsStarted`,
`RunText` (which also says whether the text was valid; our help system still logs the invalid one and shows text 0),
`IsTextRead`, `ClearAllText` (our `ClearDialogue`), `CloseDialogue` and `GetNarrator` (the help text table's narrator,
text 0 past the last). Before the help system starts they do nothing and read false, as the natives did. RUN_TEXT,
TEXT_READ, SPIRIT_SPEAKS, GAME_CLEAR_DIALOGUE and GAME_CLOSE_DIALOGUE go through the slot; the other text natives and
the game loop still reach `help::Get()`. `InterruptAdvisors` is taken too: it interrupts the good advisor, then the
evil one, as the help system's click and the dialogue's release did through their own hook, which now calls it. So is
`Layout`: the overlay's fill takes the frame to draw from it (our help text display's cached layout, with the help
system's draw flags), keeping its own film gate; the frame is still drawn by `Renderer::DrawHelpText` at the
original's place in the frame, and his `GameInterface::DrawDialogue` is not taken. Not taken: his `Start(texts,
screenHeight)`, `Reset` and `Update(Frame)`, which start, reset and run his `DialogueText` and `AdvisorVoices`; our
start and per-frame click handling stay where they are until the speech wait and the dialogue draw are measured.

#### Help text database

His `TextDatabase::AddHelpScript`, `GetHelpText` (his `HelpText`: `narrator`, `important`, `text`), `GetHelpNames` and
`GetHelpTextCount` are taken, and `GameInterface::Create` reads the first script with `AddHelpScript`, as his does. The
numbered texts are read by our help texts' reader (`helptext::Parse`), so they keep our load conversion ("~" the fixed
space, a backslash and n a line break, as the original's reader does) and −1 for a narrator the script does not
declare, where his keeps the text as it is written and gives such a narrator 0. By name the help texts are read as
`AddScript` reads every script, unchanged: his reader's string that goes on over a line break is not taken. His two
tests are taken with our expectations: "One\nTwo" is a line break by number too, and the text whose closing quote is
on the next line has its number but no text.

#### Help texts served from the text database

`helptext::Get`, `GetEntry` and `Count` read the interface's text database once the help texts' slot has it
(`SetTexts`/`GetTexts`, ours: his `Start(texts, screenHeight)` also starts his dialogue, which ours starts in
`help::Start`). `Game::CreateInterface` gives it the database after the interface is made, and `ShutDownGame` takes it
away before the interface goes. Without it (no interface, the unit tests) the help text cache is read as before, so
the cache stays as the fallback (see Pending) and for the patch texts. Both read the same bytes with the same reader,
so every text, narrator and name is the same. `GetEntry` returns the entry by value, its first argument 1 for an
important text (only compared with 0).

#### Dialogue text

His `help::DialogueText` (`Help/DialogueText`) names the text part of our help system, moved out of `HelpSystem`
unchanged: the six texts, the history, the reading time, the click, the icons, the speaker override, the read speed
and the profile's draw flags, with `CountWords`, `ReadSpeedFactor`, `RouteOf` and `ApplySpeakerOverride`, and his
`k_HelpTextCount`, `k_NarratorGoodAdvisor` and `k_NarratorEvilAdvisor` (our help text constants). `HelpSystem` holds
it and forwards every text method to it, in the same order, so its callers, its `Queries`/`Hooks` and
`test_help_system` are unchanged; it keeps the wide screen, the help switch and level, the category turns and the
message sets. Its constructor moves the dialogue's queries and hooks into the dialogue, which asks it for the script's
wide screen through his `scriptWideScreen` query (so `HelpSystem` is neither copied nor moved). His names taken:
`RunText` returns whether the text was valid, `ProcessClick` (our `ProcessInterface`, which keeps its return and reads
the skip key through its query), `Layout`, `GetCurrentText`, `IsWaitingForClick`, `GetEndTurn` and `GetEndMs`. Ours
kept: our `Queries` and `Hooks` (info.dat's reading times, `MsPerTurn`, the film and playback gates, the voice table),
`Draw3D` with its click cue icon, TEMP_TEXT, the history and the speaker override. Not taken: his `Settings`,
`AdvanceClickCue`/`GetClickCueShare` and `IsDrawn`. His `test_dialogue_text` cases are taken on our
`help::DialogueText` (`test/test_dialogue_text.cpp`): his settings are our info.dat values, `SetReadSpeed` and the
TEXT_DRAW value the display's gate is asked with, and his key is our skip key query; his continue cue case is not
taken, as ours is the input prompt icon the dialogue makes as it draws.

#### Dialogue owner

The dialogue is owned by the `helpTextSystem` slot, as in his tree, with his `GetDialogue` (nothing before the start).
Its lifetime is ours: `help::Start` makes the help system, whose constructor makes the dialogue at the same point as
before and hands it out (`HelpSystem(info, queries, hooks, dialogue)`), and gives it to the slot (`Start(dialogue)`,
ours; his `Start(texts, screenHeight)` makes his dialogue itself); `help::Shutdown` takes it away (`Shutdown`, ours)
right after the help system, still before the services go. His `Reset` (the texts and voices when a land opens) is not
taken: the help system's reset, which the new land's preparation runs as before, still clears the dialogue in its own
order (the texts, then the wide screen, then the icons and the history). `IsStarted` and the slot's text methods read
the dialogue they hold, so they see what `help::Get()` saw. `HelpSystem` keeps a reference and forwards to it, so its
callers, the help scripts and the tests are unchanged; the natives TEMP_TEXT, TEMP_TEXT_WITH_NUMBER and
RUN_TEXT_WITH_NUMBER, the game's click, the text click test switch, `Draw3D` and the read speed reach the dialogue
through the slot.

#### Intro service

His `introSystem` slot and `IntroSystemInterface` front our opening specials, whose state stays with the world
effects, so `IntroSystem` holds nothing; the stateful part moves from `ECS/IntroSpecial` into
`Implementations/IntroSystem.cpp` unchanged. Taken: `Play`, `ReleaseAll`, `HasLiftFinished` (our pick-up clip's wrap)
and `Update`, which hands the SuperVillagers the hand's grip and then steps the specials, the two calls the frame made
one after the other. Ours next to his: `FillFrame`, the light's entry of the frame's overlay, instead of his
`GetLightFrame` and `GetLightSortPoint` (his frame holds his sprites). PLAY_JC_SPECIAL, IS_PLAYING_JC_SPECIAL,
THING_JC_SPECIAL 18, the frame and the script reboot go through the slot. Not taken: his `GetCameraView` (our specials
move the script camera's debug view themselves) and `GetState` (the inspector is parked), his `Components/IntroHand`
(our hand is an entity with our components), and his light's look, which is to be shown bit-exact on the Land 1
opening first.

#### Tip bubble service

His `tipBubbleSystem` slot and `TipBubbleSystemInterface` front our did-you-know bubble (`help::Bubble`), which leaves
the help system. `Show`, `Hide`, `GetSign`, `GetText`, `ProcessTurn` (our on-screen test of the sign), `UpdateFrame`,
`Hover`, `IsUp`, `GetDisplayTime` and `GetAnchor` (the sign's height, as before) are his names over our code. The tap
that opens and closes it stays with our script highlights, written as his caller is. His `GetScroll` and his
`Help/TipBubble` maths are not taken: ours stays behind `GetBubble` until each difference from the original is
measured and changed in its own commit. `Reset` is there, but nothing calls it yet. Our words are laid out by the pure
`help::LayOutBubbleWords`. His rules taken into it or into our layout, each in its own commit, where his `LayOutWords`,
`FarSizes`, `BoxShape` or `DrawTipBubble` already has the original's:

- the middle part a single space, so a blank line stands between the tip and the title (his `DrawTipBubble` items);
- each line centred across the text area (his `AddLines`, under `LayOutWords`);
- the shadow's bounds, its fades, clip and start included, all two pixels down and right (his `shadowBounds`);
- no more parts once one has reached above the box's top, its height counted (his `LayOutWords` loop);
- no line drawn where a line cannot fit, nor once one starts below the clip (his `AddLines` guards and stop);
- the far margin half the width × 1/35, not half the height (his `FarSizes`);
- the box's and the tail's corners cut to whole pixels (his `BoxShape` with `Pixel`), the alphas still ours: byte
  steps taken at the uncut heights, as the original;
- the bubble laid out wherever the sign's point falls, past the screen's edges too, given up only nearer than the
  near clip (`help::AnchorOnScreen`; his `DrawTipBubble` gives up only behind the eye).

#### Player profiles

His `PlayerProfiles` (the list, its text file `Profiles.txt` in place of the original's registry, the creature file's
scrambled name, the folder key) is taken as he wrote it, behind our existing `playerProfileSystem` slot, which is the
one owner of the profiles, their folders and their files (no second system, no `profileFolderSystem` slot). Refits:

- **Where they are kept.** A person's profiles are kept in `Profiles` under the program's working directory (his dev#65),
  never in the game's folder; `--profile-folder <dir>` keeps them in that folder, as a player's, whatever the run. His
  user-data folder (#185) and his `creatureMinds` folder are not taken. A kept profile's creature (its new
  `C<scrambled>.erc` and the physique beside it) is read and saved in the profile's own folder,
  `<cwd>/Profiles/<key>/` (`player_creature::CurrentMindFolder`), so nothing new is written into the game's folder
  (decision, 2026-10-17); the creature-file setting's creature (no profile played, a run of the checks) stays in the
  game's `Scripts/CreatureMind` as before.
- **A run of the checks** (the one check-run flag, `EngineConfig::checkRun`) plays one profile made in memory from the
  command line, in place of his "Developer" profile: the greeting's name (`OPENBLACK_PLAYER_NAME`, else the login name,
  else "Player"), the `--creature-file` file, symbol 15, the game's own settings with the hand as drawn. It reads and
  writes nothing, so every check is as before.
- **Files** go through the file system service (`Exists`, `ReadAll`, `resources::WriteFile`, and `MakeFolder`, a
  non-pure virtual with a default that makes nothing), never through streams of its own. A folder that cannot be written
  is reported and the profiles stay in memory for the run.
- **The scramble's random numbers** come from `game_random::crt::Next`, the C runtime's rand on a seed of its own, never
  from the game's stream (which is in the state hash).
- **The creature file** the game reads is `player_creature::ProfileCreatureFile()`: a kept profile's own, else the
  `--creature-file` setting.
- **Lifetime.** Made once at the launch (`InitializePlayerProfiles`, after the game path is set); a restart and a new
  game keep it, only the program's end takes it.
- **The profile's folder** (`PlayerFolder`, `CurrentPlayerFolder`, `SavedGamesFolder`, `ReadPlayerFile`,
  `WritePlayerFile`) is the one place later code keeps a player's files (the creature's web page and pictures, the help
  statistics): none in a run of the checks, so those runs write nothing with no special case. The vortex's saved lands
  do not use it (they stay under the game path).

A value the original reads from the player's profile is read from the profile system
([where state lives](refactor-conventions.md#where-state-lives)).

### The hand

#### The hand's pure pieces

They take his files and names over our formulas: `3D/HandCrossFade.h` (the state blend, owned by `HandSystem`),
`3D/HandMorph` (`hand_morph`, the alignment look, a member of `HandSystem`), `3D/HandOrientation` (the heading along a
ray), `Magic/HandMotion` (the grain's smoothers, `CubicSpline` and the pour) and `Magic/HandHoldPose` (`hand_hold`).
His tests come with them, with our numbers where ours differ: hold times are not rounded, the spline is natural, and
the hand system blends the skin at the drawn alignment (it reads only whether `Advance` asks for a blend, so his
`Advance` and its test are taken). Left ours, because his would change what is drawn: his `Components/HandMorph.h`
(ours lasts the whole game, not one land), where the influence is tested (ours uses the interaction point; his pure
`NextPoint` and its test are taken, with no caller yet), `TurnToHeading`/`StandOnSlope` (our `HandMatrixRotation`),
`HoldCycle`/`HoldFrame` (ours picks the clip by name) and `HandFade` (the state blend already does it). His
navigation poses (H1) are taken as `3D/HandNavigationPose.h` under his names (`Pose`, `WhileDragging`,
`WhileHovering`, `UpTowardsFocus`, `k_RotateLiftShare`), with the exe's lean of 0.3 (his 0.4) and its zero way at the
focus; his glue in `Game.cpp` (`PlaceHand`, `UpdateHandNavigation`, `OrientHand`) is not taken, as `HandSystem` places
and turns our hand (`NavigationPose`, `HoverPose`, `Place`).

- The skin blend is his: `creature_skin::BlendWeight` and `BlendTexel`. Our masked 4444 formula gives the same texel
  for every weight and the same weight for any alignment the hand is drawn at, pinned by a test.
- The morph update is his `HandSystemInterface::UpdateAlignmentMorph(picked, holdPoint)`, with our body. Ours reads
  neither argument and is still called from the hand's `Update` with `(std::nullopt, false)`; testing the influence
  at the picked point, and calling it from the game's frame, are behaviour changes of their own.

#### Hand grab

His `handGrabSystem` slot and `HandGrabSystemInterface`, served by our `HandSystem` (one object, the second slot not
owning, as the gestures' two) until his `HandGrabSystem` and its world are taken. Declared, as they map one to one:
`GetHeld` (our `GetHeldObject`) and `ForceDrop` (our `ForceDropHeld`). Ours counts a spell seed in the hand as held,
which his leaves to the magic, so code taken from his tree that calls `GetHeld` is checked for seeds. His press,
release, frame and turn entry points are not declared: ours decides them inside the hand's frame update and applies
them as packets at the next turn, and his `ProcessTurn` would share one override with our `HandSystem`'s method of the
same name, which does another job.

His pure rules, `Hand/HandGrabRules` (`hand_grab`), and their test are his text. One of ours is replaced by his only
where the two do the same arithmetic in the same order: `IsThrow`, `PotPours`, `ElapsedMs` and `k_GrabWaitMs`,
`CountDown` (the release impulse) and `CursorRaise`. The others stay ours until a measured change: the rock's 3.6 (NaN) and `HandSpring` (its step). His `ScoopAmount` (double, whole turns), `HandRise` (nothing for the None and Fingers holds, where ours is `HoldLift`), `ReleaseSpinTorque` (another order of the products), `IsFastRelease` (another order of the sums) and `HoldOf` (its poo hold is the original's, now ours too; its toys go by mesh where ours go by row), never called, are removed with their tests (2026-10-25): the hand's own are the original's.

#### Giving the held object to the creature

His PR #387 gives the hand five more methods on `HandGrabSystemInterface`, each with a default: `HandOver`,
`TakeIntoHand`, `GiveHeldTo` (the last two for the testbed), `GetOfferedCreature` and `GetOfferTurn`. Our `HandSystem`
serves them (`HandGiving.cpp`); his `HandGrabSystem`, `GameHandGrabWorld` and `components::HandGrab` are still not taken.
His per-frame offer, press and reach are a pure step of his rules, `creature_giving::HandGiving` with `KeepOffer`,
`GiveTarget`, `Given` and `ReachFrame` (`Creature/CreatureGiving`), which `HandSystem` calls and his hand tests are
ported against. Our hand's rule holds: the frame decides, the turn applies. The give is the Give packet (0x1F: the
creature, the held object, the hand and the camera's point on the ground), whose handler calls the mind's
`TakeFromHand`; the call-off when the hand goes 15 creature sizes away is packet 0x52 (`GiveCallOff`), whose handler
cancels the reach and abandons the creature's action. Since 2026-10-19 the giving does not end when the creature no
longer means to take the thing (his rule, and his "until the turn after the Give was sent" count, are gone): as the
original's, it lasts until the take, the call-off or the creature's end, a press gives again meanwhile and a creature
hovered over that would take it becomes the one given to (ours wins; [giving](../hand/grab-hold-and-throw.md#giving-a-held-object)). The offered thing's roll goes into `HeldSway`'s Zoomer term, not into his `Game.cpp` tilt; the hand
drawn to the creature's hand is the holding grip in `Place`, from the ground point before the hold's lift. The
creature's take is its object action's moment at turn time (`Moment`), not his per-frame `LateUpdate`. The give's
verdict asks `CanPickUpIgnoringMap` (the original's test of a creature able to pick a thing up wherever it is), since
our `CanPickUp` refuses anything off the map, as a thing in the hand is; his took only the thing. The creature's help
message for a thing it can't pick up (7, "too big to handle") is raised through the creature help, his #353
(2026-10-17), which decides whether it is told
([the creature help in openblack](../creature/mind.md#the-creature-help-in-openblack)).

#### The scoop's hand

The scoop is HOLDING (`HandSystem::RequiredHandState`), as in the original. His hand grab rules kept the unused
state's numbers (`hand_grab::k_ScoopHoverAbove`, `hand_grab::ScoopTip`), which openblack had as he wrote them and
used nowhere; they were removed with their test on 2026-10-25. His `HandGrabSystem` hovers the scooping hand 3 above its source, tips it (`GetScoopTip`) and calls
`PinCursor` for the scoop; the original does none of these. His pin is not taken: the original pins the cursor's
image only in the totem's state, on the hand's point, which ours does (`HandSystem::GetCursorPin`, 2026-10-22, see
[the village totem](../magic/village-totem.md#openblack)); openblack draws no cursor image while the hand is shown, as
the original; his `GameActionMap::GetCursorImagePosition` has no reader at his 0222f9f5 either. The multi pick-up's grip cut is not
ported (the state never runs).

#### The left-handed hand

His `EngineConfig::rightHandedHand`, the debug bar's Hand menu and the options box's left-handed box kept in step with
it. A right-handed hand mirrors the model's x scale, which our hand applies where it sets the scale
(`hand_detail::HandModelScale`) rather than in Game. Ours defaults to off, unlike his: our hand has always been drawn
as the mesh is, and the mirror is to be checked against the original before it becomes the default. His seed-holding
pose's handedness is not wired: our hold pose doesn't use it yet.

#### The hand's clicks, ours behind his component

What the player last clicked is his `components::HandClicked` (`ECS/Components/HandClicked.h`) on the hand, which
`HandSystem::Initialize` gives it and a new land carries over. His pure `hand_click` rules (`Hand/HandClickRules.*`)
are what the hand and the natives use. Ours stays where his differs: the turns gone by are counted unsigned, and a
release over no land clicks the map's corner.

#### Hand demos, ours, with his click retarget

The hand demo player stays ours (`Input/HandDemo`, records parsed by us, driving our `HandSystem`); his
`HandDemoSystem` and `HandGrabSystem` are not taken. His recorded throw (PR #333, the velocity only, as
`HandGrab::recordedThrow`) is ours already and more complete: every message but a mouse move gives the hand the whole
recorded block through `HandSystem::SetThrowBlock` (velocity, spin, hand position and angles), and the 124 cap is the
throw data's. His click retarget (PR #361) is taken as pure rules in `Input/HandDemo` (`ClickCanRetarget`,
`TargetOfClick`, `TakesRecordedClick`, `OutcomeOfFind`): the playback keeps the frame's action click
(`hand_demo::TakeClick`) and the hand applies it to its object under the cursor (`HandSystem::RetargetDemoClick`,
`_cursorObject` and `_cursorObjectDistance`) before the screen object and the press read them. His wrote
`PickingSystem`'s pick (`RetargetObject`), point included; ours leaves the action's point where the hand put it. The
search is our `map_cells::FindNearForScript`, measured to `ScriptDistancePoint` (a worship site from its totem), with
the scripts' type filter. His TODO is settled from the exe: a find that is not an object leaves the click as it was,
and an object the hand cannot act on (`HandSystem::Interactable`) makes it land on no thing.

### The camera

#### The helper icons by the hand

His `src/Hand/HandTricons` and its test whole, as of his 9b8718f30 (his commits 5e023ea96, 03c82e38e, 47a9f0616 and
9b8718f30, authorship kept). His hooks are refitted, not taken: his `Game::UpdateHandTricons` and
`GameInterface::DrawHandTricons` are our `Game::FillHandTricons` (into `OverlayFrame::handTricons`) and
`Renderer::DrawHandTricons`; his `UpdateIconFrame` is our `_iconFrame` (`hand_tricons::WorldCameraFrame`), filled at the end of
our `DefaultWorldCameraModel::HandleActions` from our own fields (`_rotateAroundDelta`, `_gripping`, `_gripOnly`,
`_dragGivenUp`, `_fight`, and the keys' modifiers, `_keyIconHints` from `ModifierIconHints`); his "the hand holds
nothing" test is the original's hand-state allow-list (`hand_tooltips::ShowsCameraIcons`: holding something keeps the
icons unless it scoops); his influence at the cursor is our interface's influence at the hand's point; his anchor
at the posed hand's wrist bone (`AnchorPoint`, 47a9f0616) gives way to the original's, the hand's point projected (its
own commit, so his could be switched back). His demonstration mouse is drawn by the icons while a hand demo plays
(2026-10-21), on the side his `LabelOnLeft` keeps, placed by our `PlaceDemoMouse` and its buttons lit by our
`StepLitButtons` from the presses the hand took (both in his file); his `LayoutDemoMouse` is not called. His later 4:3
and Hor+ camera commits (c008d0713, 2b4c69284, 8dafed687) are not taken. Deviation 11 (the 4:3 sizing) is taken by
the user's choice of 2026-10-20.

#### Near clipping

His `Camera/NearClipping.h` whole (`NearPlane`, (h·0.05)·3.2 + 0.3, the original's float steps) and his close
clipping (`SET_GRAPHICS_CLIPPING`, `CinematicDirectorSystem::SetCloseClipping`) in our `Game` update; the projection
is made again whenever the plane changes, as the original sets it every frame. His `Camera::SetNearClip` and his
path-camera close clipping are not taken (the path's own near plane:
[Placed paths](../camera/script-camera.md#placed-paths-cameramodepath)).

#### Camera help

His `cameraHelpSystem` slot and interface over our `CameraHelp`, keeping our bits 0x10 (double-click flight) and 0x20
(second zoom), as the original reads them
([Player camera features](../camera/player-camera.md#player-camera-features-camerahelp)), where his has fights and
turning around the mouse.

#### World camera

`DefaultWorldCameraModel` takes his `_features`, `GetZoomScale`, `k_PitchPerInput` and `GetHandCues`,
`camera_help::AutoPitchInput` and his hints and drag state (which the hand reads through `GetHandCues` for its poses,
tooltips and helper icons) over our feature bits, help feeds, map disc and woosh (M15). The land grip takes his
`LandGrip`, `UpdateModeDragging` and `camera_pan::Pan`; the stop short of the land goes through the picking slot's
`LandOrSeaAlong` (our cell walk, the original's own land cast, with the 7500 sea rule:
[the land line's callers](port-decisions-engine-world.md#the-land-lines-callers-on-his-picking-slot)) rather than his
physics land cast and `SeaHit`, and the give-up first, with the original's flight. The edge and pitch
drags act through his classifier, `EdgeRotate` (with `WarpCursor`) and `PitchStep`, plus the original's 0.45 edge in
a window (our display mode), its RotateCCW report on still frames and its feature gates. Auto pitch is his
`AutoPitchInput` and height, with the original's fifth, dropped player tilt and height rule. The wheel is his 60 a
notch from `GetMouseWheelDelta`, read only with a zoom action, as the original. Both buttons take his `TwoButtonTurn`
and cursor freeze call site, gated with the middle button on our bit 0x20. The clear view takes his half-second
zoomer, on our `Zoomer`, and adds the original's glide to the hand's point. Watching a fight is ours
(`camera::FightWatch`, `fight_orbit::`), his `DuringFight` mask under our bit 0x10. His
`DefaultWorldCameraModel(origin, focus)`, a model starting at the pose it takes over, is taken as he wrote it, with
our fighters' radii; nothing makes one yet. His `test_camera_handback` is taken for the model alone (his
`ScriptControlSystem` hand-back is not ours yet), with his `NoLandPicking`, a `MockPickingSystem` with no land under
any pixel, in the picking slot. The original's rules are in [The player camera](../camera/player-camera.md).

#### Placed camera paths

His `CameraPathSystem` slot keeps `HoldsCamera` and `HandlePlayerControl`, with the path as an `entt::resource` from
the camera path cache rather than a path read by the caller. His `FollowPlaced` is reshaped to the original's camera
mode: `Begin` (refused while a script holds the camera), `FollowAt` (the path's time from the animation's drawn
frame, given by its owner every frame) and `Release`, on the camera's own zoomers. Not taken: his own clock, his end
at 999/1000 of the clip's ms and his end with the miracle. The original's mode:
[Placed paths (CameraModePath)](../camera/script-camera.md#placed-paths-cameramodepath).

#### The camera editor's file and its tracks

His `components/edt` reader, his `3D/CameraEdits` (the file in the resource cache, `camera_edits::FindCamera` and
`FindTrack`) and his `3D/CameraTrack` functions (`Bezier`, `WayRunner`, `StartWalk`, `WalkTurn`, `Percentage`,
`WayLength`) take the place of our `3D/CameraTracks`, which did the same sums. A way's length is ours: summed in
single precision as the original sums it, where his sums in double precision. Where a walking thing is drawn stays
ours (`Transform` the turn's position, `DrawPosition` the frame's), since the state hash reads every `Transform`.

The things a script walks along a track are his `WalkPath` component and `WalkPathSystem` (slot `walkPathSystem`), its
turn ours: a thing marked for deletion does not walk, and nothing is set dirty. A villager's walk keeps a type of its
own (`components::LivingWalkPath`, his `WalkPath` inside) until the villagers' walk along a track takes his `living`
flag. The file and the walk: [The file](../formats/land-and-script-files.md#the-file).

#### The camera scenarios' mock

It answers the land under a pixel through his `LandAtPixel`, keyed as before by the recordings' screen pixels (our
`RecordedMockPickingSystem`, its branches and its frames).

#### The camera zones: his port against the original

His candidate (`src/Camera/CameraZones.cpp` at his 0222f9f5: `HeightLimit`, `SlideUnderLimit`,
`PushInsideFence`, `FlightOriginInsideFence`, `ScriptInfluenceCounts`; callers `ConstrainZones` and `SetFlight` in
`DefaultWorldCameraModel.cpp`, `FenceHit` in `CameraZoneSystem.cpp`), compared step by step with the original's
`CameraModeNew3::Update` ([what the player camera does with the zones](../camera/player-camera.md#what-the-player-camera-does-with-the-zones)).
Of it, the fence is taken (2026-10-21): his newer `PushInsideFence` and fence hit where they match the exe, with the
angle, the sines, what moves with the camera, the not-a-number cases, the inclusion distance and the help report as
the exe has them; the force feedback and the force field's pulse are not ported, and the 50 radius still keys on his
edge-rotate drag ([the player camera in openblack](../camera/player-camera.md#openblack)), and the map's disc is the
original's (2026-10-22: `camera_zones::KeepInsideMapDisc` in `ConstrainDisc`), though still run in his place in the
order, before the fence. The rest is not taken yet; these are the differences found, to fix when it is ported:

- **Order:** his Update runs ConstrainCamera (ConstrainAltitude, ConstrainDisc, the move threshold) → AutoPitch →
  ConstrainZones (PushInsideFence / FenceHit → ConstrainAltitude → the slide). The fence → lowest height → slide order
  matches; the disc clamp and the move threshold come after the slide in the original, before the fence in his.
- **HeightLimit:** the arithmetic is identical except for NaN (the original keeps the NaN sum, his `a <= b ? a : b`
  gives the highest). The ground must be `GetAltitude(MapCoords)` with the ftol quantisation at p of step 1, not
  `GetHeightAt(_targetOrigin)` after the fence push; his `k_FloatingHeight` is 2.9999 where the original adds 3.0.
- **SlideUnderLimit:** `k_SteepEnough` is 0.01 in the original, not 2.0; the original also moves the mode's focus point
  (his `_focusAtClick`); the limit is step 1's.
- **PushInsideFence:** the angle must be rounded to float (`static_cast<float>(turn) * k_SearchStep`), not a double
  product; sin / cos are the x87's, rounded once (his double-rounds; to pin with the oracle: 63 angles × 2 radii);
  the push sets p.x and p.z exactly and moves the focus by (f − p_old) + p_new on all three axes (his adds the push to
  both, y untouched); the original also shifts `_focusAtClick`; the radius 50 keys on drag state 4, not on
  `_dragging && mode == EdgeRotate` (unknown whether his EdgeRotate is exactly state 4); NaN cases differ.
- **FenceHit:** the original has no hit counter (`GetFenceHits` has no counterpart): it keeps the inclusion distance
  0xC5E13C, read by GET_INCLUSION_DISTANCE; his has no such value. StartImmersion (force feedback) and CameraHelp 0x202
  are missing. The order (sound → shake → held back → pulse) differs with no visible effect. His `LightSheet::Pulse`
  matches `PulseForceField` except for a NaN distance. The listener (`LH3DTech::GetCameraPosition()` against his
  `camera.GetOrigin()`) is the same if that is the drawn camera. His force field's 256 points, 0x808080 and 250 high
  were not compared.
- **FlightOriginInsideFence:** the normalisation sums (z² + y²) + x², his (x² + y²) + z²; NaN skips it in the original.
  His `SetFlight` always plays the woosh; the original only beyond 150. The flight is not started when t < 0 (his has
  no t). Tick source GetTickCount & 3 against his `machine_clock::Ticks() & 3` (unknown whether the same clock).
- **ScriptInfluenceCounts:** identical; the point must be float(double(int) × double(10/65536)). No caller in his tree.

#### The tricons: his version against the original

The icons, their fades, the cross and the demo mouse are his `HandTricons`
([the helper icons by the hand](#the-helper-icons-by-the-hand)), the poses his `3D/HandNavigationPose.h` with the
exe's 0.3 lean ([the hand's pure pieces](#the-hands-pure-pieces)); the hand reads the camera's cues
(`GetHandCues()`), the land tooltips offer Pitch and Rotate from the camera's hints, and the hand demo's records give
the display and the angle. Still open: WITHIN_ROTATION (a stub pushing false) and the wheel gate +0x2EC; of the
active word +0x94 the keys' modifiers and this frame's turn, tilt and zoom are in
([the player camera's Pending](../camera/player-camera.md#pending)).

His version, as compared: `3D/HandNavigationPose.h` and `Camera/CameraDrag.cpp` (`IdleTricons`, `DragClassifier`)
were faithful in the zones, the hover and drag pose order, the 0.33 lift, Cgrip for the pan, the undecided drag and
the clear view. They were not faithful in the up lean (0.4 where the original has 0.3, 0x5B6946, 0x5B0677), in the
feature gate (the default 0x1BF, not the live features), in the hover pose on a full hand, and in the Zoom pose on
k_Zoom 0x08, which the exe never sets. The tricons follow the player's camera mode (`script_camera::HasMode()`), as
the original shows none under a script camera.

### The input

#### The mouse for the camera

The action map takes his `GetMouseWheelDelta` (the notches turned this frame), `WarpCursor` / `GetCursorWarp` and the
cursor freeze (`AllowCursorFreeze`, `IsCursorFrozen`, his `Input/CursorFreeze` and its test), off until the camera
allows it. In the test runs (fixed mouse, ignored real input, `OPENBLACK_MOUSE_AT`) a warp or a freeze moves only the
game's cursor, never the real pointer. Not taken: his scripted pointer; our testbed moves the cursor through
`OPENBLACK_MOUSE_AT` and presses the hand's buttons itself (`testbed_pointer`), with no hook in the game. The world
camera uses them since M15: the edge rotate warps the cursor, the cursor stays where the middle button or both
buttons were pressed while they are held (the game allows the freeze for the world camera only), and the wheel zooms
by its notches ([World camera](#world-camera)).

#### The one-press keys

His `Input/ShortcutKeys` (the temple and realm keys, the villagers' names), called right after the temple's room
keys, as his, and made to do what the original does (behaviour, keys only). The decision is a pure function,
`ShortcutActionsFor`, tested with fakes like `SpeedKeys` (`test/input/test_shortcut_keys`): no key acts while a
script holds the cinema bars (`HelpSystem::IsScriptWideScreen`, the names key too) or inside the temple; the temple
and realm keys also ask the script's interface interaction (`IsActionBlocked`, by the action's place in the options
list, which is the original's). The realm key flies over the middle between the first and the last blocks' corners
(`RealmMiddle`), a single tap while a fight is watched does not move the camera (`TempleKeyMovesCamera`), and
`ZoomToPlaces` takes his `IsDoubleTap`, the original's floats for the temple and tap pitches, and ends a tap when
either key flies back. A tap's time is `zoom_to::TapTime()` (the engine's frame clock). Left out: his log line for a
key whose feature isn't built (the original has none), his villagers' details key (the original's S key is a
per-villager toggle, not ported; we have no `showVillagerDetails`), and his help profile's count of the flights, which
needs his profile. What still differs from the original is in
[The one-press keys](../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names).

#### Blocked actions and missed lettings go

The action map takes his `SetBlockedActions` (blocked actions read as not held, nothing blocked by default), his
`PointerState` and his making up of the key and button releases that went elsewhere (`ReleaseKeysNoLongerHeld`, and
the buttons the menu or a debug window took). `PointerState` keeps our test-run reads: under a fixed mouse or ignored
real input it reads no real button and gives the fixed or the game's own cursor. The made-up releases only run outside
the test runs (no fixed mouse, no ignored input, no `OPENBLACK_MOUSE_AT`), so the runs read no real key or button
state.

#### The script's blocked actions

His name `BlockedActions()` and his feed in `Game.cpp` (every frame, just before the action map's `Frame`) are taken,
with his two masks. The switches stay in `help::interface_interaction`, which also gives the set at once on `Set` and
`Reset`: his `CameraHelp::SetInterfaceLevel` is not taken, because its reach table (levels 2, 5, 11 and 15 leave the
reach, where the original sets 1800) and its missing level and interface flag writes differ from the original's
SET_INTERFACE_INTERACTION.

### Drawing, the map, picking and physics

#### Instance inputs

His `test_instance_data_inputs` and the reader it needs, `shader_samplers::ReadVertexAttributes` (with his
`Reader::Skip` and `SkipToAttributes`, copied as he wrote them). The source checks are his. The Direct3D check lists
only the instanced shaders we have, and asks each for the instance columns its source takes: the temple's lightmap
shader takes four, so it has no fourth texture coordinate. The file's source-folder definition is its own, so it is
built without its group's precompiled header.

#### The object draw list, ours only

The pure stages are in `ECS/DrawList/` (no Locator), the state is a service (`ObjectDrawListSystemInterface`, slot
`objectDrawListSystem`) whose `Update` takes everything it reads as arguments, so its tests build it directly. What belongs
to an object stays on the object: its listed mark is the component `DrawListed` and its DontDraw mark the component
`DontDraw`, so both go with the entity and an index made again is a new object. The objects reach the service through
a probe (`draw_list::RegistryObjects` over the registry it is given, a fake in the tests), and an object's Draw is a
consumer the caller passes per type.

#### The map's cells

`entitiesMap` takes his `MapInterface` shape: `GetFixedInGridCell` and `GetMobileInGridCell` as spans,
`GetAllInCell`, `Sync` (was `Rebuild`) and `Refile`, and the registry has his `OnConstruct`. Behind them
`MapProduction` reads our ordered linked lists (`ecs::map_cells`), which our hooks keep as things are made, move and
go; his `CellLists` and his registry listeners are not taken. A span is a copy of the cell, kept until the next call,
since our lists run through the things in them. The walkers' obstacle grid of his tree (`FileObstacles`,
`ObstaclesIn`, kept for a while in our pathfinding) is gone: the walkers read the cell lists and the things' collide
data through `thing_circles` (his name: `thing_circles::CirclesOf`, over our `map_cells::CollideDataOf` rather than
his model boxes), as the original does. Not taken: his second `Sync` after the living's turn. His walkers are never
refiled as they move, so his map needs it; the original refiles each thing in its own move, and so does ours (the
walkers as they step, the animals, the camera-track walk and the creature's pose), while the other movers wait for the
next turn, so that `Sync` would be a change of behaviour. Not taken either: his map reset before the registry (our map
listens to no registry signal, so his reason does not apply, and nothing shows that the registry's teardown never
reaches the map). His `test_map_cells` tests his `CellLists`, so it is not taken; `test_map_interface` covers ours
under his names.

#### Picking, his interface over our walk and mesh test

`PickingSystemInterface` has his `Pick`, `Frame`, his six methods (`LandAlong`, `LandOrSeaAlong`, `LandUnderPixel`,
`PickUnderCursor`, `GetPick`, `FeelModel`) and his `picking::Locked` / `CarryHover` with his bodies; the eight Bullet
methods went with Bullet ([Bullet removed](port-decisions-engine-world.md#bullet-removed-his-834f657f-under-our-names)).
His `3D/ScreenPick.{h,cpp}` is taken whole; the temple's creature room finds the creature under the cursor through it.
The land along a line is our own cell walk with the height `GetHeightAt` gives, as his. `LandUnderPixel` is his rule
over our walk, kept within reach of the map's middle by his `KeptInReach`, and is the interface's land under the
cursor since that change of behaviour (2026-10-13); the gestures and the console read it too. `PickUnderCursor` still
picks nothing (the hand picks its object along its own ray), and `FeelModel` is served by our mesh test
([feeling a model](port-decisions-engine-world.md#feeling-a-model-our-mesh-test-behind-his-method)). His
`Camera::OnNearPlane` is taken with no caller. Every fake of the interface answers the new methods without changing
what its tests do.

#### The turn manager, ours behind his interface

Our physics objects' functions are the methods of his `DynamicsSystemInterface`
(`ECS/Systems/DynamicsSystemInterface.h`) and `Implementations/DynamicsSystem`, in his slot `Locator::dynamicsSystem`,
emplaced and reset where the old `physicsObjectsSystem` slot was, which is gone. Where he has the same function his
name is taken (`ProcessTurn`, `ResetSimulation`, `WeightOf`, `EndPhysicsAsObject`, `RaiseClearOfWhatIsUnder`,
`ForEachEntry`, `InitialisePhysics` for the start from the hand, `Find`, `IsFlying`, `RemoveObject`), with our
signature and our body; the methods only we have (`ClassHooks`, the constants, the release prediction, the dropped
objects, the catching hook, `PushObject` with its pusher) sit next to his in the same interface. His methods that we
do not implement yet are not declared until they are ported. A body is his `ecs::PhysicsEntry`
(`ECS/PhysicsEntry.h`, with `PhysicsClass` and `ImpactInfo`), our fields unchanged, and the draw pose is in his
`ECS/Components/Physics.h`. The state stays one `physics::State` member of the system, reached through `GetState()`,
because the start from the hand still works on it from its own file; the bodies stay the same ordered list, so the
simulation's order is unchanged. Our collision sounds and dust puffs are in the system's own file: the puffs go in
through his `AddPuff` (his parameter order), and ours only sit next to it (`SyncedPuffVelocity`, `ClearDust`,
`SnapshotDust`, `PlayAnimEffect`); `PhysicallyDestroysAbodes` is on the interface, as in his tree.

### Test hooks

None of its own: the debug controls of these systems are listed in [Debugging](debugging.md).

## Pending

The "not taken" items above that are behaviour changes, each waiting for its own measured commit:

- His tooltip glow and arrows.
- The big buttons' label size for looks other than 2 (the caller's size, as the original).
- The world camera's order of the disc and the fence (the original's: the fence, the lowest height, the slide, then
  the disc and the move threshold).
- The four-fifths cue size for the languages that need bigger text.
- His advisors' `Reset`, `GetModel`, `GetDraws`, `GetTrails` and renderer.
- His `ScriptCameraModel` measured against our script camera. His reset of the script control slots as a new land
  opens and his leaving Creature Mode before `START_CAMERA_CONTROL` are not the original's (see "Not taken" above);
  what the original's follow mode does under the script mode is not read.
- His `Start(texts, screenHeight)`, `Reset` and `Update(Frame)` for the help texts, once the speech wait and the
  dialogue draw are measured; the help text cache as a fallback.
- His `TipBubble` maths and `GetScroll`, one measured difference at a time.
- The morph's influence at the picked point and its call from the game's frame; the hand grab rules that stay ours
  (`HandSpring`).
- The right-handed hand as the default, once the mirror is checked against the original.
- `PickUnderCursor` served (the hand still picks its object along its own ray).
- His intro light's look, shown bit-exact on the Land 1 opening first.
