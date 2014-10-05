/*
 * Use Arduino Yun and curl to print text on Adafruit Thermal Printer:
 * Based on tutorials from
 * - http://www.adafruit.com/products/597 and
 * - http://arduino.cc/en/Guide/ArduinoYun
 *
 * Use the modified version of the Adafruit Thermal Printer Library that supports setFont('B'), at 
 * https://github.com/wa5znu/Adafruit-Thermal-Printer-Library
 *
 * Also works on Doghunter Linino.
 *
 * Sends HTTP requests and uses If-None-Match and ETags request/response headers to avoid printing the same text twice.
 * Stores updated ETag in EEPROM only when it changes.
 * 
 * No additional software required on Linino side; Arduino side just uses curl.
 *
 * Because of the way the ArduinoYun works, reprogramming the Arduino side erases the EEPROM so it will re-print the last message after code changes.
 * 
 * Example request where content did not match last message printed:
 *
 *     GET /print.txt HTTP/1.1 
 *     User-Agent: curl/7.29.0 
 *     Host: example.com 
 *     Accept: */* 
 *     If-None-Match: "12e1c95-2d-504a14d41153f" 
 *      
 *     HTTP/1.1 200 OK 
 *     Date: Sat, 04 Oct 2014 23:39:16 GMT 
 *     Server: Apache/2.2.22 (Ubuntu) 
 *     Last-Modified: Sat, 04 Oct 2014 23:39:13 GMT 
 *     ETag: "12e1c95-29-504a15b68da13" 
 *     Accept-Ranges: bytes 
 *     Content-Length: 41 
 *     Vary: Accept-Encoding 
 *     Content-Type: text/plain 
 *     X-Pad: avoid browser bug 
 *      
 *     Do something unusual today.  Pay a bill.
 *     
 * Next request after that is an example where content matched.
 * 
 *     GET /print.txt HTTP/1.1 
 *     User-Agent: curl/7.29.0 
 *     Host: example.com 
 *     Accept: */* 
 *     If-None-Match: "12e1c95-29-504a15b68da13" 
 *      
 *     HTTP/1.1 304 Not Modified 
 *     Date: Sat, 04 Oct 2014 23:39:21 GMT 
 *     Server: Apache/2.2.22 (Ubuntu) 
 *     ETag: "12e1c95-29-504a15b68da13" 
 *     Vary: Accept-Encoding 
 *
 */ 

#include <Process.h>
#include "SoftwareSerial.h"
#include "Adafruit_Thermal.h"
#include <avr/pgmspace.h>

#include <EEPROM.h>
#include "config.h"

int printer_RX_Pin = 5;  // This is the green wire
int printer_TX_Pin = 6;  // This is the yellow wire

Adafruit_Thermal printer(printer_RX_Pin, printer_TX_Pin);

const byte maxlinelen = 42;
const byte buflen = (maxlinelen+1);
char linebuf[buflen];

const byte header_line_len = 50;
char etag[header_line_len];
char if_none_match[header_line_len];

long last_time = 0;

const int EEPROM_END = 512;
const int EEPROM_FLAG_ADDRESS = 511;
const byte EEPROM_FLAG_VALUE = 0xAA;

void setup() {
#if SERIAL_DEBUG
  Serial.begin(9600);
  while (! Serial) {
  }
  Serial.println(F("setup()"));
#endif

#if PRINT
  printer.begin();
  printer.setFont('B');
  printer.setSize('S');
  printer.println(PAGE);
#endif

  Bridge.begin();

#if SERIAL_DEBUG
  Serial.print(F("PAGE: "));
  Serial.println(PAGE);
#endif

  etag[0] = 0;
  if_none_match[0] = 0;

  validate_eeprom();

  read_etag_from_eeprom();
}

void read_etag_from_eeprom() {
  for (int i = 0; i < header_line_len; i++) {
    char value = EEPROM.read(i);
    etag[i] = value;
    if (value == 0) break;
  }
  set_if_none_match();
}

void write_etag_to_eeprom() {
  for (int i = 0; i < header_line_len; i++) {
    char value = etag[i];
    EEPROM.write(i, value);
    if (value == 0) break;
  }
}

void validate_eeprom() {
  byte value = EEPROM.read(EEPROM_FLAG_ADDRESS);
  if (value != EEPROM_FLAG_VALUE) {
    clear_eeprom();
    EEPROM.write(EEPROM_FLAG_ADDRESS, EEPROM_FLAG_VALUE);
  }
}

void clear_eeprom() {
  for (int i = 0; i < EEPROM_END; i++) {
    EEPROM.write(i, 0x00);
  }
}

void set_etag(char *linebuf) {
  strlcpy(etag, linebuf, header_line_len-1);
  set_if_none_match();
  write_etag_to_eeprom();
}

// copy etag 'Etag: "xxx-xxx-xxxx"\n' to if_none_match 'If-None-Match: "xxx-xxx-xxxx"\n"
void set_if_none_match() {
  // copy etag 'Etag: "xxx-xxx-xxxx"\n' to if_none_match 'If-None-Match: "xxx-xxx-xxxx"\n"
  char *p = etag;
  char c = 0;
  while (true) {
    c = *p++;
    if ((c == 0) || (c == ' ' && *p != 0)) {
      // leave p pointing at character after space, or at EOD.
      // check C -- if it's a zero then it's EOD otherwise it's the etag value.
      break;
    }
  }
  if (c != 0) {
    strlcpy(if_none_match, "If-None-Match: ", header_line_len-1);
    strlcat(if_none_match, p, header_line_len-1-strlen("If-None-Match: ")-1);
  }
}

void loop() {
  long now = millis();
  if ((now - last_time) > check_millis) {
    last_time = now;
    fetchAndPrint(PAGE);
  }
}

void fetchAndPrint(char *url) {
  // Launch "curl" command and get Arduino ascii art logo from the network
  // curl is command line program for transferring data using different internet protocols
  Process p;        // Create a process and call it "p"
  p.begin("curl");  // Process that launch the "curl" command
  if (if_none_match[0] != 0) {
    p.addParameter("-H"); // add If-None-Match request header if we have a saved etag.
    p.addParameter(if_none_match);
  }
  p.addParameter("-D"); // output headers first, so we can check Etags and Date-Modified
  p.addParameter("-");
  p.addParameter(url); // Add the URL parameter to "curl"
  p.run();      // Run the process and wait for its termination

  // Print arduino logo over the Serial
  // A process output can be read with the stream methods
  boolean in_header = true;
  boolean etag_match = false;

  byte i = 0;
  while (p.available() > 0) {
    char c = p.read();
    linebuf[i++] = c;
    if (c == '\n' || i == buflen-1) {
      linebuf[i++] = 0;
      if (in_header) {
        in_header = process_header_line(&etag_match);
      } else if (! etag_match) {
        process_content_line();
      }
      i = 0;
    }
  }
  if (i > 0) {
    linebuf[i++] = 0;
    if (! in_header && ! etag_match) {
      process_content_line();
    }
  }
}

// if this is end of headers, just return false.
// otherwise, process header line looking for ETag header, and return true.
// to process Etag, check against the ETag we already have, set etag_match accordingly, and save out new etag if it changed.
boolean process_header_line(boolean *etag_match) {
  if ((linebuf[0] == '\n') || (linebuf[0] == '\r')) {
    return false;
  } else {
    if (strncasecmp(linebuf, "ETag:", 5) == 0) {
      if (etag[0] != 0 && strcmp(linebuf, etag) == 0) {
        *etag_match = true;
      } else {
        // etag did not match; update our if_none_match header
        // in anticipation of processing the content.
        set_etag(linebuf);
        *etag_match = false;
      }
    }
    return true;
  }
}

void process_content_line() {
#if PRINT
    printer.print(linebuf);
#endif
}
