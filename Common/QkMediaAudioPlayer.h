#pragma once

#include <QObject>
#include <QtMultimedia/QAudioOutput>

#include "utils/QkFrameHolderQueue.h"
//#include "Audio/QkAudioProcessing.h"
#include "windows.h"
class PCMWaveOutAudioOutputStream;

class QkMediaAudioPlayer : public QObject
{
	Q_OBJECT

public:
	QkMediaAudioPlayer();
	virtual ~QkMediaAudioPlayer();

	bool init();
	void ClearData();
	int inputDate(QkFrameHolder& stFrame);
	static DWORD WINAPI  PlayFunc(LPVOID arg);
private:
	void release();
	void stop();

signals:
	void signalPlayAudio();

public slots:
	void slotsOpen();
	void slotsPlayAudio();

private:
	QAudioOutput*	m_pstAudioOutput;
	QkFrameHolderQueue	m_stQueue;
	QIODevice *m_pstIODevice;

	bool		m_bPlayerStatus;

	int		m_iSampleRate;
	int		m_iChannels;
	int		m_iBitsPerSample;
	bool    m_bExit;
	PCMWaveOutAudioOutputStream*	m_stOutPutStream;
	HANDLE m_hPlayAudioThread;
};

