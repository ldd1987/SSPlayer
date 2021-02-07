#include "SSPlayer.h"
#include <QFileDialog>
#include <QResizeEvent>
#include "../Common/Filter.h"
#include "../InputSourceFilter/InputSourceFilter.h"
#include "../VideoRenderFilter/VideoRenderFilter.h"
#include "../AudioRenderFilter/AudioRenderFilter.h"
#include "../Common/SSLogger.h"
#include "../Common/Helpers.h"
#include "../Common/SSMainConfiguration.h"
SSMainConfiguration * g_pstMainConfig = NULL;

SSPlayer::SSPlayer(QWidget *parent)
	: QMainWindow(parent)
{
	g_pstMainConfig = new SSMainConfiguration();
	initLoggingSystem(__argc, __argv);
	timeBeginPeriod(1);
	ui.setupUi(this);
	ui.widget->setFixedSize(1920, 1080);
	ui.widget->move(0, 0);
	ui.widget->show();
	std::string strName = "dx11-render";
	m_pVideoRenderFilter = new VideoRenderFilter(0,ui.widget, strName, true);

	strName = "audio-render";
	m_pAudioRenderFilter = new AudioRenderFilter(strName);
	m_pInputSourceFilter =0;
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
	myMenu.addAction(QStringLiteral("����"));

	QAction* selectedItem = myMenu.exec(globalPos);
	if (selectedItem)
	{
		if (m_pInputSourceFilter)
		{
			//CStreamInfo info = m_pInputSourceFilter->GetStreamInfo();
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
		m_pVideoRenderFilter->ResizeBackBuffer(ui.widget->width(), ui.widget->height());
	}
	if (m_pVideoRenderFilter)
	{
		m_pVideoRenderFilter->ResizeBackBuffer(ui.widget->width(), ui.widget->height());
	}
}

SSPlayer::~SSPlayer()
{
	timeEndPeriod(1);
	disconnect(m_pVedioFileInputAct, SIGNAL(triggered()), this, SLOT(SlotOpenFile()));
}

void SSPlayer::SlotOpenFile()
{
	QString Video = QString::fromLocal8Bit("��Ƶ�ļ�(*.mp4 *.rm *.rmvb *.avi *.mkv *.flv *.3gp *.mpg *.mov *.ts *.wmv *.vob *.mxf *.dat *.webm)");
	QString Image = QString::fromLocal8Bit("��Ƶ�ļ�(*.mp3 *.aac)");
	QFileDialog stFileDialog(NULL, QString::fromLocal8Bit("���ļ�"), NULL);
	QStringList filters;
	//���ֻ����ӵ��б����
	filters << Video << Image;
	stFileDialog.setNameFilters(filters);
	stFileDialog.setFileMode(QFileDialog::ExistingFile);
	//��ʵ������ʵ���ļ����ļ��еĻ�ѡ
	if (stFileDialog.exec() == QDialog::Accepted)
	{
		QStringList list = stFileDialog.selectedFiles();
		QString strFileName = "";
		if (list.size() > 0)
		{
			strFileName = list[0];
		}
		
			if (!strFileName.isEmpty())
			{
				
				if (m_pInputSourceFilter)
				{
					m_pInputSourceFilter->StopService();
					delete m_pInputSourceFilter;
				}
				CInputSourceParam param;
				param.m_strFileName = strFileName.toLocal8Bit().toStdString();
				m_pInputSourceFilter = new CInputFileSource(param);
				
				m_pInputSourceFilter->ConnectFilter(m_pVideoRenderFilter);
				m_pInputSourceFilter->ConnectFilter(m_pAudioRenderFilter);
				m_pInputSourceFilter->StartService();
			}
	}
	else
	{

	}
}
void initLoggingSystem(int argc, char* argv[])
{
	(void)argc;
	// ������С����־�ȼ�
	int iMinLogLevel = google::GLOG_WARNING; //Ĭ��ֵ
	FLAGS_v = 0;
	std::string strLoggingPath = getExecutableDir() + "\\log\\";
	makeDirRecursively(strLoggingPath.c_str());
	google::InitGoogleLogging(argv[0]);
	google::SetLogDestination(google::GLOG_INFO, strLoggingPath.c_str());
	FLAGS_log_dir = strLoggingPath;
	FLAGS_max_log_size = 10; //���log�ļ��Ĵ�С��10M�ֽ�
	FLAGS_minloglevel = iMinLogLevel;
	FLAGS_logbuflevel = google::GLOG_INFO; //ֻ��buf info�ģ���������sync.
	FLAGS_logbufsecs = 900;
	FLAGS_logtostderr = false;
	//���е���־���������ڿ���̨
	FLAGS_stderrthreshold = google::NUM_SEVERITIES;

}
