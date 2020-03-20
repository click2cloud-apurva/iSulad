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
 * Author: wujing
 * Create: 2019-4-19
 * Description: provide stoppable thread definition
 *********************************************************************************/
#ifndef __STOPPABLE_THREAD_H_
#define __STOPPABLE_THREAD_H_

#include <iostream>
#include <chrono>
#include <future>
#include <mutex>
#include <utility>

class StoppableThread {
public:
    StoppableThread() : m_future_obj(m_exit_signal.get_future()) {}

    explicit StoppableThread(StoppableThread &&obj) : m_exit_signal(std::move(obj.m_exit_signal)),
        m_future_obj(std::move(obj.m_future_obj)) {}

    StoppableThread &operator=(StoppableThread &&obj);

    virtual ~StoppableThread() = default;

    virtual void run() = 0;

    void operator()()
    {
        run();
    }

    bool stopRequested();

    void stop();

private:
    std::promise<void> m_exit_signal;
    std::future<void> m_future_obj;
};

#endif /* __STOPPABLE_THREAD_H_ */

