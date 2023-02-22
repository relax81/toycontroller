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

function update_switch(element){
    var value = document.getElementById(element.id).checked;
    websocket.send(element.id + "?" + value.toString());
}

function update_button(element) {
    websocket.send(element.id);
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

function onMessage(event) 
{
    console.log(event.data);
    var values = JSON.parse(event.data);

    for (const key in values) 
	{
		if (key === "mode") // unused placeholder
		{
            SetValueToElementChecked(key+"_"+values[key]);
            continue;
		}
		if (key === "buzzer")
		{
            SetValueToElementChecked(key+"_"+values[key], true);
			continue;
		}
        // if (key === "lb1")
		// {
        //     console.log("key lb1 triggered");
        //     SetValueToElementChecked(key+"_"+values[key], true);
		// 	continue;
		// }
        // if (key === "lb2")
		// {
        //     console.log("key lb2 triggered");
        //     SetValueToElementChecked(key+"_"+values[key], true);
		// 	continue;
		// }

        if (key === "toggle_a")
        {
            SetValueToElementChecked((key), values[key], false);
            // document.getElementById(key).checked = values[key];
            continue;
        }

        if (key === "toggle_b")
        {
            document.getElementById(key).checked = values[key];
            continue;
        }
        if (key === "toggle_c")
        {
            // SetValueToElementChecked(key+"_"+values[key], true);
            document.getElementById(key).checked = values[key];
            continue;
        }
        if (key === "toggle_d")
        {
            // SetValueToElementChecked(key+"_"+values[key], true);
            document.getElementById(key).checked = values[key];
            continue;
        }
        if (key === "toggle_e")
        {
            // SetValueToElementChecked(key+"_"+values[key], true);
            document.getElementById(key).checked = values[key];
            continue;
        }
    

        SetValueToElementInnerHTML(key+"_value", values[key]);
        SetValueToElementValue(key, values[key]); 
    }
}

