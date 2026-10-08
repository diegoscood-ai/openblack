# Throwing Stones

A silver scroll of the first land that teaches throwing: a boulder sits on top of a pillar down on the shore, and the
player throws rocks from an everlasting pile on the hill above until one knocks the boulder off. A fisherman watches
every throw from outside his hut. The prize is a toy ball for the creature, and afterwards the boulder keeps coming
back for practice, turning into water miracle seeds. The land as a whole is in [../land_1.md](../land_1.md); the
mini-game summary is in [../minigames.md](../minigames.md).

**Land:** 1 · **Giver:** the advisors (scroll over the hill above the pillar) · **Script:** ThrowingStones ·
**Reward:** a toy ball in a chest from the sky; then up to six water miracle seeds for practice throws ·
**Repeatable:** no (the practice afterwards is endless until six seeds)

Sources: the quest's challenge script (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's setup and control scripts, the throwing hand demo, the shared reward and did-you-know
scripts, the game's text table and the executable (how a scroll record and its alignment are applied). openblack's state
is judged on this tree: the land's control script starts this quest with the start-up box's second, third and fourth
answers (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and of the 64 script functions the quest
and the shared scripts it runs (notice, reward, did-you-know, hand demo) call, 10 only log "not implemented" in
`src/CHLApi.cpp` (the held-object read included). The 54 that work include the camera reads, widescreen and its
transition check, distance and position reads, random numbers, the game time, the existence check, setting a position
and making rocks and mobile statics. Every row below is todo unless the notes say otherwise.

**Progress: 1/47 done, 27 partial — 31%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's setup marks the boulder's spot with an area villagers avoid (20 across the target) | partial | `SetupLand1` runs in our tree and `INFLUENCE_POSITION` works (`src/Magic/Script/CHLInfluence.cpp`); not checked in game |
| The land's control script starts the quest at the very beginning of the land, in the background, right after the family's "follow us" walk and the temple guide (or at once when skipping to the creature choice), and lets it go at once | partial | started with the second, third and fourth tutorial answers (before the creature glade); with the first answer the intro never finishes, so it never starts |
| The boulder on the pillar is always drawn, however far away it is | done | our tree fades no object with distance, so the boulder is always drawn; the special is accepted (`ecs::super_villager::ThingJcSpecial`) |
| A bronze did-you-know scroll is placed nearby at once: "Silver Reward Scrolls are not vital so you don't have to do them, if you don't want to. But there are often rewards available if you complete them." | partial | the did-you-know script, `CREATE_HIGHLIGHT`, `HIGHLIGHT_PROPERTIES` and the text work (`src/ECS/ScriptHighlight.cpp`, `src/Help`); not checked in game |
| A silver challenge scroll is put up on the hill above the pillar, 15 up in the air | partial | `CREATE_HIGHLIGHT` and the height (`SET_PROPERTY` of the Y position) work; not checked in game |
| While it is unclicked, whenever the camera is within 150 of it and it is on screen (at most every 30 seconds, and not during a film), the good advisor points at it: "Look. A Silver Reward Scroll. Let's see what it means." and the evil advisor adds "Yeah, Silver ones aren't as vital as the Gold ones, but we can still check them out." | partial | the quest's notice script runs on working natives (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, `SPIRIT_EJECT`, `SPIRIT_POINT_POS`, `RUN_TEXT`); not checked in game |
| Clicking the scroll starts the quest; the scroll is switched to active | partial | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight work; not checked in game |
| If the player knocks the boulder off before ever clicking the scroll, the scroll's notice stops, there is no introduction, no fisherman and no reward: the evil advisor just points at the pillar and says "You rule at rock throwing, Boss." and the practice round starts | partial | the check and the advisor's line work; the practice round that follows never gets going (see below) |

## The fisherman

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On clicking, one Japanese fisherman is made at his hut by the shore and walks to a spot on the hill by the rock pile, faces the camera and plays an idle animation | partial | `CREATE` makes the fisherman (`VillagerArchetype`), `MOVE_GAME_THING`, `SET_FOCUS`, `SET_SCRIPT_ULONG` on a villager work; not checked in game |
| Every second he checks what the hand holds; only a rock counts (trees were meant to, but were left out) and only while he is on screen with the camera within 100 | todo | `GET_OBJECT_HELD` and `IS_OF_TYPE` are stubs: he never sees a rock in the hand |
| He watches the rock in the hand; if it comes within 10 of him he ducks like a goalkeeper and waits a moment | todo | `GET_OBJECT_HELD` and `IS_OF_TYPE` are stubs: he never sees a rock in the hand |
| Once thrown, he follows its flight with his eyes; the script tracks how close the rock came to the foot of the pillar | todo | `GET_OBJECT_HELD` and `IS_OF_TYPE` are stubs: he never sees a rock in the hand |
| If it came within 12 of the pillar he cheers ("Ooh. Now that was close."), otherwise he despairs ("Hey, I saw that. You missed the pillar by miles!"); the lines are spoken from him only if no other dialogue is running | todo | `GET_OBJECT_HELD` and `IS_OF_TYPE` are stubs: he never sees a rock in the hand |
| If his hut drops below three quarters of its health (before the quest is won), the evil advisor points at it: "Great shot! The house didn't stand a chance." and the good advisor: "Please watch where you're throwing things. You could have someone's eye out."; he plays an angry animation and stomps off to a friend's house, then is left to his own life | todo | `GET_PROPERTY` of health is not ported (answers 0), so the hut and the fisherman read wrong from the start |
| Once the quest is won, if his hut is undamaged he walks back home and is left to his own life | todo | health answers 0 (`GET_PROPERTY`), so these tests are wrong |
| If he dies, his watching stops | todo | health answers 0 (`GET_PROPERTY`), so these tests are wrong |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen): the camera glides over 5 seconds to a view of the rock pile; both advisors come out | partial | `SET_WIDESCREEN`, `MOVE_CAMERA_POSITION`, `MOVE_CAMERA_FOCUS`, `SPIRIT_EJECT` work; not checked in game |
| Evil: "OK. Let's get our projectile skills up to scratch." Good: "What do you suggest? I presume it involves killing…" Evil: "Not at all. I was going to propose, er, we, um… " | partial | `RUN_TEXT` and `TEXT_READ` work (`src/Help`); not checked in game |
| The camera swings over 4 seconds to look at the boulder on its pillar; the good advisor looks at it and points: "I know, let's see if we can hit that pillar over there." | partial | `LOOK_GAME_THING`, `SPIRIT_POINT_POS`, the camera moves work; not checked in game |
| Good: "It's a test of our godly powers." — "And nobody gets hurt." | partial | `RUN_TEXT` works; not checked in game |
| The camera goes back up to the hill; five rocks of half to nine tenths of full size appear in a pile there; the evil advisor points at them: "How nice. And I guess we use this everlasting pile of stones." Good: "Hmm. Handy, that." | partial | `CREATE_WITH_ANGLE_AND_SCALE` makes the rocks, and the film around them works; not checked in game |
| The advisors go home, the dialogue box closes and the quest is recorded under the title "Throwing Stones" at 0% with the good advisor's reminder "See if you can hit that pillar with these handily-placed stones." | partial | the advisors and the dialogue work; the record is `SNAPSHOT`, a stub |
| The throwing hand demo plays: the camera settles on the hill and a recorded hand shows how to throw, pausing at each step for the good advisor: "Move your Hand over a rock." — "Press the Action Button until the rock is in your hand." — "Hold down the Action Button." — "Put momentum on the throw by moving the Hand in the correct direction." — "Throw the rock by releasing the Action Button." then "Try throwing a rock now." | partial | `PLAY_HAND_DEMO`, `HAND_DEMO_TRIGGER`, `IS_PLAYING_HAND_DEMO` work (the intro's drag demo is checked in game); this throw demo is not checked |
| After the film a second did-you-know is placed by the hill, explaining aftertouch: "You can throw in different ways. A long smooth throw sends the missile low along the ground, and a quick hand move and release will throw the object higher. Moving the Hand after you release gives you aftertouch…" | partial | the did-you-know script works; not checked in game |

## The everlasting pile

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rock from the pile that is destroyed comes back at its place in the pile, at nine tenths size whatever size it was | partial | `THING_VALID` and `CREATE_WITH_ANGLE_AND_SCALE` work; not checked in game |
| A rock that has come to rest (not flying, not in the hand or the creature's hand) more than 40 from its place is put back there at half size | partial | `SET_POSITION` and the flying test work; the in-hand property answers 0 and `IN_CREATURE_HAND` is a stub, so a held rock far away is also pulled back; setting a rock's scale is not ported |
| The pile stops refilling once the quest is won, so it is "everlasting" only while the quest runs | partial | the script's own logic; not checked in game |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Throw rocks at the pillar on the shore until the boulder on top is knocked more than 2 away from its spot; the script checks every 2 seconds | partial | picking up and throwing rocks work; the script's distance test on the boulder uses working natives; not checked in game |
| Hitting the pillar while the boulder stays on top (checked 2 seconds after the hit): a film looking at the pillar; the evil advisor: "You're doing well but you have to knock the rock clean off." The record moves to 50% with the reminder "Can you knock the rock off the top? Eh?" This happens once only | todo | `GAME_THING_HIT` is a stub |
| There is no time limit, no limit on throws and no way to fail; damaging houses only upsets the fisherman | partial | nothing in our tree adds a limit; the fisherman part is not working (see above) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the camera flies over 2 seconds to look at the shore, following the falling boulder; the good advisor: "You hit it. Marvellous." and the record moves to 50% | partial | `SET_FOCUS_FOLLOW` and the camera work; the record is `UPDATE_SNAPSHOT`, a stub |
| The camera rises over 5 seconds; the quest is recorded as fully successful (no alignment change) and the evil advisor: "Neat, Boss. You're really gonna do damage with this skill later!" | partial | the camera and the line work; the record is `SNAPSHOT`, a stub |
| The camera then looks at where the reward will land, the area villagers avoided is removed and an attraction 10 across is put at the reward spot | partial | `INFLUENCE_POSITION` works; not checked in game |
| The scroll is removed | partial | `OBJECT_DELETE` of a highlight works |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A chest holding a toy ball for the creature falls from the sky by the pillar, without a film | todo | `CREATE_REWARD` is a stub: no chest |
| If it is the game's first reward (likely, as this quest is open from the start of the land) the good advisor: "Wow. We've been given a chest. I wonder if there's anything in it?" and the evil advisor: "Yeah, click the Action Button on it to open it."; once opened, the reward's help line and "Now click on the contents to activate them." | todo | `CREATE_REWARD` is a stub: no chest |
| Otherwise, once the chest is clicked, the reward's help line is spoken in one line if the dialogue is free | todo | `CREATE_REWARD` is a stub: no chest |

## Practice afterwards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new boulder is put back on the pillar (through a line of the land's map script), with a small area villagers avoid around it | todo | `MAP_SCRIPT_FUNCTION` is a stub: no new boulder, so the practice script waits for good |
| Each time it is knocked off and comes to rest, it fades away and a water miracle seed appears where it stopped | todo | `MAP_SCRIPT_FUNCTION` is a stub: no new boulder, so the practice script waits for good |
| A boulder destroyed rather than knocked off gives no seed; a new one is put up either way | todo | `MAP_SCRIPT_FUNCTION` is a stub: no new boulder, so the practice script waits for good |
| After six seeds the boulder is no longer put back | todo | `MAP_SCRIPT_FUNCTION` is a stub: no new boulder, so the practice script waits for good |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest plays no music of its own; the fisherman's cheer and jeer are spoken from him | partial | no music, as the original; the cheer and jeer (`GAME_PLAY_SAY_SOUND_EFFECT` works) never come, since he never sees a rock |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature isn't needed; rocks in its hand count as held (they aren't put back) but the fisherman only watches rocks in the player's hand | todo | `IN_CREATURE_HAND` is a stub and the creature holding objects is not wired |
| The toy ball is a plaything for the creature | todo | no ball: `CREATE_REWARD` is a stub and `CREATE` does not make balls |

## Script quirks and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Knocking the boulder off before clicking the scroll forfeits the toy ball and the record (the quest is never recorded) | partial | the script's own logic; in our tree there is no record or chest either way |
| After the win, if the fisherman's hut is damaged but still above three quarters, he never goes home and keeps watching throws | todo | health answers 0 (`GET_PROPERTY`), so the hut test is wrong |
| Each practice round puts up another small avoid-area at the pillar and never removes the old ones | todo | the practice rounds never run (`MAP_SCRIPT_FUNCTION` is a stub) |
| A counter of rocks put back at the pile is kept but never read | n/a | |
| An earlier version had three boulders to knock off (each with its own advisor line and the reminder "See if you can hit that pillar…" until all three fell); it was commented out and its lines are gone from the text table | n/a | |
| Only one fisherman is made, though the script is written to make several watchers spread around the spot | n/a | |
