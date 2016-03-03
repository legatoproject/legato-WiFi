#
# Rules to generate C header files containing Doxygen markup for documentation purposes only.
#
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#
#

message(STATUS "MSG:MangOH  Inside  $ENV{MANGOH_WIFI_ROOT}/CMakeLists.txt")
message(STATUS "MSG:MangOH:Wifi: CMakeLists.txt called")
generate_header_extern("$ENV{MANGOH_WIFI_ROOT}/le_wifiClient.api")
generate_header_extern("$ENV{MANGOH_WIFI_ROOT}/le_wifiAp.api")

