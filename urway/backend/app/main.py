import sys
import os
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

# Ensure ai_engine package is resolvable on pythonpath
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from app.config import settings
from app.database import connect_to_mongo, close_mongo_connection
from app.routes import ingest, roadmap

@asynccontextmanager
async def lifespan(app: FastAPI):
    print(f"[U'rWay Server] Starting {settings.PROJECT_NAME} v{settings.VERSION}...")
    print(f"[U'rWay Server] Tagline: '{settings.PROJECT_TAGLINE}'")
    await connect_to_mongo()
    yield
    await close_mongo_connection()

app = FastAPI(
    title=settings.PROJECT_NAME,
    description="U'rWay: AI-Powered Personalized Roadmap & Behavioral Intelligence Platform API",
    version=settings.VERSION,
    lifespan=lifespan
)

# Enable CORS for Next.js frontend & Chrome/VS Code Extensions
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include Route Handlers
app.include_router(ingest.router, prefix=settings.API_V1_STR)
app.include_router(roadmap.router, prefix=settings.API_V1_STR)

@app.get("/")
async def root():
    return {
        "project": settings.PROJECT_NAME,
        "tagline": settings.PROJECT_TAGLINE,
        "version": settings.VERSION,
        "status": "online",
        "docs_url": "/docs"
    }
