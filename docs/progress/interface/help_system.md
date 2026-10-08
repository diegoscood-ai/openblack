# Help system

The help the game gives as it is played: the advisors' messages when something happens for the first time or goes
wrong, the answers to the Help key, the creature's learning messages, alerts, banter and tips. Who the advisors are and
how they look and fly is in ../story/; the words on screen are in [on_screen_text.md](on_screen_text.md).

**Progress: 9/37 done, 10 partial — 38%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Running help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help messages come in sets, and a set picks one of its messages at random | done | the set table, and mode 1 picks one text at random on the local stream (`src/Help/HelpMessageSets.cpp`, `message_sets::RunMessageSet`); run by the banter and the debug Consciences window (`src/Debug/Consciences.cpp`) |
| A message is spoken by the good or evil advisor, or both in turn, with their voice and lip sync | done | `help::HelpSystem::SayText` routes a text to the good or evil advisor by its narrator (`RouteOf`, `SpiritWhoTalks`); the advisors fly and speak with lip sync (`src/Help/Spirits.cpp`, `src/Help/SpiritsRuntime.cpp`, `src/Audio/Services/Advisor.cpp`); tests `HelpSystem.VoiceHooks`, `Spirits.*` |
| Only one message runs at a time; a new, more important one stops the running one, the advisor coughing or grumbling at the interruption | done | one text and one help script at a time: `script_control::StopHelpScriptsForNewHelp` stops the running help scripts, and a click cuts both advisors (`advisor::Interrupt`); tests `ScriptControl.*`. Our wiki differs: the original picks an interruption line but never says it, so the advisor is not heard grumbling ([audio](../../bw1-notes/audio.md#how-it-works)) |
| A message waits until it has been read (by its length and the player's read speed) or clicked through before the next | done | `HelpSystem::IsTextRead`: the click, the voice or the reading time from the word count and READ_SPEED; tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpSystem.ReadWithVoice`, `HelpSystem.WithInteraction` |
| Messages already given are remembered so they are not repeated | partial | each set's sent turn and count are kept (`message_sets::MarkMessageSetSent`), so the banter does not repeat a set until all 25 are used (`GetRandomBanterSet`); the history of texts is kept (`HelpSystem::GetHistory`); the per-thing "already explained" memory is not ported |
| The help level setting sets how much help is offered, none turning it off | partial | the help system's guidance level gates the remarks (`audio::guidance`, `HelpSystem::GetGuidanceLevel`) and level 0 turns HELP_SYSTEM_ON off, but the menu's Help level (`MenuSettings::helpLevel`) is not passed to it |
| Land scripts turn the help system on and off, and ask whether it is on | done | `HELP_SYSTEM_ON` and `SET_HELP_SYSTEM` in `src/CHLApi.cpp` read and set `HelpSystem::IsHelpSystemOn` / `SetHelpOn` |
| Help is held back while the player is inside the temple, and what was on screen comes back on leaving | todo | nothing puts the help text away on entering the temple |
| Help can take the camera into widescreen bars while it speaks | done | the help scripts' SET_WIDESCREEN goes through `help::script_control` and the `wideScreen` hook to the bars (`ScreenFade::SetWideScreen` in `src/3D/ScreenFade.cpp`, `src/Game.cpp`); test `ScriptControl.SetWideScreen` |
| What help has been given and how often is kept with the player's profile | partial | the help record of 49 events is counted (`src/Help/HelpProfile.cpp`, `help_profile::Trigger`), but there are no profiles: it starts at 0 every run |

## What triggers help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first time the player holds each kind of thing (each animal, each miracle seed, food, wood, rocks, a tree, a villager, a scaffold, poo, a ball, fire) | todo | the help engine's "first time held" sets (the mode 2 sets in `src/Help/HelpMessageSets.cpp`) are in the table but nothing runs them |
| The first time each miracle is cast, and when it is powered up | todo | the spell seeds count a help event on casting (`SpellSeed.cpp`, `help_profile::Event::CastSpell`) but no message is run |
| The first time each miracle seed is seen | todo |  |
| Wonders: on first seeing one, and for each tribe's wonder | todo |  |
| Learning the controls: picking up by pulling, gestures on the ground, turning by the screen's sides, and the like, each praised once learnt | todo | the camera help's features and the help record's camera events are kept (`src/Camera/CameraHelp.cpp`, `help_profile::CameraHelpCallback`), but no praise or hint is run |
| Looking at the sky, looking closely at the land, or bumping the camera against the land | todo | `help_profile::ProcessSpecialTriggers` has the look-at-sky and look-at-land events, but the player camera does not give it the view (`Queries::playerCamera` unset), so they never come |
| Villagers who don't believe, who aren't interested, vagrants, children, and each kind of disciple | todo |  |
| A challenge scroll lit up nearby | partial | a did-you-know sign's first tap runs "FirstDYKExplained" (`src/ECS/ScriptHighlight.cpp`); nothing points out a challenge scroll |
| The village centre and the temple, the first times they are seen | todo |  |
| Town alerts: the store running low on food or wood, few people, unhappy villagers, worshippers dying | done | `audio::guidance::WarnLowOnFood`, `WarnLowOnWood`, `WarnVillagersUnhappy` (from `src/ECS/Town/TownDesire.cpp`), `WarnLowOnPeople`, `WarnLosingVillagers`, `WarnWorshippersDying` (from `src/ECS/Villager/VillagerDeath.cpp`), `WarnLosingBelief` (`src/ECS/Town/TownBelief.cpp`); tests `GuidanceTest.*` |
| Battle alerts: the player's people or towns attacked, the creature attacked or fighting | partial | `WarnTownUnderAttack`, `WarnCreatureUnderAttack`, `RemarkCreatureFight` and `PlayTownAttackRemark` exist in `src/Audio/Services/Guidance.cpp` but nothing calls them yet |
| Remarks on the player's deeds: killing people, destroying buildings, being very good or very evil, acting against their alignment | partial | `RemarkKillingPeople` (villager deaths), `RemarkGeneralGood` / `RemarkGeneralBad` (town belief) and `UpdateAlignmentRemarks` (`src/ECS/Effects/Alignment.cpp`) are wired; `RemarkBuildingDestroyed` and `RemarkAttackingTown` have no caller |
| Remarks on the moon's phase and on the player not watching | partial | `audio::guidance::RemarkOnMoonPhase` runs every turn (`guidance::ProcessGameTurn`); idle chatter after two minutes is the banter below; no other "not watching" remark found |
| Banter: idle chatter between the advisors from 25 sets | done | `message_sets::ProcessBanter`: after 120 s with no interface event (help record), a random banter set 1..25 not sent yet (`GetRandomBanterSet`), from `help::Process` |
| Reminders of what a challenge still needs | todo |  |

## Asking for help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Help key asks about what is in the hand, else what the hand is over, else the ground under it | todo | F1 toggles the renderer's debug view (`Game.cpp`); the Help action is not handled |
| Each kind of thing has its own answer: villagers, animals and objects, buildings, the town centre, magic | todo |  |
| The ground answers by its kind: solid ground, deep or shallow water, and so on | todo |  |
| Help that teaches a control shows the key or mouse button to press, as the player has bound it | partial | help texts with $M codes are parsed (`HelpTextDisplay`) and tooltips and the did-you-know bubble draw the bound key or button (`help::input_prompt`, `Renderer::DrawKeyOrMouse`); the $M icon in a help text is not drawn |

## The creature's help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tells the player what the creature wants now and why it failed at something | todo | the creature's mind exists (`src/Creature`) but no help message is run from it |
| Tells the player when the creature has learnt, or nearly learnt, an action or a spell, or can't learn one yet | todo |  |
| Tells the player a desire or an opinion went up or down after a reward or a punishment | todo |  |
| Walks the player through the stages of the creature's life (meeting the guide, the leashes, eating, fighting, helping and impressing towns, punishment, growing up) | todo |  |
| Turned off by the creature help setting | partial | the menu's Creature help box is kept in `MenuSettings::creatureHelp` but nothing reads it, and there is no creature help to turn off |

## Tips

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Did you know" texts, shown in the temple's library | partial | the library's scroll lists help texts (`src/3D/TempleScrolls.cpp`, `Content::LibraryDidYouKnow`); see ../temple/ |
| "Did you know" scrolls lit in the world, read by tapping them | done | `ecs::script_highlight` did-you-know signs (CREATE_HIGHLIGHT row 1, `InterfaceTap`) open the help bubble (`src/Help/Bubble.cpp`, drawn by the renderer) and are marked read; tests `HelpSystem.BubbleOpensWithEachTappedSignsTextAndTheSameSignClosesIt`, `DidYouKnowBubble.*`, `ScriptHighlight.DidYouKnowReadLists` |
| Tips of the day | todo | (unconfirmed where the game shows them) |
