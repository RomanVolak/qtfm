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


#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    //connect to daemon if available, otherwise create new instance
    if(app.arguments().count() == 1)
    {
        QLocalServer server;
        if(!server.listen("qtfm"))
        {
            QLocalSocket client;
            client.connectToServer("qtfm");
            client.waitForConnected(1000);
            if(client.state() != QLocalSocket::ConnectedState) QFile::remove(QDir::tempPath() + "/qtfm");
            else
            {
                client.close();
                return 0;
            }
        }
        server.close();
    }

    Q_INIT_RESOURCE(resources);

    app.setOrganizationName("qtfm");
    app.setApplicationName("qtfm");

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator qtfmTranslator;
    qtfmTranslator.load("/usr/share/qtfm/qtfm_" + QLocale::system().name());
    app.installTranslator(&qtfmTranslator);

    MainWindow mainWin;
    return app.exec();
}

