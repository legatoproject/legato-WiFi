# --------------------------------------------------------------------------------------------------
# Makefile used to build the mangOH Wifi service .
#
# Currenly only builds for wp85
#
#
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
# --------------------------------------------------------------------------------------------------


MANGOH_WIFI_ROOT = $(PWD)
export MANGOH_WIFI_ROOT

SERVICE_DIR = $(PWD)/service
SERVICE_EXE = $(SERVICE_DIR)/wifiService.wp85.update

CLIENT_SAMPLE_DIR = $(PWD)/apps/sample/wifiClientTest
CLIENT_SAMPLE_EXE = $(CLIENT_SAMPLE_DIR)/wifiClientTest.wp85.update

AP_SAMPLE_DIR = $(PWD)/apps/sample/wifiApTest
AP_SAMPLE_EXE = $(AP_SAMPLE_DIR)/wifiApTest.wp85.update


# By default, build for the wp85
.PHONY: default
#service clientsample apsample
default: service clientsample apsample
	@echo "*****************************"
	@echo Successfully built Wifi Service and Apps
	@echo To load Wifi Service type:
	@echo instapp $(SERVICE_EXE) YOUR_TARGETS_IP
	@echo "*****************************"
	@echo To load Wifi Client Sample App type:
	@echo instapp $(CLIENT_SAMPLE_EXE) YOUR_TARGETS_IP
	@echo "*****************************"
	@echo To load Wifi AP Sample App type:
	@echo instapp $(AP_SAMPLE_EXE) YOUR_TARGETS_IP


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

# Cleaning rule.
.PHONY: clean
clean:
	$(MAKE) -C $(SERVICE_DIR) clean
	$(MAKE) -C $(CLIENT_SAMPLE_DIR) clean
	$(MAKE) -C $(AP_SAMPLE_DIR) clean
