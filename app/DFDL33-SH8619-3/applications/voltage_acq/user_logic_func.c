#include "user_logic_func.h"

#include <stdint.h>
#include <math.h>

#define CN_SAM_RATE_DER  (1.0f / 40.0f)

typedef struct {
    float Re;    // 实部
    float Im;    // 虚部
    float Amp;   // (Re²+Im²)/2，即有效值的平方
} vector;

// S1ReTab[n] = cos(2π * n / 40), n = 0~10
static const float S1ReTab[11] = {
    1.0f,       // n=0:  cos(0) = 1
    0.987688f,  // n=1:  cos(2π/40)
    0.951057f,  // n=2
    0.891007f,  // n=3
    0.809017f,  // n=4
    0.707107f,  // n=5
    0.587785f,  // n=6
    0.453991f,  // n=7
    0.309017f,  // n=8
    0.156434f,  // n=9
    0.0f        // n=10: cos(π/2) = 0
};

// S1ImTab[n] = sin(2π * n / 40), n = 0~10
static const float S1ImTab[11] = {
    0.0f,       // n=0:  sin(0) = 0
    0.156434f,  // n=1
    0.309017f,  // n=2
    0.453991f,  // n=3
    0.587785f,  // n=4
    0.707107f,  // n=5
    0.809017f,  // n=6
    0.891007f,  // n=7
    0.951057f,  // n=8
    0.987688f,  // n=9
    1.0f        // n=10: sin(π/2) = 1
};

void Alg_FourierAmpVecS1P40(vector *ptAmpVec, int32_t *piSamData, float_t fChannelCoe, float_t fBalanceCoe)
{
    float_t    fRe=0.0;
    float_t    fIm=0.0;

    fRe += (piSamData[10] - piSamData[30]) * S1ReTab[10];//加快计算速度
    fIm += (piSamData[0] - piSamData[20]) * S1ImTab[0];
    fRe += ((piSamData[1] - piSamData[21]) + (piSamData[19] - piSamData[39])) * S1ReTab[1];
    fIm += ((piSamData[1] - piSamData[21]) - (piSamData[19] - piSamData[39])) * S1ImTab[1];
    fRe += ((piSamData[2] - piSamData[22]) + (piSamData[18] - piSamData[38])) * S1ReTab[2];
    fIm += ((piSamData[2] - piSamData[22]) - (piSamData[18] - piSamData[38])) * S1ImTab[2];
    fRe += ((piSamData[3] - piSamData[23]) + (piSamData[17] - piSamData[37])) * S1ReTab[3];
    fIm += ((piSamData[3] - piSamData[23]) - (piSamData[17] - piSamData[37])) * S1ImTab[3];
    fRe += ((piSamData[4] - piSamData[24]) + (piSamData[16] - piSamData[36])) * S1ReTab[4];
    fIm += ((piSamData[4] - piSamData[24]) - (piSamData[16] - piSamData[36])) * S1ImTab[4];
    fRe += ((piSamData[5] - piSamData[25]) + (piSamData[15] - piSamData[35])) * S1ReTab[5];
    fIm += ((piSamData[5] - piSamData[25]) - (piSamData[15] - piSamData[35])) * S1ImTab[5];
    fRe += ((piSamData[6] - piSamData[26]) + (piSamData[14] - piSamData[34])) * S1ReTab[6];
    fIm += ((piSamData[6] - piSamData[26]) - (piSamData[14] - piSamData[34])) * S1ImTab[6];
    fRe += ((piSamData[7] - piSamData[27]) + (piSamData[13] - piSamData[33])) * S1ReTab[7];
    fIm += ((piSamData[7] - piSamData[27]) - (piSamData[13] - piSamData[33])) * S1ImTab[7];
    fRe += ((piSamData[8] - piSamData[28]) + (piSamData[12] - piSamData[32])) * S1ReTab[8];
    fIm += ((piSamData[8] - piSamData[28]) - (piSamData[12] - piSamData[32])) * S1ImTab[8];
    fRe += ((piSamData[9] - piSamData[29]) + (piSamData[11] - piSamData[31])) * S1ReTab[9];
    fIm += ((piSamData[9] - piSamData[29]) - (piSamData[11] - piSamData[31])) * S1ImTab[9];

   fRe *= (2.0 * CN_SAM_RATE_DER * fChannelCoe * fBalanceCoe);      //需注意40点的时候，CN_SAM_RATE_DER要记得更改
   fIm *= (2.0 * CN_SAM_RATE_DER * fChannelCoe * fBalanceCoe);

   ptAmpVec->Re = fRe;                            // 实部
   ptAmpVec->Im = fIm;                            // 虚部
   ptAmpVec->Amp = (fRe*fRe + fIm*fIm)/2;
}


/* ================================================================== */
/*          40 点基波提取 → 有效值计算                                  */
/* ================================================================== */
float calculate_rms(uint16_t *buf, uint8_t len)
{
    int32_t piSamData[40];
    vector amp_vec;

    uint32_t sum = 0;
    for (int i = 0; i < 40; i++)
        sum += buf[i];
    int32_t dc_offset = sum / 40;

    for (int i = 0; i < 40; i++)
        piSamData[i] = (int32_t)buf[i] - dc_offset;

    float fChannelCoe = 3.3f / 4095.0f;
    float fBalanceCoe = 1.0f;

    Alg_FourierAmpVecS1P40(&amp_vec, piSamData, fChannelCoe, fBalanceCoe);

    return sqrtf(amp_vec.Amp);
}

