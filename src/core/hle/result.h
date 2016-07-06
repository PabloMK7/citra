// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <new>
#include <utility>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

// All the constants in this file come from http://3dbrew.org/wiki/Error_codes

/// Detailed description of the error. This listing is likely incomplete.
enum class ErrorDescription : u32 {
    Success = 0,
    WrongPermission = 46,
    OS_InvalidBufferDescriptor = 48,
    WrongAddress = 53,
    FS_ArchiveNotMounted = 101,
    FS_NotFound = 120,
    FS_AlreadyExists = 190,
    FS_InvalidOpenFlags = 230,
    FS_NotAFile = 250,
    FS_NotFormatted = 340, ///< This is used by the FS service when creating a SaveData archive
    OutofRangeOrMisalignedAddress = 513, // TODO(purpasmart): Check if this name fits its actual usage
    GPU_FirstInitialization = 519,
    FS_InvalidPath = 702,
    InvalidSection = 1000,
    TooLarge = 1001,
    NotAuthorized = 1002,
    AlreadyDone = 1003,
    InvalidSize = 1004,
    InvalidEnumValue = 1005,
    InvalidCombination = 1006,
    NoData = 1007,
    Busy = 1008,
    MisalignedAddress = 1009,
    MisalignedSize = 1010,
    OutOfMemory = 1011,
    NotImplemented = 1012,
    InvalidAddress = 1013,
    InvalidPointer = 1014,
    InvalidHandle = 1015,
    NotInitialized = 1016,
    AlreadyInitialized = 1017,
    NotFound = 1018,
    CancelRequested = 1019,
    AlreadyExists = 1020,
    OutOfRange = 1021,
    Timeout = 1022,
    InvalidResultValue = 1023,
};

/**
 * Identifies the module which caused the error. Error codes can be propagated through a call
 * chain, meaning that this doesn't always correspond to the module where the API call made is
 * contained.
 */
enum class ErrorModule : u32 {
    Common = 0,
    Kernel = 1,
    Util = 2,
    FileServer = 3,
    LoaderServer = 4,
    TCB = 5,
    OS = 6,
    DBG = 7,
    DMNT = 8,
    PDN = 9,
    GX = 10,
    I2C = 11,
    GPIO = 12,
    DD = 13,
    CODEC = 14,
    SPI = 15,
    PXI = 16,
    FS = 17,
    DI = 18,
    HID = 19,
    CAM = 20,
    PI = 21,
    PM = 22,
    PM_LOW = 23,
    FSI = 24,
    SRV = 25,
    NDM = 26,
    NWM = 27,
    SOC = 28,
    LDR = 29,
    ACC = 30,
    RomFS = 31,
    AM = 32,
    HIO = 33,
    Updater = 34,
    MIC = 35,
    FND = 36,
    MP = 37,
    MPWL = 38,
    AC = 39,
    HTTP = 40,
    DSP = 41,
    SND = 42,
    DLP = 43,
    HIO_LOW = 44,
    CSND = 45,
    SSL = 46,
    AM_LOW = 47,
    NEX = 48,
    Friends = 49,
    RDT = 50,
    Applet = 51,
    NIM = 52,
    PTM = 53,
    MIDI = 54,
    MC = 55,
    SWC = 56,
    FatFS = 57,
    NGC = 58,
    CARD = 59,
    CARDNOR = 60,
    SDMC = 61,
    BOSS = 62,
    DBM = 63,
    Config = 64,
    PS = 65,
    CEC = 66,
    IR = 67,
    UDS = 68,
    PL = 69,
    CUP = 70,
    Gyroscope = 71,
    MCU = 72,
    NS = 73,
    News = 74,
    RO = 75,
    GD = 76,
    CardSPI = 77,
    EC = 78,
    WebBrowser = 79,
    Test = 80,
    ENC = 81,
    PIA = 82,
    ACT = 83,
    VCTL = 84,
    OLV = 85,
    NEIA = 86,
    NPNS = 87,

    AVD = 90,
    L2B = 91,
    MVD = 92,
    NFC = 93,
    UART = 94,
    SPM = 95,
    QTM = 96,
    NFP = 97,

    Application = 254,
    InvalidResult = 255
};

/// A less specific error cause.
enum class ErrorSummary : u32 {
    Success = 0,
    NothingHappened = 1,
    WouldBlock = 2,
    OutOfResource = 3,      ///< There are no more kernel resources (memory, table slots) to
                            ///< execute the operation.
    NotFound = 4,           ///< A file or resource was not found.
    InvalidState = 5,
    NotSupported = 6,       ///< The operation is not supported or not implemented.
    InvalidArgument = 7,    ///< Returned when a passed argument is invalid in the current runtime
                            ///< context. (Invalid handle, out-of-bounds pointer or size, etc.)
    WrongArgument = 8,      ///< Returned when a passed argument is in an incorrect format for use
                            ///< with the function. (E.g. Invalid enum value)
    Canceled = 9,
    StatusChanged = 10,
    Internal = 11,

    InvalidResult = 63
};

/// The severity of the error.
enum class ErrorLevel : u32 {
    Success = 0,
    Info = 1,

    Status = 25,
    Temporary = 26,
    Permanent = 27,
    Usage = 28,
    Reinitialize = 29,
    Reset = 30,
    Fatal = 31
};

/// Encapsulates a CTR-OS error code, allowing it to be separated into its constituent fields.
union ResultCode {
    u32 raw;

    BitField<0, 10, ErrorDescription> description;
    BitField<10, 8, ErrorModule> module;

    BitField<21, 6, ErrorSummary> summary;
    BitField<27, 5, ErrorLevel> level;

    // The last bit of `level` is checked by apps and the kernel to determine if a result code is an error
    BitField<31, 1, u32> is_error;

    explicit ResultCode(u32 raw) : raw(raw) {}
    ResultCode(ErrorDescription description_, ErrorModule module_,
            ErrorSummary summary_, ErrorLevel level_) : raw(0) {
        description.Assign(description_);
        module.Assign(module_);
        summary.Assign(summary_);
        level.Assign(level_);
    }

    ResultCode& operator=(const ResultCode& o) { raw = o.raw; return *this; }

    bool IsSuccess() const {
        return is_error == 0;
    }

    bool IsError() const {
        return is_error == 1;
    }
};

inline bool operator==(const ResultCode& a, const ResultCode& b) {
    return a.raw == b.raw;
}

inline bool operator!=(const ResultCode& a, const ResultCode& b) {
    return a.raw != b.raw;
}

// Convenience functions for creating some common kinds of errors:

/// The default success `ResultCode`.
const ResultCode RESULT_SUCCESS(0);

/// Might be returned instead of a dummy success for unimplemented APIs.
inline ResultCode UnimplementedFunction(ErrorModule module) {
    return ResultCode(ErrorDescription::NotImplemented, module,
            ErrorSummary::NotSupported, ErrorLevel::Permanent);
}

/**
 * This is an optional value type. It holds a `ResultCode` and, if that code is a success code,
 * also holds a result of type `T`. If the code is an error code then trying to access the inner
 * value fails, thus ensuring that the ResultCode of functions is always checked properly before
 * their return value is used.  It is similar in concept to the `std::optional` type
 * (http://en.cppreference.com/w/cpp/experimental/optional) originally proposed for inclusion in
 * C++14, or the `Result` type in Rust (http://doc.rust-lang.org/std/result/index.html).
 *
 * An example of how it could be used:
 * \code
 * ResultVal<int> Frobnicate(float strength) {
 *     if (strength < 0.f || strength > 1.0f) {
 *         // Can't frobnicate too weakly or too strongly
 *         return ResultCode(ErrorDescription::OutOfRange, ErrorModule::Common,
 *             ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
 *     } else {
 *         // Frobnicated! Give caller a cookie
 *         return MakeResult<int>(42);
 *     }
 * }
 * \endcode
 *
 * \code
 * ResultVal<int> frob_result = Frobnicate(0.75f);
 * if (frob_result) {
 *     // Frobbed ok
 *     printf("My cookie is %d\n", *frob_result);
 * } else {
 *     printf("Guess I overdid it. :( Error code: %ux\n", frob_result.code().hex);
 * }
 * \endcode
 */
template <typename T>
class ResultVal {
public:
    /// Constructs an empty `ResultVal` with the given error code. The code must not be a success code.
    ResultVal(ResultCode error_code = ResultCode(-1))
        : result_code(error_code)
    {
        ASSERT(error_code.IsError());
    }

    /**
     * Similar to the non-member function `MakeResult`, with the exception that you can manually
     * specify the success code. `success_code` must not be an error code.
     */
    template <typename... Args>
    static ResultVal WithCode(ResultCode success_code, Args&&... args) {
        ResultVal<T> result;
        result.emplace(success_code, std::forward<Args>(args)...);
        return result;
    }

    ResultVal(const ResultVal& o)
        : result_code(o.result_code)
    {
        if (!o.empty()) {
            new (&object) T(o.object);
        }
    }

    ResultVal(ResultVal&& o)
        : result_code(o.result_code)
    {
        if (!o.empty()) {
            new (&object) T(std::move(o.object));
        }
    }

    ~ResultVal() {
        if (!empty()) {
            object.~T();
        }
    }

    ResultVal& operator=(const ResultVal& o) {
        if (!empty()) {
            if (!o.empty()) {
                object = o.object;
            } else {
                object.~T();
            }
        } else {
            if (!o.empty()) {
                new (&object) T(o.object);
            }
        }
        result_code = o.result_code;

        return *this;
    }

    /**
     * Replaces the current result with a new constructed result value in-place. The code must not
     * be an error code.
     */
    template <typename... Args>
    void emplace(ResultCode success_code, Args&&... args) {
        ASSERT(success_code.IsSuccess());
        if (!empty()) {
            object.~T();
        }
        new (&object) T(std::forward<Args>(args)...);
        result_code = success_code;
    }

    /// Returns true if the `ResultVal` contains an error code and no value.
    bool empty() const { return result_code.IsError(); }

    /// Returns true if the `ResultVal` contains a return value.
    bool Succeeded() const { return result_code.IsSuccess(); }
    /// Returns true if the `ResultVal` contains an error code and no value.
    bool Failed() const { return empty(); }

    ResultCode Code() const { return result_code; }

    const T& operator* () const { return object; }
          T& operator* ()       { return object; }
    const T* operator->() const { return &object; }
          T* operator->()       { return &object; }

    /// Returns the value contained in this `ResultVal`, or the supplied default if it is missing.
    template <typename U>
    T ValueOr(U&& value) const {
        return !empty() ? object : std::move(value);
    }

    /// Asserts that the result succeeded and returns a reference to it.
    T& Unwrap() {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return **this;
    }

    T&& MoveFrom() {
        return std::move(Unwrap());
    }

private:
    // A union is used to allocate the storage for the value, while allowing us to construct and
    // destruct it at will.
    union { T object; };
    ResultCode result_code;
};

/**
 * This function is a helper used to construct `ResultVal`s. It receives the arguments to construct
 * `T` with and creates a success `ResultVal` contained the constructed value.
 */
template <typename T, typename... Args>
ResultVal<T> MakeResult(Args&&... args) {
    return ResultVal<T>::WithCode(RESULT_SUCCESS, std::forward<Args>(args)...);
}

/**
 * Check for the success of `source` (which must evaluate to a ResultVal). If it succeeds, unwraps
 * the contained value and assigns it to `target`, which can be either an l-value expression or a
 * variable declaration. If it fails the return code is returned from the current function. Thus it
 * can be used to cascade errors out, achieving something akin to exception handling.
 */
#define CASCADE_RESULT(target, source) \
        auto CONCAT2(check_result_L, __LINE__) = source; \
        if (CONCAT2(check_result_L, __LINE__).Failed()) \
            return CONCAT2(check_result_L, __LINE__).Code(); \
        target = std::move(*CONCAT2(check_result_L, __LINE__))
