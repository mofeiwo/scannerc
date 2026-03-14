#include "windows_cache_scan_table_model.h"

#include <QBrush>
#include <QColor>

#include <algorithm>

WindowsCacheScanTableModel::WindowsCacheScanTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int WindowsCacheScanTableModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return items_.size();
}

int WindowsCacheScanTableModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return ColumnCount;
}

QVariant WindowsCacheScanTableModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) return {};

  const CacheMatchItem& item = items_.at(index.row());
  if (role == Qt::DisplayRole) {
    switch (index.column()) {
      case AppNameColumn:
        return item.appName;
      case CategoryColumn:
        return item.category;
      case SizeColumn:
        return WindowsCacheScannerQt::formatSize(item.bytes);
      case FilesColumn:
        return item.files;
      case SafeColumn:
        return item.deleteSafe ? QStringLiteral("Yes") : QStringLiteral("No");
      case RiskColumn:
        return item.riskLevel;
      case PathColumn:
        return item.path;
      default:
        return {};
    }
  }

  if (role == Qt::UserRole) return item.bytes;
  if (role == Qt::UserRole + 1) return item.files;
  if (role == Qt::TextAlignmentRole && (index.column() == SizeColumn || index.column() == FilesColumn)) {
    return Qt::AlignRight | Qt::AlignVCenter;
  }
  if (role == Qt::ForegroundRole && index.column() == RiskColumn) {
    if (item.riskLevel.compare("high", Qt::CaseInsensitive) == 0) return QBrush(QColor("#b42318"));
    if (item.riskLevel.compare("medium", Qt::CaseInsensitive) == 0) return QBrush(QColor("#b54708"));
    return QBrush(QColor("#027a48"));
  }
  return {};
}

QVariant WindowsCacheScanTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};

  switch (section) {
    case AppNameColumn:
      return QStringLiteral("App");
    case CategoryColumn:
      return QStringLiteral("Category");
    case SizeColumn:
      return QStringLiteral("Size");
    case FilesColumn:
      return QStringLiteral("Files");
    case SafeColumn:
      return QStringLiteral("Safe Delete");
    case RiskColumn:
      return QStringLiteral("Risk");
    case PathColumn:
      return QStringLiteral("Path");
    default:
      return {};
  }
}

Qt::ItemFlags WindowsCacheScanTableModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void WindowsCacheScanTableModel::sort(int column, Qt::SortOrder order) {
  beginResetModel();
  std::sort(items_.begin(), items_.end(), [column, order](const CacheMatchItem& a, const CacheMatchItem& b) {
    const bool asc = order == Qt::AscendingOrder;
    switch (column) {
      case AppNameColumn:
        return asc ? a.appName.localeAwareCompare(b.appName) < 0 : a.appName.localeAwareCompare(b.appName) > 0;
      case CategoryColumn:
        return asc ? a.category.localeAwareCompare(b.category) < 0 : a.category.localeAwareCompare(b.category) > 0;
      case SizeColumn:
        return asc ? a.bytes < b.bytes : a.bytes > b.bytes;
      case FilesColumn:
        return asc ? a.files < b.files : a.files > b.files;
      case SafeColumn:
        return asc ? a.deleteSafe < b.deleteSafe : a.deleteSafe > b.deleteSafe;
      case RiskColumn:
        return asc ? a.riskLevel.localeAwareCompare(b.riskLevel) < 0
                   : a.riskLevel.localeAwareCompare(b.riskLevel) > 0;
      case PathColumn:
        return asc ? a.path.localeAwareCompare(b.path) < 0 : a.path.localeAwareCompare(b.path) > 0;
      default:
        return asc ? a.bytes < b.bytes : a.bytes > b.bytes;
    }
  });
  endResetModel();
}

void WindowsCacheScanTableModel::setItems(const QVector<CacheMatchItem>& items) {
  beginResetModel();
  items_ = items;
  endResetModel();
}

const QVector<CacheMatchItem>& WindowsCacheScanTableModel::items() const {
  return items_;
}

qint64 WindowsCacheScanTableModel::totalBytes() const {
  qint64 total = 0;
  for (const CacheMatchItem& item : items_) total += item.bytes;
  return total;
}
