# Lethys

The evil rival god of the second and third lands. On the second land he is the third player, raining fireballs and
taunting the player while they win his towns; on the third he is the second player, holding the player's creature
prisoner on Nemesis's behalf until the player frees it and takes his last town, when his temple is destroyed.

**Progress: 6/50 done, 0 chosen, 13 partial, 0 inferred — 23%**

See [../story/land_2.md](../story/land_2.md) and [../story/land_3.md](../story/land_3.md) for the lands' challenges, and
[ai.md](ai.md), [magic.md](magic.md), [towns_and_influence.md](towns_and_influence.md) and
[creatures.md](creatures.md) for how every computer god works.

## On the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script makes the third player a computer player | partial 40% | `TOGGLE_COMPUTER_PLAYER` adds the player (`PlayerArchetype`) with no mind behind it; re-measured 2026-10-21: the player is added and marked a computer player; no mind runs it |
| His temple and a Celtic worship site stand in the land's snowy corner, owned by him | done | the temple with its owner (`CitadelArchetype`) and the site (`magic::script::CreateWorshipSite`) |
| He owns three Celtic towns: one with lightning, one with heal, and his home town with food, shield, physical shield and water | partial 40% | the towns and owners are made (`TownArchetype`) and hold their miracles (`magic::script::CreateNewTownSpell`); no god casts them; re-measured 2026-10-21: the towns, owners and miracles are made; no computer god casts them |
| Each starts believing 0.65 in him, with belief in him and Khazar capped at 0.75 | done | `SET_TOWN_BELIEF` and `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetBeliefInPlayer`, `SetCap`); see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| His influence rings his temple and towns in his player colour | done | the influence of his temple and towns (`src/ECS/Influence`), drawn in his colour (`influence::k_CircleColours`) |
| His mind is told to expand his influence only a little (one fifth of full) | partial 30% | the weight is set (`SET_COMPUTER_PLAYER_PERSONALITY`, `computer_player::SetPersonality`); the mind's turn that reads it is not wired; see [ai.md](ai.md); re-measured 2026-10-21: the score rules are ported and pinned to the oracle; the mind's turn that would use them is not wired |
| His attitude to Khazar is set to a quarter | partial 40% | set (`SET_COMPUTER_PLAYER_ATTITUDE`, `computer_player::SetAttitude`); no score function reads it yet; see [ai.md](ai.md); re-measured 2026-10-21: the value is set; nothing reads it yet |
| Lethys and the player are not allies | done | no alliance exists in our tree, which matches |

## His creature on the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wolf loaded from his mind file near his temple, drawn 1.5 times its auto-scaled size | partial 60% | loaded by `LOAD_CREATURE` (`ecs::player_creature::ScriptLoadCreature`); the 1.5 times size through `CREATURE_AUTOSCALE` is not confirmed in a run (no run reaches the land); see [creatures.md](creatures.md); re-measured 2026-10-21: loaded as the script asks; the size is not seen in a run yet |
| Named from the game's text (the script's note calls it Laetes), fully grown, every skill and nearly every miracle known | pending | `SET_CREATURE_NAME`, `CREATURE_AUTOSCALE` and `CREATURE_SET_KNOWS_ACTION` are real |
| Once the player has a creature it takes the opposite alignment | pending | the creature is never loaded |
| When Khazar dies, his creature is made strong, goes to Khazar's creature and leaves through the vortex | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [khazar.md](khazar.md); the vortices: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-khazars-death-and-lethyss-vortex); [../story/gold_scrolls/nemesis_no.md](../story/gold_scrolls/nemesis_no.md#the-film-lethys-takes-the-creed) |

## Raids and taunts on the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player arrives, his hand tours eight points of his land at speed 150, at a fixed height | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| Every 40 to 80 seconds (the first after 50) his hand flies to the first point and he fakes an attack | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| A fake attack is three level-two fireballs at one spot, 2, 5 and 2 seconds apart, thrown from his hand | partial 50% | dormant: `SPELL_AT_POS` casts them, from the land under his hand (`GET_COMPUTER_PLAYER_POSITION`) and as the neutral player; Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the natives it needs work; the story has not reached it in a run yet |
| The tour stops and his mind takes over once the arrival ends | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While he lives, whenever his influence at the camera beats the player's he comes to taunt them, at most once a minute | pending | `GET_INFLUENCE` works (`magic::script::GetInfluence`), but coming over needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| His hand flies to 10 metres in front of the camera at speed 350, rechecking every 0.3 seconds, and is put there after 10 seconds | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play |
| His taunts are spoken in his own voice (one of six) rather than shown as text | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; spoken lines come from the extra sound bank |
| He stays quiet while another script is using him | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When the player wins one of his towns, Khazar praises it and 6 seconds later Lethys rages (one of four lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [towns_and_influence.md](towns_and_influence.md) |
| When he wins one of the player's towns he gloats (one of four lines) | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Losing the second land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking any one of his three towns can bring about Khazar's death | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [khazar.md](khazar.md) |
| After Khazar's death, once two of his three towns are lost or he holds one or none, the vortex out of the land opens | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; Lethys takes the player's creature through it: [../story/gold_scrolls/lethys_has_taken_our_creature.md](../story/gold_scrolls/lethys_has_taken_our_creature.md) |
| Storm clouds gather over his temple: five monsoon clouds, 80 seconds, still and out of the wind | partial 50% | dormant: the weather objects exist (CHL `CREATE` of a weather thing, `src/Magic/Script/CHLWeather.cpp`), but Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../weather/](../weather/); re-measured 2026-10-21: the natives it needs work; the story has not reached it in a run yet |
| When he holds no town and his temple is at a tenth of its health or less, the player can leave through the vortex | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the temple's heart keeps a life (`CitadelArchetype`, `ecs::life`) |
| The final scroll counts how many of his towns remain | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../story/land_2.md](../story/land_2.md); [../story/gold_scrolls/destroy_it.md](../story/gold_scrolls/destroy_it.md) |

## On the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He is the land's second player, a computer player | partial 40% | `TOGGLE_COMPUTER_PLAYER` adds the player (`PlayerArchetype`) with no mind behind it; re-measured 2026-10-21: the player is added and marked a computer player; no mind runs it |
| His temple stands on a plateau, with Egyptian and Tibetan worship sites | done | the temple with its owner and the sites (`magic::script::CreateWorshipSite`) |
| His Tibetan town at the foot of the plateau has lightning, heal, food and storm, and believes 1.9 in him, capped at 2 | partial 40% | town, owner, belief and its cap are made (`SetBeliefInPlayer`, `SetCap`); it holds its miracles (`CreateNewTownSpell`), but no god casts them; re-measured 2026-10-21: the towns, owners and miracles are made; no computer god casts them |
| His Egyptian town has fire, teleport, shield and physical shield, and believes about 0.73 in him | partial 40% | as above; re-measured 2026-10-21: town, owner and belief are made; no god casts its miracles |
| He is not allied to the player | done | matches: no alliance exists in our tree |
| His mind ignores aggressive creatures and doesn't try to expand at all | partial 30% | both weights are set (`SET_COMPUTER_PLAYER_PERSONALITY`); the mind's turn that reads them is not wired; see [ai.md](ai.md); re-measured 2026-10-21: the score rules are ported and pinned to the oracle; the mind's turn that would use them is not wired |
| His wolf is loaded again on this land at 1.5 times size, opposite in alignment to the player's creature | pending | `LOAD_CREATURE` is real |

## The captive creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the three prison statues are switched off, his creature stays by the prisoner and never tires | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../story/land_3.md](../story/land_3.md); [../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#the-prison) |
| If cattle are within 50 metres it stamps on one and eats it | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Within 75 metres of the player's creature it torments it in turn: a powered fireball, a powered lightning bolt, then an itchy spell, each at full energy, 3 seconds apart | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Further away it walks back to its post | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A frozen moan is heard from the prisoner every tenth round | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| When freed, it plays confused, frightened and angry four times over and both creatures can be leashed | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## His raids on the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five minutes in, unless freed already, a pack of 20 wolves gathers and walks at half speed towards an Indian town | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../animal/](../animal/); [../story/gold_scrolls/the_wolves_are_possessed.md](../story/gold_scrolls/the_wolves_are_possessed.md) |
| If a wolf reaches within 25 metres of the town, it loses faith: belief in Lethys rises by 1 and in the player is set to 0.3 | pending | `OBJECT_RELATIVE_BELIEF` and `SET_PLAYER_BELIEF` are real; not checked in play |
| If the monk's quest is done, the monk appears and turns wolves into cows until 8 are left; the player must kill the rest before any reaches the town | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; [../story/gold_scrolls/the_wolves_are_possessed.md](../story/gold_scrolls/the_wolves_are_possessed.md#the-monk-helps) |
| His hand appears near a town, he boasts and throws three level-one fireballs at a beach campfire, setting sixteen fishermen alight | partial 50% | dormant: the fireballs can be cast (`SPELL_AT_POS`); the hand's natives are real now, and the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; [../story/gold_scrolls/fire_fire_im_on_fire.md](../story/gold_scrolls/fire_fire_im_on_fire.md); re-measured 2026-10-21: the natives it needs work; the story has not reached it in a run yet |
| The player is scored by how many burning fishermen they save: all 16 gives alignment +1, 6 to 15 nothing, 5 or fewer −0.8 | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; [../story/gold_scrolls/fire_fire_im_on_fire.md](../story/gold_scrolls/fire_fire_im_on_fire.md); see [../story/land_3.md](../story/land_3.md) |

## Losing the third land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the creature is freed, the player's belief in his Tibetan town is pushed back by 1 every 10 seconds | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; `OBJECT_RELATIVE_BELIEF` is real |
| After that, the town can be won once his belief there leads the player's by less than 0.4 | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; `BELIEF_FOR_PLAYER` is real (towns); not checked in play |
| When the player wins it, his hand flies over his temple at speed 300 and the temple explodes, to Nemesis's music | pending | needs his hand: the computer player's hand natives are real in `src/CHLApi.cpp`; not checked in play; [../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| A level-one explosion falls on his temple from 150 metres above | partial 50% | dormant: the explosion can be cast by script (`SPELL_AT_POS`), but the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the natives it needs work; the story has not reached it in a run yet |
| He is switched off for good | pending | `ENABLE_DISABLE_COMPUTER_PLAYER` is real |
