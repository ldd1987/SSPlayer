#ifndef INC_INPUTSOURCEFILTER_H
#define INC_INPUTSOURCEFILTER_H

#include "inputsourcefilter_global.h"
#include "../Common/Filter.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/rational.h"
#include "libavdevice/avdevice.h"
#include "libavutil/mathematics.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include <libavutil/time.h>
}

#include <Windows.h>

class  InputSourceFilter : public CSSFilter
{
public:
	InputSourceFilter(CInputSourceParam &param);
	virtual ~InputSourceFilter();
	virtual int InputData(CFrameSharePtr &frame);
	int Start();
	void Stop();
	bool OpenFile();
	void Close();
	CStreamInfo GetStreamInfo();
	static DWORD WINAPI  DecoderAudioFunc(LPVOID arg);
	static DWORD WINAPI  DecoderVideoFunc(LPVOID arg);
	static DWORD WINAPI  SendVideoFunc(LPVOID arg);
	static DWORD WINAPI  SendAudioFunc(LPVOID arg);
	void SyncAudio();
	void SyncVideoSyncAudio();
	void SyncVideoSyncVideo();

	void DecoderAudioPacket();
	void DecoderVideoPacket();

	int ResampleAudio(AVFrame* frame, unsigned char * out_buffer, int out_samples, int& actual_out_samples);
	int ConverToYUV420P(AVFrame* frame, unsigned char* rgb_buffer, int nWidth, int nHeight);
private:
	CInputSourceParam m_InputSourceParam;
	AVFormatContext*     m_pstFmtCtx;
	int m_nVideoIndex;
	int m_nAudioIndex;
	int m_nSubscriptIndex;
	AVCodecContext* m_pstAudioCodecCtx;
	AVCodecContext* m_pstVideoCodecCtx;
	struct SwrContext* m_pstSwrContext;

	SYNCTYPE m_eSyncType;

	CFrameSharePtrQueue   m_ListAudio;
	CFrameSharePtrQueue   m_ListVideo;

	std::mutex m_Mutex;
	std::mutex m_MutexPacketVideo;
	std::deque<AVPacket> m_deqVideoPacket;
	SwsContext* m_pSwsCtx;
	AVPicture *m_picture;

	int m_nOrgAudioChannelLayOut;
	bool m_bAudioFile;
	bool m_bStartSupportData;
	bool m_bSeek;
	bool m_bExit;
	HANDLE m_hDecoderAudioThread;
	HANDLE m_hDecoderVideoThread;

	HANDLE m_hSendVideoThread;
	HANDLE m_hSendAudioThread;
	double m_dbRatio;
double m_dbPlaySpeed;
bool m_bPlay;
	long long m_nFirstOrgAudioPts;
	bool m_bReadFinish;
	long long m_nFirstOrgVideoPts;
	std::mutex  m_MutexSyncVideoTime;
	std::mutex  m_MutexSyncAudioTime;
	HANDLE	m_hEventVideoHandle;
	HANDLE	m_hEventAudioHandle;
	long long m_nSyncVideoTime;
	long long m_nFirstVideoTime;
	long long m_nLastAudioTime;
	long long m_nSyncTime;
	long long m_nFirstAudioTime;

	CStreamInfo  m_StreamInfo;
};
#endif