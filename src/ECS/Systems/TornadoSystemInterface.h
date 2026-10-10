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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{

/// What a tornado does to the things of the world: it picks things up and carries them round its funnel, each with a
/// particle of its own, and lets them go when the particle goes: the living die where they land, anything else is gone.
///
/// The tornado's particles find what its foot reaches, split the piles and let go of what they carried; this service
/// is what they and the game loop ask of the world.
class TornadoSystemInterface
{
public:
	virtual ~TornadoSystemInterface() = default;

	/// Starts carrying an object with a particle: whether the particle carries it. Something no longer there or
	/// already in flight is not carried; something another particle already carries is carried by this one too, and
	/// is not taken again.
	virtual bool Carry(entt::entity object) = 0;

	/// Every frame: what is carried is put where its particle is drawn
	virtual void Update() = 0;
	/// How many things the tornadoes carry, as their list counts them: it is not emptied when a new land opens
	[[nodiscard]] virtual size_t CarriedCount() const = 0;
};

} // namespace openblack::ecs::systems
