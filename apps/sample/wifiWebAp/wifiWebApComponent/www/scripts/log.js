function updateIPLog()
{
    var xmlhttp;
    var d=new Date();
    var t=d.toLocaleTimeString();

    if (window.XMLHttpRequest)
    {// code for IE7+, Firefox, Chrome, Opera, Safari
        xmlhttp=new XMLHttpRequest();
    }

    xmlhttp.onreadystatechange=function()
    {
        if (( xmlhttp.readyState == 4 ) && ( xmlhttp.status == 200 ))
        {
            var temp = xmlhttp.responseText;
            document.getElementById("ipLog").innerHTML = temp;
        }
    }
    xmlhttp.open("GET", "/cgi-bin/readlogfile?"+t, true);
    xmlhttp.send();
}

function refreshLogs()
{
    var logArea = document.getElementById('ipLog');
    updateIPLog();
    logArea.scrollTop = logArea.scrollHeight;
}

