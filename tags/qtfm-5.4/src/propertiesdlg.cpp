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
#include "propertiesdlg.h"
#include "icondlg.h"
#include "mainwindow.h"
#include <sys/vfs.h>
#include <sys/stat.h>
#include <magic.h>

//---------------------------------------------------------------------------
propertiesDialog::propertiesDialog(QStringList paths, myModel *modelList)
{
    setWindowTitle(tr("Properties"));
    fileList = paths;
    pathName = paths.at(0);
    model = modelList;
    QFileInfo file(pathName);

    folderIcons = 0;
    fileIcons = 0;
    iconChanged = 0;

    files = 0;
    folders = 0;
    totalSize = 0;

    QVBoxLayout *layout = new QVBoxLayout(this);
    QGroupBox *fileFrame = new QGroupBox(this);
    QGroupBox *driveFrame = new QGroupBox(this);
    QGroupBox * permissions;

    path = new QLabel;
    path->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    sizeInfo = new QLabel;
    containsInfo = new QLabel;
    modifiedInfo = new QLabel(file.lastModified().toString());
    permissionsNumeric = new QLineEdit;

    QGridLayout *layoutPath = new QGridLayout(fileFrame);
    layoutPath->addWidget(path,0,1);
    layoutPath->setRowMinimumHeight(0,30);

    layoutPath->addWidget(new QLabel(tr("Size:")),2,0);
    layoutPath->addWidget(sizeInfo,2,1,Qt::AlignRight);
    layoutPath->addWidget(containsInfo,3,1,Qt::AlignRight);

    if(paths.count() == 1)
    {
        if(file.isDir())
        {
            type = 1;
            folderIcons = modelList->folderIcons;

            iconButton = new QToolButton;
            if(folderIcons->contains(file.fileName())) iconButton->setIcon(folderIcons->value(file.fileName()));
            else iconButton->setIcon(QIcon::fromTheme("folder"));

            iconButton->setIconSize(QSize(24,24));
            iconButton->setAutoRaise(1);
            connect(iconButton,SIGNAL(clicked()),this,SLOT(changeIcon()));

            layoutPath->addWidget(iconButton,0,0);
            layoutPath->addWidget(new QLabel(tr("Contains:")),3,0);
        }
        else
        {
            type = 2;
            fileIcons = modelList->mimeIcons;

            QLabel *iconLabel = new QLabel();
            QIcon theIcon;

            if(file.suffix().isEmpty())
            {
                if(file.isExecutable()) theIcon = fileIcons->value("exec");
                else theIcon = fileIcons->value("none");
            }
            else
                theIcon = fileIcons->value(file.suffix());

            iconLabel->setPixmap(theIcon.pixmap(24,24));

            layoutPath->addWidget(iconLabel,0,0);
            layoutPath->addWidget(new QLabel(tr("Filetype:")),3,0);
            containsInfo->setText(gGetMimeType(pathName));
        }

        path->setWordWrap(1);
        path->setMinimumWidth(140);
        path->setText("<b>" + pathName);
        layoutPath->addWidget(new QLabel(tr("Modified:")),4,0);
        layoutPath->addWidget(modifiedInfo,4,1,Qt::AlignRight);
    }
    else
    {
        type = 3;

        //calculate selected files and folders count
        foreach(QString selectedPaths, paths)
        {
            if (QFileInfo(selectedPaths).isDir()) folders++;
            else files++;
        }

        if((files) && (folders))
            path->setText(QString(tr("<b>%1 files, %2 folders")).arg(files).arg(folders));
        else if((!files) && (folders)) //no files, folders only
            path->setText(QString(tr("<b>%1 folders")).arg(folders));
        else if((files) && (!folders)) //no folders, files only
            path->setText(QString(tr("<b>%1 files")).arg(files));

        QLabel *iconLabel = new QLabel();
        iconLabel->setPixmap(QIcon::fromTheme("folder-new").pixmap(24,24));
        layoutPath->addWidget(iconLabel,0,0);
        layoutPath->addWidget(new QLabel(tr("Total:")),3,0);
    }


    //permissions
    if(files == 0 || folders == 0)
    {
        permissions = new QGroupBox(this);
        QGridLayout *layoutPermissions = new QGridLayout(permissions);

        ownerRead = new QCheckBox;
        ownerWrite = new QCheckBox;
        ownerExec = new QCheckBox;

        groupRead = new QCheckBox;
        groupWrite = new QCheckBox;
        groupExec = new QCheckBox;

        otherRead = new QCheckBox;
        otherWrite = new QCheckBox;
        otherExec = new QCheckBox;

        connect(ownerRead,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(ownerWrite,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(ownerExec,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));

        connect(groupRead,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(groupWrite,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(groupExec,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));

        connect(otherRead,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(otherWrite,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));
        connect(otherExec,SIGNAL(clicked(bool)),this,SLOT(checkboxesChanged()));

        layoutPermissions->addWidget(new QLabel(tr("Read")),1,0);
        layoutPermissions->addWidget(new QLabel(tr("Write")),2,0);
        layoutPermissions->addWidget(new QLabel(tr("Execute")),3,0);

        layoutPermissions->addWidget(new QLabel(" "),0,1);     //blank column
        layoutPermissions->setColumnStretch(1,1);

        layoutPermissions->addWidget(new QLabel(tr("Owner")),0,2);
        layoutPermissions->addWidget(new QLabel(tr("Group")),0,3);
        layoutPermissions->addWidget(new QLabel(tr("Other")),0,4);

        layoutPermissions->addWidget(ownerRead,1,2);
        layoutPermissions->addWidget(ownerWrite,2,2);
        layoutPermissions->addWidget(ownerExec,3,2);

        layoutPermissions->addWidget(groupRead,1,3);
        layoutPermissions->addWidget(groupWrite,2,3);
        layoutPermissions->addWidget(groupExec,3,3);

        layoutPermissions->addWidget(otherRead,1,4);
        layoutPermissions->addWidget(otherWrite,2,4);
        layoutPermissions->addWidget(otherExec,3,4);

        layoutPermissions->setVerticalSpacing(2);

        connect(permissionsNumeric,SIGNAL(textChanged(QString)),this,SLOT(numericChanged(QString)));
        layoutPermissions->addWidget(new QLabel(tr("Numeric")),4,0,1,1);
        layoutPermissions->addWidget(permissionsNumeric,4,2,1,3);

        struct stat perms;
        stat(pathName.toLocal8Bit(), &perms);
        permString = QString("%1%2%3").arg(((perms.st_mode & S_IRWXU) >> 6))
                                    .arg(((perms.st_mode & S_IRWXG) >> 3))
                                    .arg((perms.st_mode & S_IRWXO));

        permissionsNumeric->setText(permString);

        QRegExp input("^[0-7]*$");
        QValidator *permNumericValidator = new QRegExpValidator(input, this);
        permissionsNumeric->setValidator(permNumericValidator);
        permissionsNumeric->setMaxLength(3);

        int ret = chmod(pathName.toLocal8Bit(),permString.toInt(0,8));
        if(ret) permissions->setDisabled(1);
    }


    //drive info frame
    QLabel *driveIcon = new QLabel(this);
    driveIcon->setPixmap(QIcon::fromTheme("drive-harddisk").pixmap(24,24));
    driveInfo = new QLabel();

    QGridLayout *layoutDrive = new QGridLayout(driveFrame);
    layoutDrive->addWidget(driveIcon,0,0);
    layoutDrive->addWidget(driveInfo,0,1,Qt::AlignRight);
    driveInfo->setText(getDriveInfo(pathName));

    //buttons
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(0);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    //main layout
    layout->addWidget(fileFrame);

    if(files == 0 || folders == 0) layout->addWidget(permissions);

    layout->addWidget(driveFrame);
    layout->addWidget(buttons);
    setLayout(layout);

    layout->setMargin(6);
    layout->setSpacing(4);

    connect(this,SIGNAL(updateSignal()),this,SLOT(update()));
    connect(this,SIGNAL(finishedSignal()),this,SLOT(finished()));

    show();
    setMinimumSize(size());

    this->setAttribute(Qt::WA_DeleteOnClose,1);
    thread = QtConcurrent::run(this,&propertiesDialog::folderProperties, paths);
}

//---------------------------------------------------------------------------
void propertiesDialog::folderProperties(QStringList paths)
{
    foreach(QString path, paths)
        recurseProperties(path);

    emit updateSignal();
    emit finishedSignal();
}

//---------------------------------------------------------------------------
void propertiesDialog::recurseProperties(QString path)
{
    QDirIterator it(path,QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden, QDirIterator::Subdirectories);
    if(it.hasNext())
        while (it.hasNext())
        {
            if(this->result()) return;      //user cancelled

            it.next();
            if(it.fileInfo().isDir())
            {
                folders++;
                if(folders % 32 == 0) emit updateSignal();
            }
            else
            {
                files++;
                totalSize += it.fileInfo().size();
            }
        }
    else
        totalSize += QFileInfo(path).size();
}

//---------------------------------------------------------------------------
void propertiesDialog::update()
{
    sizeInfo->setText(formatSize(totalSize));
    if(type != 2) containsInfo->setText(QString(tr("%1 Files, %2 folders")).arg(files).arg(folders));
}

//---------------------------------------------------------------------------
void propertiesDialog::finished()
{
    buttons->button(QDialogButtonBox::Ok)->setEnabled(1);
    this->activateWindow();
}

//---------------------------------------------------------------------------
void propertiesDialog::accept()
{
    this->setResult(1);
    thread.waitForFinished();

    if(permissionsNumeric->text() != permString)
    {
        foreach(QString file, fileList)
        {
            chmod(file.toLocal8Bit(),permissionsNumeric->text().toInt(0,8));    //convert to octal

            myModelItem *item = static_cast<myModelItem*>(model->index(file).internalPointer());
            item->mPermissions.clear();
        }
    }

    if(iconChanged) folderIcons->insert(QFileInfo(pathName).fileName(),iconButton->icon());

    emit propertiesUpdated();
    this->done(1);
}

//---------------------------------------------------------------------------
void propertiesDialog::reject()
{
    this->setResult(1);
    thread.waitForFinished();
    this->done(0);
}

//---------------------------------------------------------------------------
void propertiesDialog::checkboxesChanged()
{
    permissionsNumeric->setText(QString("%1%2%3").arg(ownerRead->isChecked()*4 + ownerWrite->isChecked()*2 + ownerExec->isChecked())
                                .arg(groupRead->isChecked()*4 + groupWrite->isChecked()*2 + groupExec->isChecked())
                                .arg(otherRead->isChecked()*4 + otherWrite->isChecked()*2 + otherExec->isChecked()));
}

//---------------------------------------------------------------------------
void propertiesDialog::numericChanged(QString text)
{
    if(text.count() != 3) return;

    int owner = QString(text.at(0)).toInt();
    ownerRead->setChecked(owner / 4);
    ownerWrite->setChecked((owner - owner / 4 * 4- owner % 2));
    ownerExec->setChecked(owner % 2);

    int group = QString(text.at(1)).toInt();
    groupRead->setChecked(group / 4);
    groupWrite->setChecked((group - group / 4 * 4 - group % 2));
    groupExec->setChecked(group % 2);

    int other = QString(text.at(2)).toInt();
    otherRead->setChecked(other / 4);
    otherWrite->setChecked((other - other / 4 * 4 - other % 2));
    otherExec->setChecked(other % 2);
}

//---------------------------------------------------------------------------
void propertiesDialog::changeIcon()
{
    icondlg *icons = new icondlg;
    if(icons->exec() == 1)
    {
        iconChanged = 1;
        iconButton->setIcon(QIcon::fromTheme(icons->result));
    }
    delete icons;
}

//---------------------------------------------------------------------------
QString getDriveInfo(QString path)
{
    struct statfs info;
    statfs(path.toLocal8Bit(), &info);

    if(info.f_blocks == 0) return "";

    return QString("%1  /  %2  (%3%)").arg(formatSize((qint64) (info.f_blocks - info.f_bavail)*info.f_bsize))
                       .arg(formatSize((qint64) info.f_blocks*info.f_bsize))
                       .arg((info.f_blocks - info.f_bavail)*100/info.f_blocks);
}

//---------------------------------------------------------------------------------
QString gGetMimeType(QString path)
{
    magic_t cookie = magic_open(MAGIC_MIME);
    magic_load(cookie,0);
    QString temp = magic_file(cookie,path.toLocal8Bit());
    magic_close(cookie);

    return temp.left(temp.indexOf(";"));
}
