// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

namespace Cheats {
class CheatBase;
}

namespace Ui {
class CheatDialog;
} // namespace Ui

class CheatDialog : public QDialog {
    Q_OBJECT

public:
    explicit CheatDialog(QWidget* parent = nullptr);
    ~CheatDialog();

private:
    /**
     * Loads the cheats from the CheatEngine, and populates the table.
     */
    void LoadCheats();

    /**
     * Pops up a message box asking if the user wants to save the current cheat.
     * If the user selected Yes, attempts to save the current cheat.
     * @return true if the user selected No, or if the cheat was saved successfully
     *         false if the user selected Cancel, or if the user selected Yes but saving failed
     */
    bool CheckSaveCheat();

    /**
     * Saves the current cheat as the row-th cheat in the cheat list.
     * @return true if the cheat is saved successfully, false otherwise
     */
    bool SaveCheat(int row);

    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnCancel();
    void OnRowSelected(int row, int column);
    void OnCheckChanged(int state);
    void OnTextEdited();
    void OnDeleteCheat();
    void OnAddCheat();

private:
    std::unique_ptr<Ui::CheatDialog> ui;
    std::vector<std::shared_ptr<Cheats::CheatBase>> cheats;
    bool edited = false, newly_created = false;
    int last_row = -1, last_col = -1;
};
