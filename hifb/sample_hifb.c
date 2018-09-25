/******************************************************************************

  Copyright (C), 2011-2021, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_hifb.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2013/7/1
  Description   :
  History       :
  1.Date        : 2013/7/1
    Author      :
    Modification: Created file

******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include "sample_comm.h"

#include <linux/fb.h>
#include "hifb.h"
#include "loadbmp.h"
#include "hi_tde_api.h"
#include "hi_tde_type.h"
#include "hi_tde_errcode.h"


static VO_DEV VoDev = SAMPLE_VO_DEV_DSD0;
static HI_CHAR gs_cExitFlag = 0;
VO_INTF_TYPE_E g_enVoIntfType = VO_INTF_BT656;

static struct fb_bitfield s_r16 = {10, 5, 0};
static struct fb_bitfield s_g16 = {5, 5, 0};
static struct fb_bitfield s_b16 = {0, 5, 0};
static struct fb_bitfield s_a16 = {15, 1, 0};

#define WIDTH_720              720
#define HEIGHT_576             576

#define SAMPLE_IMAGE_WIDTH     300
#define SAMPLE_IMAGE_HEIGHT    150
#define SAMPLE_IMAGE_NUM       20
#define HIFB_RED_1555   0xFC00

#define GRAPHICS_LAYER_G0  0

#define SAMPLE_IMAGE1_PATH		"./res/%d.bmp"

typedef struct hiPTHREAD_HIFB_SAMPLE
{
    HI_S32 fd;
    HI_S32 layer;
    HI_S32 ctrlkey;
} PTHREAD_HIFB_SAMPLE_INFO;

HI_S32 SAMPLE_HIFB_LoadBmp(const char* filename, HI_U8* pAddr)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0)
    {
        SAMPLE_PRT("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;

    CreateSurfaceByBitMap(filename, &Surface, pAddr);

    return HI_SUCCESS;
}

HI_VOID* SAMPLE_HIFB_PANDISPLAY(void* pData)
{
    HI_S32 i, x, y, s32Ret;
    TDE_HANDLE s32Handle;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    HI_U32 u32FixScreenStride = 0;
    HI_U8* pShowScreen;
    HI_U8* pHideScreen;
    HI_U32 u32HideScreenPhy = 0;
    HI_U16* pShowLine;
    HI_U16* ptemp = NULL;
    HIFB_ALPHA_S stAlpha;
    HIFB_POINT_S stPoint = {0, 0};
    HI_CHAR file[12] = "/dev/fb0";

    HI_CHAR image_name[128];
    HI_U8* pDst = NULL;
    HI_BOOL bShow;
    PTHREAD_HIFB_SAMPLE_INFO* pstInfo;
    HIFB_COLORKEY_S stColorKey;
    TDE2_RECT_S stSrcRect, stDstRect;
    TDE2_SURFACE_S stSrc, stDst;
    HI_U32 Phyaddr;
    HI_VOID* Viraddr;
    HI_U32 u32Width;
    HI_U32 u32Height;

    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        u32Width = WIDTH_720;
        u32Height = HEIGHT_576;
    }
    else
    {
        u32Width = WIDTH_LCD;
        u32Height = HEIGHT_LCD;
    }

    if (HI_NULL == pData)
    {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO*)pData;
    switch (pstInfo->layer)
    {
        case GRAPHICS_LAYER_G0 :
            strcpy(file, "/dev/fb0");
            break;
        default:
            strcpy(file, "/dev/fb0");
            break;
    }

    /* 1. open framebuffer device overlay 0 */
    pstInfo->fd = open(file, O_RDWR, 0);
    if (pstInfo->fd < 0)
    {
        SAMPLE_PRT("open %s failed!\n", file);
        return HI_NULL;
    }

    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        SAMPLE_PRT("FBIOPUT_SHOW_HIFB failed!\n");
        return HI_NULL;
    }
    /* 2. set the screen original position */
    switch (pstInfo->layer)
    {
        case GRAPHICS_LAYER_G0:
        {
            stPoint.s32XPos = 0;
            stPoint.s32YPos = 0;
        }
        break;
        default:
            break;
    }

    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
    {
        SAMPLE_PRT("set screen original show position failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 3. get the variable screen info */
    if (ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &var) < 0)
    {
        SAMPLE_PRT("Get variable screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 4. modify the variable screen info
          the screen size: IMAGE_WIDTH*IMAGE_HEIGHT
          the virtual screen size: VIR_SCREEN_WIDTH*VIR_SCREEN_HEIGHT
          (which equals to VIR_SCREEN_WIDTH*(IMAGE_HEIGHT*2))
          the pixel format: ARGB1555
    */
    usleep(4 * 1000 * 1000);
    switch (pstInfo->layer)
    {
        case GRAPHICS_LAYER_G0:
        {
            var.xres_virtual = u32Width;
            var.yres_virtual = u32Height * 2;
            var.xres = u32Width;
            var.yres = u32Height;
        }
        break;
        default:
            break;
    }

    var.transp = s_a16;
    var.red = s_r16;
    var.green = s_g16;
    var.blue = s_b16;
    var.bits_per_pixel = 16;
    var.activate = FB_ACTIVATE_NOW;

    /* 5. set the variable screeninfo */
    if (ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &var) < 0)
    {
        SAMPLE_PRT("Put variable screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 6. get the fix screen info */
    if (ioctl(pstInfo->fd, FBIOGET_FSCREENINFO, &fix) < 0)
    {
        SAMPLE_PRT("Get fix screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    u32FixScreenStride = fix.line_length;   /*fix screen stride*/

    /* 7. map the physical video memory for user use */
    pShowScreen = mmap(HI_NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, pstInfo->fd, 0);
    if (MAP_FAILED == pShowScreen)
    {
        SAMPLE_PRT("mmap framebuffer failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    memset(pShowScreen, 0x00, fix.smem_len);

    /* time to play*/
    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        SAMPLE_PRT("FBIOPUT_SHOW_HIFB failed!\n");
        munmap(pShowScreen, fix.smem_len);
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (GRAPHICS_LAYER_G0 == pstInfo->layer)
    {
        for (i = 0; i < 1; i++)
        {
            if (i % 2)
            {
                var.yoffset = var.yres;
            }
            else
            {
                var.yoffset = 0;
            }
            ptemp = (HI_U16*)(pShowScreen + var.yres * u32FixScreenStride * (i % 2));
            for (y = 10; y < 200; y++)
            {
                for (x = 10; x < 200; x++)
                {
                    *(ptemp + y * var.xres + x) = HIFB_RED_1555;
                }
            }
            SAMPLE_PRT("expected: the red box will appear!\n");
            sleep(2);
            stAlpha.bAlphaEnable = HI_TRUE;
            stAlpha.u8Alpha0 = 0x0;
            stAlpha.u8Alpha1 = 0x0;
            if (ioctl(pstInfo->fd, FBIOPUT_ALPHA_HIFB,  &stAlpha) < 0)
            {
                SAMPLE_PRT("Set alpha failed!\n");
                close(pstInfo->fd);
                return HI_NULL;
            }
            SAMPLE_PRT("expected: after set alpha = 0, the red box will disappear!\n");
            sleep(2);

            stAlpha.u8Alpha0 = 0;
            stAlpha.u8Alpha1 = 0xFF;
            if (ioctl(pstInfo->fd, FBIOPUT_ALPHA_HIFB,  &stAlpha) < 0)
            {
                SAMPLE_PRT("Set alpha failed!\n");
                close(pstInfo->fd);
                return HI_NULL;
            }
            SAMPLE_PRT("expected:after set set alpha = 0xFF, the red box will appear again!\n");
            sleep(2);

            SAMPLE_PRT("expected: the red box will erased by colorkey!\n");
            stColorKey.bKeyEnable = HI_TRUE;
            stColorKey.u32Key = HIFB_RED_1555;
            s32Ret = ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey);
            if (s32Ret < 0)
            {
                SAMPLE_PRT("FBIOPUT_COLORKEY_HIFB failed!\n");
                close(pstInfo->fd);
                return HI_NULL;
            }
            sleep(2);
            SAMPLE_PRT("expected: the red box will appear again!\n");
            stColorKey.bKeyEnable = HI_FALSE;
            s32Ret = ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey);
            if (s32Ret < 0)
            {
                SAMPLE_PRT("FBIOPUT_COLORKEY_HIFB failed!\n");
                close(pstInfo->fd);
                return HI_NULL;
            }
            sleep(2);
        }
    }

    /* show bitmap */
    switch (pstInfo->ctrlkey)
    {
        case 2:
        {
            /*change bmp*/
            if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&Phyaddr, ((void**)&Viraddr),
                                                  NULL, NULL, SAMPLE_IMAGE_WIDTH * SAMPLE_IMAGE_HEIGHT * 2))
            {
                SAMPLE_PRT("allocate memory (maxW*maxH*2 bytes) failed\n");
                close(pstInfo->fd);
                return HI_NULL;
            }

            s32Ret = HI_TDE2_Open();
            if (s32Ret < 0)
            {
                SAMPLE_PRT("HI_TDE2_Open failed :%d!\n", s32Ret);
                HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                close(pstInfo->fd);
                return HI_NULL;
            }

            SAMPLE_PRT("expected:two red  line!\n");
            for (i = 0; i < SAMPLE_IMAGE_NUM; i++)
            {
                if ('q' == gs_cExitFlag)
                {
                    printf("process exit...\n");
                    break;
                }
                /* step1: draw two red line*/
                if (i % 2)
                {
                    var.yoffset = var.yres;
                }
                else
                {
                    var.yoffset = 0;
                }

                pHideScreen = pShowScreen + (u32FixScreenStride * var.yres) * (i % 2);
                memset(pHideScreen, 0x00, u32FixScreenStride * var.yres);
                u32HideScreenPhy = fix.smem_start + (i % 2) * u32FixScreenStride * var.yres;

                pShowLine = (HI_U16*)pHideScreen;
                for (y = (u32Height / 2 - 2); y < (u32Height / 2 + 2); y++)
                {
                    for (x = 0; x < u32Width; x++)
                    {
                        *(pShowLine + y * var.xres + x) = HIFB_RED_1555;
                    }
                }
                for (y = 0; y < u32Height; y++)
                {
                    for (x = (u32Width / 2 - 2); x < (u32Width / 2 + 2); x++)
                    {
                        *(pShowLine + y * var.xres + x) = HIFB_RED_1555;
                    }
                }

                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0)
                {
                    SAMPLE_PRT("FBIOPAN_DISPLAY failed!\n");
                    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                /* step2: draw gui picture*/
                sprintf(image_name, SAMPLE_IMAGE1_PATH, i % 2);
                pDst = (HI_U8*)Viraddr;
                SAMPLE_HIFB_LoadBmp(image_name, pDst);

                /* 0. open tde */
                stSrcRect.s32Xpos = 0;
                stSrcRect.s32Ypos = 0;
                stSrcRect.u32Height = SAMPLE_IMAGE_HEIGHT;
                stSrcRect.u32Width = SAMPLE_IMAGE_WIDTH;
                stDstRect.s32Xpos = 0;
                stDstRect.s32Ypos = 0;
                stDstRect.u32Height = stSrcRect.u32Height;
                stDstRect.u32Width = stSrcRect.u32Width;

                stDst.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
                stDst.u32Width = u32Width;
                stDst.u32Height = u32Height;
                stDst.u32Stride = u32FixScreenStride;
                stDst.u32PhyAddr = u32HideScreenPhy;

                stSrc.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
                stSrc.u32Width = SAMPLE_IMAGE_WIDTH;
                stSrc.u32Height = SAMPLE_IMAGE_HEIGHT;
                stSrc.u32Stride = 2 * SAMPLE_IMAGE_WIDTH;
                stSrc.u32PhyAddr = Phyaddr;
                stSrc.bAlphaExt1555 = HI_TRUE;
                stSrc.bAlphaMax255 = HI_TRUE;
                stSrc.u8Alpha0 = 0XFF;
                stSrc.u8Alpha1 = 0XFF;

                /* 1. start job */
                s32Handle = HI_TDE2_BeginJob();
                if (HI_ERR_TDE_INVALID_HANDLE == s32Handle)
                {
                    SAMPLE_PRT("start job failed!\n");
                    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                s32Ret = HI_TDE2_QuickCopy(s32Handle, &stSrc, &stSrcRect, &stDst, &stDstRect);
                if (s32Ret < 0)
                {
                    SAMPLE_PRT("HI_TDE2_QuickCopy:%d failed,ret=0x%x!\n", __LINE__, s32Ret);
                    HI_TDE2_CancelJob(s32Handle);
                    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                /* 3. submit job */
                s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 10);
                if (s32Ret < 0)
                {
                    SAMPLE_PRT("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
                    HI_TDE2_CancelJob(s32Handle);
                    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0)
                {
                    SAMPLE_PRT("FBIOPAN_DISPLAY failed!\n");
                    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
                    close(pstInfo->fd);
                    return HI_NULL;
                }
                sleep(1);

            }
            HI_TDE2_Close();
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
        }
        break;
        default:
        {
        }
    }

    /* unmap the physical memory */
    munmap(pShowScreen, fix.smem_len);
    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        SAMPLE_PRT("FBIOPUT_SHOW_HIFB failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    close(pstInfo->fd);
    return HI_NULL;
}

HI_VOID* SAMPLE_HIFB_REFRESH(void* pData)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HIFB_LAYER_INFO_S stLayerInfo = {0};
    HIFB_BUFFER_S stCanvasBuf;
    HI_U16* pBuf;
    HI_U8* pDst = NULL;
    HI_U32 x;
    HI_U32 y;
    HI_U32 i;
    HI_CHAR image_name[128];
    HI_BOOL bShow;
    HIFB_POINT_S stPoint = {0};
    struct fb_var_screeninfo stVarInfo;
    HI_CHAR file[12] = "/dev/fb0";
    HI_U32 maxW;
    HI_U32 maxH;
    PTHREAD_HIFB_SAMPLE_INFO* pstInfo;
    HIFB_COLORKEY_S stColorKey;
    TDE2_RECT_S stSrcRect, stDstRect;
    TDE2_SURFACE_S stSrc, stDst;
    HI_U32 Phyaddr;
    HI_VOID* Viraddr;
    TDE_HANDLE s32Handle;

    if (HI_NULL == pData)
    {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO*)pData;

    switch (pstInfo->layer)
    {
        case GRAPHICS_LAYER_G0:
            strcpy(file, "/dev/fb0");
            break;
        default:
            strcpy(file, "/dev/fb0");
            break;
    }

    /* 1. open framebuffer device overlay 0 */
    pstInfo->fd = open(file, O_RDWR, 0);
    if (pstInfo->fd < 0)
    {
        SAMPLE_PRT("open %s failed!\n", file);
        return HI_NULL;
    }
    /*all layer surport colorkey*/
    stColorKey.bKeyEnable = HI_TRUE;
    stColorKey.u32Key = 0x0;
    if (ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey) < 0)
    {
        SAMPLE_PRT("FBIOPUT_COLORKEY_HIFB!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    s32Ret = ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &stVarInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("GET_VSCREENINFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
    {
        SAMPLE_PRT("set screen original show position failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (VO_INTF_BT656== g_enVoIntfType)
    {
        maxW = WIDTH_720;
        maxH = HEIGHT_576;
    }
    else
    {
        maxW = WIDTH_LCD;
        maxH = HEIGHT_LCD;
    }

    stVarInfo.transp = s_a16;
    stVarInfo.red = s_r16;
    stVarInfo.green = s_g16;
    stVarInfo.blue = s_b16;
    stVarInfo.bits_per_pixel = 16;
    stVarInfo.activate = FB_ACTIVATE_NOW;
    stVarInfo.xres = stVarInfo.xres_virtual = maxW;
    stVarInfo.yres = stVarInfo.yres_virtual = maxH;
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &stVarInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("PUT_VSCREENINFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    switch (pstInfo->ctrlkey)
    {
        case 0 :
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_ONE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
            break;
        }

        case 1 :
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_DOUBLE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
            break;
        }

        default:
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_NONE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
        }
    }
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_LAYER_INFO, &stLayerInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("PUT_LAYER_INFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        SAMPLE_PRT("FBIOPUT_SHOW_HIFB failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&(stCanvasBuf.stCanvas.u32PhyAddr), ((void**)&pBuf),
                                          NULL, NULL, maxW * maxH * 2))
    {
        SAMPLE_PRT("allocate memory (maxW*maxH*2 bytes) failed\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    stCanvasBuf.stCanvas.u32Height = maxH;
    stCanvasBuf.stCanvas.u32Width = maxW;
    stCanvasBuf.stCanvas.u32Pitch = maxW * 2;
    stCanvasBuf.stCanvas.enFmt = HIFB_FMT_ARGB1555;
    memset(pBuf, 0x00, stCanvasBuf.stCanvas.u32Pitch * stCanvasBuf.stCanvas.u32Height);

    /*change bmp*/
    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&Phyaddr, ((void**)&Viraddr),
                                          NULL, NULL, SAMPLE_IMAGE_WIDTH * SAMPLE_IMAGE_HEIGHT * 2))
    {
        SAMPLE_PRT("allocate memory  failed\n");
        HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
        close(pstInfo->fd);
        return HI_NULL;
    }

    s32Ret = HI_TDE2_Open();
    if (s32Ret < 0)
    {
        SAMPLE_PRT("HI_TDE2_Open failed :%d!\n", s32Ret);
        HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
        HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
        close(pstInfo->fd);
        return HI_NULL;
    }

    SAMPLE_PRT("expected:two red  line!\n");
    /*time to play*/
    for (i = 0; i < SAMPLE_IMAGE_NUM; i++)
    {
        if ('q' == gs_cExitFlag)
        {
            printf("process exit...\n");
            break;
        }

        for (y = (maxH / 2 - 2); y < (maxH / 2 + 2); y++)
        {
            for (x = 0; x < maxW; x++)
            {
                *(pBuf + y * maxW + x) = HIFB_RED_1555;
            }
        }
        for (y = 0; y < maxH; y++)
        {
            for (x = (maxW / 2 - 2); x < (maxW / 2 + 2); x++)
            {
                *(pBuf + y * maxW + x) = HIFB_RED_1555;
            }
        }

        stCanvasBuf.UpdateRect.x = 0;
        stCanvasBuf.UpdateRect.y = 0;
        stCanvasBuf.UpdateRect.w = maxW;
        stCanvasBuf.UpdateRect.h = maxH;
        s32Ret = ioctl(pstInfo->fd, FBIO_REFRESH, &stCanvasBuf);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("REFRESH failed!\n");
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
            HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
            close(pstInfo->fd);
            return HI_NULL;
        }
        sleep(2);

        sprintf(image_name, SAMPLE_IMAGE1_PATH, i % 2);
        pDst = (HI_U8*)Viraddr;
        SAMPLE_HIFB_LoadBmp(image_name, pDst);

        /* 0. open tde */
        stSrcRect.s32Xpos = 0;
        stSrcRect.s32Ypos = 0;
        stSrcRect.u32Height = SAMPLE_IMAGE_HEIGHT;
        stSrcRect.u32Width = SAMPLE_IMAGE_WIDTH;
        stDstRect.s32Xpos = 0;
        stDstRect.s32Ypos = 0;
        stDstRect.u32Height = stSrcRect.u32Width;
        stDstRect.u32Width = stSrcRect.u32Width;

        stDst.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
        stDst.u32Width = maxW;
        stDst.u32Height = maxH;
        stDst.u32Stride = maxW * 2;
        stDst.u32PhyAddr = stCanvasBuf.stCanvas.u32PhyAddr;

        stSrc.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
        stSrc.u32Width = SAMPLE_IMAGE_WIDTH;
        stSrc.u32Height = SAMPLE_IMAGE_HEIGHT;
        stSrc.u32Stride = 2 * SAMPLE_IMAGE_WIDTH;
        stSrc.u32PhyAddr = Phyaddr;
        stSrc.bAlphaExt1555 = HI_TRUE;
        stSrc.bAlphaMax255 = HI_TRUE;
        stSrc.u8Alpha0 = 0XFF;
        stSrc.u8Alpha1 = 0XFF;

        /* 1. start job */
        s32Handle = HI_TDE2_BeginJob();
        if (HI_ERR_TDE_INVALID_HANDLE == s32Handle)
        {
            SAMPLE_PRT("start job failed!\n");
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
            HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
            close(pstInfo->fd);
            return HI_NULL;
        }

        s32Ret = HI_TDE2_QuickCopy(s32Handle, &stSrc, &stSrcRect, &stDst, &stDstRect);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("HI_TDE2_QuickCopy:%d failed,ret=0x%x!\n", __LINE__, s32Ret);
            HI_TDE2_CancelJob(s32Handle);
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
            HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
            close(pstInfo->fd);
            return HI_NULL;
        }

        /* 3. submit job */
        s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 10);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
            HI_TDE2_CancelJob(s32Handle);
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
            HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
            close(pstInfo->fd);
            return HI_NULL;
        }

        stCanvasBuf.UpdateRect.x = 0;
        stCanvasBuf.UpdateRect.y = 0;
        stCanvasBuf.UpdateRect.w = maxW;
        stCanvasBuf.UpdateRect.h = maxH;
        s32Ret = ioctl(pstInfo->fd, FBIO_REFRESH, &stCanvasBuf);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("REFRESH failed!\n");
            HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
            HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
            close(pstInfo->fd);
            return HI_NULL;
        }

        sleep(2);
    }

    HI_MPI_SYS_MmzFree(Phyaddr, Viraddr);
    HI_MPI_SYS_MmzFree(stCanvasBuf.stCanvas.u32PhyAddr, pBuf);
    close(pstInfo->fd);

    return HI_NULL;
}

HI_VOID SAMPLE_HIFB_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}

HI_VOID SAMPLE_HIFB_Usage1(HI_CHAR* sPrgNm)
{
    printf("Usage : %s <intf>\n", sPrgNm);
    printf("intf:\n");
    printf("\t 0) vo BT656 output, default.\n");
    printf("\t 1) vo LCD output.\n");

    return;
}

HI_VOID SAMPLE_HIFB_Usage2(HI_VOID)
{
    printf("\n\n/************************************/\n");
    printf("please choose the case which you want to run:\n");
    printf("\t0:  ARGB1555 standard mode\n");
    printf("\t1:  ARGB1555 BUF_DOUBLE mode\n");
    printf("\t2:  ARGB1555 BUF_ONE mode\n");
    printf("\t3:  ARGB1555 BUF_NONE mode\n");
    printf("\tq:  quit the whole sample\n");
    printf("sample command:");

    return;
}

HI_S32 SAMPLE_HIFB_StandardMode(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    pthread_t phifb0 = -1;
    PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    HI_U32 u32PicWidth;
    HI_U32 u32PicHeight;
    SIZE_S  stSize;

    VO_LAYER VoLayer = 0;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_U32 u32VoFrmRate;

    VB_CONF_S       stVbConf;
    HI_U32 u32BlkSize;

    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        u32PicWidth = WIDTH_720;
        u32PicHeight = HEIGHT_576;
    }
    else
    {
        u32PicWidth = WIDTH_LCD;
        u32PicHeight = HEIGHT_LCD;
    }

    u32BlkSize = CEILING_2_POWER(u32PicWidth, SAMPLE_SYS_ALIGN_WIDTH)\
                 * CEILING_2_POWER(u32PicHeight, SAMPLE_SYS_ALIGN_WIDTH) * 2;

    stVbConf.u32MaxPoolCnt = 128;

    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_StandarMode_0;
    }

    /******************************************
     step 3:  start vo dev.
    *****************************************/
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        stPubAttr.enIntfSync = VO_OUTPUT_PAL;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }
    else
    {
        stPubAttr.enIntfSync = SYNC_LCD;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }

    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

    s32Ret = SAMPLE_COMM_VO_GetWH(stPubAttr.enIntfSync, &stSize.u32Width, \
                                  &stSize.u32Height, &u32VoFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vo wh failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_StandarMode_0;
    }
    memcpy(&stLayerAttr.stImageSize, &stSize, sizeof(stSize));

    stLayerAttr.u32DispFrmRt = 30 ;
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = stSize.u32Width;
    stLayerAttr.stDispRect.u32Height = stSize.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo dev failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_StandarMode_0;
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_TRUE);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo layer failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_StandarMode_1;
    }

    /******************************************
     step 4:  start hifb.
    *****************************************/
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  2;
    if (0 != pthread_create(&phifb0, 0, SAMPLE_HIFB_PANDISPLAY, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread failed!\n");
        goto SAMPLE_HIFB_StandarMode_2;
    }

    while (1)
    {

        HI_CHAR ch;

        printf("press 'q' to exit this sample.\n");
        ch = (char)getchar();
        getchar();
        if ('q' == ch)
        {
            gs_cExitFlag = ch;
            break;
        }
        else
        {
            printf("input invaild! please try again.\n");
        }
    }
    if (-1 != phifb0)
    {
        pthread_join(phifb0, 0);
    }

SAMPLE_HIFB_StandarMode_2:  
    SAMPLE_COMM_VO_StopLayer(VoLayer);
SAMPLE_HIFB_StandarMode_1:
    SAMPLE_COMM_VO_StopDev(VoDev);
SAMPLE_HIFB_StandarMode_0:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_HIFB_DoubleBufMode(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    pthread_t phifb0 = -1;

    PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    HI_U32 u32PicWidth;
    HI_U32 u32PicHeight;
    SIZE_S  stSize;

    VO_LAYER VoLayer = 0;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_U32 u32VoFrmRate;

    VB_CONF_S       stVbConf;
    HI_U32 u32BlkSize;

    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        u32PicWidth = WIDTH_720;
        u32PicHeight = HEIGHT_576;
    }
    else
    {
        u32PicWidth = WIDTH_LCD;
        u32PicHeight = HEIGHT_LCD;
    }

    u32BlkSize = CEILING_2_POWER(u32PicWidth, SAMPLE_SYS_ALIGN_WIDTH)\
                 * CEILING_2_POWER(u32PicHeight, SAMPLE_SYS_ALIGN_WIDTH) * 2;

    stVbConf.u32MaxPoolCnt = 128;

    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_DoubleBufMode_0;
    }

    /******************************************
     step 3:  start vo dev.
    *****************************************/
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        stPubAttr.enIntfSync = VO_OUTPUT_PAL;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }
    else
    {
        stPubAttr.enIntfSync = SYNC_LCD;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }

    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

    s32Ret = SAMPLE_COMM_VO_GetWH(stPubAttr.enIntfSync, &stSize.u32Width, \
                                  &stSize.u32Height, &u32VoFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vo wh failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_DoubleBufMode_0;
    }
    memcpy(&stLayerAttr.stImageSize, &stSize, sizeof(stSize));

    stLayerAttr.u32DispFrmRt = 30 ;
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = stSize.u32Width;
    stLayerAttr.stDispRect.u32Height = stSize.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo dev failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_DoubleBufMode_0;
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_TRUE);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo layer failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_DoubleBufMode_1;
    }

    /******************************************
     step 4:  start hifb.
    *****************************************/
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  1;
    if (0 != pthread_create(&phifb0, 0, SAMPLE_HIFB_REFRESH, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread failed!\n");
        goto SAMPLE_HIFB_DoubleBufMode_2;
    }
    while (1)
    {
        HI_CHAR ch;

        printf("press 'q' to exit this sample.\n");
        ch = (char)getchar();
        getchar();
        if ('q' == ch)
        {
            gs_cExitFlag = ch;
            break;
        }
        else
        {
            printf("input invaild! please try again.\n");
        }
    }
    if (-1 != phifb0)
    {
        pthread_join(phifb0, 0);
    }

SAMPLE_HIFB_DoubleBufMode_2:  
    SAMPLE_COMM_VO_StopLayer(VoLayer);
SAMPLE_HIFB_DoubleBufMode_1:
    HI_MPI_VO_Disable(VoDev);
SAMPLE_HIFB_DoubleBufMode_0:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_HIFB_OneBufMode(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    pthread_t phifb0 = -1;

    PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    HI_U32 u32PicWidth;
    HI_U32 u32PicHeight;
    SIZE_S  stSize;

    VO_LAYER VoLayer = 0;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_U32 u32VoFrmRate;

    VB_CONF_S       stVbConf;
    HI_U32 u32BlkSize;

    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        u32PicWidth = WIDTH_720;
        u32PicHeight = HEIGHT_576;
    }
    else
    {
        u32PicWidth = WIDTH_LCD;
        u32PicHeight = HEIGHT_LCD;
    }

    u32BlkSize = CEILING_2_POWER(u32PicWidth, SAMPLE_SYS_ALIGN_WIDTH)\
                 * CEILING_2_POWER(u32PicHeight, SAMPLE_SYS_ALIGN_WIDTH) * 2;

    stVbConf.u32MaxPoolCnt = 128;

    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_OneBufMode_0;
    }

    /******************************************
     step 3:  start vo dev.
    *****************************************/
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        stPubAttr.enIntfSync = VO_OUTPUT_PAL;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }
    else
    {
        stPubAttr.enIntfSync = SYNC_LCD;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }

    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

    s32Ret = SAMPLE_COMM_VO_GetWH(stPubAttr.enIntfSync, &stSize.u32Width, \
                                  &stSize.u32Height, &u32VoFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vo wh failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_OneBufMode_0;
    }
    memcpy(&stLayerAttr.stImageSize, &stSize, sizeof(stSize));

    stLayerAttr.u32DispFrmRt = 30 ;
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = stSize.u32Width;
    stLayerAttr.stDispRect.u32Height = stSize.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo dev failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_OneBufMode_0;
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_TRUE);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo layer failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_OneBufMode_1;
    }

    /******************************************
     step 4:  start hifb.
    *****************************************/
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  0;
    if (0 != pthread_create(&phifb0, 0, SAMPLE_HIFB_REFRESH, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread failed!\n");
        goto SAMPLE_HIFB_OneBufMode_2;
    }
    
    while (1)
    {
        HI_CHAR ch;

        printf("press 'q' to exit this sample.\n");
        ch = (char)getchar();
        getchar();
        if ('q' == ch)
        {
            gs_cExitFlag = ch;
            break;
        }
        else
        {
            printf("input invaild! please try again.\n");
        }
    }
    if (-1 != phifb0)
    {
        pthread_join(phifb0, 0);
    }

SAMPLE_HIFB_OneBufMode_2:   
    SAMPLE_COMM_VO_StopLayer(VoLayer);
SAMPLE_HIFB_OneBufMode_1:
    HI_MPI_VO_Disable(VoDev);
SAMPLE_HIFB_OneBufMode_0:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_HIFB_NoneBufMode(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    pthread_t phifb0 = -1;

    PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    HI_U32 u32PicWidth;
    HI_U32 u32PicHeight;
    SIZE_S  stSize;

    VO_LAYER VoLayer = 0;
    VO_PUB_ATTR_S stPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_U32 u32VoFrmRate;

    VB_CONF_S       stVbConf;
    HI_U32 u32BlkSize;

    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        u32PicWidth = WIDTH_720;
        u32PicHeight = HEIGHT_576;
    }
    else
    {
        u32PicWidth = WIDTH_LCD;
        u32PicHeight = HEIGHT_LCD;
    }

    u32BlkSize = CEILING_2_POWER(u32PicWidth, SAMPLE_SYS_ALIGN_WIDTH)\
                 * CEILING_2_POWER(u32PicHeight, SAMPLE_SYS_ALIGN_WIDTH) * 2;

    stVbConf.u32MaxPoolCnt = 128;

    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt =  6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_NoneBufMode_0;
    }

    /******************************************
     step 3:  start vo dev.
    *****************************************/
    if (VO_INTF_BT656 == g_enVoIntfType)
    {
        stPubAttr.enIntfSync = VO_OUTPUT_PAL;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }
    else
    {
        stPubAttr.enIntfSync = SYNC_LCD;
        stPubAttr.enIntfType = g_enVoIntfType;
        stPubAttr.u32BgColor = 0x0000FF;
    }

    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

    s32Ret = SAMPLE_COMM_VO_GetWH(stPubAttr.enIntfSync, &stSize.u32Width, \
                                  &stSize.u32Height, &u32VoFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vo wh failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_NoneBufMode_0;
    }
    memcpy(&stLayerAttr.stImageSize, &stSize, sizeof(stSize));

    stLayerAttr.u32DispFrmRt = 30 ;
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = stSize.u32Width;
    stLayerAttr.stDispRect.u32Height = stSize.u32Height;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo dev failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_NoneBufMode_0;
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_TRUE);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo layer failed with %d!\n", s32Ret);
        goto SAMPLE_HIFB_NoneBufMode_1;
    }

    /******************************************
     step 4:  start hifb.
    *****************************************/
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  3;
    if (0 != pthread_create(&phifb0, 0, SAMPLE_HIFB_REFRESH, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread failed!\n");
        goto SAMPLE_HIFB_NoneBufMode_2;
    }
    
    while (1)
    {
        HI_CHAR ch;

        printf("press 'q' to exit this sample.\n");
        ch = (char)getchar();
        getchar();
        if ('q' == ch)
        {
            gs_cExitFlag = ch;
            break;
        }
        else
        {
            printf("input invaild! please try again.\n");
        }
    }
    if (-1 != phifb0)
    {
        pthread_join(phifb0, 0);
    }

SAMPLE_HIFB_NoneBufMode_2:   
    SAMPLE_COMM_VO_StopLayer(VoLayer);
SAMPLE_HIFB_NoneBufMode_1:
    HI_MPI_VO_Disable(VoDev);
SAMPLE_HIFB_NoneBufMode_0:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

int main(int argc, char* argv[])
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    HI_BOOL bExit = HI_FALSE;

    if ( (argc < 2) || (1 != strlen(argv[1])))
    {
        SAMPLE_HIFB_Usage1(argv[0]);
        return HI_FAILURE;
    }
    signal(SIGINT, SAMPLE_HIFB_HandleSig);
    signal(SIGTERM, SAMPLE_HIFB_HandleSig);
    if ((argc > 1) && *argv[1] == '1') /* '1': LCD, else: BT656 */
    {
        g_enVoIntfType = INTF_LCD;
    }

    /******************************************
     1 choose the case
    ******************************************/
    while (1)
    {
        SAMPLE_HIFB_Usage2();
        gs_cExitFlag = 0;
        ch = (char)getchar();
        getchar();
        switch (ch)
        {
            case '0':
            {
                SAMPLE_HIFB_StandardMode();
                break;
            }
            case '1':
            {
                SAMPLE_HIFB_DoubleBufMode();
                break;
            }
            case '2':
            {
                SAMPLE_HIFB_OneBufMode();
                break;
            }
            case '3':
            {
                SAMPLE_HIFB_NoneBufMode();
                break;
            }
            case 'q':
            case 'Q':
            {
                bExit = HI_TRUE;
                break;
            }

            default :
            {
                printf("input invaild! please try again.\n");
                break;
            }
        }

        if (bExit)
        {
            break;
        }
    }

    return s32Ret;
}

