/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Hitter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <optional>

#include <glm/vec3.hpp>

#include "Physics/TurnRules.h"

namespace openblack::ecs::physics::hitter
{
void ClearTurnContacts(PhysicsEntry& po)
{
	po.forceSum = glm::vec3(0.0f);
	po.body.lastHit = nullptr;
}

void CaptureLastHits(Entries& objects)
{
	for (auto& po : objects)
	{
		po->lastHitSlot = std::nullopt;
		if (po->body.lastHit == nullptr)
		{
			continue;
		}
		for (size_t j = 0; j < objects.size(); ++j)
		{
			if (&objects[j]->body == po->body.lastHit)
			{
				po->lastHitSlot = static_cast<uint32_t>(j);
				break;
			}
		}
	}
}

bool TakeImpact(PhysicsEntry& po)
{
	const auto impact = openblack::physics::turn::CountedImpact(po.forceSum);
	if (impact.has_value())
	{
		po.impact = *impact;
	}
	return impact.has_value();
}

void FindHitter(Entries& objects, size_t i, bool listGrewInThisLoop)
{
	auto& po = *objects.at(i);
	using openblack::physics::turn::HitterLookup;
	switch (openblack::physics::turn::LookUpHitter(po.lastHitSlot.has_value(), listGrewInThisLoop))
	{
	case HitterLookup::None:
		po.hitBySlot = std::nullopt;
		break;
	case HitterLookup::Keep:
		break;
	case HitterLookup::Slot:
	{
		const auto slot = *po.lastHitSlot;
		// only the listed places are looked at: a place past the end matches nothing, and the hitter stays
		if (slot >= objects.size())
		{
			break;
		}
		po.hitBySlot = slot;
		// the damage goes to whoever threw what hit it
		if (!po.byPlayer)
		{
			po.byPlayer = objects[slot]->byPlayer;
		}
		break;
	}
	}
}

PhysicsEntry* HitterEntry(const Entries& objects, const PhysicsEntry& po)
{
	if (!po.hitBySlot.has_value())
	{
		return nullptr;
	}
	if (*po.hitBySlot >= objects.size())
	{
		// A hitter is read only by the reaction of a body whose force counts, just after its lookup. The lookup gives a
		// listed place, or keeps the old one when the list grew during the walk; a growth happens only with every place
		// in use, and the list never shrinks during the walk, so a kept place is always listed
		std::fprintf(stderr,
		             "physics: a body's hitter names place %u of a list of %zu entries, which the original never reads\n",
		             static_cast<unsigned>(*po.hitBySlot), objects.size());
		std::abort();
	}
	return objects[*po.hitBySlot].get();
}
} // namespace openblack::ecs::physics::hitter
