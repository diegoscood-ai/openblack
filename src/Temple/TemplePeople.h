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

#include <memory>
#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"
#include "Temple/TemplePeopleRules.h"

namespace openblack
{

/// One of the player's worshippers the camera sees walking about the temple this frame: its model, where and how it is
/// placed in the world, where its feet are, the clip it plays, and where it is in it at each of the frame's draws of it
/// in the rooms themselves and in their reflections, in the order they draw it
struct TemplePerson
{
	entt::id_type mesh;
	glm::mat4 model;
	glm::vec3 feet;
	AnimId clip;
	std::vector<uint32_t> roomClipPlaces;
	std::vector<uint32_t> reflectionClipPlaces;
};

/// The player's worshippers walking about the temple's floors while the player is inside it (see temple_people)
class TemplePeople
{
public:
	/// The worshippers of the player's worship sites as the player comes in from the land, on the temple's walk mesh.
	/// Nobody comes in without the walk mesh, or without a temple of the player's.
	[[nodiscard]] static std::unique_ptr<TemplePeople> Gather(PlayerNames player);

	/// Walks them on by a frame's whole milliseconds, the last to come in first
	void Walk(uint32_t milliseconds);

	/// As the rooms are drawn: those whose feet the camera sees on a screen of `resolution` pixels, with the temple's
	/// people shown at the detail level, take the speed their clip gives and play it on by the frame's milliseconds at
	/// each of the frame's draws of them (temple_people::DrawsInFrame). The temple is placed by its matrix, and the
	/// camera sees it by its view and projection.
	void See(uint32_t milliseconds, std::span<const temple_people::Pass> draws, const glm::mat4& temple,
	         const glm::mat4& viewProjection, float nearClip, glm::vec2 resolution, bool shown);

	/// Those the camera sees this frame
	[[nodiscard]] const std::vector<TemplePerson>& GetSeen() const { return _seen; }
	/// How many came in, seen or not
	[[nodiscard]] size_t GetCount() const { return _visitors.size(); }

private:
	struct Visitor
	{
		entt::id_type mesh;
		float scale;
		bool man;
		temple_people::Walker walker;
	};

	temple_people::WalkMesh _mesh;
	/// The last to come in first
	std::vector<Visitor> _visitors;
	std::vector<TemplePerson> _seen;
};

} // namespace openblack
