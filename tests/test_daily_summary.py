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

from summaries.daily_summary_service import (
    generate_daily_summary
)

result = generate_daily_summary()

print(
    "\n===== DAILY SUMMARY =====\n"
)

print(result)