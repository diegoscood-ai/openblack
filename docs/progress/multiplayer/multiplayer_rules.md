# Multiplayer rules

The rules of a multiplayer game: teams and clans, the winning conditions and time limit of patch 1.2, and the points of
the online ranking.

**Progress: 0/21 done, 0 partial — 0%**

## Teams and clans

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players form teams in the front end | todo | no network code or multiplayer front end in our tree |
| A team's leader can play for a clan, whose creature it then uses (patch 1.2) | n/a | clan creatures came from the original's servers, which are gone; our tree has no online code (the server side is only in bwgame-service, see [clans.md](clans.md)) |
| Clan details and lists | n/a | the original's servers are gone; our tree has no online code (bwgame-service only, see [clans.md](clans.md)) |

## Winning conditions (patch 1.2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers killed | todo | no network play in our tree, so no network game sets up winning conditions |
| Villagers eaten by the creature | todo | no network play in our tree, so no network game sets up winning conditions |
| Villagers born in the player's towns | todo | no network play in our tree, so no network game sets up winning conditions |
| Houses built | todo | no network play in our tree, so no network game sets up winning conditions |
| Wonders built | todo | no network play in our tree, so no network game sets up winning conditions |
| Food in the town stores | todo | no network play in our tree, so no network game sets up winning conditions |
| Wood in the town stores | todo | no network play in our tree, so no network game sets up winning conditions |
| Towns taken over | todo | no network play in our tree, so no network game sets up winning conditions |
| Prayer power generated | todo | no network play in our tree, so no network game sets up winning conditions |
| Buildings smashed by the player | todo | no network play in our tree, so no network game sets up winning conditions |
| Buildings smashed by the creature | todo | no network play in our tree, so no network game sets up winning conditions |
| Belief in the player across the world | todo | no network play in our tree, so no network game sets up winning conditions |
| New towns built (a town centre, a store and at least one lived-in house) | todo | no network play in our tree, so no network game sets up winning conditions |
| Villagers converted | todo | no network play in our tree, so no network game sets up winning conditions |
| Trees grown | todo | no network play in our tree, so no network game sets up winning conditions |
| Each condition has a value, and once reached it stays done | todo | no winning conditions in our tree; the per-map defaults would come from the online map files and the condition template, which our tree does not read |
| Conditions are mixed and matched; the first to complete all wins | todo | no network play in our tree, so no network game sets up winning conditions |
| Others are ranked by the share of conditions done; ties go to the most influence | todo | no network play in our tree, so no network game sets up winning conditions |
| A time limit from 10 minutes to 24 hours; at its end the highest share wins | todo | no time limit in our tree |

## Points

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Internet games earn points for the online ranking, more for beating a better player | n/a | the original's ranking servers are gone |
| Creatures from the original's games online keep what they learn | todo | no online games in our tree; see [online_services.md](online_services.md) |
