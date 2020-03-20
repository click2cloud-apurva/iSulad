/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Description: specs llt
 * Author: lifeng
 * Create: 2020-02-18
 */

#include <stdlib.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "mock.h"
#include "oci_runtime_spec.h"
#include "specs.h"
#include "host_config.h"
#include "container_config.h"
#include "oci_llt_common.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "isulad_config_mock.h"
#include "utils.h"

using ::testing::Args;
using ::testing::ByRef;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::NotNull;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::_;

using namespace std;

class SpecsUnitTest : public testing::Test {
public:
    void SetUp() override
    {
        MockIsuladConf_SetMock(&m_isulad_conf);
        ::testing::Mock::AllowLeak(&m_isulad_conf);
    }
    void TearDown() override
    {
        MockIsuladConf_SetMock(nullptr);
    }

    NiceMock<MockIsuladConf> m_isulad_conf;
};

#define HOST_CONFIG_FILE "specs/specs/hostconfig.json"
#define OCI_RUNTIME_SPEC_FILE "specs/specs/oci_runtime_spec.json"

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup_1)
{
    // All parameter NULL
    ASSERT_NE(merge_conf_cgroup(NULL, NULL), 0);
}

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup_2)
{
    oci_runtime_spec *oci_spec = NULL;

    // Parameter host_spec is NULL
    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);
    ASSERT_NE(merge_conf_cgroup(oci_spec, NULL), 0);
    free_oci_runtime_spec(oci_spec);
    oci_spec = NULL;
}

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup_3)
{
    char *host_config_file = NULL;
    host_config *host_spec = NULL;
    char *err = NULL;

    // Parameter oci_spec is NULL
    host_config_file = json_path(HOST_CONFIG_FILE);
    ASSERT_TRUE(host_config_file != NULL);
    host_spec = host_config_parse_file(host_config_file, NULL, &err);
    ASSERT_TRUE(host_spec != NULL);
    free(err);
    err = NULL;
    free(host_config_file);
    host_config_file = NULL;
    ASSERT_NE(merge_conf_cgroup(NULL, host_spec), 0);
    free_host_config(host_spec);
    host_spec = NULL;
}

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup)
{
    char *host_config_file = NULL;
    host_config *host_spec = NULL;
    oci_runtime_spec *oci_spec = NULL;
    char *err = NULL;

    // All parameter correct
    host_config_file = json_path(HOST_CONFIG_FILE);
    ASSERT_TRUE(host_config_file != NULL);
    host_spec = host_config_parse_file(host_config_file, NULL, &err);
    ASSERT_TRUE(host_spec != NULL);
    free(err);
    err = NULL;
    free(host_config_file);
    host_config_file = NULL;

    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);

    ASSERT_EQ(merge_conf_cgroup(oci_spec, host_spec), 0);

    free_host_config(host_spec);
    host_spec = NULL;
    free_oci_runtime_spec(oci_spec);
    oci_spec = NULL;
}

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup_cpu)
{
    char *host_config_file = NULL;
    host_config *host_spec = NULL;
    char *oci_config_file = NULL;
    oci_runtime_spec *oci_spec = NULL;
    char *err = NULL;

    // cpu
    host_config_file = json_path(HOST_CONFIG_FILE);
    ASSERT_TRUE(host_config_file != NULL);
    host_spec = host_config_parse_file(host_config_file, NULL, &err);
    ASSERT_TRUE(host_spec != NULL);
    free(err);
    err = NULL;
    free(host_config_file);
    host_config_file = NULL;

    oci_config_file = json_path(OCI_RUNTIME_SPEC_FILE);
    ASSERT_TRUE(oci_config_file != NULL);
    oci_spec = oci_runtime_spec_parse_file(oci_config_file, NULL, &err);
    ASSERT_TRUE(oci_spec != NULL);
    free(err);
    err = NULL;
    free(oci_config_file);
    oci_config_file = NULL;

    host_spec->cpu_period = 123;
    host_spec->cpu_quota = 234;
    host_spec->cpu_realtime_period = 456;
    host_spec->cpu_realtime_runtime = 789;
    host_spec->cpu_shares = 321;
    free(host_spec->cpuset_cpus);
    host_spec->cpuset_cpus = util_strdup_s("0-3");
    free(host_spec->cpuset_mems);
    host_spec->cpuset_mems = util_strdup_s("0");

    ASSERT_EQ(merge_conf_cgroup(oci_spec, host_spec), 0);

    ASSERT_EQ(oci_spec->linux->resources->cpu->period, 123);
    ASSERT_EQ(oci_spec->linux->resources->cpu->quota, 234);
    ASSERT_EQ(oci_spec->linux->resources->cpu->realtime_period, 456);
    ASSERT_EQ(oci_spec->linux->resources->cpu->realtime_runtime, 789);
    ASSERT_EQ(oci_spec->linux->resources->cpu->shares, 321);
    ASSERT_STREQ(oci_spec->linux->resources->cpu->cpus, "0-3");
    ASSERT_STREQ(oci_spec->linux->resources->cpu->mems, "0");

    free_host_config(host_spec);
    host_spec = NULL;
    free_oci_runtime_spec(oci_spec);
    oci_spec = NULL;
}

TEST(merge_conf_cgroup_llt, test_merge_conf_cgroup_mem)
{
    char *host_config_file = NULL;
    host_config *host_spec = NULL;
    char *oci_config_file = NULL;
    oci_runtime_spec *oci_spec = NULL;
    char *err = NULL;

    // mem
    // int64_t kernel_memory;
    // int64_t memory_reservation;
    // int64_t memory_swap;
    host_config_file = json_path(HOST_CONFIG_FILE);
    ASSERT_TRUE(host_config_file != NULL);
    host_spec = host_config_parse_file(host_config_file, NULL, &err);
    ASSERT_TRUE(host_spec != NULL);
    free(err);
    err = NULL;
    free(host_config_file);
    host_config_file = NULL;

    oci_config_file = json_path(OCI_RUNTIME_SPEC_FILE);
    ASSERT_TRUE(oci_config_file != NULL);
    oci_spec = oci_runtime_spec_parse_file(oci_config_file, NULL, &err);
    ASSERT_TRUE(oci_spec != NULL);
    free(err);
    err = NULL;
    free(oci_config_file);
    oci_config_file = NULL;

    host_spec->kernel_memory = 123;
    host_spec->memory_reservation = 234;
    host_spec->memory_swap = 456;

    ASSERT_EQ(merge_conf_cgroup(oci_spec, host_spec), 0);

    ASSERT_EQ(oci_spec->linux->resources->memory->kernel, 123);
    ASSERT_EQ(oci_spec->linux->resources->memory->reservation, 234);
    ASSERT_EQ(oci_spec->linux->resources->memory->swap, 456);

    free_host_config(host_spec);
    host_spec = NULL;
    free_oci_runtime_spec(oci_spec);
    oci_spec = NULL;
}

/* conf get routine rootdir */
char *invoke_conf_get_isulad_cgroup_parent_null()
{
    return nullptr;
}

/* conf get routine rootdir */
char *invoke_conf_get_isulad_cgroup_parent()
{
    return util_strdup_s("/var/lib/isulad/engines/lcr");
}

TEST_F(SpecsUnitTest, test_merge_oci_cgroups_path_1)
{
    ASSERT_EQ(merge_oci_cgroups_path(nullptr, nullptr, nullptr), -1);
}

TEST_F(SpecsUnitTest, test_merge_oci_cgroups_path_2)
{
    oci_runtime_spec *oci_spec = nullptr;
    host_config *host_spec = nullptr;

    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);

    host_spec = (host_config *) util_common_calloc_s(sizeof(host_config));
    ASSERT_TRUE(host_spec != NULL);

    EXPECT_CALL(m_isulad_conf, GetCgroupParent()).WillRepeatedly(Invoke(invoke_conf_get_isulad_cgroup_parent_null));

    ASSERT_EQ(merge_oci_cgroups_path("123", oci_spec, host_spec), 0);

    ASSERT_STREQ(oci_spec->linux->cgroups_path, "/isulad/123");

    free_oci_runtime_spec(oci_spec);
    free_host_config(host_spec);

    testing::Mock::VerifyAndClearExpectations(&m_isulad_conf);
}

TEST_F(SpecsUnitTest, test_merge_oci_cgroups_path_3)
{
    oci_runtime_spec *oci_spec = nullptr;
    host_config *host_spec = nullptr;

    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);

    host_spec = (host_config *) util_common_calloc_s(sizeof(host_config));
    ASSERT_TRUE(host_spec != NULL);

    host_spec->cgroup_parent = util_strdup_s("/test");

    EXPECT_CALL(m_isulad_conf, GetCgroupParent()).WillRepeatedly(Invoke(invoke_conf_get_isulad_cgroup_parent_null));

    ASSERT_EQ(merge_oci_cgroups_path("123", oci_spec, host_spec), 0);

    ASSERT_STREQ(oci_spec->linux->cgroups_path, "/test/123");

    free_oci_runtime_spec(oci_spec);
    free_host_config(host_spec);

    testing::Mock::VerifyAndClearExpectations(&m_isulad_conf);
}

TEST_F(SpecsUnitTest, test_merge_oci_cgroups_path_4)
{
    oci_runtime_spec *oci_spec = nullptr;
    host_config *host_spec = nullptr;

    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);

    host_spec = (host_config *) util_common_calloc_s(sizeof(host_config));
    ASSERT_TRUE(host_spec != NULL);

    EXPECT_CALL(m_isulad_conf, GetCgroupParent()).WillRepeatedly(Invoke(invoke_conf_get_isulad_cgroup_parent));

    ASSERT_EQ(merge_oci_cgroups_path("123", oci_spec, host_spec), 0);

    ASSERT_STREQ(oci_spec->linux->cgroups_path, "/var/lib/isulad/engines/lcr/123");

    free_oci_runtime_spec(oci_spec);
    free_host_config(host_spec);

    testing::Mock::VerifyAndClearExpectations(&m_isulad_conf);
}

TEST_F(SpecsUnitTest, test_merge_oci_cgroups_path_5)
{
    oci_runtime_spec *oci_spec = nullptr;
    host_config *host_spec = nullptr;

    oci_spec = (oci_runtime_spec *) util_common_calloc_s(sizeof(oci_runtime_spec));
    ASSERT_TRUE(oci_spec != NULL);

    host_spec = (host_config *) util_common_calloc_s(sizeof(host_config));
    ASSERT_TRUE(host_spec != NULL);

    host_spec->cgroup_parent = util_strdup_s("/test");

    EXPECT_CALL(m_isulad_conf, GetCgroupParent()).WillRepeatedly(Invoke(invoke_conf_get_isulad_cgroup_parent));

    ASSERT_EQ(merge_oci_cgroups_path("123", oci_spec, host_spec), 0);

    ASSERT_STREQ(oci_spec->linux->cgroups_path, "/test/123");

    free_oci_runtime_spec(oci_spec);
    free_host_config(host_spec);

    testing::Mock::VerifyAndClearExpectations(&m_isulad_conf);
}
