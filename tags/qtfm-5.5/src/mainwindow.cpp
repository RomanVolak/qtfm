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
#include "customactions.h"
#include "actions.cpp"
#include "bookmarks.cpp"
#include "progressdlg.h"

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

    restoreState(settings->value("windowState").toByteArray(),1);
    resize(settings->value("size", QSize(600, 400)).toSize());

    setWindowIcon(QIcon(":/images/qtfm.png"));


    modelBookmarks = new bookmarkmodel(modelList->folderIcons);

    settings->beginGroup("bookmarks");
    foreach(QString key,settings->childKeys())
    {
        QStringList temp(settings->value(key).toStringList());
        modelBookmarks->addBookmark(temp.at(0),temp.at(1),temp.at(2),temp.last());
    }
    settings->endGroup();

    autoBookmarkMounts();
    bookmarksList->setModel(modelBookmarks);
    bookmarksList->setResizeMode(QListView::Adjust);
    bookmarksList->setFlow(QListView::TopToBottom);
    bookmarksList->setIconSize(QSize(24,24));

    wrapBookmarksAct->setChecked(settings->value("wrapBookmarks",0).toBool());
    bookmarksList->setWrapping(wrapBookmarksAct->isChecked());

    lockLayoutAct->setChecked(settings->value("lockLayout",0).toBool());
    toggleLockLayout();

    zoom = settings->value("zoom",48).toInt();
    zoomTree = settings->value("zoomTree",16).toInt();
    zoomList = settings->value("zoomList",24).toInt();
    zoomDetail = settings->value("zoomDetail",16).toInt();

    detailTree->setIconSize(QSize(zoomDetail,zoomDetail));
    tree->setIconSize(QSize(zoomTree,zoomTree));

    thumbsAct->setChecked(settings->value("showThumbs",1).toBool());

    detailAct->setChecked(settings->value("viewMode",0).toBool());
    iconAct->setChecked(settings->value("iconMode",0).toBool());
    toggleDetails();

    hiddenAct->setChecked(settings->value("hiddenMode",0).toBool());
    toggleHidden();

    detailTree->header()->restoreState(settings->value("header").toByteArray());
    detailTree->setSortingEnabled(1);

    if(isDaemon) startDaemon();
    else show();

    QTimer::singleShot(0,this,SLOT(lateStart()));
}

//---------------------------------------------------------------------------
void MainWindow::lateStart()
{
    status->showMessage(getDriveInfo(curIndex.filePath()));

    bookmarksList->setDragDropMode(QAbstractItemView::DragDrop);
    bookmarksList->setDropIndicatorShown(true);
    bookmarksList->setDefaultDropAction(Qt::MoveAction);
    bookmarksList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    tree->setDragDropMode(QAbstractItemView::DragDrop);
    tree->setDefaultDropAction(Qt::MoveAction);
    tree->setDropIndicatorShown(true);
    tree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    detailTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    detailTree->setDragDropMode(QAbstractItemView::DragDrop);
    detailTree->setDefaultDropAction(Qt::MoveAction);
    detailTree->setDropIndicatorShown(true);
    detailTree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    list->setResizeMode(QListView::Adjust);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setSelectionRectVisible(true);
    list->setFocus();

    list->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    connect(list,SIGNAL(activated(QModelIndex)),this,SLOT(listDoubleClicked(QModelIndex)));

    if(settings->value("singleClick").toInt() == 1)
    {
        connect(list,SIGNAL(clicked(QModelIndex)),this,SLOT(listItemClicked(QModelIndex)));
        connect(detailTree,SIGNAL(clicked(QModelIndex)),this,SLOT(listItemClicked(QModelIndex)));
    }
    if(settings->value("singleClick").toInt() == 2)
    {
        connect(list,SIGNAL(clicked(QModelIndex)),this,SLOT(listDoubleClicked(QModelIndex)));
        connect(detailTree,SIGNAL(clicked(QModelIndex)),this,SLOT(listDoubleClicked(QModelIndex)));
    }

    customActions = new QMultiHash<QString,QAction*>;
    customMapper = new QSignalMapper();
    connect(customMapper, SIGNAL(mapped(QString)),this, SLOT(actionMapper(QString)));

    int fd = open("/proc/self/mounts",O_RDONLY,0);
    notify = new QSocketNotifier(fd,QSocketNotifier::Write);
    connect(notify, SIGNAL(activated(int)), this, SLOT(mountWatcherTriggered()),Qt::QueuedConnection);


    term = settings->value("term").toString();
    progress = 0;
    clipboardChanged();

    customComplete = new myCompleter;
    customComplete->setModel(modelTree);
    customComplete->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    customComplete->setMaxVisibleItems(10);
    pathEdit->setCompleter(customComplete);

    tabs->setDrawBase(0);
    tabs->setExpanding(0);
    tabsOnTopAct->setChecked(settings->value("tabsOnTop",0).toBool());
    tabsOnTop();

    connect(pathEdit,SIGNAL(activated(QString)),this,SLOT(pathEditChanged(QString)));
    connect(customComplete,SIGNAL(activated(QString)), this,SLOT(pathEditChanged(QString)));
    connect(pathEdit->lineEdit(),SIGNAL(cursorPositionChanged(int,int)),this,SLOT(addressChanged(int,int)));

    connect(bookmarksList,SIGNAL(activated(QModelIndex)),this,SLOT(bookmarkClicked(QModelIndex)));
    connect(bookmarksList,SIGNAL(clicked(QModelIndex)),this,SLOT(bookmarkClicked(QModelIndex)));
    connect(bookmarksList,SIGNAL(pressed(QModelIndex)),this,SLOT(bookmarkPressed(QModelIndex)));

    connect(QApplication::clipboard(),SIGNAL(changed(QClipboard::Mode)),this,SLOT(clipboardChanged()));
    connect(detailTree,SIGNAL(activated(QModelIndex)),this,SLOT(listDoubleClicked(QModelIndex)));
    connect(listSelectionModel,SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),this,SLOT(listSelectionChanged(const QItemSelection, const QItemSelection)));

    connect(this,SIGNAL(copyProgressFinished(int,QStringList)),this,SLOT(progressFinished(int,QStringList)));

    connect(modelBookmarks,SIGNAL(bookmarkPaste(const QMimeData *, QString, QStringList)),this,SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));
    connect(modelList,SIGNAL(dragDropPaste(const QMimeData *, QString, QStringList)),this,SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));

    connect(tabs,SIGNAL(currentChanged(int)),this,SLOT(tabChanged(int)));
    connect(tabs,SIGNAL(dragDropTab(const QMimeData *, QString, QStringList)),this,SLOT(pasteLauncher(const QMimeData *, QString, QStringList)));
    connect(list,SIGNAL(pressed(QModelIndex)),this,SLOT(listItemPressed(QModelIndex)));
    connect(detailTree,SIGNAL(pressed(QModelIndex)),this,SLOT(listItemPressed(QModelIndex)));

    connect(modelList,SIGNAL(thumbUpdate(QModelIndex)),this,SLOT(thumbUpdate(QModelIndex)));

    qApp->setKeyboardInputInterval(1000);

    connect(&daemon,SIGNAL(newConnection()),this,SLOT(newConnection()));

    QTimer::singleShot(100,this,SLOT(readCustomActions()));
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
    setWindowTitle(curIndex.fileName() + " - qtFM v5.5");

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

        QHashIterator<QString, QAction*> i(*customActions);
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
void MainWindow::pasteLauncher(const QMimeData * data, QString newPath, QStringList cutList)
{
    QList<QUrl> files = data->urls();

    if(!QFile(files.at(0).path()).exists())
    {
        QMessageBox::information(this,tr("No paste for you!"),tr("File no longer exists!"));
        pasteAct->setEnabled(false);
        return;
    }

    int replace = 0;
    QStringList completeList;
    QString baseName = QFileInfo(files.at(0).toLocalFile()).path();

    if(newPath != baseName)			    //only if not in same directory, otherwise we will do 'Copy(x) of'
    {
    	foreach(QUrl file, files)
        {
            QFileInfo temp(file.toLocalFile());

            if(temp.isDir() && QFileInfo(newPath + "/" + temp.fileName()).exists())     //merge or replace?
            {
                QMessageBox message(QMessageBox::Question,tr("Existing folder"),QString("<b>%1</b><p>Already exists!<p>What do you want to do?")
                    .arg(newPath + "/" + temp.fileName()),QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

                message.button(QMessageBox::Yes)->setText(tr("Merge"));
                message.button(QMessageBox::No)->setText(tr("Replace"));

                int merge = message.exec();
                if(merge == QMessageBox::Cancel) return;
                if(merge == QMessageBox::Yes) recurseFolder(temp.filePath(),temp.fileName(),&completeList);
                else
                {
                    //physically remove files from disk
                    QStringList children;
                    QDirIterator it(newPath + "/" + temp.fileName(),QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
                    while (it.hasNext())
                        children.prepend(it.next());
                    children.append(newPath + "/" + temp.fileName());
                    foreach(QString child,children)
                        QFile(child).remove();
                }
            }
            else completeList.append(temp.fileName());
        }

        foreach(QString file, completeList)
        {
            QFileInfo temp(newPath + "/" + file);
            if(temp.exists())
            {
                QFileInfo orig(baseName + "/" + file);
                if(replace != QMessageBox::YesToAll && replace != QMessageBox::NoToAll)
                        replace = QMessageBox::question(0,tr("Replace"),QString("Do you want to replace:<p><b>%1</p><p>Modified: %2<br>Size: %3 bytes</p><p>with:<p><b>%4</p><p>Modified: %5<br>Size: %6 bytes</p>")
                                .arg(temp.filePath()).arg(temp.lastModified().toString()).arg(temp.size())
                                .arg(orig.filePath()).arg(orig.lastModified().toString()).arg(orig.size()),QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel);

                if(replace == QMessageBox::Cancel) return;
                if(replace == QMessageBox::Yes || replace == QMessageBox::YesToAll)
                    QFile(temp.filePath()).remove();
            }

        }
    }

    QString title;
    if(cutList.count() == 0) title = tr("Copying...");
    else title = tr("Moving...");

    progress = new myProgressDialog(title);
    connect(this,SIGNAL(updateCopyProgress(qint64, qint64, QString)),progress,SLOT(update(qint64, qint64, QString)));

    listSelectionModel->clear();
    QtConcurrent::run(this,&MainWindow::pasteFile, files, newPath, cutList);
}

//---------------------------------------------------------------------------
void MainWindow::recurseFolder(QString path, QString parent, QStringList *list)
{
    QDir dir(path);
    QStringList files = dir.entryList(QDir::AllEntries | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    for(int i = 0; i < files.count(); i++)
    {
        if(QFileInfo(files.at(i)).isDir()) recurseFolder(files.at(i),parent + "/" + files.at(i),list);
        else list->append(parent + "/" + files.at(i));
    }
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
bool MainWindow::pasteFile(QList<QUrl> files,QString newPath, QStringList cutList)
{
    bool ok = true;
    QStringList newFiles;

    if(!QFileInfo(newPath).isWritable() || newPath == QDir(files.at(0).toLocalFile()).path())        //quit if folder not writable
    {
        emit copyProgressFinished(1,newFiles);
        return 0;
    }

    //get total size in bytes
    qint64 total = 1;
    foreach(QUrl url, files)
    {
        QFileInfo file = url.path();
        if(file.isFile()) total += file.size();
        else
        {
            QDirIterator it(url.path(),QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                it.next();
            total += it.fileInfo().size();
            }
        }
    }

    //check available space on destination before we start
    struct statfs info;
    statfs(newPath.toLocal8Bit(), &info);

    if((qint64) info.f_bavail*info.f_bsize < total)
    {
        //if it is a cut/move on the same device it doesn't matter
        if(cutList.count())
        {
            qint64 driveSize = (qint64) info.f_bavail*info.f_bsize;
            statfs(files.at(0).path().toLocal8Bit(),&info);
            if((qint64) info.f_bavail*info.f_bsize != driveSize)        //same device
            {
                emit copyProgressFinished(2,newFiles);
                return 0;
            }
        }
        else
        {
            emit copyProgressFinished(2,newFiles);
            return 0;
        }
    }


    //main loop
    for(int i = 0; i < files.count(); ++i)
    {
        if(progress->result() == 1)			//cancelled
        {
            emit copyProgressFinished(0,newFiles);
            return 1;
        }

        QFileInfo temp(files.at(i).toLocalFile());
        QString destName = temp.fileName();

        if(temp.path() == newPath)			// only do 'Copy(x) of' if same folder
        {
            int num = 1;
            while(QFile(newPath + "/" + destName).exists())
            {
                destName = QString("Copy (%1) of %2").arg(num).arg(temp.fileName());
                num++;
            }
        }

        destName = newPath + "/" + destName;
        QFileInfo dName(destName);

        if(!dName.exists() || dName.isDir())
        {
            newFiles.append(destName);				    //keep a list of new files so we can select them later

            if(cutList.contains(temp.filePath()))			    //cut action
            {
                if (temp.isFile()) //files
                {
                    QFSFileEngine file(temp.filePath());
                    if(!file.rename(destName))			    //rename will fail if across different filesystem, so use copy/remove method
                        ok = cutCopyFile(temp.filePath(), destName, total, true);
                }
                else
                {
                    ok = QFile(temp.filePath()).rename(destName);
                    if(!ok)	//file exists or move folder between different filesystems, so use copy/remove method
                    {
                        if(temp.isDir())
                        {
                            ok = true;
                            copyFolder(temp.filePath(), destName, total, true);
                            modelList->clearCutItems();
                        }
                        //file already exists, don't do anything
                    }
                }
            }
            else
            {
                if(temp.isDir())
                    copyFolder(temp.filePath(),destName,total,false);
                else
                    ok = cutCopyFile(temp.filePath(), destName, total, false);
            }
        }
    }

    emit copyProgressFinished(0,newFiles);
    return 1;
}

//---------------------------------------------------------------------------
bool MainWindow::cutCopyFile(QString source, QString dest, qint64 totalSize, bool cut)
{
    QFile in(source);
    QFile out(dest);

    if(out.exists()) return 1;  //file exists, don't do anything

    if (dest.length() > 50) dest = "/.../" + dest.split("/").last();

    in.open(QFile::ReadOnly);
    out.open(QFile::WriteOnly);

    char block[4096];
    qint64 total = in.size();
    qint64 steps = total >> 7;        //shift right 7, same as divide 128, much faster
    qint64 interTotal = 0;

    while(!in.atEnd())
    {
        if(progress->result() == 1) break;                 //cancelled

        qint64 inBytes = in.read(block, sizeof(block));
        out.write(block, inBytes);
        interTotal += inBytes;

        if(interTotal > steps)
        {
            emit updateCopyProgress(interTotal,totalSize,dest);
            interTotal = 0;
        }
    }

    emit updateCopyProgress(interTotal,totalSize,dest);

    out.close();
    in.close();

    if(out.size() != total) return 0;
    if(cut) in.remove();  //if file is cut remove the source
    return 1;
}

//---------------------------------------------------------------------------
bool MainWindow::copyFolder(QString sourceFolder, QString destFolder, qint64 total, bool cut)
{
    QDir sourceDir(sourceFolder);
    QDir destDir(QFileInfo(destFolder).path());
    QString folderName = QFileInfo(destFolder).fileName();

    bool ok = true;

    if(!QFileInfo(destFolder).exists()) destDir.mkdir(folderName);
    destDir = QDir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    for(int i = 0; i < files.count(); i++)
    {
        QString srcName = sourceDir.path() + "/" + files[i];
        QString destName = destDir.path() + "/" + files[i];
        if(!cutCopyFile(srcName, destName, total, cut)) ok = false;     //don't remove source folder if all files not cut

        if(progress->result() == 1) return 0;                           //cancelled
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);

    for(int i = 0; i < files.count(); i++)
    {
        if(progress->result() == 1) return 0;			    //cancelled

        QString srcName = sourceDir.path() + "/" + files[i];
        QString destName = destDir.path() + "/" + files[i];
        copyFolder(srcName, destName, total, cut);
    }

    //remove source folder if all files moved ok
    if(cut && ok) sourceDir.rmdir(sourceFolder);
    return ok;
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

    QString mimeType = gGetMimeType(curIndex.filePath());

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
void MainWindow::readCustomActions()
{
    customMenus = new QMultiHash<QString,QMenu*>;

    settings->beginGroup("customActions");
    QStringList keys = settings->childKeys();

    for(int i = 0; i < keys.count(); ++i)
    {
        keys.insert(i,keys.takeLast());  //reverse order

        QStringList temp(settings->value(keys.at(i)).toStringList());

        // temp.at(0) - FileType
        // temp.at(1) - Text
        // temp.at(2) - Icon
        // temp.at(3) - Command

        QAction *tempAction = new QAction(QIcon::fromTheme(temp.at(2)),temp.at(1),this);
        customMapper->setMapping(tempAction,temp.at(3));
        connect(tempAction, SIGNAL(triggered()), customMapper, SLOT(map()));

        actionList->append(tempAction);

        QStringList types = temp.at(0).split(",");

        foreach(QString type,types)
        {
            QStringList children(temp.at(1).split(" / "));
            if(children.count() > 1)
            {
                QMenu * parent = 0;
                tempAction->setText(children.at(1));

                foreach(QMenu *subMenu,customMenus->values(type))
                    if(subMenu->title() == children.at(0)) parent = subMenu;

                if(parent == 0)
                {
                    parent = new QMenu(children.at(0));
                    customMenus->insert(type,parent);
                }

                parent->addAction(tempAction);
                customActions->insert("null",tempAction);
            }
            else
                customActions->insert(type,tempAction);
        }
    }
    settings->endGroup();

    readShortcuts();
}

//---------------------------------------------------------------------------
void MainWindow::editCustomActions()
{
    //remove all custom actions from list because we will add them all again below.
    foreach(QAction *action, *actionList)
        if(customActions->values().contains(action))
        {
            actionList->removeOne(action);
            delete action;
        }

    QList<QMenu*> temp = customMenus->values();

    foreach(QMenu* menu, temp)
        delete menu;

    customActions->clear();
    customMenus->clear();

    customActionsDialog *dlg = new customActionsDialog(this);
    dlg->exec();
    readCustomActions();
    delete dlg;
}

//---------------------------------------------------------------------------
void MainWindow::writeSettings()
{
    settings->setValue("size", size());
    settings->setValue("viewMode",stackWidget->currentIndex());
    settings->setValue("iconMode",iconAct->isChecked());
    settings->setValue("zoom",zoom);
    settings->setValue("zoomTree",zoomTree);
    settings->setValue("zoomList",zoomList);
    settings->setValue("zoomDetail",zoomDetail);

    settings->setValue("showThumbs",thumbsAct->isChecked());
    settings->setValue("hiddenMode",hiddenAct->isChecked());
    settings->setValue("lockLayout",lockLayoutAct->isChecked());
    settings->setValue("tabsOnTop",tabsOnTopAct->isChecked());
    settings->setValue("windowState", saveState(1));
    settings->setValue("header",detailTree->header()->saveState());

    settings->remove("bookmarks");
    settings->beginGroup("bookmarks");

    for(int i = 0; i < modelBookmarks->rowCount(); i++)
    {
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

                QHashIterator<QString, QAction*> i(*customActions);
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

                QHashIterator<QString, QMenu*> m(*customMenus);
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

                foreach(QMenu* parent, customMenus->values("*"))
                    popup->addMenu(parent);

                actions = (customActions->values("*"));
                popup->addActions(actions);
                popup->addAction(deleteAct);
                popup->addSeparator();
                actions = customActions->values(curIndex.path());    //children of $parent
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

                foreach(QMenu* parent, customMenus->values("*"))
                    popup->addMenu(parent);

                actions = customActions->values("*");
                popup->addActions(actions);
                popup->addAction(deleteAct);
                popup->addSeparator();

                foreach(QMenu* parent, customMenus->values("folder"))
                    popup->addMenu(parent);

                actions = customActions->values(curIndex.fileName());               //specific folder
                actions.append(customActions->values(curIndex.path()));    //children of $parent
                actions.append(customActions->values("folder"));                    //all folders
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

            foreach(QMenu* parent, customMenus->values("folder"))
                popup->addMenu(parent);
            actions = customActions->values(curIndex.fileName());
            actions.append(customActions->values("folder"));
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

        foreach(QMenu* parent, customMenus->values("folder"))
            popup->addMenu(parent);
        actions = customActions->values(curIndex.fileName());
        actions.append(customActions->values(curIndex.path()));
        actions.append(customActions->values("folder"));
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

    temp = cmd.split(" ");

    QString exec = temp.at(0);
    temp.removeAt(0);

    temp.replaceInStrings("\\","\ ");

    QProcess *customProcess = new QProcess();
    customProcess->setWorkingDirectory(pathEdit->itemText(0));

    connect(customProcess,SIGNAL(error(QProcess::ProcessError)),this,SLOT(customActionError(QProcess::ProcessError)));
    connect(customProcess,SIGNAL(finished(int)),this,SLOT(customActionFinished(int)));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if(exec.at(0) == '|')
    {
        exec.remove(0,1);
        env.insert("QTFM", "1");
        customProcess->setProcessEnvironment(env);
    }

    customProcess->start(exec,temp);
}

//---------------------------------------------------------------------------------
void MainWindow::customActionError(QProcess::ProcessError error)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    QMessageBox::warning(this,"Error",process->errorString());
    customActionFinished(0);
}

//---------------------------------------------------------------------------------
void MainWindow::customActionFinished(int ret)
{
    QProcess* process = qobject_cast<QProcess*>(sender());

    if(process->processEnvironment().contains("QTFM"))
    {
        QString output = process->readAllStandardError();
        if(!output.isEmpty()) QMessageBox::warning(this,tr("Error - Custom action"),output);

        output = process->readAllStandardOutput();
        if(!output.isEmpty()) QMessageBox::information(this,tr("Output - Custom action"),output);
    }

    QTimer::singleShot(100,this,SLOT(clearCutItems()));                //updates file sizes
    process->deleteLater();
}

//---------------------------------------------------------------------------------
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
