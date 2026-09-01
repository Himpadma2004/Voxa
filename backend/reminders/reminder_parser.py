import re
from datetime import datetime, date, time, timedelta

MONTHS = {
    "jan": 1, "january": 1,
    "feb": 2, "february": 2,
    "mar": 3, "march": 3,
    "apr": 4, "april": 4,
    "may": 5,
    "jun": 6, "june": 6,
    "jul": 7, "july": 7,
    "aug": 8, "august": 8,
    "sep": 9, "september": 9,
    "oct": 10, "october": 10,
    "nov": 11, "november": 11,
    "dec": 12, "december": 12
}

WEEKDAYS = {
    "monday": 0, "mon": 0,
    "tuesday": 1, "tue": 1, "tues": 1,
    "wednesday": 2, "wed": 2,
    "thursday": 3, "thu": 3, "thur": 3, "thurs": 3,
    "friday": 4, "fri": 4,
    "saturday": 5, "sat": 5,
    "sunday": 6, "sun": 6
}


def extract_datetime(time_text: str, fallback_text: str = None, base_dt: datetime = None) -> datetime:
    """
    Deterministically computes exact target datetimes for relative and explicit calendar expressions.
    Handles:
      - "tomorrow at 9 AM" -> tomorrow's date at 09:00:00
      - "tomorrow" -> tomorrow's date at 09:00:00
      - "next Monday at 5 PM" -> upcoming Monday at 17:00:00
      - "Friday at 3 PM" -> upcoming Friday at 15:00:00
      - "in 2 hours" -> now + 2 hours
      - "in 30 minutes" -> now + 30 minutes
      - "tonight at 8 PM" -> today at 20:00:00
      - "5th September at 10 AM" -> exact date & time
    """
    if base_dt is None:
        base_dt = datetime.now()

    # Try primary time_text first; if unresolved, combine with fallback_text (transcript)
    candidates = []
    if time_text and str(time_text).strip():
        candidates.append(str(time_text).strip())
    if fallback_text and str(fallback_text).strip():
        candidates.append(str(fallback_text).strip())
        if time_text:
            candidates.append(f"{time_text} {fallback_text}")

    if not candidates:
        return None

    for text in candidates:
        res = _parse_single_text(text, base_dt)
        if res:
            print(f"[ReminderParser] Parsed '{text}' -> {res.strftime('%Y-%m-%d %H:%M:%S (%A)')}")
            return res

    return None


def _parse_single_text(raw_text: str, base_dt: datetime) -> datetime:
    text = raw_text.lower().strip()
    text = re.sub(r'(\b\d{1,2})\.(\d{2})\b', r'\1:\2', text) # replace 5.30 with 5:30

    # 1. Relative offsets: "in X hours", "in X mins", "in X days"
    in_match = re.search(r'\bin\s+(\d+)\s*(hour|hr|minute|min|day|sec)s?\b', text)
    if in_match:
        val = int(in_match.group(1))
        unit = in_match.group(2)
        if 'hour' in unit or 'hr' in unit:
            return base_dt + timedelta(hours=val)
        elif 'min' in unit:
            return base_dt + timedelta(minutes=val)
        elif 'day' in unit:
            return base_dt + timedelta(days=val)
        elif 'sec' in unit:
            return base_dt + timedelta(seconds=val)

    # 2. Extract Time of Day (e.g. "9:30 AM", "5 PM", "14:00", "at 9")
    extracted_time = None
    time_match = re.search(r'\b(?:at\s+)?(\d{1,2})(?::(\d{2}))?\s*(am|pm)?\b', text)
    if time_match:
        hr_str, min_str, ampm = time_match.groups()
        if ampm or ':' in text or 'at ' in text:
            hr = int(hr_str)
            mn = int(min_str) if min_str else 0
            if ampm:
                if ampm == 'pm' and hr < 12:
                    hr += 12
                elif ampm == 'am' and hr == 12:
                    hr = 0
            if 0 <= hr <= 23 and 0 <= mn <= 59:
                extracted_time = time(hr, mn)

    # Contextual time keywords
    if not extracted_time:
        if 'tonight' in text or 'at night' in text:
            extracted_time = time(20, 0) # 8:00 PM
        elif 'in the morning' in text or 'morning' in text:
            extracted_time = time(9, 0)  # 9:00 AM
        elif 'afternoon' in text:
            extracted_time = time(14, 0) # 2:00 PM
        elif 'evening' in text:
            extracted_time = time(18, 0) # 6:00 PM
        else:
            extracted_time = time(9, 0)  # Default morning hour for date-only reminders

    # 3. Extract Date Component
    target_date = None

    # "day after tomorrow"
    if 'day after tomorrow' in text:
        target_date = base_dt.date() + timedelta(days=2)
    # "tomorrow"
    elif 'tomorrow' in text:
        target_date = base_dt.date() + timedelta(days=1)
    # "today" or "tonight"
    elif 'today' in text or 'tonight' in text:
        target_date = base_dt.date()
    else:
        # Check weekdays (e.g. "next monday", "on tuesday", "friday")
        for w_name, w_idx in WEEKDAYS.items():
            if re.search(r'\b(?:next\s+|this\s+|on\s+)?' + w_name + r'\b', text):
                current_w_idx = base_dt.weekday()
                days_ahead = (w_idx - current_w_idx) % 7
                if 'next' in text and days_ahead == 0:
                    days_ahead = 7
                elif days_ahead == 0:
                    if extracted_time and (extracted_time.hour < base_dt.hour or (extracted_time.hour == base_dt.hour and extracted_time.minute <= base_dt.minute)):
                        days_ahead = 7
                elif days_ahead < 0:
                    days_ahead += 7
                target_date = base_dt.date() + timedelta(days=days_ahead)
                break

        # Check explicit dates (e.g. "5th September", "September 5", "5 Sep", "Oct 12")
        if not target_date:
            for m_name, m_val in MONTHS.items():
                m_pattern = re.search(r'\b(?:(\d{1,2})(?:st|nd|rd|th)?\s+)?' + m_name + r'(?:\s+(\d{1,2})(?:st|nd|rd|th)?)?(?:,?\s*(\d{4}))?\b', text)
                if m_pattern:
                    d1, d2, yr = m_pattern.groups()
                    day_val = int(d1) if d1 else (int(d2) if d2 else 1)
                    year_val = int(yr) if yr else base_dt.year
                    try:
                        dt_candidate = date(year_val, m_val, day_val)
                        if not yr and dt_candidate < base_dt.date():
                            dt_candidate = date(year_val + 1, m_val, day_val)
                        target_date = dt_candidate
                        break
                    except ValueError:
                        pass

    # Fallback to dateparser if custom rules didn't find a date
    if not target_date:
        try:
            from dateparser.search import search_dates
            dp_res = search_dates(text, settings={"PREFER_DATES_FROM": "future", "RELATIVE_BASE": base_dt})
            if dp_res:
                return dp_res[0][1]
        except Exception:
            pass

    # If only time was specified (e.g. "remind me at 5 PM")
    if not target_date:
        target_date = base_dt.date()
        if extracted_time and (extracted_time.hour < base_dt.hour or (extracted_time.hour == base_dt.hour and extracted_time.minute <= base_dt.minute)):
            target_date += timedelta(days=1)

    return datetime.combine(target_date, extracted_time)