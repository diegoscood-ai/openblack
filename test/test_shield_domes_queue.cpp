/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shield domes in the frame's single queue (src/Graphics/ShieldDomes.h): a shield service that hands out no dome
// queues nothing, and that is what the interface does unless an implementation says otherwise; each dome handed out
// is one Z object keyed where it stands as the models are ((x^2 + z^2) + y^2), and its queue entry is not a model.
// Fake shield services with synthetic domes.

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "Graphics/ShieldDomes.h"
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"

using namespace openblack;
using openblack::ecs::systems::MagicShieldSystemInterface;

namespace
{
constexpr glm::vec3 k_Camera {0.0f, 0.0f, 0.0f};

/// Only what every shield service must have: the domes for the renderer are the interface's own
class DefaultShields: public MagicShieldSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float /*seconds*/) override {}
	void Reset() override {}
	[[nodiscard]] bool KeepsReactionOff(glm::vec3 /*watcher*/, glm::vec3 /*initiator*/) const override { return false; }
};

/// One dome where it was put, which keeps the turn's fraction it was asked for
class OneDomeShields final: public DefaultShields
{
public:
	explicit OneDomeShields(glm::vec3 at)
	    : dome {.mesh = 554, .position = at, .angle = 0.5f, .scale = 2.0f}
	{
	}

	[[nodiscard]] std::vector<DomeDraw> GetDomes(float turnFraction) const override
	{
		askedFraction = turnFraction;
		return {dome};
	}

	DomeDraw dome;
	mutable float askedFraction {-1.0f};
};
} // namespace

TEST(ShieldDomesQueue, TheInterfaceHandsOutNoDomeByDefault)
{
	const DefaultShields shields {};
	EXPECT_TRUE(shields.GetDomes(0.25f).empty());
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	EXPECT_TRUE(graphics::shield_domes::Submit(shields, 0.25f, queue, k_Camera, [](int i) { return i; }).empty());
	EXPECT_TRUE(queue.Empty());
}

TEST(ShieldDomesQueue, ADomeIsOneZObjectKeyedAsAModel)
{
	const OneDomeShields shields({3.0f, 12.0f, 4.0f});
	graphics::zsort::Queue<graphics::ZObject> queue;
	queue.Begin();
	const auto domes = graphics::shield_domes::Submit(shields, 0.75f, queue, k_Camera,
	                                                  [](int i) { return graphics::ZObject {.shieldDome = i}; });
	// the turn's fraction goes to the service, and its list comes back for the drain's indices
	EXPECT_EQ(shields.askedFraction, 0.75f);
	ASSERT_EQ(domes.size(), 1u);
	EXPECT_EQ(domes[0].mesh, 554u);
	EXPECT_EQ(domes[0].position, shields.dome.position);

	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 1u);
	EXPECT_EQ(drained[0].item->shieldDome, 0);
	// a dome's entry is not a model instance: the drain does not draw it as one
	EXPECT_FALSE(drained[0].item->IsModel());
	// the models' key: (x^2 + z^2) + y^2 from the camera
	EXPECT_EQ(drained[0].key, 169.0f);
	EXPECT_EQ(drained[0].key, graphics::zsort::Key(shields.dome.position, k_Camera, graphics::zsort::SumOrder::XZY));
}
