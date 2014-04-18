#include "qhexedit.h"

class GRamView : public QHexEdit
{
    Q_OBJECT

public:
    GRamView(QWidget* parent = NULL);

public slots:
    void OnCPUStepped();
};
