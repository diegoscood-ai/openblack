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

namespace
{
void PlaySample(audio::SoundId id)
{
	const auto soundId = static_cast<entt::id_type>(id);
	if (Locator::audio::has_value() && Locator::resources::value().GetSounds().Contains(soundId))
	{
		Locator::audio::value().PlaySound(soundId, audio::PlayType::Once);
	}
}

/// MapCoords::IsLand (0x603720): the landscape cell under the point does not have the water bit.
bool IsLand(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto& terrain = Locator::terrainSystem::value();
	const auto cell = glm::u16vec2(glm::max(glm::vec2(point.x, point.z) / LandIslandInterface::k_CellSize, glm::vec2(0.0f)));
	const auto& properties = terrain.GetCell(cell).properties;
	return properties.hasWater == 0;
}
} // namespace

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
			const auto& trees = Locator::infoConstants::value().tree;
			for (size_t i = 0; i < trees.size(); ++i)
			{
				const auto& t = trees[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "TreeInfo {} '{}': mesh={} growing={} burning={} strength={} defence={} startLife={} wood={} food={} "
				                   "weight={} carried={} minSize={} maxSize={} grows={} growth={} immersion={} collide={} "
				                   "helpInHand={} maxTrees={}",
				                   i, t.debugString.data(), static_cast<int>(t.normal), static_cast<int>(t.growing),
				                   static_cast<int>(t.burning), t.strength, t.defence, t.startLife, t.woodValue, t.foodValue, t.weight,
				                   static_cast<int>(t.carriedType), t.minSize, t.maxSize, t.growsAfterNumGameTurns, t.growthAmount,
				                   static_cast<int>(t.immersion), static_cast<int>(t.collideSound), static_cast<int>(t.helpInHand),
				                   t.maxNumTreesCanProduce);
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
	// CHand mouse smoothing: smooth += dt * vel, clamped to +-80 px of the mouse, vel = (vel + (mouse - smooth) * 20 dt)
	// * 0.03^dt. Pixels are taken on a 1024-wide screen (the original ran at 640-1024 wide; the reference is a guess).
	if (Locator::windowing::has_value())
	{
		const float width = static_cast<float>(std::max(1, Locator::windowing::value().GetSize().x));
		_mouse += mouseDelta * (1024.0f / width);
	}
	if (_smoothMouseValid && seconds > 0.0f)
	{
		_smoothMouse = glm::clamp(_smoothMouse + seconds * _smoothMouseVelocity, _mouse - 80.0f, _mouse + 80.0f);
		_smoothMouseVelocity = (_smoothMouseVelocity + (_mouse - _smoothMouse) * 20.0f * seconds) * std::pow(0.03f, seconds);
	}
	else
	{
		_smoothMouse = _mouse;
		_smoothMouseVelocity = glm::vec2(0.0f);
		_smoothMouseValid = true;
	}
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
				if (std::getenv("OPENBLACK_HAND_TEST_NO_BOULDER") == nullptr)
				archetypes::MobileStaticArchetype::Create(glm::vec3(x + 6.0f, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + 6.0f, z)), z), MobileStaticInfo::Boulder1Chalk, 0.0f, 0.0f, 0.0f,
				                                          0.0f, 1.0f);
				Locator::entitiesRegistry::value().SetDirty();
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: boulder spawned at ({}, {}, {})", x, y, z);
				// ...and a wood pile next to it, to test the multi pick up (HandWood) and special_hold fill.
				const float px = x;
				const float py = Locator::terrainSystem::value().GetHeightAt(glm::vec2(px, z));
				// OPENBLACK_HAND_TEST_FOOD=1 spawns a food pile instead.
				const bool food = std::getenv("OPENBLACK_HAND_TEST_FOOD") != nullptr;
				archetypes::PotArchetype::Create(glm::vec3(px, py, z), 0.0f, food ? PotInfo::MagicFood : PotInfo::WoodPile_1, food ? 1000 : 4000);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: {} pile spawned at ({}, {}, {})", food ? "food" : "wood", px, py, z);
			}
		}
		// Debug: OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz" flies the camera there (close-up screenshots).
		if (const char* fly = std::getenv("OPENBLACK_CAMERA_FLY"); fly != nullptr && Locator::camera::has_value())
		{
			glm::vec3 o(0.0f);
			glm::vec3 f(0.0f);
			if (std::sscanf(fly, "%f,%f,%f,%f,%f,%f", &o.x, &o.y, &o.z, &f.x, &f.y, &f.z) == 6)
			{
				Locator::camera::value().GetModel().SetFlight(o, f);
			}
		}
		// Debug: OPENBLACK_PRINT_ALTITUDE="x,z" logs the landscape height there (LH3DIsland::GetAltitude).
		if (const char* at = std::getenv("OPENBLACK_PRINT_ALTITUDE"); at != nullptr)
		{
			float x = 0.0f;
			float z = 0.0f;
			if (std::sscanf(at, "%f,%f", &x, &z) == 2)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Altitude at ({}, {}): {:.7f}", x, z,
				                   Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)));
			}
		}
		// Debug: OPENBLACK_DUMP_STATIC_GAPS=1 logs, for every MobileStatic, the gap between its lowest vertex and the
		// landscape under it (negative = buried), with its rotation and with the transposed rotation.
		if (std::getenv("OPENBLACK_DUMP_STATIC_GAPS") != nullptr)
		{
			auto& registry = Locator::entitiesRegistry::value();
			auto& meshes = Locator::resources::value().GetMeshes();
			const auto& terrain = Locator::terrainSystem::value();
			registry.Each<const Transform, const Mesh, const MobileStatic>(
			    [&](entt::entity, const Transform& t, const Mesh& mesh, const MobileStatic& statics) {
				    if (!meshes.Contains(mesh.id))
				    {
					    return;
				    }
				    const auto l3d = meshes.Handle(mesh.id);
				    float gapA = std::numeric_limits<float>::max();
				    float gapB = std::numeric_limits<float>::max();
				    const auto rt = glm::transpose(t.rotation);
				    for (const auto& sub : l3d->GetSubMeshes())
				    {
					    for (const auto& v : sub->GetCollisionPositions())
					    {
						    const auto a = t.position + t.rotation * (t.scale * v);
						    const auto b = t.position + rt * (t.scale * v);
						    gapA = std::min(gapA, a.y - terrain.GetHeightAt(glm::vec2(a.x, a.z)));
						    gapB = std::min(gapB, b.y - terrain.GetHeightAt(glm::vec2(b.x, b.z)));
					    }
				    }
				    SPDLOG_LOGGER_INFO(spdlog::get("game"), "Static gap: type {} mesh {} at ({:.2f},{:.2f}) scale {:.3f} gap {:.3f} transposed {:.3f} localMinY {:.3f}",
				                       static_cast<int>(statics.type),
				                       static_cast<int>(Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics.type)).meshId),
				                       t.position.x, t.position.z, t.scale.x, gapA, gapB, l3d->GetBoundingBox().minima.y);
			    });
		}
		// Debug: OPENBLACK_HAND_TEST_STORE_TAKE="wood,food" takes that much from every storage pit (store visuals).
		if (const char* take = std::getenv("OPENBLACK_HAND_TEST_STORE_TAKE"); take != nullptr)
		{
			uint32_t wood = 0;
			uint32_t food = 0;
			if (std::sscanf(take, "%u,%u", &wood, &food) == 2)
			{
				std::vector<entt::entity> stores;
				Locator::entitiesRegistry::value().Each<const StoragePit>([&](entt::entity e, const StoragePit&) { stores.push_back(e); });
				for (const auto store : stores)
				{
					const auto w = archetypes::AbodeArchetype::RemoveFromStoragePit(store, ResourceType::Wood, wood);
					const auto f = archetypes::AbodeArchetype::RemoveFromStoragePit(store, ResourceType::Food, food);
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store took wood {} food {}, left wood {} food {}", w, f,
					                   archetypes::AbodeArchetype::StoragePitAmount(store, ResourceType::Wood),
					                   archetypes::AbodeArchetype::StoragePitAmount(store, ResourceType::Food));
				}
			}
		}
		// Debug: OPENBLACK_HAND_TEST_TREE="x,z" plants a beech there, and logs the nearest village store.
		if (const char* at = std::getenv("OPENBLACK_HAND_TEST_TREE"); at != nullptr)
		{
			float x = 0.0f;
			float z = 0.0f;
			if (std::sscanf(at, "%f,%f", &x, &z) == 2)
			{
				const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
				archetypes::TreeArchetype::Create(1, position, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
				Locator::entitiesRegistry::value().SetDirty();
				float best = std::numeric_limits<float>::max();
				glm::vec3 store(0.0f);
				Locator::entitiesRegistry::value().Each<const StoragePit, const Transform>(
				    [&](entt::entity, const StoragePit&, const Transform& t) {
					    if (glm::distance(t.position, position) < best)
					    {
						    best = glm::distance(t.position, position);
						    store = t.position;
					    }
				    });
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: beech planted at ({}, {}); nearest store at ({:.1f}, {:.1f}) d={:.1f}",
				                   x, z, store.x, store.z, best);
				// OPENBLACK_HAND_TEST_TREE="x,z,dead": a second beech 8 m away, felled as if thrown towards +x.
				if (std::strstr(at, "dead") != nullptr)
				{
					const glm::vec3 p2(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z + 8.0f)), z + 8.0f);
					MakeDeadTree(archetypes::TreeArchetype::Create(1, p2, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f),
					             glm::vec3(1.0f, 0.0f, 0.0f));
				}
				// OPENBLACK_HAND_TEST_TREE="x,z,roots": MSH_T_ROOTS and MSH_T_ROOTS_PILE next to the tree, bounding boxes logged.
				if (std::strstr(at, "roots") != nullptr)
				{
					auto& registry = Locator::entitiesRegistry::value();
					auto& meshes = Locator::resources::value().GetMeshes();
					for (const auto& [id, dx] : {std::pair {MeshId::TreeRoots, -6.0f}, std::pair {MeshId::TreeRootsPile, 6.0f},
					                            std::pair {MeshId::TreeBeech, 12.0f}})
					{
						const auto meshId = resources::HashIdentifier(id);
						if (!meshes.Contains(meshId))
						{
							continue;
						}
						const auto& box = meshes.Handle(meshId)->GetBoundingBox();
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: mesh {} box min ({:.2f},{:.2f},{:.2f}) max ({:.2f},{:.2f},{:.2f})",
						                   static_cast<int>(id), box.minima.x, box.minima.y, box.minima.z, box.maxima.x, box.maxima.y,
						                   box.maxima.z);
						if (dx != 0.0f)
						{
							const auto e = registry.Create();
							const glm::vec3 p(x + dx, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + dx, z)), z);
							registry.Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
							registry.Assign<Mesh>(e, meshId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
							if (std::strstr(at, "alpha") != nullptr)
							{
								registry.Assign<Alpha>(e, std::getenv("OPENBLACK_TEST_ALPHA") ? static_cast<float>(std::atof(std::getenv("OPENBLACK_TEST_ALPHA"))) : 0.5f);
							}
						}
					}
					registry.SetDirty();
				}
				// OPENBLACK_HAND_TEST_TREE="x,z,store": an oak put straight into the nearest village store.
				if (std::strstr(at, "store") != nullptr)
				{
					const auto oak = archetypes::TreeArchetype::Create(1, store, TreeInfo::Oak, true, 0.0f, 1.0f, 1.0f);
					if (const auto pit = FindWoodStore(store); pit)
					{
						const auto dump = [&](const char* when) {
							std::string piles;
							for (const auto pile : Locator::entitiesRegistry::value().Get<StoragePit>(*pit).woodPiles)
							{
								piles += fmt::format(" {}", Locator::entitiesRegistry::value().Get<Pot>(pile).amount);
							}
							SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store wood piles {}:{} abode wood {}", when, piles,
							                   Locator::entitiesRegistry::value().Get<Abode>(*pit).woodAmount);
						};
						dump("before");
						DepositInStore(oak, *pit);
						dump("after");
					}
				}
			}
		}
	}

	// Pick up / drop with the action button (right). Only while not gripping the land.
	if (_held && !Locator::entitiesRegistry::value().Valid(*_held))
	{
		_held.reset();
	}
	_hovered = (_held || _tug || gripping) ? std::nullopt : FindObjectUnderHand();
	const bool actionPressed = actionHeld && !_actionWasHeld;
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	if (actionPressed && _hovered && !_held)
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once (StartGrab -> packet 0x1B).
		const auto source = PotInfoOf(*_hovered);
		if (source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood)
		{
			PickUp(*_hovered);
			_pickPressHeld = _held.has_value();
		}
		else
		{
			_pendingPick = _hovered;
			_pendingPickTime = 0.0f;
		}
	}
	else if (actionPressed && _held && !_pickPressHeld)
	{
		// GInterface: with an object in the hand, press the action button again, move and release to put it down or
		// hurl it (state 12, 0x5D4DB0).
		_releaseArmed = true;
	}
	if (actionReleased && _pickPressHeld)
	{
		// Releasing the press that picked it up ends the grab / scooping (packet 0x1C); the object stays in the hand.
		_pickPressHeld = false;
		_pickSource.reset();
	}
	else if (actionReleased && _held && _releaseArmed)
	{
		_releaseArmed = false;
		// Released while moving fast: throw it (the hand velocity carries on), otherwise put it down.
		// Object::InitialisePhysicsFromHand throws when vel.x^2 + vel.z^2 > 4; the velocity is the holding spring's
		// (CHand+0x48C8, units per second, capped at 124).
		constexpr float k_ThrowSpeed = 2.0f;
		if (glm::length(glm::vec2(_handVelocity.x, _handVelocity.z)) > k_ThrowSpeed)
		{
			Throw(_handVelocity);
		}
		else
		{
			Drop();
		}
	}
	if (_pendingPick)
	{
		// The grab completes after 225 ms of holding the action button (fn_005D5250). Released earlier it is a
		// tap, which does nothing for objects (Object::InterfaceValidToTap returns false).
		constexpr float k_PickUpHoldSeconds = 0.225f;
		_pendingPickTime += seconds;
		if (!actionHeld || !Locator::entitiesRegistry::value().Valid(*_pendingPick))
		{
			_pendingPick.reset();
		}
		else if (_pendingPickTime >= k_PickUpHoldSeconds)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (Locator::entitiesRegistry::value().AllOf<Tree>(entity) && _interactionPoint)
			{
				_tug = entity;
				ComputeHoldParameters(entity);
				_tugPoint = *_interactionPoint;
				_tugRotation = Locator::entitiesRegistry::value().Get<Transform>(entity).rotation;
				_hovered.reset();
			}
			else
			{
				PickUp(entity);
				_pickPressHeld = _held.has_value();
			}
		}
	}
	UpdateTug(seconds, actionHeld);
	if (!_held && !_tug)
	{
		_releaseArmed = false;
		if (!actionHeld)
		{
			_pickPressHeld = false;
		}
	}
	UpdateMultiPickUp(seconds, actionHeld);
	UpdatePickupParticles(seconds, _pickSource.has_value() && _held.has_value() && std::getenv("OPENBLACK_NO_PICKUP_PSYS") == nullptr);
	UpdateThrown(seconds);
	UpdateRootsAndPiles(seconds);
	archetypes::PotArchetype::UpdateSizes(seconds);

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	std::string clip = gripping ? "Cgrip" : (_hovered ? "Ccan_pickup" : "Cwiggle");
	// HandStateHolding::Update (jump table 0x5B56A8): the pose and its frame (in ms) per hold type.
	//   ABOVE: Chold_above at dur * 0.5 * (1 - grip), grip = min(1, R / (3.2 s * 1.2))
	//   TREE / SIDE / VILLAGER: Chold_side at (dur >> 1) * grip, grip = min(1, R / (3.2 s))
	const bool holding = (_held.has_value() || _tug.has_value()) && _override.empty() && _holdType != HoldType::None;
	if (_held && PotInfoOf(*_held) == PotInfo::HandFood)
	{
		// PileFood::GetHoldRadius: Get2DRadius * q, q = 1 - (1 - p)^2, p = 0.05 + 0.95 * amount / 1600 (clamped):
		// the hand opens as the food in it grows. Wood keeps a constant radius.
		ComputeHoldParameters(*_held);
		const auto& pot = Locator::entitiesRegistry::value().Get<Pot>(*_held);
		const float p = std::clamp(0.05f + 0.95f * static_cast<float>(pot.amount) / 1600.0f, 0.0f, 1.0f);
		_holdRadius *= 1.0f - (1.0f - p) * (1.0f - p);
	}
	_animator->SetFrame(std::nullopt);
	if (holding && _holdType == HoldType::Above && _animator->Has("Chold_above"))
	{
		clip = "Chold_above";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale * 1.2f));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip)) * 0.5f * (1.0f - grip));
	}
	else if (holding && _holdType != HoldType::Above && _animator->Has("Chold_side"))
	{
		clip = "Chold_side";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip) >> 1) * grip);
	}
	else
	{
		_animator->SetTime(std::nullopt);
	}
	if (!_override.empty())
	{
		clip = _override;
	}
	// special_hold (Cphile/Chorn) is not used for held objects: hand piles are SIDE, Chorn is the spell seed grain.
	_animator->SetSpecialHold(std::nullopt, std::chrono::milliseconds(140));
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(clip == "Cgrip" ? 90 : 150));
	}
	// Grip drag lives mostly in the root translation of Lgrip_lr/Lgrip_fb.
	_animator->SetLayerTranslationScale(clip == "Cgrip" ? 1.0f : 0.0f);
	// The palm always faces the ground, except in the camera states where the root motion is the drag itself.
	// Cgrip and the hold poses animate the root bone (CAnim applies A_0 * R_0 to the root); the other poses are
	// placed by our own palm-down frame.
	const bool rootAnimated = clip == "Cgrip" || clip == "Chold_side" || clip == "Chold_above";
	_animator->SetRootLocked(!rootAnimated || _animator->IsSpecialHold());
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

void HandSystem::Zoomer::SetPosition(float position)
{
	value = destination = startValue = position;
	speed = destinationSpeed = startSpeed = 0.0f;
	time = duration = 0.0f;
	c2 = c3 = c4 = 0.0f;
}

void HandSystem::Zoomer::SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds)
{
	if (seconds < 0.001f)
	{
		SetPosition(target);
		return;
	}
	// Start from the current value and speed; at t = T: value = target, speed = targetSpeed, acceleration = 0.
	// Solved in normalised time (the inverse of [[1/24,1/6,1/2],[1/6,1/2,1],[1/2,1,1]] scaled by T).
	startValue = value;
	startSpeed = speed;
	destination = target;
	destinationSpeed = targetSpeed;
	duration = seconds;
	time = 0.0f;
	const float r1 = target - startValue - startSpeed * seconds;
	const float r2 = (targetSpeed - startSpeed) * seconds;
	const float e = 72.0f * r1 - 48.0f * r2;
	const float d = -2.0f * r2 - 2.0f * e / 3.0f;
	const float c = 2.0f * r2 + e / 6.0f;
	c2 = c / (seconds * seconds);
	c3 = d / (seconds * seconds * seconds);
	c4 = e / (seconds * seconds * seconds * seconds);
}

void HandSystem::Zoomer::Update(float seconds)
{
	time += seconds;
	if (time >= duration)
	{
		value = destination;
		speed = destinationSpeed;
		time = duration;
		return;
	}
	const float t = time;
	speed = startSpeed + c2 * t + c3 * t * t / 2.0f + c4 * t * t * t / 6.0f;
	value = startValue + startSpeed * t + c2 * t * t / 2.0f + c3 * t * t * t / 6.0f + c4 * t * t * t * t / 24.0f;
}

std::optional<HandSystem::CursorHit> HandSystem::PickObjectAlongRay(const glm::vec3& origin, const glm::vec3& dir) const noexcept
{
	// GInterface::SendObjectDrawCollision 0x5D56C0: every drawn object is tested with an exact triangle collide
	// (LH3DObject::CheckTriangleCollide) and the nearest wins; objects in the hand are skipped.
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& terrain = Locator::terrainSystem::value();
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
			position.y = terrain.GetHeightAt(glm::vec2(position.x, position.z)) + (sink != nullptr ? sink->offset : 0.0f);
		}
		// Cheap reject: the ray against the bounding sphere.
		const auto centre = position + transform.rotation * (transform.scale * box.Center());
		const float radius = 0.5f * glm::length(transform.scale * box.Size());
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

	// UpdateInterfaceCollide 0x5D5A70: when the land is nearer than the object, the object is kept only if the land
	// point lies inside its XZ bounding-box footprint.
	auto hit = PickObjectAlongRay(origin, dir);
	if (hit && landDistance && *landDistance < hit->t)
	{
		const auto& l = *land;
		if (l.x < hit->boxMin.x || l.x > hit->boxMax.x || l.z < hit->boxMin.z || l.z > hit->boxMax.z)
		{
			hit.reset();
		}
	}

	std::optional<glm::vec3> pos = land;
	if (hit)
	{
		_cursorObject = hit->entity;
		const auto& transform = registry.Get<const Transform>(hit->entity);
		if (registry.AllOf<Villager>(hit->entity))
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
	std::optional<entt::entity> best;
	float bestDistance = std::numeric_limits<float>::max();
	// The object under the cursor (triangle pick) is the interaction object when it can be picked up; otherwise the
	// nearest pickable object around the point (FindObjectNearMapCoord 0x5D39E0 uses a +-5 unit square).
	const bool cursorPickable = _cursorObject && registry.Valid(*_cursorObject) &&
	                            registry.AnyOf<Mobile, Tree, DeadTree, Pot>(*_cursorObject);
	registry.Each<const Transform, const Mesh>(
	    [&](entt::entity entity, const Transform& transform, const Mesh& mesh) {
		    // Pickable: mobile objects, trees and resource piles/pots (food, wood).
		    if (!registry.AnyOf<Mobile, Tree, DeadTree, Pot>(entity) || _hands[0] == entity || _hands[1] == entity)
		    {
			    return;
		    }
		    // Generous reach: the fingertip only needs to be over (or next to) the object.
		    float radius = 3.5f;
		    float radius2D = 0.0f;
		    if (meshes.Contains(mesh.id))
		    {
			    const auto size = meshes.Handle(mesh.id)->GetBoundingBox().Size() * transform.scale;
			    radius2D = 0.5f * std::max(size.x, size.z);
			    radius = std::max(3.5f, radius2D + 2.0f);
		    }
		    // Rock::ValidForPlaceInHand: boulders with a 2D radius over 3.6 cannot be lifted. Rocks are taken to be the
		    // MobileStatic types Rock and Boulder* .. Squarerock* (the Rock class in the original).
		    if (const auto* statics = registry.TryGet<const MobileStatic>(entity); statics != nullptr)
		    {
			    const auto type = static_cast<int>(statics->type);
			    const bool isRock = type == static_cast<int>(MobileStaticInfo::Rock) ||
			                        (type >= static_cast<int>(MobileStaticInfo::Boulder1Chalk) &&
			                         type <= static_cast<int>(MobileStaticInfo::SquarerockVolcanic));
			    if (isRock && radius2D > 3.6f)
			    {
				    return;
			    }
		    }
		    float distance = glm::distance(point, glm::vec2(transform.position.x, transform.position.z));
		    if (cursorPickable && entity == *_cursorObject)
		    {
			    distance = -1.0f; // wins over any proximity candidate
		    }
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
			// PotStructure::GetResource: a store pile offers the store's total.
			const auto store = archetypes::AbodeArchetype::StoragePitOfPile(entity);
			const auto resource = pots[static_cast<size_t>(sourceInfo)].resourceType;
			const uint32_t available = store != entt::null ? archetypes::AbodeArchetype::StoragePitAmount(store, resource) : pot->amount;
			const auto take = std::min<uint32_t>(available, handInfo.amountPickedUpInitially);
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
			if (store != entt::null)
			{
				archetypes::AbodeArchetype::RemoveFromStoragePit(store, resource, take);
			}
			else
			{
				pot->amount = static_cast<uint16_t>(pot->amount - take);
				SinkPile(entity);
			}
			_pickSource = entity;
			_pickTurns = 0;
			_pickLock = _interactionPoint.value_or(position);
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
	_heldRotation = transform.rotation;
	_heldTop = 1.0f;
	_heldHeight = 0.0f;
	_holdRadius = 0.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto& box = meshes.Handle(mesh->id)->GetBoundingBox();
		_heldTop = box.maxima.y * transform.scale.y;
		// Tree::GetHoldRadius = 0.2 * Get2DRadius.
		_heldHeight = _heldTop;
		_holdRadius = 0.2f * 0.5f * std::max(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	}
	if (registry.AllOf<Tree>(entity))
	{
		// Tree::InterfaceSetInMagicHand: uprooting cracks (LH_SAMPLE_G_TREEBREAK_01 + rand % 3). The player also
		// loses alignment (GPlayerInfo.treePullPutAlignmentChange). TODO: alignment once players track it.
		static constexpr auto k_TreeBreak = std::array<audio::SoundId, 3> {
		    audio::SoundId::G_TreeBreak_01_1, audio::SoundId::G_TreeBreak_02_1, audio::SoundId::G_TreeBreak_03_1};
		PlaySample(Locator::rng::value().Choose(k_TreeBreak));
		_heldAltitude = 0.0f;
	}
	else if (!registry.AllOf<DeadTree>(entity))
	{
		PlaySample(audio::SoundId::G_PickUpObject);
	}
	ComputeHoldParameters(entity);
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
	if (registry.Valid(*_held) && registry.AnyOf<Tree, DeadTree>(*_held))
	{
		const auto entity = *_held;
		_held.reset();
		_pickSource.reset();
		// Tree::ApplyThisToObject: dropped on a wood store it becomes its wood.
		if (const auto store = _interactionPoint ? FindWoodStore(*_interactionPoint) : std::nullopt; store)
		{
			DepositInStore(entity, *store);
		}
		else if (registry.AllOf<Tree>(entity))
		{
			ReleaseTree(entity);
		}
		else
		{
			auto& transform = registry.Get<Transform>(entity);
			transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
			if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
			{
				fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
			}
			registry.SetDirty();
		}
		return;
	}
	if (registry.Valid(*_held) && (PotInfoOf(*_held) == PotInfo::HandWood || PotInfoOf(*_held) == PotInfo::HandFood))
	{
		const auto pot = *_held;
		_held.reset();
		_pickSource.reset();
		PutDownHandPot(pot);
		return;
	}
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
	if (_holdType != HoldType::None)
	{
		// The grip point is the hand position itself (the model origin): obj.pos = hand->pos - lowering * up'.
		// The object matrix (fn_0046E2F0) is rebuilt from the hand: X = up' x side, Y = up', Z = side (the original is
		// mirrored, det -1; here a proper rotation), so the held object turns with the view.
		const auto centre = hand.position;
		const auto up = glm::normalize(-hand.rotation[2]);
		const auto side = glm::normalize(hand.rotation[0]);
		transform.rotation = glm::mat3(glm::cross(up, side), up, side);
		transform.position = centre - up * (_loweringMultiplier * _heldHeight);
		UpdateRoots(*_held);
		if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
		{
			static int frame = 0;
			if (++frame % 300 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Tree hold: hand ({:.1f},{:.1f},{:.1f}) grip ({:.1f},{:.1f},{:.1f}) tree ({:.1f},{:.1f},{:.1f}) h={:.1f} r={:.2f} point ({:.1f},{:.1f})",
				                   hand.position.x, hand.position.y, hand.position.z, centre.x, centre.y, centre.z, transform.position.x,
				                   transform.position.y, transform.position.z, _heldHeight, _holdRadius,
				                   _interactionPoint ? _interactionPoint->x : 0.0f, _interactionPoint ? _interactionPoint->z : 0.0f);
			}
		}
	}
	if (_lastHeldPosition && _lastDt > 0.0f && !_springActive)
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

namespace
{
// Data/Spells/ZSpellFiles/SF_MultiPickUp{Wood,Food}_txt.zzz
constexpr float k_PickupEmitRate = 8.0f;       // ER_MultiPickup.EmitRate (atoms per second)
constexpr float k_PickupRaiseTime = 1.0f;      // ER_MultiPickup.RaiseTime
constexpr float k_PickupWoodScale = 0.35f;     // ParticleMeshCreator_Wood.InitialScale (MSH_I_OFFERING_WOOD)
constexpr float k_TumbleSpeed = 0.849115f;     // AppearanceRuleTumble.TumbleSpeed
constexpr float k_MaxTumbleSpeed = 6.20088f;   // AppearanceRuleTumble.MaxTumbleSpeed (RestrictMaxRotation 1)
constexpr float k_PickupGrainScale = 0.5f;     // ParticleSpriteCreator_Grain.InitialScale
constexpr float k_PickupGrainFrameRate = 20.0f; // FrameRate, 32 frames, 8 per row of S_SpriteSheet1, looped
constexpr uint32_t k_PickupGrainFrames = 32;
// UseLandscapeColor: the original tints by the landscape light; the sprite shader only has the sheet's alpha, so
// the grains take the mean colour of S_SpriteSheet1's first 32 frames instead.
constexpr glm::vec3 k_PickupGrainColour {229.0f / 255.0f, 208.0f / 255.0f, 148.0f / 255.0f};
} // namespace

void HandSystem::UpdatePickupParticles(float seconds, bool emitting) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!emitting)
	{
		// StopMultiPickup -> PSysUtility::KillPickupPSys deletes the particle system with its atoms.
		for (const auto& particle : _pickupParticles)
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
		}
		if (!_pickupParticles.empty())
		{
			registry.SetDirty();
		}
		_pickupParticles.clear();
		_pickupOwed = 0.0f;
		_pickupEmitted = 0;
		return;
	}
	const bool wood = PotInfoOf(*_held) == PotInfo::HandWood;
	// PSysManager::GetCurrentGesturePosn: the hand.
	const auto hand = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]).position;

	// ER_MultiPickup::ModifyAtomCollection 0x6A77C0: owed += dt * EmitRate; while emitted < owed, create an atom at
	// the ground under the hand (x, z of the gesture position, y = GetAltitude).
	_pickupOwed += seconds * k_PickupEmitRate;
	while (static_cast<float>(_pickupEmitted) < _pickupOwed)
	{
		++_pickupEmitted;
		const glm::vec3 start(hand.x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(hand.x, hand.z)), hand.z);
		const auto entity = registry.Create();
		if (wood)
		{
			// RandomiseOrientations 0: SetAngleY(DefaultOrientation = 0).
			const auto& pots = Locator::infoConstants::value().pot;
			const auto meshId = resources::HashIdentifier(pots[static_cast<size_t>(PotInfo::HandWood)].meshId);
			registry.Assign<Transform>(entity, start, glm::mat3(1.0f), glm::vec3(k_PickupWoodScale));
			registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(1));
		}
		else
		{
			auto& textures = Locator::resources::value().GetTextures();
			const auto textureId = entt::hashed_string("raw/S_SpriteSheet1a");
			if (!textures.Contains(textureId))
			{
				try
				{
					auto& fileSystem = Locator::filesystem::value();
					textures.Load(textureId, resources::Texture2DLoader::FromDiskTag {},
					              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "S_SpriteSheet1a.raw"));
				}
				catch (const std::exception& e)
				{
					SPDLOG_LOGGER_WARN(spdlog::get("game"), "Pick-up grain: cannot load S_SpriteSheet1a.raw: {}", e.what());
					registry.Destroy(entity);
					continue;
				}
			}
			const auto texture = textures.Handle(textureId)->GetNativeHandle();
			// UseAdditiveAlpha 0: normal blending with a premultiplied tint. InitFrame 0, not randomised.
			registry.Assign<Sprite>(entity, texture, glm::vec2(0.0f), glm::vec2(1.0f / 8.0f), glm::vec4(k_PickupGrainColour, 1.0f), false);
			registry.Assign<Transform>(entity, start, glm::mat3(1.0f), glm::vec3(k_PickupGrainScale));
		}
		_pickupParticles.push_back({entity, 0.0f, start, start, wood});
	}

	// Each atom: removed once its age passes RaiseTime; otherwise pos = start + (hand - start) * age / RaiseTime (the
	// current hand, so the pieces follow it) and velocity = (pos - previous) / dt.
	for (auto& particle : _pickupParticles)
	{
		particle.age += seconds;
		if (particle.age > k_PickupRaiseTime || !registry.Valid(particle.entity))
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
			particle.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(particle.entity);
		const auto position = particle.start + (hand - particle.start) * (particle.age / k_PickupRaiseTime);
		const auto velocity = seconds > 0.0f ? (position - particle.previous) / seconds : glm::vec3(0.0f);
		particle.previous = position;
		transform.position = position;
		if (particle.mesh)
		{
			// AppearanceRuleTumble::ModifyAtomCore 0x6A6200: turn by clamp(|v| * TumbleSpeed, +-Max) * dt about the
			// atom's own Z axis when |vz| < |vx|, else about its X axis.
			const float rate = std::clamp(glm::length(velocity) * k_TumbleSpeed, -k_MaxTumbleSpeed, k_MaxTumbleSpeed);
			const float angle = rate * seconds;
			const glm::vec3 axis = std::abs(velocity.z) < std::abs(velocity.x) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			transform.rotation = transform.rotation * glm::mat3(glm::rotate(glm::mat4(1.0f), angle, axis));
		}
		else
		{
			auto& sprite = registry.Get<Sprite>(particle.entity);
			const auto frame = static_cast<uint32_t>(particle.age * k_PickupGrainFrameRate) % k_PickupGrainFrames;
			sprite.uvMin = {static_cast<float>(frame % 8) / 8.0f, static_cast<float>(frame / 8) / 8.0f};
		}
	}
	std::erase_if(_pickupParticles, [](const PickupParticle& particle) { return particle.entity == entt::null; });
	registry.SetDirty();
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr && !_pickupParticles.empty())
	{
		static float traceTime = 0.0f;
		traceTime += seconds;
		if (traceTime > 0.5f)
		{
			traceTime = 0.0f;
			const auto& first = _pickupParticles.front();
			const auto& p = registry.Get<Transform>(first.entity).position;
			const auto& handTransform = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
			const auto* bones = GetBoneMatrices();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick-up psys: {} atoms, oldest age {:.2f} at ({:.1f},{:.2f},{:.1f}), hand ({:.1f},{:.2f},{:.1f}) det {:.3f} scale {:.3f} bone0 {:.2f},{:.2f},{:.2f} entities {}",
			                   _pickupParticles.size(), first.age, p.x, p.y, p.z, hand.x, hand.y, hand.z,
			                   glm::determinant(handTransform.rotation), handTransform.scale.x,
			                   bones && !bones->empty() ? (*bones)[0][3][0] : -1.0f, bones && !bones->empty() ? (*bones)[0][3][1] : -1.0f,
			                   bones && !bones->empty() ? (*bones)[0][3][2] : -1.0f, static_cast<uint32_t>(entt::to_integral(first.entity)));
		}
	}
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

PotInfo HandSystem::PotInfoOf(entt::entity entity) noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return PotInfo::_COUNT;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	if (pot == nullptr)
	{
		return PotInfo::_COUNT;
	}
	if (pot->type != PotInfo::_COUNT)
	{
		return pot->type;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const auto meshId = registry.Get<const Mesh>(entity).id;
	for (size_t i = 0; i < pots.size(); ++i)
	{
		if (resources::HashIdentifier(pots[i].meshId) == meshId)
		{
			return static_cast<PotInfo>(i);
		}
	}
	return PotInfo::_COUNT;
}

void HandSystem::UpdateMultiPickUp(float seconds, bool actionHeld) noexcept
{
	if (!_held || !_pickSource || !actionHeld)
	{
		_pickSource.reset();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_pickSource) || !registry.Valid(*_held))
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
	// PileResource::ProcessInInteract (0x66E520), once per game turn while the locked select lasts (no distance
	// check; TODO: stop outside the player's influence). The values come from the hand pot's info:
	//   ticks = (1000 / msPerTurn) * multiPickUpRampTime, t = clamp(n / ticks, 0, 1)
	//   amount = (int)(perTurn + (perTurnEnd - perTurn) * t^2), limited by the source and maxAmountCanBePickedUp.
	const auto handType = PotInfoOf(*_held);
	const bool wood = handType == PotInfo::HandWood;
	const auto& info = Locator::infoConstants::value().pot[static_cast<size_t>(wood ? PotInfo::HandWood : PotInfo::HandFood)];
	constexpr float k_TurnSeconds = 0.1f;
	const auto store = archetypes::AbodeArchetype::StoragePitOfPile(*_pickSource);
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	_pickTime += seconds;
	_pickTurnAccumulator += seconds;
	bool changed = false;
	while (_pickTurnAccumulator >= k_TurnSeconds)
	{
		_pickTurnAccumulator -= k_TurnSeconds;
		++_pickTurns;
		const float ticks = std::max(1.0f, info.multiPickUpRampTime / k_TurnSeconds);
		const float t = std::clamp(static_cast<float>(_pickTurns) / ticks, 0.0f, 1.0f);
		auto take = static_cast<uint32_t>(static_cast<float>(info.amountPickedUpPerTurn) +
		                                  static_cast<float>(info.amountPickedUpPerTurnEnd - info.amountPickedUpPerTurn) * t * t);
		const uint32_t room = info.maxAmountCanBePickedUp > pile->amount ? info.maxAmountCanBePickedUp - pile->amount : 0u;
		const uint32_t available = store != entt::null ? archetypes::AbodeArchetype::StoragePitAmount(store, resource) : source->amount;
		take = std::min({take, available, room, 65535u - pile->amount});
		if (take == 0)
		{
			_pickSource.reset();
			break;
		}
		if (store != entt::null)
		{
			archetypes::AbodeArchetype::RemoveFromStoragePit(store, resource, take);
		}
		else
		{
			source->amount = static_cast<uint16_t>(source->amount - take);
		}
		pile->amount = static_cast<uint16_t>(pile->amount + take);
		changed = true;
		// LH_SAMPLE_G_PICKUPWOOD / PICKUPFOOD every turn (pitch 60 + 180 t^2 in the original; no pitch control here).
		PlaySample(wood ? audio::SoundId::G_PickUpWood : audio::SoundId::G_PickUpFood);
	}
	if (changed && _pickSource)
	{
		// An emptied loose pile is deleted; the piles of a store stay (buried when empty), and the store already
		// resized the piles it took from.
		if (store == entt::null && source->amount == 0)
		{
			registry.Destroy(*_pickSource);
			_pickSource.reset();
		}
		else if (store == entt::null)
		{
			SinkPile(*_pickSource);
		}
		registry.SetDirty();
	}
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		static float traceTime = 0.0f;
		traceTime += seconds;
		if (traceTime > 0.5f)
		{
			traceTime = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick trace: turn {} hand pile {}, source left {}", _pickTurns, pile->amount,
			                   _pickSource ? static_cast<int>(source->amount) : -1);
		}
	}
}

void HandSystem::SinkPile(entt::entity pile) noexcept
{
	// Every Just{Add,Remove}Resource calls SetSize: piles ease to their new sink offset, plain pots rescale.
	archetypes::PotArchetype::SetSize(pile, true);
}

void HandSystem::PutDownHandPot(entt::entity pot) noexcept
{
	// Pot::AddResourceToPos: merge into a same-resource pile or store within a 9-cell spiral (taken as 15 m), else a
	// new MagicWood / MagicFood pile; PILE*SMALL sounds below 200, PILE* otherwise.
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = PotInfoOf(pot);
	const bool wood = type == PotInfo::HandWood;
	const auto amount = registry.Get<Pot>(pot).amount;
	auto position = registry.Get<Transform>(pot).position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	registry.Destroy(pot);
	registry.SetDirty();
	if (amount == 0)
	{
		return;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	constexpr float k_MergeRadius = 15.0f;
	std::optional<entt::entity> target;
	float best = k_MergeRadius;
	registry.Each<const Pot, const Transform>([&](entt::entity entity, const Pot&, const Transform& transform) {
		const auto other = PotInfoOf(entity);
		if (other == PotInfo::_COUNT || other == PotInfo::HandWood || other == PotInfo::HandFood ||
		    pots[static_cast<size_t>(other)].resourceType != resource)
		{
			return;
		}
		const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance < best)
		{
			best = distance;
			target = entity;
		}
	});
	// A store pile, or a store with no pile in range: the store takes it (StoragePit::AddResource).
	auto intoStore = target ? archetypes::AbodeArchetype::StoragePitOfPile(*target) : entt::null;
	if (const auto store = wood ? FindWoodStore(position) : std::nullopt; store && !target)
	{
		intoStore = *store;
	}
	if (intoStore != entt::null)
	{
		archetypes::AbodeArchetype::AddToStoragePit(intoStore, resource, amount);
	}
	else if (target && registry.Valid(*target))
	{
		auto& into = registry.Get<Pot>(*target);
		into.amount = static_cast<uint16_t>(std::min<uint32_t>(65535u, into.amount + amount));
		SinkPile(*target);
	}
	else
	{
		const auto pile = archetypes::PotArchetype::Create(position, 0.0f, wood ? PotInfo::MagicWood : PotInfo::MagicFood, amount);
		if (pile != entt::null)
		{
			SinkPile(pile);
		}
	}
	using audio::SoundId;
	static constexpr auto k_FoodSmall = std::array<SoundId, 6> {SoundId::G_PileFoodSmall_01, SoundId::G_PileFoodSmall_02,
	                                                            SoundId::G_PileFoodSmall_03, SoundId::G_PileFoodSmall_04,
	                                                            SoundId::G_PileFoodSmall_05, SoundId::G_PileFoodSmall_06};
	static constexpr auto k_Food = std::array<SoundId, 2> {SoundId::G_PileFood_01, SoundId::G_PileFood_02};
	static constexpr auto k_WoodSmall = std::array<SoundId, 6> {SoundId::G_PileWoodSmall_01, SoundId::G_PileWoodSmall_02,
	                                                            SoundId::G_PileWoodSmall_03, SoundId::G_PileWoodSmall_04,
	                                                            SoundId::G_PileWoodSmall_05, SoundId::G_PileWoodSmall_06};
	static constexpr auto k_Wood = std::array<SoundId, 6> {SoundId::G_PileWood_01, SoundId::G_PileWood_02, SoundId::G_PileWood_03,
	                                                       SoundId::G_PileWood_04, SoundId::G_PileWood_05, SoundId::G_PileWood_06};
	if (wood)
	{
		PlaySample(amount < 200 ? Locator::rng::value().Choose(k_WoodSmall) : Locator::rng::value().Choose(k_Wood));
	}
	else
	{
		PlaySample(amount < 200 ? Locator::rng::value().Choose(k_FoodSmall) : Locator::rng::value().Choose(k_Food));
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: put down {} {} ({})", amount, wood ? "wood" : "food",
	                   target ? "merged" : "new pile");
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
		UpdateRoots(thrown.entity);
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
			if (const auto type = PotInfoOf(thrown.entity); (type == PotInfo::HandWood || type == PotInfo::HandFood) && IsLand(transform.position))
			{
				PutDownHandPot(thrown.entity);
			}
			else if (registry.AnyOf<Tree, DeadTree>(thrown.entity))
			{
				// Tree::ReactToPhysicsImpact: absorbed by a wood store it hits. Anything else: a thrown tree never
				// lands as planted (PHYSICS_OBJECT_FLAG_LANDED is only set by a gentle release) and becomes a DeadTree.
				if (const auto store = FindWoodStore(transform.position); store)
				{
					DepositInStore(thrown.entity, *store);
				}
				else if (registry.AllOf<Tree>(thrown.entity))
				{
					MakeDeadTree(thrown.entity, thrown.velocity);
				}
			}
			thrown.entity = entt::null;
		}
	}
	std::erase_if(_thrown, [](const Thrown& thrown) { return thrown.entity == entt::null; });
	registry.SetDirty();
}

bool HandSystem::IsHoldingTree() const noexcept
{
	// Tree::GetHoldType and DeadTree::GetHoldType return HOLD_TYPE_TREE.
	return _held && Locator::entitiesRegistry::value().Valid(*_held) &&
	       Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(*_held);
}

void HandSystem::ReleaseTree(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	// Tree::EndPhysics: planted again only on dry land (and not burning, TODO: fire); otherwise a DeadTree.
	if (IsLand(transform.position))
	{
		Replant(tree);
	}
	else
	{
		const float angle = Locator::rng::value().NextValue(0.0f, glm::two_pi<float>());
		MakeDeadTree(tree, glm::vec3(std::sin(angle), 0.0f, std::cos(angle)));
	}
}

void HandSystem::Replant(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	auto& component = registry.Get<Tree>(tree);
	DropRoots(tree, false);
	transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	const glm::vec2 at(transform.position.x, transform.position.z);

	// Within 25 m of a town building the tree becomes scenic (foresters leave it alone) and joins no forest.
	constexpr float k_TownRadius = 25.0f;
	bool nearTown = false;
	registry.Each<const Abode, const Transform>([&](entt::entity, const Abode&, const Transform& abode) {
		nearTown = nearTown || glm::distance(at, glm::vec2(abode.position.x, abode.position.z)) < k_TownRadius;
	});
	// Otherwise it joins the forest of the nearest tree within 25 + 10 m, or starts a new forest.
	constexpr float k_ForestRadius = 35.0f;
	std::optional<uint32_t> forest;
	uint32_t maxForest = 0;
	float nearest = k_ForestRadius;
	registry.Each<const Tree, const Transform>([&](entt::entity other, const Tree& t, const Transform& position) {
		// Scripts create scenic trees with forest -1 (and 0): no forest.
		const bool inForest = t.forestId != 0 && t.forestId != std::numeric_limits<uint32_t>::max();
		if (!inForest)
		{
			return;
		}
		maxForest = std::max(maxForest, t.forestId);
		const float distance = glm::distance(at, glm::vec2(position.position.x, position.position.z));
		if (other != tree && distance < nearest)
		{
			nearest = distance;
			forest = t.forestId;
		}
	});
	component.isNonScenic = !nearTown;
	if (nearTown)
	{
		component.forestId = 0;
	}
	else
	{
		// TODO: SPOT_VISUAL_FOREST_CREATED when a new forest is started.
		component.forestId = forest.value_or(maxForest + 1);
	}
	// Tree::DropSfx: LH_SAMPLE_G_PLANTTREE_01 + GetTickCount() % 3.
	// TODO: SmokyStuff, alignment (+treePullPutAlignmentChange).
	static constexpr auto k_PlantTree = std::array<audio::SoundId, 3> {
	    audio::SoundId::G_PlantTree_01, audio::SoundId::G_PlantTree_02, audio::SoundId::G_PlantTree_03};
	PlaySample(Locator::rng::value().Choose(k_PlantTree));
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree replanted at ({:.1f}, {:.1f}), {} forest {}", at.x, at.y,
	                   nearTown ? "scenic (town)," : (forest ? "joined" : "new"), component.forestId);
}

void HandSystem::MakeDeadTree(entt::entity tree, glm::vec3 direction) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	const auto type = registry.Get<Tree>(tree).type;
	registry.Remove<Tree>(tree);
	registry.Assign<DeadTree>(tree, type);
	// It keeps the tree mesh and comes to rest lying down, the crown towards where it was going.
	direction.y = 0.0f;
	direction = glm::length(direction) > 1e-4f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, 1.0f);
	const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction));
	transform.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), axis)) * transform.rotation;
	float trunk = 0.3f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * transform.scale;
		trunk = 0.2f * 0.5f * std::max(size.x, size.z);
	}
	transform.position.y =
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z)) + trunk;
	if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	registry.SetDirty();
	// DeadTree::Draw: the roots break off and fall to the ground (fn_00826280).
	UpdateRoots(tree, true);
	DropRoots(tree, true);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree became a dead tree at ({:.1f}, {:.1f})", transform.position.x,
	                   transform.position.z);
}

std::optional<entt::entity> HandSystem::FindWoodStore(glm::vec3 point) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> store;
	float best = std::numeric_limits<float>::max();
	registry.Each<const StoragePit, const Transform>([&](entt::entity entity, const StoragePit&, const Transform& transform) {
		float radius = 8.0f;
		if (const auto* fixed = registry.TryGet<const Fixed>(entity); fixed != nullptr)
		{
			radius = std::max(radius, fixed->boundingRadius);
		}
		const float distance = glm::distance(glm::vec2(point.x, point.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance <= radius && distance < best)
		{
			best = distance;
			store = entity;
		}
	});
	return store;
}

void HandSystem::DepositInStore(entt::entity object, entt::entity store) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// Object::DoDeleteObjectAndTakeResource: AddResource(WOOD, GetDefaultResource()), with
	// Tree::GetWoodValue = life (1 for a fresh tree) * woodValue * scale * GLandBalance::Values[5] (1 by default).
	const auto type = registry.AllOf<Tree>(object) ? registry.Get<Tree>(object).type : registry.Get<DeadTree>(object).type;
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	auto wood = static_cast<uint32_t>(static_cast<float>(info.woodValue) * registry.Get<Transform>(object).scale.x);
	const uint32_t total = wood;
	archetypes::AbodeArchetype::AddToStoragePit(store, ResourceType::Wood, wood);
	static constexpr auto k_TreeMulch = std::array<audio::SoundId, 4> {
	    audio::SoundId::G_TreeMulch_01, audio::SoundId::G_TreeMulch_02, audio::SoundId::G_TreeMulch_03,
	    audio::SoundId::G_TreeMulch_04};
	PlaySample(Locator::rng::value().Choose(k_TreeMulch));
	DropRoots(object, false);
	registry.Destroy(object);
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: {} wood added to the village store", total);
}

glm::vec3 HandSystem::GripCentre() const noexcept
{
	if (!_animator || _vertices.empty())
	{
		return _palmCenter;
	}
	const auto& bones = _animator->GetBoneMatrices();
	glm::vec3 palm(0.0f);
	glm::vec3 tips(0.0f);
	int palmCount = 0;
	int tipCount = 0;
	for (size_t v = 0; v < _vertices.size(); ++v)
	{
		const auto bone = _vertexBones[v];
		if (bone == 0)
		{
			palm += ModelPosition(v, bones);
			++palmCount;
		}
		else if (bone == 10 || bone == 13 || bone == 16 || bone == 19 || bone == 21)
		{
			tips += ModelPosition(v, bones);
			++tipCount;
		}
	}
	if (palmCount == 0 || tipCount == 0)
	{
		return _palmCenter;
	}
	return 0.5f * (palm / static_cast<float>(palmCount) + tips / static_cast<float>(tipCount));
}

void HandSystem::UpdateTug(float seconds, bool actionHeld) noexcept
{
	if (!_tug)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_tug) || !registry.AllOf<Tree>(*_tug))
	{
		_tug.reset();
		return;
	}
	auto& transform = registry.Get<Transform>(*_tug);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(*_tug); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto& box = meshes.Handle(mesh->id)->GetBoundingBox();
		_heldHeight = box.maxima.y * transform.scale.y;
		_holdRadius = 0.2f * 0.5f * std::max(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	}
	if (!actionHeld)
	{
		// Let go before it came out: it stays planted.
		transform.rotation = _tugRotation;
		_tug.reset();
		registry.SetDirty();
		return;
	}
	// HandStateTug::Update: F = 1000 * (hand - grab point), the tree comes out once |F| > weight
	// (GetMaxForce = 600000 always beats weight * 9.81 for trees), i.e. the hand is weight / 1000 away.
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(registry.Get<Tree>(*_tug).type));
	const auto cursor = _interactionPoint.value_or(_tugPoint);
	auto pull = glm::vec3(cursor.x - _tugPoint.x, 0.0f, cursor.z - _tugPoint.z);
	const float distance = glm::length(pull);
	const float threshold = std::max(0.01f, info.weight / 1000.0f);
	// The tree leans towards the hand while pulled (the tug matrix copied into its G3D; the lean angle is a guess).
	if (distance > 1e-3f)
	{
		pull /= distance;
		const float lean = 0.25f * std::min(1.0f, distance / threshold);
		const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), pull));
		transform.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), lean, axis)) * _tugRotation;
	}
	registry.SetDirty();
	if (distance > threshold)
	{
		transform.rotation = _tugRotation;
		const auto tree = *_tug;
		_tug.reset();
		Uproot(tree);
	}
	static_cast<void>(seconds);
}

void HandSystem::Uproot(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	// fn_0074BD20: a roots pile (MeshPack TreeRootsPile) where the tree stood, scaled
	// (mesh extent x + extent z) * scale * 0.3, lasting 15 s, plus SmokyStuff (the grip dust stands in for it).
	auto& meshes = Locator::resources::value().GetMeshes();
	float extentX = 1.0f;
	float extentZ = 1.0f;
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		// LH3DMesh +0x24 / +0x2C, taken as the half extents of the bounding box.
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size();
		extentX = 0.5f * size.x;
		extentZ = 0.5f * size.z;
	}
	const auto pileMesh = resources::HashIdentifier(MeshId::TreeRootsPile);
	if (meshes.Contains(pileMesh))
	{
		const auto pile = registry.Create();
		const float scale = (extentX + extentZ) * transform.scale.x * 0.3f;
		registry.Assign<Transform>(pile, transform.position, transform.rotation, glm::vec3(scale));
		registry.Assign<Mesh>(pile, pileMesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
		_rootsPiles.emplace_back(pile, 15.0f);
	}
	EmitGripDust(transform.position);
	PickUp(tree);
	UpdateRoots(tree);
}

void HandSystem::UpdateRoots(entt::entity tree, bool dying) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// Only rooted objects (Object::IsARootedObject: living trees) have roots; a dying tree hands them over to the fall.
	if (!registry.Valid(tree) || (!dying && !registry.AllOf<Tree>(tree)))
	{
		return;
	}
	const auto rootsMesh = resources::HashIdentifier(MeshId::TreeRoots);
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto* treeMesh = registry.TryGet<const Mesh>(tree);
	if (!meshes.Contains(rootsMesh) || treeMesh == nullptr || !meshes.Contains(treeMesh->id))
	{
		return;
	}
	auto it = std::find_if(_roots.begin(), _roots.end(), [tree](const auto& pair) { return pair.first == tree; });
	if (it == _roots.end())
	{
		const auto roots = registry.Create();
		registry.Assign<Transform>(roots, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(roots, rootsMesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
		_roots.emplace_back(tree, roots);
		it = std::prev(_roots.end());
	}
	// fn_00511270: the tree matrix with its rotation scaled by 0.15 * mesh extent (LH3DMesh +0x24).
	const auto& transform = registry.Get<Transform>(tree);
	const float extent = 0.5f * meshes.Handle(treeMesh->id)->GetBoundingBox().Size().x;
	auto& roots = registry.Get<Transform>(it->second);
	roots.position = transform.position;
	roots.rotation = transform.rotation;
	roots.scale = transform.scale * (0.15f * extent);
	registry.SetDirty();
}

void HandSystem::DropRoots(entt::entity tree, bool fall) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto it = std::find_if(_roots.begin(), _roots.end(), [tree](const auto& pair) { return pair.first == tree; });
	if (it == _roots.end())
	{
		return;
	}
	const auto roots = it->second;
	_roots.erase(it);
	if (!registry.Valid(roots))
	{
		return;
	}
	if (!fall)
	{
		registry.Destroy(roots);
		registry.SetDirty();
		return;
	}
	// fn_008263C0 / fn_008261D0: y = y0 - 20 t^2 down to altitude + 0.1 * extent * scale, gone after 20 s.
	auto& transform = registry.Get<Transform>(roots);
	float extent = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		extent = 0.5f * meshes.Handle(mesh->id)->GetBoundingBox().Size().x;
	}
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	_fallingRoots.push_back({roots, 0.0f, transform.position.y, ground + 0.1f * extent * transform.scale.x});
}

void HandSystem::UpdateRootsAndPiles(float seconds) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	bool dirty = false;
	// fn_00825350: the pile fades out over its last second (alpha = life * 0.255 of 255).
	for (auto& [pile, life] : _rootsPiles)
	{
		life -= seconds;
		if (life < 1.0f && life > 0.0f && registry.Valid(pile))
		{
			registry.AssignOrReplace<Alpha>(pile, life);
			dirty = true;
		}
		if (life <= 0.0f && registry.Valid(pile))
		{
			registry.Destroy(pile);
			pile = entt::null;
			dirty = true;
		}
	}
	std::erase_if(_rootsPiles, [](const auto& pair) { return pair.first == entt::null; });
	for (auto& roots : _fallingRoots)
	{
		if (!registry.Valid(roots.entity))
		{
			roots.entity = entt::null;
			continue;
		}
		roots.age += seconds;
		auto& transform = registry.Get<Transform>(roots.entity);
		transform.position.y = std::max(roots.groundY, roots.startY - 20.0f * roots.age * roots.age);
		// fn_008261D0: alpha = 255 * (1 - (t - 18) / 2) from 18 s.
		if (roots.age > 18.0f)
		{
			registry.AssignOrReplace<Alpha>(roots.entity, std::max(0.0f, 1.0f - (roots.age - 18.0f) / 2.0f));
		}
		dirty = true;
		if (roots.age >= 20.0f)
		{
			registry.Destroy(roots.entity);
			roots.entity = entt::null;
		}
	}
	std::erase_if(_fallingRoots, [](const FallingRoots& roots) { return roots.entity == entt::null; });
	std::erase_if(_roots, [&registry](const auto& pair) {
		if (registry.Valid(pair.first))
		{
			return false;
		}
		if (registry.Valid(pair.second))
		{
			registry.Destroy(pair.second);
		}
		return true;
	});
	if (dirty)
	{
		registry.SetDirty();
	}
}

glm::mat3 HandSystem::HeldSway(glm::vec3 at) const noexcept
{
	// HandStateHolding::Update: up to 0.3 rad about the view direction ((smooth - mouse).x, clamped to 80 px) and
	// about the side axis ((mouse - smooth).y), pivoting on the grip.
	glm::mat3 sway(1.0f);
	if (!Locator::camera::has_value())
	{
		return sway;
	}
	auto toCamera = Locator::camera::value().GetOrigin() - at;
	if (glm::length(toCamera) <= 1e-4f)
	{
		return sway;
	}
	toCamera = glm::normalize(toCamera);
	const float tiltX = glm::clamp(_smoothMouse.x - _mouse.x, -80.0f, 80.0f) * (0.3f / 80.0f);
	const float tiltY = glm::clamp(_mouse.y - _smoothMouse.y, -80.0f, 80.0f) * (0.3f / 80.0f);
	const auto side = glm::vec3(-toCamera.z, 0.0f, toCamera.x);
	sway = glm::mat3(glm::rotate(glm::mat4(1.0f), tiltX, toCamera));
	if (glm::length(side) > 1e-4f)
	{
		sway = sway * glm::mat3(glm::rotate(glm::mat4(1.0f), tiltY, glm::normalize(side)));
	}
	return sway;
}

void HandSystem::ComputeHoldParameters(entt::entity entity) noexcept
{
	// objects.cpp / NOTES_objects.md (class table):
	//   Object (default)          ABOVE     R = 0.75 * height   lowering 0
	//   Tree / DeadTree           TREE      R = 0.2 * R2D       lowering 0.1   (Tree is rooted)
	//   MobileObject, Pot         SIDE      R = R2D             lowering 0.7
	//   MobileStatic gate totems, weeping stones: SIDE 0.7; singing stone 1: SIDE 0.4; others ABOVE
	//   Villager                  VILLAGER  R = R2D             lowering 0.65
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(entity);
	float radius2D = 0.5f;
	_heldHeight = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * transform.scale;
		radius2D = 0.5f * std::max(size.x, size.z);
		_heldHeight = size.y; // Object::GetHeight = 2 * half extent y * scale
	}
	_holdType = HoldType::Above;
	_holdRadius = 0.75f * _heldHeight;
	_loweringMultiplier = 0.0f;
	_rooted = registry.AllOf<Tree>(entity);
	if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		_holdType = HoldType::Tree;
		_holdRadius = 0.2f * radius2D;
		_loweringMultiplier = 0.1f;
	}
	else if (registry.AnyOf<MobileObject, Pot>(entity))
	{
		_holdType = HoldType::Side;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.7f;
	}
	else if (registry.AllOf<Villager>(entity))
	{
		_holdType = HoldType::Villager;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.65f;
	}
	else if (const auto* statics = registry.TryGet<const MobileStatic>(entity); statics != nullptr)
	{
		switch (statics->type)
		{
		case MobileStaticInfo::GateTotemApe:
		case MobileStaticInfo::GateTotemBlank:
		case MobileStaticInfo::GateTotemCow:
		case MobileStaticInfo::GateTotemTiger:
		case MobileStaticInfo::WeepingStone:
		case MobileStaticInfo::WeepingStoneReward:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.7f;
			break;
		case MobileStaticInfo::SingingStone_1:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.4f;
			break;
		default:
			break;
		}
	}
}
