/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{
namespace resources
{
class ResourcesInterface;
}
namespace ecs
{
class Registry;
namespace systems
{
class DynamicsSystemInterface;
}
} // namespace ecs
} // namespace openblack

/// What breaking buildings needs of the rest of the game: the entities, the caches of the models their broken shapes are
/// drawn with, and the physics their pieces fly in. The game's own world works through the game's services; tests give
/// a fake.
namespace openblack::ecs::building_world
{

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	[[nodiscard]] virtual const Registry& Entities() const = 0;
	/// The caches of the models a broken building and its pieces are drawn with
	[[nodiscard]] virtual resources::ResourcesInterface& Resources() = 0;
	/// The physics the pieces fly in
	[[nodiscard]] virtual systems::DynamicsSystemInterface& Dynamics() = 0;
};

} // namespace openblack::ecs::building_world
