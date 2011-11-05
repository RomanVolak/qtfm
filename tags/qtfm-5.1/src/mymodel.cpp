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


#include "mymodel.h"

//---------------------------------------------------------------------------------
myModel::myModel()
{
    mimeGeneric = new QHash<QString,QString>;
    mimeGlob = new QHash<QString,QString>;
    mimeIcons = new QHash<QString,QIcon>;
    folderIcons = new QHash<QString,QIcon>;
    thumbs = new QHash<QString,QByteArray>;
    icons = new QHash<QString,QIcon>;

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
void myModel::loadThumbs(QModelIndexList indexes)
{
    QStringList files,types;
    types << "jpg" << "png" << "bmp" << "ico" << "svg" << "gif";

    foreach(QModelIndex item,indexes)
    {
        if(types.contains(QFileInfo(fileName(item)).suffix(),Qt::CaseInsensitive))
            files.append(filePath(item));
    }

    if(files.count())
    {
        if(thumbs->count() == 0)
        {
            QFile fileIcons(QDir::homePath() + "/.config/qtfm/thumbs.cache");
            fileIcons.open(QIODevice::ReadOnly);
            QDataStream out(&fileIcons);
            out >> *thumbs;
            fileIcons.close();
            thumbCount = thumbs->count();
        }

        foreach(QString item, files)
        {
            if(!thumbs->contains(item)) thumbs->insert(item,getThumb(item));
            emit thumbUpdate(this->index(item));
        }
    }
}


//---------------------------------------------------------------------------
QByteArray myModel::getThumb(QString item)
{
    QImage theThumb, background;
    QImageReader pic(item);
    int w,h,target;
    w = pic.size().width();
    h = pic.size().height();

    if( w > 128 || h > 128)
    {
        target = 114;
        background.load(":/images/background.jpg");
    }
    else
    {
        target = 64;
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

    return buffer.buffer();
}

//---------------------------------------------------------------------------------
QVariant myModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::ForegroundRole)
    {
        QFileInfo type(filePath(index));
	if(cutItems.contains(type.filePath())) return QBrush(QColor(Qt::lightGray));
        if(type.isHidden()) return QBrush(QColor(Qt::darkGray));
        if(type.isSymLink()) return QBrush(QColor(Qt::blue));
        if(type.isDir()) return QFileSystemModel::data(index,role);
        if(type.isExecutable()) return QBrush(QColor(Qt::darkGreen));
    }
    else
    if(role == Qt::TextAlignmentRole)
    {
	if(index.column() == 1) return Qt::AlignRight + Qt::AlignVCenter;
    }
    else
    if(role == Qt::DisplayRole)
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
    }
    else
    if(role == Qt::DecorationRole)
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
                if(icons->contains(type.filePath())) return icons->value(type.filePath());
                else
                    if(thumbs->contains(type.filePath()))
                    {
                        QPixmap pic;
                        pic.loadFromData(thumbs->value(type.filePath()));
                        icons->insert(type.filePath(),QIcon(pic));
                        return icons->value(type.filePath());
                    }
            }

            QString suffix = type.suffix();
            if(mimeIcons->contains(suffix)) return mimeIcons->value(suffix);

            QIcon theIcon;

            if(suffix.isEmpty())
            {
                if(type.isExecutable()) suffix = "exec";
                else suffix = "none";

                if(mimeIcons->contains(suffix)) return mimeIcons->value(suffix);

                if(suffix == "exec") theIcon = QIcon::fromTheme("application-x-executable");
                else theIcon = QIcon(qApp->style()->standardIcon(QStyle::SP_FileIcon));
             }
            else
            {
		if(mimeGlob->count() == 0) loadMimeTypes();

                //try mimeType as it is
		QString mimeType = mimeGlob->value(type.suffix().toLower());
                if(QIcon::hasThemeIcon(mimeType)) theIcon = QIcon::fromTheme(mimeType);
                else
                {
                    //try matching generic icon
		    if(QIcon::hasThemeIcon(mimeGeneric->value(mimeType))) theIcon = QIcon::fromTheme(mimeGeneric->value(mimeType));
                    else
                    {
                        //last resort try adding "-x-generic" to base type
                        if(QIcon::hasThemeIcon(mimeType.split("-").at(0) + "-x-generic")) theIcon = QIcon::fromTheme(mimeType.split("-").at(0) + "-x-generic");
                        else theIcon = QIcon(qApp->style()->standardIcon(QStyle::SP_FileIcon));
                    }
                }
            }

            mimeIcons->insert(suffix,theIcon);
	    return theIcon;
        }
    }
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

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if(mods != Qt::ControlModifier)                                     //cut by default, holding ctrl is copy
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
            case 0: return tr("Name");
            case 1: return tr("Size");
            case 2: return tr("Type");
            case 4: return tr("Owner");
            case 3: return tr("Date Modified");
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


