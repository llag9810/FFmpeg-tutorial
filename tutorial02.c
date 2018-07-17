#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <SDL.h>
#include <SDL_thread.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);
int main(int argc, char *argv[])
{
  av_register_all();
  AVFormatContext *pFormatCtx = NULL;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
  {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }
  // open video file.
  if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
  {
    return -1; // cannot open file.
  }

  // Retrieve stream information.
  if (avformat_find_stream_info(pFormatCtx, NULL) != 0)
  {
    return -1;
  }

  av_dump_format(pFormatCtx, 0, argv[1], 0);

  int videoStream = -1;
  AVCodecContext *pCodecCtxOrig = NULL;
  AVCodecContext *pCodecCtx = NULL;

  for (int i = 0; i < pFormatCtx->nb_streams; i++)
  {
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      videoStream = i;
      break;
    }
  }

  if (videoStream == -1)
  {
    return -1;
  }

  // Get a pointer to the codec context for the video stream.
  pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

  // find the decoder for the video stream.
  AVCodec *pCodec = NULL;
  pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
  if (pCodec == NULL)
  {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }

  pCodecCtx = avcodec_alloc_context3(pCodec);

  if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
  {
    fprintf(stderr, "Couldn't copy codec context");
    return -1; // Error copying codec context
  }

  if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
  {
    return -1; // Could not open codec
  }

  AVFrame *pFrame = NULL;
  AVFrame *pFrameRGB = NULL;
  // Allocate video frame
  pFrame = av_frame_alloc();
  // Allocate an AVFrame structure
  pFrameRGB = av_frame_alloc();
  if (pFrameRGB == NULL)
    return -1;

  uint8_t *buffer = NULL;
  int numBytes;

  // Determine required buffer size and allocate buffer
  numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
  buffer = av_malloc(numBytes * sizeof(uint8_t));

  struct SwsContext *sws_ctx = NULL;

  int frameFinished = 0;
  AVPacket packet;

  // Assign appropriate parts of buffer to image planes in pFrameRGB
  // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
  // of AVPicture
  avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                 pCodecCtx->width, pCodecCtx->height);
  sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                           pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
                           NULL, NULL, NULL);

  SDL_Surface *screen;
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
  if (!screen)
  {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }
  SDL_Overlay *bmp = NULL;
  bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                             SDL_YV12_OVERLAY, screen);
  int i = 0;

  while (av_read_frame(pFormatCtx, &packet) >= 0)
  {
    if (packet.stream_index == videoStream)
    {
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      
      if (frameFinished)
      {
        SDL_LockYUVOverlay(bmp);

        AVPicture pict;
        pict.data[0] = bmp->pixels[0];
        pict.data[1] = bmp->pixels[2];
        pict.data[2] = bmp->pixels[1];

        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];

        // Convert the image into YUV format that SDL uses
        sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height,
                  pict.data, pict.linesize);

        SDL_UnlockYUVOverlay(bmp);
        rect.x = 0;
	      rect.y = 0;
	      rect.w = pCodecCtx->width;
	      rect.h = pCodecCtx->height;
	      SDL_DisplayYUVOverlay(bmp, &rect);
      }

      if (++i < 5)
      {
        SaveFrame(pFrameRGB, pCodecCtx->width,
                  pCodecCtx->height, i);
      }
    }
    av_free_packet(&packet);
  }

  // Free the RGB image
  av_free(buffer);
  av_free(pFrameRGB);

  // Free the YUV frame
  av_free(pFrame);

  // Close the codecs
  avcodec_close(pCodecCtx);
  avcodec_close(pCodecCtxOrig);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
  FILE *pFile;
  char szFilename[32];
  int y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if (pFile == NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  // Write pixel data
  for (y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}
