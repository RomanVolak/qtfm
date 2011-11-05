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


#include "icondlg.h"

//---------------------------------------------------------------------------
icondlg::icondlg()
{
    setWindowTitle(tr("Select icon"));

    iconList = new QListWidget;
    iconList->setIconSize(QSize(24,24));
    iconList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    buttons = new QDialogButtonBox;
    buttons->setStandardButtons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(iconList);
    layout->addWidget(buttons);
    setLayout(layout);

    QStringList themes, fileNames;
    QSettings inherits("/usr/share/icons/" + QIcon::themeName() + "/index.theme",QSettings::IniFormat,this);
    foreach(QString theme, inherits.value("Icon Theme/Inherits").toStringList())
	themes.prepend(theme);
    themes.append(QIcon::themeName());

    foreach(QString theme, themes)
    {
	QDirIterator it("/usr/share/icons/" + theme,QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
	    it.next();
	    if(it.filePath().contains("22"))
		fileNames.append(QFileInfo(it.fileName()).baseName());
	}
    }

    fileNames.removeDuplicates();
    fileNames.sort();
    foreach(QString name, fileNames)
	new QListWidgetItem(QIcon::fromTheme(name),name,iconList);
}

//---------------------------------------------------------------------------
void icondlg::accept()
{
    result = iconList->currentItem()->text();
    this->done(1);
}
