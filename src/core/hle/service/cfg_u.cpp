// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_U

namespace CFG_U {

// TODO(Link Mauve): use a constexpr once MSVC starts supporting it.
#define C(code) ((code)[0] | ((code)[1] << 8))

static const std::array<u16, 187> country_codes = {
    0,       C("JP"), 0,       0,       0,       0,       0,       0,       // 0-7
    C("AI"), C("AG"), C("AR"), C("AW"), C("BS"), C("BB"), C("BZ"), C("BO"), // 8-15
    C("BR"), C("VG"), C("CA"), C("KY"), C("CL"), C("CO"), C("CR"), C("DM"), // 16-23
    C("DO"), C("EC"), C("SV"), C("GF"), C("GD"), C("GP"), C("GT"), C("GY"), // 24-31
    C("HT"), C("HN"), C("JM"), C("MQ"), C("MX"), C("MS"), C("AN"), C("NI"), // 32-39
    C("PA"), C("PY"), C("PE"), C("KN"), C("LC"), C("VC"), C("SR"), C("TT"), // 40-47
    C("TC"), C("US"), C("UY"), C("VI"), C("VE"), 0,       0,       0,       // 48-55
    0,       0,       0,       0,       0,       0,       0,       0,       // 56-63
    C("AL"), C("AU"), C("AT"), C("BE"), C("BA"), C("BW"), C("BG"), C("HR"), // 64-71
    C("CY"), C("CZ"), C("DK"), C("EE"), C("FI"), C("FR"), C("DE"), C("GR"), // 72-79
    C("HU"), C("IS"), C("IE"), C("IT"), C("LV"), C("LS"), C("LI"), C("LT"), // 80-87
    C("LU"), C("MK"), C("MT"), C("ME"), C("MZ"), C("NA"), C("NL"), C("NZ"), // 88-95
    C("NO"), C("PL"), C("PT"), C("RO"), C("RU"), C("RS"), C("SK"), C("SI"), // 96-103
    C("ZA"), C("ES"), C("SZ"), C("SE"), C("CH"), C("TR"), C("GB"), C("ZM"), // 104-111
    C("ZW"), C("AZ"), C("MR"), C("ML"), C("NE"), C("TD"), C("SD"), C("ER"), // 112-119
    C("DJ"), C("SO"), C("AD"), C("GI"), C("GG"), C("IM"), C("JE"), C("MC"), // 120-127
    C("TW"), 0,       0,       0,       0,       0,       0,       0,       // 128-135
    C("KR"), 0,       0,       0,       0,       0,       0,       0,       // 136-143
    C("HK"), C("MO"), 0,       0,       0,       0,       0,       0,       // 144-151
    C("ID"), C("SG"), C("TH"), C("PH"), C("MY"), 0,       0,       0,       // 152-159
    C("CN"), 0,       0,       0,       0,       0,       0,       0,       // 160-167
    C("AE"), C("IN"), C("EG"), C("OM"), C("QA"), C("KW"), C("SA"), C("SY"), // 168-175
    C("BH"), C("JO"), 0,       0,       0,       0,       0,       0,       // 176-183
    C("SM"), C("VA"), C("BM")                                               // 184-186
};

#undef C

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

    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        ERROR_LOG(KERNEL, "requested country code id=%d is invalid", country_code_id);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        return;
    }

    cmd_buffer[1] = 0;
    cmd_buffer[2] = country_codes[country_code_id];
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
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    _dbg_assert_(HLE, country_codes[0] == 0);

    for (size_t id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    if (0 == country_code_id) {
        ERROR_LOG(KERNEL, "requested country code name=%c%c is invalid", country_code & 0xff, country_code >> 8);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        cmd_buffer[2] = 0xFFFF;
        return;
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
