/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string>
#include <vector>

#include "GraphicsHandle.h"
#include "ShaderSamplers.h"
#include "UniformTable.h"

namespace openblack::graphics
{
class SamplerDefaults;
class Texture2D;

class ShaderProgram
{
public:
	enum class Type
	{
		Vertex,
		Fragment,
		Compute,
	};

	ShaderProgram() = delete;
	/// The samplers are those the program's shaders declare, which it keeps bound on every submit with the defaults
	ShaderProgram(const std::string& name, ShaderHandle vertexShader, ShaderHandle fragmentShader,
	              std::vector<shader_samplers::Sampler> samplers, SamplerDefaults& samplerDefaults);
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;
	~ShaderProgram();

	/// flags: bgfx sampler flags, UINT32_MAX = the texture's own
	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const Texture2D& texture,
	                       uint32_t flags = UINT32_MAX) const;
	void SetTextureSampler(const char* samplerName, uint8_t bindPoint, const graphics::TextureHandle& texture) const;
	/// num: array elements (1 for plain uniforms)
	void SetUniformValue(const char* uniformName, const void* value, uint16_t num = 1) const;

	/// Draws with this program in the given view. Each sampler it declares that no texture was set for since the
	/// bindings were last discarded gets a white texture of its kind first, so that no sampler is left unbound.
	void Submit(uint16_t viewId, uint32_t depth = 0, uint8_t discardFlags = k_DiscardAll) const;

	[[nodiscard]] ProgramHandle GetRawHandle() const { return _program; }

	/// bgfx's BGFX_DISCARD_ALL, everything set for a draw dropped after it
	static constexpr uint8_t k_DiscardAll = 0xFF;

private:
	std::string _name;
	ProgramHandle _program;
	UniformTable _uniforms;
	std::vector<shader_samplers::Sampler> _samplers;
	SamplerDefaults& _samplerDefaults;
};

} // namespace openblack::graphics
