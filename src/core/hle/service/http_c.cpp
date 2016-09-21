// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/http_c.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HTTP_C

namespace HTTP_C {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, nullptr, "Initialize"},
    {0x00020082, nullptr, "CreateContext"},
    {0x00030040, nullptr, "CloseContext"},
    {0x00040040, nullptr, "CancelConnection"},
    {0x00050040, nullptr, "GetRequestState"},
    {0x00060040, nullptr, "GetDownloadSizeState"},
    {0x00070040, nullptr, "GetRequestError"},
    {0x00080042, nullptr, "InitializeConnectionSession"},
    {0x00090040, nullptr, "BeginRequest"},
    {0x000A0040, nullptr, "BeginRequestAsync"},
    {0x000B0082, nullptr, "ReceiveData"},
    {0x000C0102, nullptr, "ReceiveDataTimeout"},
    {0x000D0146, nullptr, "SetProxy"},
    {0x000E0040, nullptr, "SetProxyDefault"},
    {0x000F00C4, nullptr, "SetBasicAuthorization"},
    {0x00100080, nullptr, "SetSocketBufferSize"},
    {0x001100C4, nullptr, "AddRequestHeader"},
    {0x001200C4, nullptr, "AddPostDataAscii"},
    {0x001300C4, nullptr, "AddPostDataBinary"},
    {0x00140082, nullptr, "AddPostDataRaw"},
    {0x00150080, nullptr, "SetPostDataType"},
    {0x001600C4, nullptr, "SendPostDataAscii"},
    {0x00170144, nullptr, "SendPostDataAsciiTimeout"},
    {0x001800C4, nullptr, "SendPostDataBinary"},
    {0x00190144, nullptr, "SendPostDataBinaryTimeout"},
    {0x001A0082, nullptr, "SendPostDataRaw"},
    {0x001B0102, nullptr, "SendPOSTDataRawTimeout"},
    {0x001C0080, nullptr, "SetPostDataEncoding"},
    {0x001D0040, nullptr, "NotifyFinishSendPostData"},
    {0x001E00C4, nullptr, "GetResponseHeader"},
    {0x001F0144, nullptr, "GetResponseHeaderTimeout"},
    {0x00200082, nullptr, "GetResponseData"},
    {0x00210102, nullptr, "GetResponseDataTimeout"},
    {0x00220040, nullptr, "GetResponseStatusCode"},
    {0x002300C0, nullptr, "GetResponseStatusCodeTimeout"},
    {0x00240082, nullptr, "AddTrustedRootCA"},
    {0x00250080, nullptr, "AddDefaultCert"},
    {0x00260080, nullptr, "SelectRootCertChain"},
    {0x002700C4, nullptr, "SetClientCert"},
    {0x002B0080, nullptr, "SetSSLOpt"},
    {0x002C0080, nullptr, "SetSSLClearOpt"},
    {0x002D0000, nullptr, "CreateRootCertChain"},
    {0x002E0040, nullptr, "DestroyRootCertChain"},
    {0x002F0082, nullptr, "RootCertChainAddCert"},
    {0x00300080, nullptr, "RootCertChainAddDefaultCert"},
    {0x00350186, nullptr, "SetDefaultProxy"},
    {0x00360000, nullptr, "ClearDNSCache"},
    {0x00370080, nullptr, "SetKeepAlive"},
    {0x003800C0, nullptr, "SetPostDataTypeSize"},
    {0x00390000, nullptr, "Finalize"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
