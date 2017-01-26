# --------------------------------------------------------------------------------------------------
# Makefile used to build the Wifi service.
#
# Currenly only builds for wp85
#
#
# Copyright (C) Sierra Wireless Inc.
# --------------------------------------------------------------------------------------------------

ifndef YOUR_TARGETS_IP
  YOUR_TARGETS_IP = "YOUR_TARGETS_IP"
  export YOUR_TARGETS_IP
endif

$(info ********************* VERSION ********************)
# Check wether the version is already defined.
ifeq ($(origin LEGATO_WIFI_VERSION), undefined)
  # If not, try to define it through Git information
  LEGATO_WIFI_VERSION := $(shell (git describe --tags || git rev-parse HEAD) 2> /dev/null)
  $(info Legato WiFi version is $(LEGATO_WIFI_VERSION))
  # If the folder is not managed by Git
ifeq ($(LEGATO_WIFI_VERSION),)
  # Define the version as "UNDEFINED"
  LEGATO_WIFI_VERSION := UNDEFINED
  $(warning Legato WiFi version is undefined...)
endif
  # And finally, export it.
  export LEGATO_WIFI_VERSION
else
  # Otherwise, display the current version
  $(info Legato WiFi version is already defined: $(LEGATO_WIFI_VERSION))
endif
$(info **************************************************)

SERVICE_DIR = $(PWD)/service
SERVICE_EXE = $(SERVICE_DIR)/wifiService.wp85.update

CLIENT_SAMPLE_DIR = $(PWD)/apps/sample/wifiClientTest
CLIENT_SAMPLE_EXE = $(CLIENT_SAMPLE_DIR)/wifiClientTest.wp85.update

AP_SAMPLE_DIR = $(PWD)/apps/sample/wifiApTest
AP_SAMPLE_EXE = $(AP_SAMPLE_DIR)/wifiApTest.wp85.update

AP_WEB_SAMPLE_DIR = $(PWD)/apps/sample/wifiWebAp
AP_WEB_SAMPLE_EXE = $(AP_WEB_SAMPLE_DIR)/wifiWebAp.wp85.update

# Wifi Command Line Client 'wifi'
WIFI_CMD_LINE_DIR = $(PWD)/apps/tools/wifi
WIFI_CMD_LINE_EXE = $(WIFI_CMD_LINE_DIR)/wifi.wp85.update



# By default, build for the wp85
.PHONY: default
#service clientsample apsample
default: service clientsample apsample wifi
	@echo "*****************************"
	@echo Successfully built Wifi Service and Apps
	@echo "*****************************"
	@echo Tip: You can set YOUR_TARGETS_IP with
	@echo export YOUR_TARGETS_IP=NNN.NNN.NNN.NNN
	@echo "*****************************"
	@echo To load Wifi Service type:
	@echo instapp $(SERVICE_EXE) $(YOUR_TARGETS_IP)
	@echo "*****************************"
	@echo To load Wifi Client Sample App type:
	@echo instapp $(CLIENT_SAMPLE_EXE) $(YOUR_TARGETS_IP)
	@echo "*****************************"
	@echo To load Wifi AP Sample App type:
	@echo instapp $(AP_SAMPLE_EXE) $(YOUR_TARGETS_IP)
	@echo "*****************************"
	@echo To load Wifi AP Web Sample App type:
	@echo instapp $(AP_WEB_SAMPLE_EXE) $(YOUR_TARGETS_IP)
	@echo "*****************************"
	@echo To load Wifi Command Line App \"wifi\" type:
	@echo instapp $(WIFI_CMD_LINE_EXE) $(YOUR_TARGETS_IP)


# By default, build for the wp85
wp85: default

# wifi service
service: $(SERVICE_EXE)

$(SERVICE_EXE) :
	$(MAKE) -C $(SERVICE_DIR) wp85

# Wifi Client Sample
clientsample: $(CLIENT_SAMPLE_EXE)

$(CLIENT_SAMPLE_EXE):
	$(MAKE) -C $(CLIENT_SAMPLE_DIR) wp85

# Wifi Access Point Sample

apsample: $(AP_SAMPLE_EXE)

$(AP_SAMPLE_EXE):
	$(MAKE) -C $(AP_SAMPLE_DIR) wp85

# Wifi Web Access Point Sample

apsample: $(AP_WEB_SAMPLE_EXE)

$(AP_WEB_SAMPLE_EXE):
	$(MAKE) -C $(AP_WEB_SAMPLE_DIR) wp85


# Wifi Command Line Client 'wifi'
wifi: $(WIFI_CMD_LINE_EXE)

$(WIFI_CMD_LINE_EXE):
	$(MAKE) -C $(WIFI_CMD_LINE_DIR) wp85

# Cleaning rule.
.PHONY: clean
clean:
	$(MAKE) -C $(SERVICE_DIR) clean
	$(MAKE) -C $(CLIENT_SAMPLE_DIR) clean
	$(MAKE) -C $(AP_SAMPLE_DIR) clean
	$(MAKE) -C $(WIFI_CMD_LINE_DIR) clean
	$(MAKE) -C $(AP_WEB_SAMPLE_DIR) clean
