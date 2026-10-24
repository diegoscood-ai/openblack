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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureTattoo.h"
#include "Enums.h"
#include "Particles/ParticleEffect.h"
#include "TestbedCrowd.h"
#include "TestbedFixtures.h"
#include "TestbedLandTimeline.h"

/// The testbed's scenarios: ready-made set ups of the flat testbed that show one facet of the creatures at a time, such
/// as a thirsty creature finding water or a lineup of bodies. Each is plain data: the land, time of day and weather,
/// where the camera looks, the creatures with their bodies, needs and desires, the objects about them, and a timeline of
/// commands. The debug window's runner applies them through the game's systems; everything here is pure, so the
/// registry and the maths of applying it are tested on their own.
namespace openblack::testbed_scenarios
{

/// Which side of the creatures a scenario shows, for finding it in the list
enum class Facet : uint8_t
{
	/// What it does with nothing to do: fidgeting, sitting, wandering off
	Idle,
	/// Its faces, gestures and the emotes that show its desires
	Expressions,
	/// What it looks at
	Senses,
	/// Its body's needs and how it sees to them: thirst, hunger, sleep, poo, sick, fainting, cold
	Needs,
	/// Growing up and the shapes its body takes
	Growth,
	/// Its skins, hair, tattoos, wounds and eyes
	Appearance,
	/// Its shadow and its reflection
	Light,
	/// Walking, running, turning and finding its way
	Movement,
	Footprints,
	Audio,
	/// Picking things up, looking them over, putting them down, throwing them, eating them, knocking them down,
	/// pointing, and what curiosity, play and anger have it do with them
	Objects,
	/// The player's hand on it: stroking, slapping, and its status panel
	Hand,
	/// Being led on the leashes, tied up and kept at home
	Leash,
	/// Fighting another creature, being knocked out and coming round
	Combat,
	/// What it learns: from strokes and slaps, from mind files, as it grows up, and by copying the player
	Mind,
	/// The particle effects of the miracles and spot visuals
	Particles,
	/// Set ups for trying the in-game editor on: picking, moving and placing things, and its cameras
	Editor,
	/// The miracles: dispensers, casting them and what they do, the creature spells among them
	Miracles,
	/// Crowds of hundreds to thousands of creatures or villagers, all fully simulated, for measuring how the game's
	/// costs grow with their number
	Benchmark,
	/// Creature Mode, the camera locked onto a creature, and the Creature Cave
	CreatureMode,
	/// Things thrown, dropped and knocked: how they fly, what they break and how they come to rest, and what a frame
	/// costs meanwhile
	Physics,
	/// Short scenarios built from the fixtures, as models to copy for new ones
	Examples,
	/// The good and evil advisor spirits on the screen, the trails they leave as they fly, the dialogue's texts, and
	/// the help's did-you-know bubble
	Advisors,
	/// The sky: the sun and its glare, and the light of the day and of the night on the land's trees
	Sky,
	/// The land's animals of their own: the birds' flocks, the sharks, and the grazers' walks round what stands in their
	/// way
	Animals,
	/// The story's chapters, played on the game's own lands through a land entry's timeline=
	Story,

	_Count
};
constexpr size_t k_FacetCount = static_cast<size_t>(Facet::_Count);
[[nodiscard]] std::string_view Name(Facet facet);

/// The weather laid over the whole island, the climates breeding no storms of their own; clear is a mild day
enum class Weather : uint8_t
{
	Clear,
	Rain,
	Thunderstorm,
	Snow,
	Blizzard,
};
[[nodiscard]] std::string_view Name(Weather weather);

struct Environment
{
	/// The hour of the day, as scripts give it, and whether the clock then runs on or stands still
	float hour {12.0f};
	bool clockRuns {true};
	Weather weather {Weather::Clear};
	/// Game turns of the creatures' bodies that pass each game turn
	float bodyTimeScale {1.0f};
	/// Whether creatures faint when exhausted, starved or out of life
	bool fainting {true};
	/// Whether angry creatures pick fights with others nearby by themselves
	bool angerStartsFights {false};
	/// Smiley faces for footprints as on the first of April, or by the date when not given
	std::optional<bool> aprilFools;
	/// The grid of every miracle's dispenser the testbed lays out in front of the camera stays, or is cleared away
	bool dispenserGrid {true};
	/// The footpaths are drawn as lines, as the debug menu's Footpaths box draws them, until the scenario stops
	bool drawFootpaths {false};
	/// The player's prayer power, for watching a maintained miracle run out of it; the testbed's plenty when not given
	std::optional<float> prayer;
	/// The player's alignment, -1 evil to 1 good, as the scenario starts; neutral when not given
	std::optional<float> playerAlignment;
	/// Where the cursor, and so the hand, is put, as a share of the window from its top left, until the mouse moves
	std::optional<glm::vec2> cursor;
	/// The graphics detail level (graphics::detail_level, 0 to 6) the scenario is drawn at; the game's when not given
	std::optional<uint8_t> detailLevel;
	/// The lake has a water point at the middle of its south edge, as a land's script marks a place to drink; only the
	/// scenes where a creature drinks ask for it, so no other scene's things are numbered differently
	bool lakeWaterPoint {false};
};

/// Where the camera looks as the scenario starts
enum class Shot : uint8_t
{
	/// Where the testbed puts it: above and behind the middle of the map, looking north
	Testbed,
	/// Above and to the south of everything in the scenario, all of it in view
	Overview,
	/// Behind and above one creature, keeping up with it
	Follow,
	/// In front of one creature's face, looking at its head and eyes
	Head,
	/// From the framing's eye to the point it looks at, as low or high as they say
	Placed,
};
[[nodiscard]] std::string_view Name(Shot shot);

struct Framing
{
	Shot shot {Shot::Overview};
	/// The creature followed or looked at, by its place in the scenario's creatures
	size_t creature {0};
	/// Further points kept in view by the overview, from the middle of the map, such as the lake; it keeps the creatures,
	/// the objects and wherever the commands send the creatures in view by itself
	std::vector<glm::vec2> include;
	/// Further away for more than 1, closer for less
	float distance {1.0f};
	/// A placed shot's eye and the point it looks at: x east and z north from the middle of the map, y above the land
	/// under each
	glm::vec3 eye {0.0f};
	glm::vec3 look {0.0f};
};

/// A creature's body as the scenario wants it; what is not given is as the creature's species starts
struct NeedOverrides
{
	std::optional<float> energy;
	std::optional<float> exhaustion;
	std::optional<float> dehydration;
	std::optional<float> poo;
	std::optional<float> life;
	std::optional<float> warmth;
	/// Hours of game time it has lived
	std::optional<uint32_t> age;

	[[nodiscard]] bool Empty() const;
};

/// A desire set to a fraction of its maximum, and activated if the creature's stage of growing up hadn't brought it
struct DesireOverride
{
	creature_desires::Desire desire;
	float fraction {1.0f};
};

struct CreatureSetup
{
	/// What the readout calls it, such as "fat"; the species' name when empty
	std::string_view label;
	CreatureType species {CreatureType::Tiger};
	/// Where it stands, from the middle of the map, x east and y north in units of the land
	glm::vec2 offset {0.0f};
	/// Its facing as the spawner gives it: 0 faces south, towards the testbed's camera, and 180 faces north
	float facingDegrees {0.0f};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	/// Whether it is the one creature its owner can lead on the leash. When not given, a player's first creature is
	/// and the others aren't, by the testbed's rule (Scenario::firstCreatureLeads); with that rule off, the game's: the
	/// newest made is. Given, it has the last word, set once all the scenario's creatures are made
	std::optional<bool> leashable;
	/// Its alignment, fatness and strength, the species' start when not given, and its size, 1 when not given: a
	/// creature well grown, about 15 units tall, rather than as small as a new one starts. It keeps any size it is made
	/// with, and its body is drawn at that size kept between 0.05 and 4
	std::optional<float> alignment;
	std::optional<float> fatness;
	std::optional<float> strength;
	std::optional<float> size;
	/// How far it has grown up, from 0; fully grown up when not given
	std::optional<uint32_t> phase;
	NeedOverrides needs;
	std::vector<DesireOverride> desires;
	/// The needs and desires are set again every frame, so they stay as given however time goes
	bool hold {false};
	/// The mind leaves the body alone, to be posed by the commands
	bool pauseMind {false};
	std::vector<creature_tattoo::Slot> tattoos;
	/// Wounds and burns, and drops of blood, on its skin
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;
	/// A mind file it takes up as it starts: "game:NAME" for one of the game's Scripts/CreatureMind files, or
	/// "chosen:NAME" for the one last opened with the spawner's file dialog, falling back to the game's file NAME
	std::string_view mindFile;
};

/// Something put on the land for the creatures: an object, a tree, a feature such as a pillar of rock, a villager, a pot
/// or pile of food or wood, a building or field of the scenario's town, which is made with the first of them, or an
/// animal. A mobile object of the whale's kind is a shark, as a script's CREATE of a whale makes it
struct ObjectSetup
{
	std::variant<MobileObjectInfo, TreeInfo, FeatureInfo, VillagerInfo, PotInfo, AbodeInfo, FieldTypeInfo, AnimalInfo,
	             MobileStaticInfo, AnimatedStaticInfo, FishFarmInfo>
	    type;
	glm::vec2 offset {0.0f};
	float scale {1.0f};
	float yawDegrees {0.0f};
	/// How much a pot holds, which for food is what it is worth to eat: a meal for a grown up creature by default
	int32_t amount {800};
	/// A villager walks to this point, from the middle of the map, once the delay is up, and then back and forth between
	/// it and where it started at the repeat
	std::optional<glm::vec2> walkTo;
	float walkAfterSeconds {1.0f};
	std::optional<float> walkRepeatSeconds;
	/// What a walking villager is to do once there: deciding afresh unless given. A villager walking to decide afresh takes
	/// up no reaction on its way, as in the game
	VillagerStates walkFinal {VillagerStates::DecideWhatToDo};
	/// A villager belongs to the scenario's town of the player, rather than to none
	bool joinTown {false};
	/// A villager lives in the abode made by the scenario's object of this place in its list, once every object is out
	std::optional<size_t> livesIn;
	/// A villager is put down on the nearest teleport stone after this many seconds, as the hand drops one, and jumps at
	/// once to the stone nearest where it was walking
	std::optional<float> dropOnStoneSeconds;
	/// A villager sets off to worship at this point once the walk delay is up, going through its player's teleport
	/// stones when the point is further than villagers walk to worship
	std::optional<glm::vec2> worshipAt;
	/// A villager's, animal's or building's life, 0 (dead) to 1, full when not given; a building is hurt down to it
	std::optional<float> life;
	/// A tree belongs to the land's forest of this number, none for 0; and grows to this size, its scale when not given
	uint32_t forest {0};
	std::optional<float> fullSize;
	/// A villager or animal that has eaten poison
	bool poisoned {false};
	/// An abode is put down as a plan of the scenario's town being built, with its building site, nothing of it built
	bool underConstruction {false};
	/// A villager's whole speed (map units a turn, as the game keeps it), set by a script's SET_PROPERTY of SPEED as each
	/// of its walks starts. The villager is then the script's own from the start, as one a script creates and keeps in
	/// a variable, so that it keeps the speed through its walk
	std::optional<uint16_t> scriptSpeed;
	/// A building of the town holds this much food, or wood, rather than the scenario town's usual stock
	std::optional<uint32_t> storedFood;
	std::optional<uint32_t> storedWood;
	/// Made immovable as a script's SET_ID_MOVEABLE false makes it: the physics gives it no body
	bool immovable {false};
	/// An animal starts as hungry as its kind gets, as its needs leave it once its hunger has had its turns, so that
	/// food put down within its reach draws it at once
	bool hungry {false};
	/// A field starts with all its crops sown, ripe and full of food
	bool ripeCrop {false};
};

/// A particle effect played on the land
struct ParticleSetup
{
	ParticleType type {ParticleType::Smoke};
	/// An effect file played in place of the type's, by its name, for the effects no particle type names
	std::string_view file;
	particles::DrawPath path {particles::DrawPath::Sorted};
	/// The scenario's creatures are given to it to act on, as the heal miracle is given the people it heals
	bool targetsCreatures {false};
	/// One of the scenario's objects, by its place, given to it to act on, as a script gives an effect its object
	std::optional<size_t> targetObject;
	glm::vec2 offset {0.0f};
	/// Above the land
	float height {0.0f};
	float magnitude {1.0f};
	/// The player whose colour it takes where it takes one
	int player {0};
	/// Seconds after which it closes down and starts again, for effects that end; none to run until the scenario stops
	float restartSeconds {0.0f};
};

/// A miracle dispenser, or a one-shot bubble on its own, put down for the scenario
struct DispenserSetup
{
	MagicType type {MagicType::Fireball};
	glm::vec2 offset {0.0f};
	/// Only the bubble, floating this high above the land, without its dispenser
	std::optional<float> bubbleHeight;
	/// A testbed set-up step: the dispenser starts with its period already counted and floats its globe at once, rather
	/// than after its period as a dispenser put down in the game does
	bool charged {true};
};

/// A miracle cast in the scenario, as if from a hand above the land
struct MiracleCast
{
	enum class Target : uint8_t
	{
		/// At the point
		Point,
		/// On one of the scenario's creatures, by its place
		Creature,
	};
	MagicType type {MagicType::Fireball};
	Target target {Target::Point};
	/// The point cast at, from the middle of the map
	glm::vec2 point {0.0f};
	size_t creature {0};
	/// Where the hand casting it is, from the middle of the map, and how high above the land
	glm::vec2 handOffset {0.0f};
	float handHeight {15.0f};
	/// The hand's movement as it lets go, which throws a fireball
	glm::vec3 throwVelocity {0.0f};
	/// Seconds after the scenario starts that it is cast
	float delaySeconds {1.0f};
	/// A miracle held in the hand (lightning, water, food, wood) runs this long, then stops; none to run its course
	std::optional<float> holdSeconds;
	/// Seconds after which it is cast again, none for once
	std::optional<float> repeatSeconds;
	/// Cast through the hand as a player does, rather than straight at its point: a seed of it is put in the hand, the
	/// action button pressed with the hand over its point (or creature), and let go after its hold, the hand moving by
	/// its throw velocity meanwhile
	bool byHand {false};
	/// By hand, the seed comes from the player's worship, charged from their prayer power and ready after a moment, rather
	/// than from a bubble
	bool fromWorship {false};
	/// By hand, a circle drawn round the point first, of this radius, for the storms and shields; none draws none
	std::optional<float> circleRadius;
	/// Cast straight at its point by this player, rather than the player's
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// By hand, the hand sweeps on at its throw velocity for this long after letting go, as a player sweeps a flock out
	std::optional<float> sweepSeconds;
	/// The caster's alignment, set as it is cast (a flying flock is bats for an evil caster, doves for a good one)
	std::optional<float> casterAlignment;
	/// By hand, the seed is held this many seconds before the button goes down, rather than just long enough to be ready
	std::optional<float> holdBeforePress;
	/// A shield is broken this many seconds after it is cast, as by a blow its caster can't pay for: its people react to
	/// its fall and it closes down. A testbed set-up step.
	std::optional<float> breakSeconds;
};

/// Something a creature is told to do, in turn with the scenario's other commands
struct Command
{
	enum class Kind : uint8_t
	{
		/// Going to the point, by walking or running; following the other creature; running away from the point;
		/// turning to face the point, or the camera; stopping
		WalkTo,
		RunTo,
		Follow,
		FleeFrom,
		TurnToFace,
		FaceCamera,
		Stop,
		/// Playing the animation: an action, a gesture on top of the body, or a face
		PlayAction,
		PlayGesture,
		PullFace,
		/// Pulling the face its mind would for a feeling or what it does (value, by the face cues)
		ShowFeeling,
		SitDown,
		StandUp,
		/// Seeing to a need now: sleeping, waking, eating the nearest food, drinking at the nearest water, having a
		/// poo, being sick, fainting
		Sleep,
		Wake,
		Eat,
		Drink,
		Poo,
		Puke,
		Faint,
		/// As if the player's hand stroked or slapped it
		Stroke,
		Slap,
		/// The hour of the day jumps, for every creature
		SetHour,
		/// Walking up to the scenario's object and picking it up; then, with what it holds, putting it down, tossing it
		/// away, lobbing it, eating it, or looking it over with one of its four ways of doing so (stroking, shaking,
		/// smelling, examining); throwing it at the point
		PickUp,
		PutDown,
		Discard,
		Lob,
		EatHeld,
		Examine,
		ThrowAt,
		/// Walking up to the scenario's object and knocking it down; pointing at the point
		KnockDown,
		PointAt,
		/// The player's hand stroking a part of its body, or slapping it at a height and hard or gently, held to it
		/// until the hand lets go, which tells its mind how it was treated
		HandStroke,
		HandSlap,
		HandLetGo,
		/// Putting a leash on it, which it is first taught; tying the leash to the scenario's object, untying it to the
		/// hand, taking it off; keeping it within a radius of where it stands
		PutOnLeash,
		TieLeash,
		UntieLeash,
		TakeOffLeash,
		ConfineToHome,
		/// Making it the one creature its owner can lead, which the owner's other creature stops being; the player's
		/// hand tapping it to put the picked leash on, as the Action button does; the player pressing a leash shortcut
		/// (value: 0 the leash key L, 1 the previous leash V, 2 the next leash B), which acts on the player's creature
		MakeLeashable,
		HandTapLeash,
		LeashKey,
		/// The player shaking the leash off: a scribble drawn with the empty hand through the same gesture recogniser
		/// the cursor goes through, which takes off a leash held in the hand
		LeashShake,
		/// Fighting the other creature; then, in the fight, a blow high, in the middle or low charged for a while, a
		/// block, a step forward, back, right or left, the special move, and fighting by itself or not, as the player's
		/// clicks and the debug tools give them; a spell (value, by its magic type) cast in the fight
		StartFight,
		FightBlow,
		FightBlock,
		FightStep,
		FightSpecial,
		FightAuto,
		FightCast,
		/// Being knocked out, as a fight's loser is, and brought round again
		KnockOut,
		BringRound,
		/// Tying the leash it wears to the other creature, which has it fight that creature
		TieLeashToCreature,
		/// A desire (by value) is set to a fraction (amount) of its maximum; the creature grows up to a stage (by value)
		SetDesire,
		SetPhase,
		/// From now on, each thing it does to something is judged once done, by the mind's trainer: stroked if the
		/// thing is of a kind (value, the game's belief type, such as 6 for a villager), else slapped
		RewardIf,
		/// It watches a skill (value, by its row of the game's table) or a miracle near it; the player does one of the
		/// deeds creatures copy (value) at the point
		SeeSkill,
		SeeMiracle,
		PlayerDid,
		/// C pressed: Creature Mode locks onto the player's creature, or lets go of it; a double click on the creature;
		/// the cursor keys (value: left, right, up or down) held with Shift, or with Ctrl, for some seconds (amount);
		/// Ctrl and Shift pressed together for a clear view; the camera given back
		CreatureKey,
		DoubleClick,
		CameraKeys,
		ClearCameraView,
		LeaveCreatureMode,
		/// F5 pressed: the Creature Cave opens, on a page (value); a symbol (value) tattooed on a place (body part), or
		/// the place's symbols taken off
		OpenCreatureCave,
		ApplyTattoo,
		RemoveTattoo,
		/// The player's hand is given a seed (value, by the game's seed number) as if from a bubble; a gesture (value, by
		/// the game's gesture number) is drawn with the hand across the middle of the screen, through the same recogniser
		/// the cursor goes through. Neither needs a creature.
		HoldSeed,
		DrawGesture,
		/// The player's hand is given a seed (value) summoned from their worship, charged from their prayer power, which
		/// can be powered up
		SummonSeed,
		/// The creature knows a miracle (value, by its magic type) as if it had learnt it; it is told to cast a miracle
		/// (value) at the scenario's object, or at another creature (atCreature), going about it as by itself
		KnowMiracle,
		CastMiracle,
		/// The player presses a key's action (value, the bindable action's flag) for one frame, as the options screen's
		/// presses are made. Needs no creature
		PressKey,
		/// The player's hand takes hold of the newest fireball in flight it can see, the pointer kept on it: an empty hand
		/// clicks on it, or holds the action button past the grab's 225 ms wait with value 1 (either catches another
		/// player's); a hand with a seed rests over it with no button (a fire seed takes it in)
		HandTakeFireBall,
		/// The player's mouse, through the same input the real one goes through: the pointer put at a point on the
		/// screen (point, as fractions of its width and height from the top left); a button (value: 1 left, 2 middle,
		/// 3 right) pressed or let go; the mouse moved by point, as fractions of the screen, over some seconds
		/// (amount); the wheel turned some notches (value) away from the player, or towards with ctrl
		PointerTo,
		PointerPress,
		PointerRelease,
		PointerSweep,
		WheelTurn,
		/// The player's alignment jumps, which the hand shows
		SetAlignment,
		/// The game changes to the land of a script (landScript) through its own land change, as a map script asks for
		/// one, which ends the scenario. Needs no creature
		ChangeLand,
		/// The scenario's villager (object) walks to the point, going round what stands in its way as the villagers'
		/// own walks do, and stands there once there, as a script's villager does. Needs no creature
		VillagerWalkTo,
		/// The scenario's storage pit (value, by its place in the objects) takes the scenario's object as a thrown
		/// object that lands in it is taken: its resource goes into the pit and the object is deleted at once. Waiting
		/// until free, it waits until a villager walks round the object. Needs no creature
		StoreTakes,
		/// The game speed is set (gameSpeed), as a script sets it; it stays as set once the scenario ends, as the
		/// window's speed buttons do. Needs no creature
		SetGameSpeed,
		/// The Action (right) button goes down as the second click of a double click, as the window marks a quick
		/// second press, through the same input as PointerPress, and the hand takes the double click; PointerRelease
		/// lets it go. Needs no creature
		PointerDoubleClick,
		/// A script's open (value 1) or close (value 0) of the scenario's object (a gate, the gate stone plinth); the
		/// player's hand laying the scenario's object, a gate stone, in the plinth that is the scenario's object of this
		/// place (value). Neither needs a creature.
		SetOpenClose,
		LayGateStone,
		/// An advisor spirit (value: 0 the good one, 1 the evil one) flies out from its corner of the screen to a spot
		/// on it, as the spirits' debug window's Eject does; it flies back home. Needs no creature
		SpiritOut,
		SpiritHome,
		/// A map script's CREATE_WORSHIP_SITE: the player's temple's worship site of a tribe (value, the game's tribe
		/// number) is built at once, and the player's town of that tribe joins it. Needs no creature
		CreateWorshipSite,
		/// A script's cinema bars come on (value 1) or go (value 0), as a script's wide screen does, owned by the
		/// scenario as by a script's task: the interface is inactive while they are up. Put away once the scenario
		/// stops. Needs no creature
		WideScreen,
		/// A script's RUN_TEXT of the text (value, its id) in the dialogue, waiting for the player's click. Needs no
		/// creature
		ShowText,
		/// A full screen film (value): 0 the intro film, as a script's SET_AVI_SEQUENCE(1) starts it; 1 the falling
		/// spell's film, as SET_AVI_SEQUENCE(2) starts it but without its test for the player's creature. Like every
		/// film it pauses the game, so the scenario's clock stands still until it ends. Needs no creature
		PlayFilm,
		/// A land vortex made at the point, as a script's CREATE makes one: value 0 the way out of a land (In), 1 the way
		/// into the next (Out), 2 the volcano's mouth; a vortex the scenario made (value, by the order it was made in) starts
		/// to fade in, or to fade out, as a script fades it. None needs a creature
		CreateVortex,
		VortexFadeIn,
		VortexFadeOut,
		/// A way into a land the scenario made (value, by the order it was made in) reads the text of a crossing file
		/// (crossingFile) in place of the file in the save folder, as if the way out of the land before had written it:
		/// given in the same frame as the vortex is made, before its first emission. A run of the checks never writes
		/// the file, so this is how a scenario gives one. Needs no creature
		VortexCrossingFile,
		/// A script's SET_FADE to a colour (fadeColour, 0xRRGGBB) over fadeSeconds, or with value 1 its SET_FADE_IN
		/// back to normal over fadeSeconds, through the cinematic director as a script's goes. Put back to normal at once
		/// when the scenario stops, if it faded. Needs no creature
		ScriptFade,
		/// A script's SET_HIGH_GRAPHICS_DETAIL on the scenario's object (object, a villager): value 1 draws it in high
		/// detail with its eyes, through the highDetailSystem slot as the script's goes, value 0 back to normal. Only
		/// while a script's wide screen is up (WideScreen) does it stay so. Needs no creature
		HighDetail,
		/// A key of the keyboard (value, its SDL scancode) goes down, or is let go, as the window gives a real key's
		/// press and release to the game's input. A key still down when the scenario stops is let go then. Neither
		/// needs a creature
		KeyDown,
		KeyUp,
		/// A script's SET_FOCUS on the creature, at the point: its arguments pushed on the scripts' stack and the native
		/// called, as a script's call does. The creature is taken under the script's control, stops what it was doing and
		/// turns to face the point, its head looking there; then a script's RELEASE_FROM_SCRIPT on it the same way, which
		/// gives it back to its own mind
		ScriptFocus,
		ScriptRelease,
		/// A script's GET_PROPERTY of HEALTH on the scenario's object (object), through the native as a script's call
		/// goes; the log says what it gave back, and as which type. Needs no creature
		ScriptHealth,
		/// A script's SET_PROPERTY of a property (value, a script ObjectPropertyType: HEALTH or BUILT_PERCENTAGE) to
		/// `amount` on the totem of the scenario's town centre (object), found by GET_TOTEM_STATUE, through the natives as
		/// a script's calls go; the log says what was set. Needs no creature
		ScriptTotemProperty,
		/// Only logs whether the creature is on a leash now, and which, changing nothing
		LeashState,
		/// A script's CREATE_REWARD of a type (value, 1 to 60) at the point, on the land or from the sky (fromSky), through
		/// the native as a script's call goes; the log says what it gave. Needs no creature
		ScriptReward,
		/// A script's DANCE_CREATE of a dance (value, a row of the game's dances) at the point, around the scenario's
		/// object (danceAround) or none, for a time (danceTime, as the script gives it), through the native as a
		/// script's call goes; the log says what it gave. Needs no creature
		ScriptDance,
		/// The player's hand taps the script highlight nearest the point, as the interface's tap gives it: a
		/// did-you-know sign with a tip opens its tip bubble, and a second tap on it closes the bubble. Needs no
		/// creature
		TapHighlight,
		/// A script's FLOCK_ATTACH of the scenario's object (object) to the dance the last ScriptDance made, not as its
		/// leader, through the native as a script's call goes; the log says what it gave. Needs no creature
		ScriptDanceAttach,
		/// A script's SET_INTERFACE_INTERACTION of a level (value, the game's interface level: 0 normal, 8 just the
		/// hand's moves, 13 just grabbing, double clicks and turning, ...), through the native as a script's call goes.
		/// The level the scenario found is put back once it stops. Needs no creature
		ScriptInterfaceLevel,
		/// The player's (player) temple built this far (built, 0 to 1), as its building would leave it: the citadel's
		/// process draws it so built from its next turn. Needs no creature
		TempleBuilt,
		/// The Creature Cave's tattoo editor opened on the cave's creature, as the cave's own button opens it. While it
		/// is open the scenario's pointer (PointerTo, PointerPress, PointerSweep, PointerRelease) is handed to its dialog
		/// as the window's mouse is. Needs no creature (the cave's own)
		OpenTattooEditor,
		/// One of the player's advisors (value: 0 the good one, 1 the evil one) is told to do something, as the scripts
		/// tell it (advisor). Needs no creature
		Advisor,
		/// The scenario's object (object), an abode put down being built, is built this far (built, 0 to 1), as
		/// building work leaves it: drawn cut at that height, with its inner walls and the cap over them. Needs no
		/// creature
		AbodeBuilt,
		/// Taking the creature out of the game for good, as a script's DELETE of it does
		RemoveCreature,
		/// Leaving the temple, as its way out does once the frame is done with it. Needs no creature
		LeaveTemple,
		/// A script's FLOCK_CREATE at the point, then its POPULATE_CONTAINER of the new flock with `amount` things of a
		/// type (value, the scripts' object type) and a subtype (subtype), both through the natives as a script's calls
		/// go; the log says what came of it. Needs no creature
		ScriptPopulate,
		/// A script's SET_SCRIPT_ULONG on the creature, of an action (value, by its number in the creature's
		/// animations: 52 and over an individual action, up to 51 a static one) and how many times (plays), then its
		/// SET_SCRIPT_STATE, through the natives as a script's calls go: the creature plays it under the script's control
		ScriptPlay,
		/// A script's PLAYED on the creature, through the native as a script's call goes; the log says what it gave back
		ScriptPlayed,
		/// A script's MOVE_GAME_THING of the creature to the point, within radius (0 for the game's own two legs),
		/// through the native as a script's call goes: the creature walks there under the script's control
		ScriptMove,
		/// A script's CREATURE_DO_ACTION on the creature: an action of the game's table (value, its row) done to the
		/// scenario's object (object), or to another creature (atCreature), with the scenario's object `with` or with
		/// nothing, through the native as a script's call goes
		ScriptDoAction,
		/// An advisor spirit (value: 0 the good one, 1 the evil one) says a line of its own voices (spiritLine), as a
		/// script's text in that spirit's voice does: both spirits' lines are stopped, then it says it. Needs no creature
		SpiritSay,
		/// A script's START_MUSIC of a music type (value: 0 stops the script's music), through the native as a script's
		/// call goes. Needs no creature
		ScriptMusic,
		/// Only logs the script's music: its type, and the line and beat its markers are at, changing nothing. Needs no
		/// creature
		MusicState,
		/// The scenario's town takes belief (amount) in the player, as a gift to its store gives it: the town's next
		/// belief fold adds it to what it gained this period, which the next tenth turn tells the player as believers
		/// gained, over the land under the cursor. Needs no creature
		TownGainsBelief,
		/// A script's OBJECT_DELETE of the player's (player) temple's heart with the temple's explosion, through the native
		/// as a script's call goes: a heart being destroyed has its model broken up, any other starts its destruction.
		/// Needs no creature
		ScriptDeleteTemple,
		/// A script's WALK_PATH of the scenario's object (object) along a track of the game's camera file (value, its
		/// number), from and to shares of the track's time (pathFrom, pathTo), backwards or not (pathBackwards),
		/// through the native as a script's call goes; the log says whether it is walking. Needs no creature
		ScriptWalkPath,
		/// A script's snapshot of the testbed's challenge, through the native as a script's call goes: SNAPSHOT (value 0),
		/// UPDATE_SNAPSHOT (1) or UPDATE_SNAPSHOT_PICTURE (2), with its success (amount) and alignment (alignment). The
		/// log then lists every challenge record and the local player's alignment. Needs no creature
		ScriptChallengeSnapshot,
		/// A script's OBJECT_DELETE of the scenario's object (object), plainly deleted (mode 0), through the native as a
		/// script's call goes: it leaves the map on the next turn. Needs no creature
		ScriptDelete,
		/// A script's SNAPSHOT of a challenge (value, its number) through the native: its title (subtype), how far it
		/// is done (amount) and its alignment, a quest or not (quest), no reminder script and no camera. Needs no
		/// creature
		ScriptSnapshot,
		/// The player's hand takes the scenario's object at once, as if picked up from where it stands (needs no
		/// creature); the hand gives what it holds to the creature, as the Action button held over it does
		HandTake,
		HandGive,
		/// The player reads a "did you know" tip (value, by the scripts' number) of a category (amount, rounded), as when
		/// the hand taps its sign. The library lists the tip. Needs no creature
		ReadTip,
		/// The temple's camera looks at a scroll of the room the player is in (value: its place among the room's
		/// scrolls), as when the player presses the scroll. Needs no creature
		LookAtTempleScroll,
		/// Every kind of the advisors' guidance counts as heard as long ago as can be, so the next remark due is said
		/// at once rather than after its usual wait. Needs no creature
		EndGuidanceWaits,
		/// A script swaps the minds of the creature and another (value, by its place in the scenario's creatures), as
		/// "swap creature from A to B" does
		SwapMinds,
		/// A script's CREATE_WITH_ANGLE_AND_SCALE of a totem (a puzzle totem: script type 34) of an abode info (value) at
		/// the point, turned angleDegrees and of scale amount, then its HEIGHT set to its MAX_HEIGHT times `built` (1 all
		/// its height, 0 sunk) and, when unpickupable, SET_ID_PICKUPABLE false, through the natives as a script's calls
		/// go; the log says what was made and its HEIGHT and MAX_HEIGHT. Needs no creature
		ScriptPuzzleTotem,
		/// Only logs the HEIGHT and MAX_HEIGHT of every puzzle totem the scenario's scripts made, as GET_PROPERTY reads
		/// them, and whether the hand holds it, changing nothing. Needs no creature
		PuzzleTotemHeights,
		/// A script's CREATE at the point of a thing of a type (value, the scripts' object type) and a subtype (subtype),
		/// through the native as a script's call goes; the log says what was made and, for a creature, its owner and
		/// whether it takes up reactions. Needs no creature
		ScriptCreate,
		/// Kicking the scenario's object in front of the creature
		Kick,
		/// The temple's rooms open as the main room's first drawing opens them, as the temple debug window's "Open the
		/// rooms" does, for a scenario that goes straight into a room on the first visit. Needs no creature
		OpenTempleRooms,
		/// The creature catches fire, and burns as at the start of a fire. With a value of 1, the scenario's object (by
		/// its place) catches fire in place of the creature
		SetAlight,
		/// The player's hand takes hold of the creature as its own pick of a creature does (not as a command's hold, so
		/// the creature's mind is told it was taken hold of); HandLetGo lets go
		HandGrab,
		/// A script's natives on a creature, through the natives as a script's calls go, the log saying what came of
		/// them: CREATURE_SET_DESIRE_ACTIVATED of every desire off; a desire (value) on, with CREATURE_SET_DESIRE_VALUE
		/// and CREATURE_SET_DESIRE_MAXIMUM both at `amount`. Both act on the creature the scenario's last
		/// ScriptCreateFromCreature made, when one did, else on the command's creature
		ScriptDesiresOff,
		ScriptDesire,
		/// A script's CREATE_REACTION of a kind (value) at the creature
		ScriptReaction,
		/// A script's CREATURE_CREATE_RELATIVE_TO_CREATURE from the creature, at the point, of a species (value, as the
		/// scripts number them) at a scale (amount)
		ScriptCreateFromCreature,
		/// A script's SET_PROPERTY of SPEED on the creature to `amount`, then its GET_PROPERTY of SCALE
		ScriptSpeed,
		/// The share of the scenario's town's people at worship set to `amount` (0 to 1), as the hand letting go of the
		/// town's totem sets it: the town sends its villagers to its worship site, or calls them back, to match. Without
		/// a worship site the share stays 0. Needs no creature
		SetWorshipShare,
		/// Waits, as long as it takes, for a script global (global) to hold up against `amount` as `compare` says; for a
		/// task of a script (script) to be running (value 0), none to be running (1), or none to be running after one
		/// was (2); for a text (value, its id) to have been shown, and with `read` to have been read too. Each waits
		/// until free, and needs no creature
		WaitGlobal,
		WaitTask,
		WaitText,
		/// A script global (global) set to `amount`, keeping its type; a script (script) taking no parameters started as
		/// a new task, as the LHVM viewer's Start does. Neither needs a creature
		SetGlobal,
		StartScript,
		/// Only logs the script globals named in `global` and whether each script named in `script` runs, both
		/// separated by spaces, changing nothing: a chapter's end check. Needs no creature
		LogGlobals,
		/// From now on (value 1), or no longer (value 0), a text waiting for the player's click is clicked on as soon as
		/// it takes the click, as the player's click does. Needs no creature
		ClickTexts,
		/// The camera put at a point (eye, x and z from the scenario's origin, y above the land there) looking at
		/// another (look), as a placed shot is; or kept behind and above a thing (thing), or the command's creature
		/// without one, at a distance (amount, 1 as a follow shot) until value 1 lets it go. Neither needs a creature
		CameraTo,
		CameraFollow,
		/// The creature's size set to `amount`, as the creature spawner's size slider sets it
		SetCreatureSize,
		/// A script's SET_POSITION of a thing (thing), or of the command's creature without one, to the point, through
		/// the native as a script's call goes. Needs no creature
		MoveThing,
		/// The player's hand held over the point of the land, the pointer kept on it wherever the camera goes, until
		/// value 1 lets the pointer go; the hand lets go of what it holds where it is, as when it is made to. Neither
		/// needs a creature
		HandOver,
		HandDrop,
		/// Waits, as long as it takes, for the camera to come within `radius` of the point of the land (point, at the
		/// land's height there), measured as a script's distance from the camera to a marker is. Waits until free, and
		/// needs no creature
		WaitCamera,
		/// Not a command: how many kinds there are
		Count,
	};
	Kind kind {Kind::Stop};
	/// Which creature, by its place in the scenario's creatures
	size_t creature {0};
	/// The player whose hand or keys do it
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// Seconds after the last command before this one, counted once its creature is free when it waits for that
	float delaySeconds {0.0f};
	/// Waits until its creature has stopped moving and its body plays nothing, such as having arrived; for a command
	/// to the player's hand, until the hand has drawn the whole of the gesture it was drawing; for a store taking an
	/// object, until a villager of the scenario walks round that object
	bool waitUntilFree {false};
	/// The point gone to, fled from or faced, from the middle of the map
	glm::vec2 point {0.0f};
	/// The animation played, the creature followed or fought, the band of a blow (high, middle, low), the step
	/// (forward, back, right, left), or whether it fights by itself (1) or not
	size_t value {0};
	/// The hour jumped to
	float hour {12.0f};
	/// The object picked up, knocked down, tied to or taken by a store, or the villager walking, by its place in the
	/// scenario's objects
	size_t object {0};
	/// The leash put on
	LeashType leash {LeashType::Rope};
	/// The part of the body stroked, by its place in the hand's parts (the head first)
	size_t bodyPart {0};
	/// How high a slap lands, as a share of the creature's height, whether it is gentle, and which way it sweeps
	float slapHeight {0.85f};
	bool gentle {false};
	bool sweepsRight {false};
	/// How far from where it stands it is kept
	float radius {0.0f};
	/// How long a blow's click is held, charging it
	float chargeMs {0.0f};
	/// A desire's fraction of its maximum, or the seconds the camera's keys are held
	float amount {1.0f};
	/// The camera's keys are held with Ctrl rather than Shift
	bool ctrl {false};
	/// The creature a miracle is cast at, or that a script's action is done to in place of the object, by its place in
	/// the scenario's creatures
	std::optional<size_t> atCreature;
	/// The player's alignment jumped to, from -1 (evil) to 1 (good)
	float alignment {0.0f};
	/// The land's script changed to, in the game's Scripts folder, such as "Land2.txt"
	std::string_view landScript;
	/// A crossing file's text, its land-script lines as a way out of a land writes them
	std::string_view crossingFile;
	/// The game speed set: how much faster than normal the game's time runs (2 twice as fast, 0.5 half as fast)
	float gameSpeed {1.0f};
	/// A script fade's colour, 0xRRGGBB, and its time in seconds (truncated to whole seconds, as SET_FADE's)
	uint32_t fadeColour {0};
	float fadeSeconds {0.0f};
	/// An action played mirrored or not, rather than on the toss of a coin
	std::optional<bool> mirrored;
	/// A script's reward falls from the sky, rather than being put on the land
	bool fromSky {false};
	/// The object a script's dance is made around, by its place in the scenario's objects, or none
	std::optional<size_t> danceAround;
	/// A script's dance's time, the number the script gives (the game takes its whole part)
	float danceTime {0.0f};
	/// How far the player's temple, or the scenario's abode, is built, 0 to 1
	float built {1.0f};
	/// What an advisor is told: to come out of its corner, or to appear in a puff of smoke; to go home, or to vanish; to
	/// cling to or fly to a point on the screen (point, from 0 to 1 across and down); to point at that point on the
	/// screen, or at a point of the land (point, from the middle of the map), out in the world or not (gentle); to look
	/// at the point of the land; to play an anim (anim) at the point on the screen at a speed (amount); to feel an
	/// emotion (anim)
	enum class AdvisorAction : uint8_t
	{
		Out,
		Appear,
		Home,
		Vanish,
		Cling,
		Fly,
		PointOnScreen,
		PointAtLand,
		LookAtLand,
		PlayAnim,
		Feel,
	};
	AdvisorAction advisor {AdvisorAction::Out};
	uint32_t anim {0};
	/// The subtype a script gives with a type, as ScriptPopulate takes it
	uint32_t subtype {0};
	/// How many times a script's play plays its action
	uint32_t plays {1};
	/// The line a spirit says, by its place among the advisors' voices (HelpSprites), from 1
	uint32_t spiritLine {0};
	/// The shares of a track's time a script's walk along it starts at and stops at, and whether it goes backwards
	float pathFrom {0.0f};
	float pathTo {1.0f};
	bool pathBackwards {false};
	/// A challenge's snapshot is a quest's
	bool quest {false};
	/// A script's action is done to the scenario's first temple in place of the object
	bool onTemple {false};
	/// What a script's CREATURE_DO_ACTION has the creature do the action with, by its place in the scenario's objects;
	/// none for nothing
	std::optional<size_t> with;
	/// ScriptPuzzleTotem: the angle the script turns it, in degrees, and whether the script makes it unpickupable
	float angleDegrees {0.0f};
	bool unpickupable {false};
	/// A script global's name, or a list of them separated by spaces (LogGlobals)
	std::string_view global;
	/// A script's name, or a list of them separated by spaces (LogGlobals)
	std::string_view script;
	/// How a global waited on holds up against `amount`
	testbed_land_timeline::Compare compare {testbed_land_timeline::Compare::Equal};
	/// A text waited on is read too, not only shown
	bool read {false};
	/// The thing of the land the command acts on, in place of the scenario's object (object), and for LayGateStone the
	/// plinth, in place of the scenario's object of that place (value): what a land timeline acts on, the land having
	/// no objects of the scenario's
	std::optional<testbed_land_timeline::ThingNear> thing;
	std::optional<testbed_land_timeline::ThingNear> other;
	/// A camera's eye and the point it looks at: x and z from the scenario's origin, y above the land under each
	glm::vec3 eye {0.0f};
	glm::vec3 look {0.0f};
};
/// What the testbed knows of each kind of command: its name, and whether it needs no creature or is the player's mouse
struct CommandKindInfo
{
	Command::Kind kind {Command::Kind::Stop};
	std::string_view name;
	bool needsNoCreature {false};
	bool pointer {false};
};
/// One row for every kind of command, in the enum's order
[[nodiscard]] std::span<const CommandKindInfo> CommandKinds();
[[nodiscard]] std::string_view Name(Command::Kind kind);
/// Whether a command is the player's mouse, which needs no creature
[[nodiscard]] bool IsPointerCommand(Command::Kind kind);

/// The player's hand held still over the land for the whole scenario, as a player holds it: from the middle of the map,
/// and how high above the land
struct HandHold
{
	glm::vec2 offset {0.0f};
	float height {6.0f};
};

/// One of the scenario's objects thrown into the physics as the player's hand throws it, again and again if asked
/// A flock of the land's birds, made as a land script makes one: the flock, then its birds about a point, each at a
/// random age
struct BirdFlockSetup
{
	AnimalInfo kind {AnimalInfo::Dove};
	/// Where the flock is made and its home, from the middle of the map
	glm::vec2 offset {0.0f};
	uint32_t count {10};
	/// How far from its home the leader's legs take it (its kind's way for 0), and how far from the leader the others
	/// keep
	float reach {0.0f};
	float flockDistance {10.0f};
};

struct ThrowSetup
{
	/// The object, by its place in the scenario's objects
	size_t object {0};
	/// It is lifted to this point, from the middle of the map, before it is thrown; none to throw it from where it lies
	std::optional<glm::vec2> from;
	/// How high above the land it is let go
	float height {6.0f};
	glm::vec3 velocity {0.0f};
	/// Seconds after the scenario starts that it is thrown
	float delaySeconds {1.0f};
	/// Seconds after which it is thrown again, none for once. A throw due while it still flies waits for it to land.
	std::optional<float> repeatSeconds;
};

/// A town with no members, as a land script's CREATE_TOWN makes one: the player's Celtic town at a point, with no abode,
/// villager or field of its own
struct BareTown
{
	glm::vec2 offset {0.0f};
	/// How much it believes in the player, as the scenario's town's belief; left as made when not given
	std::optional<float> belief;
};

/// A footpath as a land's footpath file lays one: its nodes in order, at points from the middle of the map, starting
/// on an object's own place if given; and the objects whose footpaths it is, as the file's links give them
struct FootpathSetup
{
	std::optional<size_t> startsAt;
	std::vector<glm::vec2> nodes;
	std::vector<size_t> leadsFrom;
};

struct Scenario
{
	/// Unique and never changed, for picking it from the command line or a test
	std::string_view id;
	std::string_view name;
	Facet facet {Facet::Idle};
	/// What it sets up and what to look for
	std::string_view description;
	std::string_view expected;
	Environment environment;
	Framing framing;
	std::vector<CreatureSetup> creatures;
	std::vector<ObjectSetup> objects;
	std::vector<ParticleSetup> particles;
	std::vector<DispenserSetup> dispensers;
	std::vector<MiracleCast> miracles;
	std::vector<Command> commands;
	/// After the last command, the commands go round again from this one
	std::optional<size_t> repeatFrom;
	/// The hand held still in view, rather than left wherever the mouse is
	std::optional<HandHold> hand;
	/// The scenario's town's belief in the player, set once its objects are out, as a map script's SET_TOWN_BELIEF
	/// follows the town it makes. Without it the town believes in no one, and its first belief fold gives it to the
	/// neutral player
	std::optional<float> townBelief;
	/// A testbed set-up step until worship sets it: the player's power multiplier for a tribe, put back to 1 when the
	/// scenario stops
	std::optional<std::pair<Tribe, float>> tribalPower;
	/// A crowd spawned a batch a frame, after the creatures and objects above; the runner measures the frames once it
	/// is all there
	std::optional<Crowd> crowd;
	/// Every miracle's position and the land's height under it are logged this often, in seconds
	std::optional<float> logMiraclesEvery;
	/// Every building's life, and whether it is a site being repaired and by how many, are logged this often, in seconds
	std::optional<float> logBuildingsEvery;
	/// Objects thrown by the player's hand
	std::vector<ThrowSetup> throws;
	/// The footpaths between the objects, laid once they are made
	std::vector<FootpathSetup> footpaths;
	/// Lines of a land's script, run after the footpaths are laid. Each place is a distance from the middle of the map,
	/// as for the objects
	std::string_view landScript;
	/// The land's colour of the frame, the whole mean of its bytes and the hand light's strength from it are logged each
	/// time the colour changes
	bool logHandLight {false};
	/// Which of the sun glare's samples the land hides from the camera is logged each time that changes
	bool logSunGlare {false};
	/// The things set out before it runs, each in a line or two: dispensers, storms, the creature, villages, fires, trees,
	/// piles, script highlights, flowers, a seed in the hand (Debug/TestbedFixtures.h)
	testbed_fixtures::Fixtures fixtures;
	/// Towns with no members, made after the scenario's own town
	std::vector<BareTown> bareTowns;
	/// Flocks of birds, made after the scenario's objects
	std::vector<BirdFlockSetup> birdFlocks;
	/// The testbed's own rule, not the game's: its clock runs on while the game is paused, for a scenario of what only
	/// the paused game shows (the temple's rooms and their dialogs); otherwise it stands still with the game
	bool clockWhilePaused {false};
	/// The testbed's own rule, not the game's: once the scenario's creatures are all made, the first creature of each
	/// owner who leads creatures is the one they lead, so a scenario's leash stays on the creature it was written for.
	/// The game makes the newest creature made for a player the one they lead; turned off, the scenario shows that.
	/// A creature's own leashable and the MakeLeashable command still have the last word
	bool firstCreatureLeads {true};
	/// Written to be played on one of the game's own lands, through a land entry's timeline=, not on the testbed: it sets
	/// nothing out, its points are map coordinates (x east, y the map's z), its creature (creature 0) is the player's
	/// creature, and it carries on through a change of land
	bool onLand {false};
};

/// The miracles' scenarios, added to every scenario by the registry
void AddMiracleScenarios(std::vector<Scenario>& all);
/// The miracles' globes and dispensers close up, and the hand's miracle effects
void AddGlobeScenarios(std::vector<Scenario>& all);
void AddCreatureDanceScenarios(std::vector<Scenario>& all);
/// The heal, the lightning bolt, the creature spells and how the living react to miracles
void AddLifeLightScenarios(std::vector<Scenario>& all);
/// The blasts, fire and water
void AddBlastFireScenarios(std::vector<Scenario>& all);
/// What the blast spares and does on a coast, the water over fields and forests and before the people watching a fire
/// put out, and the hand catching a fireball or taking one into a fire seed
void AddFirewaterScenarios(std::vector<Scenario>& all);
/// Creatures casting miracles
void AddCreatureCastingScenarios(std::vector<Scenario>& all);
/// Creature Mode's and the Creature Cave's scenarios
void AddCreatureModeScenarios(std::vector<Scenario>& all);
/// The gestures drawn with the hand: sizing and powering up miracles, scribbles, and the leash's gestures
void AddGestureScenarios(std::vector<Scenario>& all);
/// The storm miracles: rain over a village, its fields and a fire, and the powered up storm's lightning
void AddStormScenarios(std::vector<Scenario>& all);
/// The flock miracles: doves, bats and wolves swept out by hand
void AddFlockScenarios(std::vector<Scenario>& all);
/// The teleport miracle: stones, villagers jumping between them
void AddTeleportScenarios(std::vector<Scenario>& all);
/// The tornado's scenarios: through a village and a wood, and meeting a creature
void AddTornadoScenarios(std::vector<Scenario>& all);
/// The shield and forest miracles: what each shield stops and what it costs, the forest on each ground, growing and
/// withering
void AddShieldForestScenarios(std::vector<Scenario>& all);
/// Whether a command is the player's alone, given whether or not the scenario has creatures
[[nodiscard]] bool NeedsNoCreature(Command::Kind kind);
/// The player's hand moving over the land, dragging it and turning and zooming the camera
void AddHandNavigationScenarios(std::vector<Scenario>& all);
/// The scenarios of how the hand looks for its player's alignment
void AddHandLookScenarios(std::vector<Scenario>& all);
/// The examples to copy: short scenarios set out with the fixtures, a hand demo and a land change
void AddExampleScenarios(std::vector<Scenario>& all);
/// The physics' scenarios: things lying still, thrown and dropped, rocks split, bodies coming to rest, and how much a
/// frame costs while they do
void AddPhysicsScenarios(std::vector<Scenario>& all);
/// The Norse gate of the creatures' glade and the gate stone plinth: opening, closing and laying the stones
void AddGateScenarios(std::vector<Scenario>& all);
void AddFishScenarios(std::vector<Scenario>& all);
/// The vortices between the lands: opening, levelling the ground and closing
void AddVortexScenarios(std::vector<Scenario>& all);
/// The advisor spirits flying out and home, their rainbow trails, and the dialogue under a film
void AddAdvisorScenarios(std::vector<Scenario>& all);
/// The birds' flocks across the land
void AddBirdScenarios(std::vector<Scenario>& all);
/// The grazers' walks round what stands in their way, and what they do when a thing of many cells leaves the map
void AddAnimalWalkScenarios(std::vector<Scenario>& all);
/// A town centre's totem, which the hand takes hold of, slides up and lets go
void AddVillageTotemScenarios(std::vector<Scenario>& all);
void AddPuzzleTotemScenarios(std::vector<Scenario>& all);
/// The sounds of the land: a village with its houses, its people and its animals, by day, at night and at dawn
void AddWorldSoundScenarios(std::vector<Scenario>& all);
/// The sky's scenarios: the sun's glare dimmed by the land and by what stands in front of it, and a row of trees by
/// day and at night
void AddSkyScenarios(std::vector<Scenario>& all);
/// The hand knocking on a house at night, its people asleep inside
void AddKnockScenarios(std::vector<Scenario>& all);
/// The player's statistics counting as their town lives
void AddStatisticsScenarios(std::vector<Scenario>& all);
/// The land script's flower patches, both kinds on a bank by day
void AddFlowersScenarios(std::vector<Scenario>& all);
/// Creatures going to the places of their map of the land: a hill and its ridge, and the beach
void AddCreatureExplorationScenarios(std::vector<Scenario>& all);
/// Creatures helping a town or playing with its people: food from a field to a storage pit, a cow into a storage pit, a
/// villager kissed in play
void AddCreatureTownHelpScenarios(std::vector<Scenario>& all);
/// Two creatures doing things together: side by side with a friend, and with a friend at a town
void AddFriendActionScenarios(std::vector<Scenario>& all);
/// A tutorial hand demo's recorded cursor on the picture between the cinema bars
void AddHandDemoScenarios(std::vector<Scenario>& all);
/// Footballs made as the script makes them, each given to the town nearest it
void AddBallScenarios(std::vector<Scenario>& all);
/// A town's belief in the player: the believers it gained float up from the land under the cursor
void AddBeliefScenarios(std::vector<Scenario>& all);
/// The hand reaching past the player's border, keeping their influence for a while
void AddVirtualInfluenceScenarios(std::vector<Scenario>& all);
/// The hand opening as it lets go of a miracle's seed it casts or a dispenser takes
void AddSeedThrowScenarios(std::vector<Scenario>& all);
/// A temple destroyed: deleted by a script with its explosion; and boulders thrown at it, the blows its heart takes or
/// passes on
void AddTempleScenarios(std::vector<Scenario>& all);
/// The sharks in the lake: one swimming, one walking a track of the game's cameras
void AddSharkScenarios(std::vector<Scenario>& all);
/// The story's chapters of Land 1, played on the land itself: the opening to the hand-over, and Choose Your Creature's
/// tiger stone
void AddStoryScenarios(std::vector<Scenario>& all);

/// Every scenario, in the order the window lists them
[[nodiscard]] std::span<const Scenario> All();
[[nodiscard]] const Scenario* Find(std::string_view id);
/// What is wrong with a scenario's data, if anything, one line each
[[nodiscard]] std::vector<std::string> Problems(const Scenario& scenario);
/// Whether the testbed's grid of dispensers stays for a scenario: unless it asks for none, or one of its creatures or
/// things would stand on it
[[nodiscard]] bool KeepsDispenserGrid(const Scenario& scenario);

/// A point given from the middle of the map, on the land
[[nodiscard]] glm::vec2 MapPoint(glm::vec2 middle, glm::vec2 offset);

/// About how tall a creature of a size stands
[[nodiscard]] float CreatureHeight(float size);

/// The hand's light as the game works it out from the land's colour of a frame (0xRRGGBB, the alpha left out): the
/// sum of the colour's three bytes and its whole mean, whether the land is dark enough for the lights, and the light's
/// strength, 0 to 1, through the game's own functions
struct HandLightReading
{
	uint32_t landColour {0};
	int32_t sum {0};
	int32_t mean {0};
	bool dark {false};
	float strength {0.0f};
};
[[nodiscard]] HandLightReading ReadHandLight(uint32_t landColour);
/// One line of the log for it at an hour
[[nodiscard]] std::string Describe(const HandLightReading& reading, float hour);

/// The sun's glare as the game looks for what hides it from a camera at an hour: how high the sun stands, and which of
/// its samples, by their offsets from the sun, the land hides, by the given test of the line from the camera to each,
/// through the game's own functions. None while the sun is down
struct SunGlareReading
{
	float sunHeight {0.0f};
	size_t samples {0};
	std::vector<glm::vec2> hiddenByLand;
};
[[nodiscard]] std::optional<SunGlareReading> ReadSunGlare(float hour, glm::vec3 camera, float nearClip,
                                                          const std::function<bool(const glm::vec3&)>& landInTheWay);
/// One line of the log for it
[[nodiscard]] std::string Describe(const SunGlareReading& reading);

/// Where the camera is and what it looks at
struct CameraPlacement
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// The camera above and to the south of a box on the land, by its middle and half its size east to west and south to
/// north, looking down at its middle from far enough back for all of it to be seen through the fields of view, across
/// and up and down, in radians
[[nodiscard]] CameraPlacement Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor);
/// The camera behind and above a creature, by its height, looking at its middle from the south
[[nodiscard]] CameraPlacement Follow(glm::vec3 position, float height, float distanceFactor);
/// The camera at an eye looking at a point, each given as x and z from the origin and y above the ground there
[[nodiscard]] CameraPlacement Placed(glm::vec3 eye, glm::vec3 look, glm::vec2 origin,
                                     const std::function<float(glm::vec2)>& groundAt);
/// The camera in front of a creature's face, by its height, its position and the way it faces on the land
[[nodiscard]] CameraPlacement Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor);
/// The box round points on the land, at least so big: its middle and half its size
struct Bounds
{
	glm::vec2 centre;
	glm::vec2 halfSize;
};
[[nodiscard]] Bounds BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize);

/// Sets a body's needs to the overrides; the energy is at most the creature's size, as a meal can fill it
void Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size);
/// Sets desires to their fractions of their maxima, activating them
void Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires);

/// Where the timeline of a scenario's commands is
struct Timeline
{
	/// The next command, and the seconds counted towards its delay: since the last was given, or since its creature
	/// was free when it waits for that
	size_t next {0};
	float seconds {0.0f};
	/// Seconds since the last command was given
	float sinceGiven {0.0f};
	/// The next command's creature has been free since the last command, so its delay counts on whatever the
	/// creature's idle mind has it do meanwhile
	bool freed {false};
	/// Every command has been given and none go round again
	bool done {false};
};
/// The timeline some seconds on: the commands due now, in order. A command waiting for its creature (or the hand) to
/// be free asks isFree; its delay only counts from when it first is.
[[nodiscard]] std::vector<size_t> Advance(Timeline& timeline, std::span<const Command> commands,
                                          std::optional<size_t> repeatFrom, float seconds,
                                          const std::function<bool(const Command& command)>& isFree);

} // namespace openblack::testbed_scenarios
