// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_U

namespace CFG_U {

static const std::array<const char*, 187> country_codes = { 
    nullptr, "JP", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    // 0-7
    "AI", "AG", "AR", "AW", "BS", "BB", "BZ", "BO",                         // 8-15
    "BR", "VG", "CA", "KY", "CL", "CO", "CR", "DM",                         // 16-23
    "DO", "EC", "SV", "GF", "GD", "GP", "GT", "GY",                         // 24-31
    "HT", "HN", "JM", "MQ", "MX", "MS", "AN", "NI",                         // 32-39
    "PA", "PY", "PE", "KN", "LC", "VC", "SR", "TT",                         // 40-47
    "TC", "US", "UY", "VI", "VE", nullptr, nullptr, nullptr,                // 48-55
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 56-63
    "AL", "AU", "AT", "BE", "BA", "BW", "BG", "HR",                         // 64-71
    "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR",                         // 72-79
    "HU", "IS", "IE", "IT", "LV", "LS", "LI", "LT",                         // 80-87
    "LU", "MK", "MT", "ME", "MZ", "NA", "NL", "NZ",                         // 88-95
    "NO", "PL", "PT", "RO", "RU", "RS", "SK", "SI",                         // 96-103
    "ZA", "ES", "SZ", "SE", "CH", "TR", "GB", "ZM",                         // 104-111
    "ZW", "AZ", "MR", "ML", "NE", "TD", "SD", "ER",                         // 112-119
    "DJ", "SO", "AD", "GI", "GG", "IM", "JE", "MC",                         // 120-127
    "TW", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    // 128-135
    "KR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    // 136-143
    "HK", "MO", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,       // 144-151
    "ID", "SG", "TH", "PH", "MY", nullptr, nullptr, nullptr,                // 152-159
    "CN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    // 160-167
    "AE", "IN", "EG", "OM", "QA", "KW", "SA", "SY",                         // 168-175
    "BH", "JO", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,       // 176-183
    "SM", "VA", "BM",                                                       // 184-186
};

/**
 * CFG_User::GetCountryCodeString service function
 *  Inputs:
 *      1 : Country Code ID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country's 2-char string
 */
static void GetCountryCodeString(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 country_code_id = cmd_buffer[1];

    if (country_code_id >= country_codes.size()) {
        ERROR_LOG(KERNEL, "requested country code id=%d is invalid", country_code_id);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        return;
    }
    
    const char* code = country_codes[country_code_id];
    if (code != nullptr) {
        cmd_buffer[1] = 0;
        cmd_buffer[2] = code[0] | (code[1] << 8);
    } else {
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        DEBUG_LOG(KERNEL, "requested country code id=%d is not set", country_code_id);
    }
}

/**
 * CFG_User::GetCountryCodeID service function
 *  Inputs:
 *      1 : Country Code 2-char string
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country Code ID
 */
static void GetCountryCodeID(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u16 country_code = cmd_buffer[1];
    u16 country_code_id = -1;

    for (u32 i = 0; i < country_codes.size(); ++i) {
        const char* code_string = country_codes[i];

        if (code_string != nullptr) {
            u16 code = code_string[0] | (code_string[1] << 8);
            if (code == country_code) {
                country_code_id = i;
                break;
            }
        }
    }

    cmd_buffer[1] = 0;
    cmd_buffer[2] = country_code_id;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, nullptr,               "GetConfigInfoBlk2"},
    {0x00020000, nullptr,               "SecureInfoGetRegion"},
    {0x00030000, nullptr,               "GenHashConsoleUnique"},
    {0x00040000, nullptr,               "GetRegionCanadaUSA"},
    {0x00050000, nullptr,               "GetSystemModel"},
    {0x00060000, nullptr,               "GetModelNintendo2DS"},
    {0x00070040, nullptr,               "unknown"},
    {0x00080080, nullptr,               "unknown"},
    {0x00090040, GetCountryCodeString,  "GetCountryCodeString"},
    {0x000A0040, GetCountryCodeID,      "GetCountryCodeID"},
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
