#include "InputSourceFilter.h"
#include "../Common/MediaTimer.h"
#include "../Common/SSLogger.h"
const int BUFFERSIZE = 125;
#include <QFileInfo>
CInputFileSource::CInputFileSource(CInputSourceParam &param) : CSSFilter(param.m_strFileName)
{
	m_nFirstAudioPts = AV_NOPTS_VALUE;
	m_nFirstVideoPts = AV_NOPTS_VALUE;
	m_InputSourceParam = param;

	m_pstFmtCtx = NULL;
	m_pstAudioCodecCtx = NULL;
	m_pstVideoCodecCtx = NULL;
	m_nVideoIndex = -1;
	m_nAudioIndex = -1;
	m_nSubscriptIndex = -1;
	m_nOrgAudioChannelLayOut = -1;
	m_nIndex = 1;
	m_nFixerPreFrameTimestemp = -1;
	m_nFixerOffset = 0;
	m_bReadFinish = false;
	m_pstSwrContext = NULL;
	m_nFrameRate = -1;
	//m_stLastNotifyTimer.reset();
	m_nLastProgressTime = 0;

	m_bPauseStatus = false;

	//
	m_nTempTime = 0;
	m_dbNowTime = 0;

	m_fLeftVolume = 0;
	m_fRightVolume = 0;

	m_bFinished = true;
	//
	m_dbLastTime = 0;
	m_dbVideoLastTime = 0;
	m_pSwsCtx = NULL;
	m_picture = NULL;

	m_nLaseFireVideoTime = 0;
	m_dbAudioLastTime = 0;
	m_nWidth = 0;
	m_nHeight = 0;
	m_dbVideoTemp = 0;
	m_dbAudioTemp = 0;
	m_bExit = false;
	m_bPause = false;
	m_hDecoderAudioThread = NULL;
	m_hDecoderVideoThread = NULL;
	m_hReadThread = NULL;
	m_hSendVideoThread = NULL;
	m_hSendAudioThread = NULL;
	m_bStartSupportData = false;
	m_nSyncTime = -1;
	m_nSyncVideoTime = -1;
	m_nLastAudioTime = -1;
	m_bExit = false;
	m_nFirstAudioTime = -1;
	m_nFirstVideoTime = -1;
	m_nLastVideoTime = -1000000;
	m_dbPlaySpeed = 1;
	m_bPlay = true;
	m_nFirstOrgAudioPts = -1;
	m_nFirstOrgVideoPts = -1;
	m_bHardWare = false;
	m_bSeek = false;
	m_mapIndex2Pro.clear();
	m_eSyncType = SYNC_AUDIO;
	m_bAudioFile = false;
	m_hEventVideoHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hEventAudioHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
}


void CInputFileSource::StartService()
{
	m_bStartSupportData = false;
	int nRet = Open();
	if (nRet == 0)
	{
		m_hReadThread = CreateThread(NULL, 0, ReadFunc, this, 0, NULL);
		m_hDecoderVideoThread = CreateThread(NULL, 0, DecoderVideoFunc, this, 0, NULL);
		m_hSendVideoThread = CreateThread(NULL, 0, SendVideoFunc, this, 0, NULL);
		m_hSendAudioThread = CreateThread(NULL, 0, SendAudioFunc, this, 0, NULL);

	}
}

DWORD WINAPI  CInputFileSource::ReadFunc(LPVOID arg)
{
	CInputFileSource *pThis = (CInputFileSource *)arg;
	if (NULL == pThis)
	{
		return -1;
	}



	while (pThis->m_bExit == false)
	{
		if (pThis->m_bSeek)
		{
			av_usleep(10 * 1000); // 1ms(1);
			continue;
		}
		if (pThis->m_bStartSupportData)
		{
			if (pThis->m_bAudioFile)
			{
				if ((pThis->m_ListAudio.GetTimestampInterval() > 1500))
				{
					av_usleep(10 * 1000); // 1ms(1);
					continue;
				}
			}
			else
			{
				if (pThis->m_nVideoIndex != -1)
				{
					if (pThis->m_deqVideoPacket.size() + pThis->m_ListVideo.Size() >= BUFFERSIZE)
					{
						av_usleep(10 * 1000); // 1ms(1);
						continue;
					}
				}
				else
				{
					if ((pThis->m_ListAudio.GetTimestampInterval() > 1500))
					{
						av_usleep(10 * 1000); // 1ms(1);
						continue;
					}
				}
			}

		}
		else
		{
			if (pThis->m_bAudioFile)
			{
				if ((pThis->m_ListAudio.GetTimestampInterval() > 200))
				{
					pThis->m_bStartSupportData = true;
					continue;
				}
			}
		}
		Sleep(3);
		QMutexLocker stLock(&pThis->m_Mutex);
		AVPacket packet;
		av_init_packet(&packet);

		int ret = av_read_frame(pThis->m_pstFmtCtx, &packet);
		if (ret != 0)
		{
			if (ret == AVERROR_EOF || avio_feof(pThis->m_pstFmtCtx->pb))
			{
				//设置读取文件已经完毕，真正的停止播放在队列中控制
				//m_pstQueue->setFinished(true);
				pThis->m_bReadFinish = true;
			}
			av_packet_unref(&packet);
			av_free_packet(&packet);
			continue;
		}
		if (packet.stream_index == pThis->m_nVideoIndex)
		{
			QMutexLocker stLockVideo(&pThis->m_MutexPacketVideo);
			pThis->m_deqVideoPacket.push_back(packet);
		}
		else if (packet.stream_index == pThis->m_nAudioIndex)
		{
			AVFrame * m_pstDecodedAudioFrame = av_frame_alloc();
			while (1)
			{
				int nRet = avcodec_receive_frame(pThis->m_pstAudioCodecCtx, m_pstDecodedAudioFrame);
				if (nRet < 0)
				{
					SS_LOG(LOG_INFO, "audio decoder error ret is:%d", ret);
					break;
				}
				else
				{
					long nDstSamples = av_rescale_rnd(m_pstDecodedAudioFrame->nb_samples, kOutSampleRate, m_pstDecodedAudioFrame->sample_rate, AV_ROUND_UP);
					long nMaxDstSamples = nDstSamples;
					if (pThis->m_pstSwrContext)
					{
						nDstSamples = av_rescale_rnd(swr_get_delay(pThis->m_pstSwrContext, m_pstDecodedAudioFrame->sample_rate) +
							m_pstDecodedAudioFrame->nb_samples, kOutSampleRate, m_pstDecodedAudioFrame->sample_rate, AV_ROUND_UP);
					}
					if (nMaxDstSamples < nDstSamples)
					{
						nMaxDstSamples = nDstSamples;
					}


					int out_buffer_size = nMaxDstSamples * kOutBytesPerSample * kOutChannels;
					unsigned char * out_buffer = new unsigned char[out_buffer_size];
					memset(out_buffer, 0, out_buffer_size);
					int actual_out_samples = 0;
					if (0 == pThis->ResampleAudio(m_pstDecodedAudioFrame, out_buffer, nDstSamples, actual_out_samples))
					{
						int actual_out_bytes = actual_out_samples * kOutBytesPerSample * kOutChannels;
						CFrameSharePtr frame = NewShareFrame();
						frame->m_nAudioChannel = kOutChannels;
						frame->m_nAudioSampleRate = kOutSampleRate;
						frame->m_nBitPerSample = kOutBytesPerSample * 8;
						frame->m_nTimesTamp = 0;
						frame->m_nLen = actual_out_bytes;
						frame->AllocMem(actual_out_bytes);
						memcpy(frame->GetDataPtr(), out_buffer, actual_out_bytes);
						frame->m_eFrameType = eAudioFrame;
					
						/*				FILE *fp = fopen("E:\\PCM.pcm", "ab+");
						fwrite(frame->m_ucData, frame->m_iLength, 1, fp);
						fclose(fp);*/
						m_pstDecodedAudioFrame->pts = av_frame_get_best_effort_timestamp(m_pstDecodedAudioFrame);
						double dbPts = av_q2d(pThis->m_pstFmtCtx->streams[pThis->m_nAudioIndex]->time_base) * m_pstDecodedAudioFrame->pts * 1000LL;
						long long pts = dbPts;
						if (pThis->m_nFirstOrgAudioPts < 0)
						{
							pThis->m_nFirstOrgAudioPts = pts;
						}
						if (pThis->m_pstFmtCtx->duration != AV_NOPTS_VALUE)
						{
							double dbTotalTime = pThis->m_pstFmtCtx->duration * 1000 / AV_TIME_BASE;
						}
						frame->m_nTimesTamp = pts;
						frame->m_nShowTime = dbPts - pThis->m_nFirstOrgAudioPts;
						pThis->m_ListAudio.Enqueue(frame);

					}
					delete[]out_buffer;
				}
			}
			int nRet = avcodec_send_packet(pThis->m_pstAudioCodecCtx, &packet);
			if (nRet < 0)
			{
				SS_LOG(LOG_INFO, "avcodec_send_packet auido error m_uiIndex : %u", pThis->m_nIndex);
			}
			if (m_pstDecodedAudioFrame != NULL)
			{
				av_frame_free(&m_pstDecodedAudioFrame);
				m_pstDecodedAudioFrame = NULL;
			}
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
		else
		{
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
	}


	return 0;
}

void CInputFileSource::SyncAudio()
{
	if (m_bReadFinish && m_bFlush)
	{
		if (m_bStartSupportData == false)
		{
			m_bStartSupportData = true;
		}
	}
	if (m_bStartSupportData == false)
	{
		return;
	}
	if (!m_bPlay || m_bSeek)
	{
		return;
	}

	CFrameSharePtr frameaudio = m_ListAudio.Front();
	if (frameaudio)
	{
		QMutexLocker stLock(&m_MutexSyncAudioTime);
		if (m_nSyncTime < 0)
		{
			m_nSyncTime = QkTimer::now();
		}
		if (m_nFirstAudioTime < 0)
		{
			m_nFirstAudioTime = frameaudio->m_nTimesTamp;
		}
		float dbTemp = frameaudio->m_nTimesTamp - m_nFirstAudioTime;
		m_nLastAudioTime = frameaudio->m_nTimesTamp;

		long long nMayNowTime = (long long)(dbTemp / m_dbPlaySpeed) + m_nSyncTime;
		long long nRealNowTime = QkTimer::now();

		int nTempTime = nMayNowTime - nRealNowTime;
		if (nTempTime > 0)
		{
			Sleep(1);
			return;
		}
		else if (nTempTime < -250 /*&& m_bDelAudioFrame*/)
		{
			SS_LOG(LOG_WARNING, "Audio slow and the time  is : %d", nTempTime);
			
		}
			m_nLastAudioTime = frameaudio->m_nTimesTamp;
			frameaudio->m_nTimesTamp = nRealNowTime;
			
			{
				DeliverData(frameaudio);
			}
			m_ListAudio.Dequeue();
			if (m_bPlayVideo == false)
			{
				CFrameSharePtr framevideo = m_ListVideo.Front();
				if (framevideo)
				{
					if (framevideo->m_nTimesTamp > m_nLastAudioTime)
					{
						SS_LOG(LOG_WARNING, "send audio time is:%lld, first video time is:%lld", m_nLastAudioTime, framevideo->m_nTimesTamp);
					}
					else
					{
						SS_LOG(LOG_WARNING, "send audio time is:%lld, first video time is:%lld", m_nLastAudioTime, framevideo->m_nTimesTamp);
						m_bPlayVideo = true;
					}
				}
			}
			
	}
	else
	{
		//Sleep(2);
	}

}

void CInputFileSource::DecoderVideo()
{
	if (!m_bPlay || m_bSeek || m_nVideoIndex < 0)
	{
		return;
	}
	if (m_bStartSupportData == false)
	{
		if (m_ListVideo.Size() >= (m_nFrameRate > 25 ? m_nFrameRate : 25))
		{
			m_bStartSupportData = true;
			m_bPlayVideo = true;
			return;
		}
	}
	else
	{
		if (m_ListVideo.Size() > 30)
		{
			return;
		}
	}

	{

		QMutexLocker stLock(&m_Mutex);
		bool bempty = false;
		AVPacket packet;
		av_init_packet(&packet);
		packet.stream_index = m_nVideoIndex;
		{
			QMutexLocker stLocker(&m_MutexPacketVideo);
			if (m_deqVideoPacket.size() <= 0)
			{
				bempty = true;
			}
			else
			{
				packet = m_deqVideoPacket.front();
				m_deqVideoPacket.pop_front();
			}
			
		}
		if (bempty)
		{
			if (m_bReadFinish)
			{
				int nRet = avcodec_send_packet(m_pstVideoCodecCtx, NULL);
				if (nRet < 0)
				{
					return;
				}
			}
			else
			{
				return;
			}
		}

		int frame_finished = 0;
		if (packet.stream_index == m_nVideoIndex)
		{
			AVFrame * m_pstDecodedVideoBuffer = av_frame_alloc();
			if (false == bempty)
			{
				int nRet = avcodec_send_packet(m_pstVideoCodecCtx, &packet);
				if (nRet < 0)
				{
					SS_LOG(LOG_INFO, "avcodec_send_packet video error m_uiIndex : %u", m_nIndex);
				}
			}
			while (1)
			{
				int nRet = avcodec_receive_frame(m_pstVideoCodecCtx, m_pstDecodedVideoBuffer);
				if (nRet < 0)
				{
					SS_LOG(LOG_INFO, "avcodec_receive_frame video error m_uiIndex : %u", m_nIndex);
					SS_LOG(LOG_INFO, "decoder video error %d, and %s", nRet, strerror(nRet));
					if (nRet == AVERROR_EOF)
					{
						m_bFlush = true;
					}
					break;
				}
				else
				{
					int width = 0;
					int height = 0;
					int nSize = 0;
					int pixels_buffer_size = 0;
					if (m_pstVideoCodecCtx)
					{
						width = m_pstVideoCodecCtx->width;
						height = m_pstVideoCodecCtx->height;
						nSize = height * width;
						pixels_buffer_size = nSize * 3 / 2;
					}

					bool bAudio = true;
					if (m_nAudioIndex == -1)
					{
						bAudio = false;
					}
					long long nTotalTime = 0;
					if (m_pstFmtCtx->duration != AV_NOPTS_VALUE)
					{
						nTotalTime = m_pstFmtCtx->duration * 1000 / AV_TIME_BASE;
					}
					if (m_bHardWare)
					{
					

					}
					else
					{
						if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_YUV420P)
						{
							CFrameSharePtr stFrame = NewShareFrame();
							stFrame->m_nWidth = width;
							stFrame->m_nHeight = height;
							stFrame->m_nTimesTamp = 0;
							stFrame->m_nLen = pixels_buffer_size;
							stFrame->m_eFrameType = eVideoFrame;
							stFrame->m_ePixType = eYUV420P;
							stFrame->colorspace = (m_pstVideoCodecCtx->colorspace == AVCOL_PRI_UNSPECIFIED) ? AVCOL_SPC_BT709 : m_pstVideoCodecCtx->colorspace;
							stFrame->color_primaries = m_pstVideoCodecCtx->color_primaries == AVCOL_PRI_UNSPECIFIED ? AVCOL_PRI_BT709 : m_pstVideoCodecCtx->color_primaries;;
							stFrame->color_range = AVCOL_RANGE_MPEG;
							stFrame->color_trc = m_pstVideoCodecCtx->color_trc == AVCOL_TRC_UNSPECIFIED ? AVCOL_TRC_BT709 : m_pstVideoCodecCtx->color_trc;;
							stFrame->hasDisplayMetadata = false;
							stFrame->hasLightMetadata = false;
							AVFrameSideData *sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
							if (sd)
							{
								stFrame->displayMetadata = *(AVMasteringDisplayMetadata *)sd->data;
								stFrame->hasDisplayMetadata = true;
							}

							sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
							if (sd)
							{
								stFrame->lightMetadata = *(AVContentLightMetadata *)sd->data;
								stFrame->hasLightMetadata = true;
							}
							if (1)
							{
								stFrame->AllocMem(pixels_buffer_size);
								int a0 = 0, i;
								int nHightTemp = height >> 1;
								int nWidhtTemp = width >> 1;
								int a1 = nSize;
								int a2 = a1 * 5 / 4;
								int nTempp = m_pstDecodedVideoBuffer->linesize[1] / 2;
								unsigned char *pFrameData = stFrame->GetDataPtr();
								for (i = 0; i < height; i++)
								{
									memcpy(pFrameData + a0, m_pstDecodedVideoBuffer->data[0] + i * m_pstDecodedVideoBuffer->linesize[0], width);
									if (i % 2 == 0)
									{
										memcpy(pFrameData + a1, m_pstDecodedVideoBuffer->data[1] + i *nTempp, nWidhtTemp);
										memcpy(pFrameData + a2, m_pstDecodedVideoBuffer->data[2] + i * nTempp, nWidhtTemp);
										a1 += nWidhtTemp;
										a2 += nWidhtTemp;
									}
									a0 += width;
								}
							}
							
							m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);

							long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
							stFrame->m_nTimesTamp = nPTS;// +2000;
							if (m_nFirstOrgVideoPts < 0)
							{
								m_nFirstOrgVideoPts = nPTS;
							}
							m_dbVideoLastTime = nPTS;
							stFrame->m_nShowTime = stFrame->m_nTimesTamp - m_nFirstOrgVideoPts;
							m_ListVideo.Enqueue(stFrame);
						}
						else if (AV_PIX_FMT_YUV420P10LE == m_pstDecodedVideoBuffer->format)
						{
							CFrameSharePtr stFrame = NewShareFrame();
							stFrame->m_nWidth = width;
							stFrame->m_nHeight = height;
							stFrame->m_nTimesTamp = 0;
							stFrame->m_nLen = width * height * 3;
							stFrame->AllocMem(width * height * 3);
							stFrame->m_eFrameType = eVideoFrame;
							stFrame->m_ePixType = eYUV420P10;
							stFrame->colorspace = (m_pstVideoCodecCtx->colorspace == AVCOL_PRI_UNSPECIFIED) ? AVCOL_SPC_BT709 : m_pstVideoCodecCtx->colorspace;
							stFrame->color_primaries = m_pstVideoCodecCtx->color_primaries == AVCOL_PRI_UNSPECIFIED ? AVCOL_PRI_BT709 : m_pstVideoCodecCtx->color_primaries;;
							stFrame->color_range = AVCOL_RANGE_MPEG;
							stFrame->color_trc = m_pstVideoCodecCtx->color_trc == AVCOL_TRC_UNSPECIFIED ? AVCOL_TRC_BT709 : m_pstVideoCodecCtx->color_trc;;
							stFrame->hasDisplayMetadata = false;
							stFrame->hasLightMetadata = false;
							stFrame->m_nPixBits = 10;
							AVFrameSideData *sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
							if (sd)
							{
								stFrame->displayMetadata = *(AVMasteringDisplayMetadata *)sd->data;
								stFrame->hasDisplayMetadata = true;
							}
							
							sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
							if (sd)
							{
								stFrame->lightMetadata = *(AVContentLightMetadata *)sd->data;
								stFrame->hasLightMetadata = true;
							}
							

							int a0 = 0, i;
							int a1 = width * height * 2;
							int a2 = a1 + width * height / 2;
							unsigned char *pFrameData = stFrame->GetDataPtr();
							for (i = 0; i < height; i++)
							{
								memcpy(pFrameData + a0, m_pstDecodedVideoBuffer->data[0] + i * m_pstDecodedVideoBuffer->linesize[0], width * 2);
								if (i % 2 == 0)
								{
									memcpy(pFrameData + a1, m_pstDecodedVideoBuffer->data[1] + i / 2 * m_pstDecodedVideoBuffer->linesize[1], width);
									memcpy(pFrameData + a2, m_pstDecodedVideoBuffer->data[2] + i / 2 * m_pstDecodedVideoBuffer->linesize[2], width);
									a1 += width;
									a2 += width;
								}
								a0 += width * 2;
							}

							m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);

							long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
							stFrame->m_nTimesTamp = nPTS;// +2000;
							if (m_nFirstOrgVideoPts < 0)
							{
								m_nFirstOrgVideoPts = nPTS;
							}

							stFrame->m_nShowTime = stFrame->m_nTimesTamp - m_nFirstOrgVideoPts;
							m_ListVideo.Enqueue(stFrame);
						}
						else if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_BGRA)
						{
							CFrameSharePtr frame = NewShareFrame();
							frame->m_nWidth = width;
							frame->m_nHeight = height;
							frame->m_nTimesTamp = 0;
							frame->m_nLen = width * height * 4;
							frame->colorspace = (m_pstVideoCodecCtx->colorspace == AVCOL_PRI_UNSPECIFIED) ? AVCOL_SPC_BT709 : m_pstVideoCodecCtx->colorspace;
							frame->color_primaries = m_pstVideoCodecCtx->color_primaries == AVCOL_PRI_UNSPECIFIED ? AVCOL_PRI_BT709 : m_pstVideoCodecCtx->color_primaries;;
							frame->color_range = AVCOL_RANGE_MPEG;
							frame->color_trc = m_pstVideoCodecCtx->color_trc == AVCOL_TRC_UNSPECIFIED ? AVCOL_TRC_BT709 : m_pstVideoCodecCtx->color_trc;;
							frame->hasDisplayMetadata = false;
							frame->hasLightMetadata = false;
							AVFrameSideData *sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
							if (sd)
							{
								frame->displayMetadata = *(AVMasteringDisplayMetadata *)sd->data;
								frame->hasDisplayMetadata = true;
							}

							sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
							if (sd)
							{
								frame->lightMetadata = *(AVContentLightMetadata *)sd->data;
								frame->hasLightMetadata = true;
							}
							frame->m_eFrameType = eVideoFrame;
							frame->m_ePixType = eBGRA;
							if (1)
							{
								frame->AllocMem(frame->m_nLen);
								if (width * 4 == m_pstDecodedVideoBuffer->linesize[0])
								{
									memcpy(frame->GetDataPtr(), m_pstDecodedVideoBuffer->data[0], frame->m_nLen);
								}
								else
								{
									unsigned char *pFrameData = frame->GetDataPtr();
									int a0 = 0;
									int nRealCopyWidth = width * 4;
									for (int i = 0; i < height; i++)
									{
										memcpy(pFrameData + a0, m_pstDecodedVideoBuffer->data[0] + i * m_pstDecodedVideoBuffer->linesize[0], nRealCopyWidth);
										a0 += nRealCopyWidth;
									}
								}
							}
							
							m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
							long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
							frame->m_nTimesTamp = nPTS;// +2000;
							if (m_nFirstOrgVideoPts < 0)
							{
								m_nFirstOrgVideoPts = nPTS;
							}
							m_dbVideoLastTime = nPTS;
							frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
							m_ListVideo.Enqueue(frame);
						}
						else if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_RGBA)
						{
							CFrameSharePtr frame = NewShareFrame();
							frame->m_nWidth = width;
							frame->m_nHeight = height;
							frame->m_nTimesTamp = 0;
							frame->m_nLen = width * height * 4;
							frame->colorspace = (m_pstVideoCodecCtx->colorspace == AVCOL_PRI_UNSPECIFIED) ? AVCOL_SPC_BT709 : m_pstVideoCodecCtx->colorspace;
							frame->color_primaries = m_pstVideoCodecCtx->color_primaries == AVCOL_PRI_UNSPECIFIED ? AVCOL_PRI_BT709 : m_pstVideoCodecCtx->color_primaries;;
							frame->color_range = AVCOL_RANGE_MPEG;
							frame->color_trc = m_pstVideoCodecCtx->color_trc == AVCOL_TRC_UNSPECIFIED ? AVCOL_TRC_BT709 : m_pstVideoCodecCtx->color_trc;;
							frame->hasDisplayMetadata = false;
							frame->hasLightMetadata = false;
							AVFrameSideData *sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
							if (sd)
							{
								frame->displayMetadata = *(AVMasteringDisplayMetadata *)sd->data;
								frame->hasDisplayMetadata = true;
							}

							sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
							if (sd)
							{
								frame->lightMetadata = *(AVContentLightMetadata *)sd->data;
								frame->hasLightMetadata = true;
							}
							frame->m_eFrameType = eVideoFrame;
							frame->m_ePixType = eRGBA;
							if (1)
							{
								frame->AllocMem(frame->m_nLen);
								if (width * 4 == m_pstDecodedVideoBuffer->linesize[0])
								{
									memcpy(frame->GetDataPtr(), m_pstDecodedVideoBuffer->data[0], frame->m_nLen);
								}
								else
								{
									unsigned char *pFrameData = frame->GetDataPtr();
									int a0 = 0;
									int nRealCopyWidth = width * 4;
									for (int i = 0; i < height; i++)
									{
										memcpy(pFrameData + a0, m_pstDecodedVideoBuffer->data[0] + i * m_pstDecodedVideoBuffer->linesize[0], nRealCopyWidth);
										a0 += nRealCopyWidth;
									}
								}
							}
							
							m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
							long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
							frame->m_nTimesTamp = nPTS;// +2000;
							if (m_nFirstOrgVideoPts < 0)
							{
								m_nFirstOrgVideoPts = nPTS;
							}
							m_dbVideoLastTime = nPTS;
							frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
							m_ListVideo.Enqueue(frame);
						}
						else
						{
							CFrameSharePtr frame = NewShareFrame();
							frame->m_nWidth = width;
							frame->m_nHeight = height;
							frame->m_nTimesTamp = 0;
							frame->m_nLen = pixels_buffer_size;
							frame->AllocMem(pixels_buffer_size);
							frame->m_eFrameType = eVideoFrame;
							frame->m_ePixType = eYUV420P;
							frame->colorspace = (m_pstVideoCodecCtx->colorspace == AVCOL_PRI_UNSPECIFIED) ? AVCOL_SPC_BT709 : m_pstVideoCodecCtx->colorspace;
							frame->color_primaries = m_pstVideoCodecCtx->color_primaries == AVCOL_PRI_UNSPECIFIED ? AVCOL_PRI_BT709 : m_pstVideoCodecCtx->color_primaries;;
							frame->color_range = AVCOL_RANGE_MPEG;
							frame->color_trc = m_pstVideoCodecCtx->color_trc == AVCOL_TRC_UNSPECIFIED ? AVCOL_TRC_BT709 : m_pstVideoCodecCtx->color_trc;;
							frame->hasDisplayMetadata = false;
							frame->hasLightMetadata = false;
							AVFrameSideData *sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
							if (sd)
							{
								frame->displayMetadata = *(AVMasteringDisplayMetadata *)sd->data;
								frame->hasDisplayMetadata = true;
							}

							sd = av_frame_get_side_data(m_pstDecodedVideoBuffer, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
							if (sd)
							{
								frame->lightMetadata = *(AVContentLightMetadata *)sd->data;
								frame->hasLightMetadata = true;
							}
							ConverToYUV420P(m_pstDecodedVideoBuffer, frame->GetDataPtr(), width, height);
							m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
							long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
							frame->m_nTimesTamp = nPTS;// +2000;
							if (m_nFirstOrgVideoPts < 0)
							{
								m_nFirstOrgVideoPts = nPTS;
							}
							m_dbVideoLastTime = nPTS;
							frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
							m_ListVideo.Enqueue(frame);
						}
					}
				}
			}
			
			
			if (m_pstDecodedVideoBuffer != NULL)
			{
				av_frame_free(&m_pstDecodedVideoBuffer);
				m_pstDecodedVideoBuffer = NULL;
			}
		}
		av_packet_unref(&packet);
		av_free_packet(&packet);
	}
}

void CInputFileSource::SyncVideoSyncVideo()
{
	if (m_bReadFinish && m_bFlush)
	{
		if (m_bStartSupportData == false)
		{
			m_bStartSupportData = true;
			m_bPlayVideo = true;
		}
	}
	if (!m_bPlay || m_bSeek || m_bPlayVideo == false || false == m_bStartSupportData)
	{
		return;
	}
	do
	{
		CFrameSharePtr framevideo = m_ListVideo.Front();
		if (framevideo)
		{
			QMutexLocker stLock(&m_MutexSyncVideoTime);
			if (m_nSyncVideoTime < 0)
			{
				m_nSyncVideoTime = QkTimer::now();
			}
			if (m_nFirstVideoTime < 0)
			{
				m_nFirstVideoTime = framevideo->m_nTimesTamp;
			}


			bool bSend = true;
			float dbTemp = framevideo->m_nTimesTamp - m_nFirstVideoTime;
			float dbDiff = framevideo->m_nTimesTamp - m_nLastVideoTime;
			

			quint64 nMayNowTime = (quint32)(dbTemp / m_dbPlaySpeed) + m_nSyncVideoTime;
			quint64 nRealNowTime = QkTimer::now();

			int nTempTime = nMayNowTime - nRealNowTime;
			if (nTempTime > 0)
			{
				break;
			}

			
			{
				{
					//	QMutexLocker stLockRatio(&m_MutexRatio);
					//	m_dbRatio = framevideo->m_nShowTime / double(framevideo->m_nTotalTime);
				}

				
				{
					m_nLastVideoTime = framevideo->m_nShowTime;
					framevideo->m_nTimesTamp = nRealNowTime;
					DeliverData(framevideo);
				}
			}
			m_ListVideo.Dequeue();

		}


	} while (0);


}

void CInputFileSource::StopService()
{
	m_bExit = true;
	if (m_hReadThread)
	{
		WaitForSingleObject(m_hReadThread, INFINITE);
		CloseHandle(m_hReadThread);
		m_hReadThread = NULL;
	}
	if (m_hDecoderVideoThread)
	{
		WaitForSingleObject(m_hDecoderVideoThread, INFINITE);
		CloseHandle(m_hDecoderVideoThread);
		m_hDecoderVideoThread = NULL;
	}
	if (m_hDecoderAudioThread)
	{
		WaitForSingleObject(m_hDecoderAudioThread, INFINITE);
		CloseHandle(m_hDecoderAudioThread);
		m_hDecoderAudioThread = NULL;
	}
	if (m_hSendVideoThread)
	{
		WaitForSingleObject(m_hSendVideoThread, INFINITE);
		CloseHandle(m_hSendVideoThread);
		m_hSendVideoThread = NULL;
	}
	if (m_hSendAudioThread)
	{
		WaitForSingleObject(m_hSendAudioThread, INFINITE);
		CloseHandle(m_hSendAudioThread);
		m_hSendAudioThread = NULL;
	}
	if (m_hEventVideoHandle)
	{
		CloseHandle(m_hEventVideoHandle);
		m_hEventVideoHandle = NULL;
	}
	if (m_hEventAudioHandle)
	{
		CloseHandle(m_hEventAudioHandle);
		m_hEventAudioHandle = NULL;
	}
	Close();
	m_bStartSupportData = false;
}

int CInputFileSource::ConverToYUV420P(AVFrame* frame, unsigned char* rgb_buffer, int nWidth, int nHeight)
//统一转成ARGB图片
{
	int ret = 0;
	if (NULL == m_pSwsCtx)
	{
		m_pSwsCtx = sws_getCachedContext(
			NULL,
			frame->width,							 //source width
			frame->height,							 //source height
			(AVPixelFormat)frame->format,    //source format
			nWidth,							 //destination width
			nHeight,							 //destination height
			AV_PIX_FMT_YUV420P,                 //destination pixel format
			SWS_POINT,               //quality algorithm.
			NULL,
			NULL,
			NULL);
	}


	if (NULL == m_pSwsCtx)
	{
		return -1;
	}

	if (NULL == m_picture)
	{
		m_picture = new AVPicture();
		avpicture_alloc(m_picture, AV_PIX_FMT_YUV420P, nWidth, nHeight);
		//	memset(picture->data[0], 0, iWidth * iHeight * 3 / 2);
	}

	ret = sws_scale(m_pSwsCtx,
		frame->data, frame->linesize, 0, frame->height, m_picture->data, m_picture->linesize);
	if (ret < 0)
	{
		sws_freeContext(m_pSwsCtx);
		avpicture_free(m_picture);
		delete m_picture;
		m_picture = NULL;
		m_pSwsCtx = NULL;
		return -1;
	}

	int a = 0, i;
	int nWidhtTemp = nWidth >> 1;
	int  nHightTemp = nHeight >> 1;
	for (i = 0; i < nHeight; i++)
	{
		memcpy(rgb_buffer + a, m_picture->data[0] + i * m_picture->linesize[0], nWidth);
		a += nWidth;
	}
	for (i = 0; i < nHightTemp; i++)
	{
		memcpy(rgb_buffer + a, m_picture->data[1] + i * m_picture->linesize[1], nWidhtTemp);
		a += nWidhtTemp;
	}
	for (i = 0; i < nHightTemp; i++)
	{
		memcpy(rgb_buffer + a, m_picture->data[2] + i * m_picture->linesize[2], nWidhtTemp);
		a += nWidhtTemp;
	}
	return 0;
}


DWORD WINAPI  CInputFileSource::SendAudioFunc(LPVOID arg)
{
	CInputFileSource *pThis = (CInputFileSource *)arg;
	if (NULL == pThis)
	{
		return -1;
	}
	while (pThis->m_bExit == false)
	{
		DWORD dwRet = WaitForSingleObject(pThis->m_hEventVideoHandle, 2);
		if (WAIT_OBJECT_0 == dwRet)
		{
			ResetEvent(pThis->m_hEventVideoHandle);
			
		}
		else if (WAIT_TIMEOUT == dwRet)
		{
			
		}
		else if (WAIT_FAILED == dwRet)
		{
			
		}
		else
		{
			
		}
		pThis->SyncAudio();
	}
	return 0;
}



DWORD WINAPI  CInputFileSource::SendVideoFunc(LPVOID arg)
{
	CInputFileSource *pThis = (CInputFileSource *)arg;
	if (NULL == pThis)
	{
		return -1;
	}

	while (pThis->m_bExit == false)
	{
		DWORD dwRet = WaitForSingleObject(pThis->m_hEventVideoHandle, 3);
		if (WAIT_OBJECT_0 == dwRet)
		{
			ResetEvent(pThis->m_hEventVideoHandle);
			
		}
		else if (WAIT_TIMEOUT == dwRet)
		{
			
		}
		else if (WAIT_FAILED == dwRet)
		{
			
		}
		else
		{
			
		}
		pThis->SyncVideoSyncVideo();

	}
	return 0;
}

DWORD WINAPI CInputFileSource::DecoderVideoFunc(LPVOID arg)
{
	CInputFileSource *pThis = (CInputFileSource *)arg;
	if (NULL == pThis)
	{
		return -1;
	}
	while (pThis->m_bExit == false)
	{
		pThis->DecoderVideo();
		Sleep(2);
	}
	return 0;
}


CInputFileSource::~CInputFileSource()
{

}



int CInputFileSource::Open()
{
	QList<QString> listProgram;
	/*av_register_all();
	avformat_network_init();*/
	QString strName = QString::fromLocal8Bit(m_InputSourceParam.m_strFileName.c_str());
	QByteArray bPath = strName.toUtf8();
	QFileInfo fileinfo;
	fileinfo = QFileInfo(bPath);
	//文件名
	QString strExternFormat = fileinfo.suffix();
	//文件后缀


	if (strExternFormat.toLower() == "mp3" || strExternFormat.toLower() == "aac")
	{
		m_bAudioFile = true;
	}
	if (avformat_open_input(&m_pstFmtCtx, bPath.data(), NULL, NULL) < 0)
	{
		goto label_error;
	}

	if (avformat_find_stream_info(m_pstFmtCtx, NULL) < 0)
	{
		goto label_error;
	}

	//	m_pstFmtCtx->programs
	int nMin = m_pstFmtCtx->duration / AV_TIME_BASE / 60;

	m_mapIndex2Pro.clear();
	char szBuf[128] = { 0 };
	for (int i = 0; i < m_pstFmtCtx->nb_programs; i++) {
		AVProgram* pro = m_pstFmtCtx->programs[i];

		bool bHasVideo = false;
		for (int j = 0; j < pro->nb_stream_indexes; j++) {
			unsigned int nStreamIndex = pro->stream_index[j];
			if (m_pstFmtCtx->streams[nStreamIndex]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
				bHasVideo = true;
				break;
			}
		}
		if (!bHasVideo) {
			continue;
		}

		m_mapIndex2Pro.insert(make_pair(i, pro->id));
		if (av_dict_count(pro->metadata)) {
			AVDictionaryEntry* entry1 = av_dict_get(pro->metadata, "", NULL, AV_DICT_IGNORE_SUFFIX);
			AVDictionaryEntry* entry2 = NULL;
			if (entry1)
			{
				entry2 = av_dict_get(pro->metadata, "", entry1, AV_DICT_IGNORE_SUFFIX);
			}

			if (entry1)
			{
				if (entry2)
				{
					snprintf(szBuf, 128, "%s[%s]", entry1->value, entry2->value);
				}
				else
				{
					snprintf(szBuf, 128, "%s", entry1->value);
				}
			}
		}
		else {
			snprintf(szBuf, 128, "%d", pro->id);
		}
		listProgram.push_back(szBuf);
	}

	int nIndex = (int)m_nIndex;
	

	if (m_mapIndex2Pro.size() >= 2)
	{
		
		AVProgram* pro = m_pstFmtCtx->programs[0];
		for (int i = 0; i < pro->nb_stream_indexes; i++)
		{
			int index = pro->stream_index[i];
			if (m_nVideoIndex < 0 && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				m_nVideoIndex = index;
				m_pstVideoCodecCtx = m_pstFmtCtx->streams[i]->codec;
			}
			else if (m_nAudioIndex < 0 && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				m_nAudioIndex = index;
				m_pstAudioCodecCtx = m_pstFmtCtx->streams[i]->codec;
			}
		}
	}
	else
	{
		
		for (unsigned i = 0; i < m_pstFmtCtx->nb_streams; i++)
		{
			if (m_nVideoIndex < 0 && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				m_nVideoIndex = i;
				m_pstVideoCodecCtx = m_pstFmtCtx->streams[i]->codec;
			}
			else if (m_nAudioIndex < 0 && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				m_nAudioIndex = i;
				m_pstAudioCodecCtx = m_pstFmtCtx->streams[i]->codec;
			}
		}
	}



	if (m_nVideoIndex != -1)
	{
		AVCodec* codec = avcodec_find_decoder(m_pstVideoCodecCtx->codec_id);
		if (codec == NULL)
		{
			goto label_error;
		}
		if (m_bHardWare)
		{
			avcodec_close(m_pstVideoCodecCtx);
			bool bAccel = true;
			switch (codec->id)
			{
			case AV_CODEC_ID_MPEG2VIDEO:
			case AV_CODEC_ID_H264:
			case AV_CODEC_ID_VC1:
			case AV_CODEC_ID_WMV3:
			case AV_CODEC_ID_HEVC:
			case AV_CODEC_ID_VP9:
			{


				bAccel = false;
				break;
			}
			default:
				bAccel = false;
				break;
			}

			if (!bAccel)
			{
				m_bHardWare = false;
				avcodec_close(m_pstVideoCodecCtx);
				m_pstVideoCodecCtx = m_pstFmtCtx->streams[m_nVideoIndex]->codec;
				AVCodec* codec = avcodec_find_decoder(m_pstVideoCodecCtx->codec_id);
				if (codec == NULL)
				{
					goto label_error;
				}
			}
		}


		/////////////////////////////////////////
		/*if (m_bHardWare == false)
		{
		m_pstVideoCodecCtx->thread_type = FF_THREAD_FRAME;
		m_pstVideoCodecCtx->thread_count = 2;
		}*/
		AVDictionary *opts = NULL;
		av_dict_set(&opts, "threads", "auto", 0);
		av_dict_set(&opts, "refcounted_frames", "1", 0);
		if (avcodec_open2(m_pstVideoCodecCtx, codec, &opts) < 0)
		{
			goto label_error;
		}
		if (m_bAudioFile)
		{
			m_eSyncType = SYNC_AUDIO;
			m_bPlayVideo = true;
		}
		if (m_pstFmtCtx->streams[m_nVideoIndex]->avg_frame_rate.num *m_pstFmtCtx->streams[m_nVideoIndex]->avg_frame_rate.den > 0)
		{
			m_nFrameRate = m_pstFmtCtx->streams[m_nVideoIndex]->avg_frame_rate.num / m_pstFmtCtx->streams[m_nVideoIndex]->avg_frame_rate.den;//?????
		}
		else if (m_pstFmtCtx->streams[m_nVideoIndex]->r_frame_rate.den *m_pstFmtCtx->streams[m_nVideoIndex]->r_frame_rate.num > 0)
		{
			m_nFrameRate = m_pstFmtCtx->streams[m_nVideoIndex]->r_frame_rate.num / m_pstFmtCtx->streams[m_nVideoIndex]->r_frame_rate.den;//?????
		}
		else
		{
			m_nFrameRate = 25;
		}

		SS_LOG(LOG_WARNING, " width is:%d, height is:%d, fps is:%d",  m_pstVideoCodecCtx->width, m_pstVideoCodecCtx->height, m_nFrameRate);

		m_nWidth = m_pstVideoCodecCtx->width;
		m_nHeight = m_pstVideoCodecCtx->height;
		
	}
	else
	{
		m_bFlush = true;
		m_bPlayVideo = true;
		m_eSyncType = SYNC_AUDIO;
	}

	if (m_nAudioIndex != -1)
	{
		AVCodec* codec = avcodec_find_decoder(m_pstAudioCodecCtx->codec_id);
		if (codec == NULL)
		{
			goto label_error;
		}

		if (avcodec_open2(m_pstAudioCodecCtx, codec, NULL) < 0)
		{
			goto label_error;
		}
	}
	else
	{
		m_eSyncType = SYNC_VIDEO;
		m_bPlayVideo = true;

	}

	m_bExit = false;
	m_bPause = false;
	if (m_nVideoIndex >= 0 && m_nAudioIndex >= 0)
	{
		m_dbVideoTemp = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * 1000LL;
		m_dbAudioTemp = av_q2d(m_pstFmtCtx->streams[m_nAudioIndex]->time_base) * 1000LL;

		AVStream * audiostream = m_pstFmtCtx->streams[m_nAudioIndex];
		AVStream *videostream = m_pstFmtCtx->streams[m_nVideoIndex];
		int nAudioStart = m_pstFmtCtx->streams[m_nAudioIndex]->start_time * av_q2d(m_pstFmtCtx->streams[m_nAudioIndex]->time_base) * 1000;
		int nVideodStart = m_pstFmtCtx->streams[m_nVideoIndex]->start_time * av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * 1000;
		if (abs(nAudioStart - nVideodStart) > 3000)
		{
			m_eSyncType = SYNC_SYSTEM;
			m_bPlayVideo = true;
		}
	}
	if (false)
	{
		m_eSyncType = SYNC_SYSTEM;
		m_bPlayVideo = true;
	}

	return 0;

label_error:
	SS_LOG(LOG_WARNING, "open  error.");
	if (m_pstFmtCtx != NULL)
	{
		avformat_close_input(&m_pstFmtCtx);
		m_pstFmtCtx = NULL;
	}
	m_nVideoIndex = -1;
	m_nAudioIndex = -1;
	m_pstAudioCodecCtx = NULL;
	m_pstVideoCodecCtx = NULL;

	return -1;
}

int CInputFileSource::InputData(CFrameSharePtr &frame)
{
	return 0;
}


int CInputFileSource::ResampleAudio(AVFrame* frame, unsigned char* out_buffer, int out_samples, int& actual_out_samples)
//统一转成48000 采样频率，是否移动到调音台
{
	const unsigned char **in = (const unsigned char **)frame->extended_data;
	int ret = 0;
	int channel_layerout = AV_CH_LAYOUT_STEREO;
	int nLayOut = av_get_default_channel_layout(frame->channels);
	if (m_nOrgAudioChannelLayOut != nLayOut)
	{
		if (m_nOrgAudioChannelLayOut >= 0)
		{
			SS_LOG(LOG_WARNING, "Audio decoder error .the org lay out is:%d, new layout is :%d", m_nOrgAudioChannelLayOut, nLayOut);

		}
		if (m_pstSwrContext)
		{
			swr_free(&m_pstSwrContext);
			m_pstSwrContext = NULL;
		}
	}

	if (m_pstSwrContext == NULL)
	{
		long in_channel_layout = av_get_default_channel_layout(frame->channels);
		struct SwrContext* swr_context = swr_alloc_set_opts(
			NULL,
			AV_CH_LAYOUT_STEREO,								//out channel layout
			AV_SAMPLE_FMT_S16,									//out format
			kOutSampleRate,										//out sample rate
			in_channel_layout,									//in channel layout
			(AVSampleFormat)frame->format,       //in format.
			frame->sample_rate,					//in sample rate
			NULL,
			NULL);

		if (NULL == swr_context)
		{
			return -1;
		}

		ret = swr_init(swr_context);
		if (ret)
		{
			swr_free(&swr_context);
			return -1;
		}
		m_pstSwrContext = swr_context;
		m_nOrgAudioChannelLayOut = nLayOut;
	}

	ret = swr_convert(m_pstSwrContext,
		(unsigned char**)&out_buffer,
		out_samples, //buffer长度能容纳每个通道多少个采样，输出缓冲器中每个通道的采样数
		in, //number of input samples available in one channel
		frame->nb_samples   //每个通道的采样数
		);

	if (ret <= 0)
	{
		swr_free(&m_pstSwrContext);
		m_pstSwrContext = NULL;
		return -1;
	}

	actual_out_samples = ret;

	return 0;
}

void  CInputFileSource::SetOutFormat(int nFormatType)
{

}

int CInputFileSource::Close()
{

	if (m_pSwsCtx)
	{
		sws_freeContext(m_pSwsCtx);
		m_pSwsCtx = NULL;
	}

	if (m_picture)
	{
		avpicture_free(m_picture);
		delete m_picture;
		m_picture = NULL;
	}
	QMutexLocker stLock(&m_Mutex);
	if (m_pstAudioCodecCtx != NULL)
	{
		avcodec_close(m_pstAudioCodecCtx);
		m_pstAudioCodecCtx = NULL;
	}

	if (m_pstVideoCodecCtx != NULL)
	{

		avcodec_close(m_pstVideoCodecCtx);
		if (m_bHardWare)
		{

		}
		m_pstVideoCodecCtx = NULL;

	}


	if (m_pstFmtCtx != NULL)
	{
		avformat_close_input(&m_pstFmtCtx);
		m_pstFmtCtx = NULL;
	}

	if (m_pstSwrContext != NULL)
	{
		swr_free(&m_pstSwrContext);
		m_pstSwrContext = NULL;
	}

	m_ListAudio.Clear();
	m_ListVideo.Clear();
	{
		QMutexLocker stLock(&m_MutexPacketVideo);
		int nSize = m_deqVideoPacket.size();
		for (int i = 0; i < nSize; i++)
		{
			AVPacket packet = m_deqVideoPacket[i];
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
		m_deqVideoPacket.clear();
	}
	return 0;

}

bool CInputFileSource::IsProcessOver()
{
	QMutexLocker stLock1(&m_Mutex);
	QMutexLocker stLock(&m_MutexPacketVideo);

	if (m_bReadFinish == true && m_ListAudio.Size() <= 0 && m_bFlush && m_ListVideo.Size() <= 0 &&  m_deqVideoPacket.size() <= 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

double CInputFileSource::GetSourceAllTime()
{
	return 1000;
}

int CInputFileSource::SetSourceSeek(double dbRatio)
{
	QMutexLocker stLock(&m_Mutex);
	double dbRatioOrg = dbRatio;
	m_bSeek = true;
	m_bStartSupportData = false;
	m_bFlush = false;
	m_bReadFinish = false;
	if (dbRatio < 0)
	{
		return -1;
	}
	if (dbRatio >= 1)
	{
		dbRatio = 1;
	}
	if (m_pstFmtCtx == NULL)
	{
		return 0;
	}
	long long timestamp = m_pstFmtCtx->duration;
	timestamp *= dbRatio;
	SS_LOG(LOG_WARNING, "setFileSeek timestamp : %lld , druation is:%lld", timestamp, m_pstFmtCtx->duration);

	AVRational ra;
	ra.num = 1;
	ra.den = AV_TIME_BASE;
	bool bSeekSucess = false;
	if (m_pstFmtCtx->start_time != AV_NOPTS_VALUE)
	{
		timestamp = timestamp + m_pstFmtCtx->start_time;
	}
	int64_t seek_target = timestamp;
	int64_t seek_min = INT64_MIN;
	int64_t seek_max = INT64_MAX;
	//timestamp = av_rescale_q(timestamp, ra, m_pstFmtCtx->streams[m_nAudioIndex]->time_base);
	if (NULL != m_pstVideoCodecCtx)
	{
		avcodec_flush_buffers(m_pstVideoCodecCtx);
	}

	if (NULL != m_pstAudioCodecCtx)
	{
		avcodec_flush_buffers(m_pstAudioCodecCtx);
	}
	m_ListVideo.Clear();
	m_ListAudio.Clear();
	m_nFirstAudioTime = -1;
	m_nFirstVideoTime = -1;
	m_nLastVideoTime = -10000000;
	m_nSyncTime = -1;
	m_nSyncVideoTime = -1;
	m_nLastAudioTime = -1;
	{
		QMutexLocker stLock(&m_MutexPacketVideo);
		int nSize = m_deqVideoPacket.size();
		for (int i = 0; i < nSize; i++)
		{
			AVPacket packet = m_deqVideoPacket[i];
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
		m_deqVideoPacket.clear();
	}

	if (dbRatioOrg >= 1)
	{
		/*m_bReadFinish = true;
		m_bFlush = true;*/
	}
	int nRet = avformat_seek_file(m_pstFmtCtx, -1, seek_min, seek_target, seek_max, 0);
	if (nRet >= 0)
	{
		bSeekSucess = true;
	}
	else
	{
		SS_LOG(LOG_WARNING, "avformat_seek_file %s error.", m_strPath.toLocal8Bit().toStdString().c_str());
	}
	m_dbVideoLastTime = 0;
	m_dbAudioLastTime = 0;
	m_bSeek = false;
	return 0;
}


void CInputFileSource::SetPlaySpeed(double dbSpeed)
{

	m_bSeek = true;
	QMutexLocker stLock(&m_MutexSyncVideoTime);
	m_nSyncVideoTime = -1;
	m_nFirstVideoTime = -1;
	QMutexLocker stLockEx(&m_MutexSyncAudioTime);
	m_nSyncTime = -1;
	m_nFirstAudioTime = -1;

	m_dbPlaySpeed = dbSpeed;
	m_bSeek = false;
}

void CInputFileSource::SetPsIndex(int index) {
	bool bAudioSet = false, bVideoSet = false; // 使用 ps 中的第一个音频和视频
	for (int i = 0; i < m_pstFmtCtx->nb_programs; i++) {
		AVProgram* pro = m_pstFmtCtx->programs[i];
		if (pro->id == m_mapIndex2Pro[index]) {
			for (int j = 0; j < pro->nb_stream_indexes; j++) {
				int stream_id = pro->stream_index[j];
				if (!bAudioSet && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
					m_nAudioIndex = stream_id;
					bAudioSet = true;
				}
				else if (!bVideoSet && m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					m_nVideoIndex = stream_id;
					bVideoSet = true;
				}
			}
		}
	}
}



void CInputFileSource::SetPlay(bool bPlay)
{
	if (bPlay && m_bPlay == false)
	{
		m_nFirstAudioTime = -1;
		m_nFirstVideoTime = -1;
		m_nLastVideoTime = -10000000;
		m_nSyncTime = -1;
		m_nSyncVideoTime = -1;
		m_nLastAudioTime = -1;
	}
	m_bPlay = bPlay;
}