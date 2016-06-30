#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#
# ($1:) -d Debug logs
# $1: wlan interface (ex: wlan0)
# $2: Command (ex:  WIFI_START
#           WIFI_STOP
#           WIFICLIENT_CONNECT_SECURITY_NONE
#           WIFICLIENT_CONNECT_SECURITY_WEP
#           WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL
#           WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL
#           WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE
#           WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE
# $3: SSID
# $4: WEP key or WPAx_EAP_PEAP0_ENTERPRISE identity
# $5: WPAx_EAP_PEAP0_ENTERPRISE password

if [ "$1" = "-d" ]; then
    shift
    set -x
fi

# WiFi interface
IFACE=$1

# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Check the connection on the WiFi network interface.
# Exit with 0 if connected otherwise exit with 30 (number of retries)
CheckConnection()
{
    retries=30
    echo "Checking connection..."
    # Verify connection status
    for i in $(seq 1 ${retries})
    do
      echo "loop=${i}"
      (/usr/sbin/iw $1 link | grep "Connected to") && break
      sleep 1
    done
    if [ "${i}" -eq "${retries}" ]; then
        exit ${retries}
    fi
    exit 0
}

case $2 in
  WIFI_START)
    echo "WIFI_START"
    # Mount the WiFi network interface
    /sbin/ifup ${IFACE} || exit 91
    exit 0 ;;

  WIFI_STOP)
    echo "WIFI_STOP"
    # Unmount the WiFi network interface
    /sbin/ifdown ${IFACE} || exit 92
    exit 0 ;;

  WIFI_WLAN_UP)
    echo "WIFI_WLAN_UP"
    exit 0 ;;

  WIFI_WLAN_DOWN)
    echo "WIFI_WLAN_DOWN"
    exit 0 ;;

  WIFI_SET_EVENT)
    echo "WIFI_SET_EVENT"
    /usr/sbin/iw event || exit 94
    exit 0 ;;

  WIFIAP_HOSTAPD_START)
    echo "WIFIAP_HOSTAPD_START"
    (/bin/hostapd /tmp/hostapd.conf &)|| exit 95
    exit 0 ;;

  WIFIAP_HOSTAPD_STOP)
    echo "WIFIAP_HOSTAPD_STOP"
    rm -f /tmp/dnsmasq.wlan.conf; touch /tmp/dnsmasq.wlan.conf
    (/etc/init.d/dnsmasq stop; /etc/init.d/dnsmasq start) || exit 100
    (killall hostapd) || exit 100
    exit 0 ;;

  WIFICLIENT_START_SCAN)
    echo "WIFICLIENT_START_SCAN"
    (/usr/sbin/iw dev wlan0 scan | grep 'SSID\|signal') || exit 96
    exit 0 ;;

  WIFICLIENT_DISCONNECT)
    echo "WIFICLIENT_DISCONNECT"
    /sbin/wpa_cli -i${IFACE} terminate || exit 97
    exit 0 ;;

  WIFICLIENT_CONNECT_WPA_PASSPHRASE)
    echo "WIFICLIENT_CONNECT_WPA_PASSPHRASE"
    /sbin/wpa_passphrase "$3" $4 || exit 98
    echo "ctrl_interface=DIR=/var/run/wpa_supplicant" | tee -a /tmp/wpa_supplicant.conf
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_NONE)
    echo "WIFICLIENT_CONNECT_SECURITY_NONE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i${IFACE} -B || exit 99

    /sbin/wpa_cli -i${IFACE} disconnect
    for i in `/sbin/wpa_cli -i${IFACE} list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i${IFACE} remove_network $i
    done
    (/sbin/wpa_cli -i${IFACE} add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i${IFACE} set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i${IFACE} set_network 0 key_mgmt NONE | grep OK) || exit 3
    (/sbin/wpa_cli -i${IFACE} set_network 0 mode 0 | grep OK) || exit 4
    (/sbin/wpa_cli -i${IFACE} set_network 0 ssid \"$3\" | grep OK) || exit 5
    (/sbin/wpa_cli -i${IFACE} select_network 0 | grep OK) || exit 6
    (/sbin/wpa_cli -i${IFACE} enable_network 0 | grep OK) || exit 7
    (/sbin/wpa_cli -i${IFACE} reassociate | grep OK) || exit 8
    /sbin/wpa_cli -i${IFACE} status

    CheckConnection ${IFACE} ;;

  WIFICLIENT_CONNECT_SECURITY_WEP)
    echo "WIFICLIENT_CONNECT_SECURITY_WEP mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i${IFACE} -B || exit 99

    /sbin/wpa_cli -i${IFACE} disconnect
    for i in `/sbin/wpa_cli -i${IFACE} list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i${IFACE} remove_network $i
    done
    (/sbin/wpa_cli -i${IFACE} add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i${IFACE} set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i${IFACE} set_network 0 wep_key0 \"$4\" | grep OK) || exit 3
    (/sbin/wpa_cli -i${IFACE} set_network 0 key_mgmt NONE | grep OK) || exit 4
    (/sbin/wpa_cli -i${IFACE} set_network 0 mode 0 | grep OK) || exit 5
    (/sbin/wpa_cli -i${IFACE} set_network 0 ssid \"$3\" | grep OK) || exit 6
    (/sbin/wpa_cli -i${IFACE} select_network 0 | grep OK) || exit 7
    (/sbin/wpa_cli -i${IFACE} enable_network 0 | grep OK) || exit 8
    (/sbin/wpa_cli -i${IFACE} reassociate | grep OK) || exit 9
    /sbin/wpa_cli -i${IFACE} status

    CheckConnection ${IFACE} ;;

  WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL mode"
    (/sbin/wpa_passphrase $3 $5 > /tmp/wpa_supplicant.conf) || exit 1

    # Run wpa_supplicant daemon
    (/sbin/wpa_supplicant -d -Dnl80211 -c /tmp/wpa_supplicant.conf -i${IFACE} -B) || exit 99

    CheckConnection ${IFACE} ;;

  WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL mode"
    (/sbin/wpa_passphrase $3 $5 > /tmp/wpa_supplicant.conf) || exit 1

    # Run wpa_supplicant daemon
    (/sbin/wpa_supplicant -d -Dnl80211 -c /tmp/wpa_supplicant.conf -i${IFACE} -B) || exit 99

    /sbin/wpa_cli -i${IFACE} status

    CheckConnection ${IFACE} ;;

  WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i${IFACE} -B || exit 99

    /sbin/wpa_cli -i${IFACE} disconnect
    for i in `/sbin/wpa_cli -i${IFACE} list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i${IFACE} remove_network $i
    done
    (/sbin/wpa_cli -i${IFACE} add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i${IFACE} set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i${IFACE} set_network 0 key_mgmt WPA-EAP | grep OK) || exit 3
    (/sbin/wpa_cli -i${IFACE} set_network 0 pairwise TKIP | grep OK) || exit 4
    (/sbin/wpa_cli -i${IFACE} set_network 0 group TKIP | grep OK) || exit 5
    (/sbin/wpa_cli -i${IFACE} set_network 0 proto WPA | grep OK) || exit 6
    (/sbin/wpa_cli -i${IFACE} set_network 0 eap PEAP | grep OK) || exit 7
    (/sbin/wpa_cli -i${IFACE} set_network 0 identity \"$4\" | grep OK) || exit 8
    (/sbin/wpa_cli -i${IFACE} set_network 0 password \"$5\" | grep OK) || exit 9
    (/sbin/wpa_cli -i${IFACE} set_network 0 phase1 \"peapver=0\" | grep OK) || exit 10
    (/sbin/wpa_cli -i${IFACE} set_network 0 phase2 \"MSCHAPV2\" | grep OK) || exit 11
    (/sbin/wpa_cli -i${IFACE} set_network 0 mode 0 | grep OK) || exit 12
    (/sbin/wpa_cli -i${IFACE} set_network 0 ssid \"$3\" | grep OK) || exit 13
    (/sbin/wpa_cli -i${IFACE} select_network 0 | grep OK) || exit 14
    (/sbin/wpa_cli -i${IFACE} enable_network 0 | grep OK) || exit 15
    (/sbin/wpa_cli -i${IFACE} reassociate | grep OK) || exit 16
    /sbin/wpa_cli -i${IFACE} status

    CheckConnection ${IFACE} ;;

  WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i${IFACE} -B || exit 99

    /sbin/wpa_cli -i${IFACE} disconnect
    for i in `/sbin/wpa_cli -i${IFACE} list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i${IFACE} remove_network $i
    done
    (/sbin/wpa_cli -i${IFACE} add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i${IFACE} set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i${IFACE} set_network 0 key_mgmt WPA-EAP | grep OK) || exit 3
    (/sbin/wpa_cli -i${IFACE} set_network 0 pairwise CCMP | grep OK) || exit 4
    (/sbin/wpa_cli -i${IFACE} set_network 0 group CCMP | grep OK) || exit 5
    (/sbin/wpa_cli -i${IFACE} set_network 0 proto WPA2 | grep OK) || exit 6
    (/sbin/wpa_cli -i${IFACE} set_network 0 eap PEAP | grep OK) || exit 7
    (/sbin/wpa_cli -i${IFACE} set_network 0 identity \"$4\" | grep OK) || exit 8
    (/sbin/wpa_cli -i${IFACE} set_network 0 password \"$5\" | grep OK) || exit 9
    (/sbin/wpa_cli -i${IFACE} set_network 0 phase1 \"peapver=0\" | grep OK) || exit 10
    (/sbin/wpa_cli -i${IFACE} set_network 0 phase2 \"MSCHAPV2\" | grep OK) || exit 11
    (/sbin/wpa_cli -i${IFACE} set_network 0 mode 0 | grep OK) || exit 12
    (/sbin/wpa_cli -i${IFACE} set_network 0 ssid \"$3\" | grep OK) || exit 13
    (/sbin/wpa_cli -i${IFACE} select_network 0 | grep OK) || exit 14
    (/sbin/wpa_cli -i${IFACE} enable_network 0 | grep OK) || exit 15
    (/sbin/wpa_cli -i${IFACE} reassociate | grep OK) || exit 16
    /sbin/wpa_cli -i${IFACE} status

    CheckConnection ${IFACE} ;;

  *)
    echo "Parameter not valid"
    exit 99 ;;
esac
