var refreshInterval;

function toggleFeature(featureId, flag) {
    var classRef = document.getElementsByClassName(featureId);

    console.log("Feature: " + featureId + " - Flag: " + flag);

    for (i = 0; i < classRef.length; i++) {
        classRef[i].disabled = flag;
    }
}

function toggleSecurity(securitySwitch, featureId) {
    var securityFlag;

    console.log("Security: " + securitySwitch.value );

    if (securitySwitch.value == 0)
        securityFlag = true;
    else
        securityFlag = false;

    toggleFeature(featureId, securityFlag);
}

function toggleAutoIPSetup(setupSwitch, featureId) {
    var fieldObjs = document.getElementsByClassName("ipSetupField");
    console.log("Auto IP setup: " + setupSwitch.checked);

    for (i = 0; i < fieldObjs.length; i++)
        resetAPIPRange(fieldObjs[i].id);

    toggleFeature(featureId, setupSwitch.checked);
}

function resetAPIPRange(ipField) {
    var fieldObjs = document.getElementsByClassName("ipSetupField");

    for (i = 0; i < fieldObjs.length; i++) {
        if (fieldObjs[i].id == ipField) {
            fieldObjs[i].value = fieldObjs[i].defaultValue;
            break;
        }
    }
}

function checkForm() {
    if ( checkIPRange() == false )
        return false;

    refreshInterval = setInterval(refreshLogs, 1000);
}

var endOfStopAction;

function waitForTheEnd() {
    clearInterval(refreshInterval);
}

function stopAP() {
    endOfStopAction = setTimeout(waitForTheEnd, 5000);
}

function onLoad() {
    if (typeof refreshInterval !== 'undefined') {
        clearInterval(refreshInterval);
    }
}
