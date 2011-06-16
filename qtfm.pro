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
    src/propertiesdlg.h
SOURCES += src/main.cpp \
    src/mainwindow.cpp \
    src/actions.cpp \
    src/customactions.cpp \
    src/mymodel.cpp \
    src/bookmarks.cpp \
    src/progressdlg.cpp \
    src/icondlg.cpp \
    src/propertiesdlg.cpp
CONFIG += release \
    warn_off
RESOURCES += resources.qrc
QT+= network
