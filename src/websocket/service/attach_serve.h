/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Description: Attach streaming service implementation.
 * Author: wujing
 * Create: 2019-01-02
 ******************************************************************************/

#ifndef __ATTACH_SERVE_H_
#define __ATTACH_SERVE_H_

#include "route_callback_register.h"
#include <chrono>
#include <string>
#include <thread>
#include "ws_server.h"

#include "api.pb.h"
#include "log.h"
#include "callback.h"
#include "request_cache.h"

class AttachServe : public StreamingServeInterface {
public:
    AttachServe() = default;
    AttachServe(const AttachServe &) = delete;
    AttachServe &operator=(const AttachServe &) = delete;
    virtual ~AttachServe() = default;
    int Execute(struct lws *wsi, const std::string &token, int read_pipe_fd) override;
private:
    int RequestFromCri(const runtime::v1alpha2::AttachRequest *grequest,
                       container_attach_request **request);
};
#endif /* __ATTACH_SERVE_H_ */

