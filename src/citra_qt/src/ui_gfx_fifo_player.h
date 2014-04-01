/********************************************************************************
** Form generated from reading UI file 'gfx_fifo_player.ui'
**
** Created by: Qt User Interface Compiler version 4.8.5
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GFX_FIFO_PLAYER_H
#define UI_GFX_FIFO_PLAYER_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDockWidget>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QSpinBox>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_GfxFifoPlayerControl
{
public:
    QWidget *recordingGroup;
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_2;
    QRadioButton *stopManuallyButton;
    QHBoxLayout *horizontalLayout;
    QRadioButton *radioButton_2;
    QSpinBox *spinBox;
    QComboBox *comboBox;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *horizontalSpacer;
    QCheckBox *pauseWhenDoneCheckbox;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *startStopRecordingButton;
    QPushButton *saveRecordingButton;
    QGroupBox *playbackGroup;
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label;
    QComboBox *playbackSourceCombobox;
    QCheckBox *checkBox_2;
    QPushButton *startPlaybackButton;
    QSpacerItem *verticalSpacer;

    void setupUi(QDockWidget *GfxFifoPlayerControl)
    {
        if (GfxFifoPlayerControl->objectName().isEmpty())
            GfxFifoPlayerControl->setObjectName(QString::fromUtf8("GfxFifoPlayerControl"));
        GfxFifoPlayerControl->resize(275, 297);
        recordingGroup = new QWidget();
        recordingGroup->setObjectName(QString::fromUtf8("recordingGroup"));
        verticalLayout = new QVBoxLayout(recordingGroup);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        groupBox = new QGroupBox(recordingGroup);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        verticalLayout_2 = new QVBoxLayout(groupBox);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        stopManuallyButton = new QRadioButton(groupBox);
        stopManuallyButton->setObjectName(QString::fromUtf8("stopManuallyButton"));
        stopManuallyButton->setChecked(true);

        verticalLayout_2->addWidget(stopManuallyButton);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        radioButton_2 = new QRadioButton(groupBox);
        radioButton_2->setObjectName(QString::fromUtf8("radioButton_2"));

        horizontalLayout->addWidget(radioButton_2);

        spinBox = new QSpinBox(groupBox);
        spinBox->setObjectName(QString::fromUtf8("spinBox"));
        spinBox->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        spinBox->setMinimum(1);
        spinBox->setMaximum(99999);

        horizontalLayout->addWidget(spinBox);

        comboBox = new QComboBox(groupBox);
        comboBox->setObjectName(QString::fromUtf8("comboBox"));

        horizontalLayout->addWidget(comboBox);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout);

        pauseWhenDoneCheckbox = new QCheckBox(groupBox);
        pauseWhenDoneCheckbox->setObjectName(QString::fromUtf8("pauseWhenDoneCheckbox"));

        verticalLayout_2->addWidget(pauseWhenDoneCheckbox);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        startStopRecordingButton = new QPushButton(groupBox);
        startStopRecordingButton->setObjectName(QString::fromUtf8("startStopRecordingButton"));

        horizontalLayout_2->addWidget(startStopRecordingButton);

        saveRecordingButton = new QPushButton(groupBox);
        saveRecordingButton->setObjectName(QString::fromUtf8("saveRecordingButton"));
        saveRecordingButton->setEnabled(false);

        horizontalLayout_2->addWidget(saveRecordingButton);


        verticalLayout_2->addLayout(horizontalLayout_2);


        verticalLayout->addWidget(groupBox);

        playbackGroup = new QGroupBox(recordingGroup);
        playbackGroup->setObjectName(QString::fromUtf8("playbackGroup"));
        verticalLayout_3 = new QVBoxLayout(playbackGroup);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        label = new QLabel(playbackGroup);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout_3->addWidget(label);

        playbackSourceCombobox = new QComboBox(playbackGroup);
        playbackSourceCombobox->setObjectName(QString::fromUtf8("playbackSourceCombobox"));

        horizontalLayout_3->addWidget(playbackSourceCombobox);


        verticalLayout_3->addLayout(horizontalLayout_3);

        checkBox_2 = new QCheckBox(playbackGroup);
        checkBox_2->setObjectName(QString::fromUtf8("checkBox_2"));

        verticalLayout_3->addWidget(checkBox_2);

        startPlaybackButton = new QPushButton(playbackGroup);
        startPlaybackButton->setObjectName(QString::fromUtf8("startPlaybackButton"));

        verticalLayout_3->addWidget(startPlaybackButton);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);


        verticalLayout->addWidget(playbackGroup);

        GfxFifoPlayerControl->setWidget(recordingGroup);

        retranslateUi(GfxFifoPlayerControl);

        QMetaObject::connectSlotsByName(GfxFifoPlayerControl);
    } // setupUi

    void retranslateUi(QDockWidget *GfxFifoPlayerControl)
    {
        GfxFifoPlayerControl->setWindowTitle(QApplication::translate("GfxFifoPlayerControl", "Fifo Player", 0, QApplication::UnicodeUTF8));
        groupBox->setTitle(QApplication::translate("GfxFifoPlayerControl", "Recording", 0, QApplication::UnicodeUTF8));
        stopManuallyButton->setText(QApplication::translate("GfxFifoPlayerControl", "Stop manually", 0, QApplication::UnicodeUTF8));
        radioButton_2->setText(QApplication::translate("GfxFifoPlayerControl", "Stop after", 0, QApplication::UnicodeUTF8));
        comboBox->clear();
        comboBox->insertItems(0, QStringList()
         << QApplication::translate("GfxFifoPlayerControl", "Frames", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("GfxFifoPlayerControl", "Flushes", 0, QApplication::UnicodeUTF8)
        );
        pauseWhenDoneCheckbox->setText(QApplication::translate("GfxFifoPlayerControl", "Pause when done", 0, QApplication::UnicodeUTF8));
        startStopRecordingButton->setText(QApplication::translate("GfxFifoPlayerControl", "Start", 0, QApplication::UnicodeUTF8));
        saveRecordingButton->setText(QApplication::translate("GfxFifoPlayerControl", "Save to File...", 0, QApplication::UnicodeUTF8));
        playbackGroup->setTitle(QApplication::translate("GfxFifoPlayerControl", "Playback", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("GfxFifoPlayerControl", "Playback source:", 0, QApplication::UnicodeUTF8));
        playbackSourceCombobox->clear();
        playbackSourceCombobox->insertItems(0, QStringList()
         << QApplication::translate("GfxFifoPlayerControl", "Last Recording", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("GfxFifoPlayerControl", "From File...", 0, QApplication::UnicodeUTF8)
        );
        checkBox_2->setText(QApplication::translate("GfxFifoPlayerControl", "Loop", 0, QApplication::UnicodeUTF8));
        startPlaybackButton->setText(QApplication::translate("GfxFifoPlayerControl", "Start", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class GfxFifoPlayerControl: public Ui_GfxFifoPlayerControl {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GFX_FIFO_PLAYER_H
