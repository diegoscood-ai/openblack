# The computer players' minds

## Summary

Every player of the original has a computer player, and every computer player has a mind: a fixed tree of 198 nodes
(126 distinct rows) from six broad aims down to single hand actions. Each node has a weight (its personality), a
suppression that drops when its action runs and comes back with the turns, and usually a score function that looks at
the world. The mind weighs the tree and runs the action of the node it chose. The land scripts tune it by node name
(`SET_COMPUTER_PLAYER_PERSONALITY`, `SET_COMPUTER_PLAYER_SUPPRESSION`), set how it feels about each player
(`SET_COMPUTER_PLAYER_ATTITUDE`), and force or queue named nodes on its hand (see
[the computer players' hands](computer-hands.md#forced-and-queued-actions)).

openblack keeps the tree as a code table (`ECS/ComputerPlayerMindTree.h`, generated once from the exe) and each
player's copy of it as a `ComputerMind` component on the player's entity (`computer_player::MakeMind`), set by the
scripts and kept from land to land as the original keeps it.

## The original

### Where the tree comes from

- ✅ **From the exe, not from a file.** The tree is a fixed set of static info rows (`CPDesireNodeInfo`, 0x78 bytes
  each), filled by the CRT initialisers 0x655A90 and 0x65DBD0..0x663B30. `GComputerPlayer::Init` (0x656E60) builds the
  runtime tree from them (`CPDecisionPlayer`, 0x656880) **only when the computer player has none yet** (+0x2C is 0):
  the root on the row 0xD47F68 and its six aims (0x656580, the rows 0xD4BA30 + n × 0x78), then each node's children by
  index into the next level's table (0xD4B148, 0xD497F8, 0xD48088). The rows of the fourth level have no children.
- ✅ The `.cps` files of the game data are textures (the gods' symbols); no mind data is read from any file in the
  campaign. The one file the mind can read is a personality file (0x6642B0 → 0x6641D0: for each node in pre-order the
  row's name, `strlen + 1` bytes read over the row's own name, then a float into the node's personality), from native
  380 `LOAD_COMPUTER_PLAYER_PERSONALITY` (0x6EF090) and the land-script keyword `LOAD_COMPUTER_PLAYER_PERSONALLTY`
  (GSetup 0x717A3A); 381 writes it. No script of challenge.chl and no Land1-5.txt line uses them
  ([The personality file](#the-personality-file)).
- ✅ **Info row** (0x78 bytes):

  | Offset | Field |
  |---|---|
  | +8 | the name ("Name: n ", with the space) |
  | +0x18 | the score function (a member-function pointer of `GComputerPlayer`, 16 bytes; called by 0x655A50; tested by 0x6560D0) |
  | +0x28 | a third member function, one of 18 per branch (e.g. 0x65D550: the first other active player into the node's thing, score 1.0) |
  | +0x38 | the action function (tested by 0x656120, run by 0x656170) |
  | +0x48 | the starting personality |
  | +0x4C | the suppression set when the action succeeds |
  | +0x50 | the suppression's recovery rate (1.001, or 1.0 on the root, DIRECTPickupAndDropObject and three worship rows) |
  | +0x54 | the period in turns (read only on the six aims) |
  | +0x58 | up to eight children by index into the next level's table, −1 ends the list (`GetNumChildren` 0x655B70) |

- ✅ **Runtime node** (`CPDesireNode`, 0xAC bytes, ctor 0x655B90): +0 suppression (1.0), +4 personality (the row's
  +0x48), +8 the row, +0xC the parent, +0x10 the turn it was last weighed, +0x14 the turn it last ran, +0x1C a
  `PlayerSubActionArgument` (its thing at +0x34, its point at +0x84), +0x94 the thing it chose, +0x9C / +0xA0 its
  children and their count, +0xA4 its score, +0xA8 the node it chose. The root (`CPDecisionPlayer`, 0xB8 bytes) adds
  +0xB4, the score of the plan under way.
- ✅ **Counts**: 126 distinct rows (1 root, 6 aims, then 19, 52 and 48 by the level where each is first met), 117
  distinct names up to their ':', 198 runtime nodes (rows are shared under several parents, and AttendTownDesires
  names GiveBuildingSiteWood twice); 102 rows have a score function (86 distinct), 67 an action (58 distinct).
- ✅ **The name lookup** (`CPDesireNode::FindDesireNode` 0x6640A0, through `CPDecisionPlayer::FindDesireNode`
  0x664080 with a count n): from the root, each node before its children, a node matches when its name up to its last
  ':' (`strrchr`; the whole name when it has none) begins the name asked for (`strncmp` over that length); the n-th
  match is returned. So "TakeOverTownByImpressing" also names the three TakeOverTown nodes.
- ✅ The third member function (+0x28) is never called in play: its only reader is 0x656220 (it calls the function
  and returns the result cut to a whole number), and 0x656220 has no caller, no jump and no pointer to it anywhere in
  the exe (xref `callers`, and a search of every section for its address). So openblack's rows leave it out.
- ✅ The nodes start zeroed: the tree's nodes are allocated with `calloc` (0x7DB790, from 0x656580 and `Init`), and the
  ctor (0x655B90) sets only the fields above and the argument's kind (5) and two of its fields (0x651240). So a node's
  argument point (+0x84) is (0, 0, 0) until something writes it.

### Weighing a node

✅ Read from the exe (0x655DF0, 0x655EB0, 0x655F60) and pinned by the oracle (xref #628, #629).

1. **Recovery** (0x655F60). If the node's suppression `s` is not 1.0, it becomes
   `((rate − 1) × (turn − last weighed) + 1) × s`, kept only when below 1.0 (else 1.0). The turns are the unsigned
   difference, made a float. Oracle: s 0.01 weighed at turn 1000, now 1234 → `0x3C4A2E2B`; s 0.5 after 1000 turns → 1.0.
2. The node's chosen node (+0xA8), thing (+0x94) and score (+0xA4) are cleared, and it keeps the game turn (+0x10).
3. **With a score function** (0x6560D0): score = (f(node) + 0) × personality × suppression, and the node chooses
   itself whatever the score (0x655E42). A score of 0 (or not a number) ends it there: its children are not weighed
   (the other test, cp+0x1C4, is only ever written 0: the ctor 0x656DBD and `Init` 0x656EE1). Otherwise its children
   are weighed and its score becomes `best × score` (0x655F2D).
4. **Without one**, it takes the thing of the nearest node above it that has one (0x655CB0, from the parent up; none
   for the root), its children are weighed, and its score is the best.
5. **The children** (0x655EB0), in list order, best = 0. A child is weighed only while the parent's personality ×
   suppression is at least best (the gate: best only grows, so once it shuts every later child is skipped). The child's
   result × the parent's personality × suppression replaces best only when **strictly** greater, and then the parent's
   chosen node is the child's. A node without children keeps the score of step 3 or 4.
6. Every product rounds to a float: the game runs the FPU at 24 bits.

The oracle (`weigh_oracle.py`, real exe code with the score functions stubbed) on a made-up tree: a root of
personality 0.25 without a score function over A, B and C (0.3, 0.6 and 0.9):

| Case | Root | Chosen | Notes |
|---|---|---|---|
| every score 0.5 | `0x3DE66666` | C | A `0x3E19999A`, B `0x3E99999A`, C `0x3EE66666` |
| every score 5.0 | 0.375 | A | the gate shuts after A (0.375 > 0.25): B and C are never weighed, their turns stay 0 |
| every score 0 | 0 | none | each leaf chose itself |
| B over D (0.5) and E (0.8), scores 0.5 | `0x3DE66666` | C | B `0x3D9374BD` (0.3 × 0.24), chosen E |

### Fields the turn tests that nothing sets

- ✅ The action state's +0x88 is always 0, so the busy turn's outbid test is always made: its writers are the ctor's
  0x64FFD0 (0x65004C), 0x650170 (0x650176) and `NewProcess` (0x6575AA), all 0; `SetToZero`, `Save` and `Load` of the
  action state do not touch it. Of the 204 writes of a non-zero value to a +0x88 field anywhere in the exe, none is in
  a function that reaches a computer player or its action state (each named, or checked for +0x944 and the action
  state's functions). openblack keeps no such field.
- ✅ cp+0x1C4 is written only 0 (the ctor 0x656DBD, `Init` 0x656EE1) and cp+0x1CC is written only by 0x656930 and read
  by nothing (every `+0x1C4` / `+0x1CC` operand in 0x64E000..0x700000 listed): openblack keeps neither.

### Running a chosen node

- 0x6563E0 runs the node's chosen node, nothing when it chose none. 0x656080 keeps the turn in the node (+0x14); a node
  with an action runs it (0x656170), one without runs its parent, up to the root (which has none: nothing).
- 0x656170: the hand gives up its steps first (0x6587C0 → 0x650250); a done action (non-zero) sets the suppression to
  the row's +0x4C and starts the action state's time estimate (0x6507D0: the turn into +0x3C, and into +0x40 the sum of
  each queued step's own estimate); a failed one sets it to 0.01 (`0x3C23D70A`).
- ✅ The time estimate is read by nothing in play: +0x3C / +0x40 of the action state are read only by its `Save` and
  `Load` and by 0x650850 (the share of the estimate gone by), which no call, jump or pointer anywhere in the exe
  reaches. openblack's `MindWorld::TimeSteps` is where it would go; with no saved games, nothing is kept.
- 0x656950 runs the root's chosen node; done, the plan's score (root +0xB4) becomes the root's score.

### A turn of the mind

`GComputerPlayer::NewProcess` (0x657420), in each player's turn, in its mode 1 (`Init` sets +0x30 to 1; mode 0 is
never set):

1. The upkeep (the hand's place, the lists, the queue and the steps' things) whatever the mind's state; then the gate:
   on (+0x1B8), not paused (+0x1BC) and not held by the wide screen (0x658B70). Its start on the land once (+0x64), and
   0x657D90, 0x6589F0, 0x6573D0 (see [the computer players' hands](computer-hands.md)).
2. **Busy or held by a script** (`IsBusy` 0x650840, +0x1C0): a turn of the steps (0x6573E0), and +0x1C8 = 1. Held, the
   turn ends. Otherwise the due aims are weighed (0x656980: an aim is due when its period is 0 or the turn is a
   multiple of it, 0x656040; each due aim is weighed on its own, and one strictly better than the best so far gives the
   root its score and chosen node). If the action state's +0x88 is 0 and `best − plan > plan` (plan = root +0xB4), the
   plan runs (0x656950).
3. **Free and not held:** the next queued action is forced (`ForceNext` 0x664480); if that gave the hand something,
   the turn ends. Otherwise, in the same turn:
   - if +0x1B4 is set and fewer than 30 turns have gone since it (unsigned), +0x1C8 = 0 and the turn ends;
   - else if +0x1C8 is set, it is cleared and the turn ends (`Init` sets it, so a new mind rests its first free turn);
   - else the whole tree is weighed (0x656930, which also sets cp+0x1CC, read by nothing); a score above 0 clears the
     action state's +0x88, runs the plan (0x656950) and clears +0x1B4; any other sets +0x1B4 to the turn.

### What the mind sees round its hand

The mind keeps a list of what it has seen round its hand (cp +0x40, an `LHLinkedList<GComputerSeen*>`: the head at
+0x40, the count at +0x44). An entry (0x1C bytes, vtable 0x8E40D8, constructor 0x6569F0) holds the thing (+0x14) and
the turn it was first seen (+0x18), which nothing refreshes. Only the look round the hand and `GetWoodFromForest` read it
(0x656C60 reads it too, but nothing calls it).

- **The look round the hand** (0x657D90, from the mind's turn at 0x6574AA, after the gate and its start on the land,
  before the weighing; so only while the mind is on):
  1. while more than cp +0x60 (50, from the constructor 0x656D40; `GPlayer::Init` passes 90, 30 and 50) are kept
     (unsigned) and the turn is a multiple of 500, every entry with `turn − seen > 8000` (unsigned, strict: a later
     seen turn wraps round) goes (0x656AA0);
  2. from the computer hand's place (`GetHandPos`), a spiral of whole cells (dir 1, count 1, `MapXZDirections`, the
     hand's fraction kept), at most 99999 cells, redone from the hand each turn: a cell off the map is passed over with
     no distance test; for a cell on it the look ends unless `10 + cp +0x5C (30) > dist(hand, cell point)` (the sum a
     float; a tie or NaN ends it); else each thing of the cell, as `FindType(ANY)` walks it (the fixed list from its
     head, then the mobile list), is asked `GetComputerSeen` 0x656B30: an available thing whose vt +0x44 gives a tree's
     forest (else the tree itself, `Tree` and `MagicTree`) or a seed (`OneOffSpellSeed`; nothing for anything else),
     itself available and in no entry yet, goes in a new entry at the head with the turn. From the middle of a cell
     that is the 30 cells up to the corner three down and left (42.4 m): (0,0) (−1,0) (−1,−1) (0,−1) (1,−1) (1,0) (1,1)
     (0,1) (−1,1) (−2,1) (−2,0) (−2,−1) (−2,−2) (−1,−2) (0,−2) (1,−2) (2,−2) (2,−1) (2,0) (2,1) (2,2) (1,2) (0,2)
     (−1,2) (−2,2) (−3,2) (−3,1) (−3,0) (−3,−1) (−3,−2).
- **The sweep** (0x656A30, in the mind's turn before its gate, so also while it is off): an entry whose thing is no
  longer available (vt +0x2C) goes.
- A land's clear (`OnEndOfClearMap` → 0x6572C0) and `ToBeDeleted` empty it; Save and Load keep it (0x6576D4, 0x657A38).
- The oracle (S1-S8, P1-P3): the order, the corner, the map's edge, forests through
- An oracle run (S1-S8, P1-P3): the order, the corner, the map's edge, forests through
  their trees, the cell's order and the forgetting.

### The scripts set the mind

✅ Read from the exe:

| Native | Function | What it does |
|---|---|---|
| 364 `SET_COMPUTER_PLAYER_PERSONALITY` (player, name, value) | 0x6EED50 → 0x664000 | POPs the value, the name and the player (`ConvertScriptPlayerToGamePlayer`, `GGame::GetPlayer`, its computer player +0x944); every node `FindDesireNode` finds for the name (n = 0, 1, … until none) takes the value as its personality (+4) |
| 365 `SET_COMPUTER_PLAYER_SUPPRESSION` (player, name, value) | 0x6EEDE0 → 0x664040 | the same POPs; the value goes into each node's suppression (+0) |
| 378 `SET_COMPUTER_PLAYER_ATTITUDE` (player, toward, value) | 0x6EF190 → 0x664B20 | POPs the value, the player it is toward and the player; keeps `value + value` (`fadd st0, st0`) in cp+0x1D8[toward's number] +0x14; nothing when either player is missing |
| 379 `GET_COMPUTER_PLAYER_ATTITUDE` (player, toward) | 0x6EF240 → 0x664B00 | POPs toward, then the player; pushes the kept value × 0.5, or 0 when either is missing |

- The eight attitudes are `GComputerAttitudeToPlayer` objects the ctor (0x656D40) makes, each +0x14 = 1.0: a mind no
  script set reads 0.5.
- In the story: SetUpComputerPlayersAtStartOfLand2 sets "ExpandInfluence" 0.2 for Khazar and Lethys, their attitudes
  to each other 0.25 (kept 0.5) and player 1's own toward player 1 (−1); SetupLand3 sets Lethys's
  "ReactToAggressiveCreature" and "ExpandInfluence" to 0, FreezeRayStatue raises the first to 1; BeginLand5 sets
  Nemesis's attitude to the player to 2 (kept 4). No live script calls 365 or 379.

### The personality file

✅ Read from the exe (xref notes on each function):

| Native | Function | What it does |
|---|---|---|
| 380 `LOAD_COMPUTER_PLAYER_PERSONALITY` (player, file) | 0x6EF090 → 0x6642B0 → 0x6641D0 | POPs the file's name (`STRING`), then the player (`ConvertScriptPlayerToGamePlayer`, `GGame::GetPlayer`); nothing when the player, the name or its computer player (+0x944) is missing; else reads the file over the mind |
| 381 `SAVE_COMPUTER_PLAYER_PERSONALITY` (player, file) | 0x6EF110 → 0x664240 → 0x664160 | the same POPs and checks; writes the mind to the file |

- **Opening.** 0x6642B0 opens the file as a `GameOSFile` in mode 1 (`LHReleasedOSFile::Open` 0x7BC7E0 →
  `LHOSFile::Open` 0x7BC730): it must exist (`LHOSFile::Exists`), and is opened for reading and writing
  (`CreateFileA` 0xC0000000, share 3, `OPEN_ALWAYS`); when that fails the name is tried again on the game's CD drive
  (`"%c:\%s"`, `g_GameDriveCharacter`). Not opened, nothing is read and the mind is untouched. 0x664240 opens it in
  mode 0: made new or emptied (`CREATE_ALWAYS` when missing, `TRUNCATE_EXISTING` when there), not tried on the CD.
- **Reading** (0x6641D0, from the root at cp+0x2C): for each node, the node first and then its children in the
  order of its list (+0x9C, the order `FindDesireNode` searches): `Read(row name, strlen(row name) + 1)` then
  `Read(node + 4, 4)`. The info row's name (+8) is a pointer into `.data` (0xC00B30 for the root, all 126 rows' names
  in `.data`, writable), so **the bytes read go over the row's name itself**: no name is compared, a different
  name renames the row for every computer player (the rows are shared) until the game ends, since the CRT
  initialisers set them once and nothing writes them again. A later lookup by name (364, 365, FORCE, QUEUE) finds the
  row by its new name. A row met again later (a row under several parents) is read with the length of its name as
  it is then, so a nul read early shortens the next read.
- **A short or long file.** `LHOSFile::Read` (0x7BC8E0) is one `ReadFile`, and its result is not checked; at the end
  of the file a read takes what is left and writes only that much, the rest of the name or the float as it was.
  Bytes after the last node are not read. Every node of the tree is walked, all 198, root included.
- **Writing** (0x664160): the same walk, `Write(row name, strlen + 1)` then `Write(node + 4, 4)`; the results are
  not checked.
- ⚠️ Two cases openblack cannot follow: a file whose byte at a name's nul is not a nul makes the original's name run
  on into the bytes after it in `.data` (the next name's or padding), which openblack does not keep, so openblack's
  name ends at its own end; and a file the original cannot open for writing (marked read-only) it does not read,
  while openblack reads it.
- No script of challenge.chl calls 380 or 381 (the 98 disassembled scripts name only 364 among the personality
  natives, 25 times), so the campaign never reaches this.
- ✅ **The land-script keywords** (GSetup's command table rows 0xC214B0 and 0xC214C0, both spelt "PERSONALLTY"; their
  cases in GSetup's switch, `sub_7165CA`). A command's arguments are kept as text (+0x800 per argument), as integers
  (+0x6000) and as floats (+0x6030); a position's text is only made a position (`GSetup::GetScriptPos`) by the cases
  that ask for one, and these two do not:

  | Keyword | Types | Case | What it does |
  |---|---|---|---|
  | `LOAD_COMPUTER_PLAYER_PERSONALLTY` (player, file) | "NA" | 0x717A3A | `GGame::GetPlayer(` the first argument's integer `)`, its computer player (+0x944), and 0x6642B0 with the **second argument's text** as the file's name; no check: a number of 8 or more gives no player, and the original then reads through a null pointer |
  | `SET_COMPUTER_PLAYER_PERSONALLTY` (player, name, value) | "LAF" | 0x717A6B | `GPlayer::GetPlayerFromText(` the first argument's text `)` (any other name is the neutral player's, never none), its computer player, and 0x664000 (as 364) with the second argument's text as the node's name and the third argument's float as the weight |

- No line of Land1-5.txt, LandT.txt or any other `.txt` of the game data uses either keyword (grepped:
  "PERSONALLTY" is only in the exe and CreatureIsle.exe).

### A land change keeps the mind

- ✅ The players are `GGame`'s own array of eight `GPlayer` (+0x18); a land change (`GGame::ClearMap` 0x552BB0) calls
  each one's `OnEndOfClearMap` (0x64CD00 → 0x6588D0): 0x6572C0 clears Active (+0x1B8), its start on the land (+0x64),
  the lists and +0x1B4, then `Init` (0x656E60) sets +0x1C4, +0x1D4 to 0, +0x1C8 and +0x30 to 1 and +0x1D0 to 0xFF.
  **`Init` builds a tree only when the computer player has none (+0x2C is 0), and nothing on the way frees it.** The
  computer player itself is made once (`GPlayer::Init` 0x649190, only when +0x944 is 0) and freed only by
  `GPlayer::Uninit` (from `GGame::Close` and `GPlayer::ToBeDeleted`). So a player's tree, its personalities,
  suppressions, scores and plan, and its eight attitudes last the whole game, from land to land (xref #635).
- So the story's settings carry over by player slot: PLAYER_TWO (script player 2) is Khazar on Land 2, Lethys on
  Land 3 and Nemesis on Land 5. ✅ **Nemesis plays Land 5 with "ExpandInfluence" at 0 and "ReactToAggressiveCreature"
  at 1**, read statically:
  - every write of a personality in the campaign is a `SET_COMPUTER_PLAYER_PERSONALITY` in challenge.chl (no Land1-5.txt
    line and no script uses the land-script keyword or the personality file); in the mind's own code a node's +4 is
    written only by 0x664000, the ctor and the file reader 0x6641D0: the weighing, the runs and the forcing write
    other fields, and the 86 score functions write their node's fields, DefendTown its parent's thing and the
    spell helpers a count ([Scoring a node](#scoring-a-node));
  - for PLAYER_TWO's ExpandInfluence they are: Land 2's SetUpComputerPlayersAtStartOfLand2 (74862-74865: 0.2), Land 3's
    SetupLand3 (103152-103155: 0) and nothing after; the Land 5 sites are all in ControlNemesisBattleStrategy and its
    CurrentAction, which no RUN or START in challenge.chl names and which is not the auto-run script (dead code);
  - ReactToAggressiveCreature: SetupLand3 (103148-103151: 0), then FreezeRayStatue (108909-108912: 1) on Land 3's gold
    path, and nothing after;
  - Land 4 sets nothing, and its land change keeps the tree.
  So the values are settled for a game played from Land 3 on (FreezeRayStatue reached). A Land 5 loaded without Land 3
  having been played in the same game would keep the tree's own 0.75 and 1.

### Forcing an action through the nodes

✅ Read from the exe (`GComputerPlayer::ForceAction` 0x663BE0, 0x663DA0, 0x663EA0; xref #538, #571). FORCE (366) and
the queue's `ForceNext` (0x664480) both call it with a worth of 1.0:

1. The first node the name names (`FindDesireNode` n = 0); none: nothing.
2. A name beginning "DIRECT" (`_strnicmp`, 6 letters): the node's thing (+0x94) is the first thing and its argument's
   thing (+0x34) the second as a `GameThingWithPos` (none when it has no place); its action runs at once (0x656170: the hand gives up its steps,
   and the suppression is set after; the node's run turn is not kept). Done (1): the busy flag (+0x1F8) and true.
   Otherwise it goes on as below.
3. A second thing that is a `ScriptMarker` → 0x663EA0 with the marker's point (the marker itself is not kept): the
   node's argument point (+0x84) is the point, its thing the first thing, its score 1.0. With an action and children,
   the first child takes the point too and is the node's chosen node; with an action and no children, nothing more;
   without an action, its children are weighed (0x655EB0). Then the chosen node runs (0x6563E0).
4. Any other → 0x663DA0: the node's score is 1.0 and its thing the first thing. With an action and children: no second
   thing → its children are weighed; else the first child's thing is the second thing and the child chooses itself and
   is the node's choice. With an action and no children: its parent's thing is the first thing and its own the second.
   Without an action: its children are weighed, and a node that then has itself as its choice refuses (false). Then the
   chosen node runs.
5. Done: the busy flag and true; else false (`ForceNext` deletes the queue entry either way).

So forcing changes the node's suppression (the row's after success, 0.01 after failure) and its things, which the next
weighing sees. A leaf like ShallCastWoodSpell: 132 runs only when its chosen node is set, which only a weighing that
reached it does ([the computer players' hands](computer-hands.md#forced-and-queued-actions)).

### Scoring a node

✅ Read from the exe, function by function (notes kept outside the repository; xref
notes #637 on). Each score function is a `thiscall` member of the computer player (`ecx` = cp) that takes the node
✅ Read from the exe, function by function (xref notes #637 on). Each score function is a `thiscall` member of the computer player (`ecx` = cp) that takes the node
(`[esp+4]`), returns its score on the x87 stack and pops 4. What they share:

- **The thing above** (0x655CC0): most of them start from the thing of the nearest node above the node's parent (the
  node's own +0x94 was just cleared by the weighing), and test its class (`__RTDynamicCast` to Town, Creature, Abode,
  Object, GameThingWithPos, GPlayer...). A few start at the node itself (0x655CB0), which comes to the same.
- **What they write:** the node's thing (+0x94); some its argument's point (+0x84), integer (+0x78) or float (+0x80);
  DefendTown its parent's thing; and the spell helpers a count in the computer player (below). None writes the world,
  except where said.
- **The player's creature** (0x65BCB0): the player's (+0x28) creature (+0xA4C), none while its word +0x24 has 0x400
  (a script controls it). Some read +0xA4C itself, unfiltered (said where).
- **Comparisons** keep the x87 tests: `fcom` + `test ah,1` is "below" and `test ah,0x41` "below or equal", both true
  for a value that is not a number; `test ah,0x40` is "equal", also true for one. Every product and sum rounds to a
  float (24-bit control word). A turn difference is an unsigned 32-bit number loaded whole (`fild` of {n, 0}).
- `sub_4017F9` (push ecx; push eax; call 0x4012A4), which the patch put in the padding, sits only after a `ret` in
  these functions: never run.

#### The pieces they share

| Piece | Function | What it gives |
|---|---|---|
| The influence factor of a place | 0x658630 | 1.0 when `CalculatePlayerInfluence(place, P, 0, 0, allies 1) > 0`; else from the nearest place inside (0x64D360) `m = GetDistanceModifier(distance, reach)`, 0 when `m < 0.01` (a double compare), else `m × m`; 0 when there is no nearest place. The reach is 0x76D240: `power (P+0x8C) × 0.2 + virtualInfluenceMaxDistance [0xC5E230]` |
| The nearest place inside the influence | `GPlayer::FindNearestPosInsideInfluence` 0x64D360 | the place itself when inside; else, from 9999999: the citadel (+0xA48) edge `centre + GetPosFromAngle(Get3DAngleFromXZ(centre, place), influence)` when `distance − influence` is below 9999999 (the constant, not the best); then each town of the player in its list order (+0xA50, next +0x75C), its edge by its radius (+0x5C8) when `distance − radius` is strictly below the best. `Citadel::GetInfluence` (0x464090) is read twice |
| An icon's keenness | `WorshipSpellIcon::GetChargeUpSpeedDesire` 0x780510 | `x = store (+0x134) + 0.001`; with a site (vt +0x30C): `x = (GetChantsAvailable + battery (+0xF0)) + x`, and 0 when `x` is below the cost (`GetMagicEffectInfo(0)` +0x78) and `GetChantsVillagerCanSupply` is 0; then `x / (cost + 0.001)` |
| An icon's turns to be made | `WorshipSpellIcon::GetNumGameTurnsToCreate` 0x780480 | −1 without a site; `r = (cost − store) − GetTotalChantsAvailable`; 0 when `r ≤ 0`; −1 when the supply is 0; else `r / supply` |
| How keen the mind is on an icon | 0x664A50 | none without a citadel; the icon of the first of the citadel's six sites (+0x34..) that has one (0x463AD0, `GetSpellIconFromMagicType`); its keenness; **then cp+0x68[type] += 1** (0x658020); a keenness below 1 counts only at a site that is built (vt +0x890), repaired (vt +0x88C) and has worshippers; then the icon |
| A miracle's worth at a place | 0x6668F0 | 0 when the type is not enabled (`IsMagicTypeEnabled`); `k = 0x664A50`, and when below 1 `k = 0x664A50` again (so the count rises by 2); 0 without an icon or with turns below 0; `((place ? factor : 1) × GetDistanceModifier(turns, 2000)) × k`, up to 1; the icon goes out only past those tests |
| The best miracle by a weight | 0x666850 | over the types 1..41: a weight (a member function) of 0 is skipped; `worth × weight`, the strictly best and its icon |
| One miracle for the node | 0x666820 | 0x6668F0 with the out slot preset to the place pointer: the node's thing is the icon, **or the place pointer when no icon came out** (the score is then 0, so nothing reads it; openblack's node keeps nothing then) |
| A miracle cast there already | 0x665D40 | over the cp's cast list (+0x38): a spell whose cast place (+0xCC, +0xD0) is within 0x2000 map units on each side (signed integer difference, its magnitude strictly below), of the type (+0xB4), whose radius (vt +0x60) is at least the one asked for or any when that is 0. The list is filled only by the mind's cast actions and is emptied at a land change (0x6572C0) |
| A town's harm lately | 0x741980 | over the town's first 7 player slots (+0x9F4 + 0x80 n, the neutral one not counted): `x = min((turn − the slot's last attack turn +8) × 1/600 (0x3ADA740E), 1)`, the turns as an unsigned whole number (a later turn wraps round to long ago; a slot never attacked counts turn 0), the product rounded once; `fade = 1 − (x × x) × x`; the slot with the strictly highest fade above 0 gives the fade, its last attack's burn (+0x44) and its crush + hit (+0x48 + +0x4C); true when the burn or the crush + hit is above 0. So a later attack that did nothing (a physical shield's knock) hides an earlier one |
| The creature's free time | 0x658BC0 | 0 without the creature; 1.0 when no leash of its player is on (`GetInterfaceStatusLeashOn` 0x4CF060); else `t = turn − tied turn (+0x12C → +0x34)`, 0 when `t ≤ 450` or it fights; `k = (t − 450) × 1/450`; `life > 0.5 ? 1 : life`, `exhaustion < 0.5 ? 1 : 1 − exhaustion`, `energy > 0.5 ? 1 : energy`; `p = life × (energy × rest)`, 0 when the plan's desire (mind +0xF50) is 15 (to heal); `p × k` up to 1 |
| The creature's worth for a job | 0x65D1E0 (target, same town) | 0 without the creature, when the leash that is on is tied (+0x24) to the target, or (same town) to a thing of the target's town; else `(free × (1 − exhaustion)) × GetDistanceModifier(distance, size × 12 + 1000)`, the size being the physical +0x6C × 15 (0x65D1C0) |
| Where a town's villagers are | `Town::GetPosWhereVillagersAre(from, &where)` 0x740450 | 0 when the town's raw desire for sleep (`GetRawDesire(16)`) is above 0.5 (a double compare), and then `where` is not written; else the places in this order: the storage pit's door (vt +0x864, only with a pit), the meeting point (`GetCongregationPos` 0x7408B0, which works it out and keeps it at its first call, with its two `GameFloatRand` when no clear area is found), the town centre (+0x9A4, +0x14) and the fields (+0x780, newest first); each scored `count × GetDistanceModifier(from.GetDistance(place), 1000)`, the count an unsigned whole number (`fild` of {n, 0}), and put before the first one whose score it strictly beats (inline at 0x74056F / 0x740664 / 0x74075E, `LHOrderedLinkedList::Insert` 0x742310 for the fields: the first of equals stays first); the head's place goes out and its score is the answer |
| The thing near a place | 0x604870 (`this` the place; the computer player, a weight member function, the reach) | the player (vt +0x1C); the cells: each edge `ftol(((coordinate × MetresPerMapCell) × 2⁻¹⁶ ∓ reach) × 65536 / MetresPerMapCell)`, every product rounded to a float, its high word as a signed cell; x from the low edge to the high one (signed words), and for each x, when the low z is not above the high one, `(high z as an unsigned word) − low z + 1` cells from the low z (0x60499D..0x6049B5); a cell (fn_00601F40: its middle, the low words `(8 >> 1) × 0x2000`, altitude 0) counts when it is on the map (`InBounds` 0x6042C0, unsigned against [g+0x59C8] / [g+0x59C4]) and `CalculatePlayerInfluence(middle, P, 0, 0, allies 1) > 0`; its things the fixed list then the mobile one (`GetFirstIterator` 0x6034D0, next vt +0x53C); a thing whose weight is not below the best (`test ah, 1`) is worth `GetDistanceModifier(place.GetDistance(its place), reach) × weight`, the strictly best (`test ah, 0x41`); the best starts at 0 (in the first argument's slot, 0x6048D0); none when nothing beats 0 |
| The factor beyond the nearest place | 0x658710 (nearest, from) | `from + GetPosFromAngle(Get3DAngleFromXZ(from, nearest), speed (cp+0x58) + speed)` (`MapCoords::operator+`, the altitude too), the influence factor there (0x658630). The speed is the player's 90 m/s that `GPlayer::Init` gives |
| A town's best thing to attack | `Town::GetBestAttackObject(from)` 0x740C50 | over its buildings (+0x754, next +0x9C, newest first; the fields are among them): (life × percent built) × the building info's `computerAttackDesire` (+0x28 → +0xFC), the life vt +0x11C (`Object::GetLife`, +0x48) and the percent built vt +0x880, which is `MultiMapFixed::GetPercentBuilt` 0x4014F0 (+0x5C) in every building's table; then over its fields (+0x780): (food (+0xDC) / `totalFoodInField` (field type +0x120 → +0x130)) × the field type's `computerAttackDesire` (+0xFC); each × `GetDistanceModifier(from.GetDistance(its place), 1000)` (always given here); the strictly best above 0, none when nothing is |
| The villagers near a place | `GUtils::GetNumVillagersNear(place, 9)` 0x74F490 | the villagers `FindType(VILLAGER)` lists in each of 9 cells of the spiral from the place's cell (`GUtils::Spiral`, the place's own cell first: the 3 × 3 square round it), no bounds test |
| A town's radius | `Town::GetRadius` 0x73D6E0 | each area corner (+0x728..+0x738) × `MetresPerMapCell` (one rounding) × 2⁻¹⁶; the sides truncated and made positive; half the larger (the first when they are equal, in whole metres) |

The oracle (real exe code over made-up objects):
An oracle run (real exe code over made-up objects):

| Case | Result |
|---|---|
| place (0x400000, 0x380000) inside | 1.0 |
| outside; citadel (0x300000, 0x300000) influence 50; a town (0x420000, 0x3A0000) radius 30; power 100, reach 200 | `0x3F7FFB46`, nearest (0x3FE0F2, 0x37E0F2) |
| the same without the town | `0x3C569026`, nearest (0x347916, 0x323BFD) |
| place (0x900000, 0x900000), power 10, reach 20 | 0 (fall-off below 0.01), nearest (0x440202, 0x3C3AA1) |
| no citadel, no town | 0, none |
| towns (0x200000, 0x380000) r 30 and (0x3F0000, 0x370000) r 12.5, power 37.5, reach 150 | `0x3F7FFB46`, nearest (0x3FE246, 0x37E246) |
| icon store 10, cost 100; available 5, battery 2, total 30, supply 1.5 | keenness `0x3E2E16A8`, turns 40 |
| the same, supply 0 | 0, −1 |
| store 120, total 130, supply 0 | `0x3FA28F46`, 0 |
| no site | `0x3DCCD185`, −1 |
| store 0, cost 3000, available 17.25, battery 0.5, total 40, supply 0.75 | `0x3BC1E360`, `0x4576AAAB` |
| food miracle worth, store 120 (keen) | `0x3F7FFDA3`, count 1 |
| store 10 | `0x3E2E150D`, count 2; not built: 0, count 2; not enabled or no icon: 0, count 0; cost 3000 supply 0.5: `0x345B450C` |
| free time / job worth, no leash, target (0x80000, 0) | 1.0 / `0x3F6638AE` |
| tied 400 or 450 turns ago | 0 / 0 |
| tied 700 ago, life 0.375, exhaustion 0.625, energy 0.25, scale 0.8 | `0x3CA00000` / `0x3BEFD060`; plan to heal: 0 / 0 |
| tied 1900 ago, life 0.9, exhaustion 0.2, energy 0.75, scale 1.3, target (0x500000, 0x234567) | 1.0 / `0x3E589DCF`; fighting: 0 / 0 |
| tied at turn 0, now 123457, target (0x1500000, 0x34567) | 1.0 / `0x37F1D3D5` |

Where a town's villagers are (the oracle, the hand at (0x100000, 0x100000); the villagers near each
Where a town's villagers are (an oracle run, the hand at (0x100000, 0x100000); the villagers near each
place given call by call):

| Case | Result |
|---|---|
| sleep 0.6 | 0, `where` not written, nothing asked |
| door (0x140000, 0x100000) 1, meeting point (0x180000, 0x100000) 4, centre (0x1C0000, 0x100000) 4, fields (0x200000, 0x100000) 2 and (0x240000, 0x100000) 9 (sleep 0.25 or 0.5) | `0x410FB09F` at the second field |
| door (0x140000, 0x100000) 3, meeting point (0x100000, 0x140000) 3, centre (0x0C0000, 0x100000) 5, field (0x100000, 0x0C0000) 5 | `0x409FFBE5` at the centre (the first of equals) |
| door (0x2E0000, 0x100000) 10, meeting point (0x420000, 0x100000) 6, centre (0x560000, 0x100000) 3 | `0x411D5F3C` at the door |
| no store; meeting point (0x140000, 0x100000) 2, centre (0x6A0000, 0x100000) 40, field (0x180000, 0x180000) 1 | `0x3FFFF96E` at the meeting point |
| door and meeting point with none, field (0x8A0000, 0x7A0000) 1 | `0x38172465` at the field |
| nobody near any place | 0 at the door, else (no store) at the meeting point |
| door (0x6140000, 0x100000), meeting point (0x6120000, 0x100000), field (0x6108000, 0x100000), 5 each | `0x393CED7E` at the door |
| the best thing to attack, the hand at (0x100000, 0x100000): buildings (0x110000, 0x100000) built 1, life 1, wanted 0.5 and (0x120000, 0x100000) 1, 1, 0.75; field (0x130000, 0x100000) 100 / 400, 1.0 | the second building |
| two buildings at (0x110000, 0x100000) worth 0.5 each (0.5 × 0.5 × 2, 1 × 0.25 × 2) | the first |
| building 0.25, 1, 1 and field 350 / 400, 0.3, both at (0x110000, 0x100000) | the field |
| nothing built and an empty field | none |
| one building at (0x7110000, 0x100000) | it |
| buildings (0x123456, 0x0FEDCB) 0.8125, 0.6, 0.7 and (0x0F1234, 0x1A0000) 0.9, 0.95, 0.66; field (0x0E8000, 0x123000) 123 / 350, 0.9 | the second building |
| the factor beyond: speed 90, nearest (0x500000, 0x500000), from (0x300000, 0x500000) / (0x400000, 0x400000) | the factor asked at (0x420000, 0x500000) / (0x4CBA59, 0x4CBA59); the from's altitude kept |
| speed 37.5, nearest (0x123456, 0x0FEDCB), from (0x2A0000, 0x050000) | at (0x232BF4, 0x081A2F) |
| speed 90, nearest = from (0x500000, 0x500000) / speed 0 | at (0x620000, 0x500000), along x / at from |
| the thing near (0x88000, 0x88000), 250 m, the rock weight, a 16 × 16 map: a rock of 2 at the place, a tree of 3 two cells off, an unthrowable rock of 9, a plain thing of 9 | the tree; no cell in the influence: none |
| two rocks of 2 in one cell / a fixed and a moving one | the one at the head of the fixed list / the fixed one |
| 25 m round (0x88000, 0x88000): rocks 1 at the place, 5 at 20 m, 50 at 40 m and 1.5 at 30 m | the first; 36 cells asked, the first (6, 6) |
| 30 m round (0x8000, 0x8000): rocks 1 there and 2 at (0x28000, 0x38000) | the first; 16 cells asked, the first (0, 0) |
| a rock of 1 at the place and one of 1.25 at (0xE8000, 0xE8000) | the second |
| `ThrowrockatCreature`, its helpers stubbed: a Town above / the creatures 40 m apart, size 10 each / size 10.5 / no nearest place / no thing / no creature of the player's | 1.0 / the factor / 0 / 0 / 0 / the factor; the search from the hand with 250 m and the weight 0x665460 |
| `ThrowrockatCreature` with the real factor (nowhere inside, a citadel at (0x300000, 0x300000) of influence 50, reach 120, speed 90): the creature at (0x400000, 0x380000), (0x320000, 0x340000), (0x900000, 0x900000), (0x300000, 0x2A0000), (0x3A0000, 0x300000) | `0x3F48C2BB`, 0, 0, `0x3C569026`, `0x3F7CF433`, the thing in each |
| `ThrowThingsToImpress`, its helpers stubbed (the factor 0.75): no Town above / the villagers 0.5, boredom 0.8 / 1.5 / 0.01 / 0.0100001 / no nearest place / no thing / 1.0 with boredom 0.3 | 0 / `0x3E99999A` (two calls) / `0x3F19999A` (one) / 0 (two, nothing looked for) / `0x3BC49C28` / 0 / 0 / `0x3E666667`; the search with 250 m and the weight 0x6654B0 |
| `ThrowThingsToImpress` with the real pieces (as the real factor above; the store's door at (0x400000, 0x380000), the meeting point at (0x480000, 0x480000) with nobody): 1 villager at the door, boredom 0.8 / 40, 0.8 / 40, 0.25 / sleep 0.75 | `0x3CE42671` (the town asked twice) / `0x3F209BC9` / `0x3E48C2BB` / 0, nothing chosen |
| `CastSpellOnMyCreatureForALaugh170`: a Town above with a thing to attack / without / a building above / nothing above | the miracle at the thing (0x170000, 0x180000) / at the area centre / at the building's place / at (0, 0); the weight 0x669F90; the attack looked for from the hand |
| `TeachCreatureImpressiveMagic168`, the miracle 0.75 and the villagers 0.5, 1.25, 0, 1.0, 0.9999 | 0.375 (two calls), 0.75 (one), 0 (two), `0x3F19999A` for 0.6 (one), `0x3E9995AB` for 0.3 (two); nothing above: 0, nothing asked |

Town radii (5c2's oracle runs): area x 0x640000..0x820000, z 0xC80000..0xD20000 → 150; x 0x53039..0x70001, z
0x9004D..0x114321 → 41; x 0x12C8000..0x12D7FFF, z 0xFA0000..0x106FFFF → 64.5.

#### PlayerInteract and ReactToPlayer

| Score kind | Function | Rule |
|---|---|---|
| `TalktoPlayer`, `GiveToPlayer`, `ThrowThingAtPlayer`, `ReactToPlayerCastingSpells` | 0x664D60, 0x664D50, 0x664D70, 0x665030 | 0: their children are never weighed |
| `TeachCreatureAggressiveMagic164` (AttackCreatureWithSpells: 151, TeachCreatureAggressiveMagic: 164) | 0x659700 | the Creature above; with the player's unfiltered creature and that creature, 0 when their distance is below `max(2 × (size + size), 30)`; the best miracle (0x666850) whose `GMagicEffectInfo` +0x98 (the desire it makes a watcher feel) is 2 (anger), at the creature's place; the icon. ⚠️ With no creature above the original passes the place of a null object (0x14) on: a fault whenever an anger miracle's icon can be made. Only a force reaches it (every weighing path sets a creature above); openblack scores 0 there |
| `GetCreaturetoHelp` (124, 125), `UseCreatureToImpressTown` (142) | 0x6595E0 → 0x6595F0 | the Town above with a storage pit, and the creature; 0 when the first hand's leash (`GetLeaderInterfaceStatus` +0x12C → +0x24, on or not) is tied to a thing of that town; else the job worth at the pit (same town), and the creature as the thing (also at 0) |
| `AttackCreatureWithCreature` (152) | 0x659690 | the creature above (`CastCreature` vt +0xA4), and the player's creature; 0 when the first hand's leash is tied to it; 1.0, the player's creature |
| `ThrowrockatCreature` (153) | 0x665500 | the worth: 1.0; with a Creature above, 0 when the player's creature (+0xA4C, unfiltered) is strictly nearer to it than the fight distance (twice their sizes, at least 30), 0 when no place in the influence is found (0x64D360), else the factor beyond the nearest place (0x658710, from the creature); then the thing near the hand (0x604870, 250 m) by the weight 0x665460: a thing a player may throw (vt +0x4BC, `CanBeThrownByPlayer`) that breaks buildings (vt +0x7A8, `PhysicallyDestroysAbodes`) or is a tree (vt +0x338, `IsTree`), by its weight (vt +0x638, `GetWeight`), else 0. 0 without a thing; else the thing (+0x94) and **the worth, not the search's score**, also when the worth is 0 |
| `ReactToAggressiveCreature` (102) | 0x665040 | over `Creature::CreatureList` in order, but the player's creature: one whose place's most influential player (0x603830) is P, while P has a citadel, and whose player is not an ally of P: `GetDistanceModifier(to the citadel, power + 1)`, then for each town of P: +1 when `GetRadius × 1.2 > distance to its area centre`, and `+ GetDistanceModifier(that distance, the town's radius)`; the strictly best, up to 1 |
| `UseCreatureAsDiverseionAgainstPlayer` (174, DefendTownWithCreature: 155) | 0x65BDF0 | the GameThingWithPos above and the creature: `free time × GetDistanceModifier(distance, size × 2 + 200)` (0x65D2C0), the creature as the thing |
| `ReactToPlayerThrowingStuff` (104) | 0x664FA0 | P's citadel (+0xA48), its heart (+0x30) and the turn it was last struck by a thing that breaks buildings (heart +0x8C, written only by `CitadelHeart::ReactToPhysicsImpact` 0x467CCF, see [the heart's blows](../magic/temple-and-citadel.md#citadelheartreacttophysicsimpact)): 0 without them or when it is 0; `t = turn − struck` as an unsigned 32-bit number (`fild` of a 64-bit with a zero high half, so a later turn wraps round to long ago); 0 when `t ≤ 20` [0x8C7658]; else `p = t × 1/600` (`0x3ADA740E` [0x932D70], rounded once at 24 bits), `1 − min(p, 1)`, the heart as the thing (+0x94) once past the 20 turns, also when the score is 0. An oracle run, struck 1000: turn 1020 → 0, nothing chosen; 1021 → `0x3F770A3D`; 1137 → `0x3F458BF2`; 1300 → 0.5; 1599 → `0x3ADA7400`; 1600, 1601 → 0, the heart; struck 77, turn 400 → `0x3EEC5F92`; struck 5000 at turn 1000 → 0, the heart; never struck, no citadel or no heart → 0 |
| `CastPhysicalShieldToProtect` (177) | 0x665200 | the Object above; 0 when a physical shield was cast there; `2 × the miracle's worth at its place` (0x666820), and when that is below 1 the worth again, doubled |
| `DefendTown` (105) | 0x6689C0, 0x666AE0 | over P's towns and the players 0..7 of type 1 or 2 but P: a town's aggression slot for the player (+0x9F8 + 0x80 i) above 0, `min(2a, 1) × (1 − min((turn − last harm (+0x9FC)) / 1200, 1))³` (the turn whole, less the harm's turn as a float); the strictly best, the first always taken; the town as the thing, and **the attacking player as the parent's thing** when nothing above has one |
| `DefendtownWithSpells` (156, CastShieldToProtect: 175) | 0x668A50 | the Town above (0x655CC0, a dynamic cast); its harm lately (`GetHighestTownEffectMagicAndPhysical` 0x741980, below); 0 when that attack did no burn and no crush or hit; a shield (0x13) when its burn is strictly more than its crush + hit, else (a tie too) a physical shield (0x14); 0 when that miracle was cast at the town's area centre (0x665D40, any radius); `(worth × fade) × 2`, not kept to 1 (the worth 0x6668F0 at the centre); the icon (+0x94). **When no icon comes out** (the miracle not allowed, no icon, turns below 0) the worth is 0 and the out slot is never written: it is the uninitialised local at `[esp+0x18]` (0x668AE7), so the node's thing becomes whatever word an earlier call left on the stack there (the oracle's run left 0x026BFAA4), and the score is 0 |
| `CheckTownForDamage` (44) | 0x669080 | over P's towns and their buildings (newest first): `factor × life`, the strictly lowest (from 1); none: 0; the building as the thing, `lowest < 0.9 ? 1 : 1 − lowest²` |
| `FindBurningAbode` (109), `FindDamagedAbode` (110) | 0x669150, 0x6691B0 | the Abode above with a fire (+0x44) above its reaction temperature / with no fire at all: 1.0, the abode |
| `CastWaterSpell` (179) | 0x669310 | the water miracle anywhere, doubled as above |
| `CheckTownsForBeliefAttack` (43) | 0x66ABA0, 0x438A40 | over P's towns: the rival recent-belief ratio (0x438A40: the slots 0..5 not P and not P's allies, the largest recent (+0x28) above 0.008; 0 when none or P's belief is 0; `min((2 b[rival] / b[P])², 1)`) above 0 and `GetBeliefNeededOrLeft` not 0: the strictly best, the town |
| `CastSpellToDefendAgainstBelief` (186) | 0x66AC60, 0x66AD20 | the Town above with a thing to defend (`GetTownBeliefAttackDefendObject` 0x740E80: worship site, storage pit, newest building); the best miracle at its area centre by the weight shield 1.0 / physical shield 0.9 (0 when cast there); × `GetBeliefAttackImportanceMultiplier(P)` (0x741920: 1 for a town not P's, else `min(GetPercentCloseOtherPlayer × 10, 1)`); 0 without an icon |
| `CheckTownForProtection` (45) | 0x66B390 | over P's towns (+0xA50, next +0x75C) whose harm lately did a burn or a crush or hit and whose fade is strictly above the best so far: a physical shield (0x14) when the crush + hit is strictly more than the burn, else (a tie too) a shield (0x13); a town where that miracle was cast at its area centre is passed over; the strictly best fade, the town |
| `FindUnprotectedAbode` (111) | 0x669200 | the Abode above: a shield when it has a fire, else a physical shield, not cast there: 1.0, the abode |

None of these draws from the random generator.

#### CreatureInteract

The creature's desires are the mind's `Levels` from mind +0x150 (+0x160 hunger 4, +0x170 tiredness 8, +0x188 water
14, +0x19C warmer 19, +0x1A0 colder 20, +0x1C0 stay near home 28: the six readers match their nodes' names).

| Score kind | Function | Rule |
|---|---|---|
| `CreatureInteract` (14) | 0x659000 | the player's creature (0x65BCB0); 0 without it or while its 3D body (physical +0x160 → +0x58) performs a body action (`IsPerformingBodyAction` 0x4842D0: its state +0x4994 is not 0); else the free time (0x658BC0). This is the aim's own score: at 0 nothing under it is weighed. The state is written by `StateSet` 0x484EC0 (61 calls) and otherwise only by `Init`, `StartMovingToPoint` (1), `StopPointing` (9) and `DoMovingAction`: 0 standing free; 1 walking; 2, 3, 4 a static action's start, loop and end; 5, 6, 7 a dance; 8, 9 pointing; 0xA an individual action; 0xB, 0xC keeping or taking away a thing; 0xD a throw; 0xF taking from the hand; 0x11 a kiss; 0x12 a kick; 0x13, 0x14, 0x15 a catch; 0x16..0x22 the fight's states; 0x1E, 0x24, 0x1F fainting, lying fainted, getting up; 0x23 a turn on the spot; 0x25 set by `LoadBinary` (0x4EBD6E); a blended action its own (0x480F9D). Every one but 0 is busy. An oracle run: free, no leash → 1.0; walking (1), turning (0x23), lying (0x24), a sit's loop (3) → 0; free on a leash tied 700 turns ago (life 0.375, exhaustion 0.625, energy 0.25) → `0x3CA00000`, and 0 in a loop; tied 450 turns ago, fighting, a script's creature or none → 0 |
| `GiveCreatureFood` (59, CastFoodForMyCreature: 60), `MakeCreatureDrink` (61), `MakeCreatureSleep` (63), `SendCreatureHome` (64), `MoveCreaturetoWarmerArea` (65), `MoveCreaturetoCoollerArea` (66) | 0x659040, 0x659070, 0x6590A0, 0x6590D0, 0x659100, 0x659140 | the creature (its thing) and its hunger, water, tiredness, stay-near-home desire, warmer × 0.5, colder × 0.1 |
| `GetFoodFromField` (127), `GetFoodFromFishFarm` (128) | 0x666F20, 0x667030 | over P's towns and their fields (+0x780) or fish farms (+0x788), newest first: `factor × (GetDistanceModifier(distance from the hand, 250) × share)`, the share `(food (+0xDC) + 0.001) / (capacity (info +0x130) + 0.001)` for a field and 1 (0x52D240) for a farm; the strictly best above 0.1 for fields, above 0 for farms |
| `CastFoodSpellOn` (137, 160, ShallCastFoodSpell: 126, CastFoodSpellOnWorshipSite: 135) | 0x6672E0 | at the Town above's `GetResourceDropObject(FOOD)` (0x741EE0: its storage pit, else its temporary pot, **which it makes when the town has none**), or at the place of a GameThingWithPos above, or anywhere: the food miracle (0x666820) |
| `RestoreCreatureHealth` (62, ReacttoCreatureDamagedEmergency: 80) | 0x659540 | the creature at life 0.2 or less: `1 − life` |
| `RestoreMyCreatureHealth` (162, 172, CastHealSpellAtWorshipSite: 188) | 0x6599E0 | the heal miracle anywhere |
| `TeachCreatureNormalAction` (68) | 0x659180, 0x4E29E0 | the creature, when some of the six ordinary actions it could learn (`CreatureActionKnownAboutEntry` +0x64 not 0) it does not know (`KnowsAction(0, i)`): 0.3 |
| `TeachCreatureAggressiveMagic` (69), `TeachCreatureCompassionateMagic` (70), `TeachCreatureImpressiveMagic` (71) | 0x6591C0, 0x659250, 0x6592E0 | the creature and `count × 0.7` up to 1, the counts cp+0x11C, +0x118, +0x114: raised by the mind's DestroytownWithCreature (0x65C6D1) and GetCreaturetoHelp / GetCreaturetoHelpBuild actions (0x65BF6A, 0x65C18A) while the creature still learns that magic, cleared by the teaching actions; +0x114 is raised by nothing, so the impressive one is always 0 |
| `TeachCreatureCompassionateMagic166` (166) | 0x659960 | the best miracle anywhere whose watchers feel compassion (+0x98 is 1) |
| `TeachCreatureImpressiveMagic168` (168, CastSpellToImpress: 144) | 0x659830 | the Town above, else 0; `v` = the best miracle at its place (0x666850, the weight 0x659940: `GMagicEffectInfo` +0xC8, `cpImpressiveBalance`); `f = GetPosWhereVillagersAre(hand, &where)` (`where` starts at (0, 0, 0)), and when `f` is below 1 **the same call again** for `f`, else `f = 1`; the answer `f × v`. With an icon: the node's thing is the icon and its point (+0x84) `where` (`GetLHPoint`), also when `f` is 0 |
| `ThrowFoodatCreature` (73), `SlapCreature` (74), `StrokeCreature` (75), `AttachLeashtoMovingObject` (76) | 0x659370, 0x6593B0, 0x6593F0, 0x659430 | the creature (and for the leash, no leash of the player on): **`GameFloatRand(0.3) + 0.2`, `GameFloatRand(0.1)`, `GameFloatRand(0.1) + 0.1`, `GameFloatRand(0.3) + 0.2`** (lines 0xC5, 0xD0, 0xDE, 0xEC); no draw without the creature or with a leash on. Oracle from the campaign seed 0x88F89F: `0x3ECFE09D`, `0x3D8CA2F4`, `0x3E2CB7E0`, `0x3ECFE09D`, each leaving the seed at 0xE6F4C8D6 |
| `FindFreeVillagerInArea` (140) | 0x668900, 0x668510 | 36 cells in the spiral from the hand's (`GUtils::Spiral`): in each, the villagers `FindType` lists, each tested by 0x668510; the first cell with one accepted ends it, with the last accepted. 0x668510: available (0x752290), not a child (vt +0xAF8), reachable (vt +0x530), the state it heads for (vt +0xB04) serving no desire (`GVillagerStateTableInfo` +0x14 is −1), not at a worship site (+0xE0 & 2), not a disciple (+0xE0 & 0x200), building nothing (+0xFC), no script (+0x24 & 0x400); `m = GetDistanceModifier(distance from the hand, power)`, accepted when `factor × (life × m)` beats the best, **and the best becomes `m`** (the original keeps the fall-off, not the score it compared) |
| `CastSpellOnMyCreatureForALaugh` (77) | 0x659480 | the creature (its thing), with no miracle working on it (none of the 16 slots of +0x370 in use, 0x4F4DA0): `0x3D1332AE` (0.035937) |
| `CastSpellOnMyCreatureForALaugh170` (170, DestroytownWithSpell: 147) | 0x669E90 | the place: the Town above's best thing to attack from the hand (`GetBestAttackObject` 0x740C50), else its area centre (0x73AE10); else the place of the GameThingWithPos above; else (0, 0, 0); the best miracle at that place (0x666850, the weight 0x669F90: `GMagicEffectInfo` +0x114, `aggressiveAttackValue`); the icon when one is found (the out slot is the node's argument slot, which 0x666850 always writes) |
| `ReacttoCreatureStarvingEmergency` (79) | 0x6594C0 | the creature at energy (+0x1C) at most its kind's low-energy threshold (info +0x21C): `v = min(hunger, 1)`, the node's integer (+0x78) `= (int)(v × 1000)` |
| `GetFoodFromOtherTownForObject` (130, GetFoodFromOtherTownForStoragePit: 129) | 0x667120 → 0x667140 | the thing above; `need = parent's score × 1000`, nothing at 50 or less; over P's other towns with a store holding more than `(int)need` of the resource (unsigned): `x = factor × min(raw desire, 1)`, taken when `x` is below the parent's score and `1 − x` strictly beats the best; then the store's piles (food 0; wood 4 down to 0, `GetResourcePile` 0x7337D0) for one that holds something: the node's float (+0x80) = need, the pile as the thing |
| `ReacttoCreatureIllEmergency` (81) | 0x659590 | the creature's physical +0x4C at least 0.5. That field is written only by the `CreaturePhysical` ctor (0 at 0x4EF483; no other writer of a +0x4C reached from a +0x160 load, and `Load` writes +0x48 and +0x6C): always 0 |

The oracle (every place inside the influence, the hand at (0x100000, 0x100000)):
An oracle run (every place inside the influence, the hand at (0x100000, 0x100000)):

| Case | Result |
|---|---|
| fields (0x110000, 0x100000) 50 / 350 and, in a second town, (0x120000, 0x130000) 300 / 350 | field `0x3F5B4233`, the second; as fish farms `0x3F7FF96E`, the first |
| one field 1 / 350 | field 0; farm `0x3F7FF96E` |
| (0x300000, 0x100000) 350 / 350 and (0x101000, 0x100000) 0 / 350 | field 0; farm `0x3F7FFDA3`, the second |
| food from another town: parent 0.5, stores of 600, raw desires 0.25 and 0.75 | 0.75, the first town's pile, float 500 |
| the same with the parent at 0.04 / the piles empty | 0 / 0 |
| wood, parent 0.5, stores of 500 | 0 (500 is not more than 500) |
| wood, parent 0.3, stores of 600, raw desires 0.1 and 0.2 | `0x3F666666`, float 300 |

##### The body's state turn by turn

✅ Read from bw1-decomp (`Creature3D.cpp`: `StateSwitch`, `StateAction`, `DoMovingAction`, the starts) and the
exe's writers of +0x4994 (`StateSet` 0x484EC0 and its callers, xref #741, #742). A creature's
`Creature::ProcessState` runs `ProcessSubAction` (0x4737B0), which starts body actions, and then
`LH3DCreature::UpdateTime` (0x473939), which runs `StateSwitch` (the clip ends) and then `StateAction` (the walk).

- **Inside an action the state never passes through 0.** Each part goes straight to the next once its clip would
  run past its end: a static action 2 → 3 → 4, a dance 5 → 6 → 7, a fight cast 0x20 → 0x21 → 0x22 → 0x17, a block
  0x1A → 0x1B (0x1C → 0x1B) → 0x1D → 0x17. Only the last part sets 0, at the end of its clip.
- **A turn on the spot** (0x23) lasts until the heading is within 0.0001 of the one wanted, then 0 in the same
  `StateSwitch`. The fight's start (0x16), the blended pick-up and knock-down (0xE, 0x10), pointing (8, 9), the
  throw (0xD) and the catch (0x13) turn inside their own state (`TurningCompleted`), with no 0 between the turn and
  the clip.
- **A duel never passes through 0.** Every blow, step, recoil, block and cast goes back to 0x17 (`FIGHT_MAIN`).
  `EndFighting` 0x4845F0 only asks for the finish (+0x5238); 0x19 then ends in 0.
- **Between two parts of what the creature does, the state is 0 for the rest of the turn.** A walk's arrival
  (`DoMovingAction`, 0x48D320 / 0x48D4E3) and the end of a last clip set 0 inside `UpdateTime`, after that turn's
  `ProcessSubAction`. Every start refuses while the body is busy (`StartBlendedAction`, `StartTurningAction`,
  `StartFighting`, `ThrowAtPointGivenTime`, `StartPickUpFromHandAction`), so the next part starts in the next
  turn's `ProcessSubAction`. The computer player's weighing (`NewProcess`, in its player's turn) reads what the
  creature's last `ProcessState` left: 0 after such an end.

#### TownInteract

| Score kind | Function | Rule |
|---|---|---|
| `TownWithHighestFoodDesire` (83) | 0x666C50, 0x747AF0, 0x747A90 | over P's towns `GetDesire(FOOD)` (0x73E400: +0x14C + +0x108 + +0xC4), the strictly best; the town, and the node's integer `= (int)(DesiredFood − FoodAvailable)`: `info +0xDC × +0x6F4` less the whole `(int)carried (+0x708) + the store's (or the pot's) food` (one rounding, truncated towards 0) |
| `CreateDiscipleToFulfillTownDesire` (187) | 0x66AF60, 0x668790 | the Town above. With fewer than 2 breeders (+0x6DD): a villager at home (available, not at a worship site, **not** reachable, adult, sexually active, no script) of the first sex, 0 then 1, that has no breeder among them: 1.0, the integer 5. Else `w` = the functional totem's worship (+0x740, vt +0x92C) or 0; `total = 0.001 + Σ disciples[0..12]`; for each kind 1..12 with a town desire (`DiscipleInfos` +0x14: food, wood, food, to build, children): `v = adults × (1 − w) × 0.5`, skipped at 0; `S = (1 − (disciples[k] + 0.001) / total) × 0.25`; `r = GetDesire × (1 − min(total / v, 1)) × S`, the strictly best; then the builder (0x668650) × it, the integer the kind |
| `TownWithHighestWoodDesire` (84) | 0x666EB0 | over P's towns with a storage pit, `GetDesire(WOOD)`, the strictly best |
| `ShallCastWoodSpell` (132, 136, CastWoodSpellOn: 138) | 0x667360 | the wood miracle (0x666820) at the Town above's `GetResourceDropObject(WOOD)` (which makes its wood pot when it has none), the place of a GameThingWithPos above, a BuildingSite above's building, or anywhere |
| `GetWoodFromForest` (133) | 0x666CF0 | over the mind's seen list, head first (see [what the mind sees round its hand](#what-the-mind-sees-round-its-hand)), with `hand` the player's leader hand (its interface status +0xC8) and `power` P +0x8C: a **Forest** (a dynamic cast) with grown trees (+0x4C, as a float, 0 skipped) and a centre tree (`GetForestCentreTree` 0x53ABF0: the first growing tree +0x50 or the first grown +0x48, whichever is nearer the forest, the grown one on a tie) not held by a script (+0x24 bit 0x400): `v = ((wood (Tree::GetWoodValue 0x74B7B0) × factor (0x658630 at the tree)) × grown)`, then `DistanceChangeToBelief(dist(tree, hand) − its radius, power) × v`, then `factor × v` again, the candidate the **centre tree**; else a **Tree** (a magic one too) not held by a script: `DistanceChangeToBelief(dist − radius, power) × its info's wood (+0x28 → +0x6C, as a float)`, the tree; anything else (a seed) is passed over; the strictly best (a tie or a NaN keeps the first). None: 0, the node's thing untouched. Else the node's thing (+0x94) is it, and the score is `DistanceChangeToBelief(dist(the computer hand, it), power)`, with no radius. An oracle run (W1-W13) |
| `GetWoodFromOtherTown` (134) | 0x667130 → 0x667140 | as the food from another town, for wood (piles 4 down to 0) |
| `GiveBuildingSiteWood` (86) | 0x667C90 | over P's towns and their building sites (+0x790): available, with a building, wood still needed, and (built) wanting repair: `factor × (1 − min(wood / needed, 1)³)`; the strictly best, **in the first town that has one** |
| `GiveBuildingSiteWorkers` (85) | 0x667E20 | the same sites with most builders (info +0x110) and builders needed above 0: `factor × ((1 − min(builders (+0x6DC) / most, 1)) × GetDesireForVillagers)` |
| `FindFreeVillagerBuilderInTown` (139) | 0x668650 | the thing above's town (vt +0x48), or a BuildingSite's worship site's first town (+0xA4); its abodes' villagers by the free-villager test (0x668510), the last accepted and its kept best |
| `CombineScaffolds` (112) | 0x669350, 0x669580 | `Σ raw(abodes 5) < Σ raw(civic 6)` over P's towns: the best scaffold, 1.0. The best scaffold (0x669580): over P's towns, their workshops (vt +0x310) and their scaffolds (+0xE0): not the one given, not in a site (+0x68), not held or flying (+0x24 & 0x4C: 0x4 unavailable, 0x40 in physics; no `or` or immediate write of the exe sets 0x8); `m = GetDistanceModifier(distance from the hand, power + 50)`; by its value (`(int)((scale − 0.5) × 5 + 0.5)`): 1-2 `(raw(5) × 0.75 + 0.25) × m`, 0 above 175 places in its abodes (+0x644); 3-5 `(raw(6) × 0.75 + 0.25) × m`; 7 `min(2m, 1)`; else `m`; `factor × that`, the strictly best |
| `FindScaffoldToCombineWith` (180), `FindPlaceToUseScaffoldForBuilding` (113) | 0x669400, 0x669540 | the best scaffold but the Scaffold above / any: 1.0 |
| `FindLocationToUseScaffold` (181) | 0x669790 | the Scaffold above and its value `n`. A trial `PlannedAbode` (`CreateNoInit`, no town, so on no list; freed by vt +0xC at the end). Over P's towns: the want `raw(5)` (0 above 175 places) for `n ≤ 2`, `raw(6)` for `n < 5`, else 1; `r` its radius; **`dist = GameFloatRand(r × 2/3) + r × 0.5`, `angle = GameFloatRand(2π)`** (lines 0x57B, 0x57C); 4 tries at `area centre + GetPosFromAngle(angle, dist)`, the cells clamped onto the map (0x604250: only the cells move), `angle += π/4` each: a try counts when `Town::TryToCreateNewBuildingAt` finds a building (it draws **one `GameFloatRand(2π)`**, Town.cpp line 0xB31, and writes only into the trial) and the cell is not steep (0x736FD0: its four corners differ by more than 10); the strictly better want wins. So **6 draws a town**, whatever it wants. The town, and the try's place as the node's point |
| `FillWorkshop` (114) | 0x669B50 | the first workshop of P's towns that works (vt +0xD4), has room (`GetSpaceInStore`) and less wood than a scaffold's (`GScaffoldInfo` +0x6C, unsigned): `room / info +0x1A8` |
| `GiveWorshippersFood` (89) | 0x667B20 | over the citadel's six sites `CalculateDesireForFood` (vt +0x420), the strictly best; the site, the integer `CalculateFoodNeededFromTown` (0x77B920: `max(0, (int)food the dancers need − (food + food on the way +0xE8))`) |
| `GiveWorshippersRest` (90) | 0x66A7F0 | over the citadel's sites with dancers, `CalculateDesireForRest` (0x77C390: the average life desire of the dance's groups, 0x50BEB0), 0 below 0.1; `min(2 × best, 1)` |
| `TakeVillagerFromWorshipSiteToTown` (185) | 0x66A8A0 | the WorshipSite above with a desire for rest above 0.25: its villager most in want of life who has a town; `best > 0.75 ? 1 : best` |
| `WorshipSiteThatNeedsVillagers` (91) | 0x66A3B0, 0x77E400, 0x77E340, 0x741820 | over the citadel's sites `GetDesireForVillagerAvailableFromTown`: `w = GetDesireForWorshippers` (no icons (+0xE4) or not built: 0; else `1 − min(dancers / (4 × icons), 1)⁴`), 0 when `w` is 0, else `w ×` the most willing of its towns (+0xA4): `GetDesireToGiveWorshippers` (0 with no people (adults + children), in an emergency, or without a working worship site; `x = 1 − min((worshipping (+0x5C4) + on the way (+0x5CC)) / people, 1)`, `y = x³`: 1 from 0.7, **0 below 0.2**, else `y`) |
| `RaiseTotemInTown` (183) | 0x66A420 | 0 |
| `TakeVillagerFromTownToWorshipSite` (184) | 0x66A430 | the WorshipSite above and its gate (`GetGatePos`); over its towns whose will strictly beats the best: the first villager of its abodes (newest first) available for a worship site (0x752820), reachable, more than 50 m from the gate and where the factor is above 0; else the first homeless one available and more than 50 m away; the will and the villager |
| `ChargeUpSpell` (96) | 0x66A0D0 | over the citadel's sites: a built one with dancers: its icons (+0xE0, next +0x110) neither charging (+0x120) nor holding prayer (+0x134): `min(cp+0x68[type] × 0.01, 1)` for a count above 0, the strictly best; **a site not built but repaired is built by 1.0 at once** (`BuildBy`, vt +0x900): a world write during the weighing |
| `MakeDiscipleBreader` (119) | 0x66B470 | **`GameRand(number of P's towns)`** (line 0x8F2), only with a town; from that town round the list, the first with fewer than 2 breeders: 1.0 |

The oracle: two towns, areas (0x640000, 0xC80000)-(0x820000, 0xD20000) and
An oracle run: two towns, areas (0x640000, 0xC80000)-(0x820000, 0xD20000) and
(0x900000, 0x500000)-(0xAE0000, 0x5A0000), the campaign seed; the town's planning, the clamp and the steepness stubbed:

| Case | Result |
|---|---|
| scaffold value 3, civic wants 0.25 and 0.5 | 0.5, the second town; seed after 0xCE2E4D78 (12 draws); the first try at (0x7202EE, 0xBEAAC3), the second town's first at (0xAA3788, 0x4E90D8) |
| value 1, abodes wants 0.25 and 0.75 with 200 places | 0.25, the first town |
| no building found / every cell steep | 0, the same 12 draws |
| value 7 | 1.0, the first town |
| a breeder, three towns with 0 breeders / 2, 2, 1 / 2, 2, 2 | the first / the third / none; one draw, seed 0xE6F4C8D6 |
| a disciple, disciples {0, 3, 1, 0, 0, 2}, 40 adults, wants 0.9, 0.8, 0.6, 0.2 | `0x3E213DDA` (× the builder), kind 3; no disciples at all, or no adults: 0 |
| the will to send worshippers, 32 people, 15 or 14 worshipping | 0 (`y` 0.1499 and 0.178 are below 0.2); 10 people, 2 + 1: `0x3EAF9DB2` |

The desire for worshippers and the will (5c2's oracle runs): icons 1 with 0, 1, 2, 3, 4 dancers → 1, `0x3F7F0000`,
0.9375, `0x3F2F0000`, 0; icons 2 dancers 3 → `0x3F7AF000`; icons 3 dancers 5 → `0x3F7848B1`; people 15 worshipping
0 → 1, 3 + 1 → `0x3EC9EAEA`, 6 → `0x3E5D2F1C`; 7 / 1 → `0x3F21367C`; 24 / 2 + 3 → `0x3EFE097D`; 3 / 5 → 0; 9 / 1 + 1 →
`0x3EF0E65E`; 100 / 1 → 1.

#### DefeatPlayer

The players' walks: `GetNextPlayerAndNeutral` (0x550980) every slot 0..7; `GetNextActivePlayer` (0x5508D0) the slots
0..6 whose type (+0x8E0) is not 0, stopping when the slot after the last one given is of type 3. `IsAllied` (0x64D5D0):
the first player's ally value for the second (+0x950 + 4n) above 0, set only by `SET_PLAYER_ALLY` (both ways).

| Score kind | Function | Rule |
|---|---|---|
| `DefeatPlayer` (16) | 0x664B40 | 1.0 |
| `ExpandInfluence` (25) | 0x668D70, 0x668E20 | over every player but P and its allies (by their own value for P), the best town to take (0x668E20); the strictly best with a town. 0x668E20: `pop = P's population (+0x948)`; over the rival's towns: `cnt = Σ 0.5` for each icon of the town (+0x778, next +0x110) whose miracle P lacks, and 0.5 more when the mind looked for it (cp+0x68); `v = pop ? (adults + children) / pop : cnt + 1`; `r = 0` for the neutral or when P's power is 0, else `min(their power / P's, 1) × 0.85` (a double, one rounding); `w = DistanceChangeToBelief(distance from the hand, 250) × 0.5 + 0.5`; `GetDesireToBeTakenOver(P) × (v + cnt) × (1 − r) × w`, the strictly best, up to 1. `GetDesireToBeTakenOver` (0x740ED0): `1 − min(GetBeliefNeededToConvert × 0.2, 1)`, 0.75 more for P's own town (+0x5BC), from 0.1 up to 1 |
| `ThrowThingsToImpress` (143) | 0x665290 | the Town above, else 0; `b` = town +0x8A4, which is the town's boredom with reaction 9, a thing flying (`Reaction::ReactToFlyingObject` in openblack; `GBelief` at town +0x798, its `BoredomMultiplier[k]` at +0xE8 + 4k; `GameThingWithPos::GetBoredomMultiplier` 0x56FE70 reads `[town + k × 4 + 0x880]`); `f = GetPosWhereVillagersAre(hand, &where)` (`where` from (0, 0, 0)); when `f` is below 1 the same call again, and 0 when that `f` is at most 0.01 (`0x3C23D70A`, a float compare); else `f = 1`, with no such test; then 0 without a place inside the influence nearest `where` (0x64D360), else `factor` = the factor beyond it from `where` (0x658710); the worth `factor × (f × b)`; then the thing near the hand (0x604870 from the action state's place +0x14, which `GetHandPos` 0x657FE0 also gives; 250 m) by the weight 0x6654B0: a thing a player may throw (vt +0x4BC) that breaks no buildings (vt +0x7A8), `GetArtifactImpressiveModifier` (0x639900: 1, or for a town artefact (vt +0x7FC) ready to glow (0x4267F0) its vt +0xC8 + 1) + `GetImpressiveValue` (vt +0x66C), else 0; 0 without a thing; else the thing, the point `where` (`GetLHPoint`) and the worth |
| `TakeOverTownByFulfillingFoodDesire` (116) | 0x66AE60 | the Town above (its thing, always): `min(GetBeliefAttackImportanceMultiplier(P) × raw(food), 1)` |
| `DefeatPlayer37` (37) | 0x664B50 | over the active players then the neutral one whatever its type (`GetNextActivePlayerAndNeutral` 0x550930: slots 0..6 whose +0x8E0 is not 0, then slot 7) but P and those who count P an ally (`other->IsAllied(P)`): `attitude (cp + 0x1D8 + 4n → +0x14, kept doubled) × P->CalculateDesireToAttack(other)` (0x64CDB0, below); the strictly best, the first taken whatever it is worth; the player (+0x94); `min(best, 1)` (a NaN is kept). **CalculateDesireToAttack**, every step rounded to a float (24-bit precision): `gap = dist(P's fixed place, other's) − (P.power + other.power)`, 0 unless above 0 (the fixed place, `GetFixedPos` 0x64CD40: the citadel's heart, else the computer hand while its computer player is on, else the leader hand); `near = GetDistanceModifier(gap, 5120)`; with `x' = x + 0.0001` (a double, rounded once) for the powers (+0x8C) and their running averages (+0x90): `growth = max(other' / otherAvg' − P' / PAvg', 0)`, `r = other' / P'`, `strength = near × max(r − 1, 0)`, `weakness = (1 − r)³`, 0 when below 0; `revenge = 1 − (other.harm[P] + 0.001) / (P.harm[other] + 0.001)` (+0x94 + 4n, the double rounded once), 0 when below 0 (a NaN is kept); `alignment = |ftol(P.align − other.align)| / 4` in whole numbers (so 0 for alignments within −1..1); `proximity = 1 − nearest × 0.01f`, the nearest of P's buildings (towns +0xA50, abodes +0x754 / +0x9C) to P's hand, from 100 and strictly nearer; `((((proximity + alignment) + revenge) + weakness) + strength) + growth) × near`. With the other's creature (+0xA4C) it asks `CalculateMostInfluentialPlayer` at its interact point and drops the answer (no lasting effect). An oracle run (DA1-10, DP1-6) |
| `TakeOverTown` (93), `DestroyTown` (94) | 0x668D10, 0x668E10 | the GPlayer above's best town to take (0x668E20): the town, its worth |
| `CastSpellToFulfilTownNeeds` (145) | 0x669DA0, 0x740F80 | the Town above and its store, or its food pot (`GetStoragePitObjectForComputerPlayer`, **which makes the pot when it has neither**); `a = worth(WOOD) × raw(food)` and `b = worth(FOOD) × raw(wood)` at its place: **each miracle by the other's want**, as the original has them; the strictly better `b`, else `a` when not 0; the icon (only with the store) |
| `DestroytownWithCreature` (148) | 0x659680 | as `UseCreatureToImpressTown` |
| `PlayerWithMoreSpells` (38), `PlayerWithBetterCreature` (39) | 0x664E10, 0x664DA0 | the first active player but P that is not allied to P: 1.0, the player (nothing is compared) |
| `AttackCreature` (100) | 0x65D0D0 | the GPlayer above, not P, its creature (+0xA4C, unfiltered), not allied to P: 1.0 within 100 m of the hand, else `GetDistanceModifier(distance, 500)`; the creature (the most influential player at it is asked and dropped) |
| `PlayerWithMoreWorshippers` (40) | 0x664E20 | P's worshippers over its population (+ 0.0001 each), stored three times; the walk compares two of those copies, so it never chooses anyone: always 0 (a slip of the original, kept) |

The oracle, the best town to take: the hand at (0x100000, 0x100000), P's population 50 and
An oracle run, the best town to take: the hand at (0x100000, 0x100000), P's population 50 and
power 100, a rival of power 50 with towns at 80 m (20 adults, 5 children) and 10 m (10 + 2), the first with two icons
of a miracle P lacks, 1.0 of belief still needed:

| Case | Result |
|---|---|
| as above | `0x3F30A3BC`, the far town |
| the mind looked for the icons' miracle | 1.0 |
| rival power 150, P has the miracle, 3.0 needed | `0x3CF5C267` |
| P's population 0 | 1.0 |
| P's power 0, 4.5 needed | `0x3D4CCCAE` |
| the neutral | `0x3ECCCCAE` |
| P's own town of 500 at the hand, nothing needed | 1.0 |
| no towns | 0, none |
| population 64, powers 3 and 2.6, the far town alone, 2.0 needed | `0x3D7CCCAA` |

The will to be taken over (5d's oracle runs, 0x438910 stubbed): needed 0 → 1 / 1; 1 → 1 / `0x3F4CCCCD`; 3 → 1 /
`0x3ECCCCCC`; 4.5 → `0x3F599999` / 0.1; 4.9 → `0x3F451EB8` / 0.1; 6 → 0.75 / 0.1 (P's own town / another's).

### The rows

Each row as openblack's table lists it (the order of first meeting), with the exe's info row and its functions.
"Rows under it" are openblack's row numbers; the root's six are the build's (0x656580), not its own +0x58.
Both this table and the code table were written once from the exe by a script that emulates the initialisers (kept
outside the repository).

| Row | Name | Depth | Info row | Score (+0x18) | Action (+0x38) | +0x28 | Weight (+0x48) | After success (+0x4C) | Recovery (+0x50) | Period (+0x54) | Rows under it |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | `Master Decision Tree` | 0 | 0xD47F68 | - | - | - | 1.0 | 1.0 | 1.0 | 0 | 1, 12, 49, 84, 114, 125 |
| 1 | `PlayerInteract: 13` | 1 | 0xD4BA30 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 5000 | 2, 8 |
| 2 | `TalktoPlayer: 24` | 2 | 0xD4B1C0 | 0x664D60 | - | 0x65D550 | 0.25 | 1.0 | 1.001 | 0 | 3, 4, 5, 7 |
| 3 | `SendNiceTextToPlayer(Notdone): 52` | 3 | 0xD49870 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 4 | `SendNastyTextToPlayer(Notdone): 53` | 3 | 0xD498E8 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 5 | `TauntPlayer(Notdone): 54` | 3 | 0xD49960 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 | 6 |
| 6 | `DIRECTPickupAndDropObject: 182` | 4 | 0xD494B0 | - | 0x664790 | - | 0.0 | 0.0 | 1.0 | 1 |  |
| 7 | `ScarePlayer(Notdone): 55` | 3 | 0xD499D8 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 8 | `GiveToPlayer: 23` | 2 | 0xD4B148 | 0x664D50 | - | 0x65D550 | 0.25 | 1.0 | 1.001 | 0 | 9, 10, 11 |
| 9 | `GiveCreatureThingToPlayer(Notdone): 56` | 3 | 0xD49A50 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 10 | `GiveToPlayer(Notdone): 57` | 3 | 0xD49AC8 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 11 | `ThrowThingAtPlayer: 50` | 3 | 0xD497F8 | 0x664D70 | 0x664D30 | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 |  |
| 12 | `CreatureInteract: 14` | 1 | 0xD4BAA8 | 0x659000 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 100 | 13, 27, 35, 43 |
| 13 | `Lookaftercreature: 27` | 2 | 0xD4B2B0 | - | - | 0x65D5B0 | 0.4 | 1.0 | 1.001 | 0 | 14, 18, 20, 21, 23, 24, 25, 26 |
| 14 | `GiveCreatureFood: 59` | 3 | 0xD49B40 | 0x659040 | 0x658110 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 15, 16, 17 |
| 15 | `GetFoodFromField: 127` | 4 | 0xD481F0 | 0x666F20 | - | 0x65D790 | 1.0 | 1.0 | 1.001 | 0 |  |
| 16 | `GetFoodFromFishFarm: 128` | 4 | 0xD48268 | 0x667030 | - | 0x65D7F0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 17 | `CastFoodSpellOn: 137` | 4 | 0xD48628 | 0x6672E0 | 0x665A00 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 18 | `CastFoodForMyCreature: 60` | 3 | 0xD49BB8 | 0x659040 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 19 |
| 19 | `CastFoodForMyCreature: 160` | 4 | 0xD48E20 | 0x6672E0 | 0x659AD0 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 20 | `MakeCreatureDrink: 61` | 3 | 0xD49C30 | 0x659070 | 0x659C90 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 21 | `RestoreCreatureHealth: 62` | 3 | 0xD49CA8 | 0x659540 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 22 |
| 22 | `RestoreMyCreatureHealth: 162` | 4 | 0xD48E98 | 0x6599E0 | 0x659E60 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 23 | `MakeCreatureSleep: 63` | 3 | 0xD49D20 | 0x6590A0 | 0x65A020 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 24 | `SendCreatureHome: 64` | 3 | 0xD49D98 | 0x6590D0 | 0x65A1C0 | 0x65D5B0 | 0.5 | 1.0 | 1.001 | 0 |  |
| 25 | `MoveCreaturetoWarmerArea: 65` | 3 | 0xD49E10 | 0x659100 | 0x65A370 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 26 | `MoveCreaturetoCoollerArea: 66` | 3 | 0xD49E88 | 0x659140 | 0x65A470 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 27 | `TeachCreature: 28` | 2 | 0xD4B328 | - | - | 0x65D5B0 | 0.5 | 1.0 | 1.001 | 0 | 28, 29, 31, 33 |
| 28 | `TeachCreatureNormalAction: 68` | 3 | 0xD49F00 | 0x659180 | 0x65A570 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 29 | `TeachCreatureAggressiveMagic: 69` | 3 | 0xD49F78 | 0x6591C0 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 30 |
| 30 | `TeachCreatureAggressiveMagic: 164` | 4 | 0xD48F10 | 0x659700 | 0x65A6D0 | 0x65D990 | 1.0 | 1.0 | 1.001 | 0 |  |
| 31 | `TeachCreatureCompassionateMagic: 70` | 3 | 0xD49FF0 | 0x659250 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 32 |
| 32 | `TeachCreatureCompassionateMagic: 166` | 4 | 0xD48F88 | 0x659960 | 0x65A7F0 | 0x65DA40 | 1.0 | 1.0 | 1.001 | 0 |  |
| 33 | `TeachCreatureImpressiveMagic: 71` | 3 | 0xD4A068 | 0x6592E0 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 34 |
| 34 | `TeachCreatureImpressiveMagic: 168` | 4 | 0xD49000 | 0x659830 | 0x65A900 | 0x65DA40 | 1.0 | 1.0 | 1.001 | 0 |  |
| 35 | `PlayWithCreature: 29` | 2 | 0xD4B3A0 | - | - | 0x65D5B0 | 0.25 | 1.0 | 1.001 | 0 | 36, 37, 38, 39, 41 |
| 36 | `ThrowFoodatCreature: 73` | 3 | 0xD4A0E0 | 0x659370 | 0x65AA20 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 15 |
| 37 | `SlapCreature: 74` | 3 | 0xD4A158 | 0x6593B0 | 0x65AD40 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 38 | `StrokeCreature: 75` | 3 | 0xD4A1D0 | 0x6593F0 | 0x65AE00 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 |  |
| 39 | `AttachLeashtoMovingObject: 76` | 3 | 0xD4A248 | 0x659430 | 0x65AEC0 | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 | 40 |
| 40 | `FindFreeVillagerInArea: 140` | 4 | 0xD48790 | 0x668900 | - | 0x65D8D0 | 0.9 | 0.5 | 1.001 | 0 |  |
| 41 | `CastSpellOnMyCreatureForALaugh: 77` | 3 | 0xD4A2C0 | 0x659480 | - | 0x65D5B0 | 0.0 | 1.0 | 1.001 | 0 | 42 |
| 42 | `CastSpellOnMyCreatureForALaugh: 170` | 4 | 0xD49078 | 0x669E90 | 0x65B070 | 0x65D990 | 1.0 | 1.0 | 1.001 | 0 |  |
| 43 | `ReactToCreatureEmergency: 30` | 2 | 0xD4B418 | - | - | 0x65D5B0 | 0.95 | 1.0 | 1.001 | 0 | 44, 46, 48 |
| 44 | `ReacttoCreatureStarvingEmergency: 79` | 3 | 0xD4A338 | 0x6594C0 | 0x659A00 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 17, 15, 16, 45 |
| 45 | `GetFoodFromOtherTownForObject: 130` | 4 | 0xD48358 | 0x667120 | 0x667730 | 0x65D850 | 1.0 | 1.0 | 1.001 | 0 |  |
| 46 | `ReacttoCreatureDamagedEmergency: 80` | 3 | 0xD4A3B0 | 0x659540 | - | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 | 47 |
| 47 | `RestoreMyCreatureHealth: 172` | 4 | 0xD490F0 | 0x6599E0 | 0x659E60 | 0x65D990 | 1.0 | 1.0 | 1.001 | 0 |  |
| 48 | `ReacttoCreatureIllEmergency: 81` | 3 | 0xD4A428 | 0x659590 | 0x65B2A0 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 49 | `TownInteract: 15` | 1 | 0xD4BB20 | - | - | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 300 | 50, 65, 72, 82 |
| 50 | `AttendTownDesires: 32` | 2 | 0xD4B490 | - | - | 0x65D3C0 | 0.7 | 1.0 | 1.001 | 0 | 51, 56, 60, 62, 60 |
| 51 | `TownWithHighestFoodDesire: 83` | 3 | 0xD4A4A0 | 0x666C50 | 0x667460 | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 52, 53, 54, 15, 16, 55 |
| 52 | `CreateDiscipleToFulfillTownDesire: 187` | 4 | 0xD49708 | 0x66AF60 | 0x66B150 | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 53 | `GetCreaturetoHelp: 124` | 4 | 0xD48088 | 0x6595E0 | 0x65BE90 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 54 | `ShallCastFoodSpell: 126` | 4 | 0xD48178 | 0x6672E0 | 0x6678C0 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 55 | `GetFoodFromOtherTownForStoragePit: 129` | 4 | 0xD482E0 | 0x667120 | 0x667560 | 0x65D850 | 1.0 | 1.0 | 1.001 | 0 |  |
| 56 | `TownWithHighestWoodDesire: 84` | 3 | 0xD4A518 | 0x666EB0 | 0x667460 | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 52, 53, 57, 58, 59 |
| 57 | `ShallCastWoodSpell: 132` | 4 | 0xD483D0 | 0x667360 | 0x6678C0 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 58 | `GetWoodFromForest: 133` | 4 | 0xD48448 | 0x666CF0 | - | 0x65D860 | 1.0 | 1.0 | 1.001 | 0 |  |
| 59 | `GetWoodFromOtherTown: 134` | 4 | 0xD484C0 | 0x667130 | 0x669C00 | 0x65D8C0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 60 | `GiveBuildingSiteWood: 86` | 3 | 0xD4A608 | 0x667C90 | 0x6680F0 | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 61, 58, 59 |
| 61 | `ShallCastWoodSpell: 136` | 4 | 0xD485B0 | 0x667360 | 0x668480 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 62 | `GiveBuildingSiteWorkers: 85` | 3 | 0xD4A590 | 0x667E20 | 0x667FC0 | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 52, 63, 64 |
| 63 | `GetCreaturetoHelpBuild: 125` | 4 | 0xD48100 | 0x6595E0 | 0x65C0C0 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 64 | `FindFreeVillagerBuilderInTown: 139` | 4 | 0xD48718 | 0x668650 | - | 0x65D8D0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 65 | `ExpandTown: 33` | 2 | 0xD4B508 | - | - | 0x65D3C0 | 0.7 | 1.0 | 1.001 | 0 | 66, 68, 70 |
| 66 | `CombineScaffolds: 112` | 3 | 0xD4AD88 | 0x669350 | 0x669470 | - | 1.0 | 1.0 | 1.001 | 0 | 67 |
| 67 | `FindScaffoldToCombineWith: 180` | 4 | 0xD493C0 | 0x669400 | - | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 68 | `FindPlaceToUseScaffoldForBuilding: 113` | 3 | 0xD4AE00 | 0x669540 | 0x669A30 | - | 1.0 | 1.0 | 1.001 | 0 | 69 |
| 69 | `FindLocationToUseScaffold: 181` | 4 | 0xD49438 | 0x669790 | - | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 70 | `FillWorkshop: 114` | 3 | 0xD4AE78 | 0x669B50 | 0x658040 | - | 1.0 | 1.0 | 1.001 | 0 | 71, 58, 59 |
| 71 | `CastWoodSpellOn: 138` | 4 | 0xD486A0 | 0x667360 | 0x665A00 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 72 | `AttendToWorshippersWorshippers: 34` | 2 | 0xD4B580 | - | - | 0x65D3C0 | 0.7 | 1.0 | 1.001 | 0 | 73, 75, 78, 81 |
| 73 | `GiveWorshippersFood: 89` | 3 | 0xD4A6F8 | 0x667B20 | 0x667BB0 | 0x65D5E0 | 1.0 | 1.0 | 1.001 | 0 | 74, 15, 16, 45 |
| 74 | `CastFoodSpellOnWorshipSite: 135` | 4 | 0xD48538 | 0x6672E0 | 0x6683D0 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 75 | `GiveWorshippersRest: 90` | 3 | 0xD4A770 | 0x66A7F0 | - | - | 1.0 | 1.0 | 1.001 | 0 | 76, 77 |
| 76 | `CastHealSpellAtWorshipSite: 188` | 4 | 0xD49780 | 0x6599E0 | 0x66B290 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 77 | `TakeVillagerFromWorshipSiteToTown: 185` | 4 | 0xD49618 | 0x66A8A0 | 0x66A980 | - | 1.0 | 1.0 | 1.0 | 0 |  |
| 78 | `WorshipSiteThatNeedsVillagers: 91` | 3 | 0xD4A7E8 | 0x66A3B0 | - | - | 1.0 | 1.0 | 1.001 | 0 | 79, 80 |
| 79 | `RaiseTotemInTown: 183` | 4 | 0xD49528 | 0x66A420 | 0x6679A0 | - | 1.0 | 1.0 | 1.0 | 0 |  |
| 80 | `TakeVillagerFromTownToWorshipSite: 184` | 4 | 0xD495A0 | 0x66A430 | 0x66A5F0 | - | 1.0 | 1.0 | 1.0 | 0 |  |
| 81 | `ChargeUpSpell: 96` | 3 | 0xD4A950 | 0x66A0D0 | 0x66A220 | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 82 | `IncreaseTownPopulation: 35` | 2 | 0xD4B5F8 | - | - | - | 0.7 | 1.0 | 1.001 | 0 | 83 |
| 83 | `MakeDiscipleBreader: 119` | 3 | 0xD4B0D0 | 0x66B470 | - | - | 1.0 | 1.0 | 1.001 | 0 | 52 |
| 84 | `DefeatPlayer: 16` | 1 | 0xD4BB98 | 0x664B40 | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 200 | 85, 91, 98, 99, 104, 113 |
| 85 | `ExpandInfluence: 25` | 2 | 0xD4B238 | 0x668D70 | - | - | 0.75 | 1.0 | 1.001 | 0 | 86, 90, 90 |
| 86 | `TakeOverTownByImpressing: 115` | 3 | 0xD4AEF0 | - | - | - | 1.0 | 1.0 | 1.001 | 0 | 87, 88, 89 |
| 87 | `UseCreatureToImpressTown: 142` | 4 | 0xD48808 | 0x6595F0 | 0x65C370 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 3300 |  |
| 88 | `ThrowThingsToImpress: 143` | 4 | 0xD48880 | 0x665290 | 0x65CF50 | 0x65D930 | 1.0 | 1.0 | 1.001 | 0 |  |
| 89 | `CastSpellToImpress: 144` | 4 | 0xD488F8 | 0x659830 | 0x65CD60 | 0x65DA40 | 1.0 | 1.0 | 1.001 | 0 |  |
| 90 | `TakeOverTownByFulfillingFoodDesire: 116` | 3 | 0xD4AF68 | 0x66AE60 | 0x667460 | - | 1.0 | 1.0 | 1.001 | 0 | 54, 15, 16, 55 |
| 91 | `DefeatPlayer: 37` | 2 | 0xD4B670 | 0x664B50 | - | 0x65D550 | 0.1 | 1.0 | 1.001 | 0 | 92, 94, 81 |
| 92 | `TakeOverTown: 93` | 3 | 0xD4A860 | 0x668D10 | - | 0x65D470 | 1.0 | 1.0 | 1.001 | 0 | 87, 88, 89, 93 |
| 93 | `CastSpellToFulfilTownNeeds: 145` | 4 | 0xD48970 | 0x669DA0 | 0x6678C0 | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 94 | `DestroyTown: 94` | 3 | 0xD4A8D8 | 0x668E10 | - | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 95, 96, 97 |
| 95 | `DestroytownWithSpell: 147` | 4 | 0xD489E8 | 0x669E90 | 0x669FB0 | 0x65D990 | 1.0 | 1.0 | 1.001 | 0 |  |
| 96 | `DestroytownWithCreature: 148` | 4 | 0xD48A60 | 0x659680 | 0x65C5E0 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 97 | `DestroytownWithObject: 149` | 4 | 0xD48AD8 | - | 0x65CC50 | 0x65D930 | 1.0 | 1.0 | 1.001 | 0 |  |
| 98 | `PlayerWithMoreSpells: 38` | 2 | 0xD4B6E8 | 0x664E10 | - | 0x65D550 | 0.1 | 1.0 | 1.001 | 0 | 92, 94, 81 |
| 99 | `PlayerWithBetterCreature: 39` | 2 | 0xD4B760 | 0x664DA0 | - | 0x65D550 | 0.1 | 1.0 | 1.001 | 0 | 100 |
| 100 | `AttackCreature: 100` | 3 | 0xD4A9C8 | 0x65D0D0 | - | 0x65D6B0 | 1.0 | 1.0 | 1.001 | 0 | 101, 102, 103 |
| 101 | `AttackCreatureWithSpells: 151` | 4 | 0xD48B50 | 0x659700 | 0x65CA10 | 0x65D990 | 1.0 | 0.9 | 1.001 | 0 |  |
| 102 | `AttackCreatureWithCreature: 152` | 4 | 0xD48BC8 | 0x659690 | 0x65C800 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 103 | `ThrowrockatCreature: 153` | 4 | 0xD48C40 | 0x665500 | 0x665880 | 0x65D930 | 0.95 | 1.0 | 1.001 | 0 |  |
| 104 | `ReactToPlayer: 42` | 2 | 0xD4B850 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 | 105, 106, 108, 110 |
| 105 | `ReactToAggressiveCreature: 102` | 3 | 0xD4AA40 | 0x665040 | - | 0x65D6B0 | 1.0 | 1.0 | 1.001 | 0 | 101, 102, 103 |
| 106 | `ReactToPlayerCastingSpells: 103` | 3 | 0xD4AAB8 | 0x665030 | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 | 107 |
| 107 | `UseCreatureAsDiverseionAgainstPlayer: 174` | 4 | 0xD49168 | 0x65BDF0 | 0x665870 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 108 | `ReactToPlayerThrowingStuff: 104` | 3 | 0xD4AB30 | 0x664FA0 | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 0 | 109, 107 |
| 109 | `CastPhysicalShieldToProtect: 177` | 4 | 0xD49258 | 0x665200 | 0x668C30 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 110 | `DefendTown: 105` | 3 | 0xD4ABA8 | 0x6689C0 | - | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 111, 112 |
| 111 | `DefendTownWithCreature: 155` | 4 | 0xD48CB8 | 0x65BDF0 | 0x65C960 | 0x65D5B0 | 1.0 | 1.0 | 1.001 | 0 |  |
| 112 | `DefendtownWithSpells: 156` | 4 | 0xD48D30 | 0x668A50 | 0x668B30 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 113 | `PlayerWithMoreWorshippers: 40` | 2 | 0xD4B7D8 | 0x664E20 | - | 0x65D550 | 0.1 | 1.0 | 1.001 | 0 | 92, 94 |
| 114 | `ReactToPlayer: 17` | 1 | 0xD4BC10 | - | - | 0x65D550 | 1.0 | 1.0 | 1.001 | 50 | 104, 115, 119, 122 |
| 115 | `CheckTownForDamage: 44` | 2 | 0xD4B940 | 0x669080 | - | 0x65D3C0 | 1.0 | 1.0 | 1.001 | 0 | 116, 118 |
| 116 | `FindBurningAbode: 109` | 3 | 0xD4AC20 | 0x669150 | 0x668210 | 0x65DAD0 | 1.0 | 1.0 | 1.001 | 0 | 117 |
| 117 | `CastWaterSpell: 179` | 4 | 0xD49348 | 0x669310 | 0x665A00 | 0x65D720 | 1.0 | 1.0 | 1.001 | 0 |  |
| 118 | `FindDamagedAbode: 110` | 3 | 0xD4AC98 | 0x6691B0 | 0x668210 | 0x65DAD0 | 1.0 | 1.0 | 1.001 | 0 | 53 |
| 119 | `CheckTownsForBeliefAttack: 43` | 2 | 0xD4B8C8 | 0x66ABA0 | - | - | 1.0 | 1.0 | 1.001 | 0 | 120 |
| 120 | `DefendTownAgainstBeliefAttack: 118` | 3 | 0xD4B058 | - | - | - | 1.0 | 1.0 | 1.001 | 0 | 121, 87 |
| 121 | `CastSpellToDefendAgainstBelief: 186` | 4 | 0xD49690 | 0x66AC60 | 0x66AD70 | - | 1.0 | 1.0 | 1.001 | 0 |  |
| 122 | `CheckTownForProtection: 45` | 2 | 0xD4B9B8 | 0x66B390 | - | - | 1.0 | 1.0 | 1.001 | 0 | 123 |
| 123 | `FindUnprotectedAbode: 111` | 3 | 0xD4AD10 | 0x669200 | - | - | 1.0 | 1.0 | 1.001 | 0 | 124 |
| 124 | `CastShieldToProtect: 175` | 4 | 0xD491E0 | 0x668A50 | 0x668B30 | 0x65D990 | 1.0 | 1.0 | 1.001 | 0 |  |
| 125 | `WorldInteract: 18` | 1 | 0xD4BC88 | - | - | - | 1.0 | 1.0 | 1.001 | 5000 |  |

| Score kind | Function |
|---|---|
| `TalktoPlayer` | 0x664D60 |
| `GiveToPlayer` | 0x664D50 |
| `ThrowThingAtPlayer` | 0x664D70 |
| `CreatureInteract` | 0x659000 |
| `GiveCreatureFood` | 0x659040 |
| `GetFoodFromField` | 0x666F20 |
| `GetFoodFromFishFarm` | 0x667030 |
| `CastFoodSpellOn` | 0x6672E0 |
| `MakeCreatureDrink` | 0x659070 |
| `RestoreCreatureHealth` | 0x659540 |
| `RestoreMyCreatureHealth` | 0x6599E0 |
| `MakeCreatureSleep` | 0x6590A0 |
| `SendCreatureHome` | 0x6590D0 |
| `MoveCreaturetoWarmerArea` | 0x659100 |
| `MoveCreaturetoCoollerArea` | 0x659140 |
| `TeachCreatureNormalAction` | 0x659180 |
| `TeachCreatureAggressiveMagic` | 0x6591C0 |
| `TeachCreatureAggressiveMagic164` | 0x659700 |
| `TeachCreatureCompassionateMagic` | 0x659250 |
| `TeachCreatureCompassionateMagic166` | 0x659960 |
| `TeachCreatureImpressiveMagic` | 0x6592E0 |
| `TeachCreatureImpressiveMagic168` | 0x659830 |
| `ThrowFoodatCreature` | 0x659370 |
| `SlapCreature` | 0x6593B0 |
| `StrokeCreature` | 0x6593F0 |
| `AttachLeashtoMovingObject` | 0x659430 |
| `FindFreeVillagerInArea` | 0x668900 |
| `CastSpellOnMyCreatureForALaugh` | 0x659480 |
| `CastSpellOnMyCreatureForALaugh170` | 0x669E90 |
| `ReacttoCreatureStarvingEmergency` | 0x6594C0 |
| `GetFoodFromOtherTownForObject` | 0x667120 |
| `ReacttoCreatureIllEmergency` | 0x659590 |
| `TownWithHighestFoodDesire` | 0x666C50 |
| `CreateDiscipleToFulfillTownDesire` | 0x66AF60 |
| `GetCreaturetoHelp` | 0x6595E0 |
| `TownWithHighestWoodDesire` | 0x666EB0 |
| `ShallCastWoodSpell` | 0x667360 |
| `GetWoodFromForest` | 0x666CF0 |
| `GetWoodFromOtherTown` | 0x667130 |
| `GiveBuildingSiteWood` | 0x667C90 |
| `GiveBuildingSiteWorkers` | 0x667E20 |
| `FindFreeVillagerBuilderInTown` | 0x668650 |
| `CombineScaffolds` | 0x669350 |
| `FindScaffoldToCombineWith` | 0x669400 |
| `FindPlaceToUseScaffoldForBuilding` | 0x669540 |
| `FindLocationToUseScaffold` | 0x669790 |
| `FillWorkshop` | 0x669B50 |
| `GiveWorshippersFood` | 0x667B20 |
| `GiveWorshippersRest` | 0x66A7F0 |
| `TakeVillagerFromWorshipSiteToTown` | 0x66A8A0 |
| `WorshipSiteThatNeedsVillagers` | 0x66A3B0 |
| `RaiseTotemInTown` | 0x66A420 |
| `TakeVillagerFromTownToWorshipSite` | 0x66A430 |
| `ChargeUpSpell` | 0x66A0D0 |
| `MakeDiscipleBreader` | 0x66B470 |
| `DefeatPlayer` | 0x664B40 |
| `ExpandInfluence` | 0x668D70 |
| `UseCreatureToImpressTown` | 0x6595F0 |
| `ThrowThingsToImpress` | 0x665290 |
| `TakeOverTownByFulfillingFoodDesire` | 0x66AE60 |
| `DefeatPlayer37` | 0x664B50 |
| `TakeOverTown` | 0x668D10 |
| `CastSpellToFulfilTownNeeds` | 0x669DA0 |
| `DestroyTown` | 0x668E10 |
| `DestroytownWithCreature` | 0x659680 |
| `PlayerWithMoreSpells` | 0x664E10 |
| `PlayerWithBetterCreature` | 0x664DA0 |
| `AttackCreature` | 0x65D0D0 |
| `AttackCreatureWithCreature` | 0x659690 |
| `ThrowrockatCreature` | 0x665500 |
| `ReactToAggressiveCreature` | 0x665040 |
| `ReactToPlayerCastingSpells` | 0x665030 |
| `UseCreatureAsDiverseionAgainstPlayer` | 0x65BDF0 |
| `ReactToPlayerThrowingStuff` | 0x664FA0 |
| `CastPhysicalShieldToProtect` | 0x665200 |
| `DefendTown` | 0x6689C0 |
| `DefendtownWithSpells` | 0x668A50 |
| `PlayerWithMoreWorshippers` | 0x664E20 |
| `CheckTownForDamage` | 0x669080 |
| `FindBurningAbode` | 0x669150 |
| `CastWaterSpell` | 0x669310 |
| `FindDamagedAbode` | 0x6691B0 |
| `CheckTownsForBeliefAttack` | 0x66ABA0 |
| `CastSpellToDefendAgainstBelief` | 0x66AC60 |
| `CheckTownForProtection` | 0x66B390 |
| `FindUnprotectedAbode` | 0x669200 |

| Action kind | Function |
|---|---|
| `DirectPickupAndDropObject` | 0x664790 |
| `ThrowThingAtPlayer` | 0x664D30 |
| `GiveCreatureFood` | 0x658110 |
| `CastFoodSpellOn` | 0x665A00 |
| `CastFoodForMyCreature` | 0x659AD0 |
| `MakeCreatureDrink` | 0x659C90 |
| `RestoreMyCreatureHealth` | 0x659E60 |
| `MakeCreatureSleep` | 0x65A020 |
| `SendCreatureHome` | 0x65A1C0 |
| `MoveCreaturetoWarmerArea` | 0x65A370 |
| `MoveCreaturetoCoollerArea` | 0x65A470 |
| `TeachCreatureNormalAction` | 0x65A570 |
| `TeachCreatureAggressiveMagic` | 0x65A6D0 |
| `TeachCreatureCompassionateMagic` | 0x65A7F0 |
| `TeachCreatureImpressiveMagic` | 0x65A900 |
| `ThrowFoodatCreature` | 0x65AA20 |
| `SlapCreature` | 0x65AD40 |
| `StrokeCreature` | 0x65AE00 |
| `AttachLeashtoMovingObject` | 0x65AEC0 |
| `CastSpellOnMyCreatureForALaugh` | 0x65B070 |
| `ReacttoCreatureStarvingEmergency` | 0x659A00 |
| `GetFoodFromOtherTownForObject` | 0x667730 |
| `ReacttoCreatureIllEmergency` | 0x65B2A0 |
| `TownWithHighestFoodDesire` | 0x667460 |
| `CreateDiscipleToFulfillTownDesire` | 0x66B150 |
| `GetCreaturetoHelp` | 0x65BE90 |
| `ShallCastFoodSpell` | 0x6678C0 |
| `GetFoodFromOtherTownForStoragePit` | 0x667560 |
| `GetWoodFromOtherTown` | 0x669C00 |
| `GiveBuildingSiteWood` | 0x6680F0 |
| `ShallCastWoodSpell` | 0x668480 |
| `GiveBuildingSiteWorkers` | 0x667FC0 |
| `GetCreaturetoHelpBuild` | 0x65C0C0 |
| `CombineScaffolds` | 0x669470 |
| `FindPlaceToUseScaffoldForBuilding` | 0x669A30 |
| `FillWorkshop` | 0x658040 |
| `GiveWorshippersFood` | 0x667BB0 |
| `CastFoodSpellOnWorshipSite` | 0x6683D0 |
| `CastHealSpellAtWorshipSite` | 0x66B290 |
| `TakeVillagerFromWorshipSiteToTown` | 0x66A980 |
| `RaiseTotemInTown` | 0x6679A0 |
| `TakeVillagerFromTownToWorshipSite` | 0x66A5F0 |
| `ChargeUpSpell` | 0x66A220 |
| `UseCreatureToImpressTown` | 0x65C370 |
| `ThrowThingsToImpress` | 0x65CF50 |
| `CastSpellToImpress` | 0x65CD60 |
| `DestroytownWithSpell` | 0x669FB0 |
| `DestroytownWithCreature` | 0x65C5E0 |
| `DestroytownWithObject` | 0x65CC50 |
| `AttackCreatureWithSpells` | 0x65CA10 |
| `AttackCreatureWithCreature` | 0x65C800 |
| `ThrowrockatCreature` | 0x665880 |
| `UseCreatureAsDiverseionAgainstPlayer` | 0x665870 |
| `CastPhysicalShieldToProtect` | 0x668C30 |
| `DefendTownWithCreature` | 0x65C960 |
| `DefendtownWithSpells` | 0x668B30 |
| `FindBurningAbode` | 0x668210 |
| `CastSpellToDefendAgainstBelief` | 0x66AD70 |

## openblack

- A villager's facts (`VillagerFactsOf`, `Systems/Implementations/ComputerPlayerGameWorld.cpp`) read "at a worship site" from the villager's +0xE0 & 2 flag, `Villager::k_FlagAtWorshipSite`, as 0x668510 does; since 2026-10-29 the worship code sets it (before, it was never set, so the free-villager and breeder tests never turned down a worshipper). Test `WorshipperTest.BeingCountedAtTheSiteIsTheVillagersOwnFlag`.
- `ECS/ComputerPlayerMindTree.h` (`openblack::computer_player`): `ScoreKind` (86 ways and `None`) and `ActionKind` (58
  and `None`), each named after the first row that uses it; `k_MindRows`, the 126 rows above (name, score, action,
  personality, suppression after success, recovery, period, children); `MindNodes()`, the 198 nodes in the
  original's order, written out with the rows by the same generator (`RowOf`, `ChildrenOf`); `NamePrefix`, `NameMatches`, `FindNodes` (every match in
  order, as the n-th-match loop; a node matches when its name before the ':' begins the name asked for, as the
  original's lookup compares only that many characters, so `AttackCreatureWithSpells` finds `AttackCreature: 100`
  first and its own row second; test `ComputerMindTree.TheRowsHoldBackAfterSuccessAsTheGameDoes`), `FindNode` and `MindNodeNames` (the 117 names, each where first met).
- `ecs::components::ComputerMind` (`ECS/Components/ComputerMind.h`): one `ComputerMindNode` per node (suppression,
  personality, the turns last weighed and run, its thing, second thing and point (0 at the start, as the original's zeroed nodes), score and chosen node), the plan's
  score, the turn of the last empty weighing, the rest after steps (1 at the start, as `Init` sets +0x1C8) and the
  attitude to each of the eight players (1.0 each as kept, the ctor's `GComputerAttitudeToPlayer` +0x14).
  `computer_player::MakeMind` makes it.
- `computer_hand::FindMindAction` (the hands' named actions) reads the tree: the first node the name names, and what
  its row does.
- The weighing and the runs, pure (`ECS/ComputerPlayerMind.h`), over a `MindTree` (the game's is `GameTree()`) and a
  `MindWorld` seam for the score functions and actions: `Recover`, `Weigh`, `WeighChildren`, `WeighAll`, `IsDue`,
  `WeighDueAims`, `RunAction`, `RunNode`, `RunChosen`, `RunPlan`, `OutbidsPlan`, `BusyTurn`, `NextFreeTurn`
  (`FreeTurn`) and `WeighAndRun`; every product a float. An action openblack lacks (`MindWorld::HasAction`) is not run
  and leaves the suppression as it was. The game does not call them yet. Tests: `ComputerMindWeighing.*`,
  `ComputerMindRunning.*`, `ComputerMindTurns.*`, pinned to the oracle's bits.
- The natives (`src/CHLApi.cpp`): 364 and 365 → `ComputerPlayerSystem::SetPersonality` / `SetSuppression`
  (`computer_player::SetPersonality` / `SetSuppression`, every node `FindNodes` gives), 378 → `SetAttitude` (kept
  doubled), 379 → `Attitude` (halved; 0.5 for a mind no script set, 0 for a number naming no player). The
  `ComputerMind` is made on the player's entity the first time a script sets it.
- The personality file: 380 → `ComputerPlayerSystem::LoadPersonality`, 381 → `SavePersonality` (the same player
  check as 364). The file is read afresh through the file system service (`Locator::filesystem`, the path through
  `FileSystemInterface::FixPath`; not the resource caches, since a script may write it and read it again) and
  written with `resources::WriteFile`. The bytes are pure functions: `computer_player::ReadPersonality` (one run of
  bytes over the 198 nodes in order, each read taking what is left, the float's bytes lowest first) and
  `WritePersonality`. The rows' names that the lookups read are a `computer_player::MindRowNames` the system keeps for
  the whole game, shared by every mind (`OwnRowNames()` is the rows' own, which nothing writes): `FindNodes`,
  `FindNode`, `SetPersonality`, `SetSuppression`, `Force` and `computer_hand::ForceAction` take it, so a name a file
  wrote over a row is the one 364, 365, FORCE and QUEUE find (`computer_hand::FindMindAction` too, which tells what the
  node found does by its row). Tests: `ComputerMindPersonalityFile.*` on synthetic bytes.
- The land-script keywords (`src/LHScriptX/FeatureScriptCommands.cpp`): `LoadComputerPlayerPersonality` (player by
  number, the file's name as text) → `ComputerPlayerSystem::LoadPersonality`, with a number of 0 to 7 only (openblack's
  check, where the original has no player); `SetComputerPlayerPersonality` (player by name through
  `FeatureScriptCommands::PlayerFromText`, the node's name as text, the weight) → `SetPersonality`. Their second
  argument is a string, as the "A" slot's text the original reads. Test
  `PlayerSystem.theLandScriptsPersonalityKeywordsReachTheComputerPlayers`.
- A land change keeps it: `Game::PrepareNewLand` calls `ComputerPlayerSystem::KeepForNextLand`, which keeps each
  player's mind (`computer_player::StartNewLand`: no empty weighing, the rest after steps set, as `Init`), and the
  player's entity on a later land takes it up (`computer_player::TakeUp`) when the mind is next asked for.
- The debug bar's **Computer hands** window shows each mind: the plan's worth, the empty weighing's turn, the rest, the
  attitudes as a script reads them, and the tree (folded, pre-order) with every node's weight, suppression, worth,
  choice and turns; and a **Hold this mind** switch, off by default (`ComputerMind::heldByDebug`), which the mind's
  turn will read once it weighs.
- Forcing: `computer_player::Force` (`IsDirectName`, `ForcedOn`, `CanWeighUnder`, `k_ForcedScore`), the steps above
  over the game's tree; `computer_hand::ForceAction` runs it with the hand's actions (`HandActions`) and sets the hand
  busy when it was done; FORCE and the queue's next action go through it (`ComputerPlayerSystem::Force`, the player's
  mind made or taken up when first forced). The actions read the thing nearest above the node (`ThingAbove`, 0x655CB0)
  and, for the scaffold, the point of the node it chose (`ChosenBelow`, 0x655CD0); a DIRECT node keeps its second
  thing only when it has a place (`ThingFacts::HasPlace`, a `Transform`). A force that would weigh a way of scoring openblack lacks is refused before
  it changes anything, and a chosen action openblack lacks is not run (its suppression stays).
- Tests: `test/test_computer_mind.cpp` (group `test_magic`), `ComputerMindTree.*`, `ComputerMindScripts.*`,
  `ComputerMindForcing.*`; the hand's side in `test/test_computer_hand.cpp` (`ComputerHandActions.*`).
- The score rules (`ECS/ComputerPlayerScores.h`, `computer_player::Score` and `IsScorePorted`, over the `ScoreWorld`
  seam of what they read, always for the mind's own player): the shared pieces above (`PlayersCreature`,
  `VirtualInfluenceReach`, `NearestPlaceInInfluence`, `InfluenceFactor`, `ChargeUpSpeedDesire`, `TurnsToCreate`,
  `IconWant`, `SpellWorth`, `CastNear`, `CreatureFreeTime`, `CreatureJobWorth`, `TownRadius`), pinned to the oracle in
  `test/test_computer_mind_scores.cpp` (`ComputerMindScores.*`). The mind keeps the count of icons found
  (`ComputerMind::iconsFound`, the whole game) and the miracles it cast (`spellsCast`, emptied at a land change by
  `StartNewLand`). The game's answers are `computer_world::MakeScoreWorld` (`src/ECS/Systems/Implementations/
  ComputerPlayerGameWorld.cpp`), read from the game's own state as the original's queries read theirs (read in the
  v1.42 exe 2026-10-27): the place a miracle was cast at is the mind's own, kept as it casts (`ComputerMindCast`, the
  thing's map place or the point's, as 0x6542A0 / 0x6544F0 / 0x654690 push it; 0x665D40 reads it); the tied turn is
  the player's leash's, kept through taking it off and putting it on (`CreatureLeash::lastTiedTurn`, the tie's
  write 0x5E6D17); a scaffold is busy in a god's hand or in the physics (+0x24 & 0x4C: `InPhysics`); the leader's
  hand is where the hand was as the turn began (status +0xC8: the step's start 0x6503B4, else where the last step
  left it; nowhere before the first, `ComputerHand::lastStepTurn`); a thing a player may throw by its class
  (vt +0x4BC: a fragment never, a pile while not empty, a rock, the bonfire or a dead tree at most 3.6 across, else a
  moving object, the whale among them, a mobile static or a tree; openblack makes no arrow, ball pit ball or
  puzzle block) and its own weight with no lower bound (vt +0x638,
  `DynamicsSystemInterface::ObjectWeight`); the plan's desire, which the original's stop of a plan sets to none (of
  the twelve idle actions openblack falls back to, none has desire 15 in info.dat); the
  ordinary actions not known as the mind holds them from its making (its file's, else learnt, then a script's); a
  player's fixed place, the computer hand's map place as of its last step, or the local player's synced turn hand
  (`HandSystemInterface::GetTurnHand`); charging up a miracle (`ChargeUpSpell`, 0x66A0D0) builds the worship site
  and its altar by the amount (`WorshipSite::BuildBy` 0x77DC50, openblack's `abodes::BuildBy`).
- **The mind's turn** (`ComputerPlayerSystem::ProcessTurn`, in each player's turn as `NewProcess` 0x657420, wired
  2026-10-27): whatever the mind's state, the miracles it cast that are gone and what it saw that is gone are
  forgotten (0x665C70, 0x656A30), and the queued actions and the hand's steps whose things are gone are dropped
  (0x6644D0, 0x650300); then the gate (on, not paused, not held by the wide screen), its start on the land once,
  the look round the hand (`ScanAroundHand`, `computer_world::MakeSeenWorld`), and `computer_player::TakeTurn`:
  a busy or held hand goes through its steps, and while no script holds it the due aims are weighed and a best
  worth more than twice the plan under way runs; a free hand forces the next queued action, and when that gave it
  nothing the mind rests (30 turns after an empty weighing, or its first free turn after steps) or weighs the whole
  tree in the same turn. A forced action weighs with the same scores. Test
  `ComputerMindTurns.ATurnStepsTheBusyHandOrForcesTheQueueBeforeTheWeighing`.
  - **TakeVillagerFromTownToWorshipSite** (row 80, its action 0x66A5F0, read 2026-10-28; no random draw): the
    node's thing as a `Villager` and the thing above its parent as a `WorshipSite` (0x655CB0 / 0x655CC0), each
    checked (`sub_658230`, "No Villager" / "No Worship Site": the action fails, 0); then the site's gate
    (`WorshipSite::GetGatePos` 0x77CEB0, special point 9) and the pick-up (0x658AA0: refused for a thing with bit
    0x4 or 0x8 of +0x24, in a god's hand) each, when they fail, end the action as done (1) with no step (the jump to
    0x66A7D7). Else four steps: PickUpObject (the villager, the player's speed +0x58), MoveHandToPos (the gate's
    point, and a float 900 its start does not read: 0x652750 reads only the first argument), Wait (1.0 s) and
    DropObjectInHandAtPos (the gate's point, the player's speed); done (1). openblack (2026-10-28):
    `computer_hand::TakeToGate` from `HandActions::RunAction`, the gate as `worship::site::GetSpecialPos(Arrive)`
    through the map grid (`ThingFacts::GatePoint`); tests `ComputerHandActions.AVillagerIsTakenToTheGateHeldASecondAndDropped`,
    `ComputerHandActions.NoGateOrAVillagerInAHandIsDoneWithNoSteps`, `ComputerHandActions.NoVillagerOrNoWorshipSiteFails`.
  - **GetFoodFromOtherTownForObject** (row 45, under ReacttoCreatureStarvingEmergency and GiveWorshippersFood, its
    action 0x667730, read 2026-10-28; no random draw): the node's thing (the pile its score chose) and the thing above
    its parent, each as an `Object` (0x655CB0 / 0x655CC0, "No Object": the action fails, 0); then three steps:
    MoveHandToPos (the pile's map place, +0x14), CreateFoodFromContainer (the pile, and the parent's integer +0x78
    through its +0xC: the food the creature asks for, or the food the worship site needs from a town) and
    DropObjectInHandOnObject (the object above); done (1). The steps are on
    [the computer players' hands](computer-hands.md#holding). openblack (2026-10-28): `computer_hand::FetchFoodFor`
    from `HandActions::RunAction`, the pile's place through the map grid (`ThingFacts::MapPoint`); the give is the
    held pot's own rule (`held_apply`; `Pot::ValidToApplyThisToObject` 0x66DD50: a store of its resource or a pot of
    it), so a worship site's store takes it, and a creature, no store (`Object::IsResourceStore`), does not: the pot
    stays in the hand, in the original's too. Tests
    `ComputerHandActions.FoodIsFetchedFromThePileAndGivenToTheObject`, `ComputerHandActions.NoPileOrNoObjectToFeedFails`.
  - **GetWoodFromOtherTown** (row 59, under TownWithHighestWoodDesire, GiveBuildingSiteWood and FillWorkshop, its
    action 0x669C00, read 2026-10-28; no random draw): the node's thing as a `GameThingWithPos` (the pile its score
    chose) and the thing above its parent as one, or, when that is none, the thing above its parent as a
    `BuildingSite` and then its building (`BuildingSite::GetBuilding` 0x43BC70); "No Pile", "No to object", and the
    pile's wood (vt +0x98 with WOOD, unsigned, "No wood in Object") must not be 0, else the action fails (0). Then
    two steps: CreateWoodFromContainer (the pile and **all its wood**, read again) and DropObjectInHandOnObject (the
    thing); done (1). No move of its own: the take's move in front (MoveHandToObject) goes to the pile.
    ⚠️ Under TownWithHighestWoodDesire the thing is the town, and the give onto a town reads a function the town
    does not have: `Pot::ValidToApplyThisToObject` 0x66DD50 calls the target's vt +0x680 (`IsResourceStore`), and
    the Town vtable (0x999AE4) is shorter than that: the dword there, 0x99A164, is 0 (read through the oracle,
    2026-10-28), so the original calls address 0 and stops ([a vanilla bug](../project/progress/vanilla_bugs/engine.md#soft-locks-and-lost-progress)).
    Reachable: no land script changes the weights of TownInteract, AttendTownDesires, TownWithHighestWoodDesire or
    GetWoodFromOtherTown (the 25 personality calls, and no suppression call, of the story's scripts name ExpandInfluence, ReactToAggressiveCreature,
    TakeOverTown, DestroyTown, DefendTown, AttackCreatureWithSpells and DefeatPlayer), and Land 2's gods hold two
    towns with stores (they take food from one for another); openblack's Land 2 trace (2026-10-28) chose the row 383
    times at a root worth of exactly 0.7, which the town path gives with a wood desire at 1, as the food path's 78
    choices at 0.7 show for food, but that trace did not name the parent. Not seen on Land 2: the traced run from the
    land's start with the parent named (4800 turns, 2026-10-28) chose the row 59 times, all under
    GiveBuildingSiteWood, none under TownWithHighestWoodDesire; reachable by the tree, not seen on Land 2. openblack's give asks the town as a store
    (`held_apply`), which it is not: the wood pot stays in the hand. Under the other two the thing is a workshop or a
    building site's building, which take wood. openblack (2026-10-28): `computer_hand::FetchWoodFor` from
    `HandActions::RunAction`, a place as the `GameThingWithPos` (`ThingFacts::HasPlace`, as the scaffold's action),
    `ThingFacts::SiteBuilding` and `ResourceOf`. Tests `ComputerHandActions.AllThePilesWoodIsFetchedForTheThingAbove`,
    `ComputerHandActions.ABuildingSiteAboveGetsTheWoodAtItsBuilding`, `ComputerHandActions.NoPileOrAPileWithNoWoodFails`.
  - **GetFoodFromOtherTownForStoragePit** (row 55, under TownWithHighestFoodDesire, its action 0x667560, read
    2026-10-28; no random draw): the node's thing as an `Object` (the pile its score chose) and the thing above its
    parent as a `Town` ("No Object" each), then where the town takes the pile's resource (vt +0x690;
    `Town::GetResourceDropObject` 0x741EE0: its storage pit, else its temporary pot, made when it has none; "No Place
    To Drop Object"), else the action fails (0). Then three steps: MoveHandToPos (the pile's map place, +0x14),
    CreateFoodFromContainer (the pile, and the node's **own** float +0x80 (the food asked for, the parent's score ×
    1000) through `__ftol`, cut toward zero; not the parent's integer, as GetFoodFromOtherTownForObject takes) and
    DropObjectInHandOnObject (the store or pot); done (1). openblack (2026-10-28): `computer_hand::FetchFoodFor` from
    `HandActions::RunAction`, `ThingFacts::IsTown`, `ResourceTypeOf` and `ResourceDropObject` (the store, else
    `town_stores::GetTemporaryResourceStorePotOrPos` from the town's place). Tests
    `ComputerHandActions.FoodForAStoreIsTheNodesOwnAmountCutToAWholeNumber`,
    `ComputerHandActions.FoodForAStoreNeedsAPileATownAndSomewhereToTakeIt`.
  - **Temporary (openblack's, 2026-10-27):** while only 6 of the 59 actions have an executor, a chosen node whose
    action openblack lacks logs one line naming the god and the node (at info while that mind is traced, else debug), and nothing of the mind changes (no
    failure suppression), where the original would run it; the actions are ported by how often Land 2's gods
    choose them (the traced scene below).
  - Not ported: the list at cp +0x48 swept by 0x6581D0 (what it holds is not read) and the untie of an evil leash
    tied to a town object (0x6589F0); 0x6573D0 is an empty function.
  - **Debug control:** the debug window's "Trace this mind" and the testbed's `TraceComputerMind` (off by default):
    each turn of that mind writes one line, what the hand did, the queued action forced and what the weighing
    chose, and (2026-10-28) the hand as the turn left it: its first step by number and its stage, how many steps it
    has, what it holds and whether a script holds it; and the chosen node by its place in the tree with its parent's row
    (a row met under several parents, as GetWoodFromOtherTown under three, is told apart). The scene `story.land2_computer_minds` switches the traces of Land 2's two gods on where it stands in a
    land timeline; `story.land2_computer_minds_first` (2026-10-28) does only that, to be put first, so that a quest
    run after it logs every turn of the gods' minds from the land's start (a quest run's timeline
    `story.land2_computer_minds_first/quest.worship_site` on `Land2.txt`, `SetupLand2/EnterLand2`).
  - PlayerInteract and ReactToPlayer: all 23 of their rules. `ReactToPlayerThrowingStuff` reads the turn the heart
    keeps of the last blow it took itself from a thing that breaks buildings (`components::CitadelHeart::
    breakingBlowTurn`, set by the heart's strike through `temple_heart::BreakingBlowTurn`), through
    `ScoreWorld::MyCitadelHeart` and `HeartBreakingBlowTurn`; test
    `ComputerMindScores.TheHeartStruckLatelyByAThingThatBreaksBuildingsIsReactedTo`. `DefendtownWithSpells` and
    `CheckTownForProtection` read a town's harm lately (`scores::HighestRecentHarm`) from what each player's last attack
    did, which the town's record keeps (`town_aggression::Record::lastEffects`, the burn, crush and hit, written by
    every `town_emergency::UpdateAggressor`: a destructive effect's numbers, a burn at the thing's temperature, all 0
    for a physical shield's knock), through `ScoreWorld::TownLastAttackEffect`; tests
    `ComputerMindScores.ATownsHarmLatelyIsTheOriginals`, `ATownHarmedLatelyIsShieldedAgainstWhatHarmedIt`,
    `TheTownHarmedMostLatelyIsCheckedForProtection`. Where `DefendtownWithSpells` finds no icon, the original gives the
    node a stack word it never set; openblack's node keeps nothing, as nothing in the original determines the word.
    `ThrowrockatCreature` throws the thing the map cells round the hand make best (`scores::BestThingNear`, the
    original's edges, cell order and influence test, over `ScoreWorld::CellObjects`, `ThrowFactsOf` and `MapCells`),
    worth the influence factor beyond the nearest place (`scores::FactorBeyond`, `ScoreWorld::PlayerSpeed`); tests
    `ComputerMindScores.TheThingsNearAPlaceAreLookedAtCellByCellAsTheOriginalDoes`,
    `ARockOrATreeNearTheHandIsThrownAsTheOriginalChoosesIt`, `ThePointBeyondTheNearestPlaceIsTheOriginals`,
    `ARockIsThrownAtACreatureFarFromTheMindsOwn`.
  - The alliance test (`ScoreWorld::IsAllied`) is the original's `GPlayer::IsAllied` (0x64D5D0: the ally value above
    0), which only `SET_PLAYER_ALLY` (382) sets; openblack's 382 and 398 keep and read it (`ecs::alliances`,
    `ECS/PlayerAlliances.h`), which the game's `ScoreWorld` is to answer through once the mind's turn is wired.
    0), which only `SET_PLAYER_ALLY` (382) sets; openblack keeps the alliances the natives 382 and 398 set and read
    (`components::PlayerAlliances`, `ecs::alliances::SetAlliance` / `Strength`), and `ecs::alliances::IsAllied` (the
    strength above 0) is the answer the game's world will give once the mind's turn is wired.
  - CreatureInteract: all 28 of its rules, with the node's integer and float (`ComputerMindNode::intValue`,
    `floatValue`) and the mind's lesson counts (`ComputerMind::helpLessons`, `destroyLessons`, `impressLessons`, 0 until
    the mind's actions count them). The creature-play rules draw through `ScoreWorld::GameFloatRand`, once each, only
    with the creature. `ScoreWorld::ResourceDropObject` makes the town's temporary pot where the original's does.
  - The aim's own score, `CreatureInteract`, reads whether the creature's body performs an action
    (`creature_body::IsPerformingAction` over `ScoreWorld::CreatureBodyActivity`, `Creature/CreatureBodyActivity.h`).
    openblack has no single body state: the original's states are spread over the parts that move the body, so the
    body is busy when any of them is: its own action plays (`creature_layers::IsPlaying`: the static actions, the
    single ones, a dance, a kiss), it plans a route, turns or walks (`CreatureLocomotionSystemInterface::IsMoving`),
    it plays the animations that act on a thing or catches one (`CreatureObjectActionSystemInterface::GetProgress`,
    `IsCatching`: the pick-ups, knock-downs, throws, kicks, pointing and catches; the walk or turn up to the thing is
    the locomotion's), or its fighter is in a fight state (`creature_fight::State` not `Idle`, the original's
    0x16..0x24). Test `ComputerMindScores.TheCreatureIsLookedAfterOnlyWhileItsBodyIsFree`.
  - By [the body's state turn by turn](#the-bodys-state-turn-by-turn), each part of openblack:
    - the body's own action: start, loop and end follow each other with no stand between, and the stand comes in
      the turn's last animation step, after the mind (`CreatureAnimationSystem::AdvanceTurn`): as the original;
    - walking and turning: one `IsMoving` until it arrives or faces the way. The idle mind runs before the
      locomotion in the creature's turn (`ECS/CreatureLoop.cpp`), so its next step starts the turn after the
      arrival, with a free body at that turn's end: as the original;
    - the fighter: out of `Idle` from the duel's start to the end of its finish, every round back to its stance;
      `Idle` while it waits for its opponent and after the finish: as the original;
    - the object actions: walking or turning up to the thing is the locomotion's; the system runs after the
      locomotion (`ECS/CreatureLoop.cpp`) and looks again at once, so a turn ends and the throw, point or reach
      begins in the same turn, with no free body between: as the original, which turns inside the action's state.
      After a walk up to a thing it differs: the animation begins in the turn of the arrival, where the original
      has a free body at that turn's end and begins it the turn after. That is the object action's own timing,
      not the query's: a computer player would read the body busy one turn early there.
  - `TeachCreatureImpressiveMagic168` weighs where the town's villagers are (`scores::PosWhereVillagersAre`, over
    `scores::VillagersNear`), through `ScoreWorld::DoorPlace`, `CongregationPlace` (the town's own meeting point, which
    `town_queries::GetCongregationPos` works out and keeps, drawing where the original draws), `TownCentre`,
    `TownFields`, `VillagersAt` and `RawDesire`, and the miracles' `ScoreWorld::MagicImpressiveBalance`; tests
    `ComputerMindScores.TheVillagersNearAPlaceAreThoseOfTheNineCellsRoundIt`, `WhereATownsVillagersAreIsTheOriginals`,
    `AnImpressiveMiracleIsWorthAsMuchAsTheTownsVillagersAreLikelyToSeeIt`.
  - `CastSpellOnMyCreatureForALaugh170` casts at the town's building most worth attacking from the hand
    (`scores::BestAttackObject`), through `ScoreWorld::AbodePercentBuilt` (the virtual the original reads is the
    building's percent built), `AbodeAttackDesire`, `FieldAttackDesire`, `TownAbodes`, `TownFields`, `FieldFood`,
    `FieldFoodCapacity` and `Life`, and the miracles' `ScoreWorld::MagicAggressiveAttackValue`; tests
    `ComputerMindScores.ATownsBuildingMostWorthAttackingIsTheOriginals`,
    `AnAttackMiracleGoesWhereTheTownIsMostWorthAttacking`.
  - TownInteract: 20 of its 22 rules. The town's planning (`ScoreWorld::TryToBuildAt`) draws as the original's does,
    into a plan of its own, so `FindLocationToUseScaffold` makes no trial abode: its town would be none and the
    original's trial is on no list, so nothing of it is seen. `ScoreWorld::BuildSite` builds a repaired worship site
    where `ChargeUpSpell` does. Pure pieces: `ClampCellsOntoMap`, `CellIsSteep`, `DesireForWorshippers`,
    `DesireToGiveWorshippers`, `DesireForVillagerAvailableFromTown`. `GetWoodFromForest` reads what the mind has seen
    round its hand (`ComputerMind::seen`, `ComputerMindSeen`: a tree, a seed or a forest by its number, and the turn),
    which `computer_player::ScanAroundHand` fills, `ForgetSeenGone` sweeps and `StartNewLand` empties
    (`ECS/ComputerPlayerSeen.h`, over the `SeenWorld` seam); the mind's turn sweeps what is gone and then looks round
    the hand before it acts (`ComputerPlayerSystem::ProcessTurn`, 967c20f42).
    Tests `ComputerMindSeen.*` (`test/test_computer_mind.cpp`) and
    `ComputerMindScores.WoodIsTakenFromTheBestForestOrTreeTheMindHasSeen`. Two score nothing:
    `GiveWorshippersRest` and `TakeVillagerFromWorshipSiteToTown` (the dance's groups, whose sum and count the average
    life desire reads: openblack keeps one list of dancers).
  - DefeatPlayer: all 13 of its rules, with the players' walks (`NextActivePlayer`, `scores::NextActivePlayerOrNeutral`),
    the town to take (`BestTownToTake`, `DesireToBeTakenOver`) and the double product and sum rounded once
    (`scores::MulRoundedOnce`, `AddRoundedOnce`). `DefeatPlayer37` weighs `scores::DesireToAttack` over what each player
    keeps all game (`components::PlayerRivalry`, kept by the player service: the harm each player's effects did their
    things, written by `effects::ApplyEffect` for the player `effects::HarmedPlayerOf` names (a rock and a dead tree
    keep the player they were made for, `MobileStatic::owner` and `DeadTree::owner`), and the running average of their influence power, moved by every
    `influence::CalculateInfluencePower`, once after a land's script too, `influence::NoteInfluencePowerAfterLoad`),
    through `ScoreWorld::PlayerFixedPlace`, `AverageInfluencePower`, `HarmFrom` and `AlignmentValue`; tests
    `ComputerMindScores.TheDesireToAttackAPlayerIsTheOriginals`, `ThePlayerToDefeatIsTheOneMostWantedByTheAttitude`,
    `TheActivePlayersAreWalkedThenTheNeutralOne`. `ThrowThingsToImpress` weighs where the town's villagers are, its
    boredom with things flying (`ScoreWorld::TownBoredom`, the town's own `TownBelief::boredom` that
    `town_belief::GetBoredomMultiplier` reads) and the factor beyond the nearest place, and throws the most impressive
    thing near the hand that breaks no buildings (`scores::BestThingNear`); test
    `ComputerMindScores.AThingIsThrownToImpressATownWhereItsVillagersWillSeeIt`.
  - **84 of the 86 rules are written.** The two that score nothing are named above, each with what it lacks.

## Pending

- **On unlanded branches**, not landed: on `save/s1r` (tip 9534be79a), the miracles a mind has cast are written with the turn each was cast, a computer hand's state writes its first step's stage, its move
  in front and its wait, and a loaded game's computer hand is reported where the save left it until it next takes a
  turn of steps; on `fix/skirmish` (tip 439647ddc), a god with no temple is out of a skirmish and its mind stops
  ([the skirmish's rows](../project/progress/multiplayer/skirmish.md)).
- The actions of the mind: 6 of the 59 have an executor; a chosen node whose action openblack lacks is only told
  (above). The two ways of scoring that score nothing (named above) still refuse a weighing under them.
