//
#include <QtCore/QThread>
#include <QtDebug>

#include "utils/QkLogger.h"
#include "QkMediaAudioPlayer.h"

#include "waveout_output_win.h"


QkMediaAudioPlayer::QkMediaAudioPlayer()
:m_pstAudioOutput(NULL)
{
	m_bPlayerStatus = false;

	m_iSampleRate = 0;
	m_iChannels = 0;
	m_iBitsPerSample = 0;
	m_bExit = false;
	m_stOutPutStream = NULL;
	m_hPlayAudioThread = NULL;
}
void QkMediaAudioPlayer::ClearData()
{
	m_stQueue.Clear();
	if (m_stOutPutStream)
	{
		m_stOutPutStream->ClearData();
	}
}

DWORD WINAPI QkMediaAudioPlayer::PlayFunc(LPVOID arg)
{
	QkMediaAudioPlayer *pThis = (QkMediaAudioPlayer *)arg;
	if (NULL == pThis)
	{
		return -1;
	}
	while (pThis->m_bExit == false || pThis->m_bPlayerStatus)
	{
		Sleep(3);
		pThis->slotsPlayAudio();
	}

}

QkMediaAudioPlayer::~QkMediaAudioPlayer()
{
	stop();
	QK_LOG(LOG_INFO, "~PA_MediaAudioPlayer :  %d ", QThread::currentThreadId());
}

void QkMediaAudioPlayer::slotsPlayAudio()
{

	do 
	{
		if (m_stQueue.IsEmpty())
		{
			Sleep(5);
			break;
		}

		QkFrameHolder frame = m_stQueue.Dequeue();
		if (frame == NULL)
		{
			break;
		}
		if (m_stOutPutStream != NULL)
		{
			if (m_iSampleRate != frame->m_nSampleRate || m_iChannels != frame->m_nChannels || m_iBitsPerSample != frame->m_nBitPerSample)
			{
				m_stOutPutStream->Close();
				delete m_stOutPutStream;
				m_stOutPutStream = NULL;
			}
		}

		if (m_stOutPutStream == NULL)
		{
			m_stOutPutStream = new PCMWaveOutAudioOutputStream(frame->m_nSampleRate, frame->m_nChannels, frame->m_nBitPerSample);
			m_stOutPutStream->Open();
			m_stOutPutStream->Start();

			m_iSampleRate = frame->m_nSampleRate;
			m_iChannels = frame->m_nChannels;
			m_iBitsPerSample = frame->m_nBitPerSample;

		}
		m_stOutPutStream->Enqueue(frame);
	} while (true);

}

bool QkMediaAudioPlayer::init()
{
	QObject::connect(this, SIGNAL(signalPlayAudio()), SLOT(slotsPlayAudio()), Qt::QueuedConnection);

	return true;
}

void QkMediaAudioPlayer::slotsOpen()
{

	m_bPlayerStatus = true;

	if (NULL == m_hPlayAudioThread)
	{
		m_hPlayAudioThread = CreateThread(NULL, 0, PlayFunc, this, 0, NULL);
	}
}

int QkMediaAudioPlayer::inputDate(QkFrameHolder& stFrame)
{
	if (m_stQueue.Size() <= 1)
	{
		m_stQueue.Enqueue(stFrame);
	}

	return 0;
}

void QkMediaAudioPlayer::stop()
{
	QObject::disconnect(this, SIGNAL(signalPlayAudio()), this, SLOT(slotsPlayAudio()));
	m_bExit = true;
	if (m_hPlayAudioThread)
	{
		WaitForSingleObject(m_hPlayAudioThread, INFINITE);
		CloseHandle(m_hPlayAudioThread);
		m_hPlayAudioThread = NULL;
	}
	

	m_stQueue.Clear();

	if (NULL != m_pstAudioOutput)
	{
		m_pstAudioOutput->stop();
		delete[] m_pstAudioOutput;
		m_pstAudioOutput = NULL;
	}

	m_iSampleRate = 0;
	m_iChannels = 0;
	m_iBitsPerSample = 0;
}

void QkMediaAudioPlayer::release()
{
	if (NULL != m_pstAudioOutput)
	{
		m_pstAudioOutput->stop();
		delete[] m_pstAudioOutput;
		m_pstAudioOutput = NULL;
	}

	m_iSampleRate = 0;
	m_iChannels = 0;
	m_iBitsPerSample = 0;
}
