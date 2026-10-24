/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconModels.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <bit>

#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_double4.hpp>

#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "GestureIconSeeds.h"
#include "Locator.h"

namespace openblack::magic::gestures
{
namespace
{
// The game's FPU rounds every step to a float: each one is its own statement here, so that none is fused. Its sines
// stay wider than a float until they are multiplied, so they are kept in double; its cosines of the leash's turns are
// stored as floats first
constexpr float k_SpinPerLeash = 0.05f;
constexpr float k_SpinZPerLeash = 0.06f;
constexpr float k_MsToSeconds = 0.001f;
constexpr float k_SpinX = 0.65823f;
constexpr float k_SpinZ = 0.79543f;
constexpr float k_SpinRate = 1.0f;
constexpr float k_LeashModelScale = 0.15f;
constexpr float k_BubbleModelScale = 0.1f;
/// A quarter turn, as a float
constexpr float k_QuarterTurn = std::bit_cast<float>(0x3FC90FDBu);
/// The object's draw leaves a model out beyond this many times its half diagonal as it is scaled
constexpr float k_FarCut = 300.0f;
constexpr float k_Half = 0.5f;

/// A float times one of the game's wide sines, rounded once to a float
float TimesSine(double sine, float value)
{
	return static_cast<float>(sine * static_cast<double>(value));
}

/// Two rows turned in their plane by an angle: a' = c a - s b, b' = c b + s a, each component on its own
void Turn(glm::vec3& a, glm::vec3& b, float c, double s)
{
	for (int j = 0; j < 3; ++j)
	{
		const float sa = TimesSine(s, a[j]);
		const float ca = c * a[j];
		const float sb = TimesSine(s, b[j]);
		const float newA = ca - sb;
		const float cb = c * b[j];
		const float newB = cb + sa;
		a[j] = newA;
		b[j] = newB;
	}
}

/// The rows as a matrix whose columns they are, k_IconDepth along z
glm::mat4 AtIconDepth(const std::array<glm::vec3, 3>& rows)
{
	glm::mat4 matrix(1.0f);
	for (int r = 0; r < 3; ++r)
	{
		matrix[r] = glm::vec4(rows.at(static_cast<size_t>(r)), 0.0f);
	}
	matrix[3] = glm::vec4(0.0f, 0.0f, k_IconDepth, 1.0f);
	return matrix;
}

/// The leashes' spins (the miracles' service's hand magic state), kept until the map is cleared
struct IconModelState
{
	LeashSpins spins;
};
} // namespace

int IconModelSeed(const GestureIcon& icon)
{
	return icon.order == 9 ? k_InverseSpiralSeed : icon.seed;
}

IconModelKind IconModelKindOf(const GestureIcon& icon)
{
	if (icon.leash != -1)
	{
		return IconModelKind::Leash;
	}
	if (IconModelSeed(icon) != -1)
	{
		return IconModelKind::SeedGraphic;
	}
	return icon.order == 8 ? IconModelKind::Bubble : IconModelKind::None;
}

glm::vec2 IconModelScreenOffset(const IconBox& box, glm::ivec2 screen)
{
	const auto halfWidth = static_cast<float>(screen.x / 2);
	const auto halfHeight = static_cast<float>(screen.y / 2);
	// across: (x1 + x0) / 2 - W / 2
	float x = box.max.x + box.min.x;
	x = x * k_Half;
	x = x - halfWidth;
	// down: (y0 - y1) / 2 + y0 - H / 2, the middle of the box above
	float y = box.min.y - box.max.y;
	y = y * k_Half;
	y = y + box.min.y;
	y = y - halfHeight;
	return {x, y};
}

std::array<glm::vec2, 4> IconModelDepthBox(const IconBox& box)
{
	float top = box.min.y + box.min.y;
	top = top - box.max.y;
	return {{{box.min.x, box.min.y}, {box.max.x, box.min.y}, {box.max.x, top}, {box.min.x, top}}};
}

glm::vec3 IconPointInWorld(const glm::dmat4& iconToClip, const glm::dmat4& clipToWorld, glm::vec2 offset, glm::vec2 screen,
                           glm::vec3 point)
{
	glm::dvec4 clip = iconToClip * glm::dvec4(glm::dvec3(point), 1.0);
	if (clip.w > 0.0)
	{
		const glm::dvec2 size(screen);
		double x = (clip.x / clip.w + 1.0) * 0.5 * size.x + static_cast<double>(offset.x);
		double y = (1.0 - clip.y / clip.w) * 0.5 * size.y + static_cast<double>(offset.y);
		x = x < 0.0 ? 0.0 : (x < size.x ? x : size.x - 1.0);
		y = y < 0.0 ? 0.0 : (y < size.y ? y : size.y - 1.0);
		clip.x = (x / size.x * 2.0 - 1.0) * clip.w;
		clip.y = (1.0 - y / size.y * 2.0) * clip.w;
	}
	const glm::dvec4 world = clipToWorld * clip;
	return glm::vec3(glm::dvec3(world) / world.w);
}

void StepLeashSpin(LeashSpin& spin, int leash, uint32_t frameMs)
{
	auto& [r0, r1, r2] = spin.rows;
	const auto l = static_cast<float>(leash);
	const float l05 = l * k_SpinPerLeash;
	float rate = 1.0f + l05;
	rate = rate * k_SpinRate;
	const float seconds = static_cast<float>(frameMs) * k_MsToSeconds;
	// about x, in the plane of rows 1 and 2
	float angle = 1.0f - l05;
	angle = angle * k_SpinRate;
	angle = angle * k_SpinX;
	angle = angle * seconds;
	Turn(r1, r2, static_cast<float>(std::cos(static_cast<double>(angle))), std::sin(static_cast<double>(angle)));
	// about y: rows 2 and 0
	angle = seconds * rate;
	Turn(r2, r0, static_cast<float>(std::cos(static_cast<double>(angle))), std::sin(static_cast<double>(angle)));
	// about z: rows 0 and 1
	const float l06 = l * k_SpinZPerLeash;
	angle = 1.0f - l06;
	angle = angle * rate;
	angle = angle * k_SpinZ;
	angle = angle * seconds;
	Turn(r0, r1, static_cast<float>(std::cos(static_cast<double>(angle))), std::sin(static_cast<double>(angle)));
}

glm::mat4 LeashModelMatrix(const LeashSpin& spin)
{
	std::array<glm::vec3, 3> rows {};
	for (size_t r = 0; r < rows.size(); ++r)
	{
		for (int j = 0; j < 3; ++j)
		{
			rows.at(r)[j] = spin.rows.at(r)[j] * k_LeashModelScale;
		}
	}
	return AtIconDepth(rows);
}

float LeashModelBand(int leash)
{
	const int band = leash - 1;
	return band == 0 ? 0.375f : static_cast<float>(band) * 0.125f;
}

glm::mat4 BubbleModelMatrix()
{
	std::array<glm::vec3, 3> rows {
	    {{k_BubbleModelScale, 0.0f, 0.0f}, {0.0f, k_BubbleModelScale, 0.0f}, {0.0f, 0.0f, k_BubbleModelScale}}};
	// a quarter turn of rows 1 and 2, its sine and cosine both kept wide
	const double c = std::cos(static_cast<double>(k_QuarterTurn));
	const double s = std::sin(static_cast<double>(k_QuarterTurn));
	auto& r1 = rows[1];
	auto& r2 = rows[2];
	for (int j = 0; j < 3; ++j)
	{
		const float s1 = TimesSine(s, r1[j]);
		const float c1 = TimesSine(c, r1[j]);
		const float s2 = TimesSine(s, r2[j]);
		const float new1 = c1 - s2;
		const float c2 = TimesSine(c, r2[j]);
		const float new2 = c2 + s1;
		r1[j] = new1;
		r2[j] = new2;
	}
	return AtIconDepth(rows);
}

bool BubbleModelDrawn(float halfDiagonal)
{
	// the squared distance from the camera to the model's place: (dx dx + dy dy) + dz dz
	const glm::vec3 d = k_IconModelFocus - k_IconModelEye;
	const float xx = d.x * d.x;
	const float yy = d.y * d.y;
	float distanceSquared = xx + yy;
	const float zz = d.z * d.z;
	distanceSquared = distanceSquared + zz;
	float reach = halfDiagonal * k_BubbleModelScale;
	reach = reach * k_FarCut;
	// drawn unless short of it (a reach that is not a number is short of it too)
	const float reachSquared = reach * reach;
	return reachSquared >= distanceSquared;
}

uint8_t IconModelAlpha(float alpha)
{
	const float scaled = alpha * 255.0f;
	return static_cast<uint8_t>(static_cast<int32_t>(scaled) & 0xFF);
}

IconModelDraw StepIconModel(const GestureIcon& icon, LeashSpins& spins, float alpha, uint32_t frameMs, float bubbleHalfDiagonal)
{
	IconModelDraw draw {.kind = IconModelKindOf(icon), .alpha = IconModelAlpha(alpha)};
	switch (draw.kind)
	{
	case IconModelKind::Leash:
	{
		// (openblack guard) the game's table has the four leash numbers 0 to 3, and its icons only -1, 1, 2 and 3
		if (icon.leash < 0 || static_cast<size_t>(icon.leash) >= spins.size())
		{
			draw.kind = IconModelKind::None;
			break;
		}
		auto& spin = spins.at(static_cast<size_t>(icon.leash));
		if (!spin.has_value())
		{
			spin.emplace();
		}
		StepLeashSpin(*spin, icon.leash, frameMs);
		draw.matrix = LeashModelMatrix(*spin);
		draw.band = LeashModelBand(icon.leash);
		break;
	}
	case IconModelKind::Bubble:
		if (!BubbleModelDrawn(bubbleHalfDiagonal))
		{
			draw.kind = IconModelKind::None;
			break;
		}
		draw.matrix = BubbleModelMatrix();
		break;
	case IconModelKind::SeedGraphic:
	case IconModelKind::None:
	default:
		break;
	}
	return draw;
}

LeashSpins& IconLeashSpins()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::gestures: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<IconModelState>().spins;
}

void ResetIconModels()
{
	IconLeashSpins() = {};
	ResetIconSeeds();
}
} // namespace openblack::magic::gestures
