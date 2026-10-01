/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalWallHug.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

/// The circle hug for the animals (docs/bw1-notes/animals.md; research dev\tmp_dis\animals\wallhug.md,
/// wallhug_circle.md). Positions in metres (the original's MapCoords / 6553.6), angles in 2048ths.
namespace openblack::ecs::animal_ai::detail
{
using components::AnimalBrain;
using components::Fixed;
using components::Transform;

namespace
{
/// NewCollideDescriptor::Init (0x46AC23): a multi-cell fixed object is in the map cells whose 7.1 m circle round the
/// cell's centre its collide circles touch
constexpr float k_CellCircleRadius = 7.1f;
/// ObjectCircleIterator::Init (0x60D21C): the circle of a water cell, round its centre
constexpr float k_WaterCircleRadius = 7.2f;
/// Tree::CreateCollideData (0x74C622): a tree's trunk
constexpr float k_TreeCircleRadius = 0.3f;
/// NewCollide::NewCollide (0x829470): a box more than 1.4 times longer than wide is a row of circles
constexpr float k_LongBox = 1.4f;
/// the sweeps' "behind me" bound, the double at 0x930668
constexpr double k_Behind = -0.2;
/// 0x930660: arc length (MapCoords) / radius (m) -> game angle, 2048 / (2 pi x 6553.6)
constexpr float k_ArcToAngle = 0.0497359186f;
/// cos / sin of 0.1 degrees (0x930680 / 0x93067C): the goal point is taken that much before the goal
constexpr float k_Cos01 = 0.99999845f;
constexpr float k_Sin01 = 0.00174532842f;
/// MoveToCircleHugCircleSquareSweep's recursion budget (0xBF42C8 / 0xBF42CC = 3)
constexpr int k_SweepDepth = 3;

/// a NewCollide::Obj the iterator gives: owner null = a water cell's
struct Circle
{
	glm::vec2 centre;
	float radius;
	entt::entity owner;
};

bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
	return trace;
}

void SetMoveState(Context& ctx, uint8_t state)
{
	if (Trace() && state != ctx.brain.moveState)
	{
		const auto& c = ctx.brain.hugCircle;
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Animal {}: move {:#x} -> {:#x} at ({:.2f}, {:.2f}) circle ({:.2f}, {:.2f}) r {:.2f}",
		                   static_cast<uint32_t>(ctx.entity), ctx.brain.moveState, state, ctx.transform.position.x,
		                   ctx.transform.position.z, c.centre.x, c.centre.y, c.radius);
	}
	ctx.brain.moveState = state;
}

glm::vec2 StepMetres(glm::ivec2 step)
{
	return glm::vec2(step) / k_MapCoordsPerMetre;
}

/// CircleHugInfo::GetObjectPtr (0x60A660). A circle whose fixed object was deleted is none [inferred: the original
/// drops the huggers of a deleted object through g_CircleHugStateInfo]
const AnimalBrain::HugCircle* HugObj(Context& ctx)
{
	auto& circle = ctx.brain.hugCircle;
	if (circle.set && circle.owner != entt::null && !Locator::entitiesRegistry::value().Valid(circle.owner))
	{
		circle.set = false;
	}
	return circle.set ? &circle : nullptr;
}

/// CircleHugInfo::SetObjectPtr (0x60A770); the rest of it is g_CircleHugStateInfo's bookkeeping
void SetObjectPtr(Context& ctx, const Circle* circle)
{
	ctx.brain.hugCircle = circle != nullptr ? AnimalBrain::HugCircle {circle->centre, circle->radius, circle->owner, true}
	                                        : AnimalBrain::HugCircle {};
	if (Trace() && circle != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: hug circle ({:.2f}, {:.2f}) r {:.2f} {} turns {}",
		                   static_cast<uint32_t>(ctx.entity), circle->centre.x, circle->centre.y, circle->radius,
		                   circle->owner == entt::null ? "water" : "object", ctx.brain.turnsToObj);
	}
}

/// MobileWallHug::SetGameAngle (0x60DA90) and RebuildMoveByStep (0x609D10): the heading, and the step along it
void SetGameAngle(Context& ctx, int32_t angle)
{
	ctx.brain.angle = static_cast<uint16_t>(angle & 0x7FF);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

void RebuildMoveByStep(Context& ctx)
{
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
}

/// FINAL_STEP (4) from a hug handler (0x60B232, 0x60B801..0x60B81E): the step is Pos - goal (unused: FINAL_STEP snaps
/// to the goal next turn, 0x60AF6C)
void SetFinalStep(Context& ctx)
{
	ctx.brain.step = glm::ivec2((Xz(ctx.transform) - ctx.brain.goal) * k_MapCoordsPerMetre);
	SetMoveState(ctx, k_MoveFinalStep);
}

// ---- the collide circles of a map cell ----

/// NewCollide::NewCollide(LH3DObject) (0x829390) for a box longer than 1.4 times its width: CreateList (0x828F40), a row
/// of int(long / short) + 1 circles of the short half-extent along the long axis
void RowOfCircles(entt::entity entity, const Fixed& fixed, const Transform& transform, std::vector<Circle>& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const components::Mesh>(entity);
	if (mesh == nullptr || !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return;
	}
	const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
	// the half-extents x the scale, at least 1 m (as archetypes::GetFixedObstacleBoundingCircle)
	const glm::vec2 size(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	const glm::vec2 half = glm::max(glm::vec2(1.0f), size * 0.5f);
	const bool alongX = half.x > half.y;
	const float longHalf = alongX ? half.x : half.y;
	const float shortHalf = alongX ? half.y : half.x;
	if (!(longHalf / shortHalf > k_LongBox))
	{
		return;
	}
	const int count = static_cast<int>(longHalf / shortHalf) + 1;
	const float spacing = 2.0f * longHalf / static_cast<float>(count);
	for (int i = 0; i < count; ++i)
	{
		const float offset = (static_cast<float>(i) + 0.5f) * spacing - longHalf;
		const glm::vec3 local = alongX ? glm::vec3(offset, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, offset);
		const glm::vec3 world = transform.rotation * local;
		out.push_back({fixed.boundingCenter + glm::vec2(world.x, world.z), shortHalf, entity});
	}
}

/// NewCollide::Obj::Collide (0x829140) against a cell's circle: d^2 <= (r1 + r2)^2
bool TouchesCell(glm::vec2 centre, float radius, glm::vec2 cellCentre)
{
	const float reach = radius + k_CellCircleRadius;
	return glm::distance2(centre, cellCentre) <= reach * reach;
}

/// the land cell under a cell index is water, or there is none (LH3DIsland::IsWater 0x60D3A0; the neighbours' test in
/// ObjectCircleIterator::Init(int) 0x60D0A0 is the same hasWater bit)
bool WaterCell(glm::ivec2 cell)
{
	if (!map_coords::InBounds(cell)) // JustMapXZ::InBounds 0x5E1860: movsx, then the unsigned compare with 512
	{
		return true;
	}
	return Collides(glm::vec2(cell) * 10.0f + 5.0f, 1u);
}

/// ObjectCircleIterator over the map cell of p (Init 0x60D280 / 0x60D0A0, GetMapChild 0x638560): the collide circles of
/// the cell's fixed objects that have any (not a field, whose circles the iterator skips; a forest has none, a tree only
/// its trunk in its own cell; a multi-cell object in each cell it touches), then the water circles of the cell and of
/// its 8 neighbours in the original's order. openblack's order of the objects is its registry's, not the cell list's.
std::vector<Circle> CellCircles(glm::vec2 p)
{
	std::vector<Circle> out;
	const auto cell = CellOf(p);
	const glm::vec2 cellCentre = glm::vec2(cell) * 10.0f + 5.0f;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<Circle> row;
	registry.Each<const Fixed, const Transform>([&](entt::entity entity, const Fixed& fixed, const Transform& transform) {
		if (registry.AnyOf<components::Field, components::BigForest>(entity))
		{
			return;
		}
		if (registry.AllOf<components::Tree>(entity))
		{
			// SingleMapFixed: in the cell of its position only [inferred]
			const glm::vec2 at = Xz(transform);
			if (CellOf(at) == cell)
			{
				out.push_back({at, k_TreeCircleRadius, entity});
			}
			return;
		}
		if (!TouchesCell(fixed.boundingCenter, fixed.boundingRadius, cellCentre))
		{
			return;
		}
		row.clear();
		RowOfCircles(entity, fixed, transform, row);
		if (row.empty())
		{
			out.push_back({fixed.boundingCenter, fixed.boundingRadius, entity});
			return;
		}
		const auto touches = [&cellCentre](const Circle& c) { return TouchesCell(c.centre, c.radius, cellCentre); };
		if (std::any_of(row.begin(), row.end(), touches))
		{
			out.insert(out.end(), row.begin(), row.end());
		}
	});
	// Init(Object*) at the end of the objects: the cell itself, then Init(n) for n = 1..8
	static constexpr glm::ivec2 k_Neighbours[] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
	for (const auto& offset : k_Neighbours)
	{
		const glm::ivec2 c = glm::ivec2(cell) + offset;
		if (WaterCell(c))
		{
			out.push_back({glm::vec2(c) * 10.0f + 5.0f, k_WaterCircleRadius, entt::null});
		}
	}
	return out;
}

// ---- MoveToCircleHugLinearSquareSweep (0x60CA50) ----

/// LinearSquareSweepStruct (0x10 bytes): lo (+0) and hi (+4) bound the distance along the step to the circle's edge
/// (dot - r and dot, equal once resolved), disc (+8) the ray/circle discriminant, the circle (+0xC)
struct LinearSweep
{
	float lo;
	float hi;
	float disc;
	const Circle* circle;
};

/// fn_0060CEE0: the true entry distance dot - sqrt(disc); behind (< -0.2) it is FLT_MAX
void Resolve(LinearSweep& s)
{
	if (s.lo == s.hi)
	{
		return;
	}
	const float t = s.hi - std::sqrt(s.disc);
	s.lo = s.hi = t;
	if (static_cast<double>(t) < k_Behind)
	{
		s.lo = s.hi = std::numeric_limits<float>::max();
	}
}

/// fn_0060CF20
LinearSweep MakeLinearSweep(const Circle& circle, glm::vec2 pos, glm::vec2 dir)
{
	const glm::vec2 rel = circle.centre - pos;
	const float dot = glm::dot(dir, rel);
	LinearSweep s {dot - circle.radius, dot, dot * dot + (circle.radius * circle.radius - glm::dot(rel, rel)), &circle};
	if (static_cast<double>(s.lo) < k_Behind && s.disc > 0.0f)
	{
		Resolve(s);
	}
	return s;
}

/// fn_0060CFF0: the candidate is nearer than the best
bool Nearer(LinearSweep& candidate, LinearSweep& best)
{
	if (candidate.hi < best.lo)
	{
		return true;
	}
	if (candidate.lo > best.hi)
	{
		return false;
	}
	Resolve(candidate);
	Resolve(best);
	return candidate.hi < best.lo;
}

/// MoveToCircleHugLinearSquareSweep (0x60CA50): the nearest collide circle of dest's cell the step's ray enters; its
/// countdown TurnsToObj (0xFF: none, or more than 255 turns away)
void LinearSquareSweep(Context& ctx, glm::vec2 dest)
{
	// 0x60CAB9: the step in metres, normalised; its length (0 for no step) divides the distance into turns
	glm::vec2 dir = StepMetres(ctx.brain.step);
	float stepLength = 0.0f;
	if (dir.x != 0.0f || dir.y != 0.0f)
	{
		stepLength = glm::length(dir);
		dir /= stepLength;
	}
	SetObjectPtr(ctx, nullptr);
	const auto circles = CellCircles(dest);
	const glm::vec2 pos = Xz(ctx.transform);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: linear sweep of cell ({}, {}): {} circles, dir ({:.2f}, {:.2f})",
		                   static_cast<uint32_t>(ctx.entity), CellOf(dest).x, CellOf(dest).y, circles.size(), dir.x, dir.y);
	}
	size_t i = 0;
	LinearSweep best {};
	for (; i < circles.size(); ++i)
	{
		best = MakeLinearSweep(circles[i], pos, dir);
		if (best.disc > 0.0f)
		{
			break;
		}
	}
	if (i >= circles.size())
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	for (++i; i < circles.size(); ++i)
	{
		auto candidate = MakeLinearSweep(circles[i], pos, dir);
		if (candidate.disc > 0.0f && Nearer(candidate, best))
		{
			best = candidate;
		}
	}
	Resolve(best);
	if (best.lo == std::numeric_limits<float>::max())
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	// 0x60CE5E: turns = distance / |step|; above 255 none, not above 0 (or unordered) 0
	float turns = best.lo / stepLength;
	if (turns > 255.0f)
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	if (!(turns > 0.0f))
	{
		turns = 0.0f;
	}
	ctx.brain.turnsToObj = static_cast<uint8_t>(static_cast<int32_t>(turns));
	if (ctx.brain.turnsToObj != 0xFF)
	{
		SetObjectPtr(ctx, best.circle);
	}
}

// ---- MoveToCircleHugCircleSquareSweep<InCircleStuff_1> (CW, 0x6159F0) / <InCircleStuff_0> (CCW, 0x614C40) ----

/// Point2DCompare: a point on the orbit and its half (+8) relative to where the animal is
struct SidePoint
{
	glm::vec2 p {0.0f};
	bool side {false};
};

/// IntersectIntervalCircle (0x2C bytes): where another circle cuts the orbit, bounded by lo (+0) and hi (+0xC) until
/// resolved; a (+0x18), h2 (+0x1C) and h (+0x20) the chord's terms in the frame of d = its centre - the orbit's centre
/// (the cut is a d + h perp(d), valid when h2 > 0); the circle (+0x28), none for the goal
struct Intersect
{
	SidePoint lo;
	SidePoint hi;
	float a {0.0f};
	float h2 {0.0f};
	float h {0.0f};
	const Circle* circle {nullptr};
};

/// the angular order along the orbit, starting from the animal (g = its position - the centre, 0xD3EE70 / 0xD3EE68)
struct OrbitOrder
{
	bool cw;
	glm::vec2 g;
	float r;

	/// Resolve__Point2DCompare (0x6101C0 / 0x610230): the half the orbit reaches first
	[[nodiscard]] bool Side(glm::vec2 p) const
	{
		const float c = g.x * p.y - g.y * p.x;
		return cw ? c <= 0.0f : c >= 0.0f;
	}

	/// Point2DCompare::operator< (0x610180 / 0x6101F0): a is reached before b
	[[nodiscard]] bool Lt(const SidePoint& a, const SidePoint& b) const
	{
		if (a.side != b.side)
		{
			return a.side;
		}
		const float c = b.p.y * a.p.x - a.p.y * b.p.x;
		return cw ? c < 0.0f : c > 0.0f;
	}

	/// fn_00616C70 / IntersectIntervalCircle_1::Resolve (0x6169F0): the exact cut
	void Exact(Intersect& s) const
	{
		if (s.lo.p == s.hi.p)
		{
			return;
		}
		s.h = std::sqrt(s.h2);
		const glm::vec2 d = s.hi.p;
		s.lo.p = cw ? glm::vec2(s.a * d.x - s.h * d.y, s.a * d.y + s.h * d.x)
		            : glm::vec2(s.a * d.x + s.h * d.y, s.a * d.y - s.h * d.x);
		s.lo.side = Side(s.lo.p);
		s.hi = s.lo;
	}

	/// fn_00616D00 / fn_00616A80 (and the same code inline for the first ones): hi = d, lo = d + perp(d) (x the radii's
	/// ratio when the other circle is not bigger); resolved at once when the bounds come out in the wrong order
	[[nodiscard]] Intersect Make(const Circle& circle, glm::vec2 centre) const
	{
		Intersect s;
		s.circle = &circle;
		const glm::vec2 d = circle.centre - centre;
		s.hi = {d, Side(d)};
		glm::vec2 v = cw ? glm::vec2(-d.y, d.x) : glm::vec2(d.y, -d.x);
		if (!(circle.radius > r))
		{
			v *= circle.radius / r;
		}
		s.lo.p = d + v;
		s.lo.side = Side(s.lo.p);
		const float dd = glm::dot(d, d);
		s.a = 0.5f * (dd + r * r - circle.radius * circle.radius) / r;
		s.h2 = dd - s.a * s.a;
		if (s.h2 > 0.0f && Lt(s.hi, s.lo))
		{
			Exact(s);
		}
		return s;
	}
};

/// the end of the sweep (0x6166E3 / 0x61593B): the heading along the orbit, towards the centre + / - 90 degrees, less 64
/// per radius the animal is off 0.9 x the radius
void AimAlongOrbit(Context& ctx, bool cw, float numCircles)
{
	const auto* circle = HugObj(ctx);
	if (circle == nullptr)
	{
		return;
	}
	const int32_t toCentre = AngleOf(circle->centre - Xz(ctx.transform));
	const int32_t angle = cw ? toCentre + 0x200 - static_cast<int32_t>(numCircles * 64.0f)
	                         : toCentre - 0x200 - static_cast<int32_t>(numCircles * -64.0f);
	SetGameAngle(ctx, angle);
	RebuildMoveByStep(ctx);
}

/// MoveToCircleHugCircleSquareSweep: along the orbit of the current circle, the first place where another circle of
/// dest's cell cuts it (or the goal, when the goal is inside this circle); TurnsToObj = the turns to get there at 2/3 of
/// the speed, then the heading along the orbit. Closer than a turn: the goal -> STEP_THROUGH; another circle -> it
/// becomes the circle and the sweep runs again from it (3 levels at most, then TurnsToObj 10).
int CircleSquareSweep(Context& ctx, glm::vec2 dest, bool cw, int depth)
{
	const auto* current = HugObj(ctx);
	if (current == nullptr)
	{
		// the original always has one here (it reads GetObjectPtr() unchecked); openblack only gets here when the
		// circle's object was deleted (HugObj) [inferred guard, not in the original]
		ctx.brain.turnsToObj = 0xFF;
		return 1;
	}
	const glm::vec2 centre = current->centre;
	const float r = current->radius;
	const glm::vec2 pos = Xz(ctx.transform);
	const OrbitOrder order {cw, pos - centre, r};
	glm::vec2 u = order.g;
	float length = 0.0f;
	if (u.x != 0.0f || u.y != 0.0f)
	{
		length = glm::length(u);
		u /= length;
	}
	const float numCircles = static_cast<float>(static_cast<double>(length / r) - 0.9);
	const glm::vec2 goal = ctx.brain.goal;
	const bool goalInside = glm::distance2(goal, centre) < r * r;

	const auto circles = CellCircles(dest);
	if (circles.empty() && !goalInside)
	{
		ctx.brain.turnsToObj = 0xFF;
		AimAlongOrbit(ctx, cw, numCircles);
		return 1;
	}

	Intersect best;
	SidePoint goalPoint;
	size_t i = 0;
	if (goalInside)
	{
		// the goal, 0.1 degrees before it along the orbit
		const glm::vec2 p = goal - centre;
		glm::vec2 q = u;
		if (p.x != 0.0f || p.y != 0.0f)
		{
			q = cw ? glm::vec2(p.x * k_Cos01 - p.y * k_Sin01, p.y * k_Cos01 + p.x * k_Sin01)
			       : glm::vec2(p.y * k_Sin01 + p.x * k_Cos01, p.y * k_Cos01 - p.x * k_Sin01);
		}
		best.lo = {q, order.Side(q)};
		best.hi = best.lo;
		best.h2 = 1.0f;
		goalPoint = best.hi;
	}
	else
	{
		for (; i < circles.size(); ++i)
		{
			best = order.Make(circles[i], centre);
			if (best.h2 > 0.0f)
			{
				break;
			}
		}
		++i;
	}
	for (; i < circles.size(); ++i)
	{
		auto candidate = order.Make(circles[i], centre);
		if (!(candidate.h2 > 0.0f))
		{
			continue;
		}
		bool first;
		if (order.Lt(candidate.hi, best.lo))
		{
			first = true;
		}
		else if (order.Lt(best.hi, candidate.lo))
		{
			first = false;
		}
		else
		{
			order.Exact(candidate);
			order.Exact(best);
			first = order.Lt(candidate.hi, best.lo);
		}
		// a water circle never replaces the goal
		if (first && !(circles[i].owner == entt::null && best.circle == nullptr))
		{
			best = candidate;
		}
	}

	if (!(best.h2 > 0.0f))
	{
		ctx.brain.turnsToObj = 0xFF;
		AimAlongOrbit(ctx, cw, numCircles);
		return 1;
	}
	order.Exact(best);
	if (goalInside)
	{
		// the goal still wins when it comes before the circle's far cut
		const glm::vec2 p = best.hi.p;
		const float a = best.a;
		const float h = best.h;
		glm::vec2 q;
		if (cw)
		{
			const float t1 = a * p.x + h * p.y;
			const float t2 = a * p.y - h * p.x;
			q = {a * t1 + h * t2, a * t2 - h * t1};
		}
		else
		{
			const float t1 = a * p.x - h * p.y;
			const float t2 = h * p.x + a * p.y;
			q = {a * t1 - h * t2, h * t1 + a * t2};
		}
		if (order.Lt(goalPoint, {q, order.Side(q)}))
		{
			best.hi = goalPoint;
			best.lo = best.hi;
			best.circle = nullptr;
		}
	}
	glm::vec2 n = best.hi.p;
	if (n.x != 0.0f || n.y != 0.0f)
	{
		n = glm::normalize(n);
	}
	float angle = std::acos(std::clamp(glm::dot(n, u), -1.0f, 1.0f));
	// Point2D::Cross (0x611240)
	const float cross = n.y * u.x - u.y * n.x;
	if (cw ? cross > 0.0f : cross < 0.0f)
	{
		angle = glm::two_pi<float>() - angle;
	}
	const double speed = static_cast<double>(ctx.brain.speed) * 10.0 * 1.5;
	const auto turns = static_cast<float>(static_cast<double>(angle) * r * 65536.0 / speed);
	if (turns > 255.0f)
	{
		ctx.brain.turnsToObj = 0xFF;
	}
	else if (turns < 1.0f)
	{
		if (best.circle == nullptr)
		{
			// at the goal's place on the orbit: straight at it (CircleHugInfo::Reset; field_0x78 = 0x10 is not kept:
			// MobileWallHug +0x78 is not identified [inferred: not read on the animals' path])
			InitStepsXZ(ctx);
			SetObjectPtr(ctx, nullptr);
			ctx.brain.turnsToObj = 0xFF;
			ctx.brain.hugGoalDistance = 0;
			SetMoveState(ctx, k_MoveStepThrough);
			return 1;
		}
		SetObjectPtr(ctx, best.circle);
		if (k_SweepDepth - depth > 0)
		{
			return CircleSquareSweep(ctx, dest, cw, depth + 1);
		}
		ctx.brain.turnsToObj = 0xA;
		return 1;
	}
	else if (turns < 4.0f)
	{
		ctx.brain.turnsToObj = 0;
	}
	else
	{
		ctx.brain.turnsToObj = static_cast<uint8_t>(static_cast<int32_t>(turns));
	}
	AimAlongOrbit(ctx, cw, numCircles);
	return 1;
}

// ---- the MoveTo handlers ----

/// MoveToCircleHug (0x60D800): the LINEAR step. A new map cell re-aims (InitStepsXZ) and sweeps again; TurnsToObj
/// running out starts the orbit, on the side the step passes the centre (LINEAR_CW / CCW keep theirs)
int MoveToCircleHug(Context& ctx)
{
	const glm::ivec2 step = ctx.brain.step;
	const glm::vec2 pos = Xz(ctx.transform);
	const glm::vec2 next = pos + StepMetres(step);
	if (CellOf(next) != CellOf(pos))
	{
		InitStepsXZ(ctx);
		LinearSquareSweep(ctx, next);
	}
	if (ctx.brain.turnsToObj != 0xFF)
	{
		const uint8_t was = ctx.brain.turnsToObj--;
		// the original reads GetObjectPtr() unchecked; null here only after its object was deleted [inferred guard]
		const auto* circle = HugObj(ctx);
		if (was == 0 && circle != nullptr)
		{
			const glm::vec2 rel = pos - circle->centre;
			const glm::vec2 s = StepMetres(ctx.brain.step);
			const float cross = rel.y * s.x - s.y * rel.x;
			uint8_t orbit;
			if (ctx.brain.moveState == k_MoveLinear)
			{
				orbit = cross > 0.0f ? k_MoveOrbitCw : k_MoveOrbitCcw;
			}
			else
			{
				orbit = ctx.brain.moveState == k_MoveLinearCw ? k_MoveOrbitCw : k_MoveOrbitCcw;
			}
			SetMoveState(ctx, orbit);
			CircleSquareSweep(ctx, pos, orbit == k_MoveOrbitCw, 1);
			// +0x76: the distance to the goal x 128 [0x930670], less 1 [0x8AB680] (beyond 0xFFFF in
			// g_CircleHugStateInfo). 0x60D9F0: MapCoords::GetMetresDistanceSq 0x605FB0 (the exact square, no table) and
			// `fsqrt` on it; the two qword constants are loaded but the FPU is at 24 bits, so it is all float
			const float v = std::sqrt(gutils::GetMetresDistanceSq(map_coords::FromMetres(pos),
			                                                     map_coords::FromMetres(ctx.brain.goal))) *
			                    128.0f -
			                1.0f;
			ctx.brain.hugGoalDistance = v > 0.0f ? static_cast<uint32_t>(v) : 0u;
		}
	}
	return MoveBy(ctx, step) ? 7 : 6;
}

int Linear(Context& ctx)
{
	// 0x60B095
	if (ctx.brain.turnsToObj != 0xFF && HugObj(ctx) == nullptr)
	{
		LinearSquareSweep(ctx, Xz(ctx.transform));
	}
	const int r = MoveToCircleHug(ctx);
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
	}
	return r;
}

/// ORBIT_CW (0x60B0E4) / ORBIT_CCW (0x60B40E): turn by the arc of one step on the circle, sweep again on a new cell or
/// when TurnsToObj runs out; leave (EXIT_CIRCLE, straight out from the centre) once nearer the goal than when the orbit
/// began, with the goal on the inner side and ahead
int Orbit(Context& ctx, bool cw)
{
	const auto* circle = HugObj(ctx);
	if (circle == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, cw ? k_MoveLinearCw : k_MoveLinearCcw);
		LinearSquareSweep(ctx, Xz(ctx.transform));
		return 1;
	}
	const auto arc = static_cast<int32_t>(static_cast<float>(ctx.brain.speed) / circle->radius * k_ArcToAngle);
	SetGameAngle(ctx, cw ? ctx.brain.angle - arc - 1 : ctx.brain.angle + arc + 1);
	RebuildMoveByStep(ctx);
	const glm::ivec2 step = ctx.brain.step;
	const glm::vec2 pos = Xz(ctx.transform);
	const glm::vec2 next = pos + StepMetres(step);
	if (CellOf(next) != CellOf(pos))
	{
		CircleSquareSweep(ctx, next, cw, 1);
	}
	if (ctx.brain.turnsToObj != 0xFF)
	{
		const uint8_t was = ctx.brain.turnsToObj--;
		if (was == 0)
		{
			CircleSquareSweep(ctx, next, cw, 1);
		}
	}
	const int r = MoveBy(ctx, step) ? 7 : 6;
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
		return r;
	}
	const auto x = static_cast<double>(ctx.brain.hugGoalDistance);
	const auto threshold = static_cast<float>(x * static_cast<double>(6.103515625e-05f) * x);
	const glm::vec2 now = Xz(ctx.transform);
	if (!(glm::distance2(now, ctx.brain.goal) < threshold))
	{
		return r;
	}
	// 0x60B2C5: the step as it is now (the sweeps above may have re-aimed it), not the one it moved by
	const glm::vec2 d = now - ctx.brain.goal;
	const glm::vec2 s(ctx.brain.step);
	const float cross = s.x * d.y - s.y * d.x;
	if (cw ? !(cross < 0.0f) : !(cross > 0.0f))
	{
		return r;
	}
	if (!(d.x * s.x + d.y * s.y < 0.0f))
	{
		return r;
	}
	if (const auto* exit = HugObj(ctx); exit != nullptr)
	{
		SetGameAngle(ctx, AngleOf(now - exit->centre));
		RebuildMoveByStep(ctx);
	}
	SetMoveState(ctx, cw ? k_MoveExitCircleCw : k_MoveExitCircleCcw);
	return r;
}

/// EXIT_CIRCLE (0x60B78F): straight on until out of the circle, then LINEAR_CW / CCW again
int ExitCircle(Context& ctx)
{
	const uint8_t linear = ctx.brain.moveState == k_MoveExitCircleCw ? k_MoveLinearCw : k_MoveLinearCcw;
	if (HugObj(ctx) == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, Xz(ctx.transform));
		return 1;
	}
	const int r = MoveBy(ctx, ctx.brain.step) ? 7 : 6;
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
		return r;
	}
	const auto* circle = HugObj(ctx);
	if (circle != nullptr && circle->radius * circle->radius < glm::distance2(circle->centre, Xz(ctx.transform)))
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, Xz(ctx.transform));
	}
	return r;
}
} // namespace

bool IsHugMoveState(uint8_t state)
{
	return state >= k_MoveLinear && state <= k_MoveExitCircleCw;
}

void SetupMoveToWithHug(Context& ctx, glm::vec2 p, AnimalState final)
{
	if (!SetCurrentAndDestinationState(ctx, final))
	{
		return;
	}
	// MobileWallHug::SetupMobileMoveToPos(p, LINEAR) (0x60ABC0): no CircleHugInfo::Reset, the sweep drops the circle.
	// +0x76 = 0 always here; the original writes it only when g_CircleHugStateInfo had an entry (0x60AC6E) and leaves
	// it stale otherwise, which nothing observes: only ORBIT reads it, after MoveToCircleHug wrote it
	ctx.brain.goal = p;
	InitStepsXZ(ctx);
	ctx.brain.hugGoalDistance = 0;
	if (AreWeThere(ctx))
	{
		ctx.brain.moveState = k_MoveArrived;
		return;
	}
	LinearSquareSweep(ctx, Xz(ctx.transform));
	SetMoveState(ctx, k_MoveLinear);
}

int HugMoveTo(Context& ctx)
{
	switch (ctx.brain.moveState)
	{
	case k_MoveLinear:
	case k_MoveLinearCw:
	case k_MoveLinearCcw:
		return Linear(ctx);
	case k_MoveOrbitCw:
		return Orbit(ctx, true);
	case k_MoveOrbitCcw:
		return Orbit(ctx, false);
	case k_MoveExitCircleCcw:
	case k_MoveExitCircleCw:
		return ExitCircle(ctx);
	default:
		return 0;
	}
}

} // namespace openblack::ecs::animal_ai::detail
