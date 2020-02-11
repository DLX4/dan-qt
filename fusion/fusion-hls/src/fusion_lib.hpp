#ifndef _XF_FUSION_LIB_
#define _XF_FUSION_LIB_

#include "xf_fusion.h"

namespace fusion {

// ƽ��ֵ��׼���װ
template<int ROWS, int COLS>
void meanStdDevWrapper(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& src, double& min, double& max) {
#pragma HLS INLINE OFF
#pragma HLS DATAFLOW
	unsigned short tmpMean[1];
	unsigned short tmpSd[1];
	xf::meanStdDev<_TYPE, ROWS, COLS, _NPC1>(src, tmpMean, tmpSd);
	double means = (float)tmpMean[0]/256;
	double sds = (float)tmpSd[0]/256;
	min = means - DYNAMIC * sds;
	max = means + DYNAMIC * sds;
}

// ���µ���ͼƬ����
template<int ROWS, int COLS>
void restoreBrightness(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& src, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dst, double min, double max) {
#pragma HLS INLINE OFF
#pragma HLS DATAFLOW
	// ��һ��
    // ��ͨ���ֱ�����ֵ��������

    int width = dst.cols;
    int height = dst.rows;
    for (int i = 0; i < height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
#pragma HLS UNROLL
        for(int j = 0; j < width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE
            double value = (src.data[i*width+j] - min) * 255 / (max - min);

            if ( value > 255) {
            	dst.write(i*width+j, 255);
            } else if (value < 0) {
            	dst.write(i*width+j, 0);
            } else {
            	dst.write(i*width+j, (unsigned char)value);
            }
        }
    }
}

template<int ROWS, int COLS>
void dstCopyFromSrc(
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& src,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dst
		) {
#pragma HLS INLINE OFF
#pragma HLS DATAFLOW
	int width = dst.cols;
	int height = dst.rows;
	for (int i = 0; i < height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
#pragma HLS UNROLL
		for(int j = 0; j < width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE
			dst.write(i*width+j, src.data[i*width+j]);
		}
	}
}


// ͨ��Դͼ����������˹������ (ע�⣺ͼ��ĳ�����Ҫ��16����)
template<int ROWS, int COLS>
void buildLaplacianPyramids(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& src,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyr0, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyr1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyr2, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyr3, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyr4,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp0Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp1Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp2Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp3Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp4Scale1,
		xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp0Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp1Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp2Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp3Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp4Scale2
) {
#pragma HLS INLINE OFF
    fusion::dstCopyFromSrc<ROWS, COLS>(src, pyr0);

    // ���¹��챾���˹������ ��1��
    xf::pyrDown<_TYPE, ROWS, COLS, _NPC1, true>(pyr0, temp0Scale1);
    fusion::dstCopyFromSrc<ROWS, COLS>(temp0Scale1, pyr1);
    // ���¹��챾���˹������ ��2��
    xf::pyrDown<_TYPE, ROWS, COLS, _NPC1, true>(pyr1, temp1Scale1);
    fusion::dstCopyFromSrc<ROWS, COLS>(temp1Scale1, pyr2);
    // ���¹��챾���˹������ ��3��
    xf::pyrDown<_TYPE, ROWS, COLS, _NPC1, true>(pyr2, temp2Scale1);
    fusion::dstCopyFromSrc<ROWS, COLS>(temp2Scale1, pyr3);
    // ���¹��챾���˹������ ��4��
    xf::pyrDown<_TYPE, ROWS, COLS, _NPC1, true>(pyr3, temp3Scale1);
    fusion::dstCopyFromSrc<ROWS, COLS>(temp3Scale1, pyr4);

    // ��һ���˹��������ȥ�����˹������*2�õ���һ��������˹��������pyr[0~N]������������˹����������N�㣨���һ�㣩������˹������ͬ��˹������
    // ��1��
    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyr1, temp0Scale2);
	{
	#pragma HLS latency min=1 max=1
		xf::absdiff<_TYPE, ROWS, COLS, _NPC1>(pyr0, temp0Scale1, pyr0);
	}

    // ��2��
    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyr2, temp1Scale2);
	{
		#pragma HLS latency min=1 max=1
		xf::absdiff<_TYPE, ROWS, COLS, _NPC1>(pyr1, temp1Scale1, pyr1);
	}

    // ��3��
    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyr3, temp2Scale2);
	{
		#pragma HLS latency min=1 max=1
		xf::absdiff<_TYPE, ROWS, COLS, _NPC1>(pyr2, temp2Scale1, pyr2);
	}

    // ��4��
    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyr4, temp3Scale2);
	{
	#pragma HLS latency min=1 max=1
		xf::absdiff<_TYPE, ROWS, COLS, _NPC1>(pyr3, temp3Scale1, pyr3);
	}
}


// �ںϲ���Ϊ��������
template<int ROWS, int COLS>
void blendLaplacianPyramidsByRE2(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageA, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageB, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageS) {
#pragma HLS INLINE OFF
#pragma HLS DATAFLOW
	// ��ֵ�˲�
    static double G[3][3] = {
        {0.1111, 0.1111, 0.1111},
        {0.1111, 0.1111, 0.1111},
        {0.1111, 0.1111, 0.1111}
    };
    double matchDegreeLimit = 0.618;

    int height = imageA.rows;
    int width = imageB.cols;

    for (int i = 0; i < height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
        for (int j = 0; j < width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE

            // �����߽�
            if ((i > 1) && (i < (height - 2)) && (j > 1) && (j < (width - 2))) {
                // 3*3
                static double deltaA = 0.0;
                static double deltaB = 0.0;
                static double matchDegree = 0.0;

                for (int rowOffset = -1; rowOffset <= 1; rowOffset++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=3
#pragma HLS UNROLL
                    for (int colOffset= -1; colOffset <= 1; colOffset++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=3
                        // ��ͨ��
                        int x = i + rowOffset;
                        int y = j + colOffset;

                        deltaA += G[1+rowOffset][1+colOffset] * imageA.data[x*width+y] * imageA.data[x*width+y];
                        deltaB += G[1+rowOffset][1+colOffset] * imageB.data[x*width+y] * imageB.data[x*width+y];
                        matchDegree += G[1+rowOffset][1+colOffset] * imageA.data[x*width+y] * imageB.data[x*width+y];
                    }
                }
                // ����ƥ���
                matchDegree = matchDegree * matchDegree / (deltaA * deltaB);

                if (hls::isnan(matchDegree) || matchDegree < matchDegreeLimit) {
                    if (deltaA == deltaB) {
                        imageS.write(i*width+j, 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j]);
                    } else if (deltaA > deltaB) {
                        imageS.write(i*width+j, imageA.data[i*width+j]);
                    } else {
                        imageS.write(i*width+j, imageB.data[i*width+j]);
                    }
                } else {
                    double wMin = 0.5 * (1 - (1 - matchDegree)/(1 - matchDegreeLimit));
                    imageS.write(i*width+j, hls::min(imageA.data[i*width+j], imageB.data[i*width+j]) * wMin + hls::max(imageA.data[i*width+j], imageB.data[i*width+j]) * (1 - wMin));
                }
            } else {
                // �߽�55�����
                imageS.write(i*width+j, 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j]);
            }
        }
    }
}


// ������ԭͼ���������˹�������ں�
template<int ROWS, int COLS>
void blendLaplacianPyramids(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrA0, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrA1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrA2, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrA3, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrA4,
                            xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrB0, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrB1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrB2, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrB3, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrB4,
                            xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrS0, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrS1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrS2, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrS3, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrS4,
							xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp0Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp1Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp2Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp3Scale1, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& temp4Scale1,
							xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp0Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp1Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp2Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp3Scale2, xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& temp4Scale2,
                            xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dst) {
#pragma HLS INLINE OFF
    // ������˹����������ֱ��ں� 0 1 2 3 4
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA0, pyrB0, pyrS0);
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA1, pyrB1, pyrS1);
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA2, pyrB2, pyrS2);
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA3, pyrB3, pyrS3);
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA4, pyrB4, pyrS4);

    // ���ͼ�� 4 3 2 1
    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrS4, temp3Scale2);
    xf::add<XF_CONVERT_POLICY_SATURATE, _TYPE, ROWS, COLS, _NPC1>(pyrS3, temp3Scale1, pyrS3);

    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrS3, temp2Scale2);
    xf::add<XF_CONVERT_POLICY_SATURATE, _TYPE, ROWS, COLS, _NPC1>(pyrS2, temp2Scale1, pyrS2);

    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrS2, temp1Scale2);
    xf::add<XF_CONVERT_POLICY_SATURATE, _TYPE, ROWS, COLS, _NPC1>(pyrS1, temp1Scale1, pyrS1);

    xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrS1, temp0Scale2);
    xf::add<XF_CONVERT_POLICY_SATURATE, _TYPE, ROWS, COLS, _NPC1>(pyrS0, temp0Scale1, pyrS0);

    // ��������
    double min = 0.0;
    double max = 0.0;
    fusion::meanStdDevWrapper(pyrS0, min, max);
    fusion::restoreBrightness<ROWS, COLS>(pyrS0, dst, min, max);
}
}
#endif
