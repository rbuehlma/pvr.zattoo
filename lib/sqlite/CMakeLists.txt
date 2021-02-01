cmake_minimum_required(VERSION 3.5)
PROJECT(sqlite)

SET(SOURCES sqlite3.c)

ADD_DEFINITIONS(-D_GNU_SOURCE)

ADD_LIBRARY(sqlite STATIC ${SOURCES})
