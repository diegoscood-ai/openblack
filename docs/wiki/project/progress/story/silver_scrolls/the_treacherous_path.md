# The Treacherous Path

A silver scroll on the fourth land. A woman (the "blind woman" of the scripts) must carry a healing potion along a
dangerous mountain path to her dying brother in another village. Wolves, a forest fire and a boulder-throwing ogre named
Morg lie on the way; she has three potions to bring herself back, but each one she uses is one fewer for her brother.
If she arrives with a potion left, the brother is healed and gives the player a wolf creature to swap for.

**Land:** 4 · **Giver:** the woman at her hut in the Aztec village · **Script:** BlindWoman · **Reward:** a wolf creature to swap for, offered for as long as the land lasts (only if the brother is healed) · **Repeatable:** no

The land as a whole is in [../land_4.md](../land_4.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md); the woman is also listed in
[../../villager/special_villagers.md](../../villager/special_villagers.md).

Sources: the land's challenge script source (checked against the PC game's compiled `challenge.chl`), the game's text
table (`Scripts/InfoScript2.txt`) and the executable. openblack is judged on this tree: of the 81 script functions the
quest and the scripts it starts call, none is a stub in `src/CHLApi.cpp` at this tree (`WALK_PATH` and
`OVERRIDE_STATE_ANIMATION` still log "not implemented" for a creature, and every creature action the quest forces, rows
15 Hurl, 21 Fight, 161 BeIdle and 189 LookForever, has an executor in `src/Creature/CreaturePlanActions.cpp`), a script
can make a creature
(`CreateScriptObject`), and Land 4's control script never runs (the story reaches it only through
the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked), so neither the woman nor Morg ever exists and
every row below is pending unless the notes say otherwise.

**Progress: 0/60 done, 0 chosen, 8 partial, 0 inferred — 5%**

## Morg, the boulder-throwing ogre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| As soon as the land's control script starts the quest (in the background, at the land's start), an ogre creature is made on a ridge above the path, modelled on the player's creature at 0.9 of its strength and size, then shrunk to half scale, named "Morg" and made fully evil | pending | `SetCreatureName` and `CreatureCreateRelativeToCreature` are real (`src/CHLApi.cpp:3482`); `LandControl4` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); see [../../scripts/land4_script.md](../../scripts/land4_script.md) |
| Morg works from the land's start, long before the scroll appears: every 10 seconds he throws a small boulder (a rock at 0.3 scale made beside him) at his target | partial 40% | making a rock works (`CreateWithAngleAndScale`), and the throw is `CreatureDoAction` row 15 Hurl, which has an executor; Morg is not made in play yet (Land 4 is not reached); re-measured 2026-10-21: part of what it calls is a stub (see the notes) |
| His target: the player's creature if it is within 75 of him; otherwise the woman, once her quest has begun and she was last seen within 50 of a grassy patch on the path; otherwise a sheep on that patch | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A boulder that does not land within 15 of him is deleted instead of thrown; thrown boulders stay 20 seconds once they come to rest, then shrink away over about 20 seconds and go | pending | `ObjectDelete` works; no throws |
| If he wanders more than 30 from his ridge he walks back (up to 25 seconds) | pending | `MoveGameThing` walks a creature under the script's control (`src/CHLApi.cpp:2678`); not reached in play |
| Whenever the camera is within 75 of him he mutters, at most once a minute: "Sheeps keep me awake with baaings at night!" | pending | `GetTimerTimeRemaining`, `SetTimerTime` and `GamePlaySaySoundEffect` work; Morg is never made |
| If the player's creature comes within 50 of him, he grows (in steps of 0.1) to the creature's size, both creatures are made to stare at each other, and Morg starts a fight with the creature | pending | `CreatureDoAction`, `IsFighting` and `CreatureAutoscale` are real |
| The script waits up to 45 seconds for the fight to start, then until it ends | pending | `IsFighting` is real |
| If Morg ends the fight with less fighting health than the creature, he is driven out: five sparkles burst where he stood, he vanishes, and an Aztec farmer is made in his place, who is given no health and so dies at once; Morg never returns | pending | the fight result reads are real (`GetProperty` fight health, `src/CHLApi.cpp:1749`); not reached in play |
| If Morg wins (or the fight never starts), he walks back to his ridge, heals 1% every 6 seconds between fights (full health in 10 minutes), and 20 seconds later shrinks back down to half scale | pending | the shared fight-healing script; its creature natives (fight health, `CreatureAutoscale`, `MoveGameThing`) are real; not reached in play |
| The player's creature is released back to the player after either result | pending | `ReleaseFromScript` works; never reached |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll appears only once the player owns the Aztec village of this land | pending | reading a town's owner (`GetProperty` player) is real (`src/CHLApi.cpp:1818`); not reached in play |
| A silver scroll appears above the woman's hut in the Aztec village, raised 10 off the ground | pending | `CreateHighlight` and the height property work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out every 30 seconds or more, points at it and says "Look. Something for you to do here." | pending | the shared notify script's natives work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The woman does not exist until the scroll is clicked | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll makes the woman (an Aztec housewife) at her hut; she cannot be picked up | partial 40% | the pick-up flag works (`SetIdPickupable`) and `Create` makes villagers; the scene never runs; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| A pack of eight wolves is made on the path (gathering within 5, roaming up to 15), and a flock of eight sheep on the grassy patch below Morg | pending | `FlockCreate` and `ChangeInnerOuterProperties` work, `PopulateContainer` is real |
| A cinematic (letterbox) with generic script music: the woman, in high detail, walks to a spot by her hut while the camera glides to look at her over three seconds; she faces the camera | partial 40% | the letterbox, the music, the camera glides and high detail work (`SetWidescreen`, `StartMusic`); the cinematic never runs; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| The scroll is entered in the challenge log as "The Treacherous Path", not yet done, with the good advisor's reminder "Help the sister reach her brother." | pending | `Snapshot` is real |
| She plays a gossiping animation. Woman: "Holy One, I must take this healing potion to my brother in the Village." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves to the start of a recorded camera path and flies along the route she must take | pending | `ConvertCameraPosition` and `RunCameraPath` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Woman: "But I've got to travel along this treacherous path." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Woman: "Please watch over me. I'd be so grateful." | pending | the source notes the spoken line was recorded without "me"; the text has it |
| The screen fades to black and back in on the woman; the music stops and the cinematic ends | partial 40% | fades, camera cuts and stopping the music work; the scene is never reached; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |

## Her journey

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| She cannot be hurt while she walks round her fence and to the start of the path; then she walks a recorded path all the way to her brother's village and can be hurt from then on | partial 50% | the indestructible flag works (`SetIndestructable`) and `WalkPath` works for villagers (`src/ECS/LivingWalkPath.cpp`); the journey never starts; re-measured 2026-10-21: the flag and the walk work; the journey never starts |
| Every 2 seconds the quest checks on her | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Near the first point of the path: good advisor, pointing at her, "Argh! The woman's heading straight for that pack of wolves!" and "We must save her, Leader." | pending | `PopulateContainer` is real (`src/CHLApi.cpp:4127`), but the quest is not reached in play; undetermined how readily wolves attack people |
| Near the second point: three birch trees in a wood beside the path are set alight (each made again first if it was gone or moved more than 5 from its place); evil advisor, pointing at her, "Now this is neat. The healing sister's walking into a forest fire." and "Get over there, Boss. This I gotta see." | partial 40% | setting a thing alight works (`SetOnFire`) and `Create` makes trees; the script never runs; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| Near the third point: the good advisor points at Morg, "There's a bizarre creature causing mayhem over there.", then at her, "Our sister with the healing potions is heading for him. We must act!" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| When she reaches the edge of her brother's village she walks into his house and the journey is over | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Potions: when she is hurt

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If her health falls to 5% or less (her health is then set to nothing), or she stops existing, she has "died" | pending | the health property (`GetProperty`, `SetProperty`) is real (`src/CHLApi.cpp:1596`, `:2029`); not reached in play |
| She has three potions to bring herself back. After each of the first three deaths, two seconds later a cinematic fades to black: any body is faded away and she is made again by her hut, gossiping, with the camera on her | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play; `ObjectDelete` with fade works |
| First death: "Holy One, I've had to use one of my healing potions to restore my own health.", "I only have two left, and I need one of those to cure my brother.", "Please watch over me on my journey." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Second death: "I've had to use another potion.", "I have one left, and my brother needs that.", "Please try to ensure I come to no harm." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Third death: "Oh no! I've got no potions left. I'm now going to see my brother on his death bed!", "If I get there at all, that is." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| The screen fades back to where the camera was, and she sets off on the whole journey again from her hut | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| A fourth death ends the quest in failure | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Arriving with a potion left (fewer than three deaths): a cinematic with epic script music fades to the brother's house; the brother (an Aztec farmer) limps out injured, very slowly, and stands in despair as she comes up to him | partial 40% | the music works (`StartMusic`); the rest is never reached; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| Woman: "Thank you for looking after me during my journey." and "I can now heal my brother." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| She points at him and casts a first-level heal miracle on him from where she stands (for 3 seconds); his health is set to full | partial 30% | casting a miracle from a script works (`SpellAtPos`, `src/Magic/Script/CHLSpells.cpp`), but there is no woman or brother; setting his health is real (`SetProperty`, `src/CHLApi.cpp:2029`); re-measured 2026-10-21: casting works; the woman and brother are never made |
| The scroll is marked done in the challenge log (success 1, alignment unchanged) | pending | `Snapshot` is real |
| He shrugs, she runs to him and they hug; then both face the camera | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Brother: "Thanks for helping my sister. We promise to worship you forever." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A wolf creature is made beside the house, made to stand idle and face the camera. Brother: "Please accept this special Wolf as a token of our gratitude." | pending | CHL `Create` makes creatures now; not checked in play |
| Good advisor, pointing at the wolf: "Ah, they shouldn't have!"; evil advisor: "The Wolf is neat but I'd have preferred it if we'd killed someone." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Afterwards the two walk into the house, and once the brother is off screen both join the Aztec village of this land as ordinary villagers; the woman can now be picked up | pending | `FlockAttach` works; attaching to a town not checked; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Arriving with no potion (after three deaths): the brother crawls out and dies; she mourns. The scroll is still marked done | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Woman: "No! My brother's passed away!" and "I got here, but I have no more heal potions." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Evil advisor, pointing at the brother: "Oh this is too neat. Feel the suffering!" and "Let's make it a job well done. Kill the woman."; good advisor: "No! Her life's already ruined. Leave her!" and "You've done enough harm already. Oh unhappy day." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| She walks away in despair into the house and joins the village; the brother's body is removed once off screen | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Dying a fourth time: the camera moves (or fades) to her body. Evil advisor, pointing: "Oh yeah! The do-gooding healer woman is pushing up the daisies." and "Her brother's finished." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| The screen fades to the brother's house; he crawls out and dies there. The scroll is still marked done | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Evil advisor, pointing at him: "I told you so! Look! Ha ha!"; good advisor: "This is not funny. Cruelty is so unbecoming." The body is removed once off screen | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |

## The reward: swapping to the wolf

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wolf is offered with the same swap as every story creature: a silver scroll over its head, the evil advisor's "Swap your Creature with this one if you want.", the good advisor's confirmation, a two-creature camera for the swap, and the old creature left in its place to swap back for ever | pending | the shared swap script, see [creature_swaps.md](creature_swaps.md); described in full in [the_fish_puzzle.md](the_fish_puzzle.md#the-reward-swapping-to-the-turtle); `SwapCreature` is real |

## Quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every ending marks the scroll as done, even her death or her brother's; only the wolf depends on her arriving with a potion | pending | `Snapshot` is real |
| The scroll itself is never removed by the quest | pending | undetermined whether marking it done removes it |
| At the third point the good advisor is brought out but the script stops and sends home the evil one, so the good advisor stays pointing until the dialogue ends | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The man left when Morg is driven out is made with no health and dies on the spot | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| After a fourth death by being removed entirely (not just killed), the closing camera is aimed at a woman who no longer exists | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Morg throws at sheep for the whole land, from its start, whether or not the quest is ever taken | pending | Morg is never made |

