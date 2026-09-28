#pragma once
#include "../core/FileRecovery.h"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <vector>

class ResultsView : public QWidget {
    Q_OBJECT
public:
    explicit ResultsView(QWidget* parent = nullptr);
    void setResults(const std::vector<RecoveredFile>& files, const std::string& rawDrivePath);
    void clearResults();

private slots:
    void onRecoverSelected();
    void onFilterChanged(const QString& text);
    void onSelectAll();

private:
    void setupUI();
    void populateTable(const std::vector<RecoveredFile>& files);
    QString methodToString(RecoveryMethod m);

    QLineEdit*    filterEdit;
    QTableWidget* table;
    QPushButton*  recoverBtn;
    QPushButton*  selectAllBtn;
    QLabel*       statusLabel;

    std::vector<RecoveredFile> allFiles;
    std::string drivePath;
};
