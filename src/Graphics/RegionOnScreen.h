/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <cstdint>

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/AxisAlignedBoundingBox.h"

/// openblack's one stand-in for LH3DBoundingBox::CheckRegionOnScreen 0x868C80 (the test LH3DObject::AddDrawing, the
/// PSys atoms 0x679F75, LH3DMist::AddDrawing 0x7FA7CE and the SuperVillagers' fn_00825400 0x82541D make before a draw).
/// Header only. It was three copies of the same function (Renderer.cpp, RendererMists.cpp, RendererSmoke.cpp).
/// SphereInView / BoxInView: (approximate) a sphere against the six planes, what the renderer's culls use.
/// ScreenView / PointOnScreen / SphereOnScreen: the faithful screen tests (fn_0081F1D0 and 0x868C80 itself) the scripts'
/// field of view needs (GAME_THING_FIELD_OF_VIEW 011 / POS_FIELD_OF_VIEW 012, Camera/FieldOfView.h; research
/// dev\documentacion\intro\spec_timers_events.md §3)
namespace openblack::graphics::region_on_screen
{

/// Whether a sphere touches the view volume of a view-projection matrix (the planes of its rows, Gribb-Hartmann)
[[nodiscard]] inline bool SphereInView(const glm::mat4& viewProjection, const glm::vec3& centre, float radius)
{
	const glm::mat4 rows = glm::transpose(viewProjection);
	for (int plane = 0; plane < 6; ++plane)
	{
		const glm::vec4 p = rows[3] + (plane % 2 == 0 ? 1.0f : -1.0f) * rows[plane / 2];
		if (glm::dot(glm::vec3(p), centre) + p.w < -radius * glm::length(glm::vec3(p)))
		{
			return false;
		}
	}
	return true;
}

/// A mesh's bounding box under a drawn model matrix (Renderer.cpp's two culls: boned instances, PSys mesh atoms): the
/// sphere round the box's centre with half its diagonal times the largest axis scale (column length) of the matrix
[[nodiscard]] inline bool BoxInView(const glm::mat4& viewProjection, const AxisAlignedBoundingBox& box, const glm::mat4& model)
{
	const float scale = std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
	                              glm::length(glm::vec3(model[2]))});
	return SphereInView(viewProjection, glm::vec3(model * glm::vec4(box.Center(), 1.0f)), glm::length(box.Size()) * 0.5f * scale);
}

/// The drawn camera as LH3DTech keeps it for the frame
struct ScreenView
{
	glm::mat4 worldToClip {1.0f}; ///< g_world_to_clipping [0xEA9E40] (glm: clip = worldToClip x (p, 1); w = depth)
	float nearW {1.0f};           ///< [0xE839E0]
	glm::ivec2 screen {640, 480}; ///< g_info_transform [0xE839E4] / [0xE839E8] (the half size [0xE839F0] / [0xE839F4])
	glm::vec3 eye {0.0f};         ///< g_camera [0xEA1DB8]
	float tanHalfFov {1.0f};      ///< [0xC3812C] / near (= near T / near): tan(horizontal fov / 2)
};

/// fn_0081F1D0(&p): clip w < near -> false; inv = 1 / w; sx = ftol((x inv + 1) halfW), sy = ftol((1 - y inv) halfH)
/// (__ftol truncates towards 0: a point under a pixel left of / above the screen still counts); on the screen when
/// 0 <= sx < W and 0 <= sy < H (0x81F286..0x81F2A8)
[[nodiscard]] inline bool PointOnScreen(const ScreenView& view, const glm::vec3& point)
{
	const auto clip = view.worldToClip * glm::vec4(point, 1.0f);
	if (clip.w < view.nearW) // 0x81F1FA: `fcom [0xE839E0]; test ah, 1`
	{
		return false;
	}
	const float inv = 1.0f / clip.w;
	const glm::vec2 half = glm::vec2(view.screen) * 0.5f;
	const auto sx = static_cast<int32_t>((clip.x * inv + 1.0f) * half.x); // 0x81F214..0x81F246
	const auto sy = static_cast<int32_t>((1.0f - clip.y * inv) * half.y); // 0x81F24B..0x81F27F
	return sx >= 0 && sy >= 0 && sx < view.screen.x && sy < view.screen.y;
}

/// LH3DBoundingBox::CheckRegionOnScreen 0x868C80, the path with [0xEA9EB4] == 0 (the other, fn_007ACC60: pending):
/// `centre` = the box centre through the object's matrix (0x868CBC..0x868D04), `radius` = the object's scale +0x44 x the
/// box's +0x1C, `origin` = the object's own position (matrix +0x38). w + r < near -> false (0x868D88); the eye closer
/// than r to `origin` -> true (0x868DA1..0x868DE1); else rs = r near inv W 0.5 / [0xC3812C] = r halfW / (w T) round the
/// centre's (sx, sy) (not truncated): false when sx + rs < 0, sx - rs > W, sy + rs < 0 or sy - rs > H, else true
/// (partly on the screen counts). (not ported) its side effects: g_b_last_on_screen [0xEA1AF0], g_last_selected_box
/// [0xEA1AD0], g_last_distance [0xEA1AF4], [0xC37EA0], the 3D object's vt +0xA0
[[nodiscard]] inline bool SphereOnScreen(const ScreenView& view, const glm::vec3& centre, float radius,
                                         const glm::vec3& origin)
{
	const auto clip = view.worldToClip * glm::vec4(centre, 1.0f);
	if (clip.w + radius < view.nearW)
	{
		return false;
	}
	if (radius * radius > glm::distance2(origin, view.eye)) // `fcompp; test ah, 0x41; jne`: only r^2 > d^2
	{
		return true;
	}
	const float inv = 1.0f / clip.w;
	const glm::vec2 half = glm::vec2(view.screen) * 0.5f;
	const float sx = (clip.x * inv + 1.0f) * half.x;
	const float sy = (1.0f - clip.y * inv) * half.y;
	const auto width = static_cast<float>(view.screen.x);
	const auto height = static_cast<float>(view.screen.y);
	const float rs = radius * inv * width * 0.5f / view.tanHalfFov; // 0x868E59..0x868E7F
	if (rs + sx < 0.0f || sx - rs > width || rs + sy < 0.0f) // 0x868E85..0x868ED4
	{
		return false;
	}
	return !(sy - rs > height); // 0x868EDA..0x868EFD
}

} // namespace openblack::graphics::region_on_screen
