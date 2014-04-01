/********************************************************************************
** Form generated from reading UI file 'hotkeys.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HOTKEYS_H
#define UI_HOTKEYS_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHeaderView>
#include <QtGui/QTreeWidget>
#include <QtGui/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_hotkeys
{
public:
    QVBoxLayout *verticalLayout;
    QTreeWidget *treeWidget;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *hotkeys)
    {
        if (hotkeys->objectName().isEmpty())
            hotkeys->setObjectName(QString::fromUtf8("hotkeys"));
        hotkeys->resize(363, 388);
        verticalLayout = new QVBoxLayout(hotkeys);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        treeWidget = new QTreeWidget(hotkeys);
        treeWidget->setObjectName(QString::fromUtf8("treeWidget"));
        treeWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
        treeWidget->setHeaderHidden(false);

        verticalLayout->addWidget(treeWidget);

        buttonBox = new QDialogButtonBox(hotkeys);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok|QDialogButtonBox::Reset);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(hotkeys);
        QObject::connect(buttonBox, SIGNAL(accepted()), hotkeys, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), hotkeys, SLOT(reject()));

        QMetaObject::connectSlotsByName(hotkeys);
    } // setupUi

    void retranslateUi(QDialog *hotkeys)
    {
        hotkeys->setWindowTitle(QApplication::translate("hotkeys", "Hotkey Settings", 0, QApplication::UnicodeUTF8));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(2, QApplication::translate("hotkeys", "Context", 0, QApplication::UnicodeUTF8));
        ___qtreewidgetitem->setText(1, QApplication::translate("hotkeys", "Hotkey", 0, QApplication::UnicodeUTF8));
        ___qtreewidgetitem->setText(0, QApplication::translate("hotkeys", "Action", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class hotkeys: public Ui_hotkeys {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HOTKEYS_H
