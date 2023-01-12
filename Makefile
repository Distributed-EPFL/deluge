OBJ := obj/
LIB := lib/
BIN := bin/

CFLAGS  ?= -O2 -flto
LDFLAGS ?= -flto

MAJOR   := 1
MINOR   := 0
PATCH   := 0

V ?= 1


ifeq ($(V),0)
  Q := @
else
ifeq ($(V),1)
  Q := @
  define cmd-print
    @echo '$(1)'
  endef
else
ifeq ($(V),2)
  Q := @
  define cmd-print
    @echo '$(1)'
  endef
  define cmd-info
    @echo '$(1)'
  endef
endif
endif
endif


-include .config/Makefile
include Commands.mk


config-commands := clean $(OBJ)%.mk

ifeq ($(filter $(config-commands), $(MAKECMDGOALS)),)
  mode := build
else
  ifneq ($(filter-out $(config-commands), $(MAKECMDGOALS)),)
    mode := mixed
  else
    mode := config
  endif
endif


ifeq ($(mode),mixed)

  %:
	$(MAKE) $@

  .NOTPARALLEL:

else


c-sources  := $(wildcard deluge/*.c)
cl-sources := $(wildcard deluge/*.cl)
cl-headers := $(call DEPCL, $(cl-sources), .)

objects  := $(patsubst %, $(OBJ)%.o, $(c-sources)) \
            $(patsubst %, $(OBJ)%.bin, $(cl-sources) $(cl-headers))


all: $(LIB)libdeluge.a $(LIB)libdeluge.so


$(LIB)libdeluge.a: $(objects) | $(LIB)
	$(call cmd-ar, $@, $^)

$(LIB)libdeluge.so: $(LIB)libdeluge.so.$(MAJOR) | $(LIB)
	$(call cmd-ln, $@, $(notdir $<))

$(LIB)libdeluge.so.$(MAJOR): $(LIB)libdeluge.so.$(MAJOR).$(MINOR) | $(LIB)
	$(call cmd-ln, $@, $(notdir $<))

$(LIB)libdeluge.so.$(MAJOR).$(MINOR): \
    $(LIB)libdeluge.so.$(MAJOR).$(MINOR).$(PATCH) | $(LIB)
	$(call cmd-ln, $@, $(notdir $<))

$(LIB)libdeluge.so.$(MAJOR).$(MINOR).$(PATCH): $(objects) | $(LIB)
	$(call cmd-ld, $@, $^, libdeluge.so.$(MAJOR), OpenCL)


$(OBJ)deluge/%.c.o: deluge/%.c | $(OBJ)deluge
	$(call cmd-cc, $@, $<, include .)

$(OBJ)deluge/%.bin: deluge/% | $(OBJ)deluge
	$(call cmd-bin, $@, $<)


$(OBJ)deluge/%.c.mk: deluge/%.c | $(OBJ)deluge
	$(call cmd-depc, $@, $<, $(patsubst %, $(OBJ)%.o, $<), include .)

$(OBJ)deluge/%.cl.mk: deluge/%.cl | $(OBJ)deluge
	$(call cmd-depcl, $@, $<, $(patsubst %, $(OBJ)%.o, $<), .)

$(OBJ).deps.mk: $(patsubst %, $(OBJ)%.mk, $(c-sources) $(cl-sources)) | $(OBJ)
	$(call cmd-cat, $@, $^)


ifeq ($(mode),build)
  -include $(OBJ).deps.mk
endif


$(OBJ) $(LIB) $(BIN):
	$(call cmd-mkdir, $@)

$(OBJ)deluge: | $(OBJ)
	$(call cmd-mkdir, $@)


clean:
	$(call cmd-clean, $(OBJ) $(LIB) $(BIN))


.PHONY: all clean


endif
