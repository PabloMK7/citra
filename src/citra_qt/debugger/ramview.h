// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "qhexedit.h"

class GRamView : public QHexEdit {
    Q_OBJECT

public:
    GRamView(QWidget* parent = nullptr);

public slots:
    void OnCPUStepped();
};
