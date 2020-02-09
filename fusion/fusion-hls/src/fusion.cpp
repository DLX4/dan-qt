#include "xf_fusion.h"

#define DYNAMIC 2
// ���µ���ͼƬ����
template<int ROWS, int COLS>
void restoreBrightness(xf::Mat<TYPE, ROWS, COLS, NPC1>& src, xf::Mat<TYPE, ROWS, COLS, NPC1>& dst) {
    // ��һ��
    // ��ͨ���ֱ�����ֵ��������
    double means = 0.0;
    double sds = 0.0;
    double mins = 0.0;
    double maxs = 0.0;

    unsigned short tmpMean[1];
    unsigned short tmpSd[1];
    xf::meanStdDev<TYPE, ROWS, COLS, NPC1>(src, tmpMean, tmpSd);
    means = (float)tmpMean[0]/256;
    sds = (float)tmpSd[0]/256;
    mins = means - DYNAMIC * sds;
    maxs = means + DYNAMIC * sds;

    int width = dst.cols;
    int height = dst.rows;
    for (int i = 0; i < height; i++) {
        for(int j = 0; j < width; j++) {
            double value = (src.read(i*width+j) - mins) * 255 / (maxs - mins);

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
void pyrDownUpDown(xf::Mat<TYPE, ROWS, COLS, NPC1>& src, xf::Mat<TYPE, ROWS/2, COLS/2, NPC1>& dst) {
	xf::Mat<TYPE, ROWS, COLS, NPC1> temp(ROWS, COLS);
	xf::pyrDown<TYPE, ROWS, COLS,  NPC1, true>(src, temp);
	int width = dst.cols;
	int height = dst.rows;
	for (int i = 0; i < height; i++) {
		for(int j = 0; j < width; j++) {
			dst.write(i*width+j, temp.read(i*width+j));
		}
	}
}

// ͨ��Դͼ����������˹������ (ע�⣺ͼ��ĳ�����Ҫ��16����)
template<int ROWS, int COLS>
void buildLaplacianPyramids(xf::Mat<TYPE, ROWS, COLS, NPC1>& src, xf::Mat<TYPE, ROWS, COLS, NPC1>& pyr0, xf::Mat<TYPE, ROWS/2, COLS/2, NPC1>& pyr1, xf::Mat<TYPE, ROWS/4, COLS/4, NPC1>& pyr2, xf::Mat<TYPE, ROWS/8, COLS/8, NPC1>& pyr3, xf::Mat<TYPE, ROWS/16, COLS/16, NPC1>& pyr4) {
    pyr0 = src;

    // ���¹��챾���˹������ ��1��
    pyrDownUpDown<ROWS, COLS>(pyr0, pyr1);
    // ���¹��챾���˹������ ��2��
    pyrDownUpDown<ROWS/2, COLS/2>(pyr1, pyr2);
    // ���¹��챾���˹������ ��3��
    pyrDownUpDown<ROWS/4, COLS/4>(pyr2, pyr3);
    // ���¹��챾���˹������ ��4��
    pyrDownUpDown<ROWS/8, COLS/8>(pyr3, pyr4);

    // ��һ���˹��������ȥ�����˹������*2�õ���һ��������˹��������pyr[0~N]������������˹����������N�㣨���һ�㣩������˹������ͬ��˹������
    // ��1��
    xf::Mat<TYPE, ROWS, COLS, NPC1> expend1(pyr0.rows, pyr0.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/2, COLS/2,  NPC1>(pyr1, expend1);
    xf::absdiff<TYPE, ROWS, COLS, NPC1>(pyr0, expend1, pyr0);

    // ��2��
    xf::Mat<TYPE, ROWS/2, COLS/2, NPC1> expend2(pyr1.rows, pyr1.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/4, COLS/4,  NPC1>(pyr2, expend2);
    xf::absdiff<TYPE, ROWS/2, COLS/2, NPC1>(pyr1, expend2, pyr1);

    // ��3��
    xf::Mat<TYPE, ROWS/4, COLS/4, NPC1> expend3(pyr2.rows, pyr2.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/8, COLS/8,  NPC1>(pyr3, expend3);
    xf::absdiff<TYPE, ROWS/4, COLS/4, NPC1>(pyr2, expend3, pyr2);

    // ��4��
    xf::Mat<TYPE, ROWS/8, COLS/8, NPC1> expend4(pyr3.rows, pyr3.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/16, COLS/16,  NPC1>(pyr4, expend4);
    xf::absdiff<TYPE, ROWS/8, COLS/8, NPC1>(pyr3, expend4, pyr3);
}


// �ںϲ���Ϊ��������
template<int ROWS, int COLS>
void blendLaplacianPyramidsByRE2(xf::Mat<TYPE, ROWS, COLS, NPC1>& imageA, xf::Mat<TYPE, ROWS, COLS, NPC1>& imageB, xf::Mat<TYPE, ROWS, COLS, NPC1>& imageS) {
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
        for (int j = 0; j < width; j++) {

            // �����߽�
            if ((i > 1) && (i < (height - 2)) && (j > 1) && (j < (width - 2))) {
                // 3*3
                static double deltaA = 0.0;
                static double deltaB = 0.0;
                static double matchDegree = 0.0;
                for (int rowOffset = -1; rowOffset <= 1; rowOffset++) {
                    for (int colOffset= -1; colOffset <= 1; colOffset++) {
                        // ��ͨ��
                        int x = i + rowOffset;
                        int y = j + colOffset;

                        deltaA += G[1+rowOffset][1+colOffset] * hls::pow(imageA.read(x*width+y), 2);
                        deltaB += G[1+rowOffset][1+colOffset] * hls::pow(imageB.read(x*width+y), 2);
                        matchDegree += G[1+rowOffset][1+colOffset] * imageA.read(x*width+y) * imageB.read(x*width+y);
                    }
                }
                // ����ƥ���
                matchDegree = hls::pow(matchDegree, (double)2) / (deltaA * deltaB);

                if (hls::isnan(matchDegree) || matchDegree < matchDegreeLimit) {
                    if (deltaA == deltaB) {
                        imageS.write(i*width+j, 0.5 * imageA.read(i*width+j) + 0.5 * imageB.read(i*width+j));
                    } else if (deltaA > deltaB) {
                        imageS.write(i*width+j, imageA.read(i*width+j));
                    } else {
                        imageS.write(i*width+j, imageB.read(i*width+j));
                    }
                } else {
                    double wMin = 0.5 * (1 - (1 - matchDegree)/(1 - matchDegreeLimit));
                    imageS.write(i*width+j, hls::min(imageA.read(i*width+j), imageB.read(i*width+j)) * wMin + hls::max(imageA.read(i*width+j), imageB.read(i*width+j)) * (1 - wMin));
                }
            } else {
                // �߽�55�����
                imageS.write(i*width+j, 0.5 * imageA.read(i*width+j) + 0.5 * imageB.read(i*width+j));
            }
        }
    }
}


// ������ԭͼ���������˹�������ں�
template<int ROWS, int COLS>
void blendLaplacianPyramids(xf::Mat<TYPE, ROWS, COLS, NPC1>& pyrA0, xf::Mat<TYPE, ROWS/2, COLS/2, NPC1>& pyrA1, xf::Mat<TYPE, ROWS/4, COLS/4, NPC1>& pyrA2, xf::Mat<TYPE, ROWS/8, COLS/8, NPC1>& pyrA3, xf::Mat<TYPE, ROWS/16, COLS/16, NPC1>& pyrA4,
                            xf::Mat<TYPE, ROWS, COLS, NPC1>& pyrB0, xf::Mat<TYPE, ROWS/2, COLS/2, NPC1>& pyrB1, xf::Mat<TYPE, ROWS/4, COLS/4, NPC1>& pyrB2, xf::Mat<TYPE, ROWS/8, COLS/8, NPC1>& pyrB3, xf::Mat<TYPE, ROWS/16, COLS/16, NPC1>& pyrB4,
                            xf::Mat<TYPE, ROWS, COLS, NPC1>& pyrS0, xf::Mat<TYPE, ROWS/2, COLS/2, NPC1>& pyrS1, xf::Mat<TYPE, ROWS/4, COLS/4, NPC1>& pyrS2, xf::Mat<TYPE, ROWS/8, COLS/8, NPC1>& pyrS3, xf::Mat<TYPE, ROWS/16, COLS/16, NPC1>& pyrS4,
                            xf::Mat<TYPE, ROWS, COLS, NPC1>& dst) {

    // ������˹����������ֱ��ں� 0 1 2 3 4
    blendLaplacianPyramidsByRE2<ROWS, COLS>(pyrA0, pyrB0, pyrS0);
    blendLaplacianPyramidsByRE2<ROWS/2, COLS/2>(pyrA1, pyrB1, pyrS1);
    blendLaplacianPyramidsByRE2<ROWS/4, COLS/4>(pyrA2, pyrB2, pyrS2);
    blendLaplacianPyramidsByRE2<ROWS/8, COLS/8>(pyrA3, pyrB3, pyrS3);
    blendLaplacianPyramidsByRE2<ROWS/16, COLS/16>(pyrA4, pyrB4, pyrS4);

    // ���ͼ�� 4 3 2 1
    xf::Mat<TYPE, ROWS/8, COLS/8, NPC1> expend4(pyrS3.rows, pyrS3.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/16, COLS/16,  NPC1>(pyrS4, expend4);
    xf::add<XF_CONVERT_POLICY_SATURATE, TYPE, ROWS/8, COLS/8, NPC1>(pyrS3, expend4, pyrS3);

    xf::Mat<TYPE, ROWS/4, COLS/4, NPC1> expend3(pyrS2.rows, pyrS2.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/8, COLS/8,  NPC1>(pyrS3, expend3);
    xf::add<XF_CONVERT_POLICY_SATURATE, TYPE, ROWS/4, COLS/4, NPC1>(pyrS2, expend3, pyrS2);

    xf::Mat<TYPE, ROWS/2, COLS/2, NPC1> expend2(pyrS1.rows, pyrS1.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/4, COLS/4,  NPC1>(pyrS2, expend2);
    xf::add<XF_CONVERT_POLICY_SATURATE, TYPE, ROWS/2, COLS/2, NPC1>(pyrS1, expend2, pyrS1);

    xf::Mat<TYPE, ROWS, COLS, NPC1> expend1(pyrS0.rows, pyrS0.cols);// upscale 2x
    xf::pyrUp<TYPE, ROWS/2, COLS/2,  NPC1>(pyrS1, expend1);
    xf::add<XF_CONVERT_POLICY_SATURATE, TYPE, ROWS, COLS, NPC1>(pyrS0, expend1, pyrS0);

    // ��������
    restoreBrightness<ROWS, COLS>(pyrS0, dst);
}

void blend(xf::Mat<TYPE, HEIGHT, WIDTH, NPC1>& srcA, xf::Mat<TYPE, HEIGHT, WIDTH, NPC1>& srcB, xf::Mat<TYPE, HEIGHT, WIDTH, NPC1>& dst) {
	int height0 = srcA.rows;
	int width0 = srcA.cols;
	int height1 = height0 / 2;
	int width1 = width0 / 2;
	int height2 = height1 / 2;
	int width2 = width1 / 2;
	int height3 = height2 / 2;
	int width3 = width2 / 2;
	int height4 = height3 / 2;
	int width4 = width3 / 2;

	xf::Mat<TYPE, HEIGHT,    WIDTH,    NPC1> pyrA0(height0, width0);
	xf::Mat<TYPE, HEIGHT/2,  WIDTH/2,  NPC1> pyrA1(height1, width1);
	xf::Mat<TYPE, HEIGHT/4,  WIDTH/4,  NPC1> pyrA2(height2, width2);
	xf::Mat<TYPE, HEIGHT/8,  WIDTH/8,  NPC1> pyrA3(height3, width3);
	xf::Mat<TYPE, HEIGHT/16, WIDTH/16, NPC1> pyrA4(height4, width4);

	xf::Mat<TYPE, HEIGHT,    WIDTH,    NPC1> pyrB0(height0, width0);
	xf::Mat<TYPE, HEIGHT/2,  WIDTH/2,  NPC1> pyrB1(height1, width1);
	xf::Mat<TYPE, HEIGHT/4,  WIDTH/4,  NPC1> pyrB2(height2, width2);
	xf::Mat<TYPE, HEIGHT/8,  WIDTH/8,  NPC1> pyrB3(height3, width3);
	xf::Mat<TYPE, HEIGHT/16, WIDTH/16, NPC1> pyrB4(height4, width4);

	xf::Mat<TYPE, HEIGHT,    WIDTH,    NPC1> pyrS0(height0, width0);
	xf::Mat<TYPE, HEIGHT/2,  WIDTH/2,  NPC1> pyrS1(height1, width1);
	xf::Mat<TYPE, HEIGHT/4,  WIDTH/4,  NPC1> pyrS2(height2, width2);
	xf::Mat<TYPE, HEIGHT/8,  WIDTH/8,  NPC1> pyrS3(height3, width3);
	xf::Mat<TYPE, HEIGHT/16, WIDTH/16, NPC1> pyrS4(height4, width4);

	// ͼ��A ������˹������
	buildLaplacianPyramids<HEIGHT, WIDTH>(srcA, pyrA0, pyrA1, pyrA2, pyrA3, pyrA4);

	// ͼ��B ������˹������
	buildLaplacianPyramids<HEIGHT, WIDTH>(srcB, pyrB0, pyrB1, pyrB2, pyrB3, pyrB4);

	// �ں�
	blendLaplacianPyramids<HEIGHT, WIDTH>(pyrA0, pyrA1, pyrA2, pyrA3, pyrA4,
						   pyrB0, pyrB1, pyrB2, pyrB3, pyrB4,
						   pyrS0, pyrS1, pyrS2, pyrS3, pyrS4,
						   dst);
}
