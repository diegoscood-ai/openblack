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
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
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
using namespace openblack::ecs::systems::hand_detail;

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
	// Debug: OPENBLACK_HAND_GRIP_PROBE=1 logs where the fist of the hold poses is relative to the model origin.
	if (std::getenv("OPENBLACK_HAND_GRIP_PROBE") != nullptr)
	{
		for (const auto& clip : _animator->ListClips())
		{
			if (clip.name != "Chold_side" && clip.name != "Chold_above" && clip.name != "Cwiggle")
			{
				continue;
			}
			const auto n = std::max<uint32_t>(1, clip.frameCount);
			for (const float f : {0.0f, 0.25f * static_cast<float>(n - 1), 0.5f * static_cast<float>(n - 1), static_cast<float>(n - 1)})
			{
				const float t = n > 1 ? f * static_cast<float>(clip.durationMs) / static_cast<float>(n - 1) : 0.0f;
				const auto free = _animator->Evaluate(clip.name, t);
				auto locked = free;
				if (locked.size() == bind.size())
				{
					const auto correction = bind[root] * glm::affineInverse(locked[root]);
					for (auto& m : locked)
					{
						m = correction * m;
					}
				}
				const std::array<std::pair<const char*, const std::vector<glm::mat4>*>, 2> variants {
				    std::pair {"free", &free}, std::pair {"locked", &locked}};
				for (const auto& [label, bones] : variants)
				{
					const auto p = centroid(k_PalmBones, *bones);
					const auto tips = centroid(k_TipBones, *bones);
					const auto rootT = glm::vec3((*bones)[root][3]);
					SPDLOG_LOGGER_INFO(logger, "Grip probe {} f={:.1f}/{} {}: palm {} tips {} root {}", clip.name, f, n, label,
					                   fmt::format("({:.0f},{:.0f},{:.0f})", p.x, p.y, p.z),
					                   fmt::format("({:.0f},{:.0f},{:.0f})", tips.x, tips.y, tips.z),
					                   fmt::format("({:.0f},{:.0f},{:.0f})", rootT.x, rootT.y, rootT.z));
				}
			}
		}
	}
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
	_lastDt = static_cast<float>(dt.count()) / 1e6f;
	// CHand::SetDistanceFromView (0x46C0D0) with the distance from the camera to the point under the cursor, and the
	// model scale k = 3.2 * handScale / 555.294 (the bind joint Y extent of Hand_Boned_Base2): the hand is 3.2 units long.
	if (groundPoint && Locator::camera::has_value())
	{
		const float d = glm::clamp(glm::distance(Locator::camera::value().GetOrigin(), *groundPoint), 2.0f, 1800.0f);
		_handScale = d < 10.0f ? std::pow(d / 10.0f, 0.8f) : 1.0f;
		if (d > 150.0f)
		{
			_handScale *= (d / 150.0f) * (1.0f - 0.3f * (d - 150.0f) / (1800.0f - 150.0f));
		}
		transform.scale = glm::vec3(3.2f * _handScale / 555.294f);
	}
	const float scale = transform.scale.x;

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
			SplashHand(*_gripPoint);
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

	auto rotation = FrameRotation(cameraForward);
	auto position = *groundPoint + glm::vec3(0.0f, _tipClearance, 0.0f) - rotation * (_hotspot * scale);
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// Side grip for trees: the hand rolls to +-pi/2 (Zoomer, speed 0.4) and floats at
	// 0.6 * (max(lowering, 1.9) + 0.1 * height) with lowering = max(0.1 * height, 3.2 * 0.3) (0x5B3E30, fn_0046DC30).
	// HandStateHolding::Update rolls to +-pi/2 (tree/side/villager) or +-pi (above) only while an object is being
	// given to the creature. TODO: creature give.
	const float rollTarget = 0.0f;
	_roll += (rollTarget - _roll) * (1.0f - std::exp(-seconds * 8.0f));
	if (std::abs(_roll) > 1e-3f)
	{
		const auto forward = rotation * _frameFingers;
		rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), _roll, forward)) * rotation;
		position = *groundPoint + glm::vec3(0.0f, _tipClearance, 0.0f) - rotation * (_hotspot * scale);
	}
	if ((_held || _tug) && _holdType != HoldType::None)
	{
		// HandStateHolding::Update: the hand model origin IS the grip point (CHand+0x78); the hold animation
		// (root bone animated) wraps palm and fingers around it. Hand matrix (fn_0046E160): model X = side,
		// Y = side x up, Z = -up, with d = back along the camera-to-cursor ray (Heading) and side = d x up'.
		// Height above the ground (jump table 0x5B568C): ABOVE 0.2, TREE/SIDE/VILLAGER max(lowering, 1.9), plus
		// 0.1 * height for rooted objects.
		const float lowering = _loweringMultiplier * _heldHeight; // GetHeight() * GetHoldLoweringMultiplier()
		const float height = (_holdType == HoldType::Above ? 0.2f : std::max(lowering, 1.9f)) + (_rooted ? 0.1f * _heldHeight : 0.0f);
		auto grip = _tug ? registry.Get<Transform>(*_tug).position +
		                       registry.Get<Transform>(*_tug).rotation * glm::vec3(0.0f, lowering, 0.0f)
		                 : *groundPoint + glm::vec3(0.0f, height, 0.0f);
		if (_pickSource && registry.Valid(*_pickSource))
		{
			// HandStateHolding::Update (0x5B3EA8) while a pile is locked: x,z frozen, y = ground + pile GetHeight().
			float pileHeight = 1.0f;
			auto& meshes = Locator::resources::value().GetMeshes();
			if (const auto* mesh = registry.TryGet<const Mesh>(*_pickSource); mesh != nullptr && meshes.Contains(mesh->id))
			{
				pileHeight = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * registry.Get<Transform>(*_pickSource).scale.y;
			}
			grip = glm::vec3(_pickLock.x,
			                 Locator::terrainSystem::value().GetHeightAt(glm::vec2(_pickLock.x, _pickLock.z)) + pileHeight,
			                 _pickLock.z);
		}
		glm::vec3 back(0.0f, 0.0f, 1.0f);
		if (Locator::camera::has_value())
		{
			const auto ray = *groundPoint - Locator::camera::value().GetOrigin();
			if (glm::length(glm::vec2(ray.x, ray.z)) > 1e-4f)
			{
				back = -glm::normalize(glm::vec3(ray.x, 0.0f, ray.z));
			}
		}
		const auto up = HeldSway(grip) * glm::vec3(0.0f, 1.0f, 0.0f);
		const auto side = glm::normalize(glm::cross(back, up));
		rotation = glm::mat3(side, glm::cross(side, up), -up);
		position = grip;
	}
	// No vertex (wrist included) below the landscape: lift just enough (our addition; not while holding a tree,
	// where the original places the hand origin at the grip point).
	if (Locator::terrainSystem::has_value() && !_held && !_tug)
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
	if (_held && _releaseArmed && _smoothedPosition)
	{
		// HandStateHolding::Update: spring in fixed 10 ms steps, a = 260 d - 40 v, |v| <= 124. Its velocity is also
		// the throw velocity (CHand+0x48C8). It only switches on while the interface is in action state 12 "IN THROW"
		// (cursor state 0x17, table 0x5D7960): the second press with the object in the hand. After the press that
		// picked the object up, the hand simply follows the cursor.
		if (!_springActive)
		{
			_springActive = true;
			_springTime = 0.0f;
			_springVelocity = glm::vec3(0.0f);
		}
		_springTime += seconds;
		while (_springTime >= 0.01f)
		{
			_springTime -= 0.01f;
			const auto d = position - *_smoothedPosition;
			_springVelocity += (d * 260.0f - _springVelocity * 40.0f) * 0.01f;
			const float speed = glm::length(_springVelocity);
			if (speed > 124.0f)
			{
				_springVelocity *= 124.0f / speed;
			}
			*_smoothedPosition += _springVelocity * 0.01f;
		}
		_handVelocity = _springVelocity;
	}
	else
	{
		_springActive = false;
		_smoothedPosition = position;
	}
	transform.rotation = rotation;
	transform.position = *_smoothedPosition;
	_interactionPoint = groundPoint;
	UpdateHeldObject();
	registry.SetDirty();
}

std::optional<HandSystem::CursorHit> HandSystem::PickObjectAlongRay(const glm::vec3& origin, const glm::vec3& dir) const noexcept
{
	// GInterface::SendObjectDrawCollision 0x5D56C0: every drawn object is tested with an exact triangle collide
	// (LH3DObject::CheckTriangleCollide) and the nearest wins; objects in the hand are skipped. Villagers (the LH3D
	// "human" flag, set only by Villager::CallVirtualFunctionsForCreation 0x74FC70) have no triangle test: they count
	// when the mouse is inside the screen circle of their bounding sphere (LH3DBoundingBox::CheckRegionOnScreen
	// 0x868C80), at the distance |centre - camera| - (R + near clip).
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& terrain = Locator::terrainSystem::value();
	glm::vec3 forward = dir;
	float nearClip = 0.3f;
	if (Locator::camera::has_value())
	{
		forward = glm::normalize(Locator::camera::value().GetForward());
		// LandFeature::GetNearClipping 0x5E2F30: 0.3 + 0.16 x the camera's height over the land, 0.3..3.5
		const float h = origin.y - terrain.GetHeightAt(glm::vec2(origin.x, origin.z));
		nearClip = h <= 0.0f ? 0.3f : h > 20.0f ? 3.5f : 0.3f + 0.16f * h;
	}
	std::optional<CursorHit> best;
	float bestT = std::numeric_limits<float>::max();
	const auto skip = [&](entt::entity entity) {
		if (entity == _hands[0] || entity == _hands[1] || (_held && entity == *_held) || (_tug && entity == *_tug))
		{
			return true;
		}
		// Anything that moves with the hand would pull the hand towards the camera frame after frame: the pick-up
		// particles and the roots hanging under a tree out of the map.
		if (std::any_of(_roots.begin(), _roots.end(), [entity](const auto& pair) { return pair.second == entity; }))
		{
			return true;
		}
		return std::any_of(_pickupParticles.begin(), _pickupParticles.end(),
		                   [entity](const PickupParticle& particle) { return particle.entity == entity; });
	};
	registry.Each<const Transform, const Mesh>([&](entt::entity entity, const Transform& transform, const Mesh& mesh) {
		if (!meshes.Contains(mesh.id) || skip(entity))
		{
			return;
		}
		const auto l3d = meshes.Handle(mesh.id);
		const auto box = l3d->GetBoundingBox();
		if (box.minima.x > box.maxima.x)
		{
			return;
		}
		// Height-map objects are drawn glued to the landscape (plus a pile's sink offset), not at their stored y.
		glm::vec3 position = transform.position;
		if (registry.AllOf<MorphWithTerrain>(entity))
		{
			const auto* sink = registry.TryGet<const PileSink>(entity);
			position.y = terrain.GetHeightAt(glm::vec2(position.x, position.z)) + (sink != nullptr ? sink->offset.value : 0.0f);
		}
		const auto centre = position + transform.rotation * (transform.scale * box.Center());
		const float radius = 0.5f * glm::length(transform.scale * box.Size());
		if (registry.AllOf<Villager>(entity))
		{
			// the mouse ray at the view depth of the centre, against the sphere there (the projected circle)
			const float depth = glm::dot(centre - origin, forward);
			const float along = glm::dot(dir, forward);
			if (depth + radius < nearClip || along <= 0.0f)
			{
				return;
			}
			const auto onPlane = origin + dir * (depth / along);
			if (glm::distance(onPlane, centre) > radius && glm::distance(origin, centre) > radius)
			{
				return;
			}
			const glm::vec3 top(position.x, position.y + 0.5f * box.Size().y * transform.scale.y, position.z);
			const float d = glm::distance(top, origin) - (radius + nearClip);
			if (d < bestT)
			{
				bestT = d;
				const auto lo = position + transform.rotation * (transform.scale * box.minima);
				const auto hi = position + transform.rotation * (transform.scale * box.maxima);
				best = CursorHit {entity, d, glm::min(lo, hi), glm::max(lo, hi)};
			}
			return;
		}
		// Cheap reject: the ray against the bounding sphere.
		const auto oc = origin - centre;
		const float b = glm::dot(oc, dir);
		const float c = glm::dot(oc, oc) - radius * radius;
		if ((c > 0.0f && b > 0.0f) || b * b - c < 0.0f || -b - radius > bestT)
		{
			return;
		}
		// Exact test in mesh space: world = position + R * (S * local).
		const auto toLocal = glm::inverse(transform.rotation);
		const auto localOrigin = (toLocal * (origin - position)) / transform.scale;
		const auto localDir = (toLocal * dir) / transform.scale;
		const auto t = l3d->RayIntersect(localOrigin, localDir);
		if (t && *t < bestT)
		{
			bestT = *t;
			const auto lo = position + transform.rotation * (transform.scale * box.minima);
			const auto hi = position + transform.rotation * (transform.scale * box.maxima);
			best = CursorHit {entity, *t, glm::min(lo, hi), glm::max(lo, hi)};
		}
	});
	return best;
}

std::optional<glm::vec3> HandSystem::ResolveCursorPoint(const glm::vec3& origin, const glm::vec3& direction,
                                                        std::optional<glm::vec3> land, bool gripping,
                                                        std::chrono::microseconds dt) noexcept
{
	_cursorObject.reset();
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	const auto mouseDir = glm::normalize(direction);
	_mouseRayOrigin = origin;
	_mouseRayDirection = mouseDir;
	const std::optional<float> landDistance = land ? std::optional(glm::distance(origin, *land)) : std::nullopt;
	if (gripping)
	{
		// The camera states place the hand themselves; HandStateNormal::Enter resets the zoomer afterwards.
		_handDistanceValid = false;
		return land;
	}
	auto& registry = Locator::entitiesRegistry::value();

	// While holding, the object ray aims at the land point under the mouse raised by the aim height
	// (g_D13F4C = hold height * 0.6; ORHP 0x5B5E70).
	auto dir = mouseDir;
	if ((_held || _tug) && land)
	{
		const float lowering = _loweringMultiplier * _heldHeight;
		const float h = (_holdType == HoldType::Above ? 0.2f : std::max(lowering, 1.9f)) + (_rooted ? 0.1f * _heldHeight : 0.0f);
		dir = glm::normalize(*land - origin + glm::vec3(0.0f, h * 0.6f, 0.0f));
	}

	// UpdateInterfaceCollide 0x5D5A70: the land pick counts 2.3 further than it is (fn_005D5980); when it is still
	// nearer than the object, the object is kept only if the land point lies inside its XZ bounding-box footprint.
	auto hit = PickObjectAlongRay(origin, dir);
	if (hit && landDistance && *landDistance + 2.3f < hit->t)
	{
		const auto& l = *land;
		if (l.x < hit->boxMin.x || l.x > hit->boxMax.x || l.z < hit->boxMin.z || l.z > hit->boxMax.z)
		{
			hit.reset();
		}
	}

	if (_tug && std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Cursor trace: hit {} t {:.2f} land {:.2f} zoomer {:.2f}", hit ? static_cast<int>(hit->entity) : -1,
		                   hit ? hit->t : -1.0f, landDistance.value_or(-1.0f), _handDistance.value);
	}
	std::optional<glm::vec3> pos = land;
	if (hit)
	{
		_cursorObject = hit->entity;
		const auto& transform = registry.Get<const Transform>(hit->entity);
		if (registry.AnyOf<Villager, Animal>(hit->entity))
		{
			// Living (not a creature): hover just in front of it, at the distance of its centre minus its 2D radius,
			// then corrected for the height difference along the ray.
			float radius2D = 0.0f;
			auto& meshes = Locator::resources::value().GetMeshes();
			if (const auto* mesh = registry.TryGet<const Mesh>(hit->entity); mesh != nullptr && meshes.Contains(mesh->id))
			{
				const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * transform.scale;
				radius2D = 0.5f * std::max(size.x, size.z);
			}
			const auto& op = transform.position;
			const float d = glm::distance(op, origin) - radius2D;
			const auto p1 = origin + mouseDir * d;
			pos = origin + mouseDir * ((p1.y - op.y) * mouseDir.y + d);
		}
		else
		{
			// Mesh intersection: the surface point; while holding, the same depth on the mouse ray, pulled towards
			// the camera by half the held object's 2D radius so that it does not sink into the surface.
			const float s = hit->t;
			auto p = origin + (_held || _tug ? mouseDir : dir) * s;
			if (s < 1.0f)
			{
				p = origin + dir;
			}
			if (_held && registry.Valid(*_held))
			{
				float heldRadius = 0.0f;
				auto& meshes = Locator::resources::value().GetMeshes();
				if (const auto* mesh = registry.TryGet<const Mesh>(*_held); mesh != nullptr && meshes.Contains(mesh->id))
				{
					const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * registry.Get<const Transform>(*_held).scale;
					heldRadius = 0.5f * std::max(size.x, size.z);
				}
				p += glm::normalize(origin - p) * (heldRadius * 0.5f);
			}
			pos = p;
		}
	}
	if (!pos)
	{
		return std::nullopt;
	}

	// g_HandDistZoomer: towards the distance of the surface, in 0.1 s when coming closer and 0.28 s when moving away
	// (2.0 s only while a creature give or a CameraModeNew3 move is pending, which openblack does not have); at least 1.
	const float target = std::max(1.0f, glm::distance(origin, *pos));
	if (!_handDistanceValid)
	{
		_handDistance.SetPosition(target);
		_handDistanceValid = true;
	}
	_handDistance.SetDestinationWithSpeedAndTime(target, 0.0f, target < _handDistance.value ? 0.1f : 0.28f);
	_handDistance.Update(seconds);
	if (_handDistance.value < 1.0f)
	{
		_handDistance.SetPosition(1.0f);
	}
	// Never beyond the land under the cursor (the view distance of CHand::fn_0046DF60).
	const float distance = landDistance ? std::min(_handDistance.value, *landDistance) : _handDistance.value;
	return origin + mouseDir * distance;
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
	// The interaction object is the one under the cursor (SendObjectDrawCollision: exact triangle pick) when it can be
	// picked up. FindObjectNearMapCoord (0x5D39E0, +-5 units) is only a fallback for a click that hit nothing, and it
	// only takes an object nearer than the clicked point (it is not a hover reach).
	std::optional<entt::entity> best;
	if (_cursorObject && registry.Valid(*_cursorObject) && registry.AnyOf<Mobile, Tree, DeadTree, Pot, Field, BigForest>(*_cursorObject) &&
	    _hands[0] != *_cursorObject && _hands[1] != *_cursorObject)
	{
		best = *_cursorObject;
		// Rock::ValidForPlaceInHand: boulders with a 2D radius over 3.6 cannot be lifted, but they stay the target of the
		// action button when they can be tapped (StartGrab 0x5D1740 goes straight to Tap).
		if (Rocks::IsRock(*best) && !Rocks::ValidForPlaceInHand(*best) && !Rocks::ValidToTap(*best))
		{
			best.reset();
		}
	}
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
