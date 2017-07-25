TARGETS := $(MAKECMDGOALS)

export LEGATO_WIFI_ROOT ?= $(PWD)/..
export LEGATO_WIFI_PA ?= ti

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	mkapp -v -t $@ \
		-i $(PWD)/platformAdaptor/inc/ \
		wifiService.adef

clean:
	rm -rf _build_* *.update

