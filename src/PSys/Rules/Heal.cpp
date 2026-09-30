/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The heal miracle's rules (SF_HealChakra, SF_HealChakraPU, SF_HealChakraInHand, SF_HealChakraOnHolder):
// UR_HealSpellChakra, one chakra atom per target that heals it (event 5) and makes it glow, CreateRuleFusedSphericalExplode,
// the burst of sprites under each chakra, and UR_HealInHand, the in-hand wiggle. Wiki: docs/bw1-notes/magic.md, "Curar".

#include "Heal.h"

#include <cmath>

#include <algorithm>
#include <memory>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/SpellSounds.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "PSys/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// 0xD4ED38 (count 0xD4ED3C): the objects some chakra is on, so that two heal spells don't chakra the same villager
std::vector<entt::entity> g_Chakraed;

bool IsChakraed(entt::entity object)
{
	return std::find(g_Chakraed.begin(), g_Chakraed.end(), object) != g_Chakraed.end();
}

bool Available(entt::entity object)
{
	// GameThing::IsAvailable (vt 0x2C) == 1: here, the object still exists
	return object != entt::null && Locator::entitiesRegistry::has_value() &&
	       Locator::entitiesRegistry::value().Valid(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// Living::SetSpecularColor 0x417480 (vt 0x5A0; Object 0x4025C0 does nothing): Living +0xD0
void SetSpecularColour(entt::entity object, glm::u8vec3 colour)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(object))
	{
		return;
	}
	if (colour == glm::u8vec3(0))
	{
		if (registry.AllOf<ecs::components::SpecularColour>(object))
		{
			registry.Remove<ecs::components::SpecularColour>(object);
		}
		return;
	}
	registry.AssignOrReplace<ecs::components::SpecularColour>(object, colour);
}

/// UR_HealSpellChakra::AtomData (0x38 bytes, vtable 0x93768C; ctor fn_006A0920, dtor fn_006A09A0)
struct ChakraData
{
	ChakraData() = default;
	ChakraData(const ChakraData&) = delete;
	ChakraData(ChakraData&&) = delete;
	ChakraData& operator=(const ChakraData&) = delete;
	ChakraData& operator=(ChakraData&&) = delete;
	/// fn_006A09A0: out of the global list; a target still there loses its glow (SetSpecularColor(0))
	~ChakraData()
	{
		if (listed != entt::null)
		{
			if (const auto it = std::find(g_Chakraed.begin(), g_Chakraed.end(), listed); it != g_Chakraed.end())
			{
				g_Chakraed.erase(it);
			}
		}
		if (Available(target))
		{
			SetSpecularColour(target, glm::u8vec3(0));
		}
	}
	/// fn_006A0A60: the target (+0x28, and +0x20 the one put in the global list, at its front)
	void SetTarget(entt::entity object)
	{
		target = object;
		listed = object;
		if (object != entt::null)
		{
			g_Chakraed.insert(g_Chakraed.begin(), object);
		}
	}

	entt::entity listed {entt::null}; ///< +0x20
	entt::entity target {entt::null}; ///< +0x28 (+0x24 the game turn it was last seen: unused)
	bool done {false};                ///< +0x2C the chakra ends: the atom goes
	bool fresh {true};                ///< +0x2D its first step (the burst under it is not made yet)
	bool soundStarted {false};        ///< +0x2E
	float radius {1.0f};              ///< +0x30 the target's Get2DRadius
	float height {1.0f};              ///< +0x34 the target's GetHeight
};

/// fn_006A0AB0: the object's MapCoords as a world point (the land's altitude + its height above it), and with
/// TakeCentrePos half its height higher
glm::vec3 TargetPosition(entt::entity object, bool centre)
{
	auto position = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object).position;
	if (centre)
	{
		position.y += ecs::effects::ObjectHeight(object) * 0.5f; // GetHeight vt 0x42C (Object 0x638120)
	}
	return position;
}

/// UR_HealSpellChakra::ModifyAtomCollection 0x6A0B20 (ctor 0x6A0810, DefineProperties 0x6B1E40). Flags 2 without 4: not
/// a creator, but the effect waits for its targets until it closes. SoundSpacing (+0x48, 0.2), ScaleRadius (+0x4C) and
/// ScaleHeight (+0x50) are read and never used.
class HealSpellChakra final: public Modifier
{
public:
	explicit HealSpellChakra(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , scaleToObject(object.Bool("ScalePropObjectSize", false))
	    , takeCentre(object.Bool("TakeCentrePos", false))
	    , soundHeal(ReadSoundAction(object, "SoundHeal"))
	    , maxAlpha(object.Float("MaxAlpha", 255.0f))
	    , ageMaxAlpha(object.Float("AtomAgeMaxAlpha", 1.0f))
	    , ageZeroAlpha(object.Float("AtomAgeZeroAlpha", 3.0f))
	    , specular(static_cast<float>(object.Int("SpecularColorR", 0)), static_cast<float>(object.Int("SpecularColorG", 0)),
	               static_cast<float>(object.Int("SpecularColorB", 0)))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// every target of the spell (SpellTargets +0xB8) becomes a chakra, unless another one already has it
		for (auto target = effect.TakeTarget(); target != entt::null; target = effect.TakeTarget())
		{
			// the dynamic_cast to Object and its 3D object (+0x40)
			if (!Available(target) || IsChakraed(target))
			{
				continue;
			}
			const auto position = TargetPosition(target, takeCentre);
			if (effect.GetSink() != nullptr)
			{
				// the heal itself: SpellEvent{5, the target's centre, no movement, strength 1, no shield check, target}
				SpellEventInfo event;
				event.type = SpellEventInfo::Object;
				event.position = position;
				event.strength = 1.0f;
				event.target = target;
				effect.SendSpellEvent(event);
			}
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			auto data = std::make_shared<ChakraData>();
			data->SetTarget(target);
			data->radius = ecs::effects::Object2DRadius(target); // vt 0x64
			data->height = ecs::effects::ObjectHeight(target);   // vt 0x42C
			atom.modifierData.insert_or_assign(this, data);
			atom.position = position;
		}
		// every chakra follows its target and fades with the burst under it; the first atom plays the heal sound
		int index = 0;
		for (size_t i = 0; i < collection.atoms.size(); ++index)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.modifierData.find(this);
			if (found == atom.modifierData.end())
			{
				++i;
				continue;
			}
			auto& data = *std::static_pointer_cast<ChakraData>(found->second);
			// 0x6A0D28..0x6A0D51: the first atom, sound not started (+0x2E) and GetAtomAge > 0 -> StartSound
			if (index == 0 && !data.soundStarted && effect.AtomAge(atom) > 0.0f)
			{
				data.soundStarted = true;
				audio::spell_sounds::StartSound(effect, atom, soundHeal);
			}
			if (data.target != entt::null && !Available(data.target))
			{
				data.target = entt::null;
			}
			// TODO: the target's flag 4 (Object +0x24, UNVERIFIED meaning: off the map?) also ends the chakra
			if (data.target != entt::null)
			{
				atom.position = TargetPosition(data.target, takeCentre);
				if (scaleToObject)
				{
					atom.ruleScale = ecs::effects::Object2DRadius(data.target); // +0x78
				}
			}
			else
			{
				data.done = true;
			}
			if (!data.done)
			{
				if (atom.subCollections.empty())
				{
					return false; // 0x6A0E1C: a chakra with no burst under it detaches the rule
				}
				Fade(effect, *atom.subCollections.front(), data);
				data.fresh = false;
			}
			if (data.done)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i)); // DeleteFromAtomList
				continue;
			}
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	bool scaleToObject; ///< +0x2C ScalePropObjectSize
	bool takeCentre;    ///< +0x2D TakeCentrePos
	SoundAction soundHeal; ///< +0x30
	float maxAlpha;        ///< +0x54
	float ageMaxAlpha;     ///< +0x58
	float ageZeroAlpha;    ///< +0x5C
	glm::vec3 specular;    ///< +0x60 R, +0x64 G, +0x68 B

private:
	/// fn_006A0E30: the burst's age gives t, up to 1 at AtomAgeMaxAlpha and back to 0 at AtomAgeZeroAlpha; its sprites
	/// get alpha t x MaxAlpha and the target glows with t x SpecularColor. An empty burst ends the chakra (not on its
	/// first step, before the burst is made).
	void Fade(const Effect& effect, Collection& burst, ChakraData& data) const
	{
		if (burst.atoms.empty())
		{
			if (!data.fresh)
			{
				data.done = true;
			}
			return;
		}
		const float t = heal::ChakraFade(effect.CollectionAge(burst), ageMaxAlpha, ageZeroAlpha);
		// __ftol: truncated
		const auto alpha = static_cast<uint8_t>(static_cast<int>(t * maxAlpha));
		if (data.target != entt::null)
		{
			SetSpecularColour(data.target, glm::u8vec3(static_cast<uint8_t>(static_cast<int>(specular.r * t)),
			                                           static_cast<uint8_t>(static_cast<int>(specular.g * t)),
			                                           static_cast<uint8_t>(static_cast<int>(specular.b * t))));
		}
		for (auto& sprite : burst.atoms)
		{
			sprite->colour[3] = alpha; // +0x8C's alpha byte
		}
	}
};

/// CreateRuleFusedSphericalExplode::ModifyAtomCollection 0x69F610 (DefineProperties 0x6B1580, defaults from the ctor
/// before 0x6BF52B): once the collection is FuseTime old, NumAtoms atoms fly out in random directions (the upper half
/// with OnlyHemisphere) at one random speed in [MinSpeed, MaxSpeed), y x ScaleYSpeed; the first plays SoundExplode and
/// DisableParent hides the parent atom (flag 0x10). Then the rule detaches.
class FusedSphericalExplode final: public Modifier
{
public:
	explicit FusedSphericalExplode(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , count(object.Int("NumAtoms", 100))
	    , minSpeed(object.String("MinSpeed"))
	    , maxSpeed(object.String("MaxSpeed"))
	    , fuseTime(object.Float("FuseTime", 1.0f))
	    , scaleYSpeed(object.Float("ScaleYSpeed", 1.0f))
	    , hemisphere(object.Bool("OnlyHemisphere", false))
	    , disableParent(object.Bool("DisableParent", true))
	    , sound(ReadSoundAction(object, "SoundExplode"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* source = effect.FindCreator(creator);
		if (source == nullptr || minSpeed.empty() || maxSpeed.empty())
		{
			return false;
		}
		if (effect.CollectionAge(collection) < fuseTime)
		{
			return true;
		}
		// the providers' current values (+0xC); one speed for the whole burst
		const float low = effect.FloatProvider(minSpeed, 0.0f);
		const float speed = effect.Random(effect.FloatProvider(maxSpeed, 0.0f) - low) + low;
		for (int i = 0; i < count; ++i)
		{
			auto& atom = effect.NewAtom(collection, source, nextGroups);
			glm::vec3 direction(0.0f);
			while (glm::dot(direction, direction) == 0.0f)
			{
				direction = effect.RandomInBall(); // PSysRandR3 0x6729F0
			}
			if (hemisphere)
			{
				direction.y = std::abs(direction.y);
			}
			direction *= 1.0f / std::sqrt(glm::dot(direction, direction));
			atom.velocity = glm::vec3(direction.x * speed, direction.y * speed * scaleYSpeed, direction.z * speed);
			if (i == 0)
			{
				audio::spell_sounds::StartSound(effect, atom, sound);
			}
		}
		if (disableParent && collection.parent != nullptr)
		{
			collection.parent->visible = false;
		}
		return false;
	}
	std::string creator;
	std::vector<int> nextGroups;
	int count;             ///< +0x38
	std::string minSpeed;  ///< +0x3C
	std::string maxSpeed;  ///< +0x40
	float fuseTime;        ///< +0x44
	float scaleYSpeed;     ///< +0x48
	bool hemisphere;       ///< +0x4C
	bool disableParent;    ///< +0x4D
	SoundAction sound;     ///< +0x50
};

/// UR_HealInHand::ModifyAtomCollection 0x6A0F40 (DefineProperties 0x6AEB80, WiggleFreq 1 by default): each atom goes to
/// the parent's position (GetCurrentParentPos 0x674AF0: the parent atom's +0x80, or the effect's origin) times
/// sin(age x WiggleFreq x 2 pi), the odd ones with the opposite sign
class HealInHand final: public Modifier
{
public:
	explicit HealInHand(const Object& object)
	    : wiggle(object.Float("WiggleFreq", 1.0f))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const glm::vec3 parent = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		int index = 0;
		for (auto& atom : collection.atoms)
		{
			const float sign = index % 2 == 1 ? -1.0f : 1.0f;
			const float s = std::sin(effect.CollectionAge(collection) * wiggle * (2.0f * std::numbers::pi_v<float>)) * sign;
			atom->position = parent * s;
			++index;
		}
		return true;
	}
	float wiggle; ///< +0x20
};
} // namespace

float openblack::psys::heal::ChakraFade(float age, float ageMaxAlpha, float ageZeroAlpha)
{
	const float t = age < ageMaxAlpha ? age / ageMaxAlpha : 1.0f - (age - ageMaxAlpha) / (ageZeroAlpha - ageMaxAlpha);
	if (!(t > 0.0f))
	{
		return 0.0f;
	}
	return t < 1.0f ? t : 1.0f;
}

void openblack::psys::RegisterHealRules()
{
	RegisterModifier("UR_HealSpellChakra", MakeModifierOf<HealSpellChakra>);
	RegisterModifier("CreateRuleFusedSphericalExplode", MakeModifierOf<FusedSphericalExplode>);
	RegisterModifier("UR_HealInHand", MakeModifierOf<HealInHand>);
}
