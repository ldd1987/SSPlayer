#ifndef INC_CSSFilter_H
#define INC_CSSFilter_H
#include <string>
#include <deque>
#include <mutex>
#include "Frame.h"
class CSSFilter
{
public:
	CSSFilter(std::string &name);
	virtual ~CSSFilter();
	virtual int InputData(CFrameSharePtr &frame) = 0;
	int ConnectFilter( CSSFilter*dst);
	int DisConncetFilter(CSSFilter *filter);
	int AddFrontFilter(CSSFilter *filter);
	virtual void DeliverData(CFrameSharePtr &frame);
protected:
	std::string m_strFilterName;

	std::mutex m_MutexConnectNext;   // std::lock_guard
	std::deque<CSSFilter *> m_listConnectNext;

	std::mutex m_MutexConnectFront;   // std::lock_guard
	std::deque<CSSFilter *> m_listConnectFront;
};




#endif
