/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack
{
class Camera;
}

namespace openblack::graphics
{
class FrameBuffer;
class ShaderManager;
class ShaderProgram;

/// Dynamic shadows of the physics objects on the land (fn_007FCE80 → fn_008745A0 / fn_00874850 / fn_00878350).
///
/// Every flying object that casts a static shadow, or is animated, gets a ShadowInfo: a 32 x 32 silhouette seen from
/// a light 15000 above it (straight down), over the tight box of its projected vertices, with 4 x 2 subsamples per
/// texel (alpha = covered / 15, at most 8/15, the outer ring left empty); faded between 50 and 80 radii from the
/// camera; draped vertically over the land blocks it touches (not over sea-level cells, never over objects).
///
/// Here the silhouettes go into slots of an atlas at 4 x 2 the resolution, a resolve pass counts the subsamples into
/// the 32 x 32 texels, and fs_terrain darkens the land with every box that covers the fragment.
class PhysicsShadows
{
public:
	static constexpr uint32_t k_SlotsPerRow = 4;
	static constexpr uint32_t k_MaxShadows = k_SlotsPerRow * k_SlotsPerRow; ///< the original has no limit
	static constexpr uint16_t k_Texels = 32;

	struct Shadow
	{
		entt::entity entity;
		entt::id_type meshId;
		uint32_t instance;
		glm::vec4 box;   ///< x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)
		glm::vec4 light; ///< xyz: light position, w: the plane y0 (the object's height)
		float fade;      ///< si+0x10 / 255
	};

	PhysicsShadows();
	~PhysicsShadows();

	/// Picks the casters and their boxes for this frame
	void Update(const Camera& camera);
	/// Silhouettes into the high resolution atlas, then the subsample count into the 32 x 32 texels
	void Draw(const ShaderManager& shaders);
	/// fs_terrain's uniforms and texture (none in the reflection)
	void BindTerrain(const ShaderProgram& program, bool enabled) const;

	[[nodiscard]] const std::vector<Shadow>& GetShadows() const { return _shadows; }

private:
	std::vector<Shadow> _shadows;
	std::unique_ptr<FrameBuffer> _silhouettes; ///< 4 x 2 subsamples per texel
	std::unique_ptr<FrameBuffer> _resolved;    ///< alpha n / 15
	std::array<glm::vec4, k_MaxShadows> _boxes {};
	std::array<glm::vec4, k_MaxShadows> _slots {};
};
} // namespace openblack::graphics
