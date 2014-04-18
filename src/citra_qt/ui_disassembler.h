/********************************************************************************
** Form generated from reading UI file 'disassembler.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DISASSEMBLER_H
#define UI_DISASSEMBLER_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDockWidget>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QPushButton>
#include <QtGui/QTreeView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DockWidget
{
public:
    QWidget *dockWidgetContents;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QPushButton *button_step;
    QPushButton *button_pause;
    QPushButton *button_continue;
    QPushButton *pushButton;
    QPushButton *button_breakpoint;
    QTreeView *treeView;

    void setupUi(QDockWidget *DockWidget)
    {
        if (DockWidget->objectName().isEmpty())
            DockWidget->setObjectName(QString::fromUtf8("DockWidget"));
        DockWidget->resize(430, 401);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QString::fromUtf8("dockWidgetContents"));
        verticalLayout = new QVBoxLayout(dockWidgetContents);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        button_step = new QPushButton(dockWidgetContents);
        button_step->setObjectName(QString::fromUtf8("button_step"));

        horizontalLayout->addWidget(button_step);

        button_pause = new QPushButton(dockWidgetContents);
        button_pause->setObjectName(QString::fromUtf8("button_pause"));

        horizontalLayout->addWidget(button_pause);

        button_continue = new QPushButton(dockWidgetContents);
        button_continue->setObjectName(QString::fromUtf8("button_continue"));

        horizontalLayout->addWidget(button_continue);

        pushButton = new QPushButton(dockWidgetContents);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));

        horizontalLayout->addWidget(pushButton);

        button_breakpoint = new QPushButton(dockWidgetContents);
        button_breakpoint->setObjectName(QString::fromUtf8("button_breakpoint"));

        horizontalLayout->addWidget(button_breakpoint);


        verticalLayout->addLayout(horizontalLayout);

        treeView = new QTreeView(dockWidgetContents);
        treeView->setObjectName(QString::fromUtf8("treeView"));
        treeView->setAlternatingRowColors(true);
        treeView->setIndentation(20);
        treeView->setRootIsDecorated(false);
        treeView->header()->setVisible(false);

        verticalLayout->addWidget(treeView);

        DockWidget->setWidget(dockWidgetContents);

        retranslateUi(DockWidget);

        QMetaObject::connectSlotsByName(DockWidget);
    } // setupUi

    void retranslateUi(QDockWidget *DockWidget)
    {
        DockWidget->setWindowTitle(QApplication::translate("DockWidget", "Disassembly", 0, QApplication::UnicodeUTF8));
        button_step->setText(QApplication::translate("DockWidget", "Step", 0, QApplication::UnicodeUTF8));
        button_pause->setText(QApplication::translate("DockWidget", "Pause", 0, QApplication::UnicodeUTF8));
        button_continue->setText(QApplication::translate("DockWidget", "Continue", 0, QApplication::UnicodeUTF8));
        pushButton->setText(QApplication::translate("DockWidget", "Step Into", 0, QApplication::UnicodeUTF8));
        button_breakpoint->setText(QApplication::translate("DockWidget", "Set Breakpoint", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class DockWidget: public Ui_DockWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DISASSEMBLER_H
