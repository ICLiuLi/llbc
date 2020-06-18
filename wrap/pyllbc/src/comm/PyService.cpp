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

#include "pyllbc/common/Export.h"

#include "pyllbc/comm/PyObjCoder.h"
#include "pyllbc/comm/PyPacketHandler.h"
#include "pyllbc/comm/PyFacade.h"
#include "pyllbc/comm/ErrorHooker.h"
#include "pyllbc/comm/PyService.h"

namespace
{
    typedef pyllbc_Service This;

    void ResetMainloopFlag(void *flagPtr)
    {
        bool *flag = reinterpret_cast<bool *>(flagPtr);
        *flag = false;
    }
}

PyObject *pyllbc_Service::_streamCls = NULL;
pyllbc_ErrorHooker *pyllbc_Service::_errHooker = LLBC_New(pyllbc_ErrorHooker);

pyllbc_Service::pyllbc_Service(LLBC_IService::Type type, const LLBC_String &name, PyObject *pySvc)
: _llbcSvc(NULL)
, _llbcSvcType(type)
, _llbcSvcName(name.c_str(), name.length())

, _pySvc(pySvc)

, _inMainloop()

, _cppFacade(NULL)
, _facades()

, _handlers()
, _preHandlers()
#if LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE
, _unifyPreHandler(NULL)
#endif // LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE

, _codec((This::Codec)(PYLLBC_CFG_DFT_SVC_CODEC))
, _codecs()

, _suppressedCoderNotFoundWarning(false)

, _beforeFrameCallables()
, _afterFrameCallables()

, _handlingBeforeFrameCallables(false)
, _handledBeforeFrameCallables(false)
, _handlingAfterFrameCallables(false)

, _started(false)
, _stoping(false)
{
    // Create llbc library Service object and set some service attributes.
    CreateLLBCService(type, name);

    // Create cobj python attribute key.
    _keyCObj = Py_BuildValue("s", "cobj");

    if (!This::_streamCls)
        This::_streamCls = pyllbc_s_TopModule->GetObject("Stream"); // Borrowed
}

pyllbc_Service::~pyllbc_Service()
{
    Stop();
    AfterStop();

    Py_DECREF(_keyCObj);
}

int pyllbc_Service::GetId() const
{
    return _llbcSvc->GetId();
}

LLBC_IService::Type pyllbc_Service::GetType() const
{
    return _llbcSvcType;
}

PyObject *pyllbc_Service::GetPyService() const
{
    return _pySvc;
}

int pyllbc_Service::GetFPS() const
{
    return _llbcSvc->GetFPS();
}

int pyllbc_Service::SetFPS(int fps)
{
    if (_llbcSvc->SetFPS(fps) != LLBC_OK)
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__);
        return LLBC_FAILED;
    }

    return LLBC_OK;
}

int pyllbc_Service::GetFrameInterval() const
{
    return _llbcSvc->GetFrameInterval();
}

int pyllbc_Service::SuppressCoderNotFoundWarning()
{
    if (_llbcSvc->SuppressCoderNotFoundWarning() != LLBC_OK)
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__, "when suppress service warning");
        return LLBC_FAILED;
    }

    _suppressedCoderNotFoundWarning = true;
    return LLBC_OK;
}

This::Codec pyllbc_Service::GetCodec() const
{
    return _codec;
}

int pyllbc_Service::SetCodec(Codec codec)
{
    if (codec != This::JsonCodec &&
        codec != This::BinaryCodec)
    {
        pyllbc_SetError("invalid codec type", LLBC_ERROR_INVALID);
        return LLBC_FAILED;
    }
    else if (_started)
    {
        pyllbc_SetError("service already start, could not change codec strategy");
        return LLBC_FAILED;
    }

    if (codec != _codec)
        _codec = codec;

    return LLBC_OK;
}

int pyllbc_Service::Start(int pollerCount)
{
    if (_started)
    {
        pyllbc_SetError("service already started", LLBC_ERROR_REENTRY);
        return LLBC_FAILED;
    }

    // Force install error hooker
    const bool errHookerInstalled = _errHooker->IsInstalled();
    _errHooker->Install();

    _started = true;
    if (_llbcSvc->Start(pollerCount) != LLBC_OK)
    {
        _started = false;
        pyllbc_TransferLLBCError(__FILE__, __LINE__);

        // Transfer hooked error to python, and Restore error hooker install status.
        _errHooker->TransferHookedErrorToPython();
        if (!errHookerInstalled)
            _errHooker->Uninstall();

        return LLBC_FAILED;
    }

    _errHooker->Cleanup();
    if (!errHookerInstalled)
        _errHooker->Uninstall();

    return LLBC_OK;
}

bool pyllbc_Service::IsStarted() const
{
    return _started;
}

void pyllbc_Service::Stop()
{
    if (!IsStarted())
        return;

    if (_inMainloop)
    {
        _stoping = true;
    }
    else
    {
        AfterStop();
        _started = false;
    }
}

int pyllbc_Service::RegisterFacade(PyObject *facade)
{
    if (_started)
    {
        pyllbc_SetError("service already started", LLBC_ERROR_INITED);
        return LLBC_FAILED;
    }

    if (std::find(_facades.begin(), _facades.end(), facade) != _facades.end())
    {
        PyObject *pyFacadeStr = PyObject_Str(facade);
        LLBC_String facadeStr = PyString_AsString(pyFacadeStr);
        Py_DECREF(pyFacadeStr);

        LLBC_String errStr;
        pyllbc_SetError(errStr.format("repeat to register facade: %s", facadeStr.c_str()), LLBC_ERROR_REPEAT);

        return LLBC_FAILED;
    }

    Py_INCREF(facade);
    _facades.push_back(facade);

    return LLBC_OK;
}

int pyllbc_Service::RegisterCodec(int opcode, PyObject *codec)
{
    if (_codec != This::BinaryCodec)
    {
        pyllbc_SetError("current codec strategy not BINARY, don't need register codec");
        return LLBC_FAILED;
    }
    else if (_llbcSvcType == LLBC_IService::Raw)
    {
        pyllbc_SetError("RAW type service don't need register codec");
        return LLBC_FAILED;
    }
    else if (!PyCallable_Check(codec))
    {
        pyllbc_SetError("codec not callable");
        return LLBC_FAILED;
    }

    if (!_codecs.insert(std::make_pair(opcode, codec)).second)
    {
        LLBC_String err;
        pyllbc_SetError(err.append_format(
            "repeat to register specify opcode's codec, opcode: %d", opcode));

        return LLBC_FAILED;
    }

    Py_INCREF(codec);

    return LLBC_OK;
}

int pyllbc_Service::Listen(const char *ip, uint16 port)
{
    int sessionId;
    if ((sessionId = _llbcSvc->Listen(ip, port)) == 0)
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__);
        return 0;
    }

    return sessionId;
}

int pyllbc_Service::Connect(const char *ip, uint16 port)
{
    int sessionId;
    if ((sessionId = _llbcSvc->Connect(ip, port)) == 0)
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__);
        return 0;
    }

    return sessionId;
}

int pyllbc_Service::AsyncConn(const char *ip, uint16 port)
{
    int sid = _llbcSvc->AsyncConn(ip, port);
    if (sid == 0)
        pyllbc_TransferLLBCError(__FILE__, __LINE__, LLBC_String().format("async connect to %s:%d failed", ip, port).c_str());

    return sid;
}

int pyllbc_Service::RemoveSession(int sessionId, const char *reason)
{
    if (_llbcSvc->RemoveSession(sessionId, reason) != LLBC_OK)
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__, 
            LLBC_String().format("when remove session: %d", sessionId));
        return LLBC_FAILED;
    }

    return LLBC_OK;
}

int pyllbc_Service::Send(int sessionId, int opcode, PyObject *data, int status, sint64 extData1, sint64 extData2, sint64 extData3)
{
    // Started check.
    if (UNLIKELY(!IsStarted()))
    {
        pyllbc_SetError("service not start");
        return LLBC_FAILED;
    }

    // Serialize python layer 'data' object to stream.
    LLBC_Stream stream;
    const int ret = SerializePyObj2Stream(data, stream);
    if (UNLIKELY(ret != LLBC_OK))
        return LLBC_FAILED;

    // Build packet & send.
    LLBC_Packet *packet = LLBC_New(LLBC_Packet);
    packet->Write(stream.GetBuf(), stream.GetPos());

    packet->SetSessionId(sessionId);
    if (_llbcSvcType != LLBC_IService::Raw)
    {
        packet->SetOpcode(opcode);
        packet->SetStatus(status);

        packet->SetExtData1(extData1);
        packet->SetExtData2(extData2);
        packet->SetExtData3(extData3);
    }

    if (UNLIKELY(_llbcSvc->Send(packet) == LLBC_FAILED))
    {
        pyllbc_TransferLLBCError(__FILE__, __LINE__, "When sending packet");
        return LLBC_FAILED;
    }

    return LLBC_OK;
}

int pyllbc_Service::Multicast(const LLBC_SessionIdList &sessionIds, int opcode, PyObject *data, int status)
{
    // Started check.
    if (UNLIKELY(!IsStarted()))
    {
        pyllbc_SetError("service not start");
        return LLBC_FAILED;
    }

    // Session Ids array is empty, done.
    if (sessionIds.empty())
        return LLBC_OK;

    // Serialize python layer 'data' object to stream.
    LLBC_Stream stream;
    if (SerializePyObj2Stream(data, stream) != LLBC_OK)
        return LLBC_FAILED;

    // Send it.
    const void *bytes = stream.GetBuf();
    const size_t len = stream.GetPos();
    return _llbcSvc->Multicast(sessionIds, opcode, bytes, len, status);
}

int pyllbc_Service::Broadcast(int opcode, PyObject *data, int status)
{
    // Started check.
    if (UNLIKELY(!IsStarted()))
    {
        pyllbc_SetError("service not start");
        return LLBC_FAILED;
    }

    // Serialize python layer 'data' object to stream.
    LLBC_Stream stream;
    if (SerializePyObj2Stream(data, stream) != LLBC_OK)
        return LLBC_FAILED;

    // Send it.
    const void *bytes = stream.GetBuf();
    const size_t len = stream.GetPos();
    return _llbcSvc->Broadcast(opcode, bytes, len, status);
}

int pyllbc_Service::Subscribe(int opcode, PyObject *handler, int flags)
{
    if (_llbcSvcType == LLBC_IService::Raw && opcode != 0)
    {
        pyllbc_SetError(LLBC_String().format(
            "RAW type service could not subscribe opcode[%d] != 0's packet", opcode), LLBC_ERROR_INVALID);
        return LLBC_FAILED;
    }

    _PacketHandlers::const_iterator it = _handlers.find(opcode);
    if (it != _handlers.end())
    {
        const LLBC_String handlerDesc = pyllbc_ObjUtil::GetObjStr(handler);

        LLBC_String err;
        err.append_format("repeat subscribe, subscribe opcode:%d, handler:%s, ", opcode, handlerDesc.c_str());
        err.append_format("the opcode already subscribed, handler:[%s]", it->second->ToString().c_str());

        pyllbc_SetError(err, LLBC_ERROR_REPEAT);

        return LLBC_FAILED;
    }

    pyllbc_PacketHandler *wrapHandler = LLBC_New1(pyllbc_PacketHandler, opcode);
    if (wrapHandler->SetHandler(handler) != LLBC_OK)
    {
        LLBC_Delete(wrapHandler);
        return LLBC_FAILED;
    }

    if (_llbcSvc->Subscribe(opcode, _cppFacade, &pyllbc_Facade::OnDataReceived) != LLBC_OK)
    {
        LLBC_Delete(wrapHandler);
        pyllbc_TransferLLBCError(__FILE__, __LINE__, "call native Service::Subscribe() failed");

        return LLBC_FAILED;
    }

    _handlers.insert(std::make_pair(opcode, wrapHandler));

    return LLBC_OK;
}

int pyllbc_Service::PreSubscribe(int opcode, PyObject *preHandler, int flags)
{
    if (_llbcSvcType == LLBC_IService::Raw && opcode != 0)
    {
        pyllbc_SetError("RAW type service could not pre-subscribe opcode != 0's packet", LLBC_ERROR_INVALID);
        return LLBC_FAILED;
    }

    _PacketHandlers::const_iterator it = _preHandlers.find(opcode);
    if (it != _preHandlers.end())
    {
        const LLBC_String handlerDesc = pyllbc_ObjUtil::GetObjStr(preHandler);

        LLBC_String err;
        err.append_format("repeat pre-subscribe, pre-subscribe opcode:%d, handler:%s, ", opcode, handlerDesc.c_str());
        err.append_format("the opcode already pre-subscribed, handler:[%s]", it->second->ToString().c_str());

        pyllbc_SetError(err, LLBC_ERROR_REPEAT);

        return LLBC_FAILED;
    }



    pyllbc_PacketHandler *wrapHandler = LLBC_New1(pyllbc_PacketHandler, opcode);
    if (wrapHandler->SetHandler(preHandler) != LLBC_OK)
    {
        LLBC_Delete(wrapHandler);
        return LLBC_FAILED;
    }

    if (!_preHandlers.insert(std::make_pair(opcode, wrapHandler)).second)
    {
        LLBC_Delete(wrapHandler);

        LLBC_String err;
        pyllbc_SetError(err.format(
            "repeat to pre-subscribe opcode: %d, the opcode already pre-subscribed", opcode), LLBC_ERROR_REPEAT);

        return LLBC_FAILED;
    }

    if (_llbcSvc->PreSubscribe(opcode, _cppFacade, &pyllbc_Facade::OnDataPreReceived) != LLBC_OK)
    {
        _preHandlers.erase(opcode);
        LLBC_Delete(wrapHandler);
        pyllbc_TransferLLBCError(__FILE__, __LINE__, "call native Service::PreSubscribe() failed");

        return LLBC_FAILED;
    }

    return LLBC_OK;
}

#if LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE
int pyllbc_Service::UnifyPreSubscribe(PyObject *preHandler, int flags)
{
    pyllbc_PacketHandler *wrapHandler = LLBC_New1(pyllbc_PacketHandler, 0);
    if (wrapHandler->SetHandler(preHandler) != LLBC_OK)
    {
        LLBC_Delete(wrapHandler);
        return LLBC_FAILED;
    }

    if (_unifyPreHandler)
    {
        LLBC_Delete(wrapHandler);
        pyllbc_SetError("repeat to unify pre-subscribe packet");

        return LLBC_FAILED;
    }

    _unifyPreHandler = wrapHandler;
    if (_llbcSvc->UnifyPreSubscribe(_cppFacade, &pyllbc_Facade::OnDataUnifyPreReceived) != LLBC_OK)
    {
        LLBC_XDelete(_unifyPreHandler);
        pyllbc_TransferLLBCError(__FILE__, __LINE__, "call native Service::UnifyPreSubscribe() failed");

        return LLBC_FAILED;
    }

    return LLBC_OK;
}
#endif // LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE

int pyllbc_Service::Post(PyObject *callable)
{
    if (!PyCallable_Check(callable))
    {
        const LLBC_String objDesc = pyllbc_ObjUtil::GetObjStr(callable);
        pyllbc_SetError(LLBC_String().format("frame callable object not callable: %s", objDesc.c_str()));

        return LLBC_FAILED;
    }

    if (_handlingBeforeFrameCallables &&
        _handlingAfterFrameCallables)
    {
        pyllbc_SetError("could not push callable object to service, internal error!");
        return LLBC_FAILED;
    }

    if (_beforeFrameCallables.find(callable) != _beforeFrameCallables.end() ||
        _afterFrameCallables.find(callable) != _afterFrameCallables.end())
    {
        const LLBC_String objDesc = pyllbc_ObjUtil::GetObjStr(callable);
        pyllbc_SetError(LLBC_String().format(
            "repeat to add callable to service, callable: %s", objDesc.c_str()));

        return LLBC_FAILED;
    }

    Py_INCREF(callable);
    if (_handlingBeforeFrameCallables)
    {
        _afterFrameCallables.insert(callable);
    }
    else
    {
        if (!_handledBeforeFrameCallables)
        {
            _beforeFrameCallables.insert(callable);
        }
        else
        {
            if (!_handlingAfterFrameCallables)
                _afterFrameCallables.insert(callable);
            else
                _beforeFrameCallables.insert(callable);
        }
    }

    return LLBC_OK;
}

bool pyllbc_Service::MainLoop()
{
    if (UNLIKELY(!_started))
        return false;

    _inMainloop = true;
    LLBC_InvokeGuard guard(&ResetMainloopFlag, &_inMainloop);

    if (!_stoping)
    {
        HandleFrameCallables(_beforeFrameCallables, _handlingBeforeFrameCallables);
        _handledBeforeFrameCallables = true;
    }

    if (!_stoping)
        _llbcSvc->OnSvc(false);

    if (!_stoping)
    {
        HandleFrameCallables(_afterFrameCallables, _handlingAfterFrameCallables);
    }

    _handledBeforeFrameCallables = false;

    if (_stoping)
    {
        AfterStop();

        _stoping = false;
        _started = false;

        return false;
    }

    // Transfer hooked error to python.
    This::_errHooker->TransferHookedErrorToPython();

    return true;
}

pyllbc_ErrorHooker *pyllbc_Service::GetErrHooker()
{
    return This::_errHooker;
}

void pyllbc_Service::CreateLLBCService(LLBC_IService::Type svcType, const LLBC_String &svcName)
{
    ASSERT(!_llbcSvc && "llbc service pointer not NULL");

    _llbcSvc = LLBC_IService::Create(svcType, svcName, NULL, false);
    _llbcSvc->SetDriveMode(LLBC_IService::ExternalDrive);
    _llbcSvc->DisableTimerScheduler();
    _llbcSvc->SuppressCoderNotFoundWarning();

    _cppFacade = LLBC_New1(pyllbc_Facade, this);
    _llbcSvc->RegisterFacade(_cppFacade);
}

void pyllbc_Service::AfterStop()
{
    _cppFacade = NULL;

    // Recreate service.
    LLBC_XDelete(_llbcSvc);
    CreateLLBCService(_llbcSvcType, _llbcSvcName);

    // Cleanup all python layer facades.
    for (_Facades::reverse_iterator it = _facades.rbegin();
         it != _facades.rend();
         it++)
        Py_DECREF(*it);
    _facades.clear();

    // Cleanup all python layer codecs.
    for (_Codecs::iterator it = _codecs.begin();
         it != _codecs.end();
         it++)
        Py_DECREF(it->second);
    _codecs.clear();

    // Cleanup all python layer handlers/prehandlers/unify_prehandlers(if enabled).
    LLBC_STLHelper::DeleteContainer(_handlers);
    LLBC_STLHelper::DeleteContainer(_preHandlers);
#if LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE
    LLBC_XDelete(_unifyPreHandler);
#endif // LLBC_CFG_COMM_ENABLE_UNIFY_PRESUBSCRIBE

    // Destroy all frame callables.
    _handledBeforeFrameCallables = false;
    DestroyFrameCallables(_beforeFrameCallables, _handlingBeforeFrameCallables);
    DestroyFrameCallables(_afterFrameCallables, _handlingAfterFrameCallables);

}

void pyllbc_Service::HandleFrameCallables(pyllbc_Service::_FrameCallables &callables, bool &usingFlag)
{
    usingFlag = true;
    for (_FrameCallables::iterator it = callables.begin();
         it != callables.end();
         it++)
    {
        PyObject *callable = *it;
        PyObject *ret = PyObject_CallFunctionObjArgs(callable, _pySvc, NULL);
        if (ret)
        {
            Py_DECREF(ret);
        }
        else
        {
            const LLBC_String objDesc = pyllbc_ObjUtil::GetObjStr(callable);
            pyllbc_TransferPyError(LLBC_String().format("When call frame-callable: %s", objDesc.c_str()));
            break;
        }
    }

    DestroyFrameCallables(callables, usingFlag);
}

void pyllbc_Service::DestroyFrameCallables(_FrameCallables &callables, bool &usingFlag)
{
    for (_FrameCallables::iterator it = callables.begin();
         it != callables.end();
         it++)
        Py_DECREF(*it);

    callables.clear();
    usingFlag = false;
}

int pyllbc_Service::SerializePyObj2Stream(PyObject *pyObj, LLBC_Stream &stream)
{
    if (_codec == This::JsonCodec)
    {
        std::string out;
        if (UNLIKELY(pyllbc_ObjCoder::Encode(pyObj, out) != LLBC_OK))
            return LLBC_FAILED;

        stream.WriteBuffer(out.data(), out.size());

        return LLBC_OK;
    }
    else
    {
        // Create python layer Stream instance.
        PyObject *arg = PyTuple_New(2);
        PyTuple_SetItem(arg, 0, PyInt_FromLong(0)); // stream init size = 0.
        
        Py_INCREF(pyObj);
        PyTuple_SetItem(arg, 1, pyObj); // initWrite = pyObj(steal reference).

        PyObject *pyStreamObj = PyObject_CallObject(This::_streamCls, arg);
        if (UNLIKELY(!pyStreamObj))
        {
            Py_DECREF(arg);
            pyllbc_TransferPyError();

            return LLBC_FAILED;
        }

        // Get cobj property.
        PyObject *cobj = PyObject_GetAttr(pyStreamObj, _keyCObj);
        if (UNLIKELY(!cobj))
        {
            Py_DECREF(pyStreamObj);
            Py_DECREF(arg);

            pyllbc_SetError("could not get llbc.Stream property 'cobj'");

            return LLBC_FAILED;
        }

        // Convert to pyllbc_Stream *.
        pyllbc_Stream *cstream = NULL;
        PyArg_Parse(cobj, "l", &cstream);

        // Let stream attach to inlStream.
        LLBC_Stream &inlStream = cstream->GetLLBCStream();

        stream.Attach(inlStream);
        (void)inlStream.Detach();
        stream.SetAttachAttr(false);

        Py_DECREF(cobj);
        Py_DECREF(pyStreamObj);
        Py_DECREF(arg);

        return LLBC_OK;
    }
}
