// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <sstream>

#include <QBoxLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalMapper>
#include <QSpinBox>
#include <QTreeView>

#include "citra_qt/util/util.h"

#include "video_core/shader/shader.h"

#include "graphics_vertex_shader.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

GraphicsVertexShaderModel::GraphicsVertexShaderModel(GraphicsVertexShaderWidget* parent): QAbstractItemModel(parent), par(parent) {

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
    return static_cast<int>(par->info.code.size());
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
            if (par->info.HasLabel(index.row()))
                return QString::fromStdString(par->info.GetLabel(index.row()));

            return QString("%1").arg(4*index.row(), 4, 16, QLatin1Char('0'));

        case 1:
            return QString("%1").arg(par->info.code[index.row()].hex, 8, 16, QLatin1Char('0'));

        case 2:
        {
            std::stringstream output;
            output.flags(std::ios::hex);

            Instruction instr = par->info.code[index.row()];
            const SwizzlePattern& swizzle = par->info.swizzle_info[instr.common.operand_desc_id].pattern;

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
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true).substr(0,1));

                    output << ", ";

                    print_input_indexed_compact(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false).substr(1,1), instr.common.AddressRegisterName());
                    output << " " << instr.common.compare_op.ToString(instr.common.compare_op.y) << " ";
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true).substr(1,1));

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
                        print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true));
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
        return GetMonospaceFont();

    case Qt::BackgroundRole:
        // Highlight instructions which have no debug data associated to them
        for (const auto& record : par->debug_data.records)
            if (index.row() == record.instruction_offset)
                return QVariant();

        return QBrush(QColor(255, 255, 127));


    // TODO: Draw arrows for each "reachable" instruction to visualize control flow


    default:
        break;
    }

    return QVariant();
}

void GraphicsVertexShaderWidget::DumpShader() {
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Shader Dump"), "shader_dump.shbin",
                                                    tr("Shader Binary (*.shbin)"));

    if (filename.isEmpty()) {
        // If the user canceled the dialog, don't dump anything.
        return;
    }

    auto& setup  = Pica::g_state.vs;
    auto& config = Pica::g_state.regs.vs;

    Pica::DebugUtils::DumpShader(filename.toStdString(), config, setup, Pica::g_state.regs.vs_output_attributes);
}

GraphicsVertexShaderWidget::GraphicsVertexShaderWidget(std::shared_ptr< Pica::DebugContext > debug_context,
                                                       QWidget* parent)
        : BreakPointObserverDock(debug_context, "Pica Vertex Shader", parent) {
    setObjectName("PicaVertexShader");

    auto input_data_mapper = new QSignalMapper(this);

    // TODO: Support inputting data in hexadecimal raw format
    for (unsigned i = 0; i < ARRAY_SIZE(input_data); ++i) {
        input_data[i] = new QLineEdit;
        input_data[i]->setValidator(new QDoubleValidator(input_data[i]));
    }

    breakpoint_warning = new QLabel(tr("(data only available at VertexLoaded breakpoints)"));

    // TODO: Add some button for jumping to the shader entry point

    model = new GraphicsVertexShaderModel(this);
    binary_list = new QTreeView;
    binary_list->setModel(model);
    binary_list->setRootIsDecorated(false);
    binary_list->setAlternatingRowColors(true);

    auto dump_shader = new QPushButton(QIcon::fromTheme("document-save"), tr("Dump"));

    instruction_description = new QLabel;

    cycle_index = new QSpinBox;

    connect(this, SIGNAL(SelectCommand(const QModelIndex&, QItemSelectionModel::SelectionFlags)),
            binary_list->selectionModel(), SLOT(select(const QModelIndex&, QItemSelectionModel::SelectionFlags)));

    connect(dump_shader, SIGNAL(clicked()), this, SLOT(DumpShader()));

    connect(cycle_index, SIGNAL(valueChanged(int)), this, SLOT(OnCycleIndexChanged(int)));

    for (unsigned i = 0; i < ARRAY_SIZE(input_data); ++i) {
        connect(input_data[i], SIGNAL(textEdited(const QString&)), input_data_mapper, SLOT(map()));
        input_data_mapper->setMapping(input_data[i], i);
    }
    connect(input_data_mapper, SIGNAL(mapped(int)), this, SLOT(OnInputAttributeChanged(int)));

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto input_data_group = new QGroupBox(tr("Input Data"));

        // For each vertex attribute, add a QHBoxLayout consisting of:
        // - A QLabel denoting the source attribute index
        // - Four QLineEdits for showing and manipulating attribute data
        // - A QLabel denoting the shader input attribute index
        auto sub_layout = new QVBoxLayout;
        for (unsigned i = 0; i < 16; ++i) {
            // Create an HBoxLayout to store the widgets used to specify a particular attribute
            // and store it in a QWidget to allow for easy hiding and unhiding.
            auto row_layout = new QHBoxLayout;
            row_layout->addWidget(new QLabel(tr("Attribute %1").arg(i, 2)));
            for (unsigned comp = 0; comp < 4; ++comp)
                row_layout->addWidget(input_data[4 * i + comp]);

            row_layout->addWidget(input_data_mapping[i] = new QLabel);

            input_data_container[i] = new QWidget;
            input_data_container[i]->setLayout(row_layout);
            input_data_container[i]->hide();

            sub_layout->addWidget(input_data_container[i]);
        }

        sub_layout->addWidget(breakpoint_warning);
        breakpoint_warning->hide();

        input_data_group->setLayout(sub_layout);
        main_layout->addWidget(input_data_group);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(binary_list);
        main_layout->addLayout(sub_layout);
    }
    main_layout->addWidget(dump_shader);
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Cycle Index:")));
        sub_layout->addWidget(cycle_index);
        main_layout->addLayout(sub_layout);
    }
    main_layout->addWidget(instruction_description);
    main_layout->addStretch();
    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    widget()->setEnabled(false);
}

void GraphicsVertexShaderWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    auto input = static_cast<Pica::Shader::InputVertex*>(data);
    if (event == Pica::DebugContext::Event::VertexLoaded) {
        Reload(true, data);
    } else {
        // No vertex data is retrievable => invalidate currently stored vertex data
        Reload(true, nullptr);
    }
    widget()->setEnabled(true);
}

void GraphicsVertexShaderWidget::Reload(bool replace_vertex_data, void* vertex_data) {
    model->beginResetModel();

    if (replace_vertex_data) {
        if (vertex_data) {
            memcpy(&input_vertex, vertex_data, sizeof(input_vertex));
            for (unsigned attr = 0; attr < 16; ++attr) {
                for (unsigned comp = 0; comp < 4; ++comp) {
                    input_data[4 * attr + comp]->setText(QString("%1").arg(input_vertex.attr[attr][comp].ToFloat32()));
                }
            }
            breakpoint_warning->hide();
        } else {
            for (unsigned attr = 0; attr < 16; ++attr) {
                for (unsigned comp = 0; comp < 4; ++comp) {
                    input_data[4 * attr + comp]->setText(QString("???"));
                }
            }
            breakpoint_warning->show();
        }
    }

    // Reload shader code
    info.Clear();

    auto& shader_setup = Pica::g_state.vs;
    auto& shader_config = Pica::g_state.regs.vs;
    for (auto instr : shader_setup.program_code)
        info.code.push_back({instr});

    for (auto pattern : shader_setup.swizzle_data)
        info.swizzle_info.push_back({pattern});

    u32 entry_point = Pica::g_state.regs.vs.main_offset;
    info.labels.insert({ entry_point, "main" });

    // Generate debug information
    debug_data = Pica::Shader::ProduceDebugInfo(input_vertex, 1, shader_config, shader_setup);

    // Reload widget state

    // Only show input attributes which are used as input to the shader
    for (unsigned int attr = 0; attr < 16; ++attr) {
        input_data_container[attr]->setVisible(false);
    }
    for (unsigned int attr = 0; attr < Pica::g_state.regs.vertex_attributes.GetNumTotalAttributes(); ++attr) {
        unsigned source_attr = shader_config.input_register_map.GetRegisterForAttribute(attr);
        input_data_mapping[source_attr]->setText(QString("-> v%1").arg(attr));
        input_data_container[source_attr]->setVisible(true);
    }

    // Initialize debug info text for current cycle count
    cycle_index->setMaximum(debug_data.records.size() - 1);
    OnCycleIndexChanged(cycle_index->value());

    model->endResetModel();
}

void GraphicsVertexShaderWidget::OnResumed() {
    widget()->setEnabled(false);
}

void GraphicsVertexShaderWidget::OnInputAttributeChanged(int index) {
    float value = input_data[index]->text().toFloat();
    Reload();
}

void GraphicsVertexShaderWidget::OnCycleIndexChanged(int index) {
    QString text;

    auto& record = debug_data.records[index];
    if (record.mask & Pica::Shader::DebugDataRecord::SRC1)
        text += tr("SRC1: %1, %2, %3, %4\n").arg(record.src1.x.ToFloat32()).arg(record.src1.y.ToFloat32()).arg(record.src1.z.ToFloat32()).arg(record.src1.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::SRC2)
        text += tr("SRC2: %1, %2, %3, %4\n").arg(record.src2.x.ToFloat32()).arg(record.src2.y.ToFloat32()).arg(record.src2.z.ToFloat32()).arg(record.src2.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::SRC3)
        text += tr("SRC3: %1, %2, %3, %4\n").arg(record.src3.x.ToFloat32()).arg(record.src3.y.ToFloat32()).arg(record.src3.z.ToFloat32()).arg(record.src3.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::DEST_IN)
        text += tr("DEST_IN: %1, %2, %3, %4\n").arg(record.dest_in.x.ToFloat32()).arg(record.dest_in.y.ToFloat32()).arg(record.dest_in.z.ToFloat32()).arg(record.dest_in.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::DEST_OUT)
        text += tr("DEST_OUT: %1, %2, %3, %4\n").arg(record.dest_out.x.ToFloat32()).arg(record.dest_out.y.ToFloat32()).arg(record.dest_out.z.ToFloat32()).arg(record.dest_out.w.ToFloat32());

    if (record.mask & Pica::Shader::DebugDataRecord::ADDR_REG_OUT)
        text += tr("Addres Registers: %1, %2\n").arg(record.address_registers[0]).arg(record.address_registers[1]);
    if (record.mask & Pica::Shader::DebugDataRecord::CMP_RESULT)
        text += tr("Compare Result: %1, %2\n").arg(record.conditional_code[0] ? "true" : "false").arg(record.conditional_code[1] ? "true" : "false");

    if (record.mask & Pica::Shader::DebugDataRecord::COND_BOOL_IN)
        text += tr("Static Condition: %1\n").arg(record.cond_bool ? "true" : "false");
    if (record.mask & Pica::Shader::DebugDataRecord::COND_CMP_IN)
        text += tr("Dynamic Conditions: %1, %2\n").arg(record.cond_cmp[0] ? "true" : "false").arg(record.cond_cmp[1] ? "true" : "false");
    if (record.mask & Pica::Shader::DebugDataRecord::LOOP_INT_IN)
        text += tr("Loop Parameters: %1 (repeats), %2 (initializer), %3 (increment), %4\n").arg(record.loop_int.x).arg(record.loop_int.y).arg(record.loop_int.z).arg(record.loop_int.w);

    text += tr("Instruction offset: 0x%1").arg(4 * record.instruction_offset, 4, 16, QLatin1Char('0'));
    if (record.mask & Pica::Shader::DebugDataRecord::NEXT_INSTR) {
        text += tr(" -> 0x%2").arg(4 * record.next_instruction, 4, 16, QLatin1Char('0'));
    } else {
        text += tr(" (last instruction)");
    }

    instruction_description->setText(text);

    // Scroll to current instruction
    const QModelIndex& instr_index = model->index(record.instruction_offset, 0);
    emit SelectCommand(instr_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    binary_list->scrollTo(instr_index, QAbstractItemView::EnsureVisible);
}
