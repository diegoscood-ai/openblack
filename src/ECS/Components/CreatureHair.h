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

#include <bitset>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "Creature/CreatureHair.h"

namespace openblack::ecs::components
{

/// A creature's hair, for the species that have any: each tuft's strands as they hang this frame, and how they are
/// drawn
struct CreatureHair
{
	/// The texture, Data/C_Ape_Hair.raw, and its alpha, Data/C_Ape_Haira.raw, every species' hair is drawn with
	static constexpr entt::id_type k_TextureId = entt::hashed_string("creature/hair");
	static constexpr entt::id_type k_AlphaTextureId = entt::hashed_string("creature/hair_alpha");

	struct Group
	{
		std::vector<creature_hair::Strand> strands;
		/// 0 to 255 a channel
		glm::ivec3 colour {0};
		/// How wide each strand is drawn, either side of its line
		float halfWidth {0.0f};
		/// Drawn with the hair texture rather than in its colour alone
		bool textured {false};
		/// Shown: a hidden tuft is neither moved nor drawn, and its strands wait where they were
		bool shown {true};
	};
	std::vector<Group> groups;
	/// Whether the strands have been started on the body yet; every point starts at 0 (creature_hair::Fresh)
	bool started {false};

	/// A creature has at most this many tufts
	static constexpr size_t k_MaxGroups = 8;
	/// The tufts its animations' moments have hidden, by their place among the file's groups; every tuft starts shown
	std::bitset<k_MaxGroups> hidden;
};

} // namespace openblack::ecs::components
