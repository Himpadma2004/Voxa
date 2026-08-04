from pydantic import BaseModel, Field
from typing import List, Optional, Dict
from datetime import datetime

class BrowserActivityPayload(BaseModel):
    domain: str = Field(..., example="github.com")
    session_duration_seconds: int = Field(..., ge=1)
    is_productive: bool = Field(default=True)

class VSCodeActivityPayload(BaseModel):
    language: str = Field(..., example="python")
    active_coding_seconds: int = Field(..., ge=0)
    idle_seconds: int = Field(default=0, ge=0)

class YouTubeActivityPayload(BaseModel):
    video_category: str = Field(..., example="Technology") # Education, Technology, Business, Motivation, Self Improvement, Entertainment, Gaming, Music, Sports
    duration_watched_seconds: int = Field(..., ge=1)
    is_short: bool = Field(default=False)

class IngestResponse(BaseModel):
    status: str = "success"
    encrypted: bool = True
    recorded_at: datetime = Field(default_factory=datetime.utcnow)
