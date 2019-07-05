#!/bin/sh
# Copyright (C) Sierra Wireless Inc.
#
# ($1:) -d Debug logs
# $1: Command (ex:  WIFI_START
#                   WIFICLIENT_CONNECT
# $2: wpa_supplicant.conf file directory

if [ "$1" = "-d" ]; then
    shift
    set -x
fi

CMD=$1
# WiFi interface
IFACE=wlan0
# If WLAN interface does not exist but driver is installed, means WiFi hardware is absent
HARDWAREABSENCE=50
# QCA wifi module name
QCAWIFIMOD=wlan
# If wpa_supplicant is running already
WPADUPLICATE=14
# WiFi driver is not installed
NODRIVER=100
TIMEOUT=8
SUCCESS=0
ERROR=127
# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Check the connection on the WiFi network interface.
# Exit with 0 if connected otherwise exit with 8 (time out)
CheckConnection()
{
    retries=10
    echo "Checking connection..."
    # Verify connection status
    for i in $(seq 1 ${retries})
    do
        echo "loop=${i}"
        (/usr/sbin/iw ${IFACE} link | grep "Connected to") && break
        sleep 1
    done
    if [ "${i}" -eq "${retries}" ]; then
        # Connection request time out.
        exit ${TIMEOUT}
    fi
    # Connected.
    exit ${SUCCESS}
}

echo "${CMD}"
case ${CMD} in
    WIFI_START)
        retries=10
        # Do clean up, even just after reboot
        /usr/bin/qca9377 wifi client stop > /dev/null 2>&1
        sleep 1
        # Run wifi start background
        /usr/bin/qca9377 wifi client init > /dev/null 2>&1 &
        for i in $(seq 1 ${retries})
        do
            echo "loop=${i}"
            sleep 1
            [ -e /sys/class/net/${IFACE} ] && break
        done
        if [ "${i}" -ne "${retries}" ]; then
            /sbin/ifconfig ${IFACE} up
            exit ${SUCCESS}
        fi
        moduleString=$(/sbin/lsmod | grep ${QCAWIFIMOD}) > /dev/null
        if [ -n "${moduleString}" ]; then
            ret=${HARDWAREABSENCE}
        else
            ret=${ERROR}
        fi
        # Do clean up
        /usr/bin/qca9377 wifi client stop > /dev/null 2>&1
        exit ${ret} ;;

  WIFI_STOP)
    # If wpa_supplicant is still running, terminate it
    (/bin/ps -ax | grep wpa_supplicant | grep ${IFACE} >/dev/null 2>&1) \
    && /sbin/wpa_cli -i${IFACE} terminate
    # Unmount the WiFi network interface
    /usr/bin/qca9377 wifi client stop > /dev/null 2>&1 || exit ${ERROR}
    ;;

  WIFI_SET_EVENT)
    /usr/sbin/iw event || exit ${ERROR}
    ;;

  WIFI_UNSET_EVENT)
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] && exit ${SUCCESS}
    for i in $(seq 1 "${count}")
    do
        pid=$(/usr/bin/pgrep -n iw)
        /bin/kill -9 "${pid}"
    done
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] || exit ${ERROR}
    ;;

  WIFI_CHECK_HWSTATUS)
    #Client request disconnection if interface in up
    /sbin/ifconfig | grep ${IFACE} > /dev/null 2>&1
    [ $? -eq 0 ] && exit ${SUCCESS}
    sleep 1
    #Check WiFi stop called or not
    /sbin/lsmod | grep ${QCAWIFIMOD} > /dev/null 2>&1
    #Driver stays, hardware removed
    [ $? -eq 0 ] && exit ${HARDWAREABSENCE}
    #WiFi stop called
    exit ${NODRIVER} ;;

  WIFIAP_HOSTAPD_START)
    (/bin/hostapd /tmp/hostapd.conf -i${IFACE} -B) && exit ${SUCCESS}
    # Fail to start hostapd, do cleanup
    /usr/bin/qca9377 wifi client stop
    exit ${ERROR} ;;

  WIFIAP_HOSTAPD_STOP)
    rm -f /tmp/dnsmasq.wlan.conf
    touch /tmp/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    killall hostapd
    sleep 1;
    pidof hostapd && (kill -9 "$(pidof hostapd)" || exit ${ERROR})
    pidof dnsmasq && (kill -9 "$(pidof dnsmasq)" || exit ${ERROR})
    /etc/init.d/dnsmasq start || exit ${ERROR}
    ;;

  WIFIAP_WLAN_UP)
    AP_IP=$2
    /sbin/ifconfig | grep ${IFACE} || exit ${ERROR}
    /sbin/ifconfig ${IFACE} "${AP_IP}" up || exit ${ERROR}
    ;;

  DNSMASQ_RESTART)
    echo "interface=${IFACE}" >> /tmp/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    pkill -9 dnsmasq
    /etc/init.d/dnsmasq start || exit ${ERROR}
    ;;

  WIFICLIENT_START_SCAN)
    (/usr/sbin/iw dev ${IFACE} scan | grep 'BSS\|SSID\|signal') || exit ${ERROR}
    ;;

  WIFICLIENT_CONNECT)
    WPA_CFG=$2
    [ -f "${WPA_CFG}" ] || exit ${ERROR}
    # wpa_supplicant is running, return duplicated request
    /bin/ps -A | grep wpa_supplicant && exit ${WPADUPLICATE}
    /sbin/wpa_supplicant -d -Dnl80211 -c "${WPA_CFG}" -i${IFACE} -B || exit ${ERROR}
    CheckConnection ;;

  WIFICLIENT_DISCONNECT)
    /sbin/wpa_cli -i${IFACE} terminate || exit ${ERROR}
    echo "WiFi client disconnected."
    ;;

  IPTABLE_DHCP_INSERT)
    /usr/sbin/iptables -I INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit ${ERROR}
    ;;

  IPTABLE_DHCP_DELETE)
    /usr/sbin/iptables -D INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit ${ERROR}
    ;;

  *)
    echo "Parameter not valid"
    exit ${ERROR} ;;
esac
exit ${SUCCESS}

