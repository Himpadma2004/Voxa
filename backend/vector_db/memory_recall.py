from datetime import datetime
from vector_db.search_service import search_memories
from ai.recall_client import recall_answer
from config.settings import ACTIVE_MODEL
from database.mongodb import get_all_notes
from reminders.reminder_repository import load_all_reminders


def recall_memories(user_query: str) -> str:
    """
    Searches user's database records (MongoDB audio_notes, MongoDB reminders, and ChromaDB vector store)
    and uses the LLM to answer questions about the user's data (e.g. "when I need to call Jennifer").
    """
    print(f"\n[RecallEngine] Processing recall query: \"{user_query}\"")

    # 1. Query ChromaDB vector database
    vector_docs = []
    try:
        results = search_memories(user_query, n_results=5)
        if results and "documents" in results and results["documents"]:
            vector_docs = results["documents"][0]
    except Exception as e:
        print(f"[RecallEngine] Vector search warning: {e}")

    # 2. Query MongoDB Scheduled Reminders
    mongo_reminders = []
    try:
        mongo_reminders = load_all_reminders()
    except Exception as e:
        print(f"[RecallEngine] MongoDB reminders load warning: {e}")

    formatted_reminders = []
    for r in mongo_reminders:
        title = r.get("title", "Untitled")
        time_str = r.get("reminder_time", "Unknown time")
        status = r.get("status", "pending")
        formatted_reminders.append(f"- Title: {title} | Time: {time_str} | Status: {status}")

    # 3. Query MongoDB Voice Notes & Transcripts
    mongo_notes = []
    try:
        mongo_notes = get_all_notes()
    except Exception as e:
        print(f"[RecallEngine] MongoDB notes load warning: {e}")

    formatted_notes = []
    for n in mongo_notes:
        if n.get("status") == "processed":
            cat = n.get("category", "Note")
            summary = n.get("summary", "")
            transcript = n.get("transcript", "")
            rems = n.get("reminders", [])
            tasks = n.get("tasks", [])
            created = n.get("processed_at") or n.get("created_at") or ""

            entry = f"[{cat}] Summary: {summary}"
            if rems:
                entry += f" | Reminders: {rems}"
            if tasks:
                entry += f" | Tasks: {tasks}"
            if transcript:
                entry += f" | Full Transcript: {transcript}"
            if created:
                entry += f" | Date: {created}"
            formatted_notes.append(entry)

    # 4. Assemble Database Context
    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S (%A)")
    context_parts = []

    if formatted_reminders:
        context_parts.append("=== SCHEDULED REMINDERS (DATABASE) ===\n" + "\n".join(formatted_reminders))

    if formatted_notes:
        context_parts.append("=== USER NOTES & RECORDINGS (DATABASE) ===\n" + "\n".join(formatted_notes))

    if vector_docs:
        context_parts.append("=== RELEVANT SEMANTIC MEMORIES (VECTOR DB) ===\n" + "\n\n".join(vector_docs))

    if not context_parts:
        return "No memories or notes found in database."

    full_context = "\n\n".join(context_parts)

    prompt = f"""You are Voxa Memory Recall Engine, an intelligent assistant that searches user database records (notes, reminders, voice transcripts, memories) to accurately answer user questions.

Current Local Time: {now_str}

User Question/Search: "{user_query}"

User Database Data:
{full_context}

Instructions:
1. Search through all notes, reminders, transcripts, and memories provided above.
2. If the user asks about a specific person (e.g. "Jennifer"), task, time, or event (e.g. "when I need to call Jennifer"), find the exact matching item and state the time/date/details clearly and concisely.
3. If specific information exists in the data, give a direct, natural language answer.
4. If no relevant information is found in the database, clearly state that no matching records were found.
5. Keep your response concise, helpful, and natural.
"""

    answer = recall_answer(prompt, ACTIVE_MODEL)
    return answer