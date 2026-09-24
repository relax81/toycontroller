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
    v2Reset();
    send({ t: "get" }); // full state, marks this client as a JSON client
}

function onClose(event) {
    console.log('Connection closed');
    v2Reset();
    setTimeout(initWebSocket, 2000);
}

function update_slider(element) {
    var value = document.getElementById(element.id).value;
    document.getElementById(element.id + "_value").innerHTML = value;
    var key = element.getAttribute("data-key");
    if (key)
        send({ t: "set", d: { [key]: Number(value) } });
    else
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
    var key = element.getAttribute("data-key");
    if (key)
        send({ t: "set", d: { [key]: value } });
    else
        websocket.send(element.id + "?" + value.toString());
}

function update_button(element) {
    var cmd = element.getAttribute("data-cmd");
    if (cmd)
        send({ t: "cmd", c: cmd });
    else
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

    if (values.t !== undefined) // JSON protocol: state / patch / ack / err
    {
        onV2Message(values);
        return;
    }

    for (const key in values) 
	{
		if (key === "mode") // unused placeholder
		{
            SetValueToElementChecked(key+"_"+values[key]);
            continue;
		}
		// if (key === "buzzer")
		// {
        //     SetValueToElementChecked(key+"_"+values[key], true);
		// 	continue;
		// }
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
        if (key === "toggle_f")
        {
            // SetValueToElementChecked(key+"_"+values[key], true);
            document.getElementById(key).checked = values[key];
            continue;
        }
        if (key === "toggle_g")
        {
            // SetValueToElementChecked(key+"_"+values[key], true);
            document.getElementById(key).checked = values[key];
            continue;
        }
    

        SetValueToElementInnerHTML(key+"_value", values[key]);
        SetValueToElementValue(key, values[key]); 
    }
}

// ---------------------------------------------------------------------------
// JSON protocol: {"t":"state"|"patch"|"ack"|"err", ...}, keys in dot notation.
// The elements are found by their data-key attribute (index.html).
// ---------------------------------------------------------------------------
var v2State = { hasState: false, n: 0, store: {} };
var v2Keys = null; // key -> [elements]
var v2MsgId = 0;

function send(obj)
{
    obj.id = ++v2MsgId;
    websocket.send(JSON.stringify(obj));
}

function v2Reset()
{
    v2State.hasState = false;
    v2State.n = 0;
}

function v2BuildKeyMap()
{
    v2Keys = {};
    document.querySelectorAll('[data-key]').forEach(function (el) {
        var k = el.getAttribute('data-key');
        (v2Keys[k] = v2Keys[k] || []).push(el);
    });
}

function v2Apply(d)
{
    if (v2Keys === null)
        v2BuildKeyMap();
    for (const key in d)
    {
        v2State.store[key] = d[key]; // keys without an element (ble.*, sys.*) are kept for later pages
        var els = v2Keys[key];
        if (!els)
            continue;
        els.forEach(function (el) {
            if (el === document.activeElement)
                return; // being operated right now, it gets the value afterwards
            if (el.type === "checkbox")
            {
                el.checked = d[key];
            }
            else
            {
                el.value = d[key];
                SetValueToElementInnerHTML(el.id + "_value", d[key]);
            }
        });
    }
}

function onV2Message(m)
{
    switch (m.t)
    {
        case "state":
            v2State.store = {};
            v2Apply(m.d);
            v2State.n = m.n;
            v2State.hasState = true;
            break;
        case "patch":
            if (!v2State.hasState || m.n <= v2State.n)
                return; // no state yet, or already known
            if (m.n !== v2State.n + 1) // a patch got lost: ask for the full state
            {
                v2Reset();
                send({ t: "get" });
                return;
            }
            v2State.n = m.n;
            v2Apply(m.d);
            break;
        case "ack":
            break;
        case "err":
            console.warn("server error", m);
            break;
    }
}
