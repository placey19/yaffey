#-------------------------------------------------
#
# Project created by QtCreator 2012-04-27T16:17:40
#
#-------------------------------------------------

QT        += core gui xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET     = yaffey
TEMPLATE   = app
RC_FILE    = yaffey.rc

SOURCES   += main.cpp\
    MainWindow.cpp \
    YaffsModel.cpp \
    YaffsItem.cpp \
    YaffsTreeView.cpp \
    DialogEditProperties.cpp \
    YaffsControl.cpp \
    yaffs2/yaffs_packedtags2.c \
    yaffs2/yaffs_hweight.c \
    yaffs2/yaffs_ecc.c \
    DialogFastboot.cpp \
    DialogImport.cpp \
    YaffsManager.cpp \
    Utils.cpp

HEADERS   += \
    MainWindow.h \
    YaffsModel.h \
    YaffsItem.h \
    YaffsTreeView.h \
    DialogEditProperties.h \
    YaffsControl.h \
    yaffs2/yaffs_trace.h \
    yaffs2/yaffs_packedtags2.h \
    yaffs2/yaffs_hweight.h \
    yaffs2/yaffs_guts.h \
    yaffs2/yaffs_ecc.h \
    AndroidIDs.h \
    Yaffs2.h \
    DialogFastboot.h \
    DialogImport.h \
    YaffsManager.h \
    Utils.h

FORMS     += \
    MainWindow.ui \
    DialogEditProperties.ui \
    DialogFastboot.ui \
    DialogImport.ui

RESOURCES += \
    icons.qrc
