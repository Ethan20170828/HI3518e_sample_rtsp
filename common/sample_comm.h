/******************************************************************************
  Hisilicon Hi35xx sample programs head file.

  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-2 Created
******************************************************************************/

#ifndef __SAMPLE_COMM_H__
#define __SAMPLE_COMM_H__

#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_comm_isp.h"
#include "hi_comm_vi.h"
#include "hi_comm_vo.h"
#include "hi_comm_venc.h"
#include "hi_comm_vpss.h"
#include "hi_comm_vdec.h"
#include "hi_comm_region.h"
#include "hi_comm_adec.h"
#include "hi_comm_aenc.h"
#include "hi_comm_ai.h"
#include "hi_comm_ao.h"
#include "hi_comm_aio.h"
#include "hi_defines.h"

#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vi.h"
#include "mpi_vo.h"
#include "mpi_venc.h"
#include "mpi_vpss.h"
#include "mpi_vdec.h"
#include "mpi_region.h"
#include "mpi_adec.h"
#include "mpi_aenc.h"
#include "mpi_ai.h"
#include "mpi_ao.h"
#include "mpi_isp.h"
#include "mpi_ae.h"
#include "mpi_awb.h"
#include "mpi_af.h"
#include "hi_vreg.h"
#include "hi_sns_ctrl.h"


#define	RTSP_ENABLE 	1

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#if	RTSP_ENABLE

#include "list.h"
#if !defined(WIN32)
	#define __PACKED__        __attribute__ ((__packed__))
#else
	#define __PACKED__ 
#endif

typedef enum
{
	RTSP_VIDEO=0,
	RTSP_VIDEOSUB=1,
	RTSP_AUDIO=2,
	RTSP_YUV422=3,
	RTSP_RGB=4,
	RTSP_VIDEOPS=5,
	RTSP_VIDEOSUBPS=6
}enRTSP_MonBlockType;

// ����ṹ�����RTP��ͷ
struct _RTP_FIXED_HEADER
{
    /**//* byte 0 */
    unsigned char csrc_len:4;        /**//* expect 0 */
    unsigned char extension:1;        /**//* expect 1, see RTP_OP below */
    unsigned char padding:1;        /**//* expect 0 */
    unsigned char version:2;        /**//* expect 2 */
    /**//* byte 1 */
    unsigned char payload:7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned char marker:1;        /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short seq_no;            
    /**//* bytes 4-7 */
    unsigned  long timestamp;        
    /**//* bytes 8-11 */
    unsigned long ssrc;            /**//* stream number is used here. */
} __PACKED__;

typedef struct _RTP_FIXED_HEADER RTP_FIXED_HEADER;

struct _NALU_HEADER
{
    //byte 0
	unsigned char TYPE:5;
    unsigned char NRI:2;
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _NALU_HEADER NALU_HEADER;

struct _FU_INDICATOR
{
    	//byte 0
    unsigned char TYPE:5;
	unsigned char NRI:2; 
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _FU_INDICATOR FU_INDICATOR;

struct _FU_HEADER
{
   	//byte 0
    unsigned char TYPE:5;
	unsigned char R:1;
	unsigned char E:1;
	unsigned char S:1;    
} __PACKED__; /**//* 1 BYTES */
typedef struct _FU_HEADER FU_HEADER;

struct _AU_HEADER
{
    //byte 0, 1
    unsigned short au_len;
    //byte 2,3
    unsigned  short frm_len:13;  
    unsigned char au_index:3;
} __PACKED__; /**//* 1 BYTES */
typedef struct _AU_HEADER AU_HEADER;


extern void RtspServer_init(void);
extern void RtspServer_exit(void);

int AddFrameToRtspBuf(int nChanNum,enRTSP_MonBlockType eType, char * pData, unsigned int  nSize, unsigned int  nVidFrmNum,int bIFrm);

extern HI_S32 SAMPLE_COMM_VENC_Sentjin(VENC_STREAM_S *pstStream);
extern HI_S32 saveStream(VENC_STREAM_S *pstStream);
extern HI_VOID* vdRTPSendThread(HI_VOID *p);



#define nalu_sent_len        1400			// ÿ��Ҫ���͵����ݴ�С
//#define nalu_sent_len        1400
#define RTP_H264                    96
#define RTP_AUDIO              97
#define MAX_RTSP_CLIENT       1
#define RTSP_SERVER_PORT      554
#define RTSP_RECV_SIZE        1024
#define RTSP_MAX_VID          (640*1024)
#define RTSP_MAX_AUD          (15*1024)

#define AU_HEADER_SIZE    4
#define PARAM_STRING_MAX        100



typedef unsigned short u_int16_t;
typedef unsigned char u_int8_t;
typedef u_int16_t portNumBits;
typedef u_int32_t netAddressBits;
typedef long long _int64;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif
#define AUDIO_RATE    8000
#define PACKET_BUFFER_END            (unsigned int)0x00000000


typedef struct 
{
	int startblock;
	int endblock;
	int BlockFileNum;
	
}IDXFILEHEAD_INFO;

typedef struct 
{
	_int64 starttime;
	_int64 endtime;
	int startblock;
	int endblock;
	int stampnum;
}IDXFILEBLOCK_INFO;

typedef struct 
{
	int blockindex;
	int pos;
	_int64 time;
}IDXSTAMP_INFO;

typedef struct 
{
	char filename[150];
	int pos;
	_int64 time;
}FILESTAMP_INFO;

typedef struct 
{
	char channelid[9];
	_int64 starttime;
	_int64 endtime;
	_int64 session;
	int		type;
	int		encodetype;
}FIND_INFO;

typedef enum
{
	RTP_UDP,
	RTP_TCP,
	RAW_UDP
}StreamingMode;


RTP_FIXED_HEADER  *rtp_hdr;
NALU_HEADER		  *nalu_hdr;
FU_INDICATOR	  *fu_ind;
FU_HEADER		  *fu_hdr;
AU_HEADER         *au_hdr;


extern char g_rtp_playload[20];
extern int   g_audio_rate;

typedef enum
{
	RTSP_IDLE = 0,
	RTSP_CONNECTED = 1,
	RTSP_SENDING = 2,
}RTSP_STATUS;

typedef struct
{
	int  nVidLen;
	int  nAudLen;
	int bIsIFrm;
	int bWaitIFrm;
	int bIsFree;
	char vidBuf[RTSP_MAX_VID];
	char audBuf[RTSP_MAX_AUD];
}RTSP_PACK;

typedef struct
{
	int index;
	int socket;
	int reqchn;
	int seqnum;			// ���к�
	int seqnum2;
	unsigned int tsvid;	// ����timestamp��ֵ
	unsigned int tsaud;
	int status;
	int sessionid;
	int rtpport[2];
	int rtcpport;
	char IP[20];
	char urlPre[PARAM_STRING_MAX];
}RTSP_CLIENT;

typedef struct
{
	int  vidLen;
	int  audLen;
	int  nFrameID;
	char vidBuf[RTSP_MAX_VID];
	char audBuf[RTSP_MAX_AUD];
}FRAME_PACK;

typedef struct
{
  int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  unsigned max_size;            //! Nal Unit Buffer size
  int forbidden_bit;            //! should be always FALSE
  int nal_reference_idc;        //! NALU_PRIORITY_xxxx
  int nal_unit_type;            //! NALU_TYPE_xxxx
  char *buf;                    //! contains the first byte followed by the EBSP
  unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;
extern int udpfd;
extern RTSP_CLIENT g_rtspClients[MAX_RTSP_CLIENT];


typedef struct _rtpbuf
{
	struct list_head list;	// ˫������
	HI_S32 len;		// ����Ǹ�����Ч���ݵĳ��ȣ�
	// ָ�����һ���ַ�����Ҳ��֪���ַ����ж೤��������Ҫһ��len������������Ч���ݵĳ��ȣ�
	char * buf;		// ���ָ����ָ��ĵ�ַ�����������洢��Ч���ݵģ�
}RTPbuf_s;


extern struct list_head RTPbuf_head;

#endif


/*******************************************************
    macro define 
*******************************************************/
#define FILE_NAME_LEN               128
#define ALIGN_UP(x, a)              ((x+a-1)&(~(a-1)))
#define ALIGN_BACK(x, a)              ((a) * (((x) / (a))))
#define SAMPLE_SYS_ALIGN_WIDTH      64
#define CHECK_CHN_RET(express,Chn,name)\
	do{\
		HI_S32 Ret;\
		Ret = express;\
		if (HI_SUCCESS != Ret)\
		{\
			printf("\033[0;31m%s chn %d failed at %s: LINE: %d with %#x!\033[0;39m\n", name, Chn, __FUNCTION__, __LINE__, Ret);\
			fflush(stdout);\
			return Ret;\
		}\
	}while(0)

#define CHECK_RET(express,name)\
    do{\
        HI_S32 Ret;\
        Ret = express;\
        if (HI_SUCCESS != Ret)\
        {\
            printf("\033[0;31m%s failed at %s: LINE: %d with %#x!\033[0;39m\n", name, __FUNCTION__, __LINE__, Ret);\
            return Ret;\
        }\
    }while(0)
#define SAMPLE_PIXEL_FORMAT         PIXEL_FORMAT_YUV_SEMIPLANAR_420

#define TW2865_FILE "/dev/tw2865dev"
#define TW2960_FILE "/dev/tw2960dev"
#define TLV320_FILE "/dev/tlv320aic31"


#if (HICHIP == HI3518E_V200) 
#define SAMPLE_VO_DEV_DSD1 0
#define SAMPLE_VO_DEV_DSD0 SAMPLE_VO_DEV_DSD1
#else
#error HICHIP define may be error
#endif

/*** for init global parameter ***/
#define SAMPLE_ENABLE 		    1
#define SAMPLE_DISABLE 		    0
#define SAMPLE_NOUSE 		    -1

#define SENSOR_HEIGHT  1080
#define SENSOR_WIDTH   1920

#define SAMPLE_AUDIO_AI_DEV 0
#define SAMPLE_AUDIO_AO_DEV 0
#define SAMPLE_AUDIO_PTNUMPERFRM   320

#define VI_MST_NOTPASS_WITH_VALUE_RETURN(s32TempRet) \
do{\
    NOT_PASS(s32TempRet);\
    VIMST_ExitMpp();\
    return s32TempRet;\
}while(0)

#define VI_PAUSE()  do {\
                         printf("---------------press the Enter key to exit!---------------\n");\
                         getchar();\
                     } while (0)
                     

#define SAMPLE_PRT(fmt...)   \
    do {\
        printf("[%s]-%d: ", __FUNCTION__, __LINE__);\
        printf(fmt);\
       }while(0)

#define CHECK_NULL_PTR(ptr)\
    do{\
        if(NULL == ptr)\
         {\
            printf("func:%s,line:%d, NULL pointer\n",__FUNCTION__,__LINE__);\
            return HI_FAILURE;\
         }\
      }while(0)

#ifdef LCD_ILI9341
#define INTF_LCD            VO_INTF_LCD_6BIT
#define SYNC_LCD            VO_OUTPUT_240X320_50;
#define WIDTH_LCD           240
#define HEIGHT_LCD          320
#endif
        
#ifdef LCD_ILI9342
#define INTF_LCD            VO_INTF_LCD_6BIT
#define SYNC_LCD            VO_OUTPUT_320X240_50
#define WIDTH_LCD           320
#define HEIGHT_LCD          240
#endif
        
#ifdef LCD_OTA5182
#define INTF_LCD            VO_INTF_LCD_8BIT
#define SYNC_LCD            VO_OUTPUT_320X240_60
#define WIDTH_LCD           320
#define HEIGHT_LCD          240
#endif


#define CHIP_HI3516C_V200   0x3516C200
#define CHIP_HI3518E_V200   0x3518E200
#define CHIP_HI3518E_V201   0x3518E201

#if (CHIP_ID == CHIP_HI3516C_V200)
    #define VB_CNT  8
#elif (CHIP_ID == CHIP_HI3518E_V200)
    #define VB_CNT  6
#elif (CHIP_ID == CHIP_HI3518E_V201)
    #define VB_CNT  4
#else
    #error CHIP_ID is not defined
#endif


/*******************************************************
    enum define 
*******************************************************/

typedef enum sample_ispcfg_opt_e
{
    CFG_OPT_NONE    = 0,
    CFG_OPT_SAVE    = 1,
    CFG_OPT_LOAD    = 2,
    CFG_OPT_BUTT
}SAMPLE_CFG_OPT_E;

typedef enum sample_vi_mode_e
{   
    APTINA_AR0130_DC_720P_30FPS = 0,
    APTINA_9M034_DC_720P_30FPS,
    APTINA_AR0230_HISPI_1080P_30FPS,
    SONY_IMX222_DC_1080P_30FPS,
    SONY_IMX222_DC_720P_30FPS,
    PANASONIC_MN34222_MIPI_1080P_30FPS,
    OMNIVISION_OV9712_DC_720P_30FPS,
    OMNIVISION_OV9732_DC_720P_30FPS,
    OMNIVISION_OV9750_MIPI_720P_30FPS,
    OMNIVISION_OV9752_MIPI_720P_30FPS,
    OMNIVISION_OV2718_MIPI_1080P_25FPS,
    SAMPLE_VI_MODE_1_D1,
    SAMPLE_VI_MODE_BT1120_720P,
    SAMPLE_VI_MODE_BT1120_1080P,
}SAMPLE_VI_MODE_E;

typedef enum 
{
    VI_DEV_BT656_D1_1MUX = 0,
    VI_DEV_BT656_D1_4MUX,
    VI_DEV_BT656_960H_1MUX,
    VI_DEV_BT656_960H_4MUX,
    VI_DEV_720P_HD_1MUX,
    VI_DEV_1080P_HD_1MUX,
    VI_DEV_BUTT
}SAMPLE_VI_DEV_TYPE_E;

typedef enum sample_vi_chn_set_e
{
    VI_CHN_SET_NORMAL = 0,      /* mirror, flip close */
    VI_CHN_SET_MIRROR,          /* open MIRROR */
    VI_CHN_SET_FLIP,            /* open filp */
    VI_CHN_SET_FLIP_MIRROR      /* mirror, flip */
}SAMPLE_VI_CHN_SET_E;

typedef enum sample_vo_mode_e
{
    VO_MODE_1MUX = 0,
    VO_MODE_2MUX = 1,
    VO_MODE_BUTT
}SAMPLE_VO_MODE_E;
    
typedef enum sample_rc_e
{
    SAMPLE_RC_CBR = 0,
    SAMPLE_RC_VBR,
    SAMPLE_RC_FIXQP
}SAMPLE_RC_E;

typedef enum sample_rgn_change_type_e
{
    RGN_CHANGE_TYPE_FGALPHA = 0,
    RGN_CHANGE_TYPE_BGALPHA,
    RGN_CHANGE_TYPE_LAYER
}SAMPLE_RGN_CHANGE_TYPE_EN;


/*******************************************************
    structure define 
*******************************************************/
typedef struct sample_vi_param_s
{
    HI_S32 s32ViDevCnt;         // VI Dev Total Count
    HI_S32 s32ViDevInterval;    // Vi Dev Interval
    HI_S32 s32ViChnCnt;         // Vi Chn Total Count
    HI_S32 s32ViChnInterval;    // VI Chn Interval
}SAMPLE_VI_PARAM_S;

typedef struct sample_video_loss_s
{
    HI_BOOL bStart;
    pthread_t Pid;
    SAMPLE_VI_MODE_E enViMode;
} SAMPLE_VIDEO_LOSS_S;


typedef struct sample_vi_frame_info_s
{
    VB_BLK VbBlk;    
    VIDEO_FRAME_INFO_S stVideoFrame;
    HI_U32 u32FrmSize;
}SAMPLE_VI_FRAME_INFO_S;


typedef struct sample_venc_getstream_s
{
     HI_BOOL bThreadStart;
     HI_S32  s32Cnt;
}SAMPLE_VENC_GETSTREAM_PARA_S;

typedef struct sample_vi_config_s
{
    SAMPLE_VI_MODE_E enViMode;
    VIDEO_NORM_E enNorm;           /*DC: VIDEO_ENCODING_MODE_AUTO */    
    ROTATE_E enRotate;
    SAMPLE_VI_CHN_SET_E enViChnSet; 
    WDR_MODE_E  enWDRMode;
}SAMPLE_VI_CONFIG_S;


/*******************************************************
    function announce  
*******************************************************/
HI_S32 SAMPLE_COMM_SYS_GetPicSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, SIZE_S *pstSize);
HI_U32 SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, PIXEL_FORMAT_E enPixFmt, HI_U32 u32AlignWidth);
HI_U32 VI_COMM_SYS_CalcPicVbBlkSize(VIDEO_NORM_E enNorm,HI_U32  u32Width ,HI_U32  u32Height, PIXEL_FORMAT_E enPixFmt, HI_U32 u32AlignWidth);
HI_S32 SAMPLE_COMM_SYS_MemConfig(HI_VOID);
HI_VOID SAMPLE_COMM_SYS_Exit(void);
HI_S32 SAMPLE_COMM_SYS_Init(VB_CONF_S *pstVbConf);
HI_S32 SAMPLE_COMM_SYS_Init_With_DCF(VB_CONF_S* pstVbConf);
HI_S32 SAMPLE_COMM_SYS_Payload2FilePostfix(PAYLOAD_TYPE_E enPayload, HI_CHAR* szFilePostfix);

HI_S32 SAMPLE_COMM_ISP_Init(WDR_MODE_E  enWDRMode);
HI_VOID SAMPLE_COMM_ISP_Stop(void);
HI_S32 SAMPLE_COMM_ISP_Run(void);
HI_S32 SAMPLE_COMM_ISP_ChangeSensorMode(HI_U8 u8Mode);

HI_S32 SAMPLE_COMM_VI_GetSizeBySensor(PIC_SIZE_E *penSize);
HI_S32 SAMPLE_COMM_VI_GetRgbirAttrBySensor(ISP_RGBIR_ATTR_S *pstRgbirAttr);
HI_S32 SAMPLE_COMM_VI_GetRgbirCtrlBySensor(ISP_RGBIR_CTRL_S *pstRgbirCtrl);

HI_S32 SAMPLE_COMM_VI_Mode2Param(SAMPLE_VI_MODE_E enViMode, SAMPLE_VI_PARAM_S *pstViParam);
HI_S32 SAMPLE_COMM_VI_Mode2Size(SAMPLE_VI_MODE_E enViMode, VIDEO_NORM_E enNorm, SIZE_S *pstSize);
VI_DEV SAMPLE_COMM_VI_GetDev(SAMPLE_VI_MODE_E enViMode, VI_CHN ViChn);
HI_S32 SAMPLE_COMM_VI_StartDev(VI_DEV ViDev, SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_StartChn(VI_CHN ViChn, RECT_S *pstCapRect, SIZE_S *pstTarSize, SAMPLE_VI_CONFIG_S* pstViConfig);
HI_S32 SAMPLE_COMM_VI_StartBT656(SAMPLE_VI_CONFIG_S* pstViConfig);
HI_S32 SAMPLE_COMM_VI_StopBT656(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_BindVpss(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_UnBindVpss(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_BindVenc(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_StartMIPI(SAMPLE_VI_CONFIG_S* pstViConfig);
HI_S32 SAMPLE_COMM_VI_UnBindVenc(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_MemConfig(SAMPLE_VI_MODE_E enViMode);
HI_S32 SAMPLE_COMM_VI_GetVFrameFromYUV(FILE *pYUVFile, HI_U32 u32Width, HI_U32 u32Height,HI_U32 u32Stride, VIDEO_FRAME_INFO_S *pstVFrameInfo);
HI_S32 SAMPLE_COMM_VI_ChangeCapSize(VI_CHN ViChn, HI_U32 u32CapWidth, HI_U32 u32CapHeight,HI_U32 u32Width, HI_U32 u32Height);
HI_S32 SAMPLE_COMM_VI_StartVi(SAMPLE_VI_CONFIG_S* pstViConfig);
HI_S32 SAMPLE_COMM_VI_StopVi(SAMPLE_VI_CONFIG_S* pstViConfig);
HI_S32 SAMPLE_COMM_VI_SwitchResParam( SAMPLE_VI_CONFIG_S* pstViConfig,
                                               ISP_PUB_ATTR_S *pstPubAttr, 
                                               RECT_S *pstCapRect );


HI_S32 SAMPLE_COMM_VI_StartMIPI_BT1120(SAMPLE_VI_MODE_E enViMode);

HI_S32 SAMPLE_COMM_VI_FPN_CALIBRATE_CONFIG(const char* fpn_file,    /* fpn file name */
                                           ISP_FPN_TYPE_E enFpnType, /* line/frame */
                                           PIXEL_FORMAT_E enPixelFormat,
                                           COMPRESS_MODE_E	enCompressMode,
                                           HI_U32 u32FrmNum,
                                           HI_U32 u32Threshold);

HI_S32 SAMPLE_COMM_VI_CORRECTION_CONFIG(const char* fpn_file,     /* fpn file_name */
                                        ISP_FPN_TYPE_E enFpnType, /* line/frame */
                                        ISP_OP_TYPE_E  enOpType,  /* auto/manual */
                                        HI_U32 u32Strength,       /* strength */                                       
                                        PIXEL_FORMAT_E enPixelFormat);
	
HI_S32 SAMPLE_COMM_VPSS_MemConfig();
HI_S32 SAMPLE_COMM_VPSS_Start(HI_S32 s32GrpCnt, SIZE_S *pstSize, HI_S32 s32ChnCnt,VPSS_GRP_ATTR_S *pstVpssGrpAttr);
HI_S32 SAMPLE_COMM_VPSS_Stop(HI_S32 s32GrpCnt, HI_S32 s32ChnCnt) ;
HI_S32 SAMPLE_COMM_VPSS_StartGroup(VPSS_GRP VpssGrp, VPSS_GRP_ATTR_S *pstVpssGrpAttr);
HI_S32 SAMPLE_COMM_VPSS_EnableChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, 
                                                  VPSS_CHN_ATTR_S *pstVpssChnAttr,
                                                  VPSS_CHN_MODE_S *pstVpssChnMode,
                                                  VPSS_EXT_CHN_ATTR_S *pstVpssExtChnAttr);
HI_S32 SAMPLE_COMM_VPSS_StopGroup(VPSS_GRP VpssGrp);
HI_S32 SAMPLE_COMM_VPSS_DisableChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn);

HI_S32 SAMPLE_COMM_VO_GetWH(VO_INTF_SYNC_E enIntfSync, HI_U32 *pu32W,HI_U32 *pu32H, HI_U32 *pu32Frm);
HI_S32 SAMPLE_COMM_VO_MemConfig(VO_DEV VoDev, HI_CHAR *pcMmzName);
HI_S32 SAMPLE_COMM_VO_StartDev(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr);
HI_S32 SAMPLE_COMM_VO_StopDev(VO_DEV VoDev);
HI_S32 SAMPLE_COMM_VO_StartLayer(VO_LAYER VoLayer,const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, HI_BOOL bVgsBypass);
HI_S32 SAMPLE_COMM_VO_StopLayer(VO_LAYER VoLayer);
HI_S32 SAMPLE_COMM_VO_StartChn(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode);
HI_S32 SAMPLE_COMM_VO_StopChn(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode);
HI_S32 SAMPLE_COMM_VO_BindVpss(VO_LAYER VoLayer,VO_CHN VoChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn);
HI_S32 SAMPLE_COMM_VO_UnBindVpss(VO_LAYER VoLayer,VO_CHN VoChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn);
HI_S32 SAMPLE_COMM_VO_BindVi(VO_LAYER VoLayer, VO_CHN VoChn, VI_CHN ViChn);
HI_S32 SAMPLE_COMM_VO_UnBindVi(VO_LAYER VoLayer, VO_CHN VoChn);

HI_S32 SAMPLE_COMM_VENC_MemConfig(HI_VOID);
HI_S32 SAMPLE_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, VIDEO_NORM_E enNorm, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode,HI_U32 u32Profile);
HI_S32 SAMPLE_COMM_VENC_Stop(VENC_CHN VencChn);
HI_S32 SAMPLE_COMM_VENC_SnapStart(VENC_CHN VencChn, SIZE_S *pstSize, HI_BOOL bSupportDCF);
HI_S32 SAMPLE_COMM_VENC_SnapProcess(VENC_CHN VencChn, HI_BOOL bSaveJpg, HI_BOOL bSaveThm);
HI_S32 SAMPLE_COMM_VENC_SnapStop(VENC_CHN VencChn);
HI_S32 SAMPLE_COMM_VENC_StartGetStream(HI_S32 s32Cnt);
HI_S32 SAMPLE_COMM_VENC_StopGetStream();
HI_S32 SAMPLE_COMM_VENC_BindVpss(VENC_CHN VencChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn);
HI_S32 SAMPLE_COMM_VENC_UnBindVpss(VENC_CHN VencChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn);
HI_S32 SAMPLE_COMM_VENC_StartGetStream_Svc_t(HI_S32 s32Cnt);


HI_S32 SAMPLE_COMM_VDA_MdStart(VDA_CHN VdaChn, HI_U32 u32Chn, SIZE_S *pstSize);
HI_S32 SAMPLE_COMM_VDA_OdStart(VDA_CHN VdaChn, HI_U32 u32Chn, SIZE_S *pstSize);
HI_VOID SAMPLE_COMM_VDA_MdStop(VDA_CHN VdaChn, HI_U32 u32Chn);
HI_VOID SAMPLE_COMM_VDA_OdStop(VDA_CHN VdaChn, HI_U32 u32Chn);

HI_S32 SAMPLE_COMM_AUDIO_CreatTrdAiAo(AUDIO_DEV AiDev, AI_CHN AiChn, AUDIO_DEV AoDev, AO_CHN AoChn);
HI_S32 SAMPLE_COMM_AUDIO_CreatTrdAiAenc(AUDIO_DEV AiDev, AI_CHN AiChn, AENC_CHN AeChn);
HI_S32 SAMPLE_COMM_AUDIO_CreatTrdAencAdec(AENC_CHN AeChn, ADEC_CHN AdChn, FILE *pAecFd);
HI_S32 SAMPLE_COMM_AUDIO_CreatTrdFileAdec(ADEC_CHN AdChn, FILE *pAdcFd);
HI_S32 SAMPLE_COMM_AUDIO_CreatTrdAoVolCtrl(AUDIO_DEV AoDev);
HI_S32 SAMPLE_COMM_AUDIO_DestoryTrdAi(AUDIO_DEV AiDev, AI_CHN AiChn);
HI_S32 SAMPLE_COMM_AUDIO_DestoryTrdAencAdec(AENC_CHN AeChn);
HI_S32 SAMPLE_COMM_AUDIO_DestoryTrdFileAdec(ADEC_CHN AdChn);
HI_S32 SAMPLE_COMM_AUDIO_DestoryTrdAoVolCtrl(AUDIO_DEV AoDev);
HI_S32 SAMPLE_COMM_AUDIO_DestoryAllTrd();
HI_S32 SAMPLE_COMM_AUDIO_AoBindAdec(AUDIO_DEV AoDev, AO_CHN AoChn, ADEC_CHN AdChn);
HI_S32 SAMPLE_COMM_AUDIO_AoUnbindAdec(AUDIO_DEV AoDev, AO_CHN AoChn, ADEC_CHN AdChn);
HI_S32 SAMPLE_COMM_AUDIO_AoBindAi(AUDIO_DEV AiDev, AI_CHN AiChn, AUDIO_DEV AoDev, AO_CHN AoChn);
HI_S32 SAMPLE_COMM_AUDIO_AoUnbindAi(AUDIO_DEV AiDev, AI_CHN AiChn, AUDIO_DEV AoDev, AO_CHN AoChn);
HI_S32 SAMPLE_COMM_AUDIO_AencBindAi(AUDIO_DEV AiDev, AI_CHN AiChn, AENC_CHN AeChn);
HI_S32 SAMPLE_COMM_AUDIO_AencUnbindAi(AUDIO_DEV AiDev, AI_CHN AiChn, AENC_CHN AeChn);
HI_S32 SAMPLE_COMM_AUDIO_CfgAcodec(AIO_ATTR_S *pstAioAttr);
HI_S32 SAMPLE_COMM_AUDIO_DisableAcodec();
HI_S32 SAMPLE_COMM_AUDIO_StartAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt,
                                 AIO_ATTR_S* pstAioAttr, AUDIO_SAMPLE_RATE_E enOutSampleRate, HI_BOOL bResampleEn, HI_VOID* pstAiVqeAttr, HI_U32 u32AiVqeType);
HI_S32 SAMPLE_COMM_AUDIO_StopAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt, HI_BOOL bResampleEn, HI_BOOL bVqeEn);
HI_S32 SAMPLE_COMM_AUDIO_StartAo(AUDIO_DEV AoDevId, HI_S32 s32AoChnCnt,
                                 AIO_ATTR_S* pstAioAttr, AUDIO_SAMPLE_RATE_E enInSampleRate, HI_BOOL bResampleEn, HI_VOID* pstAoVqeAttr, HI_U32 u32AoVqeType);
HI_S32 SAMPLE_COMM_AUDIO_StopAo(AUDIO_DEV AoDevId, HI_S32 s32AoChnCnt, HI_BOOL bResampleEn, HI_BOOL bVqeEn);
HI_S32 SAMPLE_COMM_AUDIO_StartAenc(HI_S32 s32AencChnCnt, HI_U32 u32AencPtNumPerFrm, PAYLOAD_TYPE_E enType);
HI_S32 SAMPLE_COMM_AUDIO_StopAenc(HI_S32 s32AencChnCnt);
HI_S32 SAMPLE_COMM_AUDIO_StartAdec(ADEC_CHN AdChn, PAYLOAD_TYPE_E enType);
HI_S32 SAMPLE_COMM_AUDIO_StopAdec(ADEC_CHN AdChn);
HI_VOID SAMPLE_COMM_SYS_Exit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* End of #ifndef __SAMPLE_COMMON_H__ */
