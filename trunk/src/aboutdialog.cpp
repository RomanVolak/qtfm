#include "aboutdialog.h"

#include <QTableWidget>
#include <QLabel>

/**
 * @brief Creates about dialog
 * @param parent
 */
AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {

  QString authors = "<strong>8. 2012 - 5. 2013</strong><br/> Michal Rost ("
                    "<a href=\"mailto:rost.michal@gmail.com\">"
                    "rost.michal@gmail.com</a>)<br/><br/>"
                    "<strong>5. 2010 - 8. 2012</strong><br/> Wittfella ("
                    "<a href=\"mailto:wittfella@qtfm.org\">"
                    "wittfella@qtfm.org</a>)";
  QString thanks = "Eugene Pivnev - TI_Eugene ("
                   "<a href=\"mailto:ti.eugene@gmail.com\">ti.eugene@gmail.com"
                   "</a>)";

  this->setMinimumWidth(400);
  this->setMinimumHeight(350);

  QVBoxLayout* layout = new QVBoxLayout(this);
  QTabWidget* tabWidget = new QTabWidget(this);
  QLabel* info = new QLabel(this);
  info->setText("<strong>QtFM</strong><br/>version: 5.8");

  QWidget* authorsTab = new QWidget(tabWidget);
  tabWidget->addTab(authorsTab, tr("Authors"));
  QGridLayout* authorsLayout = new QGridLayout(authorsTab);
  QLabel* lblAuthors = new QLabel(authors, authorsTab);
  lblAuthors->setAlignment(Qt::AlignHCenter);
  authorsLayout->addWidget(lblAuthors, 0, 1);
  authorsLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 0);
  authorsLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 2);

  QWidget* thanksTab = new QWidget(tabWidget);
  tabWidget->addTab(thanksTab, tr("Thanks"));
  QGridLayout* thanksLayout = new QGridLayout(thanksTab);
  QLabel* lblThanks = new QLabel(thanks, thanksTab);
  lblThanks->setAlignment(Qt::AlignHCenter);
  thanksLayout->addWidget(lblThanks, 0, 1);
  thanksLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 0);
  thanksLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 2);

  layout->addWidget(info);
  layout->addWidget(tabWidget);
}
//---------------------------------------------------------------------------
