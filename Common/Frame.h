#ifndef INC_FRAME_H
#define INC_FRAME_H
#include "utils.h"
#include <memory>
#include <deque>
#include<mutex>
class CFrame
{
public:

	CFrame();
	CFrame(CFrame&frame);
	~CFrame();
	inline unsigned char *GetDataPtr()
	{
		return m_pData;
	}
	bool AllocMem(long nSize);
	bool FreeMem();
public:
	FrameType m_eFrameType;

	int m_nWidth;
	int m_nHeight;
	int m_nLen;
	PixType m_ePixType;
	int m_nAudioSampleRate;
	int m_nAudioChannel;
	long long m_nTimesTamp;
	long m_nBitPerSample;
	long long m_nShowTime;
private:
	unsigned char *m_pData;
};

typedef std::shared_ptr<CFrame> CFrameSharePtr;

inline CFrameSharePtr NewShareFrame()
{
	return CFrameSharePtr(new CFrame);
}

// Ç³¿½±´
inline CFrameSharePtr CloneSharedFrame(CFrameSharePtr& stFrame, bool bCopyData = false)
{
	if (stFrame)
	{
		CFrameSharePtr stNewFrame = CFrameSharePtr(new CFrame(*stFrame));
		if (bCopyData && stFrame->GetDataPtr())
		{
			stNewFrame->AllocMem (stNewFrame->m_nLen);
			memcpy(stNewFrame->GetDataPtr(), stFrame->GetDataPtr(), stFrame->m_nLen);
		}
		return stNewFrame;
	}
	else
	{
		return NULL;
	}
}


class CFrameSharePtrQueue
{
public:
	CFrameSharePtrQueue();
	~CFrameSharePtrQueue();

	bool IsEmpty();
	void EnqueueFront(CFrameSharePtr& element);
	void Enqueue(CFrameSharePtr& element);
	void ForntQueue(CFrameSharePtr& element);

	CFrameSharePtr Dequeue();
	CFrameSharePtr Front();
	CFrameSharePtr Back();

	long long GetBytes();
	int Size();
	void Clear();

	long long FrontTimetemp();
	long long	GetTimestampInterval();

private:
	std::deque<CFrameSharePtr> m_listFrameHolder;
	std::mutex m_stFrameListMutex;
	long long					m_nTotalBytes;
};

#endif
