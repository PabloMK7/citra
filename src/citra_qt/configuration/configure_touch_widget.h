// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <utility>
#include <vector>
#include <QFrame>
#include <QPointer>

class QLabel;

// Widget for representing touchscreen coordinates
class TouchScreenPreview : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor dotHighlightColor MEMBER dot_highlight_color)

public:
    TouchScreenPreview(QWidget* parent);
    ~TouchScreenPreview() override;

    void SetCoordLabel(QLabel* const);
    int AddDot(const int device_x, const int device_y);
    void RemoveDot(const int id);
    void HighlightDot(const int id, const bool active = true) const;
    void MoveDot(const int id, const int device_x, const int device_y) const;

signals:
    void DotAdded(const QPoint& pos);
    void DotSelected(const int dot_id);
    void DotMoved(const int dot_id, const QPoint& pos);

protected:
    virtual void resizeEvent(QResizeEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent*) override;
    virtual void leaveEvent(QEvent*) override;
    virtual void mousePressEvent(QMouseEvent*) override;
    virtual bool eventFilter(QObject*, QEvent*) override;

private:
    std::optional<QPoint> MapToDeviceCoords(const int screen_x, const int screen_y) const;
    void PositionDot(QLabel* const dot, const int device_x = -1, const int device_y = -1) const;

    bool ignore_resize = false;
    QPointer<QLabel> coord_label;

    std::vector<std::pair<int, QLabel*>> dots;
    int max_dot_id = 0;
    QColor dot_highlight_color;
    static constexpr char prop_id[] = "dot_id";
    static constexpr char prop_x[] = "device_x";
    static constexpr char prop_y[] = "device_y";

    struct {
        bool active = false;
        QPointer<QLabel> dot;
        QPoint start_pos;
    } drag_state;
};
