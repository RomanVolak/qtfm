#include "mimeutils.h"
#include "fileutils.h"

#include <QProcess>
#include <QDebug>
#include <QMessageBox>
#include <magic.h>

/**
 * @brief Creates mime utils
 * @param parent
 */
MimeUtils::MimeUtils(QObject *parent) : QObject(parent) {
  defaultsFileName = "/.local/share/applications/mimeapps.list";
}
//---------------------------------------------------------------------------

/**
 * @brief Loads list of default applications for mimes
 * @return properties with default applications
 */
Properties MimeUtils::loadDefaults() const {
  return Properties(QDir::homePath() + defaultsFileName);
}
//---------------------------------------------------------------------------

/**
 * @brief Returns mime type of given file
 * @note This operation is slow, prevent its mass application
 * @param path path to file
 * @return mime type
 */
QString MimeUtils::getMimeType(const QString &path) {
  magic_t cookie = magic_open(MAGIC_MIME);
  magic_load(cookie, 0);
  QString temp = magic_file(cookie, path.toLocal8Bit());
  magic_close(cookie);
  return temp.left(temp.indexOf(";"));
}
//---------------------------------------------------------------------------

/**
 * @brief Returns list of mime types
 * @return list of available mimetypes
 */
QStringList MimeUtils::getMimeTypes() const {

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
 * @brief Opens file in a default application
 * @param file
 * @param processOwner
 */
void MimeUtils::openInApp(const QFileInfo &file, QObject *processOwner) {
  QString mime = getMimeType(file.filePath());
  QString app = loadDefaults().value(mime).toString().split(";").first();
  if (!app.isEmpty()) {
    //qDebug() << mime;
    //qDebug() << file.filePath();
    //qDebug() << app;
    openInApp(app.remove(".desktop"), file, processOwner);
  } else {
    QString title = tr("No default application");
    QString msg = tr("No default application for mime: %1!").arg(mime);
    QMessageBox::warning(NULL, title, msg);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Opens file in a given application
 * @param exe name of application to be executed
 * @param file to be opened in executed application
 * @param processOwner process owner (default NULL)
 */
void MimeUtils::openInApp(QString exe, const QFileInfo &file,
                          QObject *processOwner) {

  // Separate application name from its arguments
  QStringList split = exe.split(" ");
  QString name = split.takeAt(0);
  QString args = split.join(" ");

  // Get relative path
  //args = args.split(QDir::separator()).last();

  // Replace parameters with file name. If there are no parameters simply append
  // file name to the end of argument list
  if (args.toLower().contains("%f")) {
    args.replace("%f", file.filePath(), Qt::CaseInsensitive);
  } else if (args.toLower().contains("%u")) {
    args.replace("%u", file.filePath(), Qt::CaseInsensitive);
  } else {
    args.append(args.isEmpty() ? "" : " ");
    args.append(/*"\"" + */file.filePath()/* + "\""*/);
  }

  //qDebug() << name << args;

  // Start application
  QProcess *myProcess = new QProcess(processOwner);
  myProcess->startDetached(name, QStringList() << args);
  myProcess->waitForFinished(1000);
  //myProcess->terminate();
}
//---------------------------------------------------------------------------

/**
 * @brief Sets defaults file name (name of file where defaults are stored)
 * @param fileName
 */
void MimeUtils::setDefaultsFileName(const QString &fileName) {
  this->defaultsFileName = fileName;
}
//---------------------------------------------------------------------------

/**
 * @brief Returns defaults file name
 * @return name of file where defaults are stored
 */
QString MimeUtils::getDefaultsFileName() const {
  return defaultsFileName;
}
//---------------------------------------------------------------------------

/**
 * @brief Generates default application-mime associations
 */
void MimeUtils::generateDefaults() {

  // Load list of applications and list of default mime-applications
  // associations
  QList<DesktopFile> apps = FileUtils::getApplications();
  Properties defaults = loadDefaults();
  QStringList names;

  // Indicates change of defaults
  bool defaultsChanged = false;

  // Find defaults; for each application...
  // ------------------------------------------------------------------------
  foreach (DesktopFile a, apps) {

    // For each mime of current application...
    QStringList mimes = a.getMimeType();
    foreach (QString mime, mimes) {

      // Current app name
      QString name = a.getPureFileName() + ".desktop";
      names.append(name);

      // If current mime is not mentioned in the list of defaults, add it
      // together with current application and continue
      if (!defaults.contains(mime)) {
        defaults.set(mime, name);
        defaultsChanged = true;
        continue;
      }

      // Retrieve list of default applications for current mime, if it does
      // not contain current application, add this application to list
      QStringList appNames = defaults.value(mime).toString().split(";");
      if (!appNames.contains(name)) {
        appNames.append(name);
        defaults.set(mime, appNames.join(";"));
        defaultsChanged = true;
      }
    }
  }

  // Delete dead defaults (non existing apps)
  // ------------------------------------------------------------------------
  foreach (QString key, defaults.getKeys()) {
    QStringList tmpNames1 = defaults.value(key).toString().split(";");
    QStringList tmpNames2 = QStringList();
    foreach (QString name, tmpNames1) {
      if (names.contains(name)) {
        tmpNames2.append(name);
      }
    }
    if (tmpNames1.size() != tmpNames2.size()) {
      defaults.set(key, tmpNames2.join(";"));
      defaultsChanged = true;
    }
  }

  // Save defaults if changed
  if (defaultsChanged) {
    defaults.save(QDir::homePath() + defaultsFileName, "Default Applications");
  }
}
//---------------------------------------------------------------------------
