/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

#include "GraphicsHandle.h"

/// The uniforms and samplers a mesh draw (Renderer::DrawSubMesh) sets for every primitive, looked up once per program by
/// handle rather than by name on every primitive
namespace openblack::graphics::mesh_uniforms
{

/// In the order the draw first sets them
enum class Uniform : uint8_t
{
	Diffuse,
	Heightmap,
	IslandExtent,
	ObjectClip,
	LandLightTable,
	LandCells,
	CellMap,
	ObjectLight,
	Haze,
	HazeColour,
	ModelLight,
	Window,
	MaterialColour,
	SnowDepth,
	SnowTexture,
	SnowAlpha,
	Snow,
	DynamicShadow,
	DynamicShadowBox,
	DynamicShadowCull,
	SkyAlphaThreshold,
	MorphWeights,
	CreatureSpellLook,
	StaticAlpha,
	AdvisorPupil0,
	AdvisorPupil1,
	AdvisorPupilCentre,
	BonePalette,
	BonePaletteBones,

	_count
};

/// Their names in the shaders, by Uniform
constexpr std::array<std::string_view, static_cast<size_t>(Uniform::_count)> k_Names {
    "s_diffuse",            //
    "s_heightmap",          //
    "u_islandExtent",       //
    "u_objectClip",         //
    "s_landLightTable",     //
    "s_landCells",          //
    "u_cellMap",            //
    "u_objectLight",        //
    "u_haze",               //
    "u_hazeColour",         //
    "u_modelLight",         //
    "u_window",             //
    "u_materialColour",     //
    "s_snowDepth",          //
    "s_snow",               //
    "s_snowAlpha",          //
    "u_snow",               //
    "s_dynamicShadow",      //
    "u_dynamicShadowBox",   //
    "u_dynamicShadowCull",  //
    "u_skyAlphaThreshold",  //
    "u_morphWeights",       //
    "u_creatureSpellLook",  //
    "s_staticAlpha",        //
    "u_advisorPupil0",      //
    "u_advisorPupil1",      //
    "u_advisorPupilCentre", //
    "s_bonePalette",        //
    "u_bonePalette",        //
};

[[nodiscard]] constexpr std::string_view NameOf(Uniform uniform)
{
	return k_Names.at(static_cast<size_t>(uniform));
}

/// A program's handle of each, absent where the program doesn't have it
using Handles = std::array<std::optional<UniformHandle>, static_cast<size_t>(Uniform::_count)>;

/// Each name looked up once with `find` (a program's FindUniform)
template <typename Find>
[[nodiscard]] Handles Resolve(const Find& find)
{
	Handles handles {};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		handles.at(i) = find(k_Names.at(i));
	}
	return handles;
}

} // namespace openblack::graphics::mesh_uniforms
