from reminders.reminder_parser import (
    extract_datetime
)

from reminders.reminder_repository import (
    save_reminder
)


def process_reminders(
    audio_id,
    transcript,
    llm_data
):

    reminders = llm_data.get(
        "reminders",
        []
    )

    if not reminders:

        print(
            "No reminders found"
        )

        return

    for reminder in reminders:

        reminder_text = reminder.get(
            "text",
            ""
        )

        reminder_time_text = reminder.get(
            "time",
            ""
        )

        reminder_datetime = extract_datetime(
            reminder_time_text
        )

        print(
            f"Reminder Text: {reminder_text}"
        )

        print(
            f"Reminder Time Text: {reminder_time_text}"
        )

        print(
            f"Reminder DateTime: {reminder_datetime}"
        )

        save_reminder(

            audio_id=audio_id,

            title=reminder_text,

            reminder_time=reminder_datetime
        )