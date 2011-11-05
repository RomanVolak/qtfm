TEMPLATE = app
DEPENDPATH += . src
INCLUDEPATH += . src
OBJECTS_DIR = build
MOC_DIR = build
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
CONFIG += thread warn_on debug_and_release build_all
RESOURCES += resources.qrc
QT += network core gui
TARGET = qtfm
target.path = /usr/bin
desktop.files += qtfm.desktop
desktop.path += /usr/share/applications
icon.files += images/qtfm.png
icon.path += /usr/share/pixmaps
INSTALLS += target desktop icon
TRANSLATIONS += l10n/qtfm_ru.ts
