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
			document.getElementById(key+"_"+values[key]).checked = true;
			continue;
		}
		if (key === "adc")
		{
			document.getElementById(key+"_"+values[key]).checked = true;
			continue;
		}
		
        document.getElementById(key+"_value").innerHTML = values[key];
        document.getElementById(key).value = values[key];
        if (values["adc"] === "on")
        {
			console.log("adc is on");
            document.getElementById('adc_on').checked = true;
		}
		else
		{
			console.log("adc is off");
            document.getElementById('adc_off').checked = true;
		}
    }
}