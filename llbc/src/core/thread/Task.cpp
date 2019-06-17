// The MIT License (MIT)

// Copyright (c) 2013 lailongwei<lailongwei@126.com>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of 
// this software and associated documentation files (the "Software"), to deal in 
// the Software without restriction, including without limitation the rights to 
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of 
// the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "llbc/common/Export.h"
#include "llbc/common/BeforeIncl.h"

#include "llbc/core/os/OS_Atomic.h"

#include "llbc/core/timer/TimerScheduler.h"
#include "llbc/core/objectpool/ObjectPool.h"

#include "llbc/core/thread/MessageBlock.h"
#include "llbc/core/thread/ThreadManager.h"
#include "llbc/core/thread/Guard.h"
#include "llbc/core/thread/Task.h"


__LLBC_INTERNAL_NS_BEGIN

int static __LLBC_BaseTaskEntry(void *arg)
{
    LLBC_NS LLBC_BaseTask *task = reinterpret_cast<LLBC_NS LLBC_BaseTask *>(arg);

    LLBC_NS __LLBC_LibTls *tls = LLBC_NS __LLBC_GetLibTls();

    tls->coreTls.task = task;
    tls->coreTls.timerScheduler = LLBC_New0(LLBC_NS LLBC_TimerScheduler);
    tls->coreTls.objectPool = LLBC_New0(LLBC_NS LLBC_ThreadObjectPool);

    task->OnTaskThreadStart();

    task->Svc();

    task->OnTaskThreadStop();

    tls->coreTls.task = NULL;
    LLBC_Delete(reinterpret_cast<
        LLBC_NS LLBC_TimerScheduler *>(tls->coreTls.timerScheduler));
    LLBC_Delete(reinterpret_cast<
        LLBC_NS LLBC_IObjectPool*>(tls->coreTls.objectPool));

    return 0;
}

__LLBC_INTERNAL_NS_END

__LLBC_NS_BEGIN

LLBC_BaseTask::LLBC_BaseTask(LLBC_ThreadManager *threadMgr)
    : _threadNum(0)
    , _curThreadNum(0)
    , _startCompleted(false)
    , _threadManager(threadMgr ? threadMgr : LLBC_ThreadManagerSingleton)
{
}

LLBC_BaseTask::~LLBC_BaseTask()
{
    Wait();
}

int LLBC_BaseTask::Activate(int threadNum,
                        int flags,
                        int priority,
                        LLBC_Handle groupHandle,
                        const int stackSize[],
                        LLBC_BaseTask *task)
{
    task = task ? task : this;

    _lock.Lock();

    if (_threadManager->CreateThreads(threadNum,
                                      &LLBC_INTERNAL_NS __LLBC_BaseTaskEntry,
                                      task,
                                      flags,
                                      priority,stackSize,
                                      task,
                                      groupHandle) == LLBC_INVALID_HANDLE)
    {
		_lock.Unlock();
        return LLBC_FAILED;
    }

    _threadNum = threadNum;

    _lock.Unlock();

    while (!_startCompleted)
    {
        LLBC_ThreadManager::Sleep(20);
    }

    return LLBC_OK;
}

int LLBC_BaseTask::GetThreadCount() const
{
    LLBC_BaseTask *nonConstTask = const_cast<LLBC_BaseTask *>(this);
    LLBC_LockGuard guard(nonConstTask->_lock);

    return _threadNum;
}

int LLBC_BaseTask::Wait()
{
    return _threadManager->WaitTask(this);
}

int LLBC_BaseTask::Suspend()
{
    return _threadManager->SuspendTask(this);
}

int LLBC_BaseTask::Resume()
{
    return _threadManager->ResumeTask(this);
}

int LLBC_BaseTask::Cancel()
{
    return _threadManager->CancelTask(this);
}

int LLBC_BaseTask::Kill(int signo)
{
    return _threadManager->KillTask(this, signo);
}

int LLBC_BaseTask::Push(LLBC_MessageBlock *block)
{
    _msgQueue.PushBack(block);
    return LLBC_OK;
}

int LLBC_BaseTask::Pop(LLBC_MessageBlock *&block)
{
    _msgQueue.PopFront(block);
    return LLBC_OK;
}

int LLBC_BaseTask::TryPop(LLBC_MessageBlock *&block)
{
    if (_msgQueue.TryPopFront(block))
        return LLBC_OK;

    return LLBC_FAILED;
}

int LLBC_BaseTask::TimedPop(LLBC_MessageBlock *&block, int interval)
{
    if (_msgQueue.TimedPopFront(block, interval))
        return LLBC_OK;

    return LLBC_FAILED;
}

void LLBC_BaseTask::OnTaskThreadStart()
{
    LLBC_LockGuard guard(_lock);
    if (++_curThreadNum == _threadNum)
        _startCompleted = true;
}

void LLBC_BaseTask::OnTaskThreadStop()
{
    while (!_startCompleted)
    {
        LLBC_ThreadManager:: Sleep(20);
    }

    _lock.Lock();
    if (--_curThreadNum == 0)
    {
        _lock.Unlock();
        Cleanup();

        return;
    }

    _lock.Unlock();
}

__LLBC_NS_END

#include "llbc/common/AfterIncl.h"
