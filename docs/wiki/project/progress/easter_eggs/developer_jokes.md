# Developer jokes

Jokes, puns, nods to Lionhead and its people, and developers' messages to each other, in the game's text and inside the
program. The game's text is in `Scripts/InfoScript2.txt` (UTF-16); the in-game credits are owned by
[../temple/library_room.md](../temple/library_room.md); the named villagers taken from the staff list by
[../villager/special_villagers.md](../villager/special_villagers.md).

**Progress: 3/4 done, 0 chosen, 1 partial, 0 inferred — 88%**

## In the game's text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Asking the advisors about a lion creature can get "A Lion. With a nice head. Lionhead. Get it?"; about a horse, "Your Horse. I'm a little hoarse myself. Aha. Ha."; about the ogre, "The Ogre. He has his eye on you. That was a joke, Leader." | done | the Help key (F1) asks the advisors about what is in or under the hand (`help::help_key::Ask`, `src/Help/HelpKey.cpp`), a creature by its species' text (`help_query::CreatureTexts`; test `HelpQuery.CreatureTexts`); see [../interface/help_system.md](../interface/help_system.md); read 2026-11-05: `Creature::GetQueryFirstEnumText` 0x47D8F0 gives the species row's first text, the next for a creature not the local player's, except the chimp's and the ogre's rows (12, 13), and `GetQueryLastEnumText` 0x47D940 the same text; `Scripts/InfoScript2.txt` gives HELP_TEXT_QUESTION_LION_CREATURE_02 (another god's lion), HELP_TEXT_QUESTION_HORSE_CREATURE_01 (the player's horse) and, for the ogre whatever its owner, HELP_TEXT_QUESTION_VILLAGER_INFO_OGRE_01, as `help_query::CreatureTexts` picks them |
| Asking about something the advisors find dull can get "Maybe this is something we'll put in the sequel." | done | read 2026-11-07: the line is `HELP_TEXT_QUESTION_NOT_INTERESTED_13`, text 2998 of the frozen game's text table (`Scripts/InfoScript2.txt`, the 2999th `ADD_TEXT`), one of the 14 texts 2986..2999 of message set 33, which says one of them at random (mode 1, `first + LocalRand(last - first + 1)`); the Help key answers set 33 for anything with no help texts of its own and for ground with no point or no material (`help_query::About`, `AboutGround`, `src/Help/HelpQuery.cpp`; `help::message_sets::RunMessageSet`); tests `HelpMessageSets.TheSetsAreTheGames`, `HelpMessageSets.ASetSaysItsTextsOrOneOfThemAtRandom`, `HelpQuery.*`; see [../interface/help_system.md](../interface/help_system.md) |
| The advisors' own banter includes "Are we gonna figure in the sequel?", "Maybe we can tip over the monitor!" and "I got a CD of speed-thrash hymns in my car!" | done | the advisors' banter sets are said when the player is idle (`help::message_sets::ProcessBanter`, `GetRandomBanterSet`, `src/Help/HelpMessageSets.cpp`); tests `HelpMessageSets.BanterGoesThroughEverySetOnceThenStartsAgain`, `HelpMessageSets.ABanterSetStopsTheRunningHelpMessageFirst`; see [../story/advisors.md](../story/advisors.md) |
| Puns in the challenges: "Wow. The saintly one made a joke." (the lost flock), "Hey! A Palm Pony. Oh dear. I'm not very good at jokes." | partial 50% | the lost flock's line is done, with a quest_check PASS of a natural run ([../story/silver_scrolls/the_lost_flock.md](../story/silver_scrolls/the_lost_flock.md)); the palm pony line is not checked (which script says it is not determined); read 2026-11-07: it is `HELP_TEXT_NEW_HAND_43` (text 3181 of the frozen table), one of the hand's "what you are holding" lines (`HELP_TEXT_NEW_HAND_01` to `_44`), not a challenge's; no shipped script says it (`refs/chl`), so what says it is still undetermined |

