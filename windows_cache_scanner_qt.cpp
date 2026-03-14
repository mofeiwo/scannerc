#include "windows_cache_scanner_qt.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>
#include <optional>

namespace {

QString replaceAll(QString text, const QString& from, const QString& to) {
  text.replace(from, to, Qt::CaseInsensitive);
  return text;
}

QRegularExpression wildcardToRegex(const QString& wildcard) {
  QString pattern = QRegularExpression::escape(wildcard);
  pattern.replace("\\*", ".*");
  pattern.replace("\\?", ".");
  return QRegularExpression("^" + pattern + "$", QRegularExpression::CaseInsensitiveOption);
}

}  // namespace

QString WindowsCacheScannerQt::expandEnvPattern(QString text) {
  const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QList<QPair<QString, QString>> vars = {
      {"%USERPROFILE%", env.value("USERPROFILE")},
      {"%LOCALAPPDATA%", env.value("LOCALAPPDATA")},
      {"%APPDATA%", env.value("APPDATA")},
      {"%PROGRAMFILES%", env.value("ProgramFiles")},
      {"%PROGRAMFILES(X86)%", env.value("ProgramFiles(x86)")},
      {"%TEMP%", env.value("TEMP")},
  };

  for (const auto& item : vars) {
    text = replaceAll(text, item.first, item.second);
  }
  return QDir::fromNativeSeparators(text);
}

QStringList WindowsCacheScannerQt::expandWildcardPath(const QString& pattern) {
  const QString expanded = expandEnvPattern(pattern);
  if (!expanded.contains('*') && !expanded.contains('?')) {
    return QFileInfo::exists(expanded) ? QStringList{QDir::cleanPath(expanded)} : QStringList{};
  }

  const QFileInfo info(expanded);
  const QString parentPath = info.dir().absolutePath();
  const QString leafPattern = info.fileName();
  QDir parentDir(parentPath);
  if (!parentDir.exists()) return {};

  const QRegularExpression regex = wildcardToRegex(leafPattern);
  QStringList results;
  const QFileInfoList entries =
      parentDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files, QDir::Name);
  for (const QFileInfo& entry : entries) {
    if (regex.match(entry.fileName()).hasMatch()) {
      results.push_back(QDir::cleanPath(entry.absoluteFilePath()));
    }
  }
  return results;
}

bool WindowsCacheScannerQt::containsTokenI(const QString& text, const QString& token) {
  return text.contains(token, Qt::CaseInsensitive);
}

bool WindowsCacheScannerQt::isExcludedFile(const QFileInfo& info, const QJsonObject& globalRules) {
  const QJsonArray excludeNames = globalRules.value("default_exclude_file_names").toArray();
  for (const QJsonValue& value : excludeNames) {
    if (QString::compare(info.fileName(), value.toString(), Qt::CaseInsensitive) == 0) {
      return true;
    }
  }

  const QJsonArray excludeExtensions = globalRules.value("default_exclude_extensions").toArray();
  const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();
  for (const QJsonValue& value : excludeExtensions) {
    if (QString::compare(suffix, value.toString(), Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return false;
}

bool WindowsCacheScannerQt::olderThanMinutes(const QFileInfo& info, int minutes) {
  return info.lastModified().secsTo(QDateTime::currentDateTime()) >= minutes * 60;
}

CacheRule WindowsCacheScannerQt::parseRule(const QJsonObject& object) {
  CacheRule rule;
  rule.type = object.value("type").toString();
  for (const QJsonValue& value : object.value("include").toArray()) {
    rule.include.push_back(value.toString());
  }
  for (const QJsonValue& value : object.value("exclude").toArray()) {
    rule.exclude.push_back(value.toString());
  }
  rule.deleteSafe = object.value("delete_safe").toBool(false);
  rule.category = object.value("category").toString("cache");
  return rule;
}

CacheAppRule WindowsCacheScannerQt::parseAppRule(const QJsonObject& object) {
  CacheAppRule app;
  app.id = object.value("id").toString();
  app.name = object.value("name").toString();
  app.vendor = object.value("vendor").toString();
  app.category = object.value("category").toString();
  app.riskLevel = object.value("risk_level").toString();

  for (const QJsonValue& value : object.value("paths").toArray()) {
    app.paths.push_back(value.toString());
  }
  for (const QJsonValue& value : object.value("exclude_paths").toArray()) {
    app.excludePaths.push_back(value.toString());
  }
  for (const QJsonValue& value : object.value("rules").toArray()) {
    app.rules.push_back(parseRule(value.toObject()));
  }
  return app;
}

std::optional<CacheMatchItem> WindowsCacheScannerQt::scanDirectoryTree(const QString& appName,
                                                                       const QString& category,
                                                                       const QString& riskLevel,
                                                                       const QString& rootPath,
                                                                       bool deleteSafe,
                                                                       const QJsonObject& globalRules) {
  QDir root(rootPath);
  if (!root.exists()) return std::nullopt;

  CacheMatchItem result;
  result.appName = appName;
  result.category = category;
  result.path = QDir::cleanPath(rootPath);
  result.riskLevel = riskLevel;
  result.deleteSafe = deleteSafe;

  const int minAgeMinutes = globalRules.value("default_min_file_age_minutes").toInt(10);
  QDirIterator it(rootPath, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    it.next();
    const QFileInfo info = it.fileInfo();
    if (!olderThanMinutes(info, minAgeMinutes)) continue;
    if (isExcludedFile(info, globalRules)) continue;

    result.bytes += info.size();
    result.files += 1;
  }

  if (result.files == 0) return std::nullopt;
  return result;
}

QVector<CacheMatchItem> WindowsCacheScannerQt::scanApp(const CacheAppRule& app,
                                                       const QJsonObject& globalRules) {
  QVector<CacheMatchItem> results;
  QStringList excludedPrefixes;
  for (const QString& path : app.excludePaths) {
    excludedPrefixes.push_back(expandEnvPattern(path));
  }

  for (const QString& basePattern : app.paths) {
    const QStringList basePaths = expandWildcardPath(basePattern);
    for (const QString& basePath : basePaths) {
      bool skipBase = false;
      for (const QString& excluded : excludedPrefixes) {
        if (!excluded.isEmpty() && containsTokenI(basePath, excluded)) {
          skipBase = true;
          break;
        }
      }
      if (skipBase) continue;

      for (const CacheRule& rule : app.rules) {
        if (rule.type == "relative_dir") {
          for (const QString& relative : rule.include) {
            const QString target = QDir(basePath).filePath(QDir::fromNativeSeparators(relative));
            auto item = scanDirectoryTree(app.name, rule.category, app.riskLevel, target, rule.deleteSafe, globalRules);
            if (item.has_value()) results.push_back(*item);
          }
          continue;
        }

        if (rule.type == "file_pattern") {
          QVector<QRegularExpression> patterns;
          for (const QString& pattern : rule.include) {
            patterns.push_back(wildcardToRegex(pattern));
          }

          CacheMatchItem item;
          item.appName = app.name;
          item.category = rule.category;
          item.path = QDir::cleanPath(basePath);
          item.riskLevel = app.riskLevel;
          item.deleteSafe = rule.deleteSafe;

          const int minAgeMinutes = globalRules.value("default_min_file_age_minutes").toInt(10);
          QDirIterator it(basePath, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
          while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            if (!olderThanMinutes(info, minAgeMinutes)) continue;
            if (isExcludedFile(info, globalRules)) continue;

            bool matched = false;
            for (const auto& regex : patterns) {
              if (regex.match(info.fileName()).hasMatch()) {
                matched = true;
                break;
              }
            }
            if (!matched) continue;

            item.bytes += info.size();
            item.files += 1;
          }

          if (item.files > 0) results.push_back(item);
          continue;
        }

        if (rule.type == "dir_name") {
          QDirIterator it(basePath,
                          QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
          while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            const QString fullPath = QDir::cleanPath(info.absoluteFilePath());

            bool included = false;
            for (const QString& include : rule.include) {
              if (QString::compare(info.fileName(), include, Qt::CaseInsensitive) == 0 ||
                  containsTokenI(fullPath, QDir::fromNativeSeparators(include))) {
                included = true;
                break;
              }
            }
            if (!included) continue;

            bool excluded = false;
            for (const QString& exclude : rule.exclude) {
              if (containsTokenI(fullPath, QDir::fromNativeSeparators(exclude))) {
                excluded = true;
                break;
              }
            }
            if (excluded) continue;

            auto item = scanDirectoryTree(app.name, rule.category, app.riskLevel, fullPath,
                                          rule.deleteSafe, globalRules);
            if (item.has_value()) results.push_back(*item);
          }
        }
      }
    }
  }

  return results;
}

QVector<CacheMatchItem> WindowsCacheScannerQt::scanRuleFile(const QString& ruleFilePath,
                                                            QString* errorMessage) {
  QFile file(ruleFilePath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorMessage) *errorMessage = QStringLiteral("Failed to open rules file: %1").arg(ruleFilePath);
    return {};
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (errorMessage) *errorMessage = QStringLiteral("Invalid JSON rules file: %1").arg(parseError.errorString());
    return {};
  }

  const QJsonObject root = document.object();
  const QJsonObject globalRules = root.value("global_rules").toObject();
  const QJsonArray apps = root.value("apps").toArray();

  QVector<CacheMatchItem> allMatches;
  for (const QJsonValue& value : apps) {
    const CacheAppRule appRule = parseAppRule(value.toObject());
    const QVector<CacheMatchItem> matches = scanApp(appRule, globalRules);
    allMatches += matches;
  }

  std::sort(allMatches.begin(), allMatches.end(), [](const CacheMatchItem& a, const CacheMatchItem& b) {
    return a.bytes > b.bytes;
  });
  return allMatches;
}

QString WindowsCacheScannerQt::formatSize(qint64 bytes) {
  static const QStringList units = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  int index = 0;
  while (value >= 1024.0 && index < units.size() - 1) {
    value /= 1024.0;
    ++index;
  }
  return QString::number(value, 'f', index == 0 ? 0 : 2) + " " + units[index];
}
