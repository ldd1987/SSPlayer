#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <mutex>
#include "Frame.h"
#include <deque>
#include <qmutex.h>
struct CChannelD3DTextureInfo
{
	CChannelD3DTextureInfo()
	{
		m_nWidth = -1;
		m_nHeight = -1;
		m_pHandle = NULL;
		
	}
	int m_nWidth;
	int m_nHeight;
	int m_nBit; // bit 
	AVColorSpace colorspace;              /** video color space */
	AVColorPrimaries primaries;       /** video color primaries */
	AVColorTransferCharacteristic transfer;        /** video transfer function */
	void *m_pHandle;
};

class SSMainConfiguration
{
public:
	SSMainConfiguration();
	
	// 获取静态全局变量
	static SSMainConfiguration& instance();
	inline void SetPgmIndex(int index)
	{
		m_iPGMIndex = index;
	}

	inline int GetPgmIndex()
	{
		return m_iPGMIndex;
	}


	CChannelD3DTextureInfo GetTextInfo(int nChannel)
	{
		{
			QMutexLocker stLock(&m_TextureMutex);
			CChannelD3DTextureInfo info;
			QMap< long long, CChannelD3DTextureInfo >::iterator itor = m_deqChannelTextureInfo.find(nChannel); //找到特定的“
			if (itor != m_deqChannelTextureInfo.end())
			{
				info = *itor;
			}
			return info;
		}
		
	}
	void  SetTextInfo(int nChannel, CChannelD3DTextureInfo info)
	{
		{
			QMutexLocker stLock(&m_TextureMutex);

			QMap< long long, CChannelD3DTextureInfo >::iterator itor = m_deqChannelTextureInfo.find(nChannel); //找到特定的“
			if (itor != m_deqChannelTextureInfo.end())
			{
				*itor = info;
			}
			else
			{
				m_deqChannelTextureInfo.insert(nChannel, info);
			}
		}
		
	}

public:
	QMutex m_TextureMutex;
	QMap< long long, CChannelD3DTextureInfo >  m_deqChannelTextureInfo;
private:
	int m_iPGMIndex;
	int m_iPVMIndex;
	
};

