import requests

from ai.prompts import (
    VOXA_RECALL_PROMPT
)

LM_STUDIO_URL = (
    "http://127.0.0.1:1234/v1/chat/completions"
)


def recall_answer(
    prompt,
    model_name
):

    response = requests.post(

        LM_STUDIO_URL,

        json={

            "model": model_name,

            "messages": [

                {
                    "role": "system",
                    "content":
                        VOXA_RECALL_PROMPT
                },

                {
                    "role": "user",
                    "content":
                        prompt
                }
            ],

            "temperature": 0.3,

            "max_tokens": 300
        },

        timeout=120
    )

    response.raise_for_status()

    data = response.json()

    return data[
        "choices"
    ][0][
        "message"
    ][
        "content"
    ]