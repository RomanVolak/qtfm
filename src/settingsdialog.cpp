#include "settingsdialog.h"
#include "icondlg.h"
#include "fileutils.h"
#include "comboboxdelegate.h"

/**
 * @brief Creates settings dialog
 * @param actionList
 * @param settings
 * @param parent
 */
SettingsDialog::SettingsDialog(QList<QAction *> *actionList,
                               QSettings *settings,
                               QWidget *parent) : QDialog(parent) {

  // Store pointer to custom action manager
  this->actionListPtr = actionList;
  this->settingsPtr = settings;

  // Main widgets of this dialog
  selector = new QListWidget(this);
  stack = new QStackedWidget(this);
  selector->setMaximumWidth(150);
  stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Buttons
  QDialogButtonBox* btns = new QDialogButtonBox(this);
  btns->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  connect(btns, SIGNAL(accepted()), this, SLOT(accept()));
  connect(btns, SIGNAL(rejected()), this, SLOT(reject()));

  // Size
  this->setMinimumWidth(640);
  this->setMinimumHeight(480);

  // Layouts
  QHBoxLayout* layoutMain = new QHBoxLayout(this);
  QVBoxLayout* layoutRight = new QVBoxLayout();
  layoutMain->addWidget(selector);
  layoutMain->addItem(layoutRight);
  layoutRight->addWidget(stack);
  layoutRight->addItem(new QSpacerItem(0, 10));
  layoutRight->addWidget(btns);

  // Icons
  QIcon icon1 = QIcon::fromTheme("system-file-manager");
  QIcon icon2 = QIcon::fromTheme("applications-system");
  QIcon icon3 = QIcon::fromTheme("accessories-character-map");
  QIcon icon4 = QIcon::fromTheme("preferences-desktop-filetype-association");

  // Add widget with configurations
  selector->setMinimumWidth(160);
  selector->setViewMode(QListView::ListMode);
  selector->setIconSize(QSize(32, 32));
  selector->addItem(new QListWidgetItem(icon1, tr("General"), selector));
  selector->addItem(new QListWidgetItem(icon2, tr("Custom actions"), selector));
  selector->addItem(new QListWidgetItem(icon3, tr("Shortcuts"), selector));
  selector->addItem(new QListWidgetItem(icon4, tr("Mime types"), selector));

  stack->addWidget(createGeneralSettings());
  stack->addWidget(createActionsSettings());
  stack->addWidget(createShortcutSettings());
  stack->addWidget(createMimeProgress());
  stack->addWidget(createMimeSettings());
  connect(selector, SIGNAL(currentRowChanged(int)), stack,
          SLOT(setCurrentIndex(int)));
  connect(selector, SIGNAL(currentRowChanged(int)), SLOT(loadMimes(int)));


  // Align items
  for (int i = 0; i < selector->count(); i++) {
    selector->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  }

  // Read settings
  readSettings();
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with general settings
 * @return widget
 */
QWidget *SettingsDialog::createGeneralSettings() {

  // Main widget and layout
  QWidget* widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Appearance
  QGroupBox* grpAppear = new QGroupBox(tr("Appearance"), widget);
  QFormLayout* layoutAppear = new QFormLayout(grpAppear);
  checkThumbs = new QCheckBox(grpAppear);
  checkHidden = new QCheckBox(grpAppear);
  checkTabs = new QCheckBox(grpAppear);
  layoutAppear->addRow(tr("Show thumbnails: "), checkThumbs);
  layoutAppear->addRow(tr("Show hidden files: "), checkHidden);
  layoutAppear->addRow(tr("Tabs on top: "), checkTabs);

  // Confirmation
  QGroupBox* grpConfirm = new QGroupBox(tr("Confirmation"), widget);
  QFormLayout* layoutConfirm = new QFormLayout(grpConfirm);
  checkDelete = new QCheckBox(grpConfirm);
  layoutConfirm->addRow(tr("Ask before file is deleted: "), checkDelete);

  // Terminal emulator
  QGroupBox* grpTerm = new QGroupBox(tr("Terminal emulator"), widget);
  QFormLayout* layoutTerm = new QFormLayout(grpTerm);
  editTerm = new QLineEdit(grpTerm);
  layoutTerm->addRow(tr("Command: "), editTerm);

  // Layout of widget
  layoutWidget->addWidget(grpAppear);
  layoutWidget->addWidget(grpConfirm);
  layoutWidget->addWidget(grpTerm);
  layoutWidget->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Fixed,
                                              QSizePolicy::MinimumExpanding));
  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with custom actions settings
 * @return widget
 */
QWidget* SettingsDialog::createActionsSettings() {

  // Layouts
  QWidget* widget = new QWidget(this);
  QVBoxLayout *outerLayout = new QVBoxLayout(widget);
  QGroupBox* grpMain = new QGroupBox(tr("Custom actions"), widget);
  QVBoxLayout* mainLayout = new QVBoxLayout(grpMain);
  outerLayout->addWidget(grpMain);

  // Create actions widget
  actionsWidget = new QTreeWidget(grpMain);
  actionsWidget->setAlternatingRowColors(true);
  actionsWidget->setRootIsDecorated(false);
  actionsWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  actionsWidget->setColumnWidth(1, 160);
  actionsWidget->setColumnWidth(2, 160);
  actionsWidget->setDragDropMode(QAbstractItemView::InternalMove);
  mainLayout->addWidget(actionsWidget);

  // Create header of actions widget
  QTreeWidgetItem *header = actionsWidget->headerItem();
  header->setText(0, tr("Filetype"));
  header->setText(1, tr("Text"));
  header->setText(2, tr("Icon"));
  header->setText(3, tr("Command"));

  // Connect action widget
  connect(actionsWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *,int)),
          this, SLOT(getIcon(QTreeWidgetItem *,int)));
  connect(actionsWidget, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
          this, SLOT(onActionChanged(QTreeWidgetItem*,int)));

  // Create control buttons
  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  addButton = new QToolButton();
  delButton = new QToolButton();
  infoButton = new QToolButton();
  addButton->setIcon(QIcon::fromTheme("list-add"));
  delButton->setIcon(QIcon::fromTheme("list-remove"));
  infoButton->setIcon(QIcon::fromTheme("dialog-question",
                                       QIcon::fromTheme("help-browser")));

  // Connect buttons
  connect(addButton, SIGNAL(clicked()), this, SLOT(addCustomAction()));
  connect(delButton, SIGNAL(clicked()), this, SLOT(delCustomAction()));
  connect(infoButton, SIGNAL(clicked()), this, SLOT(infoCustomAction()));

  // Layouts
  horizontalLayout->addWidget(infoButton);
  horizontalLayout->addWidget(addButton);
  horizontalLayout->addWidget(delButton);
  horizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding));
  mainLayout->addLayout(horizontalLayout);

  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with shortcuts settings
 * @return widget
 */
QWidget* SettingsDialog::createShortcutSettings() {

  // Widget and its layout
  QWidget *widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Shortcuts group box
  QGroupBox* grpShortcuts = new QGroupBox(tr("Configure shortcuts"), widget);
  QVBoxLayout *layoutShortcuts = new QVBoxLayout(grpShortcuts);

  // Tree widget with list of shortcuts
  shortsWidget = new QTreeWidget(grpShortcuts);
  shortsWidget->setAlternatingRowColors(true);
  shortsWidget->setRootIsDecorated(false);
  shortsWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  QTreeWidgetItem *header = shortsWidget->headerItem();
  header->setText(0, tr("Action"));
  header->setText(1, tr("Shortcut"));
  shortsWidget->setColumnWidth(0, 220);
  layoutShortcuts->addWidget(shortsWidget);
  layoutWidget->addWidget(grpShortcuts);

  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with mime progress bar
 * @return widget
 */
QWidget* SettingsDialog::createMimeProgress() {

  // Widget and its layout
  QWidget* widget = new QWidget();
  QGridLayout* layout = new QGridLayout(widget);

  // Mime progress bar
  progressMime = new QProgressBar(widget);
  progressMime->setMinimumWidth(250);
  progressMime->setMaximumWidth(250);
  layout->addWidget(new QLabel(tr("Loading mime types..."), widget), 1, 1);
  layout->addWidget(progressMime, 2, 1);
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 0, 4);
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding), 0, 2, 4);
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed,
                                  QSizePolicy::MinimumExpanding), 0, 1);
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed,
                                  QSizePolicy::MinimumExpanding), 3, 1);

  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates widget with mime settings
 * @return widget
 */
QWidget* SettingsDialog::createMimeSettings() {

  // Widget and its layout
  QWidget *widget = new QWidget();
  QVBoxLayout* layoutWidget = new QVBoxLayout(widget);

  // Shortcuts group box
  QGroupBox* grpMimes = new QGroupBox(tr("Mime types"), widget);
  QVBoxLayout *layoutMimes = new QVBoxLayout(grpMimes);

  // Tree widget with list of shortcuts
  mimesWidget = new QTreeWidget(grpMimes);
  mimesWidget->setAlternatingRowColors(true);
  mimesWidget->setRootIsDecorated(false);
  mimesWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  QTreeWidgetItem *header = mimesWidget->headerItem();
  header->setText(0, tr("Mime"));
  header->setText(1, tr("Application"));
  mimesWidget->setColumnWidth(0, 220);
  layoutMimes->addWidget(mimesWidget);
  layoutWidget->addWidget(grpMimes);

  // Load application list
  QStringList apps = FileUtils::getApplications();
  apps.replaceInStrings(".desktop", "");
  apps.sort();

  // Prepare source of icons
  QDir appIcons("/usr/share/pixmaps","", 0, QDir::Files | QDir::NoDotAndDotDot);
  QStringList iconFiles = appIcons.entryList();
  QIcon defaultIcon = QIcon::fromTheme("application-x-executable");

  // Loads icon list
  QList<QIcon> icons;
  foreach (QString app, apps) {
    QPixmap temp = QIcon::fromTheme(app).pixmap(16, 16);
    if (!temp.isNull()) {
      icons.append(temp);
    } else {
      QStringList searchIcons = iconFiles.filter(app);
      if (searchIcons.count() > 0) {
        icons.append(QIcon("/usr/share/pixmaps/" + searchIcons.at(0)));
      } else {
        icons.append(defaultIcon);
      }
    }
  }

  // Set delegate
  mimesWidget->setItemDelegateForColumn(1, new ComboBoxDelegate(apps, icons));

  return widget;
}
//---------------------------------------------------------------------------

/**
 * @brief Reads settings
 */
void SettingsDialog::readSettings() {

  // Read general settings
  checkThumbs->setChecked(settingsPtr->value("showThumbs", true).toBool());
  checkTabs->setChecked(settingsPtr->value("tabsOnTop", false).toBool());
  checkHidden->setChecked(settingsPtr->value("hiddenMode", true).toBool());
  checkDelete->setChecked(settingsPtr->value("confirmDelete", true).toBool());
  editTerm->setText(settingsPtr->value("term").toString());

  // Read custom actions
  settingsPtr->beginGroup("customActions");
  QStringList keys = settingsPtr->childKeys();
  for (int i = 0; i < keys.count(); ++i) {
    QStringList temp = settingsPtr->value(keys.at(i)).toStringList();
    bool setChecked = 0;
    QString cmd = temp.at(3);
    if (cmd.at(0) == '|') {
      cmd.remove(0,1);
      setChecked = 1;
    }
    QStringList itemData;
    itemData << temp.at(0) << temp.at(1) << temp.at(2) << cmd;
    QTreeWidgetItem *item = new QTreeWidgetItem(actionsWidget, itemData,0);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled
                   | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable);
    item->setCheckState(3, setChecked ? Qt::Checked : Qt::Unchecked);
  }
  settingsPtr->endGroup();

  // Add default actions
  if (keys.count() == 0) {
    QStringList def1, def2, def3;
    def1 << "gz,bz2" << tr("Extract here") << "package-x-generic" << "tar xf %f";
    def2 << "folder" << tr("Term here") << "terminal" << "urxvt -cd %F";
    def3 << "*" << tr("Compress") << "filesave" << "tar czf %n.tar.gz %f";
    QTreeWidgetItem *item1 = new QTreeWidgetItem(actionsWidget, def1, 0);
    QTreeWidgetItem *item2 = new QTreeWidgetItem(actionsWidget, def2, 0);
    QTreeWidgetItem *item3 = new QTreeWidgetItem(actionsWidget, def3, 0);
    item1->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable
                    | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    item2->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable
                    | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    item3->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable
                    | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
  }

  // Loads icons for actions
  for (int x = 0; x < actionsWidget->topLevelItemCount(); x++) {
    QString name = actionsWidget->topLevelItem(x)->text(2);
    actionsWidget->topLevelItem(x)->setIcon(2, QIcon::fromTheme(name));
  }

  // Read shortcuts
  readShortcuts();
}
//---------------------------------------------------------------------------

/**
 * @brief Reads shortcuts
 */
void SettingsDialog::readShortcuts() {

  // Delete list of shortcuts
  shortsWidget->clear();

  // Icon configuration
  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);
  QIcon blank(pixmap);

  // Read shorcuts
  QHash<QString, QString> shortcuts;
  settingsPtr->beginGroup("customShortcuts");
  QStringList keys = settingsPtr->childKeys();
  for (int i = 0; i < keys.count(); ++i) {
    QStringList temp(settingsPtr->value(keys.at(i)).toStringList());
    shortcuts.insert(temp.at(0), temp.at(1));
  }
  settingsPtr->endGroup();

  // Assign shortcuts to action and bookmarks
  for (int i = 0; i < actionListPtr->count(); ++i) {
    QAction* act = actionListPtr->at(i);
    QString text = shortcuts.value(act->text());
    text = text.isEmpty() ? text : QKeySequence::fromString(text).toString();
    QStringList list;
    list << act->text() << text;
    QTreeWidgetItem *item = new QTreeWidgetItem(shortsWidget, list);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    item->setIcon(0, act->icon());
    if (item->icon(0).isNull()) {
      item->setIcon(0, blank);
    }
  }

  // Assign shortcuts to custom actions
  for (int i = 0; i < actionsWidget->topLevelItemCount(); i++) {
    QTreeWidgetItem *srcItem = actionsWidget->topLevelItem(i);
    QString text = shortcuts.value(srcItem->text(1));
    text = text.isEmpty() ? text : QKeySequence::fromString(text).toString();
    QStringList list;
    list << srcItem->text(1) << text;
    QTreeWidgetItem *item = new QTreeWidgetItem(shortsWidget, list);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    item->setIcon(0, QIcon::fromTheme(srcItem->text(2)));
    if (item->icon(0).isNull()) {
      item->setIcon(0, blank);
    }
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Loads mime types
 * @param section
 */
void SettingsDialog::loadMimes(int section) {

  // Mime progress section
  const int MIME_PROGRESS_SECTION = 3;

  // If section is not mime type configuration section exit
  if (section != MIME_PROGRESS_SECTION) {
    return;
  }

  // If mimes have been already loaded move to another section (mime config)
  if (mimesWidget->topLevelItemCount() > 0) {
    stack->setCurrentIndex(MIME_PROGRESS_SECTION + 1);
    return;
  }

  // Load list of mimes
  QStringList mimes = FileUtils::getMimeTypes();

  // Init process
  progressMime->setRange(1, mimes.size());

  // Load list of default applications
  // NOTE: xdg changes -> now uses mimeapps.list instead of defaults.list
  QString xdgDefaults;
  QString path = QDir::homePath() + "/.local/share/applications/mimeapps.list";
  if (QFileInfo(path).exists()) {
    xdgDefaults = path;
  } else {
    xdgDefaults = QDir::homePath() + "/.local/share/applications/defaults.list";
  }
  QSettings defaults(xdgDefaults, QSettings::IniFormat, this);

  // Load mime settings
  foreach (QString mime, mimes) {

    // Updates progress
    progressMime->setValue(progressMime->value() + 1);

    // Skip all 'inode' nodes including 'inode/directory'
    if (mime.startsWith("inode")) {
      continue;
    }

    // Skip all 'x-content' and 'message' nodes
    if (mime.startsWith("x-content") || mime.startsWith('message')) {
      continue;
    }

    // Load icon and default application for current mime
    QIcon icon = FileUtils::getMimeIconOrUnknown(mime);
    QString appName = defaults.value("Default Applications/" + mime).toString();

    // Create item from current mime
    QTreeWidgetItem *item = new QTreeWidgetItem(mimesWidget);
    item->setIcon(0, icon);
    item->setText(0, mime);
    item->setText(1, appName.remove(".desktop"));
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
  }

  // Move to mimes
  stack->setCurrentIndex(MIME_PROGRESS_SECTION + 1);
}
//---------------------------------------------------------------------------

/**
 * @brief Saves settings
 * @return true if successfull
 */
bool SettingsDialog::saveSettings() {

  // General settings
  // ------------------------------------------------------------------------
  settingsPtr->setValue("showThumbs", checkThumbs->isChecked());
  settingsPtr->setValue("tabsOnTop", checkTabs->isChecked());
  settingsPtr->setValue("hiddenMode", checkHidden->isChecked());
  settingsPtr->setValue("confirmDelete", checkDelete->isChecked());
  settingsPtr->setValue("term", editTerm->text());

  // Custom actions
  // ------------------------------------------------------------------------
  settingsPtr->remove("customActions");
  settingsPtr->beginGroup("customActions");
  for (int i = 0; i < actionsWidget->topLevelItemCount(); i++) {
    QTreeWidgetItem *item = actionsWidget->topLevelItem(i);
    QStringList temp;
    QString cmd = item->text(3);
    if (item->checkState(3) == Qt::Checked) {
      cmd.prepend("|");
    }
    temp << item->text(0) << item->text(1) << item->text(2) << cmd;
    settingsPtr->setValue(QString(i),temp);
  }
  settingsPtr->endGroup();
  settingsPtr->setValue("customHeader", actionsWidget->header()->saveState());

  // Shortcuts
  // ------------------------------------------------------------------------
  QStringList shortcuts, duplicates;
  settingsPtr->remove("customShortcuts");
  settingsPtr->beginGroup("customShortcuts");
  for (int i = 0; i < shortsWidget->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = shortsWidget->topLevelItem(i);
    if (!item->text(1).isEmpty()) {
      int existing = shortcuts.indexOf(item->text(1));
      if (existing != -1) {
        duplicates.append(QString("<b>%1</b> - %2").arg(shortcuts.at(existing))
                          .arg(item->text(0)));
      }
      shortcuts.append(item->text(1));
      QStringList temp;
      temp << item->text(0) << item->text(1);
      settingsPtr->setValue(QString(shortcuts.count()), temp);
    }
  }
  settingsPtr->endGroup();

  // Mime types
  // ------------------------------------------------------------------------
  QProcess *p = new QProcess(this);
  for (int i = 0; i < mimesWidget->topLevelItemCount(); ++i) {
    QString mime = mimesWidget->topLevelItem(i)->text(0);
    QString appName = mimesWidget->topLevelItem(i)->text(1);
    if (!appName.isEmpty()) {
      appName += ".desktop";
      p->start("xdg-mime", QStringList() << "default" << appName << mime);
      p->waitForFinished();
    }
  }

  // Check for shortcuts duplicity
  // ------------------------------------------------------------------------
  if (duplicates.count()) {
    QMessageBox::information(this, tr("Warning"),
                             QString(tr("Duplicate shortcuts detected:<p>%1"))
                             .arg(duplicates.join("<p>")));
  }

  return true;
}
//---------------------------------------------------------------------------

/**
 * @brief Accepts settings configuration
 */
void SettingsDialog::accept() {
  if (saveSettings()) this->done(1);
}
//---------------------------------------------------------------------------

/**
 * @brief Selects icon of custom action
 * @param item
 * @param column
 */
void SettingsDialog::getIcon(QTreeWidgetItem* item, int column) {
  if (column == 2) {
    icondlg *icons = new icondlg;
    if (icons->exec() == 1) {
      item->setText(column, icons->result);
      item->setIcon(column, QIcon::fromTheme(icons->result));
    }
    delete icons;
  }
  return;
}
//---------------------------------------------------------------------------

/**
 * @brief Updates shortcuts
 * @param item
 * @param column
 */
void SettingsDialog::onActionChanged(QTreeWidgetItem *item, int column) {
  if (column == 1 || column == 2) {
    readShortcuts();
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Adds new custom action
 */
void SettingsDialog::addCustomAction() {
  actionsWidget->clearSelection();
  QTreeWidgetItem *tmp = new QTreeWidgetItem(actionsWidget,
                                             QStringList() << "*", 0);
  tmp->setFlags(Qt::ItemIsSelectable
                | Qt::ItemIsEditable
                | Qt::ItemIsEnabled
                | Qt::ItemIsDragEnabled
                | Qt::ItemIsUserCheckable);
  tmp->setCheckState(3,Qt::Unchecked);
  tmp->setSelected(true);
  actionsWidget->setCurrentItem(tmp);
  actionsWidget->scrollToItem(tmp);
  readShortcuts();
}
//---------------------------------------------------------------------------

/**
 * @brief Deletes custom action
 */
void SettingsDialog::delCustomAction() {
  delete actionsWidget->currentItem();
  readShortcuts();
}
//---------------------------------------------------------------------------

/**
 * @brief Displays info about custom action
 */
void SettingsDialog::infoCustomAction() {

  // Info
  QString info = tr("Use 'folder' to match all folders.<br>" \
                    "Use a folder name to match a specific folder.<br>" \
                    "Set text to 'Open' to override xdg default." \
                    "<p>%f - selected files<br>" \
                    "%F - selected files with full path<br>" \
                    "%n - current filename</p>" \
                    "<p>[] - tick checkbox to monitor output and errors.</p>");

  // Displays info
  QMessageBox::question(this, tr("Usage"), info);
}
//---------------------------------------------------------------------------
