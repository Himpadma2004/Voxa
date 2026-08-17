import re
from dateparser.search import search_dates


def extract_datetime(
    time_text
):
    if not time_text:
        return None

    # Normalize dot time notation e.g. "5.30" -> "5:30", "5.30 PM" -> "5:30 PM"
    normalized_text = re.sub(r'(\b\d{1,2})\.(\d{2})\b', r'\1:\2', str(time_text))

    results = search_dates(
        normalized_text,
        settings={
            "PREFER_DATES_FROM": "future"
        }
    )

    print(
        f"Date Search Result for '{normalized_text}': {results}"
    )

    if not results:
        return None

    return results[0][1]