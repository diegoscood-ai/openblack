/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHighlight.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "CHLApi.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/DontDraw.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/ObjectDraw.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ZSort.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::script_highlight
{
using components::Mesh;
using components::Transform;
using HighlightComponent = components::ScriptHighlight;

namespace
{
/// The did-you-know bubble (Locator::tipBubbleSystem)
systems::TipBubbleSystemInterface& TipBubbles()
{
	if (!Locator::tipBubbleSystem::has_value())
	{
		std::fputs("script_highlight: no tip bubble in the locator (Locator::tipBubbleSystem)\n", stderr);
		std::abort();
	}
	return Locator::tipBubbleSystem::value();
}

/// The turning highlight's radians per game ms (pi / 1000)
constexpr float k_SpinPerMs = 0.00314159f;
/// 1 / 2 pi and 2 pi, at the original's precision
constexpr float k_InvTwoPi = 0.159155f;
constexpr float k_TwoPi = 6.28319f;
/// The pulse's radians per second of turn (with 0.001 for ms to seconds)
constexpr float k_PulsePerSecond = 5.0f;
/// The draw's distance bounds and 1 / 30
constexpr uint32_t k_NearDistance = 10;
constexpr uint32_t k_FarDistance = 30;
constexpr float k_InvFarDistance = 0.0333333f;
/// The sprite stands 1.4 radii towards the eye, 2 radii wide (at least 0.0001); the click sphere is r + 0.5
constexpr float k_SpriteTowardsEye = 1.4f;
constexpr float k_SpriteSizePerRadius = 2.0f;
constexpr float k_MinSpriteSize = 0.0001f;
constexpr float k_ClickMargin = 0.5f;
/// The glow's Y row x 3
constexpr float k_GlowStretch = 3.0f;
/// 48 entries a category
constexpr uint32_t k_DidYouKnowListSize = 0x30;
constexpr size_t k_DidYouKnowCategoryCount = 5;
/// The sparkles: an 8000 ms cycle; the angle drawn in [0, 2 pi); the rise 20 x ms^2 x 1.5625e-8 (20 units at the end);
/// the half width from 2 to 1.2; the alpha up over 5000 ms, then down; a new cell every 50 ms on 32 cells
constexpr int32_t k_SparkleCycleMs = 8000;
constexpr float k_SparkleAngleRange = 6.2831855f;
constexpr float k_SparkleRisePerMsSquared = 1.5625e-8f;
constexpr float k_SparkleRise = -20.0f; // subtracted
constexpr float k_SparkleStartSize = 2.0f;
constexpr float k_SparkleEndSize = 1.2f;
constexpr int32_t k_SparkleFadeInMs = 5000;
constexpr float k_SparkleFadeInPerMs = 0.0002f;
constexpr float k_SparkleFadeOutPerMs = 0.00033333333f; // 1 / 3000 as a float
constexpr int32_t k_SparkleCellMs = 50;
constexpr int32_t k_SparkleCells = 32;

/// What this module keeps between calls (Locator::scriptState)
struct ScriptHighlightState
{
	Pulse pulse {};
	/// The list, head first
	std::vector<entt::entity> list {};
	std::array<std::vector<uint32_t>, k_DidYouKnowCategoryCount> didYouKnowRead {};
	/// The gold scrolls' sparkles their Draw queued this frame, far from the eye first (the one Z queue's order)
	graphics::zsort::Queue<entt::entity> queuedSparkles {};
	/// The highlights whose sparkles stepped at the last frame's end: the only ones drawn
	std::vector<entt::entity> drawnSparkles {};
};

ScriptHighlightState& ScriptHighlightData()
{
	return openblack::Locator::scriptState::value().Get<ScriptHighlightState>();
}

ecs::Registry& Reg()
{
	return Locator::entitiesRegistry::value();
}

const GScriptHighlightInfo* InfoRow(uint32_t index)
{
	if (!Locator::infoConstants::has_value() || index >= k_InfoCount)
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().scriptHighlight.at(index);
}

HighlightComponent* Get(entt::entity thing)
{
	auto& registry = Reg();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<HighlightComponent>(thing) : nullptr;
}

float GroundAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

glm::vec3 WorldOf(const HighlightComponent& highlight)
{
	return {highlight.x, GroundAt(highlight.x, highlight.z) + highlight.altitude, highlight.z};
}

/// The info's normal or active mesh, as SetActivated and Create choose; the first mesh when out of range
void SetMeshOf(entt::entity thing, const HighlightComponent& highlight)
{
	const auto* info = InfoRow(highlight.infoIndex);
	if (info == nullptr)
	{
		return;
	}
	const MeshId mesh = highlight.active ? info->active : info->normal;
	auto& registry = Reg();
	const auto id = resources::HashIdentifier(mesh);
	if (auto* current = registry.TryGet<Mesh>(thing))
	{
		current->id = id;
	}
	else
	{
		registry.Assign<Mesh>(thing, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	registry.SetDirty();
}

/// An effect the highlight steps itself (scale 1.0), on this computer's random numbers, at the point. The highlight's
/// Draw steps and draws it with the frame's game time, on the frames the draw list draws it. 0 when the row has no
/// particle type or the type no file
uint32_t StartOwnedEffect(ParticleType type, const glm::vec3& point)
{
	if (type == ParticleType::None)
	{
		return 0; // PARTICLE_TYPE 0, none
	}
	const auto file = particles::ParticleTypeFile(type);
	if (file.empty())
	{
		return 0;
	}
	auto& effects = Locator::particleSystem::value();
	const auto id = effects.StartForSpell(file, point, glm::vec3(0.0f), 1.0f, nullptr, false);
	if (id != 0)
	{
		psys::manager::SetPerFrame(id);
		effects.SetDrawPath(id, psys::DrawPath::Sorted);
	}
	return id;
}

void DeleteEffect(uint32_t& id)
{
	if (id != 0)
	{
		Locator::particleSystem::value().Delete(id); // at once
		id = 0;
	}
}

/// An owned effect's steps of one Draw, each by the frame's game time with power 1 and enabled. An effect that ends is
/// gone. Returns whether it is drawn this frame
bool StepOwnedEffect(uint32_t& effect, bool& firstStepPending, uint32_t steps, uint32_t frameMs)
{
	if (steps == 0)
	{
		return false;
	}
	const psys::ProcessInfo info {
	    .power = 1.0f,
	    .enabled = true,
	};
	const float seconds = static_cast<float>(frameMs) * 0.001f;
	for (uint32_t step = 0; step < steps && effect != 0; ++step)
	{
		if (!Locator::particleSystem::value().ProcessForSpell(effect, info, seconds))
		{
			effect = 0;
		}
	}
	firstStepPending = false;
	return effect != 0;
}

/// The glints' steps of one Draw (HighlightDrawStep)
void StepGlints(HighlightComponent& highlight, uint32_t steps, uint32_t frameMs)
{
	if (steps == 0)
	{
		return;
	}
	highlight.glintsDrawn = StepOwnedEffect(highlight.glintsEffect, highlight.glintsFirstStepPending, steps, frameMs);
}

/// The sparkles' creator for the particle sprite path: S_SpriteSheet1 and its alpha file, added (mode 13), 8 cells a
/// row, the cell taken as it is (SparkleSprite::cell), centred, turned by its angle
const psys::Creator& SparklesCreator()
{
	static const auto creator = [] {
		psys::Creator made;
		made.kind = psys::Creator::Kind::Sprite;
		made.className = "ScriptHighlight";
		made.texture = k_SparklesTexture;
		made.spritesPerRow = 8;
		made.numFrames = 64;
		made.additive = true;
		return made;
	}();
	return creator;
}

/// The sparkles drawn this frame, each highlight's as one Z object at the point they rise from (the original's key is
/// the eye's squared distance to that point, as the particle path's for a Queued drawable)
void CollectSparkles(std::vector<psys::manager::Drawable>& out)
{
	if (!Locator::scriptState::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	for (const auto thing : ScriptHighlightData().drawnSparkles)
	{
		const auto* highlight = Get(thing);
		if (highlight == nullptr || !highlight->sparkles.has_value())
		{
			continue;
		}
		const auto& sparkles = *highlight->sparkles;
		psys::manager::Drawable drawable {sparkles.position, {}};
		for (size_t i = 0; i < sparkles.sparkles.size(); ++i)
		{
			const auto sprite = SparkleSpriteOf(sparkles, i);
			if (!sprite.has_value())
			{
				continue;
			}
			// (approximate) the path takes the angle from a matrix, atan2 of its sine and cosine: the same turn up to
			// the last bit
			glm::mat3 rotation(1.0f);
			rotation[0][0] = std::cos(sprite->angle);
			rotation[0][2] = std::sin(sprite->angle);
			drawable.atoms.push_back({&SparklesCreator(),
			                          sprite->position,
			                          rotation,
			                          sprite->size,
			                          1.0f,
			                          static_cast<float>(sprite->alpha),
			                          static_cast<float>(sprite->cell),
			                          {0xFF, 0xFF, 0xFF}});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	}
}

/// Whether it has a 3D object for its Draw: a Transform and a Mesh that the cache holds (a mesh the cache does not hold
/// counts as none)
bool HasDrawnObject(entt::entity thing)
{
	const auto& registry = Reg();
	const auto* mesh = registry.TryGet<const Mesh>(thing);
	return registry.AllOf<Transform>(thing) && mesh != nullptr && Locator::resources::has_value() &&
	       Locator::resources::value().GetMeshes().Contains(mesh->id);
}

/// The mesh's centre and radius after the Draw has placed it: its box (the centre and the half extents of every
/// submesh's points) through the matrix the 3D object is placed with (its position, its Y angle and its scale). None
/// without a mesh in the cache
std::optional<MeshBounds> DrawnBoundsOf(entt::entity thing, const HighlightComponent& highlight)
{
	auto& registry = Reg();
	const auto* transform = registry.TryGet<const Transform>(thing);
	const auto* mesh = registry.TryGet<const Mesh>(thing);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	const auto placed =
	    affine::FromModel(affine::PlacementMatrix(transform->position, highlight.drawAngle, transform->scale.x));
	return DrawnMeshBounds(placed, box.Center(), box.Size() * 0.5f);
}

/// One highlight's game turn
void Process(entt::entity thing, HighlightComponent& highlight)
{
	// the altitude = the draw height, or the top of what it stands on
	if (highlight.drawHeight)
	{
		highlight.altitude = *highlight.drawHeight;
	}
	else
	{
		const auto coords = map_coords::FromMetres(glm::vec2(highlight.x, highlight.z));
		highlight.altitude = map_cells::TallestOverlapping(coords, thing, coords, object::Get2DRadius(thing), true);
	}
	// a did-you-know not lit yet, every 8 turns by (turn + its unique id) & 7, is lit once its text has been read.
	// (approximate) the unique id is the creation index (openblack has no object heap)
	if (!highlight.active && IsDidYouKnowInfo(highlight.infoIndex))
	{
		const auto unique = static_cast<uint32_t>(object_index::Of(thing));
		if (((game_clock::Turn() + unique) & 7u) == 0 && did_you_know_read::IsRead(highlight.scriptId, highlight.category))
		{
			SetActivated(thing, true);
		}
	}
}
} // namespace

// ---- Pure rules -----------------------------------------------------------------------------------------------------

uint8_t SpriteAlpha(uint32_t index)
{
	switch (index)
	{
	case 1:
		return 0x32;
	case 2:
		return 0x96;
	case 3:
		return 0x64;
	default:
		return 0;
	}
}

float DistanceScale(float scale, float distanceToCamera, bool didYouKnow)
{
	auto distance = static_cast<uint32_t>(static_cast<int64_t>(distanceToCamera)); // truncated, then unsigned compares
	if (distance > k_FarDistance)
	{
		distance = k_FarDistance;
	}
	else if (distance < k_NearDistance)
	{
		distance = k_NearDistance;
	}
	if (didYouKnow)
	{
		return scale;
	}
	// the distance x scale x (1 / 30)
	return static_cast<float>(distance) * scale * k_InvFarDistance;
}

float SpinAngle(float angle, uint32_t frameMs)
{
	const float turned = angle + static_cast<float>(frameMs) * k_SpinPerMs;
	const auto turns = static_cast<int32_t>(turned * k_InvTwoPi); // truncated
	return turned - static_cast<float>(turns) * k_TwoPi;
}

DrawPose PoseOfDraw(float angle, bool facesEye, const glm::vec3& point, const glm::vec3& eye, float scale, bool didYouKnow,
                    uint32_t frameMs)
{
	float turned = 0.0f;
	if (facesEye)
	{
		turned = graphics::billboard::YawToEyeAngle(point, eye);
		const auto turns = static_cast<int32_t>(turned * k_InvTwoPi); // truncated
		turned -= static_cast<float>(turns) * k_TwoPi;
	}
	else
	{
		turned = SpinAngle(angle, frameMs);
	}
	return {.angle = turned, .scale = DistanceScale(scale, gutils::GetDistance(eye, point), didYouKnow)};
}

bool SavesGameWhenClicked(uint32_t scriptId)
{
	return scriptId == 0x38 || scriptId == 0x3B || scriptId == 0x3C || scriptId == 0x3D;
}

bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId)
{
	if (didYouKnow)
	{
		return scriptId != 0;
	}
	return active && scriptId != 0;
}

uint32_t OverwriteTapToolTip(ObjectType infoType)
{
	return static_cast<int>(infoType) == 1 ? 0xEF2u : 0u;
}

DrawStep HighlightDrawStep(bool earlyExit, bool onScreen, bool drawnBesides, bool hasGlints, bool firstStepPending)
{
	if (earlyExit)
	{
		return {.glintSteps = 0, .onScreen = true};
	}
	const uint32_t steps = !hasGlints ? 0u : (firstStepPending ? 2u : 1u);
	return {.glintSteps = steps, .onScreen = onScreen || drawnBesides};
}

MeshBounds DrawnMeshBounds(const affine::AffineMatrix& object, const glm::vec3& boxCentre, const glm::vec3& halfExtents)
{
	const auto& m = object.m;
	const glm::vec3& c = boxCentre;
	const glm::vec3& h = halfExtents;
	MeshBounds bounds;
	bounds.centre = {((c.z * m[6] + c.y * m[3]) + c.x * m[0]) + m[9], ((c.x * m[1] + c.z * m[7]) + c.y * m[4]) + m[10],
	                 ((c.y * m[5] + c.x * m[2]) + c.z * m[8]) + m[11]};
	const float x = (h.z * m[6] + h.y * m[3]) + h.x * m[0];
	const float y = (h.x * m[1] + h.z * m[7]) + h.y * m[4];
	const float z = (h.y * m[5] + h.x * m[2]) + h.z * m[8];
	bounds.radius = std::sqrt((x * x + z * z) + y * y);
	return bounds;
}

uint32_t OwnedEffectSteps(bool firstStepPending)
{
	return firstStepPending ? 2u : 1u;
}

components::HighlightSparkles MakeSparkles(const FloatRandFn& floatRand, const RandFn& rand)
{
	components::HighlightSparkles made;
	const auto count = static_cast<int32_t>(made.sparkles.size());
	for (int32_t i = 0; i < count; ++i)
	{
		// spread over the cycle: -8000 i / 20; a clock below 0 is not drawn yet
		auto& sparkle = made.sparkles.at(static_cast<size_t>(i));
		sparkle.ms = -(k_SparkleCycleMs * i) / count;
		sparkle.angle = floatRand(k_SparkleAngleRange);
	}
	made.cellOffset = static_cast<uint8_t>(rand(k_SparkleCells));
	return made;
}

glm::vec3 SparklesPoint(const glm::vec3& centre, float radius)
{
	return {centre.x, centre.y + radius, centre.z};
}

void StepSparkles(components::HighlightSparkles& sparkles, uint32_t frameMs, const FloatRandFn& floatRand)
{
	for (auto& sparkle : sparkles.sparkles)
	{
		// a 32-bit add; one cycle back at most
		sparkle.ms = static_cast<int32_t>(static_cast<uint32_t>(sparkle.ms) + frameMs);
		if (sparkle.ms > k_SparkleCycleMs)
		{
			sparkle.ms -= k_SparkleCycleMs;
			sparkle.angle = floatRand(k_SparkleAngleRange);
		}
	}
}

std::optional<SparkleSprite> SparkleSpriteOf(const components::HighlightSparkles& sparkles, size_t index)
{
	if (index >= sparkles.sparkles.size() || sparkles.sparkles.at(index).ms < 0)
	{
		return std::nullopt;
	}
	const auto& sparkle = sparkles.sparkles.at(index);
	const int32_t ms = sparkle.ms;
	// s = ms^2 (a 32-bit product) x 1.5625e-8, rounded once to a float as the game's
	const auto square = static_cast<int32_t>(static_cast<uint32_t>(ms) * static_cast<uint32_t>(ms));
	const auto s = static_cast<float>(static_cast<double>(square) * static_cast<double>(k_SparkleRisePerMsSquared));
	SparkleSprite sprite;
	sprite.position = sparkles.position;
	sprite.position.y = sparkles.position.y - k_SparkleRise * s;
	sprite.size = (k_SparkleEndSize - k_SparkleStartSize) * s + k_SparkleStartSize;
	const float fade = ms < k_SparkleFadeInMs ? static_cast<float>(ms) * k_SparkleFadeInPerMs
	                                          : 1.0f - static_cast<float>(ms - k_SparkleFadeInMs) * k_SparkleFadeOutPerMs;
	// truncated, the low byte
	sprite.alpha = static_cast<uint8_t>(static_cast<int64_t>(static_cast<float>(sparkles.maxAlpha) * fade) & 0xFF);
	sprite.angle = sparkle.angle;
	sprite.cell =
	    static_cast<uint32_t>((ms / k_SparkleCellMs + static_cast<int32_t>(index) + sparkles.cellOffset) % k_SparkleCells);
	return sprite;
}

bool DrawsBesidesTheMesh(uint32_t index, bool active)
{
	const bool glow = active && !IsDidYouKnowInfo(index);
	const bool goldParticle = index == static_cast<uint32_t>(Info::Gold);
	return glow || goldParticle;
}

void StepPulse(Pulse& pulse, uint32_t msPerTurn)
{
	pulse.phase += static_cast<float>(msPerTurn) * k_PulsePerSecond * 0.001f;
	if (pulse.phase > k_TwoPi) // only when above
	{
		pulse.phase -= k_TwoPi;
	}
	pulse.previous = pulse.value;
	pulse.value = (1.0f - std::cos(pulse.phase)) * 0.5f;
}

float ActivePulse(const Pulse& pulse, float turnFraction)
{
	float t = (pulse.value - pulse.previous) * turnFraction + pulse.previous;
	if (!(t > 0.0f)) // not above 0
	{
		t = 0.0f;
	}
	else if (!(t < 1.0f)) // not below 1
	{
		t = 1.0f;
	}
	return t * 0.6f + 0.4f;
}

uint32_t ActiveGlowArgb(uint32_t index, float activePulse)
{
	if (index == static_cast<uint32_t>(Info::Silver))
	{
		return 0x14B4DCFFu;
	}
	const auto whole = static_cast<uint8_t>(static_cast<int32_t>(activePulse)); // truncated, the low byte
	const auto alpha = static_cast<uint8_t>(whole * 0x50);                      // an 8-bit product
	return (static_cast<uint32_t>(alpha) << 24) | 0xFFFF00u;
}

// ---- The read did-you-knows -----------------------------------------------------------------------------------------

bool did_you_know_read::IsRead(uint32_t text, DykCategory category)
{
	const auto c = static_cast<size_t>(category);
	if (c >= k_DidYouKnowCategoryCount) // not a category: not read
	{
		return false;
	}
	const auto& list = ScriptHighlightData().didYouKnowRead.at(c);
	return std::find(list.begin(), list.end(), text) != list.end();
}

void did_you_know_read::MarkRead(uint32_t text, DykCategory category)
{
	auto& state = ScriptHighlightData();
	const auto c = static_cast<size_t>(category);
	if (c >= k_DidYouKnowCategoryCount || IsRead(text, category) || state.didYouKnowRead.at(c).size() >= k_DidYouKnowListSize)
	{
		return; // (inferred)
	}
	state.didYouKnowRead.at(c).push_back(text);
}

std::span<const uint32_t> did_you_know_read::In(DykCategory category)
{
	const auto c = static_cast<size_t>(category);
	if (c >= k_DidYouKnowCategoryCount)
	{
		return {};
	}
	return ScriptHighlightData().didYouKnowRead.at(c);
}

uint32_t did_you_know_read::Total()
{
	const auto& lists = ScriptHighlightData().didYouKnowRead;
	return std::accumulate(lists.begin(), lists.end(), uint32_t {0},
	                       [](uint32_t total, const auto& list) { return total + static_cast<uint32_t>(list.size()); });
}

void did_you_know_read::Clear()
{
	for (auto& list : ScriptHighlightData().didYouKnowRead)
	{
		list.clear();
	}
}

// ---- The highlights -------------------------------------------------------------------------------------------------

entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale)
{
	auto& state = ScriptHighlightData();
	const auto* info = InfoRow(infoIndex);
	if (infoIndex >= k_InfoCount)
	{
		return entt::null;
	}
	auto& registry = Reg();
	const auto thing = registry.Create();
	// the next creation index
	object_index::Assign(thing);
	auto& highlight = registry.Assign<HighlightComponent>(thing);
	highlight.infoIndex = infoIndex;
	highlight.x = position.x;
	highlight.z = position.z;
	highlight.altitude = 0.0f; // the script's point as map coordinates: y - ground (inferred: 0 for these)
	if (Locator::terrainSystem::has_value())
	{
		highlight.altitude = position.y - GroundAt(position.x, position.z);
	}
	highlight.scale = scale;
	highlight.drawAngle = yAngle;
	highlight.scriptId = scriptId;
	state.list.insert(state.list.begin(), thing); // the head
	registry.Assign<Transform>(thing, WorldOf(highlight), affine::AngleY(yAngle), glm::vec3(scale));

	// the 3D object, and the head of its cell's fixed list
	SetMeshOf(thing, highlight);
	map_cells::InsertMapObject(thing);
	// (pending, renderer) two calls on the 3D object (one only for a scroll); the sprite and the glow object
	// (mesh k_GlowMesh, material properties {1, 0, 0, 1, 1}) are ExtrasOf's; one more 3D object call is not read
	// a did-you-know already read is lit at once
	if (IsDidYouKnowInfo(infoIndex) && did_you_know_read::IsRead(highlight.scriptId, highlight.category))
	{
		SetActivated(thing, true);
	}
	// unless a flag is set (inferred clear for a new object), the info's glints at its point, with this as their
	// target, then its render particle
	auto& made = registry.Get<HighlightComponent>(thing);
	if (info != nullptr)
	{
		made.glintsEffect = StartOwnedEffect(info->particleTypeGlints, WorldOf(made));
		if (made.glintsEffect != 0)
		{
			made.glintsFirstStepPending = true;
			Locator::particleSystem::value().AddTarget(made.glintsEffect, thing);
		}
	}
	// then its sparkles, on this computer's random numbers, whatever the row (only a gold scroll's Draw draws them)
	made.sparkles = MakeSparkles(game_random::LocalFloatRand, game_random::LocalRand);
	psys::manager::AddDrawableSource(&CollectSparkles);
	return thing;
}

bool IsHighlight(entt::entity thing)
{
	return Get(thing) != nullptr;
}

uint32_t InfoIndexOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->infoIndex : script_type::k_NoSubtype;
}

bool IsDidYouKnow(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && IsDidYouKnowInfo(highlight->infoIndex);
}

bool IsActive(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && highlight->active;
}

uint32_t ScriptIdOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->scriptId : 0;
}

void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category)
{
	if (auto* highlight = Get(thing))
	{
		highlight->scriptId = scriptId;
		highlight->category = category;
	}
}

void SetYPos(entt::entity thing, float height)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->drawHeight = height;
	highlight->altitude = height;
	// an object with a 3D object: moved to the ground + altitude, with its angle and scale
	if (auto* transform = Reg().TryGet<Transform>(thing))
	{
		transform->position = WorldOf(*highlight);
		Reg().SetDirty();
	}
}

float GetYPos(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->altitude : 0.0f;
}

void SetActivated(entt::entity thing, bool on)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->active = on;
	DeleteEffect(highlight->activeEffect);
	if (on)
	{
		if (!IsDidYouKnowInfo(highlight->infoIndex))
		{
			// the sound (bank InGame, owner this, sample 0x86). (pending, audio) the options' mapping to
			// audio::PlaySoundEffect(PlayOptions) is not checked
		}
		if (const auto* info = InfoRow(highlight->infoIndex); info != nullptr)
		{
			highlight->activeEffect = StartOwnedEffect(info->particleTypeActive, WorldOf(*highlight));
			highlight->activeFirstStepPending = highlight->activeEffect != 0;
		}
	}
	SetMeshOf(thing, *highlight);
}

void ProcessHighlights()
{
	auto& scriptHighlightState = ScriptHighlightData();
	StepPulse(scriptHighlightState.pulse, game_clock::MsPerTurn());
	// from the head, the next read before Process; only the available ones.
	// (inferred) every highlight in the list is available: openblack has no other state for them
	const auto list = scriptHighlightState.list; // Process may light one (SetActivated), never delete one
	for (const auto thing : list)
	{
		if (auto* highlight = Get(thing))
		{
			Process(thing, *highlight);
		}
	}
}

void SyncMeshes()
{
	auto& registry = Reg();
	bool removed = false;
	for (const auto thing : ScriptHighlightData().list)
	{
		auto* highlight = Get(thing);
		if (highlight == nullptr || !registry.AllOf<Transform>(thing))
		{
			continue;
		}
		const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
		// hidden, or the scripts' SET_DRAW_HIGHLIGHT 0 for a scroll: no Draw at
		// all, the 3D object stays where it was (inferred: openblack hides the mesh)
		const bool drawn =
		    highlight->hidden == 0 && (help::script_control::GetCameraControl().drawHighlight != 0 || didYouKnow);
		if (!drawn)
		{
			if (registry.AllOf<Mesh>(thing))
			{
				registry.Remove<Mesh>(thing);
				removed = true;
			}
			continue;
		}
		if (!registry.AllOf<Mesh>(thing))
		{
			SetMeshOf(thing, *highlight);
		}
	}
	if (removed)
	{
		registry.SetDirty();
	}
}

bool Draw(entt::entity thing, const draw_list::OnScreenView& view, uint32_t frameMs)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return true;
	}
	const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
	// hidden, the scripts' SET_DRAW_HIGHLIGHT 0 for a scroll, or no mesh (openblack: a mesh the cache does not hold
	// counts as none)
	const bool shown = highlight->hidden == 0 && (help::script_control::GetCameraControl().drawHighlight != 0 || didYouKnow);
	const bool posed = shown && HasDrawnObject(thing);
	if (posed)
	{
		// the pose, only on the frames the list draws it: a lit did-you-know faces this frame's eye, the others turn by
		// this frame's game ms (no catch-up for the frames it was not drawn), and the scale is the distance scale of its
		// own scale; then the 3D object is placed there
		const auto point = WorldOf(*highlight);
		const auto pose = PoseOfDraw(highlight->drawAngle, highlight->active && didYouKnow, point, view.eye, highlight->scale,
		                             didYouKnow, frameMs);
		highlight->drawAngle = pose.angle;
		auto& transform = Reg().Get<Transform>(thing);
		transform.position = point;
		transform.rotation = affine::AngleY(pose.angle);
		transform.scale = glm::vec3(pose.scale);
		Reg().SetDirty();
	}
	const auto inputs = posed ? draw_list::OnScreenInputsOf(Reg(), thing) : std::nullopt;
	const bool onScreen = inputs.has_value() && draw_list::OnScreen(*inputs, view);
	const auto step =
	    HighlightDrawStep(!inputs.has_value(), onScreen, DrawsBesidesTheMesh(highlight->infoIndex, highlight->active),
	                      highlight->glintsEffect != 0, highlight->glintsFirstStepPending);
	StepGlints(*highlight, step.glintSteps, frameMs);
	if (!inputs.has_value())
	{
		return step.onScreen; // an early exit
	}
	// the mesh's centre and radius through the matrix the 3D object is placed with
	const auto bounds = DrawnBoundsOf(thing, *highlight);
	if (!bounds.has_value())
	{
		return step.onScreen;
	}
	const glm::vec3 centre = bounds->centre;
	const float radius = bounds->radius;
	// the active effect: moved to the centre, then stepped as the glints
	if (highlight->activeEffect != 0)
	{
		Locator::particleSystem::value().SetOrigin(highlight->activeEffect, centre);
		highlight->activeDrawn = StepOwnedEffect(highlight->activeEffect, highlight->activeFirstStepPending,
		                                         OwnedEffectSteps(highlight->activeFirstStepPending), frameMs);
	}
	// a gold scroll's sparkles: over the centre by the radius, one Z object at the eye's squared distance; they step
	// and are drawn when the queue drains at the frame's end
	if (highlight->sparkles.has_value() && highlight->infoIndex == static_cast<uint32_t>(Info::Gold))
	{
		highlight->sparkles->position = SparklesPoint(centre, radius);
		ScriptHighlightData().queuedSparkles.Submit(thing, graphics::zsort::Key(highlight->sparkles->position, view.eye));
	}
	return step.onScreen;
}

void EndDrawnFrame()
{
	auto& state = ScriptHighlightData();
	for (const auto thing : state.list)
	{
		auto* highlight = Get(thing);
		if (highlight == nullptr)
		{
			continue;
		}
		if (highlight->glintsEffect != 0)
		{
			Locator::particleSystem::value().SetDrawn(highlight->glintsEffect, highlight->glintsDrawn);
		}
		if (highlight->activeEffect != 0)
		{
			Locator::particleSystem::value().SetDrawn(highlight->activeEffect, highlight->activeDrawn);
		}
		highlight->glintsDrawn = false;
		highlight->activeDrawn = false;
	}
}

void DrainSparkles(uint32_t frameMs)
{
	if (!Locator::scriptState::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& state = ScriptHighlightData();
	// each queued highlight's sparkles step by the frame's game ms and are drawn, far first
	state.drawnSparkles.clear();
	for (const auto& entry : state.queuedSparkles.Drain())
	{
		auto* highlight = Get(*entry.item);
		if (highlight == nullptr || !highlight->sparkles.has_value())
		{
			continue;
		}
		StepSparkles(*highlight->sparkles, frameMs, game_random::LocalFloatRand);
		state.drawnSparkles.push_back(*entry.item);
	}
	state.queuedSparkles.Begin();
}

DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye)
{
	DrawExtras extras;
	const auto* highlight = Get(thing);
	const auto* transform = highlight != nullptr ? Reg().TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr || !Reg().AllOf<Mesh>(thing))
	{
		return extras;
	}
	extras.drawn = true;
	const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
	// the mesh's centre through the 3D object's matrix and |M x (the mesh's half extents)|.
	// (approximate) ObjectMetrics exports no box centre: the centre is taken at half the height over the position
	const auto half = object::ObjectHalfExtents(thing).value_or(glm::vec3(0.0f));
	const glm::mat3 matrix = transform->rotation * glm::mat3(transform->scale.x);
	extras.radius = glm::length(matrix * half);
	extras.centre = transform->position + glm::vec3(0.0f, half.y * transform->scale.y, 0.0f);
	// the glow of an active scroll: YawToEye's rows, the Y row x 3, at the centre
	if (highlight->active && !didYouKnow)
	{
		glm::mat3 axes = graphics::billboard::YawToEye(extras.centre, eye);
		axes[1] *= k_GlowStretch;
		glm::mat4 glow(axes);
		glow[3] = glm::vec4(extras.centre, 1.0f);
		extras.glow = glow;
		const auto pulse = ActivePulse(ScriptHighlightData().pulse, game_clock::TurnFraction());
		extras.glowArgb = ActiveGlowArgb(highlight->infoIndex, pulse);
	}
	// a scroll is clicked through an invisible sphere
	if (!didYouKnow)
	{
		extras.clickRadius = extras.radius + k_ClickMargin;
	}
	// the sprite, not for a lit did-you-know
	if (!(didYouKnow && highlight->active))
	{
		const glm::vec3 towards = eye - extras.centre;
		const float length = glm::length(towards);
		const glm::vec3 direction = length > 0.0f ? towards / length : glm::vec3(0.0f);
		graphics::billboard::Sprite sprite {
		    .position = extras.centre + direction * (extras.radius * k_SpriteTowardsEye),
		    .size = std::max(k_SpriteSizePerRadius * extras.radius, k_MinSpriteSize),
		    .argb = (static_cast<uint32_t>(SpriteAlpha(highlight->infoIndex)) << 24) | 0xFFFFFFu,
		    .cell = 0,
		};
		extras.sprite = sprite;
	}
	return extras;
}

bool InterfaceValidToTap(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && ValidToTap(IsDidYouKnowInfo(highlight->infoIndex), highlight->active, highlight->scriptId);
}

bool InterfaceTap(entt::entity thing, bool byLocalPlayer)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return true;
	}
	// the help event (a script id ? 0x22 : 0x23) for the local player's tap. (inferred) byLocalPlayer stands for the
	// original's check that the tapping player is the local one
	if (byLocalPlayer)
	{
		help_profile::Trigger(highlight->scriptId != 0 ? help_profile::Event::Reminder : help_profile::Event::ScriptActivate);
	}
	if (highlight->scriptId == 0)
	{
		return true;
	}
	SetActivated(thing, true);
	if (IsDidYouKnowInfo(highlight->infoIndex))
	{
		// the local interface taps: the sound, bank InGame, sample 0xAD, the other options at their defaults
		if (byLocalPlayer)
		{
			audio::PlayOptions options;
			options.sample = {audio::Bank(audio::SfxBank::InGame), 0xAD};
			audio::PlaySoundEffect(options);
		}
		// the help bubble shows its text (script id and category); the same sign again closes it
		if (help::Get() != nullptr)
		{
			auto& tipBubble = TipBubbles();
			const bool same = tipBubble.GetSign() == thing;
			if (!same && did_you_know_read::Total() == 0) // the first one ever
			{
				chlapi::CHLApi::StartHelpScript("FirstDYKExplained");
			}
			if (same)
			{
				tipBubble.Hide();
				return true; // closed, nothing marked
			}
			tipBubble.Show(thing, highlight->scriptId);
		}
		did_you_know_read::MarkRead(highlight->scriptId, highlight->category);
		return true;
	}
	// (pending) a help script holding the dialogue -> the help scripts stop; no dialogue owner -> the temple's
	// challenge room starts the script id
	return true;
}

void OnToBeDeleted(entt::entity thing)
{
	auto& state = ScriptHighlightData();
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	state.list.erase(std::remove(state.list.begin(), state.list.end(), thing), state.list.end());
	DeleteEffect(highlight->glintsEffect);
	DeleteEffect(highlight->activeEffect);
	highlight->sparkles.reset();
	std::erase(state.drawnSparkles, thing);
}

void OnClearMap()
{
	auto& state = ScriptHighlightData();
	state.pulse = Pulse {};
	state.list.clear(); // openblack: the map clear deleted them
	state.queuedSparkles.Begin();
	state.drawnSparkles.clear();
}

const std::vector<entt::entity>& All()
{
	return ScriptHighlightData().list;
}

} // namespace openblack::ecs::script_highlight
