#ifndef INC_UTILS_H
#define INC_UTILS_H
#include <string>
#include <deque>
enum InputSourceType
{
	eUnknowSource = -1,
	eVideoSource,
	eAudioSource,
};

enum FrameType
{
	eUnknowFrame = -1,
	eVideoFrame,
	eAudioFrame,
};

enum PixType
{
	eUnknowPix = -1,
	eBGRA,
	eRGBA,
	eBGR,
	eYUV420P,
	eYVYU422,
	eUYVY422,
	eYUYV422,
	eYUV420P10,
	eYUV422P10,
	eYUV422P,

};

enum SYNCTYPE
{
	SYNC_AUDIO,
	SYNC_VIDEO,
	SYNC_SYSTEM,
};

struct CInputSourceParam
{
	std::string m_strFileName;
};

const int kOutSampleRate = 48000;
const int kOutChannels = 2;
const int kOutBytesPerSample = 2;
struct CAudioStreamInfo
{
	int m_nStreamID;
	std::string m_strFormat;
	std::string m_strFormatInfo;
	std::string m_strCodeID;
	long long m_nDuration;
	long m_nBitRate;
	int m_nChannel;
	int m_nChannelLayOut;
	long m_nSampleRate;
	double m_dbFrameRate;
	std::string m_strLanguage;
};

struct CVideoStreamInfo
{
	int m_nStreamID;
	std::string m_strFormat;
	std::string m_strFormatInfo;
	std::string m_strCodeID;
	long long m_nDuration;
	long m_nBitRate;
	int m_nWidth;
	int m_nHeight;
	double m_dbFrameRate;
	std::string m_strColorSpace;
};

struct CStreamInfo
{
	std::deque< CVideoStreamInfo> m_deqVideoStream;
	std::deque< CAudioStreamInfo> m_deqAudioStream;
};


#endif
