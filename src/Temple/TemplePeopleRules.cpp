/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePeopleRules.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "3D/ObjectMatrix.h"

namespace openblack::temple_people
{
namespace
{
/// The milliseconds in a second
constexpr float k_Milliseconds = 1000.0f;

/// A place on the screen cut towards zero to a whole pixel; one too far either way for an integer (or no number) is the
/// game's integer indefinite, far off the screen
int32_t ToPixel(float place)
{
	constexpr float k_Limit = 2147483648.0f;
	if (!(place > -k_Limit && place < k_Limit))
	{
		return std::numeric_limits<int32_t>::min();
	}
	return static_cast<int32_t>(place);
}

int32_t TriangleBeyond(const std::vector<WalkMesh::Triangle>& triangles, uint32_t from, uint32_t to)
{
	for (size_t i = 0; i < triangles.size(); ++i)
	{
		const auto& c = triangles[i].corners;
		if ((c[0] == from && c[1] == to) || (c[1] == from && c[2] == to) || (c[2] == from && c[0] == to))
		{
			return static_cast<int32_t>(i);
		}
	}
	return -1;
}

uint32_t CornerOf(std::vector<glm::vec3>& corners, const glm::vec3& vertex)
{
	const auto found = std::ranges::find(corners, vertex);
	if (found != corners.end())
	{
		return static_cast<uint32_t>(found - corners.begin());
	}
	corners.push_back(vertex);
	return static_cast<uint32_t>(corners.size() - 1);
}

/// The best of the samples for one way on: the weight of how far it carries on along the heading, cubed
struct Way
{
	glm::vec3 point {0.0f};
	float weight {0.0f};
	int32_t triangle {-1};
};
} // namespace

WalkMesh BuildWalkMesh(std::span<const glm::vec3> vertices, std::span<const std::array<uint16_t, 3>> triangles)
{
	WalkMesh mesh;
	mesh.triangles.reserve(triangles.size());
	for (const auto& triangle : triangles)
	{
		WalkMesh::Triangle walk;
		for (size_t k = 0; k < 3; ++k)
		{
			walk.corners.at(k) = CornerOf(mesh.corners, vertices[triangle.at(k)]);
		}
		mesh.triangles.push_back(walk);
	}
	for (auto& triangle : mesh.triangles)
	{
		for (size_t k = 0; k < 3; ++k)
		{
			// The edge facing corner k, the other way round
			triangle.beyond.at(k) =
			    TriangleBeyond(mesh.triangles, triangle.corners.at((k + 2) % 3), triangle.corners.at((k + 1) % 3));
		}
	}
	return mesh;
}

glm::vec3 PointIn(const WalkMesh& mesh, int32_t triangle, Dice& dice)
{
	const float along = dice.UpTo(1.0f);
	const float left = 1.0f - along;
	const float across = dice.UpTo(left);
	const auto& corners = mesh.triangles.at(static_cast<size_t>(triangle)).corners;
	const auto& a = mesh.corners.at(corners[0]);
	const auto& b = mesh.corners.at(corners[1]);
	const auto& c = mesh.corners.at(corners[2]);
	glm::vec3 point;
	for (int i = 0; i < 3; ++i)
	{
		const float first = (b[i] - a[i]) * along;
		const float second = (c[i] - a[i]) * across;
		point[i] = first + a[i] + second;
	}
	return point;
}

glm::vec3 PointOnEdge(const WalkMesh& mesh, int32_t triangle, int edge, Dice& dice)
{
	const float along = dice.UpTo(1.0f);
	const auto& corners = mesh.triangles.at(static_cast<size_t>(triangle)).corners;
	const auto& a = mesh.corners.at(corners.at(static_cast<size_t>((edge + 1) % 3)));
	const auto& b = mesh.corners.at(corners.at(static_cast<size_t>((edge + 2) % 3)));
	glm::vec3 point;
	for (int i = 0; i < 3; ++i)
	{
		point[i] = (b[i] - a[i]) * along + a[i];
	}
	return point;
}

float HeadingBetween(const glm::vec3& from, const glm::vec3& to)
{
	// The game's octant arc tangent of the two floor differences, stored as a float
	return static_cast<float>(affine::ArcTanOctant(to.x - from.x, to.z - from.z));
}

float FacingOf(const glm::vec3& direction)
{
	// The game's yaw of a direction: none for one this short along the floor
	return static_cast<float>(affine::GetYAngle(direction));
}

Walker Arrive(const WalkMesh& mesh, Dice& dice)
{
	Walker walker;
	const auto triangle = static_cast<int32_t>(dice.Below(static_cast<int32_t>(mesh.triangles.size())));
	walker.triangle = triangle;
	walker.nextTriangle = triangle;
	walker.position = PointIn(mesh, triangle, dice);
	walker.target = PointIn(mesh, triangle, dice);
	walker.heading = HeadingBetween(walker.position, walker.target);
	walker.facing = FacingOf(walker.target - walker.position);
	return walker;
}

namespace
{
/// Chooses where to go on reaching a point, and whether to stop there first. Returns the gait to have.
Gait ChooseNext(Walker& walker, const WalkMesh& mesh, Dice& dice)
{
	if (walker.nextTriangle >= 0)
	{
		walker.triangle = walker.nextTriangle;
	}
	if (walker.triangle < 0)
	{
		walker.triangle = static_cast<int32_t>(dice.Below(static_cast<int32_t>(mesh.triangles.size())));
		walker.nextTriangle = walker.triangle;
	}
	const float along = std::cos(walker.heading);
	const float across = std::sin(walker.heading);
	const auto& beyond = mesh.triangles.at(static_cast<size_t>(walker.triangle)).beyond;
	// The three ways out across its edges, then on inside it
	const std::array<int32_t, 4> ways {beyond[0], beyond[1], beyond[2], walker.triangle};
	std::array<Way, 4> best {};
	float total = 0.0f;
	for (size_t k = 0; k < ways.size(); ++k)
	{
		best.at(k).triangle = ways.at(k);
		if (ways.at(k) < 0)
		{
			best.at(k).point = PointIn(mesh, walker.triangle, dice);
			best.at(k).weight = k_DeadEndWeight;
		}
		else
		{
			for (int sample = 0; sample < k_SamplesPerWay; ++sample)
			{
				auto point = PointIn(mesh, ways.at(k), dice);
				if (!walker.inside && k < 3)
				{
					point = PointOnEdge(mesh, walker.triangle, static_cast<int>(k), dice);
				}
				const float ahead = (point.x - walker.position.x) * along + across * (point.z - walker.position.z);
				const float weight = std::max(ahead * ahead * ahead, 1.0f);
				if (weight > best.at(k).weight)
				{
					best.at(k).weight = weight;
					best.at(k).point = point;
				}
			}
		}
		total += best.at(k).weight;
	}
	float draw = dice.UpTo(total);
	size_t chosen = 0;
	while (chosen < 3 && draw >= best.at(chosen).weight)
	{
		draw -= best.at(chosen).weight;
		++chosen;
	}
	if (walker.inside)
	{
		chosen = 3;
	}
	walker.inside = !walker.inside;
	walker.target = best.at(chosen).point;
	walker.nextTriangle = best.at(chosen).triangle;
	walker.heading = HeadingBetween(walker.position, walker.target);
	walker.facing = FacingOf(walker.target - walker.position);
	return dice.UpTo(1.0f) < k_StopChance ? Gait::Standing : Gait::Walking;
}
} // namespace

void Step(Walker& walker, const WalkMesh& mesh, float seconds, Dice& dice)
{
	walker.timer += seconds;
	auto gait = walker.gait;
	if (walker.gait == Gait::Walking)
	{
		const auto toGo = walker.target - walker.position;
		const float distance = std::sqrt(toGo.z * toGo.z + toGo.y * toGo.y + toGo.x * toGo.x);
		const float step = seconds * walker.speed;
		const float share = step < distance ? step / distance : 1.0f;
		bool reached = true;
		if (distance >= k_ReachedDistance)
		{
			walker.position += toGo * share;
			reached = share >= k_ReachedShare;
		}
		if (reached)
		{
			walker.position = walker.target;
			gait = ChooseNext(walker, mesh, dice);
		}
	}
	else if (walker.timer > walker.pause)
	{
		gait = Gait::Walking;
	}
	if (gait != walker.gait)
	{
		walker.gait = gait;
		walker.timer = 0.0f;
		walker.clipPlace = 0;
		walker.pause = dice.UpTo(k_PauseSpread) + k_PauseLength;
	}
}

std::vector<entt::entity> ChooseVisitors(std::span<const std::vector<entt::entity>> worshippersBySite,
                                         const std::function<bool(entt::entity)>& available)
{
	std::vector<entt::entity> visitors;
	for (const auto& worshippers : worshippersBySite)
	{
		for (const auto worshipper : worshippers)
		{
			if (visitors.size() >= k_MostVisitors)
			{
				return visitors;
			}
			if (available(worshipper) && std::ranges::find(visitors, worshipper) == visitors.end())
			{
				visitors.push_back(worshipper);
			}
		}
	}
	return visitors;
}

glm::vec4 FeetInClip(const glm::mat4& worldToClip, const glm::vec3& feet)
{
	glm::vec4 clip;
	for (int row = 0; row < 4; ++row)
	{
		const float xz = worldToClip[0][row] * feet.x + worldToClip[2][row] * feet.z;
		const float xzy = xz + worldToClip[1][row] * feet.y;
		clip[row] = xzy + worldToClip[3][row];
	}
	return clip;
}

bool Drawn(const glm::vec4& feet, float nearPlane, glm::vec2 resolution)
{
	if (feet.w < nearPlane)
	{
		return false;
	}
	// The feet's pixel, y from the top, each place cut towards zero to a whole pixel
	const float rhw = 1.0f / feet.w;
	const float halfWidth = resolution.x * 0.5f;
	const float halfHeight = resolution.y * 0.5f;
	const auto x = ToPixel((feet.x * rhw + 1.0f) * halfWidth);
	const auto y = ToPixel((1.0f - feet.y * rhw) * halfHeight);
	if (x < 0 || y < 0 || x >= static_cast<int32_t>(resolution.x) || y >= static_cast<int32_t>(resolution.y))
	{
		return false;
	}
	const float fade = 1.0f - (feet.w - k_FadeStart) * k_FadePerUnit;
	return fade > k_DrawnFade;
}

std::vector<Pass> DrawsInFrame(std::span<const RoomDrawn> rooms, bool reflections)
{
	std::vector<Pass> draws;
	for (const auto& room : rooms)
	{
		if (room.mirrored && reflections)
		{
			draws.push_back(Pass::Reflection);
		}
		draws.push_back(Pass::Room);
		if (room.drawsHand)
		{
			break;
		}
	}
	return draws;
}

Look LookOf(Pass pass, const std::optional<FadeAtFeet>& fade)
{
	const uint32_t grey = pass == Pass::Reflection ? k_ReflectionColour : k_RoomColour;
	if (!fade.has_value())
	{
		return {.colour = grey, .specular = 0};
	}
	const auto bytes = [](glm::vec3 rgb) {
		return (static_cast<uint32_t>(rgb.r) << 16u) | (static_cast<uint32_t>(rgb.g) << 8u) | static_cast<uint32_t>(rgb.b);
	};
	const glm::vec3 rgb {static_cast<float>((grey >> 16u) & 0xFFu), static_cast<float>((grey >> 8u) & 0xFFu),
	                     static_cast<float>(grey & 0xFFu)};
	const auto fogged = temple_fade::Once(rgb, glm::vec3(0.0f), fade->depth, fade->fade, fade->endColour);
	return {.colour = (grey & 0xFF000000u) | bytes(fogged.colour), .specular = 0xFF000000u | bytes(fogged.added)};
}

AnimId ClipOf(Gait gait, bool man)
{
	if (gait == Gait::Standing)
	{
		return AnimId::PStand;
	}
	return man ? AnimId::PWalkMan : AnimId::PWalkWoman;
}

float SpeedOf(float stride, uint32_t playTime, float scale)
{
	if (playTime == 0)
	{
		return 0.0f;
	}
	return stride * k_Milliseconds / static_cast<float>(playTime) * scale;
}

uint32_t AdvanceClipPlace(uint32_t place, uint32_t played, uint32_t playTime, bool looping)
{
	if (playTime == 0)
	{
		return 0;
	}
	const auto moved = static_cast<int32_t>(place) + static_cast<int32_t>(played);
	if (looping)
	{
		return static_cast<uint32_t>(moved % static_cast<int32_t>(playTime));
	}
	return static_cast<uint32_t>(std::min(moved, static_cast<int32_t>(playTime) - 1));
}

} // namespace openblack::temple_people
