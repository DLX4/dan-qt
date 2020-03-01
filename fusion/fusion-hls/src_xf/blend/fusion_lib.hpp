#ifndef _XF_FUSION_LIB_
#define _XF_FUSION_LIB_

#include "xf_fusion.h"

namespace fusion {

// ƽ��ֵ��׼���װ
template<int ROWS, int COLS>
void meanStdDevWrapper(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& src, double& min, double& max) {
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

template<int ROWS, int COLS>
void scaleDown(
		xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& src,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dst
		) {
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

// ͨ��Դͼ����������˹������
template<int ROWS, int COLS>
void lapPyrUpSubLevel(
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrX,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrU,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& tempScale1,
		xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& tempScale2,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dstX
) {
	// ��һ���˹��������ȥ�����˹������*2�õ���һ��������˹��������pyr[0~N]������������˹����������N�㣨���һ�㣩������˹������ͬ��˹������
	xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrX, tempScale2);
	tempScale1.rows = pyrU.rows;
	tempScale1.cols = pyrU.cols;
	fusion::scaleDown(tempScale2, tempScale1);
	{
	#pragma HLS latency min=1 max=1
		xf::absdiff<_TYPE, ROWS, COLS, _NPC1>(pyrU, tempScale1, dstX);
	}
}

// �ںϲ���Ϊ��������
template<int ROWS, int COLS>
void blendLaplacianPyramidsBorder(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageA, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageB, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageS) {
    int height = imageA.rows;
    int width = imageB.cols;

    for (int i = 0; i <= 1; i++) {
#pragma HLS LOOP_TRIPCOUNT min=2 max=2
		for (int j = 0; j < width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE
			// �߽�55�����
			imageS.data[i*width+j] = 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j];
		}
    }

    for (int i = height-1; i >= height - 2; i--) {
#pragma HLS LOOP_TRIPCOUNT min=2 max=2
		for (int j = 0; j < width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE
			// �߽�55�����
			imageS.data[i*width+j] = 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j];
		}
    }

    for (int i = 0; i < height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
		for (int j = 0; j <= 1; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2
#pragma HLS PIPELINE
			// �߽�55�����
			imageS.data[i*width+j] = 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j];
		}
    }

    for (int i = 0; i < height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
		for (int j = width-1; j <= width - 2; j--) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2
#pragma HLS PIPELINE
			// �߽�55�����
			imageS.data[i*width+j] = 0.5 * imageA.data[i*width+j] + 0.5 * imageB.data[i*width+j];
		}
    }
}

template<int ROWS, int COLS>
void blendLaplacianPyramidsByRE2(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageA, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageB, xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageS) {
	// ��ֵ�˲�
    float G[3][3] = {
        {0.1111, 0.1111, 0.1111},
        {0.1111, 0.1111, 0.1111},
        {0.1111, 0.1111, 0.1111}
    };
    float matchDegreeLimit = 0.618;

    int height = imageA.rows;
    int width = imageB.cols;

    for (int i = 2; i < height - 2; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
        for (int j = 2; j < width - 2; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE
        	// 3*3
        	float deltaA = 0.0;
        	float deltaB = 0.0;
			float matchDegree = 0.0;

			for (int rowOffset = -1; rowOffset <= 1; rowOffset++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=3
#pragma HLS UNROLL
				for (int colOffset= -1; colOffset <= 1; colOffset++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=3
					// ��ͨ��
					int x = i + rowOffset;
					int y = j + colOffset;
					int index = (int)(x*width+y);

					deltaA += G[1+rowOffset][1+colOffset] * imageA.read(index) * imageA.read(index);
					deltaB += G[1+rowOffset][1+colOffset] * imageB.read(index) * imageB.read(index);
					matchDegree += G[1+rowOffset][1+colOffset] * imageA.read(index) * imageB.read(index);
				}
			}
			// ����ƥ���
			matchDegree = matchDegree * matchDegree / (deltaA * deltaB);

			int pix = (int)(i*width+j);
			if (hls::isnan(matchDegree) || matchDegree < matchDegreeLimit) {
				if (deltaA == deltaB) {
					imageS.write(pix, (int)(0.5 * imageA.read(pix) + 0.5 * imageB.read(pix)));
				} else if (deltaA > deltaB) {
					imageS.write(pix, imageA.read(pix));
				} else {
					imageS.write(pix, imageB.read(pix));
				}
			} else {
				float wMin = 0.5 * (1 - (1 - matchDegree)/(1 - matchDegreeLimit));

				int min = hls::min<int>(imageA.read(pix), imageB.read(pix));
				int max = hls::max<int>(imageA.read(pix), imageB.read(pix));
				float value = min * wMin + max * (1 - wMin);
				imageS.write(pix, (int)value);
			}
        }
    }
}

template<int ROWS, int COLS>
void operatorOperator(
		hls::stream< ap_uint<8> > &_srcA,
		hls::stream< ap_uint<8> > &_srcB,
		hls::stream< ap_uint<8> > &_dst,
		uint16_t img_height, uint16_t img_width)
{
#pragma HLS INLINE
	for (int i = 0; i < img_height; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
		for (int j = 0; j < img_width; j++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE II=1
			ap_uint<8> res = (_srcA.read() + _srcB.read()) >> 1;
			_dst.write(res);
		}
	}
}

// �Ż���ͼ���ں��㷨
template<int ROWS, int COLS>
void blendOpt(xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageA,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageB,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& imageS) {
#pragma HLS DATAFLOW
	hls::stream< ap_uint<8> > _imageA_in;
	hls::stream< ap_uint<8> > _imageB_in;
	hls::stream< ap_uint<8> > _imageS_out;
	unsigned int read_pointer = 0;
	for(int i=0; i < imageA.rows; i++)
	{
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
		for(int j=0; j < imageA.cols; j++)
		{
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE II=1
			_imageA_in.write(imageA.read(read_pointer));
			_imageB_in.write(imageB.read(read_pointer));
			read_pointer++;
		}
	}
	operatorOperator<ROWS, COLS>(_imageA_in, _imageB_in, _imageS_out, imageA.rows, imageA.cols);

	unsigned int write_ptr = 0;
	for(int i=0;i<imageA.rows;i++)
	{
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
		for(int j=0;j<imageA.cols;j++)
		{
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE II=1
			ap_uint<8> read_fil_out = _imageS_out.read();
			imageS.write(write_ptr,read_fil_out);
			write_ptr++;
		}
	}
	return;
}

// ͨ��Դͼ����������˹������
template<int ROWS, int COLS>
void lapPyrUpAddLevel(
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrX,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& pyrU,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& tempScale1,
		xf::Mat<_TYPE, ROWS*2, COLS*2, _NPC1>& tempScale2,
		xf::Mat<_TYPE, ROWS, COLS, _NPC1>& dstX
) {
	// ��һ���˹��������ȥ�����˹������*2�õ���һ��������˹��������pyr[0~N]������������˹����������N�㣨���һ�㣩������˹������ͬ��˹������
	xf::pyrUp<_TYPE, ROWS, COLS,  _NPC1>(pyrX, tempScale2);
	tempScale1.rows = pyrU.rows;
	tempScale1.cols = pyrU.cols;
	fusion::scaleDown(tempScale2, tempScale1);
    xf::add<XF_CONVERT_POLICY_SATURATE, _TYPE, ROWS, COLS, _NPC1>(pyrU, tempScale1, dstX);
}
}
#endif