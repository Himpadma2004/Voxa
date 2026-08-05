import threading
import time
import requests
from datetime import datetime, timedelta
from database.mongodb import db

reminders_collection = db["reminders"]

# Thread-safe global variables
ESP32_IP = None
IP_LOCK = threading.Lock()
SCHEDULER_INTERVAL_SEC = 1.0  # Configurable scan interval

def get_esp32_ip():
    global ESP32_IP
    with IP_LOCK:
        return ESP32_IP

def set_esp32_ip(ip):
    global ESP32_IP
    with IP_LOCK:
        if ESP32_IP != ip:
            ESP32_IP = ip
            print(f"[Scheduler] Registered ESP32 IP address: {ip}", flush=True)

def scheduler_loop():
    print("[Scheduler] Started", flush=True)
    while True:
        try:
            print("[Scheduler] Next scan...", flush=True)
            now = datetime.utcnow()
            # 60 second window: PENDING reminders where reminderTime <= now + 60s
            window = now + timedelta(seconds=60)
            
            # Query MongoDB for PENDING reminders that are inside the 60-second window
            pending_reminders = list(reminders_collection.find({
                "status": "pending",
                "reminder_time": {
                    "$lte": window
                }
            }))
            
            # Query MongoDB for SNOOZED reminders whose snoozeTime is reached
            snoozed_reminders = list(reminders_collection.find({
                "status": "snoozed",
                "snooze_until": {
                    "$lte": now
                }
            }))
            
            # Query MongoDB for overdue reminders whose date/time has passed
            overdue_reminders = list(reminders_collection.find({
                "status": {"$in": ["pending", "snoozed"]},
                "reminder_time": {"$lt": now - timedelta(minutes=1)}
            }))
            for r in overdue_reminders:
                reminders_collection.update_one(
                    {"_id": r["_id"]},
                    {"$set": {"status": "missed", "notice": "This reminder was missed by you."}}
                )

            # Combine triggered lists
            triggered = pending_reminders + snoozed_reminders
            
            for reminder in triggered:
                print(f"[Scheduler] Reminder found", flush=True)
                
                # Check if missed or active
                r_time = reminder.get("reminder_time")
                is_missed = isinstance(r_time, datetime) and r_time < now
                new_status = "missed" if is_missed else "active"

                # Mark status in MongoDB
                reminders_collection.update_one(
                    {"_id": reminder["_id"]},
                    {"$set": {"status": new_status, "notice": "This reminder was missed by you." if is_missed else ""}}
                )
                
                # Send immediate notification to the ESP32 (if registered)
                esp_ip = get_esp32_ip()
                if esp_ip:
                    print(f"[Scheduler] Sending reminder to ESP32", flush=True)
                    payload = {
                        "id": reminder.get("reminder_id"),
                        "title": reminder.get("title", ""),
                        "description": reminder.get("description", "") or reminder.get("comments", "") or "",
                        "reminderTime": int(reminder.get("reminder_time").timestamp()) if reminder.get("reminder_time") else int(time.time())
                    }
                    try:
                        res = requests.post(f"http://{esp_ip}:80/api/reminders/notify", json=payload, timeout=5)
                        if res.status_code == 200:
                            print(f"[Scheduler] Reminder acknowledged", flush=True)
                        else:
                            print(f"[Scheduler] Warning: ESP32 returned status code {res.status_code}", flush=True)
                    except Exception as ex:
                        print(f"[Scheduler] Error sending notification to ESP32: {ex}", flush=True)
                else:
                    print(f"[Scheduler] Warning: ESP32 IP is not registered. Cannot send immediate notification.", flush=True)
                    
        except Exception as e:
            print(f"[Scheduler] Error in loop: {e}", flush=True)
            
        time.sleep(SCHEDULER_INTERVAL_SEC)

def start_scheduler():
    t = threading.Thread(target=scheduler_loop, name="ReminderSchedulerThread", daemon=True)
    t.start()
