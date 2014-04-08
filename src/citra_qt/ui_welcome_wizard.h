/********************************************************************************
** Form generated from reading UI file 'welcome_wizard.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WELCOME_WIZARD_H
#define UI_WELCOME_WIZARD_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLineEdit>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWizard>
#include <QtGui/QWizardPage>
#include "path_list.hxx"

QT_BEGIN_NAMESPACE

class Ui_WelcomeWizard
{
public:
    QWizardPage *wizardPage1;
    QWizardPage *wizardPage2;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QLineEdit *edit_path;
    QPushButton *button_browse_path;
    QHBoxLayout *horizontalLayout_2;
    GPathList *path_list;
    QVBoxLayout *verticalLayout_3;
    QPushButton *button_add_path;
    QSpacerItem *verticalSpacer;

    void setupUi(QWizard *WelcomeWizard)
    {
        if (WelcomeWizard->objectName().isEmpty())
            WelcomeWizard->setObjectName(QString::fromUtf8("WelcomeWizard"));
        WelcomeWizard->resize(510, 300);
        WelcomeWizard->setModal(true);
        wizardPage1 = new QWizardPage();
        wizardPage1->setObjectName(QString::fromUtf8("wizardPage1"));
        WelcomeWizard->addPage(wizardPage1);
        wizardPage2 = new QWizardPage();
        wizardPage2->setObjectName(QString::fromUtf8("wizardPage2"));
        verticalLayout_2 = new QVBoxLayout(wizardPage2);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        edit_path = new QLineEdit(wizardPage2);
        edit_path->setObjectName(QString::fromUtf8("edit_path"));
        edit_path->setReadOnly(true);

        horizontalLayout->addWidget(edit_path);

        button_browse_path = new QPushButton(wizardPage2);
        button_browse_path->setObjectName(QString::fromUtf8("button_browse_path"));

        horizontalLayout->addWidget(button_browse_path);


        verticalLayout_2->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        path_list = new GPathList(wizardPage2);
        path_list->setObjectName(QString::fromUtf8("path_list"));

        horizontalLayout_2->addWidget(path_list);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        button_add_path = new QPushButton(wizardPage2);
        button_add_path->setObjectName(QString::fromUtf8("button_add_path"));

        verticalLayout_3->addWidget(button_add_path);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);


        horizontalLayout_2->addLayout(verticalLayout_3);


        verticalLayout_2->addLayout(horizontalLayout_2);

        WelcomeWizard->addPage(wizardPage2);

        retranslateUi(WelcomeWizard);

        QMetaObject::connectSlotsByName(WelcomeWizard);
    } // setupUi

    void retranslateUi(QWizard *WelcomeWizard)
    {
        WelcomeWizard->setWindowTitle(QApplication::translate("WelcomeWizard", "Welcome", 0, QApplication::UnicodeUTF8));
        wizardPage1->setTitle(QApplication::translate("WelcomeWizard", "Welcome", 0, QApplication::UnicodeUTF8));
        wizardPage1->setSubTitle(QApplication::translate("WelcomeWizard", "This wizard will guide you through the initial configuration.", 0, QApplication::UnicodeUTF8));
        wizardPage2->setTitle(QApplication::translate("WelcomeWizard", "ISO paths", 0, QApplication::UnicodeUTF8));
        wizardPage2->setSubTitle(QApplication::translate("WelcomeWizard", "If you have a collection of game images, you can add them to the path list here. Gekko will automatically show a list of your collection on startup then.", 0, QApplication::UnicodeUTF8));
        edit_path->setText(QString());
        edit_path->setPlaceholderText(QApplication::translate("WelcomeWizard", "Select a path to add ...", 0, QApplication::UnicodeUTF8));
        button_browse_path->setText(QString());
        button_add_path->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class WelcomeWizard: public Ui_WelcomeWizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WELCOME_WIZARD_H
