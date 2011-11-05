TEMPLATE = app
DEPENDPATH += . \
    src
INCLUDEPATH += . \
    src
OBJECTS_DIR = build
MOC_DIR = build

# Input
HEADERS += src/mainwindow.h \
    src/customactions.h \
    src/mymodel.h \
    src/bookmarkmodel.h \
    src/progressdlg.h \
    src/icondlg.h \
    src/propertiesdlg.h \
    src/tabbar.h
SOURCES += src/main.cpp \
    src/mainwindow.cpp \
    src/actions.cpp \
    src/customactions.cpp \
    src/mymodel.cpp \
    src/bookmarks.cpp \
    src/progressdlg.cpp \
    src/icondlg.cpp \
    src/propertiesdlg.cpp \
    src/tabbar.cpp
CONFIG += release warn_off thread
RESOURCES += resources.qrc
QT+= network

TARGET = qtfm
target.path = /usr/bin
desktop.files += qtfm.desktop
desktop.path += /usr/share/applications
icon.files += images/qtfm.png
icon.path += /usr/share/pixmaps
INSTALLS += target desktop icon