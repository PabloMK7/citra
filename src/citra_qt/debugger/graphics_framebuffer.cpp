// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QComboBox>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "core/hw/gpu.h"
#include "video_core/color.h"
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
    framebuffer_format_control->addItem(tr("RGBA5551"));
    framebuffer_format_control->addItem(tr("RGB565"));
    framebuffer_format_control->addItem(tr("RGBA4"));

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

        auto framebuffer = Pica::registers.framebuffer;
        using Framebuffer = decltype(framebuffer);

        framebuffer_address = framebuffer.GetColorBufferPhysicalAddress();
        framebuffer_width = framebuffer.GetWidth();
        framebuffer_height = framebuffer.GetHeight();
        // TODO: It's unknown how this format is actually specified
        framebuffer_format = Format::RGBA8;

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
    u32 bytes_per_pixel = GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(framebuffer_format));

    switch (framebuffer_format) {
    case Format::RGBA8:
    {
        QImage decoded_image(framebuffer_width, framebuffer_height, QImage::Format_ARGB32);
        u8* color_buffer = Memory::GetPointer(Pica::PAddrToVAddr(framebuffer_address));
        for (unsigned int y = 0; y < framebuffer_height; ++y) {
            for (unsigned int x = 0; x < framebuffer_width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width * bytes_per_pixel;
                u8* value = color_buffer + offset;

                decoded_image.setPixel(x, y, qRgba(value[3], value[2], value[1], 255/*value >> 24*/));
            }
        }
        pixmap = QPixmap::fromImage(decoded_image);
        break;
    }

    case Format::RGB8:
    {
        QImage decoded_image(framebuffer_width, framebuffer_height, QImage::Format_ARGB32);
        u8* color_buffer = Memory::GetPointer(Pica::PAddrToVAddr(framebuffer_address));
        for (unsigned int y = 0; y < framebuffer_height; ++y) {
            for (unsigned int x = 0; x < framebuffer_width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width * bytes_per_pixel;
                u8* pixel_pointer = color_buffer + offset;

                decoded_image.setPixel(x, y, qRgba(pixel_pointer[0], pixel_pointer[1], pixel_pointer[2], 255/*value >> 24*/));
            }
        }
        pixmap = QPixmap::fromImage(decoded_image);
        break;
    }

    case Format::RGBA5551:
    {
        QImage decoded_image(framebuffer_width, framebuffer_height, QImage::Format_ARGB32);
        u8* color_buffer = Memory::GetPointer(Pica::PAddrToVAddr(framebuffer_address));
        for (unsigned int y = 0; y < framebuffer_height; ++y) {
            for (unsigned int x = 0; x < framebuffer_width; ++x) {
                const u32 coarse_y = y & ~7;
                u32 offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width * bytes_per_pixel;
                u16 value = *(u16*)(color_buffer + offset);
                u8 r = Color::Convert5To8((value >> 11) & 0x1F);
                u8 g = Color::Convert5To8((value >> 6) & 0x1F);
                u8 b = Color::Convert5To8((value >> 1) & 0x1F);
                u8 a = Color::Convert1To8(value & 1);

                decoded_image.setPixel(x, y, qRgba(r, g, b, 255/*a*/));
            }
        }
        pixmap = QPixmap::fromImage(decoded_image);
        break;
    }

    default:
        qDebug() << "Unknown fb color format " << static_cast<int>(framebuffer_format);
        break;
    }

    framebuffer_address_control->SetValue(framebuffer_address);
    framebuffer_width_control->setValue(framebuffer_width);
    framebuffer_height_control->setValue(framebuffer_height);
    framebuffer_format_control->setCurrentIndex(static_cast<int>(framebuffer_format));
    framebuffer_picture_label->setPixmap(pixmap);
}
