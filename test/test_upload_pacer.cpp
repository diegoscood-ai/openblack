/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "Graphics/UploadPacer.h"

using openblack::graphics::UploadPacer;

namespace
{

/// Long enough for a thread that isn't held to have moved on
constexpr auto k_Settle = std::chrono::milliseconds(50);
/// How long a test waits for the upload thread to have started at all, on a loaded machine
constexpr auto k_StartDeadline = std::chrono::seconds(10);

/// Uploads 100 bytes twice through the pacer on a thread of its own, counting each that went
class PacedUploads
{
public:
	explicit PacedUploads(UploadPacer& pacer)
	    : _thread([this, &pacer] {
		    const UploadPacer::Scope paced(pacer);
		    UploadPacer::Pace(100);
		    ++_done;
		    UploadPacer::Pace(100);
		    ++_done;
	    })
	{
	}

	[[nodiscard]] int Done() const { return _done; }

	/// Waits for the first upload, which always goes, then long enough for a second that is not held to go too, so
	/// that a slow start of the thread is not taken for a held upload
	void WaitForTheFirst() const
	{
		const auto deadline = std::chrono::steady_clock::now() + k_StartDeadline;
		while (_done < 1 && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::yield();
		}
		std::this_thread::sleep_for(k_Settle);
	}

	void Join() { _thread.join(); }

private:
	std::atomic<int> _done {0};
	std::thread _thread;
};

} // namespace

TEST(UploadPacer, TheMainThreadIsNeverHeld)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 10});
	UploadPacer::Pace(1000);
	UploadPacer::Pace(1000);
	EXPECT_EQ(pacer.GetUploaded(), 0);
}

TEST(UploadPacer, BeforeTheFirstFrameEverythingGoes)
{
	UploadPacer pacer;
	PacedUploads uploads(pacer);
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
	EXPECT_EQ(pacer.GetUploaded(), 200);
}

TEST(UploadPacer, WhatDoesNotFitInAFrameWaitsForTheNext)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 150});
	PacedUploads uploads(pacer);
	// The first goes, as the first of a frame always does; the second would go over
	uploads.WaitForTheFirst();
	EXPECT_EQ(uploads.Done(), 1);
	pacer.BeginFrame({.bytes = 150});
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
	EXPECT_EQ(pacer.GetUploaded(), 100);
}

TEST(UploadPacer, MoreUploadsThanAFrameHasRoomForWaitForTheNext)
{
	UploadPacer pacer;
	// Room for every byte, but for only one buffer or texture
	pacer.BeginFrame({.bytes = 1000, .uploads = 1});
	PacedUploads uploads(pacer);
	uploads.WaitForTheFirst();
	EXPECT_EQ(uploads.Done(), 1);
	EXPECT_EQ(pacer.GetUploadCount(), 1);
	pacer.BeginFrame({.bytes = 1000, .uploads = 1});
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
	EXPECT_EQ(pacer.GetUploadCount(), 1);
}

TEST(UploadPacer, UploadsWithinBothAllowancesGoInOneFrame)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 200, .uploads = 2});
	PacedUploads uploads(pacer);
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
	EXPECT_EQ(pacer.GetUploaded(), 200);
	EXPECT_EQ(pacer.GetUploadCount(), 2);
}

TEST(UploadPacer, TheFirstUploadOfAFrameGoesHoweverLarge)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 10});
	PacedUploads uploads(pacer);
	uploads.WaitForTheFirst();
	EXPECT_EQ(uploads.Done(), 1);
	pacer.BeginFrame({.bytes = 10});
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
}

TEST(UploadPacer, HurryingLetsEverythingThrough)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 150});
	PacedUploads uploads(pacer);
	uploads.WaitForTheFirst();
	{
		const UploadPacer::Hurry hurry(pacer);
		uploads.Join();
	}
	EXPECT_EQ(uploads.Done(), 2);
}

TEST(UploadPacer, ReleasingLetsEverythingThrough)
{
	UploadPacer pacer;
	pacer.BeginFrame({.bytes = 150});
	PacedUploads uploads(pacer);
	uploads.WaitForTheFirst();
	pacer.Release();
	uploads.Join();
	EXPECT_EQ(uploads.Done(), 2);
}
