
#include <QtWidgets/QApplication>
#include "SSMainConfiguration.h"
#include "SSLogger.h"
extern "C"
{
#include <libavcodec/avcodec.h>  
#include <libavformat/avformat.h>  
#include <libswscale/swscale.h> 
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
};
extern SSMainConfiguration * g_pstMainConfig;
SSMainConfiguration::SSMainConfiguration()
{

}




SSMainConfiguration& SSMainConfiguration::instance()
{
	return *g_pstMainConfig;
}

