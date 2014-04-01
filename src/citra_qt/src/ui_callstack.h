/********************************************************************************
** Form generated from reading UI file 'callstack.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CALLSTACK_H
#define UI_CALLSTACK_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDockWidget>
#include <QtGui/QHeaderView>
#include <QtGui/QTreeView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CallStack
{
public:
    QWidget *dockWidgetContents;
    QVBoxLayout *verticalLayout;
    QTreeView *treeView;

    void setupUi(QDockWidget *CallStack)
    {
        if (CallStack->objectName().isEmpty())
            CallStack->setObjectName(QString::fromUtf8("CallStack"));
        CallStack->resize(400, 300);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QString::fromUtf8("dockWidgetContents"));
        verticalLayout = new QVBoxLayout(dockWidgetContents);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        treeView = new QTreeView(dockWidgetContents);
        treeView->setObjectName(QString::fromUtf8("treeView"));
        treeView->setAlternatingRowColors(true);
        treeView->setRootIsDecorated(false);
        treeView->setItemsExpandable(false);

        verticalLayout->addWidget(treeView);

        CallStack->setWidget(dockWidgetContents);

        retranslateUi(CallStack);

        QMetaObject::connectSlotsByName(CallStack);
    } // setupUi

    void retranslateUi(QDockWidget *CallStack)
    {
        CallStack->setWindowTitle(QApplication::translate("CallStack", "Call stack", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class CallStack: public Ui_CallStack {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CALLSTACK_H
