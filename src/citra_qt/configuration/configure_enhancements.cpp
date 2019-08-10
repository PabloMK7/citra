#include "configure_enhancements.h"
#include "ui_configure_enhancements.h"

ConfigureEnhancements::ConfigureEnhancements(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureEnhancements)
{
    ui->setupUi(this);
}

ConfigureEnhancements::~ConfigureEnhancements()
{
    delete ui;
}
