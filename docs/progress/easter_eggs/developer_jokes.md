# Developer jokes

Jokes, puns, nods to Lionhead and its people, and developers' messages to each other, in the game's text and inside the
program. The game's text is in `Scripts/InfoScript2.txt` (UTF-16); the in-game credits are owned by
[../temple/library_room.md](../temple/library_room.md); the named villagers taken from the staff list by
[../villager/special_villagers.md](../villager/special_villagers.md).

**Progress: 1/7 done, 1 partial — 21%**

## In the game's text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Asking the advisors about a lion creature can get "A Lion. With a nice head. Lionhead. Get it?"; about a horse, "Your Horse. I'm a little hoarse myself. Aha. Ha."; about the ogre, "The Ogre. He has his eye on you. That was a joke, Leader." | pending | the Help key's query is not handled in our tree (F1 toggles the renderer's debug view); see [../interface/help_system.md](../interface/help_system.md) |
| Asking about something the advisors find dull can get "Maybe this is something we'll put in the sequel." | pending | same |
| Villagers' grumbles include fourth-wall lines: "I wonder if I'll get a part in the sequel?", "I had to audition for this. Can you believe it?", "I could have been someone. I could have been in Dungeon Keeper.", "I know Peter Molyneux personally.", "Lord, won't you buy me a Mercedes Benz?", "It feels like someone's watching me." | pending | villagers say no banter lines in our tree; see [../villager/looks_and_voices.md](../villager/looks_and_voices.md) |
| The advisors' own banter includes "Are we gonna figure in the sequel?", "Maybe we can tip over the monitor!" and "I got a CD of speed-thrash hymns in my car!" | done | the advisors' banter sets are said when the player is idle (`help::message_sets::ProcessBanter`, `GetRandomBanterSet`, `src/Help/HelpMessageSets.cpp`); see [../story/advisors.md](../story/advisors.md) |
| The man who wants to be thrown says "I can see your mouse from here." | pending | a Land 3 challenge, which play has not yet reached: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../story/minigames.md](../story/minigames.md) |
| Puns in the challenges: "Gutted! Ha! Fishermen. Gutted. Get it?" (the drowning fishermen), "Wow. The saintly one made a joke." (the lost flock), "Hey! A Palm Pony. Oh dear. I'm not very good at jokes." | partial 50% | the lost flock's lines run through working natives but are not checked in game ([../story/silver_scrolls/the_lost_flock.md](../story/silver_scrolls/the_lost_flock.md)); "Gutted! Ha! Fishermen..." is used by no script ([../story/silver_scrolls/the_saviour.md](../story/silver_scrolls/the_saviour.md)); the palm pony line is not checked; re-measured 2026-10-21: the lost flock's lines run through working natives; none is seen in a run, and the fishermen's pun is used by no script |
| The phone box's "One of the Lionhead people." comes from a set of hidden-script lines written for a Lionhead person's birthday ("Hey Boss! Today is a special day.", "Yes. It's someone very important's birthday.", "So many happy returns to them!") and Christmas ("Happy Christmas!", "Bah, humbug!"); no challenge script says the birthday or "Happy Christmas!" lines, and "Bah, humbug!" is reused by the evil advisor at the Land 2 sacrifice when a small sacrifice is made | pending | the phone box is in [hidden_content.md](hidden_content.md); whether the program itself says the birthday lines on special days belongs to [../pc_integration/](../pc_integration/) |

