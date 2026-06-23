from datetime import datetime

from database.mongodb import (
    collection
)

from ai.llm_client import (
    analyze_transcript
)

from summaries.summary_repository import (
    save_daily_summary
)

from config.settings import (
    ACTIVE_MODEL
)


def generate_daily_summary():

    today = datetime.utcnow().date()

    records = list(
        collection.find()
    )

    daily_text = ""

    for record in records:

        created_at = record.get(
            "created_at"
        )

        if not created_at:
            continue

        if created_at.date() != today:
            continue

        daily_text += (
            f"\nCategory: "
            f"{record.get('category', '')}"
        )

        daily_text += (
            f"\nSummary: "
            f"{record.get('summary', '')}"
        )

        daily_text += "\n"

    if not daily_text:

        return "No records today"

    prompt = f"""
Create a concise daily summary.

Records:

{daily_text}

Include:

1. Key Ideas
2. Important Questions
3. Tasks
4. Reminders
5. Important Notes
6. Overall Summary

Return plain text only.
"""

    summary = analyze_transcript(
        prompt,
        ACTIVE_MODEL
    )

    save_daily_summary(
        str(today),
        summary
    )

    return summary