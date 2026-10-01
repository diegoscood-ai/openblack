/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Ocean.h"

#include <bgfx/bgfx.h>
#include <glm/vec2.hpp>

#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"

using namespace openblack;
using namespace openblack::graphics;

Ocean::Ocean() noexcept
{
	_reflectionFrameBuffer =
	    std::make_unique<FrameBuffer>("Reflection", static_cast<uint16_t>(1024), static_cast<uint16_t>(1024),
	                                  graphics::TextureFormat::RGBA8, graphics::TextureFormat::Depth24Stencil8);
	CreateMesh();
}
Ocean::~Ocean() noexcept = default;

void Ocean::ResizeReflectionFramebuffer(uint16_t width, uint16_t height)
{
	uint16_t currentWidth = 0;
	uint16_t currentHeight = 0;
	_reflectionFrameBuffer->GetSize(currentWidth, currentHeight);
	if (width == 0 || height == 0 || (width == currentWidth && height == currentHeight))
	{
		return;
	}
	// The original draws what is under the sea straight into the frame (GLandscape::Draw 0x5E48AE..0x5E4E6B)
	_reflectionFrameBuffer = std::make_unique<FrameBuffer>("Reflection", width, height, graphics::TextureFormat::RGBA8,
	                                                       graphics::TextureFormat::Depth24Stencil8);
}

void Ocean::CreateMesh()
{
	VertexDecl decl;
	decl.reserve(1);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(2), VertexAttrib::Type::Float);

	// fn_0087A090 (detail level 0): the quad of +-70000 (0x4788B800)
	static constexpr std::array<glm::vec2, 4> k_Points = {
	    glm::vec2(-70000.0f, 70000.0f),
	    glm::vec2(70000.0f, 70000.0f),
	    glm::vec2(70000.0f, -70000.0f),
	    glm::vec2(-70000.0f, -70000.0f),
	};

	static constexpr std::array<uint16_t, 6> k_Indices = {2, 1, 0, 0, 3, 2};

	const auto* mem = bgfx::makeRef(k_Points.data(), static_cast<uint32_t>(k_Points.size() * sizeof(k_Points[0])));
	auto* vertexBuffer = new VertexBuffer("Water", mem, decl);
	auto* indexBuffer =
	    new IndexBuffer("Water", k_Indices.data(), static_cast<uint32_t>(k_Indices.size()), IndexBuffer::Type::Uint16);

	_mesh = std::make_unique<Mesh>(vertexBuffer, indexBuffer, graphics::Mesh::Topology::TriangleList);

	// The screen rows (fn_00879930): the whole screen in clip space
	static constexpr std::array<glm::vec2, 4> k_ScreenPoints = {
	    glm::vec2(-1.0f, 1.0f),
	    glm::vec2(1.0f, 1.0f),
	    glm::vec2(1.0f, -1.0f),
	    glm::vec2(-1.0f, -1.0f),
	};
	const auto* screenMem =
	    bgfx::makeRef(k_ScreenPoints.data(), static_cast<uint32_t>(k_ScreenPoints.size() * sizeof(k_ScreenPoints[0])));
	auto* screenVertexBuffer = new VertexBuffer("WaterRows", screenMem, decl);
	auto* screenIndexBuffer =
	    new IndexBuffer("WaterRows", k_Indices.data(), static_cast<uint32_t>(k_Indices.size()), IndexBuffer::Type::Uint16);
	_screenMesh = std::make_unique<Mesh>(screenVertexBuffer, screenIndexBuffer, graphics::Mesh::Topology::TriangleList);
}
