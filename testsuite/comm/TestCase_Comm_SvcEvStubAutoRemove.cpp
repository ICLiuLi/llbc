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


#include "comm/TestCase_Comm_SvcEvStubAutoRemove.h"

namespace
{
    LLBC_EventMgr *eventMgr;
    enum EventIds
    {
        TEST_EV_INIT = 1,
        TEST_EV_LATE_INIT = 2,
        TEST_EV_START = 3,
        TEST_EV_LATE_START = 4,
        TEST_EV_RUNNING = 5,
    };

    class TestCompA : public LLBC_Component
    {
    public:
        int OnInit(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_INIT, this, &TestCompA::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompA init!");
            return LLBC_OK;
        }
        
        int OnLateInit(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_LATE_INIT, this, &TestCompA::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompA late init!");
            return LLBC_OK;
        }
        
        void OnEarlyDestroy(bool &finished) override
        {
            LLBC_PrintLn("TestCompA early destroy!");
        }
        
        void OnDestroy(bool &finished) override
        {
            LLBC_PrintLn("TestCompA destroy!");
        }
        
        int OnStart(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_START, this, &TestCompA::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompA start!");
            finished = false;
            return LLBC_FAILED;
        }
        
        int OnLateStart(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_LATE_START, this, &TestCompA::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompA late start!");
            return LLBC_OK;
        }
        
        void OnEarlyStop(bool &finished) override
        {
            LLBC_PrintLn("TestCompA early stop!");
        }
        
        void OnStop(bool &finished) override
        {
            LLBC_PrintLn("TestCompA stop!");
        }
        
        void Do()
        {
            eventMgr->AddListener(EventIds::TEST_EV_RUNNING, this, &TestCompA::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompA do something!");
        }
        
    private:
        void _OnEv_TestHandle(LLBC_Event &) {}
    };

    class TestCompB : public LLBC_Component
    {
    public:
        int OnInit(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_INIT, this, &TestCompB::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompB init!");
            return LLBC_OK;
        }
        
        int OnLateInit(bool &finished) override
        {
            test_stub_late_init = eventMgr->AddListener(EventIds::TEST_EV_LATE_INIT, this, &TestCompB::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompB late init!");
            return LLBC_OK;
        }
        
        void OnEarlyDestroy(bool &finished) override
        {
            LLBC_PrintLn("TestCompB early destroy!");
        }
        
        void OnDestroy(bool &finished) override
        {
            LLBC_PrintLn("TestCompB destroy!");
        }
        
        int OnStart(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_START, this, &TestCompB::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompB start!");
            return LLBC_OK;
        }
        
        int OnLateStart(bool &finished) override
        {
            eventMgr->AddListener(EventIds::TEST_EV_LATE_START, this, &TestCompB::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompB late start!");

            auto svc = GetService();
            svc->Post([&](LLBC_Service *svc) {
                Do();
                svc->GetComponent<TestCompA>()->Do();

                eventMgr->RemoveListener(test_stub_do);
                eventMgr->RemoveListener(test_stub_late_init);
            });

            return LLBC_OK;
        }
        
        void OnEarlyStop(bool &finished) override
        {
            LLBC_PrintLn("TestCompB early stop!");
        }
        
        void OnStop(bool &finished) override
        {
            LLBC_PrintLn("TestCompB stop!");
        }
        
        void Do()
        {
            test_stub_do = eventMgr->AddListener(EventIds::TEST_EV_RUNNING, this, &TestCompB::_OnEv_TestHandle);
            LLBC_PrintLn("TestCompB do something!");
        }
        
    private:
        void _OnEv_TestHandle(LLBC_Event &) {}

    private:
        LLBC_ListenerStub test_stub_late_init;
        LLBC_ListenerStub test_stub_do;
    };
}

int TestCase_Comm_SvcEvStubAutoRemove::Run(int argc, char *argv[])
{
    // Create service
    LLBC_Service *svc = LLBC_Service::Create("SvcTest");

    eventMgr = new LLBC_EventMgr;
    svc->AddCollaborativeEventMgr(eventMgr);

    svc->AddComponent<TestCompA>();
    svc->AddComponent<TestCompB>();

    svc->Start();

    LLBC_PrintLn("Service running!");

    svc->Stop();

    delete svc;
    delete eventMgr;

    std::cout <<"press any key to continue ..." <<std::endl;
    getchar();

    return 0;
}
