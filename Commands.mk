cflags ?= -Wall -Wextra
ldopt  := -Wl,-soname,

define cmd-ar
  $(call cmd-print,  AR      $(strip $(1)))
  $(Q)ar -crs $(1) $(2)
endef

define cmd-bin
  $(call cmd-print,  BIN     $(strip $(1)))
  $(Q)ld -r -b binary $(2) -o $(1)
endef

define cmd-cat
  $(call cmd-info,  CAT     $(strip $(1)))
  $(Q)cat $(2) > $(1)
endef

define cmd-cc
  $(call cmd-print,  CC      $(strip $(1)))
  $(Q)gcc $(cflags) $(CFLAGS) $(addprefix -I, $(3)) -fPIC -c $(2) -o $(1)
endef

define cmd-clean
  $(call cmd-print,  CLEAN)
  $(Q)rm -rf $(1) 2> '/dev/null' || true
endef

define cmd-ld
  $(call cmd-print,  LD      $(strip $(1)))
  $(Q)gcc $(LDFLAGS) -shared $(addprefix $(ldopt), $(3)) $(2) -o $(1) \
      -Wl,--as-needed $(addprefix -l, $(4))
endef

define cmd-ln
  $(call cmd-print,  LN      $(strip $(1)))
  $(Q)rm $(1) 2> '/dev/null' ; ln -s $(2) $(1)
endef

define cmd-depc
  $(call cmd-info,  DEPC    $(strip $(1)))
  $(Q)gcc -MM $(2) -MT $(3) -MF $(1) $(addprefix -I, $(4))
endef

define cmd-depcl
  $(call cmd-info,  DEPCL   $(strip $(1)))
  $(Q)gcc -x c -D__OPENCL_VERSION__ -MM $(2) -MT $(3) -MF $(1) \
      $(addprefix -I, $(4))
endef

define DEPCL
  $(sort $(shell gcc -x c -MM $(1) $(addprefix -I, $(2)) -D__OPENCL_VERSION__ \
                 | sed -r 's/^.*\.cl //' \
                 | tr -d '\\' \
                 | tr '\n' ' '))
endef

define cmd-mkdir
  $(call cmd-info,  MKDIR   $(strip $(1)))
  $(Q)mkdir $(1)
endef
