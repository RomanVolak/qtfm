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


#include "mymodel.h"

//---------------------------------------------------------------------------------
myModel::myModel()
{
    mimeGeneric = new QHash<QString,QString>;
    mimeGlob = new QHash<QString,QString>;
    mimeIcons = new QHash<QString,QIcon>;
    folderIcons = new QHash<QString,QIcon>;
    thumbs = new QHash<QString,QByteArray>;

    QFile fileIcons(QDir::homePath() + "/.config/qtfm/file.cache");
    fileIcons.open(QIODevice::ReadOnly);
    QDataStream out(&fileIcons);
    out >> *mimeIcons;
    fileIcons.close();

    fileIcons.setFileName(QDir::homePath() + "/.config/qtfm/folder.cache");
    fileIcons.open(QIODevice::ReadOnly);
    out.setDevice(&fileIcons);
    out >> *folderIcons;
    fileIcons.close();
}

//---------------------------------------------------------------------------------
void myModel::cacheInfo()
{
    QFile fileIcons(QDir::homePath() + "/.config/qtfm/file.cache");
    fileIcons.open(QIODevice::WriteOnly);
    QDataStream out(&fileIcons);
    out << *mimeIcons;
    fileIcons.close();

    fileIcons.setFileName(QDir::homePath() + "/.config/qtfm/folder.cache");
    fileIcons.open(QIODevice::WriteOnly);
    out.setDevice(&fileIcons);
    out << *folderIcons;
    fileIcons.close();

    if(thumbs->count() > thumbCount)
    {
	fileIcons.setFileName(QDir::homePath() + "/.config/qtfm/thumbs.cache");
	fileIcons.open(QIODevice::WriteOnly);
	out.setDevice(&fileIcons);
	out << *thumbs;
	fileIcons.close();
    }
}

//---------------------------------------------------------------------------------
void myModel::setMode(bool icons)
{
    showThumbs = icons;
}

//---------------------------------------------------------------------------------
void myModel::loadMimeTypes() const
{
    QFile mimeInfo("/usr/share/mime/globs");
    mimeInfo.open(QIODevice::ReadOnly);
    QTextStream out(&mimeInfo);

    do
    {
        QStringList line = out.readLine().split(":");
        if(line.count() == 2)
        {
            QString suffix = line.at(1);
            suffix.remove("*.");
            QString mimeName = line.at(0);
            mimeName.replace("/","-");
	    mimeGlob->insert(suffix,mimeName);
        }
    }
    while (!out.atEnd());

    mimeInfo.close();


    mimeInfo.setFileName("/usr/share/mime/generic-icons");
    mimeInfo.open(QIODevice::ReadOnly);
    out.setDevice(&mimeInfo);

    do
    {
        QStringList line = out.readLine().split(":");
        if(line.count() == 2)
        {
            QString mimeName = line.at(0);
            mimeName.replace("/","-");
            QString icon = line.at(1);
	    mimeGeneric->insert(mimeName,icon);
        }
    }
    while (!out.atEnd());

    mimeInfo.close();
}

//---------------------------------------------------------------------------
void myModel::loadThumbs(QString path)
{
    QFileInfoList list = QDir(path,"*.jpg;*.png;*.svg;*.gif",0,QDir::Files).entryInfoList();
    if(list.count() == 0) return;

    if(thumbs->count() == 0)
    {
	QFile fileIcons(QDir::homePath() + "/.config/qtfm/thumbs.cache");
	fileIcons.open(QIODevice::ReadOnly);
	QDataStream out(&fileIcons);
	out >> *thumbs;
	fileIcons.close();
	thumbCount = thumbs->count();
    }

    QStringList files;
    for(int x = 0; x < list.count(); ++x)
        if(!thumbs->contains(list.at(x).filePath())) files.append(list.at(x).filePath());

    if(files.count() == 0) return;

    QList<QHash<QString,QByteArray> > icons = QtConcurrent::blockingMapped(files,thumbsMap);

    for(int i = 0; i < icons.count(); ++i)
	thumbs->unite(icons.at(i));
}

//---------------------------------------------------------------------------
QHash<QString,QByteArray> myModel::thumbsMap(QString item)
{
    QImage theThumb, background;
    QImageReader pic(item);
    int w,h,target;
    w = pic.size().width();
    h = pic.size().height();

    if( w > 128 || h > 128)
    {
        target = 110;
        background.load(":/images/back.jpg");
    }
    else
    {
        target =64;
        background = QImage(128,128,QImage::Format_ARGB32);
        background.fill(QApplication::palette().color(QPalette::Base).rgb());
    }

    if(w > h)
    {
        int newHeight = h * target / w;
        pic.setScaledSize(QSize(target,newHeight));
    }
    else
    {
        int newWidth = w * target / h;
        pic.setScaledSize(QSize(newWidth,target));
    }

    theThumb = pic.read();

    int thumbWidth = theThumb.width();
    int thumbHeight = theThumb.height();

    QPainter painter(&background);
    painter.drawImage(QPoint((128-thumbWidth)/2,(128 - thumbHeight)/2),theThumb);

    QBuffer buffer;
    QImageWriter writer(&buffer,"jpg");
    writer.setQuality(50);
    writer.write(background);

    QHash<QString,QByteArray> temp;
    temp.insert(item,buffer.buffer());
    return temp;
}

//---------------------------------------------------------------------------------
QVariant myModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
        return QVariant();
 
    switch (role)
    {
    case Qt::ForegroundRole:
    {
        QFileInfo type(filePath(index));
	if(cutItems.contains(type.filePath())) return QBrush(QColor(Qt::lightGray));
        if(type.isSymLink()) return QBrush(QColor(Qt::blue));
        if(type.isDir()) return QFileSystemModel::data(index,role);
        if(type.isExecutable()) return QBrush(QColor(Qt::darkGreen));
        if(type.isHidden()) return QBrush(QColor(Qt::darkGray));
        break;
    }
    case Qt::TextAlignmentRole:
	if(index.column() == 1) return Qt::AlignRight + Qt::AlignVCenter;
        break;
    case Qt::DisplayRole:
    {
	if(index.column() == 4)
	{
	    QString str;
	    QFlags<QFile::Permissions> perms = permissions(index);
	    if(perms.testFlag(QFile::ReadOwner)) str.append("r"); else str.append(("-"));
	    if(perms.testFlag(QFile::WriteOwner)) str.append("w"); else str.append(("-"));
	    if(perms.testFlag(QFile::ExeOwner)) str.append("x"); else str.append(("-"));
	    if(perms.testFlag(QFile::ReadGroup)) str.append("r"); else str.append(("-"));
	    if(perms.testFlag(QFile::WriteGroup)) str.append("w"); else str.append(("-"));
	    if(perms.testFlag(QFile::ExeGroup)) str.append("x"); else str.append(("-"));
	    if(perms.testFlag(QFile::ReadOther)) str.append("r"); else str.append(("-"));
	    if(perms.testFlag(QFile::WriteOther)) str.append("w"); else str.append(("-"));
	    if(perms.testFlag(QFile::ExeOther)) str.append("x"); else str.append(("-"));
	    str.append(" " + fileInfo(index).owner() + " " + fileInfo(index).group());
	    return str;
	}
        break;
    }
    case Qt::DecorationRole:
    {
        if(index.column() != 0) return QVariant();

        QFileInfo type(filePath(index));

        if(isDir(index))
        {
            if(folderIcons->contains(type.fileName())) return folderIcons->value(type.fileName());
        }
        else
        {
	    if(showThumbs)
            {
		if(thumbs->contains(type.filePath()))
		{
		    QPixmap pic;
		    pic.loadFromData(thumbs->value(type.filePath()));
		    return QIcon(pic);
		}
            }

	    QIcon theIcon;
	    QString suffix(type.suffix());

            if(mimeIcons->contains(suffix))
                return mimeIcons->value(suffix);

            if(suffix.isEmpty())
            {
                if(type.isExecutable())
                {
                    suffix = "exec";
                    theIcon = QIcon::fromTheme("application-x-executable");
                }
                else
                {
                    suffix = "none";
                    theIcon = QIcon::fromTheme("text-x-generic");
                }
                if(mimeIcons->contains(suffix))
                    return mimeIcons->value(suffix);
            }
            else
            {
		if(mimeGlob->count() == 0)
                    loadMimeTypes();

                //try mimeType as it is
		QString mimeType(mimeGlob->value(type.suffix().toLower()));
                if(QIcon::hasThemeIcon(mimeType))
                {
                    theIcon = QIcon::fromTheme(mimeType);
                }
                else
                {
                    //try matching generic icon
		    if(QIcon::hasThemeIcon(mimeGeneric->value(mimeType)))
                    {
                        theIcon = QIcon::fromTheme(mimeGeneric->value(mimeType));
                    }
                    else
                    {
                        //last resort try adding "-x-generic" to base type
                        if(QIcon::hasThemeIcon(mimeType.split("-").at(0) + "-x-generic"))
                        {
                            theIcon = QIcon::fromTheme(mimeType.split("-").at(0) + "-x-generic");
                        }
                        else
                        {
                            theIcon = QIcon::fromTheme("text-x-generic");
                        }
                    }
                }
            }

	    mimeIcons->insert(suffix,theIcon);
	    return theIcon;
        }
        break;
    }
    default:
        return QFileSystemModel::data(index,role);
    } // switch

    // failback
    return QFileSystemModel::data(index,role);
}

//---------------------------------------------------------------------------------
bool myModel::remove(const QModelIndex & theIndex) const
{
    QString path = filePath(theIndex);

    QDirIterator it(path,QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    QStringList children;
    while (it.hasNext())
        children.prepend(it.next());
    children.append(path);

    children.removeDuplicates();

    bool error = false;
    for (int i = 0; i < children.count(); i++)
    {
        QFileInfo info(children.at(i));
        if (info.isDir()) error |= QDir().rmdir(info.filePath());
        else error |= QFile::remove(info.filePath());
    }
    return error;
}

//---------------------------------------------------------------------------------
bool myModel::dropMimeData(const QMimeData * data,Qt::DropAction action,int row,int column,const QModelIndex & parent )
{
    QList<QUrl> files = data->urls();
    QStringList cutList;

    //don't do anything if you drag and drop in same folder
    if(QFileInfo(files.at(0).path()).canonicalPath() == filePath(parent)) return false;

    if(action == 2)                             //cut, holding ctrl to copy is action 1
        foreach(QUrl item, files)
            cutList.append(item.path());

    emit dragDropPaste(data, filePath(parent), cutList);
    return false;
}

//---------------------------------------------------------------------------------
QVariant myModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole)
	switch(section)
	{
	    case 0: return "Name";
	    case 1: return "Size";
	    case 2: return "Type";
	    case 4: return "Owner";
	    case 3: return "Date Modified";
	    default: return QVariant();
	}

    return QFileSystemModel::headerData(section,orientation,role);
}

//---------------------------------------------------------------------------------
int myModel::columnCount(const QModelIndex &parent) const
{
    return (parent.column() > 0) ? 0 : 5;
}

//---------------------------------------------------------------------------------
void myModel::addCutItems(QStringList files)
{
    cutItems = files;
}

//---------------------------------------------------------------------------------
void myModel::clearCutItems()
{
    cutItems.clear();
    QFile(QDir::tempPath() + "/qtfm.temp").remove();
}



