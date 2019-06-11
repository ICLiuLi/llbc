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

#include "llbc/core/objectpool/ThreadObjectPoolManager.h"

__LLBC_INTERNAL_NS_BEGIN

static LLBC_NS LLBC_ThreadObjectPool *__g_entryThreadObjectPool = NULL;

__LLBC_INTERNAL_NS_END

__LLBC_NS_BEGIN

void LLBC_ThreadObjectPoolManager::CreateEntryThreadObjectPool()
{
    LLBC_ThreadObjectPool *&objectPool =
        LLBC_INTERNAL_NS __g_entryThreadObjectPool;
    if (!objectPool)
    {
        objectPool = LLBC_New0(LLBC_ThreadObjectPool);
    }
}

void LLBC_ThreadObjectPoolManager::DestroyEntryThreadObjectPool()
{
    LLBC_XDelete(LLBC_INTERNAL_NS __g_entryThreadObjectPool);
}

LLBC_ThreadObjectPool * LLBC_ThreadObjectPoolManager::GetEntryThreadObjectPool()
{
    return LLBC_INTERNAL_NS __g_entryThreadObjectPool;
}

LLBC_ThreadObjectPool * LLBC_ThreadObjectPoolManager::GetCurrentThreadObjectPool()
{
    __LLBC_LibTls *tls = __LLBC_GetLibTls();
    return reinterpret_cast<LLBC_ThreadObjectPool *>(tls->coreTls.objectPool);
}

__LLBC_NS_END

#include "llbc/common/AfterIncl.h"
