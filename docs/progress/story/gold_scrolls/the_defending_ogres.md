# The Defending Ogres

The fourth land's opening story: the player arrives in a ruined land under fireballs, lightning and a darkened sky, and a
frightened man of the Japanese village tells how Nemesis set three Guardian Stones to hold the land. The scroll logged
by his story is the ogre's: Sleg guards the lightning stone in his lair and must be beaten in a creature fight, while
his four gremlins raid the player's village for people to feed him.

**Land:** 4 · **Giver:** a Japanese farmer who comes to pray at the player's temple (the story); Sleg the ogre in his lair (the fight) · **Script:** Land4Meteorites (the man's story, the meteors), Land4Ogre (Sleg and the gremlins), BeginLand4 (arrival) · **Reward:** the lightning Guardian Stone is destroyed: the lightning storms and rain end · **Repeatable:** the fight, until won

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table (`InfoScript2.txt`) for every spoken line and its speaker, and the executable.
openblack is judged on this tree: the land never runs there (openblack starts the story's top script, which always runs
Land 1's control script first, and the map-loading command has an empty body). Of the 99 commands this story needs, 17
only log "not implemented" in `src/CHLApi.cpp`, among them making and naming a creature, creature orders, desires and
fights, and the challenge log; the rest work, among them casting a miracle from a script (`SpellAtPos`), the time of day
and clock (`SetGameTime`, `GameTimeOnOff`, `MoveGameTime`), villagers, highlights, dialogue, advisors, camera moves,
weather settings, fades, music, the indestructible and pick-up flags and taking off or disabling the leash. Every row is
todo unless the notes say otherwise. The land as a whole is in [../land_4.md](../land_4.md); its script program in
[../../scripts/land4_script.md](../../scripts/land4_script.md).

**Progress: 0/79 done, 0 partial — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script sets up the land, plays the arrival, sets up the undead village, then starts in the background the totem puzzles, the fish puzzle, the creature breeder, the town-building advice, Sleg, the Heartbroken Man, the Totem Puzzle, the creature's ogre fence, the man's story and the blind woman's scroll | todo | `LandControl4` is never reached: `LOAD_MAP` is empty |
| The land then waits for all three Guardian Stones to be broken (lightning: Sleg; darkness: [the_heartbroken_man.md](the_heartbroken_man.md); fire: [the_totem_puzzle.md](the_totem_puzzle.md)), in any order, before [undead_village.md](undead_village.md) | todo | plain script waits on flags; see [the_heartbroken_man.md](the_heartbroken_man.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Sleg is waiting from the start of the land: led on the leash, the creature can find and fight him before the man has told his story; the story only logs the scroll and starts the gremlins | todo | Sleg is never made (`CreatureCreateRelativeToCreature` is a stub) |
| A second gold scroll over Sleg's lair is put up only when the Heartbroken Man's stone is broken and Sleg is still unbeaten; nothing waits for it to be clicked, and it is removed when Sleg is beaten | todo | `CreateHighlight` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A notify script written for this scroll (an advisor nagging until the lair is clicked or the creature comes within 100) is never used by the game | n/a | written but never run |
| A test launcher for the ogre alone (it sets up the land, logs the scroll as a silver challenge, puts up the lair's scroll and runs Sleg) exists in the source but is not in the shipped scripts | n/a | not compiled into `challenge.chl` |

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's setup: the gates by the home village are opened; the home village, the Japanese village and the Aztec village are found; the home village's need for sleep is lowered by 0.5 and for food by 0.2 (done twice, by the setup and by the arrival) | todo | `SetOpenClose` is a stub; the town look-ups and desire boost are partly real (`SetTownDesireBoost`) |
| The exit vortex of the last land opens at the arrival point, the player's people come through it into the home village and the creature is loaded beside it; after 15 seconds it starts to fade and 8 seconds later it is closed | todo | `CREATE` of a vortex is ported and `LoadMyCreature` is real; see [../portals.md](../portals.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| While the vortex is open the creature is made to point at it again and again, so it can't walk into it | todo | `CreatureDoAction` is a stub |
| The temple stands at 80% built, the town centre at 80%, the workshop and two houses at 50%: the player's buildings survived, damaged | todo | `BuildBuilding` and the built percentage (property 22) are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A shield miracle dispenser stands half built near the temple and gives a shield one-shot every minute, without any announcement | todo | `CREATE` of a spell dispenser is real (`magic::script::CreateSpellDispenser`); see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The arrival scene, with Nemesis's music: the camera starts inside the vortex, fades in over 6 seconds, rises 65 out of it over 10 seconds, sweeps to the village over 12 seconds and then to the land's first camera path | todo | `StartMusic`, `SetFadeIn`, the camera moves and `RunCameraPath` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| If Lethys was spared on the third land: the good advisor "Wow! Here we are." / "You know, you did the right thing by sparing Lethys, Leader."; if he was killed: the evil advisor "Hey Boss! Here we are!" | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Along the camera path: good "Just look at the state of the place."; evil "It's been utterly blasted. How cool is that?"; good "It's awful. It used to be green and pleasant."; evil "What? We've never been here before!"; good (pointing at the gates) "We have, actually. You remember these?"; good "And I recall some of these buildings."; good (pointing at the temple) "And even our Temple. It's all rather damaged, though."; evil "You're right. Still, the atmosphere's much better now." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The music stops and the land counts the arrival as finished; the man's story can't start before this | todo | plain script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The three curses (the meteors)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lightning: five drizzle storms sit near the village and temple, along the road to the ogre and at the gates: rain 0.2, overcast 0.9, 6 clouds of shade 1.7 at height 180, sheet lightning 2 to 10 and forked lightning 2 to 5, reach 200 to 400, lasting practically for ever and not blown by the wind | todo | `CREATE` of a weather thing and the `Change*Properties` natives are real (`src/Magic/Script/CHLWeather.cpp`); see [../../weather/storms.md](../../weather/storms.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| When Sleg's stone breaks, each storm is set to end within 8 seconds, fading over 12 | todo | the weather time-fade natives are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Darkness: the time of day is set to 16:50 and moved towards 19:00 over 1,000 seconds; after two minutes the clock is stopped, leaving an evening sky until the darkness stone breaks, when the clock runs again | todo | `SetGameTime`, `MoveGameTime` and `GameTimeOnOff` drive the sky clock (`src/CHLApi.cpp`); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fire: five fireball throwers, all from one point high in the sky (height 255) to the north, each aimed at its own spot near the home village with a random spread of 50 (100 for one); the level-one fireball miracle, cast as no player's, radius 1 | todo | `SpellAtPos` casts at a point ([../../miracles/fireball.md](../../miracles/fireball.md)); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The fireballs come in waves: the first wave starts after 4 minutes, then they run for 2 minutes and stop for 2 minutes in turn, until the fire stone breaks | todo | the timers are real (`src/ECS/ScriptTimer`); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| When a wave stops: evil "Oh. The fireballs have stopped!", good "For now, at least."; when one starts: good "Oh dear. Here come the fireballs again." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Once the temple's worship site has been seen fully built, a wave with the town centre built waits 30 seconds, the good advisor warns "Some of those fiery orbs are getting close to our Village!", and 30 seconds later the fireballs turn aggressive: two of the throwers aim at the village's edge instead, at double strength | todo | the built percentage read and the advisors are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| How long a thrower waits between fireballs | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The waves don't switch on or off while the Totem Puzzle is being played, nor during the man's story (the fireballs are forced on for his scene) | todo | plain script flags; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Building up the village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 30 seconds into the land: good (pointing at the town centre) "We have to build up our Village and shelter the people from this terrible storm."; evil "No, leave them. It'll put hairs on their chests." / "Better to explore and find out what's going down in this land." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| When the workshop is fully built and the camera is within 150 of the village or looking at it or the workshop: good (pointing at the workshop) "Excellent. The Workshop is built. Now let's concentrate on making a Village Store." / "You'll need three combined Scaffolds to build it." / "And we mustn't leave our poor Villagers homeless in this weather."; evil "Hey. Okay, we need a Village Store. But homes for the people? No way!" / "Forget them. We gotta get exploring. Somewhere there'll be an element of the Creed, and I want to find it." | todo | the built percentage read, field-of-view and advisors are real; see [../../building/workshop_and_scaffolds.md](../../building/workshop_and_scaffolds.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The man's story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story waits until the arrival scene is over, then 30 more seconds, so a player skipping the arrival text isn't pulled straight into it | todo | plain script; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| It then waits five minutes, or less if the player explores too early: the camera within 150 of the Japanese village or 250 of Sleg's lair, or the creature straying within 150 of Sleg | todo | distance and field-of-view reads are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A creature that strays too near Sleg before the story is sent back towards the village (to a spot on the road, within 50) | todo | moving a creature by script (`MoveGameThing`) is not done |
| When his spot by the temple is out of view, a Japanese farmer appears and walks onto it; he can't be picked up or hurt; he faces the temple and prays | todo | `CREATE` makes villagers and the flags and animations are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| If the player hasn't explored too early, a gold scroll appears by him and the good advisor nags "Your godly attention is required here, Leader." (within 100, at most every 30 seconds); the scene waits for the scroll to be clicked, or for the player to explore too early | todo | `CreateHighlight` and the notifier are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Exploring too early skips the scroll: the player is pulled straight into the scene with a fade to black, and the man prays once more before standing | todo | fades and villager animations are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The scene, with the guide's music; the fireballs are forced on for it. Clicked: the camera glides to him over 6 seconds and he stands up from praying | todo | `StartMusic` and the camera are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The man, gossiping: "Please don't harm me." / "I come, trembling with fear, before you." / (pointing at his village; the camera turns to look that way) "I'm from the Village nearby." | todo | villager animations and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to his village; the camera pans around it for 5 seconds: "I ask that you remove the curse on our land." | todo | fades, the camera and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Back with him: "A god visited us long ago." / "He showed us his power and his might." / "It was a terrifying display." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the past: the time of day set to noon and stopped, a beech tree made in a valley, sound effects off, the camera drifting for 20 seconds: "You see, this land used to be beautiful." / "We lived in peace here." / "And we didn't want for anything." / "Then the gods themselves appeared with the concepts of good and evil." / "At first, all was well." / "But then the one called Nemesis arrived and punished us for following you." | todo | the clock, `CREATE` of a tree and `SetGameSound` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Sound effects come back and an explosion miracle (level two, radius 50, 15 seconds) falls on the tree from 30 above; 2.5 seconds later a fade to white and the tree is gone | todo | `SpellAtPos` casts the explosion; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Over the land at 16:50: "We were forced to worship him." / "He brought forth fire from the heavens." | todo | real clock, camera and text natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the ogre's road: "He gave us pillars of lightning." / "Our lives have become a living hell." | todo | real natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the mountains: "Even the sky turned the colour of blood." | todo | real natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Back with him: "But Nemesis got bored with us and left us alone." / "He wanted to continue his quest to be the only god." / "So he set three Guardian Stones to control this land." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the Japanese village from above: "The first lies under a Spiritual Shield." / "Even the wisest of our people don't know how to reach it." | todo | real natives; see [the_totem_puzzle.md](the_totem_puzzle.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to Sleg's lair, the camera creeping along for 25 seconds; the scroll is entered in the challenge log here, titled "The Defending Ogres", at 0%, with the reminder "Defeat the Ogre to get the Guardian Stone we need." (said by the good advisor when the entry is clicked) | todo | `Snapshot` is a stub |
| "The second Guardian Stone was entrusted to a vicious Ogre." / "He's too strong for any mere mortal to beat." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the hills of the Heartbroken Man, the camera moving for 20 seconds: "The third Stone was given to a lonely old man." / "Who worships only Nemesis." | todo | real natives; see [the_heartbroken_man.md](the_heartbroken_man.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Back with him: "This is everything we know." / "Please help us in our plight." / "You have all of our support, Holy One."; he turns round and the music stops | todo | real natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Afterwards he can be picked up and hurt; he walks home along six points of the road and joins the Japanese village; the gremlins are let loose | todo | the flags and walking are real, but joining the town (`AttachToGame`) is a stub |
| Quirk: the scene leaves the time of day at 16:50 and doesn't restart the clock (the darkness had stopped it anyway) | todo | the clock natives are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Every 3 minutes until the player's belief in the Japanese village reaches 0.3 (or the village is gone), checked every 5 seconds: good "Leader, let's start impressing the Village."; evil "If it means we get our hands on the Guardian Stone, I'm for it." | todo | `BeliefForPlayer` is a stub |

## Sleg

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sleg is made in his lair as an ogre built from the player's creature at 0.9 of its size, named "Sleg", with every desire off except anger (0.2, at most 0.3), no hunger and no wish to make friends; he scales himself at 1.1 | todo | `CreatureCreateRelativeToCreature`, `SetCreatureName` and the desire natives are stubs |
| His strength is the creature's times 1.3, at most full; full health; fully evil; slow (speed 0.2) | todo | Sleg is never made, and these properties are not handled |
| The Guardian Stone is a meteor beside him at 0.3 scale, 2 above the ground, indestructible, fixed and not pick-up-able, under a huge bonfire smoke (scale 25) that burns practically for ever | todo | the flags and `CREATE` of the meteor are real, as is `SpecialEffectPosition`; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| He sits in his lair, facing out, until the creature comes within 150 of the fighting ground before his lair or within 80 of him | todo | Sleg is never made |
| Without the leash the creature can't reach him on its own: while the stone stands, a creature not on the leash that comes within 250 of the fighting ground (or 100 of Sleg) but not within 150 of the ground is sent back to the home village (within 50) for 3 seconds | todo | `IsLeashed` is real; moving the creature by script is not done |
| When the creature arrives, its leash is taken off and it is put on its side of the fighting ground | todo | `DetachObjectLeash` and `SetPosition` are real ([../../creature/leash.md](../../creature/leash.md)); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The scene, with Sleg's music: the camera swings to the fighting ground; Sleg stands up from his lair and walks to his side | todo | `StartMusic` is real; Sleg is never made |
| The first time: he waits 5 seconds facing the creature, the camera rises to his height, the creature is set facing him; Sleg, in his own voice: "I am Sleg, son of Sleg." / "And yes, I have a Guardian Stone." / (the camera slides along for 20 seconds) "If you want it, you must be prepared to fight." / "Many have tried but I have never been defeated." | todo | Sleg is never made |
| Later times: after 3 seconds, "My name is Sleg and I am undefeated." and the camera is set above the ground | todo | Sleg is never made |
| The leash is switched off and Sleg starts the fight; the game waits up to 45 seconds for the creature to be fighting, then switches the leash back on | todo | duels exist in our tree ([../../creature/fighting.md](../../creature/fighting.md)), but Sleg is never made and `CreatureDoAction`, `IsFighting` are stubs |
| When the fight stops, if either side's fight health is below 0.1 the fight is decided: the creature wins if its fight health is higher | todo | fight health is not a handled property |
| Losing: the creature's health is set to 0 and it is made to die; Sleg laughs, faces the camera and walks back; fade, and the creature is set by the temple; Sleg heals 1% every 6 seconds (10 minutes to full) unless fighting again | todo | creature health and states by script are not done |
| Walking away or stopping without a decision: the same end scene (Sleg laughs, the creature is put by the temple) without the death | todo | Sleg is never made |
| The fight can be tried again as often as needed: Sleg goes back to his lair and waits again | todo | Sleg is never made |
| Nothing fails this quest: the land waits for the stone for ever | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Sleg beaten

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll goes to 100% in the challenge log and the lair's gold scroll is removed if it is up | todo | `UpdateSnapshot` is a stub |
| After 2 seconds the evil advisor, pointing at Sleg: "Oh boy. You told him, Boss. You kicked his butt!" | todo | Sleg is never made; the line is real |
| Fade to the stone; Sleg is removed from the world during the fade; the evil advisor, pointing at the stone: "The Guardian Stone is yours." | todo | Sleg is never made |
| The Guardian Stones' music, a light shake (radius 300, 0.1, 5 seconds); the camera follows the stone as it rises to 20 above the ground, then it bursts with a bang, a flash and an explosion sound | todo | `StartMusic`, `ShakeCamera` and `SpecialEffectPosition` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade to the sky over the road; 3 seconds later the stone counts as broken and the lightning storms end | todo | the weather natives are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Good "The lightning is stopping. See?"; evil "And it's not raining! Huh. I hate rain. It cools me down." | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Fade back to where the player was looking; the music stops; the smoke over the lair is removed | todo | fades, `StopMusic` and `ObjectDelete` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The gremlins

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once the man has told his story, four gremlins are made around Sleg's lair (within 15): little ogres made from the player's creature, a third of normal size, fully evil, friends with Sleg, not pick-up-able | todo | `CreatureCreateRelativeToCreature` and `CreatureForceFriends` are stubs |
| Raids: checked every 30 seconds, a raid starts if none is on, the home village has more than 10 people, the first 10 minutes since the gremlins appeared have passed and no fireball wave is falling; it lasts 150 seconds, and the next can't start for 5 minutes | todo | the gremlins are never made |
| On a raid each gremlin walks by two road points to the home village, picks up a villager of that village within 45 that no script is using, carries him back to Sleg and throws him at Sleg; the villager is left at 0.1 health | todo | the gremlins are never made |
| A gremlin flees 50 away from the creature whenever the creature is within 50, and idles around a spot by the lair between raids | todo | the gremlins are never made |
| While the creature is not within 150, Sleg eats any villager within 50 of him alive, one every 20 seconds at most, and walks home if he has strayed; with the creature near, he only stares at it | todo | Sleg is never made |
| A gremlin brought below 0.3 health dies: it turns into a dead villager in a burst of five sparkles, and the good advisor says "Oh no. That little ogre turned into a human when it died." | todo | the gremlins are never made |
| When all four are dead: good "All the people who were turned into gremlins have been killed." | todo | the gremlins are never made |
| The first time a gremlin (any ogre) is within 75 of the home village: good, pointing, "Our Village is under attack from gremlins!"; later, at most every 4 minutes, "The gremlins are attacking our Village again!"; this stops once all the gremlins are dealt with | todo | the gremlins are never made |
| Raids stop for good once Sleg is beaten, but the gremlins live on | todo | the gremlins are never made |

## Unused or cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Curing a gremlin back into a villager (turning it human and adding it to the home village) was planned but is commented out, so the "You've cured all the cursed Villagers." / "They've recovered fully." and "All those little ogres have been dealt with." / "You haven't cured all the cursed Villagers." lines can never play | n/a | the count of cured gremlins never rises |
| An unused gold-scroll notify for the ogre (above) and the ogre-only test launcher | n/a | unused or not compiled |
