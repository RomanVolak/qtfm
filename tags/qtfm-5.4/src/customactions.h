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

#ifndef CUSTOMACTIONS_H
#define CUSTOMACTIONS_H

#include <QtGui>
#include <QDialog>

#include "mainwindow.h"

class customActionsDialog : public QDialog
{
    Q_OBJECT

public:
    customActionsDialog(MainWindow *parent = 0);


public slots:
    void accept();
    void addItem();
    void delItem();
    void infoItem();
    void readItems();
    void saveItems();
    void getIcon(QTreeWidgetItem *item,int column);

private:
    QVBoxLayout *mainLayout;
    QTreeWidget *treeWidget;
    QDialogButtonBox *buttons;
    QHBoxLayout *horizontalLayout;
    QToolButton *addButton;
    QToolButton *delButton;
    QToolButton *infoButton;
    MainWindow *mainWindow;
};

#endif // CUSTOMACTIONS_H
