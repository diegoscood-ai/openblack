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

#include <memory>
#include <optional>

#include <glm/vec3.hpp>

#include "ECS/Weather/Storms.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"

// The storm miracle's particle systems (SF_LightningStormPush and SF_StormCast): the cloud cores drift with the wind
// (UR_CloudMoverNew), the clouds gather round each core and register a storm that rains on the land (UR_CloudGather),
// fork lightning strikes from random clouds (UR_Lightning of Rules/Lightning.cpp in its parent mode) and, at power-up
// level 1, a tornado sucks up objects and piles (UR_Tornado). UR_StormCast is the swirl at the hand while casting.
// See docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado.

namespace openblack::particles
{
struct Atom;
struct Collection;
} // namespace openblack::particles

namespace openblack::particles::storm
{

// ---- the formulas, exposed for the tests ----

/// What UR_CloudGather registers. `radius` is the RadiusFloatProvider value (1.2 x magnitude), `magnitude` the
/// manager's, `power` the process strength, `heading` the manager heading at the first update ((cameraForward.x, 0,
/// cameraForward.z), not normalised), `rain` the storm spell's rain amount (or < 0 without a storm spell: 100).
struct GatherStormInput
{
	float radius {0.0f};
	float magnitude {0.0f};
	float power {1.0f};
	glm::vec3 heading {0.0f};
	float rainAmount {-1.0f};
	bool rainOn {true};
	float timeToForm {10.0f};
	float cloudHeight {0.0f}; ///< the CloudHeight float provider value
	float windMinSpeed {40.0f};
	float windMaxSpeed {100.0f};
	float magnitudeForWindMinSpeed {20.0f};
	float magnitudeForWindMaxSpeed {100.0f};
};
[[nodiscard]] weather::storms::StormDescriptor GatherStormDescriptor(const GatherStormInput& input);

/// UR_Tornado: the funnel radius at the height fraction h (clamped 0..1),
/// (BaseRadius + (TopRadius - BaseRadius) h^2) x tornadoScale
[[nodiscard]] float FunnelRadius(float h, float baseRadius, float topRadius, float tornadoScale);
/// Schlick's bias: x^(ln b / ln 0.5), the logarithm and the power as the game's FPU and C runtime work them out
[[nodiscard]] float Bias(float b, float x);
/// Schlick's gain: x < 0.5 ? bias(1 - g, 2x) / 2 : 1 - bias(1 - g, 2 - 2x) / 2
[[nodiscard]] float Gain(float g, float x);
/// UR_Tornado: the flying atoms' angular speed at the height fraction h, lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias,
/// h)) x the atom's speed x the tornado's strength, with ThetaBias first kept between 0 and 1 (not a number gives 0)
[[nodiscard]] float ThetaDot(float h, float thetaBias, float baseThetaDot, float topThetaDot, float speed, float strength);
/// UR_Tornado's centre line: from the base to the top, bent in x and z by gain(FunnelBendParameter, h), with a wiggle
/// of TornadoScale x WiggleAmplitude whose phase is that gain x WiggleCount x pi
struct FunnelLine
{
	glm::vec3 base {0.0f};
	glm::vec3 top {0.0f};
	float bend {0.5f};
	int wiggleCount {5};
	float wiggleAmplitude {10.0f};
	float tornadoScale {1.0f};
};
/// The funnel's centre at the height fraction h (kept between 0 and 1)
[[nodiscard]] glm::vec3 FunnelCentre(float h, const FunnelLine& line);
/// UR_Tornado: a flying atom's next point round the funnel. `d` is the atom less the centre at its height, `rho2` its
/// new distance from the centre line, `r` the funnel's radius, `w` the spin at its height, `vy` its rise: the angle of
/// d about the centre line turns by the spin x dt (slowed by r / (rho2 + 0.1) outside the wall), at rho2 from the
/// centre, and the atom rises by vy x dt
[[nodiscard]] glm::vec3 OrbitPoint(glm::vec3 d, glm::vec3 centre, float rho2, float r, float w, float vy, float dt);
/// The midpoint (RK2) rate of d rho / dt = -0.5 (rho - r), the flying atoms' pull towards the
/// funnel wall
[[nodiscard]] float RadialRate(float r, float rho, float dt);
/// UR_CloudMoverNew's velocity after a step in the wind (the climate's smoothed wind at the core): towards the wind
/// times WindMagnification times 0.1, by WindDamping times the difference times dt
[[nodiscard]] glm::vec3 CloudWindVelocity(glm::vec3 velocity, glm::vec3 wind, float windMagnification, float windDamping,
                                          float dt);

/// UR_CloudGather, one cloud at its formed fraction f = age / TimeToForm
struct CloudLook
{
	float grow;  ///< FracToMaxSize ramp: f / Frac, then 1 - (f - Frac) / (1 - Frac)
	float ratio; ///< MaxCloudRatio + (MinCloudRatio - MaxCloudRatio) f
	int colour;  ///< trunc(MaxColor + (MinColor - MaxColor) f)
	int alpha;   ///< trunc(MinAlpha + (MaxAlpha - MinAlpha) grow)
	float scale; ///< (MinScaleFactor + (MaxScaleFactor - MinScaleFactor) grow) x the CloudScale float provider
};
struct CloudLookParams
{
	float fracToMaxSize {0.5f};
	float minCloudRatio {1.0f}, maxCloudRatio {5.0f};
	int minColor {50}, maxColor {255};
	int minAlpha {100}, maxAlpha {100};
	float minScaleFactor {0.0f}, maxScaleFactor {1.0f};
	float scaleProvider {1.0f};
};
[[nodiscard]] CloudLook CloudLookAt(float f, const CloudLookParams& params);

/// UR_CloudGather: the collection-age smoothstep s = (3 - 2t) t^2 with t = clamp(age x 0.1, 0, 1);
/// returns c + (1 - c) s (CloudRatioMaxCollection or CollectionRadiusInitialScale as c)
[[nodiscard]] float CollectionScale(float collectionAge, float c);

// ---- the game side ----

/// Every frame (magic::Update): the objects the tornados carry follow their atoms (each atom's draw matrix is copied
/// into its object's 3D object)
void UpdateCarriedObjects();
/// How many strikes the clouds have made since the program started (the test hooks)
[[nodiscard]] size_t StrikeCount();
/// How many objects the tornados carry now (the traces and tests)
[[nodiscard]] size_t CarriedObjectCount();
/// OPENBLACK_STORM_TRACE (read once)
[[nodiscard]] bool TraceEnabled();

/// What a tornado does with a thing within its reach, in the game's order: nothing when the thing is not available
/// (being deleted, or a villager that is dying), else asks to suck it up when it fits, else catches a creature, else
/// takes from a pile; nothing otherwise
enum class PickUpChoice : uint8_t
{
	Ask,
	Creature,
	Pile,
	Nothing,
};
[[nodiscard]] PickUpChoice PickUpBranch(bool available, bool fits, bool isCreature, bool isPile);
/// How far a tornado reaches for things: twice the mean of its radii at the bottom and the top, times its spell's
/// tribal power kept from 1 to 5 when it has a spell
[[nodiscard]] float PickUpReach(float bottomRadius, float topRadius, std::optional<float> tribalPower);
/// Whether a thing of this 2D radius fits a tornado whole: twice the funnel's radius at the bottom, and its radius at
/// the top, are both larger than it
[[nodiscard]] bool FitsTheFunnel(float objectRadius, float bottomRadius, float topRadius);
/// Whether a storm's drifting cores bounce off the shields they meet: only the tornado's (power-up level 1), not the
/// plain storm's (-1) nor the electric storm's (0)
[[nodiscard]] bool CoresBounceOffShields(int powerUpLevel);
/// A strike hangs the storm's lightning collection from `cloud`, whole, with every atom it holds: from the gather, which
/// owns it (`orphan`) while it hangs from no cloud, or from the cloud of `clouds` that holds it. False when neither has
/// it (its cloud went)
bool HandOverLightning(Collection& clouds, const Collection* lightning, std::unique_ptr<Collection>& orphan, Atom& cloud);
/// A strike's thunder: the storm's lightning sound with a size class by the draw r in [0, 1) (below 0.33 small, below
/// 0.66 medium, else large), delayed by its distance and heard from the land under the cloud
[[nodiscard]] SoundAction Thunder(SoundAction lightning, float r);
/// Where a tornado's flying atom goes this step
enum class FlyingAtomExit : uint8_t
{
	Stays,     ///< it keeps flying round the funnel
	CloseDown, ///< to the group for a closing tornado (or gone without one)
	Done,      ///< to the group for one that has risen to the top (or gone without one)
};
/// A carried object (state 0) or a pretend one (1) leaves the funnel when the tornado closes, else once it has risen
/// past 0.9 of the funnel's height h (0 at the base, 1 at the top); the funnel's own sprites (2) never leave
[[nodiscard]] FlyingAtomExit FlyingAtomLeaves(int state, bool closing, float h);
/// The event a tornado sends at its base every step while it is not closing: a point event with the base's velocity,
/// strength 1, no shields and no target; none once it closes
[[nodiscard]] std::optional<SpellEventInfo> BaseEvent(const glm::vec3& base, const glm::vec3& baseVelocity, bool closing);

} // namespace openblack::particles::storm
