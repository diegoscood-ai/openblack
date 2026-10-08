# Clans

Clans of players who play internet games together: a team can play for a clan, which then scores instead of its
players, brings its own creature from game to game, and shows its logo as the team's symbol. The game asks the online
servers for all of it; the clans themselves were run on the game's web site.

**Codebase: bwgame-service** (`C:\projects\bwgame-service`), the stand-in for the game's online servers and web site,
not openblack. Every "done" below is done there; paths in the notes are relative to that repository. What the game
itself does with clans in a match is in [multiplayer_rules.md](multiplayer_rules.md) (openblack).

**Progress: 0/0 done, 0 partial — 0%**

## What the game asks the servers for

Checked against the original game's requests and how it reads the replies.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game lists the clans a player belongs to | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `online/services.py`, `tests/test_clans.py::test_game_clan_list_puts_the_first_joined_clan_first`); our tree has no online code |
| Each clan in that list carries a yes/no flag | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A player's details show their clan | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The clan details: name, logo, and the lines "Clan Description:", "Clan Leader:", "Clan URL:" and "Credits" | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The clan logo is a picture file the game loads, keeping a 64 by 64 shape in 16 shades | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `online/logo.py`, `docs/research/clan-logo.md`); our tree has no online code |
| A team picking a clan gets the clan's creature | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A clan with no creature yet tells the game to create one of the chosen type and name | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `tests/test_clans.py::test_clan_creature_numbers_are_the_games_table_rows`); our tree has no online code |
| At the end of a clan game, the team leader's game uploads the clan creature | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| At the end of an internet game, the host reports the results and gets everyone's new points | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `online/services.py`); our tree has no online code |
| A team playing for a clan scores for the clan instead of its players | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Clan names and texts reach the game in its character set | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The clan logo becomes the symbol of a team playing for the clan, over its towns and on its hand | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`); no clans in our tree; the symbol over towns is drawn from the `ChooseSymbol` sheet (`src/Particles/TownBelief.cpp`), never a clan logo |
| The clan logo shows in the multiplayer screens' clan box | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`); no multiplayer screens in our tree |

## Protection of clan scoring

Not in the original as far as is known; the server's own rules, so that results can't be made up.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Results count only when sent with the password of an account that played in that game | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `tests/test_abuse.py::test_results_need_a_valid_password`, `test_results_only_from_a_player_of_that_game`); our tree has no online code |
| A team scores for a clan only when all its players are members of it | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `tests/test_abuse.py::test_a_team_scores_for_a_clan_only_when_all_are_members`); our tree has no online code |
| Oversized result reports are refused | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |

## Running a clan on the web site

The original site's clan pages are lost; how they worked is unknown. This is bwgame-service's own design, built so the
game's side above works. Tests in `tests/test_clans.py`.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A signed-in player founds a clan and leads it | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `online/clans.py`, `web/views.py`); our tree has no online code |
| A clan has one leader, officers and members | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `online/models.py`); our tree has no online code |
| Each clan chooses who can join: anyone, by request, or by invitation only | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Players ask to join; the leader or an officer approves or declines; the player can withdraw | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader or an officer invites a player by name; the player accepts or declines; the invitation can be withdrawn | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| An invitation lets a player in whatever the clan's joining rule | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader or an officer removes members; officers can't remove officers or the leader | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Removed players can be banned from rejoining, asking or being invited, and the ban lifted | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader makes members officers and back | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader hands the leadership to a member, becoming an officer | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A leader who leaves is followed by the longest-serving officer, else the longest-serving member | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A clan whose last member leaves is disbanded | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader disbands the clan, typing its name to confirm | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader edits the name, motto, web site, joining rule, and the creature's type and name | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader uploads a logo, shown on the site as the game will draw it | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader removes the logo | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The leader resets the clan creature, so the next clan game starts a new one | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A player belongs to at most a few clans, and a clan has at most a set number of members | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Founding clans, joining requests and clan changes are rate-limited | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `accounts/throttle.py`); our tree has no online code |
| Clan names, mottos, web sites and creature names are checked for blocked words, reserved names and hidden characters | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `moderation/`); our tree has no online code |
| The clan list shows each clan's logo, leader, members, joining rule and points | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The ranking lists clans by points | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Clan tags shown beside players' names | n/a | the game has no clan tags; a player could only type one into their own name |

## Moderation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every new logo is put on the admins' review list (pictures aren't checked automatically) | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Admins see and change a clan's members, requests, invitations and bans | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Admins remove a clan's logo, or clear its motto, web site and creature name | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Clan text that matches a blocked word added later is hidden on the pages | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
