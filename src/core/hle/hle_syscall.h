// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

//template <class T>
//class KernelObject {
//public:
//	virtual ~KernelObject() {}
//
//	T GetNative() const {
//        return m_native;
//    }
//
//    void SetNative(const T& native) {
//        m_native = native;
//    }
//
//	virtual const char *GetTypeName() {return "[BAD KERNEL OBJECT TYPE]";}
//	virtual const char *GetName() {return "[UNKNOWN KERNEL OBJECT]";}
//
//private:
//    T m_native;
//};

//class Handle : public KernelObject<u32> {
//    const char* GetTypeName() { 
//        return "Handle";
//    }
//};


typedef u32 Handle;
typedef s32 Result;


Result ConnectToPort(Handle* out, const char* port_name);
