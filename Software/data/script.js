// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}


function onOpen(event) {
    console.log('Connection opened');
    websocket.send("getValues");
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function update_slider(element) {
    var value = document.getElementById(element.id).value;
    document.getElementById(element.id + "_value").innerHTML = value;
    websocket.send(element.id + "?" + value.toString());
}

function update_radio(element) {
    var value = document.getElementById(element.id).value;
    console.log("update radio output");
    console.log(value);
    websocket.send(element.name + "?" + value.toString());
}

function update_button(element) {
    websocket.send(element.name);
}

function onMessage(event) 
{
    console.log(event.data);
    var values = JSON.parse(event.data);

    for (const key in values) 
	{
		if (key === "mode")
		{
            if(!SetValueToElementChecked(key+"_"+values[key], true))
                console.log("irgendwastollesmachenhier");
            
			continue;
		}
		if (key === "adc")
		{
            SetValueToElementChecked(key+"_"+values[key], true);
			continue;
		}
		
        SetValueToElementInnerHTML(key+"_value", values[key]);
        document.getElementById(key).value = values[key];
        // SetValueToElementValue(key+"_value", values[key]);  //buggy
    }
}

function SetValueToElementChecked(id, value)
{
    if(!DoesElementExistsById(id))
        return false;
    document.getElementById(id).checked = value;
    return true;
}

function SetValueToElementInnerHTML(id, value)
{
    if(!DoesElementExistsById(id))
        return false;
    document.getElementById(id).innerHTML = value;
    return true;
}

function SetValueToElementValue(id, value)
{
    if(!DoesElementExistsById(id))
        return false;
    document.getElementById(id).value = value;
    return true;
}

function DoesElementExistsById(id)
{
    if(document.getElementById(id) == null)
    {
        console.log("Coud not find Element Id: " + id);
        return false;
    }

    return true;
}