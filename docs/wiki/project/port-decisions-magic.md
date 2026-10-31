# Port decisions: magic

## Summary

What openblack took from raffclar's tree for spells, miracles, worship, influence and the reactions, what it kept of
its own, and why, decision by decision. The rule (the project's conventions) is that his architecture, names and
slots are taken, and that where both trees do the same job his version is taken only once it is shown to behave as
the original does, bit for bit; otherwise ours stays, moved behind his names. The original's behaviour each decision
follows is on the magic pages; the general rules are on [the refactor conventions](refactor-conventions.md).

## The original

Not the subject of this page: each decision links or names the magic page that reads the original for it
([the spell core](../magic/spell-core.md) and the pages it lists).

## openblack

### The temple

- **His temple destruction, taken with three changes (N-A-12).** `TempleDestructionSystem` behind
  `TempleDestructionSystemInterface`, its fake-able `temple_world::World` and his state on `components::Temple` are
  his. Not taken: his `ProcessTurn` at the start of the game's turn (the original steps a heart in its citadel's turn,
  `ProcessHeart` from `worship::citadel::Process`); his list of loops kept by the system (our sound tags are the
  original's, and `SoundTag::Remove` at the explosion stops every loop of the temple); his model's triangles from the
  L3D file cache (the temple is not in it: ours are the drawn mesh's). His pure steps worked in double precision or in
  another order (`NextBeam`, `BeamLookAt`, `SmokeTurns`, the heart's `Harm`) are the game's float steps here, in its
  order. The struck heart is his `StrikeHeart` in `ECS/PhysicsGameHooks.cpp`, its choice and beams the system's
  `ActualObjectToEffect`, its hurt beams his tree's damage effect in the system's heart turn. Not taken: his return
  when the thing that struck the heart has gone (the original still passes the blow on). See
  [the temple's destruction](../magic/temple-and-citadel.md#the-temples-destruction).

### Alignment

- **Taken with corrections: his alignment motes (449d6ebd).** `creature_alignment_motes` (names, constants, draw order
  and his pure tests) and `physics::turn::PuffKind` are his. Following the exe instead: the alignment is the body's
  target (his reads the shape already drawn), the motes come in `PoseTurn` before the new pose and read the turn pose
  (his come after the breath in `ProcessTurn`, from the frame's bones), no creature sparkles while the falling spell
  runs, a puff goes once its age is past its life (his at it), and the size curves take the original's order of
  products and its extended sine. His `DustPuffSize` / `DustPuffFrame` take a kind, an age and a size here, as our dust
  is not his `DustPuff`.

### Reactions

- **Reaction service:** his `Locator::reactionSystem` slot and `ReactionSystemInterface` name front our reaction list.
  Of his methods it carries the ones our list serves as he describes them: `RemoveFrom` (both), `Remove`, `IsActive`,
  `HasReaction`, `Reset` and `GetReactions`, over our `Reaction` record (`Find` keeps his name and returns our record).
  The shut-down of removed reactions moved into the service unchanged. His `Create`, `Move`, `ProcessTurn`,
  `ReactionsAt`, the land balance and his `Source`/`Active` records are not taken: our reactions are made and spread
  by `ECS/Effects/Reactions` ([reactions](../magic/influence-and-alignment.md#reactions)).
- **The turn's walk over the reactions** (2026-10-17, ours, from the exe): the original offers one reaction again
  each game turn ([animal-life](../beings/animal-life.md#reactions)). His later `ReactionSystem::ProcessTurn` also
  takes one reaction a turn, grows it, ends a timed-out one or spreads it, but walks his list from the oldest with an
  index that a removal does not fix, and spreads an inactive one. Ours is taken: `reactions::ProcessTurn` with the
  pure step of `ECS/Effects/ReactionTurnRules.h` (newest to oldest, the cursor moved to the next older reaction when
  its own goes, an initiator that is gone stops the step, an inactive reaction is not spread), the cursor kept by our
  `ReactionSystem` (`Cursor` / `SetCursor`, with default bodies on the interface so no fake has to change).
- **The reaction a villager follows** is his `components::LivingReaction` (`ECS/Components/LivingReaction.h`), with
  our fields: the reaction, its type, the object its setup reacts to and the place among its followers. His start
  turn, creature memory and previous state are not taken, and the creature keeps our `CreatureReaction` (a byte type
  and a follower flag) until the two are measured as one. The service's tests are his `test_reactions` file and
  `Reactions` fixture with our bodies; his cases there run his whole system over villagers and creatures, which ours
  pin in `test_villager_reactions` and `test_creature_miracle_reactions`.
- **Villagers running from and watching miracles:** his state ids and the names of his two state functions
  (`villager_reactions::Fleeing` for 6, `Watching` for 7 and 30) are taken, behind our reaction slot and our scoring
  (the reaction type table's rows, `ScoreOf`, the records and the switch rule). Where his reading differs from the
  original, the original's is followed: the distances are in the plane, to the miracle where it is now; the urgency
  of a frightening miracle is whole-number arithmetic on the fast distance; the "a caster never runs" test is the
  creature's, not the villager's; the object gone also ends the reaction. The flee point is our animals' rule made a
  pure module (`ECS/Effects/FleeGeometry`), its arithmetic and its coming-towards test (in three dimensions) the
  original's; the animals have used it too, in place of their own copy, since it was measured for them (2026-10-15).
- **Belief from the miracles villagers react to:** his `Magic/Impressiveness` module and names come in
  (`ImpressionInputs`, `ImpressiveValue`, `ReactionMultiplier`, `ImpressionAlignment`), merged into our town belief:
  the belief goes through `town_stores::AddToBelief` and our `TownBelief` boredom, the distance's share is our
  `gutils::DistanceChangeToBelief` (his copy of the curve is left out), and the alignment through our
  `effects::alignment`. The impression is the villager's `UpdateHowImpressed`, called where the original calls it
  (the villager taking up a reaction), not his reaction system's. Where his reading differs, the original's is
  followed: the factors multiply in the original's order, the share comes from the town's stats, not a count of its
  villagers, and the shield's per-town overrides are added (`ShieldImpressiveValue`). His `BoredomAfterImpression`,
  `BoredomAtTownTurn`, the creature's multipliers and his copy of `FleeFromSpellPriority` are not taken: our town belief
  and the flee geometry (`effects::flee::FleeFromSpellPriority`, the original's) already do those jobs. His belief voice (its own gaps, the hand's distance) is not taken either: the
  belief sound is the original's guidance path we already had (`audio::guidance::PlayBeliefRemark`), now asked for by
  `town_stores::AddToBelief` with the local interface's map point.
- **The impression tests** are in his `test_magic_foundation` file (the magic group), with his `Impressiveness` names
  where ours pin the same numbers; the food and wood case is his `ReactionImpressiveness` one, which his tree has in
  `test_map_cells`. Our `ImpressionTest` cases (a town gaining belief through a villager) stay beside them: his run
  through his reaction system in `test_reactions`. His `components::TownImpression` and `CreatureImpression`
  (`ECS/Components/MiracleImpression.h`) are not taken: a town's belief and boredom are our `TownBelief`, which holds
  the whole of the town's belief, and no creature impression is ported yet.

### Influence

- **Influence service:** his `Locator::influenceSystem` slot and `InfluenceSystemInterface` front our influence code
  in `ECS/Influence`, which keeps its state with the land in the registry. `InfluenceSystem` keeps nothing of its own.
  The turn's reach, the border drawn again, the hand's crossing with its ripples, the circles and the ripples the
  renderer draws go through the slot. Our anti rings and the measure (`CalcType`, allies) are methods too, and the
  border has its own `UpdateBorders`, because it is drawn later in the turn than the reach is worked out. Three of his
  methods are left out. `PlayerInfluence` by map position would round the distances ours measures in metres. `Reset`
  isn't needed, because a new land's registry starts the state afresh. `GetScrollOffset` isn't served, because the
  scroll clock is the renderer's. His `InfluenceSource` comes later. The other callers still use the `influence::`
  functions, which tests reach with fakes. The slot is emplaced unless a test put one in, and reset after the
  registry.

### The magic window and the gestures

- **Magic window files:** as in his tree, the particles' tab of `debug::gui::Magic` (where an effect is started, the
  tab, its draw counts and the list of started effects) is in `Debug/MagicParticles.cpp`, and the miracle tables stay
  in `Debug/Magic.cpp`. The functions moved unchanged; his file picker, targets and timed close are not taken. Our
  separate Miracles debug window is his first tab of the Magic window, "Running", drawn by `Magic::DrawMiracles` in
  `Debug/MagicMiracles.cpp` as in his tree. Its content and buttons are ours, unchanged: the choice of seed, power-up,
  charge and player, the casts, the dispensers, the hand's seed, infinite prayer power, the running miracles, the
  dispensers' and creatures' spells and the villagers' reactions. Its click, update and event handlers are the Magic
  window's now. `MiraclesModel` and `MiraclesCaster` keep their names and logic, since his tree has nothing in their
  place. The debug window list loses its Miracles entry; his own tab body (his prayer sandbox and casting readout) is
  not taken.
- **Gesture paths:** his `Gestures/GesturePaths` (`gesture::Trace`, `TraceTemplate`, `TraceCircle`, `TraceScribble`)
  as he wrote them, for paths drawn by a program. `TraceGesture` and `MakeTemplate` keep his names and arithmetic over
  our recogniser (`Magic/Gestures`) rather than his `GestureRecorder` and `GestureMatcher`: a path is recorded by our
  sample buffer and matched by `MatchGesture` against the templates turned into the recogniser's list
  (`TemplateFromFile`, now public), and `MakeTemplate` takes the recogniser's key points (`KeySample`) and its
  `Aspect`, which is his `AspectOf`. Nothing in the game calls the paths so far.
- **Gestures:** his `gestureSystem` and `gestureEvents` slots (one object, the second not owning) and his
  `GestureEventsInterface` as his text. `GestureSystemInterface` keeps only the members that map one to one onto our
  `magic::gestures` module: `Update` (the test stroke, the mouse sampling and `ProcessPowerUpSystem`, called from
  `hand_casting::Update` at the same point), `Reset` on a new land, `ForgetPath`, `GetScreenAspect`,
  `GetCircleSecondsLeft` and `IsGesturing` (the gesture trail's condition). Our recogniser, buffer and
  `PowerUpSystem` stay, with their state in the hand magic state (`MagicSystemInterface::HandMagic`, below); they act
  on a gesture at once, so they put no events in, and the spiral selection and the R repeat stay ours only. His
  request list, `GestureRecorder`, `DrawPath` and per-frame view are not taken (our sampling reads the cursor, window
  and camera itself, every 28 ms of mouse events),
  nor his leash gestures as he acts on them: his `GestureSystem::Act` shakes, toggles and changes the leash at once,
  where the original sends the player's leash orders to the next turn (which refuses a spell and takes the temple's
  pick), so the gestures go into our `PowerUpSystem`'s pass in the original's order
  ([what is looked for and when](../magic/gestures.md#what-is-looked-for-and-when)). His names are taken:
  `LeashSystemInterface::Shake` (the leash held in the hand comes off) and `GestureSystemInterface::IsLeashPickerOpen`,
  shown in the gestures window. The per-turn `ProcessPowerUpSystem` stays a direct call.
- **The gestures from the cue labels (N2-M-5, behaviour):** as his, the help texts slot's `recording` seam gives the
  line's labels and its `VoiceHooks` give the tags to the advisors' control (`SetSentenceTags`, `StopSentence`, once
  the advisors are loaded). Ours kept: the wave's bytes come from our sample (`Sound::buffer`, or its bank's file
  through `banks::ReadWave` for the dialogue banks) instead of his decoded resource.

### The miracles' service

- **Miracles:** his `magicSystem` slot and `MagicSystemInterface`, in front of our miracles under `src/Magic`.
  `MagicSystem` owns the miracles' state, as his keeps it as its own members: the spell store, the magic objects, the
  falling spell and the hand magic state are value members, reached through `SpellStore`, `MagicObjects`,
  `FallingSpellStore` and `HandMagic` (ours), and have no slot of their own. They are destroyed spells first and
  falling spell last, the order their slots were reset in, when the service is reset after the registry. Each method
  hands on to our `magic::` functions, in his groups and names with our types (`particles::ProcessInfo`,
  `magic::SpellCastData`): the casts, `CloseDown`, the spell events and paying, `SpellStrength`, `SpellAt`,
  `RainOnFire`, the dispensers and one-shot bubbles, the seed in the hand, the debug state and `GetSpells`, plus our
  turn steps (`ProcessGameInputs`, `ProcessTurnStart`, `ProcessForests`, `RunDebugHooks`,
  `ProcessSpellParticlesEndOfLoop`, `ProcessHandTurn`). In `Game.cpp` and the Magic window's Running tab the casts,
  one-shots and turn, frame and land steps go only through it; the hand effects' interface, the players' state hash
  and the window's read-only lookups (spell and effect info, strength, chant context, gesture icons) still call our
  `magic::` functions directly. His `PressAction` and `ReleaseAction` hand the action, with a seed in the hand only, to
  the hand system's own seed press and release (`HandSystemInterface::PressSeedAction` / `ReleaseSeedAction`), the
  branches its input takes, so a miracle is never cast twice; his `HandResult` comes back from them and is kept as
  `GetLastHandResult`, unless the branch did nothing; `DiscardHeldSeed` keeps `Discarded` and a new land `None`, as
  his. `TapAction` takes nothing: our hand taps the bubbles under it itself. The testbed's runner drives the hand through ours
  (`DriveHand`, `GetDrivenHand`: the hand's ray is the scenario's while one is given); the rest of his hand drive
  (`UpdateHand`, `GetHandCastState`, `OrbAlong`) is not taken. His facade's `ApplyEffectAt`, `ApplyEffectToObject`
  (from a player or an `EffectSource`), `SetDispenserActive`, `SetDispenserTurns`, `SummonSeedAtSite` and
  `GetTribalPower` are on ours under his names: the effects go to the game's world, the dispensers' switch and turns
  to our `worship::dispenser`, the tribal power is read from the player's `PlayerMagic` through the const registry, and
  `SummonSeedAtSite` asks that site's icon for the seed, as `SummonSeed` asks the best icon (the worship's own
  request): a full icon puts its seed in a ready hand and that seed is returned, otherwise the charge starts and none
  is ([the spell core](../magic/spell-core.md#pending)). The
  scripts' SET_ACTIVE and SET_TIMER_TIME reach a dispenser through the facade, as his do; SET_TIMER_TIME keeps our
  conversion of its seconds to turns (`game_clock::TicksForSeconds`, which follows SET_GAME_TICK_TIME) before
  `SetDispenserTurns`.
- **His `MagicWorldInterface`** (`src/Magic/MagicWorldInterface.h`, his text) is what the miracles do to the world and
  ask of it; it stands in front of our spells, which keep their own code behind it, one family of them at a time. Two
  changes from his: it takes our `ecs::effects::EffectValues` (his `SpellRules.h` values are not taken; ours also say
  who applied the effect), through `magic::EffectValues`; and the game's world is reached through a public
  `MagicSystemInterface::World()`, where his is private to his spell behaviours' services, because our spells are free
  functions. `EffectSource` is filled as his is. Who applied an effect comes from our values; when they name nobody it
  is taken from the source, as his facade does (`Magic/EffectSourceRules`). Tests use `test::FakeMagicWorld`, his tests' fake world. The game's world is his
  `GameMagicWorld`, a member of our `MagicSystem`, and each of its bodies calls our own code, never the world (the cast rules' bounds and
  land, the piles' dry land, the influence, the effects, the water's `LandDrop`, the heal's `HealTargetsAround`); it
  keeps no state, and it does not override his hand and deed hooks, whose defaults are what our game does. Its points
  are our map positions (x, z and the altitude above the land), but for `ApplyEffectAt`'s, which is a point in the
  world as his is. The miracles reach it through `magic::TheWorld()` (a missing miracles' service aborts): the cast
  rules, an event's default effect, food and wood, the heal's search, the water's drops, the flocks, the seeds, the
  hand's seed and the map's altitudes under `ToWorld`. The influence measures across the land only, so a world point
  is given with its x and z as they are. The lightning's tips keep `ApplyEffectToMapPos`, whose altitude
  is not 0: a round trip through a world point would not be exact.
- **His worship battery and chant rules** (`Magic/WorshipBattery`, `Magic/SpellChants`) are taken, the same rules as
  ours term for term ([the battery](../magic/worship.md#the-battery-and-the-sites-turn),
  [chants](../magic/spell-core.md#chants)).
- **The miracles' service's `SummonSeed`**: raffclar's version (charged at once from a player's store, in the hand
  before it is full) is not the original's and was not taken ([the spell core, Pending](../magic/spell-core.md#pending)).
- **The fireball's heat row**: raffclar's tree reads row 0 for every ball (its `.at(0)`), which is not taken; ours
  gives the row as the original ([the fireball](../magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire)).

### Shields

- **Shields:** his `magicShieldSystem` slot and `MagicShieldSystemInterface`, in front of our shields
  (`magic::map_shield` and `magic::spell_shield`); his implementation is not taken. `MagicShieldSystem` holds only the
  shield objects' serial counter (below); the shield spells' list stays in the spell service. Declared, with a caller
  each: `ProcessTurn` (the domes' turn, still at the start of the miracles' turn), `Update(seconds)` (the domes' draw
  between turns, at the same place of the miracles' frame; the seconds are not used), `Reset` (the spells' list and
  the serials, in a new land), our `NextShieldSerial` and `KeepsReactionOff` (the reaction spread's check, with map
  positions). The miracles' turn, frame and land steps and the reactions reach the shields only through it; the
  physics, the route plan and the villagers' shield reaction still call our functions directly. His other methods
  come with their first caller: `ShieldStruck` (ours goes through the spell class's table), `HasObject`,
  `CreatureAvoids` (our route plan asks per object), `ShieldAt` (the scripts do not ask yet). His `DomeDraw` and
  `GetDomes(turnFraction)` are declared with an empty default, and his `Renderer::DrawShieldDomes`
  (`Graphics/RendererShields.cpp`) reads them in the world view only, pushing each dome into our single Z queue keyed
  as a model (`zsort::SumOrder::XZY`, `graphics::shield_domes::Submit`) right after the models; it makes no bgfx call.
  `MagicShieldSystem` does not override `GetDomes`: our domes stay `MapShield` map objects, lerped by their rows and
  drawn and queued with the other models, re-melted on the land and not reflected, as the wiki has it. His Reflection
  draw, his bgfx-depth sort of a translucent view, his angle lerp and his `alphaTexturedAdditive` submit flag are not
  taken (our mesh's 4/5 -> 13 rewrite does what the flag does). Feeding `GetDomes` means taking the dome's mesh off the
  map object in the same, measured commit, or it is drawn twice. The queue's entry (`ZObject`) is in
  `Graphics/ZObject.h`, so that the hooks in their own files can push to it. The fireballs keep their list in the
  magic objects' service.

### Teleport, tornado and flocks

- **Teleport, tornado and flocks:** his `teleportSystem` and `tornadoSystem` slots (`TeleportSystemInterface`,
  `TornadoSystemInterface`) and his `magic::FlockMiracleInterface`, in front of our stones
  (`Magic/Objects/MagicTeleport`), our tornado rule (`Particles/Rules/Storm.cpp`) and our flock spells
  (`Magic/Spells/SpellFlock.cpp`). The pattern is the miracles' one: the implementations hold no state and hand on to
  our functions; only the methods with a caller are declared, with our types, and the read-only lookups (a stone's
  seed, position, reaction and player, the hand's touch stones, the traces) still call our functions.
  - Teleport, his names: `CanPlaceStone` (the cast rule), `ShouldReact` and `DoTeleport` (the villagers' teleport
    reaction), `DropOnStone` (the hand; the dropper is not read: the stone must be the villager's player's), and
    `RouteStoneFor` (the worshippers), `ProcessTurn` and `Reset`; ours: `CanDropOnStone`, `RegisterDestination`,
    `Update(seconds)` and `RunDebugHooks`. The miracles' loop drives the turn, frame and land steps through it, at the
    same places as before. Our spell class still makes and removes its stones, so his `CreateStone`, `RemoveStone`,
    `GetStones`, `StoneOf`, `BlocksNewBuilding`, `SetupReact`, `StoneAlong` and `RouteWorshipper` wait, with the
    creature's teleport.
  - **The creature and the stones**: raffclar's tree walks the creature to within 5 m of the stone, jumps it, fades it
    out and in over 2 s and walks it on to within 5 m of its destination. Only the last 5 m matches a value of the
    original (sub-action 8's 5.0); the 5 m approach and the 2 s fade are not in the original's functions, so his
    walk-and-fade model is not taken ([a creature and the stones](../magic/teleport.md#a-creature-and-the-stones)).
  - Tornado: `Carry(object)`, `Update()` (both with other signatures than his, below) and `CarriedCount`. Our tornado
    lets go of what it carried when the particle goes, as the original (the object leaves the physics, its player is
    passed on to the effect, its position kept in fixed point: [the tornado](../magic/storm-and-tornado.md#the-tornado)), so his
    `ProcessTurn` and `LetGo` are not taken; his `Candidates` and `TakeFromPile` wait, since our pile split draws its
    size from the effect after the new pot is made; `CatchCreature` is taken for the creature's branch: the creature
    faints through the fight system's `ForceFaint`, once, behind his `IsKnockedOut` guard. His `CaughtByTornado` mark
    is not taken (nothing reads or removes it, and it would add a storage), nor his fallback with no fight system (a
    missing one fails with a clear message, as `TheTornadoSystem()` does). His `Reset` has nothing to clear in ours. Our
    carried mark takes his name, `CarriedByTornado`, and stays an empty component.
  - His `InPhysics` tag (`ECS/Components/Physics.h`) is taken in his meaning: on every object in the physics. Our
    dynamics system sets it again from its list after each change of an entry (`SyncInPhysics`: added, knocked into
    flight, at rest, gone), so it is on the objects with a moving body, on what the carry's `Take` holds (beside
    `CarriedByTornado`; his vortex keeps both too), and on an immovable or script-owned thing a hand let go of, which
    hangs where it was let go with no body (`physics::from_hand`, as the original); `Release` removes both. It is a simulation-only component
    (`AssignOrReplaceState` / `RemoveState`): the drawing never reads it. Readers that asked "moving body or carried"
    read it now; readers whose answer would change for a carried object (VillagerDeath, the villagers' food, the fire)
    stay on `IsFlying` / `Find` until the behaviour commits that make them the original's.
  - Flocks: `FlockMiracle` is held by `MagicSystem` and reached through `MagicSystemInterface::Flocks()` (his tree has
    it on his spell services). Both flock classes' table of operations hands `ParticleTypeOf`, `Start` and
    `ProcessTurn` to it, and it picks the flying or the ground flock's own step by the spell's class. The maths stay
    ours: they keep the original's map coordinates' fixed point, where his lerp in floats and floor
    ([flocks](../magic/flocks.md#the-original)). His `AnimalsLeft` and `FollowsHand` wait for his spell behaviours.
  - Same names, other signatures: a later port must not read his meaning into these. The teleport methods have his
    signatures. The others do not:
    - `TornadoSystemInterface::Carry`: his is `std::optional<glm::mat3> Carry(const std::shared_ptr<CarriedObject>&)`,
      the rotation the carried object starts with, or none; ours is `bool Carry(entt::entity object)`, whether the
      particle carries the object (our rule keeps its own carried list and reads the rotation from the object).
    - `TornadoSystemInterface::Update`: his is `Update(float turnFraction)`; ours is `Update()`, which puts what is
      carried where its particle is drawn and takes no fraction.
    - `FlockMiracleInterface::ParticleTypeOf`: his takes `const Spell&`; ours takes the spell's entity.
    - `FlockMiracleInterface::Start`: his is `void Start(SpellServicesInterface&, Spell&)`; ours is `int
      Start(entt::entity spell, const glm::vec3& position, SpellCastData*, const particles::ProcessInfo&)`, our spell
      table's start operation, with its result.
    - `FlockMiracleInterface::ProcessTurn`: his is `void ProcessTurn(SpellServicesInterface&, Spell&)`; ours is `int
      ProcessTurn(entt::entity spell)`, our spell table's turn operation, with its result.

### Cinematics

- **Cinematics:** his `cinematicDirectorSystem` slot (the `screenFade` slot is gone) over our fade and bars, split into
  his `Gui/ScriptFade` and `Gui/CinemaBars`; his `Gui/ScreenFade` name holds our temple fade, which the falling spell's
  film still owns and which writes the script fade's colour. Our signatures stay: float seconds truncated as the
  original does, the bars' time passed by the caller, `SetFadeColour` and `SnapWideScreen`. Not taken: his owner,
  interface and dialog flags (ours are the help system's) and the reset as a land opens; his close clipping (M11) is
  taken ([near clipping](port-decisions-hand-camera-interface.md#near-clipping)). His
  `test_screen_fade` cases are taken (`test/gui/test_screen_fade.cpp`) on our `gui::ScreenFade`; where his expect a
  fade that lands exactly on its target to count a turn and reset, ours hold the original's: the fade is held there
  ([the temple fade](../video/playback.md)).

### Dispensers and the miracle effects

- **Dispenser rules:** `Magic/DispenserRules` takes his names (`DispenserTimer`, `DispenserStep`, `StepDispenser`,
  `OrbPosition`, `FaceTowards`, `DispensableMiracles`, the `k_Orb*` constants), and the
  `SpellDispenser` component his fields (`magicType`, `timer`, `orb`, `effect`). Our counting stays behind them:
  `MakeOrb` leaves the count to the orb being made, and periods go
  through `game_clock::TicksForSeconds` (his `PeriodTurns` is not taken). His `OrbStillThere` has gone and his stored
  `orbPosition` is not taken: the orb stays while it is available and touches the dispenser, as the original
  (`OrbTouches`, the distance across the land less both 2D radii within `k_TouchTolerance`). The rest of the dispenser stays in
  `Worship/SpellDispenser`. The rules' tests take his layout: `test/test_magic_spells.cpp`, suite `DispenserRules`, in
  the `test_magic` group, written to our counting. The two differences that remain are on
  [the spell core's Pending](../magic/spell-core.md#pending).
- **A slot over several of our modules.** His `miracleFxSystem` (`MiracleFxSystemInterface`, `MiracleFxSystem`)
  fronts three modules of ours that keep their state where it was: the hand's effects (`magic::hand_fx`, in the hand's
  magic state), the globes (`magic::one_off`) and the piles (`PotArchetype`). The system keeps no state and forwards.
  His single `Update(seconds, gameSeconds)` runs the globes, the hand and the piles one after the other; ours are
  stepped at three moments of the frame (the magic loop's start, the hand casting step, the hand system's update), so
  his private steps are public methods under his names (`UpdateGlobes`, `UpdateHand`, `UpdatePiles`), each called
  where its module was, and his `Update` is not taken. Where our call needs more than his signature gives, ours is
  added to it (`SeedInHand` takes the seed, whose effect plays in the hand). His callers guard with `has_value()`, and
  so do ours; the test services emplace the slot, since the modules it fronts always ran without any set-up. The
  renderer reads the tribes' names through the slot: his `GetTextTexture` and `GetTribalPowerText`, the latter for one
  ring or column (ours), and our `GetTribalPowerRunners`, all with empty defaults and forwarded to `magic::hand_fx`.
  Each ring or column stays a Z object of its own in our single queue (`graphics::tribal_power::Submit`), keyed at its
  centre and drawn with our text shader, as the wiki has it ([the tribal power column](../magic/hand-casting.md#the-tribal-power-column));
  his one submit of every name in a fixed slot of the frame is not taken. His `BubbleMesh`, `BandMesh` and
  `UpdateTribalPower` wait for the renderer merge.

### The miracles' object components

- **The miracles' object components take his names, with our fields:**
  - `components::TeleportStone` (his `ECS/Components/TeleportStone.h`) is our stone: `player`, `spell`, `reaction`,
    `pool` (the swirl effect) and `travellers`. `player` stays optional: a stone whose spell has no player is in no
    player's network and spreads no reaction, where his `NEUTRAL` would put it in that player's list. `Traveller` has
    no `finalState` and his `TeleportTraveller` is not taken: those come with his creature and villager arrival paths,
    measured. Our `scale` goes, as nothing read it. The logic stays in `Magic/Objects/MagicTeleport`.
  - `components::MagicShield` (his `ECS/Components/MagicShield.h`) is our shield object, `Kind::Spiritual` or
    `Kind::Physical`. It keeps our fields: `magicType`, `position` and `objectScale` beside his `kind` and `spell`, and
    the physical dome's fields until his `ShieldDome` is taken. His `player`, `radius`, `town`, `reaction` and
    `struckReaction` are not on it: ours stay on the spell (`magic::SpellShieldData`). The logic stays in
    `magic::map_shield`; `ObjectType::MapShield` and the info's `GMapShieldInfo` are the original's names and stay.
  - `components::FlockSpell` (his `ECS/Components/FlockSpell.h`) is our flock spell's data, on the spell entity:
    `flock`, `created`, `emitted`, `lastSpawn`, `lastSpawnHeight` and `castEffect`. `lastSpawn` stays a `glm::ivec2` in
    map coordinates, where his is a `glm::vec2` in metres: ours keeps the truncation the spawn points are compared
    with. Our `castTurn` (the flock screenshot hook's) is added. His `evil` (fixed at the cast) is not taken: ours is
    worked out from the caster's alignment each time it is needed. His `closedDown` would repeat the spell's own, and
    his `castEffectStarted` comes with his hand trail. `SpellFlockAnimal` stays ours until the animals' row.
  - **An ordered list becomes a serial on the component,** read through the const registry and sorted by it, newest
    first. Serials only go up, so this is the same order as a list with new entries at its head and stable removals;
    the entity and its place in the list go together. The player's stones are `TeleportStone::serial`, as his: the
    counter is our `TeleportSystem`'s (`NextStoneSerial`, zeroed by `Reset`), the one state that service holds, and
    `PlayerMagic::teleportStones` goes. A stone with no player gets no serial and is in no list. The shield objects are
    `MagicShield::serial` in the same way (his `MagicShield` has none: his list is the storage's order, which a removal
    from the middle reorders); the counter is our `MagicShieldSystem`'s (`NextShieldSerial`, zeroed by `Reset`), and
    the magic objects' shield list goes.

### The editor

- **The editor's miracles and creatures, his code on our editor:** the palette's Miracles tab (dispensers, or bubbles
  on their own, of each miracle a dispenser can give, named from the magic tables), and in `EditorEntities` his
  creature, dispenser and bubble cases of `Place`, `ItemOf`, `MeshOf` and `Duplicate`, `Remove` through the magic
  system first, and a creature's walk stopped and its heading set when it is moved or turned. The editor gets his
  Scripts tab, his tab order and his toolbar that wraps onto more rows; the inspector gets his creature movement and
  mind lines. Ours stays where both trees do the job: the nearest town by our own search, `Remove` destroying the rest
  at once, `MoveTo` setting the position and marking it teleported, our clock helpers, the left-click capture and the
  scenarios tab rule. Not yet taken: his Creatures tab and creature inspector sections, which host the creature tools'
  window inside the editor (`CreatureSpawner` hosted calls), and moving a dispenser's bubble or a bubble's seed with it
  and naming a bubble by its miracle (his `OneOffSpellSeed` and `SpellDispenser` fields). His villager life, age and
  food fields came later, with his `ECS/VillagerAge` (see "A villager's age" on the beings' port decisions).

### The village totem

- **Against raffclar's village totem** (his tree at 0222f9f5): his rules (his `VillageTotem.h`) match the original's
  numbers: the grip at 0.7 of the icon's height, 3 icon heights per screen, 0.1 of share per unit, the 3.2 hand, the
  2.5 low hand, the tilt, the stop at the land, the clamp and the frame (dur >> 1) × closure × 0.5. **His closure
  differs**: it uses the icon's ground radius (max(x, z) ÷ 2 of its box), where the original uses `GetHoldRadius` =
  0.75 × `GetHeight` (`ecs::object::GetHoldRadius(object, true)`). His AddToPlayer (the angle + π/2, the icon mesh, no
  tooltip after it), his let go (the empathy when above 0, the town's share or the totem's, the held share back to the
  set one, the grey number), his see-through totem at 0x80, his tooltip 120 (0xE73 + 120 = 0xEEB) and his reaction at
  the grip are the original's. **His grip test differs**: his "town centre built" is the totem's own `IsFunctional`
  (available, built, life above 0.75), his "any temple of the player" is the citadel heart built, and the press also
  needs the hand in the influence and the totem not held already. Not in his: the creature's desire update at the grip,
  the immersion 0xF, the guidance sound 3, and the re-mesh when a player's creature changes
  ([the village totem](../magic/village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go)).
- **The village totem, ours behind his names:** our totem (the rise, its ease on our `Zoomer`, the sounds, the arrival
  check and the villagers sent to worship, all in `worship::percentage`) is moved to his layout, one identical step at
  a time. His hand grip, slide and let go, his see-through second totem, tooltip, floating number and reactions have
  since come in through measured commits, as the entries below say.
  - The component is his `components::VillageTotem` in `ECS/Components/VillageTotem.h`, with his field names `icon`
    and `restY`. Our `rise` stays, and so do the object, script and burn types and the info named after the totem
    statue.
  - His rules header `ECS/VillageTotem.h` (`ecs::village_totem`) holds only what ours matches bit for bit:
    `k_EaseMsPerShare` (5200), `k_SnapMs` (the `Zoomer`'s shortest move), `k_ArrivedWithin` (0.005, compared as a
    double as before), `k_RiseAtFullShare` (8), `k_IconAbovePlinth` (2.729), `RiseOf` and `AsPercentage`. His `Ease` is
    not taken: a closed form that matches our `Zoomer` only to about 1e-3. The ease stays the `Zoomer`, now behind the
    pure `worship::percentage::SetRise`, `StepRise` and `TakeRiseArrival`, extracted unchanged.
  - `test_village_totem` runs his `VillageTotemEase` tests, under his names, on our ease. All his expected values hold
    for ours. His `RiseOf` and `AsPercentage` asserts are in `VillageTotemRules`.
  - The totem is made by his `VillageTotemArchetype::Create` (`ECS/Archetypes/VillageTotemArchetype.h`), with our
    signature and body: it is still made where a town centre becomes functional (`MakeTownCentreFunctional`), not in
    his `AbodeArchetype::Create`, and returns nothing.
  - His `villageTotemSystem` slot (`VillageTotemSystemInterface`, `VillageTotemSystem`) declares only what ours
    serves: `SetTownShare` (our worship percentage setter, which also sends or calls back the villagers), `Update`
    (our totems' draw, which moves on by the engine clock, not by the milliseconds given) and `TotemOf` (the plinth of
    a picked plinth or icon). The methods named in the entries below
    follow, and the scripts' HEALTH and BUILT_PERCENTAGE on a totem (`GetBuiltPercentage`,
    `SetBuiltPercentageFromScript`, `SetLifeFromScript`). It is made and reset beside
    `worshipState`.
  - The six callers (the worship update, a town centre that stops being functional, the town's emergency twice, the
    land script's town centre and the worship debug hook) go through the slot, and fall back to `worship::percentage`
    when it is absent, as in the tests that make no game. The private `TotemOf(town)` is `GetTotemStatue(town)`.
  - `GCreatureInfo` names his `totemIcon` (a `MeshId`, pinned by a `static_assert`) at 0x334 of the record, split out
    of the unknown bytes at 0x2dc: that is where info.dat keeps it. His 0x344 is the original's offset in memory, where
    each info record sits 0x10 bytes behind the file; at the file's 0x344 there are floats. The real info.dat test
    (`CreatureInfo.realInfoDatTotemIcons`) checks the species' icons. The struct's size and every other offset stay.
  - The dead `components::Town::worshipPercentage` goes: only `CREATE_TOWN_CENTRE` wrote it, to pass it on. The share
    lives in `TownMagic::worshipPercentage`, as before; his `Town::worshipShare` waits for his worship site port.
  - The hand's grip, slide and let go are the original's, not his (his grip test and his closure differ, above): his
    component fields `held` and `gripped`, his rules `k_GripHeight`, `k_SlidePerScreen`, `k_SharePerSlide`,
    `HandSlide`, `SlideShare`, `GripY` and `ShownShares` with his tests under his names (his stop at the land keeps the
    original's test; only a slide down is held to the land, 2026-10-24), and his
    `VillageTotemSystemInterface::Slide`. Where his flow differs from ours the hand's locked select stays ours: the
    press is a `hand_press` branch, the start and end go through the locked select packets, and the share is sent at
    the end as packet 0x29 (`game_packets::Type::TotemShare`). So the slot declares the original's split:
    `ValidForLockedSelect` (the four tests, as `village_totem::ValidForLockedSelect` over `GripFacts`), `IsGripped`,
    `StartLockedSelect`, `SetGripped`, `EndLockedSelect` and `LetGo(totem, share)` (the packet's handler), in place of
    his `Grip`, `GetGripped` and `LetGo()`. `worship::percentage::TotemTown`, which had no caller, goes.
  - His `AddToPlayer(totem)` is the slot's, called by his `VillageTotemArchetype::Create` as he does, with the
    original's body: the angle `village_totem::FacingSite` (the game's quantised `Get3DAngleFromXZ` + π/2, on the
    totem's and the site's MapCoords; his is the same), the icon `village_totem::IconMesh` (his `IconMeshFor`, with the
    original's "outside the pack is 0"), then the town's share 0. Ours only: `SetPlayersCreature(totem)`,
    `TownChangedPlayer(town)` (from `town_belief::TakeOverTown`) and `PlayersCreatureSet(player)` (from
    `CreatureArchetype::Create` and `LOAD_CREATURE`), which his has not. The component's `onCentre` stands for the
    town centre's link to its totem, which the original makes only after the totem's creation: the share set inside
    `AddToPlayer` at the creation does not reach the new totem.
  - His `ReactToHandUsingTotem` (through his world) is our `FriendlyStartLockedSelect(totem, player)`, the start
    packet's handler before `SetGripped`, which makes the reaction with `ecs::effects::reactions::CreateReaction`
    (stamped with the turn, as the original's last argument). The creature's desire update before it, which his has
    not either, waits for the creature mind.
  - The script's `HEIGHT` on a totem and `GET_TOTEM_STATUE`, which his has not, are ours on the slot: `GetShare`,
    `SetShareFromScript` and `TotemOfTownCentre`. `worship::percentage::SetWorshipPercentage` also sets `held`, as the
    original's town setter does.
  - The hand's pose in hand state 5 takes his names `k_HandSize`, `k_LowHand`, `k_HandTilt`, `HandTiltAt`,
    `GripClosure`, `GripTimeMs` and his slot's `GetHandHold` (ours takes the totem, as the locked select's object, and
    gives the hold radius where his gives the closure). Where his differ the original's are kept: the closure is over
    `GetHoldRadius` (0.75 × the icon's height), not the icon's ground radius; the frame is worked in floats and
    truncated as the FPU does, not in double; the matrix is `village_totem::HoldRotation`, the original's rows, not his
    camera-facing turn; and the pointer is not warped (the original only pins its cursor image, which openblack does
    not draw). It is placed in our `HandSystem::Place` and `Update`, not in Game.cpp.

### Test hooks

None of its own; each decision's tests are named in it.

## Pending

- The decisions that wait on a later port are named in each entry ("waits", "not yet taken"): his hand drive's
  `UpdateHand`, `GetHandCastState` and `OrbAlong`, the shield domes' `GetDomes`, the stones' creation and the creature's teleport, the tornado's
  `Candidates` and `TakeFromPile`, the flocks' `AnimalsLeft` and `FollowsHand`, the editor's Creatures tab, the
  renderer merge's `BubbleMesh`, `BandMesh` and `UpdateTribalPower`, and his `InfluenceSource`.
