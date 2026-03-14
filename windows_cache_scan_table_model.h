#pragma once

#include "windows_cache_scanner_qt.h"

#include <QAbstractTableModel>

class WindowsCacheScanTableModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Column {
    AppNameColumn = 0,
    CategoryColumn,
    SizeColumn,
    FilesColumn,
    SafeColumn,
    RiskColumn,
    PathColumn,
    ColumnCount
  };

  explicit WindowsCacheScanTableModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  void setItems(const QVector<CacheMatchItem>& items);
  const QVector<CacheMatchItem>& items() const;
  qint64 totalBytes() const;

private:
  QVector<CacheMatchItem> items_;
};
