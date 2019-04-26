#include "AudioRenderFilter.h"
#include "../Common/waveout_output_win.h"

AudioRenderFilter::AudioRenderFilter(std::string &name) : CSSFilter(name)
{
	m_iSampleRate = 0;
	m_iChannels = 0;
	m_iBitsPerSample = 0;
	m_stOutPutStream = NULL;
}

int AudioRenderFilter::InputData(CFrameSharePtr &frame)
{
	if (eAudioFrame == frame->m_eFrameType)
	{
		if (frame == NULL)
		{
			return 0;
		}
		std::lock_guard<std::mutex> stLock(m_MutexOutPutStream);
		if (m_stOutPutStream != NULL)
		{
			if (m_iSampleRate != frame->m_nAudioSampleRate || m_iChannels != frame->m_nAudioChannel || m_iBitsPerSample != frame->m_nBitPerSample)
			{
				m_stOutPutStream->Close();
				delete m_stOutPutStream;
				m_stOutPutStream = NULL;
			}
		}

		if (m_stOutPutStream == NULL)
		{
			m_stOutPutStream = new PCMWaveOutAudioOutputStream(frame->m_nAudioSampleRate, frame->m_nAudioChannel, frame->m_nBitPerSample);
			m_stOutPutStream->Open();
			m_stOutPutStream->Start();

			m_iSampleRate = frame->m_nAudioSampleRate;
			m_iChannels = frame->m_nAudioChannel;
			m_iBitsPerSample = frame->m_nBitPerSample;

		}
		int nRet = m_stOutPutStream->Enqueue(frame);
		if (nRet != 0)
		{
			
		}
	}
	return 0;
}


AudioRenderFilter::~AudioRenderFilter()
{
	std::lock_guard<std::mutex> stLock(m_MutexOutPutStream);
	if (m_stOutPutStream != NULL)
	{

		m_stOutPutStream->Close();
		delete m_stOutPutStream;
		m_stOutPutStream = NULL;

	}
}

void AudioRenderFilter::ClearData()
{
	std::lock_guard<std::mutex> stLock(m_MutexOutPutStream);
	if (m_stOutPutStream != NULL)
	{
		m_stOutPutStream->ClearData();
	}
}