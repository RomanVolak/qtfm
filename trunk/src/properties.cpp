#include "properties.h"

#include <QTextStream>
#include <QStringList>
#include <QFile>

/**
 * @brief Constructor
 * @param fileName
 */
Properties::Properties(const QString &fileName) {
  if (!fileName.isEmpty()) {
    load(fileName);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Loads property file
 * @param fileName
 * @return true if load was successful
 */
bool Properties::load(const QString &fileName) {

  // NOTE: This class is used for reading of property files instead of QSettings
  // class, which considers separator ';' as comment

  // TODO: Read groups like [Desktop Entry] etc.

  // Try open file
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  // Clear old data
  data.clear();

  // Read propeties
  QTextStream in(&file);
  while (!in.atEnd()) {

    // Read new line
    QString line = in.readLine();

    // Skip empty line or line with invalid format
    if (line.isEmpty() || !line.contains("=")) {
      continue;
    }

    // Read data
    QStringList tmp = line.split("=");
    data.insert(tmp.at(0), tmp.at(1));
  }
  file.close();
}
//---------------------------------------------------------------------------

/**
 * @brief Returns value
 * @param key
 * @param defaultValue
 * @return value
 */
QVariant Properties::value(const QString &key, const QVariant &defaultValue) {
  return data.value(key, defaultValue);
}
//---------------------------------------------------------------------------
