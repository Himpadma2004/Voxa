import sys
import os

sys.path.append(
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            "..",
            "backend"
        )
    )
)

from reminders.reminder_parser import (
    extract_datetime
)

print(
    extract_datetime(
        "Tomorrow at 8pm remind me to call Rahul"
    )
)

print(
    extract_datetime(
        "Remind me in 2 minutes"
    )
)

print(
    extract_datetime(
        "Next Monday at 10am"
    )
)