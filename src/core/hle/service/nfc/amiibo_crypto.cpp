// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2017 socram8888/amiitool
// Licensed under MIT
// Refer to the license.txt file included.

#include <array>
#include <cryptopp/aes.h>
#include <cryptopp/hmac.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/hle/service/nfc/amiibo_crypto.h"

namespace Service::NFC::AmiiboCrypto {

bool IsAmiiboValid(const EncryptedNTAG215File& ntag_file) {
    const auto& amiibo_data = ntag_file.user_memory;
    LOG_DEBUG(Service_NFC, "uuid_lock=0x{0:x}", ntag_file.static_lock);
    LOG_DEBUG(Service_NFC, "compability_container=0x{0:x}", ntag_file.compability_container);
    LOG_DEBUG(Service_NFC, "write_count={}", static_cast<u16>(amiibo_data.write_counter));

    LOG_DEBUG(Service_NFC, "character_id=0x{0:x}", amiibo_data.model_info.character_id);
    LOG_DEBUG(Service_NFC, "character_variant={}", amiibo_data.model_info.character_variant);
    LOG_DEBUG(Service_NFC, "amiibo_type={}", amiibo_data.model_info.amiibo_type);
    LOG_DEBUG(Service_NFC, "model_number=0x{0:x}",
              static_cast<u16>(amiibo_data.model_info.model_number));
    LOG_DEBUG(Service_NFC, "series={}", amiibo_data.model_info.series);
    LOG_DEBUG(Service_NFC, "tag_type=0x{0:x}", amiibo_data.model_info.tag_type);

    LOG_DEBUG(Service_NFC, "tag_dynamic_lock=0x{0:x}", ntag_file.dynamic_lock);
    LOG_DEBUG(Service_NFC, "tag_CFG0=0x{0:x}", ntag_file.CFG0);
    LOG_DEBUG(Service_NFC, "tag_CFG1=0x{0:x}", ntag_file.CFG1);

    // Validate UUID
    constexpr u8 CT = 0x88; // As defined in `ISO / IEC 14443 - 3`
    if ((CT ^ ntag_file.uuid.uid[0] ^ ntag_file.uuid.uid[1] ^ ntag_file.uuid.uid[2]) !=
        ntag_file.uuid.uid[3]) {
        return false;
    }
    if ((ntag_file.uuid.uid[4] ^ ntag_file.uuid.uid[5] ^ ntag_file.uuid.uid[6] ^
         ntag_file.uuid.nintendo_id) != ntag_file.uuid.lock_bytes[0]) {
        return false;
    }

    // Check against all know constants on an amiibo binary
    if (ntag_file.static_lock != 0xE00F) {
        return false;
    }
    if (ntag_file.compability_container != 0xEEFF10F1U) {
        return false;
    }
    if (amiibo_data.model_info.tag_type != PackedTagType::Type2) {
        return false;
    }
    if ((ntag_file.dynamic_lock & 0xFFFFFF) != 0x0F0001U) {
        return false;
    }
    if (ntag_file.CFG0 != 0x04000000U) {
        return false;
    }
    if (ntag_file.CFG1 != 0x5F) {
        return false;
    }
    return true;
}

bool IsAmiiboValid(const NTAG215File& ntag_file) {
    return IsAmiiboValid(EncodedDataToNfcData(ntag_file));
}

NTAG215File NfcDataToEncodedData(const EncryptedNTAG215File& nfc_data) {
    NTAG215File encoded_data{};

    encoded_data.uid = nfc_data.uuid.uid;
    encoded_data.nintendo_id = nfc_data.uuid.nintendo_id;
    encoded_data.static_lock = nfc_data.static_lock;
    encoded_data.compability_container = nfc_data.compability_container;
    encoded_data.hmac_data = nfc_data.user_memory.hmac_data;
    encoded_data.constant_value = nfc_data.user_memory.constant_value;
    encoded_data.write_counter = nfc_data.user_memory.write_counter;
    encoded_data.amiibo_version = nfc_data.user_memory.amiibo_version;
    encoded_data.settings = nfc_data.user_memory.settings;
    encoded_data.owner_mii = nfc_data.user_memory.owner_mii;
    encoded_data.padding = nfc_data.user_memory.padding;
    encoded_data.owner_mii_aes_ccm = nfc_data.user_memory.owner_mii_aes_ccm;
    encoded_data.application_id = nfc_data.user_memory.application_id;
    encoded_data.application_write_counter = nfc_data.user_memory.application_write_counter;
    encoded_data.application_area_id = nfc_data.user_memory.application_area_id;
    encoded_data.application_id_byte = nfc_data.user_memory.application_id_byte;
    encoded_data.unknown = nfc_data.user_memory.unknown;
    encoded_data.mii_extension = nfc_data.user_memory.mii_extension;
    encoded_data.unknown2 = nfc_data.user_memory.unknown2;
    encoded_data.register_info_crc = nfc_data.user_memory.register_info_crc;
    encoded_data.application_area = nfc_data.user_memory.application_area;
    encoded_data.hmac_tag = nfc_data.user_memory.hmac_tag;
    encoded_data.lock_bytes = nfc_data.uuid.lock_bytes;
    encoded_data.model_info = nfc_data.user_memory.model_info;
    encoded_data.keygen_salt = nfc_data.user_memory.keygen_salt;
    encoded_data.dynamic_lock = nfc_data.dynamic_lock;
    encoded_data.CFG0 = nfc_data.CFG0;
    encoded_data.CFG1 = nfc_data.CFG1;
    encoded_data.password = nfc_data.password;

    return encoded_data;
}

EncryptedNTAG215File EncodedDataToNfcData(const NTAG215File& encoded_data) {
    EncryptedNTAG215File nfc_data{};

    nfc_data.uuid.uid = encoded_data.uid;
    nfc_data.uuid.nintendo_id = encoded_data.nintendo_id;
    nfc_data.uuid.lock_bytes = encoded_data.lock_bytes;
    nfc_data.static_lock = encoded_data.static_lock;
    nfc_data.compability_container = encoded_data.compability_container;
    nfc_data.user_memory.hmac_data = encoded_data.hmac_data;
    nfc_data.user_memory.constant_value = encoded_data.constant_value;
    nfc_data.user_memory.write_counter = encoded_data.write_counter;
    nfc_data.user_memory.amiibo_version = encoded_data.amiibo_version;
    nfc_data.user_memory.settings = encoded_data.settings;
    nfc_data.user_memory.owner_mii = encoded_data.owner_mii;
    nfc_data.user_memory.padding = encoded_data.padding;
    nfc_data.user_memory.owner_mii_aes_ccm = encoded_data.owner_mii_aes_ccm;
    nfc_data.user_memory.application_id = encoded_data.application_id;
    nfc_data.user_memory.application_write_counter = encoded_data.application_write_counter;
    nfc_data.user_memory.application_area_id = encoded_data.application_area_id;
    nfc_data.user_memory.application_id_byte = encoded_data.application_id_byte;
    nfc_data.user_memory.unknown = encoded_data.unknown;
    nfc_data.user_memory.mii_extension = encoded_data.mii_extension;
    nfc_data.user_memory.unknown2 = encoded_data.unknown2;
    nfc_data.user_memory.register_info_crc = encoded_data.register_info_crc;
    nfc_data.user_memory.application_area = encoded_data.application_area;
    nfc_data.user_memory.hmac_tag = encoded_data.hmac_tag;
    nfc_data.user_memory.model_info = encoded_data.model_info;
    nfc_data.user_memory.keygen_salt = encoded_data.keygen_salt;
    nfc_data.dynamic_lock = encoded_data.dynamic_lock;
    nfc_data.CFG0 = encoded_data.CFG0;
    nfc_data.CFG1 = encoded_data.CFG1;
    nfc_data.password = encoded_data.password;

    return nfc_data;
}

HashSeed GetSeed(const NTAG215File& data) {
    HashSeed seed{
        .magic = data.write_counter,
        .padding = {},
        .uid_1 = data.uid,
        .nintendo_id_1 = data.nintendo_id,
        .uid_2 = data.uid,
        .nintendo_id_2 = data.nintendo_id,
        .keygen_salt = data.keygen_salt,
    };

    return seed;
}

std::vector<u8> GenerateInternalKey(const InternalKey& key, const HashSeed& seed) {
    const std::size_t seed_part1_len = sizeof(key.magic_bytes) - key.magic_length;
    const std::size_t string_size = key.type_string.size();
    std::vector<u8> output(string_size + seed_part1_len);

    // Copy whole type string
    memccpy(output.data(), key.type_string.data(), '\0', string_size);

    // Append (16 - magic_length) from the input seed
    memcpy(output.data() + string_size, &seed, seed_part1_len);

    // Append all bytes from magicBytes
    output.insert(output.end(), key.magic_bytes.begin(),
                  key.magic_bytes.begin() + key.magic_length);

    output.insert(output.end(), seed.uid_1.begin(), seed.uid_1.end());
    output.emplace_back(seed.nintendo_id_1);
    output.insert(output.end(), seed.uid_2.begin(), seed.uid_2.end());
    output.emplace_back(seed.nintendo_id_2);

    for (std::size_t i = 0; i < sizeof(seed.keygen_salt); i++) {
        output.emplace_back(static_cast<u8>(seed.keygen_salt[i] ^ key.xor_pad[i]));
    }

    return output;
}

void CryptoInit(CryptoCtx& ctx, CryptoPP::HMAC<CryptoPP::SHA256>& hmac_ctx, const HmacKey& hmac_key,
                const std::vector<u8>& seed) {
    // Initialize context
    ctx.used = false;
    ctx.counter = 0;
    ctx.buffer_size = sizeof(ctx.counter) + seed.size();
    memcpy(ctx.buffer.data() + sizeof(u16), seed.data(), seed.size());

    // Initialize HMAC context
    hmac_ctx.SetKey(hmac_key.data(), hmac_key.size());
}

void CryptoStep(CryptoCtx& ctx, CryptoPP::HMAC<CryptoPP::SHA256>& hmac_ctx, DrgbOutput& output) {
    // If used at least once, reinitialize the HMAC
    if (ctx.used) {
        hmac_ctx.Restart();
    }

    ctx.used = true;

    // Store counter in big endian, and increment it
    ctx.buffer[0] = static_cast<u8>(ctx.counter >> 8);
    ctx.buffer[1] = static_cast<u8>(ctx.counter >> 0);
    ctx.counter++;

    // Do HMAC magic
    hmac_ctx.CalculateDigest(
        output.data(), reinterpret_cast<const unsigned char*>(ctx.buffer.data()), ctx.buffer_size);
}

DerivedKeys GenerateKey(const InternalKey& key, const NTAG215File& data) {
    const auto seed = GetSeed(data);

    // Generate internal seed
    const std::vector<u8> internal_key = GenerateInternalKey(key, seed);

    // Initialize context
    CryptoCtx ctx{};
    CryptoPP::HMAC<CryptoPP::SHA256> hmac_ctx;
    CryptoInit(ctx, hmac_ctx, key.hmac_key, internal_key);

    // Generate derived keys
    DerivedKeys derived_keys{};
    std::array<DrgbOutput, 2> temp{};
    CryptoStep(ctx, hmac_ctx, temp[0]);
    CryptoStep(ctx, hmac_ctx, temp[1]);
    memcpy(&derived_keys, temp.data(), sizeof(DerivedKeys));

    return derived_keys;
}

void Cipher(const DerivedKeys& keys, const NTAG215File& in_data, NTAG215File& out_data) {
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d2;
    d2.SetKeyWithIV(keys.aes_key.data(), keys.aes_key.size(), keys.aes_iv.data(),
                    keys.aes_iv.size());

    constexpr std::size_t encrypted_data_size = HMAC_TAG_START - SETTINGS_START;
    d2.ProcessData(reinterpret_cast<unsigned char*>(&out_data.settings),
                   reinterpret_cast<const unsigned char*>(&in_data.settings), encrypted_data_size);

    // Copy the rest of the data directly
    out_data.uid = in_data.uid;
    out_data.nintendo_id = in_data.nintendo_id;
    out_data.lock_bytes = in_data.lock_bytes;
    out_data.static_lock = in_data.static_lock;
    out_data.compability_container = in_data.compability_container;

    out_data.constant_value = in_data.constant_value;
    out_data.write_counter = in_data.write_counter;

    out_data.model_info = in_data.model_info;
    out_data.keygen_salt = in_data.keygen_salt;
    out_data.dynamic_lock = in_data.dynamic_lock;
    out_data.CFG0 = in_data.CFG0;
    out_data.CFG1 = in_data.CFG1;
    out_data.password = in_data.password;
}

bool LoadKeys(InternalKey& locked_secret, InternalKey& unfixed_info) {
    const auto citra_keys_dir = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir);
    auto keys_file = FileUtil::IOFile(citra_keys_dir + "key_retail.bin", "rb");

    if (!keys_file.IsOpen()) {
        LOG_ERROR(Service_NFC, "No keys detected");
        return false;
    }

    if (keys_file.ReadBytes(&unfixed_info, sizeof(InternalKey)) != sizeof(InternalKey)) {
        LOG_ERROR(Service_NFC, "Failed to read unfixed_info");
        return false;
    }
    if (keys_file.ReadBytes(&locked_secret, sizeof(InternalKey)) != sizeof(InternalKey)) {
        LOG_ERROR(Service_NFC, "Failed to read locked-secret");
        return false;
    }

    return true;
}

bool IsKeyAvailable() {
    const auto citra_keys_dir = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir);
    return FileUtil::Exists(citra_keys_dir + "key_retail.bin");
}

bool DecodeAmiibo(const EncryptedNTAG215File& encrypted_tag_data, NTAG215File& tag_data) {
    InternalKey locked_secret{};
    InternalKey unfixed_info{};

    if (!LoadKeys(locked_secret, unfixed_info)) {
        return false;
    }

    // Generate keys
    NTAG215File encoded_data = NfcDataToEncodedData(encrypted_tag_data);
    const auto data_keys = GenerateKey(unfixed_info, encoded_data);
    const auto tag_keys = GenerateKey(locked_secret, encoded_data);

    // Decrypt
    Cipher(data_keys, encoded_data, tag_data);

    // Regenerate tag HMAC. Note: order matters, data HMAC depends on tag HMAC!
    constexpr std::size_t input_length = DYNAMIC_LOCK_START - UUID_START;
    CryptoPP::HMAC<CryptoPP::SHA256> tag_hmac(tag_keys.hmac_key.data(), sizeof(HmacKey));
    tag_hmac.CalculateDigest(reinterpret_cast<unsigned char*>(&tag_data.hmac_tag),
                             reinterpret_cast<const unsigned char*>(&tag_data.uid), input_length);

    // Regenerate data HMAC
    constexpr std::size_t input_length2 = DYNAMIC_LOCK_START - WRITE_COUNTER_START;
    CryptoPP::HMAC<CryptoPP::SHA256> data_hmac(data_keys.hmac_key.data(), sizeof(HmacKey));
    data_hmac.CalculateDigest(reinterpret_cast<unsigned char*>(&tag_data.hmac_data),
                              reinterpret_cast<const unsigned char*>(&tag_data.write_counter),
                              input_length2);

    if (tag_data.hmac_data != encrypted_tag_data.user_memory.hmac_data) {
        LOG_ERROR(Service_NFC, "hmac_data doesn't match");
        return false;
    }

    if (tag_data.hmac_tag != encrypted_tag_data.user_memory.hmac_tag) {
        LOG_ERROR(Service_NFC, "hmac_tag doesn't match");
        return false;
    }

    return true;
}

bool EncodeAmiibo(const NTAG215File& tag_data, EncryptedNTAG215File& encrypted_tag_data) {
    InternalKey locked_secret{};
    InternalKey unfixed_info{};

    if (!LoadKeys(locked_secret, unfixed_info)) {
        return false;
    }

    // Generate keys
    const auto data_keys = GenerateKey(unfixed_info, tag_data);
    const auto tag_keys = GenerateKey(locked_secret, tag_data);

    NTAG215File encoded_tag_data{};

    // Generate tag HMAC
    constexpr std::size_t input_length = DYNAMIC_LOCK_START - UUID_START;
    constexpr std::size_t input_length2 = HMAC_TAG_START - WRITE_COUNTER_START;
    CryptoPP::HMAC<CryptoPP::SHA256> tag_hmac(tag_keys.hmac_key.data(), sizeof(HmacKey));
    tag_hmac.CalculateDigest(reinterpret_cast<unsigned char*>(&encoded_tag_data.hmac_tag),
                             reinterpret_cast<const unsigned char*>(&tag_data.uid), input_length);

    // Generate data HMAC
    CryptoPP::HMAC<CryptoPP::SHA256> data_hmac(data_keys.hmac_key.data(), sizeof(HmacKey));
    data_hmac.Update(reinterpret_cast<const unsigned char*>(&tag_data.write_counter),
                     input_length2);
    data_hmac.Update(reinterpret_cast<unsigned char*>(&encoded_tag_data.hmac_tag),
                     sizeof(HashData));
    data_hmac.Update(reinterpret_cast<const unsigned char*>(&tag_data.uid), input_length);
    data_hmac.Final(reinterpret_cast<unsigned char*>(&encoded_tag_data.hmac_data));

    // Encrypt
    Cipher(data_keys, tag_data, encoded_tag_data);

    // Convert back to hardware
    encrypted_tag_data = EncodedDataToNfcData(encoded_tag_data);

    return true;
}

} // namespace Service::NFC::AmiiboCrypto
