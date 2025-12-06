| Supported Targets | ESP32-S3 | ESP32 | ESP32-C3 | ESP32-C2 |
| ----------------- | -------- | ----- | -------- | -------- |

# HTTPS GET over 4G SIM (SIM7600G) Example

Demonstrates how to use a SIM7600G 4G/LTE modem connected over UART to an ESP32-S3 to perform an HTTPS GET request via AT commands.

This example covers:

- Checking network registration and PDP context attachment
- Configuring APN, authentication, and activating PDP
- Initializing the SIM HTTP service with SSL enabled
- Issuing an HTTP GET and parsing the `+HTTPACTION` URC
- Reading the remote payload via `AT+HTTPREAD`
- Cleaning up and disabling the HTTP service

## How to use this example

1. **Set up your hardware**:

   - ESP32-S3 development board (other ESP32 targets may work with pin adjustments).
   - SIM7600G (or similar) 4G modem wired to UART2 (TX→GPIO18, RX→GPIO17).
   - LED on board for status indication (optional).
   - Valid SIM card inserted with active data plan.

2. **Configure APN and credentials**:

   - In `hello_HTTP_SIM.c`, update the PDP parameters in the `sim_http_get_sample()` function:
     ```c
     send_at("AT+CGDCONT=1,\"IP\",\"<YOUR_APN>\"", ...);
     send_at("AT+CGAUTH=1,1,\"<USER>\",\"<PASS>\"", ...);
     ```

3. **Build and flash**:

   ```sh
   idf.py set-target esp32s3
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

4. **Observe the log output**:

   - The app prints chip info on startup.
   - It will show `+CGREG`, `+CGATT`, and PDP address URCs.
   - On success you’ll see:
     ```
     +HTTPACTION: 0,200,<length>
     +HTTPREAD: <length>\n<payload>
     ```
   - On failure, it logs warnings and aborts the HTTP flow.

## Folder structure

```
├── CMakeLists.txt          # Top-level CMake project file
├── main
│   ├── CMakeLists.txt      # Component build instructions
│   └── hello_HTTP_SIM.c    # Example application source
└── README.md               # This file
```

## Troubleshooting

- **No +CGREG or +CGATT response**: Check SIM power and antenna. Ensure SIM is registered on network.
- **PDP context activation fails**: Verify APN and credentials. Run `AT+CGDCONT?` manually to inspect.
- **+HTTPACTION never arrives**: Increase UART read timeout or implement separate URC read. See code comments.
- **SSL handshake errors**: Confirm HTTPS port is open; verify firmware supports `AT+HTTPSSL`.
