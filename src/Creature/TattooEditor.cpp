/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooEditor.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <ranges>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_tattoo_editor;

namespace
{
/// The view's speeds are in radians a 0.3 seconds' worth, and die away by e to the power of 8 a 0.3 seconds' worth
constexpr float k_TurnScale = 0.0003f;
constexpr float k_SpeedDecay = -8.0f;
/// How far the pointer from where the rotation button was pressed speeds it up, a millisecond
constexpr float k_SteerScale = 0.001f;
constexpr float k_TurnSteer = 0.2f;
constexpr float k_TipSteer = 0.1f;
/// The lowest the view goes: an eighth as high above the focus as it is far from it across the ground
constexpr float k_LowestTip = 0.125f;
/// A place's centre is in texels of a skin 256 wide
constexpr double k_TexelWidth = 1.0 / 256.0;

/// A vector of length 1 the same way, or the vector as it is when it has no length
glm::dvec3 NormalisedUnlessZero(const glm::dvec3& vector)
{
	if (vector == glm::dvec3(0.0))
	{
		return vector;
	}
	return vector * (1.0 / std::sqrt(glm::dot(vector, vector)));
}

/// A pixel's coordinate cut towards zero; one too far for an int is the int's lowest, as the game's conversion gives
int PixelOf(float value)
{
	constexpr auto k_Limit = 2147483648.0f;
	if (!(value > -k_Limit && value < k_Limit))
	{
		return std::numeric_limits<int>::min();
	}
	return static_cast<int>(value);
}
} // namespace

Session creature_tattoo_editor::Open(const creature_tattoo::Slots& slots)
{
	return {.slots = slots, .opened = slots, .changed = false};
}

bool creature_tattoo_editor::Drop(Session& session, uint8_t design, const glm::u8vec3& colour, std::optional<uint8_t> site)
{
	if (!site.has_value() || *site >= creature_tattoo::k_SlotCount)
	{
		return false;
	}
	session.changed = true;
	auto& slots = session.slots;
	const auto found =
	    std::ranges::find_if(slots, [&](const auto& slot) { return slot.site == *site && slot.design == design; });
	if (found != slots.end())
	{
		// The same symbol again on the same place only takes the new colour
		found->colour = colour;
		return false;
	}
	const auto slot = creature_tattoo::SlotFor(slots, *site, design);
	if (!slot.has_value())
	{
		return false;
	}
	slots.at(*slot) = {.design = design, .site = *site, .colour = colour};
	return true;
}

std::optional<creature_tattoo::Slot> creature_tattoo_editor::Lift(Session& session, uint8_t site)
{
	auto& slots = session.slots;
	const auto found =
	    std::ranges::find_if(slots | std::views::reverse, [site](const auto& slot) { return slot.site == site; });
	if (found == (slots | std::views::reverse).end())
	{
		return std::nullopt;
	}
	auto lifted = *found;
	found->site = creature_tattoo::k_NoSite;
	session.changed = true;
	return lifted;
}

Accept creature_tattoo_editor::Ok(const Session& session, bool networkGame)
{
	return networkGame && session.changed ? Accept::Warn : Accept::Close;
}

glm::uvec2 creature_tattoo_editor::PaletteCell(glm::ivec2 point, const PickerRect& rect)
{
	const auto cell = [](int offset, int length, int count) {
		const auto value = (offset * count) / length;
		// The first is taken below one, the last above the one before it
		if (value < 1)
		{
			return 0u;
		}
		return static_cast<uint32_t>(std::min(value, count - 1));
	};
	return {cell(point.x - rect.min.x, rect.max.x - rect.min.x, static_cast<int>(creature_tattoo::k_PaletteColumns)),
	        cell(point.y - rect.min.y, rect.max.y - rect.min.y, static_cast<int>(creature_tattoo::k_PaletteRows))};
}

float creature_tattoo_editor::SliderPosition(int y, const PickerRect& rect)
{
	const auto position = static_cast<float>(y - rect.min.y) / static_cast<float>(rect.max.y - rect.min.y);
	return position > 0.0f ? std::min(position, 1.0f) : 0.0f;
}

std::optional<uint8_t> creature_tattoo_editor::SiteUnderPointer(std::span<const SiteOnScreen> sites, glm::ivec2 pointer)
{
	std::optional<uint8_t> nearest;
	int best = 0;
	for (const auto& site : sites)
	{
		if (site.facingCamera <= k_FacingLimit)
		{
			continue;
		}
		const auto offset = site.screen - pointer;
		const auto distance = (offset.x * offset.x) + (offset.y * offset.y);
		if (!nearest.has_value() || distance < best)
		{
			best = distance;
			nearest = site.site;
		}
	}
	if (nearest.has_value() && best > k_PickDistanceSquared)
	{
		return std::nullopt;
	}
	return nearest;
}

std::optional<SiteTriangle> creature_tattoo_editor::FindSiteTriangle(std::span<const TexturedPrimitive> primitives,
                                                                     const creature_tattoo::Site& site)
{
	// The differences of the coordinates are floats; the rest is worked out more precisely, in double, as the game
	// works it out, before u and v are kept as floats
	const auto centreU = static_cast<double>(site.u) * k_TexelWidth;
	const auto centreV = static_cast<double>(site.v) * k_TexelWidth;
	for (size_t p = 0; p < primitives.size(); ++p)
	{
		const auto& primitive = primitives[p];
		if (primitive.skin != site.skin)
		{
			continue;
		}
		const auto& vertices = primitive.vertices;
		for (const auto& triangle : primitive.triangles)
		{
			if (std::ranges::any_of(triangle, [&vertices](uint16_t vertex) { return vertex >= vertices.size(); }))
			{
				continue;
			}
			const auto first = vertices[triangle[0]].uv;
			const auto toSecond = vertices[triangle[1]].uv - first;
			const auto toThird = vertices[triangle[2]].uv - first;
			const auto determinant = (static_cast<double>(toThird.x) * static_cast<double>(toSecond.y)) -
			                         (static_cast<double>(toThird.y) * static_cast<double>(toSecond.x));
			if (std::abs(determinant) <= static_cast<double>(k_FlatOnSkin))
			{
				continue;
			}
			const auto divisor = static_cast<double>(static_cast<float>(determinant));
			const auto offsetU = centreU - static_cast<double>(first.x);
			const auto offsetV = centreV - static_cast<double>(first.y);
			const auto u = static_cast<float>(
			    ((offsetV * static_cast<double>(toThird.x)) - (offsetU * static_cast<double>(toThird.y))) / divisor);
			const auto v = static_cast<float>(
			    ((offsetU * static_cast<double>(toSecond.y)) - (offsetV * static_cast<double>(toSecond.x))) / divisor);
			if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && static_cast<double>(u) + static_cast<double>(v) <= 1.0)
			{
				return SiteTriangle {
				    .primitive = p,
				    .vertices = triangle,
				    .bones = {vertices[triangle[0]].bone, vertices[triangle[1]].bone, vertices[triangle[2]].bone},
				    .u = u,
				    .v = v,
				};
			}
		}
	}
	return std::nullopt;
}

std::optional<glm::ivec2> creature_tattoo_editor::ProjectToPixels(const glm::mat4& worldToClip, float nearPlane,
                                                                  glm::ivec2 screenSize, const glm::vec3& point)
{
	const auto clip = worldToClip * glm::vec4(point, 1.0f);
	if (!(clip.w >= nearPlane))
	{
		return std::nullopt;
	}
	const auto inverse = 1.0f / clip.w;
	const auto half = glm::vec2(screenSize) * 0.5f;
	return glm::ivec2(PixelOf(((clip.x * inverse) + 1.0f) * half.x), PixelOf((1.0f - (clip.y * inverse)) * half.y));
}

std::vector<SiteOnScreen> creature_tattoo_editor::SitesOnScreen(std::span<const SitePoint> points, const SiteView& view)
{
	std::vector<SiteOnScreen> sites;
	for (const auto& point : points)
	{
		const auto pixels = ProjectToPixels(view.worldToClip, view.nearPlane, view.screenSize, point.position);
		if (!pixels.has_value())
		{
			continue;
		}
		// The way from the eye is worked out in double, as the game does it at the x87's precision; the normal is
		// kept as floats
		const auto fromEye = NormalisedUnlessZero(glm::dvec3(point.position) - glm::dvec3(view.eye));
		const auto inward = glm::vec3(NormalisedUnlessZero(glm::dvec3(point.inwardNormal)));
		sites.push_back({
		    .site = point.site,
		    .screen = *pixels,
		    .facingCamera = static_cast<float>(glm::dot(glm::dvec3(inward), fromEye)),
		});
	}
	return sites;
}

glm::vec3 creature_tattoo_editor::CavePlace(const glm::vec3& first, const glm::vec3& third, float size, bool coin)
{
	auto along = (2.0f - size) * 0.5f;
	if (coin && size < k_CaveCoinBelowSize)
	{
		along = (along - 0.5f) + (along - 0.5f);
	}
	if (along <= 0.0f)
	{
		along = 0.0f;
	}
	else if (along >= 1.0f)
	{
		along = 1.0f;
	}
	return first + ((third - first) * along);
}

Orbit creature_tattoo_editor::CaveView(const glm::vec3& creature, float scale)
{
	auto reach = scale * 15.0f;
	if (reach <= 1.0f)
	{
		reach = 1.0f;
	}
	else if (reach >= 20.0f)
	{
		reach = 20.0f;
	}
	return {.eye = creature + glm::vec3(reach * -1.5f, reach * 0.75f, 0.0f),
	        .focus = creature + glm::vec3(0.0f, reach * 0.5f, 0.0f)};
}

void creature_tattoo_editor::DrawCopyMoment(float alignment, int32_t bones, const std::function<uint32_t(int32_t)>& localRand)
{
	// a moment of the copy's time: its alignment, held between -1 and 1 (one that is not a number as -1), makes the
	// sparkles of a good or an evil creature, each a place on the body, a bone and a colour, and a good one's kind. They
	// are not shown yet, but their draws are made
	auto shown = alignment;
	if (!(shown > -1.0f))
	{
		shown = -1.0f;
	}
	else if (!(shown < 1.0f))
	{
		shown = 1.0f;
	}
	constexpr int k_Sparkles = 3;
	const bool good = shown > 0.2f;
	const bool evil = !good && shown < -0.2f;
	for (int i = 0; (good || evil) && i < k_Sparkles; ++i)
	{
		for (const int32_t range : {101, 101, 101, bones, 6})
		{
			static_cast<void>(localRand(range));
		}
		if (good)
		{
			static_cast<void>(localRand(2));
		}
	}
}

CopyRest creature_tattoo_editor::DrawCopyRest(float size, float alignment, int32_t bones,
                                              const std::function<uint32_t(int32_t)>& localRand)
{
	CopyRest rest;
	// the coin is always tossed, and kept only for a creature smaller than 1.25
	rest.sleeping = localRand(2) != 0;
	if (size >= k_CaveCoinBelowSize)
	{
		rest.sleeping = false;
	}
	DrawCopyMoment(alignment, bones, localRand);
	// the rest's own flag is tossed after it, sleeping or sitting
	rest.restFlag = localRand(2) != 0;
	// and once it faces the fire, another moment
	DrawCopyMoment(alignment, bones, localRand);
	return rest;
}

uint32_t creature_tattoo_editor::CopyFrameMs(uint32_t frameRealMs)
{
	return std::min(frameRealMs, k_CopyMaxFrameMs);
}

std::array<size_t, 3> creature_tattoo_editor::CopyRestAnimations(bool sleeping)
{
	// the rest is the action around its loop: the one before starts it and the one after ends it
	const auto loop = sleeping ? creature_layers::animations::k_Sleep : creature_layers::animations::k_Sit;
	return {loop - 1, loop, loop + 1};
}

bool creature_tattoo_editor::CopyStartsYawn(bool sleeping, bool reflection, const std::function<uint32_t(int32_t)>& localRand)
{
	constexpr int32_t k_YawnOneIn = 1000;
	constexpr uint32_t k_YawnDraw = 23;
	return !reflection && !sleeping && localRand(k_YawnOneIn) == k_YawnDraw;
}

CopyTurn creature_tattoo_editor::StartCopyTurn(float heading, float target,
                                               const std::function<std::optional<int32_t>(size_t)>& durationMs)
{
	constexpr float k_QuarterTurn = 1.57079637f;
	constexpr float k_PerQuarterTurn = 0.636619747f;
	CopyTurn turn {.target = target};
	// the angle to turn by, within half a turn; the heading's way round plays the one side's spins, else the other's, by
	// the angle turned
	auto angle = affine::WrapAngle(target - heading);
	const auto first = angle > 0.0f ? k_CopySpinsAround : k_CopySpinsBack;
	if (first == k_CopySpinsBack)
	{
		angle = -angle;
	}
	const std::array lengths {durationMs(first), durationMs(first + 1), durationMs(first + 2)};
	if (std::ranges::any_of(lengths, [](const auto& length) { return !length.has_value(); }))
	{
		return turn;
	}
	// up to a quarter turn the first two, beyond it the last two; a turn that is not a number takes the first two
	const bool beyond = angle >= k_QuarterTurn;
	const auto from = beyond ? first + 1 : first;
	const auto weight = beyond ? (angle - k_QuarterTurn) * k_PerQuarterTurn : angle * k_PerQuarterTurn;
	const auto fromMs = *lengths.at(from - first);
	const auto toMs = *lengths.at(from - first + 1);
	// the blend's length, worked out in floats and rounded to the nearest
	const auto blended = (static_cast<float>(toMs - fromMs) * weight) + static_cast<float>(fromMs);
	turn.spins = CopyTurn::Spins {
	    .from = from, .to = from + 1, .weight = weight, .durationMs = static_cast<int32_t>(std::nearbyint(blended))};
	return turn;
}

void creature_tattoo_editor::StepCopyTurn(std::optional<CopyTurn>& turn, float& heading, int32_t step, int32_t standMs)
{
	if (!turn.has_value())
	{
		return;
	}
	constexpr double k_Done = 1e-4f;
	// it is over once it faces the turn's way (or the difference is not a number)
	if (!(static_cast<double>(std::abs(heading - turn->target)) >= k_Done))
	{
		heading = turn->target;
		turn.reset();
		return;
	}
	if (turn->spins.has_value())
	{
		// the heading stays as it was until the spins end
		turn->timeMs += step;
		if (turn->timeMs >= turn->spins->durationMs)
		{
			heading = turn->target;
			turn->timeMs = 0;
			turn->spins.reset();
		}
		return;
	}
	// without spins: the share of the stand's length gone so far, of what is left of the turn
	if (standMs > 0)
	{
		const auto left = affine::WrapAngle(turn->target - heading);
		const auto share = (static_cast<float>(turn->timeMs) * left) / static_cast<float>(standMs);
		heading = affine::WrapAngle(share + heading);
	}
	turn->timeMs += step;
	if (turn->timeMs >= standMs)
	{
		heading = turn->target;
		turn->timeMs = 0;
	}
}

int32_t creature_tattoo_editor::CopyAnimationStep(float size, uint32_t milliseconds)
{
	constexpr float k_Rate = 1.6f;
	constexpr float k_SlowingPerSize = 0.85f;
	// each a float, as the original works them out, then cut towards zero
	const auto slowing = (size * 0.5f) * k_SlowingPerSize;
	return static_cast<int32_t>((k_Rate - slowing) * static_cast<float>(milliseconds));
}

CopyStepped creature_tattoo_editor::CopyStep(CopyLife& life, creature_layers::BodyAction& body,
                                             creature_layers::GestureLayer& gesture, std::optional<glm::vec3>& lookAt,
                                             const CopyFrame& frame, const std::function<bool(size_t)>& hasAnimation,
                                             const std::function<uint32_t(int32_t)>& localRand)
{
	CopyStepped stepped;
	const auto busy = [&body, &frame] { return creature_layers::IsPlaying(body) || frame.turning; };
	// its rest, or the sit, while the body is free and the species has the three animations
	const auto rest = [&](bool sleeping, bool flag) {
		const auto animations = CopyRestAnimations(sleeping);
		if (!busy() && std::ranges::all_of(animations, hasAnimation))
		{
			if (const auto resting = creature_layers::PlaySequence(body, animations[0], animations[1], animations[2]))
			{
				body = *resting;
				stepped.restFlag = flag;
			}
		}
	};
	const auto nextWait = [&localRand] { return static_cast<int32_t>(localRand(k_CopyIdleWaitMs)) + k_CopyIdleWaitMs; };
	if (frame.editorOpen)
	{
		// the editor's first frame with it: it gets up, to turn to face the editor's eye; then the editor, shown, starts
		// the wait for its idle actions
		if (!life.editorHadCopy)
		{
			body = creature_layers::EndLoop(body);
			life.turnWanted = true;
			life.turnHeading = static_cast<float>(affine::GetYAngle(frame.eye - frame.focus));
			life.idleMs = nextWait();
			life.idles = 0;
		}
		life.editorHadCopy = true;
		// its idle actions, by the frame's time, an action's flag drawn whether or not it starts
		life.idleMs -= frame.frameMs;
		if (life.idleMs <= 0)
		{
			const auto once = [&](size_t animation) {
				static_cast<void>(localRand(2));
				if (!busy() && hasAnimation(animation))
				{
					body = creature_layers::PlayOnce(body, animation, false).value_or(body);
				}
			};
			++life.idles;
			if (life.idles < k_CopyIdlesBeforeSitting)
			{
				switch (localRand(3))
				{
				case 0:
					once(k_CopyIdleShowOff);
					break;
				case 1:
					once(k_CopyIdleScratch);
					break;
				case 2:
					if (hasAnimation(creature_layers::animations::k_Yawn))
					{
						gesture = creature_layers::PlayGesture(gesture, creature_layers::animations::k_Yawn).value_or(gesture);
					}
					break;
				default:
					break;
				}
			}
			else if (!busy())
			{
				rest(false, localRand(2) != 0);
			}
			life.idleMs = nextWait();
		}
		// the left button starts the wait again and ends its rest
		if (frame.leftHeld)
		{
			life.idleMs = nextWait();
			life.idles = 0;
			if (busy())
			{
				body = creature_layers::EndLoop(body);
			}
		}
		// a button held: it looks at the pointer for the first 1.5 s of the system's time, then nowhere
		if (!frame.buttonHeld)
		{
			life.heldSince = 0;
			lookAt.reset();
		}
		else
		{
			if (life.heldSince == 0)
			{
				life.heldSince = frame.ticks;
			}
			if (frame.ticks - life.heldSince >= k_CopyLooksForMs)
			{
				lookAt.reset();
			}
			else
			{
				lookAt = frame.pointer;
			}
		}
	}
	else
	{
		life.editorHadCopy = false;
		// it faces the fire: then it rests, its flag drawn every such frame, and sitting looks into the fire
		life.turnHeading = static_cast<float>(affine::GetYAngle(frame.fire - frame.place));
		if (!(std::abs(life.turnHeading - frame.heading) >= k_CopyFacingWithin))
		{
			rest(frame.sleeping, localRand(2) != 0);
			if (!frame.sleeping)
			{
				lookAt = frame.fire;
			}
		}
		else
		{
			// else it gets up, to turn to the fire
			body = creature_layers::EndLoop(body);
			life.turnWanted = true;
		}
	}
	// a turn waits for the body to be free
	if (life.turnWanted && !busy())
	{
		life.turnWanted = false;
		stepped.turn = life.turnHeading;
	}
	return stepped;
}

uint32_t creature_tattoo_editor::CopyColour(uint32_t citadelColour, float depth, const TempleFade& fade)
{
	// the temple's light on every channel and the alpha, each a byte times another over 256, then the fade
	uint32_t lit = 0;
	for (const uint32_t shift : {0u, 8u, 16u, 24u})
	{
		lit |= ((((k_CopyColour >> shift) & 0xFFu) * ((citadelColour >> shift) & 0xFFu)) >> 8) << shift;
	}
	return temple_fade::Apply(lit, depth, fade);
}

float creature_tattoo_editor::CopyHeading(const glm::vec3& place, const glm::vec3& fire)
{
	// the way to the fire is taken as a float, then kept within half a turn
	const auto heading = static_cast<float>(affine::GetYAngle(fire - place));
	return affine::WrapAngle(heading);
}

void creature_tattoo_editor::Steer(Orbit& orbit, glm::ivec2 grab, glm::ivec2 pointer, float milliseconds)
{
	const auto scale = milliseconds * k_SteerScale;
	orbit.turnSpeed += static_cast<float>(grab.x - pointer.x) * scale * k_TurnSteer;
	orbit.tipSpeed -= static_cast<float>(grab.y - pointer.y) * scale * k_TipSteer;
}

void creature_tattoo_editor::Turn(Orbit& orbit, float milliseconds)
{
	const auto step = milliseconds * k_TurnScale;
	const auto offset = orbit.eye - orbit.focus;
	const auto angle = orbit.turnSpeed * step;
	const auto cosine = std::cos(angle);
	const auto sine = std::sin(angle);
	const auto across = (cosine * offset.x) - (sine * offset.z);
	const auto along = (cosine * offset.z) + (sine * offset.x);
	const auto distance = std::sqrt((across * across) + (along * along));
	const auto wanted = (orbit.tipSpeed * distance * step) + offset.y;
	auto height = distance * k_LowestTip;
	if (height < wanted)
	{
		height = std::min(wanted, distance);
	}
	const auto decay = std::exp(step * k_SpeedDecay);
	orbit.turnSpeed *= decay;
	orbit.tipSpeed *= decay;
	orbit.eye = orbit.focus + glm::vec3(across, height, along);
}
