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

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/LH/MusicBank.h"

// LHMusic, the music engine of LHaudiodllR.dll: 6 channels (LH_MusicInfo, sys+0x84, 6 x 0x6C), one of them the master
// (0x10056284), fades of +4 / -3 per pass of the "music" thread 0x1000EB40 (one pass, then Sleep(120)), streamed
// segments with up to 4 queued per channel (0x1005628C), sync groups, markers and the callbacks of the caller.
// The logic is the DLL's; what QMixer does (the channel, its wave queue and the MPEG decoder of the bank) is behind
// IMusicSink, so that the engine runs without OpenAL (test_music_engine) and over OpenAL in the game (MusicStream).
// Sources: dev\tmp_dis\audio\music.md §2.3 and the disassembly in music_dll_play.txt, music_dll_thread.txt and
// music_dll_stop_etc.txt (cited at each step). The engine has no lock of its own: the caller serialises the calls, as
// the critical section 0x100562B0 does in the DLL (MusicSystem in MusicStream.h).

namespace openblack::audio
{

/// sys+0x84: 6 channels of 0x6C bytes (malloc 0x288 at 0x1000DDD4; every loop stops at 0x288 or at 6)
inline constexpr int k_MusicChannelCount = 6;
/// The thread queues while fewer than 4 chunks are queued on the channel (0x1000ECC1, 0x1000F4CD)
inline constexpr int k_MusicQueueDepth = 4;
/// Sleep(0x78) after a pass (0x1000F726), Sleep(0x1388) after a failed read (0x1000F71F)
inline constexpr uint32_t k_MusicPassSleepMs = 0x78;
inline constexpr uint32_t k_MusicRetrySleepMs = 0x1388;
/// Fade steps per pass: +4 for the master up to its target (0x1000F59F), -3 for the others or above the target
/// (0x1000F613)
inline constexpr int k_MusicFadeInStep = 4;
inline constexpr int k_MusicFadeOutStep = 3;
/// Volumes are 0..127 (0x7F: LH_MusicPlayOptions +0x04 0x1000D982, sad volume 0x1000E0C5, master 0x1000DE08)
inline constexpr int k_MusicMaxVolume = 0x7F;
/// LHMusicSetPitch 0x1000E970 clamps to 50..250 (0x1000E9AC / 0x1000E9BB)
inline constexpr int k_MusicMinPitch = 0x32;
inline constexpr int k_MusicMaxPitch = 0xFA;
/// No channel (the null LH_MusicInfo* of the DLL)
inline constexpr int k_NoMusicChannel = -1;

/// LH_MusicInfo +0x24 (0 free, 1 playing, 2 paused, 3 finished, 4 last chunk queued; music.md §2.3)
enum class MusicStatus : int
{
	Free = 0,
	Playing = 1,
	Paused = 2,
	Finished = 3,
	LastQueued = 4,
};

/// LH_MusicPlayOptions (0x50 bytes). The defaults are those of its constructor 0x1000D970.
struct MusicPlayOptions
{
	MusicBank* bank {nullptr}; ///< +0x00 (0)
	int volume {0x7F};         ///< +0x04: the target volume 0..127
	int field08 {0};           ///< +0x08 -> channel +0x18 (no use seen)
	int field0C {0};           ///< +0x0C -> channel +0x1C (no use seen)
	int field10 {0};           ///< +0x10 -> channel +0x20 (no use seen)
	int startChunk {1};        ///< +0x14: 1-based; 0 or > number of segments starts at 1 (0x1000ED6A..0x1000ED77)
	int loops {0};             ///< +0x18: replaced by the .sad's with flag 0x40; -1 = forever
	int sync {0};              ///< +0x1C: start where another channel of the same group plays
	int fade {0};              ///< +0x20: fade in (0 = straight to the volume)
	int is3D {0};              ///< +0x24
	int pitch {0x64};          ///< +0x28: percent
	MusicDistanceMapping distance {10.0f, 100.0f, 1.0f}; ///< +0x2C / +0x30 / +0x34 (0x41200000, 0x42C80000, 0x3F800000)
	glm::vec3 position {0.0f};                           ///< +0x38..+0x40
	std::function<void(int userData)> finished;          ///< +0x44: called with +0x4C when the last chunk has played
	std::function<void(std::string_view label)> marker;  ///< +0x48: called with each marker's label (0x1000DB90)
	int userData {0};                                    ///< +0x4C
};

/// The marker list of a channel (channel +0x68): the 0x18 bytes header made by 0x1000DB30 over the nodes of 0x1000D9E0
struct MusicMarkerList
{
	std::vector<MusicMarker> nodes; ///< +0x00 head (the order of the linked list)
	size_t cursor {0};              ///< +0x04: first node still worth looking at (nodes.size() = null)
	int64_t previous {0};           ///< +0x08 / +0x0C: last position, (chunk << 32) | sample
	uint32_t startTick {0};         ///< +0x10: GetTickCount when the first chunk was queued (0x1000F48D)
	int chunkBase {1};              ///< +0x14: the chunk the clock starts from
};

/// One LH_MusicInfo (0x6C bytes)
struct MusicChannel
{
	int is3D {0};                     ///< +0x00
	int sync {0};                     ///< +0x04
	int isNew {0};                    ///< +0x08: no chunk queued yet
	int fade {0};                     ///< +0x0C: with 0 the next raise jumps to the target and sets it to 1
	int loops {0};                    ///< +0x10: loops left (-1 = forever)
	int group {0};                    ///< +0x14: LHBankGetMusicGroupId
	int field18 {0};                  ///< +0x18 (opts +0x08)
	int field1C {0};                  ///< +0x1C (opts +0x0C)
	int field20 {0};                  ///< +0x20 (opts +0x10)
	MusicStatus status {MusicStatus::Free}; ///< +0x24
	MusicStatus savedStatus {MusicStatus::Free}; ///< +0x28: the status before LHMusicPause (0x1000EA5C)
	int sadVolume {0x7F};             ///< +0x2C: 127 or the .sad's with flag 0x20
	int target {0};                   ///< +0x30
	int current {0};                  ///< +0x34
	int pitch {0x64};                 ///< +0x38
	int startChunk {1};               ///< +0x3C
	uint32_t nextChunk {1};           ///< +0x40: next chunk to read (1-based)
	uint32_t syncSample {0};          ///< +0x44: play position of the synced channel (QMixer lStart)
	uint32_t playingChunk {1};        ///< +0x48: the audible chunk (advanced by the end of chunk callback)
	uint32_t chunkCount {0};          ///< +0x4C: number of segments
	uint32_t sampleRate {0};          ///< +0x50: Hz of the first segment
	MusicBank* bank {nullptr};        ///< +0x58 (0 = free)
	std::function<void(int)> finished;            ///< +0x5C
	std::function<void(std::string_view)> marker; ///< +0x60
	int userData {0};                             ///< +0x64
	std::unique_ptr<MusicMarkerList> markers;     ///< +0x68
	/// LH_AudioBank +0x138 +0x58 (the bank's decoder): LHMusicPlay asks for a fresh decoder (0x1000E448) and the next
	/// read rebuilds it (0x1000F0CF..0x1000F1C5). Kept on the channel: a bank plays on one channel at a time.
	bool resetDecoder {false};
};

/// Receives the end of each queued chunk: the QMixer callback 0x1000DC80 of QSWaveMixPlayEx (0x1000F314)
class IMusicChunkListener
{
public:
	virtual ~IMusicChunkListener() = default;
	virtual void OnChunkDone(int channel, bool last) = 0;
};

/// What LHMusic asks of QMixer for its 6 channels (sys+0xCC + i), and the bank's MPEG decoder (0x1000F740)
class IMusicSink
{
public:
	virtual ~IMusicSink() = default;
	/// Where the end of chunk callbacks go. FlushChannel calls it for each flushed chunk before returning: LHMusicClose
	/// spins on the queued count right after the flush (0x1000E854..0x1000E869), so QMixer notifies inside the flush
	/// (QSWaveMixFlushChannel 0x18001810 -> 0x18006294 with notify = 1 -> 0x1800A380) (inferred from the spin).
	virtual void SetListener(IMusicChunkListener* listener) = 0;
	/// QSWaveMixEnableChannel 2D (0x100, 0x1000E3F7) or 3D (0x110/0x111 | 0x100, 0x1000E2D1) with, in 3D,
	/// QSWaveMixSetDistanceMapping (0x1000E355) and QSWaveMixSetSourcePosition (0x1000E38A)
	virtual void EnableChannel(int channel, bool is3D, const MusicDistanceMapping& mapping, glm::vec3 position) = 0;
	/// Decode one segment (with a fresh decoder if resetDecoder) and queue it (QSWaveMixOpenWaveEx 0x1000F2FF +
	/// QSWaveMixPlayEx(0x400 = queue) 0x1000F4C4) starting at startSample (QMIXPLAYPARAMS lStart, 0x1000F395);
	/// last is the callback data +4 (0x1000F326)
	virtual void QueueChunk(int channel, const std::vector<uint8_t>& segment, bool resetDecoder, uint32_t startSample,
	                        bool last) = 0;
	/// QSWaveMixSetPolarPosition(0, 0, 0) of a 2D channel before each chunk (0x1000F3F0)
	virtual void SetCentred(int channel) = 0;
	/// QSWaveMixSetFrequency (0x1000F41D, 0x1000E9EF)
	virtual void SetFrequency(int channel, uint32_t hz) = 0;
	/// QSWaveMixSetVolume, 0..32766 (0x1000F472...)
	virtual void SetVolume(int channel, uint32_t volume) = 0;
	/// QSWaveMixIsChannelDone (0x1000EC5A...)
	[[nodiscard]] virtual bool IsChannelDone(int channel) = 0;
	/// QSWaveMixFlushChannel (0x1000EC77...)
	virtual void FlushChannel(int channel) = 0;
	/// QSWaveMixGetPlayPosition(channel, ..., 1) (0x1000F385): the position in the playing wave (samples, inferred)
	[[nodiscard]] virtual uint32_t GetPlayPosition(int channel) = 0;
	/// QSWaveMixSetSourcePosition (0x1000FBE8)
	virtual void SetSourcePosition(int channel, glm::vec3 position) = 0;
	/// QSWaveMixPauseChannel / QSWaveMixRestartChannel (0x1000EA46 / 0x1000EAB6)
	virtual void PauseChannel(int channel) = 0;
	virtual void RestartChannel(int channel) = 0;
};

/// LH_AudioSystem's music part (LHMusic*). Channels are numbered 0..5 (the LH_MusicInfo* of the DLL).
class MusicEngine final: public IMusicChunkListener
{
public:
	/// The music part of LH_AudioSystem init 0x1000DD50: master volume 127 (0x1000DE08), no master (0x1000DDFE), every
	/// channel free, 2D enabled (0x1000DEF1), installed (+0x1C) and active (+0x24) (0x1000DF43/0x1000DF46)
	explicit MusicEngine(IMusicSink& sink);
	MusicEngine(const MusicEngine&) = delete;
	MusicEngine& operator=(const MusicEngine&) = delete;
	~MusicEngine() override;

	/// LHMusicPlay 0x1000DF60: the channel playing the options' bank or a new one; k_NoMusicChannel without a music bank
	/// (after LHMusicStop(1)) or without a free channel
	int Play(const MusicPlayOptions& options);
	/// LHMusicStop(int) 0x1000E530: fade == 1 sets every target to 0 (they fade out), otherwise they are cut
	void Stop(int fade);
	/// LHMusicStop(LH_MusicInfo*, int) 0x1000E620: one channel; the master stops them all (0x1000E6B2)
	void Stop(int channel, int fade);
	/// LHMusicGetInfo 0x1000E750: the channel playing that bank
	[[nodiscard]] int GetInfo(const MusicBank* bank) const;
	/// LHMusicGetStatus 0x1000FC00
	[[nodiscard]] MusicStatus GetStatus(int channel) const;
	/// LHMusicGetCurrentChunk 0x1000FB40: +0x48, the audible chunk (0 without a channel)
	[[nodiscard]] uint32_t GetCurrentChunk(int channel) const;
	/// LHMusicGetMasterInfo 0x1000FB70
	[[nodiscard]] int GetMasterInfo() const;
	/// LHMusicGetTotalGroups 0x1000FB60: sys+0x40, the highest group of the registered banks (LHBankRegister
	/// 0x100027AB..0x100027C7, which the caller reports here)
	[[nodiscard]] uint32_t GetTotalGroups() const;
	void NoteBankRegistered(const MusicBank& bank);
	/// LHMusicSetMasterVolume 0x1000E890 (unsigned argument: above 127, or negative, gives 127)
	void SetMasterVolume(uint32_t volume);
	/// LHMusicGetMasterVolume 0x1000E950 (-1 when not active)
	[[nodiscard]] int GetMasterVolume() const;
	/// LHMusicSetPitch 0x1000E970
	void SetPitch(int channel, uint32_t pitch);
	/// LHMusicSet3DPosition 0x1000FBA0
	void Set3DPosition(int channel, glm::vec3 position);
	/// LHMusicPause 0x1000EA10 / LHMusicRestart 0x1000EA80
	void Pause();
	void Restart();
	/// LHMusicSwitch 0x1000EB00: 0 = LHMusicStop(0) and inactive, 1 = active again (nothing restarts)
	void Switch(uint32_t on);
	/// The channel part of LHMusicClose 0x1000E7A0 (after the thread has ended, 0x1000E7BC..0x1000E7DB): every target to
	/// 0, volume 0 and flush (the DLL then spins until nothing is queued, 0x1000E854); no longer installed (0x1000E870)
	void Close();
	/// LHMusicIsInstalled 0x1000EAE0 / LHMusicIsActive 0x1000EAF0
	[[nodiscard]] bool IsInstalled() const { return _installed; }
	[[nodiscard]] bool IsActive() const { return _active; }

	/// One pass of the music thread 0x1000EB40 over the 6 channels at GetTickCount() = nowMs. Returns false when a
	/// segment could not be read: the pass stops there and the thread sleeps k_MusicRetrySleepMs instead of
	/// k_MusicPassSleepMs (0x1000F6FA..0x1000F728).
	bool Process(uint32_t nowMs);

	/// 0x1000DC80, the QMixer callback at the end of each queued chunk
	void OnChunkDone(int channel, bool last) override;

	[[nodiscard]] const MusicChannel& GetChannel(int channel) const { return _channels[static_cast<size_t>(channel)]; }
	/// 0x1005628C[i]: chunks queued on the channel
	[[nodiscard]] int GetQueued(int channel) const { return _queued[static_cast<size_t>(channel)]; }
	/// The volume sent to QSWaveMixSetVolume: floor(floor(cur * sad * 258 / 127) * master / 127) (0x1000F423..0x1000F464)
	[[nodiscard]] uint32_t GetMixerVolume(int channel) const;

private:
	[[nodiscard]] bool IsValid(int channel) const { return channel >= 0 && channel < k_MusicChannelCount; }
	/// The checks of most entry points: sys+0x1C (installed), sys+0x24 (active), sys+4 (the wave system, always there)
	[[nodiscard]] bool IsUsable() const { return _installed && _active; }
	void ApplyVolume(int channel);
	/// The cut of LHMusicStop(0) for one channel (0x1000E57F..0x1000E5F3, 0x1000E6C3..0x1000E730)
	void Cut(int channel);
	/// 0x1000DB90: fire the markers between the last position and the clock's
	void DispatchMarkers(MusicChannel& channel, uint32_t nowMs);
	/// The queueing loop of one channel (0x1000EC9E..0x1000F4D5); false when a read failed
	bool QueueChunks(int index, uint32_t nowMs);

	IMusicSink& _sink;
	std::array<MusicChannel, k_MusicChannelCount> _channels;
	std::array<int, k_MusicChannelCount> _queued {}; ///< 0x1005628C
	int _master {k_NoMusicChannel};                   ///< 0x10056284
	uint32_t _masterVolume {0x7F};                    ///< 0x10056280
	uint32_t _totalGroups {0};                        ///< sys+0x40
	bool _installed {false};                          ///< sys+0x1C
	bool _active {false};                             ///< sys+0x24
	std::vector<uint8_t> _readBuffer;                 ///< the malloc(size + 4) of each read (0x1000EE23)
};

} // namespace openblack::audio
