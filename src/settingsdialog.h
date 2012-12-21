#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QTreeWidget>
#include <QToolButton>
#include <QSettings>

/**
 * @class SettingsDialog
 * @brief Represents dialog with application settings
 * @author Michal Rost
 * @date 18.12.2012
 */
class SettingsDialog : public QDialog {
  Q_OBJECT
public:
  explicit SettingsDialog(QList<QAction*> *actionList, QSettings* settings,
                          QWidget *parent = 0);
public slots:
  void accept();
  void readSettings();
  void readShortcuts();
  bool saveSettings();
protected slots:
  void addCustomAction();
  void delCustomAction();
  void infoCustomAction();
  void getIcon(QTreeWidgetItem *item, int column);
  void onActionChanged(QTreeWidgetItem *item, int column);
protected:
  QWidget* createGeneralSettings();
  QWidget* createActionsSettings();
  QWidget* createShortcutSettings();

  QSettings* settingsPtr;
  QList<QAction*> *actionListPtr;

  QListWidget* selector;
  QStackedWidget* stack;

  QCheckBox* checkThumbs;
  QCheckBox* checkHidden;
  QCheckBox* checkTabs;
  QCheckBox* checkDelete;
  QLineEdit* editTerm;

  QTreeWidget *actionsWidget;
  QToolButton *addButton;
  QToolButton *delButton;
  QToolButton *infoButton;

  QTreeWidget* shortsWidget;
};

#endif // SETTINGSDIALOG_H
