#include <time.h>

#include "MediaTimer.h"

#include <windows.h>
#include <Mmsystem.h>


double QkTimer::m_quadpart = -1;

QkTimer::QkTimer()
{
	
}


long long  QkTimer::now()
{
	if (m_quadpart <= 0)
	{
		LARGE_INTEGER frequency;    //计时器频率  
		QueryPerformanceFrequency(&frequency);
		m_quadpart = (double)frequency.QuadPart;//计时器频率 
	}
	LARGE_INTEGER timeMiddle;      //结束时间  
	QueryPerformanceCounter(&timeMiddle);
	double dbTime = timeMiddle.QuadPart / m_quadpart * 1000;
	return long long(dbTime);
}


static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static long long winver = 0;

static inline long long get_clockfreq(void)
{
	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}

	return clock_freq.QuadPart;
}
long long QkTimer::nowns()
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (long long)time_val;
}
