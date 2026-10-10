/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The tribes' names in the frame's single queue (src/Graphics/TribalPower.h): each ring or column the miracles' looks
// hand out is one Z object keyed at its centre, far to near, with equal keys in the list's order; and the miracles'
// looks hand out none unless an implementation says otherwise. A fake miracles' looks service with two synthetic
// runners is injected through the Locator.

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "Graphics/TribalPower.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Magic/TribalPowerSpin.h"
#include "support/RestoreService.h"

using namespace openblack;
using magic::tribal_spin::Runner;
using openblack::ecs::systems::MiracleFxSystemInterface;

namespace
{
constexpr glm::vec3 k_Camera {0.0f, 0.0f, 0.0f};
constexpr glm::u8vec4 k_Colour {255, 0, 0, 255};

/// Only what every miracles' looks service must have: everything about the tribes' names is the interface's own
class DefaultMiracleFx: public MiracleFxSystemInterface
{
public:
	void UpdateGlobes(float /*gameSeconds*/) override {}
	void UpdateHand(float /*gameSeconds*/) override {}
	void UpdatePiles(float /*seconds*/) override {}
	void SeedInHand(entt::entity /*seed*/, int /*powerUp*/, int /*previousPowerUp*/) override {}
	void SeedLeftHand() override {}
	void SeedShakenOff() override {}
	void Reset() override {}
};

/// A ring round the hand, near the camera, then a column rising farther away
class TwoRunnersMiracleFx final: public DefaultMiracleFx
{
public:
	TwoRunnersMiracleFx(glm::vec3 ringAt, glm::vec3 columnAt)
	    : ring(u"Norse Power", ringAt, k_Colour, true)
	    , column(u"Norse Power", columnAt, k_Colour, false)
	{
	}

	[[nodiscard]] std::vector<const Runner*> GetTribalPowerRunners() const override { return {&ring, &column}; }

	Runner ring;
	Runner column;
};

/// The items of the queue in draw order, and their keys
struct Drawn
{
	std::vector<int> items;
	std::vector<float> keys;
};

Drawn Drain(graphics::zsort::Queue<int>& queue)
{
	Drawn drawn;
	for (const auto& entry : queue.Drain())
	{
		drawn.items.push_back(*entry.item);
		drawn.keys.push_back(entry.key);
	}
	return drawn;
}

std::vector<const Runner*> SubmitAll(const MiracleFxSystemInterface& source, graphics::zsort::Queue<int>& queue)
{
	return graphics::tribal_power::Submit(source, queue, k_Camera, [](int i) { return i; });
}
} // namespace

TEST(TribalPowerQueue, EachRunnerIsAZObjectKeyedAtItsCentreFarthestFirst)
{
	const TwoRunnersMiracleFx fx({3.0f, 4.0f, 0.0f}, {0.0f, 6.0f, 8.0f});
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	const auto runners = SubmitAll(fx, queue);
	// the list comes back as the service gave it, for the drain's indices
	ASSERT_EQ(runners.size(), 2u);
	EXPECT_EQ(runners[0], &fx.ring);
	EXPECT_EQ(runners[1], &fx.column);
	const auto drawn = Drain(queue);
	// the column (index 1) is farther, so it is drawn first; the keys are the squared distances
	EXPECT_EQ(drawn.items, (std::vector<int> {1, 0}));
	EXPECT_EQ(drawn.keys, (std::vector<float> {100.0f, 25.0f}));
	EXPECT_EQ(drawn.keys[0], graphics::zsort::Key(fx.column.Position(), k_Camera));
	EXPECT_EQ(drawn.keys[1], graphics::zsort::Key(fx.ring.Position(), k_Camera));
}

TEST(TribalPowerQueue, EqualKeysAreDrawnInTheListsOrder)
{
	const TwoRunnersMiracleFx fx({0.0f, 0.0f, 5.0f}, {5.0f, 0.0f, 0.0f});
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	(void)SubmitAll(fx, queue);
	const auto drawn = Drain(queue);
	EXPECT_EQ(drawn.items, (std::vector<int> {0, 1}));
	EXPECT_EQ(drawn.keys, (std::vector<float> {25.0f, 25.0f}));
}

TEST(TribalPowerQueue, TheInterfaceHandsOutNothingByDefault)
{
	const DefaultMiracleFx fx {};
	const Runner runner(u"Norse Power", {1.0f, 2.0f, 3.0f}, k_Colour, true);
	EXPECT_TRUE(fx.GetTribalPowerRunners().empty());
	EXPECT_TRUE(fx.GetTribalPowerText(runner).empty());
	EXPECT_EQ(fx.GetTextTexture(), nullptr);
	graphics::zsort::Queue<int> queue;
	queue.Begin();
	EXPECT_TRUE(SubmitAll(fx, queue).empty());
	EXPECT_TRUE(queue.Empty());
}

TEST(TribalPowerQueue, TheRendererReadsTheInjectedService)
{
	const test::RestoreService<Locator::miracleFxSystem> restore;
	auto& fake =
	    Locator::miracleFxSystem::emplace<TwoRunnersMiracleFx>(glm::vec3 {1.0f, 0.0f, 0.0f}, glm::vec3 {2.0f, 0.0f, 0.0f});
	EXPECT_EQ(&graphics::tribal_power::Source(), &fake);
}
