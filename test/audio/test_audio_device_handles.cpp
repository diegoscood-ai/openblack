/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Device/DeviceHandles.h"

// The wave device's owners (audio::device::DeviceHandles) over a fake device API: what is released, and in which
// order, when the state is dropped without device::Close, and when Close releases the members one by one. No audio
// device is needed.

using namespace openblack::audio;

namespace
{
/// What the fake device API and the fake output were asked to do, in order
std::vector<std::string> g_Calls;

struct FakeDevice
{
};
struct FakeContext
{
};
/// The one device and context the fake API "opens": the releases only record, nothing is freed
FakeDevice g_Device;
FakeContext g_Context;

/// The device API's four calls, recorded; one context may be current
struct FakeAlc
{
	using Device = FakeDevice;
	using Context = FakeContext;
	static inline Context* s_Current {nullptr};

	static void CloseDevice(Device* device) noexcept
	{
		g_Calls.emplace_back(device == &g_Device ? "close device" : "close another device");
	}
	static void DestroyContext(Context* context) noexcept
	{
		g_Calls.emplace_back(context == s_Current ? "destroy current context" : "destroy context");
	}
	static Context* CurrentContext() noexcept { return s_Current; }
	static void MakeContextCurrent(Context* context) noexcept
	{
		g_Calls.emplace_back(context == nullptr ? "no current context" : "make context current");
		s_Current = context;
	}
};

/// The channels' sources: their deletion is recorded, with whether the context was still current
struct FakeOutput
{
	FakeOutput() = default;
	FakeOutput(const FakeOutput&) = delete;
	FakeOutput& operator=(const FakeOutput&) = delete;
	FakeOutput(FakeOutput&&) = delete;
	FakeOutput& operator=(FakeOutput&&) = delete;
	~FakeOutput() { g_Calls.emplace_back(FakeAlc::s_Current != nullptr ? "delete sources" : "delete sources, no context"); }
};

using Handles = device::DeviceHandles<FakeAlc, FakeOutput>;

/// As device::Open: the device, its context made current, the output
void Open(Handles& handles)
{
	handles.device.reset(&g_Device);
	handles.context.reset(&g_Context);
	FakeAlc::MakeContextCurrent(handles.context.get());
	handles.output = std::make_unique<FakeOutput>();
	g_Calls.clear();
}

class DeviceHandlesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_Calls.clear();
		FakeAlc::s_Current = nullptr;
	}
	void TearDown() override { FakeAlc::s_Current = nullptr; }
};
} // namespace

TEST_F(DeviceHandlesTest, DroppedWithoutCloseReleasesInReverse)
{
	{
		Handles handles;
		Open(handles);
	}
	const std::vector<std::string> expected = {"delete sources", "no current context", "destroy context", "close device"};
	EXPECT_EQ(g_Calls, expected);
	EXPECT_EQ(FakeAlc::s_Current, nullptr);
}

TEST_F(DeviceHandlesTest, CloseOrderThenTheDropDoesNothing)
{
	{
		Handles handles;
		Open(handles);
		// device::Close: the output, (the wave buffers,) the context, the device
		handles.output.reset();
		handles.context.reset();
		handles.device.reset();
		const std::vector<std::string> expected = {"delete sources", "no current context", "destroy context", "close device"};
		EXPECT_EQ(g_Calls, expected);
		g_Calls.clear();
	}
	EXPECT_TRUE(g_Calls.empty());
}

TEST_F(DeviceHandlesTest, AContextNotCurrentIsOnlyDestroyed)
{
	// device::Open's failure path: a context made but never made current
	Handles handles;
	handles.device.reset(&g_Device);
	handles.context.reset(&g_Context);
	handles.context.reset();
	handles.device.reset();
	const std::vector<std::string> expected = {"destroy context", "close device"};
	EXPECT_EQ(g_Calls, expected);
}

TEST_F(DeviceHandlesTest, NothingOpenNothingReleased)
{
	{
		const Handles handles;
	}
	EXPECT_TRUE(g_Calls.empty());
}
