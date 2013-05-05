/****************************************************************************
* This file is part of qtFM, a simple, fast file manager.
* Copyright (C) 2010,2011,2012 Wittfella
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*
* Contact e-mail: wittfella@qtfm.org
*
****************************************************************************/

#include <QtGui>
#include <QDockWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QApplication>
#include <QStatusBar>
#include <QMenu>
#include <sys/vfs.h>
#include <fcntl.h>

#if QT_VERSION >= 0x050000
  #include <QtConcurrent/QtConcurrent>
#else
#endif

#include "mainwindow.h"
#include "mymodel.h"
#include "progressdlg.h"
#include "fileutils.h"
#include "applicationdialog.h"

MainWindow::MainWindow()
{
    isDaemon = 0;
    startPath = QDir::currentPath();
    QStringList args = QApplication::arguments();

    if(args.count() > 1)
    {
        if(args.at(1) == "-d") isDaemon = 1;
        else startPath = args.at(1);

        #if QT_VERSION >= 0x040800
        if(QUrl(startPath).isLocalFile())
            startPath = QUrl(args.at(1)).toLocalFile();
        #endif
    }

    settings = new QSettings();

    QString temp = settings->value("forceTheme").toString();
    if(temp.isNull())
    {
        //get theme from system (works for gnome/kde)
        temp = QIcon::themeName();

        //Qt doesn't detect the theme very well for non-DE systems,
        //so try reading the '~/.gtkrc-2.0' or '~/.config/gtk-3.0/settings.ini'

        if(temp == "hicolor")
        {
            //check for gtk-2.0 settings
            if(QFile::exists(QDir::homePath() + "/" + ".gtkrc-2.0"))
            {
                QSettings gtkFile(QDir::homePath() + "/.gtkrc-2.0",QSettings::IniFormat,this);
                temp = gtkFile.value("gtk-icon-theme-name").toString().remove("\"");
            }
            else
            {
                //try gtk-3.0
                QSettings gtkFile(QDir::homePath() + "/.config/gtk-3.0/settings.ini",QSettings::IniFormat,this);
                temp = gtkFile.value("gtk-fallback-icon-theme").toString().remove("\"");
            }

            //fallback
            if(temp.isNull())
            {
                if(QFile::exists("/usr/share/icons/gnome")) temp = "gnome";
                else if(QFile::exists("/usr/share/icons/oxygen")) temp = "oxygen";
                else temp = "hicolor";

                settings->setValue("forceTheme",temp);
            }
        }
    }

    QIcon::setThemeName(temp);

    // Create mime utils
    mimeUtils = new MimeUtils(this);
    QString tmp = "/.local/share/applications/mimeapps.list";
    QString name = settings->value("defMimeAppsFile", tmp).toString();
    mimeUtils->setDefaultsFileName(name);

    // Create filesystem model
    bool realMime = settings->value("realMimeTypes", true).toBool();
    modelList = new myModel(realMime, mimeUtils);

    dockTree = new QDockWidget(tr("Tree"),this,Qt::SubWindow);
    dockTree->setObjectName("treeDock");

    tree = new QTreeView(dockTree);
    dockTree->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, dockTree);

    dockBookmarks = new QDockWidget(tr("Bookmarks"),this,Qt::SubWindow);
    dockBookmarks->setObjectName("bookmarksDock");
    dockBookmarks->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    bookmarksList = new QListView(dockBookmarks);
    bookmarksList->setMinimumHeight(24);	// Docks get the minimum size from their content widget
    dockBookmarks->setWidget(bookmarksList);
    addDockWidget(Qt::LeftDockWidgetArea, dockBookmarks);

    QWidget *main = new QWidget;
    mainLayout = new QVBoxLayout(main);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0,0,0,0);

    stackWidget = new QStackedWidget();
    QWidget *page = new QWidget();
    QHBoxLayout *hl1 = new QHBoxLayout(page);
    hl1->setSpacing(0);
    hl1->setContentsMargins(0,0,0,0);
    list = new QListView(page);
    hl1->addWidget(list);
    stackWidget->addWidget(page);

    QWidget *page2 = new QWidget();
    hl1 = new QHBoxLayout(page2);
    hl1->setSpacing(0);
    hl1->setContentsMargins(0,0,0,0);
    detailTree = new QTreeView(page2);
    hl1->addWidget(detailTree);
    stackWidget->addWidget(page2);

    tabs = new tabBar(modelList->folderIcons);

    mainLayout->addWidget(stackWidget);
    mainLayout->addWidget(tabs);

    setCentralWidget(main);

    modelTree = new mainTreeFilterProxyModel();
    modelTree->setSourceModel(modelList);
    modelTree->setSortCaseSensitivity(Qt::CaseInsensitive);

    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setModel(modelTree);
    tree->hideColumn(1);
    tree->hideColumn(2);
    tree->hideColumn(3);
    tree->hideColumn(4);

    modelView = new viewsSortProxyModel();
    modelView->setSourceModel(modelList);
    modelView->setSortCaseSensitivity(Qt::CaseInsensitive);

    list->setWrapping(true);
    list->setModel(modelView);
    listSelectionModel = list->selectionModel();

    detailTree->setRootIsDecorated(false);
    detailTree->setItemsExpandable(false);
    detailTree->setUniformRowHeights(true);
    detailTree->setModel(modelView);
    detailTree->setSelectionModel(listSelectionModel);

    pathEdit = new QComboBox();
    pathEdit->setEditable(true);
    pathEdit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

    status = statusBar();
    status->setSizeGripEnabled(true);
    statusName = new QLabel();
    statusSize = new QLabel();
    statusDate = new QLabel();
    status->addPermanentWidget(statusName);
    status->addPermanentWidget(statusSize);
    status->addPermanentWidget(statusDate);

    treeSelectionModel = tree->selectionModel();
    connect(treeSelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(treeSelectionChanged(QModelIndex,QModelIndex)));
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
    tree->scrollTo(tree->currentIndex());

    createActions();
    createToolBars();
    createMenus();

    setWindowIcon(QIcon(":/images/qtfm.png"));

    // Create custom action manager
    customActManager = new CustomActionsManager(settings, actionList, this);

    // Create bookmarks model
    modelBookmarks = new bookmarkmodel(modelList->folderIcons);

    // Load settings before showing window
    loadSettings();

    if (isDaemon) startDaemon();
    else show();

    QTimer::singleShot(0, this, SLOT(lateStart()));
}
//---------------------------------------------------------------------------

/**
 * @brief Initialization
 */
void MainWindow::lateStart() {

  // Update status panel
  status->showMessage(getDriveInfo(curIndex.filePath()));

  // Configure bookmarks list
  bookmarksList->setDragDropMode(QAbstractItemView::DragDrop);
  bookmarksList->setDropIndicatorShown(true);
  bookmarksList->setDefaultDropAction(Qt::MoveAction);
  bookmarksList->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // Configure tree view
  tree->setDragDropMode(QAbstractItemView::DragDrop);
  tree->setDefaultDropAction(Qt::MoveAction);
  tree->setDropIndicatorShown(true);
  tree->setEditTriggers(QAbstractItemView::EditKeyPressed |
                        QAbstractItemView::SelectedClicked);

  // Configure detail view
  detailTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  detailTree->setDragDropMode(QAbstractItemView::DragDrop);
  detailTree->setDefaultDropAction(Qt::MoveAction);
  detailTree->setDropIndicatorShown(true);
  detailTree->setEditTriggers(QAbstractItemView::EditKeyPressed |
                              QAbstractItemView::SelectedClicked);

  // Configure list view
  list->setResizeMode(QListView::Adjust);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list->setSelectionRectVisible(true);
  list->setFocus();
  list->setEditTriggers(QAbstractItemView::EditKeyPressed |
                        QAbstractItemView::SelectedClicked);

  // Watch for mounts
  int fd = open("/proc/self/mounts", O_RDONLY, 0);
  notify = new QSocketNotifier(fd, QSocketNotifier::Write);

  // Clipboard configuration
  progress = 0;
  clipboardChanged();

  // Completer configuration
  customComplete = new myCompleter;
  customComplete->setModel(modelTree);
  customComplete->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
  customComplete->setMaxVisibleItems(10);
  pathEdit->setCompleter(customComplete);

  // Tabs configuration
  tabs->setDrawBase(0);
  tabs->setExpanding(0);

  // Connect mouse clicks in views
  if (settings->value("singleClick").toInt() == 1) {
    connect(list, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listItemClicked(QModelIndex)));
    connect(detailTree, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listItemClicked(QModelIndex)));
  }
  if (settings->value("singleClick").toInt() == 2) {
    connect(list, SIGNAL(clicked(QModelIndex))
            ,this, SLOT(listDoubleClicked(QModelIndex)));
    connect(detailTree, SIGNAL(clicked(QModelIndex)),
            this, SLOT(listDoubleClicked(QModelIndex)));
  }

  // Connect list view
  connect(list, SIGNAL(activated(QModelIndex)),
          this, SLOT(listDoubleClicked(QModelIndex)));

  // Connect notifier
  connect(notify, SIGNAL(activated(int)), this, SLOT(mountWatcherTriggered()),
          Qt::QueuedConnection);

  // Connect custom action manager
  connect(customActManager, SIGNAL(actionMapped(QString)),
          SLOT(actionMapper(QString)));
  connect(customActManager, SIGNAL(actionsLoaded()), SLOT(readShortcuts()));
  connect(customActManager, SIGNAL(actionFinished()), SLOT(clearCutItems()));

  // Connect path edit
  connect(pathEdit, SIGNAL(activated(QString)),
          this, SLOT(pathEditChanged(QString)));
  connect(customComplete, SIGNAL(activated(QString)),
          this, SLOT(pathEditChanged(QString)));
  connect(pathEdit->lineEdit(), SIGNAL(cursorPositionChanged(int,int)),
          this, SLOT(addressChanged(int,int)));

  // Connect bookmarks
  connect(bookmarksList, SIGNAL(activated(QModelIndex)),
          this, SLOT(bookmarkClicked(QModelIndex)));
  connect(bookmarksList, SIGNAL(clicked(QModelIndex)),
          this, SLOT(bookmarkClicked(QModelIndex)));
  connect(bookmarksList, SIGNAL(pressed(QModelIndex)),
          this, SLOT(bookmarkPressed(QModelIndex)));

  // Connect selection
  connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
          this, SLOT(clipboardChanged()));
  connect(detailTree,SIGNAL(activated(QModelIndex)),
          this, SLOT(listDoubleClicked(QModelIndex)));
  connect(listSelectionModel,
          SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),
          this, SLOT(listSelectionChanged(const QItemSelection,
                                          const QItemSelection)));

  // Connect copy progress
  connect(this, SIGNAL(copyProgressFinished(int,QStringList)),
          this, SLOT(progressFinished(int,QStringList)));

  // Connect bookmark model
  connect(modelBookmarks,
          SIGNAL(bookmarkPaste(const QMimeData *, QString, QStringList)), this,
          SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));
  connect(modelBookmarks, SIGNAL(rowsInserted(QModelIndex, int, int)),
          this, SLOT(readShortcuts()));
  connect(modelBookmarks, SIGNAL(rowsRemoved(QModelIndex, int, int)),
          this, SLOT(readShortcuts()));

  // Conect list model
  connect(modelList,
          SIGNAL(dragDropPaste(const QMimeData *, QString, myModel::DragMode)),
          this,
          SLOT(dragLauncher(const QMimeData *, QString, myModel::DragMode)));

  // Connect tabs
  connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
  connect(tabs, SIGNAL(dragDropTab(const QMimeData *, QString, QStringList)),
          this, SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));
  connect(list, SIGNAL(pressed(QModelIndex)),
          this, SLOT(listItemPressed(QModelIndex)));
  connect(detailTree, SIGNAL(pressed(QModelIndex)),
          this, SLOT(listItemPressed(QModelIndex)));

  connect(modelList, SIGNAL(thumbUpdate(QModelIndex)),
          this, SLOT(thumbUpdate(QModelIndex)));

  qApp->setKeyboardInputInterval(1000);

  connect(&daemon, SIGNAL(newConnection()), this, SLOT(newConnection()));

  // Read custom actions
  QTimer::singleShot(100, customActManager, SLOT(readActions()));

  // Read defaults
  QTimer::singleShot(100, mimeUtils, SLOT(generateDefaults()));
}
//---------------------------------------------------------------------------

/**
 * @brief Loads application settings
 */
void MainWindow::loadSettings() {

  // Restore window state
  restoreState(settings->value("windowState").toByteArray(), 1);
  resize(settings->value("size", QSize(600, 400)).toSize());

  // Load info whether use real mime types
  modelList->setRealMimeTypes(settings->value("realMimeTypes", true).toBool());

  // Load information whether hidden files can be displayed
  hiddenAct->setChecked(settings->value("hiddenMode", 0).toBool());
  toggleHidden();

  // Remove old bookmarks
  modelBookmarks->removeRows(0, modelBookmarks->rowCount());

  // Load bookmarks
  settings->beginGroup("bookmarks");
  foreach (QString key,settings->childKeys()) {
    QStringList temp(settings->value(key).toStringList());
    modelBookmarks->addBookmark(temp[0], temp[1], temp[2], temp.last());
  }
  settings->endGroup();

  // Set bookmarks
  autoBookmarkMounts();
  bookmarksList->setModel(modelBookmarks);
  bookmarksList->setResizeMode(QListView::Adjust);
  bookmarksList->setFlow(QListView::TopToBottom);
  bookmarksList->setIconSize(QSize(24,24));

  // Load information whether bookmarks are displayed
  wrapBookmarksAct->setChecked(settings->value("wrapBookmarks", 0).toBool());
  bookmarksList->setWrapping(wrapBookmarksAct->isChecked());

  // Lock information whether layout is locked or not
  lockLayoutAct->setChecked(settings->value("lockLayout", 0).toBool());
  toggleLockLayout();

  // Load zoom settings
  zoom = settings->value("zoom", 48).toInt();
  zoomTree = settings->value("zoomTree", 16).toInt();
  zoomList = settings->value("zoomList", 24).toInt();
  zoomDetail = settings->value("zoomDetail", 16).toInt();
  detailTree->setIconSize(QSize(zoomDetail, zoomDetail));
  tree->setIconSize(QSize(zoomTree, zoomTree));

  // Load information whether thumbnails can be shown
  thumbsAct->setChecked(settings->value("showThumbs", 1).toBool());

  // Load view mode
  detailAct->setChecked(settings->value("viewMode", 0).toBool());
  iconAct->setChecked(settings->value("iconMode", 0).toBool());
  toggleDetails();

  // Restore header of detail tree
  detailTree->header()->restoreState(settings->value("header").toByteArray());
  detailTree->setSortingEnabled(1);

  // Load sorting information and sort
  currentSortColumn = settings->value("sortBy", 0).toInt();
  currentSortOrder = (Qt::SortOrder) settings->value("sortOrder", 0).toInt();
  switch (currentSortColumn) {
    case 0 : setSortColumn(sortNameAct); break;
    case 1 : setSortColumn(sortSizeAct); break;
    case 3 : setSortColumn(sortDateAct); break;
  }
  setSortOrder(currentSortOrder);
  modelView->sort(currentSortColumn, currentSortOrder);

  // Load terminal command
  term = settings->value("term").toString();

  // Load information whether tabs can be shown on top
  tabsOnTopAct->setChecked(settings->value("tabsOnTop", 0).toBool());
  tabsOnTop();
}
//---------------------------------------------------------------------------

/**
 * @brief Close event
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event) {

  // Save settings
  writeSettings();

  // If deamon, ignore event
  if (isDaemon) {
    this->setVisible(0);
    startDaemon();
    customComplete->setModel(0);
    modelList->refresh();
    tabs->setCurrentIndex(0);
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
    tree->scrollTo(tree->currentIndex());
    customComplete->setModel(modelTree);
    event->ignore();
  } else {
    modelList->cacheInfo();
    event->accept();
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Closes main window
 */
void MainWindow::exitAction() {
  isDaemon = 0;
  close();
}
//---------------------------------------------------------------------------

void MainWindow::treeSelectionChanged(QModelIndex current, QModelIndex previous)
{
    QFileInfo name = modelList->fileInfo(modelTree->mapToSource(current));
    if(!name.exists()) return;

    curIndex = name;
    setWindowTitle(curIndex.fileName() + " - QtFM 5.8");

    if(tree->hasFocus() && QApplication::mouseButtons() == Qt::MidButton)
    {
        listItemPressed(modelView->mapFromSource(modelList->index(name.filePath())));
        tabs->setCurrentIndex(tabs->count() - 1);
        if(currentView == 2) detailTree->setFocus(Qt::TabFocusReason);
        else list->setFocus(Qt::TabFocusReason);
    }

    if(curIndex.filePath() != pathEdit->itemText(0))
    {
        if(tabs->count()) tabs->addHistory(curIndex.filePath());
        pathEdit->insertItem(0,curIndex.filePath());
        pathEdit->setCurrentIndex(0);
    }

    if(!bookmarksList->hasFocus()) bookmarksList->clearSelection();

    if(modelList->setRootPath(name.filePath())) modelView->invalidate();

    //////
    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(name.filePath()));

    if(currentView == 2) detailTree->setRootIndex(baseIndex);
    else list->setRootIndex(baseIndex);

    if(tabs->count())
    {
        tabs->setTabText(tabs->currentIndex(),curIndex.fileName());
        tabs->setTabData(tabs->currentIndex(),curIndex.filePath());
        tabs->setIcon(tabs->currentIndex());
    }

    if(backIndex.isValid())
    {
        listSelectionModel->setCurrentIndex(modelView->mapFromSource(backIndex),QItemSelectionModel::ClearAndSelect);
        if(currentView == 2) detailTree->scrollTo(modelView->mapFromSource(backIndex));
        else list->scrollTo(modelView->mapFromSource(backIndex));
    }
    else
    {
        listSelectionModel->blockSignals(1);
        listSelectionModel->clear();
    }

    listSelectionModel->blockSignals(0);
    QTimer::singleShot(30,this,SLOT(dirLoaded()));
}

//---------------------------------------------------------------------------
void MainWindow::dirLoaded()
{
    if(backIndex.isValid())
    {
        backIndex = QModelIndex();
        return;
    }

    qint64 bytes = 0;
    QModelIndexList items;
    bool includeHidden = hiddenAct->isChecked();

    for(int x = 0; x < modelList->rowCount(modelList->index(pathEdit->currentText())); ++x)
        items.append(modelList->index(x,0,modelList->index(pathEdit->currentText())));


    foreach(QModelIndex theItem,items)
    {
        if(includeHidden || !modelList->fileInfo(theItem).isHidden())
            bytes = bytes + modelList->size(theItem);
        else
            items.removeOne(theItem);
    }

    QString total;

    if(!bytes) total = "";
    else total = formatSize(bytes);

    statusName->clear();
    statusSize->setText(QString("%1 items").arg(items.count()));
    statusDate->setText(QString("%1").arg(total));

    if(thumbsAct->isChecked()) QtConcurrent::run(modelList,&myModel::loadThumbs,items);
}

//---------------------------------------------------------------------------
void MainWindow::thumbUpdate(QModelIndex index)
{
    if(currentView == 2) detailTree->update(modelView->mapFromSource(index));
    else list->update(modelView->mapFromSource(index));
}

//---------------------------------------------------------------------------
void MainWindow::listSelectionChanged(const QItemSelection selected, const QItemSelection deselected)
{
    QModelIndexList items;

    if(listSelectionModel->selectedRows(0).count()) items = listSelectionModel->selectedRows(0);
    else items = listSelectionModel->selectedIndexes();

    statusSize->clear();
    statusDate->clear();
    statusName->clear();

    if(items.count() == 0)
    {
        curIndex = pathEdit->itemText(0);
        return;
    }

    if(QApplication::focusWidget() != bookmarksList) bookmarksList->clearSelection();

    curIndex = modelList->fileInfo(modelView->mapToSource(listSelectionModel->currentIndex()));

    qint64 bytes = 0;
    int folders = 0;
    int files = 0;

    foreach(QModelIndex theItem,items)
    {
        if(modelList->isDir(modelView->mapToSource(theItem))) folders++;
        else files++;

        bytes = bytes + modelList->size(modelView->mapToSource(theItem));
    }

    QString total,name;

    if(!bytes) total = "";
    else total = formatSize(bytes);

    if(items.count() == 1)
    {
        QFileInfo file(modelList->filePath(modelView->mapToSource(items.at(0))));

        name = file.fileName();
        if(file.isSymLink()) name = "Link --> " + file.symLinkTarget();

        statusName->setText(name + "   ");
        statusSize->setText(QString("%1   ").arg(total));
        statusDate->setText(QString("%1").arg(file.lastModified().toString(Qt::SystemLocaleShortDate)));
    }
    else
    {
        statusName->setText(total + "   ");
        if(files) statusSize->setText(QString("%1 files  ").arg(files));
        if(folders) statusDate->setText(QString("%1 folders").arg(folders));
    }
}

//---------------------------------------------------------------------------
QString formatSize(qint64 num)
{
    QString total;
    const qint64 kb = 1024;
    const qint64 mb = 1024 * kb;
    const qint64 gb = 1024 * mb;
    const qint64 tb = 1024 * gb;

    if (num >= tb) total = QString("%1TB").arg(QString::number(qreal(num) / tb, 'f', 2));
    else if(num >= gb) total = QString("%1GB").arg(QString::number(qreal(num) / gb, 'f', 2));
    else if(num >= mb) total = QString("%1MB").arg(QString::number(qreal(num) / mb, 'f', 1));
    else if(num >= kb) total = QString("%1KB").arg(QString::number(qreal(num) / kb,'f', 1));
    else total = QString("%1 bytes").arg(num);

    return total;
}

//---------------------------------------------------------------------------
void MainWindow::listItemClicked(QModelIndex current)
{
    if(modelList->filePath(modelView->mapToSource(current)) == pathEdit->currentText()) return;

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if(mods == Qt::ControlModifier || mods == Qt::ShiftModifier) return;
    if(modelList->isDir(modelView->mapToSource(current)))
        tree->setCurrentIndex(modelTree->mapFromSource(modelView->mapToSource(current)));
}

//---------------------------------------------------------------------------
void MainWindow::listItemPressed(QModelIndex current)
{
    //middle-click -> open new tab
    //ctrl+middle-click -> open new instance

    if(QApplication::mouseButtons() == Qt::MidButton)
        if(modelList->isDir(modelView->mapToSource(current)))
        {
            if(QApplication::keyboardModifiers() == Qt::ControlModifier)
                openFile();
            else
                addTab(modelList->filePath(modelView->mapToSource(current)));
        }
        else
            openFile();
}

//---------------------------------------------------------------------------
void MainWindow::openTab()
{
    if(curIndex.isDir())
        addTab(curIndex.filePath());
}

//---------------------------------------------------------------------------
int MainWindow::addTab(QString path)
{
    if(tabs->count() == 0) tabs->addNewTab(pathEdit->currentText(),currentView);
    return tabs->addNewTab(path,currentView);
}

//---------------------------------------------------------------------------
void MainWindow::tabsOnTop()
{
    if(tabsOnTopAct->isChecked())
    {
        mainLayout->setDirection(QBoxLayout::BottomToTop);
        tabs->setShape(QTabBar::RoundedNorth);
    }
    else
    {
        mainLayout->setDirection(QBoxLayout::TopToBottom);
        tabs->setShape(QTabBar::RoundedSouth);
    }
}

//---------------------------------------------------------------------------
void MainWindow::tabChanged(int index)
{
    if(tabs->count() == 0) return;

    pathEdit->clear();
    pathEdit->addItems(*tabs->getHistory(index));

    int type = tabs->getType(index);
    if(currentView != type)
    {
        if(type == 2) detailAct->setChecked(1);
        else detailAct->setChecked(0);

        if(type == 1) iconAct->setChecked(1);
        else iconAct->setChecked(0);

        toggleDetails();
    }

    if(!tabs->tabData(index).toString().isEmpty())
        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(tabs->tabData(index).toString())));
}


//---------------------------------------------------------------------------

/**
 * @brief Doubleclick on icon/launcher
 * @param current
 */
void MainWindow::listDoubleClicked(QModelIndex current) {
  Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
  if (mods == Qt::ControlModifier || mods == Qt::ShiftModifier) {
    return;
  }
  if (modelList->isDir(modelView->mapToSource(current))) {
    QModelIndex i = modelView->mapToSource(current);
    tree->setCurrentIndex(modelTree->mapFromSource(i));
  } else {
    executeFile(current, 0);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Reaction for change of path edit (location edit)
 * @param path
 */
void MainWindow::pathEditChanged(QString path) {
  QString info = path;
  if (!QFileInfo(path).exists()) return;
  info.replace("~",QDir::homePath());
  tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(info)));
}
//---------------------------------------------------------------------------

/**
 * @brief Reaction for change of clippboard content
 */
void MainWindow::clipboardChanged() {
  if (QApplication::clipboard()->mimeData()->hasUrls()) {
    pasteAct->setEnabled(true);
  } else {
    modelList->clearCutItems();
    pasteAct->setEnabled(false);
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Pastes from clipboard
 */
void MainWindow::pasteClipboard() {
  QString newPath;
  QStringList cutList;

  if (curIndex.isDir()) newPath = curIndex.filePath();
  else newPath = pathEdit->itemText(0);

  // Check list of files that are to be cut
  QFile tempFile(QDir::tempPath() + "/qtfm.temp");
  if (tempFile.exists()) {
    tempFile.open(QIODevice::ReadOnly);
    QDataStream out(&tempFile);
    out >> cutList;
    tempFile.close();
  }
  pasteLauncher(QApplication::clipboard()->mimeData(), newPath, cutList);
}
//---------------------------------------------------------------------------

/**
 * @brief Drags data to the new location
 * @param data data to be pasted
 * @param newPath path of new location
 * @param dragMode mode of dragging
 */
void MainWindow::dragLauncher(const QMimeData *data, const QString &newPath,
                              myModel::DragMode dragMode) {

  // Retrieve urls (paths) of data
  QList<QUrl> files = data->urls();

  // If drag mode is unknown then ask what to do
  if (dragMode == myModel::DM_UNKNOWN) {
    QMessageBox box;
    box.setWindowTitle(tr("What do you want to do?"));
    QAbstractButton *move = box.addButton(tr("Move here"), QMessageBox::ActionRole);
    QAbstractButton *copy = box.addButton(tr("Copy here"), QMessageBox::ActionRole);
    QAbstractButton *link = box.addButton(tr("Link here"), QMessageBox::ActionRole);
    QAbstractButton *canc = box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == move) {
      dragMode = myModel::DM_MOVE;
    } else if (box.clickedButton() == copy) {
      dragMode = myModel::DM_COPY;
    } else if (box.clickedButton() == link) {
      dragMode = myModel::DM_LINK;
    } else if (box.clickedButton() == canc) {
      return;
    }
  }

  // If moving is enabled, cut files from the original location
  QStringList cutList;
  if (dragMode == myModel::DM_MOVE) {
    foreach (QUrl item, files) {
      cutList.append(item.path());
    }
  }

  // Paste launcher (this method has to be called instead of that with 'data'
  // parameter, because that 'data' can timeout)
  pasteLauncher(files, newPath, cutList, dragMode == myModel::DM_LINK);
}
//---------------------------------------------------------------------------

/**
 * @brief Pastes data to the new location
 * @param data data to be pasted
 * @param newPath path of new location
 * @param cutList list of items to remove
 */
void MainWindow::pasteLauncher(const QMimeData *data, const QString &newPath,
                               const QStringList &cutList) {
  QList<QUrl> files = data->urls();
  pasteLauncher(files, newPath, cutList);
}
//---------------------------------------------------------------------------

/**
 * @brief Pastes files to the new path
 * @param files list of files
 * @param newPath new path
 * @param cutList files to remove from original path
 * @param link true if link should be created (default value = false)
 */
void MainWindow::pasteLauncher(const QList<QUrl> &files, const QString &newPath,
                               const QStringList &cutList, bool link) {

  // File no longer exists?
  if (!QFile(files.at(0).path()).exists()) {
    QString msg = tr("File '%1' no longer exists!").arg(files.at(0).path());
    QMessageBox::information(this, tr("No paste for you!"), msg);
    pasteAct->setEnabled(false);
    return;
  }

  // Temporary variables
  int replace = 0;
  QStringList completeList;
  QString baseName = QFileInfo(files.at(0).toLocalFile()).path();

  // Only if not in same directory, otherwise we will do 'Copy(x) of'
  if (newPath != baseName) {

    foreach (QUrl file, files) {

      // Merge or replace?
      QFileInfo temp(file.toLocalFile());

      if (temp.isDir() && QFileInfo(newPath + QDir::separator() + temp.fileName()).exists()) {
        QString msg = QString("<b>%1</b><p>Already exists!<p>What do you want to do?").arg(newPath + QDir::separator() + temp.fileName());
        QMessageBox message(QMessageBox::Question, tr("Existing folder"), msg, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        message.button(QMessageBox::Yes)->setText(tr("Merge"));
        message.button(QMessageBox::No)->setText(tr("Replace"));

        int merge = message.exec();
        if (merge == QMessageBox::Cancel) return;
        if (merge == QMessageBox::Yes) {
          FileUtils::recurseFolder(temp.filePath(), temp.fileName(), &completeList);
        }
        else {
          FileUtils::removeRecurse(newPath, temp.fileName());
        }
      }
      else completeList.append(temp.fileName());
    }

    // Ask whether replace files if files with same name already exist in
    // destination directory
    foreach (QString file, completeList) {
      QFileInfo temp(newPath + QDir::separator() + file);
      if (temp.exists()) {
        QFileInfo orig(baseName + QDir::separator() + file);
        if (replace != QMessageBox::YesToAll && replace != QMessageBox::NoToAll) {
          // TODO: error dispalys only at once
          replace = showReplaceMsgBox(temp, orig);
        }
        if (replace == QMessageBox::Cancel) {
          return;
        }
        if (replace == QMessageBox::Yes || replace == QMessageBox::YesToAll) {
          QFile(temp.filePath()).remove();
        }
      }
    }
  }

  // If only links should be created, create them and exit
  if (link) {
    linkFiles(files, newPath);
    return;
  }

  // Copy/move files
  QString title = cutList.count() == 0 ? tr("Copying...") : tr("Moving...");
  progress = new myProgressDialog(title);
  connect(this, SIGNAL(updateCopyProgress(qint64, qint64, QString)), progress, SLOT(update(qint64, qint64, QString)));
  listSelectionModel->clear();
  QtConcurrent::run(this, &MainWindow::pasteFiles, files, newPath, cutList);
}
//---------------------------------------------------------------------------

/**
 * @brief Asks user whether replace file 'f1' with another file 'f2'
 * @param f1 file to be replaced with f2
 * @param f2 file to replace f1
 * @return result
 */
int MainWindow::showReplaceMsgBox(const QFileInfo &f1, const QFileInfo &f2) {

  // Create message
  QString t = tr("Do you want to replace:<p><b>%1</p><p>Modified: %2<br>"
                 "Size: %3 bytes</p><p>with:<p><b>%4</p><p>Modified: %5"
                 "<br>Size: %6 bytes</p>");

  // Populate message with data
  t = t.arg(f1.filePath()).arg(f1.lastModified().toString()).arg(f1.size())
       .arg(f2.filePath()).arg(f2.lastModified().toString()).arg(f2.size());

  // Show message
  return QMessageBox::question(0, tr("Replace"), t, QMessageBox::Yes
                               | QMessageBox::YesToAll | QMessageBox::No
                               | QMessageBox::NoToAll | QMessageBox::Cancel);
}
//---------------------------------------------------------------------------

void MainWindow::progressFinished(int ret,QStringList newFiles)
{
    if(progress != 0)
    {
        progress->close();
        delete progress;
        progress = 0;
    }

    if(newFiles.count())
    {
        disconnect(listSelectionModel,SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),this,SLOT(listSelectionChanged(const QItemSelection, const QItemSelection)));

        qApp->processEvents();              //make sure notifier has added new files to the model

        if(QFileInfo(newFiles.first()).path() == pathEdit->currentText())       //highlight new files if visible
        {
            foreach(QString item, newFiles)
                listSelectionModel->select(modelView->mapFromSource(modelList->index(item)),QItemSelectionModel::Select);
        }

        connect(listSelectionModel,SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),this,SLOT(listSelectionChanged(const QItemSelection, const QItemSelection)));
        curIndex.setFile(newFiles.first());

        if(currentView == 2) detailTree->scrollTo(modelView->mapFromSource(modelList->index(newFiles.first())),QAbstractItemView::EnsureVisible);
        else list->scrollTo(modelView->mapFromSource(modelList->index(newFiles.first())),QAbstractItemView::EnsureVisible);

        if(QFile(QDir::tempPath() + "/qtfm.temp").exists()) QApplication::clipboard()->clear();

        clearCutItems();
    }

    if(ret == 1) QMessageBox::information(this,tr("Failed"),tr("Paste failed...do you have write permissions?"));
    if(ret == 2) QMessageBox::warning(this,tr("Too big!"),tr("There is not enough space on the destination drive!"));
}

//---------------------------------------------------------------------------

void MainWindow::folderPropertiesLauncher()
{
    QModelIndexList selList;
    if(focusWidget() == bookmarksList) selList.append(modelView->mapFromSource(modelList->index(bookmarksList->currentIndex().data(32).toString())));
    else if(focusWidget() == list || focusWidget() == detailTree)
        if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
        else selList = listSelectionModel->selectedIndexes();

    if(selList.count() == 0) selList << modelView->mapFromSource(modelList->index(pathEdit->currentText()));

    QStringList paths;

    foreach(QModelIndex item, selList)
        paths.append(modelList->filePath(modelView->mapToSource(item)));

    properties = new PropertiesDialog(paths, modelList);
    connect(properties,SIGNAL(propertiesUpdated()),this,SLOT(clearCutItems()));
}

//---------------------------------------------------------------------------

/**
 * @brief Writes settings into config file
 */
void MainWindow::writeSettings() {

  // Write general settings
  settings->setValue("size", size());
  settings->setValue("viewMode", stackWidget->currentIndex());
  settings->setValue("iconMode", iconAct->isChecked());
  settings->setValue("zoom", zoom);
  settings->setValue("zoomTree", zoomTree);
  settings->setValue("zoomList", zoomList);
  settings->setValue("zoomDetail", zoomDetail);
  settings->setValue("sortBy", currentSortColumn);
  settings->setValue("sortOrder", currentSortOrder);
  settings->setValue("showThumbs", thumbsAct->isChecked());
  settings->setValue("hiddenMode", hiddenAct->isChecked());
  settings->setValue("lockLayout", lockLayoutAct->isChecked());
  settings->setValue("tabsOnTop", tabsOnTopAct->isChecked());
  settings->setValue("windowState", saveState(1));
  settings->setValue("header", detailTree->header()->saveState());
  settings->setValue("realMimeTypes",  modelList->isRealMimeTypes());

  // Write bookmarks
  settings->remove("bookmarks");
  settings->beginGroup("bookmarks");
  for (int i = 0; i < modelBookmarks->rowCount(); i++) {
    QStringList temp;
    temp << modelBookmarks->item(i)->text()
         << modelBookmarks->item(i)->data(32).toString()
         << modelBookmarks->item(i)->data(34).toString()
         << modelBookmarks->item(i)->data(33).toString();
    settings->setValue(QString(i),temp);
  }
  settings->endGroup();
}
//---------------------------------------------------------------------------

/**
 * @brief Display popup menu
 * @param event
 */
void MainWindow::contextMenuEvent(QContextMenuEvent * event) {

  // Retreive widget under mouse
  QMenu *popup;
  QWidget *widget = childAt(event->pos());

  // Create popup for tab or for status bar
  if (widget == tabs) {
    popup = new QMenu(this);
    popup->addAction(closeTabAct);
    popup->exec(event->globalPos());
    return;
  } else if (widget == status) {
    popup = createPopupMenu();
    popup->addSeparator();
    popup->addAction(lockLayoutAct);
    popup->exec(event->globalPos());
    return;
  }

  // Continue with poups for folders and files
  QList<QAction*> actions;
  popup = new QMenu(this);

  if (focusWidget() == list || focusWidget() == detailTree) {

    // Clear selection in bookmarks
    bookmarksList->clearSelection();

    // Could be file or folder
    if (listSelectionModel->hasSelection())	{

      // Get index of source model
      curIndex = modelList->filePath(modelView->mapToSource(listSelectionModel->currentIndex()));

      // File
      if (!curIndex.isDir()) {
        QString type = modelList->getMimeType(modelList->index(curIndex.filePath()));

        // Add custom actions to the list of actions
        QHashIterator<QString, QAction*> i(*customActManager->getActions());
        while (i.hasNext()) {
          i.next();
          if (type.contains(i.key())) actions.append(i.value());
        }

        // Add run action or open with default application action
        if (curIndex.isExecutable()) {
          popup->addAction(runAct);
        } else {
          popup->addAction(openAct);
        }

        // Add open action
        /*foreach (QAction* action, actions) {
          if (action->text() == "Open") {
            popup->addAction(action);
            break;
          }
        }*/

        // Add open with menu
        popup->addMenu(createOpenWithMenu());

        //if (popup->actions().count() == 0) popup->addAction(openAct);

        // Add custom actions that are associated only with this file type
        if (!actions.isEmpty()) {
          popup->addSeparator();
          popup->addActions(actions);
        }

        // Add menus
        QHashIterator<QString, QMenu*> m(*customActManager->getMenus());
        while (m.hasNext()) {
          m.next();
          if (type.contains(m.key())) popup->addMenu(m.value());
        }

        // Add cut/copy/paste/rename actions
        popup->addSeparator();
        popup->addAction(cutAct);
        popup->addAction(copyAct);
        popup->addAction(pasteAct);
        popup->addSeparator();
        popup->addAction(renameAct);
        popup->addSeparator();

        // Add custom actions that are associated with all file types
        foreach (QMenu* parent, customActManager->getMenus()->values("*")) {
          popup->addMenu(parent);
        }
        actions = (customActManager->getActions()->values("*"));
        popup->addActions(actions);
        popup->addAction(deleteAct);
        popup->addSeparator();
        actions = customActManager->getActions()->values(curIndex.path());    //children of $parent
        if (actions.count()) {
          popup->addActions(actions);
          popup->addSeparator();
        }
      }
      // Folder/directory
      else {
        popup->addAction(openAct);
        popup->addSeparator();
        popup->addAction(cutAct);
        popup->addAction(copyAct);
        popup->addAction(pasteAct);
        popup->addSeparator();
        popup->addAction(renameAct);
        popup->addSeparator();

        foreach (QMenu* parent, customActManager->getMenus()->values("*")) {
          popup->addMenu(parent);
        }

        actions = customActManager->getActions()->values("*");
        popup->addActions(actions);
        popup->addAction(deleteAct);
        popup->addSeparator();

        foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
          popup->addMenu(parent);
        }
        actions = customActManager->getActions()->values(curIndex.fileName());   // specific folder
        actions.append(customActManager->getActions()->values(curIndex.path())); // children of $parent
        actions.append(customActManager->getActions()->values("folder"));        // all folders
        if (actions.count()) {
          popup->addActions(actions);
          popup->addSeparator();
        }
      }
      popup->addAction(folderPropertiesAct);
    }
    // Whitespace
    else {
      popup->addAction(newDirAct);
      popup->addAction(newFileAct);
      popup->addSeparator();
      if (pasteAct->isEnabled()) {
        popup->addAction(pasteAct);
        popup->addSeparator();
      }
      popup->addAction(addBookmarkAct);
      popup->addSeparator();

      foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
        popup->addMenu(parent);
      }
      actions = customActManager->getActions()->values(curIndex.fileName());
      actions.append(customActManager->getActions()->values("folder"));
      if (actions.count()) {
        foreach (QAction*action, actions) {
          popup->addAction(action);
        }
        popup->addSeparator();
      }
      popup->addAction(folderPropertiesAct);
    }
  }
  // Tree or bookmarks
  else {
    if (focusWidget() == bookmarksList) {
      listSelectionModel->clearSelection();
      if (bookmarksList->indexAt(bookmarksList->mapFromGlobal(event->globalPos())).isValid()) {
        curIndex = bookmarksList->currentIndex().data(32).toString();
        popup->addAction(delBookmarkAct);
        popup->addAction(editBookmarkAct);	//icon
      } else {
        bookmarksList->clearSelection();
        popup->addAction(addSeparatorAct);	//seperator
        popup->addAction(wrapBookmarksAct);
      }
      popup->addSeparator();
    } else {
      bookmarksList->clearSelection();
      popup->addAction(newDirAct);
      popup->addAction(newFileAct);
      popup->addAction(openTabAct);
      popup->addSeparator();
      popup->addAction(cutAct);
      popup->addAction(copyAct);
      popup->addAction(pasteAct);
      popup->addSeparator();
      popup->addAction(renameAct);
      popup->addSeparator();
      popup->addAction(deleteAct);
    }
    popup->addSeparator();

    foreach (QMenu* parent, customActManager->getMenus()->values("folder")) {
      popup->addMenu(parent);
    }
    actions = customActManager->getActions()->values(curIndex.fileName());
    actions.append(customActManager->getActions()->values(curIndex.path()));
    actions.append(customActManager->getActions()->values("folder"));
    if (actions.count()) {
      foreach (QAction*action, actions) popup->addAction(action);
      popup->addSeparator();
    }
    popup->addAction(folderPropertiesAct);
  }

  popup->exec(event->globalPos());
  delete popup;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates menu for opening file in selected application
 * @return menu
 */
QMenu* MainWindow::createOpenWithMenu() {

  // Add open with functionality ...
  QMenu *openMenu = new QMenu(tr("Open with"));

  // Select action
  QAction *selectAppAct = new QAction(tr("Select..."), openMenu);
  selectAppAct->setStatusTip(tr("Select application for opening the file"));
  //selectAppAct->setIcon(actionIcons->at(18));
  connect(selectAppAct, SIGNAL(triggered()), this, SLOT(selectApp()));

  // Load default applications for current mime
  QString mime = mimeUtils->getMimeType(curIndex.filePath());
  Properties defaults = mimeUtils->loadDefaults();
  QStringList appNames = defaults.value(mime).toString().split(";");

  // Create actions for opening
  QList<QAction*> defaultApps;
  foreach (QString appName, appNames) {

    // Skip empty app name
    if (appName.isEmpty()) {
      continue;
    }

    // Load desktop file for application
    DesktopFile df = DesktopFile("/usr/share/applications/" + appName);

    // Create action
    QAction* action = new QAction(df.getName(), openMenu);
    action->setData(df.getExec());
    action->setIcon(FileUtils::searchAppIcon(df));
    defaultApps.append(action);

    // TODO: icon and connect
    connect(action, SIGNAL(triggered()), SLOT(openInApp()));

    // Add action to menu
    openMenu->addAction(action);
  }

  // Add open action to menu
  if (!defaultApps.isEmpty()) {
    openMenu->addSeparator();
  }
  openMenu->addAction(selectAppAct);
  return openMenu;
}
//---------------------------------------------------------------------------

/**
 * @brief Selects application for opening file
 */
void MainWindow::selectApp() {

  // Select application in the dialog
  ApplicationDialog *dialog = new ApplicationDialog(this);
  if (dialog->exec()) {
    if (dialog->getCurrentLauncher().compare("") != 0) {
      QString appName = dialog->getCurrentLauncher() + ".desktop";
      DesktopFile df = DesktopFile("/usr/share/applications/" + appName);
      mimeUtils->openInApp(df.getExec(), curIndex, this);
    }
  }
}
//---------------------------------------------------------------------------

/**
 * @brief Opens file in application
 */
void MainWindow::openInApp() {
  QAction* action = dynamic_cast<QAction*>(sender());
  if (action) {
    mimeUtils->openInApp(action->data().toString(), curIndex, this);
  }
}
//---------------------------------------------------------------------------

void MainWindow::actionMapper(QString cmd)
{
    QModelIndexList selList;
    QStringList temp;

    if(focusWidget() == list || focusWidget() == detailTree)
    {
        QFileInfo file = modelList->fileInfo(modelView->mapToSource(listSelectionModel->currentIndex()));

        if(file.isDir())
            cmd.replace("%n",file.fileName().replace(" ","\\"));
        else
            cmd.replace("%n",file.baseName().replace(" ","\\"));

        if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
        else selList = listSelectionModel->selectedIndexes();
    }
    else
        selList << modelView->mapFromSource(modelList->index(curIndex.filePath()));


    cmd.replace("~",QDir::homePath());


    //process any input tokens
    int pos = 0;
    while(pos >= 0)
    {
        pos = cmd.indexOf("%i",pos);
        if(pos != -1)
        {
            pos += 2;
            QString var = cmd.mid(pos,cmd.indexOf(" ",pos) - pos);
            QString input = QInputDialog::getText(this,tr("Input"), var, QLineEdit::Normal);
            if(input.isNull()) return;              //cancelled
            else cmd.replace("%i" + var,input);
        }
    }


    foreach(QModelIndex index,selList)
        temp.append(modelList->fileName(modelView->mapToSource(index)).replace(" ","\\"));

    cmd.replace("%f",temp.join(" "));

    temp.clear();

    foreach(QModelIndex index,selList)
        temp.append(modelList->filePath(modelView->mapToSource(index)).replace(" ","\\"));

    cmd.replace("%F",temp.join(" "));

    customActManager->execAction(cmd, pathEdit->itemText(0));
}
//---------------------------------------------------------------------------

void MainWindow::refresh()
{
    QApplication::clipboard()->clear();
    listSelectionModel->clear();

    modelList->refreshItems();
    modelTree->invalidate();
    modelTree->sort(0,Qt::AscendingOrder);
    modelView->invalidate();
    dirLoaded();

    return;
}

//---------------------------------------------------------------------------------
void MainWindow::newConnection()
{
    showNormal();
    daemon.close();
}

//---------------------------------------------------------------------------------
void MainWindow::startDaemon()
{
    if(!daemon.listen("qtfm")) isDaemon = 0;
}

//---------------------------------------------------------------------------------
void MainWindow::clearCutItems()
{
    //this refreshes existing items, sizes etc but doesn't re-sort
    modelList->clearCutItems();
    modelList->update();

    QModelIndex baseIndex = modelView->mapFromSource(modelList->index(pathEdit->currentText()));

    if(currentView == 2) detailTree->setRootIndex(baseIndex);
    else list->setRootIndex(baseIndex);
    QTimer::singleShot(50,this,SLOT(dirLoaded()));
    return;
}

//---------------------------------------------------------------------------------
bool mainTreeFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    myModel* fileModel = qobject_cast<myModel*>(sourceModel());

    if(fileModel->isDir(index0))
        if(this->filterRegExp().isEmpty() || fileModel->fileInfo(index0).isHidden() == 0) return true;

    return false;
}

//---------------------------------------------------------------------------------
bool viewsSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if(this->filterRegExp().isEmpty()) return true;

    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    myModel* fileModel = qobject_cast<myModel*>(sourceModel());

    if(fileModel->fileInfo(index0).isHidden()) return false;
    else return true;
}

//---------------------------------------------------------------------------------
bool viewsSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    myModel* fsModel = dynamic_cast<myModel*>(sourceModel());

    if((fsModel->isDir(left) && !fsModel->isDir(right)))
        return sortOrder() == Qt::AscendingOrder;
    else if(!fsModel->isDir(left) && fsModel->isDir(right))
        return sortOrder() == Qt::DescendingOrder;

    if(left.column() == 1)          //size
    {
        if(fsModel->size(left) > fsModel->size(right)) return true;
        else return false;
    }
    else
    if(left.column() == 3)          //date
    {
        if(fsModel->fileInfo(left).lastModified() > fsModel->fileInfo(right).lastModified()) return true;
        else return false;
    }

    return QSortFilterProxyModel::lessThan(left,right);
}

//---------------------------------------------------------------------------------
QStringList myCompleter::splitPath(const QString& path) const
{
    QStringList parts = path.split("/");
    parts[0] = "/";

    return parts;
}

//---------------------------------------------------------------------------------
QString myCompleter::pathFromIndex(const QModelIndex& index) const
{
    if(!index.isValid()) return "";

    QModelIndex idx = index;
    QStringList list;
    do
    {
        QString t = model()->data(idx, Qt::EditRole).toString();
        list.prepend(t);
        QModelIndex parent = idx.parent();
        idx = parent.sibling(parent.row(), index.column());
    }
    while (idx.isValid());

    list[0].clear() ; // the join below will provide the separator

    return list.join("/");
}
