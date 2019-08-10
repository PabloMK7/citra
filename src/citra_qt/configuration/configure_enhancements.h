#ifndef CONFIGURE_ENHANCEMENTS_H
#define CONFIGURE_ENHANCEMENTS_H

#include <QWidget>

namespace Ui {
class ConfigureEnhancements;
}

class ConfigureEnhancements : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigureEnhancements(QWidget *parent = nullptr);
    ~ConfigureEnhancements();

private:
    Ui::ConfigureEnhancements *ui;
};

#endif // CONFIGURE_ENHANCEMENTS_H
