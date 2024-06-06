'use strict';

var spectrum, logger, ws;

function connectWebSocket(spectrum) {

    ws = new WebSocket("ws://" + window.location.host + "/websocket");
    //ws = new WebSocket("ws://192.168.1.184:81/");

    spectrum.setWebSocket(ws);
  
    ws.onopen = function(evt) {
        console.log("WS connected!");
    }
    ws.onclose = function(evt) {
        console.log("WS closed");
        setTimeout(function() {
            connectWebSocket(spectrum);
        }, 1000);
    }
    ws.onerror = function(evt) {
        console.log("WS error: " + evt.message);
    }
    ws.onmessage = function (evt) {
        var data = JSON.parse(evt.data);
        if (data.s) {
            spectrum.addData(data.s);
        } else {
            if (data.center) {
                spectrum.setCenterHz(data.center);                
            }
            if (data.span) {
                spectrum.setSpanHz(data.span);
            }
            if (data.gain) {
                spectrum.setGain(data.gain);
            }
            if (data.framerate) {
                spectrum.setFps(data.framerate);
            }
            spectrum.log(" > Freq:" + data.center / 1000000 + " MHz | Span: " + data.span / 1000000 + " MHz | Gain: " + data.gain + "dB | Fps: " + data.framerate);
        }
    }
}


function main() {
    
    // Create spectrum object on canvas with ID "waterfall"
    spectrum = new Spectrum(
        "waterfall", {
            spectrumPercent: 50,
            logger: 'log',            
    });

    // Connect to websocket
    connectWebSocket(spectrum);

    // Bind keypress handler
    window.addEventListener("keydown", function (e) {
        spectrum.onKeypress(e);
    });

    
}

window.onload = main;
