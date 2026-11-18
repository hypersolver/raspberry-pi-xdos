#!/usr/bin/env python3
"""Local LAN test HTTP server: /ok returns instantly; /stall delays headers 30s.
Run as root if using port 80: sudo python3 http-timeout-test.py
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from time import sleep

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/stall':
            sleep(30)
        body=b'<html><body><h1>XD OS NETWORK OK</h1><p>Timeout test server</p></body></html>'
        try:
            self.send_response(200)
            self.send_header('Content-Type','text/html')
            self.send_header('Content-Length',str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):
            pass

if __name__=='__main__':
    print('Test HTTP server on TCP 80. Use /ok or /stall (30s delayed response).')
    ThreadingHTTPServer(('0.0.0.0',80),Handler).serve_forever()
