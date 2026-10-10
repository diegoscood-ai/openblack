/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules that tie an effect to a game object: CreateRule_GameObjectRef keeps an unseen atom on each object the
// effect is given, and ER_EmitFromParentAtom lets atoms out from random points of the surface of the object under
// that atom, as the sparkles over a food pile that speeds up the people who take from it. Wiki:
// docs/bw1-notes/particles.md, "The object rules".

#include "ParticleObjectRules.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <atomic>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

/// What an atom kept on an object holds: the object, and whether it was found unavailable this step. The game lets go
/// of such an object once the step's sub-collections have been through, so the atom's emitters still see it this step;
/// here it is let go at the start of the next step, before anything reads it again
struct ObjectRef
{
	entt::entity object {entt::null};
	bool letGo {false};
};

/// The mobile objects and the mobile statics, whose world matrix turns by all three angles: piles and pots, the
/// whale, scaffolds, one-shot spell seeds, rocks, bonfires, dead and felled trees, fragments, magic teleports and
/// landscape vortices
bool IsMobileClass(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	return registry.AnyOf<MobileObject, MobileStatic, Pot, Shark, Scaffold, OneOffSpellSeed>(entity) ||
	       registry.AnyOf<DeadTree, FelledTree, Fragment, MagicTeleport, LandscapeVortex>(entity);
}

/// Has a place this step (an unavailable object still has one until it is let go)
bool HasPlace(entt::entity entity)
{
	if (entity == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<ecs::components::Transform>(entity);
}

/// The model of a loaded mesh, walked as the game walks its file: every sub-mesh, each of its primitives, each of their
/// triangles, with the file's vertices
class MeshModel final: public object_surface::Model
{
public:
	explicit MeshModel(const graphics::L3DMesh& mesh)
	    : _mesh(mesh)
	{
	}
	[[nodiscard]] uint32_t PartCount() const override { return static_cast<uint32_t>(_mesh.GetSubMeshes().size()); }
	[[nodiscard]] uint32_t ItemCount(uint32_t part) const override
	{
		return static_cast<uint32_t>(Part(part).GetCollisionRanges().size());
	}
	[[nodiscard]] uint32_t TriangleCount(uint32_t part, uint32_t item) const override
	{
		return Part(part).GetCollisionRanges().at(item).second / 3;
	}
	[[nodiscard]] std::array<glm::vec3, 3> Corners(uint32_t part, uint32_t item, uint32_t triangle) const override
	{
		const auto& sub = Part(part);
		const auto first = sub.GetCollisionRanges().at(item).first + (triangle * 3);
		const auto& indices = sub.GetCollisionIndices();
		const auto& vertices = sub.GetSkinLocalPositions();
		return {vertices.at(indices.at(first)), vertices.at(indices.at(first + 1)), vertices.at(indices.at(first + 2))};
	}

private:
	[[nodiscard]] const graphics::L3DSubMesh& Part(uint32_t part) const { return *_mesh.GetSubMeshes().at(part); }

	const graphics::L3DMesh& _mesh;
};

/// CreateRule_GameObjectRef. Flag 2 without 4: it makes no atoms of its own accord, but the effect waits for its
/// targets until it closes down. One object target a step becomes an atom with no creator (never drawn), with its next
/// groups under it; every atom of the collection is put where its object stands, on the land at its map coordinates,
/// raised by its own height above the land and by the offset. An atom whose object has gone stays where it was.
class GameObjectRef final: public Modifier, public SurfacePointSource
{
public:
	explicit GameObjectRef(const Object& object)
	    : nextGroups(object.IntArray("NextGroups"))
	    , alpha(object.Int("Alpha", 0xFF))
	    , offsetY(object.Float("OffsetY", 0.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		for (auto& atom : collection.atoms)
		{
			if (auto* data = Data(*atom); data != nullptr && data->letGo)
			{
				data->object = entt::null;
				data->letGo = false;
			}
		}
		// One target a step, taken even when it is not an object
		if (!effect.GetTargets().empty())
		{
			const auto target = effect.TakeTarget();
			if (object_surface::IsGameObject(target))
			{
				auto& atom = effect.NewAtom(collection, nullptr, nextGroups);
				// The alpha's low byte
				atom.colour[3] = static_cast<uint8_t>(alpha & 0xFF);
				AtomDataOf<ObjectRef>(atom, this).object = target;
			}
		}
		for (auto& atom : collection.atoms)
		{
			auto* data = Data(*atom);
			if (data == nullptr || data->object == entt::null)
			{
				continue;
			}
			if (HasPlace(data->object))
			{
				// The game's own sums: the land's height at the map coordinates plus the object's height above it, plus
				// the offset
				auto place = map_coords::ToWorld(ecs::object::MapCoordsOf(data->object));
				place.y = place.y + offsetY;
				atom->position = place;
			}
			if (!ecs::IsAvailable(data->object))
			{
				data->letGo = true;
			}
		}
		return true;
	}

	Result RandomSurfacePoint(Effect& effect, const Atom& atom, glm::vec3& out) const override
	{
		const auto found = atom.modifierData.find(this);
		const auto object =
		    found != atom.modifierData.end() ? std::static_pointer_cast<ObjectRef>(found->second)->object : entt::null;
		// Its object gone: the atom's own place
		if (object == entt::null)
		{
			out = effect.GlobalPosition(atom);
			return Result::Point;
		}
		// (pending) An object already destroyed has no model here, where the game still reads the one it had
		if (!HasPlace(object))
		{
			return Result::Nothing;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		// (pending) An animated model gives one of its bones, which is not ported. A model is animated when it was made
		// as an animated one: a boned mesh whose class keeps the default model type, or a whale; here the people, the
		// animals and the boned models that play clips
		if (registry.AnyOf<ecs::components::SkeletalAnimation, ecs::components::Villager, ecs::components::Animal>(object))
		{
			return Result::NotPorted;
		}
		// (pending) A creature's model is not an animated one: the game walks the body mesh of the level of detail it
		// was last drawn at, which is not ported
		if (registry.AllOf<ecs::components::Creature>(object))
		{
			return Result::NotPorted;
		}
		if (!Locator::resources::has_value())
		{
			return Result::Nothing;
		}
		const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
		const auto& meshes = Locator::resources::value().GetMeshes();
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			return Result::Nothing;
		}
		const MeshModel model(*meshes.Handle(mesh->id));
		// Through the game object's own world matrix, not its 3D object's
		const auto point = object_surface::RandomPoint(effect, model, object_surface::WorldMatrixOf(object));
		if (!point.has_value())
		{
			return Result::Nothing;
		}
		out = *point;
		return Result::Point;
	}

private:
	[[nodiscard]] ObjectRef* Data(Atom& atom) const
	{
		const auto found = atom.modifierData.find(this);
		return found != atom.modifierData.end() ? std::static_pointer_cast<ObjectRef>(found->second).get() : nullptr;
	}

	std::vector<int> nextGroups;
	/// (pending) DoShower only picks what the atom draws: a golden shower on the object, which is not ported
	int alpha;
	float offsetY;
};

/// What the emitter keeps for a collection: the atoms it is owed and the atoms it has tried to let out
struct EmitterData
{
	float owed {0.0f};
	int32_t emitted {0};
};

/// ER_EmitFromParentAtom. A creator: up to MaxAtoms atoms over AtomAgeZeroSize seconds, no more than MaxAtoms alive,
/// each let out at a random point of what its collection's parent atom stands for, and, with EmitOnlyAboveLandscape,
/// only where that point is above the land (a point below still counts as let out). They may grow and pulse over their
/// lives, and go once AtomAgeZeroSize is past.
class EmitFromParentAtom final: public Modifier
{
public:
	explicit EmitFromParentAtom(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , maxAtoms(object.Int("MaxAtoms", 20))
	    , pulseMagnitude(object.Float("PulseMagnitude", 1.0f))
	    , pulseSpeed(object.Float("PulseSpeed", 1.0f))
	    , ageMaxSize(object.Float("AtomAgeMaxSize", 0.25f))
	    , ageZeroSize(object.Float("AtomAgeZeroSize", 2.0f))
	    , parentCondition(object.String("EmitConditionOfParent"))
	    , doScaling(object.Bool("DoScaling", true))
	    , deleteAtoms(object.Bool("DeleteAtoms", true))
	    , onlyAboveLand(object.Bool("EmitOnlyAboveLandscape", false))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		auto* parent = collection.parent;
		if (atomCreator == nullptr || parent == nullptr)
		{
			return false;
		}
		auto& data = CollectionDataOf<EmitterData>(collection, this);
		const float rate = static_cast<float>(maxAtoms) / ageZeroSize;
		if (!(rate > 0.0f))
		{
			return false;
		}
		if (parentCondition.empty() || effect.ConditionForAtom(parentCondition, *parent))
		{
			Emit(effect, collection, *parent, *atomCreator, data, rate);
		}
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const float age = effect.AtomAge(atom);
			if (doScaling)
			{
				atom.ruleScale = object_surface::EmittedAtomScale(age, pulseSpeed, pulseMagnitude, ageMaxSize, ageZeroSize);
			}
			if (deleteAtoms && age > ageZeroSize)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			++i;
		}
		return true;
	}

private:
	void Emit(Effect& effect, Collection& collection, const Atom& parent, const Creator& atomCreator, EmitterData& data,
	          float rate) const
	{
		const float owed = effect.GetDt() * rate;
		data.owed = owed + data.owed;
		if (!(data.owed > static_cast<float>(data.emitted)))
		{
			return;
		}
		// One point for the whole step: a parent that gives no point leaves it as it was, and each atom made turns it
		// into that atom's local position. (pending) The game starts it unset; here at the parent's place
		glm::vec3 point = effect.GlobalPosition(parent);
		do
		{
			if (static_cast<int32_t>(collection.atoms.size()) >= maxAtoms)
			{
				break;
			}
			++data.emitted;
			if (ParentSurfacePoint(effect, parent, point) == SurfacePointSource::Result::NotPorted)
			{
				static std::atomic_bool s_logged {false};
				// the game's logger is not there when a test runs on its own
				if (auto logger = spdlog::get("game"); logger != nullptr && !s_logged.exchange(true))
				{
					SPDLOG_LOGGER_WARN(logger, "PSys: ER_EmitFromParentAtom on an animated model or a creature: not ported");
				}
				break;
			}
			// Only a land at or above the point keeps the atom from being made (a point that is not a number is made)
			if (onlyAboveLand)
			{
				const auto cell = object_surface::LandTestCell(point);
				if (map_coords::ToWorld(map_coords::MapCoords {cell.x, cell.y, 0.0f}).y >= point.y)
				{
					continue;
				}
			}
			auto& atom = effect.NewAtom(collection, &atomCreator, nextGroups);
			point = effect.GlobalToLocal(collection, point);
			atom.position = point;
		} while (static_cast<float>(data.emitted) < data.owed);
	}

	/// The parent's random point: the rule that made it when that rule gives one; else the parent's place, for an atom
	/// with a creator; nothing for an atom without one
	static SurfacePointSource::Result ParentSurfacePoint(Effect& effect, const Atom& parent, glm::vec3& point)
	{
		for (const auto& [modifier, unused] : parent.modifierData)
		{
			if (const auto* source = dynamic_cast<const SurfacePointSource*>(modifier); source != nullptr)
			{
				return source->RandomSurfacePoint(effect, parent, point);
			}
		}
		if (parent.creator == nullptr)
		{
			return SurfacePointSource::Result::Nothing;
		}
		// (pending) A mesh atom gives a point of its mesh, which is not ported; no file emits from one
		if (parent.creator->kind == Creator::Kind::Mesh)
		{
			return SurfacePointSource::Result::NotPorted;
		}
		point = effect.GlobalPosition(parent);
		return SurfacePointSource::Result::Point;
	}

	std::string creator;
	std::vector<int> nextGroups;
	int maxAtoms;
	float pulseMagnitude;
	float pulseSpeed;
	float ageMaxSize;
	float ageZeroSize;
	std::string parentCondition;
	bool doScaling;
	bool deleteAtoms;
	bool onlyAboveLand;
};
} // namespace

std::optional<glm::vec3> object_surface::RandomPoint(Effect& effect, const Model& model, const Matrix& matrix)
{
	// (port guard) the game reads past an empty list; no model has one
	const auto parts = model.PartCount();
	if (parts == 0)
	{
		return std::nullopt;
	}
	const auto part = static_cast<uint32_t>(effect.Rand(static_cast<int32_t>(parts)));
	const auto items = model.ItemCount(part);
	if (items == 0)
	{
		return std::nullopt;
	}
	const auto item = static_cast<uint32_t>(effect.Rand(static_cast<int32_t>(items)));
	const auto triangles = model.TriangleCount(part, item);
	if (triangles == 0)
	{
		return std::nullopt;
	}
	const auto triangle = static_cast<uint32_t>(effect.Rand(static_cast<int32_t>(triangles)));
	const auto corners = model.Corners(part, item, triangle);
	float a = effect.Random(1.0f);
	float b = effect.Random(1.0f);
	if (a + b > 1.0f)
	{
		a = 1.0f - a;
		b = 1.0f - b;
	}
	return ToWorld(matrix, TrianglePoint(corners, a, b));
}

glm::vec3 object_surface::TrianglePoint(const std::array<glm::vec3, 3>& corners, float a, float b)
{
	const auto& [c0, c1, c2] = corners;
	const glm::vec3 alongSecond = (c1 - c0) * a;
	const glm::vec3 alongThird = (c2 - c0) * b;
	return (alongSecond + c0) + alongThird;
}

glm::vec3 object_surface::ToWorld(const Matrix& matrix, const glm::vec3& point)
{
	// z first, then y, then x, then the place, each sum a float
	const auto axis = [&](int i) {
		const float zy = (matrix.z[i] * point.z) + (matrix.y[i] * point.y);
		const float zyx = zy + (matrix.x[i] * point.x);
		return zyx + matrix.translation[i];
	};
	return {axis(0), axis(1), axis(2)};
}

float object_surface::EmittedAtomScale(float age, float pulseSpeed, float pulseMagnitude, float ageMaxSize, float ageZeroSize)
{
	// The cosine is taken at the FPU's full precision and rounded with the magnitude's product
	const float phase = (age * pulseSpeed) * k_TwoPi;
	float pulse = static_cast<float>(std::cos(static_cast<double>(phase)) * static_cast<double>(pulseMagnitude));
	pulse = (pulse * 0.5f) + 1.0f;
	if (!(pulse >= 0.0f))
	{
		pulse = 0.0f;
	}
	float size = 0.0f;
	if (!(age >= ageMaxSize))
	{
		size = age / ageMaxSize;
	}
	else
	{
		size = 1.0f - ((age - ageMaxSize) / (ageZeroSize - ageMaxSize));
	}
	if (!(size > 0.0f))
	{
		size = 0.0f;
	}
	else if (!(size < 1.0f))
	{
		size = 1.0f;
	}
	return size * pulse;
}

object_surface::Matrix object_surface::ObjectWorldMatrix(const glm::vec3& place, float yAngle, float scale)
{
	// the comparisons with 0 and 1 count a value that is not a number as equal
	const bool noAngle = !(yAngle < 0.0f || yAngle > 0.0f);
	const bool unitScale = !(scale < 1.0f || scale > 1.0f);
	Matrix matrix {.translation = place};
	if (noAngle)
	{
		const float diagonal = unitScale ? 1.0f : scale;
		matrix.x = {diagonal, 0.0f, 0.0f};
		matrix.y = {0.0f, diagonal, 0.0f};
		matrix.z = {0.0f, 0.0f, diagonal};
		return matrix;
	}
	// The cosine and sine are rounded only where they are stored or multiplied; the sums with 0 are the game's, which
	// make a zero positive
	const double cosine = std::cos(static_cast<double>(yAngle));
	const double sine = std::sin(static_cast<double>(yAngle));
	float c = static_cast<float>(cosine);
	float s = static_cast<float>(sine);
	float up = 1.0f;
	if (!unitScale)
	{
		c = static_cast<float>(cosine * static_cast<double>(scale));
		s = static_cast<float>(sine * static_cast<double>(scale));
		up = scale;
	}
	matrix.x = {0.0f + c, 0.0f, 0.0f + s};
	matrix.y = {0.0f, up, 0.0f};
	matrix.z = {0.0f - s, 0.0f, c - 0.0f};
	return matrix;
}

object_surface::Matrix object_surface::MobileWorldMatrix(const glm::vec3& place, const glm::mat3& rotation, float scale)
{
	Matrix matrix {.x = rotation[0], .y = rotation[1], .z = rotation[2], .translation = place};
	// a scale of 1, or one that is not a number, leaves the cells as they are
	if (scale < 1.0f || scale > 1.0f)
	{
		matrix.x = scale * matrix.x;
		matrix.y = scale * matrix.y;
		matrix.z = scale * matrix.z;
	}
	return matrix;
}

glm::ivec2 object_surface::LandTestCell(const glm::vec3& point)
{
	return {map_coords::MetresToFixedForHandLookup(point.x), map_coords::MetresToFixedForHandLookup(point.z)};
}

bool object_surface::IsGameObject(entt::entity entity)
{
	using namespace ecs::components;
	if (entity == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	// The components that stand for the game's object classes. A fish farm has no model and is still one
	return IsMobileClass(registry, entity) || registry.AnyOf<Abode, StoragePit, Workshop, Wonder, SpellDispenser>(entity) ||
	       registry.AnyOf<Field, FishFarm, Fixed, Feature, AnimatedStatic, BigForest, Tree, MagicTree>(entity) ||
	       registry.AnyOf<Mobile, Villager, Animal, Creature>(entity) ||
	       registry.AnyOf<MapShield, ScriptHighlight, SpellIcon, WorshipSpellIcon, TownCentreSpellIcon, TotemStatue>(entity) ||
	       registry.AnyOf<StreetLantern, MagicFireBall, SpellSeed, ShowNeedsVisuals>(entity) ||
	       registry.AnyOf<CitadelHeart, CitadelEntrance, WorshipSite, WorshipTotem>(entity);
}

object_surface::Matrix object_surface::WorldMatrixOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const ecs::components::Transform>(object);
	const auto place = map_coords::ToWorld(ecs::object::MapCoordsOf(object));
	const float scale = ecs::object::GetScale(object);
	// The rotation of the three angles is what the Transform holds: it is made from them, and from them again at the
	// end of the physics
	if (IsMobileClass(registry, object))
	{
		return MobileWorldMatrix(place, transform.rotation, scale);
	}
	// (pending) The temple's is its 3D object's own matrix, taken as the Transform's rotation, scale and position
	if (registry.AllOf<ecs::components::CitadelHeart>(object))
	{
		return {
		    .x = transform.rotation[0] * transform.scale.x,
		    .y = transform.rotation[1] * transform.scale.y,
		    .z = transform.rotation[2] * transform.scale.z,
		    .translation = transform.position,
		};
	}
	// The others turn about Y only, and their Transform holds the angle's cosine and sine as floats: with a scale of 1
	// those are the cells. (pending) Another scale is multiplied into the cosine before it is rounded, and openblack
	// keeps no angle for these objects: it is read back from the Transform
	if (!(scale < 1.0f || scale > 1.0f))
	{
		return MobileWorldMatrix(place, transform.rotation, 1.0f);
	}
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(transform.rotation, y, x, z);
	return ObjectWorldMatrix(place, y, scale);
}

void openblack::psys::RegisterObjectRules()

{
	RegisterModifier("CreateRule_GameObjectRef", MakeModifierOf<GameObjectRef>);
	RegisterModifier("ER_EmitFromParentAtom", MakeModifierOf<EmitFromParentAtom>);
}
