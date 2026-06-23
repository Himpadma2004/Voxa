def build_memory_text(
    transcript,
    structured_data
):

    category = structured_data.get(
        "category",
        "Other"
    )

    if category == "Idea":

        ideas = structured_data.get(
            "ideas",
            []
        )

        if ideas:
            return "\n".join(
                ideas
            )

    if category == "Question":

        questions = structured_data.get(
            "questions",
            []
        )

        if questions:
            return "\n".join(
                questions
            )

    if category == "Thought":

        thoughts = structured_data.get(
            "thoughts",
            []
        )

        if thoughts:
            return "\n".join(
                thoughts
            )

    if category == "Task":

        tasks = structured_data.get(
            "tasks",
            []
        )

        if tasks:
            return "\n".join(
                tasks
            )

    if category == "Reminder":

        reminders = structured_data.get(
            "reminders",
            []
        )

        if reminders:
            return str(
                reminders
            )

    return transcript