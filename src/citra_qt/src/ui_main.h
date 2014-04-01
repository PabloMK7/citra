/********************************************************************************
** Form generated from reading UI file 'main.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *action_load_elf;
    QAction *action_Exit;
    QAction *action_Start;
    QAction *action_Stop;
    QAction *action_Pause;
    QAction *action_About;
    QAction *action_Single_Window_Mode;
    QAction *action_Hotkeys;
    QAction *action_Configure;
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QMenuBar *menubar;
    QMenu *menu_File;
    QMenu *menu_Emulation;
    QMenu *menu_View;
    QMenu *menu_Help;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1081, 730);
        QIcon icon;
        icon.addFile(QString::fromUtf8("src/pcafe/res/icon3_64x64.ico"), QSize(), QIcon::Normal, QIcon::Off);
        MainWindow->setWindowIcon(icon);
        MainWindow->setTabShape(QTabWidget::Rounded);
        MainWindow->setDockNestingEnabled(true);
        action_load_elf = new QAction(MainWindow);
        action_load_elf->setObjectName(QString::fromUtf8("action_load_elf"));
        action_Exit = new QAction(MainWindow);
        action_Exit->setObjectName(QString::fromUtf8("action_Exit"));
        action_Start = new QAction(MainWindow);
        action_Start->setObjectName(QString::fromUtf8("action_Start"));
        action_Stop = new QAction(MainWindow);
        action_Stop->setObjectName(QString::fromUtf8("action_Stop"));
        action_Stop->setEnabled(false);
        action_Pause = new QAction(MainWindow);
        action_Pause->setObjectName(QString::fromUtf8("action_Pause"));
        action_Pause->setEnabled(false);
        action_Pause->setVisible(false);
        action_About = new QAction(MainWindow);
        action_About->setObjectName(QString::fromUtf8("action_About"));
        action_Single_Window_Mode = new QAction(MainWindow);
        action_Single_Window_Mode->setObjectName(QString::fromUtf8("action_Single_Window_Mode"));
        action_Single_Window_Mode->setCheckable(true);
        action_Hotkeys = new QAction(MainWindow);
        action_Hotkeys->setObjectName(QString::fromUtf8("action_Hotkeys"));
        action_Configure = new QAction(MainWindow);
        action_Configure->setObjectName(QString::fromUtf8("action_Configure"));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 1081, 20));
        menu_File = new QMenu(menubar);
        menu_File->setObjectName(QString::fromUtf8("menu_File"));
        menu_Emulation = new QMenu(menubar);
        menu_Emulation->setObjectName(QString::fromUtf8("menu_Emulation"));
        menu_View = new QMenu(menubar);
        menu_View->setObjectName(QString::fromUtf8("menu_View"));
        menu_Help = new QMenu(menubar);
        menu_Help->setObjectName(QString::fromUtf8("menu_Help"));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menu_File->menuAction());
        menubar->addAction(menu_Emulation->menuAction());
        menubar->addAction(menu_View->menuAction());
        menubar->addAction(menu_Help->menuAction());
        menu_File->addAction(action_load_elf);
        menu_File->addSeparator();
        menu_File->addAction(action_Exit);
        menu_Emulation->addAction(action_Start);
        menu_Emulation->addAction(action_Pause);
        menu_Emulation->addAction(action_Stop);
        menu_Emulation->addSeparator();
        menu_Emulation->addAction(action_Configure);
        menu_View->addAction(action_Single_Window_Mode);
        menu_View->addAction(action_Hotkeys);
        menu_Help->addAction(action_About);

        retranslateUi(MainWindow);
        QObject::connect(action_Exit, SIGNAL(triggered()), MainWindow, SLOT(close()));
        QObject::connect(action_Configure, SIGNAL(triggered()), MainWindow, SLOT(OnConfigure()));

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "Citra", 0, QApplication::UnicodeUTF8));
        action_load_elf->setText(QApplication::translate("MainWindow", "Load ELF ...", 0, QApplication::UnicodeUTF8));
        action_Exit->setText(QApplication::translate("MainWindow", "E&xit", 0, QApplication::UnicodeUTF8));
        action_Start->setText(QApplication::translate("MainWindow", "&Start", 0, QApplication::UnicodeUTF8));
        action_Stop->setText(QApplication::translate("MainWindow", "&Stop", 0, QApplication::UnicodeUTF8));
        action_Pause->setText(QApplication::translate("MainWindow", "&Pause", 0, QApplication::UnicodeUTF8));
        action_About->setText(QApplication::translate("MainWindow", "About Citra", 0, QApplication::UnicodeUTF8));
        action_Single_Window_Mode->setText(QApplication::translate("MainWindow", "Single Window Mode", 0, QApplication::UnicodeUTF8));
        action_Hotkeys->setText(QApplication::translate("MainWindow", "Configure &Hotkeys ...", 0, QApplication::UnicodeUTF8));
        action_Configure->setText(QApplication::translate("MainWindow", "Configure ...", 0, QApplication::UnicodeUTF8));
        menu_File->setTitle(QApplication::translate("MainWindow", "&File", 0, QApplication::UnicodeUTF8));
        menu_Emulation->setTitle(QApplication::translate("MainWindow", "&Emulation", 0, QApplication::UnicodeUTF8));
        menu_View->setTitle(QApplication::translate("MainWindow", "&View", 0, QApplication::UnicodeUTF8));
        menu_Help->setTitle(QApplication::translate("MainWindow", "&Help", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAIN_H
