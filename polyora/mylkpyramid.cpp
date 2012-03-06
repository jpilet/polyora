/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include <cv.h>
#include <cxmisc.h>
#include <float.h>
#include <stdio.h>
#include "mylkpyramid.h"

#ifndef CV_MAX_THREADS
#define CV_MAX_THREADS 16
#endif

//Added
//#include "C:\Program Files\OpenCV\cv\src\_cv.h"
#define CV_8TO32F(x)  icv8x32fTab_cv[(x)+256]

static void
intersect( CvPoint2D32f pt, CvSize win_size, CvSize imgSize,
           CvPoint* min_pt, CvPoint* max_pt )
{
    CvPoint ipt;

    ipt.x = cvFloor( pt.x );
    ipt.y = cvFloor( pt.y );

    ipt.x -= win_size.width;
    ipt.y -= win_size.height;

    win_size.width = win_size.width * 2 + 1;
    win_size.height = win_size.height * 2 + 1;

    min_pt->x = MAX( 0, -ipt.x );
    min_pt->y = MAX( 0, -ipt.y );
    max_pt->x = MIN( win_size.width, imgSize.width - ipt.x );
    max_pt->y = MIN( win_size.height, imgSize.height - ipt.y );
}



static void
icvInitPyramidalAlgorithm( const PyrImage* imgA, const PyrImage* imgB,
                           CvTermCriteria * criteria,
                           int max_iters, int flags,
                           uchar *** imgI, uchar *** imgJ,
                           int **step, CvSize** size,
                           float **scale, uchar ** buffer )
{

    int level = imgA->nbLev;
    const int ALIGN = 8;
    int pyrBytes, bufferBytes = 0, elem_size;
    int level1 = level + 1;

    int i;
    CvSize imgSize, levelSize;

    *buffer = 0;
    *imgI = *imgJ = 0;
    *step = 0;
    *scale = 0;
    *size = 0;

    switch( criteria->type )
    {
    case CV_TERMCRIT_ITER:
        criteria->epsilon = 0.f;
        break;
    case CV_TERMCRIT_EPS:
        criteria->max_iter = max_iters;
        break;
    case CV_TERMCRIT_ITER | CV_TERMCRIT_EPS:
        break;
    }


    /* compare squared values */
    criteria->epsilon *= criteria->epsilon;

    /* set pointers and step for every level */
    pyrBytes = 0;

    imgSize = cvGetSize(imgA->images[0]);
    elem_size = 1;
    levelSize = imgSize;

    for( i = 1; i < level1; i++ )
    {
        levelSize.width = (levelSize.width + 1) >> 1;
        levelSize.height = (levelSize.height + 1) >> 1;

        int tstep = cvAlign(levelSize.width,ALIGN) * elem_size;
        pyrBytes += tstep * levelSize.height;
    }

    assert( pyrBytes <= imgSize.width * imgSize.height * elem_size * 4 / 3 );

    /* buffer_size = <size for patches> */
    bufferBytes = (int)(
        (sizeof(imgI[0][0]) * 2 + sizeof(step[0][0]) +
         sizeof(size[0][0]) + sizeof(scale[0][0])) * level1);

    ( *buffer = (uchar *)cvAlloc( bufferBytes ));

    *imgI = (uchar **) buffer[0];
    *imgJ = *imgI + level1;
    *step = (int *) (*imgJ + level1);
    *scale = (float *) (*step + level1);
    *size = (CvSize *)(*scale + level1);

    for (int i=0; i<level; i++) {
	    imgI[0][i] = (uchar *)imgA->images[i]->imageData;
	    imgJ[0][i] = (uchar *)imgB->images[i]->imageData;
	    step[0][i] = imgA->images[i]->widthStep;
	    size[0][i] = cvGetSize(imgA->images[i]);
	    scale[0][i] = 1.0f / float(1<<i);
    }

}


/* compute dI/dx and dI/dy */
static void
icvCalcIxIy_32f( const float* src, int src_step, float* dstX, float* dstY, int dst_step,
                 CvSize src_size, const float* smooth_k, float* buffer0 )
{
    int src_width = src_size.width, dst_width = src_size.width-2;
    int x, height = src_size.height - 2;
    float* buffer1 = buffer0 + src_width;

    src_step /= sizeof(src[0]);
    dst_step /= sizeof(dstX[0]);

    for( ; height--; src += src_step, dstX += dst_step, dstY += dst_step )
    {
        const float* src2 = src + src_step;
        const float* src3 = src + src_step*2;

        for( x = 0; x < src_width; x++ )
        {
            float t0 = (src3[x] + src[x])*smooth_k[0] + src2[x]*smooth_k[1];
            float t1 = src3[x] - src[x];
            buffer0[x] = t0; buffer1[x] = t1;
        }

        for( x = 0; x < dst_width; x++ )
        {
            float t0 = buffer0[x+2] - buffer0[x];
            float t1 = (buffer1[x] + buffer1[x+2])*smooth_k[0] + buffer1[x+1]*smooth_k[1];
            dstX[x] = t0; dstY[x] = t1;
        }
    }
}


/*
icvOpticalFlowPyrLKInitAlloc_8u_C1R_t icvOpticalFlowPyrLKInitAlloc_8u_C1R_p = 0;
icvOpticalFlowPyrLKFree_8u_C1R_t icvOpticalFlowPyrLKFree_8u_C1R_p = 0;
icvOpticalFlowPyrLK_8u_C1R_t icvOpticalFlowPyrLK_8u_C1R_p = 0;
*/







//Comment Out
//CvStatus CV_STDCALL icvGetRectSubPix_8u32f_C1R
//( const uchar* src, int src_step, CvSize src_size,
//  float* dst, int dst_step, CvSize win_size, CvPoint2D32f center );



//Added -from here
static const float icv8x32fTab_cv[] =
{
    -256.f, -255.f, -254.f, -253.f, -252.f, -251.f, -250.f, -249.f,
    -248.f, -247.f, -246.f, -245.f, -244.f, -243.f, -242.f, -241.f,
    -240.f, -239.f, -238.f, -237.f, -236.f, -235.f, -234.f, -233.f,
    -232.f, -231.f, -230.f, -229.f, -228.f, -227.f, -226.f, -225.f,
    -224.f, -223.f, -222.f, -221.f, -220.f, -219.f, -218.f, -217.f,
    -216.f, -215.f, -214.f, -213.f, -212.f, -211.f, -210.f, -209.f,
    -208.f, -207.f, -206.f, -205.f, -204.f, -203.f, -202.f, -201.f,
    -200.f, -199.f, -198.f, -197.f, -196.f, -195.f, -194.f, -193.f,
    -192.f, -191.f, -190.f, -189.f, -188.f, -187.f, -186.f, -185.f,
    -184.f, -183.f, -182.f, -181.f, -180.f, -179.f, -178.f, -177.f,
    -176.f, -175.f, -174.f, -173.f, -172.f, -171.f, -170.f, -169.f,
    -168.f, -167.f, -166.f, -165.f, -164.f, -163.f, -162.f, -161.f,
    -160.f, -159.f, -158.f, -157.f, -156.f, -155.f, -154.f, -153.f,
    -152.f, -151.f, -150.f, -149.f, -148.f, -147.f, -146.f, -145.f,
    -144.f, -143.f, -142.f, -141.f, -140.f, -139.f, -138.f, -137.f,
    -136.f, -135.f, -134.f, -133.f, -132.f, -131.f, -130.f, -129.f,
    -128.f, -127.f, -126.f, -125.f, -124.f, -123.f, -122.f, -121.f,
    -120.f, -119.f, -118.f, -117.f, -116.f, -115.f, -114.f, -113.f,
    -112.f, -111.f, -110.f, -109.f, -108.f, -107.f, -106.f, -105.f,
    -104.f, -103.f, -102.f, -101.f, -100.f,  -99.f,  -98.f,  -97.f,
     -96.f,  -95.f,  -94.f,  -93.f,  -92.f,  -91.f,  -90.f,  -89.f,
     -88.f,  -87.f,  -86.f,  -85.f,  -84.f,  -83.f,  -82.f,  -81.f,
     -80.f,  -79.f,  -78.f,  -77.f,  -76.f,  -75.f,  -74.f,  -73.f,
     -72.f,  -71.f,  -70.f,  -69.f,  -68.f,  -67.f,  -66.f,  -65.f,
     -64.f,  -63.f,  -62.f,  -61.f,  -60.f,  -59.f,  -58.f,  -57.f,
     -56.f,  -55.f,  -54.f,  -53.f,  -52.f,  -51.f,  -50.f,  -49.f,
     -48.f,  -47.f,  -46.f,  -45.f,  -44.f,  -43.f,  -42.f,  -41.f,
     -40.f,  -39.f,  -38.f,  -37.f,  -36.f,  -35.f,  -34.f,  -33.f,
     -32.f,  -31.f,  -30.f,  -29.f,  -28.f,  -27.f,  -26.f,  -25.f,
     -24.f,  -23.f,  -22.f,  -21.f,  -20.f,  -19.f,  -18.f,  -17.f,
     -16.f,  -15.f,  -14.f,  -13.f,  -12.f,  -11.f,  -10.f,   -9.f,
      -8.f,   -7.f,   -6.f,   -5.f,   -4.f,   -3.f,   -2.f,   -1.f,
       0.f,    1.f,    2.f,    3.f,    4.f,    5.f,    6.f,    7.f,
       8.f,    9.f,   10.f,   11.f,   12.f,   13.f,   14.f,   15.f,
      16.f,   17.f,   18.f,   19.f,   20.f,   21.f,   22.f,   23.f,
      24.f,   25.f,   26.f,   27.f,   28.f,   29.f,   30.f,   31.f,
      32.f,   33.f,   34.f,   35.f,   36.f,   37.f,   38.f,   39.f,
      40.f,   41.f,   42.f,   43.f,   44.f,   45.f,   46.f,   47.f,
      48.f,   49.f,   50.f,   51.f,   52.f,   53.f,   54.f,   55.f,
      56.f,   57.f,   58.f,   59.f,   60.f,   61.f,   62.f,   63.f,
      64.f,   65.f,   66.f,   67.f,   68.f,   69.f,   70.f,   71.f,
      72.f,   73.f,   74.f,   75.f,   76.f,   77.f,   78.f,   79.f,
      80.f,   81.f,   82.f,   83.f,   84.f,   85.f,   86.f,   87.f,
      88.f,   89.f,   90.f,   91.f,   92.f,   93.f,   94.f,   95.f,
      96.f,   97.f,   98.f,   99.f,  100.f,  101.f,  102.f,  103.f,
     104.f,  105.f,  106.f,  107.f,  108.f,  109.f,  110.f,  111.f,
     112.f,  113.f,  114.f,  115.f,  116.f,  117.f,  118.f,  119.f,
     120.f,  121.f,  122.f,  123.f,  124.f,  125.f,  126.f,  127.f,
     128.f,  129.f,  130.f,  131.f,  132.f,  133.f,  134.f,  135.f,
     136.f,  137.f,  138.f,  139.f,  140.f,  141.f,  142.f,  143.f,
     144.f,  145.f,  146.f,  147.f,  148.f,  149.f,  150.f,  151.f,
     152.f,  153.f,  154.f,  155.f,  156.f,  157.f,  158.f,  159.f,
     160.f,  161.f,  162.f,  163.f,  164.f,  165.f,  166.f,  167.f,
     168.f,  169.f,  170.f,  171.f,  172.f,  173.f,  174.f,  175.f,
     176.f,  177.f,  178.f,  179.f,  180.f,  181.f,  182.f,  183.f,
     184.f,  185.f,  186.f,  187.f,  188.f,  189.f,  190.f,  191.f,
     192.f,  193.f,  194.f,  195.f,  196.f,  197.f,  198.f,  199.f,
     200.f,  201.f,  202.f,  203.f,  204.f,  205.f,  206.f,  207.f,
     208.f,  209.f,  210.f,  211.f,  212.f,  213.f,  214.f,  215.f,
     216.f,  217.f,  218.f,  219.f,  220.f,  221.f,  222.f,  223.f,
     224.f,  225.f,  226.f,  227.f,  228.f,  229.f,  230.f,  231.f,
     232.f,  233.f,  234.f,  235.f,  236.f,  237.f,  238.f,  239.f,
     240.f,  241.f,  242.f,  243.f,  244.f,  245.f,  246.f,  247.f,
     248.f,  249.f,  250.f,  251.f,  252.f,  253.f,  254.f,  255.f,
     256.f,  257.f,  258.f,  259.f,  260.f,  261.f,  262.f,  263.f,
     264.f,  265.f,  266.f,  267.f,  268.f,  269.f,  270.f,  271.f,
     272.f,  273.f,  274.f,  275.f,  276.f,  277.f,  278.f,  279.f,
     280.f,  281.f,  282.f,  283.f,  284.f,  285.f,  286.f,  287.f,
     288.f,  289.f,  290.f,  291.f,  292.f,  293.f,  294.f,  295.f,
     296.f,  297.f,  298.f,  299.f,  300.f,  301.f,  302.f,  303.f,
     304.f,  305.f,  306.f,  307.f,  308.f,  309.f,  310.f,  311.f,
     312.f,  313.f,  314.f,  315.f,  316.f,  317.f,  318.f,  319.f,
     320.f,  321.f,  322.f,  323.f,  324.f,  325.f,  326.f,  327.f,
     328.f,  329.f,  330.f,  331.f,  332.f,  333.f,  334.f,  335.f,
     336.f,  337.f,  338.f,  339.f,  340.f,  341.f,  342.f,  343.f,
     344.f,  345.f,  346.f,  347.f,  348.f,  349.f,  350.f,  351.f,
     352.f,  353.f,  354.f,  355.f,  356.f,  357.f,  358.f,  359.f,
     360.f,  361.f,  362.f,  363.f,  364.f,  365.f,  366.f,  367.f,
     368.f,  369.f,  370.f,  371.f,  372.f,  373.f,  374.f,  375.f,
     376.f,  377.f,  378.f,  379.f,  380.f,  381.f,  382.f,  383.f,
     384.f,  385.f,  386.f,  387.f,  388.f,  389.f,  390.f,  391.f,
     392.f,  393.f,  394.f,  395.f,  396.f,  397.f,  398.f,  399.f,
     400.f,  401.f,  402.f,  403.f,  404.f,  405.f,  406.f,  407.f,
     408.f,  409.f,  410.f,  411.f,  412.f,  413.f,  414.f,  415.f,
     416.f,  417.f,  418.f,  419.f,  420.f,  421.f,  422.f,  423.f,
     424.f,  425.f,  426.f,  427.f,  428.f,  429.f,  430.f,  431.f,
     432.f,  433.f,  434.f,  435.f,  436.f,  437.f,  438.f,  439.f,
     440.f,  441.f,  442.f,  443.f,  444.f,  445.f,  446.f,  447.f,
     448.f,  449.f,  450.f,  451.f,  452.f,  453.f,  454.f,  455.f,
     456.f,  457.f,  458.f,  459.f,  460.f,  461.f,  462.f,  463.f,
     464.f,  465.f,  466.f,  467.f,  468.f,  469.f,  470.f,  471.f,
     472.f,  473.f,  474.f,  475.f,  476.f,  477.f,  478.f,  479.f,
     480.f,  481.f,  482.f,  483.f,  484.f,  485.f,  486.f,  487.f,
     488.f,  489.f,  490.f,  491.f,  492.f,  493.f,  494.f,  495.f,
     496.f,  497.f,  498.f,  499.f,  500.f,  501.f,  502.f,  503.f,
     504.f,  505.f,  506.f,  507.f,  508.f,  509.f,  510.f,  511.f,
};




static const void*
icvAdjustRect( const void* srcptr, int src_step, int pix_size,
               CvSize src_size, CvSize win_size,
               CvPoint ip, CvRect* pRect )
{
    CvRect rect;
    const char* src = (const char*)srcptr;

    if( ip.x >= 0 )
    {
        src += ip.x*pix_size;
        rect.x = 0;
    }
    else
    {
        rect.x = -ip.x;
        if( rect.x > win_size.width )
            rect.x = win_size.width;
    }

    if( ip.x + win_size.width < src_size.width )
        rect.width = win_size.width;
    else
    {
        rect.width = src_size.width - ip.x - 1;
        if( rect.width < 0 )
        {
            src += rect.width*pix_size;
            rect.width = 0;
        }
        assert( rect.width <= win_size.width );
    }

    if( ip.y >= 0 )
    {
        src += ip.y * src_step;
        rect.y = 0;
    }
    else
        rect.y = -ip.y;

    if( ip.y + win_size.height < src_size.height )
        rect.height = win_size.height;
    else
    {
        rect.height = src_size.height - ip.y - 1;
        if( rect.height < 0 )
        {
            src += rect.height*src_step;
            rect.height = 0;
        }
    }

    *pRect = rect;
    return src - rect.x*pix_size;
}

static CvStatus  mycvGetRectSubPix_8u32f_C1R
( const uchar* src, int src_step, CvSize src_size,
  float* dst, int dst_step, CvSize win_size, CvPoint2D32f center )
{
    CvPoint ip;
    float  a12, a22, b1, b2;
    float a, b;
    double s = 0;
    int i, j;

    center.x -= (win_size.width-1)*0.5f;
    center.y -= (win_size.height-1)*0.5f;

    ip.x = cvFloor( center.x );
    ip.y = cvFloor( center.y );

    if( win_size.width <= 0 || win_size.height <= 0 )
        return CV_BADRANGE_ERR;

    a = center.x - ip.x;
    b = center.y - ip.y;
    a = MAX(a,0.0001f);
    a12 = a*(1.f-b);
    a22 = a*b;
    b1 = 1.f - b;
    b2 = b;
    s = (1. - a)/a;

    src_step /= sizeof(src[0]);
    dst_step /= sizeof(dst[0]);

    if( 0 <= ip.x && ip.x + win_size.width < src_size.width &&
        0 <= ip.y && ip.y + win_size.height < src_size.height )
    {
        // extracted rectangle is totally inside the image
        src += ip.y * src_step + ip.x;

#if 0
        if( icvCopySubpix_8u32f_C1R_p &&
            icvCopySubpix_8u32f_C1R_p( src, src_step, dst,
                dst_step*sizeof(dst[0]), win_size, a, b ) >= 0 )
            return CV_OK;
#endif

        for( ; win_size.height--; src += src_step, dst += dst_step )
        {
            float prev = (1 - a)*(b1*CV_8TO32F(src[0]) + b2*CV_8TO32F(src[src_step]));
            for( j = 0; j < win_size.width; j++ )
            {
                float t = a12*CV_8TO32F(src[j+1]) + a22*CV_8TO32F(src[j+1+src_step]);
                dst[j] = prev + t;
                prev = (float)(t*s);
            }
        }
    }
    else
    {
        CvRect r;

        src = (const uchar*)icvAdjustRect( src, src_step*sizeof(*src),
                               sizeof(*src), src_size, win_size,ip, &r);

        for( i = 0; i < win_size.height; i++, dst += dst_step )
        {
            const uchar *src2 = src + src_step;

            if( i < r.y || i >= r.height )
                src2 -= src_step;

            for( j = 0; j < r.x; j++ )
            {
                float s0 = CV_8TO32F(src[r.x])*b1 +
                           CV_8TO32F(src2[r.x])*b2;

                dst[j] = (float)(s0);
            }

            if( j < r.width )
            {
                float prev = (1 - a)*(b1*CV_8TO32F(src[j]) + b2*CV_8TO32F(src2[j]));

                for( ; j < r.width; j++ )
                {
                    float t = a12*CV_8TO32F(src[j+1]) + a22*CV_8TO32F(src2[j+1]);
                    dst[j] = prev + t;
                    prev = (float)(t*s);
                }
            }

            for( ; j < win_size.width; j++ )
            {
                float s0 = CV_8TO32F(src[r.width])*b1 +
                           CV_8TO32F(src2[r.width])*b2;

                dst[j] = (float)(s0);
            }

            if( i < r.height )
                src = src2;
        }
    }

    return CV_OK;
}

//end


















CvStatus CV_STDCALL icvGetQuadrangleSubPix_8u32f_C1R ( const unsigned char * src, int src_step, CvSize src_size,\
  float *dst, int dst_step, CvSize win_size, const float *matrix );

#define icvTransformVector_64d( matr, src, dst, w, h ) \
    icvMulMatrix_64d( matr, w, h, src, 1, w, dst )

CV_INLINE void icvMulMatrix_64d( const float* src1, int w1, int h1,
                                 const float* src2, int w2, int h2,
                                 float* dst )
{
    int i, j, k;

    if( w1 != h2 )
    {
        assert(0);
        return;
    }

    for( i = 0; i < h1; i++, src1 += w1, dst += w2 )
        for( j = 0; j < w2; j++ )
        {
            float s = 0;
            for( k = 0; k < w1; k++ )
                s += src1[k]*src2[j + k*w2];
            dst[j] = s;
        }

}


void myCalcOpticalFlowPyrLK( const PyrImage* arrA, const PyrImage* arrB,
                        const CvPoint2D32f * featuresA,
                        CvPoint2D32f * featuresB,
                        int count, CvSize winSize, int level,
                        char *status, float *error,
                        CvTermCriteria criteria, int flags )
{
    uchar *pyrBuffer = 0;
    uchar *buffer = 0;
    char* _status = 0;

    const int MAX_ITERS = 100;

    CvSize imgSize;
    static const float smoothKernel[] = { 0.09375, 0.3125, 0.09375 };  /* 3/32, 10/32, 3/32 */
    
    int bufferBytes = 0;
    uchar **imgI = 0;
    uchar **imgJ = 0;
    int *step = 0;
    float *scale = 0;
    CvSize* size = 0;

#ifdef _OPENMP
    int threadCount = cvGetNumThreads();
#else
	int threadCount = 1;
#endif
	
#ifndef CV_MAX_THREADS
#define CV_MAX_THREADS 64
#endif
    float* _patchI[CV_MAX_THREADS];
    float* _patchJ[CV_MAX_THREADS];
    float* _Ix[CV_MAX_THREADS];
    float* _Iy[CV_MAX_THREADS];

    int i, l;

    CvSize patchSize = cvSize( winSize.width * 2 + 1, winSize.height * 2 + 1 );
    int patchLen = patchSize.width * patchSize.height;
    int srcPatchLen = (patchSize.width + 2)*(patchSize.height + 2);

    imgSize = cvGetSize( arrA->images[0] );

    if( count == 0 ) return;

    for( i = 0; i < threadCount; i++ )
        _patchI[i] = _patchJ[i] = _Ix[i] = _Iy[i] = 0;

    icvInitPyramidalAlgorithm( arrA, arrB, 
        &criteria, MAX_ITERS, flags,
        &imgI, &imgJ, &step, &size, &scale, &pyrBuffer );
    level = arrA->nbLev-1;

    if( !status )
        ( status = _status = (char*)cvAlloc( count*sizeof(_status[0]) ));


    /* buffer_size = <size for patches> + <size for pyramids> */
    bufferBytes = (srcPatchLen + patchLen * 3) * sizeof( _patchI[0][0] ) * threadCount;
    ( buffer = (uchar*)cvAlloc( bufferBytes ));

    for( i = 0; i < threadCount; i++ )
    {
        _patchI[i] = i == 0 ? (float*)buffer : _Iy[i-1] + patchLen;
        _patchJ[i] = _patchI[i] + srcPatchLen;
        _Ix[i] = _patchJ[i] + patchLen;
        _Iy[i] = _Ix[i] + patchLen;
    }

    memset( status, 1, count );
    if( error )
        memset( error, 0, count*sizeof(error[0]) );

    if( !(flags & CV_LKFLOW_INITIAL_GUESSES) )
        memcpy( featuresB, featuresA, count*sizeof(featuresA[0]));

    /* do processing from top pyramid level (smallest image)
       to the bottom (original image) */
    for( l = level; l >= 0; l-- )
    {
        CvSize levelSize = size[l];
        int levelStep = step[l];

        {
#if 0 // _OPENMP
        #pragma omp parallel for num_threads(threadCount) schedule(guided) 
#endif // _OPENMP
        /* find flow for each given point */
        for( i = 0; i < count; i++ )
        {
            CvPoint2D32f v;
            CvPoint minI, maxI, minJ, maxJ;
            CvSize isz, jsz;
            int pt_status;
            CvPoint2D32f u;
            CvPoint prev_minJ = { -1, -1 }, prev_maxJ = { -1, -1 };
            float Gxx = 0, Gxy = 0, Gyy = 0, D = 0, minEig = 0;
            float prev_mx = 0, prev_my = 0;
            int j, x, y;
            int threadIdx = cvGetThreadNum();
            float* patchI = _patchI[threadIdx];
            float* patchJ = _patchJ[threadIdx];
            float* Ix = _Ix[threadIdx];
            float* Iy = _Iy[threadIdx];

            v.x = featuresB[i].x;
            v.y = featuresB[i].y;
            if( l < level )
            {
                v.x += v.x;
                v.y += v.y;
            }
            else
            {
                v.x = (float)(v.x * scale[l]);
                v.y = (float)(v.y * scale[l]);
            }

            pt_status = status[i];
            if( !pt_status )
                continue;

            minI = maxI = minJ = maxJ = cvPoint( 0, 0 );

            u.x = (float) (featuresA[i].x * scale[l]);
            u.y = (float) (featuresA[i].y * scale[l]);

            intersect( u, winSize, levelSize, &minI, &maxI );
            isz = jsz = cvSize(maxI.x - minI.x + 2, maxI.y - minI.y + 2);
            u.x += (minI.x - (patchSize.width - maxI.x + 1))*0.5f;
            u.y += (minI.y - (patchSize.height - maxI.y + 1))*0.5f;

            if( isz.width < 3 || isz.height < 3 ||
                mycvGetRectSubPix_8u32f_C1R( imgI[l], levelStep, levelSize,
                    patchI, isz.width*sizeof(patchI[0]), isz, u ) < 0 )
            {
                /* point is outside the image. take the next */
                status[i] = 0;
                continue;
            }

            icvCalcIxIy_32f( patchI, isz.width*sizeof(patchI[0]), Ix, Iy,
                (isz.width-2)*sizeof(patchI[0]), isz, smoothKernel, patchJ );

            for( j = 0; j < criteria.max_iter; j++ )
            {
                float bx = 0, by = 0;
                float mx, my;
                CvPoint2D32f _v;

                intersect( v, winSize, levelSize, &minJ, &maxJ );

                minJ.x = MAX( minJ.x, minI.x );
                minJ.y = MAX( minJ.y, minI.y );

                maxJ.x = MIN( maxJ.x, maxI.x );
                maxJ.y = MIN( maxJ.y, maxI.y );

                jsz = cvSize(maxJ.x - minJ.x, maxJ.y - minJ.y);

                _v.x = v.x + (minJ.x - (patchSize.width - maxJ.x + 1))*0.5f;
                _v.y = v.y + (minJ.y - (patchSize.height - maxJ.y + 1))*0.5f;

                if( jsz.width < 1 || jsz.height < 1 ||
                    mycvGetRectSubPix_8u32f_C1R( imgJ[l], levelStep, levelSize, patchJ,
                                                jsz.width*sizeof(patchJ[0]), jsz, _v ) < 0 )
                {
                    /* point is outside image. take the next */
                    pt_status = 0;
                    break;
                }

                if( maxJ.x == prev_maxJ.x && maxJ.y == prev_maxJ.y &&
                    minJ.x == prev_minJ.x && minJ.y == prev_minJ.y )
                {
                    for( y = 0; y < jsz.height; y++ )
                    {
                        const float* pi = patchI +
                            (y + minJ.y - minI.y + 1)*isz.width + minJ.x - minI.x + 1;
                        const float* pj = patchJ + y*jsz.width;
                        const float* ix = Ix +
                            (y + minJ.y - minI.y)*(isz.width-2) + minJ.x - minI.x;
                        const float* iy = Iy + (ix - Ix);

                        for( x = 0; x < jsz.width; x++ )
                        {
                            float t0 = pi[x] - pj[x];
                            bx += t0 * ix[x];
                            by += t0 * iy[x];
                        }
                    }
                }
                else
                {
                    Gxx = Gyy = Gxy = 0;
                    for( y = 0; y < jsz.height; y++ )
                    {
                        const float* pi = patchI +
                            (y + minJ.y - minI.y + 1)*isz.width + minJ.x - minI.x + 1;
                        const float* pj = patchJ + y*jsz.width;
                        const float* ix = Ix +
                            (y + minJ.y - minI.y)*(isz.width-2) + minJ.x - minI.x;
                        const float* iy = Iy + (ix - Ix);

                        for( x = 0; x < jsz.width; x++ )
                        {
                            float t = pi[x] - pj[x];
                            bx += (float) (t * ix[x]);
                            by += (float) (t * iy[x]);
                            Gxx += ix[x] * ix[x];
                            Gxy += ix[x] * iy[x];
                            Gyy += iy[x] * iy[x];
                        }
                    }

                    D = Gxx * Gyy - Gxy * Gxy;
                    if( D < DBL_EPSILON )
                    {
                        pt_status = 0;
                        break;
                    }

                    // Adi Shavit - 2008.05
                    if( flags & CV_LKFLOW_GET_MIN_EIGENVALS )
                        minEig = (Gyy + Gxx - sqrt((Gxx-Gyy)*(Gxx-Gyy) + 4.*Gxy*Gxy))/(2*jsz.height*jsz.width);

                    D = 1. / D;

                    prev_minJ = minJ;
                    prev_maxJ = maxJ;
                }

                mx = (float) ((Gyy * bx - Gxy * by) * D);
                my = (float) ((Gxx * by - Gxy * bx) * D);

                v.x += mx;
                v.y += my;

                if( mx * mx + my * my < criteria.epsilon )
                    break;

                if( j > 0 && fabs(mx + prev_mx) < 0.01 && fabs(my + prev_my) < 0.01 )
                {
                    v.x -= mx*0.5f;
                    v.y -= my*0.5f;
                    break;
                }
                prev_mx = mx;
                prev_my = my;
            }

            featuresB[i] = v;
            status[i] = (char)pt_status;
            if( l == 0 && error && pt_status )
            {
                /* calc error */
                float err = 0;
                if( flags & CV_LKFLOW_GET_MIN_EIGENVALS )
                    err = minEig;
                else
                {
                    for( y = 0; y < jsz.height; y++ )
                    {
                        const float* pi = patchI +
                            (y + minJ.y - minI.y + 1)*isz.width + minJ.x - minI.x + 1;
                        const float* pj = patchJ + y*jsz.width;

                        for( x = 0; x < jsz.width; x++ )
                        {
                            float t = pi[x] - pj[x];
                            err += t * t;
                        }
                    }
                    err = sqrt(err);
                }
                error[i] = (float)err;
            }
        } // end of point processing loop (i)
        }
    } // end of pyramid levels loop (l)

    //if( ipp_optflow_state )
      //  icvOpticalFlowPyrLKFree_8u_C1R_p( ipp_optflow_state );

    cvFree( &pyrBuffer );
    cvFree( &buffer );
    cvFree( &_status );
}



static void
icvGetRTMatrix( const CvPoint2D32f* a, const CvPoint2D32f* b,
                int count, CvMat* M, int full_affine )
{
    if( full_affine )
    {
        float sa[36], sb[6];
        CvMat A = cvMat( 6, 6, CV_32F, sa ), B = cvMat( 6, 1, CV_32F, sb );
        CvMat MM = cvMat( 6, 1, CV_32F, M->data.db );

        int i;

        memset( sa, 0, sizeof(sa) );
        memset( sb, 0, sizeof(sb) );

        for( i = 0; i < count; i++ )
        {
            sa[0] += a[i].x*a[i].x;
            sa[1] += a[i].y*a[i].x;
            sa[2] += a[i].x;

            sa[6] += a[i].x*a[i].y;
            sa[7] += a[i].y*a[i].y;
            sa[8] += a[i].y;

            sa[12] += a[i].x;
            sa[13] += a[i].y;
            sa[14] += 1;

            sb[0] += a[i].x*b[i].x;
            sb[1] += a[i].y*b[i].x;
            sb[2] += b[i].x;
            sb[3] += a[i].x*b[i].y;
            sb[4] += a[i].y*b[i].y;
            sb[5] += b[i].y;
        }

        sa[21] = sa[0];
        sa[22] = sa[1];
        sa[23] = sa[2];
        sa[27] = sa[6];
        sa[28] = sa[7];
        sa[29] = sa[8];
        sa[33] = sa[12];
        sa[34] = sa[13];
        sa[35] = sa[14];

        cvSolve( &A, &B, &MM, CV_SVD );
    }
    else
    {
        float sa[16], sb[4], m[4], *om = M->data.fl;
        CvMat A = cvMat( 4, 4, CV_32F, sa ), B = cvMat( 4, 1, CV_32F, sb );
        CvMat MM = cvMat( 4, 1, CV_32F, m );

        int i;

        memset( sa, 0, sizeof(sa) );
        memset( sb, 0, sizeof(sb) );

        for( i = 0; i < count; i++ )
        {
            sa[0] += a[i].x*a[i].x + a[i].y*a[i].y;
            sa[1] += 0;
            sa[2] += a[i].x;
            sa[3] += a[i].y;

            sa[4] += 0;
            sa[5] += a[i].x*a[i].x + a[i].y*a[i].y;
            sa[6] += -a[i].y;
            sa[7] += a[i].x;

            sa[8] += a[i].x;
            sa[9] += -a[i].y;
            sa[10] += 1;
            sa[11] += 0;

            sa[12] += a[i].y;
            sa[13] += a[i].x;
            sa[14] += 0;
            sa[15] += 1;

            sb[0] += a[i].x*b[i].x + a[i].y*b[i].y;
            sb[1] += a[i].x*b[i].y - a[i].y*b[i].x;
            sb[2] += b[i].x;
            sb[3] += b[i].y;
        }

        cvSolve( &A, &B, &MM, CV_SVD );

        om[0] = om[4] = m[0];
        om[1] = -m[1];
        om[3] = m[1];
        om[2] = m[2];
        om[5] = m[3];
    }
}


CV_IMPL int
mycvEstimateRigidTransform( const CvArr* _A, const CvArr* _B, CvMat* _M, int full_affine )
{
    int result = 0;
    
    const int COUNT = 15;
    const int WIDTH = 160, HEIGHT = 120;
    const int RANSAC_MAX_ITERS = 100;
    const int RANSAC_SIZE0 = 3;
    const float MIN_TRIANGLE_SIDE = 20;
    const float RANSAC_GOOD_RATIO = 0.5;

    int allocated = 1;
    CvMat *sA = 0, *sB = 0;
    CvPoint2D32f *pA = 0, *pB = 0;
    int* good_idx = 0;
    char *status = 0;
    CvMat* gray = 0;

    CV_FUNCNAME( "cvEstimateRigidTransform" );

    //__BEGIN__;

    CvMat stubA, *A;
    CvMat stubB, *B;
    CvSize sz0, sz1;
    int cn, equal_sizes;
    int i, j, k, k1;
    int count_x, count_y, count;
    float scale = 1;
    CvRNG rng = cvRNG(-1);
    float m[6]={0};
    CvMat M = cvMat( 2, 3, CV_32F, m );
    int good_count = 0;

    ( A = cvGetMat( _A, &stubA ));
    ( B = cvGetMat( _B, &stubB ));

    if( !CV_IS_MAT(_M) )
        CV_ERROR( _M ? CV_StsBadArg : CV_StsNullPtr, "Output parameter M is not a valid matrix" );

    if( !CV_ARE_SIZES_EQ( A, B ) )
        CV_ERROR( CV_StsUnmatchedSizes, "Both input images must have the same size" );

    if( !CV_ARE_TYPES_EQ( A, B ) )
        CV_ERROR( CV_StsUnmatchedFormats, "Both input images must have the same data type" );

    if( CV_MAT_TYPE(A->type) == CV_8UC1 || CV_MAT_TYPE(A->type) == CV_8UC3 )
    {
        cn = CV_MAT_CN(A->type);
        sz0 = cvGetSize(A);
        sz1 = cvSize(WIDTH, HEIGHT);

        scale = MAX( (float)sz1.width/sz0.width, (float)sz1.height/sz0.height );
        scale = MIN( scale, 1. );
        sz1.width = cvRound( sz0.width * scale );
        sz1.height = cvRound( sz0.height * scale );

        equal_sizes = sz1.width == sz0.width && sz1.height == sz0.height;

        if( !equal_sizes || cn != 1 )
        {
            ( sA = cvCreateMat( sz1.height, sz1.width, CV_8UC1 ));
            ( sB = cvCreateMat( sz1.height, sz1.width, CV_8UC1 ));

            if( !equal_sizes && cn != 1 )
                ( gray = cvCreateMat( sz0.height, sz0.width, CV_8UC1 ));

            if( gray )
            {
                cvCvtColor( A, gray, CV_BGR2GRAY );
                cvResize( gray, sA, CV_INTER_AREA );
                cvCvtColor( B, gray, CV_BGR2GRAY );
                cvResize( gray, sB, CV_INTER_AREA );
            }
            else if( cn == 1 )
            {
                cvResize( gray, sA, CV_INTER_AREA );
                cvResize( gray, sB, CV_INTER_AREA );
            }
            else
            {
                cvCvtColor( A, gray, CV_BGR2GRAY );
                cvResize( gray, sA, CV_INTER_AREA );
                cvCvtColor( B, gray, CV_BGR2GRAY );
            }

            cvReleaseMat( &gray );
            A = sA;
            B = sB;
        }

        count_y = COUNT;
        count_x = cvRound((float)COUNT*sz1.width/sz1.height);
        count = count_x * count_y;

        ( pA = (CvPoint2D32f*)cvAlloc( count*sizeof(pA[0]) ));
        ( pB = (CvPoint2D32f*)cvAlloc( count*sizeof(pB[0]) ));
        ( status = (char*)cvAlloc( count*sizeof(status[0]) ));

        for( i = 0, k = 0; i < count_y; i++ )
            for( j = 0; j < count_x; j++, k++ )
            {
                pA[k].x = (j+0.5f)*sz1.width/count_x;
                pA[k].y = (i+0.5f)*sz1.height/count_y;
            }

        // find the corresponding points in B
        cvCalcOpticalFlowPyrLK( A, B, 0, 0, pA, pB, count, cvSize(10,10), 3,
                                status, 0, cvTermCriteria(CV_TERMCRIT_ITER,40,0.1), 0 );

        // repack the remained points
        for( i = 0, k = 0; i < count; i++ )
            if( status[i] )
            {
                if( i > k )
                {
                    pA[k] = pA[i];
                    pB[k] = pB[i];
                }
                k++;
            }

        count = k;
    }
    else if( CV_MAT_TYPE(A->type) == CV_32FC2 || CV_MAT_TYPE(A->type) == CV_32SC2 )
    {
        count = A->cols*A->rows;

        if( CV_IS_MAT_CONT(A->type & B->type) && CV_MAT_TYPE(A->type) == CV_32FC2 )
        {
            pA = (CvPoint2D32f*)A->data.ptr;
            pB = (CvPoint2D32f*)B->data.ptr;
            allocated = 0;
        }
        else
        {
            CvMat _pA, _pB;

            ( pA = (CvPoint2D32f*)cvAlloc( count*sizeof(pA[0]) ));
            ( pB = (CvPoint2D32f*)cvAlloc( count*sizeof(pB[0]) ));
            _pA = cvMat( A->rows, A->cols, CV_32FC2, pA );
            _pB = cvMat( B->rows, B->cols, CV_32FC2, pB );
            cvConvert( A, &_pA );
            cvConvert( B, &_pB );
        }
    }
    else
        CV_ERROR( CV_StsUnsupportedFormat, "Both input images must have either 8uC1 or 8uC3 type" );

    ( good_idx = (int*)cvAlloc( count*sizeof(good_idx[0]) ));

    if( count < RANSAC_SIZE0 )
	    k=0;
    else

    // RANSAC stuff:
    // 1. find the consensus
    for( k = 0; k < RANSAC_MAX_ITERS; k++ )
    {
        int idx[RANSAC_SIZE0];
        CvPoint2D32f a[3];
        CvPoint2D32f b[3];

        memset( a, 0, sizeof(a) );
        memset( b, 0, sizeof(b) );

        // choose random 3 non-complanar points from A & B
        for( i = 0; i < RANSAC_SIZE0; i++ )
        {
            for( k1 = 0; k1 < RANSAC_MAX_ITERS; k1++ )
            {
                idx[i] = cvRandInt(&rng) % count;
                
                for( j = 0; j < i; j++ )
                {
                    if( idx[j] == idx[i] )
                        break;
                    // check that the points are not very close one each other
                    if( fabs(pA[idx[i]].x - pA[idx[j]].x) +
                        fabs(pA[idx[i]].y - pA[idx[j]].y) < MIN_TRIANGLE_SIDE )
                        break;
                    if( fabs(pB[idx[i]].x - pB[idx[j]].x) +
                        fabs(pB[idx[i]].y - pB[idx[j]].y) < MIN_TRIANGLE_SIDE )
                        break;
                }

                if( j < i )
                    continue;

                if( i+1 == RANSAC_SIZE0 )
                {
                    // additional check for non-complanar vectors
                    a[0] = pA[idx[0]];
                    a[1] = pA[idx[1]];
                    a[2] = pA[idx[2]];

                    b[0] = pB[idx[0]];
                    b[1] = pB[idx[1]];
                    b[2] = pB[idx[2]];

                    if( fabs((a[1].x - a[0].x)*(a[2].y - a[0].y) - (a[1].y - a[0].y)*(a[2].x - a[0].x)) < 1 ||
                        fabs((b[1].x - b[0].x)*(b[2].y - b[0].y) - (b[1].y - b[0].y)*(b[2].x - b[0].x)) < 1 )
                        continue;
                }
                break;
            }

            if( k1 >= RANSAC_MAX_ITERS )
                break;
        }

        if( i < RANSAC_SIZE0 )
            continue;

        // estimate the transformation using 3 points
        icvGetRTMatrix( a, b, 3, &M, full_affine );

        for( i = 0, good_count = 0; i < count; i++ )
        {
            if( fabs( m[0]*pA[i].x + m[1]*pA[i].y + m[2] - pB[i].x ) +
                fabs( m[3]*pA[i].x + m[4]*pA[i].y + m[5] - pB[i].y ) < 8 )
                good_idx[good_count++] = i;
        }

        if( good_count >= count*RANSAC_GOOD_RATIO )
            break;
    }

    if( k >= RANSAC_MAX_ITERS )
        result=0;
    else {

	    if( good_count < count )
	    {
		    for( i = 0; i < good_count; i++ )
		    {
			    j = good_idx[i];
			    pA[i] = pA[j];
			    pB[i] = pB[j];
		    }
	    }

	    icvGetRTMatrix( pA, pB, good_count, &M, full_affine );
	    m[2] /= scale;
	    m[5] /= scale;
	    ( cvConvert( &M, _M ));
	    result = 1;
    }

exit:

    cvReleaseMat( &sA );
    cvReleaseMat( &sB );
    cvFree( &pA );
    cvFree( &pB );
    cvFree( &status );
    cvFree( &good_idx );
    cvReleaseMat( &gray );

    return result;
}


/* End of file. */
