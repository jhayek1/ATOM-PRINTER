# Atom Printer

## Overview

M5Stack Atom Printer firmware

## How to use?

- 1.connect to AP `ATOM_PRINTER-xxxx`
- 2.Print data via web page (visit 192.168.4.1) 
- 3.Configure Wi-Fi in the web page

<img src="./docs/atom_printer_config_01.png" width="60%">
<img src="./docs/atom_printer_config_02.jpg" width="60%">


- 4.print data through mqtt server. publish topic is the device mac address.

```shell
topic: xx:xx:xx:xx:xx:xx
```

- 5.mqtt payload

```shell
TEXT,10,1:Hai
```

```shell
BAR:1234
```

```shell
QR:1234
```

Binary payloads are passed to the printer as ESC/POS bytes:

- starting with `GS v 0` (`1D 76 30`): a raster image, printed as-is
- `RAW:` followed by any ESC/POS bytes: written to the printer untouched

- 6.private broker (optional, recommended)

By default the printer uses the public `mqtt.m5stack.com` broker, where anyone
can read and send messages on any topic. To use your own broker with a login
and TLS (e.g. a free [EMQX Serverless](https://www.emqx.com/en/cloud/serverless-mqtt)
deployment), copy `examples/PRINTER_FW/ATOM_PRINTER_SECRETS.h.example` to
`ATOM_PRINTER_SECRETS.h` (ignored by git), fill it in and flash. Port 8883
switches to TLS.

The setup access point turns itself off 2 minutes after Wi-Fi connects. Hold the
button for 5 seconds to reset the settings and bring it back.

- 7.editing the web pages

The pages are stored gzipped. After editing `index.html` or `image.html`, run
`python3 tools/html_to_header.py` to regenerate the headers.

## Related Link

[Document & AT Command](https://docs.m5stack.com/en/atom/atom_printer)

