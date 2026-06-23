import json


DEFAULT_RESPONSE = {

    "category": "Other",

    "summary": "",

    "tasks": [],

    "reminders": [],

    "ideas": [],

    "questions": [],

    "priority": "Low"
}


def parse_llm_output(
    llm_output
):

    try:

        parsed = json.loads(
            llm_output
        )

        return parsed

    except Exception as e:

        print(
            f"JSON Parse Error: {e}"
        )

        return DEFAULT_RESPONSE