# How a computer god wins and loses towns

A computer god spreads its influence by winning towns the way a person does: impressing them, meeting their needs and
attacking its rivals' towns; it looks after its own towns and worshippers so they keep believing and praying. The story
lands add their own rules on top, holding some towns back until the right moment.

**Progress: 8/42 done, 2 partial — 21%**

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
| It finds the town it most wants to win, from every player's towns | todo | no computer god mind in our tree |
| It judges how much it wants to win or destroy each town | todo | no computer god mind in our tree |
| It finds which town of its own most needs defending, food, wood or people | todo | no computer god mind in our tree |
| It notices which of its towns has just been attacked | todo | no computer god mind in our tree; a town remembers its last attacker (`Town::aggressor`), and `GET_TIME_SINCE_OBJECT_ATTACKED` is a stub |
| It finds another town to take food or wood from | todo | no computer god mind in our tree |

## Winning towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| By sending its creature to impress the town | todo | no computer god mind in our tree; see [creatures.md](creatures.md) |
| By throwing things near the town to impress it | todo | no computer god mind in our tree |
| By casting impressive miracles at it | todo | no computer god mind in our tree; see [magic.md](magic.md) |
| By meeting its food needs: food miracles, fields, fish farms or food carried from another town to its storehouse | todo | no computer god mind in our tree |
| By meeting its wood needs (never chosen when expanding, see [ai.md](ai.md)) | todo | no computer god mind in our tree |
| Belief it wins shows over the town in its colour, as the person's does | partial | the belief symbols over each town centre are drawn for every player in its colour (`psys::town_belief`, `src/Particles/TownBelief.cpp`), but no god wins belief; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| A town changes hands when its belief in the god passes what it needs | done | the same rule as for the person: `town_belief::Fold` and `town_belief::TakeOverTown` (`src/ECS/Town/TownBelief.cpp`) |

## Destroying and defending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It destroys a rival's town with miracles, with its creature, or by throwing things | todo | no computer god mind in our tree |
| It defends its own town with its creature or with miracles | todo | no computer god mind in our tree |
| It defends a town whose belief is under attack, with a miracle or by impressing it with its creature | todo | no computer god mind in our tree |
| It puts out its burning buildings with water | todo | no computer god mind in our tree |
| It sends its creature to help repair damaged buildings | todo | no computer god mind in our tree |
| It shields buildings left unprotected | todo | no computer god mind in our tree |

## Looking after its towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It carries food and wood to the town that wants them most, or casts for them | todo | no computer god mind in our tree |
| It sends workers and wood to building sites | todo | no computer god mind in our tree |
| It places scaffolds from its workshop and combines small ones into bigger ones | todo | no computer god mind in our tree |
| It fills its workshop with wood | todo | no computer god mind in our tree |
| It makes breeder disciples to grow its towns | todo | no computer god mind in our tree |
| It makes disciples to meet a town's needs | todo | no computer god mind in our tree |
| It raises the town's totem (unconfirmed when) | todo | no computer god mind in our tree |

## Its worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It takes villagers from its towns to the worship site when the site needs them | todo | no computer god mind in our tree; see [../worship/worship_sites.md](../worship/worship_sites.md) |
| It feeds hungry worshippers: food on the site, or carried from fields, fish farms or other towns | todo | no computer god mind in our tree |
| It rests tired worshippers by healing them at the site or taking them home | todo | no computer god mind in our tree |

## Story rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the second land, a script checks each of eleven towns for a change of owner every tenth of a second and makes the gods react | todo | the story never reaches the land (`LOAD_MAP` is empty); see [khazar.md](khazar.md) and [lethys.md](lethys.md) |
| On the second land, both gods' towns can hold at most 0.75 belief in either god | done | Land 2's land script caps them (`SET_TOWN_BELIEF_CAP`, `ecs::town_belief::SetCap`) when the land is loaded |
| On the third land, the player can't win Lethys's last town until their creature is free, then only once his lead is under 0.4 | todo | the story never reaches the land (`LOAD_MAP` is empty); see [lethys.md](lethys.md) |
| On the fifth land, Nemesis's home town can't be won while he holds any other town | todo | the story never reaches the land (`LOAD_MAP` is empty); see [nemesis.md](nemesis.md) |
| On the fifth land, Nemesis's home town also counts as won if it shrinks to six or fewer (unconfirmed whether this script runs) | todo | the story never reaches the land (`LOAD_MAP` is empty) |
| Scripts count each player's towns to decide when gods die and vortexes open | todo | `GET_PLAYER_TOWN_TOTAL` is a stub |
| Scripts compare each player's influence at the camera to make gods react to trespassing | partial | dormant: `GET_INFLUENCE` works (`magic::script::GetInfluence`), but the story never reaches the land (`LOAD_MAP` is empty) |
| The person gets "virtual influence" on the third and fifth lands | todo | `SET_VIRTUAL_INFLUENCE` is a stub; see [../worship/influence.md](../worship/influence.md) |
