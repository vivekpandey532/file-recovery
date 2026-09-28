#include "MainWindow.h"
#include "ScanDialog.h"
#include "../core/NTFSParser.h"
#include "../core/FATParser.h"
#include "../core/FileCarver.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("File Recovery Tool");
    setMinimumSize(900, 600);
    setupUI();
    setupMenuBar();
}

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 4);

    resultsView = new ResultsView(this);
    layout->addWidget(resultsView);

    setCentralWidget(central);

    // Status bar
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    progressBar->setMaximumWidth(200);
    statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(statusLabel);
    statusBar()->addPermanentWidget(progressBar);
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");
    newScanAction  = fileMenu->addAction("&New Scan", this, &MainWindow::onNewScan);
    newScanAction->setShortcut(QKeySequence::New);
    fileMenu->addSeparator();
    fileMenu->addAction("&Exit", this, &QWidget::close);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::onAbout);

    // Toolbar
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->addAction(newScanAction);
}

void MainWindow::onNewScan() {
    ScanDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    DriveInfo drive = dlg.selectedDrive();
    if (drive.rawPath.empty()) {
        QMessageBox::warning(this, "No Drive", "Please select a valid drive.");
        return;
    }

    resultsView->clearResults();
    runScan(drive, dlg.doMFTScan(), dlg.doCarvingScan());
}

void MainWindow::runScan(const DriveInfo& drive, bool doMFT, bool doCarving) {
    newScanAction->setEnabled(false);
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // indeterminate
    statusLabel->setText("Scanning " + QString::fromStdString(drive.driveLetter) + "...");

    struct ScanResult { std::vector<RecoveredFile> files; };

    auto* watcher = new QFutureWatcher<ScanResult>(this);

    connect(watcher, &QFutureWatcher<ScanResult>::finished, this,
        [this, watcher, drive]() {
            auto result = watcher->result();
            resultsView->setResults(result.files, drive.rawPath);
            progressBar->setVisible(false);
            progressBar->setRange(0, 100);
            statusLabel->setText(QString("Scan complete. Found %1 deleted file(s).")
                                     .arg(result.files.size()));
            newScanAction->setEnabled(true);
            watcher->deleteLater();
        });

    QString rawPath = QString::fromStdString(drive.rawPath);
    FilesystemType fsType = drive.fsType;

    QFuture<ScanResult> future = QtConcurrent::run([rawPath, fsType, doMFT, doCarving]() -> ScanResult {
        ScanResult result;
        HANDLE hDrive = DiskScanner::openDrive(rawPath.toStdString());
        if (hDrive == INVALID_HANDLE_VALUE) return result;

        if (doMFT) {
            if (fsType == FilesystemType::NTFS) {
                NTFSParser parser;
                auto files = parser.scanDeletedFiles(hDrive);
                result.files.insert(result.files.end(), files.begin(), files.end());
            } else if (fsType == FilesystemType::FAT32 || fsType == FilesystemType::EXFAT) {
                FATParser parser;
                auto files = parser.scanDeletedFiles(hDrive);
                result.files.insert(result.files.end(), files.begin(), files.end());
            }
        }

        if (doCarving) {
            uint64_t driveSize = DiskScanner::getDriveSize(hDrive);
            FileCarver carver;
            auto files = carver.carve(hDrive, driveSize);
            result.files.insert(result.files.end(), files.begin(), files.end());
        }

        CloseHandle(hDrive);
        return result;
    });

    watcher->setFuture(future);
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About File Recovery Tool",
        "<b>File Recovery Tool v1.0</b><br><br>"
        "Recovers permanently deleted files from NTFS, FAT32, and exFAT drives.<br><br>"
        "Supports MFT/FAT metadata scanning and raw file carving.<br><br>"
        "<i>Run as Administrator for full disk access.</i>");
}
