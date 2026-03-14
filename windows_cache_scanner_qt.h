#pragma once

#include <QJsonObject>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

struct CacheRule {
  QString type;
  QStringList include;
  QStringList exclude;
  bool deleteSafe = false;
  QString category;
};

struct CacheAppRule {
  QString id;
  QString name;
  QString vendor;
  QString category;
  QString riskLevel;
  QStringList paths;
  QStringList excludePaths;
  QVector<CacheRule> rules;
};

struct CacheMatchItem {
  QString appName;
  QString category;
  QString path;
  QString riskLevel;
  qint64 bytes = 0;
  qint64 files = 0;
  bool deleteSafe = false;
};

class WindowsCacheScannerQt {
public:
  static QVector<CacheMatchItem> scanRuleFile(const QString& ruleFilePath, QString* errorMessage = nullptr);
  static QString formatSize(qint64 bytes);

private:
  static QString expandEnvPattern(QString text);
  static QStringList expandWildcardPath(const QString& pattern);
  static bool containsTokenI(const QString& text, const QString& token);
  static bool isExcludedFile(const QFileInfo& info, const QJsonObject& globalRules);
  static bool olderThanMinutes(const QFileInfo& info, int minutes);
  static CacheRule parseRule(const QJsonObject& object);
  static CacheAppRule parseAppRule(const QJsonObject& object);
  static std::optional<CacheMatchItem> scanDirectoryTree(const QString& appName,
                                                         const QString& category,
                                                         const QString& riskLevel,
                                                         const QString& rootPath,
                                                         bool deleteSafe,
                                                         const QJsonObject& globalRules);
  static QVector<CacheMatchItem> scanApp(const CacheAppRule& app, const QJsonObject& globalRules);
};
