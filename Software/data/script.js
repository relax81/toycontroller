// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onload);

function onload(event) {
    btCardRestoreOpen();
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
    var label = document.getElementById(element.id + "_value");
    if (label)
        label.innerHTML = value;
    var key = element.getAttribute("data-key");
    if (key)
    {
        send({ t: "set", d: { [key]: Number(value) } });
        v2Verify(element);
    }
    else
        websocket.send(element.id + "?" + value.toString());
}

function update_select(element) {
    var key = element.getAttribute("data-key");
    if (key)
    {
        send({ t: "set", d: { [key]: Number(element.value) } });
        v2Verify(element);
    }
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
var v2Keys = null;  // key -> [elements] (data-key)
var v2Holds = null; // key -> [elements] (data-hold): they get the class "ble-held" while BLE holds the output
var v2MsgId = 0;
var v2Editing = null;       // element being dragged / typed in right now: incoming values wait for it
var v2Deferred = new Map(); // element -> value that came in while it was edited

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
    v2Holds = {};
    document.querySelectorAll('[data-key]').forEach(function (el) {
        var k = el.getAttribute('data-key');
        (v2Keys[k] = v2Keys[k] || []).push(el);
    });
    document.querySelectorAll('[data-hold]').forEach(function (el) {
        var k = el.getAttribute('data-hold');
        (v2Holds[k] = v2Holds[k] || []).push(el);
    });

    // An element counts as edited from its first input event until its change event (the own value
    // wins, the server confirms or clamps it with a patch) or until it is released / left without one.
    document.addEventListener('input', function (ev) {
        if (ev.target.hasAttribute && ev.target.hasAttribute('data-key'))
            v2Editing = ev.target;
    });
    document.addEventListener('change', function (ev) {
        if (ev.target === v2Editing)
            v2Editing = null;
        v2Deferred.delete(ev.target);
    });
    document.addEventListener('pointerup', function () {
        setTimeout(v2EndEdit, 0); // after the change event of the release
    });
    document.addEventListener('focusout', function (ev) {
        if (ev.target === v2Editing)
            v2EndEdit();
        else
            v2Flush(ev.target);
    });
}

function v2EndEdit()
{
    var el = v2Editing;
    v2Editing = null;
    if (el)
        v2Flush(el);
}

function v2Flush(el)
{
    if (v2Deferred.has(el))
    {
        var value = v2Deferred.get(el);
        v2Deferred.delete(el);
        v2SetElement(el, value);
    }
}

function v2SetElement(el, value)
{
    if (el.type === "checkbox")
    {
        el.checked = value;
    }
    else
    {
        el.value = value;
        var label = document.getElementById(el.id + "_value");
        if (label)
            label.innerHTML = value;
    }
}

function v2Apply(d)
{
    if (v2Keys === null)
        v2BuildKeyMap();
    for (const key in d)
        v2State.store[key] = d[key]; // keys without an element are kept for later pages
    btCardUpdate(); // before the values are set: the collar limit of the max sliders depends on the target
    for (const key in d)
    {
        var holds = v2Holds[key];
        if (holds)
            holds.forEach(function (el) { el.classList.toggle('ble-held', !!d[key]); });
        var els = v2Keys[key];
        if (!els)
            continue;
        els.forEach(function (el) {
            if (el === v2Editing)
                v2Deferred.set(el, d[key]); // being operated right now, it gets the value afterwards
            else
                v2SetElement(el, d[key]);
        });
    }
}

// The server confirms a change with a patch. If none comes (it clamped the value to what it already
// had, or dropped it) the element would keep showing a wrong value: put the server value back.
function v2Verify(el)
{
    clearTimeout(el._verifyTimer); // only the newest change of an element counts
    el._verifyTimer = setTimeout(function () {
        var key = el.getAttribute('data-key');
        if (el === v2Editing || !v2State.hasState || v2State.store[key] === undefined)
            return;
        if (String(v2State.store[key]) !== String(el.value))
            v2SetElement(el, v2State.store[key]);
    }, 800);
}

// put the last known server value back (the server rejected the value the user sent)
function v2Restore(key)
{
    if (v2Keys === null || v2State.store[key] === undefined)
        return;
    (v2Keys[key] || []).forEach(function (el) {
        v2Deferred.delete(el);
        if (el === v2Editing)
            v2Editing = null;
        v2SetElement(el, v2State.store[key]);
    });
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
            if (m.errors)
                m.errors.forEach(function (e) { v2Restore(e.k); });
            break;
    }
}

// ---------------------------------------------------------------------------
// Bluetooth mapping card: derived display only, the values themselves are plain data-key elements
// ---------------------------------------------------------------------------
var BT_OUT_COLLAR = 6;

function btCardUpdate()
{
    var st = v2State.store;
    for (var k = 0; k < 2; k++)
    {
        // the collar takes 0-100, everything else 0-255 (the server clamps the same way)
        var limit = (st['ble.map' + k + '.out'] === BT_OUT_COLLAR) ? 100 : 255;
        // fixed ranges 0-limit for both sliders: a range that follows the other slider rescales the
        // track and the other thumb jumps although its value does not change
        ['min', 'max'].forEach(function (which) {
            var el = document.getElementById('bt_' + which + k);
            if (el)
                el.max = limit;
        });
    }
    var out0 = st['ble.map0.out'], out1 = st['ble.map1.out'];
    var warn = document.getElementById('bt_same_warn');
    if (warn)
        warn.hidden = !(out0 > 0 && out0 === out1);
    var status = document.getElementById('bt_status');
    if (status && st['ble.connected'] !== undefined)
        status.textContent = st['ble.connected'] ? '(verbunden)' : '(nicht verbunden)';
}

// Min may not be dragged above max and max not below min: the thumb stops at the other one. The server
// would clamp the value, and if that changes nothing there is no patch and the slider would keep a wrong
// value. Uses the value shown by the other slider (it is the newest one).
function btClampInput(ev)
{
    var m = /^bt_(min|max)([01])$/.exec(ev.target.id || '');
    if (!m)
        return;
    var el = ev.target;
    var other = document.getElementById('bt_' + (m[1] === 'min' ? 'max' : 'min') + m[2]);
    if (!other)
        return;
    var v = Number(el.value), o = Number(other.value);
    if (m[1] === 'min' && v > o)
        v = o;
    else if (m[1] === 'max' && v < o)
        v = o;
    if (String(v) !== el.value)
    {
        el.value = v;
        var label = document.getElementById(el.id + '_value');
        if (label)
            label.innerHTML = v;
    }
}

// open / closed state of the card, only kept in this browser
function btCardRestoreOpen()
{
    document.addEventListener('input', btClampInput, true);
    var card = document.getElementById('bt-card');
    if (!card)
        return;
    try
    {
        if (localStorage.getItem('btCardOpen') === '1')
            card.open = true;
    }
    catch (e) { }
    card.addEventListener('toggle', function () {
        try { localStorage.setItem('btCardOpen', card.open ? '1' : '0'); }
        catch (e) { }
    });
}
