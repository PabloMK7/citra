/********************************************************************************
** Form generated from reading UI file 'image_info.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_IMAGE_INFO_H
#define UI_IMAGE_INFO_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDockWidget>
#include <QtGui/QFormLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ImageInfo
{
public:
    QWidget *dockWidgetContents;
    QFormLayout *formLayout;
    QLabel *label_name;
    QLabel *label_gameid;
    QLabel *label_country;
    QLabel *label_bannertext;
    QLineEdit *line_name;
    QLineEdit *line_gameid;
    QLineEdit *line_country;
    QLabel *label_banner;
    QLabel *label_developer;
    QLineEdit *line_developer;
    QLabel *label_description;
    QPlainTextEdit *edit_description;

    void setupUi(QDockWidget *ImageInfo)
    {
        if (ImageInfo->objectName().isEmpty())
            ImageInfo->setObjectName(QString::fromUtf8("ImageInfo"));
        ImageInfo->resize(400, 300);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QString::fromUtf8("dockWidgetContents"));
        formLayout = new QFormLayout(dockWidgetContents);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label_name = new QLabel(dockWidgetContents);
        label_name->setObjectName(QString::fromUtf8("label_name"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_name);

        label_gameid = new QLabel(dockWidgetContents);
        label_gameid->setObjectName(QString::fromUtf8("label_gameid"));

        formLayout->setWidget(4, QFormLayout::LabelRole, label_gameid);

        label_country = new QLabel(dockWidgetContents);
        label_country->setObjectName(QString::fromUtf8("label_country"));

        formLayout->setWidget(5, QFormLayout::LabelRole, label_country);

        label_bannertext = new QLabel(dockWidgetContents);
        label_bannertext->setObjectName(QString::fromUtf8("label_bannertext"));

        formLayout->setWidget(9, QFormLayout::LabelRole, label_bannertext);

        line_name = new QLineEdit(dockWidgetContents);
        line_name->setObjectName(QString::fromUtf8("line_name"));
        line_name->setEnabled(true);
        line_name->setReadOnly(true);

        formLayout->setWidget(1, QFormLayout::FieldRole, line_name);

        line_gameid = new QLineEdit(dockWidgetContents);
        line_gameid->setObjectName(QString::fromUtf8("line_gameid"));
        line_gameid->setEnabled(true);
        line_gameid->setReadOnly(true);

        formLayout->setWidget(4, QFormLayout::FieldRole, line_gameid);

        line_country = new QLineEdit(dockWidgetContents);
        line_country->setObjectName(QString::fromUtf8("line_country"));
        line_country->setEnabled(true);
        line_country->setReadOnly(true);

        formLayout->setWidget(5, QFormLayout::FieldRole, line_country);

        label_banner = new QLabel(dockWidgetContents);
        label_banner->setObjectName(QString::fromUtf8("label_banner"));
        label_banner->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(9, QFormLayout::FieldRole, label_banner);

        label_developer = new QLabel(dockWidgetContents);
        label_developer->setObjectName(QString::fromUtf8("label_developer"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label_developer);

        line_developer = new QLineEdit(dockWidgetContents);
        line_developer->setObjectName(QString::fromUtf8("line_developer"));
        line_developer->setReadOnly(true);

        formLayout->setWidget(3, QFormLayout::FieldRole, line_developer);

        label_description = new QLabel(dockWidgetContents);
        label_description->setObjectName(QString::fromUtf8("label_description"));

        formLayout->setWidget(7, QFormLayout::LabelRole, label_description);

        edit_description = new QPlainTextEdit(dockWidgetContents);
        edit_description->setObjectName(QString::fromUtf8("edit_description"));
        edit_description->setMaximumSize(QSize(16777215, 80));
        edit_description->setReadOnly(true);
        edit_description->setPlainText(QString::fromUtf8(""));

        formLayout->setWidget(7, QFormLayout::FieldRole, edit_description);

        ImageInfo->setWidget(dockWidgetContents);

        retranslateUi(ImageInfo);

        QMetaObject::connectSlotsByName(ImageInfo);
    } // setupUi

    void retranslateUi(QDockWidget *ImageInfo)
    {
        ImageInfo->setWindowTitle(QApplication::translate("ImageInfo", "Image Info", 0, QApplication::UnicodeUTF8));
        label_name->setText(QApplication::translate("ImageInfo", "Name", 0, QApplication::UnicodeUTF8));
        label_gameid->setText(QApplication::translate("ImageInfo", "Game ID", 0, QApplication::UnicodeUTF8));
        label_country->setText(QApplication::translate("ImageInfo", "Country", 0, QApplication::UnicodeUTF8));
        label_bannertext->setText(QApplication::translate("ImageInfo", "Banner", 0, QApplication::UnicodeUTF8));
        line_name->setText(QString());
        line_gameid->setText(QString());
        line_country->setText(QApplication::translate("ImageInfo", "EUROPE", 0, QApplication::UnicodeUTF8));
        label_banner->setText(QString());
        label_developer->setText(QApplication::translate("ImageInfo", "Developer", 0, QApplication::UnicodeUTF8));
        label_description->setText(QApplication::translate("ImageInfo", "Description", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ImageInfo: public Ui_ImageInfo {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_IMAGE_INFO_H
