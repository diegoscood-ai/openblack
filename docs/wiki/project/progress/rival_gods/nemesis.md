# Nemesis

The game's final enemy: a god who wants to be the only one. He kills Khazar on the second land, is behind Lethys and the
curses that follow, and on the fifth land is the second player, whose four towns and temple the player must win before
facing his creature.

**Progress: 6/41 done, 8 partial — 24%**

See [../story/land_5.md](../story/land_5.md) for the land's challenges and curses, [khazar.md](khazar.md) for Khazar's death,
and [ai.md](ai.md), [magic.md](magic.md), [towns_and_influence.md](towns_and_influence.md) and
[creatures.md](creatures.md) for how every computer god works.

## On the fifth land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the second player a computer player before anything else | partial | `TOGGLE_COMPUTER_PLAYER` adds the player (`PlayerArchetype`) with no mind |
| His temple stands at the land's far side, with Japanese, Greek, Tibetan and Aztec worship sites round it | done | the temple with its owner (`CitadelArchetype`) and the sites (`magic::script::CreateWorshipSite`) |
| His Aztec home town has lightning (and its second power-up), the beam explosion (and second power-up), food, storm (and second power-up), water (and power-up) and the strong creature miracle | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| A Tibetan town of his has fire, lightning, the beam explosion, heal (each with power-ups), teleport and nature | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| His Greek town has lightning, heal, storm (with power-ups), shield, physical shield and wood | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| A second Tibetan town has fire, lightning, the beam explosion, heal (with power-ups), teleport and nature | partial | the town and its owner are made (`TownArchetype`) and it holds its miracles (`magic::script::CreateNewTownSpell`); no god casts them |
| His towns start believing 1 in him | done | `SET_TOWN_BELIEF` (`ecs::town_belief::SetBeliefInPlayer`); see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| A neutral Celtic town believes 5 in him and 5 in no one, so it is hard to win | done | `SET_TOWN_BELIEF` for him and for the neutral slot |
| His influence rings his temple and towns in his player colour | done | the influence of his temple and towns (`src/ECS/Influence`), drawn in his colour (`influence::k_CircleColours`) |
| He is not the player's ally | done | no alliance exists in our tree, which matches |
| His attitude to the player is set to 2 (kept doubled inside, as 4) | todo | `SET_COMPUTER_PLAYER_ATTITUDE` is a stub; see [ai.md](ai.md) |

## His creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A lion loaded from his mind file near his temple, drawn twice its auto-scaled size | todo | `LOAD_CREATURE` is a stub; see [creatures.md](creatures.md) |
| Named from the game's text (the script's note calls it Ichor), fully grown, every skill and nearly every miracle known | todo | `SET_CREATURE_NAME`, `CREATURE_AUTOSCALE` and `CREATURE_SET_KNOWS_ACTION` are stubs |
| Once the player has a creature it takes the opposite alignment | todo | the creature is never loaded |
| In the final fight it is turned invisible and replaced by a creature of the player's own species from his mind file | todo | see [The final fight](#the-final-fight) |

## His arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His mind is paused while his hand flies at speed 400 to meet the player, to his own music | todo | the music exists (`MUSIC_TYPE_SCRIPT_NEMESIS`), but the story never reaches Land 5 (`LOAD_MAP` is empty) and his hand's natives are stubs |
| He drifts closer at speed 8 while the camera lens widens to 100 over 15 seconds | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); his hand's natives are stubs |
| He promises to be the embodiment of good or of evil, whichever the player isn't | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); the script reads the player's alignment (`GET_ALIGNMENT` works) |
| Two short scenes show him moving (one with a storm and the time of day set to 16:30) and give his hand back afterwards (unconfirmed when they run: nothing in the land scripts starts them) | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |

## Holding his towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the land's first phase (unconfirmed what ends it), any belief the player wins in his home town above 0.1 is set back to 0, and the good advisor explains why when the town is in view | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); see [../story/land_5.md](../story/land_5.md) |
| The first time the player wins each of his three other towns, his hand appears in a scene and an explosion falls from 30 metres above | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| Winning his second Tibetan town also brings a tornado on it (radius 25, 30 seconds) and starts the "fire on high" challenge | partial | dormant: the tornado can be cast by script (`SPELL_AT_POS`), but the story never reaches Land 5 (`LOAD_MAP` is empty); see [../miracles/tornado.md](../miracles/tornado.md) |
| The "throw through the shield" challenge is started by the land's control script as the land begins; its spiritual shield guards the Tibetan village that holds the third Wonder | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); [../story/gold_scrolls/nemesis_shielded_village.md](../story/gold_scrolls/nemesis_shielded_village.md) |
| If he takes a town back, the land notices and the scene can play again (unconfirmed) | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| While he still holds any of the four other towns, the player's belief in his home town is held at 0 and they are told so | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| When he holds none of them, the home town can be won, and a line says so | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| Winning his home town ends his resistance | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |

## Defending his last town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player's creature is within 200 metres of his home town, a burning rock is fired from a cannon high on his land every 6 seconds (the first after 10) | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); [../story/gold_scrolls/nemesis_shielded_village.md](../story/gold_scrolls/nemesis_shielded_village.md#nemesiss-last-village) |
| Each rock is half size, set on fire, at 2000 degrees, out of the wind, aimed within 30 metres of a point in front of the town and trailing smoke | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); see [../physics/](../physics/) |
| A physical shield of radius 75 is raised over the town for 30 seconds, every 30 seconds | partial | dormant: the shield can be cast by script (`SPELL_AT_POS`), but the story never reaches Land 5 (`LOAD_MAP` is empty); see [../miracles/physical_shield.md](../miracles/physical_shield.md) |
| Every 15 seconds one of four is picked at random: a beam of blocks thrown at one of the three towns he lost, if the player owns it, or at the player's temple | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); [../story/gold_scrolls/nemesis_shielded_village.md](../story/gold_scrolls/nemesis_shielded_village.md#nemesiss-blast-barrages) |

## The final fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every other script on the land stops and his hand is set over the arena | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); `SET_COMPUTER_PLAYER_POSITION` is a stub |
| His lion is made invisible and a creature of the player's species is loaded from his mind file at the arena | todo | `LOAD_CREATURE` is a stub |
| The double is scaled to the player's creature and given full strength | todo | `LOAD_CREATURE` and `CREATURE_AUTOSCALE` are stubs |
| Both creatures are forced to fight once the player's comes within 300 metres | partial | dormant: the creature fight exists (`CreatureFightSystem`, see [../creature/fighting.md](../creature/fighting.md)), but the story never reaches Land 5 (`LOAD_MAP` is empty) |
| Whenever the double falls below half its fighting health, it is healed by a level-two heal and put back to full, at most four times | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| If the player's creature ends with more fighting health, the double dies for good | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| Otherwise the player's creature falls, the double heals 1% every 6 seconds until it is next in a fight (10 minutes to full), and the player may try again | todo | the story never reaches Land 5 (`LOAD_MAP` is empty) |
| Then his creature goes to the volcano, his temple is destroyed and he is switched off | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); `ENABLE_DISABLE_COMPUTER_PLAYER` is a stub |
| The villagers wave the player off | todo | the story never reaches Land 5 (`LOAD_MAP` is empty); see [../story/land_5.md](../story/land_5.md) |

## On other lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He is never a player on the second, third or fourth land; his attacks there are scripted from the vortex or from points in the sky | n/a | there is no Nemesis player to judge; see [khazar.md](khazar.md) and [../story/land_4.md](../story/land_4.md) |
| A script for a step-by-step battle plan (impress, destroy, defend, attack the creature) exists but nothing runs it | n/a | unused in the shipped game; see [challenge_scripts.md](challenge_scripts.md) |
| His mind file also supplies the creatures of one skirmish land | done | `CREATE_CREATURE_FROM_FILE` loads it for Island Wars' gods; see [skirmish_opponents.md](skirmish_opponents.md) |
