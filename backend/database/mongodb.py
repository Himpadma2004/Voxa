import uuid
from pymongo import MongoClient
from dotenv import load_dotenv
from datetime import datetime, timedelta
import os
import dns.resolver

load_dotenv()

# Override dnspython's default resolver to use public DNS servers (Google/Cloudflare).
# This prevents LifetimeTimeout / ConfigurationError when local router/adapter DNS servers fail SRV resolution.
try:
    custom_resolver = dns.resolver.Resolver()
    custom_resolver.nameservers = ['8.8.8.8', '8.8.4.4', '1.1.1.1', '1.0.0.1']
    custom_resolver.timeout = 5.0
    custom_resolver.lifetime = 10.0
    dns.resolver.default_resolver = custom_resolver
except Exception as dns_err:
    print(f"Warning: Could not set custom DNS resolver: {dns_err}")

import certifi

MONGO_URI = os.getenv("MONGO_URI")
MONGO_DB = os.getenv("MONGO_DB")
MONGO_COLLECTION = os.getenv("MONGO_COLLECTION")

try:
    client = MongoClient(
        MONGO_URI,
        tlsCAFile=certifi.where(),
        serverSelectionTimeoutMS=2500,
        connectTimeoutMS=2500,
        socketTimeoutMS=2500
    )
    client.admin.command("ping")
    print("MongoDB Connected")
except Exception as ssl_err:
    print(f"Standard TLS connection failed ({ssl_err}). Retrying with SSL fallback options...")
    try:
        client = MongoClient(
            MONGO_URI,
            tls=True,
            tlsAllowInvalidCertificates=True,
            serverSelectionTimeoutMS=2500,
            connectTimeoutMS=2500,
            socketTimeoutMS=2500
        )
        client.admin.command("ping")
        print("MongoDB Connected (with SSL fallback)")
    except Exception as e:
        print("MongoDB Connection Warning (offline/resilient mode):", e)
        try:
            client = MongoClient(MONGO_URI, serverSelectionTimeoutMS=1000, connectTimeoutMS=1000)
        except Exception:
            client = MongoClient(serverSelectionTimeoutMS=1000)


db = client[MONGO_DB]
collection = db[MONGO_COLLECTION]          # Main raw audio notes
reminders_collection = db["reminders"]      # Reminders database collection
ideas_collection = db["ideas"]              # Ideas database collection
questions_collection = db["questions"]      # Questions database collection
tasks_collection = db["tasks"]              # Tasks database collection
others_collection = db["others"]            # Others database collection


def route_and_store_category_items(audio_id, llm_data, timestamp=None):
    """
    Routes and stores structured LLM output into separate dedicated MongoDB collections:
    'ideas', 'questions', 'tasks', 'reminders', and 'others'.
    Ensures that 'others' ONLY receives items that do NOT belong to Idea, Question, Task, or Reminder!
    """
    if not timestamp:
        timestamp = datetime.now()

    cat_raw = str(llm_data.get("category", "Other")).strip()
    cat_clean = cat_raw.lower().rstrip('s')
    summary = (llm_data.get("summary") or "").strip()

    ideas = llm_data.get("ideas") or []
    questions = llm_data.get("questions") or []
    tasks = llm_data.get("tasks") or []
    reminders = llm_data.get("reminders") or []
    notes = llm_data.get("notes") or []
    thoughts = llm_data.get("thoughts") or []

    items_stored = False

    # 1. Store in Ideas Collection
    for idea in ideas:
        text = idea.get("text") or idea.get("title") or idea.get("content") or str(idea) if isinstance(idea, dict) else str(idea)
        if text and text.strip():
            items_stored = True
            if not ideas_collection.find_one({"audio_id": audio_id, "title": text.strip()}):
                ideas_collection.insert_one({
                    "item_id": str(uuid.uuid4()),
                    "audio_id": audio_id,
                    "title": text.strip(),
                    "content": text.strip(),
                    "category": "Idea",
                    "created_at": timestamp,
                    "updated_at": timestamp
                })

    if not items_stored and cat_clean == "idea" and summary:
        items_stored = True
        if not ideas_collection.find_one({"audio_id": audio_id, "title": summary}):
            ideas_collection.insert_one({
                "item_id": str(uuid.uuid4()),
                "audio_id": audio_id,
                "title": summary,
                "content": summary,
                "category": "Idea",
                "created_at": timestamp,
                "updated_at": timestamp
            })

    # 2. Store in Questions Collection
    for q in questions:
        text = q.get("text") or q.get("title") or q.get("question") or str(q) if isinstance(q, dict) else str(q)
        if text and text.strip():
            items_stored = True
            if not questions_collection.find_one({"audio_id": audio_id, "title": text.strip()}):
                questions_collection.insert_one({
                    "item_id": str(uuid.uuid4()),
                    "audio_id": audio_id,
                    "title": text.strip(),
                    "content": text.strip(),
                    "category": "Question",
                    "created_at": timestamp,
                    "updated_at": timestamp
                })

    if not items_stored and cat_clean == "question" and summary:
        items_stored = True
        if not questions_collection.find_one({"audio_id": audio_id, "title": summary}):
            questions_collection.insert_one({
                "item_id": str(uuid.uuid4()),
                "audio_id": audio_id,
                "title": summary,
                "content": summary,
                "category": "Question",
                "created_at": timestamp,
                "updated_at": timestamp
            })

    # 3. Store in Tasks Collection
    for t in tasks:
        text = t.get("text") or t.get("title") or t.get("task") or str(t) if isinstance(t, dict) else str(t)
        if text and text.strip():
            items_stored = True
            if not tasks_collection.find_one({"audio_id": audio_id, "title": text.strip()}):
                tasks_collection.insert_one({
                    "item_id": str(uuid.uuid4()),
                    "audio_id": audio_id,
                    "title": text.strip(),
                    "content": text.strip(),
                    "category": "Task",
                    "status": "pending",
                    "created_at": timestamp,
                    "updated_at": timestamp
                })

    if not items_stored and cat_clean == "task" and summary:
        items_stored = True
        if not tasks_collection.find_one({"audio_id": audio_id, "title": summary}):
            tasks_collection.insert_one({
                "item_id": str(uuid.uuid4()),
                "audio_id": audio_id,
                "title": summary,
                "content": summary,
                "category": "Task",
                "status": "pending",
                "created_at": timestamp,
                "updated_at": timestamp
            })

    # 4. Store in Reminders Collection
    from reminders.reminder_parser import extract_datetime

    for r in reminders:
        title = r.get("text") or r.get("title") or r.get("reminder") or r.get("action") or str(r) if isinstance(r, dict) else str(r)
        r_time_raw = r.get("time") or r.get("due") or r.get("due_date") or r.get("date") if isinstance(r, dict) else None
        
        parsed_dt = extract_datetime(r_time_raw) if r_time_raw else None
        if not parsed_dt and title:
            parsed_dt = extract_datetime(title)
        if not parsed_dt and summary:
            parsed_dt = extract_datetime(summary)

        reminder_time_val = parsed_dt if parsed_dt else (r_time_raw or summary)

        if title and title.strip():
            items_stored = True
            existing = reminders_collection.find_one({"audio_id": audio_id, "title": title.strip()})
            if not existing:
                reminders_collection.insert_one({
                    "reminder_id": str(uuid.uuid4()),
                    "audio_id": audio_id,
                    "title": title.strip(),
                    "reminder_time": reminder_time_val,
                    "status": "pending",
                    "created_at": timestamp
                })
            elif parsed_dt and not isinstance(existing.get("reminder_time"), datetime):
                reminders_collection.update_one(
                    {"_id": existing["_id"]},
                    {"$set": {"reminder_time": parsed_dt}}
                )

    if not items_stored and cat_clean == "reminder" and summary:
        items_stored = True
        parsed_dt = extract_datetime(summary)
        reminder_time_val = parsed_dt if parsed_dt else summary
        if not reminders_collection.find_one({"audio_id": audio_id, "title": summary}):
            reminders_collection.insert_one({
                "reminder_id": str(uuid.uuid4()),
                "audio_id": audio_id,
                "title": summary,
                "reminder_time": reminder_time_val,
                "status": "pending",
                "created_at": timestamp
            })

    # 5. Store in Others Collection ONLY IF it was NOT categorized as Idea/Question/Task/Reminder!
    if not items_stored:
        other_values = []
        if notes:
            other_values.extend([n.get("text") or n.get("content") or str(n) if isinstance(n, dict) else str(n) for n in notes])
        if thoughts:
            other_values.extend([th.get("text") or th.get("content") or str(th) if isinstance(th, dict) else str(th) for th in thoughts])
        if not other_values and summary:
            other_values.append(summary)

        for text in other_values:
            if text and text.strip():
                if not others_collection.find_one({"audio_id": audio_id, "title": text.strip()}):
                    others_collection.insert_one({
                        "item_id": str(uuid.uuid4()),
                        "audio_id": audio_id,
                        "title": text.strip(),
                        "content": text.strip(),
                        "category": cat_raw if cat_clean in ("note", "thought") else "Other",
                        "created_at": timestamp,
                        "updated_at": timestamp
                    })


def migrate_all_existing_data():
    """
    Scans all existing documents in MongoDB 'audio_notes'
    and categorizes / routes them into their corresponding dedicated collections:
    'ideas', 'questions', 'tasks', 'reminders', and 'others'.
    """
    print("\n[Migration] Starting MongoDB Data Migration for Category Collections...")
    all_notes = list(collection.find())

    for note in all_notes:
        audio_id = note.get("audio_id") or str(note.get("_id"))
        timestamp = note.get("created_at") or note.get("processed_at")
        if not timestamp and "_id" in note and hasattr(note["_id"], "generation_time"):
            timestamp = note["_id"].generation_time
        if not timestamp:
            timestamp = datetime.now()

        llm_data = {
            "category": note.get("category", "Other"),
            "summary": note.get("summary", ""),
            "ideas": note.get("ideas", []),
            "questions": note.get("questions", []),
            "tasks": note.get("tasks", []),
            "reminders": note.get("reminders", []),
            "notes": note.get("notes", []),
            "thoughts": note.get("thoughts", [])
        }

        route_and_store_category_items(audio_id, llm_data, timestamp)

    counts = {
        "ideas": ideas_collection.count_documents({}),
        "questions": questions_collection.count_documents({}),
        "tasks": tasks_collection.count_documents({}),
        "reminders": reminders_collection.count_documents({}),
        "others": others_collection.count_documents({})
    }

    print(f"[Migration] Complete! MongoDB Category Collections Summary:")
    print(f"   - Ideas Collection      : {counts['ideas']} documents")
    print(f"   - Questions Collection  : {counts['questions']} documents")
    print(f"   - Tasks Collection      : {counts['tasks']} documents")
    print(f"   - Reminders Collection  : {counts['reminders']} documents")
    print(f"   - Others Collection     : {counts['others']} documents\n")

    sync_mongodb_to_local_json_caches()


def sync_mongodb_to_local_json_caches():
    """
    Exports MongoDB category collections directly to local JSON cache files
    used by VOXA desktop application & simulator ('memory.json', 'ideas.json', 'questions.json', 'reminders.json').
    """
    import json

    target_dirs = [
        os.path.abspath("data"),
        os.path.abspath("simulator/data"),
        os.path.abspath("simulator/build/data"),
        os.path.abspath("simulator/build/Debug/data")
    ]

    for d in target_dirs:
        os.makedirs(d, exist_ok=True)

    def _doc_sort_key(doc):
        from routes.notes import parse_to_datetime
        val = (
            doc.get("reminder_time") or
            doc.get("dateTime") or
            doc.get("created_at") or
            doc.get("processed_at") or
            doc.get("timestamp")
        )
        return parse_to_datetime(val, doc.get("_id"))

    # 1. Sync Others -> memory.json
    others_docs = list(others_collection.find())
    others_docs.sort(key=_doc_sort_key, reverse=True)
    memories_payload = []
    for idx, doc in enumerate(others_docs):
        created_str = doc.get("created_at").isoformat() if hasattr(doc.get("created_at"), "isoformat") else str(doc.get("created_at", ""))
        memories_payload.append({
            "id": str(idx + 1),
            "title": doc.get("title", ""),
            "content": doc.get("content", ""),
            "category": "other",
            "tags": "other,voxa",
            "createdAt": created_str,
            "updatedAt": created_str,
            "importance": "1",
            "favorite": "false",
            "source": "MongoDB",
            "timestamp": created_str,
            "durationSeconds": "0",
            "comments": doc.get("comments", "")
        })

    # 2. Sync Ideas -> ideas.json
    ideas_docs = list(ideas_collection.find())
    ideas_docs.sort(key=_doc_sort_key, reverse=True)
    ideas_payload = []
    for idx, doc in enumerate(ideas_docs):
        created_str = doc.get("created_at").isoformat() if hasattr(doc.get("created_at"), "isoformat") else str(doc.get("created_at", ""))
        ideas_payload.append({
            "id": str(idx + 1),
            "title": doc.get("title", ""),
            "content": doc.get("content", ""),
            "timestamp": created_str
        })

    # 3. Sync Questions -> questions.json
    questions_docs = list(questions_collection.find())
    questions_docs.sort(key=_doc_sort_key, reverse=True)
    questions_payload = []
    for idx, doc in enumerate(questions_docs):
        created_str = doc.get("created_at").isoformat() if hasattr(doc.get("created_at"), "isoformat") else str(doc.get("created_at", ""))
        questions_payload.append({
            "id": str(idx + 1),
            "text": doc.get("title", ""),
            "answer": doc.get("content", ""),
            "timestamp": created_str,
            "answered": "false"
        })

    # 4. Sync Reminders -> reminders.json
    reminders_docs = list(reminders_collection.find())
    reminders_docs.sort(key=_doc_sort_key, reverse=True)
    reminders_payload = []
    for idx, doc in enumerate(reminders_docs):
        r_time = doc.get("reminder_time") or doc.get("dateTime") or doc.get("created_at")
        time_str = r_time.isoformat() if hasattr(r_time, "isoformat") else str(r_time or "")
        status_str = str(doc.get("status", "pending")).lower()
        reminders_payload.append({
            "id": str(idx + 1),
            "title": doc.get("title", ""),
            "dateTime": time_str,
            "completed": "true" if status_str in ("completed", "cleared") else "false"
        })

    for d in target_dirs:
        try:
            with open(os.path.join(d, "memory.json"), "w", encoding="utf-8") as f:
                json.dump(memories_payload, f, indent=2)
            with open(os.path.join(d, "ideas.json"), "w", encoding="utf-8") as f:
                json.dump(ideas_payload, f, indent=2)
            with open(os.path.join(d, "questions.json"), "w", encoding="utf-8") as f:
                json.dump(questions_payload, f, indent=2)
            with open(os.path.join(d, "reminders.json"), "w", encoding="utf-8") as f:
                json.dump(reminders_payload, f, indent=2)
        except Exception as err:
            pass

    print("[Sync] Exported MongoDB category collections to VOXA application JSON caches.")


def save_audio_metadata(document):
    result = collection.insert_one(
        document
    )

    return str(
        result.inserted_id
    )


def update_transcript(
    audio_id,
    transcript
):
    collection.update_one(
        {
            "audio_id": audio_id
        },
        {
            "$set": {
                "transcript": transcript,
                "status": "transcribed",
                "transcribed_at": datetime.now()
            }
        }
    )

    print(
        "MongoDB updated"
    )


def update_llm_result(
    audio_id,
    llm_data,
    model_name
):
    # Retrieve original recording timestamp from existing document in audio_notes
    existing = collection.find_one({"audio_id": audio_id})
    orig_timestamp = None
    if existing:
        orig_timestamp = existing.get("created_at") or existing.get("processed_at")
        if not orig_timestamp and "_id" in existing and hasattr(existing["_id"], "generation_time"):
            orig_timestamp = existing["_id"].generation_time
    if not orig_timestamp:
        orig_timestamp = datetime.now()

    processed_time = datetime.now()
    collection.update_one(
        {
            "audio_id": audio_id
        },
        {
            "$set": {
                "status": "processed",
                "processed_at": processed_time,
                "created_at": orig_timestamp,
                "llm_model": model_name,
                "category": llm_data.get(
                    "category",
                    "Other"
                ),
                "summary": llm_data.get(
                    "summary",
                    ""
                ),
                "tasks": llm_data.get(
                    "tasks",
                    []
                ),
                "reminders": llm_data.get(
                    "reminders",
                    []
                ),
                "ideas": llm_data.get(
                    "ideas",
                    []
                ),
                "questions": llm_data.get(
                    "questions",
                    []
                ),
                "thoughts": llm_data.get(
                    "thoughts",
                    []
                ),
                "notes": llm_data.get(
                    "notes",
                    []
                ),
                "priority": llm_data.get(
                    "priority",
                    "Low"
                ),
                "llm_raw": llm_data
            }
        }
    )

    # Automatically route and store into dedicated MongoDB category collections with the original recording timestamp!
    route_and_store_category_items(audio_id, llm_data, orig_timestamp)
    sync_mongodb_to_local_json_caches()

    print(
        "MongoDB LLM Data Updated & Category Collections Synced"
    )


def get_audio_by_id(
    audio_id
):
    return collection.find_one(
        {
            "audio_id": audio_id
        }
    )


def get_all_notes():
    return list(
        collection.find()
    )


def cleanup_old_reminders():
    """
    Cleans up legacy/old cleared or dismissed reminders older than 30 days.
    """
    try:
        cutoff = datetime.now() - timedelta(days=30)
        reminders_collection.delete_many({
            "status": {"$in": ["cleared", "dismissed", "completed"]},
            "created_at": {"$lt": cutoff}
        })
    except Exception as e:
        print(f"[Cleanup] Error in cleanup_old_reminders: {e}")
