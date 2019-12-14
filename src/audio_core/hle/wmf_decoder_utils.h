// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

// AAC decoder related APIs are only available with WIN7+
#define WINVER _WIN32_WINNT_WIN7

#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include <comdef.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>

#include "adts.h"

namespace MFDecoder {

template <typename T>
struct Symbol {
    Symbol() = default;
    Symbol(HMODULE dll, const char* name) {
        if (dll) {
            ptr_symbol = reinterpret_cast<T*>(GetProcAddress(dll, name));
        }
    }

    operator T*() const {
        return ptr_symbol;
    }

    explicit operator bool() const {
        return ptr_symbol != nullptr;
    }

    T* ptr_symbol = nullptr;
};

// Runtime load the MF symbols to prevent mf.dll not found errors on citra load
extern Symbol<HRESULT(ULONG, DWORD)> MFStartup;
extern Symbol<HRESULT(void)> MFShutdown;
extern Symbol<HRESULT(IUnknown*)> MFShutdownObject;
extern Symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)> MFCreateAlignedMemoryBuffer;
extern Symbol<HRESULT(IMFSample**)> MFCreateSample;
extern Symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*,
                      IMFActivate***, UINT32*)>
    MFTEnumEx;
extern Symbol<HRESULT(IMFMediaType**)> MFCreateMediaType;

enum class MFOutputState { FatalError, OK, NeedMoreInput, NeedReconfig, HaveMoreData };
enum class MFInputState { FatalError, OK, NotAccepted };

// utility functions / templates
template <class T>
struct MFRelease {
    void operator()(T* pointer) const {
        pointer->Release();
    };
};

template <>
struct MFRelease<IMFTransform> {
    void operator()(IMFTransform* pointer) const {
        MFShutdownObject(pointer);
        pointer->Release();
    };
};

// wrapper facilities for dealing with pointers
template <typename T>
using unique_mfptr = std::unique_ptr<T, MFRelease<T>>;

template <typename SmartPtr, typename RawPtr>
class AmpImpl {
public:
    AmpImpl(SmartPtr& smart_ptr) : smart_ptr(smart_ptr) {}
    ~AmpImpl() {
        smart_ptr.reset(raw_ptr);
    }

    operator RawPtr*() {
        return &raw_ptr;
    }

private:
    SmartPtr& smart_ptr;
    RawPtr raw_ptr = nullptr;
};

template <typename SmartPtr>
auto Amp(SmartPtr& smart_ptr) {
    return AmpImpl<SmartPtr, decltype(smart_ptr.get())>(smart_ptr);
}

// convient function for formatting error messages
void ReportError(std::string msg, HRESULT hr);

// data type for transferring ADTS metadata between functions
struct ADTSMeta {
    ADTSData ADTSHeader;
    u8 AACTag[14];
};

// exported functions

/// Loads the symbols from mf.dll at runtime. Returns false if the symbols can't be loaded
bool InitMFDLL();
unique_mfptr<IMFTransform> MFDecoderInit(GUID audio_format = MFAudioFormat_AAC);
unique_mfptr<IMFSample> CreateSample(const void* data, DWORD len, DWORD alignment = 1,
                                     LONGLONG duration = 0);
bool SelectInputMediaType(IMFTransform* transform, int in_stream_id, const ADTSData& adts,
                          const UINT8* user_data, UINT32 user_data_len,
                          GUID audio_format = MFAudioFormat_AAC);
std::optional<ADTSMeta> DetectMediaType(char* buffer, std::size_t len);
bool SelectOutputMediaType(IMFTransform* transform, int out_stream_id,
                           GUID audio_format = MFAudioFormat_PCM);
void MFFlush(IMFTransform* transform);
MFInputState SendSample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample);
std::tuple<MFOutputState, unique_mfptr<IMFSample>> ReceiveSample(IMFTransform* transform,
                                                                 DWORD out_stream_id);
std::optional<std::vector<f32>> CopySampleToBuffer(IMFSample* sample);

} // namespace MFDecoder
