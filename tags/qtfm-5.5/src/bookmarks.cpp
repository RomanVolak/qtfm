/****************************************************************************
* This file is part of qtFM, a simple, fast file manager.
* Copyright (C) 2010,2011,2012Wittfella
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

#ifndef BOOKMARKS_CPP
#define BOOKMARKS_CPP

#include <QtGui>
#include "bookmarkmodel.h"
#include "icondlg.h"
#include "mainwindow.h"

//---------------------------------------------------------------------------
void MainWindow::addBookmarkAction()
{
    modelBookmarks->addBookmark(curIndex.fileName(),curIndex.filePath(),"0","");
}

//---------------------------------------------------------------------------
void MainWindow::addSeparatorAction()
{
    modelBookmarks->addBookmark("","","","");
}

//---------------------------------------------------------------------------
bookmarkmodel::bookmarkmodel(QHash<QString,QIcon> * icons)
{
    folderIcons = icons;
}

//---------------------------------------------------------------------------
void bookmarkmodel::addBookmark(QString name, QString path, QString isAuto, QString icon)
{
    if(path.isEmpty())	    //add seperator
    {
        QStandardItem *item = new QStandardItem(QIcon::fromTheme(icon),"");
        item->setData(QBrush(QPixmap(":/images/sep.png")),Qt::BackgroundRole);
        QFlags<Qt::ItemFlag> flags = item->flags();
        flags ^= Qt::ItemIsEditable;                    //not editable
        item->setFlags(flags);
        item->setFont(QFont("sans",8));                 //force size to prevent 2 rows of background tiling
        this->appendRow(item);
        return;
    }

    QIcon theIcon;
    theIcon = QIcon::fromTheme(icon,QApplication::style()->standardIcon(QStyle::SP_DirIcon));

    if(icon.isEmpty()) if(folderIcons->contains(name)) theIcon = folderIcons->value(name);

    if(name.isEmpty()) name = "/";
    QStandardItem *item = new QStandardItem(theIcon,name);
    item->setData(path,32);
    item->setData(icon,33);
    item->setData(isAuto,34);
    this->appendRow(item);
}

//---------------------------------------------------------------------------
void MainWindow::mountWatcherTriggered()
{
    QTimer::singleShot(1000,this,SLOT(autoBookmarkMounts()));
}

//---------------------------------------------------------------------------
void MainWindow::autoBookmarkMounts()
{
    QList<QStandardItem *> theBookmarks = modelBookmarks->findItems("*",Qt::MatchWildcard);

    QStringList autoBookmarks;

    foreach(QStandardItem *item, theBookmarks)
    {
        if(item->data(34).toString() == "1")		    //is an automount
            autoBookmarks.append(item->data(32).toString());
    }

    QStringList mtabMounts;
    QFile mtab("/etc/mtab");
    mtab.open(QFile::ReadOnly);
    QTextStream stream(&mtab);
    do mtabMounts.append(stream.readLine());
    while (!stream.atEnd());
    mtab.close();

    QStringList sysMounts = QStringList() << "/dev" << "/sys" << "/pro" << "/tmp" << "/run";
    QStringList dontShowList = settings->value("hideBookmarks",0).toStringList();
    mounts.clear();

    foreach(QString item, mtabMounts)
	if(!sysMounts.contains(item.split(" ").at(1).left(4)))
        {
            QString path = item.split(" ").at(1);
            path.replace("\\040"," ");

            mounts.append(path);
            if(!dontShowList.contains(path))
                if(!autoBookmarks.contains(path))	    //add a new auto bookmark if it doesn't exist
                {
			autoBookmarks.append(path);
                    if(item.split(" ").at(2) == "iso9660") modelBookmarks->addBookmark(path,path,"1","drive-optical");
                    else if(item.split(" ").at(2).contains("fat")) modelBookmarks->addBookmark(path,path,"1","drive-removable-media");
                    else modelBookmarks->addBookmark(path,path,"1","drive-harddisk");
                }
        }

//remove existing automounts that no longer exist
    foreach(QStandardItem *item, theBookmarks)
        if(autoBookmarks.contains(item->data(32).toString()))
            if(!mounts.contains(item->data(32).toString()))
                modelBookmarks->removeRow(item->row());
}

//---------------------------------------------------------------------------
void MainWindow::delBookmark()
{
    QModelIndexList list = bookmarksList->selectionModel()->selectedIndexes();

    while(!list.isEmpty())
    {
        if(list.first().data(34).toString() == "1")		//automount, add to dontShowList
        {
            QStringList temp = settings->value("hideBookmarks",0).toStringList();
            temp.append(list.first().data(32).toString());
            settings->setValue("hideBookmarks",temp);
        }
        modelBookmarks->removeRow(list.first().row());
        list = bookmarksList->selectionModel()->selectedIndexes();
    }

}

//---------------------------------------------------------------------------------
void MainWindow::editBookmark()
{
    icondlg * themeIcons = new icondlg;
    if(themeIcons->exec() == 1)
    {
        QStandardItem * item = modelBookmarks->itemFromIndex(bookmarksList->currentIndex());
        item->setData(themeIcons->result,33);
        item->setIcon(QIcon::fromTheme(themeIcons->result));
    }
    delete themeIcons;
}

//---------------------------------------------------------------------------
void MainWindow::toggleWrapBookmarks()
{
    bookmarksList->setWrapping(wrapBookmarksAct->isChecked());
    settings->setValue("wrapBookmarks",wrapBookmarksAct->isChecked());
}

//---------------------------------------------------------------------------
void MainWindow::bookmarkPressed(QModelIndex current)
{
    if(QApplication::mouseButtons() == Qt::MidButton)
        tabs->setCurrentIndex(addTab(current.data(32).toString()));
}

//---------------------------------------------------------------------------
void MainWindow::bookmarkClicked(QModelIndex item)
{
    if(item.data(32).toString() == pathEdit->currentText()) return;

    QString info(item.data(32).toString());
    if(info.isEmpty()) return;                                  //separator
    if(info.contains("/.")) modelList->setRootPath(info);       //hidden folders

    tree->setCurrentIndex(modelTree->mapFromSource(modelList->index(item.data(32).toString())));
    status->showMessage(getDriveInfo(curIndex.filePath()));
}

//---------------------------------------------------------------------------------
QStringList bookmarkmodel::mimeTypes() const
{
    return QStringList() << "application/x-qstandarditemmodeldatalist" << "text/uri-list";
}

//---------------------------------------------------------------------------------
bool bookmarkmodel::dropMimeData(const QMimeData * data,Qt::DropAction action,int row,int column,const QModelIndex & parent )
{
    //moving its own items around
    if(data->hasFormat("application/x-qstandarditemmodeldatalist"))
	if(parent.column() == -1)
	    return QStandardItemModel::dropMimeData(data,action,row,column,parent);


    QList<QUrl> files = data->urls();
    QStringList cutList;

    foreach(QUrl path, files)
    {
        QFileInfo file(path.toLocalFile());

        //drag to bookmark window, add a new bookmark
        if(parent.column() == -1)
        {
            if(file.isDir()) this->addBookmark(file.fileName(),file.filePath(),0,"");
            return false;
        }
        else
            if(action == 2)                             //cut
                cutList.append(file.filePath());
    }

    emit bookmarkPaste(data, parent.data(32).toString(), cutList);

    return false;
}

#endif


