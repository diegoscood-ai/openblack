/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MistSystem.h"

#include <utility>

#include "3D/FrameAnim.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "Graphics/Mists.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

MistSystem::MistSystem()
    : _inView(mists::InView)
{
}

MistSystem::MistSystem(InViewTest inView)
    : _inView(std::move(inView))
{
}

void MistSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// Only a mist on screen is drawn, and only a drawn mist moves its animation on. The fraction of a step is kept, so
	// that the animation keeps its pace at any frame rate
	Locator::entitiesRegistry::value().Each<Mist, const Transform>(
	    [this, gameTime](Mist& mist, const Transform& transform) {
		    if (!_inView(transform.position, mist.size))
		    {
			    return;
		    }
		    graphics::frame_anim::MistClock clock {mist.counter, mist.counterRemainder};
		    graphics::frame_anim::MistAdvance(clock, gameTime.count());
		    mist.counter = clock.counter;
		    mist.counterRemainder = clock.remainder;
	    },
	    entt::exclude<Unavailable>);
}
