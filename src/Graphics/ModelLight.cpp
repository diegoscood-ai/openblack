/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModelLight.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "3D/LandMorph.h"
#include "Lh3dColour.h"

using namespace openblack;

namespace
{
constexpr float k_FocusLift = 10.0f;    ///< [0x8AB414] (0x5E58B1)
constexpr float k_NightSkyType = 1.5f;  ///< the double of [0x8C5838] (0x5E5A6C)
constexpr float k_LightDistance = 3.0f; ///< [0x8C2C50] (0x5E5B12)
constexpr float k_ColourSteps = 255.0f; ///< [0x8AB270]

/// [0xEA9E90]: the one point light of LH3DTech, the default sun until fn_005E5830 moves it
glm::vec3 g_light = model_light::k_DefaultSun;
/// [0xC39264]
int g_ambient = model_light::k_DefaultAmbient;
} // namespace

glm::vec3 model_light::Light()
{
	return g_light;
}

void model_light::SetLight(const glm::vec3& position)
{
	g_light = position;
}

int model_light::Ambient()
{
	return g_ambient;
}

void model_light::SetAmbient(int ambient)
{
	g_ambient = ambient;
}

void model_light::UpdateFrameLight(glm::vec3 focus, const glm::vec3& cameraPosition, float skyType)
{
	// 0x5E58AC..0x5E58C6: the focus never goes below the land under it plus 10
	focus.y = std::max(focus.y, land_morph::CurrentAltitude()(glm::vec2(focus.x, focus.z)) + k_FocusLift);
	// 0x5E5A6C..0x5E5A77: fcomp against the double 1.5 and `test ah, 0x41` / `jne`, so "not greater" (and the
	// unordered case) takes the day branch
	if (!(skyType > k_NightSkyType))
	{
		SetLight(k_DefaultSun); // 0x5E5B70: fn_0081E1F0(0xEA1C88)
		return;
	}
	// 0x5E5A7D..0x5E5B64: focus + 3 normalize(g_camera - focus), with only the exactly null vector left unnormalised
	// (0x5E5AA9..0x5E5AD6)
	const auto toCamera = cameraPosition - focus;
	const auto lengthSquared = glm::dot(toCamera, toCamera);
	const auto direction = lengthSquared > 0.0f ? toCamera / std::sqrt(lengthSquared) : toCamera;
	SetLight(focus + direction * k_LightDistance);
}

glm::vec3 model_light::LightInMeshSpace(const glm::mat4& model)
{
	// 0x855349 SetInverse (LHMatrix::SetInverse 0x7FB290: the 3x3 inverse by adjugate / determinant and the
	// translation -t A^-1, which is glm::affineInverse), then the light as a point through it (0x85534E..0x8553D3) and
	// InverseSquareRoot (0x85540A)
	const auto local = glm::vec3(glm::affineInverse(model) * glm::vec4(Light(), 1.0f));
	const auto lengthSquared = glm::dot(local, local);
	return lengthSquared > 0.0f ? local / std::sqrt(lengthSquared) : local;
}

glm::vec4 model_light::Uniform()
{
	return {Light(), static_cast<float>(Ambient())};
}

int model_light::Intensity(float dot, bool truncate)
{
	const auto lit = k_ColourSteps * dot;
	// 0x84BBBE fistp: to the nearest, halves to even (the FPU's default mode, which std::lrint keeps) or, in the
	// __ftol variant (0x859649), towards zero
	return truncate ? static_cast<int>(lit) : static_cast<int>(std::lrint(lit));
}

int model_light::Factor(int intensity, int ambient)
{
	// 0x84BBC3..0x84BBE5
	return intensity < 0 ? ambient : ambient + (((0xFF - ambient) * intensity) >> 8);
}

uint32_t model_light::Apply(uint32_t colour, int intensity, int ambient)
{
	// 0x84BBEA..0x84BC1D: one imul per channel and the bits of the byte kept, so each channel is (c f) >> 8 truncated
	return lh3d_colour::ScaleShr8_3KeepA(colour, static_cast<uint32_t>(Factor(intensity, ambient)));
}
