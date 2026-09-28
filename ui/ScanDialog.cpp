#include "ScanDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

ScanDialog::ScanDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Select Drive & Scan Type");
    setMinimumWidth(460);
    setupUI();
    populateDrives();
}

void ScanDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // --- Drive selection ---
    auto* driveGroup  = new QGroupBox("Select Drive", this);
    auto* driveLayout = new QVBoxLayout(driveGroup);

    driveCombo     = new QComboBox(this);
    driveInfoLabel = new QLabel("", this);
    driveInfoLabel->setStyleSheet("color: gray; font-size: 11px;");
    driveLayout->addWidget(driveCombo);
    driveLayout->addWidget(driveInfoLabel);
    connect(driveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScanDialog::onDriveChanged);

    // Custom path row
    customPathCheck = new QCheckBox("Use custom disk path", this);
    auto* customRow = new QHBoxLayout();
    customPathEdit  = new QLineEdit(this);
    customPathEdit->setPlaceholderText("e.g.  \\\\.\\PhysicalDrive0  or  \\\\.\\E:");
    customPathEdit->setEnabled(false);
    browseBtn = new QPushButton("Browse…", this);
    browseBtn->setEnabled(false);
    customRow->addWidget(customPathEdit);
    customRow->addWidget(browseBtn);

    driveLayout->addWidget(customPathCheck);
    driveLayout->addLayout(customRow);

    connect(customPathCheck, &QCheckBox::toggled, this, &ScanDialog::onCustomPathToggled);
    connect(browseBtn,       &QPushButton::clicked, this, &ScanDialog::onBrowseCustomPath);
    connect(customPathEdit,  &QLineEdit::textChanged, this, [this](){ updateInfoLabel(); });

    // --- Scan method ---
    auto* scanGroup  = new QGroupBox("Scan Method", this);
    auto* scanLayout = new QVBoxLayout(scanGroup);
    radioMFT     = new QRadioButton("Quick Scan (MFT/FAT metadata)", this);
    radioCarving = new QRadioButton("Deep Scan (File Carving)", this);
    radioBoth    = new QRadioButton("Full Scan (Both — Recommended)", this);
    radioBoth->setChecked(true);
    scanLayout->addWidget(radioMFT);
    scanLayout->addWidget(radioCarving);
    scanLayout->addWidget(radioBoth);

    // --- Buttons ---
    auto* btnLayout = new QHBoxLayout();
    scanBtn   = new QPushButton("Start Scan", this);
    cancelBtn = new QPushButton("Cancel", this);
    scanBtn->setDefault(true);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(scanBtn);

    connect(scanBtn,   &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addWidget(driveGroup);
    mainLayout->addWidget(scanGroup);
    mainLayout->addLayout(btnLayout);
}

void ScanDialog::populateDrives() {
    drives = DiskScanner::listDrives();
    driveCombo->clear();
    for (const auto& d : drives) {
        QString label = QString::fromStdString(d.driveLetter);
        if (!d.label.empty()) label += " [" + QString::fromStdString(d.label) + "]";
        label += d.isRemovable ? " (Removable)" : " (Fixed)";
        double gb = d.totalSize / (1024.0 * 1024.0 * 1024.0);
        label += QString(" — %1 GB").arg(gb, 0, 'f', 1);
        driveCombo->addItem(label);
    }
    if (!drives.empty()) onDriveChanged(0);
}

void ScanDialog::onDriveChanged(int index) {
    if (customPathCheck->isChecked()) return;
    if (index < 0 || index >= static_cast<int>(drives.size())) return;
    updateInfoLabel();
}

void ScanDialog::onCustomPathToggled(bool checked) {
    driveCombo->setEnabled(!checked);
    customPathEdit->setEnabled(checked);
    browseBtn->setEnabled(checked);
    updateInfoLabel();
}

void ScanDialog::onBrowseCustomPath() {
    // Let the user pick a drive letter folder; we convert it to a raw path.
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Drive Root (e.g. D:\\)", "This PC");
    if (dir.isEmpty()) return;

    // Convert  "D:/"  →  "\\.\D:"
    QString letter = dir.left(2); // "D:"
    if (letter.length() == 2 && letter[1] == ':')
        customPathEdit->setText("\\\\.\\" + letter);
    else
        customPathEdit->setText(dir);
}

void ScanDialog::updateInfoLabel() {
    if (customPathCheck->isChecked()) {
        QString path = customPathEdit->text().trimmed();
        driveInfoLabel->setText(path.isEmpty()
            ? "Enter a raw disk path above."
            : "Custom path: " + path);
        return;
    }
    int index = driveCombo->currentIndex();
    if (index < 0 || index >= static_cast<int>(drives.size())) return;
    const auto& d = drives[index];
    QString fs;
    switch (d.fsType) {
        case FilesystemType::NTFS:  fs = "NTFS";    break;
        case FilesystemType::FAT32: fs = "FAT32";   break;
        case FilesystemType::EXFAT: fs = "exFAT";   break;
        default:                    fs = "Unknown";  break;
    }
    driveInfoLabel->setText("Filesystem: " + fs + "  |  Path: " +
                             QString::fromStdString(d.rawPath));
}

DriveInfo ScanDialog::selectedDrive() const {
    if (customPathCheck->isChecked()) {
        QString path = customPathEdit->text().trimmed();
        if (path.isEmpty()) return {};
        DriveInfo custom;
        custom.rawPath    = path.toStdString();
        custom.driveLetter = path.toStdString();
        // Detect filesystem if the path looks like a drive letter (e.g. "\\.\D:")
        if (path.length() >= 4 && path[path.length()-2] == ':') {
            QString root = QString(path[path.length()-1]) + ":\\\\"; // not needed
            // Extract just the drive letter root for detection
            std::string root2 = std::string(1, path.toStdString()[path.length()-2]) + ":\\";
            custom.fsType = DiskScanner::detectFilesystem(root2);
        } else {
            custom.fsType = FilesystemType::UNKNOWN;
        }
        return custom;
    }
    int idx = driveCombo->currentIndex();
    if (idx >= 0 && idx < static_cast<int>(drives.size()))
        return drives[idx];
    return {};
}

bool ScanDialog::doMFTScan()     const { return radioMFT->isChecked()     || radioBoth->isChecked(); }
bool ScanDialog::doCarvingScan() const { return radioCarving->isChecked() || radioBoth->isChecked(); }
