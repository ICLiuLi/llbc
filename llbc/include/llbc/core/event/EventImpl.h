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

#ifdef __LLBC_CORE_EVENT_EVENT_H__

__LLBC_NS_BEGIN

inline int LLBC_Event::GetId() const
{
    return _id;
}

template <typename ParamType>
inline LLBC_Event &LLBC_Event::AddParam(const ParamType &param)
{
    const LLBC_Variant varParam(param);
    return AddParam(varParam);
}

template <typename ParamType>
inline LLBC_Event &LLBC_Event::AddParam(const LLBC_String &key, const ParamType &param)
{
    const LLBC_Variant varParam(param);
    return AddParam(key, varParam);
}

inline size_t LLBC_Event::GetSequentialParamsCount() const
{
    return _seqParams != NULL ? _seqParams->size() : 0;
}

inline size_t LLBC_Event::GetNamingParamsCount() const
{
    return _namingParams != NULL ? _namingParams->size() : 0;
}

inline bool LLBC_Event::IsDontDelAfterFire() const
{
    return _dontDelAfterFire;
}

__LLBC_NS_END

#endif // __LLBC_CORE_EVENT_EVENT_H__
