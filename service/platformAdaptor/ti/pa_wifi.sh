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
# If WLAN interface exists but can not be brought up, means WiFi hardware is inserted,
# drivers are loaded successfully, but firmware failed to boot
FIRMWAREFAILURE=100
# If WLAN interface does not exist but driver is installed, means WiFi hardware is absent
HARDWAREABSENCE=50
# WiFi driver is not installed
NODRIVER=100
TIMEOUT=8
# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

#init script from rootfs manages the start/stop functions.
TI_WIFI_SH=/etc/init.d/tiwifi

#### TI specific functions ####
# Activated only if TI Wirelink kernel drivers are embedded in Legato
TI_WILINK_MODULE_PATH=/legato/systems/current/modules/wl18xx.ko
if [ -f "${TI_WILINK_MODULE_PATH}" ]; then
    TI_WIFI_SH=tiwifi_int
fi

# TI wireless wl18xx specific applications start or stop here
# TI WIFI IoT board is managed by SDIO/MMC bus. Some signals need to be set
# and managed before the SDIO/MMC module is inserted.
# TI WIFI IoT conflicts with others devices using the SDIO/MMC bus

# Interface does not exist
TI_WIFI_PA_NO_IF_ERR=50

# Interface could not be brought up
TI_WIFI_PA_NO_IF_UP_ERR=100

# Hardware settings function place holder; default to mangOH
BOARD_CONFIG_WLAN=mangOH_cfg_wlan

# Sysfs support for GPIO expander
GPIOEXP_SYSFS=

GPIO_EXPORT=/sys/class/gpio/v2/alias_export
GPIO_UNEXPORT=/sys/class/gpio/v2/alias_unexport
GPIO_DIR=/sys/class/gpio/v2/aliases_exported/
if [ ! -e ${GPIO_EXPORT} ]; then
    GPIO_EXPORT=/sys/class/gpio/export
    GPIO_UNEXPORT=/sys/class/gpio/unexport
    GPIO_DIR=/sys/class/gpio/gpio
fi

#For backward compatibility with LXSWI which does not support sysfs control of gpio expander
gpioexp_set()
{
    # Enable all GPIOs on all EXPANDERs
    gpioexp 1 1 enable >/dev/null || return 127

    ### mangOH green has 3 expanders
    # Set IOTO_RESET, GPIO#4/EXPANDER#3 - IOT0 Reset signal is disabled
    gpioexp 3 4 output normal high >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "mangOH red board"

        # Set IOT0_GPIO2 = 1 (WP GPIO13)
        [ -d ${GPIO_DIR}13 ] || echo 13 >${GPIO_EXPORT}
        echo out >${GPIO_DIR}13/direction
        echo 1 >${GPIO_DIR}13/value

        # Set IOT0_RESET = 1 (WP GPIO2)
        [ -d ${GPIO_DIR}2 ] || echo 2 >${GPIO_EXPORT}
        echo out >${GPIO_DIR}2/direction
        echo 1 >${GPIO_DIR}2/value

        # Clear SDIO_SEL, GPIO#9/EXPANDER#1 - Select the SDIO
        gpioexp 1 9 output normal low >/dev/null || return 127
    else
        echo "mangOH green board"

        # Set IOT0_GPIO2 = 1 (WP GPIO33)
        [ -d ${GPIO_DIR}33 ] || echo 33 >${GPIO_EXPORT}
        echo out >${GPIO_DIR}33/direction
        echo 1 >${GPIO_DIR}33/value

        # Clear SDIO_SEL, GPIO#13/EXPANDER#1 - Select the SDIO
        gpioexp 1 13 output normal low >/dev/null || return 127
    fi
}

#For backward compatibility with LXSWI which does not support sysfs control of gpio expander
gpioexp_unset()
{
        # Clear IOTO_RESET, GPIO#4/EXPANDER#3 - IOT0 Reset signal is enabled
        gpioexp 3 4 output normal low >/dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo "mangOH red board"
            # Set IOT0_RESET = 1 (WP GPIO2)
            echo 0 >${GPIO_DIR}2/value

            # Clear SDIO_SEL, GPIO#9/EXPANDER#1 - Deselect the SDIO
            gpioexp 1 9 output normal high >/dev/null || return 127

            # Reset IOT0_GPIO2 = 0 (WP GPIO13)
            echo 0 >${GPIO_DIR}13/value

            echo 13 >${GPIO_UNEXPORT}
            echo 2 >${GPIO_UNEXPORT}
        else
            echo "mangOH green board"
            # Set SDIO_SEL, GPIO#13/EXPANDER#1 - Deselect the SDIO
            gpioexp 1 13 output normal high >/dev/null || return 127

            # Reset IOT0_GPIO2 = 0 (WP GPIO33)
            echo 0 >${GPIO_DIR}33/value

            # Unexport the GPIO
            echo 33 >${GPIO_UNEXPORT}
       fi
}

# Check if sysfs support exists for gpio expander
is_sysfs_gpioexp()
{
    if [ -z "$GPIOEXP_SYSFS" ]; then
        #check for the support; is SDIO_SEL pin of expander present in sysfs?
        grep SDIO_SEL /sys/class/gpio/v2/alias_map >/dev/null 2>&1
    fi
    [ $? -eq 0 ] && GPIOEXP_SYSFS=0 || GPIOEXP_SYSFS=1

    return $GPIOEXP_SYSFS #0 for valid; 1 otherwise.
}

mangOH_cfg_wlan()
{
    case $1 in
        SET_UP)
            echo "Setting up GPIOs for wlan"
            # GPIO expander settings
            if is_sysfs_gpioexp ; then
                ### mangOH green has 3 expanders
                # Set IOTO_RESET, GPIO#4/EXPANDER#3 - IOT0 Reset signal is disabled
                grep GPIO_IOT0_RESET /sys/class/gpio/v2/alias_map >/dev/null 2>&1
                if [ $? -eq 0 ]; then
                    echo "mangOH green board"

                    [ -d ${GPIO_DIR}GPIO_IOT0_RESET ] || echo GPIO_IOT0_RESET >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}GPIO_IOT0_RESET/direction
                    echo 1 >${GPIO_DIR}GPIO_IOT0_RESET/value

                    # Set IOT0_GPIO2 = 1 (WP GPIO33)
                    [ -d ${GPIO_DIR}33 ] || echo 33 >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}33/direction
                    echo 1 >${GPIO_DIR}33/value

                    # Clear SDIO_SEL, GPIO#13/EXPANDER#1 - Select the SDIO
                    [ -d ${GPIO_DIR}SDIO_SEL ] || echo SDIO_SEL >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}SDIO_SEL/direction
                    echo 0 >${GPIO_DIR}SDIO_SEL/value
                else
                    echo "mangOH red board"

                    # Set IOT0_GPIO2 = 1 (WP GPIO13)
                    [ -d ${GPIO_DIR}13 ] || echo 13 >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}13/direction
                    echo 1 >${GPIO_DIR}13/value

                    # Set IOT0_RESET = 1 (WP GPIO2)
                    [ -d ${GPIO_DIR}2 ] || echo 2 >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}2/direction
                    echo 1 >${GPIO_DIR}2/value

                    # Clear SDIO_SEL, GPIO#9/EXPANDER#1 - Select the SDIO
                    [ -d ${GPIO_DIR}SDIO_SEL ] || echo SDIO_SEL >${GPIO_EXPORT}
                    echo out >${GPIO_DIR}SDIO_SEL/direction
                    echo 0 >${GPIO_DIR}SDIO_SEL/value
                fi
            else
                gpioexp_set
            fi # is_sysfs_gpioexp
          ;;
        REVERT)
            echo "Clearing up GPIOs setting made for wlan"
            # Revert GPIO settings in expanders
            if is_sysfs_gpioexp ; then
                # Clear IOTO_RESET, GPIO#4/EXPANDER#3 - IOT0 Reset signal is enabled
                grep GPIO_IOT0_RESET /sys/class/gpio/v2/alias_map >/dev/null 2>&1
                if [ $? -eq 0 ]; then
                   echo "mangOH green board"

                    echo 0 >${GPIO_DIR}GPIO_IOT0_RESET/value
                    # Set SDIO_SEL, GPIO#13/EXPANDER#1 - Deselect the SDIO
                    echo 1 >${GPIO_DIR}SDIO_SEL/value

                    # Reset IOT0_GPIO2 = 0 (WP GPIO33)
                    echo 0 >${GPIO_DIR}33/value

                    # Unexport the GPIO
                    echo 33 >${GPIO_UNEXPORT}
                    echo SDIO_SEL >${GPIO_UNEXPORT}
                    echo GPIO_IOT0_RESET >${GPIO_UNEXPORT}
                else
                    echo "mangOH red board"
                    # Reset IOT0_RESET = 0 (WP GPIO2)
                    echo 0 >${GPIO_DIR}2/value

                    # Set SDIO_SEL, GPIO#9/EXPANDER#1 - Deselect the SDIO
                    echo 1 >${GPIO_DIR}SDIO_SEL/value

                    # Reset IOT0_GPIO2 = 0 (WP GPIO13)
                    echo 0 >${GPIO_DIR}13/value

                    echo 13 >${GPIO_UNEXPORT}
                    echo 2 >${GPIO_UNEXPORT}
                    echo SDIO_SEL >${GPIO_UNEXPORT}
               fi
            else
                gpioexp_unset
            fi # is_sysfs_gpioexp
          ;;
    esac
}

ti_wifi_start()
{
    echo "Starting TI Wifi module"

    # Add mdev rule for crda
    grep crda /etc/mdev.conf > /dev/null
    if [ $? -ne 0 ]; then
        (mount | grep -q " on /etc type ") || \
            (cp /etc/mdev.conf /tmp; mount -o bind /tmp/mdev.conf /etc/mdev.conf)
        echo "\$COUNTRY=.. root:root 0660 */sbin/crda" >> /etc/mdev.conf
    fi

    lsmod | grep wlcore >/dev/null
    if [ $? -ne 0 ]; then
        #Do the necessary hardware settings for wlan
        [ -z "$BOARD_CONFIG_WLAN" ] || ${BOARD_CONFIG_WLAN} SET_UP

        #Load kernel modules
        kmod load wlcore.ko || return 127
        kmod load wlcore_sdio.ko || return 127
        kmod load wl18xx.ko || return 127
    fi
    attempt=6
    for i in $(seq 1 ${attempt})
    do
        if [ $i -ne 1 ]; then
            sleep 1
        fi
        (ifconfig -a | grep wlan0 > /dev/null) && break
    done
    if [ $? -ne 0 ]; then
        echo "Failed to start TI wifi, interface does not exist."
        return ${TI_WIFI_PA_NO_IF_ERR}
    fi
    ifconfig wlan0 up > /dev/null
    if [ $? -ne 0 ] ; then
        echo "Failed to start TI wifi, interface can not be brought up."
        return ${TI_WIFI_PA_NO_IF_UP_ERR}
    fi

    echo "TI Wifi module started"
    return 0
}

ti_wifi_stop()
{
    echo "Stopping TI Wifi module"

    ifconfig | grep wlan0 >/dev/null
    if [ $? -eq 0 ]; then
        ifconfig wlan0 down
    fi
    lsmod | grep wlcore >/dev/null

    # If module unloading fails, it may be for different reasons: someone is
    # using it, cannot be unloaded because it crashed, etc. Since there is
    # nothing we can do about it, we should just continue on.
    if [ $? -eq 0 ]; then
        kmod unload wl18xx.ko >/dev/null 2>&1
        kmod unload wlcore_sdio.ko >/dev/null 2>&1
        kmod unload wlcore.ko >/dev/null 2>&1

        #Do the necessary hardware settings for wlan
        [ -z "$BOARD_CONFIG_WLAN" ] || ${BOARD_CONFIG_WLAN} REVERT
    fi

    echo "TI Wifi module stopped"
    return 0
}

tiwifi_int()
{
    case "$1" in
        start)
            ti_wifi_start
            ;;
        stop)
            ti_wifi_stop
            ;;
        *)
            return 1
            ;;
    esac
}

################################

# Check the connection on the WiFi network interface.
# Exit with 0 if connected otherwise exit with 8 (time out)
CheckConnection()
{
    local retries=10
    echo "Checking connection..."
    # Verify connection status
    for i in $(seq 1 ${retries})
    do
        echo "loop=${i}"
        (/usr/sbin/iw $1 link | grep "Connected to") && break
        sleep 1
    done
    if [ "${i}" -eq "${retries}" ]; then
        # Connection request time out.
        exit ${TIMEOUT}
    fi
    # Connected.
    exit 0
}

WiFiReset()
{
    local retries=3
    echo "WiFi reset"
    for i in $(seq 1 ${retries})
    do
        sleep 1
        ${TI_WIFI_SH} stop
        sleep 1
        ${TI_WIFI_SH} start && exit 0
    done
    exit 127
}

case ${CMD} in
  WIFI_START)
    echo "WIFI_START"
    # Mount the WiFi network interface
    ${TI_WIFI_SH} start && exit 0
    # Store failure reason
    FAILUREREASON=$?
    # If ti wifi start indicates firmware fails to boot, do reset
    if [ ${FAILUREREASON} -eq ${FIRMWAREFAILURE} ]; then
        WiFiReset && exit 0
        echo "Reset fail, perform clean up"
        ${TI_WIFI_SH} stop
        exit ${FAILUREREASON}
    fi
    # Hardware is absent, do clean up
    if [ ${FAILUREREASON} -eq ${HARDWAREABSENCE} ]; then
        echo "Hardware is absent, perform clean up"
        ${TI_WIFI_SH} stop
        exit ${FAILUREREASON}
    fi
    echo "Clean up due to unknown error"
    ${TI_WIFI_SH} stop
    exit ${FAILUREREASON} ;;

  WIFI_STOP)
    echo "WIFI_STOP"
    # If wpa_supplicant is still running, terminate it
    (/bin/ps -ax | grep wpa_supplicant | grep ${IFACE} >/dev/null 2>&1) \
    && /sbin/wpa_cli -i${IFACE} terminate
    # Unmount the WiFi network interface
    ${TI_WIFI_SH} stop || exit 127
    exit 0 ;;

  WIFI_SET_EVENT)
    echo "WIFI_SET_EVENT"
    /usr/sbin/iw event || exit 127
    exit 0 ;;

  WIFI_UNSET_EVENT)
    echo "WIFI_UNSET_EVENT"
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] && exit 0
    for i in $(seq 1 ${count})
    do
        pid=$(/usr/bin/pgrep -n iw)
        /bin/kill -9 ${pid}
    done
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] || exit 127
    exit 0 ;;

  WIFI_CHECK_HWSTATUS)
    echo "WIFI_CHECK_HWSTATUS"
    #Client request disconnection if interface in up
    /sbin/ifconfig | grep ${IFACE} >/dev/null
    [ $? -eq 0 ] && exit 0
    sleep 1
    #Check WiFi stop called or not
    /sbin/lsmod | grep wlcore >/dev/null
    #Driver stays, hardware removed
    [ $? -eq 0 ] && exit ${HARDWAREABSENCE}
    #WiFi stop called
    exit ${NODRIVER} ;;

  WIFIAP_HOSTAPD_START)
    echo "WIFIAP_HOSTAPD_START"
    (/bin/hostapd /tmp/hostapd.conf -i${IFACE} -B) && exit 0
    # Fail to start hostapd, do cleanup
    ${TI_WIFI_SH} stop
    exit 127 ;;

  WIFIAP_HOSTAPD_STOP)
    echo "WIFIAP_HOSTAPD_STOP"
    rm -f /tmp/dnsmasq.wlan.conf
    /usr/bin/unlink /etc/dnsmasq.d/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    killall hostapd
    sleep 1;
    pidof hostapd && (kill -9 `pidof hostapd` || exit 127)
    pidof dnsmasq && (kill -9 `pidof dnsmasq` || exit 127)
    /etc/init.d/dnsmasq start || exit 127
    exit 0 ;;

  WIFIAP_WLAN_UP)
    echo "WIFIAP_WLAN_UP"
    AP_IP=$2
    /sbin/ifconfig | grep ${IFACE} || exit 127
    /sbin/ifconfig ${IFACE} ${AP_IP} up || exit 127
    exit 0 ;;

  DNSMASQ_RESTART)
    echo "interface=${IFACE}" >> /tmp/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    pidof dnsmasq && (kill -9 `pidof dnsmasq` || exit 127)
    /etc/init.d/dnsmasq start || exit 127
    exit 0 ;;

  WIFICLIENT_START_SCAN)
    echo "WIFICLIENT_START_SCAN"
    (/usr/sbin/iw dev ${IFACE} scan | grep 'BSS\|SSID\|signal') || exit 127
    exit 0 ;;

  WIFICLIENT_CONNECT)
    echo "WIFICLIENT_CONNECT"
    WPA_CFG=$2
    [ -f ${WPA_CFG} ] || exit 127
    # wpa_supplicant is running, return duplicated request
    /bin/ps -A | grep wpa_supplicant && exit 14
    /sbin/wpa_supplicant -d -Dnl80211 -c ${WPA_CFG} -i${IFACE} -B || exit 127
    CheckConnection ${IFACE} ;;

  WIFICLIENT_DISCONNECT)
    echo "WIFICLIENT_DISCONNECT"
    /sbin/wpa_cli -i${IFACE} terminate || exit 127
    echo "WiFi client disconnected."
    exit 0 ;;

  IPTABLE_DHCP_INSERT)
    echo "IPTABLE_DHCP_INSERT"
    /usr/sbin/iptables -I INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit 127
    exit 0 ;;

  IPTABLE_DHCP_DELETE)
    echo "IPTABLE_DHCP_DELETE"
    /usr/sbin/iptables -D INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit 127
    exit 0 ;;

  *)
    echo "Parameter not valid"
    exit 127 ;;
esac

