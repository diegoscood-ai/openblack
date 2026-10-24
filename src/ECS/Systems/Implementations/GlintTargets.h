/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/Systems/GlintTargetsInterface.h"
#include "Particles/GlintMaths.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{
/// The glints' targets in the game's registry: a script highlight's points are those of its mesh as it is drawn this
/// frame, a spell seed graphic's those of its mesh through the matrix it keeps for its glints
class GlintTargets final: public GlintTargetsInterface
{
public:
	/// The parts of a mesh, by its id; none when the mesh is not loaded
	using ModelParts = std::function<std::vector<particles::maths::GlintModelPart>(entt::id_type mesh)>;

	/// The mesh a target's points come from, and the matrix they are put through
	struct Source
	{
		entt::id_type mesh;
		glm::mat4 model;
	};
	/// A script highlight with a mesh: the mesh and the matrix it is drawn with; a spell seed graphic with a mesh: the
	/// mesh and its glint matrix; none for anything else
	[[nodiscard]] static std::optional<Source> SourceOf(const Registry& registry, entt::entity object);

	/// `parts`: where the meshes' parts are read; the resource cache's meshes when none is given
	explicit GlintTargets(ModelParts parts = {});

	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override;
	[[nodiscard]] uint32_t TargetPointCount(entt::entity object) const override;
	[[nodiscard]] std::optional<glm::vec3> TargetPoint(entt::entity object, uint32_t index) const override;
	[[nodiscard]] float TargetScale(entt::entity object) const override;

private:
	ModelParts _parts;
};
} // namespace openblack::ecs::systems
