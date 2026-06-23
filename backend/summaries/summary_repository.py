from database.mongodb import db

summaries_collection = db["summaries"]


def save_daily_summary(
    date,
    summary
):

    summaries_collection.update_one(

        {
            "date": date
        },

        {
            "$set": {
                "summary": summary
            }
        },

        upsert=True
    )

    print(
        "✅ Daily Summary Saved"
    )