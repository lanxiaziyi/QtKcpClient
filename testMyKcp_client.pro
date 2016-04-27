#-------------------------------------------------
#
# Project created by QtCreator 2016-04-24T12:11:49
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = testMyKcp_client
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    ikcp.c \
    alkcpclient.cpp \
    connect_packet.cpp

HEADERS  += widget.h \
    ikcp.h \
    alkcpclient.h \
    connect_packet.hpp

FORMS    += widget.ui
