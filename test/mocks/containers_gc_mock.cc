/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: jikui
 * Create: 2020-02-25
 * Description: provide containers_gc mock
 ******************************************************************************/

#include "containers_gc_mock.h"

namespace {
MockContainersGc *g_containers_gc_mock = NULL;
}

void MockContainersGc_SetMock(MockContainersGc *mock)
{
    g_containers_gc_mock = mock;
}

bool gc_is_gc_progress(const char *id)
{
    if (g_containers_gc_mock != nullptr) {
        return g_containers_gc_mock->GcIsGcProgress(id);
    }
    return true;
}
