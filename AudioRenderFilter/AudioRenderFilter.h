#pragma once

#include "audiorenderfilter_global.h"
#include "../Common/Filter.h"
class PCMWaveOutAudioOutputStream;
class  AudioRenderFilter : public CSSFilter
{
public:
	AudioRenderFilter(std::string &name);
	~AudioRenderFilter();
	virtual int InputData(CFrameSharePtr &frame);
	void ClearData();
private:
	int		m_iSampleRate;
	int		m_iChannels;
	int		m_iBitsPerSample;
	bool    m_bExit;
	std::mutex  m_MutexOutPutStream;
	PCMWaveOutAudioOutputStream*	m_stOutPutStream;
};
