#include "comboboxdelegate.h"

#include <QComboBox>

/**
 * @brief Creates delegate
 * @param options
 * @param icons
 * @param parent
 */
ComboBoxDelegate::ComboBoxDelegate(const QStringList &options,
    const QList<QIcon> &icons, QObject *parent) : QStyledItemDelegate(parent) {
  this->options = options;
  this->icons = icons;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates editor widget (a combo box)
 * @param parent
 * @param option
 * @param index
 * @return combo box
 */
QWidget* ComboBoxDelegate::createEditor(QWidget *parent,
    const QStyleOptionViewItem &option, const QModelIndex &index) const {

  // Unused variable
  Q_UNUSED(option);

  // Retrieve model
  const QAbstractItemModel* model = index.model();

  // Create combo box
  QComboBox* cmb = new QComboBox(parent);
  cmb->setEditable(true);
  cmb->addItems(options);
  cmb->setCurrentIndex(options.indexOf(model->data(index).toString()));

  // Assign icons
  for (int i = 0; i < icons.size(); i++) {
    cmb->setItemIcon(i, icons.at(i));
  }

  return cmb;
}
//---------------------------------------------------------------------------
