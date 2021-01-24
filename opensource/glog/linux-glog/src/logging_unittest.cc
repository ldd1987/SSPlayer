// Copyright (c) 2002, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Ray Sidney

//#include "config_for_unittests.h"
#include "utilities.h"

#include <fcntl.h>
#ifdef HAVE_GLOB_H
# include <glob.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>

//#include "base/commandlineflags.h"
#include "glog/logging.h"
#include "glog/raw_logging.h"
//#include "googletest.h"

//#pragma comment(lib,"E:\\renew\\trunk\\opensource\\glog-0.3.3_custom_version\\Debug\\libglog_static.lib")


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


class MyClass
{
public:
	void test()
	{
		LOG(WARNING) <<"log test rand(): " << rand() <<std::endl;

	}
};

#define PA_InfoLog(fmt,...)\
	do { \	char buffer[4096]; \
	_snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
	LOG(INFO) << buffer << "\n"; \
	} while (0)


int main(int argc, char** argv)
{
	std::string logging_path = getExecutableDir() + "\\log\\";

	google::InitGoogleLogging(argv[0]);
	google::SetLogDestination(google::INFO, logging_path.c_str());
	FLAGS_log_dir = logging_path;

	FLAGS_max_log_size = 1;

	FLAGS_minloglevel = google::WARNING;

	FLAGS_alsologtostderr = true;

	FLAGS_colorlogtostderr = true;
	//GLOG_log_dir 

		//FLAGS_stderrthreshold = google::NUM_SEVERITIES;

	LOG(ERROR) <<"log test rand(): " << rand() <<std::endl;

	//LOG(FATAL) << "fatal error ";
	
	MyClass obj;
	while(1)
	{
		//FLAGS_stderrthreshold = google::INFO;
	//	RAW_LOG(WARNING, "hello world rand: %d", rand() );
	//	LOG(WARNING) <<"log test rand(): " << rand() <<std::endl;

	//	PA_InfoLog("a, %d, b %d, c%d", 1, 2, 3);

			//LOG(INFO) <<"log test1 rand(): " << rand() <<std::endl;
		LOG(WARNING) <<"log test rand(): " << rand() <<std::endl;
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}
	return 0;
}
