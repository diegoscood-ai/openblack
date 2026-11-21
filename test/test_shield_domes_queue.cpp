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
// The domes the object loop holds back for after the particle effects are queued the newest shield first, keep the
// models' key, and come out after an effect at an equal key.
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
	[[nodiscard]] uint32_t NextShieldSerial() override { return 0; }
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

TEST(ShieldDomesQueue, HeldDomesAreQueuedTheNewestShieldFirst)
{
	// three domes at one point (an equal key), held in the models' order, with their shields' serials
	constexpr glm::vec3 k_Point {3.0f, 12.0f, 4.0f};
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	graphics::shield_domes::HeldDomes<int> held;
	held.Hold(1, k_Point, k_Camera, 3);
	held.Hold(2, k_Point, k_Camera, 7);
	held.Hold(3, k_Point, k_Camera, 5);
	held.Hold(4, k_Point, k_Camera, 7);
	EXPECT_EQ(held.Size(), 4u);
	held.Submit(queue);

	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 4u);
	// the highest serial first; two domes of one serial in the order they were held
	EXPECT_EQ(*drained[0].item, 2);
	EXPECT_EQ(*drained[1].item, 4);
	EXPECT_EQ(*drained[2].item, 3);
	EXPECT_EQ(*drained[3].item, 1);
}

TEST(ShieldDomesQueue, AHeldDomeKeepsTheModelsKey)
{
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	graphics::shield_domes::HeldDomes<int> held;
	constexpr glm::vec3 k_Camera2 {1.0f, 2.0f, 3.0f};
	constexpr glm::vec3 k_Point {4.0f, 14.0f, 7.0f};
	held.Hold(1, k_Point, k_Camera2, 0);
	held.Submit(queue);

	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 1u);
	// (x^2 + z^2) + y^2 from the camera: (9 + 16) + 144
	EXPECT_EQ(drained[0].key, 169.0f);
	EXPECT_EQ(drained[0].key, graphics::zsort::Key(k_Point, k_Camera2, graphics::zsort::SumOrder::XZY));
}

TEST(ShieldDomesQueue, AHeldDomeIsQueuedAfterAnEffectAtAnEqualKey)
{
	// a point whose two sums are equal: (x^2 + z^2) + y^2 = (x^2 + y^2) + z^2 = 169
	constexpr glm::vec3 k_Point {3.0f, 12.0f, 4.0f};
	ASSERT_EQ(graphics::zsort::Key(k_Point, k_Camera, graphics::zsort::SumOrder::XZY),
	          graphics::zsort::Key(k_Point, k_Camera, graphics::zsort::SumOrder::XYZ));
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	graphics::shield_domes::HeldDomes<int> held;
	// the object loop holds the dome (1), the effect (2) is queued after it, then the held domes
	held.Hold(1, k_Point, k_Camera, 0);
	queue.Submit(2, graphics::zsort::Key(k_Point, k_Camera));
	held.Submit(queue);

	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 2u);
	EXPECT_EQ(*drained[0].item, 2);
	EXPECT_EQ(*drained[1].item, 1);
	EXPECT_EQ(drained[1].key, 169.0f);
}
