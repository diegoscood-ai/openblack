/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How to add a Shader:
// Shaders in openblack are compiled using bgfx's shaderc compiler. Shaderc
// will compile different variations for different rendering APIs and
// platforms. The implementation is in Shaders.cmake.
// Shader sources are located in assets/shaders. The shader language used is a
// subset of glsl made specifically for bgfx.
// Inputs and outputs of shaders (excluding uniforms, textures samplers, etc)
// are declared in varying.def.sc.
// Once compiled they go in ${CMAKE_BINARY_DIR}/include/generated/shaders and
// are included in the openblack binary by way of ShaderManager.cpp.
// The helper header ShaderIncluder.h used with the SHADER_NAME define will
// automatically include all the shader variants in the file.
// Once included, they must be added to the s_embeddedShaders array.
// Finally, the renderer calls the shader manager to load all shaders
// named in the Shaders array.
// tldr:
// 1. Create shaders in assets/shaders
// 2. Define SHADER_NAME and include ShaderIncluder.h
// 3. Add BGFX_EMBEDDED_SHADER entries to s_embeddedShaders
// 4. Add ShaderDefinition to Shaders array

#include "ShaderManager.h"

#include <cstdint> // Shaders below need uint8_t

#include <array>

#include <bgfx/embedded_shader.h>
// BGFX has support for WSL to use windows d3d. We disable it here from the BGFX_EMBEDDED_SHADER macro.
#if BX_PLATFORM_LINUX
#undef BGFX_EMBEDDED_SHADER_DXBC
#define BGFX_EMBEDDED_SHADER_DXBC(...)
#undef BGFX_EMBEDDED_SHADER_DX9BC
#define BGFX_EMBEDDED_SHADER_DX9BC(...)
#endif

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "GraphicsHandleBgfx.h"
#include "SamplerDefaults.h"

// clang-format off
#define SHADER_NAME vs_line
#include "ShaderIncluder.h"
#define SHADER_NAME vs_line_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_line
#include "ShaderIncluder.h"

#define SHADER_NAME vs_object
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_hm_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_hm_static_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_few_bones_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_hm_few_bones_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_palette_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_morph_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_advisor_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_icon_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_creature
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_shadow
#include "ShaderIncluder.h"
#define SHADER_NAME fs_sky
#include "ShaderIncluder.h"

#define SHADER_NAME vs_terrain
#include "ShaderIncluder.h"
#define SHADER_NAME fs_terrain
#include "ShaderIncluder.h"

#define SHADER_NAME vs_water
#include "ShaderIncluder.h"
#define SHADER_NAME fs_water
#include "ShaderIncluder.h"

#define SHADER_NAME vs_sprite
#include "ShaderIncluder.h"
#define SHADER_NAME fs_sprite
#include "ShaderIncluder.h"

#define SHADER_NAME vs_celestial
#include "ShaderIncluder.h"
#define SHADER_NAME vs_blob
#include "ShaderIncluder.h"
#define SHADER_NAME fs_blob
#include "ShaderIncluder.h"
#define SHADER_NAME fs_world_textured
#include "ShaderIncluder.h"
#define SHADER_NAME fs_text
#include "ShaderIncluder.h"
#define SHADER_NAME vs_cloud
#include "ShaderIncluder.h"
#define SHADER_NAME fs_cloud
#include "ShaderIncluder.h"
#define SHADER_NAME fs_celestial
#include "ShaderIncluder.h"
#define SHADER_NAME vs_land_shadow
#include "ShaderIncluder.h"
#define SHADER_NAME fs_land_shadow
#include "ShaderIncluder.h"
#define SHADER_NAME vs_world_triangles
#include "ShaderIncluder.h"
#define SHADER_NAME vs_interface
#include "ShaderIncluder.h"
#define SHADER_NAME fs_interface
#include "ShaderIncluder.h"
#define SHADER_NAME fs_interface_text
#include "ShaderIncluder.h"
// The temple's (Graphics/RendererTemple.cpp)
#define SHADER_NAME vs_object_temple_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_lightmap_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_temple
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_lightmap
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_reflective_lightmap
#include "ShaderIncluder.h"
#define SHADER_NAME vs_object_temple_environment_instanced
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_temple_environment
#include "ShaderIncluder.h"
#define SHADER_NAME fs_reflection
#include "ShaderIncluder.h"
#define SHADER_NAME vs_beam
#include "ShaderIncluder.h"
#define SHADER_NAME fs_beam
#include "ShaderIncluder.h"
#define SHADER_NAME vs_text3d
#include "ShaderIncluder.h"
#define SHADER_NAME fs_vortex_mask
#include "ShaderIncluder.h"
#define SHADER_NAME fs_vortex_ring
#include "ShaderIncluder.h"
#define SHADER_NAME vs_particle_surface
#include "ShaderIncluder.h"
#define SHADER_NAME fs_particle_surface
#include "ShaderIncluder.h"
#define SHADER_NAME vs_volume_glow
#include "ShaderIncluder.h"
#define SHADER_NAME fs_particle
#include "ShaderIncluder.h"
#define SHADER_NAME fs_object_reflective
#include "ShaderIncluder.h"
// clang-format on

namespace openblack::graphics
{

struct ShaderDefinition
{
	const std::string_view name;
	const std::string_view vertexShaderName;
	const std::string_view fragmentShaderName;
};

// clang-format off: one entry a line
const std::array<bgfx::EmbeddedShader, 63> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(vs_line),
    BGFX_EMBEDDED_SHADER(vs_line_instanced), //
    BGFX_EMBEDDED_SHADER(fs_line),           //
    BGFX_EMBEDDED_SHADER(vs_object),
    BGFX_EMBEDDED_SHADER(vs_object_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_hm_instanced), //
    BGFX_EMBEDDED_SHADER(fs_object),
    BGFX_EMBEDDED_SHADER(fs_sky),
    BGFX_EMBEDDED_SHADER(fs_object_shadow), //
    BGFX_EMBEDDED_SHADER(vs_terrain),
    BGFX_EMBEDDED_SHADER(fs_terrain), //
    BGFX_EMBEDDED_SHADER(vs_water),
    BGFX_EMBEDDED_SHADER(fs_water), //
    BGFX_EMBEDDED_SHADER(vs_sprite),
    BGFX_EMBEDDED_SHADER(fs_sprite), //
    BGFX_EMBEDDED_SHADER(vs_celestial),
    BGFX_EMBEDDED_SHADER(fs_celestial), //
    BGFX_EMBEDDED_SHADER(vs_cloud),
    BGFX_EMBEDDED_SHADER(fs_cloud), //
    BGFX_EMBEDDED_SHADER(vs_land_shadow),
    BGFX_EMBEDDED_SHADER(fs_land_shadow), //
    BGFX_EMBEDDED_SHADER(vs_blob),
    BGFX_EMBEDDED_SHADER(fs_blob),
    BGFX_EMBEDDED_SHADER(fs_world_textured), //
    BGFX_EMBEDDED_SHADER(vs_object_static_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_hm_static_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_few_bones_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_hm_few_bones_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_palette_instanced), // Graphics/BonePalette.h
    BGFX_EMBEDDED_SHADER(vs_object_morph_instanced), // a creature's body
    BGFX_EMBEDDED_SHADER(fs_text),
    BGFX_EMBEDDED_SHADER(vs_world_triangles), // Graphics/WorldTriangles.h
    BGFX_EMBEDDED_SHADER(vs_interface),
    BGFX_EMBEDDED_SHADER(fs_interface),
    BGFX_EMBEDDED_SHADER(fs_interface_text),
    // the temple's
    BGFX_EMBEDDED_SHADER(vs_object_temple_instanced),
    BGFX_EMBEDDED_SHADER(vs_object_lightmap_instanced),
    BGFX_EMBEDDED_SHADER(fs_object_temple),
    BGFX_EMBEDDED_SHADER(fs_object_lightmap),
    BGFX_EMBEDDED_SHADER(fs_object_reflective_lightmap),
    BGFX_EMBEDDED_SHADER(vs_object_temple_environment_instanced),
    BGFX_EMBEDDED_SHADER(fs_object_temple_environment),
    BGFX_EMBEDDED_SHADER(fs_reflection),
    BGFX_EMBEDDED_SHADER(vs_beam),
    BGFX_EMBEDDED_SHADER(fs_beam),
    BGFX_EMBEDDED_SHADER(vs_text3d),
    // a landscape vortex's decal over vs_terrain (Graphics/RendererVortex.cpp)
    BGFX_EMBEDDED_SHADER(fs_vortex_mask),
    BGFX_EMBEDDED_SHADER(fs_vortex_ring),
    // a particle surface in one pass, its specular added after the texture (Graphics/RendererRevolvedSurface.cpp)
    BGFX_EMBEDDED_SHADER(vs_particle_surface),
    BGFX_EMBEDDED_SHADER(fs_particle_surface),
    // the advisors' bodies with their pupils (Graphics/RendererSpirits.cpp)
    BGFX_EMBEDDED_SHADER(vs_object_advisor_instanced),
    // the gesture icons' models, with a camera of their own (Graphics/RendererGestureIcons.cpp)
    BGFX_EMBEDDED_SHADER(vs_object_icon_instanced),
    // a creature's body and eyes with the creature spells' looks (Graphics/RendererCreature.cpp)
    BGFX_EMBEDDED_SHADER(fs_object_creature),
    // the glowing shell over a model, a worthy artefact's aura (Graphics/RendererParticles.cpp)
    BGFX_EMBEDDED_SHADER(vs_volume_glow),
    BGFX_EMBEDDED_SHADER(fs_particle),
    // a temple room's floor without a lightmap over the room's reflection (Graphics/RendererTemple.cpp)
    BGFX_EMBEDDED_SHADER(fs_object_reflective),
    BGFX_EMBEDDED_SHADER_END() //
}};
// clang-format on

constexpr std::array k_Shaders {
    ShaderDefinition {"DebugLine", "vs_line", "fs_line"},
    ShaderDefinition {"DebugLineInstanced", "vs_line_instanced", "fs_line"},
    ShaderDefinition {"Terrain", "vs_terrain", "fs_terrain"},
    ShaderDefinition {"Object", "vs_object", "fs_object"},
    ShaderDefinition {"ObjectInstanced", "vs_object_instanced", "fs_object"},
    ShaderDefinition {"ObjectHeightMapInstanced", "vs_object_hm_instanced", "fs_object"},
    ShaderDefinition {"ObjectShadowInstanced", "vs_object_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectHeightMapShadowInstanced", "vs_object_hm_instanced", "fs_object_shadow"},
    ShaderDefinition {"Sky", "vs_object", "fs_sky"},
    ShaderDefinition {"Water", "vs_water", "fs_water"},
    ShaderDefinition {"Sprite", "vs_sprite", "fs_sprite"},
    ShaderDefinition {"Celestial", "vs_celestial", "fs_celestial"},
    ShaderDefinition {"Cloud", "vs_cloud", "fs_cloud"},
    // a projected shadow over a land block (graphics::shadow_list)
    ShaderDefinition {"LandShadow", "vs_land_shadow", "fs_land_shadow"},
    ShaderDefinition {"Blob", "vs_blob", "fs_blob"},
    ShaderDefinition {"ObjectStaticInstanced", "vs_object_static_instanced", "fs_object"},
    ShaderDefinition {"ObjectHeightMapStaticInstanced", "vs_object_hm_static_instanced", "fs_object"},
    ShaderDefinition {"ObjectShadowInstancedStatic", "vs_object_static_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectHeightMapShadowInstancedStatic", "vs_object_hm_static_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectFewBonesInstanced", "vs_object_few_bones_instanced", "fs_object"},
    ShaderDefinition {"ObjectHeightMapFewBonesInstanced", "vs_object_hm_few_bones_instanced", "fs_object"},
    ShaderDefinition {"ObjectShadowInstancedB32", "vs_object_few_bones_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectHeightMapShadowInstancedB32", "vs_object_hm_few_bones_instanced", "fs_object_shadow"},
    ShaderDefinition {"ObjectPaletteInstanced", "vs_object_palette_instanced", "fs_object"},
    ShaderDefinition {"ObjectMorphInstanced", "vs_object_morph_instanced", "fs_object"},
    ShaderDefinition {"WorldTextured", "vs_blob", "fs_world_textured"},
    ShaderDefinition {"Text", "vs_blob", "fs_text"},
    ShaderDefinition {"WorldTriangles", "vs_world_triangles", "fs_object"},
    ShaderDefinition {"Interface", "vs_interface", "fs_interface"},
    ShaderDefinition {"InterfaceText", "vs_interface", "fs_interface_text"},
    // The temple's, last, so that the programs made before them keep their handles (Graphics/RendererTemple.cpp): its
    // rooms without and with a lightmap, the main room's floor over the reflection, the pool's reflection, the beams of
    // light and the glows, the text written in the rooms, and the map and the black floor under the temple
    ShaderDefinition {"ObjectTempleInstanced", "vs_object_temple_instanced", "fs_object_temple"},
    ShaderDefinition {"ObjectLightmapInstanced", "vs_object_lightmap_instanced", "fs_object_lightmap"},
    ShaderDefinition {"ObjectReflectiveLightmapInstanced", "vs_object_lightmap_instanced", "fs_object_reflective_lightmap"},
    ShaderDefinition {"Reflection", "vs_object_temple_instanced", "fs_reflection"},
    ShaderDefinition {"Beam", "vs_beam", "fs_beam"},
    ShaderDefinition {"Text3D", "vs_text3d", "fs_text"},
    ShaderDefinition {"Textured3D", "vs_text3d", "fs_interface"},
    // A landscape vortex's decal over the land's vertex program (Graphics/RendererVortex.cpp), after every other one so
    // that those keep their handles: its depth mask and its ring
    ShaderDefinition {"VortexDecalMask", "vs_terrain", "fs_vortex_mask"},
    ShaderDefinition {"VortexDecalRing", "vs_terrain", "fs_vortex_ring"},
    // A particle surface in one pass, the gesture's light sheet (Graphics/RendererRevolvedSurface.cpp), last so that the
    // others keep their handles
    ShaderDefinition {"ParticleSurface", "vs_particle_surface", "fs_particle_surface"},
    // The temple's meshes with an environment map added, as the creature cave's belts and medals past wood are
    // (Graphics/RendererTemple.cpp). Made last, so that the others keep their handles; not drawn with yet
    ShaderDefinition {"ObjectTempleEnvironmentInstanced", "vs_object_temple_environment_instanced",
                      "fs_object_temple_environment"},
    // The advisors' bodies with their pupils' texture moving over their eyes (Graphics/RendererSpirits.cpp). Made last,
    // so that the others keep their handles
    ShaderDefinition {"ObjectAdvisorInstanced", "vs_object_advisor_instanced", "fs_object"},
    // A creature fizzing out of sight, its body and its eyes, drawn through the static (Graphics/RendererCreature.cpp),
    // with the variants for the bone budgets of its submeshes. Made last, so that the others keep their handles
    ShaderDefinition {"ObjectMorphInstancedSpellLook", "vs_object_morph_instanced", "fs_object_creature"},
    ShaderDefinition {"ObjectInstancedSpellLook", "vs_object_instanced", "fs_object_creature"},
    ShaderDefinition {"ObjectStaticInstancedSpellLook", "vs_object_static_instanced", "fs_object_creature"},
    ShaderDefinition {"ObjectFewBonesInstancedSpellLook", "vs_object_few_bones_instanced", "fs_object_creature"},
    // The glowing shell laid over a model, as over a worthy artefact (Graphics/RendererParticles.cpp). Made last, so that
    // the others keep their handles
    ShaderDefinition {"VolumeGlow", "vs_volume_glow", "fs_particle"},
    // A temple room's floor without a lightmap over the room's reflection, as the picture rooms' floors are
    // (Graphics/RendererTemple.cpp). Made last, so that the others keep their handles
    ShaderDefinition {"ObjectReflectiveInstanced", "vs_object_temple_instanced", "fs_object_reflective"},
    // The gesture icons' models, drawn with a camera of their own and moved on the screen to their boxes
    // (Graphics/RendererGestureIcons.cpp). Made last, so that the others keep their handles
    ShaderDefinition {"ObjectIconInstanced", "vs_object_icon_instanced", "fs_object"},
};

namespace
{
/// The samplers a shader uses, read from its Vulkan build, which every desktop platform embeds whichever backend runs
std::vector<shader_samplers::Sampler> ReflectSamplers(std::string_view shaderName)
{
	for (const auto& shader : k_EmbeddedShaders)
	{
		if (shader.name == nullptr || shaderName != shader.name)
		{
			continue;
		}
		for (const auto& data : shader.data)
		{
			if (data.type == bgfx::RendererType::Vulkan && data.data != nullptr)
			{
				if (auto samplers = shader_samplers::ReadSpirvSamplers({data.data, data.size}))
				{
					return *std::move(samplers);
				}
			}
		}
	}
	SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Could not read the samplers of shader {}: they won't get defaults",
	                   shaderName);
	return {};
}

/// The samplers of both of a program's shaders
std::vector<shader_samplers::Sampler> SamplersOf(const ShaderDefinition& shader)
{
	return shader_samplers::Merge(ReflectSamplers(shader.vertexShaderName), ReflectSamplers(shader.fragmentShaderName));
}
} // namespace

ShaderManager::ShaderManager() = default;

ShaderManager::~ShaderManager()
{
	// Destroy the programs in name order before clearing; the map's own teardown order is not guaranteed
	for (auto& entry : _shaderPrograms)
	{
		entry.second.reset();
	}

	_shaderPrograms.clear();
	_samplerDefaults.reset();
}

void ShaderManager::LoadShaders()
{
	_samplerDefaults = std::make_unique<SamplerDefaults>();
	for (const auto& shader : k_Shaders)
	{
		bgfx::RendererType::Enum type = bgfx::getRendererType();
		auto vs = bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, shader.vertexShaderName.data());
		assert(bgfx::isValid(vs));
		auto fs = bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, shader.fragmentShaderName.data());
		assert(bgfx::isValid(fs));
		_shaderPrograms[shader.name.data()] = std::make_unique<ShaderProgram>(shader.name.data(), fromBgfx(vs), fromBgfx(fs),
		                                                                      SamplersOf(shader), *_samplerDefaults);
	}
}

std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> ShaderManager::ProgramSamplers()
{
	std::vector<std::pair<std::string, std::vector<shader_samplers::Sampler>>> programs;
	programs.reserve(k_Shaders.size());
	for (const auto& shader : k_Shaders)
	{
		programs.emplace_back(std::string(shader.name), SamplersOf(shader));
	}
	return programs;
}

const ShaderProgram* ShaderManager::GetShader(std::string_view name) const
{
	auto i = _shaderPrograms.find(name);
	if (i != _shaderPrograms.end())
	{
		return i->second.get();
	}

	// todo: return an empty shader?
	return nullptr;
}

void ShaderManager::DiscardBindings() const
{
	bgfx::discard(BGFX_DISCARD_BINDINGS);
	_samplerDefaults->Discarded(BGFX_DISCARD_BINDINGS);
}

void ShaderManager::FrameEnded() const
{
	_samplerDefaults->FrameEnded();
}

void ShaderManager::SetCamera(graphics::RenderPass viewId, const Camera& camera)
{
	auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	auto proj = camera.GetProjectionMatrix();
	bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), &view, &proj);
}

} // namespace openblack::graphics
