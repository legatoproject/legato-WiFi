#!/bin/sh
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

case $2 in
  WIFI_START)
    echo "WIFI_START"
    # run tiwifi script
    /etc/init.d/tiwifi start || exit 91
    exit 0 ;;

  WIFI_STOP)
    echo "WIFI_STOP"
    # run tiwifi script
    /etc/init.d/tiwifi stop || exit 92
    exit 0 ;;

  WIFI_WLAN_UP)
    echo "WIFI_WLAN_UP"
    /sbin/ifconfig $1 up || exit 93
    exit 0 ;;

  WIFI_SET_EVENT)
    echo "WIFI_SET_EVENT"
    /usr/sbin/iw event || exit 94
    exit 0 ;;

  WIFIAP_HOSTAPD)
    echo "WIFIAP_HOSTAPD"
    /bin/hostapd /tmp/hostapd.conf || exit 95
    exit 0 ;;

  WIFICLIENT_START_SCAN)
    echo "WIFICLIENT_START_SCAN"
    /usr/sbin/iw dev wlan0 scan |grep 'SSID\\|signal' || exit 96
    exit 0 ;;

  WIFICLIENT_DISCONNECT)
    echo "WIFICLIENT_DISCONNECT"
    /sbin/ifconfig $1 down || exit 97
    exit 0 ;;

  WIFICLIENT_CONNECT_WPA_PASSPHRASE)
    echo "WIFICLIENT_CONNECT_WPA_PASSPHRASE"
    /sbin/wpa_passphrase "$3" $4 || exit 98
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_NONE)
    echo "WIFICLIENT_CONNECT_SECURITY_NONE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt NONE | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 8
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
    do
      echo "loop=$i"
      (/usr/sbin/iw $1 link | grep "Connected to") && break
      sleep 1
    done
    if [ "$i" = "20" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_WEP)
    echo "WIFICLIENT_CONNECT_SECURITY_WEP mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 wep_key0 \"$4\" | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt NONE | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 8
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 9
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
    do
        echo "loop=$i"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "$i" = "20" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt WPA-PSK | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 psk \"$4\" | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 proto WPA | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 set_network 0 pairwise TKIP | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 set_network 0 group TKIP | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK) || exit 8
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 9
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 10
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 11
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 12
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
    do
        echo "loop=$i"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "$i" = "20" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt WPA-PSK | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 psk \"$4\" | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 proto RSN | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 set_network 0 pairwise CCMP | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 set_network 0 group CCMP | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK) || exit 8
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 9
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 10
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 11
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 12
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
    do
        echo "loop=$i"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "$i" = "20" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt WPA-EAP | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 pairwise TKIP | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 group TKIP | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 set_network 0 proto WPA | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 set_network 0 eap PEAP | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 set_network 0 identity \"$4\" | grep OK) || exit 8
    (/sbin/wpa_cli -i$1 set_network 0 password \"$5\" | grep OK) || exit 9
    (/sbin/wpa_cli -i$1 set_network 0 phase1 \"peapver=0\" | grep OK) || exit 10
    (/sbin/wpa_cli -i$1 set_network 0 phase2 \"MSCHAPV2\" | grep OK) || exit 11
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK) || exit 12
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 13
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 14
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 15
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 16
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
    do
        echo "loop=$i"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "$i" = "30" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0 ;;

  WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE)
    echo "WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE mode"
    # Run wpa_supplicant daemon
    /sbin/wpa_supplicant -d -Dnl80211 -c /etc/wpa_supplicant.conf -i$1 -B || exit 99

    /sbin/wpa_cli -i$1 disconnect
    for i in `/sbin/wpa_cli -i$1 list_networks | grep ^[0-9] | cut -f1`; do
        /sbin/wpa_cli -i$1 remove_network $i
    done
    (/sbin/wpa_cli -i$1 add_network | grep 0) || exit 1
    (/sbin/wpa_cli -i$1 set_network 0 auth_alg OPEN | grep OK) || exit 2
    (/sbin/wpa_cli -i$1 set_network 0 key_mgmt WPA-EAP | grep OK) || exit 3
    (/sbin/wpa_cli -i$1 set_network 0 pairwise CCMP | grep OK) || exit 4
    (/sbin/wpa_cli -i$1 set_network 0 group CCMP | grep OK) || exit 5
    (/sbin/wpa_cli -i$1 set_network 0 proto WPA2 | grep OK) || exit 6
    (/sbin/wpa_cli -i$1 set_network 0 eap PEAP | grep OK) || exit 7
    (/sbin/wpa_cli -i$1 set_network 0 identity \"$4\" | grep OK) || exit 8
    (/sbin/wpa_cli -i$1 set_network 0 password \"$5\" | grep OK) || exit 9
    (/sbin/wpa_cli -i$1 set_network 0 phase1 \"peapver=0\" | grep OK) || exit 10
    (/sbin/wpa_cli -i$1 set_network 0 phase2 \"MSCHAPV2\" | grep OK) || exit 11
    (/sbin/wpa_cli -i$1 set_network 0 mode 0 | grep OK || exit 12
    (/sbin/wpa_cli -i$1 set_network 0 ssid \"$3\" | grep OK) || exit 13
    (/sbin/wpa_cli -i$1 select_network 0 | grep OK) || exit 14
    (/sbin/wpa_cli -i$1 enable_network 0 | grep OK) || exit 15
    (/sbin/wpa_cli -i$1 reassociate | grep OK) || exit 16
    /sbin/wpa_cli -i$1 status

    # Verify connection status
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
    do
        echo "loop=$i"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "$i" = "30" ]; then
        exit 30
    fi
    #PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin
    #/sbin/udhcpc -R -b -i $1 || exit 31
    exit 0
    # ;;

  *)
    echo "Parameter not valid"
    #exit 99 ;;
esac


