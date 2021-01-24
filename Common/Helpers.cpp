
#include <windows.h>
#include <qdir.h>

#include "Helpers.h"

std::string getExecutableDir()
{ 
	char path[512] = "";

#ifdef _WIN32

	GetModuleFileNameA(NULL, path, sizeof(path));

#else

	ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
	if(count <= 0)
	{
		return "";
	}

#endif
	std::string strpath= path;
	std::string::size_type pos = std::string(strpath ).find_last_of( "\\/" );
	strpath=  std::string(strpath ).substr( 0, pos);
	return strpath;
}


//TODO, 测试unicode, linux下的正确性。
int makeDirRecursively(const char* strDirName)
{
	QDir stDir;
	QString strFileName(QString::fromLocal8Bit(strDirName));
	if (!stDir.exists(strFileName))
	{
		if (!stDir.mkpath(strFileName))
		{
			return -1;
		}
	}

    return 0;
}
