#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QObject>
#include <QFileInfo>
#include "progressdlg.h"

class ProgressWatcher;

/**
 * @class FileUtils
 * @brief Utility class providing static helper methods for file management
 * @author Wittefella, Michal Rost
 * @date 19.8.2012
 */
class FileUtils {
public:
  static bool removeRecurse(const QString &path, const QString &name);
  static void recurseFolder(const QString &path, const QString &parent, QStringList *list);
  static qint64 totalSize(const QList<QUrl> &files);
};

#endif // FILEUTILS_H
