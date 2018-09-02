// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <dynarmic/A32/coprocessor.h>
#include "common/common_types.h"

struct ARMul_State;

class DynarmicCP15 final : public Dynarmic::A32::Coprocessor {
public:
    using CoprocReg = Dynarmic::A32::CoprocReg;

    explicit DynarmicCP15(const std::shared_ptr<ARMul_State>&);
    ~DynarmicCP15() override;

    boost::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd,
                                                       CoprocReg CRn, CoprocReg CRm,
                                                       unsigned opc2) override;
    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
                                               CoprocReg CRm, unsigned opc2) override;
    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override;
    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm,
                                              unsigned opc2) override;
    CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) override;
    boost::optional<Callback> CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd,
                                               boost::optional<u8> option) override;
    boost::optional<Callback> CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd,
                                                boost::optional<u8> option) override;

private:
    std::shared_ptr<ARMul_State> interpreter_state;
};
