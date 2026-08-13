#ifndef __MEAS_CFG_H__
#define __MEAS_CFG_H__


enum{
    ENUM_PHASE_A = 0, ENUM_PHASE_B = 1, ENUM_PHASE_C = 2,                   ENUM_PHASE_MAX = 3,
    ENUM_CHNL_UA = 0, ENUM_CHNL_UB = 1, ENUM_CHNL_UC = 2, ENUM_CHNL_IA = 3, ENUM_CHNL_IB   = 4, ENUM_CHNL_IC = 5, ENUM_CHNL_IN = 6, ENUM_CHNL_MAX= 7,
    ENUM_UA      = 0, ENUM_UB      = 1, ENUM_UC      = 2,                   ENUM_UMAX      = 3,
    ENUM_IA      = 0, ENUM_IB      = 1, ENUM_IC      = 2, ENUM_IN      = 3, ENUM_IMAX      = 4,
    ENUM_PA      = 0, ENUM_PB      = 1, ENUM_PC      = 2, ENUM_PT      = 3, ENUM_PMAX      = 4,
    ENUM_QA      = 0, ENUM_QB      = 1, ENUM_QC      = 2, ENUM_QT      = 3, ENUM_QMAX      = 4,
    ENUM_SA      = 0, ENUM_SB      = 1, ENUM_SC      = 2, ENUM_ST      = 3, ENUM_SMAX      = 4,
    ENUM_PFA     = 0, ENUM_PFB     = 1, ENUM_PFC     = 2, ENUM_PFT     = 3, ENUM_PFMAX     = 4,
};

#endif /* __MEAS_CFG_H__ */
