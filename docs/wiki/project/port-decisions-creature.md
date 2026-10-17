# Port decisions: the creature

## Summary

The decisions taken while porting raffclar's creature code onto openblack's tree: which of his pieces were taken as he
wrote them, which sit on openblack's systems under his names, and where the original's behaviour wins over his
reading. Each row says what was taken, what was not, and why. The behaviour itself is on the creature's pages; the
general porting rules are in [The refactor's conventions](refactor-conventions.md).

## The original

The original's behaviour behind each decision is on the page linked from the row; this page holds no facts of its own
about the exe.

## openblack

### The creature's systems against his

- **Animation, skin, hair, footprints, physiology and locomotion** are his systems, with these changes:
  - the turn length is `game_clock` (`k_TurnSeconds`, `MsPerTurn()`), not his time service;
  - the eyes blink at the end of the turn on the local stream; the locomotion's run-away distance, fidget and mirror
    coin and the poo's yaw are `GameRand` / `GameFloatRand` ([Random streams](../engine/random.md#random-streams)); the
    puke's 12 drops are kept by the physiology system as draw-only data (`GetPukeDrops`), no entity, scattered by a
    private seeded generator;
  - the skins are painted only from the mesh files the loaders preloaded, never read at frame time;
  - the footprints' April Fools' day reads an injected date, once per land (`Reset`); the only wall-clock read is in
    `Locator.cpp`;
  - locomotion walks on openblack's `RouteFollower` (one per creature, made at its first move) and the `land_avoid`
    mask (`IsPosValid` at 7.1 for a destination and 7.05 while walking, `land_avoid::NearestValid` to put a stray
    creature back). The follower fills its obstacles through `route_plan_world::CheckSquareFunction`, which tells a
    creature's follower (its plan's context) from a footpath's holder (no plan, unchanged): a creature avoids another
    player's map shield, and a dead tree only while it burns and no script controls it. His own planner, obstacle
    gathering and corner cap are not ported. The `Transform` moves once a turn (`creature_pose::CommitTurnPose`,
    through the map cells); between turns only `CreatureDrawPose` and the animation slots move. A creature put back on
    the land it can stand on publishes `Teleported`.
- **Movement, body over time, spells, fights and throws:** his `CreatureLocomotion`, `CreaturePhysiology`,
  `CreatureSpells`, `CreatureFight`, `CreatureFightHud` and `CreatureThrow` are taken. His own flight (`Fly`, bounce and
  slide) is left out: what a creature throws flies through openblack's physics objects.
- **The morph animation header:** his tree names its fields (duration +0x0, looping +0x4, stride rate +0x8, stride
  length +0xC, displacement +0x10..+0x18); openblack's still has `unknown0x0..` in `MorphFile.h`.
- **The fight, leash, creature hand, mind and object actions** are his systems, with these changes:
  - **the fight is stepped by the turn**: the animation clock, the moves the animations carry the fighters by, the
    blows' moments and the turns to face move on 100 ms at a time at the start of the fight's turn; the frame only
    draws the body between the turn's start and end (`creature_fight::DrawnTime`) and charges a held blow. Its coins
    and choices draw `GameRand`, a wound's texels u then v first; the camera watches a fight by the original's rule
    ([Fights: the camera](../creature/fights.md#fights-the-camera)), through the player's camera, never by flying the
    camera itself; the player is `creature::LocalPlayer()`;
  - the leash has no posts: they come with the citadel's leash object; the keys are read once a frame
    ([The leash keys](../creature/leash.md#the-leash-keys)). The scribble with the leash held in the hand sends the
    click on the creature's packet (0x5F), which takes the leash off at the next game turn, as the original (the
    power-up pass, [What is looked for and when](../magic/gestures.md#what-is-looked-for-and-when)); `Shake` takes it
    off at once, for the debug windows and the hand demos only. Its scans are const and in entity order; its tying
    sounds are the in-game bank's 148 and 149, in turn;
  - the creature hand keeps the hand's pose and the creature under it (his `Game` members); there is no stroking by
    command;
  - the mind's draws are `GameRand` / `GameFloatRand`, the 40 first desire draws `GameFloatRange` in desire order; its
    scans pass over things that are going and take the lower entity on a tie; night is the day / night clock's; water
    is found on the `land_avoid` mask;
  - the object actions play by the turn, and their moments (taking hold, knocking down, letting go, eating) come at
    turn time, the palm sampled in the actions' animations at the moment. Something eaten dies as any death goes
    (`life::Kill` for a villager or an animal, the dead list for the rest); a home struck takes a creature's blow
    (`abodes::OnPhysicalDamage`), a tree is felled, a thing is deleted. What is let go of goes back on the map where it
    is, drawn there at once; it does not fly yet. Animals are food and can be picked up (when a hand may hold them).

### The mind

- **His casting mind goes onto openblack's magic core, and the original wins where the two differ.** His
  `CreatureMindCasting.cpp` and the casting members keep his names, on openblack's components; the cast itself goes
  through openblack's miracles' service (`MagicSystemInterface::CastByCreature`, `ReleaseCreatureCast`, and
  `CanCreatureCastAt`, the miracle's own rule), with the hands taken from the turn's pose as the original finds them.
  `TryMiracle` is a `mind_detail` function given the service, so that its tests use a fake. `KnowMiracle` (the
  testbed's) teaches through the scripts' `SetKnowsAction`, not his rounding up, so that the debug tool and the scripts
  make the same mind. Read against the original, openblack differs from his where the original does: hunger only on
  the energy test and the miracle's rule, not on every refusal; the mimic copy gated by `MayCast` as the planner is;
  no defensive fight cast without an opponent; the fight's Cast state held one update, not its animation; and the
  fizzle's stop and plan before its draws, with the sadness on the game's random stream, not his own.
- **The timed dominant desire:** his `Creature/CreatureSpellMind` names are the API (`SetCheatDominant`,
  `StepCheat`, `ClearCheatDominance`, `HeldDownByCheat`, `MakeFullyDominantOverOthers`, and his `Cheat`, now the
  state the mind component keeps), as thin wrappers over openblack's rules read from the original, which move to
  `creature_desires::detail`; `CheatTurnsLeft` is added beside them for the creature window. His `k_CheatSeconds`
  (20000) is the mood and need spells' own seconds, so it stays as the default and the caller may pass others (a
  leash, a town tie, a script). Where his reading differed, the original's wins: every other desire goes to the
  species' floor (so `SetCheatDominant` takes the floor), the chosen one's sources are filled, the wish to make friends
  under compassion ends back at the floor, and the clear lets go only when a desire was dominant (so
  `ClearCheatDominance` takes the cheat). His `MakeFullyDominantOverOthers` stays his (let go, then dominant over the
  floor, no sources and no activation, as the original's two Hunger calls that do the same), and his
  `MakeLeastDominant` stays his text, with his test's expectation (measuring it against the original is its own
  commit). His other tests run on the original's numbers. The behaviour:
  [The timed dominant desire](../creature/leash.md#the-timed-dominant-desire).
- **The creature's planner:** openblack's `creature_planner` keeps its names and functions, and its numbers are the
  original's: the product in the game's order and rounding, the goal path and the path without a goal as separate
  functions (`PlanDesire` with a `GoalSearch`, `PlanWithoutGoal`), and the gates in `MayPlan`. openblack's groups of
  actions by the kind of thing they are done to stay for the goal path, with `Replaces` keeping the original's order
  of equals.
- **Mimicry's watchers.** `PlayerDid` takes the player, and only creatures whose `Creature::owner` is that player
  watch, where his `PlayerDid` has no player and walks every creature
  ([What openblack does with a deed](../creature/mimicry.md#what-openblack-does-with-a-deed)).

- **The mind and the planner, row by row** (the behaviour is on [The creature's mind](../creature/mind.md) and
  [Plans and needs](../creature/plans-and-needs.md)):

  | Area | raffclar's version | Decision |
  |---|---|---|
  | Saving a mind | his rule (before-values, the curse globals; size = height / 15) | taken, dormant (nothing calls `SaveMind`); the original multiplies by 0.06666667 |
  | Sigmoids | a decimal table, 14 steps (16, 18, 19, 23, 24, 26, 27, 29-33, 35, 36) off by 1-2 ULP, 1 instead of 0 at a threshold of exactly 1 | openblack's: the game's 41 exact floats |
  | Look classes | SpellDove only in the Dove class | openblack's: every flying species and the Vulture (the factory's classes) |
  | Look point of an animal | its `Transform` position | taken for now; the original's look point is pending |
  | No miracle known at the start / CreatureMagicActionKnownAboutEntry +0x54 | his comment: read only by the computer players | agrees with the original |
  | Fear candidates | creatures, spells and his four frightening animals | openblack's: creatures and the animals `IsFrighteningToCreature` accepts; no spell until its belief path is read |
  | The OnFire belief | inverted (1 when on fire) | openblack's: the original's 0 when on fire |
  | Compassion and towns | his `creature_town_compassion` rules, his "being compassionate" test, his town belief's attribute numbers | taken on openblack's mind; the draws, beliefs and attributes pending |
  | Plan's town | his `about` | kept as `Plan::activityObject` |

### A script's creature play

**His pieces on openblack's mind system.** `creature_script_play` (his `Creature/CreatureScriptPlay`) makes the agenda
of a script's play and answers PLAYED; the mind system carries it out through `SetScriptPlay`, `PlayForScript` and
`HasPlayed` on `CreatureMindSystemInterface`, which SET_SCRIPT_ULONG, SET_SCRIPT_STATE and PLAYED call; the request is
kept on the mind component (`CreatureMindState::scriptPlay`), as his.

- His `CarryOutForScript` is a private step of openblack's that follows `PrepareForScript`, the start every scripted
  action shares (taken out of openblack's `FocusByScript`): unlike his, which went through `Replan`, it does not end
  what the body plays and stops the walk only when asked, as the original's start.
- Where his pieces differ from the original, openblack follows the original: a static action (51 or below) is played
  as its start, loop and end, the loop held ten seconds (his played none); the number is compared unsigned (52 or more
  is an individual action); a PLAYED that answers true for an idling creature also gives up what is left of its
  agenda. The reading is in [Playing what a script gives it](../creature/script-control.md#playing-what-a-script-gives-it-and-played).
- A script's move is the same way (`MoveForScript` on the interface, his `creature_mind::MoveForScript` agenda); his
  agenda took the height when it equalled the sixth of the way, the original only when it is more, which openblack
  follows.
- A step's `gaze` (his field) is consumed as the head's look by openblack's mind system's `GazePoint`, the original's
  look callbacks: a point is looked at on the ground beneath it (his looked at the point as given).
- SET_FOCUS keeps openblack's `FocusByScript` over his `FaceForScript`: they turn by the same rule, and openblack's also
  keeps the head on the point until the script lets the creature go, as the original's focus does.
- CREATURE_DO_ACTION calls `ScriptDoAction` (his name), which is openblack's `ForceActivity` (his `ForcePlan`) after
  `PrepareForScript`, the mind set up first as his `SetUpMind` did.
- `ForcedActivity::agenda` (his `ForcedPlan::agenda`, PR #387): an agenda given is carried out as it is in place of
  the one the action would make. A gift from the hand acted on "as when led to it" (his `ForcePlanOn`) is pushed onto
  the leash's `actOn`, which the next plan reads; its usefulness is our `Usefulness` of the act-on tree, without his
  `ObservedShare` and leash-tie rule (his #349).
- His `LookForever`, `LookButDontApproach` and the script's `SitDownAt` are executors of openblack's; where his agendas
  differ from the original, openblack follows it: the turn to the thing keeps the head on it, the look's and the sit's
  seconds are the game's own draws (`GameFloatRand(1.2)` + 1.1, `GameFloatRand(5)` + 10) in whole turns, carried in the
  situation's `drawnSeconds` (his drew a chance and his sit took the creature's own sit), and the sit pulls no face.
  His `PointAtThing` waits for the hands' service and the original's stages.
- The camera actions: his `PointAtCamera(hasPlayer, camera)` is the executor of `PointAtCamera`, with his head on the
  camera through the turn (the situation carries `hasPlayer` and the camera's own point, `eye`); where his differs,
  openblack follows the original: with no camera the agenda can't be made and the plan fails (his pointed all the
  same). `LookAtCamera` keeps openblack's idle mind's `LookAtCamera` step over his `WatchCamera` (the same rule,
  already pinned) and his `LookAtCamera(hasPlayer)` is not taken: the step itself ends at once without a player
  (`Senses::hasPlayer`), as the original's sub-action does, with his Curiosity face and head on the camera.
- His `HandStillness` component is not taken either: the original keeps the still hand's count on its interface
  status, which openblack's `HandSystem` already counts; it is read through `HandSystemInterface::GetStillTurns`. The
  step's 100 m is through the air (`Senses::cameraDistance`), as the original's.
- The cut scene's look (`LookAtCameraInWideScreen`) is his: `Build::LookAtCameraInCutScene`,
  `creature_mind::LookAtCameraInCutScene`, `StartsLookAtCameraInCutScene`, `k_CutSceneLookAction`, the `k_CutScene*`
  constants, `Step::turns`, the `Overlay` step with `Commands::playOverlay` and `Senses::overlayPlaying`, and
  `animations::k_Talk`. His `WatchCamera` with turns is openblack's `LookAtCamera` step with `Step::turns` set,
  counting in the step's own turns (`IdleMind::lookTurns`, its first turn one of them, as the game's sub-action
  count). The bars' owner is read from the help system (`help::HelpSystem::GetWideScreen` and `GetWideScreenOwner`,
  the game's `WideScreen` and `WideScreenControl`), as openblack's cinematic director keeps none; the plan is forced
  through `ForceActivity` with the game's score of 1.0, after the turn's fizzle, feedback and planning, as the game's.

### The cast against his

His `CreatureCastAgenda`, `CreatureMindCasting.cpp`, `MagicCreatureCasting.cpp` and `CreatureFightSystem.cpp` at
0222f9f5, read against the original ([Casting a miracle](../creature/casting.md#casting-a-miracle-the-originals-steps)):

- His `CastAt` has the original's order, the 1-in-5 emote (`k_CastEmoteLots` 5), the three styles' distances (50 / h +
  10; 2h / 2h; 5h / 2h), the 0.1 settle, the gesture only when non-zero, pose 41 and 3.0 s; his emotes `k_Angry` /
  `k_FeelingNice` / `k_FeelPlayful` are 53 / 64 / 67; his faces match the callbacks (`creature_face::Choose`, chosen
  by `variety % 2`, `faceVariety += random(7)`), except that his cast step has no face (the original pulls Grimace for
  2000 ms and draws `GameRand(7)`). His `height = k_HeightOfSizeOne × ShownSize` is h if `ShownSize` is the 3D body's
  +0x90 📼 1.20 only.
- His moves: the stuck rule (50, 0.01, 10 s), `k_SameDestination`, the 0.05 / 0.001 margins, the heading tolerance
  2048 / 6 and the same-cell rule, 26 cells and ×10 + 5, the 1.1 arrival, the 1.2 × radius back-off, the (1, 0, 0)
  degenerate case and `!(abs > π/8)` match. Differences: his `GoNear` fails on anything but Started; he re-issues the
  move only when not moving or heading off; his done rule lacks `IsObjectInMap`; his radius term is `TwoDRadius`; he
  adds radii for a non-Object; his `stepTurns` is not reset by a gesture; his distances are float `glm::distance`; no
  confined-area rule; no failure bookkeeping.
- His `MayTry` passes a NaN share (the original fails it); he has no player-enabled gate; his mimic path checks only
  `KnowsWhatItNeeds`, not `MayCast`. openblack's: `MayCast` with NaN failing, and the mimic path through it.
- His `TryMiracle` makes the creature hungry on any false from his `CastByCreature` (also a missing body, position,
  magic type or `CasterBodyOf`); openblack's only on the two original refusals. His release comes before the fizzle for
  both casts; the original's `CastSpellAtPos` fizzles first, then lets go.
- His `ShowFizzle` has no stop of its own, no `SuppressDesiresAfterDecidingOnActivity`, no `CopyPlanToPreviousContext`,
  sets `planActive = true` (openblack follows the leash's forced-plan convention) and draws the sadness from his own
  stream (openblack's `game_random::GameFloatRand(0.3f)`).
- His release erases the beams before the spell; the original goes spell first, then beams, and keeps the pointers.
- The hold: his and openblack's `uint32(step.seconds × float(lround(1 / senses.seconds)))`; openblack's now takes the
  turns a second as the original's whole division (`game_clock::TicksForSeconds` on the system's side).
- His hands (`HandsOf`) are the mean of the two hand bones' translations from the drawn frame
  (`CreatureAnimation::boneMatrices` placed by `PlacementMatrix`), his mirror `animation->mirror[right]`, with a
  fallback to the position; his `KeepCreatureBeams` runs after the creatures on the frame pose. openblack's: the
  fingertip means on the turn pose, the file's mirror table, beams moved in the creatures' turn. His beam order and
  size match the original.
- His fight's `CastFightSpell` chooses aggressive / defensive from the same flags, but casts a defensive spell with no
  opponent, has no +0x5238 skip, resets the spell after the cast, has no release at the end of Cast (his Cast lasts
  animation 111), and no release of the opponent's cast when a blow lands.
- His `CreatureRig::MirrorBones` name is kept; the table now comes from the species file.
- **Names, for the record:** his `CastAt`, `GoNear`, `HeadingFor` (kept unused, as the original's heading test is
  dead), `TwoDRadius` (the original's `GetNavRadius`), `ShownSize` (against `GetScale`), `CanPayFor` (the energy test),
  `KnowsWhatItNeeds` (skills, then the miracle), `TryMiracle` (openblack's `mind_detail::TryMiracle`), `CasterBodyOf`,
  `HandsOf`, `CastFightSpell`, `MagicSystem::ProcessTurn` (where his beams moved), `CreatureAnimation::boneMatrices`
  (the drawn frame he read). openblack's: `CreatureMindSystem` fills the senses' turns a second; `CreatureHands` (the
  hand point on `creature_turn_pose::Safe`, never `BonesDrawn`); the hand bone is `CreatureRig::actionPoints` (the
  file's first action point); the cast step's face is `Cue::Grimace`; the fight's Cast state ends in `creature_fight`
  `AfterAnimation` / the per-state step; the landed blow that releases the opponent's cast is in `TestHit`.

### The body and its draw

- **Vertex blends:** his `3D/VertexBlend` (`Partners`, `Apply`), his blend sources and his `vs_object` seam blend on
  openblack's creature morph program; the weight stays a float (row 1 of the source, no 32767ths) and is not clamped,
  and a vertex moves by placed + (partner - placed) x weight, as the original. His seam switch is not taken.
- **The creature's sway:** his `creature_sway` (`src/Creature/CreatureSway`) and its wiring under his names and
  slots: `components::CreatureAnimation::sway`, `CreatureAnimationSystemInterface::KickSway` and
  `physics_hooks::World::KickSway`, called by the strike (`creature_struck::HurtCreature`) with the turn's force on the
  body (`Strike::force`, the entry's `forceSum`) at the striking body's centre
  ([The sway](../creature/hits-and-catch.md#the-sway)). Where his wiring differs, the original's is taken:
  - the kick is ignored in the original's body states, not in his destroying blow: a held action
    (`BodyAction::Kind::Sequence` not timed by a fight), a fight's `Faint`, `Lying` and `GetUp`, and the locomotion's
    `Turning` and `Stepping` (`CreatureAnimationSystem::IgnoresKick`);
  - the spring is stepped once a creature turn in `PoseTurn`, before the pose, with the seconds the creature lives
    that turn (`creature_turn_pose::LivedMs`), not each frame with the frame's;
  - the leans are layered in `SampleBody`, so in the turn pose as the original has them (and in the frame pose,
    which samples the same way), by the Transform's own axes, never mirrored with the action;
  - every length in the spring and the leash's force is summed from z to x, as the original sums it (the spring is
    pinned to the original's bits by `test_creature_sway_oracle`);
  - his duplicate of the leash's pull is removed: `Sway::leashDrag`, `SetLeashDrag` and `FadeLeashDrag` and their
    test cases. openblack's stays: `components::CreatureLeash::pull`, set by `LeashSystem::Tug` and `TakeOff` and
    faded by `leash::FadePull`. The leash's push on the sway (`LeashForce`) is not called: in the original it runs only
    while that pull is above 0.05, which no running path sets, and its holder point is not read yet
    ([The leash's tug](../creature/leash-rope.md#the-leashs-tug));
  - `CreatureAnimationSystemInterface::KickSway` has an empty default, so the tests' fake animation systems need no
    override.
- **The creature spells' particle rules, his rules on openblack's engine, the original's where his differ.**
  `UR_CreatureSpell`, `UR_CreatureSpellGeneric` and `UR_CreatureSpellItch` (`Particles/ParticleCreatureSpellRules.cpp`,
  his file name; registered by `RegisterCreatureSpellRules`) call his maths (`Particles/CreatureSpellMaths`,
  `particles::maths`) as his do. The creature is read through his `CreatureBody(creature)` on a Locator slot of its
  own, `creatureSpellBodies` (`ECS/Systems/CreatureSpellBodiesInterface.h`), as the glints' targets are, implemented
  in his file name `ECS/Systems/Implementations/ParticleWorldLiving.cpp` (`CreatureSpellBodies`), plus an
  `IsAvailable` that the original's first rule asks every step.
  - The body is openblack's: the bones from the pose of the game turn (`creature_turn_pose::Safe`, the original's safe
    buffer; his read the drawn frame), the eyes from the rig and `creature_turn_pose::MirrorBone`, the invisible share
    from the creature's own spell slot (`creature_spells::Ratio`).
  - The spell side is four methods on openblack's `SpellSink` with his names (`IsCreatureCasting`,
    `CreatureSpellKind`, `MaxDirectionChange`, `CloseDown`), implemented by the spell's sink in
    `Magic/Core/Spell.cpp`.
  - Where his rules differ from the original's, openblack follows the original (the list is in
    [The creature spells' particles](../creature/casting.md#the-creature-spells-particles)); in his maths file, `SwungTooFar`
    normalises the two ways with openblack's `gutils::ModelInverseSquareRoot`, the original's table root, pinned by
    the exe oracle, not with his exact length; `WispOrbit` and `ItchOrbit` work in the original's order and rounding
    through openblack's `gutils::x87` (each sine and cosine unrounded into the operation after it, the flies' lengths
    summed y² + z², then x²), where his round each sine and cosine to a float.
  - `UR_CreatureSpellCompassion` and `UR_CreatureSpellFreeze` are his class names on openblack's engine, read anew in
    the original: neither is a creator, both let go without a parent creature, the hearts come from the point the atom
    they hang off gives (`psys::ParentSurfacePoint`, the emitter's, now shared; his took a random bone), the steam from
    about a foot (the body's `rightFoot` / `leftFoot`, from the rig's action points and `MirrorBone`), and their
    arithmetic is five new pure functions in his maths file through `gutils::x87` (`HasAtomRate`, `AtomsDue`,
    `FlungVelocity`, `SteamStart`, `SteamOffLand`), pinned by the exe oracle.
  - Against his port, rule by rule: openblack's first rule's atom stays where it was made (his followed the creature
    every step) and it lets go of a creature that is no longer available; the first and the wisps' rules keep the
    effect alive without being creators, the flies' is a creator (his were all creators); each rule lets go of its
    collection without a creator, a parent creature or (the wisps) a creature spell, and the wisps make nothing for the
    kind 16 (his went on); the creature is read from the first rule's collection, not from its atom; the wisps' and the
    flies' positions are written as the atom's own, not through the collection's frame (the same in the files, which
    have no hierarchy); an atom of the wisps' collection made by another rule gets default angles and rates (his
    skipped it); the invisible alpha is a byte; the casting creature's sound is the action 152. His hearts and steam
    were creators, never let go of their collection (without a creator, a parent creature or, the hearts, a rate), made
    the due before looking for the creature, took each heart from a random bone with a ball of 0.3 × the size about it
    and each puff with a ball of 0.3 × the size about a foot (the original: its own constant just below 0.3, the spread
    rounded first), flung them at (ball × InitSpeed) × size (the original: ball × (InitSpeed × size)), kept the steam
    off the land by its place in the collection's frame against ground + lift (the original: y − ground against the
    lift, the ground at the map coordinates of the atom's own position), and took their old atoms out without telling
    their creators.
- **Carried home:** `KillPermanently` and `Resurrect` of a creature fading out or in call `SetFizz(0, 0, false)`, where
  raffclar's code has `StopCarryingHome`; the fizz goes through openblack's `Locator::creatureFizzSystem`
  ([The creature's drawing](../creature/drawing.md)).
- **The alignment sparkles** are ported on raffclar's names (`creature_alignment_motes::PuffsOf`).
- **The animated statics (the creature gate and the plinth) run on openblack's object draw list, not his copy of
  it.** His `Scenery/AnimatedStaticRules`, the component and the `animatedStaticSystem` slot with his interface are
  taken as he wrote them, but his `Scenery/ObjectDrawList` is not ported: it is a simplified copy of the land's object
  draw list (every block reflected, heights read once, no land clip, no level of detail, no land flag, no rebuild
  requests, the time from differences of the draw time), and both trees do that job, openblack's as the original does
  it (`DrawListSystem`, `DrawnThisFrame`, the frame's game ms). His system's step is driven by openblack's. The step is
  openblack's own method `Draw(object, frameMs)` on his `AnimatedStaticSystemInterface`, next to his methods (as for
  the physics: openblack-only methods sit beside his in the same interface), called by openblack's object draw list
  for each object it draws, as the original steps the clip first thing in its Draw; his `Update` stays empty and
  uncalled, since it drove his own copy of the list.

### The hand, the catch and the fight

- **The creature hand's commands:** his `CreatureHandSystemInterface::Stroke`, `Slap` and `IsHeldByCommand` sit on
  openblack's `CreatureHandSystem`, through the same reward and slap rules and the same `ForceAction` as the button's
  `Update` (`HandOnCreature::byCommand` marks the hold). Two porting decisions:
  - A command on another creature while the hand holds one is refused, where his lets the held one go and tells its
    mind at once: openblack sends a creature's feedback as the hand's packet when the button lets go, so the command
    waits for that. The original has no such command (his facade serves the testbed); how its hand passes from one
    creature to another is not read.
  - The slap's height is the creature's drawn height (15 × the size it is drawn at, the pen's in its pen), as
    openblack's `Update` takes it, where his takes the shown size. The original's slap rules in
    `HandStateCreature::Update` are not traced, so which height they read is not known
    ([The CREATURE state](../hand/states.md#the-creature-state-and-its-packets), Pending).
- **The leash in the hand**: raffclar's rule allowed any god's creature in the hand; openblack holds only the
  player's own (it has no alliances), as the original's own-or-allied rule reduces to
  ([Leash and the hand](../creature/leash.md#leash-and-the-hand)). His shake tracker for the leash keys is left out.
  `GET_OBJECT_LEASH_TYPE` pushes what the original pushes (-1 with nothing picked), where his "None → 0" is right only
  for the failure cases; his tied / Toggle logic for `ATTACH_OBJECT_LEASH_TO_HAND` stays, the original always untying
  ([What openblack does with the leash natives](../creature/leash.md#what-openblack-does-with-the-leash-natives)).
  His pull at a rope tension above 0.8 is gone: the original never reads the rope on the turn.
- **The leash's draw**: his `RendererLeash.cpp`'s ribbon is the same in both trees; his `Translucent` sorted view, his
  GPU `Leash` shader with haze and land colour, his blended rope and shadow, his rope in the sea's reflection, his
  per-creature `drawn` flag and his white fallback texture are not taken, the original drawing none of them
  ([Drawing the leash in openblack](../creature/leash-rope.md#drawing-the-leash-in-openblack)).
- **The catch walk** sits in raffclar's slot `PhysicsClassHooks::OfferToCatchingCreatures`, filled by openblack's
  `ecs::creature_catch_walk::Offer`. His `Reaches` was not taken: the order he imported gave the other answer on all
  40 edge cases of the oracle; openblack's is worked out in the game's order (4000 random + 40 edge cases match, 20
  pinned in `test_creature_catch`) ([The creature's catch](../creature/hits-and-catch.md#the-creatures-catch)).
- **The fight interface** keeps raffclar's names `IsBlocking`, `Recoil` and `ForceFaint`; his two other additions
  (`CreatureKnockedOut::Stage::WaitingForSpells`, `CreatureKnockedOut::rest`) are not taken (the original's sub-state
  0x508C10 waits for both creatures' spells, but which activity runs it is not decoded)
  ([Fights](../creature/fights.md)).
- **A creature leaving its fight for good:** his `CreatureFightSystemInterface::Withdraw` and its default, and his
  override over openblack's fight: aborted, left at once, and forgotten by an opponent still pointing at it. His arena
  entity and fight view are not taken: openblack's arena is a value in the maker's `CreatureFighting`, gone with its
  fight, and openblack's fight system has no view of its own.
- **The creature's status panel and the fight's panel** on the interface are his: `GameInterface::SetCreaturePanel`,
  `SetFightPanel`, `DrawCreaturePanel` and `DrawFightPanel` as he wrote them, left out while the menu or the skip box
  is open (his tattoo editor's dialog is not openblack's yet). Which creature the status panel shows, inline in his
  `Game::UpdateHandInterface`, is `creature_panel_choice::Choose` (`src/Creature/CreaturePanelChoice.h`), a file of
  openblack's beside his `CreatureStatusPanel.h` so that his stays as he wrote it, with its tests in `test_creature`.
  Where his `Draw` puts the panels (under the film, the script fade, the cinema bars and the dialogue) is not the
  original's: they are drawn in the renderer's end of frame instead, over the bars and the dialogue and under the
  fade, through `DrawSceneDesc::interfaceLayers`, two hooks the game sets so that Graphics does not include Gui, and
  his two draws go on a second canvas in `Canvas::ViewUse::Shared` mode on `RenderPass::ScreenOverlay`
  ([The fight's panel](../creature/fights.md#the-fights-panel)).

### The player's creature

- **Taking a creature out of the game:** his `ECS/CreatureRemoval` (`WorldInterface`, `Remove`, `RemoveFromGame`,
  the player lists and the references let go) is his text; his game world's steps go through openblack's systems: the
  hand's `handGrabSystem` hold (openblack's hand serves it), the audio's `OnThingDeleted` for his `StopOwnedSounds`,
  and openblack's `ToBeDeleted` at once for his physics and world-object removal. The references are let go with
  `RemoveState`, as openblack's creature systems remove those components. His `Creature/PrimaryCreature`,
  `PlayerCreatures` and `LeashMarker` are taken; `PlayerCreatures` is filled by the player service's `AddCreature`
  (below). The removal keeps the original's rule, not his hand-over: the owner of a creature that goes has no creature
  after, whichever one the game kept for them (the destructor empties its player's slot, 0x475279). His `ClaimLeash`
  hand-over is removed; `LosePlayersCreature`, with `PlayersCreature` to read the slot, is a new step of openblack's
  that empties the owner's creature, last, after `RemoveFromWorld`. His `ForgetInPlayerLists` only forgets.
- **The player's primary creature:** his `PlayerSystemInterface::AddCreature` and `GetPrimaryCreature`, and his
  `GetLocalPlayer` and `IsComputerPlayer` with his default bodies, in place of openblack's `LocalPlayer`.
  `AddCreature` is his text and fills his `PlayerCreatures` list on the player's entity as a creature is made.
  `GetPrimaryCreature` keeps openblack's rule, not his "the earliest got that is still theirs": the one creature the
  player leads, which the leash keeps (the first made for them, or the one a script loads for them later; none once it
  has gone or been given away), as the original keeps one creature per player and empties it when that creature goes.
  It reads that creature from its own place, `components::PlayerCreatureSlot` on the player's entity, or the service's
  copy for a player without one (the neutral player, a script player not made yet), which an entity listed later takes
  up and a land load clears. The new `SetPrimaryCreature` writes it, and the leash calls it wherever its one creature
  per player changes (`SetLeashable`, `ClaimOnArrival`, `SetOwner`), so `Creature::leashable` stays a mirror, which
  the state hash reads. `LeashSystem::PlayersCreature` takes his shape: the player service's answer, else the
  leash's own ([The player's creature](../creature/players-creature.md#the-players-creature)).

### Tests and tools

- **His creature mode test's file is openblack's:** the 24 follow, Creature Mode and Creature Cave tests both trees
  have, under the same names, are openblack's in his `test/test_creature_mode.cpp` (in the creature group), with
  openblack's bodies: two of them follow openblack's code (the fainting rule's owner argument, the cave's miracle
  rows). openblack's tests he does not have stay in `creature/test_creature_follow.cpp`;
  `temple/test_creature_cave.cpp` is gone. His temple pen tests and his pen order (temple first) are not taken:
  openblack's puts the home first. Its double-click test is of his own press timer; openblack's Creature Mode reads the
  game's own `DOUBLE_CLICK` action instead, which `test_creature_mode_system` covers.
- **The creature spawner** (Debug > Windows > Creature spawner) is raffclar's window on openblack's creature systems
  (pure parts in `CreatureSpawnerModel`); his file dialogs, rope drawing, leash posts and editor hosting are not
  ported ([Test hooks](../creature/players-creature.md#test-hooks)).
- **The tattoo editor's lowest tip** is 0.125 of the ground distance, as the original's; raffclar's tree had 0.25
  ([The editor's view](../creature/tattoos.md#the-editors-view)).

### Test hooks

None; each decision's tests are named in its row.

## Pending

- His `MakeLeastDominant` keeps his test's expectation (the floor over 1.3) where the original's
  `MakeDesireLeastDominant` gives the species' floor; measuring it against the original is its own commit
  ([The timed dominant desire](../creature/leash.md#the-timed-dominant-desire)).
- His `PointAtThing` waits for the hands' service and the original's stages.
- How the original's hand passes from one creature to another, and which height its slap rules read, are not read.
