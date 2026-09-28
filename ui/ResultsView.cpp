#include "ResultsView.h"
#include "../core/FileRecovery.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QApplication>

ResultsView::ResultsView(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void ResultsView::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Filter bar
    auto* filterLayout = new QHBoxLayout();
    filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText("Filter by filename or extension...");
    filterLayout->addWidget(new QLabel("Filter:", this));
    filterLayout->addWidget(filterEdit);
    connect(filterEdit, &QLineEdit::textChanged, this, &ResultsView::onFilterChanged);

    // Table
    table = new QTableWidget(this);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({"File Name", "Extension", "Size", "Deleted Date", "Method", "Recoverable"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSortingEnabled(true);
    table->setAlternatingRowColors(true);

    // Bottom bar
    auto* bottomLayout = new QHBoxLayout();
    statusLabel  = new QLabel("No scan performed yet.", this);
    selectAllBtn = new QPushButton("Select All", this);
    recoverBtn   = new QPushButton("Recover Selected", this);
    recoverBtn->setEnabled(false);
    recoverBtn->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; padding: 6px 16px; }");

    connect(selectAllBtn, &QPushButton::clicked, this, &ResultsView::onSelectAll);
    connect(recoverBtn,   &QPushButton::clicked, this, &ResultsView::onRecoverSelected);
    connect(table, &QTableWidget::itemSelectionChanged, this, [this]() {
        recoverBtn->setEnabled(!table->selectedItems().isEmpty());
    });

    bottomLayout->addWidget(statusLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(selectAllBtn);
    bottomLayout->addWidget(recoverBtn);

    layout->addLayout(filterLayout);
    layout->addWidget(table);
    layout->addLayout(bottomLayout);
}

void ResultsView::setResults(const std::vector<RecoveredFile>& files, const std::string& rawDrivePath) {
    allFiles  = files;
    drivePath = rawDrivePath;
    populateTable(files);
    statusLabel->setText(QString("Found %1 deleted file(s).").arg(files.size()));
}

void ResultsView::clearResults() {
    allFiles.clear();
    table->setRowCount(0);
    statusLabel->setText("No scan performed yet.");
}

void ResultsView::populateTable(const std::vector<RecoveredFile>& files) {
    table->setSortingEnabled(false);
    table->setRowCount(static_cast<int>(files.size()));

    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        const auto& f = files[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(f.fileName)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(f.extension)));

        QString sizeStr;
        if (f.fileSize >= 1024*1024)
            sizeStr = QString("%1 MB").arg(f.fileSize / (1024.0*1024.0), 0, 'f', 2);
        else if (f.fileSize >= 1024)
            sizeStr = QString("%1 KB").arg(f.fileSize / 1024.0, 0, 'f', 1);
        else
            sizeStr = QString("%1 B").arg(f.fileSize);

        table->setItem(i, 2, new QTableWidgetItem(sizeStr));
        table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(f.deletedDate)));
        table->setItem(i, 4, new QTableWidgetItem(methodToString(f.method)));
        table->setItem(i, 5, new QTableWidgetItem(f.isRecoverable ? "Yes" : "No"));

        // Store index in UserRole for lookup
        table->item(i, 0)->setData(Qt::UserRole, i);
        if (!f.isRecoverable)
            for (int c = 0; c < 6; ++c)
                if (table->item(i, c))
                    table->item(i, c)->setForeground(Qt::gray);
    }
    table->setSortingEnabled(true);
}

void ResultsView::onFilterChanged(const QString& text) {
    if (text.isEmpty()) {
        populateTable(allFiles);
        return;
    }
    std::vector<RecoveredFile> filtered;
    for (const auto& f : allFiles) {
        QString name = QString::fromStdString(f.fileName).toLower();
        QString ext  = QString::fromStdString(f.extension).toLower();
        if (name.contains(text.toLower()) || ext.contains(text.toLower()))
            filtered.push_back(f);
    }
    populateTable(filtered);
}

void ResultsView::onSelectAll() {
    table->selectAll();
}

void ResultsView::onRecoverSelected() {
    auto selected = table->selectedRows();
    if (table->selectedItems().isEmpty()) return;

    QString destDir = QFileDialog::getExistingDirectory(this, "Select Recovery Destination");
    if (destDir.isEmpty()) return;

    // Collect unique row indices
    QSet<int> rows;
    for (auto* item : table->selectedItems())
        rows.insert(item->row());

    QProgressDialog progress("Recovering files...", "Cancel", 0, rows.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int done = 0, failed = 0;
    for (int row : rows) {
        progress.setValue(done);
        if (progress.wasCanceled()) break;

        auto* nameItem = table->item(row, 0);
        if (!nameItem) continue;
        int fileIdx = nameItem->data(Qt::UserRole).toInt();
        if (fileIdx < 0 || fileIdx >= static_cast<int>(allFiles.size())) continue;

        const auto& file = allFiles[fileIdx];
        std::string dest = destDir.toStdString() + "\\" + file.fileName;

        if (!FileRecovery::recoverFile(drivePath, file, dest))
            ++failed;
        ++done;
    }

    progress.setValue(rows.size());
    QString msg = QString("Recovered %1 file(s).").arg(done - failed);
    if (failed > 0) msg += QString("\n%1 file(s) failed.").arg(failed);
    QMessageBox::information(this, "Recovery Complete", msg);
}

QString ResultsView::methodToString(RecoveryMethod m) {
    switch (m) {
        case RecoveryMethod::MFT:     return "MFT";
        case RecoveryMethod::FAT:     return "FAT";
        case RecoveryMethod::CARVING: return "Carving";
        default:                      return "Unknown";
    }
}
