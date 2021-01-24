/*
 * PA_Logger.cpp
 *
 *  Created on: 2012-11-1
 *      Author: Administrator
 */

#include "SSLogger.h"

int g_nSetLogLevel = LOG_WARNING;

void SetQKlogLevel(int nLevel)
{
	g_nSetLogLevel = nLevel;
}
int GetQKlogLevel()
{
	return g_nSetLogLevel;
}
//void initLoggingSystem(int argc, char* argv[])
//{
//	// 
//	QkServerConfiguration stConfiguration = QkServerConfiguration::instance();
//
//	// 设置最小的日志等级
//	int iMinLogLevel = google::GLOG_INFO; //默认值
//	if (stConfiguration.getMinLogLevel() == "warning")
//	{
//		iMinLogLevel = google::GLOG_WARNING;
//	}
//	else if (stConfiguration.getMinLogLevel() == "error")
//	{
//		iMinLogLevel = google::GLOG_ERROR;
//	}
//	else if (stConfiguration.getMinLogLevel() == "fatal")
//	{
//		iMinLogLevel = google::GLOG_FATAL;
//	}
//
//	if (stConfiguration.getMinLogLevel() == "debug")
//	{
//		FLAGS_v = 1;
//	}
//	else
//	{
//		FLAGS_v = 0;
//	}
//
//	std::string strLoggingPath = getExecutableDir() + "\\log\\";
//	makeDirRecursively(strLoggingPath.c_str());
//
//	google::InitGoogleLogging(argv[0]);
//	google::SetLogDestination(google::GLOG_INFO, strLoggingPath.c_str());
//
//
//	FLAGS_log_dir = strLoggingPath;
//	FLAGS_max_log_size = 10; //最大log文件的大小，10M字节
//	FLAGS_minloglevel = iMinLogLevel;
//	FLAGS_logbuflevel = google::GLOG_INFO; //只有buf info的，其他都是sync.
//	FLAGS_logbufsecs = 900;
//	FLAGS_logtostderr = false;
//
//
//	if (0)
//	{	//是否开启控制台日志
//		FLAGS_alsologtostderr = true;
//		FLAGS_colorlogtostderr = true;
//		FLAGS_stderrthreshold = google::GLOG_INFO;
//	}
//	else
//	{
//		//所有的日志都不出现在控制台
//		FLAGS_stderrthreshold = google::NUM_SEVERITIES;
//	}
//
//}
