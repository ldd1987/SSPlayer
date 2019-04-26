#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SSPlayer.h"
class InputSourceFilter;
class VideoRenderFilter;
class AudioRenderFilter;
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

	InputSourceFilter *m_pInputSourceFilter;
	VideoRenderFilter *m_pVideoRenderFilter;
	AudioRenderFilter *m_pAudioRenderFilter;
};
