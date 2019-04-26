#ifndef PA_TIMER_H__
#define PA_TIMER_H__


//-----------------------------------------------------------------------
//  PA_Timer
//-----------------------------------------------------------------------
class QkTimer
{
public:
	QkTimer();
	~QkTimer(){};

	static long long now();
	static long long nowns();

private:
	static double m_quadpart;
};



#endif
