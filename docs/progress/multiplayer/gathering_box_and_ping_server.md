# The gathering box and the ping server

While a game world runs, the game keeps in touch with Lionhead's "ping server" (`bwping.bwgame.com`, UDP port 2611):
it says it is playing, asks which of its friends are online, and takes messages for its gathering box, the in-game box
of players, friends and others. Lionhead's staff used it to speak to players under ids the game draws in yellow.

**Codebase: bwgame-service** (`C:\projects\bwgame-service`), the stand-in for the game's online servers, not openblack.
Every "done" below is done there; paths in the notes are relative to that repository. The full protocol is in its
`docs/research/bwping.md`. What the gathering box itself does in the game is openblack's, listed as n/a here.

**Progress: 0/0 done, 0 partial — 0%**

## What the game sends and the server answers

Checked against the original game's code.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 20 seconds of play the game pings the server with its player's id; it expects no answer | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `bwgame/bwping.py`, `tests/test_bwping.py`); our tree has no online code |
| It sends the ids of its player's friends (at most 25); the server answers with those who are online and where | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`; there: `BW_PING_FRIENDS`); our tree has no online code |
| A friend listed online keeps the online mark for 7.5 minutes | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Chat between players in the gathering box goes to the address the game holds for that player | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The host of an internet game reports its teams and the map's conditions every 20 seconds and when the game is decided | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The game takes these messages from anyone, in single packets with no handshake | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |

## Staff in the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players with ids 9999999 to 10000014 are drawn in yellow in the gathering box | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A message from id 9999999, the admin, opens every player's gathering box | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A staff member's chat line shows as "name: text" in the gathering box, and in the in-game chat and its log | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| A message starting with a secret word opens a Yes/No question, or a box to type a line into; the answer goes back to the sender | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| What a player types to a staff entry in the gathering box reaches the server | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Lionhead's announcements: a text shown as up to 15 yellow names in the "Others" list | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| Who is playing right now | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`) |
| The gathering box's lists, friends list and chat window themselves | n/a | Online service, not part of this codebase: it lives in bwgame-service (`C:\projects\bwgame-service`); no gathering box, friends list or chat window in our tree |
