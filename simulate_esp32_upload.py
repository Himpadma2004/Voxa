import socket

host = "127.0.0.1"
port = 8000

# Boundary definition
boundary = "VoxaBoundary9C4F2A1D"

# Data payload (fake WAV file of 10 bytes)
file_data = b"RIFF..fmt "
file_size = len(file_data)

# Parts
part_header = (
    f"--{boundary}\r\n"
    f'Content-Disposition: form-data; name="file"; filename="voice.wav"\r\n'
    f"Content-Type: audio/wav\r\n\r\n"
).encode('ascii')

part_footer = f"\r\n--{boundary}--\r\n".encode('ascii')

# Calculated total length
total_len = len(part_header) + file_size + len(part_footer)

# HTTP Request headers
http_headers = (
    f"POST /api/voice/upload HTTP/1.1\r\n"
    f"Host: {host}:{port}\r\n"
    f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
    f"Content-Length: {total_len}\r\n"
    f"Connection: close\r\n\r\n"
).encode('ascii')

# Full payload
payload = http_headers + part_header + file_data + part_footer

print("--- RAW PAYLOAD BEING SENT ---")
print(payload)
print("------------------------------")
print("Total calculated size:", total_len)

# Connect and send via raw socket
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    s.sendall(payload)
    
    # Receive response
    response = b""
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        response += chunk
    s.close()
    
    print("\n--- SERVER RESPONSE ---")
    print(response.decode('utf-8', errors='ignore'))
    print("-----------------------")
except Exception as e:
    print("Error:", e)
