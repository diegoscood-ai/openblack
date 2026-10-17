# Feeding and thrown things

The player feeds the creature by handing it things from the hand, by putting food where it will find it, and by
stroking it while it holds food; it decides for itself what to do with what it is given, is sick on what it can't eat
and gets high on mushrooms. Things thrown at it are caught out of the air when it can reach them in time, or strike it.

How the creature eats what it finds by itself is in [object_actions.md](object_actions.md), what eating does to its body
in [physiology.md](physiology.md), the blow of a thrown thing in [../physics/impact_damage.md](../physics/impact_damage.md),
and thrown people in the air in [../physics/thrown_living.md](../physics/thrown_living.md). The hand's side of giving
(turning the held thing, drawing the hand to the creature) is in [../hand/holding.md](../hand/holding.md).

**Progress: 18/71 done, 17 partial — 37%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/hits-and-catch.md).

## Giving from the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding something over a creature that would take it, the hand offers it and turns it to face the creature | done | raffclar PR #387: `HandSystem` (`HandGiving.cpp`) with `creature_giving::KeepOffer` and `OfferTurn`; tests `CreatureGiving.*`, `CreatureGivingHand.*`; unverified against v1.42 |
| Only the creature's own player, or a player allied to it, can give it things; another god's creature won't take anything from the hand | partial | only the creature's own (local) player gives: openblack has no alliances (`HandSystem::GiveVerdict`, `creature_giving::MayGive`); unverified against v1.42 |
| A spell seed is never given; nor can anything be given to a creature under a script's control, or in one state the game excludes (not identified) | partial | a seed is never offered nor given (`creature_giving::MayGive`); the script-controlled and excluded creatures are not refused yet; unverified against v1.42 |
| Something the creature couldn't pick up isn't given, and the creature's help explains why | partial | what it couldn't pick up wherever it is (`CanPickUpIgnoringMap`) is not given; the help message (7) is not raised (no creature help messages yet); unverified against v1.42 |
| Letting go over such a creature hands the thing over instead of dropping it (the advisors: "put your hand in front of him and then hold the Action Button") | done | holding Action past the grab's 225 ms gives it (the Give packet, `HandGiving.cpp`); the release then neither drops nor throws it; unverified against v1.42 |
| Given something, the creature at once drops what it was doing, turns to the hand looking curious, and reaches to take the thing from it | done | `CreatureMindSystem::TakeFromHand` (agenda `creature_mind::TakeFromHand`: turn to the camera, curious, then `CreatureObjectActionSystem::TakeFromHand`, animation 86); unverified against v1.42 |
| Moving the hand away more than 15 times the creature's size calls the giving off, and the creature stops reaching | done | `creature_giving::ReachFrame` / `CallsOff`, packet 0x52 (`creature_hand_packets::ApplyGiveCallOff`); tests `CreatureGivingHand.MovingTheHandFifteenSizesFromTheReachingCreatureCallsItOff`, `CreatureHandPacketsTest.TheGivingCalledOff...`; unverified against v1.42 |
| Every machine in a networked game sees the creature take it | todo | no network play; see [../multiplayer/](../multiplayer/) |

## What it does with what it is given

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If it was already set on doing something that wants a thing of this kind (going off to eat, say), it carries that on with the thing given | todo |  |
| A one-off miracle given to a creature grown far enough is cast, aggressively or kindly | todo | the creature casts no miracle; see [creature_casting.md](creature_casting.md) |
| Otherwise it weighs four desires, each by how good it has learnt acting on that kind of thing is for it: curiosity looks it over, hunger eats it, wanting to play with the player throws it; compassion (stroking it) is never chosen this way | partial | curiosity, hunger and play with the player weighed by our act-on usefulness (`CreatureMindSystem::UseGift`, `creature_giving::ChooseUse`); no learnt "can't eat" kinds yet; unverified against v1.42 |
| Hunger only counts for a thing of a kind it hasn't learnt it can't eat | todo |  |
| If no desire is at least a tenth strong, the first time it is given a kind of thing it looks it over; after that it does with it what it would when led to it on the leash | done | `creature_giving::ChooseUse` with `CreatureMindState::givenKinds`; later kinds go to the leash's `actOn`; test `CreatureGiving.WithNoDesireATenthStrong...`; unverified against v1.42 |
| Being given something is not by itself a lesson; it learns from the player's stroke or slap afterwards | todo | nothing gives; the hand's row "things taken from the hand teach it the player wanted it to have them" is unconfirmed |

## What it can eat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It may try to eat anything it can pick up, other than toys, spell seeds and other creatures; stores, fields and fish farms it eats from where they stand | partial | it only tries things with a food value (`CreatureObjectActionSystem::FoodValueOf`, `NearestFood` in `CreatureMindSystem.cpp`); a storage pit's pile, fields and fish farms eaten in place are todo ([object_actions.md](object_actions.md)) |
| A thing's food value is its kind's value from the game's tables, but only for kinds marked as meat or plant food; a pot or pile is worth the food in it | partial | `CreatureObjectActionSystem::FoodValueOf` takes the villager, animal, mobile object or pot table's value (a pot by the food in it) without the food kind, so special villagers, tortoises and crops count as food; test `CreatureAnimalFoodTest.TheScansFindAnimalsToEat` |
| The tables: a villager 250, a cow or horse 1200, a sheep 800, a lion or tiger 900, a wolf 700, a pig 290, birds 40 to 50, magic food 16, a mushroom 99, a magic mushroom 50, a toadstool 150; rocks, trees, poo, crops, Egyptian pots and wood are worth nothing | partial | the values come from the game's tables (`InfoConstants`, read by `FoodValueOf`); the food kinds aren't used, see above |
| Special villagers (trainer, priests, monks, breeders, footballers, marauders and others) and tortoises are no food | todo | `FoodValueOf` counts them by their table value |
| Anything worth less than 5 it can't stomach: it is sick, its wish to eat is held back for a while, and it never tries that kind of thing again | todo | it never picks such things up to eat (test `CreatureNeedsMind.FoodItCantPickUpIsGivenUpUneaten` covers only what it can't lift) |
| Poisoned food (people, animals or piles a script poisoned) makes it sick and holds back its wish to eat, but it may try that kind again | todo | `SET_POISONED` and the poison queries are stubs in `src/CHLApi.cpp`; a toadstool counts as poisoned food too, see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Some things the game marks as not to be eaten are spat out and dropped, and it is sick | todo | which marks these are was not fully identified |
| Magic mushrooms and toadstools are eaten and make it high for a while, the cause of its stoned look; how many it has eaten is kept | todo | see [physiology.md](physiology.md) "can get high"; the temple scroll has the count's label only (`src/3D/TempleScrolls.cpp`) |
| Eating lowers its hunger by the food's value over a measure of its body (at most 0.8) times its species' factor, never below nothing nor above full | partial | hunger is lowered by the eating action's table multiplier (`Satisfied` in `CreatureMindLearning.cpp`); the body measure the game uses was not identified |
| Eating fills it, fattens it and builds up poo | done | `creature_physiology::Eat` from `CreaturePhysiologySystem`; tests `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo`, `CreaturePhysiologySystemTest.EatingAndFinishingAnAction`; see [physiology.md](physiology.md) |
| How many of each kind of thing, and of villagers, animals and mushrooms, it has eaten is counted | todo |  |
| A person eaten dies, the death put down to the creature's player, and their town is frightened | partial | killed with the cause "eaten by a creature" (`Consume` in `CreatureObjectActionSystem.cpp`, `ecs::life::Kill`), which takes it out of home and town, and its town is frightened; the death is not put down to the player; test `CreatureObjectActionSystemTest.AVillagerEatenDiesAsAnyDeathGoes` |
| Eating people is evil (the advisors say so) | todo | nothing moves a creature's alignment from what it eats; see [physiology.md](physiology.md) |
| The creature of one kind of player (likely the computer gods; not identified) never eats its own player's people, unless a script controls it | todo |  |

## Force-feeding and lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroking the creature while it holds something makes it eat it ("Stroking the Creature while he's holding an object will make him eat that object") | partial | `LearnFromFeedback` in `CreatureMindLearning.cpp`: stroked while holding food, from the second stage, with the action table's eat-when-stroked mark, hunger is made dominant and the food is learnt as good; how the game ties it to the table was not traced |
| A stroke or slap soon after eating teaches it to eat that sort of thing more or less | done | credit goes to recent actions (`CreatureMindLearning.cpp`); see [learning_from_feedback.md](learning_from_feedback.md) |
| It says what it learnt ("From now on, your Creature will eat that sort of thing more"; mushrooms have their own lines) | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Its desire panel lines: hungry from low energy, from watching people eat, from sadness, going to eat something odd, going to eat the weird mushrooms | todo | see [creature_mode.md](creature_mode.md) and [lessons_and_help.md](lessons_and_help.md) |

## Food near it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pot or pile of food raises its kind's reaction when it is made, put down by the hand, lands or stops burning | partial | the pot reaction is raised when the hand puts a pot down, when a store or site makes a pile and by the land script (`animal_ai::SetupPotReaction`, from `HandHolding.cpp`, `HandResources.cpp`, `TownStores.cpp`, `BuildingSites.cpp`); not on landing or burning out |
| A creature that can see the food takes the reaction up (not the creature of the kind of player above) | todo | no creature system reads reactions |
| Reacting to food, it only comes to know of it, so that its mind may choose to eat it; it doesn't go straight to it | todo | the creature finds food anywhere within reach without seeing it (`NearestFood` in `CreatureMindSystem.cpp`) |
| Food it has found it picks up, looks over and eats, or eats where it lies | done | plan actions `EatAfterExamining`, `EatAlive`, `EatFromFoodPile`, carried out by `CreatureObjectActionSystem`; tests `CreatureNeedsMind.ItPicksFoodUpAndEatsIt`, `CreatureIdleMind.EatingIsPickingUpExaminingAndEating`; see [object_actions.md](object_actions.md) |

## Catching things thrown at it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every thing that starts to fly, thrown or knocked, is offered to every creature to catch | done | `PhysicsClassHooks::OfferToCatchingCreatures` walks the creatures (`ecs::creature_catch_walk`), from both of the physics' starts; the creatures are asked by entity number, not in the game's list order; [creature.md](../../../creature/hits-and-catch.md#the-creatures-catch) |
| A creature tries only when not fighting or set on a fight, not under a script's control, not already catching, and free to react | done | `creature_catch::MayTry`; no script sets an only desire in openblack |
| The thing must weigh less than eight tenths of the creature and be able to be picked up | todo |  |
| It never catches what it threw itself; a throw by a god not allied to its player it catches only three times in a hundred | partial | done for the player's hand's throws; which other throws carry a god is not read, and no gods are allied in openblack |
| It must be moving across the land at 1 m/s or more and come closest within 5 seconds, late enough for the catch to be started, and pass within the catch's reach | done | `creature_catch::Reaches`, bit for bit with the original (oracle cases in `test_creature_catch`) |
| Catching, it drops what it was doing, stops, smiles and goes to catch it | done | a forced plan of play to catch, score 1; the original's hurried end of the action it was playing is not ported, as openblack's forced plans end it at once |
| It catches with a blend of four catching animations, high or low and left or right, by the thing's height and side, the other hand mirrored | done | `CreatureCatching`, a turn at a time; it turns to the thing, waits, steps across when it will pass out of reach |
| At the catch's moment, if the thing is still within reach, it is taken out of the air into the creature's hand; otherwise it is missed and flies on | done | |
| A person caught goes into the held state, an animal plays its held animation | todo | see [../physics/thrown_living.md](../physics/thrown_living.md) |
| Food caught isn't eaten as part of the catch; what it does with the thing afterwards is its next ordinary choice | todo |  |
| The "catch a fireball and throw it back" action is listed but always fails, so creatures never do it | done | the game never does it; openblack neither |
| Creatures watch things flying through the air | todo | see [reactions.md](reactions.md) |

## Being hit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A thing that strikes it and isn't caught hurts it by the blow's crush; toys don't hurt it | todo | thrown things collide with a creature's body (`creature_physics` in `src/ECS/CreaturePhysics.cpp`), but its damage is not ported: a creature takes no harm (`src/ECS/Effects/EffectValues.cpp`); see [../physics/impact_damage.md](../physics/impact_damage.md) |
| A blow sways its upper or lower body | todo |  |
| The player's own creature, while a script controls it, isn't hurt by blows at all | todo | no creature is hurt by blows (above) |
| Struck during a fight, it neither reels nor loses fight health or life | todo | blows do nothing to a creature (above), in or out of a fight |
| Outside a fight the blow can cut or scar its skin where it struck | todo | only fights add wounds (`CreatureFightSystem`); see [marks.md](marks.md) |
| Its fear and its anger from being damaged rise by the harm | todo |  |
| When the blow is put down to its own player, it thinks less of its player by the harm | todo |  |
| It doesn't turn on the thrower: no plan or reaction is aimed at whoever threw | done | no plan or reaction is aimed at the thrower |
| The collision sounds as for any blow, by the two materials | partial | the creature has a physics body (`creature_physics::SetUpBody`, through `ecs::PhysicsGameHooks`), so collisions take the generic impact path; the creature's material was not checked; see [../physics/impact_damage.md](../physics/impact_damage.md) |
| Its creature learns "damage by throwing at" from the player's throws | partial | the deed is reported for a thrown thing that damages a building (`src/ECS/Systems/Implementations/BuildingDamageSystem.cpp`, `ecs::creature_mimic`); not for throws at other things; see [../physics/impact_damage.md](../physics/impact_damage.md) |
| The life lost shows on the status panel, and at no life it faints | partial | nothing takes its life (above); with no life left it faints (`physiology::ShouldFaint`); see [physiology.md](physiology.md) |

## People given or thrown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager given from the hand is taken like anything else and looked over, eaten or thrown by its desires | done | a villager is given as anything else (`HandSystem::GiveVerdict`); scenario `creature.give_villager`; unverified against v1.42 |
| A villager thrown at it is caught if it can be, and then held; otherwise it strikes the creature and is hurt by its own landing | partial | the landing is the thrown villager's own ([../physics/thrown_living.md](../physics/thrown_living.md)); catching and the blow to the creature are missing |
| Holding a person frightens their town | done | `creature_object_actions::AttitudeTo` in `CreatureObjectActionSystem`; test `CreatureObjectActions.TheTownFearsThrowingAndEatingVillagers`; see [object_actions.md](object_actions.md) |

## Other gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Another god can't feed your creature unless allied | todo | see "Giving from the hand" |
| Another god's throws at your creature are caught only three times in a hundred | partial | as above: the player's hand's throws only |
| Another god's blows hurt, frighten and anger it but don't change its feeling for its own player | todo | blows do nothing to a creature, and there are no other gods' hands |

## The tutorial and the advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first land's lesson on eating: the creature is starved, the trainer says "Your Creature is getting hungry", "find him something to eat", "some things are better for him than others" | todo | see [../story/land_1.md](../story/land_1.md) "The creature's learning"; every step: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-2-learning-to-eat) |
| While the player looks for food, the advisors comment on what the hand is near: pork (evil), corn from the fields (good), beef (evil, "meat-eaters are more aggressive"), people (evil; children deeply evil), grain from the village store (good) | todo | same script |
| Once the hand holds something near the creature: "Give him food by putting your hand in front of him and then holding the Action Button" | todo | same script |
| The lesson waits until it has eaten once (alive, after looking over, from the hand or from a magic food pile), keeping it wanting nothing but food; then "He seemed to like that", "Try feeding him some more" | todo | same script; see [development_phases.md](development_phases.md) |
| Reminders and help: low energy ("give him something to eat"), "Your Creature is going to eat this", the stage's criteria | todo | see [lessons_and_help.md](lessons_and_help.md) |
| The ogre on the first land can be fed instead of fought, which makes him drowsy | todo | see [../story/land_1.md](../story/land_1.md) "The Ogre" |
