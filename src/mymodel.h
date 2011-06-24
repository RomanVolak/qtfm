/****************************************************************************
* This file is part of qtFM, a simple, fast file manager.
* Copyright (C) 2010 Wittfella
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


#ifndef MYMODEL_H
#define MYMODEL_H

#include <QtGui>

//---------------------------------------------------------------------------------
// Subclass QFileSystemModel and override 'data' to show mime filetype icons from
// current theme. Standard Qt class only ever returns default folder and file icon.
//
// Also override 'remove' because of Qt bug not removing hidden files.
//
// Also override 'dropMimeData' to fix Qt bug DragDrop not working for folders.
//---------------------------------------------------------------------------------

class myModel : public QFileSystemModel
{
    Q_OBJECT

public:
	myModel();
	void loadMimeTypes() const;
        void cacheInfo();
        void setMode(bool);
        bool remove(const QModelIndex & index ) const;
        bool dropMimeData(const QMimeData * data,Qt::DropAction action,int row,int column,const QModelIndex & parent);
        static QHash<QString,QByteArray> thumbsMap(QString);
	void loadThumbs(QString);
	void addCutItems(QStringList);
	void clearCutItems();
	QHash<QString,QIcon> *mimeIcons;
        QHash<QString,QIcon> *folderIcons;

signals:
        void dragDropPaste(const QMimeData * data, QString newPath, QStringList cutList);

protected:
        QVariant data(const QModelIndex & index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	int columnCount(const QModelIndex &parent) const;

private:

        bool showThumbs;
	int thumbCount;
	QStringList cutItems;
	QHash<QString,QString> *mimeGlob;
	QHash<QString,QString> *mimeGeneric;
	QHash<QString,QByteArray> *thumbs;
};

#endif // MYMODEL_H
