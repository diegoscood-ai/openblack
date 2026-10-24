# Lessons and creature help

The game explains the creature to the player as it goes. When the creature learns something, moves to a new stage of
growing up, is put under a spell, gets into trouble or fails at something, the good advisor comes to the edge of the
screen and says so in a line or two of text. These messages wait their turn on a stack, aren't repeated too often, and
can be switched off in the options. The general help system and advisors belong to [../interface/](../interface/) and
[../story/](../story/); this file covers what the creature has to say.

**Progress: 24/63 done, 0 chosen, 19 partial, 7 inferred — 59%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md),
[the creature help in openblack](../../../creature/mind.md#the-creature-help-in-openblack) and
[the help request](../../../creature/casting.md#the-help-request-on-a-fizzle). The kinds' table (level, how each is
said, its first text, in view, stacked, one waiting) is built at start-up by the original (0x4C8920); read in the
oracle on 2026-10-21, it matches `creature_help::Info` row for row.

## How creature help is shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creature help is run by the challenge script's creature help scripts, which open a dialogue with the good advisor | done | `CreatureHelpSystem` starts `HelpForCreaturePointAtCreature` or `HelpForCreatureJustText` (`Scripts/Quests/challenge.chl`) with the kind's first text plus the parameter pushed, as the original's tells do (0x4CA740..0x4CB2F0); test `CreatureHelpSystemTest.OnlyThePlayersCreatureIsToldOf` |
| Help as a single line of text | done | the kinds the original tells with `HelpForCreatureJustText` (can't learn yet, tutorial, source, body, town, misc, failure, what it wants); test `CreatureHelp.TextsAreTheGamesOwnNumbers`, `CreatureHelpSystemTest.FaintingTellsWhy` |
| Help where the advisor flies out and looks at the creature as it speaks | done | the kinds the original tells with `HelpForCreaturePointAtCreature` (the lessons, learnt and nearly learnt, the spells, messed up); `CreatureHelpSystemTest.HeldInTheHandItWaitsAndIsToldOnceLetGo` |
| Fully grown away from the first land, the help's new-stage script tells two lines and switches the creature's lesson script off | partial 50% | `creature_help::StageScript` / `StagePushed` start `HelpForCreatureMoveToNewDevelopmentPhase` with 4095 pushed twice and no lesson flag, as 0x4CAE80; test `CreatureHelp.FullyGrownOffTheFirstLandStartsTheNewStageScript`; nothing raises the stage messages yet (the original raises stage + 12 when a mind is loaded into a creature, `LoadMindIntoPreexistingCreature` 0x4E848C, from `SwapMinds` and the computer creature's load; our SWAP_CREATURE raises nothing) |
| Messages wait on a stack and are shown one at a time; some kinds stack, others replace the one waiting | done | `creature_help::Stack` and `FirstWaiting`, one raised again each turn; checked against the kinds' table and the help request (0x4CA340 stacks, 0x4CA670 raises the first waiting); tests `CreatureHelp.TheStackTellsInKindOrderAndForgetsWhatItRaised`, `HeldBackMessagesWaitIfTheirKindStacks` |
| A message isn't shown again until enough time has passed since it was last shown | done | 60 s, 5 s for what it wants (`creature_help::ToldLately`), as 0x4CA290 with 0x8C36A8 / 0x8AB6E4; tests `CreatureHelp.AMessageIsToldAgainAfterAMinute`, `WhatItWantsIsToldAgainAfterFiveSeconds` |
| Only the player's own creature gives help, and only once grown past stage 13 but for the stage messages | done | the local player's creature (`creature_help::Decide`), as the help request's gates 0x4CA036 / 0x4CA05A; tests `CreatureHelp.OnlyAGrownUpCreatureOfThePlayersGivesHelp`, `CreatureHelpSystemTest.OnlyThePlayersCreatureIsToldOf` |
| The creature help option in the game's options turns it all on or off | done | `creatureHelp` sets `CreatureHelpSystem::SetHelpOn`; off, only the stage messages pass; the original's switch is the help system's +0x4608, read at start-up (on when unset, 0x5C6D91) and shown by the options box; tests `CreatureHelp.TheOptionTurnsAllButTheStagesOff`, `GameMenu.TheCreatureHelpBoxShowsTheSwitchAndThePlayerChangesItFromThere` |
| Scripts can turn creature help on or off | partial 90% | `SET_CREATURE_HELP` sets the same switch (`src/CHLApi.cpp`), as the original's native writes +0x4608 (0x6F441D); `DEV_FUNCTION` 10 and 11 do nothing in the original ([players-creature.md](../../../creature/players-creature.md)); no test of the native itself, and the shipped challenge script never calls it |
| A message is told only when the creature's help level reaches its kind's level | pending | the kinds' levels are in `creature_help::Info` and match the original's table, but `Decide` does not read them; the original compares them with the creature's own level (Creature +0x21C); what sets that level is not read |
| In a multiplayer game the creature help is always off | pending | the original clears the switch each turn of a multiplayer game (`GGame::StartTurn` 0x54E5AF) and at the start of one (0x54CFF1); no multiplayer in our tree |

## Lessons from feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A source drives a desire more readily (for example, it will eat when only a little hungry) | done | told by the good advisor after a stroke's lesson that did not split what it knows of such things, by the source that says why it did it (LessonIncreaseSource, `creature_help::FeedbackLesson`; the source is `creature_learning::Context::source`) (raffclar PR #354; checked against v1.42) |
| A source drives a desire less readily | done | as above, after a slap (LessonDecreaseSource) (raffclar PR #354; checked against v1.42) |
| It thinks better of an action | done | told by the good advisor when a stroke's lesson split what it knows of such things (what it was for or what it was done to reaches a leaf below the root of its tree; LessonIncreaseOpinion, by the desire); test `CreatureHelp.AnOpinionChangesWhenEitherThingTaughtSitsBelowItsTreesRoot` (raffclar PR #354; checked against v1.42) |
| It thinks worse of an action | done | as above, after a slap (LessonDecreaseOpinion); test `CreatureHelp.FeedbackLessonsTellOfTheOpinionOrTheSource` (raffclar PR #354; checked against v1.42) |
| A stroke or slap taught it nothing (the "stroked, but it isn't learning" and "slapped, but it isn't learning" remarks) | done | too young to learn from it, or nothing to learn: Miscellaneous 4 (stroked) or 5 (slapped), text 1109 + the parameter (`creature_help::TooYoungToLearn`), as the original's 0x4E0B08 / 0x4E0B56; with nothing it did lately worth crediting the feedback does nothing and nothing is said; test `CreatureHelp.FeedbackTooYoungToLearnFrom`; see [learning_from_feedback.md](learning_from_feedback.md) (raffclar PR #354; checked against v1.42) |

## Lessons from watching

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has learnt an ordinary skill | partial 50% | kept as a thought and told by the good advisor (LessonNormalAction, `CreatureMindSystem::SeeSkill`), as the original's `AddActionLearnt` 0x4E2994; skills are only shown to it from the debug spawner (the original's six callers of `ConsiderLearningAction` are not ported) |
| It has learnt a miracle | partial 80% | told by the good advisor with the miracle's own text (`CreatureMindSystem::WatchMiracle`, also for a miracle seen cast in play), as 0x4E24F8 / 0x4E2975; the sightings count the mind's own turn, not the game's, and not every sighting the original reports is ported ([mind.md](../../../creature/mind.md#pending)); testbed `advisors.creature_help` |
| It has nearly learnt a skill | partial 50% | told at each sighting that doesn't teach it (NearlyLearntNormalAction, `SeeSkill`), as 0x4E271C; skills only from the debug spawner |
| It has nearly learnt a miracle | partial 80% | told on `creature_watching`'s NearlyLearnt event, with the miracle's own text, as 0x4E25D5; the same gaps as the learnt miracle |
| It can't learn a skill yet | partial 50% | raised (CantLearnNormalActionYet), as 0x4E2748; skills only from the debug spawner; that it is never heard (as raffclar's port has it) is not checked, since the kinds' help level is not ported |
| It can't learn a miracle yet | partial 50% | raised on the TooYoung event (CantLearnMagicActionYet), as 0x4E2601; never heard is not checked, as above |

## Stages of growing up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each stage has its own message as the creature reaches it, which starts that stage's lesson script: learning to eat from the hand, punishment, leash pulling and picking up, leashing to a house, meeting the guide, friends with the guide, the guide's history, the guide's spells, impressing a town, learning to fight, helping a town, the good and evil leashes, and grown up (the storm) | partial 50% | `creature_help::StageScript` and `StageStartsLesson`: the script of every stage and the in-lesson flag match the original's tells 0x4CAC40..0x4CAE80 (read 2026-10-21); test `CreatureHelp.StageMessagesStartTheirLessons`; nothing raises them: the original raises them only when a mind is loaded into a creature (stage + 12, 0x4E848C) and the first stage's on a network packet (0x63C608); see [development_phases.md](development_phases.md) |

## Spells, needs and troubles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell has been cast on it | done | told as the spell takes hold (ReceivedSpell, text 1046 + the spell's place in the game's table, from the creature spells' turn in `src/Magic/Spells/SpellCreature.cpp`), as the original's 0x4F49DD; test `CreatureHelp.ASpellTellsOfTakingHoldAndOfWearingOff`; spells themselves in [../miracles/](../miracles/) (raffclar PR #354) |
| A spell on it has worn off | done | told as the spell finishes (FinishedReceivingSpell, text 1062 + the spell), as 0x4F4A85; same test (raffclar PR #354) |
| What source drives its current desire | pending | not raised; the original raises CurrentSource (text 401 + the dominant source of its plan's desire) from a routine at 0x4F14E0 that no call reaches directly; how it is reached is not read |
| Its current desire | partial 85% | told once the hand has hovered over it long enough (0.25), when a source says why, as `UpdateHelp` 0x4C9E27..0x4C9E92; forced, once the hand takes hold of it and its body is free (`CreatureMindSystem::HandTookHold`, raffclar PR #354); on a leash order to a point further than the order's distance (`LeashSystem`); not on a leash order to a thing (the original's 0x4C61C5 / 0x4C6413); tests `CreatureHelp.TheHandOverItBuildsUpSlowly`, `TheHandsHoldShowsTheCreditedDesireElseThePlans`; testbed `advisors.creature_help_hold` |
| A bodily problem: low or very low energy, hurt or badly hurt, tired or very tired, thirsty or very thirsty | done | at most every 120 s by the species' thresholds, the first that holds in that order (`creature_help::BodyProblemNow`), as `UpdateHelp` 0x4C9C90..0x4C9E08; test `CreatureHelp.BodyProblemsEveryTwoMinutesInTheirOrder` |
| How a town feels about it | done | told when a town's attitude to it changes (TownAttitudeToCreature, `town_creature_attitude::Store` in `src/ECS/Town/TownCreatureAttitude.cpp`), as `Town::UpdateAttitudeToCreature` 0x743948, only with the creature in view; checked against [mimicry.md](../../../creature/mimicry.md); test `TownCreatureAttitude.FearIsStampedEachTurnItIsSeenRespectOnlyWhenItStarts`; town attitudes in [town_actions.md](town_actions.md) |

## Things it messed up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A throw that missed | inferred | not raised; its text is in the game (MessedUpAction 0), but the only raise of the kind with a varying parameter (0x50A196, mind +0x21F0) sits in code that no call reaches directly; whether and when it is told is not read |
| A miracle that fizzled | done | told when its try fizzles (MessedUpAction 1, `creature_help::k_MiracleFizzled`, in the mind's miracle try at a thing or a place), as 0x4F7B83 / 0x4F7FBA, checked against [casting.md](../../../creature/casting.md#the-help-request-on-a-fizzle); test `CreatureMindCastingTest.AFizzleAtAThingOrAPlaceTellsThePlayerTheCreatureMessedTheMiracleUp` (raffclar PR #354) |
| Fishing that went wrong | inferred | not raised; as the missed throw (MessedUpAction 2) |
| Dancing with villagers that went wrong | inferred | not raised; as the missed throw (MessedUpAction 3) |
| Making a fire that went wrong | inferred | not raised; as the missed throw (MessedUpAction 4) |
| Impressing villagers that went wrong | inferred | not raised; as the missed throw (MessedUpAction 5) |
| Raising a totem that went wrong | inferred | not raised; as the missed throw (MessedUpAction 6) |

## Why it failed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Where it wants to go can't be reached | partial 80% | a leash order to a point it can't get to raises FailureExplanation 3 (`LeashSystem`, `creature_help::k_Inaccessible`), as `ForceActOnMapCoord` 0x4C6A27; only the text is tested (`CreatureHelp.TextsAreTheGamesOwnNumbers`), not the raise; routes in [locomotion.md](locomotion.md) |
| The other creature refused to play | pending | not raised; the original raises FailureExplanation 5 at 0x4F63F0 |
| The other creature refused to be friends | pending | not raised; the original raises FailureExplanation 6 at 0x4F64BE and 0x4F657E |
| It lost sight of what it was following | partial 80% | a thing of its plan gone, it gives up and raises FailureExplanation 8 (`CreatureMindSystem::LoseGoneThings`, `creature_help::k_LostThing`), as the original's `RemoveBeliefsAboutDeletedObjects` 0x4D85ED; test `CreatureLostThing.TheAdvisorSaysItLostTheThing`, testbed `mind.lost_thing`; when it is lost is raffclar's PR #508, not checked against the original's trigger |

## Other remarks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nice music is playing | pending | not raised; the original raises Miscellaneous 0 from 0x47EBB9 (fn_0047E760, under the creature's reaction to music fn_0047E060, not read) |
| Nasty music is playing | pending | not raised; the original raises Miscellaneous 1 from 0x47EEC2 (fn_0047EC10, same caller) |
| It has fainted | partial 80% | it says why first: badly hurt, very low energy or very tired (`creature_help::FaintingProblem`), as the original's 0x47CF00 (life, then energy at or below 0, then exhaustion at or above 1); test `CreatureHelp.FaintingSaysWhy`; the "fainted" remark itself (Miscellaneous 3, raised by `Faint` 0x47704D) is not raised: that it is never heard is raffclar's reading, not checked; fainting itself in [physiology.md](physiology.md) |
| It has been carried home | pending | not raised; the original raises Miscellaneous 6 as its death's actions are made (`ConstructSubActionsForDeath` 0x49B9F4) |
| It can't pick that up | partial 80% | the hand offering it something it can't hold raises Miscellaneous 7 with its own text 4322 (`HandGiving`), as `InterfaceValidToGiveObject` 0x47A393; only the text is tested, not the raise |

## The first lessons (tutorial prompts)

The prompts' conditions, their 120 s, the creature in view and the choice of the one told longest ago are checked
against `Creature::UpdateHelp` 0x4C9EB2..0x4C9FB5 and its condition table (0xC82854 + 16 per prompt, filled at
start-up, read in the oracle on 2026-10-21). The original marks a prompt known in only four places; the other prompts
are told again every time their conditions hold, as ours are. Tests: `CreatureHelp.PromptConditions`,
`PromptsTakeTurnsOldestFirst`, `CreatureHelpSystemTest.PromptsComeOneAtATimeEveryTwoMinutes`, `PromptsNeedTheCreatureInView`.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| How to select the creature | partial 70% | told whenever due (`creature_help::TutorialApplies`, `TutorialNow`); the original marks it known in a routine at 0x4F4B90 (Creature +0x198, how it is reached not read); ours never marks it (`CreatureHelpSystem::PromptKnown` has no caller) |
| How to make it run | partial 70% | told from stage 3; the original marks it known when the leash sends it to a point (`ForceActOnMapCoord` 0x4C6FDB); our point order does not mark it |
| Holding the hand over it | done | told whenever due; never marked known, in the original too |
| Its home needs more rocks | done | told when what it most wants is to build its home; never marked known |
| How to make it pick something up | done | told whenever due; never marked known |
| How to make it help build | done | told when what it most wants is to build its home; never marked known |
| How to make it drop something | partial 70% | told while it carries something; the original marks it known when the leash sends it to a point (0x4C6C01); ours does not |
| How to make it eat | done | told while it carries something; never marked known |
| How to reward and punish it | done | told whenever due; never marked known |
| How to make it drink | partial 70% | told when what it most wants is water; the original marks it known on the leash's thing order (0x4C6144, raffclar's vanilla bug 94) and point order (0x4C6DDC); ours does not |
| How to make it poo | done | told when what it most wants is to poo; never marked known |

## Signs the creature gives

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its thoughts (the last few things it learnt or decided) are kept | inferred | `creature_mind_model::Think`, newest eight (`k_MaxThoughts`); shown only in `src/Debug/CreatureSpawnerMind.cpp`; the original writes "I've learnt to ..." lines into the mind after a feedback (mind +0x16D04, 256 bytes a line, `RespondToPlayerFeedback` 0x4E19BE..0x4E1ADF); how many it keeps, its other lines, and whether the released game shows them are not read |
| A light bulb appears over its head when it tries a miracle it hasn't learnt | pending | no light bulb effect; the fizzle itself is shown (`CreatureMindSystem::ShowFizzle`); the original has a "Lightbulb" creature sub-action (its table filled at 0x4FDB83), when it runs is not read |
| It makes a sound that shows its mood | pending | not modelled; its voice in general is in [animation.md](animation.md) |
| Music changes its mood, and its mood changes the music | pending | not modelled; see [../audio/](../audio/) |
| It talks: speech items it says to villagers, friends and the player | pending | not modelled |
| A web page of the creature's statistics can be written out | pending | not modelled; the original writes it every 600 turns with the creature's own save, from `Scripts/creature_html.tmpl` (`fn_0047B440`, [players-creature.md](../../../creature/players-creature.md)); the same page is in [../easter_eggs/launch_switches_and_files.md](../easter_eggs/launch_switches_and_files.md) and [../pc_integration/online_services.md](../pc_integration/online_services.md) |
