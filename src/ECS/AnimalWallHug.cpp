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
#include <array>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/WalkGoal.h"
#include "ECS/WallHugBookkeeping.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"

/// The circle hug for the animals (docs/wiki/beings/wall-hugging.md#walking-round-obstacles-mobilewallhug). Positions in metres
/// (the original's map coordinates / 6553.6), angles in 2048ths.
namespace openblack::ecs::animal_ai::detail
{
using components::AnimalBrain;
using components::Transform;

namespace
{
/// The sweeps' "behind me" bound
constexpr double k_Behind = -0.2;
/// Arc length (map coordinates) / radius (m) -> game angle, 2048 / (2 pi x 6553.6)
constexpr float k_ArcToAngle = 0.0497359186f;
/// cos / sin of 0.1 degrees: the goal point is taken that much before the goal
constexpr float k_Cos01 = 0.99999845f;
constexpr float k_Sin01 = 0.00174532842f;
/// The circle sweep's recursion budget
constexpr int k_SweepDepth = 3;

/// A collide circle of the cell: owner null = a water cell's
struct Circle
{
	glm::vec2 centre;
	float radius;
	entt::entity owner;
	uint16_t index;
	bool landscapeOrFence;
};

bool Trace()
{
	static const bool trace = debug_env::AnimalTrace();
	return debug_env::TraceLogs(trace, "game");
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

/// Whole map units to metres as the original's sweeps take them: each part times 10 / 65536, rounded once
glm::vec2 StepMetres(glm::ivec2 step)
{
	return {map_coords::ToMetres(step.x), map_coords::ToMetres(step.y)};
}

/// The animal's place and its goal as the original's sweeps and tests read them: in whole map units (WalkUnitsOf,
/// GoalUnitsOf), and those units in metres
map_coords::MapCoords PosCoords(const Context& ctx)
{
	const auto units = WalkUnitsOf(ctx);
	return {units.x, units.y, 0.0f};
}
map_coords::MapCoords GoalCoords(const Context& ctx)
{
	const auto units = GoalUnitsOf(ctx);
	return {units.x, units.y, 0.0f};
}
glm::vec2 PosPoint(const Context& ctx)
{
	return map_coords::ToMetres(PosCoords(ctx));
}
glm::vec2 GoalPoint(const Context& ctx)
{
	return map_coords::ToMetres(GoalCoords(ctx));
}

/// Whether a step from one place to the other in whole map units goes into another map cell (either whole part changes)
bool NewCell(glm::ivec2 from, glm::ivec2 to)
{
	return map_coords::CellOf(from.x) != map_coords::CellOf(to.x) || map_coords::CellOf(from.y) != map_coords::CellOf(to.y);
}

/// A 2D point made a unit one as the original's: the length sqrt(y y + x x), then each part times 1 / length. The
/// length, 0 (and the point left as it is) for none
float NormalizePoint(glm::vec2& v)
{
	if (v.x == 0.0f && v.y == 0.0f)
	{
		return 0.0f;
	}
	const float length = std::sqrt(v.y * v.y + v.x * v.x);
	const float scale = 1.0f / length;
	v = {scale * v.x, scale * v.y};
	return length;
}

/// The circle being hugged; one whose thing's collide data went was let go of there and then (OnHuggedCircleGone)
const AnimalBrain::HugCircle* HugObj(Context& ctx)
{
	const auto& circle = ctx.brain.hugCircle;
	return circle.set ? &circle : nullptr;
}

/// Sets the circle being hugged; the rest of the original's version is bookkeeping
void SetObjectPtr(Context& ctx, const Circle* circle)
{
	ctx.brain.hugCircle = circle != nullptr ? AnimalBrain::HugCircle {circle->centre, circle->radius, circle->owner,
	                                                                  true,           circle->index,  circle->landscapeOrFence}
	                                        : AnimalBrain::HugCircle {};
	if (Trace() && circle != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: hug circle ({:.2f}, {:.2f}) r {:.2f} {} turns {}",
		                   static_cast<uint32_t>(ctx.entity), circle->centre.x, circle->centre.y, circle->radius,
		                   circle->owner == entt::null ? "water" : "object", ctx.brain.turnsToObj);
	}
}

/// The heading, and the step along it
void SetGameAngle(Context& ctx, int32_t angle)
{
	ctx.brain.angle = static_cast<uint16_t>(angle & 0x7FF);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

void RebuildMoveByStep(Context& ctx)
{
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	ctx.brain.stepAltitude = 0.0f;
}

/// FINAL_STEP (4) from a hug handler: the step is Pos - goal (unused: FINAL_STEP snaps to the goal next turn)
void SetFinalStep(Context& ctx)
{
	ctx.brain.step = WalkUnitsOf(ctx) - GoalUnitsOf(ctx);
	ctx.brain.stepAltitude = 0.0f;
	SetMoveState(ctx, k_MoveFinalStep);
}

// ---- the collide circles of a map cell ----

/// The collide circles of the map cell of p, as the villagers' walk reads them (thing_circles::CirclesInCell): those
/// of the things of the cell's fixed list in its order, each thing's from its collide data (its row of circles when it
/// has one), then the water circles of the cell and of its 8 neighbours in the original's order. Which things are in
/// the cell is the list's: a multi-cell thing is in the cells its collide shape covers.
std::vector<Circle> CellCircles(glm::vec2 p)
{
	std::vector<Circle> out;
	for (const auto& found : thing_circles::CirclesInCell(glm::ivec2(CellOf(p))))
	{
		out.push_back({found.circle.centre, found.circle.radius, found.owner, found.index, found.circle.landscapeOrFence});
	}
	return out;
}

// ---- the linear sweep ----

/// lo and hi bound the distance along the step to the circle's edge (dot - r and dot, equal once resolved), disc the
/// ray/circle discriminant, then the circle
struct LinearSweep
{
	float lo;
	float hi;
	float disc;
	const Circle* circle;
};

/// The true entry distance dot - sqrt(disc); behind (< -0.2) it is FLT_MAX
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

/// The sweep entry of a circle along the step's ray
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

/// The candidate is nearer than the best
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

/// The nearest collide circle of dest's cell the step's ray enters; its countdown TurnsToObj (0xFF: none, or more than
/// 255 turns away)
void LinearSquareSweep(Context& ctx, glm::vec2 dest)
{
	// The step in metres, normalised; its length (0 for no step) divides the distance into turns
	glm::vec2 dir = StepMetres(ctx.brain.step);
	const float stepLength = NormalizePoint(dir);
	SetObjectPtr(ctx, nullptr);
	const auto circles = CellCircles(dest);
	const glm::vec2 pos = PosPoint(ctx);
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
	// Turns = distance / |step|; above 255 none, not above 0 (or unordered) 0
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

// ---- the circle sweep (CW / CCW) ----

/// A point on the orbit and its half relative to where the animal is
struct SidePoint
{
	glm::vec2 p {0.0f};
	bool side {false};
};

/// Where another circle cuts the orbit, bounded by lo and hi until resolved; a, h2 and h the chord's terms in the frame
/// of d = its centre - the orbit's centre (the cut is a d + h perp(d), valid when h2 > 0); the circle, none for the goal
struct Intersect
{
	SidePoint lo;
	SidePoint hi;
	float a {0.0f};
	float h2 {0.0f};
	float h {0.0f};
	const Circle* circle {nullptr};
};

/// The angular order along the orbit, starting from the animal (g = its position - the centre)
struct OrbitOrder
{
	bool cw;
	glm::vec2 g;
	float r;

	/// The half the orbit reaches first
	[[nodiscard]] bool Side(glm::vec2 p) const
	{
		const float c = g.x * p.y - g.y * p.x;
		return cw ? c <= 0.0f : c >= 0.0f;
	}

	/// a is reached before b
	[[nodiscard]] bool Lt(const SidePoint& a, const SidePoint& b) const
	{
		if (a.side != b.side)
		{
			return a.side;
		}
		const float c = b.p.y * a.p.x - a.p.y * b.p.x;
		return cw ? c < 0.0f : c > 0.0f;
	}

	/// The exact cut
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

	/// hi = d, lo = d + perp(d) (x the radii's ratio when the other circle is not bigger); resolved at once when the bounds
	/// come out in the wrong order
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

/// The end of the sweep: the heading along the orbit, towards the centre + / - 90 degrees, less 64 per radius the
/// animal is off 0.9 x the radius
void AimAlongOrbit(Context& ctx, bool cw, float numCircles)
{
	const auto* circle = HugObj(ctx);
	// a defensive guard, not reached: a circle that goes is let go of at once (OnHuggedCircleGone)
	if (circle == nullptr)
	{
		return;
	}
	const int32_t toCentre = game_math::GetAngleFromXZ(PosPoint(ctx), circle->centre);
	const int32_t angle = cw ? toCentre + 0x200 - static_cast<int32_t>(numCircles * 64.0f)
	                         : toCentre - 0x200 - static_cast<int32_t>(numCircles * -64.0f);
	SetGameAngle(ctx, angle);
	RebuildMoveByStep(ctx);
}

/// Along the orbit of the current circle, the first place where another circle of
/// dest's cell cuts it (or the goal, when the goal is inside this circle); TurnsToObj = the turns to get there at 2/3 of
/// the speed, then the heading along the orbit. Closer than a turn: the goal -> STEP_THROUGH; another circle -> it
/// becomes the circle and the sweep runs again from it (3 levels at most, then TurnsToObj 10).
int CircleSquareSweep(Context& ctx, glm::vec2 dest, bool cw, int depth)
{
	const auto* current = HugObj(ctx);
	if (current == nullptr)
	{
		// The original always has one here (it reads the circle unchecked). A defensive guard, not reached: a circle that
		// goes is let go of at once (OnHuggedCircleGone)
		ctx.brain.turnsToObj = 0xFF;
		return 1;
	}
	const glm::vec2 centre = current->centre;
	const float r = current->radius;
	const glm::vec2 pos = PosPoint(ctx);
	const OrbitOrder order {cw, pos - centre, r};
	glm::vec2 u = order.g;
	const float length = NormalizePoint(u);
	const float numCircles = static_cast<float>(static_cast<double>(length / r) - 0.9);
	const glm::vec2 goal = GoalPoint(ctx);
	// the goal's square distance to the circle's place in whole map units
	const bool goalInside = game_math::GetMetresDistanceSq(GoalCoords(ctx), map_coords::FromMetres(centre)) < r * r;

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
		// The goal, 0.1 degrees before it along the orbit
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
		// A water circle never replaces the goal
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
		// The goal still wins when it comes before the circle's far cut
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
	static_cast<void>(NormalizePoint(n));
	// The arc as the walkers' look along a circle has it: the runtime's arccosine, not clamped, and the long way round
	const float angle = wall_hug::ArcAngle(n, u, cw);
	// the turns over that arc at the walk's step, each step a float in the game's order
	const float turns = wall_hug::OrbitTurns(angle, r, ctx.brain.speed);
	if (turns > 255.0f)
	{
		ctx.brain.turnsToObj = 0xFF;
	}
	// Turns that are not a number (an arccosine past a full cosine) count as too close, as the game's compare does
	else if (!(turns >= 1.0f))
	{
		if (best.circle == nullptr)
		{
			// At the goal's place on the orbit: straight at it (the hug is reset)
			InitStepsXZ(ctx);
			ctx.brain.turnsUntilStepRebuild = 16;
			SetObjectPtr(ctx, nullptr);
			ctx.brain.turnsToObj = 0xFF;
			ctx.brain.hugGoalDistance = 0;
			ForgetLookahead(ctx);
			SetMoveState(ctx, k_MoveStepThrough);
			return 1;
		}
		// handed over to water, the land's edge or a fence: a walk to try both ways round
		ctx.brain.needsLookahead = best.circle->landscapeOrFence;
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

/// The LINEAR step. A new map cell re-aims (InitStepsXZ) and sweeps again; TurnsToObj
/// running out starts the orbit, on the side the step passes the centre (LINEAR_CW / CCW keep theirs)
int MoveToCircleHug(Context& ctx)
{
	const glm::ivec2 step = ctx.brain.step;
	const glm::vec2 pos = PosPoint(ctx);
	const auto nextUnits = WalkUnitsOf(ctx) + step;
	const glm::vec2 next = map_coords::ToMetres(map_coords::MapCoords {nextUnits.x, nextUnits.y, 0.0f});
	if (NewCell(WalkUnitsOf(ctx), nextUnits))
	{
		InitStepsXZ(ctx);
		LinearSquareSweep(ctx, next);
	}
	if (ctx.brain.turnsToObj != 0xFF)
	{
		const uint8_t was = ctx.brain.turnsToObj--;
		// The original reads the circle unchecked; a defensive guard, not reached since a circle that goes is let go of
		// at once (OnHuggedCircleGone)
		const auto* circle = HugObj(ctx);
		if (was == 0 && circle != nullptr)
		{
			if (circle->owner == entt::null)
			{
				ctx.brain.needsLookahead = true;
			}
			const glm::vec2 rel = pos - map_coords::ToMetres(map_coords::FromMetres(circle->centre));
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
			// The distance to the goal x 128, less 1. The exact square distance and a square root on it; the original
			// loads double constants but computes at float precision, so it is all float
			const float squared = game_math::GetMetresDistanceSq(PosCoords(ctx), GoalCoords(ctx));
			const float v = std::sqrt(squared) * 128.0f - 1.0f;
			ctx.brain.hugGoalDistance = v > 0.0f ? static_cast<uint32_t>(v) : 0u;
		}
	}
	return MoveBy(ctx, step) ? 7 : 6;
}

int Linear(Context& ctx)
{
	if (ctx.brain.turnsToObj != 0xFF && HugObj(ctx) == nullptr)
	{
		LinearSquareSweep(ctx, PosPoint(ctx));
	}
	const int r = MoveToCircleHug(ctx);
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
	}
	return r;
}

/// ORBIT_CW / ORBIT_CCW: turn by the arc of one step on the circle, sweep again on a new cell or
/// when TurnsToObj runs out; leave (EXIT_CIRCLE, straight out from the centre) once nearer the goal than when the orbit
/// began, with the goal on the inner side and ahead
int Orbit(Context& ctx, bool cw)
{
	const auto* circle = HugObj(ctx);
	// a defensive guard, not reached: a circle that goes is let go of at once (OnHuggedCircleGone)
	if (circle == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, cw ? k_MoveLinearCw : k_MoveLinearCcw);
		LinearSquareSweep(ctx, PosPoint(ctx));
		return 1;
	}
	const auto arc = static_cast<int32_t>(static_cast<float>(ctx.brain.speed) / circle->radius * k_ArcToAngle);
	SetGameAngle(ctx, cw ? ctx.brain.angle - arc - 1 : ctx.brain.angle + arc + 1);
	RebuildMoveByStep(ctx);
	const glm::ivec2 step = ctx.brain.step;
	const auto nextUnits = WalkUnitsOf(ctx) + step;
	const glm::vec2 next = map_coords::ToMetres(map_coords::MapCoords {nextUnits.x, nextUnits.y, 0.0f});
	if (NewCell(WalkUnitsOf(ctx), nextUnits))
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
	const glm::vec2 now = PosPoint(ctx);
	if (!(game_math::GetMetresDistanceSq(PosCoords(ctx), GoalCoords(ctx)) < threshold))
	{
		return r;
	}
	// The step as it is now (the sweeps above may have re-aimed it), not the one it moved by; the differences in whole
	// map units, each a float
	const auto at = WalkUnitsOf(ctx);
	const auto goalUnits = GoalUnitsOf(ctx);
	const glm::vec2 d {static_cast<float>(at.x) - static_cast<float>(goalUnits.x),
	                   static_cast<float>(at.y) - static_cast<float>(goalUnits.y)};
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
		SetGameAngle(ctx, game_math::GetAngleFromXZ(exit->centre, now));
		RebuildMoveByStep(ctx);
	}
	// a long entry distance (one the flood set, or one too long for the game's own field) is cleared as it leaves
	if (ctx.brain.hugGoalDistance >= wall_hug::k_LongEntryDistance)
	{
		ctx.brain.hugGoalDistance = 0;
	}
	SetMoveState(ctx, cw ? k_MoveExitCircleCw : k_MoveExitCircleCcw);
	wall_hug::ForgetLookaheadTrial(Locator::entitiesRegistry::value(), ctx.entity);
	return r;
}

/// EXIT_CIRCLE: straight on until out of the circle, then LINEAR_CW / CCW again
int ExitCircle(Context& ctx)
{
	const uint8_t linear = ctx.brain.moveState == k_MoveExitCircleCw ? k_MoveLinearCw : k_MoveLinearCcw;
	// a defensive guard, not reached: a circle that goes is let go of at once (OnHuggedCircleGone)
	if (HugObj(ctx) == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, PosPoint(ctx));
		return 1;
	}
	const int r = MoveBy(ctx, ctx.brain.step) ? 7 : 6;
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
		return r;
	}
	const auto* circle = HugObj(ctx);
	if (circle != nullptr && circle->radius * circle->radius <
	                             game_math::GetMetresDistanceSq(map_coords::FromMetres(circle->centre), PosCoords(ctx)))
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, PosPoint(ctx));
	}
	return r;
}
} // namespace

bool IsHugMoveState(uint8_t state)
{
	return state >= k_MoveLinear && state <= k_MoveExitCircleCw;
}

bool MarkLeaveAtOnce(AnimalBrain& brain, std::span<const entt::entity> overlapping)
{
	if (!brain.hugCircle.set || brain.hugCircle.owner == entt::null ||
	    std::ranges::find(overlapping, brain.hugCircle.owner) == overlapping.end())
	{
		return false;
	}
	brain.hugGoalDistance = wall_hug::k_LeaveAtOnce;
	return true;
}

void OnHuggedCircleGone(Context& ctx)
{
	const auto state = static_cast<wall_hug::WalkState>(ctx.brain.moveState);
	const auto gone = wall_hug::OnCircleGone(state);
	if (gone.reAim)
	{
		InitStepsXZ(ctx);
	}
	if (gone.state != state)
	{
		SetMoveState(ctx, MoveStateOf(gone.state));
	}
	if (gone.lookAgain)
	{
		// from where it stands, along the step it has now
		LinearSquareSweep(ctx, PosPoint(ctx));
	}
	if (gone.letGo)
	{
		SetObjectPtr(ctx, nullptr);
	}
}

namespace
{
/// The move to the goal just set, as LINEAR: the hug is not reset, the sweep drops the circle. hugGoalDistance = 0
/// always here; the original writes it only when the hug had an entry and leaves it stale otherwise, which nothing
/// observes: only ORBIT reads it, after MoveToCircleHug wrote it
void StartLinearWalk(Context& ctx)
{
	InitStepsXZ(ctx);
	wall_hug::ForgetLookaheadTrial(Locator::entitiesRegistry::value(), ctx.entity);
	ctx.brain.hugGoalDistance = 0;
	if (AreWeThere(ctx))
	{
		ctx.brain.moveState = k_MoveArrived;
		return;
	}
	LinearSquareSweep(ctx, PosPoint(ctx));
	SetMoveState(ctx, k_MoveLinear);
}
} // namespace

void SetupMoveToWithHug(Context& ctx, glm::vec2 p, AnimalState final)
{
	if (!SetCurrentAndDestinationState(ctx, final))
	{
		return;
	}
	walk_goal::Set(ctx.entity, ctx.brain.goal, p);
	StartLinearWalk(ctx);
}

void SetupMoveToWithHug(Context& ctx, glm::ivec2 units, AnimalState final)
{
	if (!SetCurrentAndDestinationState(ctx, final))
	{
		return;
	}
	SetGoalUnits(ctx, units);
	StartLinearWalk(ctx);
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

void ForgetLookahead(Context& ctx)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.RemoveState<components::WallHugLookaheadTried>(ctx.entity);
	registry.RemoveState<components::WallHugLookaheadPending>(ctx.entity);
}

wall_hug::TrialCopy TrialCopyOf(const Context& ctx)
{
	std::optional<components::WallHugObjectReference> circle;
	if (const auto& held = ctx.brain.hugCircle; held.set)
	{
		const bool landscape = held.owner == entt::null;
		const auto at = map_coords::FromMetres(held.centre);
		circle = components::WallHugObjectReference {
		    .entity = held.owner,
		    .circle = held.index,
		    .centre = held.centre,
		    .radius = held.radius,
		    .landscape = landscape,
		    .cell = landscape ? glm::ivec2(map_coords::CellOf(at.x), map_coords::CellOf(at.z)) : glm::ivec2(0),
		    .landscapeOrFence = landscape || held.landscapeOrFence};
	}
	return {.state = static_cast<wall_hug::WalkState>(ctx.brain.moveState),
	        .position = WalkUnitsOf(ctx),
	        .goal = GoalUnitsOf(ctx),
	        .goalAltitude = ctx.brain.goalAltitude,
	        .step = ctx.brain.step,
	        .stepAltitude = ctx.brain.stepAltitude,
	        .gameAngle = ctx.brain.angle,
	        .speed = ctx.brain.speed,
	        .turnsToObj = ctx.brain.turnsToObj,
	        .entryDistance = ctx.brain.hugGoalDistance,
	        .circle = circle};
}

void TurnRoundAfterTrial(Context& ctx)
{
	const auto turned = wall_hug::TurnRound({.state = static_cast<wall_hug::WalkState>(ctx.brain.moveState),
	                                         .angle = ctx.brain.angle,
	                                         .position = WalkUnitsOf(ctx),
	                                         .step = ctx.brain.step,
	                                         .stepAltitude = ctx.brain.stepAltitude});
	if (!turned.turned)
	{
		return;
	}
	SetMoveState(ctx, MoveStateOf(turned.pose.state));
	// MoveMapObject to where it lands, with no check of the place
	PlaceAtUnits(ctx, turned.pose.position);
	ctx.brain.step = turned.pose.step;
	ctx.brain.stepAltitude = turned.pose.stepAltitude;
	// the game angle alone, as the original writes it
	ctx.brain.angle = turned.pose.angle;
	if (turned.lookAlongCircle && HugObj(ctx) != nullptr)
	{
		CircleSquareSweep(ctx, PosPoint(ctx), turned.pose.state == wall_hug::WalkState::OrbitClockwise, 1);
	}
}

void FaceAlongCircleAfterTrial(Context& ctx)
{
	const auto* circle = HugObj(ctx);
	if (circle == nullptr)
	{
		// (openblack, guard) the original reads the circle unchecked; a walk round a circle always holds one
		return;
	}
	const auto centre = map_coords::FromMetres(circle->centre);
	const auto heading = wall_hug::HeadingWhenKept(static_cast<wall_hug::WalkState>(ctx.brain.moveState),
	                                               game_math::GetAngleFromXZ(WalkUnitsOf(ctx), glm::ivec2(centre.x, centre.z)));
	if (heading.has_value())
	{
		SetGameAngle(ctx, *heading);
		RebuildMoveByStep(ctx);
	}
}

} // namespace openblack::ecs::animal_ai::detail
