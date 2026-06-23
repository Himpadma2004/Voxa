import requests

payload = {
    "model": "meta-llama-3.1-8b-instruct",
    "messages": [
        {
            "role": "user",
            "content": "Return only JSON: {\"status\":\"ok\"}"
        }
    ],
    "temperature": 0.1,
    "max_tokens": 50
}

response = requests.post(
    "http://127.0.0.1:1234/v1/chat/completions",
    json=payload,
    timeout=120
)

print(response.json())