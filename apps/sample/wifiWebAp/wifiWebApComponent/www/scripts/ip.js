var ipClassBounds = [[ getIPValue("10.0.0.0"), getIPValue("10.255.255.255") ],
                     [ getIPValue("172.16.0.0"), getIPValue("172.31.255.255") ],
                     [ getIPValue("192.168.0.0"), getIPValue("192.168.255.255") ]];

function getIPValue(ipAddr) {
    var ipParts = ipAddr.split(".");
    var ipOctet = 0;
    var ipValue = 0;

    if (ipParts.length == 4) {
        for(i = 0; i < 4; i++) {
            ipOctet = parseInt(ipParts[i], 10);
            if ((ipOctet >= 0) && (ipOctet <= 255)) {
                ipValue = (ipValue << 8) >>> 0;
                ipValue += ipOctet;
            } else {
                ipValue = 0;
                break;
            }
        }
    }
    return ipValue;
}

function checkIPValue(ipFieldId) {
    var ipObj = document.getElementById(ipFieldId);
    var ipValue = getIPValue(ipObj.value);

    if (ipValue > 0) {
        for (ipClass = 0; ipClass < 3; ipClass++) {
            if ((ipClassBound[ipClass][0] < ipValue) && (ipValue < ipClassBound[ipClass][1])) {
                return true;
            }
        }
    }
    alert("Invalid IP LAN address !");
    ipObj.value = "";
    ipObj.focus();
    ipObj.select();
    return false;
}

function checkIPMask(ipFieldId) {
    var ipObj = document.getElementById(ipFieldId);
    var ipValue = getIPValue(ipObj.value);
    var ipMask = 0x0;

    if (ipValue > 0) {
        for (i = 31; i > 0; i--) {
            ipMask += (1 << i) >>> 0;
            if (ipMask == ipValue)
                return true;
        }
    }

    alert("Invalid IP mask !");
    ipObj.value = "";
    ipObj.focus();
    ipObj.select();
    return false;
}

function checkIPRange() {
    var ipAP = document.getElementById("ipAP");
    var ipMask = document.getElementById("ipMask");
    var ipStart = document.getElementById("ipStart");
    var ipStop = document.getElementById("ipStop");
    var maxClients = document.getElementById("maxclients");

    var ipAPValue = getIPValue(ipAP.value);
    var ipMaskValue = getIPValue(ipMask.value);
    var ipStartValue = getIPValue(ipStart.value);
    var ipStopValue = getIPValue(ipStop.value);

    var ipNetworkAddr;
    var ipAPMaskedValue;
    var ipStartMaskedValue;
    var ipStopMaskedValue;
    var ipMaskInverted;

    var nbClients = parseInt(maxClients.value);

    if (ipStartValue == ipStopValue) {
        alert("IP start and stop addresses cannot be equal !");
        ipStop.focus();
        return false;
    }

    if (ipStartValue > ipStopValue) {
        ipStartValue = ipStartValue ^ ipStopValue;
        ipStopValue = ipStopValue ^ ipStartValue;
        ipStartValue = ipStartValue ^ ipStopValue;

        ipTmp = ipStart.value;
        ipStart.value = ipStop.value;
        ipStop.value = ipTmp;
    }

    ipNetworkAddr = ( ipAPValue & ipMaskValue ) >>> 0;
    ipAPMaskedValue = ( ipAPValue - ipNetworkAddr ) >>> 0;
    ipStartMaskedValue = ( ipStartValue - ipNetworkAddr ) >>> 0;
    ipStopMaskedValue = ( ipStopValue - ipNetworkAddr ) >>> 0;
    ipMaskInverted = ( 0xFFFFFFFF ^ ipMaskValue ) >>> 0;

    if (ipAPMaskedValue == 0) {
        alert("IP AP address is invalid !");
        ipAP.focus();
        return false;
    }

    if (ipAPMaskedValue >= ipMaskInverted) {
        alert("IP AP address and netmask does not fit !");
        ipAP.focus();
        return false;
    }

    if (ipStartMaskedValue == 0) {
        alert("IP start address is invalid !");
        ipStart.focus();
        return false;
    }

    if (ipStartMaskedValue >= ipMaskInverted) {
        alert("IP start address and netmask does not fit !");
        ipStart.focus();
        return false;
    }

    if (ipStopMaskedValue == 0) {
        alert("IP stop address is invalid !");
        ipStop.focus();
        return false;
    }

    if (ipStopMaskedValue > ipMaskInverted) {
        alert("IP stop address and netmask does not fit !");
        ipStop.focus();
        return false;
    }

    if ((ipAPValue >= ipStartValue) && (ipAPValue <= ipStopValue)) {
        alert("IP AP address cannot be included with the IP addresses range !");
        ipAP.focus();
        return false;
    }

    if (nbClients > ( ipStopValue - ipStartValue + 1 )) {
        alert("There are too many clients regarding the IP addresses range !");
        maxClients.focus();
        return false;
    }

    if (nbClients > ipMaskInverted - 1) {
        alert("There are too many clients regarding the netmask !");
        maxClients.focus();
        return false;
    }

    return true;
}

