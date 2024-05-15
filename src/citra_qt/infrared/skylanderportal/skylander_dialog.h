// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>

#include <QDialog>
#include <QLineEdit>

constexpr auto UI_SKY_NUM = 8;

class CreateSkylanderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateSkylanderDialog(QWidget* parent);
    QString get_file_path() const;

protected:
    QString file_path;
};

class SkylanderPortalWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SkylanderPortalWindow(QWidget* parent);
    ~SkylanderPortalWindow();
    static SkylanderPortalWindow* get_dlg(QWidget* parent);

    SkylanderPortalWindow(SkylanderPortalWindow const&) = delete;
    void operator=(SkylanderPortalWindow const&) = delete;

protected:
    void clear_skylander(u8 slot);
    void create_skylander(u8 slot);
    void load_skylander(u8 slot);
    void load_skylander_path(u8 slot, const QString& path);

    void update_edits();

protected:
    QLineEdit* edit_skylanders[UI_SKY_NUM]{};
    static std::optional<std::tuple<u8, u16, u16>> sky_slots[UI_SKY_NUM];

private:
    static SkylanderPortalWindow* inst;
};
