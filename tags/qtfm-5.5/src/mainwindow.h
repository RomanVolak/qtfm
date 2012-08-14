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


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include <QSettings>
#include <QSplitter>
#include <QTreeView>
#include <QListView>
#include <QLabel>
#include <QStackedWidget>
#include <QSortFilterProxyModel>
#include <QComboBox>
#include <QSignalMapper>

#include "mymodel.h"
#include "bookmarkmodel.h"
#include "progressdlg.h"
#include "propertiesdlg.h"
#include "icondlg.h"
#include "tabbar.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
QT_END_NAMESPACE

//---------------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    myModel *modelList;

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void treeSelectionChanged(QModelIndex,QModelIndex);
    void listSelectionChanged(const QItemSelection, const QItemSelection);
    void listDoubleClicked(QModelIndex);
    void lateStart();
    void goUpDir();
    void goBackDir();
    void goHomeDir();
    void deleteFile();
    void cutFile();
    void copyFile();
    bool cutCopyFile(QString source, QString dest, qint64 totalSize, bool cut);
    bool pasteFile(QList<QUrl> files,QString newPath, QStringList cutList);
    void newDir();
    void newFile();
    void pathEditChanged(QString);
    void terminalRun();
    void executeFile(QModelIndex, bool);
    void runFile();
    void openFile();
    void clipboardChanged();
    void toggleDetails();
    void toggleHidden();
    void toggleIcons();
    void toggleThumbs();
    void addBookmarkAction();
    void addSeparatorAction();
    void delBookmark();
    void editBookmark();
    void toggleWrapBookmarks();
    bool xdgConfig();
    void readCustomActions();
    void editCustomActions();
    bool copyFolder(QString, QString, qint64, bool);
    void autoBookmarkMounts();
    void renameFile();
    void actionMapper(QString);
    void editShortcuts();
    void folderPropertiesLauncher();
    void bookmarkClicked(QModelIndex);
    void bookmarkPressed(QModelIndex);
    void bookmarkShortcutTrigger();
    void contextMenuEvent(QContextMenuEvent *);
    void toggleLockLayout();
    void pasteLauncher(const QMimeData * data, QString newPath, QStringList cutList);
    void pasteClipboard();
    void progressFinished(int,QStringList);
    void mountWatcherTriggered();
    void listItemClicked(QModelIndex);

    void listItemPressed(QModelIndex);
    void tabChanged(int index);
    void openTab();
    void tabsOnTop();
    int addTab(QString path);

    void refresh();
    void clearCutItems();
    void customActionFinished(int ret);
    void customActionError(QProcess::ProcessError error);
    void zoomInAction();
    void zoomOutAction();
    void focusAction();
    void openFolderAction();
    void newConnection();
    void startDaemon();
    void exitAction();

    void dirLoaded();
    void thumbUpdate(QModelIndex);

    void addressChanged(int,int);

signals:
    void updateCopyProgress(qint64, qint64, QString);
    void copyProgressFinished(int,QStringList);

private:
    void createActions();
    void createActionIcons();
    void createMenus();
    void createToolBars();
    void readShortcuts();
    void writeSettings();
    void recurseFolder(QString path, QString parent, QStringList *);

    int zoom;
    int zoomTree;
    int zoomList;
    int zoomDetail;
    int currentView;        //0=list, 1=icons, 2=details

    QCompleter *customComplete;

    tabBar *tabs;

    bool isDaemon;
    QLocalServer daemon;

    myProgressDialog * progress;
    propertiesDialog * properties;
    QSettings *settings;
    QDockWidget *dockTree;
    QDockWidget *dockBookmarks;
    QVBoxLayout *mainLayout;
    QStackedWidget *stackWidget;
    QTreeView *tree;
    QTreeView *detailTree;
    QListView *list;
    QListView *bookmarksList;
    QComboBox *pathEdit;

    QString term;
    QFileInfo curIndex;
    QModelIndex backIndex;

    QSortFilterProxyModel *modelTree;
    QSortFilterProxyModel *modelView;

    bookmarkmodel *modelBookmarks;
    QItemSelectionModel *treeSelectionModel;
    QItemSelectionModel *listSelectionModel;
    QSocketNotifier *notify;

    QStringList mounts;

    QList<QAction*> *actionList;
    QList<QIcon> *actionIcons;
    QMultiHash<QString,QAction*> *customActions;
    QMultiHash<QString,QMenu*> *customMenus;
    QSignalMapper *customMapper;

    QToolBar *editToolBar;
    QToolBar *viewToolBar;
    QToolBar *navToolBar;
    QToolBar *addressToolBar;
    QToolBar *menuToolBar;
    QStatusBar * status;
    QLabel * statusSize;
    QLabel * statusName;
    QLabel * statusDate;
    QString startPath;

    QAction *closeAct;
    QAction *exitAct;
    QAction *upAct;
    QAction *backAct;
    QAction *homeAct;
    QAction *hiddenAct;
    QAction *detailAct;
    QAction *addBookmarkAct;
    QAction *addSeparatorAct;
    QAction *delBookmarkAct;
    QAction *editBookmarkAct;
    QAction *wrapBookmarksAct;
    QAction *deleteAct;
    QAction *iconAct;
    QAction *newDirAct;
    QAction *newFileAct;
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
    QAction *customAct;
    QAction *shortcutsAct;
    QAction *editFiletypeAct;
    QAction *renameAct;
    QAction *terminalAct;
    QAction *openAct;
    QAction *runAct;
    QAction *thumbsAct;
    QAction *folderPropertiesAct;
    QAction *lockLayoutAct;
    QAction *refreshAct;
    QAction *escapeAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *focusAddressAct;
    QAction *focusTreeAct;
    QAction *focusBookmarksAct;
    QAction *focusListAct;
    QAction *openFolderAct;
    QAction *openTabAct;
    QAction *closeTabAct;
    QAction *tabsOnTopAct;
};

//---------------------------------------------------------------------------------
// Subclass QSortFilterProxyModel and override 'filterAcceptsRow' to only show
// directories in tree and not files.
//---------------------------------------------------------------------------------
class mainTreeFilterProxyModel : public QSortFilterProxyModel
{
protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
};


//---------------------------------------------------------------------------------
// Subclass QSortFilterProxyModel and override 'lessThan' for sorting in list/details views
//---------------------------------------------------------------------------------
class viewsSortProxyModel : public QSortFilterProxyModel
{
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};


//---------------------------------------------------------------------------------
// Subclass QCompleter so we can use the SortFilterProxyModel above for the address bar.
//---------------------------------------------------------------------------------
class myCompleter : public QCompleter
{
public:
    QString pathFromIndex(const QModelIndex& index) const;
    QStringList splitPath(const QString& path) const;
};

//---------------------------------------------------------------------------------
// Global functions.
//---------------------------------------------------------------------------------
QString formatSize(qint64);
QString getDriveInfo(QString);
QString gGetMimeType(QString);
#endif
