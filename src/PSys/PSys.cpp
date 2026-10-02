/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSys.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <numbers>
#include <set>
#include <string>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/StringUtils.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "PSysRegistry.h"
#include "Rules/Shield.h"
#include "Rules/SurfRevol.h"
#include "SoundAction.h"

using namespace openblack::psys;

namespace
{
/// fn_00679920 0x679B69..0x679BBF: an atom with a DrawOffset (+0x124) is drawn that much away from its PSR, every frame.
/// DrawOffsetLT::GetOffset 0x6C7690 reads my interface's hand (GInterface +0x3A0 +0x78): here the hand of the
/// HandSystem, the one the spells' PSysProcessInfo +0x0C comes from (GetSpellInfo). No hand system (the tests): none
glm::vec3 DrawOffsetOf(const Atom& atom)
{
	if (!atom.drawOffset.has_value() || !openblack::Locator::handSystem::has_value())
	{
		return glm::vec3(0.0f);
	}
	return atom.drawOffset->GetOffset(glm::vec3(openblack::Locator::handSystem::value().GetHandMatrix()[3]));
}

// A smooth value noise in [-1, 1] for UR_GustyWind; the original's VLNoise3To1 (0x590CA0) is not ported (inf)
float Hash(int x, int y, int z)
{
	uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
	             static_cast<uint32_t>(z) * 2147483647u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return static_cast<float>(h & 0xFFFFu) / 32767.5f - 1.0f;
}

float Noise(glm::vec3 p)
{
	const glm::vec3 i = glm::floor(p);
	const glm::vec3 f = p - i;
	const glm::vec3 u = f * f * (3.0f - 2.0f * f);
	const auto x = static_cast<int>(i.x);
	const auto y = static_cast<int>(i.y);
	const auto z = static_cast<int>(i.z);
	const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
	return lerp(lerp(lerp(Hash(x, y, z), Hash(x + 1, y, z), u.x), lerp(Hash(x, y + 1, z), Hash(x + 1, y + 1, z), u.x), u.y),
	            lerp(lerp(Hash(x, y, z + 1), Hash(x + 1, y, z + 1), u.x),
	                 lerp(Hash(x, y + 1, z + 1), Hash(x + 1, y + 1, z + 1), u.x), u.y),
	            u.z);
}

// The window rules (AR_FadeAlpha 0x6A4C40, UR_ChangeScale 0x6A4EE0): lerp inside [start, stop], the stop value on the
// first step past stop, nothing otherwise
bool Window(float t, float dt, float start, float stop, float from, float to, float& out)
{
	if (t >= start && t <= stop)
	{
		out = stop > start ? from + (t - start) / (stop - start) * (to - from) : to;
		return true;
	}
	if (t > stop && t - dt <= stop)
	{
		out = to;
		return true;
	}
	return false;
}

// ---- create rules and emitters (AtomCreateRule: NextGroups, PCreator) ----
class CreateRule: public Modifier
{
public:
	explicit CreateRule(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	std::string creator;
	std::vector<int> nextGroups;
};

/// CreateRuleAnAtom 0x69F410: one atom at the spawn point + offset, once, with its SoundOfCreate (sized by
/// SoundRadiusFP when it has one; ctor 0x69F350 defaults small 200, medium 500)
class CreateRuleAnAtom final: public CreateRule
{
public:
	explicit CreateRuleAnAtom(const Object& object)
	    : CreateRule(object)
	    , offset(object.Float("OffsetX", 0.0f), object.Float("OffsetY", 0.0f), object.Float("OffsetZ", 0.0f))
	    , scale(object.String("InitScaleFP"))
	    , sound(ReadSoundAction(object, "SoundOfCreate"))
	    , soundRadius(object.String("SoundRadiusFP"))
	    , soundSmall(object.Float("SoundRadiusSmall", 200.0f))
	    , soundMedium(object.Float("SoundRadiusMedium", 500.0f))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		atom.position += offset;
		atom.baseScale *= effect.FloatProvider(scale, 1.0f);
		auto action = sound;
		if (!soundRadius.empty())
		{
			action.size = openblack::audio::spell_sounds::SizeFromRadius(effect.FloatProvider(soundRadius, 0.0f), soundSmall,
			                                                             soundMedium);
		}
		openblack::audio::spell_sounds::StartSound(effect, atom, action);
		return false;
	}
	glm::vec3 offset;
	std::string scale;
	SoundAction sound;
	std::string soundRadius;
	float soundSmall;
	float soundMedium;
};

/// CreateRuleSphere 0x69E160: NumAtoms atoms inside a ball, once
class CreateRuleSphere final: public CreateRule
{
public:
	explicit CreateRuleSphere(const Object& object)
	    : CreateRule(object)
	    , count(object.Int("NumAtoms", 1))
	    , radius(object.Float("Radius", 1.0f))
	    , radiusScale(object.String("RadiusScaleFP"))
	    , scale(object.String("InitScaleFP"))
	    , frameFromIndex(object.Bool("SetInitFrameFromIndex", false))
	    , sound(ReadSoundAction(object, "SoundOfCreate"))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const float r = radius * effect.FloatProvider(radiusScale, 1.0f);
		for (int i = 0; i < count; ++i)
		{
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			atom.position += effect.RandomInBall() * r;
			atom.baseScale *= effect.FloatProvider(scale, 1.0f);
			if (frameFromIndex && atom.creator != nullptr && atom.creator->numFrames > 0)
			{
				atom.frame = static_cast<float>((count - 1 - i) % atom.creator->numFrames);
			}
			if (i == 0)
			{
				// (inferido) the first atom only: the 0x69E160 offset of the call was not noted
				openblack::audio::spell_sounds::StartSound(effect, atom, sound);
			}
		}
		return false;
	}
	int count;
	float radius;
	std::string radiusScale;
	std::string scale;
	bool frameFromIndex;
	SoundAction sound;
};

/// EmitterRule::ShouldEmit 0x6A63A0 and the emitters (Simple 0x6A6700, Disk 0x6A64D0, Conical 0x6A6A60)
class Emitter final: public CreateRule
{
public:
	enum class Shape
	{
		Simple,
		Disk,
		SpreadingDisk,
		Conical,
	};
	Emitter(const Object& object, Shape shape)
	    : CreateRule(object)
	    , shape(shape)
	    , frequency(object.Float("EmissionFreq", 0.001f))
	    , maxAtoms(object.Int("MaxAtoms", -1))
	    , maxTotal(object.Int("MaxTotalAtomsToEmit", -1))
	    , randomise(object.Bool("Randomise", true))
	    , multiple(object.Bool("AllowMultipleEmits", false) || shape == Shape::Conical)
	    , visible(object.Bool("InitiallyVisible", true))
	    , speed(object.Float("Speed", 1.0f))
	    , radius(object.Float("Radius", 0.0f))
	    // SpreadingDiskEmitter (DefineProperties 0x6AFA90): +0x54 StartRadius, +0x58 StopRadius, +0x64 Height. Its
	    // StartTime (+0x5C) and StopTime (+0x60) are properties the class never reads (ModifyAtomCollection 0x6A6610)
	    , startRadius(object.Float("StartRadius", 0.0f))
	    , stopRadius(object.Float("StopRadius", 0.0f))
	    , height(object.Float("Height", 0.0f))
	    , spread(object.Float("Spread", 0.0f))
	    , sound(ReadSoundAction(object, "SoundEmission"))
	{
		// EmitterRuleSimple / DiskEmitter make one atom per step; Conical loops with AllowMultipleEmits (0x6A6BF4) and
		// SpreadingDiskEmitter always loops (0x6A66DC)
		multiple = (object.Bool("AllowMultipleEmits", false) && shape == Shape::Conical) || shape == Shape::SpreadingDisk;
	}
	// slot.state: x = next emission time, y = emitted count
	bool ShouldEmit(Effect& effect, const Collection& collection, Collection::Slot& slot) const
	{
		const float age = effect.CollectionAge(collection);
		if (slot.first)
		{
			slot.state.x = age;
			slot.first = false;
		}
		if (maxTotal != -1 && slot.state.y >= static_cast<float>(maxTotal))
		{
			return false;
		}
		if (age + effect.GetDt() <= slot.state.x)
		{
			return false;
		}
		if (maxAtoms >= 0 && static_cast<int>(collection.atoms.size()) > maxAtoms)
		{
			return false;
		}
		slot.state.x += (1.0f / std::max(frequency, 1e-6f)) * (randomise ? 0.5f + effect.Random(0.5f) : 1.0f);
		slot.state.y += 1.0f;
		return true;
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		if (!effect.ConditionForCollection(condition, collection))
		{
			return true;
		}
		int guard = 0;
		while (ShouldEmit(effect, collection, slot) && guard++ < 256)
		{
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			atom.visible = visible;
			switch (shape)
			{
			case Shape::Simple:
			{
				const glm::vec3 d = effect.RandomInBall();
				atom.velocity = (glm::length(d) > 1e-5f ? glm::normalize(d) : glm::vec3(0.0f, 1.0f, 0.0f)) * effect.Random(speed);
				break;
			}
			case Shape::Disk:
			{
				const float angle = effect.Random(2.0f * std::numbers::pi_v<float>);
				const float r = effect.Random(radius);
				atom.position += glm::vec3(std::cos(angle) * r, height, std::sin(angle) * r);
				break;
			}
			case Shape::SpreadingDisk:
			{
				// 0x6A6674: r = PSysFloatRand(StartRadius, StopRadius), theta = PSysFloatRand(2 pi); the atom's point moves by
				// (cos theta r, Height, sin theta r)
				const float r = startRadius + effect.Random(stopRadius - startRadius);
				const float angle = effect.Random(2.0f * std::numbers::pi_v<float>);
				atom.position += glm::vec3(std::cos(angle) * r, height, std::sin(angle) * r);
				break;
			}
			case Shape::Conical:
			{
				const float phi = effect.Random(spread);
				const float theta = effect.Random(2.0f * std::numbers::pi_v<float>);
				const float s = speed * (0.66f + effect.Random(0.33f));
				atom.velocity = s * glm::vec3(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
				if (radius > 0.0f)
				{
					const float r = effect.Random(radius);
					atom.position += glm::vec3(std::cos(theta) * r, 0.0f, std::sin(theta) * r);
				}
				break;
			}
			}
			if (shape == Shape::Conical)
			{
				openblack::audio::spell_sounds::StartSound(effect, atom, sound); // EmitterRuleConical 0x6A6BF4: each atom
			}
			if (!multiple)
			{
				break;
			}
		}
		return true;
	}
	Shape shape;
	float frequency;
	int maxAtoms;
	int maxTotal;
	bool randomise;
	bool multiple;
	bool visible;
	float speed;
	float radius;
	float startRadius, stopRadius;
	float height;
	float spread;
	SoundAction sound;
};

/// UR_WillowWisp 0x6A6D20: atoms emitted along the path of the parent atom (the origin without one). Its CollectionData
/// (0x38 bytes): +0x20 the last position (slot.state xyz), +0x2C the amount emitted so far (slot.extra.x), +0x30 the atoms
/// made (slot.extra.y), +0x34 "first" (slot.first). SmoothingValue and AccelerationForCast are not read in W120, and
/// MaxAtoms is no cap: it only sets the rate.
class WillowWisp final: public CreateRule
{
public:
	explicit WillowWisp(const Object& object)
	    : CreateRule(object)
	    , maxAtoms(object.Int("MaxAtoms", 10))
	    , dieAge(object.Float("DieAge", 1.0f))
	    , speed(object.Float("Speed", 0.0f))
	    , maxSpeed(object.Float("MaxSpeed", 1e6f))
	    , randomSpeed(object.Float("RandomSpeed", 0.0f))
	    , randomRadiusMin(object.Float("RandomRadiusMin", 0.0f))
	    , randomRadiusMax(object.Float("RandomRadiusMax", 0.0f))
	    , deleteAtDieAge(object.Bool("DeleteAtomsAtDieAge", false))
	    , moving(object.Bool("EmitDueToMoving", false))
	    , movingDistance(object.Float("EmitDueToMovingDist", 1.0f))
	    , movingMaxRate(object.Float("EmitDueToMovingMaxRate", 0.0f)) // (inferido) default; 0 = no cap (0x6A6FB6)
	    , randomiseOrientation(object.Bool("RandomiseInitOrientation", false))
	    , useParentScale(object.Bool("UseParentScale", false))
	    , addCastVelocity(object.Bool("AddCastVelToInitPos", false))
	    , emitCondition(object.String("EmitConditionOfParent"))
	    , adjustScale(object.String("AdjustInitialScale"))
	    , adjustRandomVelocity(object.String("AdjustInitialRandomVel"))
	    , sound(ReadSoundAction(object, "SoundEmission"))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		if (deleteAtDieAge)
		{
			std::erase_if(collection.atoms, [&](const auto& atom) { return effect.AtomAge(*atom) > dieAge; });
		}
		// GetCurrentParentPos 0x674AF0 / GetCurrentParentVel 0x674BB0
		const glm::vec3 parent = collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin();
		const glm::vec3 parentVelocity = collection.parent != nullptr ? collection.parent->velocity : glm::vec3(0.0f);
		glm::vec3 base = parentVelocity * speed;
		if (glm::dot(base, base) > maxSpeed * maxSpeed && base != glm::vec3(0.0f))
		{
			base *= maxSpeed / glm::length(base);
		}
		if (slot.first)
		{
			slot.state = glm::vec4(parent, 0.0f);
		}
		const glm::vec3 last(slot.state);
		const float dt = effect.GetDt();
		// 0x6A6F08: fild +0x4C MaxAtoms, fdiv +0x40 DieAge (the original has no guard either)
		const float rate = static_cast<float>(maxAtoms) / dieAge;
		// EmitConditionOfParent: tested on the parent atom (none: nothing is emitted, 0x6A6F28..0x6A6F46)
		const bool emit = emitCondition.empty() ||
		                  (collection.parent != nullptr && effect.ConditionForAtom(emitCondition, *collection.parent));
		if (emit)
		{
			float amount = dt * rate;
			if (moving && movingDistance != 0.0f)
			{
				float moved = glm::distance(parent, last) * (adjustScale.empty() ? 1.0f : effect.FloatProvider(adjustScale, 1.0f));
				moved /= movingDistance;
				if (movingMaxRate != 0.0f)
				{
					moved = std::min(moved / dt, movingMaxRate) * dt;
				}
				amount = std::max(amount, moved);
			}
			const float before = slot.extra.x;
			slot.extra.x += amount;
			const float after = slot.extra.x;
			while (after - 1.0f > slot.extra.y)
			{
				slot.extra.y += 1.0f;
				auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
				glm::vec3 position = parent;
				if (after != before)
				{
					// along last -> P (0x6A7075), and aged fraction x dt: fn_00673CE0((made - before) x dt /
					// (after - before)) at 0x6A70CC..0x6A70F7 (it sets the age: birth = time - x; part_modifiers.md)
					const float fraction = (slot.extra.y - before) / (after - before);
					position = last + (parent - last) * fraction;
					atom.birth -= fraction * dt;
				}
				if (randomRadiusMax > 0.0f)
				{
					const float angle = effect.Random(2.0f * std::numbers::pi_v<float>);
					const float radius = randomRadiusMin + effect.Random(randomRadiusMax - randomRadiusMin);
					position += glm::vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
				}
				glm::vec3 random = effect.RandomInBall() * effect.Random(randomSpeed);
				if (!adjustRandomVelocity.empty())
				{
					random *= effect.FloatProvider(adjustRandomVelocity, 1.0f);
				}
				atom.velocity = base + random;
				if (addCastVelocity)
				{
					position += atom.velocity * dt;
				}
				if (!collection.hierarchy)
				{
					atom.position = position;
				}
				if (randomiseOrientation)
				{
					// AtomCore::RandomiseOrientation 0x6743E0: three PSysFloatRand(2 pi [0x40C90FDB]), each stored in the
					// slot of the argument it pushed (fstp [esp], 0x6743ED / 0x6743FA / 0x674407): the first draw is z,
					// the second y, the third x; then SetAngleXYZ(x, y, z) 0x674200 = Rz(-z) Ry(-y) Rx(-x)
					const float z = effect.Random(2.0f * std::numbers::pi_v<float>);
					const float y = effect.Random(2.0f * std::numbers::pi_v<float>);
					const float x = effect.Random(2.0f * std::numbers::pi_v<float>);
					atom.rotation = openblack::lh_matrix::AngleXYZ(x, y, z);
				}
				if (!adjustScale.empty())
				{
					atom.baseScale *= effect.FloatProvider(adjustScale, 1.0f);
				}
				if (useParentScale && collection.parent != nullptr)
				{
					atom.baseScale *= collection.parent->ruleScale * collection.parent->baseScale;
				}
				// TODO(M2): DoDrawOffsets (+0x80) with this computer's interface casting: a DrawOffset (fn_006C7840) draws
				// the atom relative to the hand until it decays
				openblack::audio::spell_sounds::StartSound(effect, atom, sound); // 0x6A7377: each atom
			}
		}
		slot.first = false;
		slot.state = glm::vec4(parent, 0.0f);
		return true;
	}
	int maxAtoms;
	float dieAge;
	float speed;
	float maxSpeed;
	float randomSpeed;
	float randomRadiusMin;
	float randomRadiusMax;
	bool deleteAtDieAge;
	bool moving;
	float movingDistance;
	float movingMaxRate;
	bool randomiseOrientation;
	bool useParentScale;
	bool addCastVelocity;
	std::string emitCondition;
	std::string adjustScale;
	std::string adjustRandomVelocity;
	SoundAction sound;
};

// ---- remove rules ----
class RemoveOldAge final: public Modifier
{
public:
	explicit RemoveOldAge(const Object& object)
	    : dieAge(object.Float("DieAge", 1.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		return effect.AtomAge(atom) <= dieAge;
	}
	float dieAge;
};

class RemoveAfterCloseDown final: public Modifier
{
public:
	explicit RemoveAfterCloseDown(const Object& object)
	    : delay(object.Float("Delay", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& /*atom*/, Collection::Slot& /*slot*/) const override
	{
		return !(effect.Closing() && effect.GetAge() - effect.GetCloseAge() > delay);
	}
	float delay;
};

/// RemoveRuleAfterConditionTrue: once ConditionForRemove holds (or there is none) the time is kept, and Delay after it
/// the atom goes. (inferido) the latch and the delay: its ModifyAtomCore was not read (no address in the notes)
class RemoveAfterConditionTrue final: public Modifier
{
public:
	explicit RemoveAfterConditionTrue(const Object& object)
	    : delay(object.Float("Delay", 0.0f))
	    , conditionForRemove(object.String("ConditionForRemove"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this];
		if (data.x == 0.0f && (conditionForRemove.empty() || effect.ConditionForAtom(conditionForRemove, atom)))
		{
			data = glm::vec4(1.0f, effect.GetAge(), 0.0f, 0.0f);
		}
		return data.x == 0.0f || effect.GetAge() - data.y <= delay;
	}
	float delay;
	std::string conditionForRemove;
};

class RemoveProb final: public Modifier
{
public:
	explicit RemoveProb(const Object& object)
	    : frequency(object.Float("RemoveFreq", 0.0f))
	    , minAtoms(object.Int("MinAtoms", 0))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		return !(static_cast<int>(atom.collection->atoms.size()) > minAtoms && effect.Random(1.0f) < effect.GetDt() * frequency);
	}
	float frequency;
	int minAtoms;
};

/// LandscapeCollide 0x67D6E0: an atom under the land is deleted; with SendEvent (+0x20) the spell first gets event 3 at
/// the atom's global position with its last global movement, strength 1, no shield check
class LandscapeCollide final: public Modifier
{
public:
	explicit LandscapeCollide(const Object& object)
	    : sendEvent(object.Bool("SendEvent", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (!openblack::Locator::terrainSystem::has_value())
		{
			return true;
		}
		const auto p = effect.GlobalPosition(atom);
		if (p.y >= openblack::Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z)))
		{
			return true;
		}
		if (sendEvent)
		{
			// GetLastGlobalMovement 0x674420 (inf: the atom's velocity over this step)
			SpellEventInfo event;
			event.type = SpellEventInfo::Landed;
			event.position = p;
			event.velocity = atom.velocity * effect.GetDt();
			effect.SendSpellEvent(event);
		}
		return false;
	}
	bool sendEvent;
};

// ---- appearance ----
class FadeAlpha final: public Modifier
{
public:
	explicit FadeAlpha(const Object& object)
	    : start(object.Float("StartTime", 0.0f))
	    , stop(object.Float("StopTime", 1.0f))
	    , from(static_cast<float>(object.Int("StartAlpha", 255)))
	    , to(static_cast<float>(object.Int("StopAlpha", 0)))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		float alpha = 0.0f;
		if (Window(effect.AtomAge(atom), effect.GetDt(), start, stop, from, to, alpha))
		{
			atom.colour[3] = static_cast<uint8_t>(std::clamp(std::trunc(alpha), 0.0f, 255.0f));
		}
		return true;
	}
	float start, stop, from, to;
};

class FadeCollectionAlpha final: public Modifier
{
public:
	explicit FadeCollectionAlpha(const Object& object)
	    : start(object.Float("StartTime", 0.0f))
	    , stop(object.Float("StopTime", 1.0f))
	    , from(static_cast<float>(object.Int("StartAlpha", 255)))
	    , to(static_cast<float>(object.Int("StopAlpha", 0)))
	    , afterCloseDown(object.Bool("TimesAreAfterCloseDown", false))
	    , holdAfterStop(object.Bool("SetAlphaAfterStopTime", false))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		float tau = effect.CollectionAge(collection);
		if (afterCloseDown)
		{
			if (!effect.Closing())
			{
				return true;
			}
			tau = std::max(0.0f, effect.GetAge() - effect.GetCloseAge());
		}
		float alpha = 0.0f;
		if (Window(tau, effect.GetDt(), start, stop, from, to, alpha) || (holdAfterStop && tau > stop && (alpha = to, true)))
		{
			collection.alpha = std::clamp(std::trunc(alpha), 0.0f, 255.0f);
		}
		return true;
	}
	float start, stop, from, to;
	bool afterCloseDown, holdAfterStop;
};

/// AR_FadeOutOnceConditionTrue 0x6A7CF0: once ConditionStartFadeOut (+0x24) holds (or there is none) the age, alpha
/// and scale are kept, and it fades out over TimeToFadeOut from there
class FadeOutOnceConditionTrue final: public Modifier
{
public:
	explicit FadeOutOnceConditionTrue(const Object& object)
	    : time(object.Float("TimeToFadeOut", 1.0f))
	    , fadeAlpha(object.Bool("FadeAlpha", true))
	    , shrink(object.Bool("ShrinkScale", false))
	    , conditionStartFadeOut(object.String("ConditionStartFadeOut"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this]; // x: latched, y: age0, z: alpha0, w: scale0
		if (data.x == 0.0f && (conditionStartFadeOut.empty() || effect.ConditionForAtom(conditionStartFadeOut, atom)))
		{
			data = glm::vec4(1.0f, effect.AtomAge(atom), atom.colour[3], atom.ruleScale);
		}
		if (data.x == 0.0f || !(fadeAlpha || shrink))
		{
			return true;
		}
		const float f = std::clamp((effect.AtomAge(atom) - data.y) / std::max(time, 1e-4f), 0.0f, 1.0f);
		if (fadeAlpha)
		{
			atom.colour[3] = static_cast<uint8_t>(data.z * (1.0f - f));
		}
		if (shrink)
		{
			atom.ruleScale = data.w * (1.0f - f);
		}
		return true;
	}
	float time;
	bool fadeAlpha, shrink;
	std::string conditionStartFadeOut;
};

class ChangeScale final: public Modifier
{
public:
	explicit ChangeScale(const Object& object)
	    : start(object.Float("StartTime", 0.0f))
	    , stop(object.Float("StopTime", 1.0f))
	    , from(object.Float("StartScale", 1.0f))
	    , to(object.Float("StopScale", 1.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		float scale = 1.0f;
		if (Window(effect.AtomAge(atom), effect.GetDt(), start, stop, from, to, scale))
		{
			atom.ruleScale = scale;
		}
		return true;
	}
	float start, stop, from, to;
};

class SetScale final: public Modifier
{
public:
	explicit SetScale(const Object& object)
	    : provider(object.String("Scale"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.ruleScale = effect.FloatProvider(provider, 1.0f);
		return true;
	}
	std::string provider;
};

class SetAtomAlpha final: public Modifier
{
public:
	explicit SetAtomAlpha(const Object& object)
	    : provider(object.String("Alpha"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.colour[3] = static_cast<uint8_t>(std::clamp(effect.FloatProvider(provider, 255.0f), 0.0f, 255.0f));
		return true;
	}
	std::string provider;
};

// ---- motion ----
/// UpdateRuleGravity 0x6A1410
class Gravity final: public Modifier
{
public:
	explicit Gravity(const Object& object)
	    : gravity(object.Float("Gravity", 10.0f))
	    , maxSpeed(object.Float("MaxSpeed", 100.0f))
	    , damping(object.Bool("UseDamping", false) ? object.Float("Damping", 0.0f) : 0.0f)
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const float dt = effect.GetDt();
		atom.velocity *= 1.0f - dt * damping;
		atom.velocity.y -= std::clamp(atom.velocity.y + maxSpeed, 0.0f, 1.0f) * gravity * atom.gravity * dt;
		atom.position += atom.velocity * dt;
		return true;
	}
	float gravity, maxSpeed, damping;
};

class PositionFromVelocity final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.position += atom.velocity * effect.GetDt();
		return true;
	}
};

/// UR_GustyWind 0x6A7500
class GustyWind final: public Modifier
{
public:
	explicit GustyWind(const Object& object)
	    : frequency(object.Float("NoiseFreq", 1.0f))
	    , speed(object.Float("WindSpeed", 1.0f))
	    , damping(object.Float("Damping", 0.0f))
	    , simulate(object.Bool("SimWind", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const glm::vec3 q = atom.position * frequency;
		const glm::vec3 wind = speed * glm::vec3(Noise(q), 0.0f, Noise(glm::vec3(q.y, q.z, q.x)));
		const float dt = effect.GetDt();
		atom.velocity += simulate ? (wind - atom.velocity) * damping * dt : wind * dt;
		atom.position += atom.velocity * dt;
		return true;
	}
	float frequency, speed, damping;
	bool simulate;
};

/// UpdateRuleRotatePrincipalAxis 0x6A1150
class RotateAxis final: public Modifier
{
public:
	explicit RotateAxis(const Object& object)
	    : axis(std::clamp(object.Int("AxisChosen", 1), 0, 2))
	    , speed(object.Float("AngularVel", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		// a = dt x AngularVel (fld [0xD4E0EC]; fmul [+0x24]); every row turned about the axis: Z (x, y) -> (c x + s y,
		// c y - s x) (0x6A116B..0x6A1211), Y (x, z) -> (c x - s z, c z + s x) (0x6A1218..0x6A12C0), X (y, z) ->
		// (c y + s z, c z - s y) (fn_006A12F0) = R_axis(-a) on the left. Z and Y store c as a float (fstp [esp + 8]
		// 0x6A117C / 0x6A1229) and keep s on the stack; fn_006A12F0 keeps both
		const float a = effect.GetDt() * speed;
		if (axis == 0)
		{
			openblack::lh_matrix::TurnRows(atom.rotation, 0, a);
		}
		else
		{
			openblack::lh_matrix::TurnRows(atom.rotation, axis, static_cast<float>(std::cos(static_cast<double>(a))),
			                    std::sin(static_cast<double>(a)));
		}
		return true;
	}
	int axis;
	float speed;
};

class FollowOrigin final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.position = atom.collection->hierarchy ? glm::vec3(0.0f) : effect.GetOrigin();
		return true;
	}
};

class FollowParent final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (const auto* parent = atom.collection->parent; parent != nullptr)
		{
			// the parent's point in this collection's frame (CommonInitNewAtom 0x674C60's rule, see SpawnPosition)
			atom.position = effect.SpawnPosition(*atom.collection);
			atom.velocity = parent->velocity;
		}
		return true;
	}
};

class ForceHeight final: public Modifier
{
public:
	ForceHeight(const Object& object, bool aboveLand)
	    : value(object.Float(aboveLand ? "Height" : "Altitude", 0.0f))
	    , aboveLand(aboveLand)
	{
	}
	bool ModifyAtom(Effect& /*effect*/, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (atom.collection->hierarchy)
		{
			return true;
		}
		float ground = 0.0f;
		if (aboveLand && openblack::Locator::terrainSystem::has_value())
		{
			ground = openblack::Locator::terrainSystem::value().GetHeightAt(glm::vec2(atom.position.x, atom.position.z));
		}
		atom.position.y = ground + value;
		return true;
	}
	float value;
	bool aboveLand;
};

/// UR_SphereSurfaceTracer 0x6A32B0 (DefineProperties 0x6AEBB0: +0x20 ThetaSpeed, +0x24 PhiSpeed, +0x28 SphereRadius,
/// +0x2C ScaleSphereRadius, +0x30 ScaleAlpha, +0x34..0x3C ScaleX/Y/Z, +0x40 Alpha, +0x44 OrientToSurface)
class SphereSurfaceTracer final: public Modifier
{
public:
	explicit SphereSurfaceTracer(const Object& object)
	    : radius(object.Float("SphereRadius", 1.0f))
	    , radiusScale(object.String("ScaleSphereRadius"))
	    , alphaScale(object.String("ScaleAlpha"))
	    , alpha(object.Int("Alpha", 255))
	    , thetaSpeed(object.Float("ThetaSpeed", 1.0f))
	    , phiSpeed(object.Float("PhiSpeed", 1.0f))
	    , scale(object.Float("ScaleX", 1.0f), object.Float("ScaleY", 1.0f), object.Float("ScaleZ", 1.0f))
	    , orientToSurface(object.Bool("OrientToSurface", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		// the first time: three PSysFloatRand(2 pi) (the third is not used here)
		auto& data = atom.data[this];
		if (data.w == 0.0f)
		{
			data = glm::vec4(effect.Random(2.0f * std::numbers::pi_v<float>), effect.Random(2.0f * std::numbers::pi_v<float>), 0.0f, 1.0f);
		}
		const float age = effect.AtomAge(atom);
		const float twoPi = 2.0f * std::numbers::pi_v<float>;
		const float th = std::fmod(age * thetaSpeed + data.x, twoPi);
		const float ph = std::fmod(age * phiSpeed + data.y, twoPi);
		const float r = radius * effect.FloatProvider(radiusScale, 1.0f);
		glm::vec3 p = r * glm::vec3(scale.x * std::cos(th) * std::cos(ph), scale.y * std::sin(ph), scale.z * std::sin(th) * std::cos(ph));
		if (!atom.collection->hierarchy)
		{
			p += effect.SpawnPosition(*atom.collection); // + GetCurrentParentPos (the collection's frame is the world)
		}
		atom.velocity = (p - atom.position) / std::max(effect.GetDt(), 1e-4f);
		atom.position = p;
		// the alpha byte: Alpha x ScaleAlpha (clamped 0..255), or Alpha
		const float a = alphaScale.empty() ? static_cast<float>(alpha)
		                                   : std::clamp(static_cast<float>(alpha) * effect.FloatProvider(alphaScale, 1.0f), 0.0f, 255.0f);
		atom.colour[3] = static_cast<uint8_t>(static_cast<int>(a) & 0xFF);
		// OrientToSurface (+0x44, 0x6A351D; set by SF_DefenseSphereInHand and SF_DefenseSphereOnHolder): each patch
		// turns to face out of the sphere at its (theta, phi), so the 15 MSH_S_SPELLBALLSURFACE02 patches tile one ball
		// instead of all keeping the same orientation. fn_006743A0(pi/2 - phi) [0x8C78D8]: rows (ca, -sa, 0),
		// (sa, ca, 0), (0, 0, 1) (the cos and sin stored as floats) and the 4th row (+0x68..+0x70) zeroed (not kept
		// here); then every row's (x, z) -> (c x - s z, c z + s x) with c = cos theta, s = sin theta (the floats stored
		// at [esp + 0x18] / [esp + 0x14], 0x6A33B3 / 0x6A33D3), 0x6A353D..0x6A35C4
		if (orientToSurface)
		{
			const float angle = std::numbers::pi_v<float> * 0.5f - ph;
			const auto ca = static_cast<float>(std::cos(static_cast<double>(angle)));
			const auto sa = static_cast<float>(std::sin(static_cast<double>(angle)));
			atom.rotation = glm::mat3(glm::vec3(ca, -sa, 0.0f), glm::vec3(sa, ca, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			const auto c = static_cast<float>(std::cos(static_cast<double>(th)));
			const auto s = static_cast<float>(std::sin(static_cast<double>(th)));
			openblack::lh_matrix::TurnRows(atom.rotation, 1, static_cast<double>(c), static_cast<double>(s));
		}
		return true;
	}
	float radius;
	std::string radiusScale;
	std::string alphaScale;
	int alpha;
	float thetaSpeed, phiSpeed;
	glm::vec3 scale;
	bool orientToSurface;
};

/// UR_OrientSpriteWithRandomAngle 0x6A2100: a fixed yaw per atom (DefineProperties 0x6AC660: +0x20 RandomAngle, +0x24
/// DefaultAngle)
class RandomAngle final: public Modifier
{
public:
	explicit RandomAngle(const Object& object)
	    : angle(object.Float("DefaultAngle", 0.0f))
	    , range(object.Float("RandomAngle", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this];
		if (data.x == 0.0f)
		{
			// the AtomData's first flag (+0x20): yaw = (1 - PSysFloatRand(2)) RandomAngle + DefaultAngle (0x6A2187..0x6A21A3)
			data.x = 1.0f;
			data.y = (1.0f - effect.Random(2.0f)) * range + angle;
		}
		// AtomCore::SetAngleY(yaw) every time (0x6A21AC)
		atom.rotation = openblack::lh_matrix::AngleY(data.y);
		return true;
	}
	float angle, range;
};

/// A class the port doesn't run yet: attached so group logic stays the same, but it does nothing
class Unsupported final: public Modifier
{
public:
	[[nodiscard]] bool Unported() const override { return true; }
};

std::unique_ptr<Modifier> MakeModifier(const Object& object)
{
	const auto& c = object.className;
	std::unique_ptr<Modifier> m;
	if (const auto factory = FindModifierFactory(c))
	{
		m = factory(object);
	}
	else if (object.properties.contains("Group"))
	{
		static std::set<std::string> logged;
		if (logged.insert(c).second)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys: {} is not ported yet (no effect)", c);
		}
		m = std::make_unique<Unsupported>();
	}
	if (m)
	{
		m->group = object.Int("Group", -1);
		m->removeOnCloseDown = object.Bool("RemoveOnCloseDown", false);
		m->condition = object.String("Condition");
	}
	return m;
}

std::unique_ptr<Creator> MakeCreator(const Object& object)
{
	const auto& c = object.className;
	// a registered creator first: ParticleMeshCreatorAnimTextured (the water's rain cone) does not end in "Creator"
	if (const auto factory = FindCreatorFactory(c))
	{
		return factory(object);
	}
	if (!c.starts_with("Particle") || !c.ends_with("Creator"))
	{
		return nullptr;
	}
	auto creator = std::make_unique<Creator>();
	ReadCreatorProperties(object, *creator);
	return creator;
}
} // namespace

std::string openblack::psys::TextureBaseName(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	if (const auto slash = path.find_last_of('/'); slash != std::string::npos)
	{
		path = path.substr(slash + 1);
	}
	if (const auto dot = path.find_last_of('.'); dot != std::string::npos)
	{
		path = path.substr(0, dot);
	}
	// Data\Textures once, lower case -> the real stem, so that the spell files' spelling always finds the texture
	static std::map<std::string, std::string, std::less<>> stems;
	static std::once_flag once;
	std::call_once(once, [] {
		if (!Locator::filesystem::has_value())
		{
			return;
		}
		auto& fileSystem = Locator::filesystem::value();
		fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Textures>(), false, [](const std::filesystem::path& file) {
			if (string_utils::LowerCase(file.extension().string()) == ".raw")
			{
				const auto stem = file.stem().string();
				stems.insert_or_assign(string_utils::LowerCase(stem), stem);
			}
		});
	});
	const auto it = stems.find(string_utils::LowerCase(path));
	return it != stems.end() ? it->second : path;
}

void openblack::psys::ReadCreatorProperties(const Object& object, Creator& creator)
{
	const auto& c = object.className;
	creator.className = c;
	creator.kind = c == "ParticleSpriteCreator" ? Creator::Kind::Sprite
	               : c == "ParticlePointCreator" ? Creator::Kind::Point
	                                             : Creator::Kind::Other;
	creator.r = static_cast<uint8_t>(object.Int("ColorR", 255));
	creator.g = static_cast<uint8_t>(object.Int("ColorG", 255));
	creator.b = static_cast<uint8_t>(object.Int("ColorB", 255));
	creator.a = static_cast<uint8_t>(object.Int("ColorA", 255));
	creator.specR = object.Int("SpecColorR", 0); // 0x6B3566, ctor 0 (0x6A91CA)
	creator.specG = object.Int("SpecColorG", 0);
	creator.specB = object.Int("SpecColorB", 0);
	creator.usePlayerColour = object.Bool("UsePlayerColor", false);
	creator.usePlayerColourBlend = object.Float("UsePlayerColorBlend", 1.0f);
	creator.initialScale = object.Float("InitialScale", 1.0f);
	creator.randomiseScale = object.Bool("RandomiseScale", false);
	creator.loopAnim = object.Bool("LoopAnim", true);
	if (creator.kind == Creator::Kind::Sprite)
	{
		creator.texture = TextureBaseName(object.String("TextureFileName"));
		creator.fileOffset = object.Int("FileOffset", 0);
		creator.spritesPerRow = std::max(1, object.Int("NumSpritesPerRow", 8));
		creator.numFrames = std::max(1, object.Int("NumFrames", 1));
		creator.initFrame = object.Int("InitFrame", 0);
		creator.randomiseInitFrame = object.Bool("RandomiseInitFrame", false);
		creator.randomiseFrameDirection = object.Bool("RandomiseFrameDirection", false);
		creator.frameRate = object.Float("FrameRate", 1.0f);
		creator.playAnim = object.Bool("PlayAnim", false);
		creator.additive = object.Bool("UseAdditiveAlpha", true);
		creator.writeDepth = object.Bool("MaterialUpdateZBuffer", false);
		creator.scaleAlpha = object.Int("ScaleAlpha", 255);
		creator.stretch = object.Float("StretchVertically", 1.0f);
		creator.horizontal = object.Bool("SetHorozontal", false);
		creator.centreAtBase = object.Bool("CentreAtBase", false);
		creator.ignoreRotation = object.Bool("IgnoreRotation", false);
		creator.originX = object.Float("SpriteOriginX", 0.0f);
		creator.originY = object.Float("SpriteOriginY", 0.0f);
	}
}

void openblack::psys::RegisterCoreModifiers()
{
	const ModifierFactory simple = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<Emitter>(o, Emitter::Shape::Simple);
	};
	const ModifierFactory disk = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<Emitter>(o, Emitter::Shape::Disk);
	};
	const ModifierFactory spreadingDisk = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<Emitter>(o, Emitter::Shape::SpreadingDisk);
	};
	const ModifierFactory conical = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<Emitter>(o, Emitter::Shape::Conical);
	};
	const ModifierFactory height = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<ForceHeight>(o, true);
	};
	const ModifierFactory altitude = [](const Object& o) -> std::unique_ptr<Modifier> {
		return std::make_unique<ForceHeight>(o, false);
	};
	RegisterModifier("CreateRuleAnAtom", MakeModifierOf<CreateRuleAnAtom>);
	RegisterModifier("CreateRuleSphere", MakeModifierOf<CreateRuleSphere>);
	RegisterModifier("EmitterRuleSimple", simple);
	RegisterModifier("DiskEmitter", disk);
	RegisterModifier("SpreadingDiskEmitter", spreadingDisk);
	RegisterModifier("EmitterRuleConical", conical);
	RegisterModifier("UR_WillowWisp", MakeModifierOf<WillowWisp>);
	RegisterModifier("RemoveRuleOldAgeOnly", MakeModifierOf<RemoveOldAge>);
	RegisterModifier("RemoveRuleAfterCloseDown", MakeModifierOf<RemoveAfterCloseDown>);
	RegisterModifier("RemoveRuleAfterConditionTrue", MakeModifierOf<RemoveAfterConditionTrue>);
	RegisterModifier("RemoveRuleProb", MakeModifierOf<RemoveProb>);
	RegisterModifier("LandscapeCollide", MakeModifierOf<LandscapeCollide>);
	RegisterModifier("AR_FadeAlpha", MakeModifierOf<FadeAlpha>);
	RegisterModifier("AR_FadeCollectionAlpha", MakeModifierOf<FadeCollectionAlpha>);
	RegisterModifier("AR_FadeOutOnceConditionTrue", MakeModifierOf<FadeOutOnceConditionTrue>);
	RegisterModifier("UR_ChangeScale", MakeModifierOf<ChangeScale>);
	RegisterModifier("SetScale", MakeModifierOf<SetScale>);
	RegisterModifier("SetAtomAlpha", MakeModifierOf<SetAtomAlpha>);
	RegisterModifier("UpdateRuleGravity", MakeModifierOf<Gravity>);
	RegisterModifier("UR_UpdatePosnFromVelocity", MakeModifierOf<PositionFromVelocity>);
	RegisterModifier("UR_GustyWind", MakeModifierOf<GustyWind>);
	RegisterModifier("UpdateRuleRotatePrincipalAxis", MakeModifierOf<RotateAxis>);
	RegisterModifier("FollowOrigin", MakeModifierOf<FollowOrigin>);
	RegisterModifier("UR_FollowParent", MakeModifierOf<FollowParent>);
	RegisterModifier("ForceConstantHeight", height);
	RegisterModifier("ForceConstantAltitude", altitude);
	RegisterModifier("UR_SphereSurfaceTracer", MakeModifierOf<SphereSurfaceTracer>);
	RegisterModifier("UR_OrientSpriteWithRandomAngle", MakeModifierOf<RandomAngle>);
	// UR_Explosion is Rules/Explosion.cpp's (RegisterExplosionRules; its water rings are PSysWaterRings'). The
	// UpdateRuleGravityWithFloor of the water work is merged into the fireball one (Rules/Fireball.cpp, which registers it)
}

bool Modifier::ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const
{
	// 0x675B10: each atom whose atom-level condition holds
	std::erase_if(collection.atoms, [&](const std::unique_ptr<Atom>& atom) {
		if (!effect.ConditionForAtom(condition, *atom))
		{
			return false;
		}
		return !ModifyAtom(effect, *atom, slot);
	});
	return true;
}

Effect::Effect(std::shared_ptr<const File> file, glm::vec3 origin, float magnitude, uint32_t seed)
    : _file(std::move(file))
    , _origin(origin)
    , _magnitude(magnitude)
    , _random(seed)
{
	const auto hierarchies = _file->header.Array("Hierarchies");
	for (size_t g = 0; g < _hierarchies.size() && g < hierarchies.size(); ++g)
	{
		_hierarchies[g] = hierarchies[g] != 0;
	}
	_deleteOnCloseDown = _file->header.Bool("DeleteOnCloseDown", true);
	_maxSpellAge = _file->header.Float("MaxSpellAge", -1.0f);
	for (const auto& object : _file->objects)
	{
		if (auto creator = MakeCreator(object))
		{
			_creators.insert_or_assign(object.name, std::move(creator));
			continue;
		}
		if (auto modifier = MakeModifier(object); modifier && modifier->group >= 0 && modifier->group < 25)
		{
			_groups[static_cast<size_t>(modifier->group)].push_back(modifier.get());
			_modifiers.push_back(std::move(modifier));
		}
	}
	// fn_00673070: the initially created groups at the origin
	const auto initially = _file->header.Array("InitiallyCreated");
	for (size_t g = 0; g < initially.size() && g < 25; ++g)
	{
		if (initially[g] != 0)
		{
			CreateCollection(static_cast<int>(g), nullptr, _roots);
		}
	}
}

Effect::~Effect()
{
	// the collections' data go with the effect: UR_AddDefensiveSphere's DefensiveSphere (fn_006A2A30 -> fn_006D0B70)
	shields::RemoveAllOf(this); // Rules/Shield.cpp
}

void Effect::SetSink(SpellSink* sink)
{
	_sink = sink;
	if (_sink != nullptr)
	{
		// fn_00673070: the effect tells its spell it started (the base Spell ignores type 1)
		SpellEventInfo event;
		event.type = SpellEventInfo::Started;
		event.position = _origin;
		SendSpellEvent(event);
	}
}

int Effect::SendSpellEvent(const SpellEventInfo& event) const
{
	return _sink != nullptr ? _sink->SpellEvent(event) : 0;
}

float Effect::Random(float max)
{
	return std::uniform_real_distribution<float>(0.0f, 1.0f)(_random) * max;
}

glm::vec3 Effect::RandomInBall()
{
	for (int i = 0; i < 64; ++i)
	{
		const glm::vec3 p(Random(2.0f) - 1.0f, Random(2.0f) - 1.0f, Random(2.0f) - 1.0f);
		if (glm::dot(p, p) <= 1.0f)
		{
			return p;
		}
	}
	return glm::vec3(0.0f);
}

float Effect::FloatProvider(const std::string& name, float fallback) const
{
	const auto it = _floatValues.find(name);
	return it == _floatValues.end() ? fallback : it->second;
}

bool Effect::ConditionForCollection(const std::string& name, const Collection& collection) const
{
	const auto* object = _file->Find(name);
	if (object == nullptr)
	{
		return true;
	}
	const auto& c = object->className;
	bool result = true;
	if (c == "EventConditionTrueOnCloseDown")
	{
		result = _closing;
	}
	else if (c == "EventConditionCollectionDelay")
	{
		result = CollectionAge(collection) > object->Float("DelayTime", 0.0f);
	}
	else if (c == "EventConditionCollectionLimitedTime")
	{
		const float t = CollectionAge(collection);
		result = t >= object->Float("StartTime", 0.0f) && t < object->Float("StopTime", 0.0f);
	}
	else if (c == "EventConditionTrueWhenEnabled")
	{
		result = _info.enabled; // PSysProcessInfo +0x38
	}
	else if (c == "EC_CollectionShouldBeEmitting")
	{
		result = true; // emitting (inf)
	}
	else if (const auto test = FindCondition(c); test != nullptr)
	{
		result = test(*this, *object, nullptr, collection);
	}
	else
	{
		return true; // atom-level condition, tested per atom
	}
	return result != object->Bool("InvertResponse", false);
}

bool Effect::ConditionForAtom(const std::string& name, const Atom& atom) const
{
	const auto* object = _file->Find(name);
	if (object == nullptr)
	{
		return true;
	}
	const auto& c = object->className;
	bool result = true;
	if (c == "EventConditionAtomDelay")
	{
		result = AtomAge(atom) > object->Float("DelayTime", 0.0f);
	}
	else if (c == "EventConditionAtomLimitedTime")
	{
		const float t = AtomAge(atom);
		result = t >= object->Float("StartTime", 0.0f) && t < object->Float("StopTime", 0.0f);
	}
	else if (c == "EventConditionAtomInUse")
	{
		result = atom.visible;
	}
	else if (c == "EventConditionAtomBelowSpeed")
	{
		result = glm::length(atom.velocity) < object->Float("CutOffSpeed", 0.0f);
	}
	else if (c == "EventConditionAtomBelowHeight")
	{
		const auto p = GlobalPosition(atom);
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z)) : 0.0f;
		result = p.y - ground < object->Float("CutOffHeight", 0.0f);
	}
	else if (c == "EC_AtomAlphaAbove")
	{
		result = static_cast<int>(atom.colour[3]) > object->Int("AlphaValue", 0); // 0x67DC80: alpha > AlphaValue
	}
	else if (const auto test = FindCondition(c); test != nullptr)
	{
		result = test(*this, *object, &atom, *atom.collection);
	}
	else
	{
		return ConditionForCollection(name, *atom.collection);
	}
	return result != object->Bool("InvertResponse", false);
}

const Creator* Effect::FindCreator(const std::string& name) const
{
	const auto it = _creators.find(name);
	return it == _creators.end() ? nullptr : it->second.get();
}

glm::vec3 Effect::GlobalPosition(const Atom& atom) const
{
	return atom.collection != nullptr ? LocalToGlobal(*atom.collection, atom.position) : atom.position;
}

glm::vec3 Effect::FrameScale(const Atom& atom)
{
	// fn_00673DB0: the rotation rows x baseScale x ruleScale, the Y row also x the stretch
	const float s = atom.baseScale * atom.ruleScale;
	return {s, s * atom.stretch, s};
}

glm::vec3 Effect::LocalToGlobal(const Collection& collection, const glm::vec3& local) const
{
	// AtomCollection::LocalToGlobal 0x6751D0: nothing for a collection outside a hierarchy; else fn_006752D0's frame,
	// the product of the local matrices (fn_00673DB0) of every ancestor atom whose group is flagged in Hierarchies (the
	// others add nothing)
	glm::vec3 p = local;
	if (!collection.hierarchy)
	{
		return p;
	}
	for (const Atom* a = collection.parent; a != nullptr && a->collection != nullptr; a = a->collection->parent)
	{
		if (_hierarchies[static_cast<size_t>(a->collection->group)])
		{
			p = a->position + a->rotation * (FrameScale(*a) * p);
		}
	}
	return p;
}

glm::vec3 Effect::GlobalToLocal(const Collection& collection, const glm::vec3& global) const
{
	// AtomCollection::GlobalToLocal 0x675410: the inverse of LocalToGlobal's frame (outermost first)
	if (!collection.hierarchy)
	{
		return global;
	}
	std::vector<const Atom*> frames;
	for (const Atom* a = collection.parent; a != nullptr && a->collection != nullptr; a = a->collection->parent)
	{
		if (_hierarchies[static_cast<size_t>(a->collection->group)])
		{
			frames.push_back(a);
		}
	}
	glm::vec3 p = global;
	for (auto it = frames.rbegin(); it != frames.rend(); ++it)
	{
		const auto s = FrameScale(**it);
		p = glm::inverse((*it)->rotation) * (p - (*it)->position);
		p = glm::vec3(s.x != 0.0f ? p.x / s.x : 0.0f, s.y != 0.0f ? p.y / s.y : 0.0f, s.z != 0.0f ? p.z / s.z : 0.0f);
	}
	return p;
}

glm::vec3 Effect::SpawnPosition(const Collection& collection) const
{
	// CommonInitNewAtom 0x674C60: no parent -> the origin; a parent whose group is flagged -> 0 (the collection is in its
	// frame); else the parent's position (fn_006744B0), in the same frame as this collection
	if (collection.parent == nullptr)
	{
		return _origin;
	}
	const auto* parentCollection = collection.parent->collection;
	if (parentCollection != nullptr && _hierarchies[static_cast<size_t>(parentCollection->group)])
	{
		return glm::vec3(0.0f);
	}
	return collection.parent->position;
}

void Effect::AddSubCollections(Atom& atom, const std::vector<int>& groups)
{
	for (const int group : groups)
	{
		if (group >= 0 && group < 25)
		{
			CreateCollection(group, &atom, atom.subCollections);
		}
	}
}

Atom* Effect::NewAtomInGroup(int group, const Creator* creator)
{
	for (const auto& root : _roots)
	{
		if (root->group == group)
		{
			return &NewAtom(*root, creator, {});
		}
	}
	return nullptr;
}

void Effect::MoveToBaseGroup(Collection& from, const Atom& atom, int group)
{
	// 0x673BD0: fn_00673180 finds the first root collection of the group, fn_00673C00 unlinks the atom and links it at
	// the head of that one (the order of a collection's atoms does not change what the rules do to each one)
	const auto it = std::find_if(from.atoms.begin(), from.atoms.end(), [&](const auto& a) { return a.get() == &atom; });
	if (it == from.atoms.end())
	{
		return;
	}
	auto moved = std::move(*it);
	from.atoms.erase(it);
	for (const auto& root : _roots)
	{
		if (root->group == group && root.get() != &from)
		{
			moved->collection = root.get();
			root->atoms.insert(root->atoms.begin(), std::move(moved));
			return;
		}
	}
	// no root collection of that group: see the header (the atom goes)
}

std::array<uint8_t, 4> openblack::psys::TintWithPlayerColour(std::array<uint8_t, 4> rgba, uint32_t playerArgb, float blend)
{
	uint32_t pc = playerArgb | 0xFF000000u;
	if (pc == 0xFF000000u)
	{
		pc = 0xFFFFFFFFu; // 0x6A8650
	}
	if (blend != 1.0f)
	{
		// 0x6A865C..0x6A86D4: the (c - 255) x b products are signed, shifted and masked to a byte
		const int b = static_cast<int>(blend * 255.0f) & 0xFF;
		const auto towardsWhite = [b](uint32_t c) {
			return static_cast<uint32_t>((255 + ((static_cast<int>(c) - 255) * b >> 8)) & 0xFF);
		};
		pc = (pc & 0xFF000000u) | (towardsWhite((pc >> 16) & 0xFF) << 16) | (towardsWhite((pc >> 8) & 0xFF) << 8) |
		     towardsWhite(pc & 0xFF);
	}
	// 0x6A86D6..0x6A8741: each byte of the colour x the same byte of pc >> 8
	return {static_cast<uint8_t>(rgba[0] * ((pc >> 16) & 0xFF) >> 8), static_cast<uint8_t>(rgba[1] * ((pc >> 8) & 0xFF) >> 8),
	        static_cast<uint8_t>(rgba[2] * (pc & 0xFF) >> 8), static_cast<uint8_t>(rgba[3] * (pc >> 24) >> 8)};
}

Atom& Effect::NewAtom(Collection& collection, const Creator* creator, const std::vector<int>& nextGroups)
{
	if (creator != nullptr)
	{
		creator = creator->Resolve(*this);
	}
	auto atom = std::make_unique<Atom>();
	atom->collection = &collection;
	atom->creator = creator;
	atom->birth = _age;
	atom->position = SpawnPosition(collection);
	atom->random = static_cast<uint32_t>(Random(256.0f));
	if (creator != nullptr)
	{
		atom->colour = {creator->r, creator->g, creator->b, creator->a};
		if (creator->usePlayerColour && _player >= 0)
		{
			atom->colour = TintWithPlayerColour(atom->colour, surf_revol::PlayerColour(_player), creator->usePlayerColourBlend);
		}
		// 0x6A8748..0x6A875B: ((R << 8 | G) << 8) | B, alpha 0
		atom->specular = ((static_cast<uint32_t>(creator->specR) << 8u | static_cast<uint32_t>(creator->specG)) << 8u) |
		                 static_cast<uint32_t>(creator->specB);
		atom->baseScale = creator->initialScale * (creator->randomiseScale ? 0.3f + Random(0.7f) : 1.0f);
		atom->stretch = creator->stretch;
		if (creator->kind == Creator::Kind::Sprite)
		{
			atom->frame = creator->randomiseInitFrame ? std::floor(Random(static_cast<float>(creator->numFrames)))
			                                          : static_cast<float>(creator->initFrame);
			atom->frameRate = creator->frameRate;
			atom->playAnim = creator->playAnim;
			if (creator->randomiseFrameDirection && Random(1.0f) < 0.5f)
			{
				atom->frameRate = -atom->frameRate;
			}
		}
		creator->InitAtom(*this, *atom);
	}
	auto& result = *atom;
	collection.atoms.push_back(std::move(atom));
	for (const int group : nextGroups)
	{
		if (group >= 0 && group < 25)
		{
			CreateCollection(group, &result, result.subCollections);
		}
	}
	return result;
}

void Effect::CreateCollection(int group, Atom* parent, std::vector<std::unique_ptr<Collection>>& into)
{
	auto collection = std::make_unique<Collection>();
	collection->group = group;
	collection->parent = parent;
	collection->birth = _age;
	// AtomCollection ctor 0x6748B0: in a hierarchy when any ancestor collection's group is flagged (+0x4C; the loop at
	// 0x674934 walks up the parents), not only the parent's
	collection->hierarchy = parent != nullptr && (_hierarchies[static_cast<size_t>(parent->collection->group)] ||
	                                              parent->collection->hierarchy);
	for (const auto* modifier : _groups[static_cast<size_t>(group)])
	{
		collection->modifiers.push_back({modifier});
	}
	into.push_back(std::move(collection));
}

void Effect::UpdateCollection(Collection& collection)
{
	for (auto& slot : collection.modifiers)
	{
		if (!slot.attached)
		{
			continue;
		}
		if (_closing && slot.modifier->removeOnCloseDown)
		{
			slot.attached = false;
			continue;
		}
		if (!ConditionForCollection(slot.modifier->condition, collection))
		{
			continue;
		}
		if (!slot.modifier->ModifyCollection(*this, collection, slot))
		{
			slot.attached = false;
		}
	}
	// by index: a sub-collection's rule may add a sibling to the atom it hangs from (UR_CloudGather's AddSubCollection
	// of the tornado group on its core, 0x6D4BBB), which the original links at the head of the list, past the
	// iteration; (aproximado) here it is appended and also updated this step, one step earlier than the original
	for (size_t a = 0; a < collection.atoms.size(); ++a)
	{
		auto& atom = collection.atoms[a];
		for (size_t s = 0; s < atom->subCollections.size(); ++s)
		{
			UpdateCollection(*atom->subCollections[s]);
		}
	}
}

void Effect::PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation,
                        const glm::vec3& parentScale)
{
	// parentPosition / Rotation / Scale: the frame of a collection in a hierarchy (LocalToGlobal), the drawn matrix of
	// the nearest ancestor atom whose group is flagged; the scale also scales the atoms drawn in it
	const bool flagged = _hierarchies[static_cast<size_t>(collection.group)];
	for (auto& atom : collection.atoms)
	{
		atom->previous = atom->current;
		auto& draw = atom->current;
		draw.rotation = collection.hierarchy ? parentRotation * atom->rotation : atom->rotation;
		draw.position = collection.hierarchy ? parentPosition + parentRotation * (parentScale * atom->position) : atom->position;
		// (aproximado) the drawn size takes the parents' scale from x only: a Y stretch of theirs does not reach it
		draw.scale = atom->baseScale * atom->ruleScale * (collection.hierarchy ? parentScale.x : 1.0f);
		draw.stretch = atom->stretch;
		draw.alpha = static_cast<float>(atom->colour[3]) * collection.alpha / 255.0f;
		// fn_00673EA0 (frame_anim::PSysFrameAdvance): previous = current; with PlayAnim, current += dt x rate and both
		// are kept in [0, 2N)
		float previousFrame = atom->frame;
		graphics::frame_anim::PSysFrameAdvance(previousFrame, atom->frame, _dt, atom->frameRate,
		                                       atom->creator != nullptr ? atom->creator->FramesPerAtom() : 0, atom->playAnim);
		atom->previous.frame = previousFrame;
		draw.frame = atom->frame;
		if (!atom->drawn)
		{
			atom->previous = draw;
			atom->drawn = true;
		}
		// a flagged atom is the frame of its sub-collections; the others pass their own frame down
		const glm::vec3 subScale = flagged ? FrameScale(*atom) * (collection.hierarchy ? parentScale : glm::vec3(1.0f)) : parentScale;
		for (auto& sub : atom->subCollections)
		{
			PostUpdate(*sub, flagged ? draw.position : parentPosition, flagged ? draw.rotation : parentRotation, subScale);
		}
	}
}

void Effect::Step(float dt)
{
	_dt = dt;
	_floatValues.clear();
	for (const auto& object : _file->objects)
	{
		const auto& c = object.className;
		if (!c.ends_with("FloatProvider"))
		{
			continue;
		}
		float value = 1.0f;
		const float scaleBy = object.Float("ScaleBy", 1.0f);
		if (c == "ConstFloatProvider")
		{
			value = object.Float("ConstValue", 1.0f);
		}
		else if (c == "MagnitudeFloatProvider" || c == "MagnitudeTimesStrengthFloatProvider" || c == "StrengthFloatProvider")
		{
			// Strength = PSysProcessInfo +0x30 (1 for an effect without a spell)
			float base = _magnitude;
			if (c == "StrengthFloatProvider")
			{
				base = _info.power;
			}
			else if (c == "MagnitudeTimesStrengthFloatProvider")
			{
				base = _magnitude * _info.power;
			}
			value = std::clamp(base * scaleBy, object.Float("Minimum", -1e6f), object.Float("Maximum", 1e6f));
		}
		else if (c == "RenderHandScaleFloatProvider" || c == "RenderHandScaleTimesStrengthFloatProvider")
		{
			value = _magnitude * scaleBy;
		}
		_floatValues.insert_or_assign(object.name, value);
	}
	for (auto& root : _roots)
	{
		UpdateCollection(*root);
	}
	_atomCount = 0;
	const std::function<void(const Collection&)> count = [&](const Collection& c) {
		_atomCount += c.atoms.size();
		for (const auto& atom : c.atoms)
		{
			for (const auto& sub : atom->subCollections)
			{
				count(*sub);
			}
		}
	};
	for (auto& root : _roots)
	{
		PostUpdate(*root, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		count(*root);
	}
	_age += dt;
}

void Effect::CloseDown()
{
	if (!_closing)
	{
		_closing = true;
		_closeAge = _age;
	}
}

bool Effect::AnyCreatorLeft(const Collection& collection) const
{
	for (const auto& slot : collection.modifiers)
	{
		// a spell's effect with a class not ported yet lives until the spell closes it (inf: that class may create atoms,
		// and ending the spell at once would cut its lifecycle short)
		// fn_00673290: a flag 2 modifier keeps it too until it closes (KeepsAlive)
		const bool creates = slot.modifier->Creates() || (!_closing && slot.modifier->KeepsAlive()) ||
		                     (_sink != nullptr && !_closing && slot.modifier->Unported());
		if (slot.attached && creates && !(_closing && slot.modifier->removeOnCloseDown))
		{
			return true;
		}
	}
	for (const auto& atom : collection.atoms)
	{
		for (const auto& sub : atom->subCollections)
		{
			if (AnyCreatorLeft(*sub))
			{
				return true;
			}
		}
	}
	return false;
}

bool Effect::Finished() const
{
	if (_maxSpellAge > 0.0f && _age > _maxSpellAge)
	{
		return true;
	}
	if (_atomCount != 0)
	{
		return false;
	}
	for (const auto& root : _roots)
	{
		if (AnyCreatorLeft(*root))
		{
			return false;
		}
	}
	return true;
}

void Effect::CollectCollection(const Collection& collection, float t, std::vector<DrawAtom>& out, Creator::Kind kind) const
{
	for (const auto& atom : collection.atoms)
	{
		if (atom->visible && atom->drawn && atom->creator != nullptr && atom->creator->kind == kind)
		{
			const auto& a = atom->previous;
			const auto& b = atom->current;
			// fn_00679920 0x67999E: interpolated only when the collection's +0x38 bit 2 is set, else the current PSR
			const float k = (collection.flags & 2) != 0 ? std::clamp(t, 0.0f, 1.0f) : 1.0f;
			const float alpha = (a.alpha + (b.alpha - a.alpha) * k) * _globalAlpha / 255.0f;
			if (alpha >= 1.0f)
			{
				// fn_00679C30: the whole 3x4 frame, rotation included, is blended element by element; the frame between
				// the steps is fn_00679920's own lerp (0x679A79..0x679B03, frame_anim::PSysFrameLerp, t' up to 5 when
				// looped), done whatever the +0x38 bit 2
				out.push_back({atom->creator, a.position + (b.position - a.position) * k + DrawOffsetOf(*atom),
				               a.rotation + (b.rotation - a.rotation) * k,
				               a.scale + (b.scale - a.scale) * k, a.stretch + (b.stretch - a.stretch) * k, alpha,
				               graphics::frame_anim::PSysFrameLerp(a.frame, b.frame, t, atom->creator->loopAnim),
				               {atom->colour[0], atom->colour[1], atom->colour[2]}, atom->specular, atom.get()});
			}
		}
		for (const auto& sub : atom->subCollections)
		{
			CollectCollection(*sub, t, out, kind);
		}
	}
}

void Effect::Collect(float t, std::vector<DrawAtom>& out, Creator::Kind kind) const
{
	for (const auto& root : _roots)
	{
		CollectCollection(*root, t, out, kind);
	}
}

void Effect::CollectChainsOf(const Collection& collection, float t, std::vector<DrawChain>& out) const
{
	// one ribbon per collection: its joints in list order, the same interpolation as CollectCollection but with no
	// alpha cut-off (a dark joint is still part of the strip)
	DrawChain chain {nullptr, {}, &collection, _origin};
	for (const auto& atom : collection.atoms)
	{
		if (atom->visible && atom->drawn && atom->creator != nullptr && atom->creator->kind == Creator::Kind::Chain)
		{
			const auto& a = atom->previous;
			const auto& b = atom->current;
			// fn_00679920 0x67999E (the joints' DrawAt goes through it too): no interpolation without +0x38 bit 2
			const float k = (collection.flags & 2) != 0 ? std::clamp(t, 0.0f, 1.0f) : 1.0f;
			chain.creator = atom->creator;
			chain.joints.push_back({atom->creator, a.position + (b.position - a.position) * k + DrawOffsetOf(*atom),
			                        a.rotation + (b.rotation - a.rotation) * k,
			                        a.scale + (b.scale - a.scale) * k, a.stretch + (b.stretch - a.stretch) * k,
			                        a.alpha + (b.alpha - a.alpha) * k,
			                        graphics::frame_anim::PSysFrameLerp(a.frame, b.frame, t, atom->creator->loopAnim),
			                        {atom->colour[0], atom->colour[1], atom->colour[2]}, atom->specular, atom.get()});
		}
		for (const auto& sub : atom->subCollections)
		{
			CollectChainsOf(*sub, t, out);
		}
	}
	if (chain.joints.size() > 1)
	{
		out.push_back(std::move(chain));
	}
}

void Effect::CollectChains(float t, std::vector<DrawChain>& out) const
{
	for (const auto& root : _roots)
	{
		CollectChainsOf(*root, t, out);
	}
}

void Effect::CollectOrderedOf(const Collection& collection, float t, std::vector<OrderedItem>& items,
                              std::vector<DrawChain>& chains) const
{
	// fn_006798B0: the collection's atoms through fn_00679920 (0x6798B9..0x6798CD), then its chain (0x6798CF..0x6798E6:
	// fn_0067B380 / fn_0067B370 by [0xC0215D]), then every atom's child collections (0x6798EB..0x67990D). The atoms are
	// interpolated as CollectCollection does (its alpha cut-off) and the joints as CollectChainsOf does (none)
	// fn_00679920 0x67999E: interpolated only when the collection's +0x38 bit 2 is set, else the current PSR
	const float k = (collection.flags & 2) != 0 ? std::clamp(t, 0.0f, 1.0f) : 1.0f;
	DrawChain chain {nullptr, {}, &collection, _origin};
	for (const auto& atom : collection.atoms)
	{
		if (!atom->visible || !atom->drawn || atom->creator == nullptr)
		{
			continue;
		}
		const auto& a = atom->previous;
		const auto& b = atom->current;
		const bool joint = atom->creator->kind == Creator::Kind::Chain;
		const float lerped = a.alpha + (b.alpha - a.alpha) * k;
		const float alpha = joint ? lerped : lerped * _globalAlpha / 255.0f;
		if (!joint && alpha < 1.0f)
		{
			continue;
		}
		const DrawAtom drawn {atom->creator,
		                      a.position + (b.position - a.position) * k + DrawOffsetOf(*atom),
		                      a.rotation + (b.rotation - a.rotation) * k,
		                      a.scale + (b.scale - a.scale) * k,
		                      a.stretch + (b.stretch - a.stretch) * k,
		                      alpha,
		                      graphics::frame_anim::PSysFrameLerp(a.frame, b.frame, t, atom->creator->loopAnim),
		                      {atom->colour[0], atom->colour[1], atom->colour[2]},
		                      atom->specular,
		                      atom.get()};
		if (joint)
		{
			chain.creator = atom->creator;
			chain.joints.push_back(drawn);
		}
		else
		{
			items.push_back({drawn, -1});
		}
	}
	if (chain.joints.size() > 1)
	{
		items.push_back({chain.joints[chain.joints.size() / 2], static_cast<int>(chains.size())});
		chains.push_back(std::move(chain));
	}
	for (const auto& atom : collection.atoms)
	{
		for (const auto& sub : atom->subCollections)
		{
			CollectOrderedOf(*sub, t, items, chains);
		}
	}
}

void Effect::CollectOrdered(float t, std::vector<OrderedItem>& items, std::vector<DrawChain>& chains) const
{
	for (const auto& root : _roots)
	{
		CollectOrderedOf(*root, t, items, chains);
	}
}
