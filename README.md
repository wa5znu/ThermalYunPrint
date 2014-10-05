# Thermal Yun Print

Use Arduino Yun and curl to print text on Adafruit Thermal Printer:
Based on tutorials from
- http://www.adafruit.com/products/597 and
- http://arduino.cc/en/Guide/ArduinoYun

Also works on Doghunter Linino.

Sends HTTP requests and uses If-None-Match and ETags request/response headers to avoid printing the same text twice.
Stores updated ETag in EEPROM only when it changes.

No additional software required on Linino side; Arduino side just uses curl.

Because of the way the Arduino Yun works, reprogramming the Arduino side erases the EEPROM so it will re-print the last message after code changes.

Example request where content did not match last message printed:

    GET /print.txt HTTP/1.1 
    User-Agent: curl/7.29.0 
    Host: example.com 
    Accept: */* 
    If-None-Match: "12e1c95-2d-504a14d41153f" 
     
    HTTP/1.1 200 OK 
    Date: Sat, 04 Oct 2014 23:39:16 GMT 
    Server: Apache/2.2.22 (Ubuntu) 
    Last-Modified: Sat, 04 Oct 2014 23:39:13 GMT 
    ETag: "12e1c95-29-504a15b68da13" 
    Accept-Ranges: bytes 
    Content-Length: 41 
    Vary: Accept-Encoding 
    Content-Type: text/plain 
    X-Pad: avoid browser bug 
     
    Do something unusual today.  Pay a bill.
    
Next request after that is an example where content matched.

    GET /print.txt HTTP/1.1 
    User-Agent: curl/7.29.0 
    Host: example.com 
    Accept: */* 
    If-None-Match: "12e1c95-29-504a15b68da13" 
     
    HTTP/1.1 304 Not Modified 
    Date: Sat, 04 Oct 2014 23:39:21 GMT 
    Server: Apache/2.2.22 (Ubuntu) 
    ETag: "12e1c95-29-504a15b68da13" 
    Vary: Accept-Encoding 



