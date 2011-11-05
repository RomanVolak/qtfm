/****************************************************************************
* This file is part of qtFM, a simple, fast file manager.
* Copyright (C) 2010,2011 Wittfella
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
        else startPath = QUrl(args.at(1)).toLocalFile();
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
                else if(QFile::exists("/usr/share/icons/Tango")) temp = "Tango";
                else temp = "hicolor";

                settings->setValue("forceTheme",temp);
            }
        }
    }

    QIcon::setThemeName(temp);

    modelList = new myModel();
    modelList->setReadOnly(false);
    modelList->setResolveSymlinks(false);

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
    QVBoxLayout *mainLayout = new QVBoxLayout(main);
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

    mainLayout->addWidget(stackWidget);

    tabs = new tabBar(modelList->folderIcons);
    mainLayout->addWidget(tabs);

    tabs->setDrawBase(0);
    tabs->setExpanding(0);
    tabs->setShape(QTabBar::RoundedSouth);

    setCentralWidget(main);

    modelTree = new FileFilterProxyModel();
    modelTree->setSourceModel(modelList);
    modelTree->setSortCaseSensitivity(Qt::CaseInsensitive);

    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setModel(modelTree);
    tree->hideColumn(1);
    tree->hideColumn(2);
    tree->hideColumn(3);
    tree->hideColumn(4);
    modelTree->sort(0);

    customComplete = new myCompleter;
    customComplete->setModel(modelTree);
    customComplete->setCompletionMode(QCompleter::InlineCompletion);
    //customComplete->setCompletionMode(QCompleter::UnfilteredPopupCompletion);

    list->setWrapping(true);
    list->setModel(modelList);
    listSelectionModel = list->selectionModel();

    detailTree->setRootIsDecorated(false);
    detailTree->setItemsExpandable(false);
    detailTree->setUniformRowHeights(true);
    detailTree->setModel(modelList);
    detailTree->setSelectionModel(listSelectionModel);

    pathEdit = new QComboBox();
    pathEdit->setEditable(true);
    pathEdit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pathEdit->setCompleter(customComplete);

    status = statusBar();
    status->setSizeGripEnabled(true);
    statusName = new QLabel();
    statusSize = new QLabel();
    statusDate = new QLabel();

    tree->setRootIndex(modelTree->mapFromSource(modelList->index("/")));
    treeSelectionModel = tree->selectionModel();
    connect(treeSelectionModel,SIGNAL(currentChanged(QModelIndex,QModelIndex)),this,SLOT(treeSelectionChanged(QModelIndex,QModelIndex)));

    connect(&timer,SIGNAL(timeout()),this,SLOT(dirLoaded()));
    connect(modelList,SIGNAL(directoryLoaded(QString)),&timer,SLOT(start()));

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
    tree->scrollTo(tree->currentIndex());

    createActions();
    createToolBars();
    createMenus();

    restoreState(settings->value("windowState").toByteArray(),1);
    resize(settings->value("size", QSize(600, 400)).toSize());

    setWindowIcon(QIcon(":/images/qtfm.png"));

    QTimer::singleShot(2,this,SLOT(lateStart()));
}

//---------------------------------------------------------------------------
void MainWindow::lateStart()
{
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

    wrapBookmarksAct->setChecked(settings->value("wrapBookmarks",0).toBool());
    bookmarksList->setWrapping(wrapBookmarksAct->isChecked());

    detailTree->header()->restoreState(settings->value("header").toByteArray());

    bookmarksList->setResizeMode(QListView::Adjust);
    bookmarksList->setFlow(QListView::TopToBottom);
    bookmarksList->setIconSize(QSize(24,24));

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

    watcher = new QFileSystemWatcher(QStringList() << "/etc/mtab");
    connect(watcher,SIGNAL(fileChanged(QString)),this,SLOT(fileWatcherTriggered(QString)));

    term = settings->value("term").toString();
    progress = 0;
    clipboardChanged();

    status->addPermanentWidget(statusName);
    status->addPermanentWidget(statusSize);
    status->addPermanentWidget(statusDate);

    connect(bookmarksList,SIGNAL(activated(QModelIndex)),this,SLOT(bookmarkClicked(QModelIndex)));
    connect(bookmarksList,SIGNAL(clicked(QModelIndex)),this,SLOT(bookmarkClicked(QModelIndex)));
    connect(bookmarksList,SIGNAL(pressed(QModelIndex)),this,SLOT(bookmarkPressed(QModelIndex)));

    connect(pathEdit,SIGNAL(activated(int)),this,SLOT(pathEditChanged(int)));
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

    connect(modelList,SIGNAL(thumbUpdate(QModelIndex)),list,SLOT(update(QModelIndex)));

    qApp->setKeyboardInputInterval(1000);
    
    tree->setMouseTracking(true);
    connect(tree,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));

    connect(&daemon,SIGNAL(newConnection()),this,SLOT(newConnection()));

    if(isDaemon) startDaemon();
    else show();

    status->showMessage(getDriveInfo(curIndex.filePath()));

    QTimer::singleShot(10,this,SLOT(readCustomActions()));
}

//---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();

    if(isDaemon)
    {
        this->setVisible(0);
        startDaemon();
        tree->collapseAll();
        tabs->setCurrentIndex(0);
        tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(startPath)));
        tree->scrollTo(tree->currentIndex());

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

    curIndex = name.filePath();
    setWindowTitle(curIndex.fileName() + " - qtFM v5.1");

    if(tree->hasFocus() && QApplication::mouseButtons() == Qt::MidButton)
    {
        listItemPressed(modelList->index(name.filePath()));
        tabs->setCurrentIndex(tabs->count() - 1);
        if(currentView == 2) detailTree->setFocus(Qt::TabFocusReason);
        else list->setFocus(Qt::TabFocusReason);
    }

    if(curIndex.filePath() != pathEdit->currentText())
    {
        if(tabs->count()) tabs->addHistory(curIndex.filePath());
        pathEdit->insertItem(0,curIndex.filePath());
        pathEdit->setCurrentIndex(0);
    }

    if(!bookmarksList->hasFocus()) bookmarksList->clearSelection();

    modelList->blockSignals(1);
    modelList->setRootPath(name.filePath());

    QTimer::singleShot(8,this,SLOT(treeSelectionChangedLate()));
}

//---------------------------------------------------------------------------
void MainWindow::treeSelectionChangedLate()
{
    QModelIndex baseIndex = modelList->index(curIndex.filePath());

    if(currentView == 2) detailTree->setRootIndex(baseIndex);
    else list->setRootIndex(baseIndex);
    modelList->blockSignals(0);

    if(tabs->count())
    {
        tabs->setTabText(tabs->currentIndex(),curIndex.fileName());
        tabs->setTabData(tabs->currentIndex(),curIndex.filePath());
        tabs->setIcon(tabs->currentIndex());
    }

    if(backIndex.isValid())
    {
        listSelectionModel->setCurrentIndex(backIndex,QItemSelectionModel::ClearAndSelect);
        if(currentView == 2) detailTree->scrollTo(backIndex);
        else list->scrollTo(backIndex);
    }
    else
    {
        listSelectionModel->blockSignals(1);
        listSelectionModel->clear();
    }

    listSelectionModel->blockSignals(0);
    timer.start(50);
}

//---------------------------------------------------------------------------
void MainWindow::dirLoaded()
{
    timer.stop();

    if(backIndex.isValid())
    {
        backIndex = QModelIndex();
        return;
    }

    qint64 bytes = 0;
    QModelIndexList items;

    for(int x = 0; x < modelList->rowCount(modelList->index(pathEdit->currentText())); ++x)
        items.append(modelList->index(x,0,modelList->index(pathEdit->currentText())));

    foreach(QModelIndex theItem,items)
        bytes = bytes + modelList->size(theItem);

    QString total;

    if(!bytes) total = "";
    else total = formatSize(bytes);

    statusName->clear();
    statusSize->setText(QString("%1 items").arg(items.count()));
    statusDate->setText(QString("%1").arg(total));

    if(list->viewMode() == QListView::IconMode)
        if(thumbsAct->isChecked()) QtConcurrent::run(modelList,&myModel::loadThumbs,items);
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

    curIndex = modelList->fileInfo(listSelectionModel->currentIndex());

    qint64 bytes = 0;
    int folders = 0;
    int files = 0;

    foreach(QModelIndex theItem,items)
    {
	if(modelList->isDir(theItem)) folders++;
	else files++;
        bytes = bytes + modelList->size(theItem);
    }

    QString total,name;

    if(!bytes) total = "";
    else total = formatSize(bytes);

    if(items.count() == 1)
    {
        QFileInfo file(modelList->filePath(items.at(0)));

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

    if (num >= tb) total = QString("%1TB").arg(QString::number(qreal(num) / tb, 'f', 3));
    else if (num >= gb) total = QString("%1GB").arg(QString::number(qreal(num) / gb, 'f', 2));
    else if (num >= mb) total = QString("%1MB").arg(QString::number(qreal(num) / mb, 'f', 1));
    else if (num >= kb) total = QString("%1KB").arg(QString::number(qreal(num) / kb,'f',1));
    else total = QString("%1 bytes").arg(num);

    return total;
}

//---------------------------------------------------------------------------
void MainWindow::listItemClicked(QModelIndex current)
{
    if(current.data(32).toString() == pathEdit->currentText()) return;

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if(mods == Qt::ControlModifier || mods == Qt::ShiftModifier) return;
    if(modelList->isDir(current))
	tree->setCurrentIndex(modelTree->mapFromSource(current));
}

//---------------------------------------------------------------------------
void MainWindow::listItemPressed(QModelIndex current)
{
    //middle-click -> open new tab
    //ctrl+middle-click -> open new instance

    if(QApplication::mouseButtons() == Qt::MidButton)
        if(modelList->isDir(current))
        {
            Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();

            if(mods == Qt::ControlModifier)
                openFile();
            else
            {
                addTab(modelList->filePath(current));
            }
        }
        else
            openFile();
}

//---------------------------------------------------------------------------
void MainWindow::openTab()
{
    addTab(curIndex.filePath());
}

//---------------------------------------------------------------------------
int MainWindow::addTab(QString path)
{
    if(tabs->count() == 0) tabs->addNewTab(pathEdit->currentText(),currentView);
    return tabs->addNewTab(path,currentView);
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

    if(modelList->isDir(current))
        tree->setCurrentIndex(modelTree->mapFromSource(current));
    else
        executeFile(current,0);
}

//---------------------------------------------------------------------------
void MainWindow::itemHover(QModelIndex current)
{
    status->showMessage(modelList->fileName(current));
}

//---------------------------------------------------------------------------
void MainWindow::executeFile(QModelIndex index, bool run)
{
    QProcess *myProcess = new QProcess(this);

    if(run)
	myProcess->startDetached(modelList->filePath(index));             //is executable?
    else
    {
        foreach(QAction *action, customActions->values(QFileInfo(modelList->filePath(index)).suffix()))
            if(action->text() == "Open")
            {
                action->trigger();
                return;
            }

        myProcess->startDetached("xdg-open",QStringList() << modelList->filePath(index));
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

    if(pathEdit->currentText().contains(pathEdit->itemText(1)))
        backIndex = modelList->index(pathEdit->currentText());

    pathEdit->removeItem(0);
    if(tabs->count()) tabs->remHistory();

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(pathEdit->itemText(0))));
}

//---------------------------------------------------------------------------
void MainWindow::goHomeDir()
{
    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(QDir::homePath())));
}

//---------------------------------------------------------------------------
void MainWindow::pathEditChanged(int index)
{
    if(index == 0) return;			    //hit return on current item

    QString info(pathEdit->itemText(index));
    if(!QFileInfo(info).exists()) return;


    info.replace("~",QDir::homePath());

    if(info.contains("/.")) modelList->setRootPath(info);

    if(info == pathEdit->itemText(1))				    //down arrow
    {
	pathEdit->removeItem(1);
	pathEdit->removeItem(0);
    }
    else
	pathEdit->removeItem(index);

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(info)));
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
    int num = 1;

    if(!QFileInfo(pathEdit->itemText(0)).isWritable())
    {
        status->showMessage(tr("Read only...cannot create folder"));
        return;
    }

    do
    {
        newDir = modelList->mkdir(modelList->index(pathEdit->itemText(0)),QString("new_folder%1").arg(num));
        num++;
    }
    while (!newDir.isValid());

    listSelectionModel->setCurrentIndex(newDir,QItemSelectionModel::ClearAndSelect);

    if(stackWidget->currentIndex() == 0) list->edit(newDir);
    else detailTree->edit(newDir);
}

//---------------------------------------------------------------------------
void MainWindow::newFile()
{
    QModelIndex fileIndex;
    int num = 0;

    if(!QFileInfo(pathEdit->itemText(0)).isWritable())
    {
        status->showMessage(tr("Read only...cannot create file"));
        return;
    }

    do
    {
        num++;
        fileIndex = modelList->index(pathEdit->itemText(0) + QString("/new_file%1").arg(num));
    }
    while (fileIndex.isValid());


    QFile newFile(pathEdit->itemText(0) + QString("/new_file%1").arg(num));

    newFile.open(QIODevice::WriteOnly);
    newFile.close();

    fileIndex = modelList->index(QFileInfo(newFile).filePath());
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
	if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
	else selList = listSelectionModel->selectedIndexes();

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
            list->setRootIndex(modelList->index(pathEdit->currentText()));

    if(iconAct->isChecked())
    {
        currentView = 1;
        list->setViewMode(QListView::IconMode);
        list->setGridSize(QSize(zoom+40,zoom+40)); //90
        list->setIconSize(QSize(zoom,zoom)); //48
        list->setFlow(QListView::LeftToRight);
        list->setWordWrap(1);
        modelList->setMode(thumbsAct->isChecked());
	thumbsAct->setEnabled(1);

	stackWidget->setCurrentIndex(0);
	detailAct->setChecked(0);
	disconnect(detailTree,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));
	detailTree->setMouseTracking(false);

        if(thumbsAct->isChecked()) dirLoaded();

        connect(list,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));

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
        list->setWordWrap(0);
        modelList->setMode(0);
	thumbsAct->setEnabled(0);
        disconnect(list,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));
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
}

//---------------------------------------------------------------------------
void MainWindow::toggleDetails()
{
    if(detailAct->isChecked() == false)
    {
	toggleIcons();

	stackWidget->setCurrentIndex(0);
        disconnect(detailTree,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));
	detailTree->setMouseTracking(false);
    }
    else
    {
        currentView = 2;
        if(detailTree->rootIndex() != modelList->index(pathEdit->currentText()))
            detailTree->setRootIndex(modelList->index(pathEdit->currentText()));
        connect(detailTree,SIGNAL(entered(QModelIndex)),this,SLOT(itemHover(QModelIndex)));
        detailTree->setMouseTracking(true);

	detailTree->setSortingEnabled(true);
        stackWidget->setCurrentIndex(1);
	thumbsAct->setEnabled(0);
        modelList->setMode(0);
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
	{
	    listSelectionModel->clear();
	}
	modelList->setFilter(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System);
    }
    else
        modelList->setFilter(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System | QDir::Hidden);
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

    if(focusWidget() == tree) selList << modelList->index(pathEdit->itemText(0));
    else
	if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
	else selList = listSelectionModel->selectedIndexes();


    foreach(QModelIndex item, selList)
	fileList.append(modelList->filePath(item));

    clearCutItems();
    modelList->addCutItems(fileList);

    //save a temp file to allow pasting in a different instance
    QFile tempFile(QDir::tempPath() + "/qtfm.temp");
    tempFile.open(QIODevice::WriteOnly);
    QDataStream out(&tempFile);
    out << fileList;
    tempFile.close();

    QApplication::clipboard()->setMimeData(modelList->mimeData(selList));

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
	if(focusWidget() == tree) selList << modelList->index(pathEdit->itemText(0));
	else return;

    clearCutItems();

    QStringList text;
    foreach(QModelIndex item,selList)
        text.append(modelList->filePath(item));

    QApplication::clipboard()->setText(text.join("\n"),QClipboard::Selection);
    QApplication::clipboard()->setMimeData(modelList->mimeData(selList));
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
	    if(temp.isDir() && QFileInfo(newPath + "/" + temp.fileName()).exists())	//merge or replace?
	    {
                QMessageBox message(QMessageBox::Question,tr("Existing folder"),QString("<b>%1</b><p>Already exists!<p>What do you want to do?")
		    .arg(newPath + "/" + temp.fileName()),QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

                message.button(QMessageBox::Yes)->setText(tr("Merge"));
                message.button(QMessageBox::No)->setText(tr("Replace"));

		int merge = message.exec();
		if(merge == QMessageBox::Cancel) return;
		if(merge == QMessageBox::Yes) recurseFolder(temp.filePath(),temp.fileName(),&completeList);
		else modelList->remove(modelList->index(newPath + "/" + temp.fileName()));
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
		    modelList->remove(modelList->index(temp.filePath()));
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

        if(QFileInfo(newFiles.first()).path() == pathEdit->currentText())       //highlight new files if visible
            foreach(QString item, newFiles)
                listSelectionModel->select(modelList->index(item),QItemSelectionModel::Select);

        connect(listSelectionModel,SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)),this,SLOT(listSelectionChanged(const QItemSelection, const QItemSelection)));
        curIndex.setFile(newFiles.first());

        if(currentView == 2) detailTree->scrollTo(modelList->index(newFiles.at(0)),QAbstractItemView::EnsureVisible);
        else list->scrollTo(modelList->index(newFiles.at(0)),QAbstractItemView::EnsureVisible);

        if(QFile(QDir::tempPath() + "/qtfm.temp").exists()) QApplication::clipboard()->clear();

        modelList->clearCutItems();

        modelList->setRootPath("");				//changing rootPath forces reread, updates file sizes
        modelList->setRootPath(pathEdit->currentText());
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

    if((qint64) info.f_bavail*4096 < total)
    {
        emit copyProgressFinished(2,newFiles);
        return 0;
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
    if(cut) QFile::remove(source);  //if file is cut remove the source
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
    if(focusWidget() == bookmarksList) selList.append(modelList->index(bookmarksList->currentIndex().data(32).toString()));
    else if(focusWidget() == list || focusWidget() == detailTree)
        if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
        else selList = listSelectionModel->selectedIndexes();

    if(selList.count() == 0) selList << modelList->index(pathEdit->currentText());

    QStringList paths;

    foreach(QModelIndex item, selList)
        paths.append(modelList->filePath(item));

    properties = new propertiesDialog(paths, modelList->folderIcons, modelList->mimeIcons);
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
void MainWindow::xdgConfig()
{
    if(!listSelectionModel->currentIndex().isValid()) return;

    QDialog *xdgConfig = new QDialog(this);
    xdgConfig->setWindowTitle(tr("Configure filetype"));

    QString mimeType = getMimeType(curIndex.filePath());

    QLabel *label = new QLabel("Filetype: <b>" + mimeType + "</b><p>Open with:");
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
    apps.sort();
    appList->addItems(apps);

    //now get the icons
    QDir appIcons("/usr/share/pixmaps","",0,QDir::Files | QDir::NoDotAndDotDot);
    apps = appIcons.entryList();
    QIcon defaultIcon = QIcon::fromTheme("application-x-executable");

    for(int i = 0; i < appList->count(); ++i)
    {
        QString baseName = appList->itemText(i).remove(".desktop");
        QPixmap temp = QIcon::fromTheme(baseName).pixmap(16,16);

        if(!temp.isNull()) appList->setItemIcon(i,temp);
        else
        {
            QStringList searchIcons = apps.filter(baseName);
            if(searchIcons.count() > 0) appList->setItemIcon(i,QIcon("/usr/share/pixmaps/" + searchIcons.at(0)));
            else appList->setItemIcon(i,defaultIcon);
        }
    }


    QProcess *myProcess = new QProcess(this);
    myProcess->start("xdg-mime",QStringList() << "query" << "default" << mimeType);
    myProcess->waitForFinished();
    appList->setCurrentIndex(appList->findText(myProcess->readAllStandardOutput().trimmed(),Qt::MatchFixedString));

    if(xdgConfig->exec())
    {
	myProcess->start("xdg-mime",QStringList() << "default" << appList->currentText() << mimeType);
    }
    delete xdgConfig;
}

//---------------------------------------------------------------------------
void MainWindow::readCustomActions()
{
    customMenus = new QMultiHash<QString,QMenu*>;

    settings->beginGroup("customActions");
    QStringList keys = settings->childKeys();
    for(int i = 0; i < keys.count(); ++i)
    {
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
    bool layout = 0;
    if(QToolBar *tb = qobject_cast<QToolBar *>(childAt(event->pos()))) layout = 1;
    if(childAt(event->pos()) == status) layout = 1;
    if(layout)
    {
        QMenu *popup = createPopupMenu();
        popup->addSeparator();
        popup->addAction(lockLayoutAct);
        popup->exec(event->globalPos());
        return;
    }

    QList<QAction*> actions;
    QMenu *popup = new QMenu(this);

    if(focusWidget() == list || focusWidget() == detailTree)
    {
	bookmarksList->clearSelection();

	if(listSelectionModel->hasSelection())	    //could be file or folder
	{
	    if(!curIndex.isDir())		    //file
	    {
                actions = customActions->values(curIndex.suffix());

                foreach(QAction*action, actions)
                    if(action->text() == "Open")
                    {
                        popup->addAction(action);
                        break;
                    }

                if(popup->actions().count() == 0) popup->addAction(openAct);

		if(curIndex.isExecutable()) popup->addAction(runAct);
		popup->addActions(actions);

                foreach(QMenu* parent, customMenus->values(curIndex.suffix()))
                    popup->addMenu(parent);

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
                actions = customActions->values(curIndex.canonicalPath());    //children of $parent
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
                actions.append(customActions->values(curIndex.canonicalPath()));    //children of $parent
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
	actions.append(customActions->values(curIndex.canonicalPath()));
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
        QFileInfo file = modelList->fileInfo(listSelectionModel->currentIndex());
	if(file.isDir())
	    cmd.replace("%n",file.fileName().replace(" ","\\"));
	else
	    cmd.replace("%n",file.baseName().replace(" ","\\"));

	if(listSelectionModel->selectedRows(0).count()) selList = listSelectionModel->selectedRows(0);
	else selList = listSelectionModel->selectedIndexes();
    }
    else
	selList << modelList->index(curIndex.filePath());


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
	temp.append(modelList->fileName(index).replace(" ","\\"));

    cmd.replace("%f",temp.join(" "));

    temp.clear();
    foreach(QModelIndex index,selList)
	temp.append(modelList->filePath(index).replace(" ","\\"));

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

    //refresh view to update file sizes etc.
    modelList->setRootPath("");                                 //changing rootPath forces reread, updates file sizes
    modelList->setRootPath(pathEdit->currentText());

    if(process->processEnvironment().contains("QTFM"))
    {
        QString output = process->readAllStandardError();
        if(!output.isEmpty()) QMessageBox::warning(this,tr("Error - Custom action"),output);

        output = process->readAllStandardOutput();
        if(!output.isEmpty()) QMessageBox::information(this,tr("Output - Custom action"),output);
    }

    process->deleteLater();
}

//---------------------------------------------------------------------------------
void MainWindow::refresh()
{
    QApplication::clipboard()->clear();
    listSelectionModel->clear();

    modelList->setRootPath("");				//changing rootPath forces reread, updates file sizes
    modelList->setRootPath(pathEdit->currentText());

    clearCutItems();
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
    //this refreshes existing items to remove cut colour
    if(focusWidget() == tree) modelTree->invalidate();
    else
    {
	QModelIndex baseIndex = modelList->index(pathEdit->currentText());
	list->setRootIndex(baseIndex);
	detailTree->setRootIndex(baseIndex);
    }

    modelList->clearCutItems();

    return;
}

//---------------------------------------------------------------------------------
bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());

    if (fileModel->isDir(index0)) return true;
    else return false;
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
    QModelIndex idx = index;
    QStringList list;
    do {
	QString t = model()->data(idx, Qt::EditRole).toString();
	list.prepend(t);
	QModelIndex parent = idx.parent();
	idx = parent.sibling(parent.row(), index.column());
    } while (idx.isValid());

    list[0].clear() ; // the join below will provide the separator

    return list.join("/");
}
