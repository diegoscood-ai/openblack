# Advisors and characters

The good and evil advisors, the player's two consciences, who hover at the edges of the screen, fly out into the world,
point, act and talk in the voices of the game's recorded lines, arguing over what the player should do; and the story's
other speaking characters. Which messages the help system sends is in ../interface/help_system.md; this file is who says
them and how. openblack draws them, moves them and plays their lines (`src/Help`, `src/Graphics/RendererSpirits.cpp`,
`src/Audio/Services/Advisor.h`); what it still lacks is in the rows.

**Progress: 39/71 done, 23 partial — 71%**

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor: a white-robed, bearded old sage with a halo | done | `Help/HelpDudeFile` loads `markgood.hd` (mesh and 80 clips), drawn by `Graphics/RendererSpirits`; test `test/test_help_dude_file.cpp`; see [intro.md](../../bw1-notes/intro.md#the-advisor-spirits) |
| The evil advisor: a small red horned imp | done | `markevil.hd` through `Help/HelpDudeFile`, drawn by `Graphics/RendererSpirits` |
| Their clips are layered and additive (a body pose plus face and gesture layers) | done | `Help/SpiritAnimClip` (set and additive layers), `Help/SpiritsRuntime` |
| They are drawn after the scene, in their own view in front of everything, while near the screen | done | `Graphics/RendererSpirits`: drawn after the scene in their own view while the in-world blend is under a half |
| Sent out into the world they are drawn in the world instead, blending between the two | done | `Help/Spirits` in-world blend; drawn in the world at a blend of 0.5 or more |
| They fade in at three times a second's alpha, out at twice | done | `Help/Spirits` alpha fade, +3 per second in, -2 out |
| The good one glows with a halo; both leave a sparkling rainbow trail and a puff of smoke when they appear | done | the halo (good only), the smoke puff and the rainbow trail in `Help/Spirits` and `Graphics/RendererSpirits` |
| Their faces blink each eye, and show smiles, frowns, sadness, shock, a raised eyebrow and anger | partial | blinks and the face records per emotion are ported (`Help/Spirits`); the emotions are set by the voice tags, which are not fed, so they rarely change; the eye-bone scales are pending |
| Their mouths move with their voice: three vowel shapes picked by analysing the sound as it plays | done | the mouth poses from the advisor lip-sync (`Audio/Services/Advisor.h` `LipSyncThisFrame`, `Help/Spirits`) |
| Gestures and emotions while talking come from cue marks in each line's recording | partial | the tag parser and walker exist (`Help/Spirits.h` `ParseAudioTags`), but the recordings' cue labels are not read, so no tag reaches them |
| They have no idle gestures of their own: between lines they just hover | done | no idle gestures, as in the original: `Help/Spirits` hovers on the base pose clip |

## Where they are and how they move

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At rest they sit at the left and right edges of the screen, a few units in front of the camera | done | `Help/Spirits` rest zones and home points |
| They drift about on smooth curves, kept apart from each other, from the hand and from the middle of the screen | done | `Help/Spirits` splines and the potential field (rest zones, partner, mouse, screen centre); `test/test_spirits.cpp` |
| They dodge left, right, up or down out of the hand's way | done | the mouse zone and the avoid clips in `Help/Spirits` |
| Appearing and disappearing, they flash in and out | done | the appear and vanish puff and flicker in `Help/Spirits` |
| Scripts make them appear and disappear | done | `SpiritAppear`, `SpiritDisappear`, `SpiritEject`, `SpiritHome` in `src/CHLApi.cpp`; seen in the Land 1 intro |
| Ejected, an advisor flies out from its corner into the middle of the view in a second | done | `SpiritEject` (`src/CHLApi.cpp`, `Help/Spirits`); seen in the Land 1 intro |
| Sent home, it flies back to its corner in a second | done | `SpiritHome` (`src/CHLApi.cpp`, `Help/Spirits`) |
| Scripts make one cling to a point on the screen (across, down) | done | `ClingSpirit` (`src/CHLApi.cpp`, `Help/Spirits`) |
| Scripts fly one to a place in the world | done | `FlySpirit` (`src/CHLApi.cpp`, `Help/Spirits`) |
| Scripts make one point at a place or an object in the world, and stop | done | `SpiritPointPos`, `SpiritPointGameThing`, `StopPointing` (`src/CHLApi.cpp`) |
| Scripts make one point at a place on the screen | done | `SpiritScreenPoint` (`src/CHLApi.cpp`) |
| Scripts make one look at a place, and stop | done | `LookAtPosition`, `LookGameThing`, `StopLooking` (`src/CHLApi.cpp`) |
| Scripts play an acted animation: nod, shake the head, laugh, cross arms, scratch the head, pick the nose, sulk, cry, dance, pray, punch the air, cover the eyes … | done | `PlaySpiritAnim` (`src/CHLApi.cpp`, the play state of `Help/Spirits`) |
| Scripts ask whether an advisor has finished its animation | done | `SpiritPlayed` (`src/CHLApi.cpp`) |
| While a dialogue runs both advisors come out; for two lines they cling near the bottom, for more they are ejected | partial | the help scripts (`HelpJustTalkMaybeWithSpirits`, `HelpSpritesTalk`) call only natives our tree has; they are started by the message sets (`Help/HelpMessageSets`) and the challenge scripts; checked in game only in the intro |

## Voices and text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every advisor line has its own recording; the good advisor's and evil advisor's lines come from the advisors' bank | partial | voice routing by narrator in `Help/HelpSystem` (`VoiceRoute`) and `Audio/Services/Advisor`; the dialogue banks load lazily; not checked line by line against the original |
| An advisor starts speaking up to half a second late, the further it is from its rest place | done | `Audio/Services/Advisor.h` `SaySentence`: a delay of up to 500 ms from the advisor's hover distance |
| The line shows in a strip above the bottom of the screen, the good advisor's in pale yellow, the evil one's in pink, others in white | partial | the strip is `Help/HelpTextDisplay` (drawn by `Renderer::DrawHelpText`), coloured by narrator; its look is not yet checked against the original |
| A line is read when the voice has finished (or, without a voice, after a time by its words and the reading speed) | done | `HelpSystem::IsTextRead`: the voice's end, or the reading time from the words and the reading speed; `test/test_help_system.cpp` |
| Clicking ends the line being read | done | `HelpSystem::ProcessInterface` (a click ends the text) |
| Six recent lines stay on screen, older ones shrinking and dimming | done | `Help/HelpTextDisplay`: a ring of six entries, older ones shrinking and dimming |
| Their voice isn't ducked under the music | done | the advisors' sentences play straight on the sample player, outside the sound filters (`Audio/Services/Advisor.h`) |

## What they talk about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Introducing themselves and teaching the hand and camera in the opening | partial | the intro's texts and advisors run up to the hand-over (checked in game); the camera lessons after it (`TeachRotate`, `TeachPitch`, `TeachZoom`) are not checked in game; see [tutorial.md](tutorial.md) |
| Introducing and reminding of challenges and pointing out scrolls | todo | `ChallengeHighlightNotify`, `QuestHighlightNotify` and the reminder scripts call only natives our tree has; most challenges are never reached (Land 1 stops at the creature gate, Lands 2-5 need `LOAD_MAP`) |
| Arguing over each challenge: the good one urges kindness, the evil one cruelty | todo | the challenge scripts' lines use real natives (`RunText`, `SpiritEject`); most challenges are never reached (Land 1 stops at the creature gate, Lands 2-5 need `LOAD_MAP`) |
| Reactions to the player's deeds: killing, destroying, being very good or evil, acting against their alignment | partial | the alignment remarks are wired (`audio::guidance::UpdateAlignmentRemarks` from `ECS/Effects/Alignment.cpp`); the help message sets' deed conditions are not computed (`message_sets::ProcessConditions` only sets the idle one) |
| Town and battle alerts, winning and losing | partial | the belief warning (`audio::guidance::WarnLosingBelief` from `ECS/Town/TownBelief.cpp`) is wired; the town and battle help conditions and the winning and losing messages are not |
| The creature's learning and what it wants | todo | the creature help scripts are in the challenge file, but nothing in our creature code starts them |
| Welcome remarks in the temple and interruptions there | todo | our temple code starts none of the temple's welcome help scripts |
| Rewards and miracle dispensers | todo | no reward is made (`CreateReward`, `CreateRewardInTown` are stubs), so their help never plays; see [rewards.md](rewards.md) |
| The moon's phase on real nights | done | `audio::guidance::RemarkOnMoonPhase`, from the real clock (`Audio/Services/Guidance.h`) |
| Remarks on the land's towns: what a town wants, food and wood given, a town under attack, disciples made, belief | partial | `audio::guidance::UpdateTownDesireRemarks` and `PlayResourceDropRemark` are wired (`ECS/PotResource.cpp`, `ECS/ObjectDelivery.cpp`); the disciple and attack remarks are not checked |
| Rare one-off remarks, once per land, each with a small chance | partial | `audio::guidance::OneOff` exists; only the spooky voices and the moon call it, its other callers are not ported |
| Saying "yes" as the player turns or tilts the camera the way a lesson asks | done | `Audio/Services/Confirmation.h` (the good advisor's yes, never a no, as the original) |
| The audio CD left in the drive | n/a | the game's own disc check |

## Idle banter

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player does nothing for two minutes, the advisors start a short exchange between themselves | done | `help::message_sets::ProcessConditions` (no interface event for 120 s, from `help_profile`) and `ProcessBanter`, each turn from `help::Process` (`Help/HelpSystem.cpp`) |
| Doing nothing means none of the interface events: moving the hand, picking up, catching, throwing, giving, tapping, casting, any gesture, turning, tilting, zooming, double clicking, dragging, the leash, fight moves or asking for help | partial | the idle clock is `help_profile`'s all-interface event; which of our interface actions feed it is not checked one by one |
| The idle clock runs on unpaused game time and stops while the game is paused or a script holds the cinema bars | partial | no idle condition while a script holds the bars (`ProcessConditions`); its behaviour while paused is not checked |
| An exchange is one of 25 banter sets, picked at random among those not yet heard | done | `message_sets::GetRandomBanterSet`: one of the 25 not heard yet, on the local random stream |
| When all 25 have been heard the record is cleared and they come round again | done | `GetRandomBanterSet` clears the record once all 25 were heard |
| Starting an exchange counts as the player asking for help, so the next waits another two minutes of idleness | done | `MarkMessageSetSent` triggers the help query event, which restarts the idle clock |
| Banter needs the help system on and a help level above none | done | `help::Process` runs the banter only while `IsHelpSystemOn` (help on, level above none) |
| It doesn't start while a script's cutscene holds the bars, nor while a challenge script is talking | partial | not while a script holds the bars (`ProcessConditions`); a talking challenge is handled by `StopHelpScriptsForNewHelp`, not checked in game |
| A help message already running is stopped for the banter | done | `script_control::StopHelpScriptsForNewHelp` before a set runs |
| For more than two lines both advisors fly out to talk; for two they cling near the bottom of the screen | partial | decided by the `Banter` help script (no stub natives); not checked in game |
| Each line is said and waits to be read before the next | partial | the `Banter` help script waits on `TextRead`; not checked in game |
| Clicking ends each line; any interface use resets the idle clock | partial | a click ends each line (`HelpSystem::ProcessInterface`) and the interface events feed `help_profile`; not checked in game |
| Multiplayer games have their own banter set of taunts | n/a | openblack has no multiplayer (the `MultiBanter` script exists, but there is no multiplayer game) |
| Whether banter runs in the temple or with the creature followed | todo | unconfirmed for the original; nothing checked in our tree |

## Interruptions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A more important message stops the running one | partial | `StopHelpScriptsForNewHelp` stops a running help script for a new one; the help priorities are not all ported |
| Interrupted, the good advisor coughs and the evil one grumbles a bitter remark | partial | `audio::advisor::Interrupt` is ported; as in the original, the interruption line is never actually said (the sample's progress is read after the stop). Our wiki differs: in the shipped game the interruption line is never said, because the sample's progress is read after it is stopped ([audio.md](../../bw1-notes/audio.md#how-it-works)) |
| Their own lines for being interrupted, and for being interrupted in the temple | partial | the interruption texts are picked in `audio::advisor::Interrupt`; never said, as in the original |

## Spooky voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On real nights (by the computer's clock, from a quarter to nine to nine, and from eleven to six) a voice whispers the player's name | done | `Audio/Services/SpookyVoices`, at real night by the computer's clock |
| The name is the player's profile name, matched by how it sounds against 100 recorded names | partial | the sound-alike match against the 100 names is ported (`Audio/Services/SpookyVoices`); openblack has no profiles, so the name is not the player's own |
| Not on the first two lands | done | `Audio/Services/SpookyVoices` (not on Lands 1 and 2) |
| The chance rises over time and resets after a whisper; pitch and volume vary at random | done | `Audio/Services/SpookyVoices` (the chance, its reset, random pitch and volume) |

## Other speaking characters

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar, the friendly god of the second land | todo | his lines are texts with a voice (`RunText`, the narration route); never reached: Land 2 needs `LOAD_MAP` |
| Lethys, who steals the creature | todo | her lines are texts with a voice (`RunText`); never reached: her scenes are on Lands 2 and 3, which need `LOAD_MAP` |
| Nemesis, the enemy god | todo | his lines are texts with a voice (`RunText`); never reached: his scenes are on Lands 2-5, which need `LOAD_MAP` |
| The creature trainer, the monk, the ogre, the island keeper and the villagers of each challenge | partial | texts with a voice (`RunText`, the narration route); the Land 1 villagers speak in the intro (checked); the trainer is past the creature gate and the others are on later lands |
| A character speaking in a scene turns to face the camera | partial | `SetFocus` turns villagers and animals (`src/CHLApi.cpp`), not creatures or other objects |
| A big booming voice for some lines | partial | the narration route plays any narrator's line (`Help/HelpSystem` `VoiceRoute::Narration`); which lines boom is not checked |
