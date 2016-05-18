ifndef LEGATO_WIFI_ROOT
LEGATO_WIFI_ROOT = $(PWD)/../../..
export LEGATO_WIFI_ROOT
endif

localhost: app.localhost

ar7: app.ar7

wp7: app.wp7

ar86: app.ar86

wp85: app.wp85

app.%:
	mkapp wifiClientTest.adef -t $* -v -i $(LEGATO_WIFI_ROOT)

clean:
	rm -rf _build_* *.localhost *.ar7 *.wp7 *.ar8 *.wp85 *.update
