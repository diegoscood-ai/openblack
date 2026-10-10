# Fighting

Creatures fight each other in duels inside a circular arena. The player directs their own creature with the hand,
clicking the opponent's body high, middle or low to strike, their own creature to block and the ground to step, or
leaves it to fight by itself as it has learnt to. Nobody dies in a fight: the loser faints, is carried home and recovers.

**Progress: 22/56 done, 16 partial — 54%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Starting a fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An angry creature can choose to fight another creature it sees | done | `CreatureFightSystem::ProcessTurn` starts a fight when an angry creature has another near (`fight::WantsToFight`); test `CreatureFightSystemTest.WhatStartsAFight`; see [decision_making.md](decision_making.md) |
| Tying the leash from the player's creature to another creature starts a fight | done | `CreatureFightSystem` takes the leash's act-on list (`CreatureMindState::leash.actOn`, filled by `LeashSystem`); see [leash.md](leash.md) |
| A creature too badly hurt won't start a fight | done | `creature_fight` health threshold; test `CreatureFight.OutcomesAndStarting` |
| Asked to fight, the other creature can refuse, and the player is told why | todo | the help text of refusals is in [lessons_and_help.md](lessons_and_help.md) |
| Neither creature may already be fighting or lying knocked out | done | `CreatureFightSystem::StartFight` returns `StartResult::Busy` |
| The creature that starts it makes an arena between the two, sized by the bigger creature, with a cap | done | test `CreatureFight.ArenaIsSizedByTheBiggerCreatureAndCapped` |
| An arena already near is reused rather than a new one made | todo | (unconfirmed how near) |
| The arena is marked out on the land and shown while the fight lasts | todo |  |
| The creatures walk to their places on either side, point at the arena, and play their fight start | partial | they walk to their places, taunt and play the start (`CreatureFighting` stages approach, taunt, ready in `CreatureFightSystem`); pointing at the arena is todo |
| Scripts can start a fight and set where the loser goes | todo | only the debug spawner calls `StartFight`; `SET_FIGHT_EXIT`, `GET_ARENA` are stubs in `src/CHLApi.cpp` |

## The player's controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the opponent's body strikes at the band clicked: high, middle or low | partial | `CreatureFightSystem::Press` and `fight::BandAt` (tests `CreatureFight.ClickHeightPicksTheBand`, `CreatureFightSystemTest.APressChargesOnThePlayersFighter`), but no hand code calls `Press`, so the player cannot click in a fight |
| Clicking their own creature blocks | partial | in `CreatureFightSystem::Press`, which the hand never calls |
| Clicking the ground steps forward, back or sideways, by the longer axis of the click | partial | test `CreatureFight.GroundClickStepsAlongTheLongerAxis`; not reached from the hand |
| Stepping back or sideways is only allowed within the arena; forward always | done | test `CreatureFight.StepsBackOrSidewaysOnlyWithinRange` |
| Holding the click charges a blow, up to 1.2 seconds, playing a power-up while it waits | partial | test `CreatureFight.ChargeRunsToOnePointTwoSeconds`; not reached from the hand |
| Moves queue up, at most twelve; a click can replace the queue | done | test `CreatureFight.QueueHoldsTwelveAndAClickReplacesIt` |
| Being hit takes away a charged blow still waiting | done | test `CreatureFight.GettingHitTakesAWaitingBlowAway` |
| Moves are taken from the queue only in range, from the stance or a block, and a blow only once charged | done | test `CreatureFight.OrdersAreTakenInRangeAndOnceCharged` |
| A special move, the species' big blow, at twice the force | partial | the special order plays and lands at twice the reach (`CreatureFightSystem`); how the player calls it in the game is unconfirmed and not wired |
| Casting a miracle in a fight: attacking ones at the opponent, defending ones on itself | todo | the fight system casts no miracle; only the debug spawner lists spell orders |
| Miracle icons appear by the arena for the miracles the creature can use in fights, to pick from | todo |  |
| The hand's tooltip during a fight says what a click will do | todo |  |
| The player taking the creature over gives it back to the computer after 15 seconds without a move, 3 at the start | partial | test `CreatureFight.PlayerMovesTakeBackControlAndTeach`; the player's fighter is under player control (`CreatureFightSystem::StartFight`), but with no clicks it is always given back |

## Blows and damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each creature has a fight health, starting at half its life plus a half, and a stamina | done | test `CreatureFight.HealthStaminaAndLife`; `fight::FightHealthAtStart` in `CreatureFightSystem::StartFight` |
| A blow's force is the attacker's size and strength against the defender's, times its speed, within limits | done | test `CreatureFight.DamageFollowsTheFormula` |
| A blocked blow does a tenth of the damage | done | test `CreatureFight.ABlockedBlowDoesATenth` |
| Blows land where the attacker's hand reaches; the creature steps in to land the chosen band | done | test `CreatureFight.ABlowIsChosenThatLandsAtTheBand` |
| The fighters never pass through each other | done | test `CreatureFight.FightersNeverPassThroughEachOther` |
| A blow throws the victim back or reels it to the side, top or bottom by where it lands, with a wobble | done | test `CreatureFight.RecoilsByHeightAndDirection` (top or bottom for high blows is a guess, noted in `src/Creature/CreatureFight.h`) |
| Blows cut, bruise or graze, leaving wounds, some bleeding | partial | `CreatureFightSystem` adds a wound at a random place on the skin (`CreatureSkinSystem::AddWound`); test `CreatureFight.WoundsByTheBlow`; which wound each blow leaves is guessed (see [marks.md](marks.md)) |
| Sparkles fly where blows land | todo |  |
| Blow and block sounds, the crowd's cheers, and fight music | partial | blow sounds play from the animations (`CreatureAudioSystem`, [animation.md](animation.md)); cheers and fight music are todo ([../audio](../audio/)) |
| Miracles cast at a fighter make it reel | todo | no miracle reaches a fighter |
| Stamina comes back slowly each turn | done | `fight::RegainStamina` in `CreatureFightSystem` |
| The game keeps counts of attacks, blocks and steps for the statistics | todo | kept in the player's help record and shown on Tech Stats ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |

## Fighting by itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Left alone the creature chooses its own moves by what its opponent is doing | done | `fight::ChooseMove` from `CreatureFightSystem`; tests `CreatureFight.ComputerChoosesByWhatTheOpponentDoes`, `OpponentStatesAsTheComputerSeesThem` |
| How aggressively it fights follows what it has learnt: each move the player makes nudges it towards attack or defence | partial | tests `CreatureFight.PlayerMovesTakeBackControlAndTeach`, `ComputerKeepsTheGameTiers`; the tendency is kept in `CreatureFightRecord`, but with no player clicks nothing nudges it |
| A creature's first fight starts it leaning by its alignment: evil attacks, good defends | done | `fight::FirstTendency` in `CreatureFightSystem::StartFight` |
| Scripts can make a creature fight by itself whatever the player does, and ask whether it does | partial | `CreatureFightSystem::SetAutoFighting` exists (debug spawner only); `SET_CREATURE_AUTO_FIGHTING` and `IS_AUTO_FIGHTING` are stubs |
| Scripts can queue blows, steps and spells for a creature and read its moves | todo | `SET_CREATURE_QUEUE_FIGHT_*`, `GET_CREATURE_FIGHT_ACTION`, `CREATURE_FIGHT_QUEUE_HITS` are stubs |
| Computer players' creatures fight with the computer's tactics | partial | any creature not the local player's fights by the computer moves; there are no computer players ([../multiplayer](../multiplayer/)) |

## The end of a fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At no fight health a creature faints; the other wins | done | `CreatureFightSystem`; test `CreatureFight.OutcomesAndStarting` |
| A quarter of the fight health lost comes off the creature's real life | done | `fight::LifeAfterFight` in `CreatureFightSystem`; test `CreatureFight.HealthStaminaAndLife` |
| The loser lies out cold for a while by its size, is carried home (fading out and in), rests until healthy enough and gets up | done | `CreatureFightSystem::KnockOut`, `CreatureKnockedOut`, `HomeOf`; test `CreatureFightSystemTest.KnockedOutItIsTakenHomeAtOnce` |
| Spells on a knocked-out creature are brought to their end | todo | `CreatureFightSystem::KnockOut` leaves its spells as they are |
| After the fight each creature responds: the winner celebrates, the loser sulks, and they remember how they feel about each other | partial | the winner plays its finish and celebrates (`CreatureFighting::Stage::Celebrate`); the loser's response and the change of attitude are todo |
| Creatures never die in a fight; only a script kills one for good | partial | fights never kill; `CreatureFightSystem::KillPermanently` exists but only the debug spawner calls it |
| A fight can be called off with no winner | done | `CreatureFightSystem::AbortFight`, used when a fighter is lost or leaves |
| The fight panel shows each creature's name over a bar of fight health and a thinner bar of stamina | partial | `src/Creature/CreatureFightHud` (test `CreatureFightHud.LayoutAndColours`) and `CreatureFightSystem::GetPanel` exist, but nothing draws the panel |
| The camera goes to watch the player's creature fight from the side of the arena, and the player can leave the fight view | partial | `CreatureFightSystem::Watch` frames the duel from the arena's side; it follows the fighters as they move where the game keeps still, and leaving the view is todo |
| The fight shows text on the screen as it ends | todo |  |
| Villagers nearby gather to watch a fight and react to it | todo | see [../villager](../villager/) |
| Creatures and animals nearby react to a fight | todo | see [reactions.md](reactions.md) |
| Fights won earn belts and medals shown in the Creature Cave | todo | see [creature_cave.md](creature_cave.md) |
| Arenas are saved and loaded with the game | todo | no saved games |
| The tutorial for the learning to fight stage of growing up shows how to fight | todo | see [development_phases.md](development_phases.md) |
