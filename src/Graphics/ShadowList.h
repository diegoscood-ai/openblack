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

#include <list>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ShadowMath.h"

/// The original's list of projected shadows (ShadowInfo, 0x4AC bytes, head [0xFAA7E0]; wiki: rendering.md, "Sombras
/// proyectadas"): one entry per caster, the newest first (fn_0087FD50 0x87FEC4..0x87FEF2), each with its own 32 x 32
/// texture (si+0x45C) that the CPU rasterizes every frame (shadow_math) and the renderer draws over the land blocks it
/// touches (fn_007FF610 -> fn_00878350, RendererShadows.cpp) and, for the ones with `onObjects`, over the objects
/// (fn_0080B050).
///
/// The casters (the producers in Frame): the hand (CHand::CHand 0x46BC0B -> CreateDynamicShadow 0x80C020, the complex
/// update fn_00814FD0), the flying physics objects (fn_007FCE80 0x7FCEC7, generic update fn_00874850, removed with the
/// physics, PhysOb::DeInitialise 0x7FB772) and the objects with a components::DynamicShadow (the launched boat,
/// PetitNavire::PetitNavire 0x5E11AE). Not here yet: the creature (LH3DCreature 0x47F543), the prediction object
/// (fn_00646FE0 0x647245), the SuperVillagers (fn_00825F20) and the PSys mesh atoms (fn_006CA340, owner: PSys).
namespace openblack::graphics::shadow_list
{

/// The hand's shadow as the original draws it (S5, checked against the user's captures of the original game,
/// 2026-10-02: the hand's silhouette with its fingers, light and see-through, a held orb darker and round under it):
/// CreateDynamicShadow sets si+0x3C = 1 (0x80C037), so fn_00880050 skips the even subrows (0x880141..0x880146: 4/15 at
/// most), 32 x 32 (fn_0087FD50), projected onto the hand's own y (si+0x18 = obj+0x3C, 0x8152B1..0x8152B4), and the
/// held object si+0x00 (= obj+0x8C: 0x80C044..0x80C04A, SetHeldObject fn_00816830 0x816855) is rasterized into the
/// same texture at full density (its own base y 0x807163, si+0x3C saved, cleared and restored around it, 0x807532 /
/// 0x80753D / 0x8075B7). false gives back openblack's look from before the list (64 x 64 at full density, projected
/// onto the ground under the hand, nothing of the held object), kept only to compare.
inline constexpr bool k_HandShadowAsOriginal = true;

/// fn_00874850 (holder, the generic casters) or fn_00814FD0 (LH3DComplexObject: the hand, the creature)
enum class Update : uint8_t
{
	Generic,
	Complex,
};

/// Where the light of si+0x444 comes from
enum class LightKind : uint8_t
{
	Vertical, ///< the caster + (0, 15000, 0) (0x87490A)
	Sun,      ///< holder+4: the fixed sun [0xEA1C88] (0x8748E7)
	Hand,     ///< obj+0xBC: the caster + (0, 200, 0) (0x8151C4)
	Creature, ///< the current light [0xEA9E90] brought within 3 radii (0x815058)
};

struct ShadowInfo
{
	entt::entity caster {entt::null}; ///< its owner: the hand, the physics object, the boat's hull
	entt::entity held {entt::null};   ///< si+0x00, the held object rasterized with the caster (R3, R4)
	Update update {Update::Generic};
	LightKind light {LightKind::Vertical};
	/// si+0x464 != 0: the land's t' takes H = GetAltitude(caster) (fn_00878350 0x878394); fn_00874850 writes it
	/// (0x874993), fn_00814FD0 never does, so the hand has H = si+0x18 and t' = 1
	bool emitter {true};
	bool active {true};     ///< si+0x08 (fn_00814FD0 clears it when the object is hidden, 0x814FEE)
	bool onObjects {false}; ///< si+0x0C == 0: drawn over the objects too (the hand, the boat)
	bool halfRows {false};  ///< si+0x3C: only the odd subrows (the hand, 0x80C037)
	int texels {shadow_math::k_Texels};
	int alpha {0};       ///< si+0x10: 0 = not drawn (fn_00881030)
	int baseAlpha {255}; ///< si+0x14 (0x87FED0)
	shadow_math::Projection projection; ///< si+0x444 light, si+0x450 caster - light, si+0x18 the caster's y
	shadow_math::Box box;               ///< si+0x1C..0x28 (= +0x2C {x0, z0, x1, z1}) and si+0x440
	float landT {1.0f};                 ///< fn_00878350's t' of this frame (one H per shadow)
	shadow_math::Texels texels16 {};    ///< the alpha nibbles n of the texture, n / 15
	bgfx::TextureHandle texture {BGFX_INVALID_HANDLE}; ///< si+0x45C: R8 of n x 17, CLAMP (material +5 = 0)
	bool seen {false};                                ///< the producers found the caster this frame
};

struct FrameInputs
{
	glm::vec3 camera {0.0f};     ///< g_camera [0xEA1DB8]
	glm::mat4 worldToClip {1.0f}; ///< g_world_to_clipping [0xEA9E40]: the visibility of the blocks (fn_00877210)
	float nearW {1.0f};          ///< [0xE839E0]: the near clip distance
	bool landRef {false};        ///< [0xE9CD8C]: the LandRef detail key (the blocks' corners and centre)
};

class List
{
public:
	List();
	~List();
	List(const List&) = delete;
	List& operator=(const List&) = delete;

	/// fn_0087FD50: a new entry in front of the others, its texture cleared (0x87FDBE..0x87FDFC)
	ShadowInfo& Add(entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows, int texels);
	/// fn_0087FF10: out of the list, its texture released (0x87FF40)
	void Remove(entt::entity caster);
	void Clear();
	/// One frame: the producers, then every entry's update (fade, alpha, light), its silhouette (projection, raster,
	/// resolve, chroma blur, baked fade) and its upload
	void Frame(const FrameInputs& inputs);

	/// fn_00881030 (si+0x08 && si+0x10) over the list, newest first
	template <typename F>
	void ForEachActive(F&& function) const
	{
		for (const auto& shadow : _shadows)
		{
			if (shadow.active && shadow.alpha != 0 && bgfx::isValid(shadow.texture))
			{
				function(shadow);
			}
		}
	}
	[[nodiscard]] const std::list<ShadowInfo>& GetShadows() const { return _shadows; }

private:
	std::list<ShadowInfo> _shadows;
	int _frame {0};
};

} // namespace openblack::graphics::shadow_list
