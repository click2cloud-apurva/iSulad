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
 * Author: zhangsong
 * Create: 2019-04-26
 * Description: provide grpc tls request authorization
 ******************************************************************************/

#ifndef _GRPC_SERVER_TLS_AUTH_H_
#define _GRPC_SERVER_TLS_AUTH_H_
#include <string>
#include <grpc++/grpc++.h>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

namespace AuthorizationPluginConfig {
extern std::string auth_plugin;
};

namespace GrpcServerTlsAuth {
Status auth(ServerContext *context, std::string action);
};

#endif /* _GRPC_SERVER_TLS_AUTH_H_ */

