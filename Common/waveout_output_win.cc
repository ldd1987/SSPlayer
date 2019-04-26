//// Copyright (c) 2012 The Chromium Authors. All rights reserved.
//// Use of this source code is governed by a BSD-style license that can be
//// found in the LICENSE file.
//
#include <WinSock2.h>
#include "waveout_output_win.h"
#define PGMAUDIOCHANNEL 0 
//
//#include <windows.h>
//#include <mmsystem.h>
//#pragma comment(lib, "winmm.lib")
//
//#include "base/atomicops.h"
//#include "base/basictypes.h"
//#include "base/debug/trace_event.h"
//#include "base/logging.h"
//#include "media/audio/audio_io.h"
//#include "media/audio/win/audio_manager_win.h"
//
//namespace media {
//
//// Some general thoughts about the waveOut API which is badly documented :
//// - We use CALLBACK_EVENT mode in which XP signals events such as buffer
////   releases.
//// - We use RegisterWaitForSingleObject() so one of threads in thread pool
////   automatically calls our callback that feeds more data to Windows.
//// - Windows does not provide a way to query if the device is playing or paused
////   thus it forces you to maintain state, which naturally is not exactly
////   synchronized to the actual device state.
//
//// Sixty four MB is the maximum buffer size per AudioOutputStream.
static const unsigned int kMaxOpenBufferSize = 1024 * 1024 * 64;
static const int kMaxAudioQueueSize = 10;
static const int kMaxPGMAudioQueueSize = 4;
static const int kIdealSize = 6144;
//
//// See Also
//// http://www.thx.com/consumer/home-entertainment/home-theater/surround-sound-speaker-set-up/
//// http://en.wikipedia.org/wiki/Surround_sound
//
static const int kMaxChannelsToMask = 8;
static const unsigned int kChannelsToMask[kMaxChannelsToMask + 1] =
{
    0,
    // 1 = Mono
    SPEAKER_FRONT_CENTER,
    // 2 = Stereo
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT,
    // 3 = Stereo + Center
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER,
    // 4 = Quad
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
    // 5 = 5.0
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
    // 6 = 5.1
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
    SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
    // 7 = 6.1
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
    SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
    SPEAKER_BACK_CENTER,
    // 8 = 7.1
    SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
    SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
    SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
    // TODO(fbarchard): Add additional masks for 7.2 and beyond.
};

inline size_t PCMWaveOutAudioOutputStream::BufferSize() const
{
    // Round size of buffer up to the nearest 16 bytes.
    return (sizeof(WAVEHDR) + buffer_size_ + 15u) & static_cast<size_t>(~15);
}

inline WAVEHDR* PCMWaveOutAudioOutputStream::GetBuffer(int n) const
{
    //DCHECK_GE(n, 0);
    //DCHECK_LT(n, num_buffers_);
    return reinterpret_cast<WAVEHDR*>(&buffers_[n * BufferSize()]);
}

void PCMWaveOutAudioOutputStream::ClearData()
{
	std::lock_guard<std::mutex> auto_lock(queue_lock_);
	queue_.clear();
	big_frame_.reset();
}

PCMWaveOutAudioOutputStream::PCMWaveOutAudioOutputStream(int sample_rate, int channels, int bits_per_sample)
{
	channels_ = channels;
	sample_rate_ = sample_rate;
	buffer_event_ = NULL;
	device_id_ = -1;

	buffer_size_ = 192000 * channels_ * bits_per_sample / 8 ;
	state_ = PCMA_BRAND_NEW;

	format_.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	format_.Format.nChannels = channels_;
	format_.Format.nSamplesPerSec = sample_rate;
	format_.Format.wBitsPerSample = bits_per_sample;
	format_.Format.cbSize = sizeof(format_) - sizeof(WAVEFORMATEX);
	// The next are computed from above.
	format_.Format.nBlockAlign = (format_.Format.nChannels *
		format_.Format.wBitsPerSample) / 8;
	format_.Format.nAvgBytesPerSec = format_.Format.nBlockAlign *
		format_.Format.nSamplesPerSec;

	if (channels > kMaxChannelsToMask) 
	{
		format_.dwChannelMask = kChannelsToMask[kMaxChannelsToMask];
	} else 
	{
		format_.dwChannelMask = kChannelsToMask[channels];
	}
	
#ifndef KSDATAFORMAT_SUBTYPE_PCM
	const GUID KSDATAFORMAT_SUBTYPE_PCM ={ 0x00000001, 0x0000, 0x0010, {0x80,
		0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71} };
#endif

	format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	format_.Samples.wValidBitsPerSample = bits_per_sample;

	num_buffers_ = 3; //vista是4.

	OSVERSIONINFOEX version_info = { sizeof version_info };
	GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
	if(version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 0)
	{
		//vista.
		num_buffers_ = 4;
	}

	rand();
}

PCMWaveOutAudioOutputStream::~PCMWaveOutAudioOutputStream()
{
    //DCHECK(NULL == waveout_);
	if (buffer_event_)
	{
		CloseHandle(buffer_event_);
	}
}

bool PCMWaveOutAudioOutputStream::Open()
{
    if (state_ != PCMA_BRAND_NEW)
        return false;
    if (BufferSize() * num_buffers_ > kMaxOpenBufferSize)
        return false;
    if (num_buffers_ < 2 || num_buffers_ > 5)
        return false;

    // Create buffer event.
    buffer_event_ = ::CreateEvent(NULL,    // Security attributes.
                                    FALSE,   // It will auto-reset.
                                    FALSE,   // Initial state.
                                    NULL);  // No name.
    if (!buffer_event_)
        return false;

    // Open the device.
    // We'll be getting buffer_event_ events when it's time to refill the buffer.
    MMRESULT result = ::waveOutOpen(
                          &waveout_,
                          -1,
                          reinterpret_cast<LPCWAVEFORMATEX>(&format_),
                          reinterpret_cast<DWORD_PTR>(buffer_event_),
                          NULL,
                          CALLBACK_EVENT);
    if (result != MMSYSERR_NOERROR)
        return false;

    SetupBuffers();
    state_ = PCMA_READY;
    return true;
}

void PCMWaveOutAudioOutputStream::SetupBuffers()
{
    buffers_.reset(new char[BufferSize() * num_buffers_]);
    for (int ix = 0; ix != num_buffers_; ++ix)
    {
        WAVEHDR* buffer = GetBuffer(ix);
        buffer->lpData = reinterpret_cast<char*>(buffer) + sizeof(WAVEHDR);
        buffer->dwBufferLength = buffer_size_;
        buffer->dwBytesRecorded = 0;
        buffer->dwFlags = WHDR_DONE;
        buffer->dwLoops = 0;
        // Tell windows sound drivers about our buffers. Not documented what
        // this does but we can guess that causes the OS to keep a reference to
        // the memory pages so the driver can use them without worries.
        ::waveOutPrepareHeader(waveout_, buffer, sizeof(WAVEHDR));
    }
}
//
void PCMWaveOutAudioOutputStream::FreeBuffers()
{
    for (int ix = 0; ix != num_buffers_; ++ix)
    {
        ::waveOutUnprepareHeader(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
    }
    buffers_.reset();
}

// Initially we ask the source to fill up all audio buffers. If we don't do
// this then we would always get the driver callback when it is about to run
// samples and that would leave too little time to react.
void PCMWaveOutAudioOutputStream::Start()
{
    if (state_ != PCMA_READY)
        return;

    // Reset buffer event, it can be left in the arbitrary state if we
    // previously stopped the stream. Can happen because we are stopping
    // callbacks before stopping playback itself.
    if (!::ResetEvent(buffer_event_))
    {
        HandleError(MMSYSERR_ERROR);
        return;
    }

    // Start watching for buffer events.
    if (!::RegisterWaitForSingleObject(&waiting_handle_,
                                       buffer_event_,
                                       &BufferCallback,
                                       this,
                                       INFINITE,
                                       WT_EXECUTEDEFAULT))
    {
        HandleError(MMSYSERR_ERROR);
        waiting_handle_ = NULL;
        return;
    }

    state_ = PCMA_PLAYING;

    // Queue the buffers.
    pending_bytes_ = 0;
    for (int ix = 0; ix != num_buffers_; ++ix)
    {
        WAVEHDR* buffer = GetBuffer(ix);
		int dummy_data_len = 0;
		buffer->dwBufferLength = dummy_data_len;
		memset(buffer->lpData, 0, dummy_data_len);
		buffer->dwFlags = WHDR_PREPARED;
      //  QueueNextPacket(buffer);  // Read more data.
        pending_bytes_ += buffer->dwBufferLength;
    }

    // From now on |pending_bytes_| would be accessed by callback thread.
    // Most likely waveOutPause() or waveOutRestart() has its own memory barrier,
    // but issuing our own is safer.
    ::MemoryBarrier();

    MMRESULT result = ::waveOutPause(waveout_);
    if (result != MMSYSERR_NOERROR)
    {
        HandleError(result);
        return;
    }

    // Send the buffers to the audio driver. Note that the device is paused
    // so we avoid entering the callback method while still here.
    for (int ix = 0; ix != num_buffers_; ++ix)
    {
        result = ::waveOutWrite(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR)
        {
            HandleError(result);
            break;
        }
    }
    result = ::waveOutRestart(waveout_);
    if (result != MMSYSERR_NOERROR)
    {
        HandleError(result);
        return;
    }
}

// Stopping is tricky if we want it be fast.
// For now just do it synchronously and avoid all the complexities.
// TODO(enal): if we want faster Stop() we can create singleton that keeps track
//             of all currently playing streams. Then you don't have to wait
//             till all callbacks are completed. Of course access to singleton
//             should be under its own lock, and checking the liveness and
//             acquiring the lock on stream should be done atomically.
void PCMWaveOutAudioOutputStream::Stop()
{
    if (state_ != PCMA_PLAYING)
        return;
    state_ = PCMA_STOPPING;
    ::MemoryBarrier();

    // Stop watching for buffer event, wait till all the callbacks are complete.
    // Should be done before ::waveOutReset() call to avoid race condition when
    // callback that is currently active and already checked that stream is still
    // being played calls ::waveOutWrite() after ::waveOutReset() returns, later
    // causing ::waveOutClose() to fail with WAVERR_STILLPLAYING.
    // TODO(enal): that delays actual stopping of playback. Alternative can be
    //             to call ::waveOutReset() twice, once before
    //             ::UnregisterWaitEx() and once after.
    if (waiting_handle_)
    {
        if (!::UnregisterWaitEx(waiting_handle_, INVALID_HANDLE_VALUE))
        {
            state_ = PCMA_PLAYING;
            HandleError(MMSYSERR_ERROR);
            return;
        }
        waiting_handle_ = NULL;
    }

    // Stop playback.
    MMRESULT res = ::waveOutReset(waveout_);
    if (res != MMSYSERR_NOERROR)
    {
        state_ = PCMA_PLAYING;
        HandleError(res);
        return;
    }

    // Wait for lock to ensure all outstanding callbacks have completed.
   std::lock_guard<std::mutex> auto_lock(lock_);

    // waveOutReset() leaves buffers in the unpredictable state, causing
    // problems if we want to close, release, or reuse them. Fix the states.
    for (int ix = 0; ix != num_buffers_; ++ix)
    {
        GetBuffer(ix)->dwFlags = WHDR_PREPARED;
    }

    // Don't use callback after Stop().

    state_ = PCMA_READY;
}

// We can Close in any state except that trying to close a stream that is
// playing Windows generates an error. We cannot propagate it to the source,
// as callback_ is set to NULL. Just print it and hope somebody somehow
// will find it...
void PCMWaveOutAudioOutputStream::Close()
{
    // Force Stop() to ensure it's safe to release buffers and free the stream.
    Stop();

    if (waveout_)
    {
        FreeBuffers();

        // waveOutClose() generates a WIM_CLOSE callback.  In case Start() was never
        // called, force a reset to ensure close succeeds.
        MMRESULT res = ::waveOutReset(waveout_);
        //DCHECK_EQ(res, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        res = ::waveOutClose(waveout_);
        //DCHECK_EQ(res, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        state_ = PCMA_CLOSED;
        waveout_ = NULL;
    }
}

//
void PCMWaveOutAudioOutputStream::HandleError(MMRESULT error)
{
    //DLOG(WARNING) << "PCMWaveOutAudio error " << error;
}

void PCMWaveOutAudioOutputStream::QueueNextPacket(WAVEHDR *buffer)
{
    //DCHECK_EQ(channels_, format_.Format.nChannels);

	std::string data;
	CFrameSharePtr frame;
	while(1)
	{
		::MemoryBarrier();
		if (state_ != PCMA_PLAYING)
			return;

		bool got = false;
		{
			std::lock_guard<std::mutex> auto_lock(queue_lock_);

			if(!queue_.empty())
			{
				frame = queue_.front();
				queue_.pop_front();
				got = true;
			}
		}

		if(got)
		{
			memcpy(buffer->lpData, frame->GetDataPtr(), frame->m_nLen);
			buffer->dwBufferLength = frame->m_nLen;
			buffer->dwFlags = WHDR_PREPARED;
			break;
		}
		else
		{
			memset(buffer->lpData, 0, kIdealSize);
			buffer->dwBufferLength = kIdealSize;// frame->m_nLength;
			buffer->dwFlags = WHDR_PREPARED;
			
			break;
		}
		
	}
}

// One of the threads in our thread pool asynchronously calls this function when
// buffer_event_ is signalled. Search through all the buffers looking for freed
// ones, fills them with data, and "feed" the Windows.
// Note: by searching through all the buffers we guarantee that we fill all the
//       buffers, even when "event loss" happens, i.e. if Windows signals event
//       when it did not flip into unsignaled state from the previous signal.
void NTAPI PCMWaveOutAudioOutputStream::BufferCallback(PVOID lpParameter,
        BOOLEAN timer_fired)
{
    //TRACE_EVENT0("audio", "PCMWaveOutAudioOutputStream::BufferCallback");

    //DCHECK(!timer_fired);
    PCMWaveOutAudioOutputStream* stream =
        reinterpret_cast<PCMWaveOutAudioOutputStream*>(lpParameter);

    // Lock the stream so callbacks do not interfere with each other.
    // Several callbacks can be called simultaneously by different threads in the
    // thread pool if some of the callbacks are slow, or system is very busy and
    // scheduled callbacks are not called on time.

	std::lock_guard<std::mutex> auto_lock(stream->lock_);
    if (stream->state_ != PCMA_PLAYING)
        return;

    for (int ix = 0; ix != stream->num_buffers_; ++ix)
    {
        WAVEHDR* buffer = stream->GetBuffer(ix);
        if (buffer->dwFlags & WHDR_DONE)
        {
            // Before we queue the next packet, we need to adjust the number of
            // pending bytes since the last write to hardware.
            stream->pending_bytes_ -= buffer->dwBufferLength;
            stream->QueueNextPacket(buffer);

            // QueueNextPacket() can take a long time, especially if several of them
            // were called back-to-back. Check if we are stopping now.
            if (stream->state_ != PCMA_PLAYING)
                return;

            // Time to send the buffer to the audio driver. Since we are reusing
            // the same buffers we can get away without calling waveOutPrepareHeader.
            MMRESULT result = ::waveOutWrite(stream->waveout_,
                                             buffer,
                                             sizeof(WAVEHDR));
            if (result != MMSYSERR_NOERROR)
                stream->HandleError(result);
            stream->pending_bytes_ += buffer->dwBufferLength;
        }
    }
}

int PCMWaveOutAudioOutputStream::Enqueue( CFrameSharePtr& input_frame )
{
	std::lock_guard<std::mutex> auto_lock(queue_lock_);
	if(queue_.size() > kMaxAudioQueueSize)
	{
		return -1;
	}
	//TODO improve.
	if(big_frame_!= NULL)
	{
		if (big_frame_->m_nAudioSampleRate != input_frame->m_nAudioSampleRate || big_frame_->m_nAudioChannel != input_frame->m_nAudioChannel)
		{
			big_frame_ = NULL; //采样率改变了，则重置
		}
	}

	if(big_frame_ == NULL)
	{
		big_frame_ = input_frame;
	}
	else
	{
		CFrameSharePtr combined_frame = NewShareFrame();
		combined_frame->m_nAudioSampleRate = input_frame->m_nAudioSampleRate;
		combined_frame->m_nAudioChannel = input_frame->m_nAudioChannel;
		int combined_length = input_frame->m_nLen + big_frame_->m_nLen;
		combined_frame->AllocMem(combined_length);
		combined_frame->m_nLen = combined_length;
		memcpy(combined_frame->GetDataPtr(), big_frame_->GetDataPtr(), big_frame_->m_nLen);
		memcpy(combined_frame->GetDataPtr() + big_frame_->m_nLen, input_frame->GetDataPtr(), input_frame->m_nLen);
		big_frame_ = combined_frame;
	}
	
	

	if (big_frame_->m_nLen < kIdealSize)
	{
		return -2;
	}

	while (big_frame_ != NULL && big_frame_->m_nLen > kIdealSize)
	{
		CFrameSharePtr out_frame = NewShareFrame();
		out_frame->AllocMem(kIdealSize);
		out_frame->m_nLen = kIdealSize;
		memcpy(out_frame->GetDataPtr(), big_frame_->GetDataPtr(), kIdealSize);

		int left = big_frame_->m_nLen - out_frame->m_nLen;
		if(left >  0)
		{
			CFrameSharePtr stFrameTemp = CloneSharedFrame(big_frame_);
			stFrameTemp->AllocMem(left);
			stFrameTemp->m_nLen = left;
			memcpy(stFrameTemp->GetDataPtr(), big_frame_->GetDataPtr() + kIdealSize, left);
			big_frame_ = stFrameTemp;
		}
		else
		{
			big_frame_ = NULL;
		}

		queue_.push_back(out_frame);
	}

	return 0;
}
