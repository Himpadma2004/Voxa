import socket

host = "0.0.0.0"
port = 8000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((host, port))
s.listen(1)

print(f"Dummy listener active on port {port}. Waiting for ESP32 connection...")

try:
    conn, addr = s.accept()
    print(f"Connection accepted from {addr}")
    
    # Read headers
    request_data = b""
    header_end = -1
    
    while True:
        chunk = conn.recv(1024)
        if not chunk:
            break
        request_data += chunk
        
        # Check if we have received the full headers
        header_end = request_data.find(b"\r\n\r\n")
        if header_end != -1:
            break
            
    if header_end != -1:
        headers_part = request_data[:header_end+4]
        body_start = request_data[header_end+4:]
        
        print("\n=== RAW REQUEST HEADERS RECEIVED ===")
        print(headers_part)
        print("====================================\n")
        
        # Print header lines clearly
        print("=== DECODED HEADERS ===")
        print(headers_part.decode('utf-8', errors='ignore'))
        print("=======================\n")
        
        # Read the rest of the body based on Content-Length if present
        content_length = 0
        for line in headers_part.decode('utf-8', errors='ignore').split("\r\n"):
            if line.lower().startswith("content-length:"):
                content_length = int(line.split(":")[1].strip())
                break
                
        print(f"Advertised Content-Length: {content_length}")
        print(f"Buffered body size initially: {len(body_start)}")
        
        # Read remaining body
        body_data = body_start
        while len(body_data) < content_length:
            chunk = conn.recv(4096)
            if not chunk:
                break
            body_data += chunk
            
        print(f"Total body size read: {len(body_data)}")
        
        # Log multipart boundaries
        print("\n=== BODY HEADERS (first 500 bytes) ===")
        print(body_data[:500])
        print("======================================\n")
        
        print("=== BODY FOOTER (last 100 bytes) ===")
        print(body_data[-100:])
        print("====================================\n")
        
    else:
        print("Could not find end of headers. Received raw:")
        print(request_data)
        
    # Send successful response
    response = (
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 88\r\n"
        "Connection: close\r\n\r\n"
        '{"success":true,"audio_id":"dummy-id-123456","status":"processing"}'
    ).encode('utf-8')
    
    conn.sendall(response)
    print("Dummy response sent successfully.")
    conn.close()
    
except Exception as e:
    print("Error:", e)
finally:
    s.close()
    print("Listener closed.")
