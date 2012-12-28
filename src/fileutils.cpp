#include "fileutils.h"
#include <QDirIterator>
#include <sys/vfs.h>
#include <magic.h>

/**
 * @brief Recursive removes file or directory
 * @param path path to file
 * @param name name of file
 * @return true if file/directory was successfully removed
 */
bool FileUtils::removeRecurse(const QString &path, const QString &name) {

  // File location
  QString url = path + QDir::separator() + name;

  // Check whether file or directory exists
  QFileInfo file(url);
  if (!file.exists()) {
    return false;
  }

  // List of files that will be deleted
  QStringList files;

  // If given file is a directory, collect all children of given directory
  if (file.isDir()) {
    QDirIterator it(url, QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot
                    | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      files.prepend(it.next());
    }
  }

  // Append given file to the list of files and delete all
  files.append(url);
  foreach (QString file, files) {
    QFile(file).remove();
  }
  return true;
}
//---------------------------------------------------------------------------

/**
 * @brief Collects all file names in given path (recursive)
 * @param path path
 * @param parent parent path
 * @param list resulting list of files
 */
void FileUtils::recurseFolder(const QString &path, const QString &parent,
                              QStringList *list) {

  // Get all files in this path
  QDir dir(path);
  QStringList files = dir.entryList(QDir::AllEntries | QDir::Files
                                    | QDir::NoDotAndDotDot | QDir::Hidden);

  // Go through all files in current directory
  for (int i = 0; i < files.count(); i++) {

    // If current file is folder perform this method again. Otherwise add file
    // to list of results
    QString current = parent + QDir::separator() + files.at(i);
    if (QFileInfo(files.at(i)).isDir()) {
      recurseFolder(files.at(i), current, list);
    }
    else list->append(current);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Returns size of all given files/dirs (including nested files/dirs)
 * @param files
 * @return total size
 */
qint64 FileUtils::totalSize(const QList<QUrl> &files) {
  qint64 total = 1;
  foreach (QUrl url, files) {
    QFileInfo file = url.path();
    if (file.isFile()) total += file.size();
    else {
      QDirIterator it(url.path(), QDir::AllEntries | QDir::System
                      | QDir::NoDotAndDotDot | QDir::NoSymLinks
                      | QDir::Hidden, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
      }
    }
  }
  return total;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime type of given file
 * @note This operation is slow, prevent its mass application
 * @param path path to file
 * @return mime type
 */
QString FileUtils::getMimeType(const QString &path) {
  magic_t cookie = magic_open(MAGIC_MIME);
  magic_load(cookie, 0);
  QString temp = magic_file(cookie, path.toLocal8Bit());
  magic_close(cookie);
  return temp.left(temp.indexOf(";"));
}
//---------------------------------------------------------------------------

/**
 * @brief Returns list of available applications
 * @return application list
 */
QStringList FileUtils::getApplications() {
  QStringList apps;
  QDirIterator it("/usr/share/applications", QStringList("*.desktop"),
                  QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    apps.append(it.fileName());
  }
  return apps;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns list of mime types
 * @return list of available mimetypes
 */
QStringList FileUtils::getMimeTypes() {

  // Check whether file with mime descriptions exists
  QFile file("/usr/share/mime/types");
  if (!file.exists()) {
    return QStringList();
  }

  // Try to open file
  if (!file.open(QFile::ReadOnly)) {
    return QStringList();
  }

  // Read mime types
  QStringList result;
  QTextStream stream(&file);
  while (!stream.atEnd()) {
    result.append(stream.readLine());
  }
  file.close();
  return result;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns real suffix for given file
 * @param name
 * @return suffix
 */
QString FileUtils::getRealSuffix(const QString &name) {

  // Strip version suffix
  QStringList tmp = name.split(".");
  bool ok;
  while (tmp.size() > 1) {
    tmp.last().toInt(&ok);
    if (!ok) {
      return tmp.last();
    }
    tmp.removeLast();
  }
  return "";
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime icon
 * @param mime
 * @return icon
 */
QIcon FileUtils::getMimeIcon(QString mime) {

  // Try to find icon
  QIcon icon = QIcon::fromTheme(mime.replace("/", "-"));
  if (!icon.isNull()) {
    return icon;
  }

  // Search for generic icon
  QStringList tmp = mime.split("-");
  while (tmp.size() > 2) {
    tmp.removeLast();
    icon = QIcon::fromTheme(tmp.join("-") + "-generic");
    if (!icon.isNull()) {
      return icon;
    }
  }

  // One last chance
  icon = QIcon::fromTheme(tmp.first() + "-x-generic");
  return icon;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime icon or uknown icon
 * @param mime
 * @return mime icon or uknown icon
 */
QIcon FileUtils::getMimeIconOrUnknown(QString mime) {
  QIcon icon = getMimeIcon(mime);
  return icon.isNull() ? QIcon::fromTheme("unknown") : icon;
}
//---------------------------------------------------------------------------
