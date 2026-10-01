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

#include <glm/vec3.hpp>

#include "ECS/Weather/Storms.h"

// The storm miracle's classes (SF_LightningStormPush and SF_StormCast; PSysTornado.cpp 0x6D15D0-0x6D6160): the cloud
// cores drift with the wind (UR_CloudMoverNew), the clouds gather round each core and register an LH3DStorm that rains
// on the land (UR_CloudGather), fork lightning strikes from random clouds (UR_Lightning of Rules/Lightning.cpp in its
// parent mode) and, at power-up level 1, a tornado sucks up objects and piles (UR_Tornado). UR_StormCast is the swirl at
// the hand while casting. Research: tmp_dis\miracles\impl\m6st\ (the disassembly read for this port); wiki:
// docs/bw1-notes/magic.md, "Tormenta".

namespace openblack::psys::storm
{

// ---- the formulas, exposed for the tests ----

/// fn_006D5730: what UR_CloudGather registers. `radius` is the RadiusFloatProvider value (1.2 x magnitude), `magnitude`
/// the manager's (+0xA0), `power` the PSysProcessInfo strength (+0x30 = manager +0x54), `heading` the manager heading
/// at the first update (GetCurrentHeading 0x673660: (cameraForward.x, 0, cameraForward.z), not normalised), `rain` the
/// spell's GMagicStormAndTornadoInfo::rainAmount (or < 0 without a storm spell: 100), `rainOn` +0x46.
struct GatherStormInput
{
	float radius {0.0f};
	float magnitude {0.0f};
	float power {1.0f};
	glm::vec3 heading {0.0f};
	float rainAmount {-1.0f};
	bool rainOn {true};
	float timeToForm {10.0f};  ///< +0x38
	float cloudHeight {0.0f};  ///< the CloudHeight float provider value (+0xA0)
	float windMinSpeed {40.0f};             ///< +0x8C
	float windMaxSpeed {100.0f};            ///< +0x88
	float magnitudeForWindMinSpeed {20.0f}; ///< +0x94
	float magnitudeForWindMaxSpeed {100.0f}; ///< +0x90
};
[[nodiscard]] weather::storms::StormDescriptor GatherStormDescriptor(const GatherStormInput& input);

/// UR_Tornado fn_006D2790: the funnel radius at the height fraction h (clamped 0..1),
/// (BaseRadius + (TopRadius - BaseRadius) h^2) x tornadoScale
[[nodiscard]] float FunnelRadius(float h, float baseRadius, float topRadius, float tornadoScale);
/// Schlick's bias (fn_006D2710 / fn_006D2910): x^(ln b / ln 0.5), the exponent through 0xD4EEA0 = 1 / ln 0.5
/// (crt_xc_fn_PSysTornado_006D1660)
[[nodiscard]] float Bias(float b, float x);
/// Schlick's gain (fn_006D2910): x < 0.5 ? bias(1 - g, 2x) / 2 : 1 - bias(1 - g, 2 - 2x) / 2
[[nodiscard]] float Gain(float g, float x);
/// fn_006D28B0 (r, rho, dt): the midpoint (RK2) rate of d rho / dt = -0.5 (rho - r), the flying atoms' pull towards the
/// funnel wall
[[nodiscard]] float RadialRate(float r, float rho, float dt);

/// UR_CloudGather 0x6D4F79..0x6D5283, one cloud at its formed fraction f = age / TimeToForm
struct CloudLook
{
	float grow;   ///< FracToMaxSize ramp: f / Frac, then 1 - (f - Frac) / (1 - Frac)
	float ratio;  ///< MaxCloudRatio + (MinCloudRatio - MaxCloudRatio) f
	int colour;   ///< ftol(MaxColor + (MinColor - MaxColor) f)
	int alpha;    ///< ftol(MinAlpha + (MaxAlpha - MinAlpha) grow)
	float scale;  ///< (MinScaleFactor + (MaxScaleFactor - MinScaleFactor) grow) x the CloudScale float provider
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

/// UR_CloudGather 0x6D4D68..0x6D4E23: the collection-age smoothstep s = (3 - 2t) t^2 with t = clamp(age x 0.1, 0, 1);
/// returns c + (1 - c) s (CloudRatioMaxCollection or CollectionRadiusInitialScale as c)
[[nodiscard]] float CollectionScale(float collectionAge, float c);

// ---- the game side ----

/// Every frame (magic::Update): the objects the tornados carry follow their atoms (RenderParticleGameObject::DrawAt
/// 0x67B170 copies the atom's draw matrix into the object's 3D object)
void UpdateCarriedObjects();
/// How many strikes the clouds have made since the program started (the test hooks)
[[nodiscard]] size_t StrikeCount();
/// How many objects the tornados carry now (the traces and tests)
[[nodiscard]] size_t CarriedObjectCount();
/// OPENBLACK_STORM_TRACE (read once)
[[nodiscard]] bool TraceEnabled();

} // namespace openblack::psys::storm
