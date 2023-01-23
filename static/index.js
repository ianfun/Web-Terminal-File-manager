const command = 0;
var terminal = new Terminal({
    fontFamily:  "'Ubuntu Mono', 'Inconsolata', 'Source Code Pro', 'monospace', 'Consolas'",
    cursorBlink: true,
    scrollback: 1000,
    windowsMode: true,
    bellStyle: "sound",
    tabStopWidth: 10
});
terminal.open(document.body.firstElementChild);
const fitAddon = new FitAddon.FitAddon();
terminal.loadAddon(fitAddon);
fitAddon.fit();
terminal.onData(function (e) { ws.send(e); } );
dir = sessionStorage.getItem("shell-dir");
if (dir)
    sessionStorage.removeItem("shell-dir");
else 
    dir = ".";
var ws = new WebSocket('ws://' + location.host + '?rows=' + terminal.rows + '&cols=' + terminal.cols + "&cmd=" + command.toString(), [encodeURIComponent(dir)]);
var oldy = terminal.rows;
var oldx = terminal.cols;
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
ws.onerror = console.error;
ws.onclose = function () {
    terminal.onData = function() {};
    window.onresize = function() {};
    terminal.write("=============== termianl closed. ===============")
}
