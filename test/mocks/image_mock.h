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
 * Author: wangfengtu
 * Create: 2020-02-19
 * Description: provide image mock
 ******************************************************************************/

#ifndef IMAGE_MOCK_H_
#define IMAGE_MOCK_H_

#include <gmock/gmock.h>
#include "image.h"

class MockImage {
public:
    virtual ~MockImage() = default;
    MOCK_METHOD2(ImGetStorageStatus, int(const char *, im_storage_status_response **));
    MOCK_METHOD1(FreeImStorageStatusResponse, void(im_storage_status_response *));
    MOCK_METHOD1(ImContainerExport, int(const im_export_request *request));
    MOCK_METHOD1(FreeImExportRequest, void(im_export_request *ptr));
};

void MockImage_SetMock(MockImage* mock);

#endif  // IMAGE_MOCK_H_
