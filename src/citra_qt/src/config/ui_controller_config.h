/********************************************************************************
** Form generated from reading UI file 'controller_config.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONTROLLER_CONFIG_H
#define UI_CONTROLLER_CONFIG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QTabWidget>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ControllerConfig
{
public:
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout;
    QComboBox *activeControllerCB;
    QSpacerItem *horizontalSpacer;
    QCheckBox *checkBox;
    QComboBox *inputSourceCB;
    QLabel *label_2;
    QLabel *label;
    QTabWidget *tabWidget;
    QWidget *mainStickTab;
    QGridLayout *gridLayout_3;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *verticalSpacer_3;
    QSpacerItem *horizontalSpacer_4;
    QSpacerItem *horizontalSpacer_3;
    QWidget *cStickTab;
    QGridLayout *gridLayout_4;
    QSpacerItem *horizontalSpacer_6;
    QSpacerItem *verticalSpacer_5;
    QSpacerItem *verticalSpacer_4;
    QSpacerItem *horizontalSpacer_5;
    QWidget *dPadTab;
    QGridLayout *gridLayout_5;
    QSpacerItem *horizontalSpacer_7;
    QSpacerItem *verticalSpacer_7;
    QSpacerItem *verticalSpacer_6;
    QSpacerItem *horizontalSpacer_8;
    QWidget *buttonsTab;
    QVBoxLayout *verticalLayout_2;

    void setupUi(QWidget *ControllerConfig)
    {
        if (ControllerConfig->objectName().isEmpty())
            ControllerConfig->setObjectName(QString::fromUtf8("ControllerConfig"));
        ControllerConfig->resize(503, 293);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ControllerConfig->sizePolicy().hasHeightForWidth());
        ControllerConfig->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(ControllerConfig);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setSizeConstraint(QLayout::SetFixedSize);
        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        activeControllerCB = new QComboBox(ControllerConfig);
        activeControllerCB->setObjectName(QString::fromUtf8("activeControllerCB"));

        gridLayout->addWidget(activeControllerCB, 1, 1, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 2, 1, 1);

        checkBox = new QCheckBox(ControllerConfig);
        checkBox->setObjectName(QString::fromUtf8("checkBox"));

        gridLayout->addWidget(checkBox, 1, 2, 1, 1);

        inputSourceCB = new QComboBox(ControllerConfig);
        inputSourceCB->setObjectName(QString::fromUtf8("inputSourceCB"));

        gridLayout->addWidget(inputSourceCB, 0, 1, 1, 1);

        label_2 = new QLabel(ControllerConfig);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        label = new QLabel(ControllerConfig);
        label->setObjectName(QString::fromUtf8("label"));

        gridLayout->addWidget(label, 0, 0, 1, 1);


        verticalLayout->addLayout(gridLayout);

        tabWidget = new QTabWidget(ControllerConfig);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        mainStickTab = new QWidget();
        mainStickTab->setObjectName(QString::fromUtf8("mainStickTab"));
        gridLayout_3 = new QGridLayout(mainStickTab);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_3->addItem(verticalSpacer_2, 2, 2, 1, 1);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_3->addItem(verticalSpacer_3, 0, 2, 1, 1);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_4, 1, 0, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_3, 1, 4, 1, 1);

        tabWidget->addTab(mainStickTab, QString());
        cStickTab = new QWidget();
        cStickTab->setObjectName(QString::fromUtf8("cStickTab"));
        gridLayout_4 = new QGridLayout(cStickTab);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        horizontalSpacer_6 = new QSpacerItem(40, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_6, 1, 0, 1, 1);

        verticalSpacer_5 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_4->addItem(verticalSpacer_5, 0, 1, 1, 1);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_4->addItem(verticalSpacer_4, 2, 1, 1, 1);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_5, 1, 2, 1, 1);

        tabWidget->addTab(cStickTab, QString());
        dPadTab = new QWidget();
        dPadTab->setObjectName(QString::fromUtf8("dPadTab"));
        gridLayout_5 = new QGridLayout(dPadTab);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_5->addItem(horizontalSpacer_7, 1, 2, 1, 1);

        verticalSpacer_7 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_5->addItem(verticalSpacer_7, 0, 1, 1, 1);

        verticalSpacer_6 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_5->addItem(verticalSpacer_6, 2, 1, 1, 1);

        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_5->addItem(horizontalSpacer_8, 1, 0, 1, 1);

        tabWidget->addTab(dPadTab, QString());
        buttonsTab = new QWidget();
        buttonsTab->setObjectName(QString::fromUtf8("buttonsTab"));
        verticalLayout_2 = new QVBoxLayout(buttonsTab);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        tabWidget->addTab(buttonsTab, QString());

        verticalLayout->addWidget(tabWidget);


        retranslateUi(ControllerConfig);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(ControllerConfig);
    } // setupUi

    void retranslateUi(QWidget *ControllerConfig)
    {
        ControllerConfig->setWindowTitle(QApplication::translate("ControllerConfig", "Controller Configuration", 0, QApplication::UnicodeUTF8));
        activeControllerCB->clear();
        activeControllerCB->insertItems(0, QStringList()
         << QApplication::translate("ControllerConfig", "Controller 1", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("ControllerConfig", "Controller 2", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("ControllerConfig", "Controller 3", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("ControllerConfig", "Controller 4", 0, QApplication::UnicodeUTF8)
        );
        checkBox->setText(QApplication::translate("ControllerConfig", "Enabled", 0, QApplication::UnicodeUTF8));
        inputSourceCB->clear();
        inputSourceCB->insertItems(0, QStringList()
         << QApplication::translate("ControllerConfig", "Keyboard", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("ControllerConfig", "Joypad", 0, QApplication::UnicodeUTF8)
        );
        label_2->setText(QApplication::translate("ControllerConfig", "Active Controller:", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("ControllerConfig", "Input Source:", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(mainStickTab), QApplication::translate("ControllerConfig", "Main Stick", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(cStickTab), QApplication::translate("ControllerConfig", "C-Stick", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(dPadTab), QApplication::translate("ControllerConfig", "D-Pad", 0, QApplication::UnicodeUTF8));
        tabWidget->setTabText(tabWidget->indexOf(buttonsTab), QApplication::translate("ControllerConfig", "Buttons", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ControllerConfig: public Ui_ControllerConfig {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONTROLLER_CONFIG_H
