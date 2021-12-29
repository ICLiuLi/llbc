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

#include "llbc/core/objectpool/Common.h"

#include "llbc/core/event/EventFirer.h"
#include "llbc/core/event/EventManager.h"

__LLBC_NS_BEGIN

void LLBC_EventFirer::Fire()
{
    if (LIKELY(_ev))
    {
        _evMgr->Fire(_ev);
        _ev = nullptr;
        _evMgr = nullptr;
    }

    LLBC_Recycle(this);
}

void LLBC_EventFirer::Clear()
{
    if (LIKELY(_ev))
    {
        LLBC_Recycle(_ev);
        _ev = nullptr;

        _evMgr = nullptr;
    }
}

void LLBC_EventFirer::OnPoolInstCreate(LLBC_IObjectPoolInst &poolInst)
{
    LLBC_IObjectPool *objPool = poolInst.GetIObjectPool();
    objPool->AcquireOrderedDeletePoolInst(typeid(LLBC_EventFirer).name(), typeid(LLBC_Event).name());
}

void LLBC_EventFirer::SetEventInfo(LLBC_Event *ev, LLBC_EventManager *evMgr)
{
    _ev = ev;
    _evMgr = evMgr;
}

__LLBC_NS_END

#include "llbc/common/AfterIncl.h"
