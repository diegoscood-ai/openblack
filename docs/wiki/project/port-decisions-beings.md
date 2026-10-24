# Port decisions: villagers, towns and animals

## Summary

How each part of the beings' code was brought onto raffclar's architecture: where his version was taken (shown to be
the original's, bit for bit), where ours stayed behind his names and interfaces, and where his reading of the game
differs from the exe and the original is followed. The behaviour itself is on the beings pages; the conventions these
decisions follow are on [Refactor conventions](refactor-conventions.md).

## The original

No facts of the original live here: each decision points to the page that holds the behaviour it keeps.

## openblack

The decisions, by area. "His" is raffclar's tree (`raffclar/openblack`, the port target at `0222f9f5` and the newer
pieces taken where they close a row or fix a difference); "ours" is the local tree.

### Clips and animation

- **A villager's clips, ours behind his names:** his pure `villager_animation` (`ECS/VillagerAnimation`) says how each
  state chooses its clip (`ClipChoiceOf`, `TransitionOf`; they replace our generated table and give the same choice
  for all 255 states, which `test_villager_animation` pins) and holds the clip rules, with our bodies. His
  `villager_animate` (`Implementations/VillagerAnimate`) applies them to a villager over our clip component
  (`StateClip`, `SetAnim`, `SetStateAnim`, `CurrentClip`, `IsHiddenByState`); the clips of a change of state stay
  ours in `ECS/VillagerAnimations`. Where his rules are the original's and ours were not (the gait by the whole speed,
  the hurt clips skipped for a villager a script moves, the poisoned pause, a large object looked at by the turns left,
  a flying object looked up at, a near creature looked at, DOWNED among the death states), each was made the
  original's in a commit of its own (2026-10-14; the near creature 2026-10-20), pinned in `test_villager_animation`,
  where his case for the flying object came as he wrote it. Not taken: his stubs for pointing at a flying
  object and for the thrown, landed, dying and dead clips (ours are the original's), and his builder's clip chosen
  from the working clip that plays (ours, from the into clip that plays, is the one the wiki reads).

- **A walking villager's clip by the ground covered:** his `MovingPlay` and `AdvanceClip` (his `animals` names) are
  taken as `villager_clips::MovingPlay` and `AdvanceClip` (`ECS/VillagerClips`), with `PlaysByGround` for the gate,
  fed by our exact `WholeSpeed`. His structure is the original's; not taken: his speed rounded from metres with a
  half added (one unit high), his double-precision steps (the game rounds each to single precision) and the missing
  replay of a held one-shot clip's sound events. Outside the moving states his villager clip plays by the frame's
  whole milliseconds through the same `AdvanceClip`, as the original's does: taken, with the choice between the two
  in a pure `villager_clips::FramePlay`. Animals keep our float advance until their own commit.

- **A script's clip on a villager:** the clip and the plays SET_SCRIPT_ULONG asks for are his
  `components::ScriptAnimation` (`clip`, `playsLeft`), set without touching the draw lists, in place of our two
  `Villager` fields; a villager without one plays clip 0 and has no plays left, as the fields' zeros did.

- **Foot blobs' bones:** his `ground_blobs::FootBones` (the drawn pose when it fits the model, else the resting
  bones) is the villagers' blob choice in the renderer, which picked the same bones inline; his two cases of it come
  with it. The animals' blobs keep their own choice.

### Speed and walking

- **A state's speed scaled by the town's player:** his pure `villager_speed` core (`ECS/VillagerSpeed`, `Inputs`,
  `StateSpeed`) and ours are the same, with his names. Its feeding in his `villager_reactions::SetStateSpeed` is not
  taken: it reads the town's belief in the neutral player from its belief slot, where the original reads the town's
  own neutral belief (`TownBelief::beliefInNeutralPlayer`, 0.5 from the start); it hard-codes the Indian tribal power
  to 1 instead of reading it from the player; and it gives a neutral town's villager no power term, where the original
  gives it the neutral player's. Ours feeds the core from the town's owner through the pure
  `villager_speed::PlayerTermsOf`.

- **Villager speed:** his pure `ecs::villager_speed` (`StateSpeed`, `PersonalFactor`, `FinalSpeed`, in
  `ECS/VillagerSpeed`) and `test_villager_speed` are taken, term for term the same maths as ours. Our entity layer
  (`SetVillagerStateSpeed`, `SetVillagerSpeed`) gathers the inputs and calls them, the player's tribal power and the
  town's belief included since 2026-10-15 (`villager_speed::PlayerTermsOf`, the entry above). Ours stays in one
  place: a carrying capacity or a town needs divisor of 0, which only made-up villagers have, gives no term instead of
  a division by 0.

- **The end of a walk:** his pure `walk_arrival::WalkIsOver` (`ECS/WalkArrival`) is what tells our walk to a
  position that it is over (the final step or the arrival, on the goal), on our walk's positions in metres where his
  are in whole map units. His `WithinAStep` is not taken yet: our three copies of the original's "are we there" test
  differ from his and from the original, and one exact test is to replace them all as a change of behaviour.

- **Trying both ways round (the walk):** his pure rules for it are taken into `ECS/WallHugRules` with his names
  (`TurnsToSpare`, `TurnedRoundIsBetter`, `HeadingRound`, `k_TrialSpeedUp`, `k_TrialWalkerAge`), but for his
  `k_TrialTurns`, which is `k_TrialWalkTurns` there since the walkers' shared bookkeeping (`ECS/WallHugBookkeeping`, in
  the same namespace) took that name. His `TurnRound` is taken with the step's height part added, because the
  original turns the step round with its along and height parts swapped. His version drops that part, so a walker
  turned round twice before its step is aimed again would lose its along part. His wiring (`WallHugLookahead`,
  `ProcessLookahead`, the stand-in's `TrialWalk`) waits for the villagers' walk as the original's, which it runs on.
  His "never turned round into a ring of circles that shuts them in" is not taken: it is a deviation from the
  original, which turns the walker round wherever the jump lands it.

- **The land line, his names over our cell walk:** his `3D/LandLine.h` (`land_line::HitInCell`, `FirstHit`,
  `LandAlong`, `LandOrSeaAlong`, `UnderPixel`, `KeptInReach`, `CellHeights`, `CellLookup` and his constants) is
  served by our walk, moved out of `LandIsland.cpp` with its arithmetic unchanged; `LandIslandInterface::RayCast`
  and `RayCastCells` are thin wrappers over it. Ours differs from his signatures where the game or our maps need it:
  16-bit heights, a trailing cells-per-side argument (512 by default), the float tenth from metres to cells and the
  widened float 0.0001 as the flat threshold, both read from the exe
  ([the land line](../world/terrain.md#the-land-line)), our sea crossing (`SeaLevelAlong`, which `RayCast` still writes
  when it is too far) and our step guard. `land_line::CellsOf(island)` reads an island's cells. `KeptInReach` is his:
  the picking slot's `LandUnderPixel` keeps its point within reach through it, as the interface's land under the cursor
  (2026-10-13). His 11 LandLine tests are in `test_picking` beside ours in `test_land_raycast`.

### The villager's life and states

- **A villager's age, ours behind his names:** his pure `villager_age` (`ECS/VillagerAge`) holds the age rules: the
  years from the birth turn (`AgeOf`, `BirthTurnFor`, `AgeNow`), the age given (`GivenAge`, `IsChildAge`, `SetAge`),
  the sizes (`StartScale`, `GrownScale`), the growth turns, the old age's check turn and its death (`DiesOfOldAge`),
  with the game's draws through a random function. Our villager's side (`ECS/Villager/VillagerAge`: growing up, the
  pregnancy, the scale on the villager) stays where it is and calls them; the two files of that name are kept, his for
  the rules and ours for the villager. The bodies are ours where his differ, as the original: a child starts at the
  size of the age before its own (at age 0 the value just before the table, which `StartScale` takes as its own
  argument so that it stays pure) and grows towards the size of the age after it, where his reads its own age's; an
  adult's size is `(0.05 - r) + 1`, not his `1.05 - r` (a different float for most draws). His cases come into
  `test_villager_age` with those numbers, compared exactly. Our villager's constructor makes its draws through his
  `MakeNewborn` (the size, the food, the last check and the wait before the first decision, in the game's order),
  still after the special villager's draw, which his archetype skips and ours keeps. The villager's birth turn is his
  unsigned field (`Villager::birthTurn`), and the debug villager panel and the inspector show and set the age, life
  and food through his names (`AgeNow`, `SetBirthTurnForAge`).

- **A villager's own needs, ours behind his names:** his pure `villager_needs` (`ECS/VillagerNeeds`) holds how much a
  villager wants food and rest (`DesireForFood`, our `Power`, which is removed; `LifeDesireFromLife`, `IsHungry`), how
  strongly they press it (`OwnDesiresTrigger`), which it sees to first (`SatisfyOwnDesire`) and whether it pauses for
  a second (`PausesForASecond`). Our villager's functions read the villager and call them. The bodies keep our
  comparisons where his use `std::min` and `std::max`: the same numbers, written as the game's own compares, so that
  a value that is not a number goes the game's way (its own commit, with tests). His `test_villager_needs` comes as
  he wrote it.

- **A villager at home and woken, his fields:** our flag bits for being inside the home and having gone to bed in
  that stay are his `components::AtHome` (`ECS/Components/AtHome.h`, with `beenToBed`), and the bit for a knock on the
  home is his `Villager::woken`. A villager gains `AtHome` as it goes in (keeping its bed mark when it already is in)
  and loses it as it leaves; nothing else changes but the component storages. His uses of `AtHome` in the renderer,
  the hand, the physics and the miracles belong to their own rows.

- **A villager's day, ours behind his names:** his pure `villager_routine` (`ECS/VillagerRoutine`) holds the rules of
  the day around the home, with his names and constants: the spot out of the door (`PosOutsideDoor`) and about a
  building (`PosOutside`), the seat about town (`ChillOutPos`), the idle choice (`NothingToDo`), the bed after a knock
  (`MaySleep`), the sleeper's check (`CheckSleep`), the homeless and vagrants' spots (`TowardsTown`, `RoundAbout`,
  `VagrantWander`, `VagrantTentSearch`, `BrokenHomeTentSearch`, `TentRetry`), the emergency's length
  (`IsInStateOfEmergency`) and who answers it (`AnswersTownEmergency`). Our villager's functions read the villager
  and the town and call them, with the game's draws through a random function. The bodies are ours: positions in
  whole map units, where his are in metres (a round trip through metres is not shown to be exact), so `Polar` adds
  the step made whole on its own to the point, where his adds in metres and then makes the sum whole; `CheckSleep`
  gives the whole restore, which the life's own increase stops at full, where his clamps the gain himself (his gain
  and the life after it can be a float step off), and says whether it gives one at all (`heals`);
  `AnswersTownEmergency` reads the game's table and takes the previous state, as the row of moving about a fire
  answers only with one, where his list leaves that row out. His `test_villager_routine` comes with positions made
  from metres and the restore expected whole, and its emergency case also pins the fire row.

- **The living interface's villager methods:** his `LivingActionSystemInterface` gets the state changes the villager
  code asks of it (`VillagerSetCurrentAndDestinationState`, `VillagerPlayAnimThenSetState`,
  `VillagerSetTopStateToFinal`, `VillagerIsReadyForNewAnimation`) and the ten the scripts use (`VillagerCanBeDirected`,
  `VillagerSetScriptState`, `VillagerScriptMoveTo`, `VillagerSetScriptAnimation`, `VillagerHasPlayedScriptAnimation`,
  `VillagerFace`, `VillagerYAngle`, `VillagerSetYAngle`, `VillagerOverrideAnimation`, `VillagerSetAge`). Ours forwards
  each to our code; the first keeps our codes (1 when it went, else the refusal's), where his `bool` loses which
  refused. The natives SET_SCRIPT_ULONG, SET_PROPERTY's age, MOVE_GAME_THING, PLAYED and OVERRIDE_STATE_ANIMATION call
  them; SET_FOCUS keeps `living::SetFocus`, which turns villagers and animals alike, and the villager code keeps calling
  ours. The tests' fake state tables share a base (`test/support/LivingActionFakes.h`) whose script methods throw.

- **The villager's state functions stay ours, under our names:** his `villager_home`
  (`Implementations/VillagerHome`) takes the villager's routine state functions (going home, sleeping, the idle
  choice, the walks to a spot, the knock on the home and the like) out of the villager into a system of his. That
  split is not taken (decision D-L2 of the villager port): the port stops at his pure modules (`villager_age`,
  `villager_needs`, `villager_routine`, `villager_clips`, `villager_animation`, `villager_script_rules`) and the
  forwards of his `LivingActionSystemInterface` (the entry above), and the remaining villager rows keep our function
  names (`ecs::villager`, `ECS/Villager/`) behind that interface, calling ours where his code calls `villager_home`.
  The whole split is a candidate for the catch-up phase after his build `0222f9f5`, not an open port row.

### Towns, homes and stores

- **His town and living interfaces over ours:** his `TownSystemInterface` gets the seven methods of his villager
  routine's commit (`CheckForClearArea`, `FindClearArea` with his `ClearAreaFilter`, `GetCongregationPos`,
  `BuildingCreated`, `IsInStateOfEmergency`, `SetInStateOfEmergency`, `ProcessTurn`), each a forward to our town
  code (`town_queries`, `town_emergency`, `plans::CheckWhenNewBuildingCreated`) in his metres on the ground plane,
  where ours works in whole map units. Our code keeps calling ours in map units: a round trip through metres is not
  shown to be exact. His `ProcessTurn` is not called from the game's turn: each town's own turn runs its emergency
  (its step 15), as the original does. `Town::emergencyStartTurn` is his `emergencyTurn` (it is written at every
  call, so it is the latest call's turn). His `LivingActionSystemInterface` gets `VillagerGetFinalState` (our
  `GetFinalState`), and the entry and exit calls take his names, `VillagerCallEntryState` and
  `VillagerCallExitState`, with our row argument and the original's codes: his `bool`, the other way round for the
  exit, cannot carry the entry's 0x23 (accepted, the states set by it). His `Update` / `UpdatePoses` villager loop is
  not taken: our one living list (`living_turn`) stays.

- **His town's home methods over ours:** his `TownSystemInterface` also gets his five home methods, each a forward
  to ours: `FindAbodeWithSpace` (`town_villagers::FindAbodeWithSpaceInTown`), `FindClosestTown`
  (`map_cells::GetNearestTown` at any distance), `AddVillagerToTown` (`town_villagers::AddVillagerToTown`),
  `AddVillagerToAbode` (`abode_villagers::AddVillagerToAbode`) and `MakeHomeless`, which is our
  `villager::MakeHomelessNoStateChange`: his method sets no state, where our `villager::MakeHomeless` also sets the
  homeless state. Our two old methods, the first building with room and the head insert, had no caller and are gone.
  Our code keeps calling ours directly, as for the seven methods above. His pure `TownHomes`, its `test_town_homes`
  and his villager archetype's home assignment are not taken: his rules count the adults, children and sexes from
  the building's list of inhabitants and walk the town's buildings in his order, where ours uses the counts the town
  and the building keep and the town's newest-first order, as the original does.

- **His store service over ours:** his `ResourceStoreSystemInterface` comes as he wrote it, in the
  `resourceStoreSystem` slot, emplaced with the land and reset with the town system. Its `ResourceStoreSystem` is
  ours: each of the eight methods forwards to our store code (`object_delivery::ResourceOf`,
  `resource_stores::IsResourceStore`, `object_resources::AddResource` / `RemoveResource` / `IsPoisoned`,
  `pot_resource::AddToPotDirect` / `AddResourceToPos`, `resource_stores::DeleteObjectAndTakeResource`,
  `resource_stores::StructureOfPile`). His giver or taker, a `std::optional<PlayerNames>`, becomes our interface
  through `pot_resource::DropperOf`: a player is that player's interface, my interface only for the local player
  (then with the hand's record of its last pick-up; another player's keeps none), none is no interface. It replaces our two copies of that, the thrower's (always player one) and the pour's (the player,
  only when human), and `PourAt` names the player only when it is human, as the pour did. His own store bodies (the
  pit's filling, the removal's count, his mulch counter) are not taken: ours are the original's, read from the exe.
  The miracles' pour (`GameMagicWorld::AddResource`, his `PourAt`) and the physics' thrown fence and thrown animal
  (his `IsStore` and `TakeObject`, the thrower being the player the physics record keeps for the throw,
  `throwerPlayer`, since 2026-10-20) go through the slot. The hand's own sites stay on ours: `held_apply` is the held
  class's rules, tested directly with its dropper given, and the hand's
  thrown pot gives the hand's own interface status, which the slot's giver only rebuilds through the locator's hand.

- **His store rules, our formulas:** his pure `store_rules` (`ECS/StoreRules`) holds the worth of what stores take,
  under his names, with our bodies pulled out unchanged: `TreeWood` / `TreeWoodValue` (the same order as his),
  `DeadTreeWood` (the original's order, the wood value by the multiplier and then the scale, where his takes the
  scale first, which is not bit-exact in general), `FenceWood` and `AnimalFood` (ours were `resource_stores`'s, the
  same products), `AmountOverMaximum` and `AskedOfPile` (the storage pit's pile split, out of
  `object_resources::RemoveResource` and `StoragePitStore::AmountOverMaximum`), and `LastTakenModifier`, which our
  town turns into his `std::optional` from its 0 for never (the original's layout: his `ResourceLastTaken` component
  is not taken). Its body is ours: the unsigned turn difference, with no case of its own for no turns to forget. His
  `BigForestWorth` / `BigForestWood` are not taken: our big forest keeps its wood value by its scale untruncated,
  which is the forests' row to read in the exe. His `test_store_rules` comes without those two, with our former
  value tests added; its values hold in the original's order too.

- **Town desires:** his `townDesireSystem` slot and `TownDesireSystemInterface` front our `town_desire` code, and the
  desire data (`TownDesire`, `TownStats`, `DesireSort`, `k_TownDesireCount`) is in his `Components/TownDesire.h`.
  The data stays inside `Town`, which includes that header, and each town still works out its desires in its own
  process, after its influence as in the original. His `ProcessTurn` is left out: his pass runs outside the town
  process, and his inputs would leave half the desires at 0 and lose the warnings and the amounts.

- **Town aggression:** his `ECS/TownAggression` rules (`Record`, `Attacked`, `ProcessTurn`, `AggressionFromDamage`)
  are his text, every constant checked against the exe, with his test; `SecondsSinceAttacked` is added beside them for
  `GET_TIME_SINCE_OBJECT_ATTACKED`, so the native only pops and pushes. His `TownAggression` component is not taken:
  the record is a field of our `Town` component, which already held the last aggressor and the wants of protection
  and mercy (now the record's), so no new storage appears in the registry. His `TownDesireSystem` turn and his
  `world_objects::AttackTown` are not taken either: the fade runs in our town process at the original's step 13,
  and the attacks reach the record through our `town_emergency::UpdateAggressor` and `AttackTown`, called where the
  original calls it (a destructive effect, burning, a Living's physics impact, a physical shield struck). Unlike his
  `AttackTown`, an attack with no harm still counts, as in the original.

- **Compassion for a town:** his `Creature/CreatureTownCompassion` rules and their state in the mind component are his
  text, with the original's edges where they differ (the list's stop on the raw desire and its boosts, the signed
  injured count, the heal share strictly above the mark, the timer restarted when a town plan is taken up). His
  `Plan::about` and `TownImpression` are ours under his names: `Plan::activityObject` and the town's `TownBelief` did
  the same job. A town's goal and act-on members come from the world (`mind_detail::TownHasMembers`, `TownMembers`),
  as the rest of his town branch does, until the mind keeps beliefs as the original's.

- **Abode knock:** his `AbodeKnockSystemInterface`, `abodeKnockSystem` slot and `ecs::abode_knock` names over our
  abode tap. The tap stays ours behind his slot: it reaches the abode with the next turn's tap packet (`ApplyTap`), as
  the original's packet 0x20 does, where his taps at once; and the hand plays the original's `Ctap_house` clip, where
  his holds a pose of his own ([tapping objects](../hand/grab-hold-and-throw.md#tapping-objects)). The read-out takes his
  `Knock`, `Tick`, `ReadoutScale` and `Readout` names with the original's float products (his works the scale out in
  double, a step down at five times), on game time where his runs on real time, and only over the buildings the
  object list draws; the rows are laid out in the frame (`OverlayFrame::knockReadout`, `knockRows`) for the renderer,
  which draws each one in the object pass right after its building, as the original's `Abode::Draw` does.

- **The broken buildings, ours behind his names:** our damaged building's mesh is his `physics::damage::Mesh` in
  `Physics/DamageMesh.*`, a class with our members and our code (his field names: `Corner::position`, `corners`,
  `neighbours`, `countedClass`; his `LinkNeighbours`, with our `Impact`, `SplitUnconnectedGroups` and draw functions,
  which do more than his free functions). The buildings' code is his `BuildingDamageSystemInterface` and
  `Implementations/BuildingDamageSystem` in his slot `Locator::buildingDamageSystem`, emplaced right after
  `dynamicsSystem` and reset right after it; it holds no state (the damage is in the `BuildingDamage` and `Fragment`
  components). His `ReactToImpact` and `PieceAtRest` keep our signatures (`PieceAtRest` is the fragment's whole end
  of physics, called by the class hooks with the matrix it sticks through); ours only (`DestroyFragment`, `Redraw`,
  `RemoveDamage`, the listener and the draw snapshot) sit next to them. The partly built draw's rules are his
  `DrawShare` and `PartialBuildOf` with our arithmetic (and our `RepairedShare`); the geometry of the cut stays in
  `ECS/Physics/PartialBuild`. The draw order of a break (its random draws, the first vertex, the sliver test) is still
  ours until its own measured commit.

- **The football as a town's plaything:** his `components::Ball`, `BallArchetype::Create` and `Town::playthings`
  are taken with his names, the archetype on our conventions (the creation number, the reaction through
  `effects::reactions::CreateReaction`, the ball in its map cell as a mobile object's class). His
  `town_playthings::Add` is taken; the script's CREATE case is `BallArchetype::CreateForNearestTown`, which finds the
  town with our `map_cells::GetNearestTownToPos` rather than his `town_playthings::Nearest`: his orders each player's
  towns by id, where the original walks each player's list in the order the towns joined it
  (`Town::ownerListStamp`), which ours follows. His `Nearest` and its test stay, unused.

### Animals

- **Animal service:** his `Locator::animalSystem` slot and `AnimalSystemInterface` name front our animals' shared
  state (`ecs::animal_ai` still runs each animal's turn in the living list). Of his methods it has the ones our
  components answer as they are: `SetScale`, `RadiusOf`, `LeaderOf`, `MembersOf` (leader first, our flock order),
  `GoalOf`, `GoalHeightOf`, and his `IsFrighteningToCreature` and `CanPlayerPickUp` as he wrote them. The hand keeps
  `animal_ai::ValidForPlaceInHand`. His turn, drawing, reset, creation, sending, fading and killing methods are left
  out: ours do those inside the animal AI, in another order.

### Dances, the intro and the family in high detail

- **Dances:** his `components/dance` reader (`DanceFile`, `DanceResult`), his `Dance`, `DanceGroup` and `Dancer`
  components, his pure `dance_rules`, `dance_shapes`, `dance_moves` and `dances` modules, his `DanceSystemInterface`
  and `DanceSystem` (slot `danceSystem`, its turn at step 4 of the game turn, in `Magic/MagicLoop.cpp`) and his
  `DanceFileLoader` cache, under his names. Where they differ from the original they were made to read as the game
  does, with no other change to his structures: the reader refuses nothing (a short file keeps what it read, bytes
  after the end are left) and orders the key frames by time as it reads them; a file that can't be opened still
  makes a dance (one group, no key frames); a dance's defaults are the game's (loop length 5, round length 100); a key
  frame naming a group past the last makes one group; the first dancer looked for by the scripts is in the groups
  the file names only. The scripts' container commands on a dance are ours (`ECS/ScriptContainers.cpp`), beside the
  town's and the flock's, rather than his `CHLApi` branches. His villager side (`VillagerDance`, the dance states,
  the clips) and DANCE_CREATE come with the dances' behaviour, fitted to our walk: `VillagerDance` keeps his name and
  place (`Implementations/VillagerDance.{h,cpp}`), its state functions take MOVE_TO_POS's step as a function (our
  walk), the place and facing are a pure `dances::DancerTurnOf` on the registry, the leaving rule a pure
  `villager_script_rules::ExitDance`. His `Available` (the top state) and `Destroy` (every dancer) follow the
  original (the final state and the unavailable flag; dancers going themselves passed over), and the dances turn in
  their making order (`Dance::made`) rather than the registry's.

- **Intro rules:** his `ECS/IntroRules` names (`ecs::intro_rules`) front the pure part of our opening specials, with
  our float bodies: the spots and constants (`k_BoySpot`, `k_PutDownSpot`, `k_LightWay`, `k_LetGoMs`,
  `k_HoldsUntilMs`, `k_HeldBelowGrip`, `k_LiftGo` and the light's), `PutDownYaw`, `AdvanceClip`, and the light's
  `Make`, `HeadPosition` and `Step` with his `State` names, `way`, `throughEverything` and `cameraAt`. The light's
  hold and fade timer moves into the light, as in his, which is identical: it is always set when the light lands,
  before it is read. `GripPoint` takes his arguments (the posed bones, the bone, the grip point and the model); the
  pose is worked out by the caller, only while the clip has not passed the time the hand lets go. Kept ours: the
  sprites are our `graphics::billboard::Sprite` (the renderer reads them; his `Sprite` has other fields), the
  sprite's size rule, `Start` (the head moved and returned, for the chasing camera), and `k_Sprites` as an `int`.
  His bodies are not taken until his light is shown bit-exact on the Land 1 opening. Of his `test_intro_rules`, the
  three cases our `test_intro_special` did not already check are taken (the light's start, its fall and landing, and
  the grip point); his other seven are covered there.

- **High-detail service:** his `highDetailSystem` slot and `HighDetailSystemInterface`, with his `ECS/HighDetailRules`
  for its `ThingSpecial` names, front our SuperVillagers (`ecs::super_villager`), whose list stays with the registry, so
  `HighDetailSystem` holds nothing. Taken: `Make` and `Release` (SET_HIGH_GRAPHICS_DETAIL on and off), `Order`
  (THING_JC_SPECIAL, every feature number passed on as before, false when the thing is not drawn in high detail) and
  `Update`, which also makes the swim rings, takes the on-screen test and sets the smooth-drawing parameters, as
  before, and `PlaceEyes`, which draws the eyes of those on screen; ours steps them by the frame's game milliseconds
  and reads neither of its arguments, since stepping them by his clock for drawing would move the blinks' random
  draws. The rest of his rules header is not called yet. Not taken: his `HighDetail` component, `VillagerEyes` and
  `RendererVillagerEyes`, a different implementation of the same models and eyes, whose eye lighting is still to be
  measured against the original's.

### Files and shared code

- **File formats in components:** when he has a reader of one of the game's files as a component under
  `components/<name>` (his `Open`/`Write`, his result enum), it is taken as his text once a test shows that it gives
  the same fields as our parse, on the real file and on a synthetic twin. Our parse stays only as that test's
  reference. Our code reads through the component and converts to its own types. It still gets its bytes from the
  resource caches, through his `Open(std::span)`, never his `Open(path)`. Example: `components/gestures`
  (`GestureFile`, `Gestures.jty`), read by `magic::gestures::LoadTemplates` and so by `GestureTemplatesLoader`. As
  before, it ignores bytes after the last record. `test_gesture_file` pins it to the former parse.

- **Taken as his, identical, before anything called them:** his `Common/FixedFormat` (numbers written as the game's C
  library writes "%3.0f"), the floating numbers (`components::FloatingNumber`, `ecs::floating_number`,
  `FloatingNumberArchetype`) and `graphics::partial_build_cap` (the cap over a building's cut walls), with his tests
  `test_floating_number` and `test_partial_build_cap` in the town group. His `fixed_format::Fixed` was then changed in
  his file by a fidelity fix: it rounds as the original's C library does, on the number's 17 significant digits found
  with that library's 80-bit arithmetic, then half up at the precision, and writes a negative zero as 0. His version
  rounded `|value| × 10^precision + 0.5`, which differs on −0.0, 0.49999999999999994, 2.675 and 0.15; the new cases are
  pinned to the exe's output in `test_floating_number`. His tooltip case in `test_floating_number` is
  left out: it tests his `gui::ToolTips`, which ours does elsewhere. The floating numbers have callers since: the
  believers a town gains float up in yellow (2026-10-16), and a sacrifice's prayer power in red (2026-10-19). Of
  `partial_build_cap`, his `Build` and `DrawsAnythingBelow` are still called by nothing; the game reads only what was
  added to it since, ours (2026-10-15): the partly built draw cuts and caps its walls through `CutEdgesOf`,
  `CutShare`, `SegmentList`, `Capped` and `k_CapQuad` (the original's rule), and the renderer gives a partly built
  building no snow through `TakesSnow`. His debug windows `Physics` and `Advisors` are
  his, closed by default, on our dynamics (`AddObject` for his `InitialisePhysics`, a body not at rest for his
  `IsFlying`), and his `help::spirits::AnimName` and `EmotionName` come with them. His `Moon` window is his too,
  closed by default, on our moon entity (`SkySystemInterface::GetMoon`, `components::Moon`): the phase scripts are told
  is read as GET_MOON_PERCENTAGE reads it, and the place and strength are worked out from the sky's hour
  (`moon::Place`), since our component keeps only the phase and the date. His `components::Moon::dateOverride` and
  `sky_frame::MoonDate` come with it: unset unless the window sets it, and when unset the phase is taken from the date
  read, as before.

### Differences of his readings from the original

What his code does differently from the exe, found while porting, and which reading the port keeps.

#### Villagers: reactions

- **A thrown thing**: raffclar's port measures with `glm::distance` of the floats and draws the clip once in the
  setup; the exe's distance and its clip function are taken.
- **The belief sound**: raffclar's voice (his own gap, the hand's distance and a sample chosen there) is not taken:
  the original's guidance path is ours already.
- **Where raffclar's impression differs**, the original is followed: he computes the share from a count of the
  villagers that name the town (the original reads the town's adults and children of its stats); he multiplies the
  factors in another order (a different last bit); he leaves the shield's per-town override out (his reactions carry
  a fixed multiplier from their creation); he keeps the impression in a component of his own instead of the town's
  belief.

#### Villagers: picked up, dropped, hurt and dead

- **Picked up or thrown**: raffclar's tree (0222f9f5, `villager_physics::IntoHand` / `StartFlying`) goes through the
  whole change of state too, with StorePreviousState.
- **The return after landing**: raffclar's `villager_physics::Land` has the same rule without the special test.
- **The injured count**: raffclar's tree decrements only while above 0; ours decrements with no guard, as the exe.

### Test hooks

None: the decisions use the hooks of the pages they point to.

## Pending

- Each "not taken" or "waits for" above is a row of the port audit; its status is in the progress pages
  ([progress](progress/)).
