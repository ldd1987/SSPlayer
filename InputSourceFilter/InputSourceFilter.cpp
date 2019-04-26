#include "InputSourceFilter.h"
#include "../Common/MediaTimer.h"
#define BUFFERSIZE 10
InputSourceFilter::InputSourceFilter(CInputSourceParam &param) : CSSFilter(param.m_strFileName)
{
	m_InputSourceParam = param;
	m_dbPlaySpeed = 1;
	m_pstFmtCtx = 0;
	m_nVideoIndex = -1;
	m_nAudioIndex = -1;
	m_nSubscriptIndex = -1;
	m_pstAudioCodecCtx = 0;
	m_pstVideoCodecCtx = 0;
	m_eSyncType = SYNC_AUDIO;
	m_nOrgAudioChannelLayOut = -1;
	m_pstSwrContext = 0;
	m_hDecoderAudioThread = NULL;
	m_hDecoderVideoThread = NULL;
 	m_hSendVideoThread = NULL;
	m_hSendAudioThread = NULL;
	m_bPlay = true;
	m_nFirstOrgAudioPts = -1;
	m_nFirstOrgVideoPts = -1;
	m_nSyncVideoTime = -1;
	m_nFirstVideoTime = -1;
	m_nLastAudioTime = -1;
	m_nSyncTime = -1;
	m_nFirstAudioTime = -1;
	m_dbRatio = 0;
	m_bSeek = false;
	m_bReadFinish = false;
	m_pSwsCtx = 0;
	m_picture = 0;
	m_bExit = false;
	m_hEventVideoHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hEventAudioHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
}


void InputSourceFilter::DecoderAudioPacket()
{
	AVFrame * m_pstDecodedAudioFrame = av_frame_alloc();
	while (m_bExit == false)
	{
		if (m_bSeek)
		{
			av_usleep(10 * 1000); // 1ms(1);
			continue;
		}
		if (m_bStartSupportData)
		{
			if (m_bAudioFile)
			{
				if ((m_ListAudio.GetTimestampInterval() > 1500))
				{
					av_usleep(10 * 1000); // 1ms(1);
					continue;
				}
			}
			else
			{
				if (m_nVideoIndex >= 0)
				{
					if (m_deqVideoPacket.size() >= BUFFERSIZE)
					{
						av_usleep(10 * 1000); // 1ms(1);
						continue;
					}
				}
				else
				{
					if ((m_ListAudio.GetTimestampInterval() > 1500))
					{
						av_usleep(10 * 1000); // 1ms(1);
						continue;
					}
				}
			}

		}
		std::lock_guard< std::mutex> stLock(m_Mutex);
		AVPacket packet;
		av_init_packet(&packet);

		int ret = av_read_frame(m_pstFmtCtx, &packet);
		if (ret != 0)
		{
			if (ret == AVERROR_EOF || avio_feof(m_pstFmtCtx->pb))
			{
				//设置读取文件已经完毕，真正的停止播放在队列中控制
				m_bReadFinish = true;
			}
			av_packet_unref(&packet);
			av_free_packet(&packet);
			break;
		}
		if (packet.stream_index == m_nVideoIndex)
		{
			std::lock_guard< std::mutex> stLockVideo(m_MutexPacketVideo);
			m_deqVideoPacket.push_back(packet);
		}
		else if (packet.stream_index == m_nAudioIndex)
		{
			int ret = 0;
			do
			{
				int frame_finished = 0;
				{
					ret = avcodec_decode_audio4(m_pstAudioCodecCtx, m_pstDecodedAudioFrame, &frame_finished, &packet);
					if (ret > 0 && frame_finished)
					{
						////resample可能是从高到低或者从低到高。
						////从低到高的话，采样数会变得更多。
						int out_samples = m_pstDecodedAudioFrame->nb_samples;
						out_samples *= kOutSampleRate;
						out_samples /= m_pstDecodedAudioFrame->sample_rate;

						//将数字适当放大。
						out_samples *= 2;

						int out_buffer_size = out_samples * kOutBytesPerSample * kOutChannels;
						unsigned char * out_buffer = new unsigned char[out_buffer_size];
						memset(out_buffer, 0, out_buffer_size);

						int actual_out_samples = 0;
						if (0 == ResampleAudio(m_pstDecodedAudioFrame, out_buffer, out_samples, actual_out_samples))
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
							double dbPts = av_q2d(m_pstFmtCtx->streams[m_nAudioIndex]->time_base) * m_pstDecodedAudioFrame->pts * 1000LL;
							long long pts = dbPts;
							if (m_nFirstOrgAudioPts < 0)
							{
								m_nFirstOrgAudioPts = pts;
							}

							frame->m_nTimesTamp = pts;
							frame->m_nShowTime = dbPts - m_nFirstOrgAudioPts;
							double dbTotalTime = m_pstFmtCtx->duration * 1000 / AV_TIME_BASE;
							m_dbRatio = (dbPts - m_nFirstOrgAudioPts) / dbTotalTime;
							m_ListAudio.Enqueue(frame);

						}
						else
						{

						}
						delete[]out_buffer;

					}
					else
					{
						
					}
					av_frame_unref(m_pstDecodedAudioFrame);
				}


			} while (false);

			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
		else
		{
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
	}

	if (m_pstDecodedAudioFrame != NULL)
	{
		av_frame_free(&m_pstDecodedAudioFrame);
		m_pstDecodedAudioFrame = NULL;
	}
}

int InputSourceFilter::ResampleAudio(AVFrame* frame, unsigned char * out_buffer, int out_samples, int& actual_out_samples)
//统一转成48000 采样频率，是否移动到调音台
{
	const uint8_t **in = (const uint8_t **)frame->extended_data;
	int ret = 0;
	int channel_layerout = AV_CH_LAYOUT_STEREO;
	int nLayOut = av_get_default_channel_layout(frame->channels);
	if (m_nOrgAudioChannelLayOut != nLayOut)
	{
		if (m_nOrgAudioChannelLayOut >= 0)
		{
			

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


 DWORD WINAPI  InputSourceFilter::DecoderAudioFunc(LPVOID arg)
{
	 InputSourceFilter *pThis = (InputSourceFilter *)arg;
	 if (NULL == pThis)
	 {
		 return -1;
	 }
	 pThis->DecoderAudioPacket();
	 return 0;
}

 void InputSourceFilter::DecoderVideoPacket()
 {
	 if (m_bSeek)
	 {
		 return;
	 }
	 if (m_bStartSupportData == false)
	 {
		 if (m_deqVideoPacket.size() > 25)
		 {
			 m_bStartSupportData = true;
		 }
		 return;
	 }
	 if (m_bReadFinish && m_ListAudio.Size() <= 0)
	 {
		 if (m_eSyncType == SYNC_AUDIO)
		 {
			 m_eSyncType = SYNC_SYSTEM;
		 }


	 }

	 if (m_ListVideo.Size() < 10)
	 {

		 std::lock_guard< std::mutex> stLock(m_Mutex);
		 AVPacket packet;
		 {
			 std::lock_guard< std::mutex> stLocker(m_MutexPacketVideo);
			 if (m_deqVideoPacket.size() <= 0)
			 {
				 return;
			 }
			 packet = m_deqVideoPacket.front();
			 m_deqVideoPacket.pop_front();
		 }

		 int frame_finished = 0;
		 if (packet.stream_index == m_nVideoIndex)
		 {
			 AVFrame * m_pstDecodedVideoBuffer = av_frame_alloc();
			 int ret = avcodec_decode_video2(m_pstVideoCodecCtx, m_pstDecodedVideoBuffer, &frame_finished, &packet);
			 if (ret > 0 && frame_finished)
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

				
				 {
					 if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_YUV420P)
					 {
						 CFrameSharePtr stFrame = NewShareFrame();
						 stFrame->m_nWidth = width;
						 stFrame->m_nHeight = height;
						 stFrame->m_nTimesTamp = 0;
						 stFrame->m_nLen = pixels_buffer_size;
						 stFrame->AllocMem(pixels_buffer_size);
						 stFrame->m_eFrameType = eVideoFrame;
						 stFrame->m_ePixType = eYUV420P;
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
								 memcpy(pFrameData + a1, m_pstDecodedVideoBuffer->data[1] + i * nTempp, nWidhtTemp);
								 memcpy(pFrameData + a2, m_pstDecodedVideoBuffer->data[2] + i * nTempp, nWidhtTemp);
								 a1 += nWidhtTemp;
								 a2 += nWidhtTemp;
							 }
							 a0 += width;
						 }
					
						 m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);

						 long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
						 stFrame->m_nTimesTamp = nPTS;// +2000;
						 if (m_nFirstOrgVideoPts < 0)
						 {
							 m_nFirstOrgVideoPts = nPTS;
						 }
			
						 stFrame->m_nShowTime = stFrame->m_nTimesTamp - m_nFirstOrgVideoPts;
						 m_dbRatio = stFrame->m_nShowTime / double(nTotalTime);



						 m_ListVideo.Enqueue(stFrame);
					 }
					 else if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_BGRA)
					 {
						 CFrameSharePtr frame = NewShareFrame();
						 frame->m_nWidth = width;
						 frame->m_nHeight = height;
						 frame->m_nTimesTamp = 0;
						 frame->m_nLen = width * height * 4;
						 frame->AllocMem(frame->m_nLen);
						 frame->m_eFrameType = eVideoFrame;
						 frame->m_ePixType = eBGRA;
						 
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
						 m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
						 long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
						 frame->m_nTimesTamp = nPTS;// +2000;
						 if (m_nFirstOrgVideoPts < 0)
						 {
							 m_nFirstOrgVideoPts = nPTS;
						 }
						 frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
						 m_dbRatio = frame->m_nShowTime / double(nTotalTime);

						 m_ListVideo.Enqueue(frame);
					 }
					 else if (m_pstDecodedVideoBuffer->format == AV_PIX_FMT_RGBA)
					 {
						 CFrameSharePtr frame = NewShareFrame();
						 frame->m_nWidth = width;
						 frame->m_nHeight = height;
						 frame->m_nTimesTamp = 0;
						 frame->m_nLen = width * height * 4;
						 frame->AllocMem(frame->m_nLen);
						 frame->m_eFrameType = eVideoFrame;
						 frame->m_ePixType = eRGBA;
						
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
						 m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
						 long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
						 frame->m_nTimesTamp = nPTS;// +2000;
						 if (m_nFirstOrgVideoPts < 0)
						 {
							 m_nFirstOrgVideoPts = nPTS;
						 }
						 frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
						 m_dbRatio = frame->m_nShowTime / double(nTotalTime);

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
						 ConverToYUV420P(m_pstDecodedVideoBuffer, frame->GetDataPtr(), width, height);
						 m_pstDecodedVideoBuffer->pts = av_frame_get_best_effort_timestamp(m_pstDecodedVideoBuffer);
						 long long nPTS = av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * m_pstDecodedVideoBuffer->pts * 1000LL;   //轉化成ms
						 frame->m_nTimesTamp = nPTS;// +2000;
						 if (m_nFirstOrgVideoPts < 0)
						 {
							 m_nFirstOrgVideoPts = nPTS;
						 }
						 frame->m_nShowTime = frame->m_nTimesTamp - m_nFirstOrgVideoPts;
						 m_dbRatio = frame->m_nShowTime / double(nTotalTime);

						 m_ListVideo.Enqueue(frame);
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

 int InputSourceFilter::ConverToYUV420P(AVFrame* frame, unsigned char* rgb_buffer, int nWidth, int nHeight)
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

 DWORD WINAPI  InputSourceFilter::DecoderVideoFunc(LPVOID arg)
 {
	 InputSourceFilter *pThis = (InputSourceFilter *)arg;
	 if (NULL == pThis)
	 {
		 return -1;
	 }
	 while (pThis->m_bExit == false)
	 {
		 pThis->DecoderVideoPacket();
		 Sleep(2);
	 }
	 
	 return 0;
 }

 
 DWORD WINAPI  InputSourceFilter::SendAudioFunc(LPVOID arg)
 {
	 InputSourceFilter *pThis = (InputSourceFilter *)arg;
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
			 //QK_LOG(LOG_WARNING, "Wait data sucess %d", pThis->m_nIndex);
		 }
		 else if (WAIT_TIMEOUT == dwRet)
		 {
			 //QK_LOG(LOG_WARNING, "Wait data time out %d", pThis->m_nIndex);
		 }
		 else if (WAIT_FAILED == dwRet)
		 {
			 //QK_LOG(LOG_WARNING, "Wait data error %d", pThis->m_nIndex);
		 }
		 else
		 {
			 //QK_LOG(LOG_WARNING, "Wait data unknow error %d", pThis->m_nIndex);
		 }
		 pThis->SyncAudio();
	 }
	 return 0;
 }

 void InputSourceFilter::SyncVideoSyncAudio()
 {
	 do
	 {
		 CFrameSharePtr framevideo = m_ListVideo.Front();
		 if (framevideo)
		 {
			 if (framevideo->m_nTimesTamp > m_nLastAudioTime)
			 {
				 break;
			 }

			 SyncVideoSyncVideo();
		 }
	 } while (0);


 }

 void InputSourceFilter::SyncVideoSyncVideo()
 {
	 do
	 {
		 CFrameSharePtr framevideo = m_ListVideo.Front();
		 if (framevideo)
		 {
			 std::lock_guard< std::mutex> stLock(m_MutexSyncVideoTime);
			 if (m_nSyncVideoTime < 0)
			 {
				 m_nSyncVideoTime = QkTimer::now();
			 }
			 if (m_nFirstVideoTime < 0)
			 {
				 m_nFirstVideoTime = framevideo->m_nTimesTamp;
			 }

			 float dbTemp = framevideo->m_nTimesTamp - m_nFirstVideoTime;

			 quint64 nMayNowTime = (quint32)(dbTemp / m_dbPlaySpeed) + m_nSyncVideoTime;
			 quint64 nRealNowTime = QkTimer::now();

			 int nTempTime = nMayNowTime - nRealNowTime;
			 if (nTempTime > 0)
			 {
				 break;
			 }
			 framevideo->m_nTimesTamp = framevideo->m_nShowTime;
			 DeliverData(framevideo);
			 m_ListVideo.Dequeue();

		 }


	 } while (0);


 }

 void InputSourceFilter::SyncAudio()
 {
	 if (m_bStartSupportData == false)
	 {
		 if (m_ListAudio.GetTimestampInterval() > 1000)
		 {
			 m_bStartSupportData = true;

		 }
		 return;
	 }
	 if (!m_bPlay || m_bSeek)
	 {
		 return;
	 }

	 CFrameSharePtr frameaudio = m_ListAudio.Front();
	 if (frameaudio)
	 {
		 std::lock_guard< std::mutex> stLock(m_MutexSyncAudioTime);
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
			
		 }

		 m_nLastAudioTime = frameaudio->m_nTimesTamp;
		 frameaudio->m_nTimesTamp = frameaudio->m_nShowTime;
		 DeliverData(frameaudio);
		 m_ListAudio.Dequeue();
	 }
	 else
	 {
		 //Sleep(2);
	 }

 }

 DWORD WINAPI  InputSourceFilter::SendVideoFunc(LPVOID arg)
 {
	 InputSourceFilter *pThis = (InputSourceFilter *)arg;
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
			 //QK_LOG(LOG_WARNING, "Wait data sucess %d", pThis->m_nIndex);
		 }
		 else if (WAIT_TIMEOUT == dwRet)
		 {
			 //QK_LOG(LOG_WARNING, "Wait data time out %d", pThis->m_nIndex);
		 }
		 else if (WAIT_FAILED == dwRet)
		 {
			 //QK_LOG(LOG_WARNING, "Wait data error %d", pThis->m_nIndex);
		 }
		 else
		 {
			 //QK_LOG(LOG_WARNING, "Wait data unknow error %d", pThis->m_nIndex);
		 }
		 if (pThis->m_eSyncType == SYNC_AUDIO)
		 {
			 pThis->SyncVideoSyncAudio();
		 }
		 else
		 {
			 pThis->SyncVideoSyncVideo();
		 }


	 }
	 return 0;
 }


InputSourceFilter::~InputSourceFilter()
{

}

int InputSourceFilter::InputData(CFrameSharePtr &frame)
{
	return 0;
 }

int InputSourceFilter::Start()
{
	m_bStartSupportData = false;
	bool bRet = OpenFile();
	if (bRet)
	{
		m_hDecoderAudioThread = CreateThread(NULL, 0, DecoderAudioFunc, this, 0, NULL);
		m_hDecoderVideoThread = CreateThread(NULL, 0, DecoderVideoFunc, this, 0, NULL);
		m_hSendVideoThread = CreateThread(NULL, 0, SendVideoFunc, this, 0, NULL);
		m_hSendAudioThread = CreateThread(NULL, 0, SendAudioFunc, this, 0, NULL);

	}
	return 0;
}

void InputSourceFilter::Stop()
{
	m_bExit = true;
	if (m_hDecoderAudioThread)
	{
		WaitForSingleObject(m_hDecoderAudioThread, INFINITE);
		CloseHandle(m_hDecoderAudioThread);
		m_hDecoderAudioThread = NULL;
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

	Close();
	m_bStartSupportData = false;
}

CStreamInfo InputSourceFilter::GetStreamInfo()
{
	return m_StreamInfo;
}

void InputSourceFilter::Close()
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
	std::lock_guard<std::mutex> stLock(m_Mutex);
	if (m_pstAudioCodecCtx != NULL)
	{
		avcodec_close(m_pstAudioCodecCtx);
		m_pstAudioCodecCtx = NULL;
	}

	if (m_pstVideoCodecCtx != NULL)
	{

		avcodec_close(m_pstVideoCodecCtx);
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
		std::lock_guard<std::mutex> stLock(m_MutexPacketVideo);
		int nSize = m_deqVideoPacket.size();
		for (int i = 0; i < nSize; i++)
		{
			AVPacket packet = m_deqVideoPacket[i];
			av_packet_unref(&packet);
			av_free_packet(&packet);
		}
		m_deqVideoPacket.clear();
	}
}

bool InputSourceFilter::OpenFile()
{
	bool bRet = false;
	av_register_all();
	avformat_network_init();
	m_StreamInfo.m_deqAudioStream.clear();
	m_StreamInfo.m_deqVideoStream.clear();
	do 
	{
		if (avformat_open_input(&m_pstFmtCtx, m_InputSourceParam.m_strFileName.c_str(), NULL, NULL) < 0)
		{
			break;
		}
		if (avformat_find_stream_info(m_pstFmtCtx, NULL) < 0)
		{
			break;
		}
		for (unsigned i = 0; i < m_pstFmtCtx->nb_streams; i++)
		{
			if (m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				CVideoStreamInfo info;
				info.m_nStreamID = i;
				info.m_nBitRate = m_pstFmtCtx->streams[i]->codec->bit_rate;
				info.m_nWidth = m_pstFmtCtx->streams[i]->codec->width;
				info.m_nHeight = m_pstFmtCtx->streams[i]->codec->height;
				info.m_dbFrameRate = double(m_pstFmtCtx->streams[i]->codec->framerate.num) / m_pstFmtCtx->streams[i]->codec->framerate.den;
				const AVCodecDescriptor *pDes = avcodec_descriptor_get(m_pstFmtCtx->streams[i]->codec->codec_id);
				if (pDes)
				{
					info.m_strCodeID = pDes->name;
				}

				//info.m_strColorSpace = m_pstVideoCodecCtx->codec->pix_fmts
				m_StreamInfo.m_deqVideoStream.push_back(info);

				if (m_nVideoIndex < 0)
				{
					m_nVideoIndex = i;
					m_pstVideoCodecCtx = m_pstFmtCtx->streams[i]->codec;
				}
				
				
			}
			else if ( m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				CAudioStreamInfo info;
				info.m_nStreamID = i;
				info.m_nBitRate = m_pstFmtCtx->streams[i]->codec->bit_rate;
				info.m_nChannel = m_pstFmtCtx->streams[i]->codec->channels;
				info.m_nChannelLayOut = m_pstFmtCtx->streams[i]->codec->channel_layout;
				info.m_dbFrameRate = double(m_pstFmtCtx->streams[i]->codec->framerate.num) / m_pstFmtCtx->streams[i]->codec->framerate.den;
				const AVCodecDescriptor *pDes = avcodec_descriptor_get(m_pstFmtCtx->streams[i]->codec->codec_id);
				if (pDes)
				{
					info.m_strCodeID = pDes->name;
				}
				info.m_nSampleRate = m_pstFmtCtx->streams[i]->codec->sample_rate;
				//info.m_strColorSpace = m_pstVideoCodecCtx->codec->pix_fmts
				m_StreamInfo.m_deqAudioStream.push_back(info);

				if (m_nAudioIndex < 0)
				{
					m_nAudioIndex = i;
					m_pstAudioCodecCtx = m_pstFmtCtx->streams[i]->codec;
				}
				
				
			}
			else if ( m_pstFmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE)
			{
				if (m_nSubscriptIndex < 0)
				{
					m_nSubscriptIndex = i;
				}
				
			}
		}
		if (m_nVideoIndex < 0 && m_nAudioIndex < 0)
		{
			break;
		}
		if (m_nVideoIndex >= 0)
		{
			AVCodec* codec = avcodec_find_decoder(m_pstVideoCodecCtx->codec_id);
			if (codec == NULL)
			{
				break;
			}
			m_pstVideoCodecCtx->thread_type = FF_THREAD_FRAME;
			m_pstVideoCodecCtx->thread_count = 2;
			if (avcodec_open2(m_pstVideoCodecCtx, codec, NULL) < 0)
			{
				break;
			}
		}
		else
		{
			m_eSyncType = SYNC_AUDIO;
		}

		if (m_nAudioIndex >= 0)
		{
			AVCodec* codec = avcodec_find_decoder(m_pstAudioCodecCtx->codec_id);
			if (codec == NULL)
			{
				break;
			}

			if (avcodec_open2(m_pstAudioCodecCtx, codec, NULL) < 0)
			{
				break;
			}
		}
		else
		{
			m_eSyncType = SYNC_VIDEO;
		}
		if (m_nVideoIndex >= 0 && m_nAudioIndex >= 0)
		{
			AVStream * audiostream = m_pstFmtCtx->streams[m_nAudioIndex];
			AVStream *videostream = m_pstFmtCtx->streams[m_nVideoIndex];
			int nAudioStart = m_pstFmtCtx->streams[m_nAudioIndex]->start_time * av_q2d(m_pstFmtCtx->streams[m_nAudioIndex]->time_base) * 1000;
			int nVideodStart = m_pstFmtCtx->streams[m_nVideoIndex]->start_time * av_q2d(m_pstFmtCtx->streams[m_nVideoIndex]->time_base) * 1000;
			if (abs(nAudioStart - nVideodStart) > 3000)
			{
				m_eSyncType = SYNC_SYSTEM;
			}
		}
		bRet = true;
	} while (0);
	if (!bRet)
	{
		if (m_pstAudioCodecCtx != NULL)
		{
			avcodec_close(m_pstAudioCodecCtx);
			m_pstAudioCodecCtx = NULL;
		}
		if (m_pstVideoCodecCtx != NULL)
		{
			avcodec_close(m_pstVideoCodecCtx);
			m_pstVideoCodecCtx = NULL;
		}
		if (m_pstFmtCtx != NULL)
		{
			avformat_close_input(&m_pstFmtCtx);
			m_pstFmtCtx = NULL;
		}
		m_nVideoIndex = -1;
		m_nAudioIndex = -1;
	}
	return bRet;
}