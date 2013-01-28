#include "desktopfile.h"
#include "properties.h"
#include <QFile>
#include <QDebug>

/**
 * @brief Loads desktop file
 * @param fileName
 */
DesktopFile::DesktopFile(const QString &fileName) {

  // Store file name
  this->fileName = fileName;

  // File validity
  if (!QFile::exists(fileName)) {
    return;
  }

  // Loads file
  Properties desktop(fileName);
  name = desktop.value("Name", "").toString();
  exec = desktop.value("Exec", "").toString();
  icon = desktop.value("Icon", "").toString();
  type = desktop.value("Type", "Application").toString();
  categories = desktop.value("Categories").toString().remove(" ").split(";");

  // Fix categories
  if (categories.first().compare("") == 0) {
    categories.removeFirst();
  }
}
//---------------------------------------------------------------------------

QString DesktopFile::getFileName() const {
  return fileName;
}
//---------------------------------------------------------------------------

QString DesktopFile::getPureFileName() const {
  return fileName.split("/").last().remove(".desktop");
}
//---------------------------------------------------------------------------

QString DesktopFile::getName() const {
  return name;
}
//---------------------------------------------------------------------------

QString DesktopFile::getExec() const {
  return exec;
}
//---------------------------------------------------------------------------

QString DesktopFile::getIcon() const {
  return icon;
}
//---------------------------------------------------------------------------

QString DesktopFile::getType() const {
  return type;
}
//---------------------------------------------------------------------------

QStringList DesktopFile::getCategories() const {
  return categories;
}
//---------------------------------------------------------------------------
