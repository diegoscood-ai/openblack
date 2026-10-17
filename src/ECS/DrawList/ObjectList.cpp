/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectList.h"

#include <cmath>

#include "ECS/DrawList/ObjectOnScreen.h"

namespace openblack::ecs::draw_list
{
namespace
{
/// The squared distance in the original's order: z, then y, then x
[[nodiscard]] float DistanceSquared(glm::vec3 a, glm::vec3 b)
{
	const glm::vec3 d = a - b;
	return (d.z * d.z + d.y * d.y) + d.x * d.x;
}

void AddIfAllowed(entt::entity entity, EntityProbe& probe, List& out)
{
	if (!TryAdd(out.entries.size(), probe.Listed(entity), probe.Available(entity), probe.DontDraw(entity)))
	{
		return;
	}
	out.entries.push_back(entity);
	probe.SetListed(entity, true);
}
} // namespace

bool RebuildDue(const RebuildState& state, uint32_t turn, bool landFlag)
{
	// The sum wraps as the original's unsigned compare does
	return state.count != 0 || turn > state.lastTurn + k_RebuildTurns || landFlag;
}

void StartRebuild(RebuildState& state, uint32_t turn)
{
	state.rebuiltThisFrame = true;
	if (state.count != 0)
	{
		--state.count;
	}
	state.lastTurn = turn;
}

bool CameraMoved(const RebuildState& state, glm::vec3 eye, glm::vec3 focus)
{
	// A NaN compares false, so it counts as still
	return DistanceSquared(eye, state.p) > k_StillDistanceSquared || DistanceSquared(focus, state.f) > k_StillDistanceSquared;
}

void StorePass(RebuildState& state, glm::vec3 eye, glm::vec3 focus)
{
	state.p = eye;
	state.f = focus;
}

bool TakeFullPass(RebuildState& state, glm::vec3 eye, glm::vec3 focus)
{
	if (!state.rebuiltThisFrame)
	{
		if (!CameraMoved(state, eye, focus))
		{
			return false;
		}
		StorePass(state, eye, focus);
	}
	state.rebuiltThisFrame = false;
	return true;
}

bool TryAdd(size_t count, bool listed, bool available, bool dontDraw)
{
	return count != k_MaxEntries && !listed && available && !dontDraw;
}

void Collect(std::span<const uint16_t> visible, std::span<const BlockState> blocks, BlockArraysView arrays,
             std::span<const entt::entity> global, EntityProbe& probe, float vanish, List& out)
{
	for (const auto entity : out.entries)
	{
		if (entity != entt::null)
		{
			probe.SetListed(entity, false);
		}
	}
	out.entries.clear();

	for (const auto block : visible)
	{
		const float distance = blocks[block].distance;
		// Ends at the first block not nearer than the vanish distance; a NaN does not end it
		if (!(distance < vanish) && !std::isunordered(distance, vanish))
		{
			break;
		}
		if (block < arrays.slotOfBlock.size())
		{
			const auto slot = arrays.slotOfBlock[block];
			if (slot < arrays.slots.size())
			{
				for (const auto entity : arrays.slots[slot])
				{
					AddIfAllowed(entity, probe, out);
				}
			}
		}
		for (const auto entity : global)
		{
			AddIfAllowed(entity, probe, out);
		}
	}

	// Entries past the old size start as No; the full pass that follows a rebuild sets every one
	out.active.resize(out.entries.size(), Active::No);
}

void FullPass(List& list, const ObjectProbe& probe, const Consumer& consumer)
{
	list.active.resize(list.entries.size(), Active::No);
	for (size_t i = 0; i < list.entries.size(); ++i)
	{
		const auto entity = list.entries[i];
		if (entity == entt::null)
		{
			list.active[i] = Active::No;
			continue;
		}
		if (!probe.Available(entity))
		{
			list.entries[i] = entt::null;
			list.active[i] = Active::No;
			continue;
		}
		const auto onScreen = consumer(entity);
		if (!onScreen.has_value())
		{
			list.active[i] = Active::NotPorted;
			continue;
		}
		const bool active = ActiveAfterDraw(*onScreen, probe.IsHuman(entity), probe.IsComplex(entity));
		list.active[i] = active ? Active::Yes : Active::No;
	}
}

void StillPass(List& list, const ObjectProbe& probe, const Consumer& consumer)
{
	list.active.resize(list.entries.size(), Active::No);
	for (size_t i = 0; i < list.entries.size(); ++i)
	{
		if (list.active[i] != Active::Yes)
		{
			continue;
		}
		const auto entity = list.entries[i];
		if (entity == entt::null || !probe.Available(entity))
		{
			list.entries[i] = entt::null;
			list.active[i] = Active::No;
			continue;
		}
		// The still pass keeps the flag the last full pass found
		consumer(entity);
	}
}
} // namespace openblack::ecs::draw_list
