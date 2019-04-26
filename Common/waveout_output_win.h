// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_

#include <windows.h>
#include <mmsystem.h>
//#include <mmreg.h>

#include <list>
#include "Frame.h"



#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct {
	WAVEFORMATEX    Format;
	union {
		WORD wValidBitsPerSample;       /* bits of precision  */
		WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
		WORD wReserved;                 /* If neither applies, set to zero. */
	} Samples;
	DWORD           dwChannelMask;      /* which channels are */
	/* present in stream  */
	GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif // !_WAVEFORMATEXTENSIBLE_

//
//  Extended PCM waveform format structure based on WAVEFORMATEXTENSIBLE.
//  Use this for multiple channel and hi-resolution PCM data
//
typedef WAVEFORMATEXTENSIBLE    WAVEFORMATPCMEX; /* Format.cbSize = 22 */

#ifndef _SPEAKER_POSITIONS_
#define _SPEAKER_POSITIONS_
// Speaker Positions for dwChannelMask in WAVEFORMATEXTENSIBLE:
#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000

// Bit mask locations reserved for future use
#define SPEAKER_RESERVED                0x7FFC0000

// Used to specify that any possible permutation of speaker configurations
#define SPEAKER_ALL                     0x80000000
#endif // _SPEAKER_POSITIONS_

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define  WAVE_FORMAT_EXTENSIBLE                 0xFFFE /* Microsoft */
#endif // !defined(WAVE_FORMAT_EXTENSIBLE)


//
class PCMWaveOutAudioOutputStream
{
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the the
  // audio manager who is creating this object and |device_id| which is provided
  // by the operating system.
  PCMWaveOutAudioOutputStream(int sample_rate, int channels, int bits_per_sample);
  virtual ~PCMWaveOutAudioOutputStream();

  // Implementation of AudioOutputStream.
   bool Open();
   void Close();
   void Start();
   void Stop();
   void SetVolume(double volume);
   void GetVolume(double* volume);
   void ClearData();
  // Sends a buffer to the audio driver for playback.
  void QueueNextPacket(WAVEHDR* buffer);

  int Enqueue(CFrameSharePtr& input_frame);

 private:
  enum State {
    PCMA_BRAND_NEW,    // Initial state.
    PCMA_READY,        // Device obtained and ready to play.
    PCMA_PLAYING,      // Playing audio.
    PCMA_STOPPING,     // Audio is stopping, do not "feed" data to Windows.
    PCMA_CLOSED        // Device has been released.
  };

  // Returns pointer to the n-th buffer.
  inline WAVEHDR* GetBuffer(int n) const;

  // Size of one buffer in bytes, rounded up if necessary.
  inline size_t BufferSize() const;

  // Windows calls us back asking for more data when buffer_event_ signalled.
  // See MSDN for help on RegisterWaitForSingleObject() and waveOutOpen().
  static void NTAPI BufferCallback(PVOID lpParameter, BOOLEAN timer_fired);

  // If windows reports an error this function handles it and passes it to
  // the attached AudioSourceCallback::OnError().
  void HandleError(MMRESULT error);

  // Allocates and prepares the memory that will be used for playback.
  void SetupBuffers();

  // Deallocates the memory allocated in SetupBuffers.
  void FreeBuffers();

  // Reader beware. Visual C has stronger guarantees on volatile vars than
  // most people expect. In fact, it has release semantics on write and
  // acquire semantics on reads. See the msdn documentation.
  volatile State state_;

  // The number of buffers of size |buffer_size_| each to use.
   int num_buffers_;

  // The size in bytes of each audio buffer, we usually have two of these.
  unsigned int buffer_size_;
  HANDLE buffer_event_;
  // Channels from 0 to 8.
   int channels_;

   int sample_rate_;

  // Number of bytes yet to be played in the hardware buffer.
   unsigned int pending_bytes_;

  // The id assigned by the operating system to the selected wave output
  // hardware device. Usually this is just -1 which means 'default device'.
  UINT device_id_;

  // Windows native structure to encode the format parameters.
  WAVEFORMATPCMEX format_;

  // Handle to the instance of the wave device.
  HWAVEOUT waveout_;

  // Handle to the buffer event.


  // Handle returned by RegisterWaitForSingleObject().
  HANDLE waiting_handle_;

  // Pointer to the allocated audio buffers, we allocate all buffers in one big
  // chunk. This object owns them.
  std::shared_ptr<char[]> buffers_;

  // Lock used to avoid the conflict when callbacks are called simultaneously.
  std::mutex lock_;

  std::mutex				 queue_lock_;
  std::list<CFrameSharePtr>  queue_;
  CFrameSharePtr			 big_frame_;

  friend class PcmWavePlayer;

//
//  DISALLOW_COPY_AND_ASSIGN(PCMWaveOutAudioOutputStream);
};


#endif  // MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
