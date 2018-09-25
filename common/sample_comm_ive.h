#ifndef __SAMPLE_COMM_IVE_H__
#define __SAMPLE_COMM_IVE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>


#include "hi_common.h"
#include "hi_debug.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_ive.h"
#include "hi_comm_vi.h"
#include "hi_comm_vo.h"
#include "hi_comm_vgs.h"

#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_ive.h"
#include "sample_comm.h"

#define VIDEO_WIDTH 352
#define VIDEO_HEIGHT 288
#define IVE_ALIGN 16
#define IVE_CHAR_CALW 8
#define IVE_CHAR_CALH 8
#define IVE_CHAR_NUM (IVE_CHAR_CALW *IVE_CHAR_CALH)
#define IVE_FILE_NAME_LEN 256

#define SAMPLE_ALIGN_BACK(x, a)     ((a) * (((x) / (a))))
#define SAMPLE_CHECK_EXPR_RET(expr, ret, fmt...)\
do\
{\
	if(expr)\
	{\
		SAMPLE_PRT(fmt);\
		return (ret);\
	}\
}while(0)
#define SAMPLE_CHECK_EXPR_GOTO(expr, label, fmt...)\
do\
{\
	if(expr)\
	{\
		SAMPLE_PRT(fmt);\
		goto label;\
	}\
}while(0)

typedef struct hiSAMPLE_IVE_VI_VO_CONFIG_S
{
	SAMPLE_VI_CONFIG_S stViConfig;
	VO_INTF_TYPE_E enVoIntfType;
	VIDEO_NORM_E enNorm;
	PIC_SIZE_E enPicSize;
}SAMPLE_IVE_VI_VO_CONFIG_S;

typedef struct hiSAMPLE_IVE_RECT_S
{
	POINT_S astPoint[4];
}SAMPLE_IVE_RECT_S;

typedef struct hiSAMPLE_RECT_ARRAY_S
{
    HI_U16 u16Num;
    SAMPLE_IVE_RECT_S astRect[50];
}SAMPLE_RECT_ARRAY_S;

typedef struct hiIVE_LINEAR_DATA_S
{
	HI_S32 s32LinearNum;
	HI_S32 s32ThreshNum;
	POINT_S *pstLinearPoint;
}IVE_LINEAR_DATA_S;


//free mmz 
#define IVE_MMZ_FREE(phy,vir)\
do{\
	if ((0 != (phy)) && (NULL != (vir)))\
	{\
		 HI_MPI_SYS_MmzFree((phy),(vir));\
		 (phy) = 0;\
		 (vir) = NULL;\
	}\
}while(0)

#define IVE_CLOSE_FILE(fp)\
do{\
    if (NULL != (fp))\
    {\
        fclose((fp));\
        (fp) = NULL;\
    }\
}while(0)

#define SAMPLE_VI_PAUSE()\
do {\
	printf("---------------press the Enter key to exit!---------------\n");\
    getchar();\
} while (0)
/******************************************************************************
* function : Mpi init
******************************************************************************/
HI_VOID SAMPLE_COMM_IVE_CheckIveMpiInit(HI_VOID);
/******************************************************************************
* function : Mpi exit
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_IveMpiExit(HI_VOID);
/******************************************************************************
* function :VGS Add draw rect job
******************************************************************************/
HI_S32 SAMPLE_COMM_VGS_AddDrawRectJob(VGS_HANDLE VgsHandle, IVE_IMAGE_S *pstSrc, IVE_IMAGE_S *pstDst, 
	RECT_S *pstRect, HI_U16 u16RectNum);
/******************************************************************************
* function : Call vgs to fill rect
******************************************************************************/
HI_S32 SAMPLE_COMM_VGS_FillRect(VIDEO_FRAME_INFO_S *pstFrmInfo, SAMPLE_RECT_ARRAY_S *pstRect,HI_U32 u32Color);
/******************************************************************************
* function :Read file
******************************************************************************/                                                
HI_S32 SAMPLE_COMM_IVE_ReadFile(IVE_IMAGE_S *pstImg, FILE *pFp);
/******************************************************************************
* function :Write file
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_WriteFile(IVE_IMAGE_S *pstImg, FILE *pFp);
/******************************************************************************
* function :Calc stride
******************************************************************************/
HI_U16 SAMPLE_COMM_IVE_CalcStride(HI_U16 u16Width, HI_U8 u8Align);
/******************************************************************************
* function : Start BT1120 720P vi/vo/venc
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_BT1120_720P_PreView(SAMPLE_IVE_VI_VO_CONFIG_S *pstViVoConfig,
	HI_BOOL bOpenVi,HI_BOOL bOpenViExt,HI_BOOL bOpenVo,HI_BOOL bOpenVenc,HI_BOOL bOpenVpss,HI_U32 u32VpssChnNum);
/******************************************************************************
* function : Stop BT1120 720P vi/vo/venc
******************************************************************************/
HI_VOID SAMPLE_COMM_IVE_BT1120_720P_Stop(SAMPLE_IVE_VI_VO_CONFIG_S *pstViVoConfig,
	HI_BOOL bOpenVi,HI_BOOL bOpenViExt,HI_BOOL bOpenVo,HI_BOOL bOpenVenc,HI_BOOL bOpenVpss,HI_U32 u32VpssChnNum);
/******************************************************************************
* function : Copy blob to rect
******************************************************************************/
HI_VOID SAMPLE_COMM_IVE_BlobToRect(IVE_CCBLOB_S *pstBlob, SAMPLE_RECT_ARRAY_S *pstRect,
												HI_U16 u16RectMaxNum,HI_U16 u16AreaThrStep,
												HI_U16 u16SrcWidth, HI_U16 u16SrcHeight,
												HI_U16 u16DstWidth,HI_U16 u16DstHeight);
/******************************************************************************
* function : Create ive image
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_CreateImage(IVE_IMAGE_S *pstImg,IVE_IMAGE_TYPE_E enType,
			HI_U16 u16Width,HI_U16 u16Height);
/******************************************************************************
* function : Create memory info
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_CreateMemInfo(IVE_MEM_INFO_S*pstMemInfo,HI_U32 u32Size);
/******************************************************************************
* function : Create ive image by cached
******************************************************************************/
HI_S32 SAMPLE_COMM_IVE_CreateImageByCached(IVE_IMAGE_S *pstImg,
		IVE_IMAGE_TYPE_E enType,HI_U16 u16Width,HI_U16 u16Height);
/******************************************************************************
* function : Dma frame info to  ive image
******************************************************************************/
HI_S32 SAMPLE_COMM_DmaImage(VIDEO_FRAME_INFO_S *pstFrameInfo,IVE_DST_IMAGE_S *pstDst,HI_BOOL bInstant);

#endif


