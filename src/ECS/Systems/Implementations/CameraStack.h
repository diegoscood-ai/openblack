/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "ECS/Systems/CameraStackInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class CameraStack final: public CameraStackInterface
{
public:
	CameraStack();
	~CameraStack() override;
	CameraStack(const CameraStack&) = delete;
	CameraStack& operator=(const CameraStack&) = delete;
	CameraStack(CameraStack&&) = delete;
	CameraStack& operator=(CameraStack&&) = delete;

	[[nodiscard]] camera_stack::ModeStack& Modes() override { return _modes; }
	[[nodiscard]] const camera_stack::ModeStack& Modes() const override { return _modes; }
	void Keep(uint32_t id, std::unique_ptr<CameraModel> model) override;
	[[nodiscard]] std::unique_ptr<CameraModel> TakeKept(uint32_t id) override;
	void DropKept(uint32_t id) override;
	void DropLost() override;
	[[nodiscard]] std::size_t KeptCount() const override { return _kept.size(); }
	void Clear() override;

private:
	/// A camera's model kept for a new player's mode, by its number
	struct KeptModel
	{
		uint32_t id = 0;
		std::unique_ptr<CameraModel> model;
	};

	camera_stack::ModeStack _modes;
	std::vector<KeptModel> _kept; ///< In the order they were kept
};

} // namespace openblack::ecs::systems
