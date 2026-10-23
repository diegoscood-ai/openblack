# Nemesis's Shielded Village

Land 5's story beats around Nemesis's defences: the Tibetan village whose Wonder curses the creature is hidden under a
spiritual shield held up by three worshippers praying at singing stones, which the player must scare off, kill or
unseat; Nemesis's own last village fires burning meteors and raises physical shields at a creature that comes near; and
once the creature is healed, Nemesis sends barrages of blasts at the player's villages and citadel until his last
village falls. None of this is a logged quest: no script makes a scroll or a challenge-log entry for it, and the game's
text table has no title for it, so this file is named after what it is.

**Land:** 5 · **Giver:** none (the evil advisor warns of the shield when the player first nears the Tibetan village) · **Script:** ThrowThroughShield (challenge THROW_THROUGH_SHIELD), and NemesisDefendHisLastTown, MakesSmokeComeOutOfMeteor and ThrowBeamBlock in the land's control script LandControl5 · **Reward:** none (bringing the shield down lets the Tibetan village be won, the third step of the curse) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, which matches the PC game's compiled `challenge.chl`)
and the game's text table (`Scripts/InfoScript2.txt`) for every line and its speaker. openblack's state is judged on
this tree: the story's top script always runs Land 1's control script first and the land-loading command (`LoadMap` in
`src/CHLApi.cpp`) changes land now, but only through the earlier lands' exits, which no check has reached, so Land 5's control script and everything here never runs. Every row is pending unless
its notes say a working command would do that part. The curse these defences guard is in
[i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md), the fight that follows in
[so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md); the land as a whole is in
[../land_5.md](../land_5.md).

**Progress: 0/46 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Is it a gold scroll?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield script logs nothing: no scroll on the map, no challenge-log entry, no progress, and no title; its challenge name only groups the script | pending | the same script, so nothing is logged either; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It is part of the story all the same: the Tibetan village it guards holds the third Wonder, and winning that village is one of the three steps that lift the curse | pending | see [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The land's control script starts it in the background as the land begins, together with the curse and Nemesis's town defences | pending | `LandControl5` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The shield: how it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the land begins, three singing stones are put at three spots away from the village (one on a hill to the east, one on a hill to its south, one far north near the player's home) and a fourth at the shield's centre over the village | pending | `CREATE` makes mobile statics; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The shield goes up when the player first has any belief in the Tibetan village, or the creature comes within 300 of the shield's centre | pending | `BeliefForPlayer` is real (towns); not checked in play |
| The three outer stones are then made indestructible | pending | `SetIndestructable` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It only goes up if the village is not already the player's and at least one outer stone is still where it was put (within 1); otherwise nothing at all happens | pending | the town owner check's reads (`GetTownWithId`, a town's player) are real; not checked in play; the stone positions are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The shield film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film with spooky music: the camera turns to the village over 4 seconds; the evil advisor points at it: "Boss, I don't mean to worry you but some serious defences are going up here." | pending | `StartMusic`, the camera, advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Three Egyptian worshippers are made by the stones (a forester, a trader and a housewife), each facing his stone | pending | `CREATE` makes villagers and `SetFocus` turns them; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| For each stone still in place, its worshipper's guard script starts and the camera flies past for 10 seconds: first worshipper "By the almighty powers, we will protect our town.", second "Nemesis, ever generous, has granted us a Spiritual Shield.", third the first line again | pending | real camera, villager and text natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera rises high over the village (10 seconds); a magic beam runs down from 80 above the centre into the village, two flashes, and the spiritual shield is cast over the village, endless, its radius 35 for each stone that is working (up to 105) | pending | the shield is cast by `SpellAtPos` ([../../miracles/spiritual_shield.md](../../miracles/spiritual_shield.md)); `SpecialEffectPosition` is real, the beam (`AddSpotVisualTargetPos`) is a stub; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Both advisors step out: evil "These three are the source of the Shield. We gotta knock them out.", good "Their strong belief in Nemesis won't make that easy.", evil "Kill them! Smash their stones. Yeah!", good "Or we could just break their concentration.", evil "Oh yeah. Break their concentration with a boulder in the face!"; the music stops | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The worshippers and their stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each worshipper does a mocking dance by his stone | pending | villager animations by script are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A stone is working while its worshipper is alive and within 1 of where he started, and the stone is within 1 of its spot: it chants (a Tibetan chanting sound, one of three, tied to him) and a magic beam runs from 5 above it to the top of the shield | pending | `AttachSoundTag` and `SpecialEffectObject` are real; the beam is a stub; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| When either moves away the chant and beam stop; they come back when both are in place again | pending | the same; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Every 3 seconds a working stone sparkles; if the creature is within 100 of it and not invisible, a lightning strike leaps from the stone at the creature's head and either side of it, then the creature is let go after 3 seconds | pending | `SpecialEffectObject` is real; whether the strike hurts is not determined; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Every 3 seconds the worshipper checks for near misses within 40: a flying rock or tree (not one the script made) or a fireball. If one is close he ducks, runs to a hiding spot, looks round for 45 seconds, then walks back and dances again | pending | `CallNear` and villager moves are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Killing a worshipper switches his stone off for good; a stone also stays off while it is out of place | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The shield shrinks or grows by 35 each time a stone stops or starts | pending | `SetMagicRadius` is a stub |

## Bringing it down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield falls the moment all three stones are off at once; a scared worshipper's 45 seconds of hiding are the window to deal with the others | pending | the stage count drives the radius, a stub; the fall itself is plain script |
| The shield and the beam are deleted and the worshippers' guard scripts end, so they never put it back up | pending | `ObjectDelete` of a spell is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If all three worshippers still exist the good advisor says "We did it! The Shield's down and no one got killed."; otherwise the evil advisor: "Sweet, Boss. One dead Shield and a few human casualties. Couldn't have done better myself." | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| With the shield down the Tibetan village can be impressed or conquered; its first capture blasts its Wonder and logs the curse's step | pending | the town capture and its belief read real natives (`BeliefForPlayer` of a town, a town's player); not checked in play; see [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |
| It cannot soft-lock: a worshipper can always be killed, and nothing restarts the shield once it has fallen | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Nemesis's last village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start of the land, whenever the creature is within 200 of a point by Nemesis's village, a burning meteor is fired at it every 6 seconds (the first after 10) | pending | `CreateTimer` and `GetTimerTimeRemaining` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Each meteor is a half-size rock made 250 up the mountain behind the village, set on fire, heated to 2000, kept out of the wind and thrown to land in 6 seconds within 30 of a fixed spot west of the village (not at the creature) | pending | `CreateWithAngleAndScale`, `SetOnFire` and `SetTemperature` are real, and so is the aim, `SetTarget` is real (not seen in play yet); `SetAffectedByWind` is real (on a rock it does nothing, as in the game); burning rocks: [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md) |
| While it flies and is not held it trails a huge smoke plume (a bonfire effect eight times normal size, renewed constantly) | pending | `SpecialEffectPosition` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| 6 seconds after launch, once it is neither held nor flying, a fireball is cast just by it and it is deleted with an explosion; a meteor caught by the hand explodes once it is let go and has landed | pending | the fireball is cast by `SpellAtPos` ([../../miracles/fireball.md](../../miracles/fireball.md)) and `ObjectDelete` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Every 30 seconds while the creature is near, a physical shield (radius 75, 30 seconds) is raised over Nemesis's village | pending | cast by `SpellAtPos` ([../../miracles/physical_shield.md](../../miracles/physical_shield.md)); the timer is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| These defences run until Nemesis's last village is won | pending | the town capture reads (a town's player, `BeliefForPlayer`) are real; not checked in play |
| While the curse is on the village cannot be won (the player's belief there is reset whenever it reaches 0.1) | pending | `SetPlayerBelief` and `BeliefForPlayer` are real (towns); not checked in play; see [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |
| After the healing scene, it can be won only while Nemesis holds none of the land's other villages (the neutral Japanese one included): whenever he holds one, the player's belief there is set back to 0 | pending | `SetPlayerBelief` is real (towns); not checked in play |
| The first time he holds one again: good "Leader! Nemesis has retaken one of our Villages!", evil "Aw, shoot. Everything's lost unless we get it back. Come on, Boss."; when he holds none again: evil "Boss! We can conquer Nemesis' last town! We got the power. I can sense it!", and the warning can then come again | pending | the town owner reads are real; not checked in play |
| Winning it ends the land's loop, stops every other Land 5 script and starts the big fight | pending | the town capture reads are real; not checked in play; see [so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md), [../ending.md](../ending.md) |

## Nemesis's blast barrages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the healing scene, every 15 seconds one of four targets is picked at random: the Aztec village, the Greek village, a spot south-east of the Tibetan village, or the player's citadel; the three villages are only attacked if the player owns them, the citadel always | pending | `Random` and `SpellAtPos` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A barrage is a line of blasts marching from a start point by Nemesis's land towards the target: each step is a row of seven blasts (one in the line and three either side, 20 apart), each a level-one blast (radius 50, 15 seconds) dropped from 30 above | pending | each blast is cast by `SpellAtPos` ([../../miracles/blast.md](../../miracles/blast.md)); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Village barrages take 20 steps, 2 seconds apart; the citadel barrage takes 10 steps, 1 second apart, from a closer start | pending | plain script timing; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The steps start at the start point and stop one step short of the target | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The barrages end when Nemesis's last village is won | pending | the town capture reads (a town's player, `BeliefForPlayer`) are real; not checked in play |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield film plays the spooky script theme | pending | `StartMusic` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The three working stones chant (three Tibetan chanting recordings, one per worshipper) | pending | `AttachSoundTag` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Script quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield's main loop, Nemesis's defence loop and the barrage loop have no pauses; the shield loop makes a flash at the top of the shield on every pass | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A new near-miss check is started every 3 seconds even while an earlier one is still sending the worshipper to hide, so two can move him at once | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the player moves all three stones off their spots before the trigger, the shield never goes up and nothing is said | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can break the shield by throwing rocks or trees near the worshippers, or by killing them; a working stone strikes at it with lightning when it is within 100 unless it is invisible | pending | the creature throws nothing by script and its flights are not checked here; see [../../creature/object_actions.md](../../creature/object_actions.md) |
| Bringing the creature within 200 of Nemesis's village draws his meteors and shields | pending | the distance checks are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
