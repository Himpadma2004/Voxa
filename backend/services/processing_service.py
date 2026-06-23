from ai.llm_client import (
    analyze_transcript
)

from ai.parser import (
    parse_llm_output
)


def process_transcript(
    transcript,
    model_name
):

    raw_output = analyze_transcript(
        transcript,
        model_name
    )

    structured_data = parse_llm_output(
        raw_output
    )

    print(
        "\n✅ Structured Data Created"
    )

    return structured_data