# Voxa

Voxa is a voice-first AI productivity and knowledge management system designed to capture ideas instantly and transform them into actionable information.

Instead of storing raw voice recordings, Voxa converts spoken thoughts into structured knowledge using AI-powered transcription, categorization, task extraction, reminder detection, semantic search, and daily summaries.

The project is inspired by minimalist note-taking devices and aims to explore the intersection of embedded systems, cloud computing, artificial intelligence, and personal knowledge management.

## Core Workflow

1. Record a voice note.
2. Upload audio to Google Cloud Storage.
3. Transcribe speech using OpenAI Whisper.
4. Store transcripts and metadata in MongoDB.
5. Extract tasks, reminders, and categories using AI.
6. Generate embeddings for semantic search.
7. Build a searchable personal knowledge base.
8. Create daily summaries and productivity insights.

## Features

* Voice note recording
* Cloud audio storage
* Speech-to-text transcription
* Automatic note categorization
* Task and reminder extraction
* Daily AI-generated summaries
* Semantic search using embeddings
* Personal knowledge graph (future)
* ESP32 + E-Ink hardware integration (future)

## Tech Stack

### Backend

* Python
* FastAPI

### Database

* MongoDB

### Cloud Storage

* Google Cloud Storage (GCS)

### AI

* OpenAI Whisper
* Local LLMs (planned)

### Search

* Qdrant / ChromaDB

### Hardware (Future)

* ESP32
* E-Ink Display
* SD Card Module

## Project Goal

The primary objective of Voxa is learning and experimentation. It serves as a hands-on project for exploring real-world AI pipelines, cloud infrastructure, embedded systems, vector search, and intelligent personal productivity tools.
