# The Singing Stones

A silver scroll on the first land: a ring of eight stone holders east of the player's village has only three of its
singing stones left. A hippy who lives in a hut beside it asks for the missing five; the player finds them around the
land and sets each in its own hole so that the ring plays a rising scale. Done, the stones sing for good, a short storm
rolls over the circle and the player is given a food miracle dispenser.

The second land has its own singing stones ([the_singing_stones_land_2.md](./the_singing_stones_land_2.md)); a cut
stone-circle quest, never compiled, is [The Miracle Stones](./the_miracle_stones.md).

**Land:** 1 · **Giver:** the hippy, from his hut beside the stone circle · **Script:** SingingStoneCircle · **Reward:** a food miracle dispenser in the middle of the circle · **Repeatable:** no

Sources: the challenge script source (`SingingStoneCircle.txt`, which matches the shipped `challenge.chl`), the shared
reward, reminder and did-you-know scripts it runs, the land control script, the game's text table and the script sound
list (`Audio/SFX/Script/ScriptSfxEnum.h`). openblack is judged on this tree: of the 76 script functions the quest and
the scripts it runs need, 7 still only log "not implemented" in `src/CHLApi.cpp`, and the land's control script starts
this one right after the land's set-up with every start-up answer, so the circle is built (see
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md)); the rest is not checked in game. Rows are
partial only where the openblack function the row needs already works. The land as a whole is in
[../land_1.md](../land_1.md); the stones as objects are in
[../../nature/one_shot_features.md](../../nature/one_shot_features.md).

**Progress: 4/66 done, 48 partial — 42%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle is set up as the land begins: the land control script starts this quest right after the land's set-up, before the family leads the player home, and it then waits for its start signal | done | `LandControl1` starts `SingingStoneCircle` right after `SetupLand1` with every tutorial answer; the circle is assembled in game ([map-loading.md](../../../bw1-notes/map-loading.md#creation-from-chl-create-27--create_with_angle_and_scale-252)) |
| The silver scroll is offered only after the creature's first lessons (seeing its pen, learning to eat and being punished; just its pen when the creature training is skipped), at the same moment as the Immersion Mushrooms | partial | only with the fourth tutorial answer: the start flag is raised after the pen scene; with the other answers the land control script never gets there (the intro or the creature glade does not finish) |
| The scroll (a challenge highlight) stands by the hippy's hut, about 48 from the circle's centre, not at the circle itself | partial | `CREATE_HIGHLIGHT` works (`src/ECS/ScriptHighlight.cpp`); reached with the fourth answer; not checked in game |
| While the scroll is waiting and the camera is within 100 of it with the scroll in view, the good advisor steps out, points at it and says "Look. Something for you to do here.", at most once every 30 seconds (the first time at once) | partial | every native of the notice script works (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, `SPIRIT_EJECT`, `SPIRIT_POINT_POS`, `RUN_TEXT`, `src/Help`); not checked in game |
| The notice ends when the scroll is clicked, or when the circle has already been completed; the scroll is then made active | partial | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight work; not checked in game |
| The hippy does not exist until the scroll is clicked: he is then made at his hut (a hippy villager) | partial | `CreateScriptObject` makes villagers (`VillagerArchetype`); not checked in game |
| If the player completes the circle before the scroll appears or before it is clicked, there is no introduction and no scroll entry: the hippy is simply made at his hut and the ending and reward follow at once | partial | the script's own logic, on working natives; not checked in game |

## The circle and its stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle's centre is a fixed spot; eight holders (singing stone bases) stand around it on a ring 17.5 across from the centre, one every eighth of a turn | done | `CreateScriptObject` makes the bases (`MobileStaticArchetype`); the circle is assembled in game ([map-loading.md](../../../bw1-notes/map-loading.md#creation-from-chl-create-27--create_with_angle_and_scale-252)) |
| Stones 1, 2 and 6 already stand in their holders (the first two and the sixth going round the ring) and can't be moved or picked up | partial | `CREATE`, `SET_ID_MOVEABLE`, `SET_ID_PICKUPABLE` work (`ecs::object_flags`); the hand's refusal is not checked in game |
| The five missing stones (3, 4, 5, 7 and 8) lie scattered over the land, between about 140 and 1,175 from the circle (stone 7 close by, stones 3 and 8 at the far west) | done | `CREATE` makes them at set-up, with every tutorial answer |
| Three wrong stones that look the same lie closer, about 280, 360 and 650 from the circle | done | `CREATE` makes them at set-up, with every tutorial answer |
| An influence ring of radius 50 is put round the circle, and each of the five missing stones carries its own small influence, so the player can pick them up wherever they lie; the circle's ring stays for the rest of the land | partial | `INFLUENCE_POSITION` and `INFLUENCE_OBJECT` work (`src/Magic/Script/CHLInfluence.cpp`); not checked in game |
| Until the circle is complete, any of the five missing stones that is destroyed is made again where it started, with a new influence; the wrong stones and the three fixed ones are never remade | partial | `KeepStonesAlive` runs from set-up on working natives (`THING_VALID`, `CREATE`, `INFLUENCE_OBJECT`); not checked in game |
| Until then the five missing stones are also kept turning to face the circle's centre, one stone per script turn | todo | `SET_FOCUS` on an object that is not a villager or an animal only logs "not implemented" |

## The notes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each right stone has its own note, the eight going up a scale round the ring; all three wrong stones play the same sour note | partial | `PLAY_SOUND_EFFECT` works and the script sound bank plays in game (the intro's stone notes, [audio.md](../../../bw1-notes/audio.md)); this quest's notes not checked |
| Tapping (clicking) any of the eleven stones, wherever it is, plays its note there with a short sparkle | partial | `GAME_THING_CLICKED`, `CLEAR_CLICKED_OBJECT`, `SPECIAL_EFFECT_OBJECT`, `PLAY_SOUND_EFFECT` work; not checked in game |
| The tap check looks at the stones in three groups (1 to 4, 5 to 8, the wrong ones), one group per script turn, and stops once the circle is complete | partial | `GAME_THING_CLICKED`, `CLEAR_CLICKED_OBJECT`, `SPECIAL_EFFECT_OBJECT`, `PLAY_SOUND_EFFECT` work; not checked in game |
| From the moment the land begins the circle plays itself round, one holder every half second with a two-second pause before the first: the stone in each holder sounds its note (a slightly longer sparkle), empty holders are silent | partial | `CheckStonePositionsAndPlayNotes` runs from set-up with every tutorial answer, on working natives; not checked in game |
| On its turn, a holder takes any singing stone (right or wrong) within 2.5 of it, snaps it into the holder and turns it to face the centre; a holder found empty is marked empty | partial | `CALL_NEAR` and `SET_POSITION` work; turning the stone to the centre does not (`SET_FOCUS` on an object is not ported) |
| Because each holder is looked at once per round, a stone put down takes up to about six seconds to snap in | partial | follows from the holder loop above; not checked in game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cut scene with the fourth generic script theme; the hippy is shown in high detail | partial | `START_MUSIC` (`src/Audio`) and `SET_HIGH_GRAPHICS_DETAIL` work; reached with the fourth answer; not checked in game |
| The camera flies over 4 seconds to a spot north-east of the circle, following the hippy as he walks out to its edge; after 2 seconds it closes in on him over 2 more | partial | `MOVE_CAMERA_POSITION`, `SET_FOCUS_FOLLOW`, `HAS_CAMERA_ARRIVED`, `MOVE_GAME_THING` on a villager work; not checked in game |
| The hippy faces the camera with a gossiping gesture: "Hi, man. Neat stone circle, huh? It's a bummer that some are missing." | partial | `SET_FOCUS`, `SET_SCRIPT_ULONG` (the gesture) and `SET_SCRIPT_STATE` on a villager, `RUN_TEXT` work; not checked in game |
| The camera cuts to a view across the circle; the good advisor steps out: "Sounds like we can help, there."; the evil advisor: "What? Help this deluded sack of burnt-out neurons?" | partial | camera cuts, `SPIRIT_EJECT`, `SPIRIT_HOME`, `RUN_TEXT` work; not checked in game |
| The hippy faces the camera again: "The Stones have to play a scale. That's what we need." | partial | camera cuts, `SPIRIT_EJECT`, `SPIRIT_HOME`, `RUN_TEXT` work; not checked in game |
| Both advisors go home; the hippy: "Tap a stone to hear its note." (with the mouse-button picture the text table marks) | partial | camera cuts, `SPIRIT_EJECT`, `SPIRIT_HOME`, `RUN_TEXT` work; not checked in game |
| The hippy walks back to his hut while the camera rises over 3 seconds to look west across the circle; the dialogue box closes | partial | `MOVE_GAME_THING`, camera moves and `GAME_CLOSE_DIALOGUE` work; not checked in game |
| The scroll's entry is made in the challenge list at no success and no alignment, titled "The Singing Stones", with the reminder "Let's find the stones to put in the stone Circle." (good advisor) | todo | `SNAPSHOT` is a stub: no challenge-list entry |
| The hippy goes back to normal detail and the music stops | partial | `SET_HIGH_GRAPHICS_DETAIL` and `STOP_MUSIC` work; not checked in game |

## Wrong order or wrong stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| These checks start only after the introduction | partial | the script's order, on working natives; not checked in game |
| The first time all five empty holders are filled with right stones in the wrong order, the scroll goes to half success (0.5) with the reminder "We've found all the stones but the order is wrong." | todo | `UPDATE_SNAPSHOT` is a stub |
| With the hippy alive, a cut scene by his hut: he walks up slowly; good advisor: "These are the right stones, but they're in the wrong order."; the advisor goes home and the hippy, unimpressed: "The Stones have to play a scale. That's what we need."; he walks back and the camera returns to where it was | todo | never plays: `GET_PROPERTY` of health is not ported and answers 0, so the script takes the hippy for dead right after the introduction |
| With the hippy dead, the good advisor alone: "Hmm. That doesn't sound right to me." | partial | this is the branch our tree takes, but for the wrong reason (health answers 0, the hippy is alive); not checked in game |
| The first time all five holders are filled and any of them holds a wrong stone, the scroll goes to 0.25 with the reminder "We've found enough stones but some really don't sound right." | todo | `UPDATE_SNAPSHOT` is a stub |
| With the hippy alive, a cut scene by his hut: he walks up, after 2 seconds plays an unimpressed gesture: "This sounds pretty inharmonious to me." / "Are you sure you've found the right stones?"; he walks back and the camera returns | todo | never plays: health answers 0, so the hippy counts as dead |
| With the hippy dead, the good advisor: "Hmm. This doesn't seem right. Some of the stones are wrong. Listen." | partial | the branch our tree takes, because health answers 0; not checked in game |
| Each of the two comments is made once only; the progress mark can therefore go from 0.25 to 0.5 or from 0.5 to 0.25 depending on which happens first | todo | the comments are made once, but the progress marks are `UPDATE_SNAPSHOT`, a stub |
| Quirk: once the holders have been filled with right stones only, the "filled" check stops for good, so wrong stones put in afterwards are never commented on | partial | the script's own logic (`CheckFilled`), on working natives; not checked in game |

## Rules for success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle is complete when each of the five missing stones is in its own holder (stone 3 in the third holder, and so on), which makes the ring's notes rise in order all the way round | partial | the holder loop and `CheckFormed` run on working natives; not checked in game |
| There is no time limit, no failure and no way to abandon it; it stays open until done | partial | nothing in our tree adds a limit; not checked in game |
| The five placed stones are then fixed: they can't be moved or picked up | partial | `SET_ID_MOVEABLE` and `SET_ID_PICKUPABLE` work |
| The tap check and the stone-remaking stop; the ring plays two more full rounds before the quest counts as finished | partial | the script's own logic, on working natives; not checked in game |
| Nothing in the scripts looks at who placed the stones: the hand, the creature carrying or throwing them, or anything else that moves them | partial | the hand can move the stones; the creature carrying or throwing objects is not wired in our tree |

## Ending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From then until the end of the land the singing-stones music is attached to the circle's centre, and every script turn a random one of the eight stones sparkles | partial | `ATTACH_MUSIC` (`src/Audio`, the singing stones music) and `SPECIAL_EFFECT_OBJECT` work; not checked in game |
| The eight holders are let go by the script (they stay in the world) | partial | `RELEASE_FROM_SCRIPT` works |
| With the hippy alive: a cut scene in high detail looks at him as he walks (at half speed) to just outside the circle with the camera following; after 4 seconds he says "Wow. Now this is a good vibe. Amazing." while the camera moves behind him over 4 seconds; on arriving he cheers (the crowd's "won" animation) | todo | never plays: health answers 0, so the script takes the dead-hippy branch |
| The camera then moves over 3 seconds to the start of a recorded camera path and runs it | partial | `CONVERT_CAMERA_POSITION`, `CONVERT_CAMERA_FOCUS`, `RUN_CAMERA_PATH` work (`src/Camera`); not checked in game |
| A monsoon is made over the circle: it lasts 50 seconds with a 5-second fade, rain at full, no snow, full overcast, clouds 10 at height 70, sheet lightning 2 to 7 (1 to 5 if the hippy is dead) and practically no forked lightning, a 20 inner and 50 outer radius, not blown by the wind | partial | the weather thing and `CHANGE_WEATHER_PROPERTIES` and the other weather commands work (`src/Magic/Script/CHLWeather.cpp`); `SET_AFFECTED_BY_WIND` is a stub |
| The scroll's entry goes to full success (alignment 0) | todo | `UPDATE_SNAPSHOT` is a stub |
| The good advisor's lines: "Something is happening." / "The stones are singing together again." | partial | `SPIRIT_EJECT`, `RUN_TEXT` work; not checked in game |
| With the hippy alive the camera then returns over 4 seconds to where it was and he is let go to live normally | todo | never plays: health answers 0, so the script takes the dead-hippy branch |
| With the hippy dead the same path, storm and lines play without him, and the camera is not taken back afterwards (the scene simply ends) | partial | the branch our tree takes (health answers 0); not checked in game |
| The scroll is deleted at the very end | partial | `OBJECT_DELETE` of a highlight works |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A food miracle dispenser (the Norse dispenser building) is built in the middle of the circle at no angle, set to food and switched on | partial | `CREATE_WITH_ANGLE_AND_SCALE` makes dispensers, `SET_MAGIC_PROPERTIES` and `SET_ACTIVE` work (`src/Magic/Script/CHLWorship.cpp`); not checked in game |
| Quirk: the reward script means to set the refill time only when one is given, but tests the game's clock instead, so it always sets the refill time to the 0 seconds asked for | partial | `DLL_GETTIME` and `SET_TIMER_TIME` work, so the same zero refill is set; its effect is not checked |
| A cut scene plays the reward sting and flies the camera over 4 seconds to look at the dispenser from 21.5 to the side and 14 up | partial | `PLAY_SOUND_EFFECT` and the camera moves work; not checked in game |
| If it is the first dispenser the player has been given: a did-you-know scroll is placed 5 in front of it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), a signpost is put up beside it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), and the evil advisor points at it: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then at the signpost: "Click on the signpost for more info." | partial | `CREATE_HIGHLIGHT`, `HIGHLIGHT_PROPERTIES`, the did-you-know script and the advisors work; not checked in game |
| Otherwise the evil advisor points at it: "Nice. Another of those cool Miracle Dispensers." | partial | as above; which land 1 dispenser comes first is not checked |
| After the scene the dispenser's own help lines for its miracle are spoken in turn, the advisors who own them stepping out (or clinging to the screen edge for a two-line group) | todo | `GET_FIRST_HELP` and `GET_LAST_HELP` are stubs |

## The hippy and his hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hippy lives in a hut that is part of the land, about 56 east of the circle | partial | the hut is made by the land's map script; finding it with `CALL` works; not checked in game |
| If the hippy dies (his health reaches nothing) the good advisor says "You killed him. How dreadful of you." once, and from then on his comments are replaced by the advisors' | todo | `GET_PROPERTY` of health is not ported (answers 0): the line comes right after the introduction, with the hippy alive |
| The first time his hut drops below 80% health with him alive: a cut scene by the hut, he walks up and faces the camera, unimpressed: "You wrecked my house. Bad karma, guru."; he then wanders slowly round that spot and the camera cuts back | todo | never plays: health answers 0 for the hut and the hippy, so the script takes the hippy for dead |
| With him dead (or gone), the evil advisor instead: "Ha. The hippy would be turning in his grave." / "If his body was in a grave. Not being pecked by vultures." | todo | the lines come at once after the introduction (both healths answer 0), not when the hut is damaged |
| The hut is watched only from the introduction on, and the watch never ends until the hut is damaged, even after the quest is over | partial | the watch script runs from the introduction on; its test is wrong while health answers 0 |
| Killing the hippy or wrecking his hut changes no alignment and doesn't stop the quest | partial | no success mark changes alignment; not checked in game |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part in the script; it can still carry, throw or knock stones into place, and can kill the hippy or wreck his hut, with the same results as the player | partial | the hand can do it; the creature carrying, throwing or knocking stones is not wired in our tree |

## Unused and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text table has no lines 1 to 5 or 11 to 16 for this quest: cut before release | n/a | nothing to play |
| A starting spot is set for stone 6, which is never used (stone 6 is one of the fixed ones) | n/a | |
| A first, commented-out scroll entry in the middle of the introduction (moved to its end) | n/a | |
| The wrong-stone notes "bad stone 2" and "bad stone 3" exist in the sound list, but all three wrong stones play "bad stone 1" | n/a | |
| An older version of the land control script (`LandControl1-steve.txt`, not in the shipped program) started a stone script of another name instead, whose source is not among the shipped scripts | n/a | not shipped; see the shared-and-unused quests |
