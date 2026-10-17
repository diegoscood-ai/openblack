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
	/// Anything it can knock down, or only trees
	Destroyable,
	Tree,
	/// Villagers, villagers hurt enough to be healed, other creatures, or either
	Villager,
	HurtVillager,
	Creature,
	Living,
	/// Anything that frightens a creature: another creature, a spell, or a frightening animal
	Frightening,
	/// Anything at all it can see
	Anything,
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
};
/// How many kinds of thing an action may be done to
inline constexpr size_t k_TargetCount = static_cast<size_t>(Target::DryField) + 1;

/// How the agenda is made
enum class Build : uint8_t
{
	Eat,
	/// Going up to a pile and eating a portion made from it
	EatPortion,
	/// Going up to a storage pit's food pile, a field or a tree, and eating a portion of wheat, or of fruit, made from it
	EatFromStoragePit,
	EatFromField,
	EatFromTree,
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
	SitDown,
	BeIdle,
	HangAround,
	ShowDesire,
	/// An action on the spot, or facing the player, or at what it goes up to
	Emote,
	FaceCameraEmote,
	/// Looking at the camera, and pointing at it
	LookAtCamera,
	PointAtCamera,
	/// Looking at the camera while a script's cut scene is on, as every creature free to think for itself is made to
	LookAtCameraInCutScene,
	ApproachEmote,
	RunFromObject,
	RunFromPlayer,
	LookAbout,
	/// What scripts force on creatures to stage a scene: looking at a thing for a day, or a while without going up to
	/// it
	LookForever,
	LookButDontApproach,
	/// Casting a miracle at what it is done to: a lightning bolt, a helpful miracle, a spell on another creature, food,
	/// water; water on the thing it is done with, and water on itself
	CastLightning,
	CastHelpful,
	CastPlayful,
	CastFood,
	CastWater,
	CastWaterOnInstrument,
	CastWaterOnItself,
	/// Catching what flies at it, only ever forced on it
	Catch,
	/// The actions that use the creature's map of the land: going to a hill to look, going to a hill, going to a hill
	/// and walking along its ridge, exploring a coast or a town, sitting on a beach, looking at the mountains or out to
	/// sea, and throwing a thing into the sea
	GoToHillAndLook,
	GoToHillAndSit,
	WalkAlongRidge,
	ExploreCoast,
	ExploreTowns,
	SitDownOnBeach,
	LookAtMountains,
	LookOutToSea,
	ThrowInTheSea,
};

struct Executor
{
	/// The action's name in the game's table
	std::string_view action;
	Target target {Target::None};
	Build build {Build::Emote};
	/// The animation played, for the builds that play one
	size_t animation {0};
	creature_mind::Activity activity {creature_mind::Activity::Planned};
};

/// Every action that can be carried out
[[nodiscard]] std::span<const Executor> All();
[[nodiscard]] const Executor* For(std::string_view action);
/// The same, with the actions that are only ever forced on a creature and that its planner doesn't weigh: catching
[[nodiscard]] const Executor* ForcedFor(std::string_view action);

/// What the agenda needs to know of where the creature is
struct Situation
{
	/// Where the player looks from, the nearest water's edge and the water, somewhere to hurl things at, and the action
	/// that shows its strongest desire. The camera is where it stands on the ground; the eye is the camera itself, and
	/// means nothing without the camera
	std::optional<glm::vec2> camera;
	glm::vec3 eye {0.0f};
	std::optional<creature_mind::Wants::WaterSpot> water;
	std::optional<glm::vec2> hurlTarget;
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
	/// The food pile of the storage pit the action is done to, by its entity's number, and where it is
	std::optional<uint32_t> pitPile;
	glm::vec2 pitPilePoint {0.0f};
	/// The creature's height
	float height {0.0f};
	/// Whether its hand holds something it can eat
	bool holdingEatable {false};
	/// Whether it has a player: only then does it turn to the camera before pointing at it
	bool hasPlayer {false};
	/// For the actions that use the creature's map of the land: the place that the creature goes to or looks at, and
	/// the sea that it looks at or throws into. None when the map gives no such place.
	std::optional<glm::vec2> explorePlace;
	std::optional<glm::vec2> exploreSea;
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
};
/// Whether an action casts a miracle
[[nodiscard]] bool IsCast(const Executor& executor);
/// Whether the game's own test of an action's row lets the creature try it: any action that casts nothing; a cast only
/// when it may cast the miracle, and water on itself only while it is on fire as well
[[nodiscard]] bool MayTry(const Executor& executor, bool mayCast, bool onFire);

/// Whether an action can be carried out now, in this situation, before any thing is chosen
[[nodiscard]] bool Possible(const Executor& executor, const Situation& situation);
/// The agenda for an action on a thing (by its entity number, and where it is), if it can be done
[[nodiscard]] std::optional<std::vector<creature_mind::Step>> Agenda(const Executor& executor, std::optional<uint32_t> object,
                                                                     glm::vec2 objectPoint, const Situation& situation,
                                                                     const creature_mind::Random& random,
                                                                     const std::optional<CastInfo>& cast = std::nullopt);

} // namespace openblack::creature_plan_actions
