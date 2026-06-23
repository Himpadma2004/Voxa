import json

from ai.llm_client import (
    analyze_transcript
)


def fallback_parser(
    transcript
):

    transcript = transcript.strip()

    if transcript.endswith("?"):

        return {

            "category":
                "Question",

            "summary":
                transcript,

            "tasks": [],

            "reminders": [],

            "ideas": [],

            "questions":
                [transcript],

            "thoughts": [],

            "notes": [],

            "priority":
                "Low"
        }

    return {

        "category":
            "Other",

        "summary":
            transcript,

        "tasks": [],

        "reminders": [],

        "ideas": [],

        "questions": [],

        "thoughts": [],

        "notes": [],

        "priority":
            "Low"
    }


def process_transcript(
    transcript,
    model_name
):

    raw_output = analyze_transcript(
        transcript,
        model_name
    )

    try:

        structured_data = json.loads(
            raw_output
        )

        print(
            "\n✅ Structured Data Created"
        )

        return structured_data

    except Exception:

        try:

            fixed_output = raw_output.strip()

            if not fixed_output.endswith("}"):

                fixed_output += "}"

            structured_data = json.loads(
                fixed_output
            )

            print(
                "\n✅ Fixed Broken JSON"
            )

            return structured_data

        except Exception as e:

            print(
                f"\nJSON Parse Error: {e}"
            )

            return fallback_parser(
                transcript
            )