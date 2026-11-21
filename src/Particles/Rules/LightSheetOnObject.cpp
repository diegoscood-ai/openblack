/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The light-sheet-on-object rule: a ring of light standing on the land round each atom it makes, as the singing stones' healing
// song shows round the stones. On its first step it makes its atoms, each given its effect's player's colour (a grey
// without a player); every step each atom without a ring gets one, round where the atom is, of the radius the rule's
// float provider gives (or its own default), ten high, its top spread out a tenth, on 32 points; and each ring takes
// its atom's colour times the atom's alpha. A ring goes with its atom. The particle system steps and draws the rings
// with its other sheets of light. Wiki: docs/wiki/physics/particle-rules.md#the-ring-of-light-round-an-atom.

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureArenaRing.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/SurfRevol.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// The ring's points, its height and how far its top is spread out
constexpr uint32_t k_RingPoints = 32;
constexpr float k_RingHeight = 10.0f;
constexpr float k_RingSpread = 1.1f;
/// Its strength moves one point along this often, and is fed in at full
constexpr float k_ShiftSeconds = 0.03f;
constexpr float k_Strength = 1.0f;
/// An atom's colour when its effect has no player
constexpr uint32_t k_NoPlayerArgb = 0xFF777777u;

/// The land's height under a point, at the map position the land lookup finds for it; 0 without a land
[[nodiscard]] float GroundAt(glm::vec2 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	const glm::vec2 cell {map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(point.x)),
	                      map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(point.y))};
	return Locator::terrainSystem::value().GetHeightAt(cell);
}

/// Each byte of an atom's colour times its alpha, over 256; the ring keeps no alpha
[[nodiscard]] uint32_t RingRgb(uint32_t argb, uint8_t alpha)
{
	const auto scaled = [argb, alpha](uint32_t shift) { return (((argb >> shift) & 0xFFu) * alpha >> 8u) << shift; };
	return scaled(16) | scaled(8) | scaled(0);
}

/// The collection's data: whether the rule has made its atoms yet
struct RingCollection
{
	bool made {false};
};

/// An atom's data: its colour and its ring, which goes with the atom
struct RingAtom
{
	uint32_t argb {0};
	std::shared_ptr<LightSheet> sheet;
};

/// The light-sheet-on-object rule. Flags 2 without 4: not a creator, but its effect stays while it is attached
class LightSheetOnObject final: public Modifier
{
public:
	explicit LightSheetOnObject(const Object& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _radiusProvider(object.String("RadiusFP"))
	    , _defaultRadius(object.Float("DefaultRadius", 20.0f))
	    , _numAtoms(object.Int("NumAtoms", 1))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* creator = effect.FindCreator(_creator);
		if (creator == nullptr)
		{
			return false;
		}
		auto& made = CollectionDataOf<RingCollection>(collection, this);
		if (!made.made)
		{
			made.made = true;
			for (int i = 0; i < _numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(collection, creator, _nextGroups);
				const int player = effect.GetPlayer();
				AtomDataOf<RingAtom>(atom, this).argb = player >= 0 ? surf_revol::PlayerColour(player) : k_NoPlayerArgb;
			}
		}
		for (const auto& atom : collection.atoms)
		{
			auto& data = AtomDataOf<RingAtom>(*atom, this);
			if (data.sheet == nullptr)
			{
				data.sheet = MakeRing(effect, atom->position);
			}
			data.sheet->SetColour(RingRgb(data.argb, atom->colour[3]));
		}
		return true;
	}

private:
	[[nodiscard]] std::shared_ptr<LightSheet> MakeRing(const Effect& effect, glm::vec3 at) const
	{
		// the rule's float provider's value now, or its own default without one
		const float radius = effect.FloatProvider(_radiusProvider, _defaultRadius);
		// the same circle of points the arena's ring stands on, each on the land
		std::vector<glm::vec3> points;
		for (const auto& point : creature_arena_ring::Points({at.x, at.z}, radius, k_RingPoints))
		{
			points.emplace_back(point.x, GroundAt(point), point.y);
		}
		auto sheet = std::make_shared<LightSheet>();
		// made white; its colour is set right after, every step
		sheet->Start(std::move(points), 0xFFFFFF, k_RingHeight, k_ShiftSeconds);
		sheet->SetSpread(k_RingSpread);
		sheet->SetStrength(k_Strength);
		// its memory starts cleared, so until it is first built it sorts at the world's origin
		sheet->SetUnbuiltMiddle(glm::vec3(0.0f));
		if (Locator::particleSystem::has_value())
		{
			Locator::particleSystem::value().AddLightSheet(sheet);
		}
		return sheet;
	}

	std::string _creator;
	std::vector<int> _nextGroups;
	std::string _radiusProvider;
	float _defaultRadius;
	int _numAtoms;
};
} // namespace

void openblack::particles::RegisterLightSheetRules()
{
	RegisterModifier("UR_LightSheetOnObject", MakeModifierOf<LightSheetOnObject>);
}
