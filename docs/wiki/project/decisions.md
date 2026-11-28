# Decisions

The dated history of the project's working rules: who decided, when, in what words, and what was later changed or
withdrawn. The rules themselves live in the working-rules file kept with the local tree; this page only records how they came about. The chosen
differences from the original keep their dates in
[reconstruction.md](reconstruction.md#what-is-deliberately-different-chosen-differences) and are not repeated here.

Newest first. "Status" is current, superseded (with the date of what replaced it) or withdrawn.

| Date | Decision | The user's words (where recorded) | Where the rule lives now | Status |
|---|---|---|---|---|
| 2026-11-28 | The original that counts draws with specular on (a normal card), as openblack does; the reference runs made in Voodoo mode lack the specular glows, so shots from them are marked "no specular" and the glow rows close by reading the code | "decide tú conforme a las normas" (the user delegated it) | CLAUDE.md Part 2 (the original) | current |
| 2026-11-28 | A debug switch, off by default, lets a check run write the vortex crossing file into its own profile folder only | (delegated, as above) | the vortex page; CLAUDE.md Part 1 (debug controls) | current |
| 2026-11-28 | Tool files a lane drafts (quest specs, recipes) are installed into tools\ by the coordinator after review | (delegated, as above) | CLAUDE.md Part 4 (one copy of everything) | current |
| 2026-11-28 | Stale references are promoted by the next train that runs their scenes | (delegated, as above) | CLAUDE.md Part 3 (a joint) | current |
| 2026-11-26 | Unfinished findings must not be lost at a runs wipe: evidence a lane still needs is moved into `refs\` before `runs\` or `_logs\` is wiped | | CLAUDE.md Part 4, new base | current |
| 2026-11-26 | Long-lived shared files (rows done log, questions for the user, phase plans, state notes) move out of one session's scratch into a shared `coordination\` folder; a session writes only its own scratch | | CLAUDE.md Where everything is; Part 4, Communication | current |
| 2026-11-25 | The game's memory need at the gate lowered from 7 GB to 6 GB per build | "Lower the limit to 6" | CLAUDE.md Part 4, Machine limits | current |
| 2026-11-25 | A detail level chosen in Options is kept between runs, as the original's; level 6 is the start only when none is kept | | CLAUDE.md Part 2, chosen differences | current (amends 2026-10-21) |
| 2026-11-24 | The coordinator runs the landing itself (land script, deploy, docs check, publish, tree release); train agents stop after the games | | CLAUDE.md Part 4, Landing | current |
| 2026-11-24 | Leftover processes are killed only by PID after their command line shows one's own session or clone | | CLAUDE.md Part 4, Machine limits | current |
| 2026-11-24 | Shared tools are changed atomically: write a copy, check it, tell the lanes, then move it over the old one (an in-place edit broke a running check) | | CLAUDE.md Part 4, One copy | current; supersedes "a tool is changed in place, between joints" (2026-10-22) |
| 2026-11-22 | Testbed, quest-run and story-chain logs close rows; an in-game run from a library save is preferred where one reaches it | | CLAUDE.md Part 3, Testbed logs | current |
| 2026-11-22 | Library saves are not replaced by repaired copies until the user allows it | | CLAUDE.md Part 3, Saved games first | open question for the user |
| 2026-11-21 | Every 30 minutes the coordinator gives each idle lane a task and keeps its own agents working | | CLAUDE.md Part 4, Communication | current |
| 2026-11-21 | A car report says which of its new tests ran, or "test not run" (quickcheck only compiles) | | CLAUDE.md Part 3, Test less | current |
| 2026-11-20 | Trains run tests first, then the games; a failing car goes back to its lane | | CLAUDE.md Part 4, trains | current |
| 2026-11-20 | Game checks start from a saved game in the natural saves library; playing to close rows is preferred, and a single-row game is fine | | CLAUDE.md Part 3, Saved games first | current |
| 2026-11-20 | Trains run 4-6 relevant scenes per area the cars touch, never dozens | | CLAUDE.md Part 4, trains | current |
| 2026-11-20 | A full GitHub update every 4 to 6 hours | | CLAUDE.md Part 4, GitHub | current |
| 2026-11-20 | A train's amends and in-place fixes go out under a new branch name; a `train/` branch is never force-pushed (trains pin hashes) | | CLAUDE.md Part 4, trains | current |
| 2026-11-19 | Write more, read less: about 20 minutes of reading per row, then the port (bw1-decomp's C taken whole); fewer, bigger cars | | CLAUDE.md Part 4, What the work is | current |
| 2026-11-18 | Test less, push sooner: a code car goes to its train after quickcheck and precheck, with no lane build or test first; one game per behaviour; about 3 aiming tries | | CLAUDE.md Part 3, Test less | current |
| 2026-11-18 | Regression guard: every code train runs the 6 hand demos and the guard list; a fix for a user-reported behaviour adds its id there | "estaba solucionado y lo habéis vuelto a romper" (it was fixed and you have broken it again) | CLAUDE.md Part 3, A joint | current |
| 2026-11-18 | A row's status is never raised by judgement; it moves only when a clause closes | | CLAUDE.md Part 3, Row status | current (first set 2026-10-26, restated 2026-11-02) |
| 2026-11-17 | Gate: no memory reserve; at most 3 games with no build running, 1 while a build runs; a waiting build holds back new games | | CLAUDE.md Part 4, Machine limits | current (the reserve was 2 GB until 2026-11-15, then 1 GB; the game count was 2) |
| 2026-11-16 | Only the coordinator, after a landing, moves `local/next` with a lease push onto the landed tip plus what was pushed meanwhile | | CLAUDE.md Part 4, local/next | current |
| 2026-11-15 | One lane's changes that move every reference in the same area may ride one train together and be measured once | | CLAUDE.md Part 4, trains | current |
| 2026-11-15 | Reaped runs include commands Claude Code itself stops for low memory | | CLAUDE.md Part 4, Machine limits | current |
| 2026-11-14 | One build tree for every session; the per-lane test trees are removed | | CLAUDE.md Part 4, One build tree | current |
| 2026-11-14 | Joints run as trains: lanes push ready branches, the coordinator stacks and measures them about every 90 minutes | | CLAUDE.md Part 4, trains | current |
| 2026-11-14 | Publish to `AI-Decompiled` after every clean landed train, about every 90 minutes | "actualiza github con los commits de cada 90 cuando sea posible" (update GitHub with the commits every 90 minutes when possible) | CLAUDE.md Part 4, GitHub | current; supersedes the 2026-10-27 coherent-point rule |
| 2026-11-14 | FULL runs are not used for a change that moves every reference either | "nada de todas las escenas, si no no acabaríamos nunca" (no running every scene, or we would never finish) | CLAUDE.md Part 3, A joint | current (extends 2026-11-03) |
| 2026-11-14 | The coordinator closes a fork issue alone once every item is proven done, with an evidence comment first | | CLAUDE.md Part 4, GitHub | current |
| 2026-11-14 | Lanes ask for tools; the coordinator builds them in `tools\` | | CLAUDE.md Part 3, Tools | current |
| 2026-11-14 | Agent cap per lane after lane workflows exhausted the commit charge | | CLAUDE.md Part 4 keeps 2 helpers per lane and 8 GB of commit left; a brief once said 3 agents and 9 GB | resolved to CLAUDE.md's number |
| 2026-11-11 | Sources in order: raffclar's code first, then bw1-decomp's C taken whole, the Ghidra export last; never a piece from scratch | | CLAUDE.md Part 3, Tools | current |
| 2026-11-10 | Save work: one measurement per writer or loader batch, the round trip only at the batch's end | | CLAUDE.md Part 3, What runs when | current |
| 2026-11-09 | Builds adapt their compile jobs to the available memory | "adapt to the available memory" | CLAUDE.md Part 4, Machine limits | current |
| 2026-11-07 | Work goes by gain: the cheapest closures first, in any family, counted in rows per hour; only row-closing work | "too slow for too little" | CLAUDE.md Part 4, What the work is | current (restated 2026-11-13) |
| 2026-11-07 | A shared delivery branch `local/next` holds what is measured and the docs/tests-only commits | | CLAUDE.md Part 4, local/next | current |
| 2026-11-04 | A tool before reading by hand; one owner per file or region | | CLAUDE.md Part 3, Tools; Part 4 | current |
| 2026-11-03 | Run only the checks known to be needed | "solo lo que sabemos que hay que testear" (only what we know has to be tested) | CLAUDE.md Part 3, What runs when | current |
| 2026-11-03 | FULL only when the user asks, not at a new base | | CLAUDE.md Part 3, A joint | current |
| 2026-11-03 | A run the memory watchdog stops is restarted without asking once there is room | | CLAUDE.md Part 4, Machine limits | current |
| 2026-11-03 | CLion's tools before builds; raffclar's and upstream's code checked before writing any piece | | CLAUDE.md Part 3, Tools | current (restated 2026-11-20) |
| 2026-11-02 | Withdrawn: the 2026-10-28 chosen difference that let game logic read a per-turn placement. Villagers', animals' and the creature's logic read the placement the last drawn frame left, as the original | "hazlo como el original todo" (do it all as the original) | CLAUDE.md Part 2 | withdrawn 2026-10-28 rule |
| 2026-10-30 | Check reduction restated | | CLAUDE.md Part 3, What runs when | current |
| 2026-10-29 | Breadth first: a playable framework first, the story played through the lands | | CLAUDE.md Part 4, What the work is | current |
| 2026-10-29 | Testbed scenes proving player actions go through the pinned mouse and presses | | CLAUDE.md Part 1, Testing | current |
| 2026-10-28 | Lands before the creature; the creature's pieces from raffclar's tree | | CLAUDE.md Part 4, What the work is | current (restated 2026-10-30, 2026-11-03) |
| 2026-10-28 | Game logic reads a per-turn placement (a chosen difference) | | | withdrawn 2026-11-02 |
| 2026-10-27 | Haptics are ignored; force-feedback rows get the `device` status | | CLAUDE.md Part 4, What the work is | current |
| 2026-10-27 | At each landing the coordinator asks each lane what it spent most time on and what tool would remove it | | CLAUDE.md Part 3, At landing | current |
| 2026-10-27 | A tool before reading by hand | | CLAUDE.md Part 3, Tools | current |
| 2026-10-27 | Report every step to the coordinator | | CLAUDE.md Part 4, Communication | current |
| 2026-10-27 | Publish to GitHub at coherent points | | | superseded 2026-11-14 |
| 2026-10-26 | Reports follow a fixed labelled format | | CLAUDE.md Part 4, Communication | current |
| 2026-10-25 | Run the fewest checks that show the change | | CLAUDE.md Part 3, What runs when | current |
| 2026-10-24 | Online is parked until the end | | CLAUDE.md Part 4, What the work is | current |
| 2026-10-23 | The build is C++23, as upstream's and raffclar's | | CLAUDE.md Part 1 | current |
| 2026-10-23 | raffclar's four-function start-up split may move services, each move measured | | CLAUDE.md Part 1, Service lifetime | current |
| 2026-10-22 | One copy of everything: tools once in `tools\`, references once in `refs\` | | CLAUDE.md Part 4, One copy | current (its "changed in place" clause superseded 2026-11-24) |
| 2026-10-19 | Paging to disk is fine, but reaching the commit limit is not: every start keeps its need below it | | CLAUDE.md Part 4, Machine limits | current |
