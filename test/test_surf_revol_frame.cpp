/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The matrix a surface of revolution is raised above the land with: the atom's own matrix (its scales, the second row
// times its stretch, at its place), through every ancestor whose group passes its frame down, and a mesh point taken
// through it in the original's order (docs/wiki/physics/particle-rules.md, ZR_SurfRevol)

#include <array>
#include <memory>
#include <string>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "Common/GameRandom.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/SurfRevol.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// An effect of no rules whose group 1 passes its frame down
std::unique_ptr<Effect> GroupOnePassesItsFrame()
{
	auto file = File::Parse("BEGINPROPERTIES\n"
	                        "PROPERTY DeleteOnCloseDown BOOL 0\n"
	                        "PROPERTY Hierarchies ARRAY SIZE 25 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	                        "PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
	                        "ENDPROPERTIES\n",
	                        "SF_SurfRevolFrameTest");
	EXPECT_TRUE(file.has_value());
	if (!file.has_value())
	{
		return nullptr;
	}
	return std::make_unique<Effect>(std::make_shared<const File>(std::move(*file)), glm::vec3(0.0f), 1.0f,
	                                game_random::particles::NetGameType::Synced);
}
} // namespace

// Outside a hierarchy the frame is the atom's own matrix: its turn's rows times its rule scale times its base scale,
// the second row times its stretch as well, at its place
TEST(SurfRevolFrame, OutsideAHierarchyItIsTheAtomsOwnMatrixWithItsStretch)
{
	const auto effect = GroupOnePassesItsFrame();
	ASSERT_NE(effect, nullptr);
	Collection collection;
	Atom atom;
	atom.baseScale = 2.0f;
	atom.ruleScale = 2.0f;
	atom.stretch = 2.0f;
	atom.position = glm::vec3(10.0f, 20.0f, 30.0f);
	const auto frame = surf_revol::GlobalFrame(*effect, collection, atom);
	EXPECT_EQ(frame.m, (std::array<float, 12> {4.0f, 0.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 0.0f, 4.0f, 10.0f, 20.0f, 30.0f}));
	EXPECT_EQ(surf_revol::Through(frame, glm::vec3(1.0f, 1.0f, 1.0f)), glm::vec3(14.0f, 28.0f, 34.0f));
	// back through its inverse
	EXPECT_EQ(surf_revol::Through(affine::Inverse(frame), glm::vec3(14.0f, 28.0f, 34.0f)), glm::vec3(1.0f, 1.0f, 1.0f));
}

// In a hierarchy the frame is the atom's own matrix times the own matrix of each ancestor whose group passes its frame
// down, the nearest first; an ancestor whose group does not pass it adds nothing
TEST(SurfRevolFrame, InAHierarchyEachAncestorThatPassesItsFrameTakesItFurther)
{
	const auto effect = GroupOnePassesItsFrame();
	ASSERT_NE(effect, nullptr);
	Collection outer {.group = 1};
	Atom grandparent;
	grandparent.collection = &outer;
	grandparent.baseScale = 2.0f;
	grandparent.position = glm::vec3(100.0f, 0.0f, 0.0f);
	Collection middle {.group = 2, .parent = &grandparent};
	middle.hierarchy = true;
	Atom parent;
	parent.collection = &middle;
	parent.position = glm::vec3(0.0f, 50.0f, 0.0f); // group 2 passes nothing
	Collection inner {.group = 0, .parent = &parent};
	inner.hierarchy = true;
	Atom atom;
	atom.collection = &inner;
	atom.position = glm::vec3(1.0f, 2.0f, 3.0f);
	const auto frame = surf_revol::GlobalFrame(*effect, inner, atom);
	EXPECT_EQ(frame.m, (std::array<float, 12> {2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 102.0f, 4.0f, 6.0f}));
	// a point goes through the atom's matrix, then the grandparent's
	EXPECT_EQ(surf_revol::Through(frame, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(104.0f, 4.0f, 6.0f));
}
