# Animals: species and the turn

## Summary

The original's animals are three classes: the grazers (sheep, tortoise, cow, horse, pig: the Cow class), the
predators (lion, tiger and leopard on the Lion code, and the wolf) and the birds (crow, dove, swallow, rock dove,
seagull, bat: the Dove class). Each turn an animal counts its hunger, tiredness and urge to breed and runs its state:
grazers wander with their flock, graze, sleep standing and breed; predators also hunt (stalk, sprint, pounce, eat),
have lairs and may take villagers; birds fly legs round their flock's home and never land. bw1-decomp has only empty
stubs of the animal code, so everything here comes from the executable.

openblack ports the whole animal AI; an audit of the animal AI found differences, listed in Pending. Their
reactions, flocks, bodies and the hand are on [Animals: reactions, flocks and bodies](animal-life.md); their walk round
obstacles on [Walking round obstacles](wall-hugging.md).

## The original

### Turn

Every turn, after the villagers (in the one living list, see [The turn](villager-states.md#the-turn)):
TurnsSinceStateChange + 1; if the state has the needs flag (info.dat `animalStateTable.field0xa4`) `ProcessNeeds`
(hunger, sleep, breeding +1 up to their info.dat maximum; the leader counts its turns as leader); then the state's
function (`g_AnimalStateTable` 0xD12108). There are no entry / exit clips nor speed changes per state
(`Animal::SetStateSpeed` empty). `SetTopState`: exit filter (in the hand only FLYING, LANDED or death; flying only
IN_HAND, LANDED or death), state, counter to 0 and the state's clip. `PlayAnimThenSetState`: WAIT_FOR_ANIMATION with
the clip unchanged until turns × 100 ms ≥ its duration.

- `Animal::SetStateSpeed` is at 0x41A2B0 (empty). `Animal::GetFinalState` 0x41A240: the top state if the table marks
  it final, otherwise the destination.
- The exit filters of the hand and of flight are `Living::ExitInHand` 0x5ED500 (only FLYING, LANDED or dying) and
  `Living::ExitInFlying` 0x5ED540 (only IN_HAND, LANDED or dying).
- `Animal::ProcessNeeds` 0x417DC0. `Animal::CheckNeeds` 0x418450 returns 3 ready to breed (otherwise the breed counter
  restarts when the flock is full), 1 hungry, 2 sleepy, 0 none. Every reader of the age asks for the live age:
  ProcessNeeds 0x417DFD, `Cow::ReactToAnimalNeeds` 0x41D333 and `Animal::IsChild` 0x41BB06 all do `call [eax+0x8D0]`
  (`Living::GetAge` = (turn − birth) / 1500), then `cmp eax,[info+0x138]`.
- `Living::ProcessLiving` calls ProcessReaction first (0x5EC8C6), then ProcessState 0x417EE0, whose first instruction
  is `inc word [esi+0x90]` (0x417EE6).
- At the start of a turn a target that is no longer available is dropped (fn_00417E90), and also one in the hand
  (+0x24 & 4, 0x417F38); a young one grows four times a year, every 1500 / 4 turns (fn_004179F0).
- `Animal::InteractDecideWhatToDo` 0x417D80 (vt +0xB88): with a flock (+0xB8) `LookForFlocksInSpiral(Pos, info, 2 ×
  domainRadius (+0x25C), merge 1)` 0x41A690 (0x417D83..0x417DA2), then `StartWander` (vt +0xB48, 0x417DAB): the Dove
  class has its own, `Animal::StartWander` 0x417C90 serves the grazers, the hunters and the SpellWolf.
- `Living::GetNumTurnsToDieOver` 0x5EC3E0 = 600 (the corpse time).
- `Animal::Animal` 0x416EB0 with `Living::Living` 0x5EBEC0: counters 0, speedDefault, DECIDE_WHAT_TO_DO, the info.dat
  life; the sleep place is the flock's domain-centre cell at creation (fn_005E18E0).
- Animals are created aged GameRand(20)+5 with a flock (0x419D47, Animal.cpp line 0x5C8) or GameRand(40)+5 alone
  (0x419C34, line 0x597), before the class factory (fn_00419E00). The creation with a flock (fn_00419D10) removes a
  non-shepherdable animal from its town, sets the flock's +0x88 = max(+0x88, count) and the sleep cell; alone
  (fn_00419C20) it calls CalculeLairPos (vt +0xBD0) at creation for **every** species (0x419CD2), then sets the sleep
  cell (0x419CE8).
- The initial heading comes from `MobileWallHug::SetToZero` 0x60F760: game angle 0, facing +x; nothing at creation
  sets another.

### Herbivores (sheep, tortoise, cow, horse, pig)

A single class (constructor 0x41D0B0, vtable "Cow"); only the clips change. Cycle:

- **DECIDE_WHAT_TO_DO** (`Cow::DecideWhatToDo` 0x41D1B0): breed if it is time; the leader (first member of the flock)
  takes the herd to a random point of the domain every `stayTime` turns (cow 200, the rest 1000) or if it left the
  domain; a member more than `flockDistance` from the leader goes back to its side; otherwise, the needs; otherwise,
  START_WANDER. The flock's +0x58 is 0 in a new game (set by `Flock::SetToZero` 0x5302E0 at 0x5302F5, called by both
  Flock constructors 0x52F809 / 0x52F9EC and `GameOSFile::LoadInstance` 0x55AAD4; only a saved game's `Flock::Load`
  0x530930 sets it, at 0x530C15), so the leader always takes the domain branch. Its only reader is
  `Cow::DecideWhatToDo` 0x41D20D: when it is not 0 the leader skips `KeepLeaderWithinDomain`.
- **WANDER** (`Cow::Wander` 0x41D280): in a straight line at `step` per turn and it only changes heading on entering
  another 10 m cell (no wall-hug nor water). The new heading (`SetNewWander` 0x41A3F0) adds, with a speed budget along
  the major axis (`fn_0041A5B0`, its budget fn_0041A660), 0.9 × speed towards the leader if it is far (the distance in
  whole metres, `__ftol` 0x41A421, compared as int with the int radii; the scale is `(9·speed)/10` over `speed`,
  0x41A474..0x41A492, fn_0041A590(v, 9·speed/10, speed)), the flock (`fn_0041AD70`: 1/5 towards the centre of the
  others, the nearest neighbour by axes, the cohesion vector **again**, 3/5 of the neighbour's step, ±(speed/5·8/5),
  −2·speed/5; the distance is compared with raw MapCoords, so it almost always attracts) and a random turn of
  ±turnAngle/2. The step itself (fn_0060BD00 → fn_0060BEA0 → `Object::MoveMapObject` 0x636A40) has no bounds test.
- **Hunger** (50 turns, horse 100): `LookForGrazePos` walks in a spiral ftol((domain / 10.0)²) cells (0x41A8B0, the
  tenth and its square in floats, 0x41A8B3..0x41A8DD, so a 15 m domain goes round 2 cells) from its own, in front
  (±viewAngle/2 = ±90°), neither its own nor that of another member (or its destination), without water nor a fixed
  object (`FindGrazingPosition` 0x41A9D3 `call 0x603790` IsFixed, `MapCell::IsFixed` 0x601EA0: the head of the cell's
  fixed list; accepted only when there is none); MOVE_TO_POS there → **START_TO_EAT** (head-lowering clip once) →
  **EAT** 20..34 times (GameRand(15) + 20), each time one of the two eating clips at random → **FINISH_EATING** (raise
  the head) → DECIDE.
- **Sleep**: a pure counter, without day / night. Full (1000) → SEEK_SLEEP to the sleeping spot (the cell at the
  centre of the domain at birth) → SLEEPS standing (−2 per turn, +1 from ProcessNeeds: ~100 s).
- **Breeding** (3000 turns): only if the flock has fewer members than it had (`maxMembers`): GIVES_BIRTH creates one
  of age 1 in the same flock. `Cow::ReactToAnimalNeeds` adds 1 to the breed count (0x41D343 `call 0x418220` = `inc
  word [ecx+0xEA]`) with no other conditions than `needToBreed != 0 && GetAge >= grownUpAge`, so a lone survivor
  refills its herd.
- Speed: always `speedDefault` (cow, sheep, pig 0.75 m/s; horse 1.5; tortoise 0.25), so always the walking clip.
  Turning in MOVE_TO_POS: `Animal::SetTowardsAngle` 0x418560, at most turnAngle per turn (cow 34 = 6°); with the
  destination inside its turning circle (R = 2 × speed / turnAngle in radians) it turns |diff| − turnAngle × d / R,
  almost fully when close (0x4186B4..0x4186FC), so it does not keep circling the destination. The distance is from
  the target +0x60, else the goal +0x80.
- `Living::KeepLeaderWithinDomain` 0x41AAD0: the leader takes the herd to a new place every stayTime turns;
  `KeepFlockMemberWithinFlockArea` 0x41ABB0 brings a member back.
- `Cow::ReactToAnimalNeeds` 0x41D310: breeding, then hunger, then sleep.
- The wander's straight move is fn_0060BD00. In `SetNewWander` the random turn is `GameRand(turn)` (0x41A509; 0 for
  0, without a draw); the heading follows the step at once (fn_0060C000).
- The speed budget's scale is fn_0041A590 = v × num / den, truncated.
- The flock steering's neighbour distance is fn_0074D090 [inferred |dx| + |dz|].
- `SetTowardsAngle`: the side and the size of the turn come from `GetAngleSign` 0x74D6F0 and `GetAngleDifference`
  0x74D740 (0x4185E9).
- `Cow::LookForFoodPos` 0x41D440 → `Animal::LookForGrazePos` 0x41A8B0 / `FindGrazingPosition` 0x41A980; fn_00419980
  refuses a cell where another member of its flock stands or is going. The spiral is `GUtils::Spiral` over a copy of
  its own MapCoords (0x41A8C5..0x41A8D5): PosWithinDomain 0x41A906, InBounds 0x41A913, FindGrazingPosition 0x41A924,
  then += 0x41A945 (whole cells on the high words).
- `Animal::MoveToPos` 0x41BAF0 → `Living::MoveToPos` 0x5EC270 (`MobileWallHug::MoveTo`; arrived (0xA) →
  SetTopStateToFinal 0x5ECA80).
- Eating: `Animal::StartToEat` 0x418280 gives 15..24 eat clips (GameRand(10) + 15, 0x41828F, Animal.cpp line 0x22E);
  the grazers' `Cow::StartToEat` 0x41D4A0 then makes it 20..34 (GameRand(15) + 20, 0x41D4B4, AnimalCow.cpp line
  0xB7). `Animal::Eat` 0x4182D0.
- Sleep: `Animal::SeekSleep` 0x4180D0 goes to a spot in a square around its sleep cell, 2 m per flock member
  (fn_0074F310); `Animal::Sleeps` 0x418330.
- Breeding: `Animal::GivesBirth` 0x418230.
- Landing: `Animal::Landed` 0x417D50 (CalculeLairPos, then the flock centres where it landed; the predators: their
  lair).
- Flocks: `LookForFlocksInSpiral` / `LookForFlocksAtPos` / fn_005302A0 (0x41A690 / 0x41A790 / 0x5302A0); flocksCanMerge
  is 0 for all 27 species, so DecideWhatToDo's merge never happens ([Flocks, age and smoke](animal-life.md#flocks-age-and-smoke)).
- `StartWander`'s SetTowardsAngle(own angle) only resets the bank zoomer: no effect on the ground. The predators'
  Wander 0x418010 is the same as the Cow's 0x41D280.

### Predators (lion, tiger, leopard, wolf)

Tiger and leopard use the lion's code (the tiger puts its lair in a forest); the wolf has its own. They wander, sleep,
breed and land with the herbivores' functions.

- **Felines** (`Lion::DecideWhatToDo` 0x41FE70): breed; after 22:00 (visual time, `test ah,0x41`) to sleep with the
  counter full (sleep = info.sleep); the leader moves the herd (stayTime lion 200); otherwise, hunger (lion
  12000 turns, tiger 9000, leopard 1200), then sleep; otherwise, wander at 1.5 m/s.
- **Lairs**. The forest list (g_game +0x205BB4) goes from the newest to the oldest; the first is taken without
  scoring and the others score with `fn_0053AD00` = `SigmoidThreshold(-0.9, -(MapCoords distance / 1000))` (the sigmoid
  of the number of trees is computed and discarded), changing only if they score strictly more. Every forest more than
  1000 MapCoords (0.15 m) away scores 0.1144, so in practice **the lair is the second newest forest** (with only one,
  that one), at its grown tree nearest to the centre.
  - Tiger (`Tiger::CalculeLairPos` 0x421470): that rule; without forests or without a grown tree, where it is. Land5:
    the tiger at (2841, 2918), next to forest 19, puts its lair in forest 18 (1358, 3577), 1600 m away.
  - Wolf (`Wolf::CalculeLairPos` 0x421730): the nearest big forest; if there is none, the tiger's rule; if the chosen
    forest has no grown tree or there are no forests, the nearest tree (all of them); otherwise, where it is.
  - Lion and leopard (`Lion::CalculeLairPos` 0x420010): where it is. Land2 has no forests: the tiger stays where it
    is.
- **Wolves** (0x4216B0): to the lair (a square the size of the pack around the centre of the domain, see Lairs) and
  lying in it (HIDE_IN_LAIR, sleeping clip). When hungry (120 turns) they only go out after 23:00: towards the nearest
  flock that is not stronger at 1.2 m (in practice their own: the leader hunts where it is) or towards the nearest
  village.
- **Prey** (`fn_004196D0`): the first in a spiral of 64 cells (±40 m, farSightDistance): another species, on the
  ground, alive, with meat (all except the tortoise), outside their turning circles; a cub only a downed prey
  (0x419808..0x419819). It is chased from the next check, at 8 m/s. **Villagers are also prey** (see
  [Hunted villagers](villager-reactions.md#hunted-villagers)). What cannot be eaten (+0x25 & 0x40) is never prey, nor
  what is controlled by a script (+0x24 & 0x400) except for a hunter that is in a script (see
  [Scripts and flags](animal-life.md#scripts-and-flags)). A prey whose flock is calmed (flock +0x5C, `cmp
  [eax+0x5C],ebx; jne` at 0x419791) is not prey.
- **Chase** (HUNTING_MOVE_TO_POS 41, 0x418DB0): stalks at 1.25 m/s (stalking clip) from 100 to 50 m, sprints at
  10 m/s below 50 m; pounces when it is at pounce stride × scale × 0.5 (~2.3 m) and the prey is within ±22.5° of its
  heading; gives up after 20 s (chaseTime) or beyond 100 m. The step fn_0060ADC0 (0x418DFE) comes before the chaseTime
  and IsHuntingTargetValid tests (0x418E0E..0x418E3E); after the angle abandon (0x418EF6) it does not return: it sets
  the sprint speed and SetAnim(0) for that one turn. Both the chase and the current-target check (fn_00419340
  0x419368, HuntingMoveToPos 0x418DC5) call `IsReachable` (vt +0x530; Villager 0x756460: at home, +0x24 & 4, TOP 236 →
  0).
- **Pounce** (TARGET_POUNCE 40, 0x419010; clip POUNCE_HI, the wolf POUNCE): at 1 m or less the prey falls (life 0.05,
  DOWNED; IsCloseToEqual 0x4190BB, with no "already downed" test); when the pounce stride ends: if it fell,
  `FinishPouncing` (hunger 0, it places itself next to the prey to eat); otherwise, it goes back to chasing it. When
  the target goes, it clears +0xF4/+0xF8 (0x419098) and still moves (0x4190A6) before StartWander.
- **The prey**: DOWNED (falling clip) → BEING_EATEN (lying down) 300 turns → DEAD, carcass 50 turns; the one that
  cannot be eaten (+0x25 & 0x40, 0x5EC4E0) goes to LANDED and stays alive.
- **Eating**: START_TO_EAT → EAT 15..24 cycles; if the prey disappears it goes back to deciding (`Lion::Eat` 0x41FE40;
  also at the end of the meal, so the getting-up clip is never seen).
- `Lion::ReactToAnimalFoodNeeds` 0x41FF40, used by every predator: chase the target seen last turn, otherwise look for
  one.
- `Animal::ReactToAnimalNeeds` 0x4183C0: hunger (no food: on to sleep), then sleep. The wolf's is
  `Wolf::ReactToAnimalNeeds` 0x421950 (hungry and calm, 0x4219B3 → 0x24, no hunt).
- `IsHuntingTargetValid` (vt +0xBB4) of the Animal class, 0x418DA0, only checks that the target is not null; the
  SpellWolf's is 0x420D60.
- Prey search (`FindPrey`, fn_00419490): `GUtils::Spiral` at 0x41953E over a copy of its own MapCoords
  (0x41949A..0x4194B6), InBounds 0x4194D6; in each cell `GetFirstIterator` 0x6034D0 (at 0x4194E9) walks the fixed
  list, then the mobile one (0x41951B..0x419531), each from its head (GetMapChild 0x41950D); the first object that
  passes fn_004196D0 (0x4194F9) wins; the target goes to +0x60 (0x419565).
- fn_004196D0 refuses all but villagers and animals (info type 2 / 4, 0x41975A); the "cannot be eaten" test is at
  0x4196F4.
- `Living::IsSkeleton` (status bit 0x40) is never set for animals, so their corpses are prey too.
- fn_004196D0's "nearer than the stored candidate" compares 2 × Chebyshev(me, prey) in raw MapCoords against the
  Chebyshev of my cell indices and the stored raw MapCoords (fn_0074CF30 / 0x74CED0): a unit mix-up, so in practice the
  new one is always nearer.
- `Animal::SetupMoveToTarget` 0x418D20: a downed prey is eaten, any other is chased (the state only, the clip stays);
  the walk is `MobileWallHug::SetupMobileMoveToObject` 0x60ACD0 (STEP_THROUGH at it), then `Living::SetState`
  0x5F2A80.
- fn_005EC240 is the current move plus SetTopStateToFinal on arrival (used by the pounce). The step at the target
  (fn_0060ADC0) gives no early 0xA within one step; its result is not read.
- `Animal::HuntingMoveToPosAbandon` 0x418FD0. `Animal::FinishPouncing` 0x419120: fed, then it walks to the prey and
  stops one scale-metre short of it to eat.
- `Living::Downed` 0x5EC4B0 plays the dying clip, then BEING_EATEN; `Living::StateBeingEaten` 0x5EC4D0 counts the
  300 turns.
- `Wolf::HideInLair` 0x4219E0 (the in-script leader's test is at 0x421A0E; it reads the calm at 0x421A87 and walks
  every flock, g_game +0x205C44 (0x421A98), including villager flocks, strength +0x12C of any Living).
- `SpellWolf::Wander` 0x420A10. The SpellWolf's vt +0xB48 is `Animal::StartWander` 0x417C90 too: after a hunt it
  wanders, and its Wander runs on.
- Lairs: the forest list (g_game +0x205BB4) is linked by +0x44; the tiger's loop is at 0x4214D2 and the wolf's at
  0x42181A; the chosen forest gives its first grown tree (Forest +0x48: the one nearest its centre). The wolf's
  nearest big forest comes from the BigForest list g_game +0x205CE4.

### Birds (crow, dove, swallow, rock dove, seagull, bat)

All of them are the Dove class (constructor 0x41DCF0); only the clips and the info.dat values change. They are born at
`altitudeNormal` above the ground (20 or 40 m), at 8-10 m/s.

- **Leader** (`Dove::DecideWhatToDo` 0x41DE40 → `StartWander` 0x41DF50 → SPECIAL_MOVE_TO_POS 44): a leg to a random
  point up to 80 m (domainRadius) **from the flock's home**: `Living::GetFlockPos` 0x5ECF60 (called at 0x41DFB8)
  returns the flock's domain centre (+0x14), which CREATE_FLOCK sets (SetDomainCentrePos 0x52FC20) and nothing in the
  Dove code moves, so a land flock keeps about its home (the miracle's flocks move theirs, [Flocks](../magic/flocks.md));
  at its goal's height (vt +0x860 GetDestPos 0x416F70: +0x80, its goal's altitude +0x88; 0x41DFDB..0x41DFE4) ±
  altitudeVariance within altitudeNormal + [altitudeMin, altitudeMax]; another leg on arrival or every stayTime (100 turns). Dove DecideWhatToDo, by contrast, uses `Flock::GetFlockPos` 0x530570 (0x41DED0), the leader's position.
- **Followers** (FOLLOW_FLOCK 45): a point 10 m from the leader, then a formation slot (`fn_0041E890`, read literally)
  and MOVE_TO_POS in 2D at a constant height above the ground; start over.
- **Height** (`Animal::MoveTo3D` 0x418AA0): in flight they keep their absolute height (they do not follow the hills)
  and rise or descend at most altitudeMovementChange per turn (0.2-0.6 m), never less than 2 m from the ground.
- **Banking**: when turning they tilt ±0.5 rad over 2 s (the Zoomer, `Animal::SetTowardsAngle`; TimeToBank /
  BankAngle 0x41F260 / 0x41F270) and level out again over 2 s; `Dove::Draw` 0x41F680 rotates the drawn matrix around
  its forward axis.
- **They never land** in the original game: info.sleep is 0 in all of them, so LAND_AT_POS / SLEEPS are never
  reached. They have no hunger, breeding nor day / night (the bats fly by day).
- **Clips**: the moving one (and the deciding one) is a coin toss between flapping and gliding on each state change
  and on each SetSpeed (the swallow among three, the bat always flaps); its SetAnim never restarts the clip.
- They cannot be picked up nor hit (playerCanPickUp 0) and are not prey (more than 2 m up). When dead they fall with
  physics at the flight speed (`Dove::Dying` 0x41F1B0) and stay on the ground as a carcass.
- The flock's altitude is fn_0041DF20: the flock's own, otherwise the info's altitudeNormal.
- A leg's height (`StartWander`, 0x41DFFD): (altitude + variance) − GameFloatRand(2 × variance).
- `Dove::ReactToAnimalNeeds` 0x41E600: no hunger, no breeding (the sleep branch is dead data: sleep 0 for all birds).
- `Dove::SpecialMoveToPos` 0x41E160: the leader re-steers every stayTime turns and starts a new leg on arrival; it
  ends through fn_005EC2A0 (SetTopStateToFinal, then SetState(0, flock +0x7C)). Its `+0x5E == 2 && d <= 6`
  SetSpeed(speedDefault) at 0x41E1A6 changes nothing.
- `Dove::FollowFlock` 0x41E0B0: when its top state is not the flock's +0x78 it jumps to DecideWhatToDo (vt +0x8C8,
  0x41E0D4); fn_0041E130: once the clip has played it tosses flap or glide again, every turn until the state changes
  (the counter is not restarted). The follow mode 2 (fn_0041E7F0) is never set by the shipped birds.
- `Dove::Dying` 0x41F1B0 starts the physics with its flight velocity along its heading: s =
  ConvertGameAngleToScawenAngle(+0x5C) (0x41F1BB), velocity (sin(s)·v, 0, −cos(s)·v) (0x41F1D5..0x41F220, the
  trigonometry in extended precision, rounded once by the fmul).
- The formation slot (fn_0041E890, 0x41E98F..0x41EA05) does not use AddDistanceFromAngle: x = ftol(((cos(a) × row) ×
  10 + leader.x × 10 × 2^-16) × 65536 / 10), z the same with sin and column, with two roundings (fimul then fmul 10
  [0x8AB470]); a non-member's index is −1 (an edge case).

#### A creature and a bird

- `Dove::CanBePickedUpByCreature` 0x41F190 (vt +0x258): `fld [ecx+0x1C]; fcomp 1.0` → true while the bird's height
  above the land (MapCoords altitude +0x1C, 0 on the ground; `Animal::EndPhysics` zeroes it) is < 1.0, strictly.
- `Dove::CanBeStompedOnByCreature` 0x41F150 (vt +0x268): height < 0.25 x the creature's `GetHeight` (vt +0x42C,
  `Creature::GetHeight` 0x477F50 = size x 15), strictly.
- Every bird is the Dove class (types 11-16, 20, 21); no other animal class overrides either. The others take the base
  ones: pick-up `Object::CanBePickedUpByCreature` 0x4E3A20 (script, town artifact, in the map, size and weight), stamp
  `Living::CanBeStompedOnByCreature` 0x417430 (not dead). No direct caller of the stamp slot was found.
- The other animals' test, `Object::CanBePickedUpByCreature` 0x4E3A20 (also villagers, mobile objects, pots and the
  other classes that keep the base slot), in order:
  1. false when the creature's player kind (GPlayer +0x8E0) is 2, the computer, the creature is not controlled by a
     script (+0x24 & 0x400 clear) and the thing is in a script (+0x24 & 0x200);
  2. `IsTownArtifact` (vt +0x800): 0 for every class reaching here;
  3. false for a villager worshipping at a site (final state 60) of the creature's own player;
  4. false when the thing is not in the map (vt +0x178) unless it is `[[creature+0x160]+0x28]`, the object the
     creature's hand has taken (only set by the LH3DCreature take, fn_00487265 at 0x488301, after
     `PhysicsObject::RemoveObject`; every other writer clears it) (0x4E3AD1..0x4E3AEF);
  5. `CanBePickedUpByCreatureIgnoringWhetherItIsInTheMap` 0x4E3960: false when the creature's leash is tied to it;
     false unless rO + rC <= reach (the two `Get2DRadius`, the reach fn_00485DF0(0xE): the pick-up animations' hand
     points, -0.4 of the back pair and 0.9 of the front pair, its length **in 3D**); false unless the thing's
     `GetWeight` (GetScale³ x `GObjectInfo::weight`, no clamp) <= 0.8 x the creature's `GetWeight`
     (`LH3DCreature::GetMass` 0x47FA80); then the thing's `CanBePickedUp` (vt +0x628: Living / MobileObject not
     CannotBePickedUp, a villager also not inside its home, Object 0). Both comparisons pass when equal and on NaN.
- A villager (Villager and SpecialVillager, the same slots) takes the own-worshipper test at 0x4E3A7A..0x4E3ACE:
  `GetFinalState` (vt +0xB04, 0x751DD0; the top state +0x8C if its state row's +0x1C is set, else the destination
  +0x8D) is 0x3C (WorshippingAtWorshipSite) and `Villager::GetPlayer` 0x7502F0 (the town's +0x2C, none without a town)
  is the creature's player. Its weight is `Object::GetWeight` 0x638480 = GetScale³ x its GVillagerInfo row's `weight`
  (82.5 on every row of the shipped info.dat), its radius `Object::Get2DRadius` 0x638180, and its say
  `Villager::CanBePickedUp` 0x55CA50: no while +0xE0 & 4 (set by ArriveHome 0x751FAC, cleared by LeaveHome 0x751FE0),
  else not CannotBePickedUp. PieceVillager and PuzzleVillager are animals (IsVillager 0) and go the animals' way.
- A mobile object is made by `MobileObject::Create` 0x607000 as the class its row's MobileObjectType (+0x104) says:
  type 21 a Creed (the Creed row and the Eyes row 22), whose test 0x50B500 is always true; the Scaffold's 0x4E3FC0 is
  `ValidForPlaceInHand` 0x6E96A0 and then the base test, with the GScaffoldInfo row's weight; Poo, FieldCrop and the
  plain MobileObject run the base test with their row's weight and `MobileObject::CanBePickedUp` 0x425B30 (not
  CannotBePickedUp). A pot (`GPotInfo::potType` 0: the food and wood pots, the branches, logs, fruit, wheat and fish in
  the hand) runs the base test with its row's weight; every pile (`PileResource::CanBePickedUpByCreature` 0x66E390: the
  food, wood, magic and store piles, and the hand's food and wood piles) asks only `!CannotBePickedUp &&
  GPotInfo::canBecomeAPhysicsObject` and, as the pick-up starts, that it is on the map. So a pile on the land, whose
  row may not move, is never lifted; the creature eats from magic food through a wheat portion made before it.
- The hand points are measured at size one once per body build and kept, as fn_004EE130 keeps them behind +0x524C
  (set at 0x4EE29F, cleared by fn_0048D750 from `LH3DCreature::Init` and `MorphAnims`). The summed vector is scaled by
  the drawn scale (+0x94, written with +0x90 by `LH3DCreature::SetSize`) before its length. The creature's weight:
  `GetMass` of +0x90 (the drawn size, `SetSize` 0x480599 its only writer) and the thin-fat and weak-strong targets
  (+0xA4, +0xAC). The leash: the original takes the first of its player's interface statuses with a leash on (one
  leashable creature per player, so the same creature).

### Addresses

The original's functions each part of openblack's animal code reproduces, as an audit of the animal AI read them.

| openblack | The original |
|---|---|
| Priority 28 (FleeFromPredatorPriority) | Living 0x5F15D0, Lion 0x4203C0 |
| Priority 7 / 9, Score, Standard, TurnsToReact, TurnsBeforeReactingAgain | 0x5F1710, 0x5F1800, 0x6E4620, 0x5F18C0.. |
| IsAvailableForReaction | 0x5F11F0 |
| AnimalReaction (no reaction yet / the switch) | 0x6E40D0..0x6E412F / 0x6E4134..0x6E42E1 |
| AddReaction | 0x5F0F30 |
| StartReacting / SetupReactToFlyingObject | 0x420410 / 0x5F14C0 / 0x4204A0 |
| FleeingPosition | 0x420550 (moving) / 0x5F2010 (still); the draws Animal lines 0xCC then 0xCD, x then z, side test, 4 m |
| MovementOf | GetMovementDirection 0x60C040: the physics direction when +0x24 & 0x40, otherwise always the step (0x60C056) |
| ComingTowards | fn_005F1E60: 3D (0x5F1E71..0x5F1EE5), `>= 0.8` at 0x5F1FE6 |
| ProcessReaction | 0x5F1270 |
| FleeingFromPredatorReaction | 0x4201F0 (the block 0x420292..0x420322 is dead code in the exe) |
| FleeingFromObjectReaction + FleeFromObjectIfComingTowardsMe | 0x5F1D10 / 0x5F1D90 |
| FleeingAndLookingReaction (state 30) | 0x5B00E0 → vt +0x890 0x5F2420 → LookingAtObjectReaction 0x5F23A0 |
| StopReactingAndSetState / ResetStateAfterReacting | 0x5F11C0 / 0x41A280 |
| GotoFoodReaction / ArrivesAtFoodReaction | 0x5F2550 / 0x41A0A0 |
| IsGrazer / HunterOf / class dispatch | vt +0x8C8/+0xBBC/+0xB48/+0xB4C/+0xBD0 of every species |
| Step / AngleOfMapCoords / Scale / SquarePos | fn_0041A590 / fn_0074F310 |
| IsPosValidForTurnAngle | 0x41B210 |
| FlockPos / PosWithinDomain | 0x530570 / 0x5ED010 |
| CallExitStateFunction / SetTopState / PlayAnimThenSetState / SetupMoveToPos | 0x41A2C0 / 0x5F28E0 / 0x5ECAC0 / 0x5F2830 |
| InitialScaleForAge / ScaleForAge / SetAge / GetAge | 0x417B20 / 0x417A40 / 0x4179C0 / 0x5ECAF0 |
| IsDead | 0x417270 |
| ProcessAnimal (the lone predator's lair) | fn_00419C20 0x419CD2 |
| ReleaseFromScript | fn_0041AA00 (+0x24 & 0x44, own flock with info +0x25C / ftol +0x21C, SetDying then DEAD, InteractDecideWhatToDo at once) |
| Merge (flocks) | fn_00530210 (+0x88 += other's each step) |
| Leader / IsLeader | tail 0x5ECF70 |
| Dove follow mode 2 | fn_0041E7F0 (not ported, never set) |

### Sources

- The executable only (bw1-decomp's animal files are empty stubs), read with the xref index; info.dat dumps of the
  animal and state tables; the oracle for the land circles.

## openblack

openblack differs from the exe in the audit rows D1, D3, D4 and D6 to D14 (listed with their files in
[Pending](#pending)), fix rows.

**Done**: the whole animal AI, in `ECS/AnimalAI.*` (turn, herbivores, hand, death, movement states),
`ECS/AnimalPredators.cpp` (predators and hunting), `ECS/AnimalLairs.cpp` (lairs), `ECS/AnimalBirds.cpp` (birds),
`ECS/AnimalFlee.cpp` (the reactions), `ECS/AnimalAIDetail.h` (what they share), `ECS/AnimalAnimations.*` (clip per state
and species; `BirdClip` for the birds), `ECS/AnimalApi.cpp` (functions for spells and scripts, in `AnimalAI.h`),
`ECS/AnimalDebugHooks.cpp` (test hooks), `ECS/DisappearSmoke.*` (was `ECS/SmokyStuff.*`; smoke from the carcass),
`components::AnimalBrain` (the Living / MobileWallHug / Animal fields that the AI uses; `leaderTurns`),
`components::Flock`, `ECS/ScriptHeld.*` + `components::ScriptHeld` / `CannotBeEaten` (what the scripts hold and their
flags) and `ECS/AnimalScript.cpp` (the animal that a script releases). Progress:
[animal](../project/progress/) (row by row). Animal state lives in ECS components and the systems are reached through
Locator services; the AI's formulas stay pure functions, unit tested with fakes (see
[Internals](../project/internals.md)).

- **The hunted villager** is driven by `components::DownedVillager`; `ecs::villager::VillagerDead` kills it at the end
  of the turn.
- **Birds**: the bank is drawn in `MobileDrawing`.
- **A creature and a bird**: `animals::birds::CreatureCanPickUp` / `CreatureCanStompOn` (`Animals/BirdRules`), the
  animal system's `CanBePickedUpByCreature` / `CanBeStompedOnByCreature` on `animal_ai::Altitude`; the creature's
  `CanPickUp` asks the first for a bird (`animal_ai::IsDoveClass`). The stamp test is not called (its caller is not
  known). The base test is `creature_pick_up::CreatureMayPickUp` (`Creature/CreaturePickUpRules`), the reach
  `creature_reach::MaxReach3D`, wired for the animals that are not birds and for villagers in
  `CreatureObjectActionSystem::CanPickUp` (`PassesPickUpTest`): `creature_pick_up::IsOwnWorshipper`,
  `components::AtHome`, `creature_pick_up::VillagerAllows`; the mobile object's type is `GMobileObjectInfo::mobileType`;
  the piles `creature_pick_up::PileAllows` and the creature eats from magic food through `EatFromFoodPile`. The hand
  points are kept in `components::CreaturePickUpReach`; the body is posed on demand when it has not been
  (`CreatureAnimationSystem::EnsurePosed`); the drawn scale is `creature_pose::DrawnScale`, the drawn size
  ClampScale(`creature_pose::DrawnSize`), the targets `creature_morph::FromAttributes`; the leash is this creature's
  worn leash.
- **Land 1's sheep**: `Land1.txt` creates none. The 9 sheep are created by the challenge "The Lost Flock"
  (`challenge.chl`, script `TheLostFlock`), which `LandControl1` launches only after the `FollowUs` and `CitadelGuide`
  intro. In openblack the intro does not finish, so without a skip they do not appear; with a SkipBox answer that
  skips the tutorial `LandControl1` starts `TheLostFlock` and its CHL `CREATE` makes them (not checked in game). Land 2
  does create them in the map.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Test hooks

`OPENBLACK_ANIMAL_TRACE=1` (state changes, landType and every 50 turns how many there are in each state),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distance[,angle[,every]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turn[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turn"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n only counts that species),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (that species hungry on turn 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turn>` (the
predators spread their flee reaction again), `OPENBLACK_TEST_HUNT_VILLAGER="<species>,<turn>"`,
`OPENBLACK_TEST_FOOD_PILE="<species>,<turn>"`, `OPENBLACK_TEST_FOOD_BEHIND="<species>,<turn>[,<type>[,<m>]]"` (the
first of that species, hungry, 8 m in front of the nearest building (0; not a field, which the circle iterator skips)
or feature (2) of 2..8 m radius or tree (1) and a food pile m (6) behind it; its route every 2 turns; with the trace,
each change of the detour), `OPENBLACK_TEST_SMOKE=<n>`, `OPENBLACK_TEST_CORPSE_TURNS=<n>`,
`OPENBLACK_TEST_LAIRS=<turn>` (lists the forests and each predator leader recomputes its lair; with the trace, the
chosen lair and the rule), `OPENBLACK_TEST_SCRIPT_HELD="n,turn[,release]"` (the n-th animal is held by a script as if
it had been created by a CREATE; at `release` it loses the reference; every 25 turns its state, counter and flags),
`OPENBLACK_TEST_CANNOT_BE_EATEN=<turn>` (on that turn all the animals and villagers receive the vortex flag; with
`HUNT_VILLAGER` before the downing there is no hunt, after it the villager gets up), `OPENBLACK_TEST_VIEW_LOCK=1` (the
camera is placed every turn next to the animal, for the birds). Land2 (`-s Land2.txt`) has lions, tigers and wolves.

## Pending

Differences an audit of the animal AI found in openblack's code against the exe, each checked against the exe and the
current code; each is fixed as its own measured commit. Done: D2 (`Animal::age` was written only while below
grownUpAge, so 20-40 % of the lions and wolves never hunted; the reads now take the live age) and D5 (the
coming-towards test is 3D), and D3's centre (it was the leader's `FlockPos`, it is now the domain centre).

- openblack differs (D1, high, switching reactions): `brain->reaction = 0;` before `StartReacting`, and only the
  `current->id == reaction.id` test, so AddReaction stores the reaction state as the previous one and the animal can end in the invalid state after a second throw (reaction 9 lasts up to 80 x 1.5 turns); the same-type
  gate (9 → 9, 7 → 7) is missing (`src/ECS/AnimalFlee.cpp:356`, `:340`), a fix row. The exe's switch path 0x6E428A
  `SetReactionDoneWhen(type)`, 0x6E4292..0x6E42D3 takes the living off the old reaction's list, then 0x6E42E1 `call
  [eax+0x994]` StartReacting with +0x94 still the old reaction; AddReaction 0x5F0F33 skips StorePreviousState when
  +0x94 != 0 (`jne 0x5F0F46`); 0x6E41B3 checks the same type, 0x6E41BE needs the table flag (only 10, 28 and 35),
  0x6E41CC a different reaction; the records like 0x6E428D. The villager side already has the check
  (VillagerReactions.cpp:162).
- openblack differs (D3, high, bird legs): the centre is the domain centre now, but the base altitude is
  `ctx.brain.altitude + variance`, where `Dove::StartWander` 0x41DF50 takes the destination's (`GetDestPos().y`, vt
  +0x860 = 0x416F70, living +0x80) + variance (+0x270) − GameFloatRand(2 variance) (0x41DFDB..0x41E00E)
  (`src/ECS/AnimalBirds.cpp:257`, should be `ctx.brain.goalAltitude`), a fix row.
- openblack differs (D4, medium, movement direction): an animal returns 0 when it did not move; the exe
  (`MobileWallHug::GetMovementDirection` 0x60C040) gives the physics direction when +0x24 & 0x40, otherwise always the
  step (0x60C056) (`src/ECS/AnimalFlee.cpp:120`), a fix row.
- openblack differs (D6, medium, grazing spot): the IsFixed test is missing (`FindGrazingPosition` 0x41A9D5 rejects a
  fixed cell; `map_cells::IsFixed`) (`src/ECS/AnimalAI.cpp:842`), a fix row.
- openblack differs (D7, low-medium, breeding): it repeats ProcessNeeds' conditions (two members, room, breed < need),
  where `Cow::ReactToAnimalNeeds` 0x41D341 adds 1 to the breed count unconditionally for an adult (fn_00418220 = `inc
  word [+0xEA]`), and at the need sets it to 0 and gives birth (0x32) if +0x88 > +0x48 (`src/ECS/AnimalAI.cpp:883`), a
  fix row. A lone survivor never breeds in openblack.
- openblack differs (D8, low-medium, the flock's calm, +0x5C): written only (`src/ECS/ScriptContainers.cpp:718`), where
  the exe reads it in `Wolf::CalculeLairPos` 0x421747, `Wolf::ReactToAnimalNeeds` 0x4219B3 (!= 0 → 0x24),
  `Wolf::HideInLair` 0x421A87 and fn_004196D0 0x419791, a fix row. Whether Land 1 or Land 2 calms a predator flock is
  unverified.
- openblack differs (D9, low, looking at the object): an instant turn, where the exe's LookAtObject(obj, 1) turns 0x80
  a turn (`LookAtPos` 0x5EC56C, a snap within it 0x5EC5AA, else ±step 0x5EC5C2..0x5EC5EE; callers 0x5F1DF1, 0x5F240A)
  (`src/ECS/AnimalFlee.cpp:629`, `:656`), a fix row.
- openblack differs (D10, low, wander): MoveBy only refuses at the edge, where `MoveToWander` 0x60BD00 makes up to 3
  cardinal tries (MapXZDirections, the bounds, CollideWithMapCell), angle quadrant << 9, step LHSinTable × (speed >> 4)
  >> 12 (0x60BE52..0x60BE86) (`src/ECS/AnimalAI.cpp:563`), a fix row. The step's scale is not settled.
- openblack differs (D11, low, order): `++turnsSinceStateChange` before ProcessReaction, where `ProcessLiving` runs
  ProcessReaction 0x5EC8C6 and then ProcessState (vt +0x620), which increments +0x90 at 0x417EE6; and the food target
  (+0xFC) is dropped only when not Available, where the exe also drops it on +0x24 bit 4 (0x417F38; its one `or`
  setter, fn_005FAFC0, is not read yet) (`src/ECS/AnimalAI.cpp:1299`, `:1303`, `:1321`), a fix row.
- openblack differs (D12, low, a lone predator's lair): computed at its first turn, where fn_00419C20 (the packet
  0x63C4D2 and script 0x6F1355 creation) calls CalculeLairPos (vt +0xBD0) at creation (0x419CD2); the lairs differ if
  the map script makes its forests after the animals (`src/ECS/AnimalAI.cpp:1551`), a fix row.
- openblack differs (D13, low-medium, IsReachable): `Available` instead of `villager::IsReachable`, where
  `HuntingMoveToPos` 0x418DC5 tests vt +0x530 (`Villager::IsReachable` 0x756460), so a lion keeps chasing a villager
  who went into a house until chaseTime (`src/ECS/AnimalPredators.cpp:569`, `:319`), a fix row.
- openblack differs (D14, low, the hunt's order): the tests come first and it returns after the abandon, where the exe
  steps (`MoveToObjectPos` at 0x418DFE) before chaseTime (0x418E22) and `IsHuntingTargetValid` (0x418E36), with the
  angle window fn_00418CD0(…, 0x100), and after the abandon (vt +0xBB8, 0x418EF6) goes on to the speed and vt +0x8FC(0)
  (`src/ECS/AnimalPredators.cpp:569`, `:590`), a fix row. Whether `preyCell` is cleared when the target goes is not
  checked.
- **A flock's SET_POSITION** (a lead, not an audit row): the exe writes the leader's +0x80 (its destination) through
  `SetDomainCentrePos` 0x52FC20, not the flock's +0x14; openblack writes `domainCentre` (CHLApi.cpp:1690), which the
  birds' StartWander reads.
- **FollowFlock** (AnimalBirds.cpp:312): returns where the exe goes to DecideWhatToDo.
- **The order** of forests and animals in the map scripts is unverified; none of the
  in-game effects above was seen on screen.
