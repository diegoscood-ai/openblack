/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

bool HandSystem::Initialize() noexcept
{
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	_hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);

	LoadAnimations();
	return false;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return _hands;
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetPlayerHandPositions() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetPlayerHands();
	std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> result = {
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Left)]).position,
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Right)]).position,
	};
	// The player's hand interacts through the index fingertip (or the grip point), not the mesh origin.
	if (_interactionPoint)
	{
		result[static_cast<size_t>(Side::Left)] = _interactionPoint;
	}
	// TODO(#693): Hand Getter should return an optional if the hand doesn't have a valid position
	// When the position is zero, it probably means it's not on the map (e.g. mouse is in the sky)
	if (result[static_cast<size_t>(Side::Left)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Left)] = std::nullopt;
	}
	if (result[static_cast<size_t>(Side::Right)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Right)] = std::nullopt;
	}
	return result;
}

void HandSystem::LoadAnimations() noexcept
{
	auto logger = spdlog::get("game");
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& mesh = Locator::resources::value().GetMeshes().Handle(Hand::k_MeshId);
		if (!mesh || !mesh->IsBoned())
		{
			SPDLOG_LOGGER_WARN(logger, "Hand mesh is not loaded or has no bones: hand stays in bind pose");
			return;
		}
		const auto hbnPath = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.HBN";
		const auto specsDirectory = fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>());
		auto animator = std::make_unique<HandAnimator>();
		if (animator->Load(fileSystem.ReadAll(hbnPath), specsDirectory, mesh->GetBoneMatrices(), mesh->GetBoneParents()))
		{
			_animator = std::move(animator);
			LoadGeometry();
		}
		if (std::getenv("OPENBLACK_HAND_INFO") != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			for (size_t i = 0; i < pots.size(); ++i)
			{
				const auto& p = pots[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "PotInfo {}: potType={} resource={} maxInPot={} next={} initial={} perTurn={} perTurnEnd={} maxPick={} ramp={} mesh={}",
				                   i, static_cast<int>(p.potType), static_cast<int>(p.resourceType), p.maxAmountInPot,
				                   static_cast<int>(p.nextPotForResource), p.amountPickedUpInitially, p.amountPickedUpPerTurn,
				                   p.amountPickedUpPerTurnEnd, p.maxAmountCanBePickedUp, p.multiPickUpRampTime, static_cast<int>(p.meshId));
			}
		}
		// Debug: OPENBLACK_HAND_ANIM=<C node> forces an animation (e.g. Cgrip) instead of the gameplay state.
		if (const char* forced = std::getenv("OPENBLACK_HAND_ANIM"); forced != nullptr && _animator)
		{
			_override = forced;
		}
		// Debug: OPENBLACK_HAND_DUMP=<file> writes the evaluated bone matrices of every clip for validation.
		if (const char* dump = std::getenv("OPENBLACK_HAND_DUMP"); dump != nullptr && _animator)
		{
			std::ofstream out(dump);
			out << "{";
			bool firstClip = true;
			for (const auto& clip : _animator->ListClips())
			{
				for (const auto& [t, lr, fb] : {std::tuple {0.0f, 0.0f, 0.0f}, {137.0f, 0.0f, 0.0f}, {0.0f, 0.6f, -0.4f}})
				{
					const auto key = fmt::format("{}@{}@{}@{}", clip.name, t, lr, fb);
					out << (firstClip ? "" : ",") << "\n\"" << key << "\":[";
					firstClip = false;
					const auto mats = _animator->Evaluate(clip.name, t, lr, fb, clip.name == "Cgrip" ? 1.0f : 0.0f);
					for (size_t b = 0; b < mats.size(); ++b)
					{
						const float* v = glm::value_ptr(mats[b]);
						out << (b ? "," : "") << "[";
						for (int k = 0; k < 16; ++k)
						{
							out << (k ? "," : "") << v[k];
						}
						out << "]";
					}
					out << "]";
				}
			}
			out << "\n}\n";
			SPDLOG_LOGGER_INFO(logger, "Hand animation dump written to {}", dump);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(logger, "Failed to load hand animations: {}", e.what());
	}
}

void HandSystem::Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept
{
	if (!_animator)
	{
		return;
	}
	const float seconds = static_cast<float>(dt.count()) / 1e6f;

	// Cursor speed (px/s) normalised to -1..+1 like the original L layers expect. Quick response while the
	// mouse moves, slower return to neutral once it stops.
	constexpr float k_ReferenceSpeed = 900.0f;
	if (seconds > 0.0f && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f))
	{
		const auto velocity = mouseDelta / std::max(seconds, 0.004f);
		_motionTarget = glm::clamp(glm::vec2(velocity.x, -velocity.y) / k_ReferenceSpeed, -1.0f, 1.0f);
		_motionAge = 0.0f;
	}
	UpdateGripDust(seconds);
	_motionAge += seconds;
	if (_motionAge > 0.045f)
	{
		_motionTarget *= std::exp(-seconds * 8.0f);
	}
	_motion += (_motionTarget - _motion) * (1.0f - std::exp(-seconds * 15.0f));
	_animator->SetMotion(_motion.x, _motion.y);

	// Debug: OPENBLACK_HAND_TEST_ROCK="x,z" spawns a pickable boulder there once, to test pick up / drop.
	if (static bool spawned = false; !spawned && Locator::terrainSystem::has_value())
	{
		spawned = true;
		if (const char* at = std::getenv("OPENBLACK_HAND_TEST_ROCK"); at != nullptr)
		{
			float x = 0.0f;
			float z = 0.0f;
			if (std::sscanf(at, "%f,%f", &x, &z) == 2)
			{
				const float y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
				archetypes::MobileStaticArchetype::Create(glm::vec3(x + 6.0f, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + 6.0f, z)), z), MobileStaticInfo::Boulder1Chalk, 0.0f, 0.0f, 0.0f,
				                                          0.0f, 1.0f);
				Locator::entitiesRegistry::value().SetDirty();
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: boulder spawned at ({}, {}, {})", x, y, z);
				// ...and a wood pile next to it, to test the multi pick up (HandWood) and special_hold fill.
				const float px = x;
				const float py = Locator::terrainSystem::value().GetHeightAt(glm::vec2(px, z));
				archetypes::PotArchetype::Create(glm::vec3(px, py, z), 0.0f, PotInfo::WoodPile_1, 4000);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: wood pile (4000) spawned at ({}, {}, {})", px, py, z);
			}
		}
	}

	// Pick up / drop with the action button (right). Only while not gripping the land.
	if (_held && !Locator::entitiesRegistry::value().Valid(*_held))
	{
		_held.reset();
	}
	_hovered = (_held || gripping) ? std::nullopt : FindObjectUnderHand();
	const bool actionPressed = actionHeld && !_actionWasHeld;
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	if (actionPressed && _hovered && !_held)
	{
		PickUp(*_hovered);
	}
	else if (actionReleased && _held)
	{
		// Released while moving fast: throw it (the hand velocity carries on), otherwise put it down.
		constexpr float k_ThrowSpeed = 25.0f;
		if (glm::length(glm::vec2(_handVelocity.x, _handVelocity.z)) > k_ThrowSpeed)
		{
			Throw(_handVelocity);
		}
		else
		{
			Drop();
		}
	}
	UpdateMultiPickUp(seconds, actionHeld);
	UpdateThrown(seconds);

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	std::string clip = gripping ? "Cgrip" : (_hovered ? "Ccan_pickup" : "Cwiggle");
	if (!_override.empty())
	{
		clip = _override;
	}
	// Static objects keep special_hold at 0% (Cphile). TODO: resources (food/wood) fill it with their amount.
	// special_hold fill: 0% (Cphile) for static objects, the amount carried for food / wood piles.
	_animator->SetSpecialHold(_held && _override.empty() ? std::optional(HeldFill()) : std::nullopt, std::chrono::milliseconds(140));
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(clip == "Cgrip" ? 90 : 150));
	}
	// Grip drag lives mostly in the root translation of Lgrip_lr/Lgrip_fb.
	_animator->SetLayerTranslationScale(clip == "Cgrip" ? 1.0f : 0.0f);
	// The palm always faces the ground, except in the camera states where the root motion is the drag itself.
	_animator->SetRootLocked(clip != "Cgrip" || _animator->IsSpecialHold());
	_animator->Update(dt);
}

const std::vector<glm::mat4>* HandSystem::GetBoneMatrices() const noexcept
{
	return _animator ? &_animator->GetBoneMatrices() : nullptr;
}

std::vector<std::string> HandSystem::GetAnimationNames() const noexcept
{
	std::vector<std::string> names;
	if (_animator)
	{
		for (const auto& clip : _animator->ListClips())
		{
			if (!clip.name.empty() && clip.name.front() == 'C')
			{
				names.push_back(clip.name);
			}
		}
	}
	return names;
}

const std::string& HandSystem::GetCurrentAnimation() const noexcept
{
	static const std::string k_None;
	return _animator ? _animator->GetCurrentClip() : k_None;
}

void HandSystem::SetAnimationOverride(const std::string& name) noexcept
{
	_override = name;
}

namespace
{
// Hand_Boned_*2.l3d: 0 = palm (root), 8..19 = four fingers (3 phalanges), 10/13/16/19 = fingertips, 20/21 = thumb.
constexpr std::array<uint32_t, 1> k_PalmBones = {0};
constexpr std::array<uint32_t, 12> k_FingerBones = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
constexpr std::array<uint32_t, 4> k_TipBones = {10, 13, 16, 19};
constexpr std::array<uint32_t, 2> k_ThumbBones = {20, 21};
constexpr std::array<const char*, 4> k_ClosedPoses = {"Chorn", "Cphile", "Cgrip", "Chold_fingers"};
constexpr const char* k_PointPose = "Ccan_pickup";
} // namespace

glm::vec3 HandSystem::ModelPosition(size_t vertex, const std::vector<glm::mat4>& bones) const noexcept
{
	return glm::vec3(bones[_vertexBones[vertex]] * glm::vec4(_vertices[vertex], 1.0f));
}

void HandSystem::LoadGeometry() noexcept
{
	auto logger = spdlog::get("game");
	auto& fileSystem = Locator::filesystem::value();
	const auto& mesh = Locator::resources::value().GetMeshes().Handle(Hand::k_MeshId);
	l3d::L3DFile file;
	const auto path = fileSystem.GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d";
	if (file.Open(fileSystem.ReadAll(path)) != l3d::L3DResult::Success || file.GetSubmeshHeaders().empty())
	{
		SPDLOG_LOGGER_WARN(logger, "Hand: cannot read {} for placement", path.string());
		return;
	}
	_vertices.clear();
	_vertexBones.clear();
	const auto& vertices = file.GetVertexSpan(0);
	size_t offset = 0;
	for (const auto& group : file.GetVertexGroupSpan(0))
	{
		for (uint32_t i = 0; i < group.vertexCount && offset < vertices.size(); ++i, ++offset)
		{
			_vertices.emplace_back(vertices[offset].position.x, vertices[offset].position.y, vertices[offset].position.z);
			_vertexBones.push_back(group.boneIndex);
		}
	}

	const auto& bind = mesh->GetBoneMatrices();
	const auto& parents = mesh->GetBoneParents();
	const auto root = static_cast<size_t>(std::distance(
	    parents.begin(), std::find(parents.begin(), parents.end(), std::numeric_limits<uint32_t>::max())));
	// Evaluate a clip with its root replaced by the bind root, so only the fingers move.
	const auto evaluateLocked = [&](const char* name) {
		auto bones = _animator->Evaluate(name, 0.0f);
		if (bones.size() == bind.size())
		{
			const auto correction = bind[root] * glm::affineInverse(bones[root]);
			for (auto& m : bones)
			{
				m = correction * m;
			}
		}
		return bones;
	};
	const auto centroid = [&](const auto& boneSet, const std::vector<glm::mat4>& bones) {
		glm::vec3 c(0.0f);
		int n = 0;
		for (size_t v = 0; v < _vertices.size(); ++v)
		{
			if (std::find(boneSet.begin(), boneSet.end(), _vertexBones[v]) != boneSet.end())
			{
				c += ModelPosition(v, bones);
				++n;
			}
		}
		return n > 0 ? c / static_cast<float>(n) : c;
	};

	const auto palm = centroid(k_PalmBones, bind);
	_palmCenter = palm;
	_frameFingers = glm::normalize(centroid(k_FingerBones, bind) - palm);
	// Palm normal: the side the fingertips close towards.
	const auto openTips = centroid(k_TipBones, bind);
	_frameNormal = glm::vec3(0.0f);
	for (const auto* name : k_ClosedPoses)
	{
		if (!_animator->Has(name))
		{
			continue;
		}
		auto d = centroid(k_TipBones, evaluateLocked(name)) - openTips;
		d -= _frameFingers * glm::dot(d, _frameFingers);
		if (glm::length(d) > 1e-3f)
		{
			_frameNormal = glm::normalize(d);
			break;
		}
	}
	if (glm::length(_frameNormal) < 0.5f)
	{
		const glm::vec3 z(0.0f, 0.0f, 1.0f);
		_frameNormal = glm::normalize(z - _frameFingers * glm::dot(z, _frameFingers));
	}
	_frameLateral = glm::cross(_frameFingers, _frameNormal);

	// Index = fingertip closest to the thumb; its front-most vertex in the pointing pose is the interaction point.
	const float side = glm::dot(centroid(k_ThumbBones, bind) - palm, _frameLateral) >= 0.0f ? 1.0f : -1.0f;
	uint32_t indexTip = k_TipBones[0];
	float best = -std::numeric_limits<float>::max();
	for (const auto tip : k_TipBones)
	{
		const float score = glm::dot(centroid(std::array {tip}, bind), _frameLateral) * side;
		if (score > best)
		{
			best = score;
			indexTip = tip;
		}
	}
	const auto pointing = _animator->Has(k_PointPose) ? evaluateLocked(k_PointPose) : bind;
	best = -std::numeric_limits<float>::max();
	_tipVertices.clear();
	for (const auto tip : k_TipBones)
	{
		size_t front = 0;
		float frontScore = -std::numeric_limits<float>::max();
		for (size_t v = 0; v < _vertices.size(); ++v)
		{
			if (_vertexBones[v] != tip)
			{
				continue;
			}
			const float score = glm::dot(ModelPosition(v, bind), _frameFingers);
			if (score > frontScore)
			{
				frontScore = score;
				front = v;
			}
			if (tip == indexTip)
			{
				const auto p = ModelPosition(v, pointing);
				if (glm::dot(p, _frameFingers) > best)
				{
					best = glm::dot(p, _frameFingers);
					_hotspot = p;
				}
			}
		}
		_tipVertices.push_back(front);
	}
	SPDLOG_LOGGER_INFO(logger, "Hand: {} vertices, index fingertip = bone {}", _vertices.size(), indexTip);
}

glm::mat3 HandSystem::FrameRotation(glm::vec3 cameraForward) const noexcept
{
	glm::vec3 forward(cameraForward.x, 0.0f, cameraForward.z);
	forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, -1.0f);
	const glm::vec3 down(0.0f, -1.0f, 0.0f);
	const auto lateral = glm::cross(forward, down);
	// Map the mesh frame (fingers, palm normal, lateral) onto (forward, down, lateral): palm towards the ground.
	const glm::mat3 world(forward, down, lateral);
	const glm::mat3 local(_frameFingers, _frameNormal, _frameLateral);
	return world * glm::transpose(local);
}

void HandSystem::Place(std::optional<glm::vec3> groundPoint, glm::vec3 cameraForward, bool gripping,
                       std::chrono::microseconds dt) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (!_animator || _vertices.empty())
	{
		return;
	}
	const auto& bones = _animator->GetBoneMatrices();
	const float scale = transform.scale.x;
	_lastDt = static_cast<float>(dt.count()) / 1e6f;

	if (gripping && !_held)
	{
		// Grip Landscape: the fingertips stay dug into the point where the land was grabbed.
		if (!_gripPoint)
		{
			if (!_interactionPoint && !groundPoint)
			{
				return;
			}
			_gripPoint = _interactionPoint ? _interactionPoint : groundPoint;
			_gripRotation = transform.rotation;
			EmitGripDust(*_gripPoint);
		}
		glm::vec3 claw(0.0f);
		for (const auto v : _tipVertices)
		{
			claw += ModelPosition(v, bones);
		}
		claw /= static_cast<float>(std::max<size_t>(1, _tipVertices.size()));
		if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr && groundPoint)
		{
			static int frame = 0;
			if (++frame % 30 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grip trace: grab ({:.1f},{:.1f},{:.1f}) under cursor ({:.1f},{:.1f},{:.1f}) gap={:.2f}",
				                   _gripPoint->x, _gripPoint->y, _gripPoint->z, groundPoint->x, groundPoint->y, groundPoint->z,
				                   glm::distance(*_gripPoint, *groundPoint));
			}
		}
		transform.rotation = _gripRotation;
		transform.position = *_gripPoint - glm::vec3(0.0f, _clawDepth, 0.0f) - _gripRotation * (claw * scale);
		_smoothedPosition = transform.position;
		_interactionPoint = _gripPoint;
		registry.SetDirty();
		return;
	}
	_gripPoint.reset();
	if (!groundPoint)
	{
		return;
	}

	const auto rotation = FrameRotation(cameraForward);
	auto position = *groundPoint + glm::vec3(0.0f, _tipClearance, 0.0f) - rotation * (_hotspot * scale);
	// No vertex (wrist included) below the landscape: lift just enough.
	if (Locator::terrainSystem::has_value())
	{
		const auto& terrain = Locator::terrainSystem::value();
		float lift = 0.0f;
		for (size_t v = 0; v < _vertices.size(); ++v)
		{
			const auto w = position + rotation * (ModelPosition(v, bones) * scale);
			lift = std::max(lift, terrain.GetHeightAt(glm::vec2(w.x, w.z)) + _vertexClearance - w.y);
		}
		position.y += lift;
	}
	// TODO(#480): the original moves the hand with a velocity; a short exponential follow keeps it responsive.
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	if (_smoothedPosition && seconds > 0.0f)
	{
		*_smoothedPosition += (position - *_smoothedPosition) * (1.0f - std::exp(-seconds * 40.0f));
	}
	else
	{
		_smoothedPosition = position;
	}
	transform.rotation = rotation;
	transform.position = *_smoothedPosition;
	_interactionPoint = groundPoint;
	UpdateHeldObject();
	registry.SetDirty();
}

std::optional<entt::entity> HandSystem::FindObjectUnderHand() const noexcept
{
	if (!_interactionPoint)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const glm::vec2 point(_interactionPoint->x, _interactionPoint->z);
	std::optional<entt::entity> best;
	float bestDistance = std::numeric_limits<float>::max();
	registry.Each<const Transform, const Mesh>(
	    [&](entt::entity entity, const Transform& transform, const Mesh& mesh) {
		    // Pickable: mobile objects, trees and resource piles/pots (food, wood).
		    if (!registry.AnyOf<Mobile, Tree, Pot>(entity) || _hands[0] == entity || _hands[1] == entity)
		    {
			    return;
		    }
		    // Generous reach: the fingertip only needs to be over (or next to) the object.
		    float radius = 3.5f;
		    if (meshes.Contains(mesh.id))
		    {
			    const auto size = meshes.Handle(mesh.id)->GetBoundingBox().Size() * transform.scale;
			    radius = std::max(3.5f, 0.5f * std::max(size.x, size.z) + 2.0f);
		    }
		    const float distance = glm::distance(point, glm::vec2(transform.position.x, transform.position.z));
		    if (distance <= radius && distance < bestDistance)
		    {
			    best = entity;
			    bestDistance = distance;
		    }
	    });
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		static int frame = 0;
		if (++frame % 60 == 0)
		{
			float nearest = std::numeric_limits<float>::max();
			int count = 0;
			registry.Each<const Transform, const Mobile>([&](entt::entity, const Transform& t, const Mobile&) {
				++count;
				nearest = std::min(nearest, glm::distance(point, glm::vec2(t.position.x, t.position.z)));
			});
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand trace: point ({:.1f},{:.1f}) mobiles={} nearest={:.2f} hovered={} held={}",
			                   point.x, point.y, count, nearest, best.has_value(), _held.has_value());
		}
	}
	return best;
}

void HandSystem::PickUp(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	_pickSource.reset();
	_pickTime = 0.0f;
	_pickTurnAccumulator = 0.0f;
	_lastHeldPosition.reset();
	_handVelocity = glm::vec3(0.0f);
	// Food / wood: the hand grabs a HandFood / HandWood pile and keeps pulling from the source while held over it
	// (GPotInfo.amountPickedUpInitially / PerTurn / PerTurnEnd / multiPickUpRampTime from info.dat).
	if (auto* pot = registry.TryGet<Pot>(entity); pot != nullptr)
	{
		const auto& pots = Locator::infoConstants::value().pot;
		const auto& sourceType = registry.Get<Mesh>(entity);
		PotInfo sourceInfo = PotInfo::FoodPot;
		for (size_t i = 0; i < pots.size(); ++i)
		{
			if (resources::HashIdentifier(pots[i].meshId) == sourceType.id)
			{
				sourceInfo = static_cast<PotInfo>(i);
				break;
			}
		}
		const bool isHandPile = sourceInfo == PotInfo::HandWood || sourceInfo == PotInfo::HandFood;
		if (!isHandPile)
		{
			const auto handType =
			    pots[static_cast<size_t>(sourceInfo)].resourceType == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
			const auto& handInfo = pots[static_cast<size_t>(handType)];
			const auto take = std::min<uint32_t>(pot->amount, handInfo.amountPickedUpInitially);
			if (take == 0)
			{
				return;
			}
			const auto position = registry.Get<Transform>(entity).position;
			const auto pile = archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(take));
			if (pile == entt::null)
			{
				return;
			}
			pot->amount = static_cast<uint16_t>(pot->amount - take);
			_pickSource = entity;
			entity = pile;
		}
	}
	// Carried objects must not be glued to the landscape by the height-map shader.
	if (registry.AllOf<MorphWithTerrain>(entity))
	{
		registry.Remove<MorphWithTerrain>(entity);
	}
	auto& transform = registry.Get<Transform>(entity);
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	                         : 0.0f;
	_heldAltitude = transform.position.y - ground;
	_heldTop = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		_heldTop = meshes.Handle(mesh->id)->GetBoundingBox().maxima.y * transform.scale.y;
	}
	_held = entity;
	_hovered.reset();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: picked up entity {}", static_cast<uint32_t>(entity));
}

void HandSystem::Drop() noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(*_held))
	{
		auto& transform = registry.Get<Transform>(*_held);
		const float ground = Locator::terrainSystem::has_value()
		                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
		                         : 0.0f;
		transform.position.y = ground + _heldAltitude;
		if (auto* fixed = registry.TryGet<Fixed>(*_held); fixed != nullptr)
		{
			fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
		}
		registry.SetDirty();
	}
	_held.reset();
	_pickSource.reset();
}

void HandSystem::UpdateHeldObject() noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_held))
	{
		_held.reset();
		return;
	}
	const auto& hand = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
	auto& transform = registry.Get<Transform>(*_held);
	// Under the palm: the top of the object touches the palm centre.
	const auto palm = hand.position + hand.rotation * (_palmCenter * hand.scale.x);
	transform.position = glm::vec3(palm.x, palm.y - _heldTop, palm.z);
	if (_lastHeldPosition && _lastDt > 0.0f)
	{
		const auto velocity = (transform.position - *_lastHeldPosition) / _lastDt;
		_handVelocity += (velocity - _handVelocity) * 0.35f;
	}
	_lastHeldPosition = transform.position;
}


namespace
{
// Data/Spells/ZSpellFiles/SF_GripLandscape_txt.zzz
constexpr uint32_t k_DustAtoms = 8;          // CreateRuleSphere.NumAtoms
constexpr float k_DustRadius = 1.41371f;     // CreateRuleSphere.Radius
constexpr float k_DustFrameRate = 20.7611f;  // ParticleSpriteCreator.FrameRate
constexpr uint32_t k_DustFrames = 32;        // ParticleSpriteCreator.NumFrames (4 rows of the 8x8 S_SpriteSheet3)
constexpr float k_DustDieAge = 2.65752f;     // RemoveRuleOldAgeOnly.DieAge
constexpr float k_DustStartScale = 0.0f;     // UR_ChangeScale
constexpr float k_DustStopScale = 1.43009f;  //
constexpr float k_DustStartAlpha = 78.0f;    // AR_FadeAlpha
constexpr float k_DustStopAlpha = 2.0f;      //
constexpr glm::vec3 k_DustColour {255.0f / 255.0f, 182.0f / 255.0f, 198.0f / 255.0f}; // ColorR/G/B
constexpr float k_DustColourAlpha = 154.0f / 255.0f;                                  // ColorA

glm::vec2 DustFrameUv(uint32_t frame)
{
	frame %= k_DustFrames;
	// S_SpriteSheet3 is an 8x8 grid; the dust animation is the first 4 rows (the rest are other effects).
	return {static_cast<float>(frame % 8) / 8.0f, static_cast<float>(frame / 8) / 8.0f};
}
} // namespace

void HandSystem::EmitGripDust(glm::vec3 point) noexcept
{
	auto& resources = Locator::resources::value();
	auto& textures = resources.GetTextures();
	const auto textureId = entt::hashed_string("raw/S_SpriteSheet3a"); // alpha of the sheet: the puff shape
	if (!textures.Contains(textureId))
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			textures.Load(textureId, resources::Texture2DLoader::FromDiskTag {},
			              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "S_SpriteSheet3a.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Grip dust: cannot load S_SpriteSheet3a.raw: {}", e.what());
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grip dust: loaded S_SpriteSheet3a.raw (present now: {})", textures.Contains(textureId));
	}
	// Dust only on land: gripping the sea (water plane at y = 0) throws no dust.
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr && Locator::terrainSystem::has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grip dust at ({:.1f},{:.1f},{:.1f}) terrain height {:.2f}", point.x, point.y,
		                   point.z, Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z)));
	}
	if (Locator::terrainSystem::has_value() && Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z)) <= 0.05f)
	{
		return;
	}
	const auto texture = textures.Handle(textureId)->GetNativeHandle();
	auto& registry = Locator::entitiesRegistry::value();
	const auto random = [this]() {
		_dustSeed = _dustSeed * 1664525u + 1013904223u;
		return static_cast<float>(_dustSeed >> 8) / static_cast<float>(1u << 24);
	};
	for (uint32_t i = 0; i < k_DustAtoms; ++i)
	{
		// Random point in the upper half of the sphere around the grab point.
		const float a = random() * glm::two_pi<float>();
		const float u = random();
		const float r = std::sqrt(1.0f - u * u);
		const float d = k_DustRadius * std::cbrt(random());
		const glm::vec3 offset(std::cos(a) * r * d, u * d * 0.5f, std::sin(a) * r * d);
		const auto entity = registry.Create();
		const auto frame = static_cast<uint32_t>(random() * k_DustFrames); // RandomiseInitFrame
		// UseAdditiveAlpha 0 in SF_GripLandscape: normal blending (tint premultiplied by alpha each frame).
		registry.Assign<Sprite>(entity, texture, DustFrameUv(frame), glm::vec2(1.0f / 8.0f), glm::vec4(k_DustColour, 0.0f), false);
		registry.Assign<Transform>(entity, point + offset, glm::mat3(1.0f), glm::vec3(k_DustStartScale));
		_dust.push_back({entity, 0.0f, frame});
	}
}

void HandSystem::UpdateGripDust(float seconds) noexcept
{
	if (_dust.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& particle : _dust)
	{
		particle.age += seconds;
		if (particle.age >= k_DustDieAge || !registry.Valid(particle.entity))
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
			particle.entity = entt::null;
			continue;
		}
		const float t = particle.age / k_DustDieAge;
		auto& sprite = registry.Get<Sprite>(particle.entity);
		sprite.uvMin = DustFrameUv(particle.initialFrame + static_cast<uint32_t>(particle.age * k_DustFrameRate));
		sprite.tint.a = (k_DustStartAlpha + (k_DustStopAlpha - k_DustStartAlpha) * t) / 255.0f * k_DustColourAlpha;
		sprite.tint = glm::vec4(k_DustColour * sprite.tint.a, sprite.tint.a); // premultiplied
		registry.Get<Transform>(particle.entity).scale = glm::vec3(k_DustStartScale + (k_DustStopScale - k_DustStartScale) * t);
	}
	std::erase_if(_dust, [](const DustParticle& particle) { return particle.entity == entt::null; });
}

float HandSystem::HeldFill() const noexcept
{
	if (!_held)
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* pot = registry.TryGet<const Pot>(*_held); pot != nullptr && pot->maxAmount > 0)
	{
		return std::clamp(static_cast<float>(pot->amount) / static_cast<float>(pot->maxAmount), 0.0f, 1.0f);
	}
	return 0.0f;
}

void HandSystem::UpdateMultiPickUp(float seconds, bool actionHeld) noexcept
{
	if (!_held || !_pickSource || !actionHeld)
	{
		_pickSource.reset();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_pickSource) || !registry.Valid(*_held) || !_interactionPoint)
	{
		_pickSource.reset();
		return;
	}
	auto* source = registry.TryGet<Pot>(*_pickSource);
	auto* pile = registry.TryGet<Pot>(*_held);
	if (source == nullptr || pile == nullptr)
	{
		_pickSource.reset();
		return;
	}
	// Only while the hand stays over the source.
	const auto& sourcePosition = registry.Get<Transform>(*_pickSource).position;
	if (glm::distance(glm::vec2(_interactionPoint->x, _interactionPoint->z), glm::vec2(sourcePosition.x, sourcePosition.z)) > 6.0f)
	{
		return;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const bool wood = pile->maxAmount == pots[static_cast<size_t>(PotInfo::HandWood)].maxAmountInPot &&
	                  registry.Get<Mesh>(*_held).id == resources::HashIdentifier(pots[static_cast<size_t>(PotInfo::HandWood)].meshId);
	const auto& info = pots[static_cast<size_t>(wood ? PotInfo::HandWood : PotInfo::HandFood)];
	// A game turn is 1/10 s. The amount per turn ramps from PerTurn to PerTurnEnd over multiPickUpRampTime.
	constexpr float k_TurnSeconds = 0.1f;
	_pickTime += seconds;
	_pickTurnAccumulator += seconds;
	while (_pickTurnAccumulator >= k_TurnSeconds)
	{
		_pickTurnAccumulator -= k_TurnSeconds;
		const float ramp = info.multiPickUpRampTime > 0.0f ? std::clamp(_pickTime / info.multiPickUpRampTime, 0.0f, 1.0f) : 1.0f;
		const auto perTurn = static_cast<uint32_t>(
		    static_cast<float>(info.amountPickedUpPerTurn) +
		    (static_cast<float>(info.amountPickedUpPerTurnEnd) - static_cast<float>(info.amountPickedUpPerTurn)) * ramp);
		const auto room = std::min<uint32_t>(pile->maxAmount - std::min<uint32_t>(pile->amount, pile->maxAmount),
		                                     info.maxAmountCanBePickedUp);
		const auto take = std::min({perTurn, static_cast<uint32_t>(source->amount), room});
		if (take == 0)
		{
			break;
		}
		source->amount = static_cast<uint16_t>(source->amount - take);
		pile->amount = static_cast<uint16_t>(pile->amount + take);
	}
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		static float traceTime = 0.0f;
		traceTime += seconds;
		if (traceTime > 0.5f)
		{
			traceTime = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick trace: t={:.1f}s hand pile {} / {} (fill {:.0f}%), source left {}", _pickTime,
			                   pile->amount, pile->maxAmount, HeldFill() * 100.0f, source->amount);
		}
	}
}

void HandSystem::Throw(glm::vec3 velocity) noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(*_held))
	{
		// Some lift so a flick of the hand becomes an arc.
		velocity.y = std::max(velocity.y, 0.35f * glm::length(glm::vec2(velocity.x, velocity.z)));
		_thrown.push_back({*_held, velocity, _heldAltitude});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: thrown at ({:.1f}, {:.1f}, {:.1f}) u/s", velocity.x, velocity.y, velocity.z);
	}
	_held.reset();
	_pickSource.reset();
}

void HandSystem::UpdateThrown(float seconds) noexcept
{
	if (_thrown.empty() || seconds <= 0.0f)
	{
		return;
	}
	constexpr float k_Gravity = 30.0f;
	constexpr float k_AirDrag = 0.4f;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* terrain = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	for (auto& thrown : _thrown)
	{
		if (!registry.Valid(thrown.entity))
		{
			thrown.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(thrown.entity);
		thrown.velocity.y -= k_Gravity * seconds;
		thrown.velocity *= std::exp(-k_AirDrag * seconds);
		transform.position += thrown.velocity * seconds;
		const float ground = terrain != nullptr ? terrain->GetHeightAt(glm::vec2(transform.position.x, transform.position.z)) : 0.0f;
		if (transform.position.y <= ground + thrown.altitude && thrown.velocity.y < 0.0f)
		{
			transform.position.y = ground + thrown.altitude;
			if (auto* fixed = registry.TryGet<Fixed>(thrown.entity); fixed != nullptr)
			{
				fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
			}
			thrown.entity = entt::null;
		}
	}
	std::erase_if(_thrown, [](const Thrown& thrown) { return thrown.entity == entt::null; });
	registry.SetDirty();
}
