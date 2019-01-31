NAME := smartlight

$(NAME)_SOURCES := smartlight.c

$(NAME)_COMPONENTS += netmgr yloop cli feature.linkkit

GLOBAL_DEFINES += CONFIG_AOS_CLI USE_LPTHREAD
GLOBAL_INCLUDES += ./