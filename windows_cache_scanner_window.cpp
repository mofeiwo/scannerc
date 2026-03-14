#include "windows_cache_scan_table_model.h"
#include "windows_cache_scanner_qt.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

int riskScore(const QString& riskLevel) {
  if (riskLevel.compare("high", Qt::CaseInsensitive) == 0) return 3;
  if (riskLevel.compare("medium", Qt::CaseInsensitive) == 0) return 2;
  return 1;
}

QString mergeRiskLevel(const QString& left, const QString& right) {
  return riskScore(left) >= riskScore(right) ? left : right;
}

CacheMatchItem scanSystemPath(const QString& appName,
                              const QString& category,
                              const QString& rootPath,
                              const QStringList& nameFilters = {},
                              bool recursive = true) {
  CacheMatchItem item;
  item.appName = appName;
  item.category = category;
  item.path = QDir::cleanPath(rootPath);
  item.riskLevel = QStringLiteral("low");
  item.deleteSafe = true;

  QDir root(rootPath);
  if (!root.exists()) return item;

  const QDateTime now = QDateTime::currentDateTime();
  const QDir::Filters filters = QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks;
  const QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
  QDirIterator it(rootPath, nameFilters, filters, flags);

  while (it.hasNext()) {
    it.next();
    const QFileInfo info = it.fileInfo();
    if (info.lastModified().secsTo(now) < 10 * 60) continue;

    item.bytes += info.size();
    item.files += 1;
  }

  return item;
}

QVector<CacheMatchItem> buildSystemCacheItems() {
  const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QString localAppData = env.value("LOCALAPPDATA");
  const QString tempPath = env.value("TEMP");
  const QString windowsDir = env.value("WINDIR", "C:/Windows");

  QVector<CacheMatchItem> items;

  const CacheMatchItem userTemp =
      scanSystemPath(QStringLiteral("System Temp"), QStringLiteral("system_temp"), tempPath);
  if (userTemp.files > 0) items.push_back(userTemp);

  const CacheMatchItem localTemp =
      scanSystemPath(QStringLiteral("Local Temp"), QStringLiteral("system_temp"), localAppData + "/Temp");
  if (localTemp.files > 0 && localTemp.path != userTemp.path) items.push_back(localTemp);

  const CacheMatchItem inetCache = scanSystemPath(QStringLiteral("Windows INetCache"),
                                                  QStringLiteral("system_cache"),
                                                  localAppData + "/Microsoft/Windows/INetCache");
  if (inetCache.files > 0) items.push_back(inetCache);

  const CacheMatchItem thumbCache = scanSystemPath(QStringLiteral("Thumbnail Cache"),
                                                   QStringLiteral("system_cache"),
                                                   localAppData + "/Microsoft/Windows/Explorer",
                                                   {"thumbcache*.db", "iconcache*.db"},
                                                   false);
  if (thumbCache.files > 0) items.push_back(thumbCache);

  const CacheMatchItem crashDumps = scanSystemPath(QStringLiteral("Crash Dumps"),
                                                   QStringLiteral("system_cache"),
                                                   localAppData + "/CrashDumps");
  if (crashDumps.files > 0) items.push_back(crashDumps);

  const CacheMatchItem windowsUpdate = scanSystemPath(QStringLiteral("Windows Update Cache"),
                                                      QStringLiteral("system_cache"),
                                                      windowsDir + "/SoftwareDistribution/Download");
  if (windowsUpdate.files > 0) items.push_back(windowsUpdate);

  return items;
}

QVector<CacheMatchItem> buildAggregatedItems(const QVector<CacheMatchItem>& rawItems) {
  QMap<QString, CacheMatchItem> grouped;
  QMap<QString, QStringList> pathBuckets;

  for (const CacheMatchItem& raw : rawItems) {
    CacheMatchItem& item = grouped[raw.appName];
    if (item.appName.isEmpty()) {
      item.appName = raw.appName;
      item.category = raw.category;
      item.path.clear();
      item.riskLevel = raw.riskLevel;
      item.deleteSafe = raw.deleteSafe;
    } else {
      item.riskLevel = mergeRiskLevel(item.riskLevel, raw.riskLevel);
      item.deleteSafe = item.deleteSafe && raw.deleteSafe;
    }

    item.bytes += raw.bytes;
    item.files += raw.files;
    pathBuckets[raw.appName].push_back(raw.path);
  }

  QVector<CacheMatchItem> result;
  for (auto it = grouped.begin(); it != grouped.end(); ++it) {
    CacheMatchItem item = it.value();
    QStringList paths = pathBuckets.value(it.key());
    paths.removeDuplicates();
    if (paths.size() == 1) {
      item.path = paths.first();
    } else if (paths.size() == 2) {
      item.path = paths.join("\n");
    } else {
      item.path = QStringLiteral("%1\n%2\n... (%3 paths)")
                      .arg(paths.value(0), paths.value(1))
                      .arg(paths.size());
    }
    result.push_back(item);
  }

  for (const CacheMatchItem& systemItem : buildSystemCacheItems()) {
    result.push_back(systemItem);
  }

  return result;
}

}  // namespace

class CacheFilterProxyModel : public QSortFilterProxyModel {
public:
  explicit CacheFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

  void setSearchText(const QString& text) {
    searchText_ = text.trimmed();
    invalidateFilter();
  }

  void setSafeOnly(bool enabled) {
    safeOnly_ = enabled;
    invalidateFilter();
  }

  void setRiskFilter(const QString& risk) {
    riskFilter_ = risk;
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
    const auto* m = sourceModel();
    if (!m) return true;

    const QModelIndex appIndex = m->index(sourceRow, WindowsCacheScanTableModel::AppNameColumn, sourceParent);
    const QModelIndex categoryIndex = m->index(sourceRow, WindowsCacheScanTableModel::CategoryColumn, sourceParent);
    const QModelIndex safeIndex = m->index(sourceRow, WindowsCacheScanTableModel::SafeColumn, sourceParent);
    const QModelIndex riskIndex = m->index(sourceRow, WindowsCacheScanTableModel::RiskColumn, sourceParent);
    const QModelIndex pathIndex = m->index(sourceRow, WindowsCacheScanTableModel::PathColumn, sourceParent);

    const QString appName = m->data(appIndex, Qt::DisplayRole).toString();
    const QString category = m->data(categoryIndex, Qt::DisplayRole).toString();
    const QString safeText = m->data(safeIndex, Qt::DisplayRole).toString();
    const QString riskText = m->data(riskIndex, Qt::DisplayRole).toString();
    const QString pathText = m->data(pathIndex, Qt::DisplayRole).toString();

    if (safeOnly_ && safeText.compare("Yes", Qt::CaseInsensitive) != 0) return false;
    if (riskFilter_ != "All" && riskText.compare(riskFilter_, Qt::CaseInsensitive) != 0) return false;

    if (searchText_.isEmpty()) return true;
    return appName.contains(searchText_, Qt::CaseInsensitive) ||
           category.contains(searchText_, Qt::CaseInsensitive) ||
           pathText.contains(searchText_, Qt::CaseInsensitive);
  }

private:
  QString searchText_;
  bool safeOnly_ = false;
  QString riskFilter_ = "All";
};

class WindowsCacheScannerWindow : public QWidget {
public:
  explicit WindowsCacheScannerWindow(QWidget* parent = nullptr)
      : QWidget(parent),
        summaryLabel_(new QLabel(this)),
        rulePathLabel_(new QLabel(this)),
        searchEdit_(new QLineEdit(this)),
        safeOnlyCheck_(new QCheckBox(QStringLiteral("Safe Only"), this)),
        riskCombo_(new QComboBox(this)),
        tableView_(new QTableView(this)),
        model_(new WindowsCacheScanTableModel(this)),
        proxyModel_(new CacheFilterProxyModel(this)) {
    setWindowTitle(QStringLiteral("Windows Cache Scanner"));
    resize(1320, 760);

    auto* chooseButton = new QPushButton(QStringLiteral("Choose Rule File"), this);
    auto* scanButton = new QPushButton(QStringLiteral("Start Scan"), this);
    auto* clearFilterButton = new QPushButton(QStringLiteral("Clear Filters"), this);
    searchEdit_->setPlaceholderText(QStringLiteral("Search app, category, or path"));
    riskCombo_->addItems({QStringLiteral("All"), QStringLiteral("low"), QStringLiteral("medium"), QStringLiteral("high")});

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(chooseButton);
    buttonLayout->addWidget(scanButton);
    buttonLayout->addSpacing(12);
    buttonLayout->addWidget(new QLabel(QStringLiteral("Search:"), this));
    buttonLayout->addWidget(searchEdit_, 1);
    buttonLayout->addWidget(safeOnlyCheck_);
    buttonLayout->addWidget(new QLabel(QStringLiteral("Risk:"), this));
    buttonLayout->addWidget(riskCombo_);
    buttonLayout->addWidget(clearFilterButton);
    buttonLayout->addStretch();

    proxyModel_->setSourceModel(model_);
    proxyModel_->setDynamicSortFilter(true);
    tableView_->setModel(proxyModel_);
    tableView_->setSortingEnabled(true);
    tableView_->setAlternatingRowColors(true);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView_->setWordWrap(false);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::PathColumn, QHeaderView::Stretch);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::AppNameColumn, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::CategoryColumn, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::SizeColumn, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::FilesColumn, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::SafeColumn, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(WindowsCacheScanTableModel::RiskColumn, QHeaderView::ResizeToContents);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(buttonLayout);
    layout->addWidget(rulePathLabel_);
    layout->addWidget(summaryLabel_);
    layout->addWidget(tableView_);

    const QString defaultRulePath = QApplication::applicationDirPath() + "/windows_cache_rules.json";
    setRuleFile(defaultRulePath);
    refreshSummary();

    connect(chooseButton, &QPushButton::clicked, this, [this]() {
      const QString filePath = QFileDialog::getOpenFileName(
          this,
          QStringLiteral("Choose Rule File"),
          ruleFilePath_.isEmpty() ? QApplication::applicationDirPath() : ruleFilePath_,
          QStringLiteral("JSON Files (*.json)"));
      if (!filePath.isEmpty()) setRuleFile(filePath);
    });

    connect(scanButton, &QPushButton::clicked, this, [this]() {
      QString errorMessage;
      const QVector<CacheMatchItem> rawItems = WindowsCacheScannerQt::scanRuleFile(ruleFilePath_, &errorMessage);
      if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Scan Failed"), errorMessage);
        return;
      }

      model_->setItems(buildAggregatedItems(rawItems));
      proxyModel_->sort(WindowsCacheScanTableModel::SizeColumn, Qt::DescendingOrder);
      refreshSummary();
    });

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
      proxyModel_->setSearchText(text);
      refreshSummary();
    });
    connect(safeOnlyCheck_, &QCheckBox::toggled, this, [this](bool checked) {
      proxyModel_->setSafeOnly(checked);
      refreshSummary();
    });
    connect(riskCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
      proxyModel_->setRiskFilter(text);
      refreshSummary();
    });
    connect(clearFilterButton, &QPushButton::clicked, this, [this]() {
      searchEdit_->clear();
      safeOnlyCheck_->setChecked(false);
      riskCombo_->setCurrentIndex(0);
      refreshSummary();
    });

    connect(tableView_, &QTableView::doubleClicked, this, [this](const QModelIndex& proxyIndex) {
      showDetailsForIndex(proxyIndex);
    });

    connect(tableView_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
      showContextMenu(pos);
    });
  }

private:
  void showDetailsForIndex(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid()) return;
    const QModelIndex appIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::AppNameColumn);
    const QModelIndex categoryIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::CategoryColumn);
    const QModelIndex sizeIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::SizeColumn);
    const QModelIndex filesIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::FilesColumn);
    const QModelIndex safeIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::SafeColumn);
    const QModelIndex riskIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::RiskColumn);
    const QModelIndex pathIndex = proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::PathColumn);

    QString details = QStringLiteral("App: %1\nCategory: %2\nSize: %3\nFiles: %4\nSafe Delete: %5\nRisk: %6\n\nPaths:\n%7")
                          .arg(proxyModel_->data(appIndex).toString(),
                               proxyModel_->data(categoryIndex).toString(),
                               proxyModel_->data(sizeIndex).toString(),
                               proxyModel_->data(filesIndex).toString(),
                               proxyModel_->data(safeIndex).toString(),
                               proxyModel_->data(riskIndex).toString(),
                               proxyModel_->data(pathIndex).toString());

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("Scan Details"));
    dialog->resize(760, 420);

    auto* textEdit = new QTextEdit(dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(details);

    auto* closeButton = new QPushButton(QStringLiteral("Close"), dialog);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);

    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(textEdit);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog->exec();
  }

  void showContextMenu(const QPoint& pos) {
    const QModelIndex proxyIndex = tableView_->indexAt(pos);
    if (!proxyIndex.isValid()) return;

    const QString pathText =
        proxyModel_->data(proxyModel_->index(proxyIndex.row(), WindowsCacheScanTableModel::PathColumn)).toString();
    QStringList candidatePaths = pathText.split('\n', Qt::SkipEmptyParts);
    if (candidatePaths.isEmpty()) return;

    QString openPath = candidatePaths.first();
    const int markerIndex = openPath.indexOf("... (");
    if (markerIndex >= 0 && candidatePaths.size() > 1) openPath = candidatePaths.at(0);
    openPath = openPath.trimmed();

    QMenu menu(this);
    QAction* openFolderAction = menu.addAction(QStringLiteral("Open Path"));
    QAction* copyPathAction = menu.addAction(QStringLiteral("Copy Path"));
    QAction* detailAction = menu.addAction(QStringLiteral("View Details"));

    QAction* chosen = menu.exec(tableView_->viewport()->mapToGlobal(pos));
    if (chosen == openFolderAction) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(openPath));
    } else if (chosen == copyPathAction) {
      QApplication::clipboard()->setText(pathText);
    } else if (chosen == detailAction) {
      showDetailsForIndex(proxyIndex);
    }
  }

  void setRuleFile(const QString& path) {
    ruleFilePath_ = path;
    rulePathLabel_->setText(QStringLiteral("Rule File: %1").arg(ruleFilePath_));
  }

  void refreshSummary() {
    qint64 visibleBytes = 0;
    for (int row = 0; row < proxyModel_->rowCount(); ++row) {
      visibleBytes += proxyModel_->data(proxyModel_->index(row, WindowsCacheScanTableModel::SizeColumn), Qt::UserRole).toLongLong();
    }
    summaryLabel_->setText(QStringLiteral("Visible Rows: %1 / %2    Visible Cache: %3    Total Cache: %4")
                               .arg(proxyModel_->rowCount())
                               .arg(model_->rowCount())
                               .arg(WindowsCacheScannerQt::formatSize(visibleBytes))
                               .arg(WindowsCacheScannerQt::formatSize(model_->totalBytes())));
  }

  QString ruleFilePath_;
  QLabel* summaryLabel_;
  QLabel* rulePathLabel_;
  QLineEdit* searchEdit_;
  QCheckBox* safeOnlyCheck_;
  QComboBox* riskCombo_;
  QTableView* tableView_;
  WindowsCacheScanTableModel* model_;
  CacheFilterProxyModel* proxyModel_;
};

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  WindowsCacheScannerWindow window;
  window.show();
  return app.exec();
}
