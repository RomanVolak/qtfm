#ifndef COMBOBOXDELEGATE_H
#define COMBOBOXDELEGATE_H

#include <QStyledItemDelegate>

class QModelIndex;
class QWidget;
class QVariant;

/**
 * @class ComboBoxDelegate
 * @brief The ComboBoxDelegate class
 * @author Michal Rost
 * @date 24.12.2012
 */
class ComboBoxDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit ComboBoxDelegate(const QStringList &options,
                            const QList<QIcon> &icons = QList<QIcon>(),
                            QObject *parent = 0);
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const;
protected:
  QStringList options;
  QList<QIcon> icons;
};

#endif // COMBOBOXDELEGATE_H
