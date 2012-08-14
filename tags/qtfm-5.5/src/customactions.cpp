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
#include "icondlg.h"
#include "customactions.h"

customActionsDialog::customActionsDialog(MainWindow *parent)
{
    mainWindow = parent;

    mainLayout = new QVBoxLayout();

    treeWidget = new QTreeWidget();
    treeWidget->setAlternatingRowColors(true);
    treeWidget->setRootIsDecorated(false);
    treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(treeWidget,SIGNAL(itemDoubleClicked(QTreeWidgetItem *,int)),this,SLOT(getIcon(QTreeWidgetItem *,int)));

    QTreeWidgetItem *header = treeWidget->headerItem();
    header->setText(0,tr("Filetype"));
    header->setText(1,tr("Text"));
    header->setText(2,tr("Icon"));
    header->setText(3,tr("Command"));

    treeWidget->setColumnWidth(1,160);
    treeWidget->setColumnWidth(2,160);

    treeWidget->setDragDropMode(QAbstractItemView::InternalMove);

    mainLayout->addWidget(treeWidget);

    horizontalLayout = new QHBoxLayout();
    addButton = new QToolButton();
    delButton = new QToolButton();
    infoButton= new QToolButton();
    addButton->setIcon(QIcon::fromTheme("list-add"));
    delButton->setIcon(QIcon::fromTheme("list-remove"));
    infoButton->setIcon(QIcon::fromTheme("dialog-question",QIcon::fromTheme("help-browser")));
    connect(addButton,SIGNAL(clicked()),this,SLOT(addItem()));
    connect(delButton,SIGNAL(clicked()),this,SLOT(delItem()));
    connect(infoButton,SIGNAL(clicked()),this,SLOT(infoItem()));


    buttons = new QDialogButtonBox;
    buttons->setStandardButtons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    horizontalLayout->addWidget(infoButton);
    horizontalLayout->addWidget(addButton);
    horizontalLayout->addWidget(delButton);
    horizontalLayout->addWidget(buttons);

    mainLayout->addLayout(horizontalLayout);
    setLayout(mainLayout);

    readItems();

    resize(600,360);

    setWindowTitle(tr("Custom Actions"));
}

//---------------------------------------------------------------------------
void customActionsDialog::accept()
{
    saveItems();
    this->done(1);
}

//---------------------------------------------------------------------------
void customActionsDialog::getIcon(QTreeWidgetItem * item, int column)
{
    if(column == 2)
    {
        icondlg *icons = new icondlg;
        if(icons->exec() == 1)
        {
            item->setText(column,icons->result);
            item->setIcon(column,QIcon::fromTheme(icons->result));
        }
        delete icons;
    }
    return;
}

//---------------------------------------------------------------------------
void customActionsDialog::addItem()
{
    treeWidget->clearSelection();
    QTreeWidgetItem *temp = new QTreeWidgetItem(treeWidget,QStringList() << "*",0);
    temp->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled|Qt::ItemIsDragEnabled|Qt::ItemIsUserCheckable);
    temp->setCheckState(3,Qt::Unchecked);
    temp->setSelected(true);
    treeWidget->setCurrentItem(temp);
    treeWidget->scrollToItem(temp);
}

//---------------------------------------------------------------------------
void customActionsDialog::delItem()
{
    delete treeWidget->currentItem();
}

//---------------------------------------------------------------------------
void customActionsDialog::infoItem()
{
    QMessageBox::question(this,tr("Usage"),tr("Use 'folder' to match all folders.<br>" \
                               "Use a folder name to match a specific folder.<br>" \
                               "Set text to 'Open' to override xdg default." \
                               "<p>%f - selected files<br>" \
                               "%F - selected files with full path<br>" \
                               "%n - current filename</p>" \
                               "<p>[] - tick checkbox to monitor output and errors.</p>" \
                               "<p>See <a href='http://www.qtfm.org/home/readme'>readme</a> for more help.</p>"));
}

//---------------------------------------------------------------------------
void customActionsDialog::readItems()
{
    QSettings settings;
    settings.beginGroup("customActions");

    QStringList keys = settings.childKeys();
    for(int i = 0; i < keys.count(); ++i)
    {
        QStringList temp = settings.value(keys.at(i)).toStringList();
        bool setChecked = 0;
        QString cmd = temp.at(3);
        if(cmd.at(0) == '|')
        {
            cmd.remove(0,1);
            setChecked = 1;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget,QStringList() << temp.at(0) << temp.at(1) << temp.at(2) << cmd,0);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled|Qt::ItemIsDragEnabled|Qt::ItemIsUserCheckable);
	
        if(setChecked) item->setCheckState(3,Qt::Checked);
        else item->setCheckState(3,Qt::Unchecked);
    }
    settings.endGroup();

    if(keys.count()==0)
    {
        QTreeWidgetItem *item1 = new QTreeWidgetItem(treeWidget, QStringList() << "gz,bz2" << tr("Extract here") << "package-x-generic" << "tar xf %f",0);
        QTreeWidgetItem *item2 = new QTreeWidgetItem(treeWidget, QStringList() << "folder" << tr("Term here") << "terminal" << "urxvt -cd %F",0);
        QTreeWidgetItem *item3 = new QTreeWidgetItem(treeWidget, QStringList() << "*" << tr("Compress") << "filesave" << "tar czf %n.tar.gz %f",0);
        item1->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);
        item2->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);
        item3->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);

        saveItems();
    }

    for(int x = 0;x < treeWidget->topLevelItemCount();x++)
	treeWidget->topLevelItem(x)->setIcon(2,QIcon::fromTheme(treeWidget->topLevelItem(x)->text(2)));
}

//---------------------------------------------------------------------------
void customActionsDialog::saveItems()
{
    QSettings settings;
    settings.remove("customActions");
    settings.beginGroup("customActions");
    for(int i = 0; i < treeWidget->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *item = treeWidget->topLevelItem(i);
        QStringList temp;
        QString cmd = item->text(3);
        
        if(item->checkState(3) == Qt::Checked) cmd.prepend("|");
        
        temp << item->text(0) << item->text(1) << item->text(2) << cmd;
        settings.setValue(QString(i),temp);
    }
    settings.endGroup();
    settings.setValue("customHeader",treeWidget->header()->saveState());
}
