// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/settings.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_u.h"

namespace Service {
namespace CFG {

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
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 country_code_id = cmd_buffer[1];

    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        LOG_ERROR(Service_CFG, "requested country code id=%d is invalid", country_code_id);
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
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u16 country_code = cmd_buffer[1];
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    DEBUG_ASSERT(country_codes[0] == 0);

    for (u16 id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    if (0 == country_code_id) {
        LOG_ERROR(Service_CFG, "requested country code name=%c%c is invalid", country_code & 0xff, country_code >> 8);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        cmd_buffer[2] = 0xFFFF;
        return;
    }

    cmd_buffer[1] = 0;
    cmd_buffer[2] = country_code_id;
}

/**
 * CFG_User::GetConfigInfoBlk2 service function
 *  Inputs:
 *      0 : 0x00010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetConfigInfoBlk2(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 size = cmd_buffer[1];
    u32 block_id = cmd_buffer[2];
    u8* data_pointer = Memory::GetPointer(cmd_buffer[4]);
    
    if (data_pointer == nullptr) {
        cmd_buffer[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buffer[1] = Service::CFG::GetConfigInfoBlock(block_id, size, 0x2, data_pointer).raw;
}

/**
 * CFG_User::SecureInfoGetRegion service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Region value loaded from SecureInfo offset 0x100
 */
static void SecureInfoGetRegion(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();

    cmd_buffer[1] = RESULT_SUCCESS.raw; // No Error
    cmd_buffer[2] = Settings::values.region_value;
}

/**
 * CFG_User::GenHashConsoleUnique service function
 *  Inputs:
 *      1 : 20 bit application ID salt
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Hash/"ID" lower word
 *      3 : Hash/"ID" upper word
 */
static void GenHashConsoleUnique(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 app_id_salt = cmd_buffer[1];

    cmd_buffer[1] = RESULT_SUCCESS.raw; // No Error
    cmd_buffer[2] = 0x33646D6F ^ (app_id_salt & 0xFFFFF); // 3dmoo hash
    cmd_buffer[3] = 0x6F534841 ^ (app_id_salt & 0xFFFFF);

    LOG_WARNING(Service_CFG, "(STUBBED) called app_id_salt=0x%08X", app_id_salt);
}

/**
 * CFG_User::GetRegionCanadaUSA service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output value
 */
static void GetRegionCanadaUSA(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();

    cmd_buffer[1] = RESULT_SUCCESS.raw; // No Error

    u8 canada_or_usa = 1;
    if (canada_or_usa == Settings::values.region_value) {
        cmd_buffer[2] = 1;
    } else {
        cmd_buffer[2] = 0;
    }
}

/**
 * CFG_User::GetSystemModel service function
 *  Inputs:
 *      0 : 0x00050000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Model of the console
 */
static void GetSystemModel(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buffer[1] = Service::CFG::GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                                     reinterpret_cast<u8*>(&data)).raw; 
    cmd_buffer[2] = data & 0xFF;
}

/**
 * CFG_User::GetModelNintendo2DS service function
 *  Inputs:
 *      0 : 0x00060000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 0 if the system is a Nintendo 2DS, 1 otherwise
 */
static void GetModelNintendo2DS(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buffer[1] = Service::CFG::GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                                     reinterpret_cast<u8*>(&data)).raw; 
    
    u8 model = data & 0xFF;
    if (model == Service::CFG::NINTENDO_2DS)
        cmd_buffer[2] = 0;
    else
        cmd_buffer[2] = 1;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, GetConfigInfoBlk2,     "GetConfigInfoBlk2"},
    {0x00020000, SecureInfoGetRegion,   "SecureInfoGetRegion"},
    {0x00030040, GenHashConsoleUnique,  "GenHashConsoleUnique"},
    {0x00040000, GetRegionCanadaUSA,    "GetRegionCanadaUSA"},
    {0x00050000, GetSystemModel,        "GetSystemModel"},
    {0x00060000, GetModelNintendo2DS,   "GetModelNintendo2DS"},
    {0x00070040, nullptr,               "WriteToFirstByteCfgSavegame"},
    {0x00080080, nullptr,               "GoThroughTable"},
    {0x00090040, GetCountryCodeString,  "GetCountryCodeString"},
    {0x000A0040, GetCountryCodeID,      "GetCountryCodeID"},
};

CFG_U_Interface::CFG_U_Interface() {
    Register(FunctionTable);
}

} // namespace CFG
} // namespace Service
