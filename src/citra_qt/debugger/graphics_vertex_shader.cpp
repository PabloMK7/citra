// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <sstream>

#include <QBoxLayout>
#include <QTreeView>

#include "video_core/vertex_shader.h"

#include "graphics_vertex_shader.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

GraphicsVertexShaderModel::GraphicsVertexShaderModel(QObject* parent): QAbstractItemModel(parent) {

}

QModelIndex GraphicsVertexShaderModel::index(int row, int column, const QModelIndex& parent) const {
    return createIndex(row, column);
}

QModelIndex GraphicsVertexShaderModel::parent(const QModelIndex& child) const {
    return QModelIndex();
}

int GraphicsVertexShaderModel::columnCount(const QModelIndex& parent) const {
    return 3;
}

int GraphicsVertexShaderModel::rowCount(const QModelIndex& parent) const {
    return static_cast<int>(info.code.size());
}

QVariant GraphicsVertexShaderModel::headerData(int section, Qt::Orientation orientation, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    {
        if (section == 0) {
            return tr("Offset");
        } else if (section == 1) {
            return tr("Raw");
        } else if (section == 2) {
            return tr("Disassembly");
        }

        break;
    }
    }

    return QVariant();
}

QVariant GraphicsVertexShaderModel::data(const QModelIndex& index, int role) const {
    switch (role) {
    case Qt::DisplayRole:
    {
        switch (index.column()) {
        case 0:
            if (info.HasLabel(index.row()))
                return QString::fromStdString(info.GetLabel(index.row()));

            return QString("%1").arg(4*index.row(), 4, 16, QLatin1Char('0'));

        case 1:
            return QString("%1").arg(info.code[index.row()].hex, 8, 16, QLatin1Char('0'));

        case 2:
        {
            std::stringstream output;
            output.flags(std::ios::hex);

            Instruction instr = info.code[index.row()];
            const SwizzlePattern& swizzle = info.swizzle_info[instr.common.operand_desc_id].pattern;

            // longest known instruction name: "setemit "
            output << std::setw(8) << std::left << instr.opcode.Value().GetInfo().name;

            // e.g. "-c92.xyzw"
            static auto print_input = [](std::stringstream& output, const SourceRegister& input,
                                         bool negate, const std::string& swizzle_mask) {
                output << std::setw(4) << std::right << (negate ? "-" : "") + input.GetName();
                output << "." << swizzle_mask;
            };

            // e.g. "-c92[a0.x].xyzw"
            static auto print_input_indexed = [](std::stringstream& output, const SourceRegister& input,
                                                 bool negate, const std::string& swizzle_mask,
                                                 const std::string& address_register_name) {
                std::string relative_address;
                if (!address_register_name.empty())
                    relative_address = "[" + address_register_name + "]";

                output << std::setw(10) << std::right << (negate ? "-" : "") + input.GetName() + relative_address;
                output << "." << swizzle_mask;
            };

            // Use print_input or print_input_indexed depending on whether relative addressing is used or not.
            static auto print_input_indexed_compact = [](std::stringstream& output, const SourceRegister& input,
                                                         bool negate, const std::string& swizzle_mask,
                                                         const std::string& address_register_name) {
                if (address_register_name.empty())
                    print_input(output, input, negate, swizzle_mask);
                else
                    print_input_indexed(output, input, negate, swizzle_mask, address_register_name);
            };

            switch (instr.opcode.Value().GetInfo().type) {
            case OpCode::Type::Trivial:
                // Nothing to do here
                break;

            case OpCode::Type::Arithmetic:
            {
                // Use custom code for special instructions
                switch (instr.opcode.Value().EffectiveOpCode()) {
                case OpCode::Id::CMP:
                {
                    // NOTE: CMP always writes both cc components, so we do not consider the dest mask here.
                    output << std::setw(4) << std::right << "cc.";
                    output << "xy    ";

                    SourceRegister src1 = instr.common.GetSrc1(false);
                    SourceRegister src2 = instr.common.GetSrc2(false);

                    print_input_indexed_compact(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false).substr(0,1), instr.common.AddressRegisterName());
                    output << " " << instr.common.compare_op.ToString(instr.common.compare_op.x) << " ";
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(false).substr(0,1));

                    output << ", ";

                    print_input_indexed_compact(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false).substr(1,1), instr.common.AddressRegisterName());
                    output << " " << instr.common.compare_op.ToString(instr.common.compare_op.y) << " ";
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(false).substr(1,1));

                    break;
                }

                default:
                {
                    bool src_is_inverted = 0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed);

                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::Dest) {
                        // e.g. "r12.xy__"
                        output << std::setw(4) << std::right << instr.common.dest.Value().GetName() + ".";
                        output << swizzle.DestMaskToString();
                    } else if (instr.opcode.Value().GetInfo().subtype == OpCode::Info::MOVA) {
                        output << std::setw(4) << std::right << "a0.";
                        output << swizzle.DestMaskToString();
                    } else {
                        output << "        ";
                    }
                    output << "  ";

                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::Src1) {
                        SourceRegister src1 = instr.common.GetSrc1(src_is_inverted);
                        print_input_indexed(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false), instr.common.AddressRegisterName());
                    } else {
                        output << "               ";
                    }

                    // TODO: In some cases, the Address Register is used as an index for SRC2 instead of SRC1
                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::Src2) {
                        SourceRegister src2 = instr.common.GetSrc2(src_is_inverted);
                        print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(false));
                    }
                    break;
                }
                }

                break;
            }

            case OpCode::Type::Conditional:
            {
                switch (instr.opcode.Value().EffectiveOpCode()) {
                case OpCode::Id::LOOP:
                    output << "(unknown instruction format)";
                    break;

                default:
                    output << "if ";

                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::HasCondition) {
                        const char* ops[] = {
                            " || ", " && ", "", ""
                        };
                        if (instr.flow_control.op != instr.flow_control.JustY)
                            output << ((!instr.flow_control.refx) ? "!" : " ") << "cc.x";

                        output << ops[instr.flow_control.op];

                        if (instr.flow_control.op != instr.flow_control.JustX)
                            output << ((!instr.flow_control.refy) ? "!" : " ") << "cc.y";

                        output << " ";
                    } else if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::HasUniformIndex) {
                        output << "b" << instr.flow_control.bool_uniform_id << " ";
                    }

                    u32 target_addr = instr.flow_control.dest_offset;
                    u32 target_addr_else = instr.flow_control.dest_offset;

                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::HasAlternative) {
                        output << "else jump to 0x" << std::setw(4) << std::right << std::setfill('0') << 4 * instr.flow_control.dest_offset << " ";
                    } else if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::HasExplicitDest) {
                        output << "jump to 0x" << std::setw(4) << std::right << std::setfill('0') << 4 * instr.flow_control.dest_offset << " ";
                    } else {
                        // TODO: Handle other cases
                    }

                    if (instr.opcode.Value().GetInfo().subtype & OpCode::Info::HasFinishPoint) {
                        output << "(return on " << std::setw(4) << std::right << std::setfill('0')
                               << 4 * instr.flow_control.dest_offset + 4 * instr.flow_control.num_instructions << ")";
                    }

                    break;
                }
                break;
            }

            default:
                output << "(unknown instruction format)";
                break;
            }

            return QString::fromLatin1(output.str().c_str());
        }

        default:
            break;
        }
    }

    case Qt::FontRole:
        return QFont("monospace");

    default:
        break;
    }

    return QVariant();
}

void GraphicsVertexShaderModel::OnUpdate()
{
    beginResetModel();

    info.Clear();

    for (auto instr : Pica::g_state.vs.program_code)
        info.code.push_back({instr});

    for (auto pattern : Pica::g_state.vs.swizzle_data)
        info.swizzle_info.push_back({pattern});

    info.labels.insert({ Pica::g_state.regs.vs.main_offset, "main" });

    endResetModel();
}


GraphicsVertexShaderWidget::GraphicsVertexShaderWidget(std::shared_ptr< Pica::DebugContext > debug_context,
                                                       QWidget* parent)
        : BreakPointObserverDock(debug_context, "Pica Vertex Shader", parent) {
    setObjectName("PicaVertexShader");

    auto binary_model = new GraphicsVertexShaderModel(this);
    auto binary_list = new QTreeView;
    binary_list->setModel(binary_model);
    binary_list->setRootIsDecorated(false);
    binary_list->setAlternatingRowColors(true);

    connect(this, SIGNAL(Update()), binary_model, SLOT(OnUpdate()));

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(binary_list);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);
    setWidget(main_widget);
}

void GraphicsVertexShaderWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    emit Update();
    widget()->setEnabled(true);
}

void GraphicsVertexShaderWidget::OnResumed() {
    widget()->setEnabled(false);
}
