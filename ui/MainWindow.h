#pragma once
#include "ResultsView.h"
#include "../core/DiskScanner.h"
#include <QMainWindow>
#include <QProgressBar>
#include <QLabel>
#include <QAction>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNewScan();
    void onAbout();

private:
    void setupUI();
    void setupMenuBar();
    void runScan(const DriveInfo& drive, bool doMFT, bool doCarving);

    ResultsView*  resultsView;
    QProgressBar* progressBar;
    QLabel*       statusLabel;
    QAction*      newScanAction;
};
