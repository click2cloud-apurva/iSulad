/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: tanyifeng
 * Create: 2017-11-22
 * Description: provide container sha256 definition
 ******************************************************************************/
#ifndef SHA256_H
#define SHA256_H 1

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SHA224_SIZE = 224 / 8 };
enum { SHA224_ALIGN = 4 };
enum { SHA256_SIZE = 256 / 4 };
enum { SHA256_ALIGN = 4 };

/* read file stream buffer */
extern int fstream_read(FILE *stream, int fd);

/* read gzfile stream buffer.  */
extern int gzstream_read(gzFile gzstream, int fd);

extern int sha256sum_calculate(void *stream, char *buffer_out, size_t len,
                               bool isfile,
                               bool isgzip);
/* Compute SHA256 (SHA224) message digest for bytes read from STREAM.
   The result is a 64 characters string without prefix "sha256:"  */
char *sha256_digest(void *stream, bool isgzip);


# ifdef __cplusplus
}
# endif

#endif

