/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureMindFileBody.h"
#include "Enums.h"

namespace openblack::creature
{
struct CreatureMind;
}
namespace openblack::creaturemind
{
struct MindFileData;
}

namespace openblack::ecs
{
class Registry;
}
namespace openblack::script
{
enum class ObjectPropertyType : uint8_t;
}
namespace openblack::ecs::systems
{
class CreatureMindSystemInterface;
class CreaturePhysiologySystemInterface;
class LeashSystemInterface;
} // namespace openblack::ecs::systems
namespace openblack::state_hash
{
class Hasher;
}
namespace openblack::ecs::components
{
struct Transform;
}

/// The player's own creature: the one the profile brings to a land (LOAD_MY_CREATURE) and the script natives that
/// act on it. Its data lives on the creature's components; nothing here keeps state
namespace openblack::ecs::player_creature
{

/// The profile's mind file in the given folder; none when the profile names no file
[[nodiscard]] std::optional<std::filesystem::path> ProfileMindPath(std::string_view file,
                                                                   const std::filesystem::path& mindFolder);

/// CURRENT_PROFILE_HAS_CREATURE: whether the profile's mind file exists in Scripts/CreatureMind. The profile's file is
/// the creature-file setting (EngineConfig::profileCreatureFile)
[[nodiscard]] bool ProfileHasCreature();

/// What LOAD_MY_CREATURE (or LOAD_CREATURE) makes of a mind file: the species and body, and where on the ground the
/// creature is made (y is the height above it, 0). What the file leaves out stays as the species starts it
struct LoadPlan
{
	CreatureType species;
	std::optional<float> size;
	std::optional<float> alignment;
	float strength;
	glm::vec3 position;
	/// The fatness, and the fatness its body showed when it was saved
	float fatness {0.5f};
	float previousFatness {0.5f};
	creature_mind_body::SavedNeeds needs;
};

/// LOAD_MY_CREATURE's plan: the species and body the file describes, at the middle of the map cell of the point it is
/// given. None when the player has a creature already, or the file is missing, unreadable or of no species
[[nodiscard]] std::optional<LoadPlan> PlanLoad(bool playerHasCreature, const creature::CreatureMind* mind, glm::vec2 pointXZ);

/// LOAD_MY_CREATURE: the local player's creature, read from the profile's mind file and made at the point's map cell;
/// nothing when PlanLoad gives none
entt::entity LoadMyCreature(glm::vec2 pointXZ);

/// What the mind file's reader puts back on the creature just made from a plan: the fatness, and the fatness its body
/// shows (creature_mind_body::LoadedShownFatness), which its body is drawn with; and through its body's physiology,
/// before the body's first turn, the turns lived, age, energy, itchiness, amount of poo, exhaustion and dehydration
void RestoreBody(Registry& registry, systems::CreaturePhysiologySystemInterface* physiology, entt::entity creature,
                 const LoadPlan& plan);

/// A live creature as a mind file (creature_mind_body::ToMindFile over what its mind system saves, with its body as it
/// is now: the needs through its body's physiology and the leash flags through the leash rules, each kept as the file
/// had them when that service is not given); none when it is not a creature or its mind is not set up yet
[[nodiscard]] std::optional<creaturemind::MindFileData> ToMindFile(const Registry& registry,
                                                                   const systems::CreatureMindSystemInterface& minds,
                                                                   const systems::CreaturePhysiologySystemInterface* physiology,
                                                                   const systems::LeashSystemInterface* leash,
                                                                   entt::entity creature);

/// The map clear of a script's land change: the local player's creature is saved to the profile's mind file, and its
/// physique beside it, through the file system, and the mind cache forgets the file, so that the next land's
/// LOAD_MY_CREATURE makes the creature again from what was saved. Nothing without a creature, a mind to save or a
/// profile file. True when the mind file was written
bool SaveMyCreature();
/// What LOAD_CREATURE makes of a mind file: the species the script names (the file's own species is not read), the body
/// the file describes, and the point itself on the ground, x and z through the fixed point (not its cell's middle). None
/// when the file is missing or unreadable, or the species is not one of the game's
[[nodiscard]] std::optional<LoadPlan> PlanScriptLoad(int32_t type, const creature::CreatureMind* mind, glm::vec2 pointXZ);

/// LOAD_CREATURE: a player's creature of the script's species, read from the named mind file in Scripts/CreatureMind
/// and made at the point. It becomes the creature its player leads, even when they led another (an error is logged
/// first, and that one stops being it), knows the three leashes and is fully grown. Nothing when PlanScriptLoad gives
/// none
entt::entity ScriptLoadCreature(int32_t type, std::string_view file, PlayerNames player, glm::vec2 pointXZ);
/// What LOAD_CREATURE does to the creature it has made: the one its player leads, the rope, evil and good leashes known,
/// and the last stage of growing up
void SettleScriptLoaded(systems::LeashSystemInterface& leash, Registry& registry, entt::entity creature);

/// The "creature" part of the state hash: each creature's owner, species, body, growing-up stage and home
void RegisterStateHash();
/// What that part hashes, in the creatures' storage order; nothing without a creature
void HashCreatures(state_hash::Hasher& h, const Registry& registry);

/// The highest stage of growing up, a fully mature creature
inline constexpr int32_t k_LastDevelopmentStage = 13;

/// CALL_PLAYER_CREATURE: the creature the player leads, if they have one
[[nodiscard]] std::optional<entt::entity> PlayersCreature(const systems::LeashSystemInterface& leash, PlayerNames player);

/// SET_CREATURE_HOME's point as the original keeps a home: x and z in its fixed point, the height dropped (the home is
/// on the ground)
[[nodiscard]] glm::vec3 HomeOnGround(glm::vec3 point, float groundHeight);

/// SET_CREATURE_HOME: a creature's home, where it is kept while it starts to grow up; anything else is left alone
void SetHome(systems::LeashSystemInterface& leash, const Registry& registry, entt::entity thing, glm::vec3 home);

/// Which of a temple mesh's special points is the place it keeps its player's creature
inline constexpr size_t k_TemplePenPoint = 15;

/// Where a temple keeps its player's creature: its mesh's pen point turned and moved with the temple, or none when the
/// mesh marks no such point
[[nodiscard]] std::optional<glm::vec3> TemplePenPoint(const components::Transform& temple,
                                                      std::span<const glm::mat4> specialPoints);

/// Each turn, while its player's temple stands built, a creature's home is that temple's pen point (x and z in the
/// fixed point, on the ground), whatever a script set before. Without a creature nothing is looked up
void FollowTemplePens(systems::LeashSystemInterface& leash, const Registry& registry,
                      const std::function<std::optional<glm::vec3>(PlayerNames)>& penOf,
                      const std::function<float(glm::vec2)>& groundAt);

/// FollowTemplePens with the game's temples, meshes and land
void FollowTemplePens();

/// A creature in its temple's pen is drawn smaller, down to a newborn's size, the nearer it is to the pen's place; its
/// own size is kept
inline constexpr float k_PenDrawnSize = 0.22f;
/// The ramp: the own size at this distance from the pen's place and farther, the pen's size at the inner one and nearer
inline constexpr float k_PenOuterRadius = 16.0f;
inline constexpr float k_PenInnerRadius = 14.0f;
/// The pen's walls, from the temple's turn: the first wall's angle after the temple's, and the angle between the walls
inline constexpr float k_PenWallAngle = 3.83f;
inline constexpr float k_PenWallsApart = 0.897598f;

/// Whether a point (x, z) is between the pen's two walls of a temple at `temple` turned by `templeYAngle`
[[nodiscard]] bool BetweenPenWalls(glm::vec2 temple, float templeYAngle, glm::vec2 point);

/// The size a creature is drawn at: its own, or within the pen's outer radius and between its walls, the ramp down
/// to the pen's size
[[nodiscard]] float PenDrawnSize(float size, float distanceToPen, bool betweenWalls);

/// The size a creature is drawn at in its player's built temple's pen, from its distance to its home and the pen's
/// walls; none when that is its own size (outside a pen, or where the ramp gives its own size)
[[nodiscard]] std::optional<float> PenSizeOf(const Registry& registry, entt::entity creature);

/// Each turn, the size every creature is drawn at, from its distance to its home and its player's built temple's pen
/// walls (components::CreatureDrawPose::size, and the scale it is drawn at, ::scale); none outside a pen
void ShrinkInPens();

/// CREATURE_AUTOSCALE: whether a creature follows the local player's creature's size each turn, and the factor it
/// follows it by (Autoscale). False for anything but a creature (nothing is changed)
bool SetAutoscale(Registry& registry, entt::entity thing, bool enabled, float factor);

/// Each turn, right after the creatures' spells step, every creature set to follow (SetAutoscale) moves halfway from its
/// size now towards the local player's creature's size before any size spell, times its factor
/// (creature_size::AutoscaleStep). While big or small is on it, the new size is the one that spell puts back as it
/// ends, its size now left to the spell. Nothing without the local player's creature, and nothing for `held`: the
/// creature the hand holds, standing in for a creature whose turn stops short of this step (approximate)
void Autoscale(Registry& registry, std::optional<entt::entity> localCreature, std::optional<entt::entity> held);

/// Autoscale with the game's local player's creature, and the creature the hand holds as the one skipped (approximate)
void Autoscale();

/// A creature's friend list gains `other` at its front, unless it is in it already. True when it was added
bool AddFriend(std::vector<entt::entity>& friends, entt::entity other);

/// CREATURE_FORCE_FRIENDS: with `enable`, each creature is added once to the other's friends
/// (components::CreatureFriends), `second` to `first`'s first; without it, nothing (no friend is taken away). False,
/// and nothing changed, unless both are creatures
bool ForceFriends(Registry& registry, entt::entity first, entt::entity second, bool enable);

/// Whether GET_PROPERTY and SET_PROPERTY read and write this property on a creature (GetCreatureProperty): its strength,
/// alignment, warmth, fatness, energy, itchiness, amount of poo, exhaustion, dehydration, fight health, and its smallest
/// and largest size
[[nodiscard]] bool IsCreatureProperty(script::ObjectPropertyType property);

/// SET_PROPERTY's limits on a creature's warmth and energy, in the original's order: below the lowest (or not a number)
/// gives the lowest, above the highest the highest
[[nodiscard]] float ClampProperty(float value, float lowest, float highest);

/// GET_PROPERTY of one of the creature properties: the strength and alignment as the creature has them, the needs as its
/// body has them (physiology, none without it), the fatness, the fight health, its fighter's in a fight, and the sizes
/// its small and big spells take it to (components::CreatureSizeLimits, else the defaults). None for anything but a
/// creature, or another property
[[nodiscard]] std::optional<float> GetCreatureProperty(const Registry& registry,
                                                       const systems::CreaturePhysiologySystemInterface* physiology,
                                                       entt::entity thing, script::ObjectPropertyType property);

/// SET_PROPERTY of one of the creature properties, the value kept as given but the warmth (-1 to 1) and the energy
/// (0 to 1); a need only through the body (physiology); the smallest or largest size in its own
/// components::CreatureSizeLimits. The body shows a new strength or alignment from its next
/// drawing. False for anything but a creature, another property, or a need without a body (nothing is changed)
bool SetCreatureProperty(Registry& registry, systems::CreaturePhysiologySystemInterface* physiology, entt::entity thing,
                         script::ObjectPropertyType property, float value);

/// SET_PROPERTY HEIGHT's size for a height: the height times this (one fifteenth as a float, multiplied, not divided)
constexpr float k_SizePerHeight = 1.0f / 15.0f;
[[nodiscard]] float SizeForHeight(float height);

/// SET_PROPERTY SCALE on a creature: its body drawn at that size kept within the 3D body's limits
/// (creature_morph::ClampScale), and its own size (Creature::size) as given. False, and nothing changed, for anything
/// but a creature
bool SetCreatureScale(Registry& registry, entt::entity thing, float scale);

/// SET_PROPERTY HEIGHT on a creature: its own size (Creature::size) the height's (SizeForHeight), within no limit. The
/// original also gives its 3D body that size, unclamped, which waits for the drawn size to be kept apart; only the drawn
/// matrix (its Transform) waits for the body's next resize, so it is left as it was. False, and nothing changed, for
/// anything but a creature
bool SetCreatureHeight(Registry& registry, entt::entity thing, float height);

/// SET_CREATURE_DEV_STAGE: a creature's stage of growing up, 0 to 13; its desires follow in the mind's next turn.
/// Anything else, or a stage out of range, is left alone
void SetDevelopmentStage(Registry& registry, entt::entity thing, int32_t stage);

/// SET_CREATURE_NAME: a creature's name. A mind that has not yet taken up its file keeps the name for when it does, so
/// that the name stays over the file's, as in the original, which reads the file as it makes the creature. False for
/// anything but a creature (nothing is changed)
bool SetName(Registry& registry, entt::entity thing, std::u16string name);

/// DEV_FUNCTION, the parts that act on the local player's creature: 2 lets it learn the rope leash, 3 the good and
/// evil leashes and makes it the one its player leads. False for a function that is not ported (nothing is done)
bool DevFunction(systems::LeashSystemInterface& leash, int32_t function, PlayerNames player);

/// CREATURE_IN_DEV_SCRIPT: whether a creature is in its growing-up scripts; anything else is left alone
void SetInDevScript(Registry& registry, entt::entity thing, bool inDevScript);

} // namespace openblack::ecs::player_creature
