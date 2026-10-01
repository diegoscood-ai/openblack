/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The storm miracle's classes (PSysTornado.cpp 0x6D15D0-0x6D6160): UR_CloudMoverNew, UR_CloudGather, UR_Tornado and
// UR_StormCast. Every constant below is the class ctor's default or a literal of the function it is read from; the
// spell files replace the defaults with their properties. The disassembly read for this port is kept in
// tmp_dis\miracles\impl\m6st\ (tornado.txt, stormcast.txt, drawclouds.txt, props.py for the property offsets). Wiki:
// docs/bw1-notes/miracles.md, "Tormenta".

#include "Storm.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <memory>
#include <numbers>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "Audio/SpellSounds.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Trees.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Weather/Weather.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "PSys/Noise.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Rules/Shield.h"
#include "PSys/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr float k_TwoPi = 6.28318548f; ///< 0x40C90FDB (the PSysFloatRand of every angle here)

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// PSysManager::PSysFloatRand(a, b) 0x6729C0: a + rand(b - a)
float RandRange(Effect& effect, float a, float b)
{
	return a + effect.Random(b - a);
}

float Clamp01(float x)
{
	return std::clamp(x, 0.0f, 1.0f);
}

/// A modifier's CollectionData in a collection's +0x24 list (made on the first call, as every ModifyAtomCollection
/// here does with ??2PSysBase + its ctor)
template <class T>
T& CollectionDataOf(Collection& collection, const Modifier* modifier)
{
	auto& slot = collection.modifierData[modifier];
	if (slot == nullptr)
	{
		slot = std::make_shared<T>();
	}
	return *std::static_pointer_cast<T>(slot);
}

/// The same for an atom's +0x24 list (AtomData)
template <class T>
T& AtomDataOf(Atom& atom, const Modifier* modifier)
{
	auto& slot = atom.modifierData[modifier];
	if (slot == nullptr)
	{
		slot = std::make_shared<T>();
	}
	return *std::static_pointer_cast<T>(slot);
}

/// AtomCollection::GetCurrentParentPos 0x674AF0: the parent atom's position, the manager's origin without one
glm::vec3 ParentPosition(const Effect& effect, const Collection& collection)
{
	return collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin();
}

/// PSysManager::GetCurrentHeading 0x673660: the PSysProcessInfo's cameraForward (manager +0x3C) with y = 0, as it is
glm::vec3 CurrentHeading(const Effect& effect)
{
	const auto& forward = effect.GetProcessInfo().cameraForward;
	return {forward.x, 0.0f, forward.z};
}

entt::entity SpellOf(const Effect& effect)
{
	return effect.GetSink() != nullptr ? effect.GetSink()->SpellEntity() : entt::null;
}

bool g_TraceRead = false;
bool g_Trace = false;
size_t g_Strikes = 0; ///< every gather's strikes since the program started (the test hooks)

/// Set when the program exits: the effects (psys::manager's statics) may outlive the storm registry and the carried
/// set, so the data destructors below must not touch them then (port bookkeeping, not in the original). ExitGuard is a
/// function-local static made on first use, after every namespace-scope static, so it is destroyed before them.
bool g_Exiting = false;
struct ExitGuard
{
	ExitGuard() = default;
	ExitGuard(const ExitGuard&) = delete;
	ExitGuard& operator=(const ExitGuard&) = delete;
	ExitGuard(ExitGuard&&) = delete;
	ExitGuard& operator=(ExitGuard&&) = delete;
	~ExitGuard() { g_Exiting = true; }
};
void TouchExitGuard()
{
	static ExitGuard guard;
}

// ================================================================================================================
// UR_CloudMoverNew (ctor 0x6D4150, props 0x6ACD10, ModifyAtomCollection 0x6D41C0)
// ================================================================================================================

/// CollectionData@UR_CloudMoverNew (0x24, ctor 0x560C10): +0x20 the first call
struct CloudMoverData
{
	bool first {true};
};

class CloudMoverNew final: public Modifier
{
public:
	explicit CloudMoverNew(const Object& object)
	    : delayBeforeMove(object.Float("DelayBeforeMove", 0.0f))    // +0x20 (ctor 0)
	    , windDamping(object.Float("WindDamping", 0.06f))          // +0x24 (ctor 0x3D75C28F)
	    , windMagnification(object.Float("WindMagnification", 60.0f)) // +0x28 (ctor 0x42700000)
	{
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<CloudMoverData>(collection, this);
		// 0x6D4242: the first call puts every atom at the manager's origin
		if (data.first)
		{
			data.first = false;
			for (auto& atom : collection.atoms)
			{
				atom->position = effect.GetOrigin();
			}
		}
		// 0x6D427B: nothing before the manager's age reaches DelayBeforeMove
		if (effect.GetAge() < delayBeforeMove)
		{
			return true;
		}
		const float dt = effect.GetDt(); // [0xD4E0EC]
		const bool script = effect.GetSink() != nullptr && effect.GetSink()->IsScriptCasting();
		for (auto& atom : collection.atoms)
		{
			const glm::vec3 old = atom->position;
			// 0x6D42EA: no wind for a script's storm; else fn_00771B10(p, smooth 1), the climate wind / 8
			const glm::vec3 wind = script ? glm::vec3(0.0f) : weather::GetWindAt(old, true);
			// 0x6D433A..0x6D446D: T = wind x WindMagnification x 0.1; v += WindDamping x (T - v) x dt
			const glm::vec3 target = wind * windMagnification * 0.1f;
			atom->velocity += windDamping * (target - atom->velocity) * dt;
			// 0x6D452C..0x6D4601 measure the slope between here and the next step and scale a copy of the horizontal
			// velocity by 1 - 0.9 x (angle / (pi / 2)): that copy is never read (the position below uses the velocity
			// just stored), so the slope has no effect in W120
			// 0x6D466C: p += v x dt (fn_0044E9F0, fn_004605F0)
			atom->position = old + atom->velocity * dt;
			// 0x6D46A7: the spell's position (+0x14) follows the core (MapCoords::Set 0x603340)
			const auto spell = SpellOf(effect);
			if (spell != entt::null)
			{
				auto& registry = Locator::entitiesRegistry::value();
				if (auto* component = registry.TryGet<ecs::components::Spell>(spell); component != nullptr)
				{
					component->position = magic::ToMap(atom->position);
				}
			}
			// 0x6D46C3: the tornado's cores bounce off shields
			if (effect.PowerUpLevel() == 1)
			{
				shields::DoAnyShieldDeflections(effect, *atom, old);
			}
		}
		return true;
	}

private:
	float delayBeforeMove;
	float windDamping;
	float windMagnification;
};

// ================================================================================================================
// UR_CloudGather (ctor 0x6D4700, props 0x6ACD70, ModifyAtomCollection 0x6D4A70)
// ================================================================================================================

/// AtomData@UR_CloudGather (0x38, ctor 0x560B60)
struct CloudAtomData
{
	float radius {0.0f};    ///< +0x20
	float theta {0.0f};     ///< +0x24
	bool flashing {false};  ///< +0x2C the cloud lit by its lightning
	float flashStart {0.0f}; ///< +0x30 the atom age at the strike
	float height {0.0f};    ///< +0x34 rand(HeightVaryAmount)
	uint32_t specular {0xFF000000u}; ///< the atom's +0x90 (its LH3DMist's specular; not drawn, see below)
};

/// CollectionData@UR_CloudGather (0x58, ctor 0x560AF0; its dtor fn_006D4900)
struct CloudGatherData
{
	float emitted {0.0f};   ///< +0x20
	int count {0};          ///< +0x24
	bool first {true};      ///< +0x28
	float rate {0.0f};      ///< +0x2C atoms per second
	float nextStrike {0.0f}; ///< +0x30 collection age of the next lightning
	float strikeEnd {0.0f};  ///< +0x34 collection age at which the strike ends
	float spin {1.0f};       ///< +0x38 (ctor 1.0)
	Collection* lightning {nullptr}; ///< +0x3C the LightningGroup collection
	Atom* lightningCloud {nullptr};  ///< +0x40 the cloud it hangs from while it strikes
	bool detached {false};  ///< +0x44 +0x3C is in no atom (then this data owns it)
	bool lightningOn {false}; ///< +0x45 GetPowerUpLevel() != -1
	bool rainOn {false};    ///< +0x46
	bool registered {false}; ///< +0x47 this collection registers the LH3DStorm
	glm::vec3 heading {0.0f}; ///< +0x48
	weather::storms::StormId storm {weather::storms::k_NoStorm}; ///< +0x54 the GWeather
	std::unique_ptr<Collection> orphan; ///< the port's owner of +0x3C while it hangs from no atom

	CloudGatherData() { TouchExitGuard(); }
	CloudGatherData(const CloudGatherData&) = delete;
	CloudGatherData& operator=(const CloudGatherData&) = delete;
	CloudGatherData(CloudGatherData&&) = delete;
	CloudGatherData& operator=(CloudGatherData&&) = delete;
	/// fn_006D4900: a detached lightning collection is deleted, and fn_006D4950 marks the storm (fn_0083F7B0)
	~CloudGatherData()
	{
		orphan.reset();
		if (storm != weather::storms::k_NoStorm && !g_Exiting)
		{
			weather::storms::MarkForDeletion(storm);
		}
	}
};

/// The clouds of the current collection that are past half formed (the static GJArray 0xD4EE88, count 0xD4EE90, grow
/// 10: shared by every gather, emptied at each call)
std::vector<Atom*> g_Clouds;

/// fn_00674A30(atom)'s unlinking: takes `collection` out of whichever cloud of `clouds` holds it (nullptr: none does)
std::unique_ptr<Collection> Detach(Collection& clouds, const Collection* collection)
{
	for (auto& atom : clouds.atoms)
	{
		auto& subs = atom->subCollections;
		const auto it = std::find_if(subs.begin(), subs.end(), [collection](const auto& sub) { return sub.get() == collection; });
		if (it != subs.end())
		{
			auto detached = std::move(*it);
			subs.erase(it);
			detached->parent = nullptr;
			return detached;
		}
	}
	return nullptr;
}

/// The cloud of `clouds` that holds `collection`, or nullptr
Atom* HolderOf(Collection& clouds, const Collection* collection)
{
	for (auto& atom : clouds.atoms)
	{
		for (const auto& sub : atom->subCollections)
		{
			if (sub.get() == collection)
			{
				return atom.get();
			}
		}
	}
	return nullptr;
}

class CloudGather final: public Modifier
{
public:
	explicit CloudGather(const Object& object)
	    : creator(object.String("PCreator"))                       // +0x28
	    , maxRadius(object.Float("MaxRadius", 100.0f))              // +0x2C (read by no code)
	    , numAtoms(object.Int("NumAtoms", 20))                      // +0x30
	    , maxAngularSpeed(object.Float("MaxAngularSpeed", 0.3f))    // +0x34
	    , timeToForm(object.Float("TimeToForm", 10.0f))             // +0x38
	    , fracToMaxSize(object.Float("FracToMaxSize", 0.5f))        // +0x3C
	    , maxColor(object.Int("MaxColor", 255))                     // +0x40
	    , minColor(object.Int("MinColor", 50))                      // +0x44
	    , minScaleFactor(object.Float("MinScaleFactor", 0.0f))      // +0x48
	    , maxScaleFactor(object.Float("MaxScaleFactor", 1.0f))      // +0x4C
	    , minAlpha(object.Int("MinAlpha", 100))                     // +0x50
	    , maxAlpha(object.Int("MaxAlpha", 100))                     // +0x54
	    , lightningGroup(object.Int("LightningGroup", -1))          // +0x58
	    , specLife(object.Float("SpecLife", 0.2f))                  // +0x5C
	    , lightningLife(object.Float("LightningLife", 0.5f))        // +0x60
	    , lightningDelay(object.Float("LightningDelay", 4.0f))      // +0x64
	    , switchLife(object.Float("SwitchLife", 0.5f))              // +0x68
	    , heightVaryAmount(object.Float("HeightVaryAmount", 0.0f))  // +0x6C
	    , cloudRatioMaxCollection(object.Float("CloudRatioMaxCollection", 2.0f)) // +0x70
	    , collectionRadiusInitialScale(object.Float("CollectionRadiusInitialScale", 3.0f)) // +0x74
	    , maxCloudRatio(object.Float("MaxCloudRatio", 5.0f))        // +0x78
	    , minCloudRatio(object.Float("MinCloudRatio", 1.0f))        // +0x7C
	    , createAllAtOnce(object.Bool("CreateAllAtOnce", false))    // +0x80
	    , tornadoGroup(object.Int("TornadoGroup", 10))              // +0x84
	    , windMaxSpeed(object.Float("WindMaxSpeed", 100.0f))        // +0x88
	    , windMinSpeed(object.Float("WindMinSpeed", 40.0f))         // +0x8C
	    , magnitudeForWindMaxSpeed(object.Float("MagnitudeForWindMaxSpeed", 100.0f)) // +0x90
	    , magnitudeForWindMinSpeed(object.Float("MagnitudeForWindMinSpeed", 20.0f))  // +0x94
	    , radiusProvider(object.String("RadiusFloatProvider"))      // +0x98
	    , scaleProvider(object.String("ScaleFloatProvider"))        // +0x9C
	    , cloudHeightProvider(object.String("CloudHeight"))         // +0xA0
	    , soundLightning(ReadSoundAction(object, "SoundLightning")) // +0xA4
	{
	}

	[[nodiscard]] bool Creates() const override { return true; } // ctor 0x6D471B: flags |= 6

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// 0x6D4A7A: the three float providers must exist
		if (radiusProvider.empty() || scaleProvider.empty() || cloudHeightProvider.empty())
		{
			return false;
		}
		auto& data = CollectionDataOf<CloudGatherData>(collection, this);
		// 0x6D4B14: IsInState(1), closing: the storm is marked (fn_006D4950) and no longer registered
		if (effect.Closing())
		{
			if (data.storm != weather::storms::k_NoStorm)
			{
				weather::storms::MarkForDeletion(data.storm);
				data.storm = weather::storms::k_NoStorm;
			}
			data.registered = false;
		}
		const glm::vec3 parentPos = ParentPosition(effect, collection);
		const int level = effect.PowerUpLevel();
		const auto* particleCreator = effect.FindCreator(creator);
		const float dt = effect.GetDt();
		if (data.first)
		{
			if (particleCreator == nullptr)
			{
				return false; // 0x6D4B5D
			}
			data.lightningOn = level != -1;
			data.rainOn = true;
			data.registered = false;
			data.heading = CurrentHeading(effect);
			// 0x6D4B8C: only the gather under the first cloud core registers the storm (fn_00673CA0: the parent's index
			// in its collection is 0; [0xC09790] is 1), and at level 1 it gets the tornado too
			if (collection.parent != nullptr && collection.parent->collection != nullptr &&
			    !collection.parent->collection->atoms.empty() && collection.parent->collection->atoms.front().get() == collection.parent)
			{
				data.registered = true;
				if (level == 1)
				{
					effect.AddSubCollections(*collection.parent, {tornadoGroup});
				}
			}
			data.rate = static_cast<float>(numAtoms) / timeToForm;
			data.spin = effect.Random(1.0f) < 0.5f ? -1.0f : 1.0f; // 0x8AB678 / 0x8AA390
			data.nextStrike = RandRange(effect, 0.5f, 1.0f) * switchLife + lightningDelay;
			if (createAllAtOnce)
			{
				CreateAllAtOnce(effect, collection, parentPos, particleCreator);
			}
		}
		// 0x6D4C27: emit at the rate while the count is below the emitted amount and NumAtoms
		data.emitted += dt * data.rate;
		while (static_cast<float>(data.count) < data.emitted && static_cast<int>(collection.atoms.size()) < numAtoms &&
		       particleCreator != nullptr)
		{
			++data.count;
			auto& atom = effect.NewAtom(collection, particleCreator, {});
			InitCloud(effect, atom, AtomDataOf<CloudAtomData>(atom, this));
		}
		// 0x6D4CF3: the strike ends: the lightning collection is let go of its cloud (fn_00674A30(0)); it stays with the
		// gather, not updated, until the next strike
		if (data.lightningCloud != nullptr && effect.CollectionAge(collection) > data.strikeEnd)
		{
			if (auto detached = Detach(collection, data.lightning); detached != nullptr)
			{
				data.orphan = std::move(detached);
			}
			data.lightningCloud = nullptr;
			data.detached = true;
		}
		// 0x6D4D68: the collection's own ramps, over its first 10 s
		const float ratioScale = storm::CollectionScale(effect.CollectionAge(collection), cloudRatioMaxCollection);
		const float radiusScale = storm::CollectionScale(effect.CollectionAge(collection), collectionRadiusInitialScale);
		g_Clouds.clear();
		const float scaleValue = effect.FloatProvider(scaleProvider, 0.0f);
		const float cloudHeight = effect.FloatProvider(cloudHeightProvider, 0.0f);
		const float invSpecLife = 1.0f / specLife;
		for (auto& atom : collection.atoms)
		{
			auto& ad = AtomDataOf<CloudAtomData>(*atom, this);
			float age = effect.AtomAge(*atom);
			// 0x6D4F4D: a cloud older than TimeToForm starts again (fn_006D4880, age 0)
			if (age > timeToForm)
			{
				InitCloud(effect, *atom, ad);
				age = effect.AtomAge(*atom);
			}
			const float f = age / timeToForm;
			// 0x6D4F8E: [0xC0979C]: fn_006D56B0(0.2, 2 - 2f) past half formed, its result discarded
			if (f > 0.5f)
			{
				g_Clouds.push_back(atom.get()); // 0x6D4FD8 ([0xC0978C])
			}
			// 0x6D5012: the angular speed peaks at half formed ([0xC097B0]: x (1 - (2f - 1)^2))
			const float spin = data.spin * maxAngularSpeed;
			const float w = spin * (1.0f - (2.0f * f - 1.0f) * (2.0f * f - 1.0f));
			ad.theta += w * dt;
			ad.theta = std::fmod(ad.theta, 6.2831854820251465f); // [0xC097B4], the double 0x8D45D8 (2 pi)
			// 0x6D5064 [0xC09798]: the radius wobbles by 1 + 0.3 cos(collection age x 0.1 + theta)
			const float wobble = 1.0f + 0.3f * std::cos(effect.CollectionAge(collection) * 0.1f + ad.theta);
			const float r = (1.0f - f) * ad.radius * wobble * radiusScale;
			glm::vec3 position(std::cos(ad.theta) * r + parentPos.x, parentPos.y, std::sin(ad.theta) * r + parentPos.z);
			const auto look = storm::CloudLookAt(
			    f, {fracToMaxSize, minCloudRatio, maxCloudRatio, minColor, maxColor, minAlpha, maxAlpha, minScaleFactor,
			        maxScaleFactor, scaleValue});
			// 0x6D51C9: +0x8C = (alpha, c, c, c); +0x78 the scale; +0x7C the ratio x the collection's ratio scale
			atom->colour = {static_cast<uint8_t>(look.colour), static_cast<uint8_t>(look.colour),
			                static_cast<uint8_t>(look.colour), static_cast<uint8_t>(look.alpha)};
			atom->ruleScale = look.scale;
			atom->stretch = look.ratio * ratioScale;
			// 0x6D51D5: y = baseScale x height x ruleScale + the CloudHeight provider (the land is added below)
			position.y = atom->baseScale * ad.height * atom->ruleScale + cloudHeight;
			atom->position = position;
			// 0x6D5205: the lightning's light on its cloud, SpecLife long: +0x90 = (A, R, G, B) = (v, 200v/256, 200v/256,
			// v) >> 8 with v = ftol((1 - t / SpecLife) x 255) (bytes 0x6D524C..0x6D5283). The LH3DMist effect branch
			// that draws the clouds has no specular (fn_007FA300), so it is kept and not drawn.
			if (ad.flashing && age - ad.flashStart > specLife)
			{
				ad.flashing = false;
				ad.specular = 0xFF000000u;
			}
			if (ad.flashing)
			{
				const auto v = static_cast<uint32_t>(static_cast<int>((1.0f - (age - ad.flashStart) * invSpecLife) * 255.0f)) & 0xFFu;
				const uint32_t a = (v * 255u) >> 8u;
				const uint32_t rg = (v * 200u) >> 8u;
				ad.specular = (a << 24u) | (rg << 16u) | (rg << 8u) | a;
			}
		}
		// 0x6D52A0: the next strike
		if (data.lightningOn && !g_Clouds.empty() && effect.CollectionAge(collection) - data.nextStrike > 0.0f &&
		    !effect.Closing())
		{
			Strike(effect, collection, data);
		}
		// 0x6D54D8 [0xC09784]: the clouds sit at the mean land altitude of a 3 x 3 grid of 0.33 x radius around the
		// core (the tornado's: at the manager's origin, the tornado's base)
		float height = effect.GetOrigin().y;
		if (level != 1)
		{
			const float r = effect.FloatProvider(radiusProvider, 0.0f) * 0.33f; // 0x8CA268
			height = 0.0f;
			for (int i = -1; i < 2; ++i)
			{
				for (int j = -1; j < 2; ++j)
				{
					height += LandAt(parentPos.x + static_cast<float>(i) * r, parentPos.z + static_cast<float>(j) * r);
				}
			}
			height *= 0.111111f; // 0x93A604
		}
		// 0x6D55B9 [0xC09788]: every cloud up by that height
		for (auto& atom : collection.atoms)
		{
			atom->position.y += height;
		}
		// 0x6D561F: the LH3DStorm, registered once by the first core's gather, kept alive at the core every step
		if (data.storm == weather::storms::k_NoStorm && data.registered)
		{
			storm::GatherStormInput input;
			input.radius = effect.FloatProvider(radiusProvider, 0.0f);
			input.magnitude = effect.GetMagnitude();
			input.power = effect.GetProcessInfo().power;
			input.heading = data.heading;
			input.rainAmount = RainAmountOf(effect);
			input.rainOn = data.rainOn;
			input.timeToForm = timeToForm;
			input.cloudHeight = cloudHeight;
			input.windMinSpeed = windMinSpeed;
			input.windMaxSpeed = windMaxSpeed;
			input.magnitudeForWindMinSpeed = magnitudeForWindMinSpeed;
			input.magnitudeForWindMaxSpeed = magnitudeForWindMaxSpeed;
			data.storm = weather::storms::Create(storm::GatherStormDescriptor(input)); // fn_0083F6F0
			if (storm::TraceEnabled())
			{
				const auto* created = weather::storms::Find(data.storm);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Storm: gather registers storm {} at ({:.1f}, {:.1f}): inner {:.1f} outer {:.1f} rain {} "
				                   "overcast {} wind ({}, {}) fade in {:.1f} s, level {}",
				                   data.storm, parentPos.x, parentPos.z, created->descriptor.innerRadius,
				                   created->descriptor.outerRadius, created->descriptor.weather.rain,
				                   created->descriptor.weather.overcast, created->descriptor.weather.windX,
				                   created->descriptor.weather.windZ, created->descriptor.fadeInTime, level);
			}
		}
		if (data.storm != weather::storms::k_NoStorm)
		{
			// fn_0083F8D0: gone or marked -> 0 (and the next step registers a new one); else fn_006D5950 moves it
			auto* registeredStorm = weather::storms::Find(data.storm);
			if (registeredStorm == nullptr)
			{
				data.storm = weather::storms::k_NoStorm;
			}
			else
			{
				registeredStorm->descriptor.position = parentPos;
			}
		}
		data.first = false;
		return true;
	}

private:
	/// fn_006D56F0: the storm spell's GMagicStormAndTornadoInfo (rainAmount, file +0x58); < 0 without one
	static float RainAmountOf(const Effect& effect)
	{
		const auto spell = SpellOf(effect);
		auto& registry = Locator::entitiesRegistry::value();
		if (spell == entt::null || !registry.Valid(spell) || !Locator::infoConstants::has_value())
		{
			return -1.0f;
		}
		const auto& component = registry.Get<const ecs::components::Spell>(spell);
		if (component.spellClass != ecs::components::SpellClass::StormAndTornado)
		{
			return -1.0f; // the dynamic cast to SpellStormAndTornado fails
		}
		const auto* info = magic::GetMagicInfoAs<GMagicStormAndTornadoInfo>(Locator::infoConstants::value(), component.magicType);
		return info != nullptr ? info->rainAmount : -1.0f;
	}

	/// fn_006D4880: a cloud's radius, angle, age 0, no flash, its height variation
	void InitCloud(Effect& effect, Atom& atom, CloudAtomData& ad) const
	{
		// PSysFloatRand(0.7, 1.0) + 0.7 (0x8AB238), times the Radius provider: 1.4..1.7 x R
		ad.radius = (RandRange(effect, 0.7f, 1.0f) + 0.7f) * effect.FloatProvider(radiusProvider, 0.0f);
		ad.theta = effect.Random(k_TwoPi);
		atom.birth = effect.GetAge(); // fn_00673CE0(0): the atom's age is 0
		ad.flashing = false;
		ad.height = effect.Random(heightVaryAmount);
		ad.specular = 0xFF000000u;
	}

	/// fn_006D4970: NumAtoms clouds at once, each at a random age up to TimeToForm
	void CreateAllAtOnce(Effect& effect, Collection& collection, const glm::vec3& /*parentPos*/, const Creator* particleCreator) const
	{
		auto& data = CollectionDataOf<CloudGatherData>(collection, this);
		for (int i = 0; i < numAtoms; ++i)
		{
			++data.count;
			auto& atom = effect.NewAtom(collection, particleCreator, {});
			InitCloud(effect, atom, AtomDataOf<CloudAtomData>(atom, this));
			atom.birth = effect.GetAge() - effect.Random(timeToForm);
		}
	}

	/// 0x6D52F9..0x6D54D6: a strike from a random formed cloud
	void Strike(Effect& effect, Collection& collection, CloudGatherData& data) const
	{
		// the next one in rand(0.5, 1) x SwitchLife / max(TribalPower, 1)
		float interval = RandRange(effect, 0.5f, 1.0f) * switchLife;
		const auto spell = SpellOf(effect);
		if (spell != entt::null)
		{
			interval /= std::max(magic::GetTribalPower(spell), 1.0f);
		}
		data.nextStrike = effect.CollectionAge(collection) + interval;
		// PSysRand(count) 0x6729E0
		const auto index = std::min(static_cast<size_t>(effect.Random(static_cast<float>(g_Clouds.size()))), g_Clouds.size() - 1);
		Atom* cloud = g_Clouds[index];
		data.lightningCloud = nullptr;
		if (data.lightning == nullptr)
		{
			// the first strike: AddSubCollection(LightningGroup) on the cloud; its first sub-collection is the new one
			if (lightningGroup < 0)
			{
				return;
			}
			effect.AddSubCollections(*cloud, {lightningGroup});
			if (cloud->subCollections.empty())
			{
				return;
			}
			data.lightning = cloud->subCollections.back().get();
			data.detached = false;
		}
		// fn_00674A30(cloud): the lightning collection moves to this cloud
		std::unique_ptr<Collection> moving;
		if (data.orphan != nullptr && data.orphan.get() == data.lightning)
		{
			moving = std::move(data.orphan);
		}
		else if (Atom* owner = HolderOf(collection, data.lightning); owner == nullptr)
		{
			data.lightning = nullptr; // its cloud went (the port's guard; the original keeps the stale pointer)
			return;
		}
		else if (owner != cloud)
		{
			moving = Detach(collection, data.lightning);
		}
		if (moving != nullptr)
		{
			moving->parent = cloud;
			cloud->subCollections.push_back(std::move(moving));
		}
		// AtomCore::StopAllSounds, then the thunder with a random size class (0x8CA268 0.33 -> 3, 0x8CF128 0.66 -> 2,
		// else 1) and flags |= 0x22 (delayed by the distance, snapped to the ground)
		audio::spell_sounds::StopAllSounds(*cloud);
		auto thunder = soundLightning;
		const float r = effect.Random(1.0f);
		thunder.size = r < 0.33f ? 3 : (r < 0.66f ? 2 : 1);
		thunder.flags |= 0x22u;
		audio::spell_sounds::StartSound(effect, *cloud, thunder);
		data.lightningCloud = cloud;
		data.detached = false;
		++g_Strikes;
		data.strikeEnd = effect.CollectionAge(collection) + RandRange(effect, 0.5f, 1.0f) * lightningLife;
		auto& ad = AtomDataOf<CloudAtomData>(*cloud, this);
		ad.flashing = true;
		ad.flashStart = effect.AtomAge(*cloud);
		if (storm::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Storm: lightning from cloud {} of {} at ({:.1f}, {:.1f}, {:.1f}), next in {:.2f} s, life {:.2f} s",
			                   index, g_Clouds.size(), cloud->position.x, cloud->position.y, cloud->position.z, interval,
			                   data.strikeEnd - effect.CollectionAge(collection));
		}
	}

	std::string creator;
	float maxRadius;
	int numAtoms;
	float maxAngularSpeed;
	float timeToForm;
	float fracToMaxSize;
	int maxColor, minColor;
	float minScaleFactor, maxScaleFactor;
	int minAlpha, maxAlpha;
	int lightningGroup;
	float specLife, lightningLife, lightningDelay, switchLife;
	float heightVaryAmount;
	float cloudRatioMaxCollection, collectionRadiusInitialScale;
	float maxCloudRatio, minCloudRatio;
	bool createAllAtOnce;
	int tornadoGroup;
	float windMaxSpeed, windMinSpeed, magnitudeForWindMaxSpeed, magnitudeForWindMinSpeed;
	std::string radiusProvider, scaleProvider, cloudHeightProvider;
	SoundAction soundLightning;
};

// ================================================================================================================
// UR_Tornado (ctor 0x6D1680, props 0x6AD550, ModifyAtomCollection 0x6D18B0)
// ================================================================================================================

/// CollectionData@UR_Tornado (0x74, ctor 0x5608A0). The original keeps the one being processed in 0xD4EEC0 and its
/// collection in 0xD4EEC4.
struct TornadoData
{
	bool first {true};         ///< +0x20
	glm::vec3 base {0.0f};     ///< +0x24
	glm::vec3 top {0.0f};      ///< +0x30
	glm::vec3 baseVelocity {0.0f}; ///< +0x3C
	glm::vec3 topVelocity {0.0f};  ///< +0x48
	float tornadoScale {1.0f}; ///< +0x60 the TornadoScale provider (1 without one)
	float strength {1.0f};     ///< +0x64 (1 every step)
	uint8_t alpha {0};         ///< +0x68 the collections' alpha
	float fade {1.0f};         ///< +0x6C the close-down fade
	bool closing {false};      ///< +0x70
};

/// FlyingAtomData@UR_Tornado (0x2C, ctor 0x5609F0)
struct FlyingAtomData
{
	int state {2};       ///< +0x20: 0 a game object, 1 a pretend object, 2 a funnel sprite
	float blend {0.0f};  ///< +0x24 the height fraction it rises to (x 0.1 per second)
	float speed {0.0f};  ///< +0x28 rand(1) + 0.5 (fn_006D1C40)
};

/// DebrisCollectionData@UR_Tornado (0x28, ctor 0x560950) and FlyingCollectionData (0x28, 0x5609A0): +0x20 made, +0x24
/// to make
struct EmitterData
{
	float made {0.0f};
	float owed {0.0f};
};

/// The game objects the tornados carry (RenderParticleGameObject, 0x58 bytes, ctor 0x6C9E60): the atom's creator here,
/// one per atom, kept in the atom's data so that it goes with the atom
struct TornadoCarrier final: Creator
{
	entt::entity object {entt::null}; ///< +0x1C
	bool hasPlayer {false};           ///< +0x20 the manager's player (fn_006CA0E0)
	PlayerNames player {PlayerNames::NEUTRAL};
	bool killOnRelease {true}; ///< +0x54 (ctor 1)
	bool released {false};

	TornadoCarrier() { TouchExitGuard(); }
	TornadoCarrier(const TornadoCarrier&) = delete;
	TornadoCarrier& operator=(const TornadoCarrier&) = delete;
	TornadoCarrier(TornadoCarrier&&) = delete;
	TornadoCarrier& operator=(TornadoCarrier&&) = delete;
	~TornadoCarrier() override;
	/// fn_006C9FC0 (the dtor, when the atom goes)
	void Release(const glm::vec3& position);
	/// where the atom was last seen (each tornado step, and each frame by UpdateCarriedObjects)
	mutable glm::vec3 lastPosition {0.0f};
};

/// The objects some tornado carries (never destroyed: see g_Exiting)
std::unordered_set<entt::entity>& Carried()
{
	static auto* carried = new std::unordered_set<entt::entity>();
	return *carried;
}
/// The key of an atom's TornadoCarrier in its data (the FlyingAtomData is under the tornado rule itself)
const char k_CarrierKeyByte = 0;
const auto* const k_CarrierKey = reinterpret_cast<const Modifier*>(&k_CarrierKeyByte);

bool IsLiving(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<ecs::components::Villager, ecs::components::Animal>(object);
}

void TornadoCarrier::Release(const glm::vec3& position)
{
	if (released)
	{
		return;
	}
	released = true;
	if (g_Exiting)
	{
		return;
	}
	Carried().erase(object);
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (killOnRelease && IsLiving(object))
	{
		// fn_006C9EF0: back on the map where the atom is (its angles from the matrix, y offset 0), EndPhysics, then
		// DestroyedByEffect(player, 1.0): a villager or an animal dies there
		if (auto* transform = registry.TryGet<ecs::components::Transform>(object); transform != nullptr)
		{
			transform->position = glm::vec3(position.x, LandAt(position.x, position.z), position.z);
		}
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			ecs::life::Kill(object, "tornado"); // Villager::DestroyedByEffect 0x7502D0 -> VillagerDead
		}
		else
		{
			ecs::animal_ai::Kill(object); // Animal::DestroyedByEffect 0x41B1B0 -> SetDying (vt+0x6A4: a spell animal fades)
		}
		return;
	}
	// vt 0xC ToBeDeleted(0): anything else the tornado took is gone
	if (registry.AllOf<ecs::components::Tree>(object))
	{
		ecs::DeleteTree(object);
		return;
	}
	ecs::fire::traits::DestroyedByEffect(object); // Object::DestroyedByEffect 0x6378E0 = ToBeDeleted
}

TornadoCarrier::~TornadoCarrier()
{
	Release(lastPosition);
}

/// Terrain::GetMaterialInfo 0x735330 at a point: the second material of the cell's altitude in its country (the reading
/// of Audio/SoundMap.cpp), and its GTerrainMaterialInfo::tornadoDustColorRGB (+0x54..+0x5C) as 0xFFRRGGBB
uint32_t TornadoDustColour(const glm::vec3& point)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return 0xFFFFFFFFu;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	// MapCoords from an LHPoint: the 10 m cell (as Audio/SoundMap.cpp)
	const auto cx = static_cast<int32_t>(std::floor(point.x / 10.0f));
	const auto cz = static_cast<int32_t>(std::floor(point.z / 10.0f));
	const int32_t last = island.GetCellsPerSide() - 1;
	if (cx < 0 || cx > last || cz < 0 || cz > last)
	{
		return 0xFFFFFFFFu;
	}
	const auto& cell = island.GetCell(glm::u16vec2(cx, cz));
	if (cell.properties.country >= countries.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(cell), 255);
	const auto material = countries[cell.properties.country].materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	if (type >= info.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto& c = info[type].tornadoDustColorRGB;
	return 0xFF000000u | ((c.x & 0xFFu) << 16u) | ((c.y & 0xFFu) << 8u) | (c.z & 0xFFu);
}

/// The pots of the land by 10 m cell: openblack's map grid lists only the Fixed and Mobile entities, and the pots and
/// piles (MobileObjects in the original's cell lists) have neither, so a pick-up adds them by their cell
/// ((aproximado): port bookkeeping, built from the registry at each pick-up instead of the cell lists)
std::unordered_map<int32_t, std::vector<entt::entity>> PotsByCell()
{
	std::unordered_map<int32_t, std::vector<entt::entity>> pots;
	Locator::entitiesRegistry::value().Each<const ecs::components::Pot, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Pot& /*pot*/, const ecs::components::Transform& transform) {
		    const int32_t x = static_cast<int32_t>(transform.position.x * 0.1f);
		    const int32_t z = static_cast<int32_t>(transform.position.z * 0.1f);
		    pots[x + z * 0x10000].push_back(entity);
	    });
	for (auto& [cell, list] : pots)
	{
		std::sort(list.begin(), list.end());
	}
	return pots;
}

/// The objects of one 10 m cell, the mobile list (+4) first and then the fixed one (+0) (0x6D2327)
void CellObjects(const glm::ivec2& cell, std::vector<entt::entity>& out,
                 const std::unordered_map<int32_t, std::vector<entt::entity>>& pots)
{
	out.clear();
	if (!Locator::entitiesMap::has_value() || cell.x < 0 || cell.y < 0 || cell.x >= ecs::MapInterface::k_GridSize.x ||
	    cell.y >= ecs::MapInterface::k_GridSize.y)
	{
		return;
	}
	const auto& map = Locator::entitiesMap::value();
	const ecs::MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
	std::vector<entt::entity> mobile(map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
	std::vector<entt::entity> fixed(map.GetFixedInGridCell(id).begin(), map.GetFixedInGridCell(id).end());
	// (inferido) the cell lists are unordered sets here: sorted, for a stable order
	std::sort(mobile.begin(), mobile.end());
	std::sort(fixed.begin(), fixed.end());
	if (const auto it = pots.find(cell.x + cell.y * 0x10000); it != pots.end())
	{
		mobile.insert(mobile.end(), it->second.begin(), it->second.end());
	}
	out = std::move(mobile);
	out.insert(out.end(), fixed.begin(), fixed.end());
}

/// GUtils::Spiral 0x74D7E0 (table 0xDA59FC: +x, +z, -x, -z): `if (--count == 0) { ++direction; count = direction / 2; }`
/// (0x74D7E9..0x74D7F7), then the step table[direction & 3]; fn_006D21B0 starts it with direction 1, count 1
/// (0x6D22E9..0x6D22F7). (audit4: the port returned the step before the update with another count rule, which walks
/// the mirror image of the original's spiral: +x, +z, -x... instead of -x, -z, +x, +x...)
glm::ivec2 SpiralStep(int& direction, int& count)
{
	constexpr std::array<glm::ivec2, 4> k_Steps = {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1)};
	if (--count == 0)
	{
		++direction;
		count = direction / 2;
	}
	return k_Steps[static_cast<size_t>(direction & 3)];
}

/// PileResource::IsPileResource 0x66ED60 (vt 0x4CC): the piles (openblack: a Pot that sinks, PileSink) (inferido)
bool IsPileResource(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.AllOf<ecs::components::Pot, ecs::components::PileSink>(object);
}

class Tornado final: public Modifier
{
public:
	explicit Tornado(const Object& object)
	    : creator(object.String("PCreator"))                        // +0x28
	    , topHeight(object.Float("TopHeight", 100.0f))               // +0x2C
	    , wiggleCount(object.Int("WiggleCount", 5))                  // +0x30
	    , wiggleAmplitude(object.Float("WiggleAmplitude", 10.0f))    // +0x34
	    , fadeOutTime(object.Float("FadeOutTime", 10.0f))            // +0x38
	    , fadeInTime(object.Float("FadeInTime", 4.0f))               // +0x3C
	    , baseRadius(object.Float("BaseRadius", 3.0f))               // +0x40
	    , topRadius(object.Float("TopRadius", 50.0f))                // +0x44
	    , baseScale(object.Float("BaseScale", 1.0f))                 // +0x48
	    , topScale(object.Float("TopScale", 10.0f))                  // +0x4C
	    , baseThetaDot(object.Float("BaseThetaDot", 15.0f))          // +0x50
	    , topThetaDot(object.Float("TopThetaDot", 2.0f))             // +0x54
	    , meshThetaDot(object.Float("MeshThetaDot", 2.0f))           // +0x58
	    , thetaBias(object.Float("ThetaBias", 0.5f))                 // +0x64
	    , funnelBend(object.Float("FunnelBendParameter", 0.5f))      // +0x68
	    , topMoveFreq(object.Float("TopMoveFreq", 0.2f))             // +0x7C
	    , topMoveAmp(object.Float("TopMoveAmp", 40.0f))              // +0x80
	    , groupFlying(object.Int("GroupFlying", -1))                 // +0x8C
	    , groupDebris(object.Int("GroupDebris", -1))                 // +0x90
	    , groupMesh(object.Int("GroupMesh", -1))                     // +0x94
	    , groupOnceDone(object.Int("GroupToMoveToOnceDone", -1))     // +0x98
	    , groupOnCloseDown(object.Int("GroupToMoveToOnCloseDown", -1)) // +0x9C
	    , showVelocityField(object.Bool("ShowVelocityField", true))  // +0xA0
	    , scaleProvider(object.String("TornadoScaleFloatProvider"))  // +0xAC
	    , delayBeforeMove(object.Float("DelayBeforeMove", 5.0f))     // +0xB0
	    , damping(object.Float("Damping", 0.1f))                     // +0xB4
	    , debrisRadius(object.Float("DebrisRadius", 10.0f))          // +0xB8
	    , debrisHeight(object.Float("DebrisHeight", 10.0f))          // +0xBC
	    , debrisSpeed(object.Float("DebrisSpeed", 10.0f))            // +0xC0
	    , debrisEmitRate(object.Float("DebrisEmitRate", 2.0f))       // +0xC4
	    , debrisSpreadAngle(object.Float("DebrisSpreadAngle", 1.0367f)) // +0xC8 (0x3F84B36D)
	    , pretendRadius(object.Float("PretendRadius", 10.0f))        // +0xCC
	    , pretendSpeed(object.Float("PretendSpeed", 10.0f))          // +0xD4
	    , pretendEmitRate(object.Float("PretendEmitRate", 2.0f))     // +0xD8
	    , pretendSpreadAngle(object.Float("PretendSpreadAngle", 1.0367f)) // +0xDC
	    , pretendGravity(object.Float("PretendGravity", 10.0f))      // +0xE0
	    , pretendBlendTime(object.Float("PretendBlendTime", 4.0f))   // +0xE4
	    , resourceMin(object.Float("ResourceAmountRemoveMin", 150.0f)) // +0xE8
	    , resourceMax(object.Float("ResourceAmountRemoveMax", 650.0f)) // +0xEC
	    , debrisCreator(object.String("DebrisSpriteCreator"))        // +0xF4
	    , pretendSmall(object.String("PretendObjectCreatorSmall"))   // +0xF8
	    , pretendMedium(object.String("PretendObjectCreatorMedium")) // +0xFC
	    , pretendLarge(object.String("PretendObjectCreatorLarge"))   // +0x100
	    , soundTornado(ReadSoundAction(object, "SoundTornado"))      // +0x104
	{
		// read by no code of W120: PretendHeight (+0xD0), MaxSearchDistance (+0x70), MaxSearchDistanceWhenNoTargets
		// (+0x74), LocalSearchDistance (+0x78), PauseBeforeAffectsGameObjects (+0x6C), UseTornadoStrength (+0x84),
		// K1_Accn (+0xA4), ScaleWhipUp (+0xA8). SpriteCreator (+0xF0) with NumAtomsToCreate (+0x88) makes funnel
		// sprites on the first call (fn_006D1AD0); the only file with UR_Tornado, SF_LightningStormPush, has 0 of
		// them, so that function is not ported.
		numAtomsToCreate = object.Int("NumAtomsToCreate", 100);
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; } // ctor: flags ^= 4 (6 -> 2)

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<TornadoData>(collection, this);
		// 0x6D1924: the TornadoScale provider (1 without one) and strength 1
		data.tornadoScale = scaleProvider.empty() ? 1.0f : effect.FloatProvider(scaleProvider, 1.0f);
		data.strength = 1.0f;
		UpdateBaseAndTopPoints(effect, collection, data);
		if (data.first)
		{
			const auto* pointCreator = effect.FindCreator(creator);
			if (pointCreator == nullptr)
			{
				return false; // 0x6D196A
			}
			// the funnel's atom, with the flying, debris and mesh groups under it (fn_00674DD0), and the loop
			auto& atom = effect.NewAtom(collection, pointCreator, {groupFlying, groupDebris, groupMesh});
			audio::spell_sounds::StartSound(effect, atom, soundTornado);
			atom.position = data.base;
		}
		// 0x6D1A2A: the parent (the tornado root) and the funnel's atom sit at the base
		if (collection.parent != nullptr)
		{
			collection.parent->position = data.base;
		}
		if (!collection.atoms.empty())
		{
			auto& funnel = *collection.atoms.front();
			funnel.position = data.base;
			for (auto& sub : funnel.subCollections)
			{
				if (sub->group == groupFlying)
				{
					UpdateFlyingAtoms(effect, *sub, data, effect.CollectionAge(collection));
				}
				else if (sub->group == groupMesh)
				{
					UpdateMeshAtoms(effect, *sub, data);
				}
				else if (sub->group == groupDebris)
				{
					UpdateDebrisAtoms(effect, *sub, data);
				}
			}
		}
		data.first = false;
		return true;
	}

private:
	/// UpdateBaseAndTopPoints 0x6D1C60
	void UpdateBaseAndTopPoints(Effect& effect, Collection& collection, TornadoData& data) const
	{
		data.fade = 1.0f;
		data.closing = data.closing || effect.Closing();
		if (data.closing)
		{
			const float t = effect.GetAge() - effect.GetCloseAge();
			data.fade = t < 0.0f ? 1.0f : (t > fadeOutTime ? 0.0f : 1.0f - t / fadeOutTime);
		}
		const float in = Clamp01(effect.CollectionAge(collection) / fadeInTime);
		const float a = Clamp01(in < data.fade ? in : data.fade);
		data.alpha = static_cast<uint8_t>(static_cast<int>(a * 255.0f));
		if (data.first)
		{
			data.top = effect.GetOrigin();
			data.base = data.top;
		}
		const glm::vec3 oldBase = data.base;
		const glm::vec3 oldTop = data.top;
		// after DelayBeforeMove the top follows the parent (the tornado root, which UR_FollowParent keeps on the core)
		if (!(effect.CollectionAge(collection) < delayBeforeMove))
		{
			data.top = ParentPosition(effect, collection);
		}
		data.base = data.top;
		// the base wanders round the top: VSNoise1To1 at n, 1.3n, 2n, 2.6n with n = age x TopMoveFreq
		const float n = effect.CollectionAge(collection) * topMoveFreq;
		const float n1 = noise::VSNoise1To1(n);
		const float n13 = noise::VSNoise1To1(n * 1.3f);  // 0x8C7A18
		const float n2 = noise::VSNoise1To1(n + n);
		const float n26 = noise::VSNoise1To1(n * 2.6f);  // 0x93A594
		const float amplitude = data.tornadoScale * topMoveAmp;
		data.base.x += (n1 + 0.5f * n2) * amplitude;
		data.base.z += (n13 + 0.5f * n26) * amplitude;
		data.base.y = LandAt(data.base.x, data.base.z);
		effect.SetOrigin(data.base); // PSysManager::SetOrigin 0x672E10
		data.top.y = data.base.y + data.tornadoScale * topHeight;
		if (data.first)
		{
			data.baseVelocity = glm::vec3(0.0f);
			data.topVelocity = glm::vec3(0.0f);
		}
		else
		{
			const float invDt = 1.0f / effect.GetDt(); // [0xD4E0F0]
			data.baseVelocity = (data.base - oldBase) * invDt;
			data.topVelocity = (data.top - oldTop) * invDt;
		}
		// every step while not closing: SpellEvent 2 at the base with its velocity (strength 1, no shields, no target)
		if (!data.closing)
		{
			SpellEventInfo event;
			event.type = SpellEventInfo::Point;
			event.position = data.base;
			event.velocity = data.baseVelocity;
			event.strength = 1.0f;
			event.checkShields = false;
			event.target = entt::null;
			effect.SendSpellEvent(event);
		}
	}

	[[nodiscard]] float Radius(float h, const TornadoData& data) const
	{
		return storm::FunnelRadius(h, baseRadius, topRadius, data.tornadoScale); // fn_006D2790
	}

	/// fn_006D2860: the sprites' scale at h, (BaseScale + (TopScale - BaseScale) h^2) x tornadoScale
	[[nodiscard]] float SpriteScale(float h, const TornadoData& data) const
	{
		h = Clamp01(h);
		return (baseScale + (topScale - baseScale) * h * h) * data.tornadoScale;
	}

	/// fn_006D2710: the angular speed at h, lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) x speed x strength
	/// (it also writes ThetaBias clamped to 0..1 back into the rule)
	[[nodiscard]] float ThetaDot(float h, const FlyingAtomData& ad, const TornadoData& data) const
	{
		const float bias = Clamp01(thetaBias);
		return (baseThetaDot + (topThetaDot - baseThetaDot) * storm::Bias(bias, h)) * ad.speed * data.strength;
	}

	/// fn_006D2910: the funnel's centre line at h: y from base to top, x and z by gain(FunnelBend, h), plus the wiggle
	/// h x WiggleCount x pi, of tornadoScale x WiggleAmplitude
	[[nodiscard]] glm::vec3 Centre(float h, const TornadoData& data) const
	{
		h = h > 0.0f ? (h < 1.0f ? h : 1.0f) : 0.0f;
		glm::vec3 out;
		out.y = data.base.y + (data.top.y - data.base.y) * h;
		const float g = storm::Gain(funnelBend, h);
		out.x = data.base.x + (data.top.x - data.base.x) * g;
		out.z = data.base.z + (data.top.z - data.base.z) * g;
		const float angle = h * static_cast<float>(wiggleCount) * 3.14159f; // 0x8C36A0
		const float amplitude = data.tornadoScale * wiggleAmplitude;
		out.x += std::cos(angle) * amplitude;
		out.z += std::sin(angle) * amplitude;
		return out;
	}

	/// fn_006D2A40: the funnel meshes at the base, the collection alpha, scaled, each spinning (1 + i x 0.13) times
	/// faster
	void UpdateMeshAtoms(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		collection.alpha = data.alpha;
		const float spin = data.strength * meshThetaDot; // fn_006D2700
		int i = 0;
		for (auto& atom : collection.atoms)
		{
			// fn_0067A4A0: every axis turned about Y, x' = c x - s z, z' = s x + c z
			const float angle = (static_cast<float>(i) * 0.13f + 1.0f) * effect.GetDt() * spin; // 0x900C60
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			for (int k = 0; k < 3; ++k)
			{
				auto& axis = atom->rotation[k];
				const float x = axis.x;
				axis.x = c * x - s * axis.z;
				axis.z = s * x + c * axis.z;
			}
			atom->position = data.base;
			atom->ruleScale = data.tornadoScale;
			++i;
		}
	}

	/// The velocity a debris or pretend object is thrown out with: baseVelocity + tornadoScale x speed x
	/// (cos th sin ph, cos ph, sin th sin ph), ph = rand(spread), th = rand(2 pi), speed = rand(0.33, 0.66) x the rule's
	/// (0x3EA8F5C3 / 0x3F28F5C3)
	glm::vec3 ThrowVelocity(Effect& effect, const TornadoData& data, float spread, float speed) const
	{
		const float phi = effect.Random(spread);
		const float theta = effect.Random(k_TwoPi);
		const float v = RandRange(effect, 0.33f, 0.66f) * speed;
		const float s = data.tornadoScale;
		return data.baseVelocity + glm::vec3(s * std::cos(theta) * v * std::sin(phi), s * v * std::cos(phi),
		                                      s * std::sin(theta) * v * std::sin(phi));
	}

	/// UpdateDebrisAtoms 0x6D2AF0: dust in the terrain's tornado colour, DebrisEmitRate per second, at most 50 alive
	void UpdateDebrisAtoms(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		const auto* debris = effect.FindCreator(debrisCreator);
		if (debris == nullptr)
		{
			return;
		}
		collection.alpha = data.alpha;
		auto& emitter = CollectionDataOf<EmitterData>(collection, this);
		emitter.owed += effect.GetDt() * debrisEmitRate;
		if (data.closing)
		{
			return;
		}
		const uint32_t dust = TornadoDustColour(data.base);
		while (emitter.owed > emitter.made && collection.atoms.size() < 50) // 0x32
		{
			emitter.made += 1.0f;
			auto& atom = effect.NewAtom(collection, debris, {});
			// 0x6D2C21..0x6D2CC5: each channel of the creator's colour x the dust's >> 8, the alpha kept
			const auto channel = [](uint8_t c, uint32_t d) { return static_cast<uint8_t>((c * (d & 0xFFu)) >> 8u); };
			atom.colour = {channel(atom.colour[0], dust >> 16u), channel(atom.colour[1], dust >> 8u),
			               channel(atom.colour[2], dust), atom.colour[3]};
			const float r1 = RandRange(effect, -debrisRadius, debrisRadius);
			const float ry = effect.Random(debrisHeight);
			const float r3 = RandRange(effect, -debrisRadius, debrisRadius);
			const float s = data.tornadoScale;
			atom.position = data.base + glm::vec3(s * r3, s * ry, s * r1);
			atom.baseScale *= data.tornadoScale;
			atom.velocity = ThrowVelocity(effect, data, debrisSpreadAngle, debrisSpeed);
		}
	}

	/// fn_006D2E70: the pretend objects (chickens and bushes), PretendEmitRate x tornadoScale per second while the
	/// base is on dry land and the tornado fully faded in (alpha > 250)
	void EmitPretendObjects(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		const auto* small = effect.FindCreator(pretendSmall);
		const auto* medium = effect.FindCreator(pretendMedium);
		const auto* large = effect.FindCreator(pretendLarge);
		if (small == nullptr || medium == nullptr || large == nullptr)
		{
			return;
		}
		auto& emitter = CollectionDataOf<EmitterData>(collection, this);
		if (data.closing || data.alpha <= 0xFA)
		{
			return;
		}
		const float rate = ecs::pot_resource::IsDryLand(data.base) ? data.tornadoScale * pretendEmitRate : 0.0f;
		emitter.owed += rate * effect.GetDt();
		while (emitter.owed > emitter.made)
		{
			emitter.made += 1.0f;
			const float r = effect.Random(1.0f);
			const auto* meshCreator = r < 0.33f ? small : (r < 0.66f ? medium : large);
			auto& atom = effect.NewAtom(collection, meshCreator, {});
			const float r1 = RandRange(effect, -pretendRadius, pretendRadius);
			const float r2 = RandRange(effect, -pretendRadius, pretendRadius);
			const float s = data.tornadoScale;
			atom.position = data.base + glm::vec3(s * r2, 0.0f, s * r1);
			if (data.tornadoScale < 1.0f)
			{
				atom.baseScale *= data.tornadoScale;
			}
			atom.velocity = ThrowVelocity(effect, data, pretendSpreadAngle, pretendSpeed);
			auto& ad = AtomDataOf<FlyingAtomData>(atom, this);
			ad.speed = effect.Random(1.0f) + 0.5f;
			ad.blend = 1.0f;
			ad.state = 1;
		}
	}

	/// fn_006D2140: the tornado can take it whole: it can be a physics object, has a 3D object, and fits the funnel
	/// (2 r(0) > its 2D radius and r(1) > it)
	bool CanSuckUp(entt::entity object, const TornadoData& data) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		const float radius = ecs::effects::Object2DRadius(object);
		// vt 0x7B0: Pot::CanBecomeAPhysicsObject 0x66E8F0 (every pot and pile class) is its GPotInfo's flag (+0x12C);
		// the other classes as the physics have them
		if (const auto* pot = registry.TryGet<const ecs::components::Pot>(object); pot != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			const auto index = static_cast<size_t>(pot->type);
			if (index >= pots.size() || pots[index].canBecomeAPhysicsObject == 0)
			{
				return false;
			}
		}
		else if (!ecs::physics::PhysicsObjects::CanBecomeAPhysicsObject(object))
		{
			return false;
		}
		// Object +0x25 bit 0x10 (not identified) is not checked (inferido: no port state behind it)
		if (!registry.AllOf<ecs::components::Mesh>(object))
		{
			return false;
		}
		return Radius(0.0f, data) + Radius(0.0f, data) > radius && Radius(1.0f, data) > radius;
	}

	/// fn_006D21B0: one object per call, every third game turn once the tornado has faded in
	void PickUp(Effect& effect, Collection& flying, const TornadoData& data, float tornadoAge) const
	{
		// the tornado collection's age (0xD4EEC4), g_game +0x205A40 % 3
		if (tornadoAge < fadeInTime || data.closing || magic::CurrentTurn() % 3 != 0)
		{
			return;
		}
		float reach = (Radius(0.0f, data) + Radius(1.0f, data)) * 0.5f;
		reach += reach;
		const auto spell = SpellOf(effect);
		if (spell != entt::null)
		{
			reach *= std::clamp(magic::GetTribalPower(spell), 1.0f, 5.0f);
		}
		auto& registry = Locator::entitiesRegistry::value();
		const glm::ivec2 start(static_cast<int>(data.base.x * 0.1f), static_cast<int>(data.base.z * 0.1f));
		const int side = static_cast<int>(std::ceil(reach / 10.0f)) + 2; // 0x93A564
		const int cells = side * side;
		glm::ivec2 cell = start;
		int direction = 1; // 0x6D22E9..0x6D22F7: [ebp-0x34] direction = 1, [ebp-0x28] count = 1
		int count = 1;
		std::vector<entt::entity> objects;
		entt::entity taken = entt::null;
		const auto pots = PotsByCell();
		for (int i = 0; i < cells && taken == entt::null; ++i)
		{
			CellObjects(cell, objects, pots);
			for (const auto object : objects)
			{
				if (taken != entt::null)
				{
					break;
				}
				if (!registry.Valid(object) || Carried().contains(object) ||
				    !registry.AllOf<ecs::components::Transform, ecs::components::Mesh>(object))
				{
					continue;
				}
				const auto& transform = registry.Get<const ecs::components::Transform>(object);
				// fn_00604F40: the object's own cell is this one (a fixed object in several cells is seen once)
				if (static_cast<int>(transform.position.x * 0.1f) != cell.x || static_cast<int>(transform.position.z * 0.1f) != cell.y)
				{
					continue;
				}
				// GUtils::GetDistanceInMetres 0x74CD70 (inferido: in x, z; both on the ground)
				const float distance = glm::distance(glm::vec2(data.base.x, data.base.z), glm::vec2(transform.position.x, transform.position.z));
				if (!(distance < ecs::effects::Object2DRadius(object) + reach))
				{
					continue;
				}
				if (CanSuckUp(object, data))
				{
					// SpellEvent 7 (CanBeDestroyedBySpell) at the object, strength 1, the object as the target
					SpellEventInfo event;
					event.type = SpellEventInfo::CanDestroy;
					event.position = transform.position;
					event.velocity = glm::vec3(0.0f);
					event.strength = 1.0f;
					event.checkShields = false;
					event.target = object;
					if (effect.SendSpellEvent(event) == 1)
					{
						taken = object;
					}
				}
				else if (IsPileResource(object))
				{
					taken = SplitPile(effect, object, data);
				}
				// a creature: fn_00477060 (no creature in openblack)
			}
			cell += SpiralStep(direction, count);
		}
		if (taken != entt::null)
		{
			Carry(effect, flying, taken);
		}
	}

	/// 0x6D246E..0x6D2569: a pile gives lerp(ResourceAmountRemoveMin, Max, clamp(tornadoScale, 0, 1)) (fistp, rounded)
	/// as a new pile of its kind (fn_0066F0D0 -> Pot::Create), scaled by rand(0.7, 1.2) x clamp(tornadoScale, 0.2, 1)
	entt::entity SplitPile(Effect& effect, entt::entity pile, const TornadoData& data) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& pot = registry.Get<ecs::components::Pot>(pile);
		const float k = Clamp01(data.tornadoScale);
		auto amount = static_cast<uint32_t>(std::lrint(resourceMin + (resourceMax - resourceMin) * k));
		// fn_0066F0D0: at most GetResource (vt 0x98: a store pile answers with its store's total, as the hand reads it)
		const auto store = ecs::StoragePitStore::OwnerOf(pile);
		const auto& pots = Locator::infoConstants::value().pot;
		const auto resource = pots[static_cast<size_t>(pot.type)].resourceType;
		const uint32_t available = store != entt::null ? ecs::StoragePitStore::GetResource(store, resource) : pot.amount;
		amount = std::min<uint32_t>(amount, available);
		if (amount == 0)
		{
			return entt::null;
		}
		// RemoveResource (vt 0xA0): from the store, or from the pile (aproximado: the amount and the sink offset, as
		// HandHolding does; the pile's own empty handling is not ported)
		if (store != entt::null)
		{
			ecs::StoragePitStore::RemoveResource(store, resource, amount);
		}
		else
		{
			pot.amount = static_cast<uint16_t>(pot.amount - amount);
			ecs::archetypes::PotArchetype::SetSize(pile, true);
		}
		// Pot::Create with the GPotInfo of vt 0x870 GetHandPotInfoType: PileFood 0x66EC30 = 12 HAND_FOOD, PileWood
		// 0x66EC40 = 11 HAND_WOOD (the hand's own pile kind), at the pile's position
		const auto position = registry.Get<const ecs::components::Transform>(pile).position;
		const auto handType = resource == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
		const auto piece = ecs::archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(amount));
		if (piece == entt::null)
		{
			return entt::null;
		}
		const float s = data.tornadoScale < 0.2f ? 0.2f : (data.tornadoScale < 1.0f ? data.tornadoScale : 1.0f);
		auto& transform = registry.Get<ecs::components::Transform>(piece);
		transform.scale *= RandRange(effect, 0.7f, 1.2f) * s; // vt 0x124 SetScale(vt 0x120 GetScale x ...)
		return piece;
	}

	/// 0x6D257A..0x6D269D: a flying atom that carries the object (RenderParticleGameObject, fn_006CA0E0), from the
	/// object's world matrix (fn_00674150), rising to rand(0.7) of the funnel's height
	void Carry(Effect& effect, Collection& flying, entt::entity object) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto carried = std::make_shared<TornadoCarrier>();
		carried->kind = Creator::Kind::Other;
		carried->className = "RenderParticleGameObject";
		carried->object = object;
		int player = 0;
		if (effect.GetSink() != nullptr && effect.GetSink()->Player(player))
		{
			carried->hasPlayer = true;
			carried->player = static_cast<PlayerNames>(player);
		}
		// Object::InitialisePhysics (fn_006CA060) puts it in the physics' hands (+0x0A |= 0x10). (inferido) here it
		// leaves any physics flight and, when alive, the ground AI (the hand's IN_HAND, the animals' PlaceInHand)
		ecs::physics::PhysicsObjects::RemoveObject(object);
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			ecs::SetVillagerState(object, VillagerStates::InHand);
		}
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			ecs::animal_ai::PlaceInHand(object);
		}
		Carried().insert(object);
		auto& atom = effect.NewAtom(flying, carried.get(), {});
		const auto& transform = registry.Get<const ecs::components::Transform>(object);
		// fn_00674150: position, ruleScale = |row 1| (the matrix's Y axis), rotation = the rows / that
		atom.position = transform.position;
		atom.ruleScale = glm::length(transform.rotation[1] * transform.scale.y);
		atom.baseScale = 1.0f;
		atom.rotation = transform.rotation;
		carried->lastPosition = transform.position;
		atom.modifierData.insert_or_assign(k_CarrierKey, std::static_pointer_cast<void>(carried));
		auto& ad = AtomDataOf<FlyingAtomData>(atom, this);
		ad.state = 0;
		ad.blend = effect.Random(0.7f);    // 0x3F333333
		ad.speed = effect.Random(1.0f) + 0.5f; // fn_006D1C40
		if (storm::TraceEnabled())
		{
			const char* kind = registry.AllOf<ecs::components::Villager>(object) ? "villager"
			                   : registry.AllOf<ecs::components::Animal>(object) ? "animal"
			                   : registry.AllOf<ecs::components::Tree>(object)   ? "tree"
			                   : registry.AllOf<ecs::components::Pot>(object)    ? "pile/pot"
			                                                                     : "object";
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm: tornado takes {} {} at ({:.1f}, {:.1f}), rises to {:.2f}", kind,
			                   static_cast<uint32_t>(object), transform.position.x, transform.position.z, ad.blend);
		}
	}

	/// UpdateFlyingAtoms 0x6D31F0
	void UpdateFlyingAtoms(Effect& effect, Collection& collection, const TornadoData& data, float tornadoAge) const
	{
		collection.alpha = data.alpha;
		EmitPretendObjects(effect, collection, data);
		PickUp(effect, collection, data, tornadoAge);
		const float dt = effect.GetDt();
		const float invDt = 1.0f / dt;
		for (size_t i = 0; i < collection.atoms.size();)
		{
			Atom& atom = *collection.atoms[i];
			auto& ad = AtomDataOf<FlyingAtomData>(atom, this); // a new one: state 2 (ctor 0x5609F0)
			const glm::vec3 p = atom.position;
			const float h = Clamp01((p.y - data.base.y) / (data.top.y - data.base.y));
			if (ad.state == 0 || ad.state == 1)
			{
				// closing: to GroupToMoveToOnCloseDown (or gone); risen past 0.9: to GroupToMoveToOnceDone (or gone)
				if (data.closing)
				{
					MoveOrDelete(effect, collection, i, groupOnCloseDown);
					continue;
				}
				ad.blend = Clamp01(ad.blend + dt * 0.1f); // 0x8AB22C
				if (h > 0.7f && h > 0.9f)                // 0x8AB238, 0x8C5844
				{
					MoveOrDelete(effect, collection, i, groupOnceDone);
					continue;
				}
			}
			const glm::vec3 centre = Centre(h, data);
			const glm::vec3 d = p - centre;
			const float rho = std::sqrt(d.x * d.x + d.z * d.z);
			const float r = Radius(h, data);
			const float w = ThetaDot(h, ad, data);
			const float rho2 = rho + storm::RadialRate(r, rho, dt) * dt;
			// fn_006D28E0: up or down towards lerp(base.y, top.y, blend), k = +0x60 (0.1) for game objects, +0x5C (0.3)
			const float k = ad.state == 0 ? 0.1f : 0.3f;
			const float targetY = data.base.y + (data.top.y - data.base.y) * ad.blend;
			const float vy = (targetY - p.y) * k;
			const float phi = std::atan2(d.z, d.x);
			// outside the funnel wall the spin slows by r / (rho + 0.1)
			const float spin = rho2 > r ? r / (rho2 + 0.1f) * w : w;
			const float phi2 = phi + spin * dt;
			const glm::vec3 next(centre.x + rho2 * std::cos(phi2), d.y + vy * dt + centre.y, centre.z + rho2 * std::sin(phi2));
			// fn_006D27E0: plus the funnel's own velocity at that height
			const glm::vec3 v = (next - p) * invDt + data.baseVelocity + (data.topVelocity - data.baseVelocity) * h;
			if (ad.state == 1)
			{
				// thrown out with gravity, then drawn into the vortex over PretendBlendTime
				const glm::vec3 gravity(0.0f, -pretendGravity, 0.0f);
				const glm::vec3 acceleration = (v - atom.velocity) * invDt;
				const float t = Clamp01(effect.AtomAge(atom) / pretendBlendTime);
				atom.velocity += (gravity + (acceleration - gravity) * t) * dt;
			}
			else if (showVelocityField)
			{
				atom.velocity = v;
			}
			else
			{
				atom.velocity += (v - atom.velocity) * damping * dt;
			}
			atom.position += atom.velocity * dt;
			if (auto* carried = dynamic_cast<TornadoCarrier*>(const_cast<Creator*>(atom.creator)); carried != nullptr)
			{
				carried->lastPosition = atom.position;
			}
			if (ad.state == 2)
			{
				if (data.fade < 0.0001f) // 0x8BF518
				{
					collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
					continue;
				}
				atom.ruleScale = SpriteScale(h, data) * data.fade;
			}
			++i;
		}
	}

	void MoveOrDelete(Effect& effect, Collection& collection, size_t& index, int group) const
	{
		if (group == -1)
		{
			collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(index)); // DeleteFromAtomList
			return;
		}
		effect.MoveToBaseGroup(collection, *collection.atoms[index], group);
	}

	std::string creator;
	float topHeight;
	int wiggleCount;
	float wiggleAmplitude;
	float fadeOutTime, fadeInTime;
	float baseRadius, topRadius, baseScale, topScale;
	float baseThetaDot, topThetaDot, meshThetaDot;
	float thetaBias, funnelBend;
	float topMoveFreq, topMoveAmp;
	int groupFlying, groupDebris, groupMesh, groupOnceDone, groupOnCloseDown;
	bool showVelocityField;
	std::string scaleProvider;
	float delayBeforeMove, damping;
	float debrisRadius, debrisHeight, debrisSpeed, debrisEmitRate, debrisSpreadAngle;
	float pretendRadius, pretendSpeed, pretendEmitRate, pretendSpreadAngle, pretendGravity, pretendBlendTime;
	float resourceMin, resourceMax;
	std::string debrisCreator, pretendSmall, pretendMedium, pretendLarge;
	SoundAction soundTornado;
	int numAtomsToCreate {100};
};

// ================================================================================================================
// UR_StormCast (ctor 0x6BCFB0, props 0x6AD3E0, ModifyAtomCollection 0x6D59B0)
// ================================================================================================================

/// CollectionData@UR_StormCast (0x40, fn_006D5F40)
struct StormCastData
{
	bool first {true};         ///< +0x20
	glm::vec3 position {0.0f}; ///< +0x24
	glm::vec3 heading {0.0f};  ///< +0x30
	float radius {0.0f};       ///< +0x3C
};

/// AtomData@UR_StormCast (0x2C, fn_006D5F70)
struct StormCastAtomData
{
	float theta {0.0f};        ///< +0x20
	float thetaFactor {0.0f};  ///< +0x24
	float radiusFactor {0.0f}; ///< +0x28
};

class StormCast final: public Modifier
{
	static constexpr float k_HeadingLength = 20.0f; ///< +0x64 (ctor 0x6BD033)

public:
	explicit StormCast(const Object& object)
	    : creator(object.String("PCreator"))                    // +0x28
	    , nextGroups(object.Array("NextGroups"))                // +0x20
	    , numAtoms(object.Int("NumAtoms", 30))                  // +0x2C (ctor 0x1E)
	    , maxRadius(object.Float("MaxRadius", 1.0f))            // +0x30
	    , minRadius(object.Float("MinRadius", 0.2f))            // +0x34
	    , thetaDotMinRadius(object.Float("ThetaDotMinRadius", 1.0f)) // +0x38
	    , thetaDotMaxRadius(object.Float("ThetaDotMaxRadius", 1.0f)) // +0x3C
	    , radiusDot(object.Float("RadiusDot", 2.0f))            // +0x40
	    , initHeight(object.Float("InitHeight", 5.0f))          // +0x44
	    , thetaDotSpread(object.Float("ThetaDotSpread", 0.5f))  // +0x48
	    , initScaleSpread(object.Float("InitScaleSpread", 0.5f)) // +0x4C
	    , initRadiusSpread(object.Float("InitRadiusSpread", 0.7f)) // +0x50
	    , dispersalAge(object.Float("DispersalAge", 6.0f))      // +0x54
	    , accnStartTime(object.Float("AccnStartTime", 1.0f))    // +0x58
	    , accnEndTime(object.Float("AccnEndTime", 4.0f))        // +0x5C
	    , fadeOutTime(object.Float("FadeOutTime", 2.0f))        // +0x60
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; } // ctor: flags |= 6, ^= 4

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<StormCastData>(collection, this);
		Atom* parent = collection.parent;
		if (parent == nullptr)
		{
			return true; // 0x6D5A1F
		}
		const float dt = effect.GetDt();
		if (data.first)
		{
			data.first = false;
			data.position += effect.GetOrigin();
			data.radius = maxRadius;
			// 0x6D5A68: the heading scaled to the length +0x64 (20, 0x41A00000: set by the ctor, no property)
			data.heading = CurrentHeading(effect);
			if (data.heading != glm::vec3(0.0f))
			{
				data.heading = glm::normalize(data.heading) * k_HeadingLength;
			}
			const auto* particleCreator = effect.FindCreator(creator);
			std::vector<int> groups(nextGroups.begin(), nextGroups.end());
			for (int i = 0; i < numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(collection, particleCreator, groups);
				auto& ad = AtomDataOf<StormCastAtomData>(atom, this);
				atom.baseScale *= RandRange(effect, 1.0f - initScaleSpread, 1.0f + initScaleSpread);
				ad.theta = effect.Random(k_TwoPi);
				ad.thetaFactor = RandRange(effect, -thetaDotSpread, thetaDotSpread) + 1.0f;
				ad.radiusFactor = RandRange(effect, 1.0f - initRadiusSpread, 1.0f);
			}
			parent->ruleScale = effect.GetMagnitude(); // +0x78 = manager +0xA0
		}
		const float s = parent->ruleScale;
		// the parent moves along the heading, accelerating from AccnStartTime to AccnEndTime (0 -> 20 m/s)
		const float age = effect.CollectionAge(collection);
		const float a = Clamp01((age - accnStartTime) / (accnEndTime - accnStartTime));
		parent->position += data.heading * dt * a;
		// the ring shrinks to MinRadius, then after DispersalAge grows and fades out over FadeOutTime
		if (age < dispersalAge)
		{
			data.radius = std::max(data.radius - dt * radiusDot, minRadius);
		}
		else
		{
			data.radius += dt * radiusDot;
			const float f = age - dispersalAge;
			if (!(f <= fadeOutTime))
			{
				collection.atoms.clear();
				return false;
			}
			collection.alpha = static_cast<float>(static_cast<uint8_t>(static_cast<int>(255.0f - f / fadeOutTime * 255.0f)));
		}
		const float rn = Clamp01((data.radius - minRadius) / (maxRadius - minRadius));
		const float spin = thetaDotMinRadius + (thetaDotMaxRadius - thetaDotMinRadius) * rn;
		for (auto& atom : collection.atoms)
		{
			auto& ad = AtomDataOf<StormCastAtomData>(*atom, this);
			ad.theta += dt * ad.thetaFactor * spin;
			const float r = ad.radiusFactor * data.radius;
			glm::vec3 local(std::cos(ad.theta) * r, 0.0f, std::sin(ad.theta) * r);
			// LocalToGlobal, the land's height + InitHeight x the parent's scale, GlobalToLocal
			glm::vec3 global = effect.LocalToGlobal(collection, local);
			global.y = LandAt(global.x, global.z) + s * initHeight;
			atom->position = effect.GlobalToLocal(collection, global);
		}
		return true;
	}

private:
	std::string creator;
	std::vector<int> nextGroups;
	int numAtoms;
	float maxRadius, minRadius, thetaDotMinRadius, thetaDotMaxRadius, radiusDot, initHeight;
	float thetaDotSpread, initScaleSpread, initRadiusSpread, dispersalAge, accnStartTime, accnEndTime, fadeOutTime;
};
} // namespace

// ================================================================================================================
// the formulas
// ================================================================================================================

weather::storms::StormDescriptor storm::GatherStormDescriptor(const GatherStormInput& input)
{
	weather::storms::StormDescriptor d; // fn_0083F490 -> fn_0083F3F0 (the position stays 0: fn_006D5950 sets it next)
	// inner = max(R, 60), outer = max(max(2.5 R, inner + 20), 80): 0x6D5751 / 0x6D576E / 0x6D5782 are
	// `fcomp; test ah, 0x41; je` that keep the value only when it is above the constant
	float inner = input.radius;
	float outer = input.radius * 2.5f; // 0x8C581C
	if (!(inner > 60.0f))              // 0x8C36A8
	{
		inner = 60.0f;
	}
	if (inner + 20.0f > outer) // 0x8C7658
	{
		outer = inner + 20.0f;
	}
	if (!(outer > 80.0f)) // 0x8D060C
	{
		outer = 80.0f;
	}
	// the wind: speed = power x lerp(WindMinSpeed, WindMaxSpeed, clamp((mag - MagMin) / (MagMax - MagMin), 0, 1)),
	// along the heading, each component clamped to -128..128 and rounded (fistp) into a byte (128 wraps to -128)
	const float f = Clamp01((input.magnitude - input.magnitudeForWindMinSpeed) /
	                        (input.magnitudeForWindMaxSpeed - input.magnitudeForWindMinSpeed));
	const float a = input.power * input.windMinSpeed;
	const float speed = a + f * (input.power * input.windMaxSpeed - a);
	const auto windByte = [](float w) {
		w = !(w > -128.0f) ? -128.0f : (w < 128.0f ? w : 128.0f); // 0x93A608 / 0x8C6CA8
		return static_cast<int8_t>(static_cast<uint8_t>(static_cast<int32_t>(std::nearbyint(w)) & 0xFF));
	};
	d.weather.windX = windByte(input.heading.x * speed);
	d.weather.windZ = windByte(input.heading.z * speed);
	d.innerRadius = inner;
	d.outerRadius = outer;
	d.fadeInTime = input.timeToForm * 0.5f; // +0x14
	d.lifeTime = 1e9f;                      // +0x18 = 0x4E6E6B28
	d.strength = 1.0f;                      // +0x1C
	d.numClouds = 0;                        // +0x20: GWeather::DrawClouds draws none for the miracle
	d.elevation = input.cloudHeight;        // +0x28
	d.weather.temperature = 20;             // +0x48
	// +0x49: rain on -> min(ftol(power x rainAmount), 100) as an unsigned byte (100 without a storm spell); off -> 0
	int rain = 0;
	if (input.rainOn)
	{
		rain = 100;
		if (!(input.rainAmount < 0.0f))
		{
			const auto value = static_cast<uint32_t>(static_cast<int32_t>(input.power * input.rainAmount)) & 0xFFu;
			rain = value <= 100u ? static_cast<int>(value) : 100;
		}
	}
	d.weather.rain = static_cast<int8_t>(rain);
	d.weather.snowCover = 0; // +0x4E
	d.weather.snow = 0;      // +0x4A
	d.weather.overcast = 80; // +0x4B = 0x50
	return d;
}

float storm::FunnelRadius(float h, float base, float top, float tornadoScale)
{
	h = h > 0.0f ? (h < 1.0f ? h : 1.0f) : 0.0f;
	return (base + (top - base) * (h * h)) * tornadoScale;
}

float storm::Bias(float b, float x)
{
	// fyl2x (ln b), x 0xD4EEA0 (1 / ln 0.5), __CIpow(x, that)
	return std::pow(x, std::log(b) * (1.0f / std::log(0.5f)));
}

float storm::Gain(float g, float x)
{
	if (x < 0.5f)
	{
		return Bias(1.0f - g, x + x) * 0.5f;
	}
	return 1.0f - Bias(1.0f - g, 2.0f - (x + x)) * 0.5f;
}

float storm::RadialRate(float r, float rho, float dt)
{
	const float k1 = (rho - r) * -0.5f; // 0x8CEFCC
	const float k2 = (k1 * dt + rho - r) * -0.5f;
	return (k1 + k2) * 0.5f;
}

storm::CloudLook storm::CloudLookAt(float f, const CloudLookParams& p)
{
	CloudLook look {};
	look.grow = f < p.fracToMaxSize ? f * (1.0f / p.fracToMaxSize) : 1.0f - (f - p.fracToMaxSize) * (1.0f / (1.0f - p.fracToMaxSize));
	look.ratio = (p.minCloudRatio - p.maxCloudRatio) * f + p.maxCloudRatio;
	look.colour = static_cast<int>(static_cast<float>(p.minColor - p.maxColor) * f + static_cast<float>(p.maxColor)) & 0xFF;
	look.scale = ((p.maxScaleFactor - p.minScaleFactor) * look.grow + p.minScaleFactor) * p.scaleProvider;
	look.alpha = static_cast<int>(static_cast<float>(p.maxAlpha - p.minAlpha) * look.grow + static_cast<float>(p.minAlpha)) & 0xFF;
	return look;
}

float storm::CollectionScale(float collectionAge, float c)
{
	const float t = Clamp01(collectionAge * 0.1f); // 0x8AC404
	return (1.0f - c) * ((3.0f - (t + t)) * t * t) + c;
}

void storm::UpdateCarriedObjects()
{
	if (Carried().empty() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* carried = dynamic_cast<const TornadoCarrier*>(atom.creator);
			if (carried == nullptr || !registry.Valid(carried->object))
			{
				continue;
			}
			carried->lastPosition = atom.position;
			// RenderParticleGameObject::DrawAt 0x67B170: the object's matrix is the atom's (rotation and scale)
			if (auto* transform = registry.TryGet<ecs::components::Transform>(carried->object); transform != nullptr)
			{
				transform->position = atom.position;
				transform->rotation = atom.rotation;
				transform->scale = glm::vec3(atom.scale);
			}
		}
	}
	registry.SetDirty();
}

size_t storm::StrikeCount()
{
	return g_Strikes;
}

size_t storm::CarriedObjectCount()
{
	return Carried().size();
}

bool storm::TraceEnabled()
{
	if (!g_TraceRead)
	{
		g_TraceRead = true;
		const char* value = std::getenv("OPENBLACK_STORM_TRACE");
		g_Trace = value != nullptr && value[0] != '\0' && value[0] != '0';
	}
	return g_Trace;
}

void openblack::psys::RegisterStormRules()
{
	RegisterModifier("UR_CloudMoverNew", MakeModifierOf<CloudMoverNew>);
	RegisterModifier("UR_CloudGather", MakeModifierOf<CloudGather>);
	RegisterModifier("UR_Tornado", MakeModifierOf<Tornado>);
	RegisterModifier("UR_StormCast", MakeModifierOf<StormCast>);
}
