/*
 * FPV UDP Streamer based on Luckfox SDK Sample
 * Modified for Low Latency
 */

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "sample_comm.h"

// ================= НАЛАШТУВАННЯ =================
#define DEST_IP "192.168.3.98"  // <--- ВАШ MAC
#define DEST_PORT 5000
// ===============================================


/*

Usage on Receiver:

On go2rtc:

streams:
  luckfox_fpv:
    - "exec:ffmpeg -fflags nobuffer -flags low_delay -f h264 -i udp://0.0.0.0:5000 -c:v copy -rtsp_transport tcp -f rtsp {output}"


Just by gstreamer:

gst-launch-1.0 -v udpsrc port=5000 ! h264parse ! vtdec ! autovideosink sync=false


*/

int sockfd;
struct sockaddr_in dest_addr;

typedef struct _rkMpiCtx {
	SAMPLE_VI_CTX_S vi;
	SAMPLE_VO_CTX_S vo;
	SAMPLE_VPSS_CTX_S vpss;
	SAMPLE_VENC_CTX_S venc;
} SAMPLE_MPI_CTX_S;

static bool quit = false;
static void sigterm_handler(int sig) {
	fprintf(stderr, "signal %d\n", sig);
	quit = true;
}

// Функція відправки UDP
void send_udp_packet(void *data, uint32_t len) {
    uint8_t *ptr = (uint8_t *)data;
    uint32_t remaining = len;
    uint32_t mtu = 1400; // Safe MTU

    while (remaining > 0) {
        uint32_t chunk = (remaining > mtu) ? mtu : remaining;
        sendto(sockfd, ptr, chunk, 0, (const struct sockaddr *)&dest_addr, sizeof(dest_addr));
        ptr += chunk;
        remaining -= chunk;
    }
}

static void *venc_get_stream(void *pArgs) {
	SAMPLE_VENC_CTX_S *ctx = (SAMPLE_VENC_CTX_S *)(pArgs);
	RK_S32 s32Ret = RK_FAILURE;
	void *pData = RK_NULL;

	while (!quit) {
		s32Ret = SAMPLE_COMM_VENC_GetStream(ctx, &pData);
		if (s32Ret == RK_SUCCESS) {

            // ВІДПРАВКА В МЕРЕЖУ
            // pData - вказівник на дані
            // ctx->stFrame.pstPack->u32Len - довжина пакету
			send_udp_packet(pData, ctx->stFrame.pstPack->u32Len);

			SAMPLE_COMM_VENC_ReleaseStream(ctx);
		}
		// Мінімальна затримка, можна прибрати usleep якщо треба максимум швидкості
		// usleep(1000);
	}
	return RK_NULL;
}

int main(int argc, char *argv[]) {
	SAMPLE_MPI_CTX_S *ctx;
	int video_width = 2304;
	int video_height = 1296;
    // Вихідна роздільна здатність (можна зменшити для економії трафіку)
	int venc_width = 1280;
	int venc_height = 720;

	CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264; // H264 зазвичай швидше декодується
	VENC_RC_MODE_E enRcMode = VENC_RC_MODE_H264CBR;
	RK_S32 s32CamId = 0;
	RK_S32 s32BitRate = 2000 * 1024; // 2 Mbps

	// 1. Налаштування мережі
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket creation failed");
		return -1;
	}
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(DEST_PORT);
	dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

	ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
	memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

	signal(SIGINT, sigterm_handler);

    // Ініціалізація AIQ (Автоекспозиція)
    // Шлях до файлів налаштування IQ (зазвичай тут лежать)
    char *iq_file_dir = "/oem/usr/share/iqfiles";
#ifdef RKAIQ
    SAMPLE_COMM_ISP_Init(s32CamId, RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    SAMPLE_COMM_ISP_Run(s32CamId);
#endif

	if (RK_MPI_SYS_Init() != RK_SUCCESS) goto __FAILED;

	// Init VI (Camera)
	ctx->vi.u32Width = video_width;
	ctx->vi.u32Height = video_height;
	ctx->vi.s32DevId = s32CamId;
	ctx->vi.u32PipeId = ctx->vi.s32DevId;
	ctx->vi.s32ChnId = 0;
	ctx->vi.stChnAttr.stIspOpt.u32BufCount = 3;
	ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	ctx->vi.stChnAttr.u32Depth = 0; // No queue
	ctx->vi.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
	SAMPLE_COMM_VI_CreateChn(&ctx->vi);

	// Init VPSS (Scaler)
	ctx->vpss.s32GrpId = 0;
	ctx->vpss.s32ChnId = 0;
	ctx->vpss.enVProcDevType = VIDEO_PROC_DEV_RGA; // Використовуємо RGA (Hardware)
	ctx->vpss.stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;
	ctx->vpss.stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE;
    // Налаштування виходу VPSS
	ctx->vpss.stVpssChnAttr[0].enChnMode = VPSS_CHN_MODE_USER;
	ctx->vpss.stVpssChnAttr[0].enPixelFormat = RK_FMT_YUV420SP;
	ctx->vpss.stVpssChnAttr[0].u32Width = venc_width;   // Зменшуємо тут
	ctx->vpss.stVpssChnAttr[0].u32Height = venc_height;
	SAMPLE_COMM_VPSS_CreateChn(&ctx->vpss);

	// Init VENC (Encoder)
	ctx->venc.s32ChnId = 0;
	ctx->venc.u32Width = venc_width;
	ctx->venc.u32Height = venc_height;
	ctx->venc.u32Fps = 30;
	ctx->venc.u32Gop = 60; // I-frame раз на 2 секунди
	ctx->venc.u32BitRate = s32BitRate;
	ctx->venc.enCodecType = enCodecType;
	ctx->venc.enRcMode = enRcMode;
	ctx->venc.getStreamCbFunc = venc_get_stream;
	ctx->venc.stChnAttr.stVencAttr.u32Profile = 100; // High Profile
    // Low Latency Settings
	ctx->venc.stChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
	SAMPLE_COMM_VENC_CreateChn(&ctx->venc);

	// BIND: VI -> VPSS -> VENC
    // З'єднуємо камеру з VPSS
	MPP_CHN_S stSrcChn, stDestChn;
	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi.s32DevId;
	stSrcChn.s32ChnId = ctx->vi.s32ChnId;
	stDestChn.enModId = RK_ID_VPSS;
	stDestChn.s32DevId = ctx->vpss.s32GrpId;
	stDestChn.s32ChnId = ctx->vpss.s32ChnId;
	SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

    // З'єднуємо VPSS з Енкодером
	stSrcChn.enModId = RK_ID_VPSS;
	stSrcChn.s32DevId = ctx->vpss.s32GrpId;
	stSrcChn.s32ChnId = ctx->vpss.s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc.s32ChnId;
	SAMPLE_COMM_Bind(&stSrcChn, &stDestChn);

	printf("FPV UDP Streamer Running...\n");
    printf("Cam: %dx%d -> Enc: %dx%d -> %s:%d\n",
            video_width, video_height, venc_width, venc_height, DEST_IP, DEST_PORT);

	// Чекаємо завершення потоку (Ctrl+C)
	if (ctx->venc.getStreamCbFunc) {
		pthread_join(ctx->venc.getStreamThread, NULL);
	}

    // Unbind
	stSrcChn.enModId = RK_ID_VPSS;
	stSrcChn.s32DevId = ctx->vpss.s32GrpId;
	stSrcChn.s32ChnId = ctx->vpss.s32ChnId;
	stDestChn.enModId = RK_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = ctx->venc.s32ChnId;
	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

	stSrcChn.enModId = RK_ID_VI;
	stSrcChn.s32DevId = ctx->vi.s32DevId;
	stSrcChn.s32ChnId = ctx->vi.s32ChnId;
	stDestChn.enModId = RK_ID_VPSS;
	stDestChn.s32DevId = ctx->vpss.s32GrpId;
	stDestChn.s32ChnId = ctx->vpss.s32ChnId;
	SAMPLE_COMM_UnBind(&stSrcChn, &stDestChn);

	SAMPLE_COMM_VENC_DestroyChn(&ctx->venc);
	SAMPLE_COMM_VPSS_DestroyChn(&ctx->vpss);
	SAMPLE_COMM_VI_DestroyChn(&ctx->vi);

__FAILED:
	RK_MPI_SYS_Exit();
#ifdef RKAIQ
    SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
	if (ctx) free(ctx);
    close(sockfd);
	return 0;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
