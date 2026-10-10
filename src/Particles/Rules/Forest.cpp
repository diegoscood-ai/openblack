/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle's butterflies (SF_Forest groups 5-7): UR_ForestPath moves each butterfly group round a sphere
// whose radius and height follow two key-point curves, and ParticleGoodEvilCreator makes butterflies or bats by the
// caster's alignment (the ParticleAnimCreator meshes, Creators/Mesh.cpp). UR_Flocking (the butterflies round their
// group) is in Flock.cpp. Wiki: docs/bw1-notes/miracles.md, "Lightning explosion and missing PSys classes".
// And the miracle's camera (SF_Forest group 2): ParticleAnimWithCameraCreator, an animated particle with no mesh whose
// camera path takes the caster's camera (Rules/Forest.h). Wiki: docs/bw1-notes/miracles.md, "Forest".

#include "Particles/Rules/Forest.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <exception>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/resource/resource.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "3D/L3DAnim.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManagerState.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/KeyPoints.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::vector<float> Floats(const Object& object, std::string_view key)
{
	const auto it = object.properties.find(key);
	return it != object.properties.end() ? it->second.numbers : std::vector<float> {};
}

/// What UR_ForestPath keeps for one atom: the first-step flag and three random angles in 0..2 pi
struct ForestPathData
{
	bool first {true};
	float theta {0.0f};
	float phi {0.0f};
	float unused {0.0f};
};

/// UR_ForestPath (defaults: ThetaSpeed, PhiSpeed, SphereRadius and ScaleX/Y/Z all 1, ScaleSphereRadius none,
/// RadiusSpline and HeightSpline with their own default keys, both with zero end slopes)
class ForestPath final: public Modifier
{
public:
	explicit ForestPath(const Object& object)
	    : thetaSpeed(object.Float("ThetaSpeed", 1.0f))
	    , phiSpeed(object.Float("PhiSpeed", 1.0f))
	    , sphereRadius(object.Float("SphereRadius", 1.0f))
	    , radiusScale(object.String("ScaleSphereRadius"))
	    , scale(object.Float("ScaleX", 1.0f), object.Float("ScaleY", 1.0f), object.Float("ScaleZ", 1.0f))
	    // the default keys (four per curve) are not ported: every spell file gives both curves
	    , radiusCurve(key_points::Make(Floats(object, "RadiusSpline")))
	    , heightCurve(key_points::Make(Floats(object, "HeightSpline")))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// the curves at the collection's age (the original leaves the outputs uninitialised:
		// (approximate) 0 here, only reached by a curve without keys)
		const float age = effect.CollectionAge(collection);
		const float r = key_points::Evaluate(radiusCurve, age, 0.0f);
		const float h = key_points::Evaluate(heightCurve, age, 0.0f);
		constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
		for (auto& atomPtr : collection.atoms)
		{
			auto& atom = *atomPtr;
			auto& data = AtomDataOf<ForestPathData>(atom, this);
			if (data.first)
			{
				data.first = false;
				data.theta = effect.Random(k_TwoPi);
				data.phi = effect.Random(k_TwoPi);
				data.unused = effect.Random(k_TwoPi);
			}
			const float atomAge = effect.AtomAge(atom);
			const float theta = std::fmod(atomAge * thetaSpeed + data.theta, k_TwoPi);
			const float phi = std::fmod(atomAge * phiSpeed + data.phi, k_TwoPi);
			// R = SphereRadius x RadiusSpline (x ScaleSphereRadius's value when there is one)
			float radius = sphereRadius * r;
			if (!radiusScale.empty())
			{
				radius *= effect.FloatProvider(radiusScale, 1.0f);
			}
			glm::vec3 p = radius * glm::vec3(std::cos(theta) * scale.x * std::cos(phi), std::sin(phi) * scale.y,
			                                 std::sin(theta) * scale.z * std::cos(phi));
			// as UR_SphereSurfaceTracer (PSys.cpp): outside a hierarchy, + the current parent position
			if (!collection.hierarchy)
			{
				p += collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
			}
			p.y += h;
			// velocity = the move x 1 / the step; (port guard) the step is never 0 here
			atom.velocity = (p - atom.position) * (1.0f / std::max(effect.GetDt(), 1e-4f));
			atom.position = p;
		}
		return true;
	}
	float thetaSpeed, phiSpeed, sphereRadius;
	std::string radiusScale;
	glm::vec3 scale;
	key_points::Spline radiusCurve, heightCurve;
};

/// ParticleGoodEvilCreator (PCreatorEvil, PCreatorGood, AlignmentSwitch, default -0.5): the evil creator when the
/// effect has a player whose alignment is under AlignmentSwitch and there is one, else the good one.
struct GoodEvilCreator final: Creator
{
	std::string evil, good;
	float alignmentSwitch {-0.5f};

	[[nodiscard]] const Creator* Resolve(const Effect& effect) const override
	{
		const auto* evilCreator = effect.FindCreator(evil);
		if (effect.GetPlayer() >= 0 && evilCreator != nullptr &&
		    ecs::effects::alignment::Get(static_cast<PlayerNames>(effect.GetPlayer())) < alignmentSwitch)
		{
			return evilCreator->Resolve(effect);
		}
		const auto* goodCreator = effect.FindCreator(good);
		return goodCreator != nullptr ? goodCreator->Resolve(effect) : this;
	}
};

std::unique_ptr<Creator> MakeGoodEvilCreator(const Object& object)
{
	auto creator = std::make_unique<GoodEvilCreator>();
	ReadCreatorProperties(object, *creator);
	creator->evil = object.String("PCreatorEvil");
	creator->good = object.String("PCreatorGood");
	creator->alignmentSwitch = object.Float("AlignmentSwitch", -0.5f);
	return creator;
}

/// A spell file's path (".\Data\SPELLS\Anims\Forest.cam") under the data folder ("SPELLS/Anims/Forest.cam")
std::string DataRelativePath(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.starts_with("./"))
	{
		path = path.substr(2);
	}
	if (path.size() > 5 && (path.starts_with("Data/") || path.starts_with("data/")))
	{
		path = path.substr(5);
	}
	return path;
}

/// CameraFileName: the .cam loaded once by its name into the camera path cache, as the creator loads its file once. 0
/// for none (NULL_STRING) or a file that cannot be read
entt::id_type SharedCameraPath(std::string path)
{
	path = DataRelativePath(std::move(path));
	if (path.empty() || path == "NULL_STRING")
	{
		return 0;
	}
	const auto id = entt::hashed_string(("psys/" + path).c_str()).value();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return id;
	}
	auto& cameraPaths = Locator::resources::value().GetCameraPaths();
	if (cameraPaths.Contains(id))
	{
		return id;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		cameraPaths.Load(id, resources::CameraPathLoader::FromDiskTag {},
		                 fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / path));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: camera path {}: {}", path, e.what());
		return 0;
	}
	return id;
}

/// The key a camera particle places its path by: its atom, which keeps one address from its creation to its removal
ecs::systems::CameraPathSystemInterface::PathOwner OwnerOf(const Atom& atom)
{
	return static_cast<ecs::systems::CameraPathSystemInterface::PathOwner>(reinterpret_cast<uintptr_t>(&atom));
}

/// The particle's matrix: its rotation times its scale, the Y axis times its stretch, and its position, as its draw
/// places it
glm::mat4 PathPlacement(const Atom& atom)
{
	glm::mat3 axes = atom.current.rotation * atom.current.scale;
	axes[1] *= atom.current.stretch;
	glm::mat4 placement(axes);
	placement[3] = glm::vec4(atom.current.position, 1.0f);
	return placement;
}

struct AnimWithCameraCreator;

/// One live camera particle: its effect, its atom and creator, its flags and its last drawn frame
struct CameraParticle
{
	const Effect* effect {nullptr};
	const Atom* atom {nullptr};
	const AnimWithCameraCreator* creator {nullptr};
	forest_camera::Flags flags {};
	int32_t drawnFrame {-1};
};

/// What this file keeps between calls (Locator::particleSystem): the live camera particles, oldest first
struct CameraParticles
{
	std::vector<CameraParticle> particles;
};

/// None without a particle system: the particles then never ask for the camera
CameraParticles* LiveCameraParticles()
{
	return Locator::particleSystem::has_value() ? &Locator::particleSystem::value().Module<CameraParticles>() : nullptr;
}

/// The live camera particle of that atom, if any
std::vector<CameraParticle>::iterator FindCameraParticle(CameraParticles& live, const Atom& atom)
{
	return std::ranges::find(live.particles, &atom, &CameraParticle::atom);
}

void ReleaseCamera(const Atom& atom)
{
	if (Locator::cameraPathSystem::has_value())
	{
		Locator::cameraPathSystem::value().Release(OwnerOf(atom));
	}
}

/// ParticleAnimWithCameraCreator (CameraFileName, PauseBeforePlay, default 4): a ParticleAnimCreator, here with no mesh,
/// whose particle carries the camera path. Its atoms are made, stepped and collected as the anim creator's, and with no
/// mesh nothing of them is drawn
struct AnimWithCameraCreator final: MeshCreator
{
	explicit AnimWithCameraCreator(const MeshCreator& anim)
	    : MeshCreator(anim)
	{
	}

	/// The path in the camera path cache, 0 for none
	entt::id_type cameraPathId {0};
	float pauseSeconds {forest_camera::k_DefaultPauseSeconds};

	/// The clip's length in whole ms, 0 without a clip
	[[nodiscard]] int32_t ClipMilliseconds() const
	{
		if (animId == 0 || !Locator::resources::has_value())
		{
			return 0;
		}
		const auto& animations = Locator::resources::value().GetAnimations();
		return animations.Contains(animId) ? animations.Handle(animId)->GetDurationMs() : 0;
	}

	/// The path in the camera path cache, none when it is not there
	[[nodiscard]] entt::resource<CameraPath> Path() const
	{
		if (cameraPathId == 0 || !Locator::resources::has_value())
		{
			return {};
		}
		auto& cameraPaths = Locator::resources::value().GetCameraPaths();
		return cameraPaths.Contains(cameraPathId) ? cameraPaths.Handle(cameraPathId) : entt::resource<CameraPath> {};
	}

	/// The anim creator's part, then the particle is a live camera particle that wants the camera
	void InitAtom(Effect& effect, Atom& atom) const override
	{
		MeshCreator::InitAtom(effect, atom);
		if (auto* live = LiveCameraParticles(); live != nullptr)
		{
			live->particles.push_back({.effect = &effect, .atom = &atom, .creator = this});
		}
	}

	/// The particle's own step, after its frame step (forest_camera::Step)
	void AfterAtomStep(Effect& effect, Atom& atom) const override
	{
		auto* live = LiveCameraParticles();
		if (live == nullptr)
		{
			return;
		}
		const auto particle = FindCameraParticle(*live, atom);
		if (particle == live->particles.end())
		{
			return;
		}
		const auto actions =
		    forest_camera::Step(particle->flags, effect.IsMyInterfaceCasting(), effect.AtomAge(atom), pauseSeconds);
		if (actions.release)
		{
			ReleaseCamera(atom);
		}
		if (actions.begin && Locator::cameraPathSystem::has_value())
		{
			// refused while a script holds the camera, or without a path, and then never asked for again
			[[maybe_unused]] const bool taken =
			    Locator::cameraPathSystem::value().Begin(OwnerOf(atom), Path(), PathPlacement(atom), pauseSeconds);
		}
		if (actions.playAnim)
		{
			atom.playAnim = true;
		}
	}

	/// The particle goes: it lets go of the camera if it asked for it, and is no longer a live camera particle
	void AtomRemoved(Effect& /*effect*/, Atom& atom) const override
	{
		auto* live = LiveCameraParticles();
		if (live == nullptr)
		{
			return;
		}
		const auto particle = FindCameraParticle(*live, atom);
		if (particle == live->particles.end())
		{
			return;
		}
		if (forest_camera::Removed(particle->flags))
		{
			ReleaseCamera(atom);
		}
		live->particles.erase(particle);
	}
};

std::unique_ptr<Creator> MakeAnimWithCameraCreator(const Object& object)
{
	// The anim creator's own reading of the same properties. Its mesh is MeshFileName NULL_STRING with no MeshEnum, that
	// is none: the mesh properties are left out, so that no file of that name is looked for
	Object anim = object;
	anim.className = "ParticleAnimCreator";
	anim.properties.erase("MeshFileName");
	anim.properties.erase("MeshEnum");
	const auto factory = FindCreatorFactory(anim.className);
	const auto made = factory != nullptr ? factory(anim) : std::unique_ptr<Creator> {};
	const auto* animCreator = dynamic_cast<const MeshCreator*>(made.get());
	MeshCreator fallback;
	if (animCreator == nullptr)
	{
		ReadCreatorProperties(object, fallback);
		animCreator = &fallback;
	}
	auto creator = std::make_unique<AnimWithCameraCreator>(*animCreator);
	creator->className = object.className;
	creator->pauseSeconds = object.Float("PauseBeforePlay", forest_camera::k_DefaultPauseSeconds);
	creator->cameraPathId = SharedCameraPath(object.String("CameraFileName"));
	return creator;
}
} // namespace

void forest_camera::UpdateFrame(float turnFraction)
{
	auto* live = LiveCameraParticles();
	if (live == nullptr || live->particles.empty())
	{
		return;
	}
	auto& effects = Locator::particleSystem::value().GetState().effects;
	for (auto& particle : live->particles)
	{
		// the draw fraction its effect is drawn at, as the collects take it
		float t = turnFraction;
		for (const auto& entry : effects)
		{
			const auto& running = entry.second;
			if (running.effect.get() == particle.effect)
			{
				t = running.perFrame ? 1.0f : turnFraction;
				break;
			}
		}
		particle.drawnFrame = particle.atom->DrawnFrame(t);
		const auto actions = forest_camera::Draw(particle.flags, particle.creator->ClipMilliseconds(), particle.drawnFrame);
		if (Locator::cameraPathSystem::has_value())
		{
			auto& cameraPaths = Locator::cameraPathSystem::value();
			const auto owner = OwnerOf(*particle.atom);
			cameraPaths.FollowAt(owner, actions.pathMilliseconds, particle.flags.playing);
			if (actions.release)
			{
				cameraPaths.Release(owner);
			}
		}
	}
}

std::vector<forest_camera::Watch> forest_camera::Watches()
{
	std::vector<Watch> watches;
	if (const auto* live = LiveCameraParticles(); live != nullptr)
	{
		for (const auto& particle : live->particles)
		{
			watches.push_back({.owner = OwnerOf(*particle.atom), .drawnFrame = particle.drawnFrame, .flags = particle.flags});
		}
	}
	return watches;
}

void openblack::psys::RegisterForestRules()
{
	RegisterModifier("UR_ForestPath", MakeModifierOf<ForestPath>);
	RegisterCreator("ParticleGoodEvilCreator", MakeGoodEvilCreator);
	// the forest's camera particle, built on ParticleAnimCreator (Creators/Mesh.cpp, registered before this file)
	RegisterCreator("ParticleAnimWithCameraCreator", MakeAnimWithCameraCreator);
}
