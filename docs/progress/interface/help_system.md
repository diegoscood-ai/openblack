# Help system

The help the game gives as it is played: the advisors' messages when something happens for the first time or goes
wrong, the answers to the Help key, the creature's learning messages, alerts, banter and tips. Who the advisors are and
how they look and fly is in ../story/; the words on screen are in [on_screen_text.md](on_screen_text.md).

**Progress: 18/29 done, 0 chosen, 6 partial, 3 inferred — 75%**

How the original does it, in our wiki: [The hand and the interface](../../../interface/tooltips.md).

## Running help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help messages come in sets, and a set picks one of its messages at random | done | the set table, and mode 1 picks one text at random on the local stream (`src/Help/HelpMessageSets.cpp`, `message_sets::RunMessageSet`); run by the banter, the Help key and the debug Consciences window (`src/Debug/Consciences.cpp`); test `HelpMessageSets.*`; read in the exe ([advisors and help](../../../interface/advisors-and-help.md#the-help-key-and-the-advisors-remarks)) |
| A message is spoken by the good or evil advisor, or both in turn, with their voice and lip sync | done | `help::HelpSystem::SayText` routes a text to the good or evil advisor by its narrator (`RouteOf`, `SpiritWhoTalks`); the advisors fly and speak with lip sync (`src/Help/Spirits.cpp`, `src/Help/SpiritsRuntime.cpp`, `src/Audio/LipSync.cpp`, `src/Help/AdvisorVoices.cpp`); tests `HelpSystem.VoiceHooks`, `Spirits.*` |
| Only one message runs at a time; a new one stops the running help scripts, and the advisor's interruption line is picked but never said | done | one text and one help script at a time: `script_control::StopHelpScriptsForNewHelp` stops the running help scripts, and a click cuts both advisors (`AdvisorVoices::Interrupt`); tests `ScriptControl.*`, `AdvisorVoices.InterruptDrawsOneLineAndStops`, `VoicesTest.InterruptionIsNeverSaidInVersion120`. The original picks an interruption line but never says it, so the advisor is not heard grumbling ([audio](../../../audio/voices.md#how-it-works)) |
| A message waits until it has been read (by its length and the player's read speed) or clicked through before the next | done | `HelpSystem::IsTextRead`: the click, the voice or the reading time from the word count and READ_SPEED; tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpSystem.ReadWithVoice`, `HelpSystem.WithInteraction` |
| Messages already given are remembered so they are not repeated | done | each set's sent turn and count are kept (`message_sets::MarkMessageSetSent`, the original's SetSetSent), so the banter does not repeat a set until all 25 are used (`GetRandomBanterSet`; test `HelpMessageSets.BanterGoesThroughEverySetOnceThenStartsAgain`); the history of texts is kept (`HelpSystem::GetHistory`). The per-thing memory of the original's field-of-view help is only saved and loaded in v1.42: nothing tells what it picks (see "What triggers help") |
| The help level setting sets how much help is offered, none turning it off | partial 75% | the Advanced page's Help level is passed to the help system as the profile's (`Game::ApplyPlayerSettings`, `HelpSystem::SetHelpLevel`, 3 without a profile); it gates the remarks (`HelpSystem::GetGuidanceLevel`, test `GuidanceTest.PlayNowGates`) and level 0 turns HELP_SYSTEM_ON off, as the exe ([voices](../../../audio/voices.md)). Missing: the field-of-view reset the level change calls, and the creature help's per-kind help level (`creature_help::Decide` does not read it); no test of the menu-to-help path |
| Land scripts turn the help system on and off, and ask whether it is on | done | `HELP_SYSTEM_ON` and `SET_HELP_SYSTEM` in `src/CHLApi.cpp` read and set `HelpSystem::IsHelpSystemOn` / `SetHelpOn` (switch and level not 0, as the exe); tests `HelpSystem.*` |
| Help is held back while the player is inside the temple, and what was on screen comes back on leaving | pending | nothing puts the world's dialogue away on entering the temple; raffclar's #468 does it, not read in the exe by us ([temple interior](../../../interface/temple-interior.md)) |
| Help can take the camera into widescreen bars while it speaks | done | the help scripts' SET_WIDESCREEN goes through `help::script_control` and the `wideScreen` hook to the bars (`ScreenFade::SetWideScreen` in `src/3D/ScreenFade.cpp`, `src/Game.cpp`); test `ScriptControl.SetWideScreen` |
| What help has been given and how often is kept with the player's profile | partial 60% | the help record of 49 events is counted as the exe counts it (`src/Help/HelpProfile.cpp`, `help_profile::Trigger`; tests in `test_timers_events.cpp`), and `helpstats.dat`'s records can be read and written (`components/helpstats`), but the profile's file is not wired: the record starts at 0 every run ([advisors and help](../../../interface/advisors-and-help.md#pending)) |

## What triggers help

In v1.42 the engine itself tells help only through the banter, the Help key and the advisors' remarks: the message
sets are run by nothing else. The help about the thing held (its only runner has no caller) and the field-of-view help
(it picks the thing in view, which is then only saved and loaded) never speak; the tutorials that praise the controls,
point out the scrolls and remind the player are the land and challenge scripts' (../scripts/, ../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Looking at the sky, or closely at the land, is counted as a help event the scripts can ask about | partial 30% | `help_profile::ProcessSpecialTriggers` has the exe's tests and thresholds (look at the land, too close, at the sky), but the player camera does not give it the view (`Queries::playerCamera` unset), so the events never come; no test |
| The first did-you-know sign the player taps is explained by the advisors | done | the first tap with no tip read yet starts "FirstDYKExplained" (`script_highlight::InterfaceTap`, `src/ECS/ScriptHighlight.cpp`; test `ScriptHighlight.*`), as the exe ([script highlights](../../../interface/script-highlights.md)) |
| Town alerts: the store running low on food or wood, few people, unhappy villagers, worshippers dying | done | `audio::guidance::WarnLowOnFood`, `WarnLowOnWood`, `WarnVillagersUnhappy` (from `src/ECS/TownDesire.cpp`), `WarnLowOnPeople`, `WarnLosingVillagers`, `WarnWorshippersDying` (from `src/ECS/Villager/VillagerDeath.cpp`), `WarnLosingBelief` (`src/ECS/Town/TownBelief.cpp`); tests `GuidanceTest.DesireSampleAndScore`, `GuidanceTest.TownDesireEveryTenTurns`; read in the exe ([voices](../../../audio/voices.md)); silent on the campaign's Land 1, as the exe |
| Battle alerts: the player's towns attacked, the player's creature attacking a town, the creature fighting | done | `RemarkAttackingTown`, `RemarkCreatureAttacking` and `PlayTownAttackRemark` are called from `town_emergency::UpdateAggressor` (`src/ECS/Town/TownEmergency.cpp`), and `PlayTownAttackRemark` plays `WarnTownUnderAttack` for a town of the player's; `RemarkCreatureFight` is called from `CreatureFightSystem.cpp`; tests `GuidanceTest.AttackingATownAndWreckingABuilding`, `GuidanceTownAttack.*`, `GuidanceTest.TheCreatureFightingRemarksWaitUntilTheyAreAllowedAgain`. The "creature under attack" warning has no caller, as in v1.42, where it has none either ([voices](../../../audio/voices.md)) |
| Remarks on the player's deeds: killing people, destroying buildings, being very good or very evil, acting against their alignment | done | `RemarkKillingPeople` (villager deaths), `RemarkGeneralGood` / `RemarkGeneralBad` (town belief), `UpdateAlignmentRemarks` (`src/ECS/Effects/Alignment.cpp`), `RemarkBuildingDestroyed` (`abodes::OnPhysicalDamage`) and `RemarkAttackingTown` (`town_emergency::UpdateAggressor`); every remark restarts the remarks' common wait (`SpiritSay`); tests `GuidanceTest.AlignmentRemarks`, `GuidanceTest.SpiritSayRestartsTheRemarksGate`, `GuidanceTest.AttackingATownAndWreckingABuilding`; read in the exe ([voices](../../../audio/voices.md)) |
| Remarks on the moon's phase | done | `audio::guidance::RemarkOnMoonPhase` every turn (`guidance::ProcessGameTurn`), the wait the cube of the phase from full; tests `GuidanceMoon.*`; read in the exe ([voices](../../../audio/voices.md)) |
| Banter: idle chatter between the advisors from 25 sets | done | `message_sets::ProcessBanter`: after 120 s with no interface event (help record), a random banter set 1..25 not sent yet (`GetRandomBanterSet`), from `help::Process`; test `HelpMessageSets.BanterGoesThroughEverySetOnceThenStartsAgain`; read in the exe |

## Asking for help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Help key asks about what is in the hand, else what the hand is over, else the ground under it | partial 90% | `help::help_key::Ask` (`src/Help/HelpKey.cpp`) from `input::ShortcutKeys::Update` on the Help action (F1), gated by a script's wide screen as the exe; test `ShortcutKeys.TheHelpKeyAsksTheAdvisors`; the debug bar's consciences window can ask it too. Not read: whether the key acts inside the temple (openblack gates every key there) ([advisors and help](../../../interface/advisors-and-help.md#the-help-key-and-the-advisors-remarks)) |
| Each kind of thing has its own answer: villagers, animals and objects, buildings, the town centre, magic | partial 75% | `help_query::About` (`src/Help/HelpQuery.cpp`, tests `HelpQuery.*`) on `ecs::help_query_facts` (`src/ECS/HelpQueryFacts.cpp`), the exe's rules (scripted, disciple, dead, child, vagrant, another player's villager, another player's wonder, a town centre or totem); a town's desire flags, the temple entrance, fireflies and the leash are not things in openblack yet |
| The ground answers by its kind: solid ground, deep or shallow water, and so on | done | `help_query::AboutGround`: a fish farm's water, shallow water, else the terrain material (`ecs::terrain_material::AtCell`); tests `HelpQuery.*`; read in the exe ([advisors and help](../../../interface/advisors-and-help.md#the-help-key-and-the-advisors-remarks)) |
| Help that teaches a control shows the key or mouse button to press, as the player has bound it | done | help texts with $M codes are parsed (`HelpTextDisplay`) and tooltips and the did-you-know bubble draw the bound key or button (`help::input_prompt`, `Renderer::DrawKeyOrMouse`); and a help text's $M code shows the bound key or mouse button at the right of the box (`DialogueText::ShowControlIcons`, `help::control_icon`; ours, from the exe: [land1-intro](../../../scripts/land1-intro.md), "The control icon itself") |

## The creature's help

The good advisor's messages about the creature are measured row by row in
[../creature/lessons_and_help.md](../creature/lessons_and_help.md); these rows follow it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tells the player what the creature wants now and why it failed at something | inferred | `CreatureHelpSystem` (`src/ECS/Systems/Implementations/CreatureHelpSystem.cpp`, tests `CreatureHelpSystemTest.*`, `CreatureHelp.*`) tells what it wants once the hand has hovered over it or taken hold of it, a fizzled miracle and the leash stopping it; raffclar's port, unverified against v1.42; most failures are not raised yet |
| Tells the player when the creature has learnt, or nearly learnt, an action or a spell, or can't learn one yet | inferred | raised from the mind's watching (`CreatureMindSystem::SeeSkill`, `WatchMiracle`) and told by `CreatureHelpSystem`; unverified against v1.42 |
| Tells the player a desire or an opinion went up or down after a reward or a punishment | partial 50% | the source and opinion lessons and "taught nothing" are told after a stroke or slap, checked against v1.42 (`creature_help::FeedbackLesson`, tests `CreatureHelp.FeedbackLessonsTellOfTheOpinionOrTheSource`, `CreatureHelp.FeedbackTooYoungToLearnFrom`); a desire wanted more or less is only kept as a thought |
| Walks the player through the stages of the creature's life (meeting the guide, the leashes, eating, fighting, helping and impressing towns, punishment, growing up) | pending | the stage kinds and their lesson scripts are in `creature_help::StageScript` (test `CreatureHelp.StageMessagesStartTheirLessons`), but nothing raises them: what raises them in the original is not found |
| Turned off by the creature help setting | inferred | the menu's Creature help box and the profile set `CreatureHelpSystem::SetHelpOn` (`Game::ApplyPlayerSettings`); off, only the stage messages pass (test `CreatureHelp.TheOptionTurnsAllButTheStagesOff`); raffclar's rule, unverified against v1.42 |

## Tips

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Did you know" texts, shown in the temple's library | done | the library's five help scrolls list the tips read, by category, in reading order (`src/3D/TempleScrolls.cpp`, `did_you_know_read::In`; test `ScriptHighlight.DidYouKnowReadLists`); the read lists are the exe's ([script highlights](../../../interface/script-highlights.md)); the library's drawing is measured in ../temple/library_room.md |
| "Did you know" scrolls lit in the world, read by tapping them | done | `ecs::script_highlight` did-you-know signs (CREATE_HIGHLIGHT row 1, `InterfaceTap`) open the help bubble (`src/Help/Bubble.cpp`, drawn by the renderer among the world's blended objects, under the dialogue texts, as one Z object keyed at its sign) and are marked read; tests `TipBubbleSystem.OpensWithEachTappedSignsTextAndTheSameSignClosesIt`, `DidYouKnowBubble.*`, `ScriptHighlight.DidYouKnowReadLists`, `ScreenPoint.AWorldPointAtADepthIsDrawnBackOnItsPoint` |
| Tips of the day: one of 34 tips, its picture and its text, on the loading screen at the start and before each game | done | `Game::StartTipOfTheDay`, `gui::LoadingScreen` (`src/GameStartup.cpp`): picked by the clock's sixteenths of a millisecond, two tips skipped, tip 0 without a profile, the text's codes blanked, fading in the first time; tests `LoadingScreen.*` (`test/gui/test_loading_screen_rules.cpp`); read in the exe ([video playback](../../../video/playback.md)) |
