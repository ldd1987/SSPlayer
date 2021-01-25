#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SSPlayer.h"

class CInputFileSource;
class VideoRenderFilter;
class AudioRenderFilter;
void initLoggingSystem(int argc, char* argv[]);
class SSPlayer : public QMainWindow
{
	Q_OBJECT

public:
	SSPlayer(QWidget *parent = Q_NULLPTR);
	~SSPlayer();
	virtual void resizeEvent(QResizeEvent* pstEvent);
private slots:
	void SlotOpenFile();
	void ShowContextMenu(const QPoint& pos); // this is a slot;
private:
	Ui::SSPlayerClass ui;

	QAction * m_pVedioFileInputAct;

	CInputFileSource *m_pInputSourceFilter;
	VideoRenderFilter *m_pVideoRenderFilter1;
	VideoRenderFilter *m_pVideoRenderFilter2;
	AudioRenderFilter *m_pAudioRenderFilter;
};
