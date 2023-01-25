var command = 0;
var enableWebLinks = true;
var enableWebGL = true;
const buffer_max = 1000;
const bufferedAmountMax = 10000;
const fitAddon = new FitAddon.FitAddon();
var terminal;
var ws;
var oldy;
var oldx;
var options = {
    fontFamily:  "'Ubuntu Mono', 'Inconsolata', 'Source Code Pro', 'monospace', 'Consolas'",
    cursorBlink: true,
    scrollback: 1000,
    windowsMode: true,
    bellStyle: "sound",
    tabStopWidth: 10
};
var gl_fonts = localStorage.getItem("_google-fonts");
var settings = localStorage.getItem("settings");
if (gl_fonts) {
    gl_fonts = JSON.parse(gl_fonts)["fonts"];
    if (gl_fonts.length) {
        WebFont.load({google: { families: gl_fonts}});
    }
}
if (settings) {
    settings = JSON.parse(settings);
    settings.theme = localStorage.getItem('theme-' + settings.theme);
    enableWebLinks = settings.WebLinks;
    enableWebGL = settings.webGL;
    if (settings.theme)
        settings.theme = JSON.parse(settings.theme);
    else
        settings.theme = {};
    if (Object.assign)
        Object.assign(options, settings);
    else
        options = {...options, ...settings};
}
terminal = new Terminal(options);
terminal.open(document.body.firstElementChild);
terminal.loadAddon(fitAddon);
fitAddon.fit();
function sendLarge(data, start) {
    if (start >= data.length)
        return;
    if (ws.bufferedAmount > bufferedAmountMax)
        return setTimeout(sendLarge, 50);
    ws.send(data.slice(start, start + buffer_max));
    sendLarge(data, start + buffer_max);
}
terminal.onData(function (e) {
    if (e.length > buffer_max)
        return sendLarge(e, 0);
    ws.send(e);
});
dir = sessionStorage.getItem("shell-dir");
if (dir)
    sessionStorage.removeItem("shell-dir");
else 
    dir = ".";
ws = new WebSocket('ws://' + location.host + '?rows=' + terminal.rows + '&cols=' + terminal.cols + "&cmd=" + command.toString(), [encodeURIComponent(dir)]);
oldy = terminal.rows;
oldx = terminal.cols;
ws.onopen = () => {
    ws.onmessage = function(m) {
        m.data.startsWith && terminal.write(m.data);
        m.data.text && m.data.text().then((t) => {
            terminal.write(t);
        });
    };
    ws.onclose = function() {window.close()};
    terminal.focus();
};
window.onresize = function () {
    fitAddon.fit();
    if (oldx != terminal.cols || oldy != terminal.rows) {
        oldx = terminal.cols;
        oldy = terminal.rows;    
        var arr = new Uint8Array(5);
        arr[1] = terminal.rows & 0xFF;
        arr[2] = terminal.rows >> 8;
        arr[3] = terminal.cols & 0xFF;
        arr[4] = terminal.cols >> 8;
        ws.send(arr);
    }
}
ws.onclose = function () {
    terminal.onData = function() {};
    window.onresize = function() {};
    terminal.write("=============== termianl closed. ===============");
}
var searchAddon = new SearchAddon.SearchAddon();
terminal.loadAddon(searchAddon);
if (enableWebLinks)
    terminal.loadAddon(new WebLinksAddon.WebLinksAddon());
if (enableWebGL)
    terminal.loadAddon(new WebglAddon.WebglAddon());
window.onbeforeunload = function() {
    if (ws.readyState == WebSocket.OPEN)
        return "Your terminal is still running, kill anyway?";
}
terminal.onTitleChange(function(title) { document.title = title; });
document.documentElement.style.backgroundColor = settings.theme.background || 'black';

var menu = document.getElementById('setting-menu');
document.getElementById('setting-bar').addEventListener('click', function(e) {
    if (menu.style.visibility == 'hidden') {
        menu.style.visibility = 'visible';
    } else {
        menu.style.visibility = 'hidden';
    }
    e.stopPropagation();
}, false);
document.addEventListener('click', function() {
    menu.style.visibility = 'hidden';
})
function toggleFullScreen() {
  if (!document.fullscreenElement &&    // alternative standard method
      !document.mozFullScreenElement && !document.webkitFullscreenElement && !document.msFullscreenElement ) {  // current working methods
    if (document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen();
    } else if (document.documentElement.msRequestFullscreen) {
      document.documentElement.msRequestFullscreen();
    } else if (document.documentElement.mozRequestFullScreen) {
      document.documentElement.mozRequestFullScreen();
    } else if (document.documentElement.webkitRequestFullscreen) {
      document.documentElement.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
    }
  } else {
    if (document.exitFullscreen) {
      document.exitFullscreen();
    } else if (document.msExitFullscreen) {
      document.msExitFullscreen();
    } else if (document.mozCancelFullScreen) {
      document.mozCancelFullScreen();
    } else if (document.webkitExitFullscreen) {
      document.webkitExitFullscreen();
    }
  }
}
var search_bar = document.getElementById('search-bar');
function show_search_bar() {
    search_bar.style.visibility = 'visible';
}
function close_search() {
    search_bar.style.visibility = 'hidden';
}
function do_search() {
    var s = search_bar.firstElementChild.value;
    if (!searchAddon.findNext(s)) {
        alert('Not found');
    }
}
function do_search_p() {
    var s = search_bar.firstElementChild.value;
    if (!searchAddon.findPrevious(s)) {
        alert('Not found');
    }
}
