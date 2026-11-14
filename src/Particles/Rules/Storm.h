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

// The storm miracle's particle systems (the storm and its cast at the hand): the cloud cores drift with the wind
// (the cloud mover), the clouds gather round each core and register a storm that rains on the land (cloud gather),
// fork lightning strikes from random clouds (the lightning rule of Rules/Lightning.cpp in its parent mode) and, at power-up
// level 1, a tornado sucks up objects and piles (the tornado). The storm cast is the swirl at the hand while casting.
// See docs/wiki/magic/storm-and-tornado.md#storm-electric-storm-and-tornado.

namespace openblack::particles
{
struct Atom;
struct Collection;
class Modifier;
} // namespace openblack::particles

namespace openblack::particles::storm
{

// ---- the formulas, exposed for the tests ----

/// What the cloud gather registers. `radius` is the RadiusFloatProvider value (1.2 x magnitude), `magnitude` the
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

/// The tornado's dust colour of the ground at a point, as 0xFFRRGGBB: the terrain material of its cell (snow where the
/// snow lying is at least 27, else the land's, ecs::terrain_material::AtCell), a material past the table's last row reading as
/// its first
[[nodiscard]] uint32_t TornadoDustColour(const glm::vec3& point);

/// The tornado: the funnel radius at the height fraction h (clamped 0..1),
/// (BaseRadius + (TopRadius - BaseRadius) h^2) x tornadoScale
[[nodiscard]] float FunnelRadius(float h, float baseRadius, float topRadius, float tornadoScale);
/// Schlick's bias: x^(ln b / ln 0.5), the logarithm and the power as the game's FPU and C runtime work them out
[[nodiscard]] float Bias(float b, float x);
/// Schlick's gain: x < 0.5 ? bias(1 - g, 2x) / 2 : 1 - bias(1 - g, 2 - 2x) / 2
[[nodiscard]] float Gain(float g, float x);
/// The tornado: the flying atoms' angular speed at the height fraction h, lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias,
/// h)) x the atom's speed x the tornado's strength, with ThetaBias first kept between 0 and 1 (not a number gives 0)
[[nodiscard]] float ThetaDot(float h, float thetaBias, float baseThetaDot, float topThetaDot, float speed, float strength);
/// The tornado's centre line: from the base to the top, bent in x and z by gain(FunnelBendParameter, h), with a wiggle
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
/// The tornado: a flying atom's next point round the funnel. `d` is the atom less the centre at its height, `rho2` its
/// new distance from the centre line, `r` the funnel's radius, `w` the spin at its height, `vy` its rise: the angle of
/// d about the centre line turns by the spin x dt (slowed by r / (rho2 + 0.1) outside the wall), at rho2 from the
/// centre, and the atom rises by vy x dt
[[nodiscard]] glm::vec3 OrbitPoint(glm::vec3 d, glm::vec3 centre, float rho2, float r, float w, float vy, float dt);
/// The midpoint (RK2) rate of d rho / dt = -0.5 (rho - r), the flying atoms' pull towards the
/// funnel wall
[[nodiscard]] float RadialRate(float r, float rho, float dt);
/// The cloud mover's velocity after a step in the wind (the climate's smoothed wind at the core): towards the wind
/// times WindMagnification times 0.1, by WindDamping times the difference times dt
[[nodiscard]] glm::vec3 CloudWindVelocity(glm::vec3 velocity, glm::vec3 wind, float windMagnification, float windDamping,
                                          float dt);

/// The cloud gather, one cloud at its formed fraction f = age / TimeToForm
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

/// The cloud gather: the collection-age smoothstep s = (3 - 2t) t^2 with t = clamp(age x 0.1, 0, 1);
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

// ---- the rules' data, as a save writes it ----

/// What the cloud gather keeps on each cloud: its radius and angle round the gather, whether its lightning lights it and
/// its age when that began, and its height above the others
struct CloudData
{
	float radius {0.0f};
	float theta {0.0f};
	bool flashing {false};
	float flashStart {0.0f};
	float height {0.0f};
};

/// What the cloud gather keeps on its collection, its references aside: the heading the storm moves along, the clouds
/// due (a fraction) and made, whether its first update is still to come, the clouds' rate, the collection's ages of
/// the next strike and of the strike's end, the clouds' spin, whether its lightning collection hangs from no cloud, and
/// whether it has lightning, rain and the weather storm
struct CloudGatherValues
{
	glm::vec3 heading {0.0f};
	float emitted {0.0f};
	int count {0};
	bool first {true};
	float rate {0.0f};
	float nextStrike {0.0f};
	float strikeEnd {0.0f};
	float spin {1.0f};
	bool detached {false};
	bool lightningOn {false};
	bool rainOn {false};
	bool registered {false};
};

/// The cloud gather's data on its collection, if the rule is the cloud gather and has made it there
[[nodiscard]] std::optional<CloudGatherValues> CloudGatherOf(const Collection& collection, const Modifier& rule);
/// Its lightning collection (none before the first strike) and the cloud that collection hangs from (none between
/// strikes)
[[nodiscard]] const Collection* LightningOf(const Collection& collection, const Modifier& rule);
[[nodiscard]] const Atom* LightningCloudOf(const Collection& collection, const Modifier& rule);
/// Its lightning collection while it hangs from no cloud, which its data then owns
[[nodiscard]] const Collection* DetachedLightningOf(const Collection& collection, const Modifier& rule);

/// For a load: the cloud gather's data on its collection as the save kept it, noted as made, with its lightning
/// collection (owned by the data when `detached` holds it) and the cloud it hangs from; false, with nothing changed,
/// for a rule of another class
bool RestoreCloudGather(Collection& collection, const Modifier& rule, const CloudGatherValues& values, Collection* lightning,
                        Atom* lightningCloud, std::unique_ptr<Collection> detached);

/// The cloud gather's data on a cloud, if the rule is the cloud gather and has made it there
[[nodiscard]] const CloudData* CloudOf(const Atom& atom, const Modifier& rule);

/// For a load: the cloud gather's data on a cloud as the save kept it, noted as made; false, with nothing changed, for a
/// rule of another class
bool RestoreCloud(Atom& atom, const Modifier& rule, const CloudData& data);

/// Whether the cloud mover's first call, which puts every atom at the effect's origin, is still to come; none for a
/// rule of another class or that has not made its data on the collection
[[nodiscard]] std::optional<bool> CloudMoverFirstOf(const Collection& collection, const Modifier& rule);

/// For a load: the cloud mover's data on its collection as the save kept it, noted as made; false, with nothing
/// changed, for a rule of another class
bool RestoreCloudMoverFirst(Collection& collection, const Modifier& rule, bool first);

/// What the tornado keeps on its collection: whether its first update is still to come, its base and top and their
/// velocities, the TornadoScale provider's value (1 without one) and the strength (1 every step), the alpha it gives
/// its collections, the close-down fade and whether it is closing down. The original keeps one more point there, which
/// it never sets after making it (0, 0, 0)
struct TornadoData
{
	bool first {true};
	glm::vec3 base {0.0f};
	glm::vec3 top {0.0f};
	glm::vec3 baseVelocity {0.0f};
	glm::vec3 topVelocity {0.0f};
	float tornadoScale {1.0f};
	float strength {1.0f};
	uint8_t alpha {0};
	float fade {1.0f};
	bool closing {false};

	bool operator==(const TornadoData&) const = default;
};

/// What the tornado keeps on each of its debris and flying collections: the atoms made and the atoms due (a fraction)
struct TornadoEmitterData
{
	float made {0.0f};
	float owed {0.0f};

	bool operator==(const TornadoEmitterData&) const = default;
};

/// What the tornado keeps on each flying atom: its state (0 a game object, 1 a pretend object, 2 a funnel sprite), the
/// height fraction it rises to and its spin's scale
struct TornadoFlyingData
{
	int state {2};
	float blend {0.0f};
	float speed {0.0f};

	bool operator==(const TornadoFlyingData&) const = default;
};

/// The tornado's data on its collection, on one of its debris and flying collections, or on a flying atom, if the rule
/// is the tornado and has made it there
[[nodiscard]] std::optional<TornadoData> TornadoOf(const Collection& collection, const Modifier& rule);
[[nodiscard]] std::optional<TornadoEmitterData> TornadoEmitterOf(const Collection& collection, const Modifier& rule);
[[nodiscard]] std::optional<TornadoFlyingData> TornadoFlyingOf(const Atom& atom, const Modifier& rule);

/// For a load: the tornado's data as the save kept it, noted as made; false, with nothing changed, for a rule of
/// another class
bool RestoreTornado(Collection& collection, const Modifier& rule, const TornadoData& data);
bool RestoreTornadoEmitter(Collection& collection, const Modifier& rule, const TornadoEmitterData& data);
bool RestoreTornadoFlying(Atom& atom, const Modifier& rule, const TornadoFlyingData& data);

} // namespace openblack::particles::storm
