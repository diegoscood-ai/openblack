/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The shield miracles' objects in the world. Each running shield miracle raises one where it was cast: the spiritual
/// shield's marks where it is (its sphere is its particle effect's), the physical shield's is a solid dome that grows,
/// spins and bobs, and stops what is thrown at it. While a shield stands no other player has influence under it, the
/// people about it shelter under it, and other players' creatures walk round it. Once its miracle lets go the spiritual
/// shield's goes at once, the dome fades away.
///
/// The shields themselves (the objects, the miracles and the villagers' reactions to them) live under src/Magic and
/// keep their lists in the magic object and spell services; this service is the way in for the turn, the frame, a new
/// land and the reactions.
class MagicShieldSystemInterface
{
public:
	virtual ~MagicShieldSystemInterface() = default;

	/// Once a game turn, at the start of the miracles' turn, before any miracle's upkeep: the domes grow, spin, bob and
	/// fade. A spiritual shield's object does nothing.
	virtual void ProcessTurn() = 0;
	/// Once a frame, after the worship sites: each dome as it is drawn, between its last two turns by the turn's
	/// fraction, and how see-through it is. What is thrown at a dome bounces off it in the physics' turn.
	virtual void Update(float seconds) = 0;
	/// A new land: no shields. The shield objects and the shield miracles leave their lists; their entities go with the
	/// registry.
	virtual void Reset() = 0;

	/// Whether a shield keeps a reaction from someone standing under it: they stand within a shield's reach on the
	/// ground and what made the reaction is not inside that shield. Both points are map positions: x and z on the map,
	/// y above the land.
	[[nodiscard]] virtual bool KeepsReactionOff(glm::vec3 watcher, glm::vec3 initiator) const = 0;

	/// A dome as the renderer would draw it this frame: its mesh, where it stands, its turn about the vertical and its
	/// size
	struct DomeDraw
	{
		entt::id_type mesh;
		glm::vec3 position;
		float angle;
		float scale;
	};
	/// The domes for the renderer to queue this frame, between their last two turns by the turn's fraction. None by
	/// default: the domes are map objects of their own (components::MapShield), drawn and queued with the other models
	[[nodiscard]] virtual std::vector<DomeDraw> GetDomes(float /*turnFraction*/) const { return {}; }
};

} // namespace openblack::ecs::systems
