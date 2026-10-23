# How a computer god wins and loses towns

A computer god spreads its influence by winning towns the way a person does: impressing them, meeting their needs and
attacking its rivals' towns; it looks after its own towns and worshippers so they keep believing and praying. The story
lands add their own rules on top, holding some towns back until the right moment.

**Progress: 9/42 done, 2 partial — 24%**

See [../town/belief_and_conversion.md](../town/belief_and_conversion.md) for belief and conversion,
[../worship/influence.md](../worship/influence.md) for influence and [ai.md](ai.md) for how the god decides.

## What it starts with

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts give each god its towns, by owner name | done | `CREATE_TOWN` (`FeatureScriptCommands::CreateTown`, `TownArchetype`, the town's owner) |
| Land scripts give each god its temple | done | `CREATE_CITADEL` (`CitadelArchetype`) |
| Land scripts give each god worship sites by its temple, one per tribe | done | `CREATE_WORSHIP_SITE` (`magic::script::CreateWorshipSite`) |
| Land scripts set how much each town believes in each god, and the most it can | done | `SET_TOWN_BELIEF` and `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetBeliefInPlayer`, `SetCap`), read by the belief rules |
| A god's influence covers its temple and its towns | done | `src/ECS/Influence` |
| Its influence ring is drawn in its colour | done | `influence::k_CircleColours` (`src/ECS/Influence/InfluenceCircles.cpp`), drawn by `src/Graphics/RendererInfluence.cpp` |

## Choosing a town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It finds the town it most wants to win, from every player's towns | pending | no computer god mind in our tree |
| It judges how much it wants to win or destroy each town | pending | no computer god mind in our tree |
| It finds which town of its own most needs defending, food, wood or people | pending | no computer god mind in our tree |
| It notices which of its towns has just been attacked | pending | no computer god mind in our tree; a town remembers its last attacker (`Town::aggressor`), and `GET_TIME_SINCE_OBJECT_ATTACKED` reads how long ago (`ecs::town_aggression::SecondsSinceAttacked`) |
| It finds another town to take food or wood from | pending | no computer god mind in our tree |

## Winning towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| By sending its creature to impress the town | pending | no computer god mind in our tree; see [creatures.md](creatures.md) |
| By throwing things near the town to impress it | pending | no computer god mind in our tree |
| By casting impressive miracles at it | pending | no computer god mind in our tree; see [magic.md](magic.md) |
| By meeting its food needs: food miracles, fields, fish farms or food carried from another town to its storehouse | pending | no computer god mind in our tree |
| By meeting its wood needs (never chosen when expanding, see [ai.md](ai.md)) | pending | no computer god mind in our tree |
| Belief it wins shows over the town in its colour, as the person's does | partial 60% | the belief symbols over each town centre are drawn for every player in its colour (`particles::town_belief`, `src/Particles/TownBelief.cpp`), but no god wins belief; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md); re-measured 2026-10-21: drawn for every player in its colour; no god wins belief |
| A town changes hands when its belief in the god passes what it needs | done | the same rule as for the person: `town_belief::Fold` and `town_belief::TakeOverTown` (`src/ECS/Town/TownBelief.cpp`) |

## Destroying and defending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It destroys a rival's town with miracles, with its creature, or by throwing things | pending | no computer god mind in our tree |
| It defends its own town with its creature or with miracles | pending | no computer god mind in our tree |
| It defends a town whose belief is under attack, with a miracle or by impressing it with its creature | pending | no computer god mind in our tree |
| It puts out its burning buildings with water | pending | no computer god mind in our tree |
| It sends its creature to help repair damaged buildings | pending | no computer god mind in our tree |
| It shields buildings left unprotected | pending | no computer god mind in our tree |

## Looking after its towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It carries food and wood to the town that wants them most, or casts for them | pending | no computer god mind in our tree |
| It sends workers and wood to building sites | pending | no computer god mind in our tree |
| It places scaffolds from its workshop and combines small ones into bigger ones | pending | no computer god mind in our tree |
| It fills its workshop with wood | pending | no computer god mind in our tree |
| It makes breeder disciples to grow its towns | pending | no computer god mind in our tree |
| It makes disciples to meet a town's needs | pending | no computer god mind in our tree |
| It raises the town's totem (unconfirmed when) | pending | no computer god mind in our tree |

## Its worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It takes villagers from its towns to the worship site when the site needs them | pending | no computer god mind in our tree; see [../worship/worship_sites.md](../worship/worship_sites.md) |
| It feeds hungry worshippers: food on the site, or carried from fields, fish farms or other towns | pending | no computer god mind in our tree |
| It rests tired worshippers by healing them at the site or taking them home | pending | no computer god mind in our tree |

## Story rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the second land, a script checks each of eleven towns for a change of owner every tenth of a second and makes the gods react | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [khazar.md](khazar.md) and [lethys.md](lethys.md) |
| On the second land, both gods' towns can hold at most 0.75 belief in either god | done | Land 2's land script caps them (`SET_TOWN_BELIEF_CAP`, `ecs::town_belief::SetCap`) when the land is loaded |
| On the third land, the player can't win Lethys's last town until their creature is free, then only once his lead is under 0.4 | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [lethys.md](lethys.md) |
| On the fifth land, Nemesis's home town can't be won while he holds any other town | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [nemesis.md](nemesis.md) |
| On the fifth land, Nemesis's home town also counts as won if it shrinks to six or fewer (unconfirmed whether this script runs) | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Scripts count each player's towns to decide when gods die and vortexes open | done | `GET_PLAYER_TOWN_TOTAL` (`src/CHLApi.cpp`): the size of `map_cells::TownsOf(player)` |
| Scripts compare each player's influence at the camera to make gods react to trespassing | partial 50% | dormant: `GET_INFLUENCE` works (`magic::script::GetInfluence`), but play has not yet reached these lands (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); re-measured 2026-10-21: the natives it needs work; the story has not reached it in a run yet |
| The person gets "virtual influence" on the third and fifth lands | pending | `SET_VIRTUAL_INFLUENCE` is real; see [../worship/influence.md](../worship/influence.md) |
