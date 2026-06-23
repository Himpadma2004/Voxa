import requests

from ai.prompts import (
    VOXA_SYSTEM_PROMPT
)

LM_STUDIO_URL = (
    "http://127.0.0.1:1234/v1/chat/completions"
)


def analyze_transcript(
    transcript,
    model_name
):

    print(
        "\n🧠 Sending transcript to LLM..."
    )

    payload = {

        "model": model_name,

        "messages": [

            {
                "role": "system",
                "content":
                    VOXA_SYSTEM_PROMPT
            },

            {
                "role": "user",
                "content":
                    transcript
            }
        ],

        "temperature": 0,

        "max_tokens": 300
    }

    response = requests.post(

        LM_STUDIO_URL,

        json=payload,

        timeout=120
    )

    if response.status_code != 200:

        print(
            "\n===== LM STUDIO ERROR ====="
        )

        print(
            response.text
        )

        print(
            "==========================="
        )

        raise Exception(
            f"LM Studio Error {response.status_code}"
        )

    data = response.json()

    content = data[
        "choices"
    ][0][
        "message"
    ][
        "content"
    ]

    print(
        "\n===== RAW LLM RESPONSE ====="
    )

    print(
        content
    )

    print(
        "============================\n"
    )

    return content