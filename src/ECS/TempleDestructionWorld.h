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

#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/ModelSurface.h"
#include "Enums.h"
#include "Particles/PlasmaCommand.h"
#include "Physics/TempleHeart.h"

namespace openblack
{
class GameRandomInterface;
}

namespace openblack::ecs
{
class Registry;
}

/// What a temple's heart needs of the rest of the game as it is destroyed: the entities, the turn's length, the local
/// player, its sounds, spot visuals and plasma beams, its model, random numbers, the local player's heartbeat, the
/// game-over script and taking things away. The game's own world works through the game's services; tests give a fake.
namespace openblack::ecs::temple_world
{

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	[[nodiscard]] virtual uint32_t MillisecondsPerTurn() const = 0;
	/// The game's turn
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// The player at this computer, none when there is none
	[[nodiscard]] virtual std::optional<PlayerNames> LocalPlayer() const = 0;
	/// A playground game, where losing a temple doesn't end the game
	[[nodiscard]] virtual bool PlaygroundGame() const = 0;
	/// The heart of a player's temple, none when the player has none
	[[nodiscard]] virtual entt::entity CitadelHeartOf(PlayerNames player) const = 0;
	/// The parts of the heart's temple still there, the newest first: its worship sites, then the heart itself
	[[nodiscard]] virtual std::vector<entt::entity> PartsOf(entt::entity heart) const = 0;
	/// A player's towns as a temple's heart looks over them for something to pass a blow on to
	[[nodiscard]] virtual std::vector<openblack::physics::temple_heart::Town> HeartTowns(PlayerNames owner) const = 0;
	/// Whether a thing is still there, and its life
	[[nodiscard]] virtual bool Available(entt::entity thing) const = 0;
	[[nodiscard]] virtual float LifeOf(entt::entity thing) const = 0;
	/// A thing's point on the map, and its height
	[[nodiscard]] virtual glm::vec3 MapPointOf(entt::entity thing) const = 0;
	[[nodiscard]] virtual float HeightOf(entt::entity thing) const = 0;

	/// A sound tag on a thing that plays one of the game's sounds there in 3D: looping until its tags go, or once
	virtual void TagSound(entt::entity thing, int sample, bool loops) = 0;
	/// Every tag of the sound on the thing goes
	virtual void RemoveSoundTags(entt::entity thing, int sample) = 0;
	/// The next of the heart's five spark sounds at its point, for a beam at what takes a blow or for a hurt beam: each
	/// goes round its own five
	virtual void PlaySpark(entt::entity heart, bool hurt) = 0;

	/// A spot visual at a place for so many turns, owned by nothing; none when it could not be made
	virtual entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns) = 0;
	/// Whether a spot visual is still there
	[[nodiscard]] virtual bool SpotVisualGoing(entt::entity visual) const = 0;
	/// The player a spot visual is shown for, its scale, and a thing its effect acts on
	virtual void SetSpotVisualPlayer(entt::entity visual, PlayerNames player) = 0;
	virtual void SetSpotVisualScale(entt::entity visual, float scale) = 0;
	virtual void AddSpotVisualTarget(entt::entity visual, entt::entity thing) = 0;
	/// The power a spot visual's effect is stepped with, as the glow over a heart follows the local player's heartbeat
	virtual void PowerGlowByHeartbeat(entt::entity visual) = 0;
	/// A plasma beam fired from a spot visual's effect
	virtual void AddPlasma(entt::entity visual, const particles::PlasmaCommand& command) = 0;

	/// The triangles of the drawn parts of a thing's model, in the model's space, and where the model is placed
	[[nodiscard]] virtual std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity object) const = 0;
	[[nodiscard]] virtual glm::mat4 PlacementOf(entt::entity object) const = 0;
	/// The local (unsynchronised) random numbers, none when there are none
	[[nodiscard]] virtual GameRandomInterface* LocalRandom() = 0;
	/// A draw of the synchronised random numbers, from nothing up to the range
	[[nodiscard]] virtual float GameFloatRand(float range) = 0;

	/// The local player's heartbeat is held at a pace, or let go with 0
	virtual void HoldHeartbeat(float pace) = 0;
	/// The heart is drawn with this alpha, and with the shell over it or not
	virtual void FadeHeart(entt::entity heart, uint8_t alpha) = 0;
	virtual void ShowShell(entt::entity heart, bool shown) = 0;
	/// The heart's model is broken up into pieces that fly from its place
	virtual void BreakUpModel(entt::entity heart) = 0;

	/// The thing is to be deleted, as the game deletes any thing
	virtual void ToBeDeleted(entt::entity object) = 0;
	/// A script of that name starts
	virtual void StartScript(std::string_view name) = 0;
};

} // namespace openblack::ecs::temple_world
