#include "SSPlayer.h"
#include <QFileDialog>
#include <QResizeEvent>
#include "../Common/Filter.h"
#include "../InputSourceFilter/InputSourceFilter.h"
#include "../VideoRenderFilter/VideoRenderFilter.h"
#include "../AudioRenderFilter/AudioRenderFilter.h"
SSPlayer::SSPlayer(QWidget *parent)
	: QMainWindow(parent)
{
	timeBeginPeriod(1);
	ui.setupUi(this);
	std::string strName = "dx11-render";
	m_pVideoRenderFilter = new VideoRenderFilter(ui.centralWidget, strName);
	strName = "audio-render";
	m_pAudioRenderFilter = new AudioRenderFilter(strName);
	m_pInputSourceFilter = 0;
	m_pVedioFileInputAct = ui.openfile;
	connect(m_pVedioFileInputAct, SIGNAL(triggered()), this, SLOT(SlotOpenFile()));
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(ShowContextMenu(const QPoint&)));
	
}

void SSPlayer::ShowContextMenu(const QPoint& pos) // this is a slot
{
	return;
	// for most widgets
	QPoint globalPos = mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(QStringLiteral("属性"));

	QAction* selectedItem = myMenu.exec(globalPos);
	if (selectedItem)
	{
		if (m_pInputSourceFilter)
		{
			CStreamInfo info = m_pInputSourceFilter->GetStreamInfo();
		}
	}
	else
	{

	}
}

void SSPlayer::resizeEvent(QResizeEvent* pstEvent)
{
	QSize size = pstEvent->size();
	if (m_pVideoRenderFilter)
	{
		m_pVideoRenderFilter->ResizeBackBuffer(ui.centralWidget->width(), ui.centralWidget->height());
	}
}

SSPlayer::~SSPlayer()
{
	timeEndPeriod(1);
	disconnect(m_pVedioFileInputAct, SIGNAL(triggered()), this, SLOT(SlotOpenFile()));
}

void SSPlayer::SlotOpenFile()
{
	QString strFileName = QFileDialog::getOpenFileName(NULL, QStringLiteral("打开视频文件"), "", QStringLiteral("视频文件(*.mp4 *.rm *.rmvb *.avi *.mkv *.flv *.3gp *.mpg *.mov *.ts *.mp3 *.aac *.wmv *.vob *.mxf *.dat)"), NULL, QFileDialog::DontUseNativeDialog);
	if (!strFileName.isEmpty())
	{
		if (m_pInputSourceFilter)
		{
			m_pInputSourceFilter->Stop();
			delete m_pInputSourceFilter;
		}
		CInputSourceParam param;
		param.m_strFileName = strFileName.toLocal8Bit().toStdString();
		m_pInputSourceFilter = new InputSourceFilter(param);
		m_pInputSourceFilter->ConnectFilter(m_pVideoRenderFilter);
		m_pInputSourceFilter->ConnectFilter(m_pAudioRenderFilter);
		m_pInputSourceFilter->Start();
	}
	else
	{

	}
}
