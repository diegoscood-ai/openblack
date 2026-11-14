/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <functional>
#include <memory>
#include <unordered_map>

#include <entt/core/type_info.hpp>

#include "Queue.h"

#pragma once

namespace openblack
{
class EventManager
{
public:
	template <typename Event, typename... Args>
	void Create(Event event)
	{
		Insist<Event>()->Produce(event);
	}

	template <typename Event>
	void AddHandler(EventHandler<Event> callback)
	{
		Insist<Event>()->AddHandler(callback);
	}

private:
	/// One queue per event type, by the type's index (EnTT's, as the registry's components have)
	mutable std::unordered_map<entt::id_type, std::unique_ptr<IEventQueue>> _queues {};
	template <typename Event>
	EventQueue<Event>* Insist()
	{
		const auto eventType = entt::type_index<Event>::value();
		std::unique_ptr<IEventQueue>& p = _queues[eventType];

		if (!p)
		{
			p = std::make_unique<EventQueue<Event>>();
		}

		return static_cast<EventQueue<Event>*>(p.get());
	}
};
} // namespace openblack
