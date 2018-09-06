// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "core/mmio.h"

namespace ArmTests {

struct WriteRecord {
    WriteRecord(std::size_t size, VAddr addr, u64 data) : size(size), addr(addr), data(data) {}
    std::size_t size;
    VAddr addr;
    u64 data;
    bool operator==(const WriteRecord& o) const {
        return std::tie(size, addr, data) == std::tie(o.size, o.addr, o.data);
    }
};

class TestEnvironment final {
public:
    /*
     * Inititalise test environment
     * @param mutable_memory If false, writes to memory can never be read back.
     *                       (Memory is immutable.)
     */
    explicit TestEnvironment(bool mutable_memory = false);

    /// Shutdown test environment
    ~TestEnvironment();

    /// Sets value at memory location vaddr.
    void SetMemory8(VAddr vaddr, u8 value);
    void SetMemory16(VAddr vaddr, u16 value);
    void SetMemory32(VAddr vaddr, u32 value);
    void SetMemory64(VAddr vaddr, u64 value);

    /**
     * Whenever Memory::Write{8,16,32,64} is called within the test environment,
     * a new write-record is made.
     * @returns A vector of write records made since they were last cleared.
     */
    std::vector<WriteRecord> GetWriteRecords() const;

    /// Empties the internal write-record store.
    void ClearWriteRecords();

private:
    friend struct TestMemory;
    struct TestMemory final : Memory::MMIORegion {
        explicit TestMemory(TestEnvironment* env_) : env(env_) {}
        TestEnvironment* env;

        ~TestMemory() override;

        bool IsValidAddress(VAddr addr) override;

        u8 Read8(VAddr addr) override;
        u16 Read16(VAddr addr) override;
        u32 Read32(VAddr addr) override;
        u64 Read64(VAddr addr) override;

        bool ReadBlock(VAddr src_addr, void* dest_buffer, std::size_t size) override;

        void Write8(VAddr addr, u8 data) override;
        void Write16(VAddr addr, u16 data) override;
        void Write32(VAddr addr, u32 data) override;
        void Write64(VAddr addr, u64 data) override;

        bool WriteBlock(VAddr dest_addr, const void* src_buffer, std::size_t size) override;

        std::unordered_map<VAddr, u8> data;
    };

    bool mutable_memory;
    std::shared_ptr<TestMemory> test_memory;
    std::vector<WriteRecord> write_records;
};

} // namespace ArmTests
