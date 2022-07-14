var ws;
terminal = new Terminal({
    fontFamily: "Inconsolata",
    cursorBlink: true,
    scrollback: 1000,
    windowsMode: true,
    bellStyle: "sound",
    cols: ~~(screen.width / 9),
    rows: ~~(screen.height / 20)
});
terminal.open(document.body);
var buf = String();
terminal.onData((e) => {
    ws.send(e);
});
//const fitAddon = new FitAddon.FitAddon();
//terminal.loadAddon(fitAddon);
//fitAddon.fit();
terminal.focus();

ws = new WebSocket('ws://' + location.host + '?rows=' + terminal.rows + '&cols=' + terminal.cols, "cmd");
ws.onopen = () => {
    terminal.write('* * * connection established * * *\r\n');
};
ws.onclose = e => {
    if (e.reason != '') {
        terminal.write("\r\n* * * connection closed * * *\r\n"+e.reason);
    }
    else {
        terminal.write('\r\n* * *connection closed...* * *\r\n');
    }
};
ws.onerror = console.error;
ws.onmessage = (m) => {
    m.data.startsWith && terminal.write(m.data);
    m.data.text && m.data.text().then((t) => {
        terminal.write(t);
    });
};
