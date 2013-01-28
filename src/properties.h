#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <QVariant>
#include <QObject>
#include <QMap>

/**
 * @class Properties
 * @brief Read property files
 * @author Michal Rost
 * @date 26.1.2013
 */
class Properties {
public:
  explicit Properties(const QString &fileName = "");
  QVariant value(const QString &key, const QVariant &defaultValue = QVariant());
  bool load(const QString &fileName);
protected:
  QMap<QString, QVariant> data;
};

#endif // PROPERTIES_H
