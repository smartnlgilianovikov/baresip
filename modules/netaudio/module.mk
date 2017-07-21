#
# module.mk
#
# Copyright (C) 2010 - 2016 Creytiv.com
#

MOD		:= netaudio
$(MOD)_SRCS	+= netaudio.c
$(MOD)_SRCS	+= player.c
$(MOD)_SRCS	+= recorder.c

include mk/mod.mk
