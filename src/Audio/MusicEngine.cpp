/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicEngine.h"

#include <algorithm>

namespace openblack::audio
{

namespace
{
// QMixer volume of one channel: ((cur * sad * 258) / 127) * master / 127 in unsigned 32-bit arithmetic, the two
// divisions by the magic 0x2040811 (0x1000F429..0x1000F464, the same at 0x1000E8F7 and 0x1000F53D..0x1000F664)
uint32_t MixerVolume(int current, int sadVolume, uint32_t master)
{
	const auto v = static_cast<uint32_t>(current) * static_cast<uint32_t>(sadVolume);
	const uint32_t scaled = (v * 258u) / 127u;
	return (scaled * master) / 127u;
}

// The marker clock of 0x1000DB90: (chunk << 32) | sample, compared as 64-bit values (0x1000DC13..0x1000DC52)
int64_t MarkerPosition(int chunk, uint32_t sample)
{
	return (static_cast<int64_t>(chunk) << 32) | static_cast<int64_t>(sample);
}
} // namespace

MusicEngine::MusicEngine(IMusicSink& sink)
    : _sink(sink)
{
	_sink.SetListener(this);
	// 0x1000DDF5..0x1000DF13: every channel free with sad volume 127, and its QMixer channel enabled in 2D (0x110)
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		_sink.EnableChannel(i, false, {}, glm::vec3(0.0f));
	}
	_installed = true;
	_active = true;
}

MusicEngine::~MusicEngine()
{
	_sink.SetListener(nullptr);
}

void MusicEngine::NoteBankRegistered(const MusicBank& bank)
{
	// LHBankRegister 0x100027AB..0x100027C7: sys+0x40 = max(sys+0x40, u16 @+0x118 of the first segment)
	if (bank.IsMusic() && bank.GetGroupId() >= 0)
	{
		_totalGroups = std::max(_totalGroups, static_cast<uint32_t>(bank.GetGroupId()));
	}
}

uint32_t MusicEngine::GetTotalGroups() const
{
	// 0x1000FB60: 0 when not installed
	return _installed ? _totalGroups : 0;
}

uint32_t MusicEngine::GetMixerVolume(int channel) const
{
	const auto& ch = _channels[static_cast<size_t>(channel)];
	return MixerVolume(ch.current, ch.sadVolume, _masterVolume);
}

void MusicEngine::ApplyVolume(int channel)
{
	_sink.SetVolume(channel, GetMixerVolume(channel));
}

int MusicEngine::Play(const MusicPlayOptions& options)
{
	// 0x1000DF6C..0x1000DFC6: not installed -> 0; no bank or not a music bank -> LHMusicStop(1) and 0
	if (!_installed)
	{
		return k_NoMusicChannel;
	}
	if (options.bank == nullptr || !options.bank->IsMusic())
	{
		Stop(1);
		return k_NoMusicChannel;
	}

	// 0x1000DFD4..0x1000DFEF: the channel already playing this bank is "re-triggered"
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		if (ch.bank != options.bank)
		{
			continue;
		}
		if (_master == i)
		{
			// 0x1000E00D..0x1000E04A: the master takes the new fade, target and sync and plays again
			ch.fade = options.fade;
			ch.target = options.volume;
			ch.sync = options.sync;
			ch.status = MusicStatus::Playing;
		}
		else if (ch.isNew == 0)
		{
			// 0x1000E06F..0x1000E0A5: an older channel becomes the master (the others fade out)
			ch.fade = options.fade;
			ch.target = options.volume;
			ch.sync = options.sync;
			_master = i;
		}
		// a channel that has not queued its first chunk yet is left as it is (0x1000E07E)
		return i;
	}

	// 0x1000DFF1..0x1000E008: the first free channel (bank 0), or 0 if all 6 are busy
	int index = k_NoMusicChannel;
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		if (_channels[static_cast<size_t>(i)].bank == nullptr)
		{
			index = i;
			break;
		}
	}
	if (index == k_NoMusicChannel)
	{
		return k_NoMusicChannel;
	}

	auto& ch = _channels[static_cast<size_t>(index)];
	auto& bank = *options.bank;
	ch.target = options.volume;              // 0x1000E0BB
	ch.sadVolume = k_MusicMaxVolume;         // 0x1000E0C5
	ch.fade = options.fade;                  // 0x1000E0D6
	ch.current = options.fade == 0 ? options.volume : 0; // 0x1000E0DA..0x1000E0F6: no fade = at the volume at once
	ch.pitch = options.pitch;                // 0x1000E103
	ch.bank = options.bank;                  // 0x1000E110
	ch.group = bank.GetGroupId();            // 0x1000E117 LHBankGetMusicGroupId
	ch.syncSample = 0;                       // 0x1000E12C
	ch.loops = options.loops;                // 0x1000E13E
	ch.isNew = 1;                            // 0x1000E148
	ch.is3D = options.is3D;                  // 0x1000E155
	ch.field18 = options.field08;            // 0x1000E161
	ch.field1C = options.field0C;            // 0x1000E16E
	ch.field20 = options.field10;            // 0x1000E17B
	ch.chunkCount = static_cast<uint32_t>(bank.GetSegmentCount()); // 0x1000E18C LHBankGetNumberOfSamples
	ch.savedStatus = MusicStatus::Free;      // 0x1000E1A1 (+0x28 = 0)
	ch.nextChunk = 1;                        // 0x1000E1AF
	ch.playingChunk = 1;                     // 0x1000E1B9
	ch.sadVolume = bank.GetVolume();         // 0x1000E1D6: u16 @+0x25C with flag 0x20
	ch.loops = bank.GetLoops(options.loops); // 0x1000E1F1: i32 @+0x248 with flag 0x40
	ch.sampleRate = bank.GetSampleRate();    // 0x1000E210

	// 0x1000E24C..0x1000E3F7: the QMixer channel, 3D with the bank's distances over the options' or 2D
	if (options.is3D == 1)
	{
		_sink.EnableChannel(index, true, bank.GetDistanceMapping(options.distance), options.position);
	}
	else
	{
		_sink.EnableChannel(index, false, options.distance, options.position);
	}

	ch.sync = options.sync != 0 ? 1 : 0; // 0x1000E3FF..0x1000E41B
	ch.startChunk = options.startChunk;  // 0x1000E42D
	ch.status = MusicStatus::Playing;    // 0x1000E437
	ch.resetDecoder = true;              // 0x1000E43B..0x1000E448: bank+0x138 +0x58 = 1
	ch.finished = options.finished;      // 0x1000E454
	ch.marker = options.marker;          // 0x1000E461
	ch.userData = options.userData;      // 0x1000E46E

	// 0x1000E472..0x1000E4E3: with a marker callback, a new marker list whose clock starts at max(startChunk, 1)
	if (options.marker)
	{
		ch.markers.reset();
		auto list = std::make_unique<MusicMarkerList>();
		list->nodes = bank.ParseMarkers();
		list->cursor = 0;
		list->previous = 0;
		list->startTick = 0;
		list->chunkBase = options.startChunk > 1 ? options.startChunk : 1;
		ch.markers = std::move(list);
	}
	return index;
}

void MusicEngine::Cut(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.bank = nullptr;              // +0x58 = 0
	ch.status = MusicStatus::Free;  // +0x24 = 0
	_sink.SetVolume(channel, 0);    // QSWaveMixSetVolume(0)
	if (!_sink.IsChannelDone(channel))
	{
		_sink.FlushChannel(channel);
	}
	ch.target = 0;                  // +0x30 = 0
}

void MusicEngine::Stop(int fade)
{
	// 0x1000E530: needs installed, active and the wave system
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		if (ch.status == MusicStatus::Free)
		{
			continue;
		}
		if (fade == 1)
		{
			ch.target = 0; // 0x1000E57A: the thread fades it out
			continue;
		}
		Cut(i);
		if (_master == i)
		{
			_master = k_NoMusicChannel; // 0x1000E5E6..0x1000E5EF
		}
	}
}

void MusicEngine::Stop(int channel, int fade)
{
	// 0x1000E620
	if (!IsUsable() || !IsValid(channel))
	{
		return;
	}
	if (_channels[static_cast<size_t>(channel)].status == MusicStatus::Free)
	{
		return;
	}
	if (_master == channel)
	{
		Stop(fade); // 0x1000E6B2: the master stops all of them
		return;
	}
	if (fade == 1)
	{
		return; // 0x1000E6C1: a fade of a channel that is not the master does nothing (it is already fading out)
	}
	Cut(channel); // 0x1000E6C3..0x1000E730 (the master index is not touched here)
}

int MusicEngine::GetInfo(const MusicBank* bank) const
{
	// 0x1000E750
	if (!IsUsable())
	{
		return k_NoMusicChannel;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		if (_channels[static_cast<size_t>(i)].bank == bank)
		{
			return i;
		}
	}
	return k_NoMusicChannel;
}

MusicStatus MusicEngine::GetStatus(int channel) const
{
	// 0x1000FC00
	if (!IsUsable() || !IsValid(channel))
	{
		return MusicStatus::Free;
	}
	return _channels[static_cast<size_t>(channel)].status;
}

uint32_t MusicEngine::GetCurrentChunk(int channel) const
{
	// 0x1000FB40: only needs installed
	if (!_installed || !IsValid(channel))
	{
		return 0;
	}
	return _channels[static_cast<size_t>(channel)].playingChunk;
}

int MusicEngine::GetMasterInfo() const
{
	// 0x1000FB70
	if (!_installed)
	{
		return k_NoMusicChannel;
	}
	return _master;
}

void MusicEngine::SetMasterVolume(uint32_t volume)
{
	// 0x1000E890: same value -> nothing; above 127 -> 127 (unsigned compare 0x1000E8C6); reapplied to the channels at
	// status 1 or 4 (0x1000E8ED..0x1000E8F5)
	if (!IsUsable() || volume == _masterVolume)
	{
		return;
	}
	_masterVolume = std::min<uint32_t>(volume, k_MusicMaxVolume);
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		const auto status = _channels[static_cast<size_t>(i)].status;
		if (status == MusicStatus::Playing || status == MusicStatus::LastQueued)
		{
			ApplyVolume(i);
		}
	}
}

int MusicEngine::GetMasterVolume() const
{
	// 0x1000E950
	if (!IsUsable())
	{
		return -1;
	}
	return static_cast<int>(_masterVolume);
}

void MusicEngine::SetPitch(int channel, uint32_t pitch)
{
	// 0x1000E970: same pitch -> nothing; clamped to 50..250 (unsigned); SetFrequency(Hz * pitch / 100) with flags 0
	if (!IsUsable() || !IsValid(channel))
	{
		return;
	}
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (pitch == static_cast<uint32_t>(ch.pitch))
	{
		return;
	}
	ch.pitch = static_cast<int>(std::clamp<uint32_t>(pitch, k_MusicMinPitch, k_MusicMaxPitch));
	_sink.SetFrequency(channel, ch.sampleRate * static_cast<uint32_t>(ch.pitch) / 100u);
}

void MusicEngine::Set3DPosition(int channel, glm::vec3 position)
{
	// 0x1000FBA0: only needs installed; a channel with a bank in 3D
	if (!_installed || !IsValid(channel))
	{
		return;
	}
	const auto& ch = _channels[static_cast<size_t>(channel)];
	if (ch.bank != nullptr && ch.is3D == 1)
	{
		_sink.SetSourcePosition(channel, position);
	}
}

void MusicEngine::Pause()
{
	// 0x1000EA10: every channel paused, its status kept in +0x28 and set to 2
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		_sink.PauseChannel(i);
		ch.savedStatus = ch.status;
		ch.status = MusicStatus::Paused;
	}
}

void MusicEngine::Restart()
{
	// 0x1000EA80: every channel restarted with the status of +0x28
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		_sink.RestartChannel(i);
		ch.status = ch.savedStatus;
	}
}

void MusicEngine::Switch(uint32_t on)
{
	// 0x1000EB00: installed and the wave system; 0 -> LHMusicStop(0) then inactive; 1 -> active
	if (!_installed)
	{
		return;
	}
	if (on == 0)
	{
		Stop(0);
		_active = false;
	}
	else if (on == 1)
	{
		_active = true;
	}
}

void MusicEngine::Close()
{
	// 0x1000E7A0: installed and the wave system
	if (!_installed)
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		_channels[static_cast<size_t>(i)].target = 0; // 0x1000E803
		_sink.SetVolume(i, 0);                        // 0x1000E81D
		if (!_sink.IsChannelDone(i))
		{
			_sink.FlushChannel(i); // 0x1000E84E
		}
	}
	_installed = false; // 0x1000E870
}

void MusicEngine::OnChunkDone(int channel, bool last)
{
	// 0x1000DC80: the wave is freed, the queued count drops; the last chunk ends the channel (status 3), any other moves
	// the audible chunk on (back to 1 after the last segment)
	if (!IsValid(channel))
	{
		return;
	}
	auto& ch = _channels[static_cast<size_t>(channel)];
	--_queued[static_cast<size_t>(channel)];
	if (last)
	{
		ch.status = MusicStatus::Finished;
		return;
	}
	if (ch.playingChunk == ch.chunkCount)
	{
		ch.playingChunk = 1;
	}
	else
	{
		++ch.playingChunk;
	}
}

void MusicEngine::DispatchMarkers(MusicChannel& channel, uint32_t nowMs)
{
	// 0x1000DB90
	auto& list = *channel.markers;
	if (list.cursor >= list.nodes.size())
	{
		return;
	}
	// samples played since the first chunk: ftol((GetTickCount - start) * Hz * 0.001f) (fild qword, fimul, fmul
	// [0x10030420] = 0.001f, 0x1001F874), here in double (inferred: the same result if the DLL's x87 runs at the 53-bit
	// precision the MSVC runtime sets by default)
	const uint32_t elapsed = nowMs - list.startTick;
	const auto hz = static_cast<double>(static_cast<int32_t>(channel.sampleRate));
	const auto samples = static_cast<int32_t>(static_cast<double>(elapsed) * hz * static_cast<double>(0.001f));
	// 0x1000DBD9..0x1000DC1D: chunk = samples / 0x5E80 + chunkBase, sample = samples % 0x5E80
	const int chunk = samples / k_MusicSamplesPerSegment + list.chunkBase;
	const auto sample = static_cast<uint32_t>(samples % k_MusicSamplesPerSegment);
	const int64_t position = MarkerPosition(chunk, sample);

	for (size_t i = list.cursor; i < list.nodes.size(); ++i)
	{
		const auto& node = list.nodes[i];
		if (node.chunk < chunk)
		{
			list.cursor = i; // 0x1000DC2E: the cursor moves to the last node of an earlier chunk
		}
		if (node.chunk > chunk)
		{
			break; // 0x1000DC36
		}
		const int64_t nodePosition = MarkerPosition(node.chunk, static_cast<uint32_t>(node.sample));
		// 0x1000DC38..0x1000DC63: fired when previous < node <= now
		if (nodePosition <= position && nodePosition > list.previous && channel.marker)
		{
			channel.marker(node.label);
		}
	}
	list.previous = position; // 0x1000DC6E / 0x1000DC71
}

bool MusicEngine::QueueChunks(int index, uint32_t nowMs)
{
	auto& ch = _channels[static_cast<size_t>(index)];
	auto& queued = _queued[static_cast<size_t>(index)];
	// 0x1000EC9E..0x1000ECD8: playing, active and fewer than 4 queued; the loop at 0x1000F4CD goes back to the active
	// check while fewer than 4 are queued
	if (ch.status != MusicStatus::Playing || !_active || queued == k_MusicQueueDepth)
	{
		return true;
	}
	while (_active)
	{
		int other = k_NoMusicChannel; // [ebp-0x10]
		if (ch.isNew != 0)
		{
			// 0x1000ECF3..0x1000ED8B: with sync, the first other channel playing (status 1) in the same group (not -1)
			// gives the chunk; otherwise the start chunk if it is 1..n (unsigned compare), else 1
			if (ch.sync == 1)
			{
				for (int j = 0; j < k_MusicChannelCount; ++j)
				{
					const auto& o = _channels[static_cast<size_t>(j)];
					if (j != index && o.status == MusicStatus::Playing && o.group == ch.group && o.group != -1)
					{
						ch.nextChunk = o.playingChunk;
						if (ch.markers)
						{
							ch.markers->chunkBase = static_cast<int>(o.playingChunk); // 0x1000ED4E
						}
						other = j;
						break;
					}
				}
			}
			if (other == k_NoMusicChannel)
			{
				const auto start = static_cast<uint32_t>(ch.startChunk);
				ch.nextChunk = (start <= ch.chunkCount && start != 0) ? start : 1;
			}
			ch.playingChunk = ch.nextChunk;
		}

		// (approximated) The original does not look at the status again inside this loop: after the last chunk
		// (status 4) with fewer than 4 queued, or when a master at status 4 is re-triggered to 1, it would read the
		// sample record n, past the end of the table. Here the end stays the end.
		if (ch.nextChunk == 0 || ch.nextChunk > ch.chunkCount)
		{
			ch.status = queued == 0 ? MusicStatus::Finished : MusicStatus::LastQueued;
			return true;
		}

		// 0x1000EDB6..0x1000EE19: the segment nextChunk - 1; past the end, loops == 0 makes it the last chunk (status 4),
		// otherwise one loop less and back to chunk 1
		const size_t segment = ch.nextChunk - 1;
		++ch.nextChunk;
		bool last = false;
		if (ch.nextChunk > ch.chunkCount)
		{
			if (ch.loops == 0)
			{
				last = true;
				ch.status = MusicStatus::LastQueued;
			}
			else
			{
				--ch.loops;
				ch.nextChunk = 1;
			}
		}

		// 0x1000EE19..0x1000EE64: read size bytes at LHAudioWaveData + offset from the open file; a failure ends the pass
		if (!ch.bank->ReadSegment(segment, _readBuffer))
		{
			return false;
		}

		// 0x1000F337..0x1000F3A8: the first chunk of a synced channel starts at the other channel's play position, and
		// the channel becomes the master
		uint32_t startSample = 0;
		if (ch.isNew != 0)
		{
			ch.isNew = 0;
			if (ch.sync == 1 && other != k_NoMusicChannel &&
			    _channels[static_cast<size_t>(other)].status == MusicStatus::Playing)
			{
				ch.syncSample = _sink.GetPlayPosition(other);
				startSample = ch.syncSample;
			}
			_master = index;
		}
		else
		{
			ch.syncSample = 0;
		}

		++queued; // 0x1000F3AB..0x1000F3C1
		if (ch.is3D == 0)
		{
			_sink.SetCentred(index); // 0x1000F3DB..0x1000F3F0
		}
		_sink.SetFrequency(index, ch.sampleRate * static_cast<uint32_t>(ch.pitch) / 100u); // 0x1000F3F6..0x1000F41D
		ApplyVolume(index);                                                                 // 0x1000F423..0x1000F472
		if (ch.markers && ch.markers->startTick == 0)
		{
			ch.markers->startTick = nowMs; // 0x1000F478..0x1000F49D
		}
		const bool resetDecoder = ch.resetDecoder;
		ch.resetDecoder = false; // 0x1000F0D4..0x1000F0E1 (+0x58 = 0)
		_sink.QueueChunk(index, _readBuffer, resetDecoder, startSample, last); // 0x1000F1F2 + 0x1000F4C4

		if (queued == k_MusicQueueDepth)
		{
			break; // 0x1000F4CD
		}
	}
	return true;
}

bool MusicEngine::Process(uint32_t nowMs)
{
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];

		// 0x1000EBBA..0x1000EBEE: the master's markers
		if ((ch.status == MusicStatus::Playing || ch.status == MusicStatus::LastQueued) && _master == i && _active &&
		    ch.markers)
		{
			DispatchMarkers(ch, nowMs);
		}

		// 0x1000EBF3..0x1000EC94: a finished channel is freed and calls back
		if (ch.status == MusicStatus::Finished)
		{
			ch.bank = nullptr;
			ch.status = MusicStatus::Free;
			if (ch.finished)
			{
				ch.finished(ch.userData);
			}
			_sink.SetVolume(i, 0);
			if (!_sink.IsChannelDone(i))
			{
				_sink.FlushChannel(i);
			}
			ch.target = 0;
			if (_master == i)
			{
				_master = k_NoMusicChannel;
			}
		}

		if (!QueueChunks(i, nowMs))
		{
			return false; // 0x1000EE64 -> 0x1000F6FA
		}

		// 0x1000F4DB..0x1000F6E2: fades, for the channels at status 1 or 4 while active (0x1000F4E8..0x1000F4FF)
		if ((ch.status != MusicStatus::Playing && ch.status != MusicStatus::LastQueued) || !_active)
		{
			continue;
		}
		if (ch.current < ch.target && _master == i)
		{
			if (ch.fade == 0)
			{
				ch.current = ch.target; // 0x1000F534: straight to the target, once
				ApplyVolume(i);
				ch.fade = 1; // 0x1000F592
			}
			else
			{
				ch.current = std::min(ch.current + k_MusicFadeInStep, ch.target); // 0x1000F59F..0x1000F5AB
				ApplyVolume(i);
			}
		}
		else if (ch.current > ch.target || _master != i)
		{
			// 0x1000F613..0x1000F620: towards 0, not towards the target
			ch.current = std::max(ch.current - k_MusicFadeOutStep, 0);
			ApplyVolume(i);
			if (ch.current == 0)
			{
				// 0x1000F682..0x1000F6E2
				if (_master == i)
				{
					_master = k_NoMusicChannel;
				}
				ch.status = MusicStatus::Free;
				ch.bank = nullptr;
				if (!_sink.IsChannelDone(i))
				{
					_sink.FlushChannel(i);
				}
			}
		}
	}
	return true;
}

} // namespace openblack::audio
