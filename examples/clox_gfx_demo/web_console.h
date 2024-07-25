const char web_console[] = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Web Terminal</title>
    <style>
        #terminal {
            background-color: black;
            color: lightblue;
            padding: 2px;
            font-family: monospace;
            font-size: 14px;
            width: 80%;
            height: 400px;
            overflow: auto;
            white-space: pre-wrap;
            overflow-y: scroll;
            word-wrap: break-word;
        }
        #input {
            position: absolute;
            left: -9999px;
        }
    </style>
</head>
<body>
    <div id="terminal"></div>
    <input id="input" type="text">
    <script>
        const terminal = document.getElementById('terminal');
        const input = document.getElementById('input');
        const ws = new WebSocket('ws://%LOCAL_ADDRESS_AND_PORT%');
        var current_line = '';
        
        ws.onopen = () => {
            terminal.textContent += 'Connected to server\n';
        };
        
        ws.onmessage = (event) => {
            terminal.textContent += event.data;
            terminal.scrollTop = terminal.scrollHeight;
        };

        terminal.addEventListener('click', () => {
            input.focus();
        });

        input.addEventListener('input', () => {
            terminal.textContent += input.value;
            current_line += input.value;
            input.value = '';
            terminal.scrollTop = terminal.scrollHeight;
        });

        input.addEventListener('paste', (event) => {
            let paste = (event.clipboardData || window.clipboardData).getData('text');
            event.preventDefault();
            const lines = paste.split(/(\r\n|\n|\r)/);
            for (let line of lines) {
                ws.send(line + '\n');
            }
        });

        input.addEventListener('keydown', (event) => {
            if (event.key === 'Enter') {
                event.preventDefault();
                terminal.textContent += '\n';
                ws.send(current_line + '\n');
                terminal.scrollTop = terminal.scrollHeight;
                current_line = '';
            }
            else if (event.key === 'Delete' || event.key === 'Backspace') {
                if (current_line.length > 0) {
                    current_line = current_line.slice(0, -1);
                    terminal.textContent = terminal.textContent.slice(0, -1);
                }
            }
        });
    </script>
</body>
</html>
)";