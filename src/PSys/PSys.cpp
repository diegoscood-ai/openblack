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
#include <numbers>
#include <set>

#include <glm/geometric.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Locator.h"

using namespace openblack::psys;

namespace
{
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

/// CreateRuleAnAtom 0x69F410: one atom at the spawn point + offset, once
class CreateRuleAnAtom final: public CreateRule
{
public:
	explicit CreateRuleAnAtom(const Object& object)
	    : CreateRule(object)
	    , offset(object.Float("OffsetX", 0.0f), object.Float("OffsetY", 0.0f), object.Float("OffsetZ", 0.0f))
	    , scale(object.String("InitScaleFP"))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		atom.position += offset;
		atom.baseScale *= effect.FloatProvider(scale, 1.0f);
		return false;
	}
	glm::vec3 offset;
	std::string scale;
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
		}
		return false;
	}
	int count;
	float radius;
	std::string radiusScale;
	std::string scale;
	bool frameFromIndex;
};

/// EmitterRule::ShouldEmit 0x6A63A0 and the emitters (Simple 0x6A6700, Disk 0x6A64D0, Conical 0x6A6A60)
class Emitter final: public CreateRule
{
public:
	enum class Shape
	{
		Simple,
		Disk,
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
	    , height(object.Float("Height", 0.0f))
	    , spread(object.Float("Spread", 0.0f))
	{
		// only Conical and SpreadingDisk loop, and only with AllowMultipleEmits
		multiple = object.Bool("AllowMultipleEmits", false) && shape == Shape::Conical;
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
	float height;
	float spread;
};

/// UR_WillowWisp 0x6A6D20: a trail emitted along the parent atom's path (the basic form)
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
	    , deleteAtDieAge(object.Bool("DeleteAtomsAtDieAge", false))
	    , moving(object.Bool("EmitDueToMoving", false))
	    , movingDistance(object.Float("EmitDueToMovingDist", 1.0f))
	    , movingMaxRate(object.Float("EmitDueToMovingMaxRate", 100.0f))
	{
	}
	// slot.state: xyz = last parent position, w = accumulator
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		if (deleteAtDieAge)
		{
			std::erase_if(collection.atoms, [&](const auto& atom) { return effect.AtomAge(*atom) > dieAge; });
		}
		if (!effect.ConditionForCollection(condition, collection))
		{
			return true;
		}
		const glm::vec3 parent = collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin();
		const glm::vec3 last = slot.first ? parent : glm::vec3(slot.state);
		slot.first = false;
		const float dt = effect.GetDt();
		float amount = dt * static_cast<float>(maxAtoms) / std::max(dieAge, 1e-3f);
		if (moving)
		{
			amount = std::max(amount, std::min(glm::distance(parent, last) / std::max(movingDistance, 1e-3f), movingMaxRate * dt));
		}
		slot.state.w += amount;
		glm::vec3 base(0.0f);
		if (collection.parent != nullptr)
		{
			base = collection.parent->velocity * speed;
			if (glm::length(base) > maxSpeed)
			{
				base = glm::normalize(base) * maxSpeed;
			}
		}
		const int count = static_cast<int>(slot.state.w);
		for (int i = 0; i < count; ++i)
		{
			if (static_cast<int>(collection.atoms.size()) > maxAtoms)
			{
				break;
			}
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			const float f = count > 1 ? static_cast<float>(i + 1) / static_cast<float>(count) : 1.0f;
			if (!collection.hierarchy)
			{
				atom.position = last + (parent - last) * f;
			}
			atom.velocity = base + effect.RandomInBall() * effect.Random(randomSpeed);
		}
		slot.state.w -= static_cast<float>(count);
		slot.state = glm::vec4(parent, slot.state.w);
		return true;
	}
	int maxAtoms;
	float dieAge;
	float speed;
	float maxSpeed;
	float randomSpeed;
	bool deleteAtDieAge;
	bool moving;
	float movingDistance;
	float movingMaxRate;
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

class RemoveAfterConditionTrue final: public Modifier
{
public:
	explicit RemoveAfterConditionTrue(const Object& object)
	    : delay(object.Float("Delay", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this];
		if (data.x == 0.0f)
		{
			data = glm::vec4(1.0f, effect.GetAge(), 0.0f, 0.0f);
		}
		return effect.GetAge() - data.y <= delay;
	}
	float delay;
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

/// LandscapeCollide 0x67D6E0
class LandscapeCollide final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (!openblack::Locator::terrainSystem::has_value())
		{
			return true;
		}
		const auto p = effect.GlobalPosition(atom);
		return p.y >= openblack::Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z));
	}
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

/// AR_FadeOutOnceConditionTrue 0x6A7CF0 (the condition is the modifier's own, tested per atom by the caller)
class FadeOutOnceConditionTrue final: public Modifier
{
public:
	explicit FadeOutOnceConditionTrue(const Object& object)
	    : time(object.Float("TimeToFadeOut", 1.0f))
	    , fadeAlpha(object.Bool("FadeAlpha", true))
	    , shrink(object.Bool("ShrinkScale", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this]; // x: latched, y: age0, z: alpha0, w: scale0
		if (data.x == 0.0f)
		{
			data = glm::vec4(1.0f, effect.AtomAge(atom), atom.colour[3], atom.ruleScale);
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
		glm::vec3 a(0.0f);
		a[axis] = 1.0f;
		atom.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), speed * effect.GetDt(), a)) * atom.rotation;
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
			atom.position = atom.collection->hierarchy ? glm::vec3(0.0f) : effect.GlobalPosition(*parent);
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

/// UR_SphereSurfaceTracer 0x6A32B0
class SphereSurfaceTracer final: public Modifier
{
public:
	explicit SphereSurfaceTracer(const Object& object)
	    : radius(object.Float("SphereRadius", 1.0f))
	    , radiusScale(object.String("SphereRadiusFP"))
	    , thetaSpeed(object.Float("ThetaSpeed", 1.0f))
	    , phiSpeed(object.Float("PhiSpeed", 1.0f))
	    , scale(object.Float("ScaleX", 1.0f), object.Float("ScaleY", 1.0f), object.Float("ScaleZ", 1.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this];
		if (data.w == 0.0f)
		{
			data = glm::vec4(effect.Random(2.0f * std::numbers::pi_v<float>), effect.Random(2.0f * std::numbers::pi_v<float>), 0.0f, 1.0f);
		}
		const float age = effect.AtomAge(atom);
		const float th = age * thetaSpeed + data.x;
		const float ph = age * phiSpeed + data.y;
		const float r = radius * effect.FloatProvider(radiusScale, 1.0f);
		const glm::vec3 p = r * glm::vec3(scale.x * std::cos(th) * std::cos(ph), scale.y * std::sin(ph), scale.z * std::sin(th) * std::cos(ph));
		atom.velocity = (p - atom.position) / std::max(effect.GetDt(), 1e-4f);
		atom.position = p;
		return true;
	}
	float radius;
	std::string radiusScale;
	float thetaSpeed, phiSpeed;
	glm::vec3 scale;
};

/// UR_OrientSpriteWithRandomAngle 0x6A2100: a fixed yaw per atom
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
			data.x = 1.0f;
			const float yaw = angle + effect.Random(2.0f * range) - range;
			atom.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)));
		}
		return true;
	}
	float angle, range;
};

/// A class the port doesn't run yet: attached so group logic stays the same, but it does nothing
class Unsupported final: public Modifier
{
};

std::unique_ptr<Modifier> MakeModifier(const Object& object)
{
	const auto& c = object.className;
	std::unique_ptr<Modifier> m;
	if (c == "CreateRuleAnAtom")
	{
		m = std::make_unique<CreateRuleAnAtom>(object);
	}
	else if (c == "CreateRuleSphere")
	{
		m = std::make_unique<CreateRuleSphere>(object);
	}
	else if (c == "EmitterRuleSimple")
	{
		m = std::make_unique<Emitter>(object, Emitter::Shape::Simple);
	}
	else if (c == "DiskEmitter" || c == "SpreadingDiskEmitter")
	{
		m = std::make_unique<Emitter>(object, Emitter::Shape::Disk);
	}
	else if (c == "EmitterRuleConical")
	{
		m = std::make_unique<Emitter>(object, Emitter::Shape::Conical);
	}
	else if (c == "UR_WillowWisp")
	{
		m = std::make_unique<WillowWisp>(object);
	}
	else if (c == "RemoveRuleOldAgeOnly")
	{
		m = std::make_unique<RemoveOldAge>(object);
	}
	else if (c == "RemoveRuleAfterCloseDown")
	{
		m = std::make_unique<RemoveAfterCloseDown>(object);
	}
	else if (c == "RemoveRuleAfterConditionTrue")
	{
		m = std::make_unique<RemoveAfterConditionTrue>(object);
	}
	else if (c == "RemoveRuleProb")
	{
		m = std::make_unique<RemoveProb>(object);
	}
	else if (c == "LandscapeCollide")
	{
		m = std::make_unique<LandscapeCollide>();
	}
	else if (c == "AR_FadeAlpha")
	{
		m = std::make_unique<FadeAlpha>(object);
	}
	else if (c == "AR_FadeCollectionAlpha")
	{
		m = std::make_unique<FadeCollectionAlpha>(object);
	}
	else if (c == "AR_FadeOutOnceConditionTrue")
	{
		m = std::make_unique<FadeOutOnceConditionTrue>(object);
	}
	else if (c == "UR_ChangeScale")
	{
		m = std::make_unique<ChangeScale>(object);
	}
	else if (c == "SetScale")
	{
		m = std::make_unique<SetScale>(object);
	}
	else if (c == "SetAtomAlpha")
	{
		m = std::make_unique<SetAtomAlpha>(object);
	}
	else if (c == "UpdateRuleGravity" || c == "UpdateRuleGravityWithFloor")
	{
		m = std::make_unique<Gravity>(object);
	}
	else if (c == "UR_UpdatePosnFromVelocity")
	{
		m = std::make_unique<PositionFromVelocity>();
	}
	else if (c == "UR_GustyWind")
	{
		m = std::make_unique<GustyWind>(object);
	}
	else if (c == "UpdateRuleRotatePrincipalAxis")
	{
		m = std::make_unique<RotateAxis>(object);
	}
	else if (c == "FollowOrigin")
	{
		m = std::make_unique<FollowOrigin>();
	}
	else if (c == "UR_FollowParent")
	{
		m = std::make_unique<FollowParent>();
	}
	else if (c == "ForceConstantHeight" || c == "ForceConstantAltitude")
	{
		m = std::make_unique<ForceHeight>(object, c == "ForceConstantHeight");
	}
	else if (c == "UR_SphereSurfaceTracer")
	{
		m = std::make_unique<SphereSurfaceTracer>(object);
	}
	else if (c == "UR_OrientSpriteWithRandomAngle")
	{
		m = std::make_unique<RandomAngle>(object);
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
	if (!c.starts_with("Particle") || !c.ends_with("Creator"))
	{
		return nullptr;
	}
	auto creator = std::make_unique<Creator>();
	creator->className = c;
	creator->kind = c == "ParticleSpriteCreator" ? Creator::Kind::Sprite
	                : c == "ParticlePointCreator" ? Creator::Kind::Point
	                                              : Creator::Kind::Other;
	creator->r = static_cast<uint8_t>(object.Int("ColorR", 255));
	creator->g = static_cast<uint8_t>(object.Int("ColorG", 255));
	creator->b = static_cast<uint8_t>(object.Int("ColorB", 255));
	creator->a = static_cast<uint8_t>(object.Int("ColorA", 255));
	creator->initialScale = object.Float("InitialScale", 1.0f);
	creator->randomiseScale = object.Bool("RandomiseScale", false);
	creator->loopAnim = object.Bool("LoopAnim", true);
	if (creator->kind == Creator::Kind::Sprite)
	{
		auto texture = object.String("TextureFileName");
		std::replace(texture.begin(), texture.end(), '\\', '/');
		if (const auto slash = texture.find_last_of('/'); slash != std::string::npos)
		{
			texture = texture.substr(slash + 1);
		}
		if (const auto dot = texture.find_last_of('.'); dot != std::string::npos)
		{
			texture = texture.substr(0, dot);
		}
		creator->texture = texture;
		creator->fileOffset = object.Int("FileOffset", 0);
		creator->spritesPerRow = std::max(1, object.Int("NumSpritesPerRow", 8));
		creator->numFrames = std::max(1, object.Int("NumFrames", 1));
		creator->initFrame = object.Int("InitFrame", 0);
		creator->randomiseInitFrame = object.Bool("RandomiseInitFrame", false);
		creator->randomiseFrameDirection = object.Bool("RandomiseFrameDirection", false);
		creator->frameRate = object.Float("FrameRate", 1.0f);
		creator->playAnim = object.Bool("PlayAnim", false);
		creator->additive = object.Bool("UseAdditiveAlpha", true);
		creator->writeDepth = object.Bool("MaterialUpdateZBuffer", false);
		creator->scaleAlpha = object.Int("ScaleAlpha", 255);
		creator->stretch = object.Float("StretchVertically", 1.0f);
		creator->horizontal = object.Bool("SetHorozontal", false);
		creator->centreAtBase = object.Bool("CentreAtBase", false);
		creator->ignoreRotation = object.Bool("IgnoreRotation", false);
		creator->originX = object.Float("SpriteOriginX", 0.0f);
		creator->originY = object.Float("SpriteOriginY", 0.0f);
	}
	return creator;
}
} // namespace

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

Effect::~Effect() = default;

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
	else if (c == "EventConditionTrueWhenEnabled" || c == "EC_CollectionShouldBeEmitting")
	{
		result = true; // the effect is enabled; emitting (inf)
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
		result = static_cast<float>(atom.colour[3]) > object->Float("Alpha", 0.0f);
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
	if (atom.collection != nullptr && atom.collection->hierarchy && atom.collection->parent != nullptr)
	{
		const auto& parent = *atom.collection->parent;
		return GlobalPosition(parent) + parent.rotation * atom.position;
	}
	return atom.position;
}

glm::vec3 Effect::SpawnPosition(const Collection& collection) const
{
	if (collection.parent == nullptr)
	{
		return _origin;
	}
	return collection.hierarchy ? glm::vec3(0.0f) : GlobalPosition(*collection.parent);
}

Atom& Effect::NewAtom(Collection& collection, const Creator* creator, const std::vector<int>& nextGroups)
{
	auto atom = std::make_unique<Atom>();
	atom->collection = &collection;
	atom->creator = creator;
	atom->birth = _age;
	atom->position = SpawnPosition(collection);
	atom->random = static_cast<uint32_t>(Random(256.0f));
	if (creator != nullptr)
	{
		atom->colour = {creator->r, creator->g, creator->b, creator->a};
		atom->baseScale = creator->initialScale * (creator->randomiseScale ? 0.3f + Random(0.7f) : 1.0f);
		atom->stretch = creator->stretch;
		if (creator->kind == Creator::Kind::Sprite)
		{
			atom->frame = creator->randomiseInitFrame ? std::floor(Random(static_cast<float>(creator->numFrames)))
			                                          : static_cast<float>(creator->initFrame);
			atom->frameRate = creator->playAnim ? creator->frameRate : 0.0f;
			if (creator->randomiseFrameDirection && Random(1.0f) < 0.5f)
			{
				atom->frameRate = -atom->frameRate;
			}
		}
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
	collection->hierarchy = parent != nullptr && _hierarchies[static_cast<size_t>(parent->collection->group)];
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
	for (auto& atom : collection.atoms)
	{
		for (auto& sub : atom->subCollections)
		{
			UpdateCollection(*sub);
		}
	}
}

void Effect::PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation)
{
	for (auto& atom : collection.atoms)
	{
		atom->previous = atom->current;
		auto& draw = atom->current;
		draw.rotation = collection.hierarchy ? parentRotation * atom->rotation : atom->rotation;
		draw.position = collection.hierarchy ? parentPosition + parentRotation * atom->position : atom->position;
		draw.scale = atom->baseScale * atom->ruleScale;
		draw.stretch = atom->stretch;
		draw.alpha = static_cast<float>(atom->colour[3]) * collection.alpha / 255.0f;
		atom->frame += _dt * atom->frameRate;
		draw.frame = atom->frame;
		if (!atom->drawn)
		{
			atom->previous = draw;
			atom->drawn = true;
		}
		for (auto& sub : atom->subCollections)
		{
			PostUpdate(*sub, draw.position, draw.rotation);
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
			// Strength (PSysProcessInfo +0x30) is 1 here (inf)
			const float base = c == "StrengthFloatProvider" ? 1.0f : _magnitude;
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
		PostUpdate(*root, glm::vec3(0.0f), glm::mat3(1.0f));
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
		if (slot.attached && slot.modifier->Creates() && !(_closing && slot.modifier->removeOnCloseDown))
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

void Effect::CollectCollection(const Collection& collection, float t, std::vector<DrawAtom>& out) const
{
	for (const auto& atom : collection.atoms)
	{
		if (atom->visible && atom->drawn && atom->creator != nullptr && atom->creator->kind == Creator::Kind::Sprite)
		{
			const auto& a = atom->previous;
			const auto& b = atom->current;
			const float k = std::clamp(t, 0.0f, 1.0f);
			const float alpha = a.alpha + (b.alpha - a.alpha) * k;
			if (alpha >= 1.0f)
			{
				out.push_back({atom->creator, a.position + (b.position - a.position) * k, b.rotation,
				               a.scale + (b.scale - a.scale) * k, a.stretch + (b.stretch - a.stretch) * k, alpha,
				               a.frame + (b.frame - a.frame) * k, {atom->colour[0], atom->colour[1], atom->colour[2]}});
			}
		}
		for (const auto& sub : atom->subCollections)
		{
			CollectCollection(*sub, t, out);
		}
	}
}

void Effect::Collect(float t, std::vector<DrawAtom>& out) const
{
	for (const auto& root : _roots)
	{
		CollectCollection(*root, t, out);
	}
}
