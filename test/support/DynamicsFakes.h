/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/DynamicsWorld.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "Physics/TurnRules.h"

namespace openblack::test
{
/// Broken buildings that never break: every call does nothing, for a test whose bodies only need buildings to forget a
/// hitter
class QuietBuildingDamage final: public ecs::systems::BuildingDamageSystemInterface
{
public:
	bool ReactToImpact([[maybe_unused]] entt::entity building, [[maybe_unused]] ecs::PhysicsEntry& entry) override
	{
		return true;
	}
	entt::entity PieceAtRest(entt::entity fragment, [[maybe_unused]] const glm::mat3& rows,
	                         [[maybe_unused]] glm::vec3 centre) override
	{
		return fragment;
	}
	void ProcessTurn() override {}
	void DestroyFragment([[maybe_unused]] entt::entity fragment) override {}
	void ConnectDamageListener() override {}
	void Redraw([[maybe_unused]] entt::entity building) override {}
	void RemoveDamage([[maybe_unused]] entt::entity building) override {}
	void ForgetHitter([[maybe_unused]] entt::entity hitter, [[maybe_unused]] entt::entity building) override {}
	void AppendDamageMeshes([[maybe_unused]] graphics::world_triangles::Frame& out,
	                        [[maybe_unused]] const physics::damage::Mesh::FrameLight& frame,
	                        [[maybe_unused]] float turnTime) override
	{
	}
	void SnapshotDamageMeshes(std::vector<ecs::systems::DamageMeshDraw>& out,
	                          [[maybe_unused]] const physics::damage::Mesh::FrameLight& frame,
	                          [[maybe_unused]] float turnTime) override
	{
		out.clear();
	}
	void AppendDamageMeshes([[maybe_unused]] graphics::world_triangles::Frame& out,
	                        [[maybe_unused]] const std::vector<ecs::systems::DamageMeshDraw>& draws) override
	{
	}
};

/// Broken buildings that never break either, but write down every hitter a building is asked to forget, as (hitter,
/// building), and every fragment's end of physics they are asked for, with its matrix
class RecordingBuildingDamage final: public ecs::systems::BuildingDamageSystemInterface
{
public:
	bool ReactToImpact([[maybe_unused]] entt::entity building, [[maybe_unused]] ecs::PhysicsEntry& entry) override
	{
		return true;
	}
	entt::entity PieceAtRest(entt::entity fragment, const glm::mat3& rows, glm::vec3 centre) override
	{
		fragmentEnds.push_back({fragment, rows, centre});
		return fragment;
	}
	void ProcessTurn() override {}
	void DestroyFragment([[maybe_unused]] entt::entity fragment) override {}
	void ConnectDamageListener() override {}
	void Redraw([[maybe_unused]] entt::entity building) override {}
	void RemoveDamage([[maybe_unused]] entt::entity building) override {}
	void ForgetHitter(entt::entity hitter, entt::entity building) override { forgotten.emplace_back(hitter, building); }
	void AppendDamageMeshes([[maybe_unused]] graphics::world_triangles::Frame& out,
	                        [[maybe_unused]] const physics::damage::Mesh::FrameLight& frame,
	                        [[maybe_unused]] float turnTime) override
	{
	}
	void SnapshotDamageMeshes(std::vector<ecs::systems::DamageMeshDraw>& out,
	                          [[maybe_unused]] const physics::damage::Mesh::FrameLight& frame,
	                          [[maybe_unused]] float turnTime) override
	{
		out.clear();
	}
	void AppendDamageMeshes([[maybe_unused]] graphics::world_triangles::Frame& out,
	                        [[maybe_unused]] const std::vector<ecs::systems::DamageMeshDraw>& draws) override
	{
	}

	/// One fragment's end of physics: the fragment, the rows and the centre it would stick through
	struct FragmentEnd
	{
		entt::entity fragment {entt::null};
		glm::mat3 rows {1.0f};
		glm::vec3 centre {0.0f};
	};

	std::vector<std::pair<entt::entity, entt::entity>> forgotten;
	std::vector<FragmentEnd> fragmentEnds;
};

/// The physics' world of a test: the test's own entities and, when it gives them, its tables and its broken buildings.
/// There is no land and no camera, and the files, the resource caches and the broken buildings it was not given fail
/// loudly: the tests that use it never reach them. With it a test holds a DynamicsSystem of its own instead of reading
/// the locator's.
class FakeDynamicsWorld final: public ecs::dynamics::World
{
public:
	explicit FakeDynamicsWorld(ecs::Registry& registry, const InfoConstants* info = nullptr,
	                           ecs::systems::BuildingDamageSystemInterface* buildingDamage = nullptr)
	    : _registry(registry)
	    , _info(info)
	    , _buildingDamage(buildingDamage)
	{
	}

	[[nodiscard]] ecs::Registry& Entities() override { return _registry; }
	[[nodiscard]] const ecs::Registry& Entities() const override { return _registry; }
	[[nodiscard]] const LandIslandInterface* Land() const override { return nullptr; }
	[[nodiscard]] const InfoConstants* Info() const override { return _info; }
	[[nodiscard]] std::optional<glm::vec3> CameraOrigin() const override { return std::nullopt; }
	[[nodiscard]] filesystem::FileSystemInterface& FileSystem() override
	{
		throw std::logic_error("the test's physics has no files");
	}
	[[nodiscard]] resources::ResourcesInterface& Resources() override
	{
		throw std::logic_error("the test's physics has no resource caches");
	}
	/// No snow lies in a test's world, unless it says so
	[[nodiscard]] int32_t SnowAt(glm::vec3 /*point*/) const override { return snow; }
	[[nodiscard]] uint32_t LandColour() const override { return landColour; }
	int32_t snow {0};
	uint32_t landColour {0};
	[[nodiscard]] ecs::systems::BuildingDamageSystemInterface& BuildingDamage() override
	{
		if (_buildingDamage == nullptr)
		{
			throw std::logic_error("the test's physics has no broken buildings");
		}
		return *_buildingDamage;
	}

private:
	ecs::Registry& _registry;
	const InfoConstants* _info;
	ecs::systems::BuildingDamageSystemInterface* _buildingDamage;
};

/// The end of a physics turn's steps over a list of named bodies, writing down each step it is asked to do as "step
/// name" (the record as "record hit<-hitter"). A body's reaction may add bodies at the end of the list, and take resting
/// bodies out of it the way AddObject does when it replaces one (the last body moves into the place), as the game's
/// reactions do.
class RecordingEndOfTurnSteps final: public openblack::physics::turn::EndOfTurnSteps
{
public:
	struct Body
	{
		char name {'?'};
		/// The turn's force counts
		bool counts {false};
		/// The body that hit it, 0 for none
		char hitter {0};
		/// What its reaction does to the list
		std::vector<char> adds {};
		std::vector<char> removes {};
	};

	explicit RecordingEndOfTurnSteps(std::vector<Body> list)
	    : bodies(std::move(list))
	{
	}

	[[nodiscard]] size_t Count() const override { return bodies.size(); }
	void ClearPushes(size_t i) override { Write("push", i); }
	void ToppleFelledTree(size_t i) override { Write("topple", i); }
	void FollowObject(size_t i) override { Write("follow", i); }
	bool TakeImpact(size_t i) override
	{
		Write("impact", i);
		return bodies.at(i).counts;
	}
	void FindHitter(size_t i) override { Write("hitter", i); }
	void CollisionSound(size_t i) override { Write("sound", i); }
	void React(size_t i) override
	{
		Write("react", i);
		_current = bodies.at(i);
		for (const char name : _current.removes)
		{
			const auto it = std::ranges::find(bodies, name, &Body::name);
			if (it != bodies.end())
			{
				physics::turn::RemoveByMovingLast(bodies, static_cast<size_t>(it - bodies.begin()));
			}
		}
		for (const char name : _current.adds)
		{
			bodies.push_back(Body {.name = name});
		}
	}
	void ConsiderDamageByThrowing(bool atWhatItHit) override
	{
		log.push_back(std::string(atWhatItHit ? "deed-at " : "deed ") + _current.name);
	}
	[[nodiscard]] bool WasHit() const override { return _current.hitter != 0; }
	void RecordHit() override { log.push_back(std::string("record ") + _current.name + "<-" + _current.hitter); }
	void AgeSoundPairs() override { log.emplace_back("age"); }

	std::vector<Body> bodies;
	std::vector<std::string> log;

private:
	void Write(const char* step, size_t i) { log.push_back(std::string(step) + " " + bodies.at(i).name); }

	Body _current;
};
} // namespace openblack::test
