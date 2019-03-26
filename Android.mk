LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        src/mavlink-router/autolog.cpp \
        src/mavlink-router/endpoint.cpp \
        src/mavlink-router/main.cpp \
        src/mavlink-router/pollable.cpp \
        src/mavlink-router/ulog.cpp \
        src/mavlink-router/binlog.cpp \
        src/mavlink-router/logendpoint.cpp \
        src/mavlink-router/mainloop.cpp \
        src/mavlink-router/timeout.cpp \
        src/mavlink-router/controller.cpp \
        src/common/conf_file.cpp \
        src/common/log.cpp \
        src/common/util.c \
        src/common/xtermios.cpp \

LOCAL_SHARED_LIBRARIES := \
        liblog \

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/src \
        $(LOCAL_PATH)/../common/include/mavlink \
        $(LOCAL_PATH)/../common/include/mavlink/ardupilotmega \

LOCAL_MODULE:= mavlink-router

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
