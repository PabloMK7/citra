/********************************************************************************
** Form generated from reading UI file 'cpu_regs.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CPU_REGS_H
#define UI_CPU_REGS_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDockWidget>
#include <QtGui/QHeaderView>
#include <QtGui/QTreeWidget>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ARMRegisters
{
public:
    QWidget *dockWidgetContents;
    QVBoxLayout *verticalLayout;
    QTreeWidget *treeWidget;

    void setupUi(QDockWidget *ARMRegisters)
    {
        if (ARMRegisters->objectName().isEmpty())
            ARMRegisters->setObjectName(QString::fromUtf8("ARMRegisters"));
        ARMRegisters->resize(400, 300);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QString::fromUtf8("dockWidgetContents"));
        verticalLayout = new QVBoxLayout(dockWidgetContents);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        treeWidget = new QTreeWidget(dockWidgetContents);
        treeWidget->setObjectName(QString::fromUtf8("treeWidget"));
        treeWidget->setAlternatingRowColors(true);

        verticalLayout->addWidget(treeWidget);

        ARMRegisters->setWidget(dockWidgetContents);

        retranslateUi(ARMRegisters);

        QMetaObject::connectSlotsByName(ARMRegisters);
    } // setupUi

    void retranslateUi(QDockWidget *ARMRegisters)
    {
        ARMRegisters->setWindowTitle(QApplication::translate("ARMRegisters", "ARM registers", 0, QApplication::UnicodeUTF8));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(1, QApplication::translate("ARMRegisters", "Value", 0, QApplication::UnicodeUTF8));
        ___qtreewidgetitem->setText(0, QApplication::translate("ARMRegisters", "Register", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ARMRegisters: public Ui_ARMRegisters {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CPU_REGS_H
