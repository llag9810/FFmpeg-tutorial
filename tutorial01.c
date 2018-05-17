#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
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

int main(int argc, char *argv[]) {
    av_register_all();
    AVFormatContext *pFormatCtx = NULL;

    // Open video file.
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
        return -1;  //cannot open file
    }

    // Retrieve stream information.
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        return -1;
    }

    // Dump information onto standard error.
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find video stream.
    int videoStream = -1;

    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }

    // Get a pointer to the codec context for the video stream
    AVCodecParameters *pCodecParam = NULL;
    pCodecParam = pFormatCtx->streams[videoStream]->codecpar;

    AVCodec *pCodec = NULL;
    pCodec = avcodec_find_decoder(pCodecParam->codec_id);
    if (pCodec == NULL) {
        return -1;
    }

    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);

    if (avcodec_parameters_to_context(pCodecCtx, pCodecParam) != 0) {
        return -1;
    }

    if (avcodec_open2(pCodecCtx, pCodec, NULL) != 0) {
        return -1;
    }

    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGB = NULL;
    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();
    if (!pFrame || !pFrameRGB) {
        return -1;
    }

    uint8_t *buffer = NULL;
    int numBytes;
    // Determine required buffer size and allocate buffer
    numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);
    buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    int ret = av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width,
                                   pCodecCtx->height, 1);
    if (ret < 0) {
        return -1;
    }

    struct SwsContext *sws_ctx = NULL;
    AVPacket packet;
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_RGB24,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL
    );

    int i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // Decode video frame
            avcodec_send_packet(pCodecCtx, &packet);
            avcodec_receive_frame(pCodecCtx, pFrame);

            // Did we get a video frame?
            // Convert the image from its native format to RGB
            sws_scale(sws_ctx, pFrame->data,
                      pFrame->linesize, 0, pCodecCtx->height,
                      pFrameRGB->data, pFrameRGB->linesize);

            // Save the frame to disk
            if (++i <= 5)
                SaveFrame(pFrameRGB, pCodecCtx->width,
                          pCodecCtx->height, i);
        }

        // Free the packet that was allocated by av_read_frame
        av_packet_unref(&packet);
    }

    av_free(buffer);
    av_free(pFrameRGB);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}