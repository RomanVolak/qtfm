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

#include <mainwindow.h>
#include "mymodel.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/ioctl.h>

//---------------------------------------------------------------------------------
myModel::myModel(bool realMime)
{
    mimeGeneric = new QHash<QString,QString>;
    mimeGlob = new QHash<QString,QString>;
    mimeIcons = new QHash<QString,QIcon>;
    folderIcons = new QHash<QString,QIcon>;
    thumbs = new QHash<QString,QByteArray>;
    icons = new QCache<QString,QIcon>;
    icons->setMaxCost(500);

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

    rootItem = new myModelItem(QFileInfo("/"),new myModelItem(QFileInfo(),0));

    currentRootPath = "/";

    QDir root("/");
    QFileInfoList drives = root.entryInfoList(QDir::AllEntries | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    foreach(QFileInfo drive, drives)
        new myModelItem(drive,rootItem);

    rootItem->walked = true;
    rootItem = rootItem->parent();

    iconFactory = new QFileIconProvider();

    inotifyFD = inotify_init();
    notifier = new QSocketNotifier(inotifyFD, QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)), this, SLOT(notifyChange()));
    connect(&eventTimer,SIGNAL(timeout()),this,SLOT(eventTimeout()));

    realMimeTypes = realMime;
}

//---------------------------------------------------------------------------------------
myModel::~myModel()
{
    delete rootItem;
    delete iconFactory;
}

//---------------------------------------------------------------------------------------
QModelIndex myModel::index(int row, int column, const QModelIndex &parent) const
{
    if(parent.isValid() && parent.column() != 0)
        return QModelIndex();

    myModelItem *parentItem = static_cast<myModelItem*>(parent.internalPointer());
    if(!parentItem) parentItem = rootItem;

    myModelItem *childItem = parentItem->childAt(row);
        if(childItem) return createIndex(row, column, childItem);

    return QModelIndex();
}

//---------------------------------------------------------------------------------------
QModelIndex myModel::index(const QString& path) const
{
    myModelItem *item = rootItem->matchPath(path.split(SEPARATOR),0);

    if(item) return createIndex(item->childNumber(),0,item);

    return QModelIndex();
}

//---------------------------------------------------------------------------------------
QModelIndex myModel::parent(const QModelIndex &index) const
{
    if(!index.isValid()) return QModelIndex();

    myModelItem *childItem = static_cast<myModelItem*>(index.internalPointer());

    if(!childItem) return QModelIndex();

    myModelItem *parentItem = childItem->parent();

    if (!parentItem || parentItem == rootItem) return QModelIndex();

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

//---------------------------------------------------------------------------------------
bool myModel::isDir(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item && item != rootItem) return item->fileInfo().isDir();

    return false;
}

//---------------------------------------------------------------------------------------
QFileInfo myModel::fileInfo(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item) return item->fileInfo();

    return QFileInfo();
}

//---------------------------------------------------------------------------------------
qint64 myModel::size(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item) return item->fileInfo().size();

    return 0;
}

//---------------------------------------------------------------------------------------
QString myModel::fileName(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item) return item->fileName();

    return "";
}

//---------------------------------------------------------------------------------------
QString myModel::filePath(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item) return item->absoluteFilePath();

    return false;
}

//---------------------------------------------------------------------------------------
QString myModel::getMimeType(const QModelIndex &index)
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(item->mMimeType.isNull())
    {
        if(realMimeTypes) item->mMimeType = gGetMimeType(item->absoluteFilePath());
        else
        {
            if(item->fileInfo().isDir()) item->mMimeType = "folder";
            else item->mMimeType = item->fileInfo().suffix();
            if(item->mMimeType.isNull()) item->mMimeType = "file";
        }
    }

    return item->mMimeType;
}

//---------------------------------------------------------------------------------------
void myModel::notifyChange()
{
    notifier->setEnabled(0);

    int buffSize = 0;
    ioctl(inotifyFD, FIONREAD, (char *) &buffSize);

    QByteArray buffer;
    buffer.resize(buffSize);
    read(inotifyFD,buffer.data(),buffSize);
    const char *at = buffer.data();
    const char * const end = at + buffSize;

    while (at < end)
    {
        const inotify_event *event = reinterpret_cast<const inotify_event *>(at);

        int w = event->wd;

        if(eventTimer.isActive())
        {
            if(w == lastEventID)
                eventTimer.start(40);
            else
            {
                eventTimer.stop();
                notifyProcess(lastEventID);
                lastEventID = w;
                eventTimer.start(40);
            }
        }
        else
        {
            lastEventID = w;
            eventTimer.start(40);
        }

        at += sizeof(inotify_event) + event->len;
    }

    notifier->setEnabled(1);
}

//---------------------------------------------------------------------------------------
void myModel::eventTimeout()
{
    notifyProcess(lastEventID);
    eventTimer.stop();
}

//---------------------------------------------------------------------------------------
void myModel::notifyProcess(int eventID)
{
    if(watchers.contains(eventID))
    {
        myModelItem *parent = rootItem->matchPath(watchers.value(eventID).split(SEPARATOR));

        if(parent)
        {
            parent->dirty = 1;

            QDir dir(parent->absoluteFilePath());
            QFileInfoList all = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

            foreach(myModelItem * child, parent->children())
            {
                if(all.contains(child->fileInfo()))
                {
                    //just remove known items
                    all.removeOne(child->fileInfo());
                }
                else
                {
                    //must of been deleted, remove from model
                    if(child->fileInfo().isDir())
                    {
                        int wd = watchers.key(child->absoluteFilePath());
                        inotify_rm_watch(inotifyFD,wd);
                        watchers.remove(wd);
                    }
                    beginRemoveRows(index(parent->absoluteFilePath()),child->childNumber(),child->childNumber());
                    parent->removeChild(child);
                    endRemoveRows();
                }
            }

            foreach(QFileInfo one, all)                 //only new items left in list
            {
                beginInsertRows(index(parent->absoluteFilePath()),parent->childCount(),parent->childCount());
                new myModelItem(one,parent);
                endInsertRows();
            }
        }
    }
    else
    {
        inotify_rm_watch(inotifyFD,eventID);
        watchers.remove(eventID);
    }
}

//---------------------------------------------------------------------------------
void myModel::addWatcher(myModelItem *item)
{
    while(item != rootItem)
    {
        watchers.insert(inotify_add_watch(inotifyFD, item->absoluteFilePath().toLocal8Bit(), IN_MOVE | IN_CREATE | IN_DELETE),item->absoluteFilePath()); //IN_ONESHOT | IN_ALL_EVENTS)
        item->watched = 1;
        item = item->parent();
    }
}

//---------------------------------------------------------------------------------
bool myModel::setRootPath(const QString& path)
{
    currentRootPath = path;

    myModelItem *item = rootItem->matchPath(path.split(SEPARATOR));

    if(item->watched == 0) addWatcher(item);

    if(item->walked == 0)
    {
        populateItem(item);
        return false;
    }
    else
    if(item->dirty)                         //model is up to date, but view needs to be invalidated
    {
        item->dirty = 0;
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------------------
bool myModel::canFetchMore (const QModelIndex & parent) const
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    if(item)
        if(item->walked) return false;

    return true;

}

//---------------------------------------------------------------------------------------
void myModel::fetchMore (const QModelIndex & parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    if(item)
    {
        populateItem(item);
        emit dataChanged(parent,parent);
    }

    return;
}

//---------------------------------------------------------------------------------------
void myModel::populateItem(myModelItem *item)
{
    item->walked = 1;

    QDir dir(item->absoluteFilePath());
    QFileInfoList all = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    foreach(QFileInfo one, all)
        new myModelItem(one,item);
}

//---------------------------------------------------------------------------------
int myModel::columnCount(const QModelIndex &parent) const
{
    return (parent.column() > 0) ? 0 : 5;
}

//---------------------------------------------------------------------------------------
int myModel::rowCount(const QModelIndex &parent) const
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());
    if(item) return item->childCount();
    return rootItem->childCount();
}

//---------------------------------------------------------------------------------
void myModel::refresh()
{
    myModelItem *item = rootItem->matchPath(QStringList("/"));

    //free all inotify watches
    foreach(int w, watchers.keys())
        inotify_rm_watch(inotifyFD,w);
    watchers.clear();

    beginResetModel();
    item->clearAll();
    endResetModel();
}

//---------------------------------------------------------------------------------
void myModel::update()
{
    myModelItem *item = rootItem->matchPath(currentRootPath.split(SEPARATOR));

    foreach(myModelItem *child, item->children())
        child->refreshFileInfo();
}

//---------------------------------------------------------------------------------
void myModel::refreshItems()
{
    myModelItem *item = rootItem->matchPath(currentRootPath.split(SEPARATOR));

    item->clearAll();
    populateItem(item);
}

//---------------------------------------------------------------------------------
QModelIndex myModel::insertFolder(QModelIndex parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    int num = 0;
    QString name;

    do
    {
        num++;
        name = QString("new_folder%1").arg(num);
    }
    while(item->hasChild(name));


    QDir temp(currentRootPath);
    if(!temp.mkdir(name)) return QModelIndex();

    beginInsertRows(parent,item->childCount(),item->childCount());
    new myModelItem(QFileInfo(currentRootPath + "/" + name),item);
    endInsertRows();

    return index(item->childCount() - 1,0,parent);
}

//---------------------------------------------------------------------------------
QModelIndex myModel::insertFile(QModelIndex parent)
{
    myModelItem *item = static_cast<myModelItem*>(parent.internalPointer());

    int num = 0;
    QString name;

    do
    {
        num++;
        name = QString("new_file%1").arg(num);
    }
    while (item->hasChild(name));


    QFile temp(currentRootPath + "/" + name);
    if(!temp.open(QIODevice::WriteOnly)) return QModelIndex();
    temp.close();

    beginInsertRows(parent,item->childCount(),item->childCount());
    new myModelItem(QFileInfo(temp),item);
    endInsertRows();

    return index(item->childCount()-1,0,parent);
}

//---------------------------------------------------------------------------------
Qt::DropActions myModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

//---------------------------------------------------------------------------------
QStringList myModel::mimeTypes() const
{
    return QStringList("text/uri-list");
}

//---------------------------------------------------------------------------------
QMimeData * myModel::mimeData(const QModelIndexList & indexes) const
{
    QMimeData *data = new QMimeData();

    QList<QUrl> files;

    foreach(QModelIndex index, indexes)
    {
        myModelItem *item = static_cast<myModelItem*>(index.internalPointer());
        files.append(QUrl::fromLocalFile(item->absoluteFilePath()));
    }

    data->setUrls(files);
    return data;
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
        if(fileIcons.size() > 10000000) fileIcons.remove();
        else
        {
            fileIcons.open(QIODevice::WriteOnly);
            out.setDevice(&fileIcons);
            out << *thumbs;
            fileIcons.close();
        }
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
    types << "jpg" << "jpeg" << "png" << "bmp" << "ico" << "svg" << "gif";

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
            emit thumbUpdate(index(item));
        }
    }
}


//---------------------------------------------------------------------------
QByteArray myModel::getThumb(QString item)
{
    QImage theThumb, background;
    QImageReader pic(item);
    int w = pic.size().width();
    int h = pic.size().height();

    background = QImage(128,128,QImage::Format_RGB32);
    background.fill(QApplication::palette().color(QPalette::Base).rgb());

    if( w > 128 || h > 128)
    {
        pic.setScaledSize(QSize(123,93));
        QImage temp = pic.read();

        theThumb.load(":/images/background.png");           //shadow template

        QPainter painter(&theThumb);
        painter.drawImage(QPoint(0,0),temp);
    }
    else
    {
        pic.setScaledSize(QSize(64,64));
        theThumb = pic.read();
    }

    QPainter painter(&background);
    painter.drawImage(QPoint((123 - theThumb.width())/2,(115 - theThumb.height())/2),theThumb);

    QBuffer buffer;
    QImageWriter writer(&buffer,"jpg");
    writer.setQuality(50);
    writer.write(background);

    return buffer.buffer();
}

//---------------------------------------------------------------------------------
QVariant myModel::data(const QModelIndex & index, int role) const
{
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    if(role == Qt::ForegroundRole)
    {
        QFileInfo type(item->fileInfo());

        if(cutItems.contains(type.filePath())) return colors.mid();
        else if(type.isHidden()) return colors.dark();
        else if(type.isSymLink()) return colors.link();
        else if(type.isDir()) return colors.windowText();
        else if(type.isExecutable()) return QBrush(QColor(Qt::darkGreen));
    }
    else
    if(role == Qt::TextAlignmentRole)
    {
        if(index.column() == 1) return Qt::AlignRight + Qt::AlignVCenter;
    }
    else
    if(role == Qt::DisplayRole)
    {
        QVariant data;
        switch(index.column())
        {
            case 0:
                data = item->fileName();
                break;
            case 1:
                if(item->fileInfo().isDir()) data = "";
                else data = formatSize(item->fileInfo().size());
                break;
            case 2:
                if(item->mMimeType.isNull())
                {
                    if(realMimeTypes) item->mMimeType = gGetMimeType(item->absoluteFilePath());
                    else
                    {
                        if(item->fileInfo().isDir()) item->mMimeType = "folder";
                        else item->mMimeType = item->fileInfo().suffix();
                        if(item->mMimeType.isNull()) item->mMimeType = "file";
                    }
                }
                data = item->mMimeType;
                break;
            case 3:
                data = item->fileInfo().lastModified().toString(Qt::LocalDate);
                break;
            case 4:
                {
                    if(item->mPermissions.isNull())
                    {
                        QString str;

                        QFlags<QFile::Permissions> perms = item->fileInfo().permissions();
                        if(perms.testFlag(QFile::ReadOwner)) str.append("r"); else str.append(("-"));
                        if(perms.testFlag(QFile::WriteOwner)) str.append("w"); else str.append(("-"));
                        if(perms.testFlag(QFile::ExeOwner)) str.append("x"); else str.append(("-"));
                        if(perms.testFlag(QFile::ReadGroup)) str.append("r"); else str.append(("-"));
                        if(perms.testFlag(QFile::WriteGroup)) str.append("w"); else str.append(("-"));
                        if(perms.testFlag(QFile::ExeGroup)) str.append("x"); else str.append(("-"));
                        if(perms.testFlag(QFile::ReadOther)) str.append("r"); else str.append(("-"));
                        if(perms.testFlag(QFile::WriteOther)) str.append("w"); else str.append(("-"));
                        if(perms.testFlag(QFile::ExeOther)) str.append("x"); else str.append(("-"));
                        str.append(" " + item->fileInfo().owner() + " " + item->fileInfo().group());
                        item->mPermissions = str;
                    }
                    return item->mPermissions;
                }
            default:
                data = "";
                break;
        }
        return data;
    }
    else
    if(role == Qt::DecorationRole)
    {
        if(index.column() != 0) return QVariant();

        QFileInfo type(item->fileInfo());

        if(type.isDir())
        {
            if(folderIcons->contains(type.fileName())) return folderIcons->value(type.fileName());
            return iconFactory->icon(type);
        }
        else
        {
            if(showThumbs)
            {
                if(icons->contains(item->absoluteFilePath())) return *icons->object(item->absoluteFilePath());
                else
                    if(thumbs->contains(item->absoluteFilePath()))
                    {
                        QPixmap pic;
                        pic.loadFromData(thumbs->value(item->absoluteFilePath()));
                        icons->insert(item->absoluteFilePath(),new QIcon(pic),1);
                        return *icons->object(item->absoluteFilePath());
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

                theIcon = QIcon(qApp->style()->standardIcon(QStyle::SP_FileIcon));
                if(suffix == "exec") theIcon = QIcon::fromTheme("application-x-executable",theIcon);
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
    else
    if(role == Qt::EditRole)
    {
        return item->fileName();
    }
    if(role == Qt::StatusTipRole)
    {
        return item->fileName();
    }

    return QVariant();
}

//---------------------------------------------------------------------------------
bool myModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    //can only set the filename
    myModelItem *item = static_cast<myModelItem*>(index.internalPointer());

    //physically change the name on disk
    bool ok = QFile::rename(item->absoluteFilePath(),item->parent()->absoluteFilePath() + SEPARATOR + value.toString());

    //change the details in the modelItem
    if(ok)
    {
        item->mMimeType.clear();                //clear the suffix/mimetype in case the user changes type
        item->changeName(value.toString());
        emit dataChanged(index,index);
    }

    return ok;
}

//---------------------------------------------------------------------------------
bool myModel::remove(const QModelIndex & theIndex)
{
    myModelItem *item = static_cast<myModelItem*>(theIndex.internalPointer());

    QString path = item->absoluteFilePath();

    //physically remove files from disk
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
        if(info.isDir())
        {
            int wd = watchers.key(info.filePath());
            inotify_rm_watch(inotifyFD,wd);
            watchers.remove(wd);
            error |= QDir().rmdir(info.filePath());
        }
        else error |= QFile::remove(info.filePath());
    }

    //remove from model
    beginRemoveRows(index(item->parent()->absoluteFilePath()),item->childNumber(),item->childNumber());
    item->parent()->removeChild(item);
    endRemoveRows();
    return error;
}

//---------------------------------------------------------------------------------
bool myModel::dropMimeData(const QMimeData * data,Qt::DropAction action,int row,int column,const QModelIndex & parent )
{
    if(isDir(parent))
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
    }
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

    return QVariant();
}

//---------------------------------------------------------------------------------------
Qt::ItemFlags myModel::flags(const QModelIndex &index) const
{
    if(!index.isValid()) return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
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



