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
    src/tabbar.h \
    src/mymodelitem.h
SOURCES += src/main.cpp \
    src/mainwindow.cpp \
    src/customactions.cpp \
    src/mymodel.cpp \
    src/bookmarks.cpp \
    src/progressdlg.cpp \
    src/icondlg.cpp \
    src/propertiesdlg.cpp \
    src/tabbar.cpp \
    src/actions.cpp \
    src/mymodelitem.cpp

CONFIG += release warn_off thread
RESOURCES += resources.qrc
QT+= network
LIBS += -lmagic

TARGET = qtfm
target.path = /usr/bin
desktop.files += qtfm.desktop
desktop.path += /usr/share/applications
icon.files += images/qtfm.png
icon.path += /usr/share/pixmaps

docs.path += /usr/share/doc/qtfm
docs.files += README CHANGELOG COPYING

trans.path += /usr/share/qtfm
trans.files += translations/qtfm_da.qm \
	       translations/qtfm_de.qm \
	       translations/qtfm_es.qm \
	       translations/qtfm_fr.qm \
               translations/qtfm_it.qm \
	       translations/qtfm_pl.qm \
               translations/qtfm_ru.qm \
               translations/qtfm_sr.qm \
               translations/qtfm_sv.qm \
               translations/qtfm_zh.qm \
               translations/qtfm_zh_TW.qm

INSTALLS += target desktop icon docs trans










