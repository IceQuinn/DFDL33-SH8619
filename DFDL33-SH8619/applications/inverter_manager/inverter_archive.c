/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_archive.h"

/* 全局逆变器档案库，固定提供12个档案槽位，上电后由应用程序负责装载和维护。 */
Inv_ArchiveLib_t g_inv_archive_lib;
