/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

namespace openblack::audio::device
{
/// The context's release: it stops being the current context first, if it is, then it is destroyed. It calls nothing
/// but the device API, so it is safe while the audio state itself is being destroyed.
template <typename Alc>
struct DestroyContext
{
	void operator()(typename Alc::Context* context) const noexcept
	{
		if (Alc::CurrentContext() == context)
		{
			Alc::MakeContextCurrent(nullptr);
		}
		Alc::DestroyContext(context);
	}
};

/// The device's release: it is closed
template <typename Alc>
struct CloseDevice
{
	void operator()(typename Alc::Device* device) const noexcept { Alc::CloseDevice(device); }
};

/// The wave device's owners. `Alc` is the device API: its Device and Context types and its CloseDevice,
/// DestroyContext, CurrentContext and MakeContextCurrent (OpenAL in Device.cpp, a fake in the tests); `Output` is
/// what plays on the context (the channels' sources).
///
/// The members are declared device, context, output, the order they are made in, so that they go in reverse whenever
/// the state is dropped: the output while its context is still current, then the context, then the device.
/// device::Close releases them one by one in that same order, with the wave buffers between the output and the context.
template <typename Alc, typename Output>
struct DeviceHandles
{
	std::unique_ptr<typename Alc::Device, CloseDevice<Alc>> device;
	std::unique_ptr<typename Alc::Context, DestroyContext<Alc>> context;
	/// The 16 channels' sources (made after the context, deleted before it)
	std::unique_ptr<Output> output;
};
} // namespace openblack::audio::device
