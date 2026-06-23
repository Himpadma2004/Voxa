from dateparser.search import search_dates


def extract_datetime(
    time_text
):

    if not time_text:

        return None

    results = search_dates(

        time_text,

        settings={
            "PREFER_DATES_FROM":
                "future"
        }
    )

    print(
        f"Date Search Result: {results}"
    )

    if not results:

        return None

    return results[0][1]