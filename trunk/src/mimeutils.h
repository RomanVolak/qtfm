#ifndef MIMEUTILS_H
#define MIMEUTILS_H

#include "properties.h"

#include <QFileInfo>

/**
 * @class MimeUtils
 * @brief Helps with mime type management
 * @author Michal Rost
 * @date 29.4.2013
 */
class MimeUtils : public QObject {
  Q_OBJECT
public:
  explicit MimeUtils(QObject* parent = 0);
  void openInApp(QString exe, const QFileInfo &file, QObject* processOwner = 0);
  void openInApp(const QFileInfo &file, QObject* processOwner = 0);
  void setDefaultsFileName(const QString &fileName);
  QString getMimeType(const QString &path);
  QStringList getMimeTypes() const;
  Properties loadDefaults() const;
  QString getDefaultsFileName() const;
public slots:
  void generateDefaults();
private:
  QString defaultsFileName;
};

#endif // MIMEUTILS_H
