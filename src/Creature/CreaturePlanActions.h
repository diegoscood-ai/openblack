/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureIdleMind.h"

/// The actions of the game's table that a creature can carry out here, and the agenda of steps each becomes. The planner
/// only weighs these; the rest (miracles, building, dancing and so on) wait for the systems they need.
namespace openblack::creature_plan_actions
{
/// The kind of thing an action is done to
enum class Target : uint8_t
{
	/// Nothing: done on the spot
	None,
	/// Anything it can eat, or only living things it can eat
	Food,
	LiveFood,
	/// A pile of magic food away from its town's storage pit, eaten from in portions
	FoodPile,
	/// Anything it can pick up
	Pickable,
	/// A thing that the creature can bring home: more than 50 m from its home, not alive, and a thing that it can pick up
	BringableHome,
	/// Anything it can knock down, anything it can kick, or only trees
	Destroyable,
	Kickable,
	Tree,
	/// Villagers, villagers hurt enough to be healed, other creatures, or either
	Villager,
	HurtVillager,
	Creature,
	Living,
	/// A villager doing something that shows a skill the creature does not know yet
	InterestingVillager,
	/// Another creature that has no partner in a joint action now, and one that has
	CreatureFreeForJoint,
	CreatureNotFreeForJoint,
	/// Anything that frightens a creature: another creature, a spell, or a frightening animal
	Frightening,
	/// Anything at all it can see
	Anything,
	/// Anything it can see that is worth playing with: never a dead villager or animal
	Playable,
	/// What food may be cast into: a built storage pit or a built worship site; what wood may be cast into: any storage
	/// pit; a worship site; a building going up; a workshop
	FoodStore,
	WoodStore,
	WorshipSite,
	BuildingSite,
	Workshop,
	/// What water may be sprinkled on: a field that needs watering or any tree; any tree, whether or not it can be
	/// knocked down; a field that needs watering, its food exactly nothing
	Sprinklable,
	AnyTree,
	DryField,
	/// A part of a temple, of any player: a temple, a worship site, or the altar at a worship site's head
	TemplePart,
	/// A villager nearer to the creature than 200 metres, to dance with
	DanceVillager,
	/// Anything alive: villagers, animals, birds and creatures
	AnyLiving,
	/// A toy it can pick up, or only a ball (the football or the toy ball)
	Toy,
	ToyBall,
	/// What thrown things can knock down: the living while they live, and what is built or set into the land
	Stonable,
	/// What it can pick up that does damage when thrown: statics other than toys, rocks and dead trees
	ThrowingDamage,
	/// What it may throw into the sea in play
	SeaThrowable,
	/// A tree it can pick up, or one it can pick up to plant somewhere else: not one of the last trees it planted, nor
	/// one near where the player did what it last copied
	PickableTree,
	PlantableTree,
	/// What it can pick up and throw to impress
	ThrowableToImpress,
	/// What a handful is taken from: a field whose crop has grown some and holds food, or a tree tall enough for the
	/// creature to eat from; and a storage pit that is fully built, which a handful is given to
	FieldWithFood,
	TreeBigEnough,
	BuiltStoragePit,
	/// A pile of wood that the wood miracle made
	MagicWoodPile,
	/// A farm animal (or a tortoise) that the creature can pick up, to drop in a store
	Cow,
	/// A villager that does not dance in worship
	VillagerNotWorshipping,
	/// A villager that the creature may make a breeder: grown up, of a town with fewer than three breeders, and no
	/// disciple yet
	BreederVillager,
	/// A villager more than 40 m from its home
	VillagerFarFromHome,
	/// The totem of a town of the player who watches: the town centre and the town's worship site built, and the
	/// player's temple there
	PlayerTotem,
	/// A mushroom: the magic mushroom that grows on the land, or a magic mushroom or toadstool that lies about
	Mushroom,
	/// What a creature may examine by looking at it: anything but a creature, a field, a fish farm, a vortex, a
	/// teleport stone or a miracle at work
	Examinable,
	/// What the poo desire may be done by: a thing that is not alive, nor a town, a forest, a planned building or a
	/// miracle, taller than one and a half metres
	PooDesireThing,
	/// What a poo may be done on: such a thing that is not a field or a fish farm either
	Pooable,
	/// A thing that the creature can steal: a toy, a scaffold or a one-shot miracle that it can pick up, out of the
	/// influence of its player
	Stealable,
	/// A storage pit no wider than its share of the creature's drawn size, whose food pile is there and holds food,
	/// for the creature to eat from where it stands
	StoragePitWithFood,
	/// A built storage pit of the player of the creature; a built storage pit of another player, or of no player; a
	/// field of another player, or of no player (see creature_town_theft)
	OwnStoragePit,
	OthersStoragePit,
	OthersField,
	/// A creature that seems friendly, to amuse with a spell
	FriendlyCreature,
	/// A rock (bonfires too) it can pick up, or any rock
	PickableRock,
	Rock,
	/// A cuddly toy, a die, or a static toy (not the football) more than 50 from the creature's home
	CuddlyToy,
	Die,
	ToyAwayFromHome,
	/// One of a town's buildings (not the temple) or a town's totem below its full life (see NeedsRepair)
	NeedsRepair,
	/// What it can stomp on: a villager or an animal while it lives, a bird low over the land, and of the things that
	/// are not alive what it can knock down
	Stompable,
	/// Anything that is on fire, which the water miracle puts out
	Burning,
	/// A villager playing football: its final state is one of the match's
	Footballer,
	/// The globe of a one-shot miracle that attacks, is kind, is playful or restores health, that no other creature holds
	OneShotAggressive,
	OneShotCompassionate,
	OneShotPlayful,
	OneShotRestoreHealth,
	/// A villager playing football in a match whose last goal is the creature's side's, or the other side's: the
	/// match's state is a goal just scored and the goal's side is, or is not, the one the creature's player plays for
	FootballerMySideScored,
	FootballerOtherSideScored,
};
/// How many kinds of thing an action may be done to
inline constexpr size_t k_TargetCount = static_cast<size_t>(Target::FootballerOtherSideScored) + 1;

/// How the agenda is made
enum class Build : uint8_t
{
	Eat,
	/// Eating a thing alive, picked up unless it holds something it can eat
	EatAlive,
	/// Stomping on a thing, picking it up and eating it, unless it holds something it can eat (see
	/// creature_mind::StompAndEat)
	StompAndEat,
	/// Going up to a pile and eating a portion made from it
	EatPortion,
	/// Going up to a storage pit's food pile, a field or a tree, and eating a portion of wheat, or of fruit, made from it
	EatFromStoragePit,
	EatFromField,
	EatFromTree,
	FishAndEat,
	Sleep,
	Poo,
	Puke,
	Drink,
	ExamineByPickingUp,
	ExamineByLooking,
	ExamineByFollowing,
	ThrowAbout,
	Hurl,
	Destroy,
	Stomp,
	/// Walking up to a thing, facing it and kicking it
	Kick,
	SitDown,
	BeIdle,
	HangAround,
	ShowDesire,
	/// An action on the spot, or facing the player, or at what it goes up to
	Emote,
	FaceCameraEmote,
	/// One action on the spot that pulls no face of its own
	Individual,
	/// Turning to face the camera, then one action, looking at the camera all the while and pulling no face of its own
	FaceCameraIndividual,
	/// Following the camera's point of focus, turning to the camera, then the pathetic clip
	BePatheticToPlayer,
	/// Turning to the camera, then the howl
	HowlAtPlayer,
	/// Looking at the camera, and pointing at it
	LookAtCamera,
	PointAtCamera,
	/// Looking at the camera while a script's cut scene is on, as every creature free to think for itself is made to
	LookAtCameraInCutScene,
	/// Looking at the player's hand
	LookAtHand,
	/// The actions with its player: go to the hand, point at the hand, show the player how nice the creature thinks the
	/// player is, pray to the player and throw a thing at the camera
	GoToHand,
	PointAtHand,
	ShowHowNice,
	PrayToPlayer,
	ThrowAtCamera,
	/// The actions with its player that first follow the camera: go to the middle of the screen, follow the player and
	/// be cross with the player. Also the action to be silly with the player.
	GoToMiddleOfScreen,
	FollowPlayer,
	BeCrossWithPlayer,
	BeSillyWithPlayer,
	/// Looking another creature over, and looking at a thing flying through the air
	InspectCreature,
	LookAtFlyingObject,
	/// Noticing a deed of its player's that it may copy: a helpful, an aggressive, a neutral, a stealing or a playful one
	/// (see creature_mind::NoticeDeed)
	NoticeHelpful,
	NoticeAggressive,
	NoticeNeutral,
	NoticeStealing,
	NoticePlayful,
	ApproachEmote,
	/// The actions with a friend: smiling at it, waving at it (which it does too), howling at it, pining for it and
	/// looking for it
	SmileAtFriend,
	WaveAtFriend,
	/// The joint actions of two friends that talk, joke, dance, argue, mope or show off (creature_mind::TalkToFriend and the
	/// others)
	TalkToFriend,
	TellFriendAJoke,
	DanceWithFriend,
	ArgueWithFriend,
	MopeAboutWithFriend,
	ShowOffToFriend,
	ConfuseFriend,
	/// Kissing a friend (creature_mind::KissFriend)
	KissFriend,
	/// Showing a friend a destructive miracle (fireball, lightning, storm), or a creating one (food, wood)
	ShowFriendDestructiveSpell,
	ShowFriendCreationSpell,
	/// Getting the attention of a friend busy with another creature, and examining another creature with a friend
	GetAttentionFromFriend,
	ExamineOtherCreatureWithFriend,
	/// Sitting with a friend on top of the nearest hill
	SitOnTopOfHillWithFriend,
	/// Playing a throwing game with a friend, with a toy
	PlayThrowingGameWithFriend,
	/// Giving a friend a toy, and showing a friend its citadel
	GiveFriendToy,
	ShowFriendCitadel,
	HowlAtFriend,
	PineForFriend,
	LookForFriend,
	/// Showing another creature it hates it, by the two creatures' heights, and how nice it reckons it is
	ShowHate,
	ShowHowNiceToCreature,
	/// Telling another creature to go away, by where the two stand and the other's height
	TellToGoAway,
	/// The joint actions side by side with a friend, and with a friend at a town (see creature_mind::BeHappyWithFriend
	/// and the others)
	BeHappyWithFriend,
	PooWithFriend,
	SitWithFriend,
	SleepWithFriend,
	DrinkWithFriend,
	EatFromFieldWithFriend,
	HelpTownWithFriend,
	AttackTownWithFriend,
	RunFromObject,
	RunFromPlayer,
	LookAbout,
	/// Turning to the sun's far point and, one time in two, pointing at it
	LookAtSun,
	/// Turning to the moon, one time in two pointing at it, and amazed when evil
	LookAtMoon,
	/// Going up to a villager and pulling silly faces at it
	PullSillyFaces,
	/// Going up to a villager and showing it an impressive pose
	ShowImpressiveAnimation,
	/// What scripts force on creatures to stage a scene: looking at a thing for a day, or a while without going up to
	/// it, and pointing at it
	LookForever,
	LookButDontApproach,
	PointAtThing,
	/// Casting a miracle at what it is done to: a lightning bolt, a helpful miracle, a spell on another creature, food,
	/// water; water on the thing it is done with, and water on itself
	CastLightning,
	CastHelpful,
	CastPlayful,
	CastFood,
	CastWater,
	CastWaterOnInstrument,
	CastWaterOnItself,
	/// Far from its home, casting the teleport miracle at itself as it casts a helpful miracle at a thing: it goes near
	/// itself, gets away from itself (to the east, the way a thing right on it is left) and turns to face itself
	CastTeleportAtItself,
	/// Casting a miracle that attacks: a tornado, the lightning storm or a power-up of the explosion; an explosion; a
	/// fireball
	CastAggressive,
	CastExplosion,
	CastFireball,
	/// Casting a shield from where it stands
	CastShield,
	/// Casting a miracle to impress the people of a villager
	CastToImpress,
	/// The odd actions: showing a lesson that it learnt, looking confused, behaving strangely
	ShowLearntLesson,
	LookConfused,
	BehaveStrangely,
	/// Casting a forest at the place where a villager is as the agenda is made
	CastForest,
	/// Casting the one-shot miracle in its hand at the thing, and picking up a globe (unless its hand holds something)
	/// to cast its one-shot miracle at the thing (see creature_mind::CastOneShotAt)
	CastOneShotInHand,
	PickUpAndCastOneShot,
	/// The same with a one-shot miracle that restores its health, cast at itself (see creature_mind::CastOneShotAtItself)
	CastRestoreHealthInHand,
	PickUpAndCastRestoreHealth,
	/// Catching what flies at it, only ever forced on it
	Catch,
	/// Fighting another creature, only ever forced on it here: the anger that picks fights starts them through the
	/// fights themselves
	Fight,
	/// The actions that use the creature's map of the land: going to a hill to look, going to a hill and walking along
	/// its ridge, exploring a coast or a town, sitting on a beach, looking at the mountains or out to
	/// sea, throwing a thing into the sea, and looking at its reflection in the sea
	GoToHillAndLook,
	WalkAlongRidge,
	ExploreCoast,
	ExploreTowns,
	SitDownOnBeach,
	LookAtMountains,
	LookOutToSea,
	ThrowInTheSea,
	LookAtReflection,
	/// The creature goes to the nearest coast, turns to the nearest sea and dances alone there
	DanceByTheSea,
	/// Resting to get better where it is, or at home after it goes there; healing itself with the heal miracle; resting on
	/// the spot a while
	RestToGetBetter,
	GoHomeToRecover,
	HealItself,
	RestOnTheSpot,
	/// Going home, running home, going into its temple, praying at its temple, and looking at a part of a temple
	GoHome,
	RunHome,
	EnterTemple,
	PrayAtTemple,
	LookAtTemple,
	/// Dancing with the villagers of a town, by the executor's row of the dance table
	DanceWithVillagers,
	/// Dancing alone where it stands
	PracticeDance,
	/// Giving the tree it uses to the store as wood, and planting the tree it uses by the thing
	GiveWoodToStore,
	PlantTree,
	/// Throwing what it holds, or else the thing after picking it up, at a point off to one side of it
	PracticeThrow,
	/// Picking up the toy it uses and throwing it at the thing
	ThrowToy,
	/// Picking up the thing, unless it holds a magic mushroom, and eating it
	GetHigh,
	/// The town help actions (see creature_town_help): giving a handful of food from a field or magic food to a store;
	/// putting food from a field by a worship site; dropping a cow in a store; making a villager a breeder; taking a
	/// villager home to sleep; bringing a villager to a worship site; raising and lowering a totem; playing with a
	/// villager
	GiveFieldFoodToStore,
	GiveMagicFoodToStore,
	/// Bringing food out of the sea at a fish farm, unless it holds food, and throwing it in a store
	GiveFishToStore,
	PutFieldFoodBySite,
	DropCowInStore,
	MakeBreeder,
	TakeVillagerHome,
	BringVillagerToSite,
	RaiseTotem,
	LowerTotem,
	PlayWithVillager,
	/// Taking a living thing to the altar of the first worship site of its temple, and putting it down there
	Sacrifice,
	/// Stealing a thing and putting it down near the home of the creature
	StealAndPutByTemple,
	/// The thefts copied from the player: a thing to a town of its player, and food or wood from a store of another
	/// player into a store of its own (see creature_town_theft)
	StealAndPutInTown,
	StealFoodFromStore,
	StealWoodFromStore,
	/// Kicking the ball about a few times
	KickBallAround,
	/// Joining in a footballer's match: the ball thrown at the goal its side attacks; a foul on the other side
	ThrowBallAtGoal,
	FoulPlayer,
	/// Practising a throw with the rock, and breaking it in two as it flies
	BreakRock,
	/// Stroking a cuddly toy, taking a toy home, throwing a die
	StrokeToy,
	TakeToyHome,
	ThrowDie,
	/// Picking up the toy it uses and throwing it to its player's hand
	ThrowToPlayer,
	/// Bringing a tree to a house and mending the house with it
	HelpRepairHouse,
	/// Casting wood at a building going up, then building it with all the wood its site holds
	HelpBuildHouse,
	/// Bringing a thing home, bringing food home, and taking wheat from a field home, where the creature puts it down
	BringHome,
	BringFoodHome,
	TakeFieldFoodHome,
	/// Dancing outside its player's temple, and around a thing (see creature_temple_dance)
	DanceOutsideTemple,
	DanceAroundThing,
	/// The creature casts a spell that it picks at random to amuse another creature
	CastAmusing,
	/// Sleeping at home, waving at something, being frightened on the spot, putting down what it holds and lying dead
	/// for good, built as the game builds them whoever chose them
	SleepAtHome,
	/// Walking home and doing a poo there
	PooAtHome,
	WaveAt,
	BeFrightened,
	/// Waiting a while looking down, sad, then a sad clip (creature_mind::BeSad)
	BeSad,
	PutDown,
	DeadForever,
	/// Dying for good, as lying dead for good for longer
	DiePermanently,
	/// Dying: knocked out, as a faint is, while the agenda waits for it to be on its feet again (see creature_mind::Die)
	Die,
	/// Holding a thing up to its player (see creature_mind::HoldObject)
	HoldObject,
	/// Sleeping by a thing, in a clear place about it, else at home
	SleepByObject,
	/// Running away from a place and going to look at a place: the place where a miracle stood that it reacts to,
	/// kept on its mind (see creature_mind::RunAwayFromMiracle and ExamineMiracle)
	RunFromPlace,
	ExaminePlace,
	/// The creature walks to the teleport stone it turned aside into, and asks the stone for a jump (see
	/// creature_mind::GoToTeleport)
	GoToTeleport,
};

/// A one-shot miracle's globe as a creature sees it: the first miracle of its seed, and whether that miracle attacks, is
/// kind or is playful (the miracle's one-shot marks)
struct OneShot
{
	uint32_t magicType {0};
	bool aggressive {false};
	bool compassionate {false};
	bool playful {false};
	bool restoreHealth {false};

	[[nodiscard]] bool Is(creature_mind::OneShotStyle style) const;
};

struct Executor
{
	/// The action's name in the game's table
	std::string_view action;
	Target target {Target::None};
	/// What it uses to do the action, which it takes up first: the thing it throws, or the tree it plants or gives
	Target use {Target::None};
	/// The thing it is done to is never what it holds
	bool notWhatItHolds {false};
	Build build {Build::Emote};
	/// The animation played, for the builds that play one
	size_t animation {0};
	/// The row of the dance table, for a dance
	uint32_t danceType {0};
	creature_mind::Activity activity {creature_mind::Activity::Planned};
	/// The action takes the creature away from its home: a young creature does not do it
	bool leavesHome {false};
	/// The miracle that the agenda casts, when the game's table gives the action no miracle. The creature then never
	/// chooses the action itself, but a script can make it do the action.
	std::optional<uint32_t> castMagic;
	/// Its agenda never uses the thing it is done to: the target only says what the planner may choose it about, and a
	/// plan forced on it with no thing is still made
	bool thingUnused {false};
	/// The kind of one-shot miracle that the action casts, for the actions that cast one
	creature_mind::OneShotStyle oneShot {creature_mind::OneShotStyle::Aggressive};
	/// The planner leaves the action out while the creature did it less than this many seconds ago (DoneWithin)
	std::optional<float> notDoneWithinSeconds;
};

/// Whether an action was done less than the seconds ago, by the turns counted since it was done: whole seconds of them,
/// a count of nothing never (the count is cleared as the action is done and grows only as other plans stop)
[[nodiscard]] constexpr bool DoneWithin(uint32_t turnsSinceDone, uint32_t turnsPerSecond, float seconds)
{
	return turnsSinceDone != 0 && turnsPerSecond != 0 && static_cast<float>(turnsSinceDone / turnsPerSecond) < seconds;
}
/// Sitting down and scratching are left out for two seconds after they were done
constexpr float k_RestRepeatSeconds = 2.0f;
constexpr float k_ScratchRepeatSeconds = 2.0f;

/// Some joint actions with a friend start only when a desire of the creature is stronger than this, or its sadness is
/// less
constexpr float k_SlightDesire = 0.1f;
/// From this stage of growing up the creature is old enough to leave its home
constexpr uint32_t k_LeaveHomeStage = 3;
/// The creature casts the teleport miracle at itself only when it is farther than this from its home
constexpr float k_TeleportFromHomeMetres = 150.0f;
/// Whether the creature is far enough from its home to cast the teleport miracle at itself
[[nodiscard]] constexpr bool FarEnoughToTeleport(float homeDistance)
{
	return homeDistance > k_TeleportFromHomeMetres;
}

/// Every action that can be carried out
[[nodiscard]] std::span<const Executor> All();
[[nodiscard]] const Executor* For(std::string_view action);
/// The same, with the actions that are only ever forced on a creature and that its planner doesn't weigh: catching and
/// fighting
[[nodiscard]] const Executor* ForcedFor(std::string_view action);

/// What the agenda needs to know of where the creature is
struct Situation
{
	/// Where the player looks from, where the creature drinks, and the action that shows its strongest desire. The
	/// camera is where it stands on the ground; the eye is the camera itself, and means nothing without the camera.
	/// The place to drink is none when it finds no place to drink.
	std::optional<glm::vec2> camera;
	glm::vec3 eye {0.0f};
	std::optional<creature_drink::Place> water;
	std::optional<size_t> showDesireAnimation;
	/// The thing the action is done with, by its entity's number, and whether its hand holds something already
	std::optional<uint32_t> instrument;
	bool handFull {false};
	/// A clear spot near it to cast water on itself in, when it has looked for one and found it
	std::optional<glm::vec2> clearSpot;
	/// How near it goes to the thing a portion of food is made from
	float portionApproach {0.0f};
	/// A script controls it, and how far it reaches about itself (its 2D radius): told to sit at a thing, it sits there
	bool controlledByScript {false};
	float radius {0.0f};
	/// How long the agenda's look or sit lasts, for the actions whose agendas the game draws it for as it makes them
	std::optional<float> drawnSeconds;
	/// The height of the thing acted on, the land's there and its own, as a walk to it keeps it
	std::optional<float> objectAltitude;
	/// The food pile of the storage pit the action is done to, by its entity's number, and where it is
	std::optional<uint32_t> pitPile;
	glm::vec2 pitPilePoint {0.0f};
	/// The creature's height
	float height {0.0f};
	/// Whether a fish farm is within 600 m of it (the original's IsThereFishFarmNearby), and, for fishing alone, where
	/// it fishes: the closest farm's shoal, or the nearest place it can stand within 30 m of it, and how near it goes (as
	/// near as it is tall, 15 m at least); none when it holds something it can eat or has nowhere to stand. To give fish
	/// to a store, the farm's shoal itself (no place to stand looked for), none when it holds something it can eat
	bool fishFarmNearby {false};
	struct Fishing
	{
		glm::vec2 point {0.0f};
		float arriveWithin {0.0f};
		/// The point's height as the walk keeps it: the shoal's own, or none (0) for a place found to stand on
		float pointHeight {0.0f};
	};
	std::optional<Fishing> fishing;
	/// The height of the creature it shows it hates it or tells to go away, and where the latter stands, for those
	/// actions alone; the height of the thing it looks over
	float objectHeight {0.0f};
	glm::vec3 objectStanding {0.0f};
	/// What it thinks of the creature it shows how nice it reckons it is (-1 to 1), for that action alone; none when what
	/// it acts on is not a creature
	std::optional<float> howNice;
	/// Whether its hand holds something it can eat, by the test of what it may eat (creature_eating::CanBeEaten), not by
	/// a food value
	bool holdingEatable {false};
	/// Whether it has a player: only then does it turn to the camera before pointing at it
	bool hasPlayer {false};
	/// Where the nearest of its player's hands is, none when its player has no hand here
	std::optional<glm::vec3> playerHand;
	/// Whether it is away from its player's hand, and whether that hand moves; and its attitude to its player
	bool awayFromHand {false};
	bool handMoving {false};
	float attitudeToPlayer {0.0f};
	/// Whether the camera of its player looks at it
	bool lookedAt {false};
	/// What its hand holds, by its entity's number, whatever the caller says of its hand being full, and whether that is
	/// a tree
	std::optional<uint32_t> held;
	/// Holding a thing up to its player, it plays with it: it held things of the kind fewer than twice before
	bool playsWithHeld {false};
	bool holdingTree {false};
	/// Whether what its hand holds is a magic mushroom
	bool holdingMushroom {false};
	/// The one-shot miracle's globe that its hand holds, and the one that the action uses; none when it is not a globe
	std::optional<OneShot> heldOneShot;
	std::optional<OneShot> usedOneShot;
	/// Where it stands
	glm::vec2 position {0.0f};
	/// Where the thing it uses is, and its radius on the ground
	glm::vec2 instrumentPoint {0.0f};
	float instrumentRadius {0.0f};
	/// For the actions that use the creature's map of the land: the place that the creature goes to or looks at, and
	/// the sea that it looks at or throws into. None when the map gives no such place.
	std::optional<glm::vec2> explorePlace;
	std::optional<glm::vec2> exploreSea;
	/// The creature itself, by its entity's number
	uint32_t self {0};
	/// Where its home is on the ground and how far it is from it, in metres, none without a home; and whether its player
	/// has a temple
	std::optional<glm::vec2> home;
	/// The height of its home as a walk there keeps it: the land's there (its home's own altitude is always 0)
	std::optional<float> homeAltitude;
	/// Where its home is, on the ground, for the friend it shows off to
	std::optional<glm::vec3> homePlace;
	std::optional<float> homeDistance;
	bool hasTemple {false};
	/// Forced on it without the action's own test of whether it can be done now (by a script or a reaction, say): the
	/// tests that only weigh whether to choose the action (praying at its temple near home, going home when away from
	/// it, resting with nothing frightening near, the player's hand and camera) are not asked
	bool forced {false};
	/// Whether it wears a leash, held or tied to a post
	bool leashed {false};
	/// The heart of its player's temple, on the ground, and the heart's radius on the ground; none without a temple
	struct TempleHeart
	{
		/// The temple, by its entity's number
		uint32_t temple {0};
		glm::vec3 point {0.0f};
		float radius {0.0f};
	};
	std::optional<TempleHeart> templeHeart;
	/// The match of the footballer the plan acts on: its ball, the goal the creature's side attacks, and the first
	/// player of the other side (none when that side has none); none when the footballer has no match or no ball
	struct FootballMatch
	{
		uint32_t ball {0};
		glm::vec3 goal {0.0f};
		std::optional<uint32_t> opponent;
	};
	std::optional<FootballMatch> football;
	/// Where it stands, with its height
	glm::vec3 standing {0.0f};
	/// No other creature is near enough to frighten it (see creature_mind::NothingFrighteningNear)
	bool nothingFrighteningNear {true};
	/// It has not shown off an impressive pose lately (see HasntShownImpressivePoseRecently)
	bool impressivePoseRested {true};
	/// It is night by the sky (DayNightClock::IsVisualNight): the moon can be looked at, the sun not
	bool visualNight {false};
	/// Where the moon stands, as the sky places it from the camera; none with no camera or sky
	std::optional<glm::vec3> moon;
	/// The creature is evil (its alignment below nothing)
	bool evil {false};
	/// A dance with villagers: the clear place for it (none when there is none or it wasn't looked for), the fewest
	/// dancers its row of the dance table asks for, and the beats of its length
	std::optional<glm::vec2> danceArea;
	uint32_t fewestDancers {0};
	uint32_t danceBeats {0};
	/// For the town help actions: whether what its hand holds is a cow or a villager; where the worship site that the
	/// action is done to keeps its food; and the home of the villager that the action is done to, by its entity's number
	bool holdingCow {false};
	bool holdingVillager {false};
	std::optional<glm::vec2> siteFood;
	std::optional<uint32_t> villagerHome;
	/// For the sacrifice: whether what its hand holds is alive, and where the altar of the first worship site of its
	/// temple stands. The altar is none when the temple has no worship site.
	bool holdingLiving {false};
	/// Whether what the plan acts on is a living thing (a villager, an animal or a creature)
	bool objectLiving {false};
	std::optional<glm::vec3> sacrificeAltar;
	/// For the theft that takes a thing to a town: the storage pit of the nearest town of its player, by its entity's
	/// number. None when the player has no town, or that town has no storage pit.
	std::optional<uint32_t> ownTownPit;
	/// The game's turns in a second, which the agendas count the times they draw in
	float turnsPerSecond {0.0f};
	/// For the joint actions with a friend: how strong its desires to drink, to have a poo and to sleep are, and how
	/// sad it is (the value of the source of sadness)
	float waterDesire {0.0f};
	float pooDesire {0.0f};
	float tiredness {0.0f};
	float sadness {0.0f};
	/// The nearest town, when the creature believes something about it: whether it wants to attack it more than to
	/// help it. None without a town, or without a belief about the nearest town.
	std::optional<bool> attackNearestTown;
	/// The friend of a joint action: where it stands and how tall it is
	struct Friend
	{
		glm::vec2 point {0.0f};
		float height {0.0f};
		/// Its radius on the ground
		float radius {0.0f};
	};
	std::optional<Friend> friendly;
	/// How far each of the two stands from the point halfway between them to kiss, none when either has no kissing
	/// points
	std::optional<float> kissingDistance;
	/// Whether the creature can see the friend (creature_mind::CanSee)
	bool seesFriend {true};
	/// Where the creature's head is, none when its body is not drawn
	std::optional<glm::vec3> head;
	/// The nearest thing the creature knows that can be set on fire, and the nearest it can help (GetNearbyObject)
	std::optional<uint32_t> flammableNearby;
	std::optional<uint32_t> helpableNearby;
	/// Where it and its friend are thrown at in a throwing game (creature_mind::ThrowPointAt)
	std::optional<std::pair<glm::vec3, glm::vec3>> throwPoints;
	/// The town of a joint action with a town, none without a town that has a storage pit
	std::optional<creature_mind::FriendsTown> friendsTown;
	/// The field nearest the creature of those it knows, by its entity's number, and its place, for eating from a field
	/// with a friend; none when it knows no field
	struct FriendsField
	{
		uint32_t field {0};
		glm::vec2 point {0.0f};
	};
	std::optional<FriendsField> friendsField;
	/// The middle of the clear place that two friends sit or sleep in, none when there is no clear place
	std::optional<glm::vec2> clearArea;
	/// The stage of growing up of the creature
	uint32_t stage {k_LeaveHomeStage};
	/// What the desire of the plan is about (the town that it helps, say), by its entity's number; none when the plan is
	/// about nothing
	std::optional<uint32_t> activity;
	/// The place it runs away from or goes to look at, none when its mind keeps none
	std::optional<glm::vec2> miraclePlace;
	/// The radius of the area that holds it, 0 when none holds it
	float confinementRadius {0.0f};
};
/// What a casting action casts, from its row of the game's table and the creature
struct CastInfo
{
	uint32_t magicType {0};
	/// The gesture drawn before casting, 0 for none
	uint32_t gesture {0};
	/// The creature's height
	float height {0.0f};
	/// What it is cast at is a sign that shows a need, which food is cast at from farther away
	bool atNeedsSign {false};
	/// The creature that casts, by its entity's number
	std::optional<uint32_t> caster;
	/// Whether what it is cast at is living
	bool targetLiving {false};
	/// For a fireball: the middle of the clear area near the creature that it goes to first, none when there is none
	std::optional<glm::vec2> clearArea;
	/// To amuse another creature: the spells on creatures that it knows and can pay for, in the order of their types
	std::vector<creature_mind::AmusingSpell> amusingSpells;
	/// For a miracle that goes to a place: where the thing is as the creature makes the agenda
	std::optional<glm::vec3> place;
};
/// Whether a thing wants mending by a creature: one of a town's buildings other than the temple, or a town's totem,
/// while below its full life. A totem's own life is meant: while it stands on its town centre, any harm to it or
/// mending of it goes to the centre instead
[[nodiscard]] constexpr bool NeedsRepair(bool building, bool temple, bool totem, float life)
{
	return ((building && !temple) || totem) && life < 1.0f;
}
/// Whether an action casts a miracle
[[nodiscard]] bool IsCast(const Executor& executor);
/// Whether the game's own test of an action's row lets the creature try it: any action that casts nothing; a cast only
/// when it may cast the miracle, and water on itself only while it is on fire as well
[[nodiscard]] bool MayTry(const Executor& executor, bool mayCast, bool onFire);

/// Whether an action's agenda goes by the creature's height: the rest, home and temple actions, the sacrifice, stealing a
/// thing for the temple, eating from a storage pit or a field, sleeping and pooing at home, waving, looking another
/// creature over, the dances outside the temple and around a thing, bringing things home and the thefts for a town. Each
/// of these reads the creature's height (its drawn size, kept within the sizes a body may take, times 15) as its plan is
/// made
[[nodiscard]] bool WalksByItsHeight(Build build);
/// Whether an action's agenda goes to within the thing's and the creature's 2D radii, a share more: eating from a food
/// pile or a tree
[[nodiscard]] bool ApproachesByRadii(Build build);

/// Whether an action can be carried out now, in this situation, before any thing is chosen
/// An impressive pose is shown again only more than this many game turns after it was last carried out
constexpr int32_t k_ImpressivePoseRepeatTurns = 200;
/// Whether a creature may choose to show off an impressive pose on a game turn: it never carried one out (its last turn
/// 0), or more than k_ImpressivePoseRepeatTurns turns have gone since, counted on whole numbers that may wrap as the
/// game counts them
[[nodiscard]] bool HasntShownImpressivePoseRecently(uint32_t turn, uint32_t lastShown);
[[nodiscard]] bool Possible(const Executor& executor, const Situation& situation);
/// The agenda for an action on a thing (by its entity number, and where it is), if it can be done
[[nodiscard]] std::optional<std::vector<creature_mind::Step>> Agenda(const Executor& executor, std::optional<uint32_t> object,
                                                                     glm::vec2 objectPoint, const Situation& situation,
                                                                     const creature_mind::Random& random,
                                                                     const std::optional<CastInfo>& cast = std::nullopt);

/// When an action's agenda can't be made, whether the plan is still taken up and then fails at once, as the original's
/// sit with no clear place to sit in (its agenda's own failure), rather than not being made: a sit not under a script's
/// control, and a sit on a hill with a friend
[[nodiscard]] bool TakenUpThenFailsWhenUnmade(Build build, bool controlledByScript);

} // namespace openblack::creature_plan_actions
