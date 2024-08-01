'use strict';

var spectrum, logger, ws;
const canvas = document.getElementById('waterfall');;

function connectWebSocket(spectrum) {

    ws = new WebSocket("ws://" + window.location.host + "/websocket");
    //ws = new WebSocket("ws://192.168.1.184:81/");

    spectrum.setWebSocket(ws);
  
    ws.onopen = function(evt) {
        ws.binaryType = 'arraybuffer';
        console.log("WS opened!");
    }

    ws.onconnect = function (evt) {
        ws.binaryType = 'arraybuffer';
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

        if (evt.data instanceof ArrayBuffer) {
            spectrum.addData(evt.data);
        }        
        else {

            var data = JSON.parse(evt.data);

            if (data.center) {
                spectrum.setCenterHz(data.center);
                //console.log(data.center/1e6);
            }
            if (data.span) {
                spectrum.setSpanHz(data.span);
            }
            if (data.gain) {
                spectrum.setGain(data.gain);
            }
            if (data.framerate) {
                spectrum.setFps(data.framerate);
<<<<<<< Updated upstream
            }
=======
            }            
>>>>>>> Stashed changes
            spectrum.log(" > Freq:" + data.center / 1e6 + " MHz | Span: " + data.span / 1e6 + " MHz | Tuning Step: " + spectrum.tuningStep/1e6 + " MHz | Gain: " + data.gain + "dB | Fps: " + data.framerate);
        }
    }
}

function updateTooltip(x, y) {
    let pxfreqratio = spectrum.canvas.width / (spectrum.spanHz/1e6);
    let mouseFreq = (x / pxfreqratio) + ((spectrum.centerHz - (spectrum.spanHz/2)) / 1e6) ;
    mouseFreq = Number(mouseFreq).toFixed(2);

    //tooltip.innerHTML = `X: ${x}, Y: ${y} | ${mouseFreq}M`;
    tooltip.innerHTML = `${mouseFreq}M`;
    tooltip.style.top = `${y+10}px`;
    tooltip.style.left = `${x}px`;
    tooltip.style.display = 'block';
}

const box = { x: 0, y: 0, w: 0, h: 0 };

function drawBox(e) {
    const ctx = canvas.getContext('2d');
    const rect = canvas.getBoundingClientRect();

    const x = Math.min(e.clientX, rect.right);
    const y = Math.min(e.clientY, rect.bottom);
    const w = Math.abs(e.clientX - box.x);
    const h = Math.abs(e.clientY - box.y);

    //ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = 'rgba(0,0,0,0.1)';
    ctx.fillRect(Math.min(box.x, x), Math.min(box.y, y), w, h);    
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

    // Bind tooltip
    let tooltip = document.getElementById('tooltip');

    canvas.addEventListener('mousemove', (event) => {
      updateTooltip(event.clientX, event.clientY);
    });

    canvas.addEventListener('mouseout', () => {
      tooltip.style.display = 'none';
    });

    canvas.addEventListener('mousedown', (e) => {
        box.x = e.offsetX;
        box.y = e.offsetY;
        canvas.addEventListener('mousemove', drawBox);
        canvas.addEventListener('mouseup', () => {
          canvas.removeEventListener('mousemove', drawBox);
        });
    });
}

window.onload = main;
