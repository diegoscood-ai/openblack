/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_ObjectArcer, the electric arcs over what a lightning bolt struck (ParticleArcRules.h). Wiki:
// docs/wiki/magic/fireball-and-lightning.md#the-arcs-over-what-the-bolt-struck.

#include "ParticleArcRules.h"

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <L3DFile.h>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "Locator.h"
#include "Particles/LightningMaths.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManagerState.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/ExplodeObject.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The rule's defaults, as the original sets them before the file's properties
constexpr int32_t k_DefaultAverageTicks = 10;
constexpr float k_DefaultRandomFrac = 0.1f;
constexpr float k_DefaultScaleTangents = 1.0f;
constexpr float k_DefaultRandomTangents = 0.2f;
constexpr int k_DefaultJoints = 19;
/// The chain's flag for drawing its joints between the last two steps: the arc's ribbon is drawn as it was laid
constexpr uint8_t k_InterpolatedFlag = 2;

manager::State* ArcState()
{
	return Locator::particleSystem::has_value() ? &Locator::particleSystem::value().GetState() : nullptr;
}

/// Not being deleted, and for a villager its final state is not dying, as the bolt asks of its targets
bool Available(entt::entity object)
{
	if (object == entt::null || !Locator::entitiesRegistry::has_value() || !ecs::IsAvailable(object))
	{
		return false;
	}
	return !Locator::entitiesRegistry::value().AllOf<ecs::components::Villager>(object) || ecs::villager::IsAvailable(object);
}

/// The model of an object, with its world matrix as the explosion takes it: its mesh of the mesh pack (nullptr when it
/// has none there), the axes scaled and the position
struct Model
{
	std::shared_ptr<const explode_object::SourceMesh> mesh;
	glm::mat3 axes {1.0f};
	glm::vec3 position {0.0f};
};
Model ModelOf(entt::entity object)
{
	if (object == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return {};
	}
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	if (mesh == nullptr || transform == nullptr)
	{
		return {};
	}
	Model model {.mesh = explode_object::PackMeshOf(mesh->id), .axes = transform->rotation, .position = transform->position};
	model.axes[0] *= transform->scale.x;
	model.axes[1] *= transform->scale.y;
	model.axes[2] *= transform->scale.z;
	return model;
}

/// A mesh with bones is drawn animated
bool Animated(const explode_object::SourceMesh& mesh)
{
	return (mesh.flags & static_cast<uint32_t>(l3d::L3DMeshFlags::HasBones)) != 0;
}

/// What an arc's atom keeps. It counts itself among the live arcs for as long as its atom lasts
struct ArcData
{
	explicit ArcData(uint32_t& liveCount)
	    : live(&liveCount)
	{
		++*live;
	}
	~ArcData() { --*live; }
	ArcData(const ArcData&) = delete;
	ArcData& operator=(const ArcData&) = delete;
	ArcData(ArcData&&) = delete;
	ArcData& operator=(ArcData&&) = delete;

	entt::entity object {entt::null};
	object_arcer::ArcPoints points;
	/// Ticks until its ends are picked again
	int32_t ticks {0};
	particles::maths::ArcEnds ends;
	uint32_t* live;
};

/// UR_ObjectArcer (an atom-creating rule)
class ObjectArcer final: public Modifier
{
public:
	explicit ObjectArcer(const Object& object)
	    : creator(object.String("PCreator"))
	    , chainCreator(object.String("ChainCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , averageTicks(object.Int("AverageTicksPerUpdate", k_DefaultAverageTicks))
	    , joints(object.Int("JointsPerArc", k_DefaultJoints))
	    , randomFrac(object.Float("RandomFrac", k_DefaultRandomFrac))
	    , randomTangents(object.Float("RandomTangents", k_DefaultRandomTangents))
	    , scaleTangents(object.Float("ScaleTangents", k_DefaultScaleTangents))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// without a creator it detaches, leaving the queue as it is
		const auto* atomCreator = effect.FindCreator(creator);
		auto* state = ArcState();
		if (atomCreator == nullptr || state == nullptr)
		{
			return false;
		}
		// the queue from its newest object: an atom each, with its points. One whose object is gone, or whose model
		// gives no point, goes at once
		while (!state->arcQueue.empty())
		{
			const auto object = state->arcQueue.back();
			state->arcQueue.pop_back();
			auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
			auto data = std::make_shared<ArcData>(state->arcAtoms);
			data->object = Available(object) ? object : entt::null;
			if (data->object != entt::null)
			{
				const auto model = ModelOf(data->object);
				if (model.mesh)
				{
					data->points = object_arcer::PickPoints(*model.mesh, model.axes, model.position,
					                                        [&effect](int32_t n) { return effect.Rand(n); });
				}
			}
			const bool none = data->points.points.empty();
			atom.modifierData[this] = std::move(data);
			if (none)
			{
				Remove(effect, collection, atom);
			}
		}
		// every atom of the collection, the new ones too
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.modifierData.find(this);
			if (found == atom.modifierData.end())
			{
				// the original gives it an arc with no object, which goes at once
				Remove(effect, collection, atom);
				continue;
			}
			auto& data = *std::static_pointer_cast<ArcData>(found->second);
			if (!Available(data.object))
			{
				data.object = entt::null;
			}
			--data.ticks;
			// gone with its object
			if (data.object == entt::null)
			{
				Remove(effect, collection, atom);
				continue;
			}
			// now and then new ends, each picked from its points on its own, so an arc may start and end together
			if (data.ticks < 0)
			{
				data.ticks = effect.Rand(averageTicks);
				const auto from = static_cast<size_t>(effect.Rand(static_cast<int32_t>(data.points.points.size())));
				const auto to = static_cast<size_t>(effect.Rand(static_cast<int32_t>(data.points.points.size())));
				data.ends = {.from = data.points.points[from],
				             .to = data.points.points[to],
				             .fromNormal = data.points.normals[from],
				             .toNormal = data.points.normals[to]};
			}
			// an atom without its ribbon's collection ends the step there, and the rule detaches
			if (atom.subCollections.empty())
			{
				return false;
			}
			Lay(effect, *atom.subCollections.front(), data.ends);
			++i;
		}
		return true;
	}

private:
	/// The atom goes, with its ribbon
	static void Remove(Effect& effect, Collection& collection, const Atom& atom)
	{
		for (auto it = collection.atoms.begin(); it != collection.atoms.end(); ++it)
		{
			if (it->get() == &atom)
			{
				effect.NotifyAtomRemoved(**it);
				collection.atoms.erase(it);
				return;
			}
		}
	}

	/// The ribbon's joints, made the first time (none without the chain's creator), laid along the arc: the tangents'
	/// two nudges are drawn first, then one jitter per joint, in the chain's order
	void Lay(Effect& effect, Collection& chain, const particles::maths::ArcEnds& ends) const
	{
		if (chain.atoms.empty())
		{
			if (const auto* jointCreator = effect.FindCreator(chainCreator); jointCreator != nullptr)
			{
				chain.flags &= static_cast<uint8_t>(~k_InterpolatedFlag);
				for (int k = 0; k < joints; ++k)
				{
					effect.NewAtom(chain, jointCreator, {});
				}
			}
		}
		const float length = particles::maths::ArcLength(ends);
		const auto fromTangent =
		    particles::maths::ArcTangent(ends.fromNormal, effect.RandomInBall(), randomTangents, length, scaleTangents);
		const auto toTangent =
		    particles::maths::ArcTangent(ends.toNormal, effect.RandomInBall(), randomTangents, length, scaleTangents);
		std::vector<glm::vec3> jitters(chain.atoms.size());
		for (auto& jitter : jitters)
		{
			jitter = effect.RandomInBall();
		}
		const auto laid = particles::maths::ArcJoints(ends, fromTangent, toTangent, jitters, randomFrac);
		for (size_t k = 0; k < chain.atoms.size(); ++k)
		{
			chain.atoms[k]->position = laid[k];
		}
	}

	std::string creator;
	std::string chainCreator;
	std::vector<int> nextGroups;
	int32_t averageTicks;
	int joints;
	float randomFrac;
	float randomTangents;
	float scaleTangents;
};
} // namespace

void openblack::psys::object_arcer::Queue(entt::entity object)
{
	auto* state = ArcState();
	if (state == nullptr)
	{
		return;
	}
	const auto model = ModelOf(object);
	if (!Queues(model.mesh != nullptr, model.mesh != nullptr && Animated(*model.mesh), state->arcAtoms))
	{
		return;
	}
	state->arcQueue.push_back(object);
}

void openblack::psys::object_arcer::Clear()
{
	if (auto* state = ArcState(); state != nullptr)
	{
		state->arcQueue.clear();
	}
}

size_t openblack::psys::object_arcer::QueuedCount()
{
	const auto* state = ArcState();
	return state != nullptr ? state->arcQueue.size() : 0;
}

uint32_t openblack::psys::object_arcer::LiveArcs()
{
	const auto* state = ArcState();
	return state != nullptr ? state->arcAtoms : 0;
}

openblack::psys::object_arcer::ArcPoints openblack::psys::object_arcer::PickPoints(const explode_object::SourceMesh& mesh,
                                                                                   const glm::mat3& axes,
                                                                                   const glm::vec3& position,
                                                                                   const std::function<int32_t(int32_t)>& rand)
{
	size_t vertices = 0;
	for (const auto& subMesh : mesh.subMeshes)
	{
		for (const auto& primitive : subMesh.primitives)
		{
			vertices += primitive.positions.size();
		}
	}
	if (vertices == 0)
	{
		return {};
	}
	const auto& subMesh = mesh.subMeshes[static_cast<size_t>(rand(static_cast<int32_t>(mesh.subMeshes.size())))];
	// (port guard) a sub-mesh without primitives, or a primitive without vertices: the original reads past them
	if (subMesh.primitives.empty())
	{
		return {};
	}
	const auto& primitive = subMesh.primitives[static_cast<size_t>(rand(static_cast<int32_t>(subMesh.primitives.size())))];
	if (primitive.positions.empty())
	{
		return {};
	}
	ArcPoints picked;
	picked.points.reserve(k_ArcPoints);
	picked.normals.reserve(k_ArcPoints);
	for (size_t i = 0; i < k_ArcPoints; ++i)
	{
		const auto vertex = static_cast<size_t>(rand(static_cast<int32_t>(primitive.positions.size())));
		picked.points.push_back(particles::maths::ArcSurfacePoint(axes, position, primitive.positions[vertex]));
		picked.normals.push_back(particles::maths::ArcSurfaceNormal(axes, position, primitive.normals[vertex]));
	}
	return picked;
}

void openblack::psys::RegisterArcRules()
{
	RegisterModifier("UR_ObjectArcer", MakeModifierOf<ObjectArcer>);
}
