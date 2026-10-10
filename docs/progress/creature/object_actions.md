# Object actions

Everything a creature does with the things around it and on the spot: picking things up and holding them, looking them
over, eating and drinking, sleeping, its bodily functions, throwing, knocking things down, and the gestures and
expressions it makes. Each action is chosen by the creature's mind (see [decision_making.md](decision_making.md)) and
played on its body by animations, with the thing taken hold of or let go of at a moment of the animation.

**Progress: 57/123 done, 17 partial — 53%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Picking up and holding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature walks up to a thing, stopping short of it by both their sizes, and reaches for it | done | `CreatureObjectActionSystem` (pick up), `CreatureLocomotionSystem::MoveToObject` |
| Reaching blends four reaching animations (front and back, left and right) by where the thing lies | done | `src/Creature/CreatureReach`; tests `CreatureReach.ACornerTakesAllTheWeight`, `TheMiddleBlendsAllFourEvenly` |
| A thing on the creature's left is reached for with the other hand, the animations mirrored | done | test `CreatureReach.TheOtherSideIsReachedMirrored` |
| Something out of reach of the four animations makes it step closer and try again, then give up | done | tests `CreatureReach.StretchingPastTheCornersExtrapolatesThenFails`, `CreatureIdleMind.AnObjectStepWaitsForTheHandsAndGivesUpWhenTheyFail` |
| The thing is taken hold of at the moment of the animation set for the species | done | `CreatureObjectActionSystem::Moment`, at the take-hold time of the species' animations |
| What it holds rides in its hand, held by its middle, turned with the hand | done | `components::CreatureHeldObject`, drawn by `CreatureObjectActionSystem::UpdateHeldDraw`; test `CreatureObjectActionSystemTest.TakenHoldOfAndLetGoOfThingsAreDrawnAtOnceWhereTheyAre` |
| Only light enough things can be picked up; the heavier the creature is the more it can lift | partial | anything mobile, villagers, animals that may be put in a hand, and food pots can be picked up (`CreatureObjectActionSystem::CanPickUp`); no weight limit by size (see [physiology.md](physiology.md)); scaffolds have their own pick-up and stealing rules (see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md)) |
| A villager picked up stops what it was doing | done | `CreatureObjectActionSystem::Moment` takes it out of the physics and the map as the player's hand does (`ecs::living::InterfaceSetInMagicHand`) |
| Carrying something heavy makes the creature stronger over time | done | `CreatureObjectActionSystem::ProcessTurn` with the species' carrying rate; test `CreaturePhysiology.CarryingMakesItStronger` |
| A young creature that hasn't learnt to pick things up can't, and is told so | todo | see [development_phases.md](development_phases.md) |
| The player can give the creature something by dropping it into its hand, and it takes it | todo | the hand can't hand things to a creature; see [../hand](../hand/) and [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Holding something it has no more use for, it puts it down | done | `creature_mind::PutDownHeld` (`src/Creature/CreatureIdleMind.cpp`) |
| Holding a villager always frightens the creature's town | done | `creature_object_actions::AttitudeTo`; test `CreatureObjectActions.TheTownFearsThrowingAndEatingVillagers`; the town's view is only read by the debug spawner |

## Looking things over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Examine by picking up: it picks the thing up and turns it over in its hand | done | plan action `ExamineByPickingUp` (`src/Creature/CreaturePlanActions.cpp`); test `CreatureIdleMind.ACuriousCreatureLooksSomethingOver` |
| While holding it, it strokes, shakes, smells or looks closely at it | done | the four keep animations (`creature_object_actions::k_KeepAnimationCount`, `Kind::Keep`) |
| Examine by looking: it goes up to the thing and looks at it for a while | done | plan action `ExamineByLooking` |
| Examine by following: it follows a living thing about for a while | done | plan action `ExamineByFollowing`; `CreatureLocomotionSystem::Follow` |
| Examine a place: it goes to where something happened and looks round there | todo | no reaction to where a miracle struck in our tree |
| Inspect another creature: walks up to it and looks it over | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| Look at what it holds in its hand, eat it, stroke it or throw it, as chosen for the thing | partial | the keep animations and eating play; choosing among them by the thing is the idle rule only (`CreatureIdleMind.cpp`); see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Look but don't approach, or look at something forever, as scripts and reactions ask | todo |  |
| What it learns about a thing it examined counts towards its beliefs about such things | partial | see [beliefs_and_opinions.md](beliefs_and_opinions.md) |

## Eating and drinking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eat after examining: picks up food, looks it over and eats it | done | plan action `EatAfterExamining` and the hunger need; tests `CreatureIdleMind.EatingIsPickingUpExaminingAndEating`, `CreatureNeedsMind.ItPicksFoodUpAndEatsIt` |
| Eat alive: grabs a living thing (villager, animal) and eats it | done | plan action `EatAlive` (live food target) |
| Eating a villager removes them from their home and town and frightens the town | done | `Consume` in `CreatureObjectActionSystem.cpp` kills it (`ecs::life::Kill`); tests `CreatureObjectActionSystemTest.AVillagerEatenDiesAsAnyDeathGoes`, `CreatureObjectActions.TheTownFearsThrowingAndEatingVillagers` |
| Each thing eaten is worth the food value the game's tables give its kind | done | `CreatureObjectActionSystem::FoodValueOf` (villagers, animals and things by their tables, pots by the food in them) |
| Eating fills it, fattens it if it overeats, and builds up poo | done | `CreaturePhysiologySystem::Eat`; see [physiology.md](physiology.md) |
| Stomp and eat: stamps on something first, then eats it | todo |  |
| Stone and eat: kills something with a thrown stone, then eats it | todo |  |
| Eat from the storehouse's food pile where it lies | partial | plan action `EatFromFoodPile` eats pots and piles of food it can pick up; eating from the storehouse in place is todo |
| Eat from a field, pulling up the crops | todo | see [../resources](../resources/) |
| Eat from a tree, shaking fruit down | todo |  |
| Eat from a magic food pile | partial | magic food piles count as food pots in `FoodValueOf` (unconfirmed they are all eaten the same way) |
| Fish and eat: wades in, scoops up fish and eats them | todo |  |
| Food it can't pick up is given up uneaten | done | test `CreatureNeedsMind.FoodItCantPickUpIsGivenUpUneaten` |
| Poisoned food makes it sick and holds back its hunger for a while | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Drink from the sea: goes to the water's edge, kneels and drinks | done | plan action `DrinkFromTheSea`; test `CreatureNeedsMind.ItDrinksAtTheWatersEdge` |
| Drinking quenches its thirst and it doesn't want water again for a while | done | `CreatureMindSystem` (`k_WaterSuppressSeconds`) |
| Eats the grain from fields only when the time is ripe | todo | (unconfirmed what "ripe" means here) |
| Eating gets the creature's own eating sounds | done | from the animations' sound moments (`CreatureAudioSystem`); see [animation.md](animation.md) |

## Sleeping and resting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sleep on the spot: lies down, sleeps with its eyes closed until rested, gets up | done | plan action `SleepOnTheSpot` and the tiredness need; test `CreatureNeedsMind.ItSleepsWithItsEyesClosedUntilRested` |
| Sleep by something (a tree, its home, a friend) | todo |  |
| Sleep at a given place, as scripts and its home ask | todo |  |
| Sleeping heals it and rests it, faster asleep than awake | done | `CreaturePhysiologySystem`; see [physiology.md](physiology.md) |
| Rest to get better: lies down when hurt until healthier | partial | after a knock-out it rests until it comes round (`CreatureFightSystem`, `CreaturePhysiologySystem::WakeFromFaint`); resting by choice when hurt is todo |
| Rest on the spot for a little while | todo |  |
| Sit down for a while, and get up | done | plan action `SitDown`; tests `CreatureIdleMind.SometimesItSitsForAWhile`, `AnAbandonedSitEnds` |
| Faints wherever it is when exhausted, starved or out of life, and comes round later | done | test `CreatureNeedsMind.FaintedItLiesStillThenComesRound`; see [physiology.md](physiology.md) |
| Woken up, it looks dazed for a moment | todo |  |

## Bodily functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Poo: squats and has a poo, which takes about four seconds | done | plan action `Poo`; test `CreatureNeedsMind.APooTakesFourSecondsAndDropsAsItEnds` |
| The poo is left on the ground as a thing others can see, step in or pick up | partial | `CreaturePhysiologySystem::Poo` leaves a lump of poo (a mobile object) behind it; nothing steps in it or reacts to it |
| Poo discreetly: goes somewhere out of the way first | todo |  |
| Some things can be pooed on and others not | todo |  |
| Puke: when ill, it is sick | done | plan action `Puke`, `CreaturePhysiologySystem::Puke` |
| Fart | todo |  |
| Sneeze | done | plan action `Sneeze` (an emote) |
| Shiver when cold | done | plan action `Shiver` |
| Show it is hot: fans itself | done | plan action `ShowHotness` |
| Scratch an itch | done | plan action `Scratch` |
| An evil winner of a fight poos on the loser | done | `CreatureFightSystem` (evil winners walk up to the loser and have a poo) |

## Throwing and letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hurl: picks something up and throws it hard at a target, playing a flat or high throw by the target's height | done | plan action `Hurl`; `src/Creature/CreatureThrow`; tests `CreatureThrow.TheReleaseVelocityLandsOnTheTarget`, `HighTargetsBlendInTheHighThrow` |
| An angry creature hurls what it picks up at a home or a tree | done | `creature_mind::Hurl` (`CreatureIdleMind.cpp`), the target from `NearestHurlTarget` (`CreatureMindSystem.cpp`); test `CreatureIdleMind.ThrowingAboutAimsFromWhereItStands` |
| Hurl what it already holds in its hand | done | the throw lets go of what is held (`CreatureObjectActionSystem::Release`) |
| What is thrown flies until it lands, bounces and comes to rest | done | the release goes into our physics objects (`PhysicsObjects::AddObject`, `from_hand::InitialisePhysicsFromHand` in `CreatureObjectActionSystem::ReleaseHeld`); test `CreatureThrow.FlightTimeIsTheTimeToFallTheDistance` |
| It won't throw at something too close | done | test `CreatureThrow.ItThrowsAtNothingTooClose` |
| Things thrown hurt what they land on, and villagers thrown are hurt or killed | done | what it releases flies in our physics objects and strikes as any thrown thing does (see [../physics](../physics/)) |
| A thrown villager frightens the creature's town | done | test `CreatureObjectActions.TheTownFearsThrowingAndEatingVillagers` |
| Throw around: tosses something about playfully nearby | done | plan action `ThrowAround` |
| Throw in the sea: carries something to the shore and throws it in | partial | plan action `ThrowInTheSea` throws it somewhere nearby, not at the sea (noted in `CreaturePlanActions.cpp`) |
| Throw to impress: throws something far in front of an audience of villagers | partial | plan action `ThrowToImpress` throws nearby without an audience |
| Practice throwing | todo |  |
| Throw stones into the sea with a friend | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| Throw something at the camera | todo |  |
| Throw a ball at something, kick a ball around | partial | plan actions `ThrowBallAtObject` and `KickBallAround` throw what it picks up nearby; balls aren't special; the toy actions in full: [../nature/toys.md](../nature/toys.md) |
| Catch something thrown at it and hold it (catching a fireball to throw back always fails in the game, so it never happens) | todo | the physics' creature catch hook (`PhysicsObjects::SetCreatureCatchHook`) is never set, so no creature catches; see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) and [physics.md](../../bw1-notes/physics.md#pending) |
| Put down: sets what it holds down gently | done | `Kind::PutDown` in `CreatureObjectActionSystem` |
| Discard: tosses what it holds away, with some of the hand's swing | done | `Kind::Discard`; test `CreatureThrow.TossingKeepsSomeOfTheHandsSpeed` |
| Lob gently | done | `Kind::Lob`, the gentle lob animation (`creature_throw::k_GentleLob`) |
| Its throw can go wrong while it is unskilled: the thing goes astray | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Knocking things down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walks up and strikes with the four striking animations blended | done | `src/Creature/CreatureReach` striking animations, `Kind::Destroy` in `CreatureObjectActionSystem` |
| Stomp on something | partial | plan action `Stomp`: at the blow a home takes damage, a tree is felled and anything else mobile is deleted; no crushing or squash |
| Kick something | partial | plan action `Kick`, as stomping |
| Kick a tree | done | plan action `KickTree`: the tree is felled (`ecs::FellTree`); see [../nature](../nature/) |
| A home struck is damaged by the creature's size | partial | `CreatureObjectActionSystem` gives the home the blow a thrown thing gives (`ecs::abodes::OnPhysicalDamage`, marked approximate), not one by the creature's size |
| Things it stamps on are crushed: villagers and animals killed, small things broken | todo | villagers and animals can't be struck (`CanDestroy`: trees, homes and mobile objects only) |
| Break a rock, smash stones in half | todo | see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Destroy whoever attacked it | todo |  |
| A struck thing's town fears the creature | done | `creature_object_actions::AttitudeTo` (a strike is feared); only the debug spawner reads it |

## Fire and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Start a fire, or set something on fire | todo | see [../physics](../physics/) |
| Put out a fire by stamping on it | todo |  |
| Put out a fire with the water miracle | todo | the creature casts no miracle; see [creature_casting.md](creature_casting.md) |
| Put out a fire on itself, by rolling or running to water | todo | a burning creature takes no harm (`src/ECS/Fire/FireObjectTraits.cpp`) |

## Gestures and expressions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Point at something, low or high, to its left or right, turning to it first | partial | `CreatureObjectActionSystem::PointAt` (`Kind::Point`), but only the debug spawner starts it; no plan action points |
| Point at the camera, at the hand, or at something off-screen to show the player | partial | plan action `PointAtCamera` turns to the camera and plays its look-at-me animation; pointing at the hand or off-screen things is todo |
| Wave at the player | done | plan action `WaveAtPlayer` |
| Wave at something | todo |  |
| Look at the hand, look at the camera | done | plan actions `LookAtHand`, `PointAtCamera` (face the camera and emote) |
| Look at the camera in wide screen, during cut scenes | todo |  |
| Pull silly faces | done | plan action `PullSillyFaces` |
| Show an impressive animation | done | plan action `ShowImpressiveAnimation` |
| Be sad, be frightened on the spot | done | plan actions `BeSad`, `BeFrightenedOnTheSpot` |
| Look confused | todo | no plan action; the confused animation only plays when a route fails (`CreatureLocomotionSystem`) |
| Be pathetic to the player, to get attention | done | plan action `BePatheticToPlayer` |
| Be cross with the player | todo |  |
| Show the player how nice it thinks they are | todo |  |
| Howl at the player | done | plan action `HowlAtPlayer` |
| Pray to the player | todo |  |
| Look at its reflection in the water | todo |  |
| Watch the telly (unconfirmed what it shows) | todo |  |
| Behave strangely, get high (from magic mushrooms and toadstools) | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Communicate its state: show the player its strongest desire | done | plan action `CommunicateState`; test `CreatureIdleMind.ItShowsItsStrongestDesireOnceAMinute` |
| Show a lesson it has learnt, by playing the action | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Draw a shape in the air for a gesture (circle, star, spiral, heart and others) | todo | the gestures before casting are in [creature_casting.md](creature_casting.md) |

## Going places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Go to the top of a hill and look round, sit on it, or walk along a ridge | todo | the look-about plan actions (`LookAtMountains` and others) gaze from where it stands |
| Explore the coast, explore the towns | todo | see [idle_behaviour.md](idle_behaviour.md) |
| Run around a race track | todo |  |
| Go to the middle of the screen, to where the player is looking | todo |  |
| Go to the hand | partial | the leash leads it to the hand ([leash.md](leash.md)); going to the hand by itself is todo |
| Run to something | partial | it runs when the plan asks for it (`CreatureLocomotionSystem`, `Pace::Run`) |
| Run away from something, from a place, from the player | done | plan actions `RunAwayFromObject` and `RunAwayFromPlayer`; `CreatureLocomotionSystem::FleeFrom` |
| Go to a teleport and use it | todo | the teleport miracle moves no creature in our tree |
| Sit down on a beach | todo |  |
| Enter the citadel | todo | see [home_and_pen.md](home_and_pen.md) |
