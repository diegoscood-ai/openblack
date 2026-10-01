/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The key-point rules: UR_KPStretchHeight and UR_KPMoveAtoms (the heal power-up's mushroom, SF_HealChakraPU) on
// KPSplineInterpolator<float>, which UR_ForestPath (Forest.cpp) shares. Disassembly: dev\tmp_dis\miracles\impl\m4h\kp.txt
// and impl\m6b; wiki: docs/bw1-notes/magic.md, "Explosión de rayo (M6b)".

#include "KeyPoints.h"

#include <algorithm>

#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"

using namespace openblack::psys;

key_points::Spline key_points::Make(const std::vector<float>& pairs, bool zeroEndSlopes)
{
	// 0x6AE170: n = (count / 2 * 2) / 2 keys, the t and the value of each copied, then fn_005B3760(keys, n, yp1, ypn)
	Spline spline;
	const size_t n = pairs.size() / 2;
	spline.keys.resize(n);
	for (size_t i = 0; i < n; ++i)
	{
		spline.keys[i] = glm::vec3(pairs[2 * i], pairs[2 * i + 1], 0.0f);
	}
	if (n < 2)
	{
		return spline; // (port guard) fn_005B3760 needs two keys
	}
	auto& k = spline.keys;
	const float yp1 = zeroEndSlopes ? 0.0f : 1e30f; // 0 or 0x7149F2CA
	const float ypn = yp1;
	std::vector<float> u(n - 1, 0.0f);
	// 0x5B37A2: the first slope; "> 0.99e30" (the double 0x9003C0) makes it natural
	if (yp1 > 0.99e30f)
	{
		k[0].z = 0.0f;
		u[0] = 0.0f;
	}
	else
	{
		k[0].z = -0.5f;
		const float h = k[1].x - k[0].x;
		u[0] = 3.0f / h * ((k[1].y - k[0].y) / h - yp1);
	}
	// 0x5B380C: the decomposition
	for (size_t i = 1; i + 1 < n; ++i)
	{
		const float sig = (k[i].x - k[i - 1].x) / (k[i + 1].x - k[i - 1].x);
		const float p = sig * k[i - 1].z + 2.0f;
		k[i].z = (sig - 1.0f) / p;
		u[i] = (k[i + 1].y - k[i].y) / (k[i + 1].x - k[i].x) - (k[i].y - k[i - 1].y) / (k[i].x - k[i - 1].x);
		u[i] = (6.0f * u[i] / (k[i + 1].x - k[i - 1].x) - sig * u[i - 1]) / p;
	}
	// 0x5B38D7: the last slope
	float qn = 0.0f;
	float un = 0.0f;
	if (ypn <= 0.99e30f)
	{
		qn = 0.5f;
		const float h = k[n - 1].x - k[n - 2].x;
		un = 3.0f / h * (ypn - (k[n - 1].y - k[n - 2].y) / h);
	}
	k[n - 1].z = (un - qn * u[n - 2]) / (qn * k[n - 2].z + 1.0f);
	// the back substitution
	for (size_t i = n - 1; i-- > 0;)
	{
		k[i].z = k[i].z * k[i + 1].z + u[i];
	}
	return spline;
}

float key_points::Evaluate(const Spline& spline, float t, float unchanged)
{
	const auto& k = spline.keys;
	if (k.size() < 2)
	{
		return unchanged; // (port guard) no pair of keys: nothing written
	}
	size_t lo = 0;
	size_t hi = k.size() - 1;
	while (hi - lo > 1)
	{
		const size_t mid = (hi + lo) >> 1;
		if (k[mid].x > t)
		{
			hi = mid;
		}
		else
		{
			lo = mid;
		}
	}
	const float h = k[hi].x - k[lo].x;
	if (h == 0.0f)
	{
		return unchanged;
	}
	const float a = (k[hi].x - t) / h;
	const float b = (t - k[lo].x) / h;
	// [0x8AB268] = 1/6
	return a * k[lo].y + b * k[hi].y + ((a * a * a - a) * k[lo].z + (b * b * b - b) * k[hi].z) * (h * h) * (1.0f / 6.0f);
}

namespace
{
std::vector<float> Floats(const Object& object, std::string_view key, std::vector<float> fallback)
{
	const auto it = object.properties.find(key);
	return it != object.properties.end() ? it->second.numbers : fallback;
}

/// UR_KPStretchHeight::ModifyAtomCore 0x6A50C0 (ctor 0x6A4F50: StartTime 0, StopTime 5, keys (0, 1), (5, 1); +0x20 the
/// keys, +0x2C StartTime, +0x30 StopTime): inside [StartTime, StopTime] (the step that passes StopTime evaluates at it)
/// the atom's stretch (+0x7C) = the curve at the atom's age (the keys are atom seconds)
class KPStretchHeight final: public Modifier
{
public:
	explicit KPStretchHeight(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 5.0f))
	    , spline(key_points::Make(Floats(object, "KeyPoints", {0.0f, 1.0f, 5.0f, 1.0f})))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		float t = effect.AtomAge(atom);
		if (t < startTime || t > stopTime)
		{
			return true;
		}
		if (effect.GetDt() + t > stopTime)
		{
			t = stopTime;
		}
		atom.stretch = key_points::Evaluate(spline, t, atom.stretch);
		return true;
	}
	float startTime, stopTime;
	key_points::Spline spline;
};

/// UR_KPMoveAtoms::ModifyAtomCollection 0x6A60B0 (ctor 0x6A5F40: StartTime 0, StopTime 5, keys (0, 0), (5, 0),
/// MovePropAtomIndex 0; +0x20 StartTime, +0x24 StopTime, +0x28 KeyPointsY, +0x34 MovePropAtomIndex): every atom inside
/// [StartTime, StopTime] of its own age goes to GetCurrentParentPos + (0, curve, 0); with MovePropAtomIndex the offset x
/// index / (count - 1), index 0 being the head of the original's list (the newest atom)
class KPMoveAtoms final: public Modifier
{
public:
	explicit KPMoveAtoms(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 5.0f))
	    , proportional(object.Bool("MovePropAtomIndex", false))
	    , spline(key_points::Make(Floats(object, "KeyPointsY", {0.0f, 0.0f, 5.0f, 0.0f})))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const size_t count = collection.atoms.size();
		// 1 / (count - 1); (port guard) the original divides by 0 with one atom (inf x 0 = NaN): 0 here
		const float inverse = count > 1 ? 1.0f / static_cast<float>(count - 1) : 0.0f;
		// GetCurrentParentPos 0x674AF0: the parent atom's +0x80, or the origin
		const glm::vec3 parent = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		for (size_t n = 0; n < count; ++n)
		{
			auto& atom = *collection.atoms[n];
			float t = effect.AtomAge(atom);
			if (t < startTime || t > stopTime)
			{
				continue;
			}
			if (effect.GetDt() + t > stopTime)
			{
				t = stopTime;
			}
			glm::vec3 offset(0.0f, key_points::Evaluate(spline, t, 0.0f), 0.0f);
			if (proportional)
			{
				// openblack keeps the atoms oldest first: the original's index is from the newest
				offset *= static_cast<float>(count - 1 - n) * inverse;
			}
			atom.position = parent + offset;
		}
		return true;
	}
	float startTime, stopTime;
	bool proportional;
	key_points::Spline spline;
};
} // namespace

void openblack::psys::RegisterKeyPointRules()
{
	RegisterModifier("UR_KPStretchHeight", MakeModifierOf<KPStretchHeight>);
	RegisterModifier("UR_KPMoveAtoms", MakeModifierOf<KPMoveAtoms>);
}
