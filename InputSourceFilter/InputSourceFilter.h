#ifndef INC_INPUTSOURCEFILTER_H
#define INC_INPUTSOURCEFILTER_H
#include<qstring.h>
#include <qmutex.h>
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
#include <deque>
#include <map>
using namespace std;

class CInputFileSource : public CSSFilter
{
public:
	CInputFileSource(CInputSourceParam &inputparam);
	virtual ~CInputFileSource();
	virtual int InputData(CFrameSharePtr &frame);
	virtual void StartService();
	virtual void StopService();
	virtual int Open();
	virtual int Close();
	virtual bool IsProcessOver();
	virtual void  SetOutFormat(int nFormatType);

	int ConverToYUV420P(AVFrame* frame, unsigned char* rgb_buffer, int nWidth, int nHeight);
	int ResampleAudio(AVFrame* frame, unsigned char* out_buffer, int out_samples, int& actual_out_samples);

	static DWORD WINAPI  ReadFunc(LPVOID arg);
	static DWORD WINAPI  DecoderVideoFunc(LPVOID arg);
	static DWORD WINAPI  SendVideoFunc(LPVOID arg);
	static DWORD WINAPI  SendAudioFunc(LPVOID arg);
	
	void SyncAudio();
	void SyncVideoSyncVideo();
	virtual int SetSourceSeek(double dbRatio);
	virtual double GetSourceAllTime();

	virtual void SetPlay(bool bPlay);
	virtual void SetPlaySpeed(double dbSpeed);
	virtual void SetPsIndex(int index);
	void DecoderVideo();
private:
	CInputSourceParam m_InputSourceParam;
	int64_t                 m_nFirstAudioPts;
	int64_t                 m_nFirstVideoPts;
	int					 m_nFormatType;
	CFrameSharePtrQueue   m_ListAudio;
	CFrameSharePtrQueue   m_ListVideo;

	quint32				m_nIndex;
	QString				m_strPath;
	bool				m_bAutoPlay;
	bool				m_bFinished;
	QMutex m_MutexPacketVideo;
	std::deque<AVPacket> m_deqVideoPacket;
	AVFormatContext*     m_pstFmtCtx;
	map<int, int> m_mapIndex2Pro;
	int m_nVideoIndex;
	int m_nAudioIndex;
	int m_nSubscriptIndex;
	AVCodecContext* m_pstAudioCodecCtx;
	AVCodecContext* m_pstVideoCodecCtx;

	//用来存放解码结果的
	//AVFrame*	m_pstDecodedVideoFrame;
	//AVFrame*	m_pstDecodedAudioFrame;

	qint64		m_nFixerPreFrameTimestemp;
	qint64		m_nFixerOffset;

	qint64		m_nTempTime;
	double		m_dbNowTime;

	//QkTimer	m_stLastNotifyTimer;
	qint64  m_nLastProgressTime;

	struct SwrContext* m_pstSwrContext;

	bool		m_bPlayerStatus;
	bool		m_bPauseStatus;

	float		m_fLeftVolume;
	float		m_fRightVolume;

	double  m_dbLastTime;
	double  m_dbAudioLastTime;
	double  m_dbVideoLastTime;
	SwsContext* m_pSwsCtx;
	AVPicture *m_picture;
	int		m_nLaseFireVideoTime;
	int   m_nWidth;
	int   m_nHeight;
	double m_dbVideoTemp;
	double m_dbAudioTemp;
	bool m_bExit;
	bool m_bPause;
	HANDLE m_hReadThread;
	HANDLE m_hDecoderAudioThread;
	HANDLE m_hDecoderVideoThread;
	HANDLE m_hSendVideoThread;
	HANDLE m_hSendAudioThread;
	bool m_bReadFinish;
	QMutex m_Mutex;
	bool m_bStartSupportData;
	long long m_nLastAudioTime;
	long long m_nFirstAudioTime;
	long long m_nSyncTime;
	long long m_nSyncVideoTime;
	double m_dbPlaySpeed;
	long long m_nLastVideoTime;
	long long m_nFirstVideoTime;
	bool m_bPlay;

	bool m_bHardWare;

	DWORD     m_dwVendorId;
	bool      m_bSeek;
	SYNCTYPE m_eSyncType;
	long long m_nFirstOrgAudioPts;
	long long m_nFirstOrgVideoPts;
	QMutex  m_MutexSyncVideoTime;
	QMutex  m_MutexSyncAudioTime;
	bool    m_bAudioFile;
	int     m_nOrgAudioChannelLayOut;
	HANDLE	m_hEventVideoHandle;
	HANDLE	m_hEventAudioHandle;
	long m_nFrameRate;
	bool m_bFlush=false;
	bool m_bPlayVideo = false;

	CStreamInfo  m_StreamInfo;
};
#endif