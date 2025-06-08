// ====================================================================
// Author:       Alushi
// Created:      2025
// Description:  Tester for 4G module (ESP32-S3-SIM7670G) via HTTP(S)
//               – Performs GET, POST, and PUT using AT commands over serial
// ====================================================================


using System.IO.Ports;
using System.Text;

namespace FourGHttpTester
{
    class Program
    {
        /// <summary>
        /// Configuration for serial port, APN, and test URLs
        /// </summary>
        static class Config
        {
            public const string PortName   = "COM5";                     // Change to your COM port
            public const int    BaudRate   = 115200;                     // Serial speed
            public const string Apn        = "everywhere";               // EE APN for 4G data
            public const string GetUrl     = "https://httpbin.org/get";  // URL for GET test
            public const string PostUrl    = "https://httpbin.org/post"; // URL for POST test
            public const string PutUrl     = "https://httpbin.org/put";  // URL for PUT test
            public const string JsonHeader = "Accept: application/json"; // Header for JSON responses
        }

        static SerialPort _port;

        static void Main()
        {
            // ----------------------------------------------------------------
            // 1) Open and configure serial port for AT‐command interface
            // ----------------------------------------------------------------
            _port = new SerialPort(
                Config.PortName,
                Config.BaudRate,
                Parity.None,
                8,
                StopBits.One
            ) {
                NewLine      = "\r\n",
                ReadTimeout  = 2000,
                WriteTimeout = 2000
            };
            _port.Open();
            Console.WriteLine($"Opened {Config.PortName} @ {Config.BaudRate} bps");

            // ----------------------------------------------------------------
            // 2) Establish 4G data link: set APN, attach to network, open PDP
            // ----------------------------------------------------------------
            SendAT($"AT+CGDCONT=1,\"IP\",\"{Config.Apn}\""); // Define PDP context
            SendAT("AT+CGATT=1", 500);                      // Attach to packet domain
            SendAT("AT+NETOPEN", 2000);                     // Open data connection

            // ----------------------------------------------------------------
            // 3) Run HTTP method tests
            // ----------------------------------------------------------------
            bool okGet  = PerformGet (Config.GetUrl);
            bool okPost = PerformPost(Config.PostUrl, Encoding.ASCII.GetBytes("{\"msg\":\"Hello POST\"}"));
            bool okPut  = PerformPut (Config.PutUrl,  Encoding.ASCII.GetBytes("{\"msg\":\"Hello PUT\"}"));

            // ----------------------------------------------------------------
            // 4) Print summary of results
            // ----------------------------------------------------------------
            Console.WriteLine("\n=== SUMMARY ===");
            Console.WriteLine($"GET  [{Config.GetUrl}]  : {(okGet  ? "OK" : "FAIL")}");
            Console.WriteLine($"POST [{Config.PostUrl}] : {(okPost ? "OK" : "FAIL")}");
            Console.WriteLine($"PUT  [{Config.PutUrl}]  : {(okPut  ? "OK" : "FAIL")}");

            // ----------------------------------------------------------------
            // 5) Clean up: terminate HTTP service and close data link
            // ----------------------------------------------------------------
            SendAT("AT+HTTPTERM");
            SendAT("AT+NETCLOSE");
            Console.WriteLine("Done.");
        }

        /// <summary>
        /// Performs an HTTP GET via the SIM7670G and returns true on 200 OK.
        /// </summary>
        static bool PerformGet(string url)
        {
            Console.WriteLine($"\n-- HTTP GET → {url}");
            SendAT("AT+HTTPTERM");                     // Ensure any old session is closed
            SendAT("AT+HTTPINIT");                     // Initialize HTTP engine
            SendAT($"AT+HTTPPARA=\"URL\",\"{url}\"");  // Point to test URL
            SendAT($"AT+HTTPPARA=\"USERDATA\",\"{Config.JsonHeader}\""); // Add Accept header

            var (code, len) = ExecuteHttpAction(0);    // 0 = GET
            if (code != 200) return false;

            string body = ReadHttp(len);               // Read response body
            Console.WriteLine("Response body:");
            Console.WriteLine(body);
            return true;
        }

        /// <summary>
        /// Performs an HTTP POST with JSON payload. Returns true on 200 OK.
        /// </summary>
        static bool PerformPost(string url, byte[] payload)
        {
            Console.WriteLine($"\n-- HTTP POST → {url}");
            SendAT("AT+HTTPTERM");
            SendAT("AT+HTTPINIT");
            SendAT($"AT+HTTPPARA=\"URL\",\"{url}\"");
            SendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
            SendAT($"AT+HTTPPARA=\"USERDATA\",\"{Config.JsonHeader}\"");

            // Enter data mode: declare payload length and timeout
            SendAT($"AT+HTTPDATA={payload.Length},30000");
            _port.Write(payload, 0, payload.Length);   // Send JSON bytes
            _port.Write(new byte[]{0x1A}, 0, 1);       // Terminate with Ctrl-Z
            Thread.Sleep(200);
            Console.WriteLine(_port.ReadExisting().Trim());

            var (code, len) = ExecuteHttpAction(1);    // 1 = POST
            if (code != 200) return false;

            string body = ReadHttp(len);
            Console.WriteLine("Response body:");
            Console.WriteLine(body);
            return true;
        }

        /// <summary>
        /// Performs an HTTP PUT with JSON payload. Returns true on 200 OK.
        /// </summary>
        static bool PerformPut(string url, byte[] payload)
        {
            Console.WriteLine($"\n-- HTTP PUT → {url}");
            SendAT("AT+HTTPTERM");
            SendAT("AT+HTTPINIT");
            SendAT($"AT+HTTPPARA=\"URL\",\"{url}\"");
            SendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
            SendAT($"AT+HTTPPARA=\"USERDATA\",\"{Config.JsonHeader}\"");

            SendAT($"AT+HTTPDATA={payload.Length},30000");
            _port.Write(payload, 0, payload.Length);
            _port.Write(new byte[]{0x1A}, 0, 1);
            Thread.Sleep(200);
            Console.WriteLine(_port.ReadExisting().Trim());

            var (code, len) = ExecuteHttpAction(4);    // 4 = PUT
            if (code != 200) return false;

            string body = ReadHttp(len);
            Console.WriteLine("Response body:");
            Console.WriteLine(body);
            return true;
        }

        /// <summary>
        /// Issues AT+HTTPACTION and waits for the +HTTPACTION URC.
        /// Returns the HTTP status code and length of the response body.
        /// </summary>
        static (int code, int len) ExecuteHttpAction(int method)
        {
            _port.DiscardInBuffer();
            Console.WriteLine($"> AT+HTTPACTION={method}");
            _port.Write($"AT+HTTPACTION={method}\r\n");

            // Wait for asynchronous URC +HTTPACTION: <method>,<code>,<len>
            var sw = System.Diagnostics.Stopwatch.StartNew();
            string urc = "";
            while (sw.ElapsedMilliseconds < 10000)
            {
                Thread.Sleep(200);
                var chunk = _port.ReadExisting();
                if (!string.IsNullOrEmpty(chunk))
                {
                    Console.Write(chunk);
                    urc += chunk;
                    if (urc.Contains("+HTTPACTION:")) break;
                }
            }
            sw.Stop();

            var line = urc
                .Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries)
                .FirstOrDefault(l => l.StartsWith("+HTTPACTION:"));
            if (line == null) return (-1, 0);

            var parts = line.Split(',');
            int code = int.Parse(parts[1]);
            int len  = int.Parse(parts[2]);
            return (code, len);
        }

        /// <summary>
        /// Reads <length> bytes from the HTTP response via AT+HTTPREAD.
        /// </summary>
        static string ReadHttp(int length)
        {
            Console.WriteLine($"> AT+HTTPREAD=0,{length}");
            _port.Write($"AT+HTTPREAD=0,{length}\r\n");
            Thread.Sleep(500);
            var resp = _port.ReadExisting();
            Console.WriteLine(resp.Trim());
            return resp;
        }

        /// <summary>
        /// Sends a generic AT command and prints its reply.
        /// </summary>
        static void SendAT(string cmd, int waitMs = 200)
        {
            Console.WriteLine($"> {cmd}");
            _port.DiscardInBuffer();
            _port.Write(cmd + "\r\n");
            Thread.Sleep(waitMs);
            var resp = _port.ReadExisting();
            Console.WriteLine(resp.Trim());
        }
    }
}
