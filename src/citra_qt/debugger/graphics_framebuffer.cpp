// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QComboBox>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "common/color.h"

#include "core/hw/gpu.h"
#include "core/memory.h"

#include "video_core/pica.h"
#include "video_core/utils.h"

#include "graphics_framebuffer.h"

#include "util/spinbox.h"

GraphicsFramebufferWidget::GraphicsFramebufferWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                                                     QWidget* parent)
    : BreakPointObserverDock(debug_context, tr("Pica Framebuffer"), parent),
      framebuffer_source(Source::PicaTarget)
{
    setObjectName("PicaFramebuffer");

    framebuffer_source_list = new QComboBox;
    framebuffer_source_list->addItem(tr("Active Render Target"));
    framebuffer_source_list->addItem(tr("Active Depth Buffer"));
    framebuffer_source_list->addItem(tr("Custom"));
    framebuffer_source_list->setCurrentIndex(static_cast<int>(framebuffer_source));

    framebuffer_address_control = new CSpinBox;
    framebuffer_address_control->SetBase(16);
    framebuffer_address_control->SetRange(0, 0xFFFFFFFF);
    framebuffer_address_control->SetPrefix("0x");

    framebuffer_width_control = new QSpinBox;
    framebuffer_width_control->setMinimum(1);
    framebuffer_width_control->setMaximum(std::numeric_limits<int>::max()); // TODO: Find actual maximum

    framebuffer_height_control = new QSpinBox;
    framebuffer_height_control->setMinimum(1);
    framebuffer_height_control->setMaximum(std::numeric_limits<int>::max()); // TODO: Find actual maximum

    framebuffer_format_control = new QComboBox;
    framebuffer_format_control->addItem(tr("RGBA8"));
    framebuffer_format_control->addItem(tr("RGB8"));
    framebuffer_format_control->addItem(tr("RGB5A1"));
    framebuffer_format_control->addItem(tr("RGB565"));
    framebuffer_format_control->addItem(tr("RGBA4"));
    framebuffer_format_control->addItem(tr("D16"));
    framebuffer_format_control->addItem(tr("D24"));
    framebuffer_format_control->addItem(tr("D24X8"));
    framebuffer_format_control->addItem(tr("X24S8"));
    framebuffer_format_control->addItem(tr("(unknown)"));

    // TODO: This QLabel should shrink the image to the available space rather than just expanding...
    framebuffer_picture_label = new QLabel;

    auto enlarge_button = new QPushButton(tr("Enlarge"));

    // Connections
    connect(this, SIGNAL(Update()), this, SLOT(OnUpdate()));
    connect(framebuffer_source_list, SIGNAL(currentIndexChanged(int)), this, SLOT(OnFramebufferSourceChanged(int)));
    connect(framebuffer_address_control, SIGNAL(ValueChanged(qint64)), this, SLOT(OnFramebufferAddressChanged(qint64)));
    connect(framebuffer_width_control, SIGNAL(valueChanged(int)), this, SLOT(OnFramebufferWidthChanged(int)));
    connect(framebuffer_height_control, SIGNAL(valueChanged(int)), this, SLOT(OnFramebufferHeightChanged(int)));
    connect(framebuffer_format_control, SIGNAL(currentIndexChanged(int)), this, SLOT(OnFramebufferFormatChanged(int)));

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Source:")));
        sub_layout->addWidget(framebuffer_source_list);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Virtual Address:")));
        sub_layout->addWidget(framebuffer_address_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Width:")));
        sub_layout->addWidget(framebuffer_width_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Height:")));
        sub_layout->addWidget(framebuffer_height_control);
        main_layout->addLayout(sub_layout);
    }
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Format:")));
        sub_layout->addWidget(framebuffer_format_control);
        main_layout->addLayout(sub_layout);
    }
    main_layout->addWidget(framebuffer_picture_label);
    main_layout->addWidget(enlarge_button);
    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    // Load current data - TODO: Make sure this works when emulation is not running
    if (debug_context && debug_context->at_breakpoint)
        emit Update();
    widget()->setEnabled(false); // TODO: Only enable if currently at breakpoint
}

void GraphicsFramebufferWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data)
{
    emit Update();
    widget()->setEnabled(true);
}

void GraphicsFramebufferWidget::OnResumed()
{
    widget()->setEnabled(false);
}

void GraphicsFramebufferWidget::OnFramebufferSourceChanged(int new_value)
{
    framebuffer_source = static_cast<Source>(new_value);
    emit Update();
}

void GraphicsFramebufferWidget::OnFramebufferAddressChanged(qint64 new_value)
{
    if (framebuffer_address != new_value) {
        framebuffer_address = static_cast<unsigned>(new_value);

        framebuffer_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsFramebufferWidget::OnFramebufferWidthChanged(int new_value)
{
    if (framebuffer_width != static_cast<unsigned>(new_value)) {
        framebuffer_width = static_cast<unsigned>(new_value);

        framebuffer_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsFramebufferWidget::OnFramebufferHeightChanged(int new_value)
{
    if (framebuffer_height != static_cast<unsigned>(new_value)) {
        framebuffer_height = static_cast<unsigned>(new_value);

        framebuffer_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsFramebufferWidget::OnFramebufferFormatChanged(int new_value)
{
    if (framebuffer_format != static_cast<Format>(new_value)) {
        framebuffer_format = static_cast<Format>(new_value);

        framebuffer_source_list->setCurrentIndex(static_cast<int>(Source::Custom));
        emit Update();
    }
}

void GraphicsFramebufferWidget::OnUpdate()
{
    QPixmap pixmap;

    switch (framebuffer_source) {
    case Source::PicaTarget:
    {
        // TODO: Store a reference to the registers in the debug context instead of accessing them directly...

        const auto& framebuffer = Pica::g_state.regs.framebuffer;

        framebuffer_address = framebuffer.GetColorBufferPhysicalAddress();
        framebuffer_width = framebuffer.GetWidth();
        framebuffer_height = framebuffer.GetHeight();

        switch (framebuffer.color_format) {
        case Pica::Regs::ColorFormat::RGBA8:
            framebuffer_format = Format::RGBA8;
            break;

        case Pica::Regs::ColorFormat::RGB8:
            framebuffer_format = Format::RGB8;
            break;

        case Pica::Regs::ColorFormat::RGB5A1:
            framebuffer_format = Format::RGB5A1;
            break;

        case Pica::Regs::ColorFormat::RGB565:
            framebuffer_format = Format::RGB565;
            break;

        case Pica::Regs::ColorFormat::RGBA4:
            framebuffer_format = Format::RGBA4;
            break;

        default:
            framebuffer_format = Format::Unknown;
            break;
        }

        break;
    }

    case Source::DepthBuffer:
    {
        const auto& framebuffer = Pica::g_state.regs.framebuffer;

        framebuffer_address = framebuffer.GetDepthBufferPhysicalAddress();
        framebuffer_width = framebuffer.GetWidth();
        framebuffer_height = framebuffer.GetHeight();

        switch (framebuffer.depth_format) {
        case Pica::Regs::DepthFormat::D16:
            framebuffer_format = Format::D16;
            break;

        case Pica::Regs::DepthFormat::D24:
            framebuffer_format = Format::D24;
            break;

        case Pica::Regs::DepthFormat::D24S8:
            framebuffer_format = Format::D24X8;
            break;

        default:
            framebuffer_format = Format::Unknown;
            break;
        }

        break;
    }

    case Source::Custom:
    {
        // Keep user-specified values
        break;
    }

    default:
        qDebug() << "Unknown framebuffer source " << static_cast<int>(framebuffer_source);
        break;
    }

    // TODO: Implement a good way to visualize alpha components!
    // TODO: Unify this decoding code with the texture decoder
    u32 bytes_per_pixel = GraphicsFramebufferWidget::BytesPerPixel(framebuffer_format);

    QImage decoded_image(framebuffer_width, framebuffer_height, QImage::Format_ARGB32);
    u8* buffer = Memory::GetPhysicalPointer(framebuffer_address);

    for (unsigned int y = 0; y < framebuffer_height; ++y) {
        for (unsigned int x = 0; x < framebuffer_width; ++x) {
            const u32 coarse_y = y & ~7;
            u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width * bytes_per_pixel;
            const u8* pixel = buffer + offset;
            Math::Vec4<u8> color = { 0, 0, 0, 0 };

            switch (framebuffer_format) {
            case Format::RGBA8:
                color = Color::DecodeRGBA8(pixel);
                break;
            case Format::RGB8:
                color = Color::DecodeRGB8(pixel);
                break;
            case Format::RGB5A1:
                color = Color::DecodeRGB5A1(pixel);
                break;
            case Format::RGB565:
                color = Color::DecodeRGB565(pixel);
                break;
            case Format::RGBA4:
                color = Color::DecodeRGBA4(pixel);
                break;
            case Format::D16:
            {
                u32 data = Color::DecodeD16(pixel);
                color.r() = data & 0xFF;
                color.g() = (data >> 8) & 0xFF;
                break;
            }
            case Format::D24:
            {
                u32 data = Color::DecodeD24(pixel);
                color.r() = data & 0xFF;
                color.g() = (data >> 8) & 0xFF;
                color.b() = (data >> 16) & 0xFF;
                break;
            }
            case Format::D24X8:
            {
                Math::Vec2<u32> data = Color::DecodeD24S8(pixel);
                color.r() = data.x & 0xFF;
                color.g() = (data.x >> 8) & 0xFF;
                color.b() = (data.x >> 16) & 0xFF;
                break;
            }
            case Format::X24S8:
            {
                Math::Vec2<u32> data = Color::DecodeD24S8(pixel);
                color.r() = color.g() = color.b() = data.y;
                break;
            }
            default:
                qDebug() << "Unknown fb color format " << static_cast<int>(framebuffer_format);
                break;
            }

            decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), 255));
        }
    }
    pixmap = QPixmap::fromImage(decoded_image);

    framebuffer_address_control->SetValue(framebuffer_address);
    framebuffer_width_control->setValue(framebuffer_width);
    framebuffer_height_control->setValue(framebuffer_height);
    framebuffer_format_control->setCurrentIndex(static_cast<int>(framebuffer_format));
    framebuffer_picture_label->setPixmap(pixmap);
}

u32 GraphicsFramebufferWidget::BytesPerPixel(GraphicsFramebufferWidget::Format format) {
    switch (format) {
        case Format::RGBA8:
        case Format::D24X8:
        case Format::X24S8:
            return 4;
        case Format::RGB8:
        case Format::D24:
            return 3;
        case Format::RGB5A1:
        case Format::RGB565:
        case Format::RGBA4:
        case Format::D16:
            return 2;
    }
}
