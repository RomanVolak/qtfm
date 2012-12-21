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
#include <sys/vfs.h>
#include <fcntl.h>

#include "mainwindow.h"
#include "mymodel.h"
#include "actions.cpp"
#include "bookmarks.cpp"
#include "progressdlg.h"
#include "fileutils.h"
#include "settingsdialog.h"

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

    modelList = new myModel(settings->value("realMimeTypes").toBool());

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
    connect(treeSelectionModel,SIGNAL(currentChanged(QModelIndex,QModelIndex)),this,SLOT(treeSelectionChanged(QModelIndex,QModelIndex)));
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
    tree->scrollTo(tree->currentIndex());

    createActions();
    createToolBars();
    createMenus();

    setWindowIcon(QIcon(":/images/qtfm.png"));

    // Creates custom action manager
    customActManager = new CustomActionsManager(settings, actionList, this);

    // Creates bookmarks model
    modelBookmarks = new bookmarkmodel(modelList->folderIcons);


    if(isDaemon) startDaemon();
    else show();

    loadSettings();

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
}
//---------------------------------------------------------------------------

/**
 * @brief Loads application settings
 */
void MainWindow::loadSettings() {

  // Restore window state
  restoreState(settings->value("windowState").toByteArray(), 1);
  resize(settings->value("size", QSize(600, 400)).toSize());

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

  // Load information whether hidden files can be displayed
  hiddenAct->setChecked(settings->value("hiddenMode", 0).toBool());
  toggleHidden();

  // Restore header of detail tree
  detailTree->header()->restoreState(settings->value("header").toByteArray());
  detailTree->setSortingEnabled(1);

  // Load sorting information
  currentSortColumn = settings->value("sortBy", 0).toInt();
  currentSortOrder = (Qt::SortOrder) settings->value("sortOrder", 0).toInt();
  switch (currentSortColumn) {
    case 0 : toggleSortBy(sortNameAct); break;
    case 1 : toggleSortBy(sortSizeAct); break;
    case 3 : toggleSortBy(sortDateAct); break;
  }
  setSortOrder(currentSortOrder);

  // Load terminal command
  term = settings->value("term").toString();

  // Load information whether tabs can be shown on top
  tabsOnTopAct->setChecked(settings->value("tabsOnTop", 0).toBool());
  tabsOnTop();
}
//---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();

    if(isDaemon)
    {
        this->setVisible(0);
        startDaemon();
        customComplete->setModel(0);
        modelList->refresh();           //clear model, reduce memory
        tabs->setCurrentIndex(0);

        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
        tree->scrollTo(tree->currentIndex());
        customComplete->setModel(modelTree);

        event->ignore();
    }
    else
    {
        modelList->cacheInfo();
        event->accept();
    }
}

//---------------------------------------------------------------------------
void MainWindow::exitAction()
{
    isDaemon = 0;
    close();
}

//---------------------------------------------------------------------------
void MainWindow::treeSelectionChanged(QModelIndex current,QModelIndex previous)
{
    QFileInfo name = modelList->fileInfo(modelTree->mapToSource(current));
    if(!name.exists()) return;

    curIndex = name;
    setWindowTitle(curIndex.fileName() + " - qtFM v5.7");

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
void MainWindow::listDoubleClicked(QModelIndex current)
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if(mods == Qt::ControlModifier || mods == Qt::ShiftModifier) return;

    if(modelList->isDir(modelView->mapToSource(current)))
        tree->setCurrentIndex(modelTree->mapFromSource(modelView->mapToSource(current)));
    else
        executeFile(current,0);
}


//---------------------------------------------------------------------------
void MainWindow::executeFile(QModelIndex index, bool run)
{
    QProcess *myProcess = new QProcess(this);

    if(run)
        myProcess->startDetached(modelList->filePath(modelView->mapToSource(index)));             //is executable?
    else
    {
        QString type = modelList->getMimeType(modelView->mapToSource(index));

        QHashIterator<QString, QAction*> i(*customActManager->getActions());
        while (i.hasNext())
        {
            i.next();
            if(type.contains(i.key()))
                if(i.value()->text() == "Open")
                {
                    i.value()->trigger();
                    return;
                }
        }

        myProcess->start("xdg-open",QStringList() << modelList->filePath(modelView->mapToSource(index)));
        myProcess->waitForFinished(1000);
        myProcess->terminate();
        if(myProcess->exitCode() != 0)
        {
            if(xdgConfig()) executeFile(index,run);
        }
    }
}

//---------------------------------------------------------------------------
void MainWindow::runFile()
{
    executeFile(listSelectionModel->currentIndex(),1);
}

//---------------------------------------------------------------------------
void MainWindow::openFolderAction()
{
    tree->setCurrentIndex(modelTree->mapFromSource(listSelectionModel->currentIndex()));
}

//---------------------------------------------------------------------------
void MainWindow::openFile()
{
    QModelIndexList items;

    if(listSelectionModel->selectedRows(0).count()) items = listSelectionModel->selectedRows(0);
    else items = listSelectionModel->selectedIndexes();

    foreach(QModelIndex index, items)
        executeFile(index,0);
}

//---------------------------------------------------------------------------
void MainWindow::goUpDir()
{
    tree->setCurrentIndex(tree->currentIndex().parent());
}

//---------------------------------------------------------------------------
void MainWindow::goBackDir()
{
    if(pathEdit->count() == 1) return;

    QString current = pathEdit->currentText();

    if(current.contains(pathEdit->itemText(1)))
        backIndex = modelList->index(current);

    do
    {
        pathEdit->removeItem(0);
        if(tabs->count()) tabs->remHistory();
    }
    while(!QFileInfo(pathEdit->itemText(0)).exists() || pathEdit->itemText(0) == current);

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(pathEdit->itemText(0))));
}

//---------------------------------------------------------------------------
void MainWindow::goHomeDir()
{
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(QDir::homePath())));
}

//---------------------------------------------------------------------------
void MainWindow::pathEditChanged(QString path)
{
    QString info = path;

    if(!QFileInfo(path).exists()) return;
    info.replace("~",QDir::homePath());

    QModelIndex temp = modelList->index(info);
    //modelTree->invalidate();

    tree->setCurrentIndex(modelTree->mapFromSource(temp));
}

//---------------------------------------------------------------------------
void MainWindow::terminalRun()
{
    if(term.isEmpty())
    {
        term = QInputDialog::getText(this,tr("Setting"), tr("Default terminal:"), QLineEdit::Normal, "urxvt");
        settings->setValue("term",term);
    }

    QStringList args(term.split(" "));
    QString name = args.at(0);
    args.removeAt(0);
    QProcess::startDetached(name,args,pathEdit->itemText(0));
}

//---------------------------------------------------------------------------
void MainWindow::newDir()
{
    QModelIndex newDir;

    if(!QFileInfo(pathEdit->itemText(0)).isWritable())
    {
        status->showMessage(tr("Read only...cannot create folder"));
        return;
    }

    newDir = modelView->mapFromSource(modelList->insertFolder(modelList->index(pathEdit->itemText(0))));
    listSelectionModel->setCurrentIndex(newDir,QItemSelectionModel::ClearAndSelect);

    if(stackWidget->currentIndex() == 0) list->edit(newDir);
    else detailTree->edit(newDir);
}

//---------------------------------------------------------------------------
void MainWindow::newFile()
{
    QModelIndex fileIndex;

    if(!QFileInfo(pathEdit->itemText(0)).isWritable())
    {
        status->showMessage(tr("Read only...cannot create file"));
        return;
    }

    fileIndex = modelView->mapFromSource(modelList->insertFile(modelList->index(pathEdit->itemText(0))));
    listSelectionModel->setCurrentIndex(fileIndex,QItemSelectionModel::ClearAndSelect);

    if(stackWidget->currentIndex() == 0) list->edit(fileIndex);
    else detailTree->edit(fileIndex);
}

//---------------------------------------------------------------------------
void MainWindow::deleteFile()
{
    QModelIndexList selList;
    bool yesToAll = false;

    if(focusWidget() == tree)
        selList << modelList->index(pathEdit->itemText(0));
    else
    {
        QModelIndexList proxyList;
        if(listSelectionModel->selectedRows(0).count()) proxyList = listSelectionModel->selectedRows(0);
        else proxyList = listSelectionModel->selectedIndexes();

        foreach(QModelIndex proxyItem, proxyList)
            selList.append(modelView->mapToSource(proxyItem));
    }

    bool ok = false;
    bool confirm;

    if(settings->value("confirmDelete").isNull())					//not defined yet
    {
        if(QMessageBox::question(this,tr("Delete confirmation"),tr("Do you want to confirm all delete operations?"),QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) confirm = 1;
        else confirm = 0;
        settings->setValue("confirmDelete",confirm);
    }
    else confirm = settings->value("confirmDelete").toBool();


    for(int i = 0; i < selList.count(); ++i)
    {
        QFileInfo file(modelList->filePath(selList.at(i)));
        if(file.isWritable())
        {
            if(file.isSymLink()) ok = QFile::remove(file.filePath());
            else
            {
                if(yesToAll == false)
                {
                    if(confirm)
                    {
                        int ret = QMessageBox::information(this,tr("Careful"),tr("Are you sure you want to delete <p><b>\"") + file.filePath() + "</b>?",QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll);
                        if(ret == QMessageBox::YesToAll) yesToAll = true;
                        if(ret == QMessageBox::No) return;
                    }
                }
                ok = modelList->remove(selList.at(i));
            }
        }
        else if(file.isSymLink()) ok = QFile::remove(file.filePath());
    }

    if(!ok) QMessageBox::information(this,tr("Failed"),tr("Could not delete some items...do you have the right permissions?"));

    return;
}
//---------------------------------------------------------------------------

void MainWindow::toggleIcons()
{
    if(list->rootIndex() != modelList->index(pathEdit->currentText()))
            list->setRootIndex(modelView->mapFromSource(modelList->index(pathEdit->currentText())));

    if(iconAct->isChecked())
    {
        currentView = 1;
        list->setViewMode(QListView::IconMode);
        list->setGridSize(QSize(zoom+32,zoom+32));
        list->setIconSize(QSize(zoom,zoom));
        list->setFlow(QListView::LeftToRight);

        modelList->setMode(thumbsAct->isChecked());

        stackWidget->setCurrentIndex(0);
        detailAct->setChecked(0);
        detailTree->setMouseTracking(false);

        list->setMouseTracking(true);

        if(tabs->count()) tabs->setType(1);
    }
    else
    {
        currentView = 0;
        list->setViewMode(QListView::ListMode);
        list->setGridSize(QSize());
        list->setIconSize(QSize(zoomList,zoomList));
        list->setFlow(QListView::TopToBottom);

        modelList->setMode(thumbsAct->isChecked());
        list->setMouseTracking(false);

        if(tabs->count()) tabs->setType(0);
    }

    list->setDragDropMode(QAbstractItemView::DragDrop);
    list->setDefaultDropAction(Qt::MoveAction);
}
//---------------------------------------------------------------------------

// Michal Rost: toggle sort by
void MainWindow::toggleSortBy(QAction *action) {

  if (list->rootIndex() != modelList->index(pathEdit->currentText())) {
    list->setRootIndex(modelView->mapFromSource(modelList->index(pathEdit->currentText())));
  }

  action->setChecked(true);

  if (action == sortNameAct) {
    currentSortColumn =  0;
  } else if (action == sortDateAct) {
    currentSortColumn =  3;
  } else if (action == sortSizeAct) {
    currentSortColumn = 1;
  }
  modelView->sort(currentSortColumn, currentSortOrder);
  settings->setValue("sortBy", currentSortColumn);
}
//---------------------------------------------------------------------------

/**
 * @brief Sets sort order
 * @param order
 */
void MainWindow::setSortOrder(Qt::SortOrder order) {

  // Set root index
  if (list->rootIndex() != modelList->index(pathEdit->currentText())) {
    list->setRootIndex(modelView->mapFromSource(modelList->index(pathEdit->currentText())));
  }

  // Change sort order
  currentSortOrder = order;
  sortAscAct->setChecked(!((bool) currentSortOrder));
  settings->setValue("sortOrder", currentSortOrder);

  // Sort
  modelView->sort(currentSortColumn, currentSortOrder);
}
//---------------------------------------------------------------------------

// Michal Rost: toggle sort order
void MainWindow::switchSortOrder() {
  setSortOrder(currentSortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
}
//---------------------------------------------------------------------------

void MainWindow::toggleThumbs()
{
    if(currentView != 2) toggleIcons();
    else toggleDetails();
}

//---------------------------------------------------------------------------
void MainWindow::toggleDetails()
{
    if(detailAct->isChecked() == false)
    {
        toggleIcons();

        stackWidget->setCurrentIndex(0);
        detailTree->setMouseTracking(false);
    }
    else
    {
        currentView = 2;
        if(detailTree->rootIndex() != modelList->index(pathEdit->currentText()))
            detailTree->setRootIndex(modelView->mapFromSource(modelList->index(pathEdit->currentText())));

        detailTree->setMouseTracking(true);

        stackWidget->setCurrentIndex(1);
        modelList->setMode(thumbsAct->isChecked());
        iconAct->setChecked(0);

        if(tabs->count()) tabs->setType(2);
    }
}

//---------------------------------------------------------------------------
void MainWindow::toggleHidden()
{
    if(hiddenAct->isChecked() == false)
    {
        if(curIndex.isHidden())
            listSelectionModel->clear();

        modelView->setFilterRegExp("no");
        modelTree->setFilterRegExp("no");
    }
    else
    {
        modelView->setFilterRegExp("");
        modelTree->setFilterRegExp("");
    }

    modelView->invalidate();
    dirLoaded();
}

//---------------------------------------------------------------------------
void MainWindow::clipboardChanged()
{
    if(QApplication::clipboard()->mimeData()->hasUrls())
        pasteAct->setEnabled(true);
    else
    {
        modelList->clearCutItems();
        pasteAct->setEnabled(false);
    }
}

//---------------------------------------------------------------------------
void MainWindow::cutFile()
{
    QModelIndexList selList;
    QStringList fileList;

    if(focusWidget() == tree) selList << modelView->mapFromSource(modelList->index(pathEdit->itemText(0)));
    else
    if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
    else selList = listSelectionModel->selectedIndexes();


    foreach(QModelIndex item, selList)
        fileList.append(modelList->filePath(modelView->mapToSource(item)));

    clearCutItems();
    modelList->addCutItems(fileList);

    //save a temp file to allow pasting in a different instance
    QFile tempFile(QDir::tempPath() + "/qtfm.temp");
    tempFile.open(QIODevice::WriteOnly);
    QDataStream out(&tempFile);
    out << fileList;
    tempFile.close();

    QApplication::clipboard()->setMimeData(modelView->mimeData(selList));

    modelTree->invalidate();
    listSelectionModel->clear();
}

//---------------------------------------------------------------------------
void MainWindow::copyFile()
{
    QModelIndexList selList;
    if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
    else selList = listSelectionModel->selectedIndexes();

    if(selList.count() == 0)
    if(focusWidget() == tree) selList << modelView->mapFromSource(modelList->index(pathEdit->itemText(0)));
    else return;

    clearCutItems();

    QStringList text;
    foreach(QModelIndex item,selList)
        text.append(modelList->filePath(modelView->mapToSource(item)));

    QApplication::clipboard()->setText(text.join("\n"),QClipboard::Selection);
    QApplication::clipboard()->setMimeData(modelView->mimeData(selList));

    cutAct->setData(0);
}

//---------------------------------------------------------------------------

/**
 * @brief Pastes from clipboard
 */
void MainWindow::pasteClipboard()
{
    QString newPath;
    QStringList cutList;

    if(curIndex.isDir()) newPath = curIndex.filePath();
    else newPath = pathEdit->itemText(0);

    //check list of files that are to be cut
    QFile tempFile(QDir::tempPath() + "/qtfm.temp");
    if(tempFile.exists())
    {
        QModelIndexList list;
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
    QPushButton *move = box.addButton(tr("Move here"), QMessageBox::ActionRole);
    QPushButton *copy = box.addButton(tr("Copy here"), QMessageBox::ActionRole);
    QPushButton *link = box.addButton(tr("Link here"), QMessageBox::ActionRole);
    QPushButton *canc = box.addButton(QMessageBox::Cancel);
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

/**
 * @brief Pastes list of files/dirs into new path
 * @param files list of files
 * @param newPath new (destination) path
 * @param cutList list of files that are going to be removed from source path
 * @return true if operation was successfull
 */
bool MainWindow::pasteFiles(const QList<QUrl> &files, const QString &newPath,
                            const QStringList &cutList) {

  // Temporary variables
  bool ok = true;
  QStringList newFiles;

  // Quit if folder not writable
  if (!QFileInfo(newPath).isWritable()
      || newPath == QDir(files.at(0).toLocalFile()).path()) {
    emit copyProgressFinished(1, newFiles);
    return 0;
  }

  // Get total size in bytes
  qint64 total = FileUtils::totalSize(files);

  // Check available space on destination before we start
  struct statfs info;
  statfs(newPath.toLocal8Bit(), &info);
  if ((qint64) info.f_bavail * info.f_bsize < total) {

    // If it is a cut/move on the same device it doesn't matter
    if (cutList.count()) {
      qint64 driveSize = (qint64) info.f_bavail*info.f_bsize;
      statfs(files.at(0).path().toLocal8Bit(),&info);

      // Same device?
      if ((qint64) info.f_bavail*info.f_bsize != driveSize) {
        emit copyProgressFinished(2, newFiles);
        return 0;
      }
    } else {
      emit copyProgressFinished(2, newFiles);
      return 0;
    }
  }

  // Main loop
  for (int i = 0; i < files.count(); ++i) {

    // Canceled ?
    if (progress->result() == 1) {
      emit copyProgressFinished(0, newFiles);
      return 1;
    }

    // Destination file name and url
    QFileInfo temp(files.at(i).toLocalFile());
    QString destName = temp.fileName();
    QString destUrl = newPath + QDir::separator() + destName;

    // Only do 'Copy(x) of' if same folder
    if (temp.path() == newPath) {
      int num = 1;
      while (QFile(destUrl).exists()) {
        destName = QString("Copy (%1) of %2").arg(num).arg(temp.fileName());
        destUrl = newPath + QDir::separator() + destName;
        num++;
      }
    }

    // If destination file does not exist and is directory
    QFileInfo dName(destUrl);
    if (!dName.exists() || dName.isDir()) {

      // Keep a list of new files so we can select them later
      newFiles.append(destUrl);

      // Cut action
      if (cutList.contains(temp.filePath())) {

        // Files or directories
        if (temp.isFile()) {

          // NOTE: Rename will fail if across different filesystem
          QFSFileEngine file(temp.filePath());
          if (!file.rename(destUrl))	{
            ok = cutCopyFile(temp.filePath(), destUrl, total, true);
          }
        } else {
          ok = QFile(temp.filePath()).rename(destUrl);

          // File exists or move folder between different filesystems, so use
          // copy/remove method
          if (!ok) {
            if (temp.isDir()) {
              ok = true;
              copyFolder(temp.filePath(), destUrl, total, true);
              modelList->clearCutItems();
            }
            // File already exists, don't do anything
          }
        }
      } else {
        if (temp.isDir()) {
          copyFolder(temp.filePath(),destUrl,total,false);
        } else {
          ok = cutCopyFile(temp.filePath(), destUrl, total, false);
        }
      }
    }
  }

  // Finished
  emit copyProgressFinished(0, newFiles);
  return 1;
}
//---------------------------------------------------------------------------

/**
 * @brief Copies source directory to destination directory
 * @param srcFolder location of source directory
 * @param dstFolder location of destination directory
 * @param total total copy size
 * @param cut true/false if source directory is going to be moved/copied
 * @return true if copy was successfull
 */
bool MainWindow::copyFolder(const QString &srcFolder, const QString &dstFolder,
                            qint64 total, bool cut) {

  // Temporary variables
  QDir srcDir(srcFolder);
  QDir dstDir(QFileInfo(dstFolder).path());
  QStringList files;
  bool ok = true;

  // Name of destination directory
  QString folderName = QFileInfo(dstFolder).fileName();

  // Id destination location does not exist, create it
  if (!QFileInfo(dstFolder).exists()) {
    dstDir.mkdir(folderName);
  }
  dstDir = QDir(dstFolder);

  // Get files in source directory
  files = srcDir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

  // Copy each file
  for (int i = 0; i < files.count(); i++) {
    QString srcName = srcDir.path() + QDir::separator() + files[i];
    QString dstName = dstDir.path() + QDir::separator() + files[i];

    // Don't remove source folder if all files not cut
    if (!cutCopyFile(srcName, dstName, total, cut)) ok = false;

    // Cancelled
    if (progress->result() == 1) return 0;
  }

  // Get directories in source directory
  files.clear();
  files = srcDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);

  // Copy each directory
  for (int i = 0; i < files.count(); i++) {
    if (progress->result() == 1) {
      return 0;
    }
    QString srcName = srcDir.path() + QDir::separator() + files[i];
    QString dstName = dstDir.path() + QDir::separator() + files[i];
    copyFolder(srcName, dstName, total, cut);
  }

  // Remove source folder if all files moved ok
  if (cut && ok) {
    srcDir.rmdir(srcFolder);
  }
  return ok;
}
//---------------------------------------------------------------------------

/**
 * @brief Copies or moves file
 * @param src location of source file
 * @param dst location of destination file
 * @param totalSize total copy size
 * @param cut true/false if source file is going to be moved/copied
 * @return true if copy was successfull
 */
bool MainWindow::cutCopyFile(const QString &src, QString dst, qint64 totalSize,
                             bool cut) {

  // Create files with given locations
  QFile srcFile(src);
  QFile dstFile(dst);

  // Destination file already exists, exit
  if (dstFile.exists()) return 1;

  // If destination location is too long make it shorter
  if (dst.length() > 50) dst = "/.../" + dst.split(QDir::separator()).last();

  // Open source and destination files
  srcFile.open(QFile::ReadOnly);
  dstFile.open(QFile::WriteOnly);

  // Determine buffer size, calculate size of file and number of steps
  char block[4096];
  qint64 total = srcFile.size();
  qint64 steps = total >> 7; // shift right 7, same as divide 128, much faster
  qint64 interTotal = 0;

  // Copy blocks
  while (!srcFile.atEnd()) {
    if (progress->result() == 1) break; // cancelled
    qint64 inBytes = srcFile.read(block, sizeof(block));
    dstFile.write(block, inBytes);
    interTotal += inBytes;
    if (interTotal > steps) {
      emit updateCopyProgress(interTotal, totalSize, dst);
      interTotal = 0;
    }
  }

  // Update copy progress
  emit updateCopyProgress(interTotal, totalSize, dst);

  dstFile.close();
  srcFile.close();

  if (dstFile.size() != total) return 0;
  if (cut) srcFile.remove();  // if file is cut remove the source
  return 1;
}
//---------------------------------------------------------------------------

/**
 * @brief Creates symbolic links to files
 * @param files
 * @param newPath
 * @return true if link creation was successfull
 */
bool MainWindow::linkFiles(const QList<QUrl> &files, const QString &newPath) {

  // Quit if folder not writable
  if (!QFileInfo(newPath).isWritable()
      || newPath == QDir(files.at(0).toLocalFile()).path()) {
    return false;
  }

  // TODO: even if symlinks are small we have to make sure that we have space
  // available for links

  // Main loop
  for (int i = 0; i < files.count(); ++i) {

    // Choose destination file name and url
    QFile file(files.at(i).toLocalFile());
    QFileInfo temp(file);
    QString destName = temp.fileName();
    QString destUrl = newPath + QDir::separator() + destName;

    // Only do 'Link(x) of' if same folder
    if (temp.path() == newPath) {
      int num = 1;
      while (QFile(destUrl).exists()) {
        destName = QString("Link (%1) of %2").arg(num).arg(temp.fileName());
        destUrl = newPath + QDir::separator() + destName;
        num++;
      }
    }

    // If file does not exists then create link
    QFileInfo dName(destUrl);
    if (!dName.exists()) {
      file.link(destUrl);
    }
  }
  return true;
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

    properties = new propertiesDialog(paths, modelList);
    connect(properties,SIGNAL(propertiesUpdated()),this,SLOT(clearCutItems()));
}

//---------------------------------------------------------------------------
void MainWindow::renameFile()
{
    if(focusWidget() == tree)
        tree->edit(treeSelectionModel->currentIndex());
    else if(focusWidget() == bookmarksList)
            bookmarksList->edit(bookmarksList->currentIndex());
    else if(focusWidget() == list)
            list->edit(listSelectionModel->currentIndex());
    else if(focusWidget() == detailTree)
            detailTree->edit(listSelectionModel->currentIndex());
}

//---------------------------------------------------------------------------
void MainWindow::toggleLockLayout()
{
    if(lockLayoutAct->isChecked())
    {
        QFrame *newTitle = new QFrame();
        newTitle->setFrameShape(QFrame::StyledPanel);
        newTitle->setMinimumSize(0,1);
        dockTree->setTitleBarWidget(newTitle);

        newTitle = new QFrame();
        newTitle->setFrameShape(QFrame::StyledPanel);
        newTitle->setMinimumSize(0,1);
        dockBookmarks->setTitleBarWidget(newTitle);

        menuToolBar->setMovable(0);
        editToolBar->setMovable(0);
        viewToolBar->setMovable(0);
        navToolBar->setMovable(0);
        addressToolBar->setMovable(0);

            lockLayoutAct->setText(tr("Unlock layout"));
    }
    else
    {
        dockTree->setTitleBarWidget(0);
        dockBookmarks->setTitleBarWidget(0);

        menuToolBar->setMovable(1);
        editToolBar->setMovable(1);
        viewToolBar->setMovable(1);
        navToolBar->setMovable(1);
        addressToolBar->setMovable(1);

        lockLayoutAct->setText(tr("Lock layout"));
    }
}

//---------------------------------------------------------------------------
bool MainWindow::xdgConfig()
{
    if(!listSelectionModel->currentIndex().isValid()) return 0;

    QDialog *xdgConfig = new QDialog(this);
    xdgConfig->setWindowTitle(tr("Configure filetype"));

    QString mimeType = FileUtils::getMimeType(curIndex.filePath());

    QLabel *label = new QLabel(tr("Filetype:") + "<b>" + mimeType + "</b><p>" + tr("Open with:"));
    QComboBox * appList = new QComboBox;

    QDialogButtonBox *buttons = new QDialogButtonBox;
    buttons->setStandardButtons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), xdgConfig, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), xdgConfig, SLOT(reject()));

    appList->setEditable(true);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(appList);
    layout->addWidget(buttons);
    xdgConfig->setLayout(layout);

    QStringList apps;
    QDirIterator it("/usr/share/applications",QStringList("*.desktop"),QDir::Files | QDir::NoDotAndDotDot , QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        apps.append(it.fileName());
    }

    apps.replaceInStrings(".desktop","");
    apps.sort();
    appList->addItems(apps);

    //now get the icons
    QDir appIcons("/usr/share/pixmaps","",0,QDir::Files | QDir::NoDotAndDotDot);
    apps = appIcons.entryList();
    QIcon defaultIcon = QIcon::fromTheme("application-x-executable");

    for(int i = 0; i < appList->count(); ++i)
    {
        QString baseName = appList->itemText(i);
        QPixmap temp = QIcon::fromTheme(baseName).pixmap(16,16);

        if(!temp.isNull()) appList->setItemIcon(i,temp);
        else
        {
            QStringList searchIcons = apps.filter(baseName);
            if(searchIcons.count() > 0) appList->setItemIcon(i,QIcon("/usr/share/pixmaps/" + searchIcons.at(0)));
            else appList->setItemIcon(i,defaultIcon);
        }
    }

    QString xdgDefaults;

    //xdg changes -> now uses mimeapps.list instead of defaults.list
    if(QFileInfo(QDir::homePath() + "/.local/share/applications/mimeapps.list").exists())
        xdgDefaults = QDir::homePath() + "/.local/share/applications/mimeapps.list";
    else
        xdgDefaults = QDir::homePath() + "/.local/share/applications/defaults.list";

    QSettings defaults(xdgDefaults,QSettings::IniFormat,this);

    QString temp = defaults.value("Default Applications/" + mimeType).toString();
    appList->setCurrentIndex(appList->findText(temp.remove(".desktop"),Qt::MatchFixedString));

    bool ok = xdgConfig->exec();
    if(ok)
    {
        QProcess *myProcess = new QProcess(this);
        myProcess->start("xdg-mime",QStringList() << "default" << appList->currentText() + ".desktop" << mimeType);
    }

    delete xdgConfig;
    return ok;
}

//---------------------------------------------------------------------------

/**
 * @brief Displays settings dialog
 */
void MainWindow::showEditDialog() {

  // Deletes current list of custom actions
  customActManager->freeActions();

  // Creates settings dialog
  SettingsDialog *dlg = new SettingsDialog(actionList, settings, this);
  dlg->exec();

  // Reload settings
  loadSettings();

  // Reads custom actions
  customActManager->readActions();
  delete dlg;
}
//---------------------------------------------------------------------------

void MainWindow::writeSettings()
{
    settings->setValue("size", size());
    settings->setValue("viewMode", stackWidget->currentIndex());
    settings->setValue("iconMode", iconAct->isChecked());
    settings->setValue("zoom", zoom);
    settings->setValue("zoomTree", zoomTree);
    settings->setValue("zoomList", zoomList);
    settings->setValue("zoomDetail", zoomDetail);

    // Michal Rost
    //-----------------------------------------------------------------------
    settings->setValue("sortBy", currentSortColumn);
    settings->setValue("sortOrder", currentSortOrder);
    //-----------------------------------------------------------------------

    settings->setValue("showThumbs", thumbsAct->isChecked());
    settings->setValue("hiddenMode", hiddenAct->isChecked());
    settings->setValue("lockLayout", lockLayoutAct->isChecked());
    settings->setValue("tabsOnTop", tabsOnTopAct->isChecked());
    settings->setValue("windowState", saveState(1));
    settings->setValue("header", detailTree->header()->saveState());

    settings->remove("bookmarks");
    settings->beginGroup("bookmarks");

    for (int i = 0; i < modelBookmarks->rowCount(); i++) {
        QStringList temp;
        temp << modelBookmarks->item(i)->text() << modelBookmarks->item(i)->data(32).toString() << modelBookmarks->item(i)->data(34).toString() << modelBookmarks->item(i)->data(33).toString();
        settings->setValue(QString(i),temp);
    }
    settings->endGroup();
}

//---------------------------------------------------------------------------------
void MainWindow::contextMenuEvent(QContextMenuEvent * event)
{
    QMenu *popup;
    QWidget *widget = childAt(event->pos());

    if(widget == tabs)
    {
        popup = new QMenu(this);
        popup->addAction(closeTabAct);
        popup->exec(event->globalPos());
        return;
    }
    else
    if(widget == status)
    {
        popup = createPopupMenu();
        popup->addSeparator();
        popup->addAction(lockLayoutAct);
        popup->exec(event->globalPos());
        return;
    }

    QList<QAction*> actions;
    popup = new QMenu(this);

    if(focusWidget() == list || focusWidget() == detailTree)
    {
        bookmarksList->clearSelection();

        if(listSelectionModel->hasSelection())	    //could be file or folder
        {
            curIndex = modelList->filePath(modelView->mapToSource(listSelectionModel->currentIndex()));

            if(!curIndex.isDir())		    //file
            {
                QString type = modelList->getMimeType(modelList->index(curIndex.filePath()));

                QHashIterator<QString, QAction*> i(*customActManager->getActions());
                while (i.hasNext())
                {
                    i.next();
                    if(type.contains(i.key())) actions.append(i.value());
                }

                foreach(QAction*action, actions)
                    if(action->text() == "Open")
                    {
                        popup->addAction(action);
                        break;
                    }

                if(popup->actions().count() == 0) popup->addAction(openAct);

                if(curIndex.isExecutable()) popup->addAction(runAct);

                popup->addActions(actions);

                QHashIterator<QString, QMenu*> m(*customActManager->getMenus());
                while (m.hasNext())
                {
                    m.next();
                    if(type.contains(m.key())) popup->addMenu(m.value());
                }

                popup->addSeparator();
                popup->addAction(cutAct);
                popup->addAction(copyAct);
                popup->addAction(pasteAct);
                popup->addSeparator();
                popup->addAction(renameAct);
                popup->addSeparator();

                foreach(QMenu* parent, customActManager->getMenus()->values("*"))
                    popup->addMenu(parent);

                actions = (customActManager->getActions()->values("*"));
                popup->addActions(actions);
                popup->addAction(deleteAct);
                popup->addSeparator();
                actions = customActManager->getActions()->values(curIndex.path());    //children of $parent
                if(actions.count())
                {
                    popup->addActions(actions);
                    popup->addSeparator();
                }
            }
            else
            {	//folder
                popup->addAction(openAct);
                popup->addSeparator();
                popup->addAction(cutAct);
                popup->addAction(copyAct);
                popup->addAction(pasteAct);
                popup->addSeparator();
                popup->addAction(renameAct);
                popup->addSeparator();

                foreach(QMenu* parent, customActManager->getMenus()->values("*"))
                    popup->addMenu(parent);

                actions = customActManager->getActions()->values("*");
                popup->addActions(actions);
                popup->addAction(deleteAct);
                popup->addSeparator();

                foreach(QMenu* parent, customActManager->getMenus()->values("folder"))
                    popup->addMenu(parent);

                actions = customActManager->getActions()->values(curIndex.fileName());               //specific folder
                actions.append(customActManager->getActions()->values(curIndex.path()));    //children of $parent
                actions.append(customActManager->getActions()->values("folder"));                    //all folders
                if(actions.count())
                {
                    popup->addActions(actions);
                    popup->addSeparator();
                }
            }

            popup->addAction(folderPropertiesAct);

        }
        else
        {   //whitespace
            popup->addAction(newDirAct);
            popup->addAction(newFileAct);
            popup->addSeparator();
            if(pasteAct->isEnabled())
            {
                popup->addAction(pasteAct);
                popup->addSeparator();
            }
            popup->addAction(addBookmarkAct);
            popup->addSeparator();

            foreach(QMenu* parent, customActManager->getMenus()->values("folder"))
                popup->addMenu(parent);
            actions = customActManager->getActions()->values(curIndex.fileName());
            actions.append(customActManager->getActions()->values("folder"));
            if(actions.count())
            {
                foreach(QAction*action, actions)
                    popup->addAction(action);
                popup->addSeparator();
            }

            popup->addAction(folderPropertiesAct);
        }
    }
    else
    {	//tree or bookmarks
        if(focusWidget() == bookmarksList)
        {
            listSelectionModel->clearSelection();
            if(bookmarksList->indexAt(bookmarksList->mapFromGlobal(event->globalPos())).isValid())
            {
                curIndex = bookmarksList->currentIndex().data(32).toString();
                popup->addAction(delBookmarkAct);
                popup->addAction(editBookmarkAct);	//icon
            }
            else
            {
                bookmarksList->clearSelection();
                popup->addAction(addSeparatorAct);	//seperator
                popup->addAction(wrapBookmarksAct);
            }
            popup->addSeparator();
        }
        else
        {
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

        foreach(QMenu* parent, customActManager->getMenus()->values("folder"))
            popup->addMenu(parent);
        actions = customActManager->getActions()->values(curIndex.fileName());
        actions.append(customActManager->getActions()->values(curIndex.path()));
        actions.append(customActManager->getActions()->values("folder"));
        if(actions.count())
        {
            foreach(QAction*action, actions)
                popup->addAction(action);
            popup->addSeparator();
        }
        popup->addAction(folderPropertiesAct);
    }

    popup->exec(event->globalPos());
    delete popup;
}

//---------------------------------------------------------------------------------
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
