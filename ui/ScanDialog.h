#pragma once
#include "../core/DiskScanner.h"
#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>

class ScanDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScanDialog(QWidget* parent = nullptr);

    DriveInfo selectedDrive() const;
    bool doMFTScan()     const;
    bool doCarvingScan() const;

private slots:
    void onDriveChanged(int index);
    void onCustomPathToggled(bool checked);
    void onBrowseCustomPath();

private:
    void setupUI();
    void populateDrives();
    void updateInfoLabel();

    QComboBox*    driveCombo;
    QCheckBox*    customPathCheck;
    QLineEdit*    customPathEdit;
    QPushButton*  browseBtn;
    QRadioButton* radioMFT;
    QRadioButton* radioCarving;
    QRadioButton* radioBoth;
    QLabel*       driveInfoLabel;
    QPushButton*  scanBtn;
    QPushButton*  cancelBtn;

    std::vector<DriveInfo> drives;
};
