/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include <Graphics/MeshUniforms.h>
#include <Graphics/UniformTable.h>
#include <gtest/gtest.h>

namespace mesh_uniforms = openblack::graphics::mesh_uniforms;
using openblack::graphics::UniformHandle;
using openblack::graphics::UniformTable;

TEST(MeshUniforms, OneNamePerUniform)
{
	EXPECT_EQ(mesh_uniforms::k_Names.size(), static_cast<size_t>(mesh_uniforms::Uniform::_count));
	const std::set<std::string_view> names(mesh_uniforms::k_Names.begin(), mesh_uniforms::k_Names.end());
	EXPECT_EQ(names.size(), mesh_uniforms::k_Names.size());
	for (const auto name : mesh_uniforms::k_Names)
	{
		// a uniform (u_) or a sampler (s_) of the shaders
		EXPECT_TRUE(name.starts_with("u_") || name.starts_with("s_")) << name;
	}
}

TEST(MeshUniforms, NamesByUniform)
{
	EXPECT_EQ(mesh_uniforms::NameOf(mesh_uniforms::Uniform::Diffuse), "s_diffuse");
	EXPECT_EQ(mesh_uniforms::NameOf(mesh_uniforms::Uniform::ObjectLight), "u_objectLight");
	EXPECT_EQ(mesh_uniforms::NameOf(mesh_uniforms::Uniform::SnowTexture), "s_snow");
	EXPECT_EQ(mesh_uniforms::NameOf(mesh_uniforms::Uniform::MorphWeights), "u_morphWeights");
}

TEST(MeshUniforms, ResolvesWhatTheProgramHas)
{
	// a program with every other uniform, under handles of their own, plus one the mesh draws don't set
	UniformTable table;
	for (size_t i = 0; i < mesh_uniforms::k_Names.size(); i += 2)
	{
		table.Add(mesh_uniforms::k_Names.at(i), UniformHandle {.id = static_cast<uint32_t>(100 + i)});
	}
	table.Add("u_vertexBlend", UniformHandle {.id = 7});

	const auto handles = mesh_uniforms::Resolve([&table](std::string_view name) { return table.Find(name); });
	for (size_t i = 0; i < handles.size(); ++i)
	{
		const auto expected = table.Find(mesh_uniforms::k_Names.at(i));
		ASSERT_EQ(handles.at(i).has_value(), expected.has_value()) << mesh_uniforms::k_Names.at(i);
		EXPECT_EQ(handles.at(i).has_value(), i % 2 == 0) << mesh_uniforms::k_Names.at(i);
		if (expected.has_value())
		{
			EXPECT_EQ(handles.at(i)->id, expected->id) << mesh_uniforms::k_Names.at(i);
		}
	}
}

TEST(MeshUniforms, LooksEachNameUpOnce)
{
	std::set<std::string> asked;
	size_t lookups = 0;
	const auto handles = mesh_uniforms::Resolve([&](std::string_view name) {
		++lookups;
		asked.emplace(name);
		return std::optional<UniformHandle> {};
	});
	EXPECT_EQ(lookups, mesh_uniforms::k_Names.size());
	EXPECT_EQ(asked.size(), mesh_uniforms::k_Names.size());
	for (const auto& handle : handles)
	{
		EXPECT_FALSE(handle.has_value());
	}
}
