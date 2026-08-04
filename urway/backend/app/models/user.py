from pydantic import BaseModel, Field, EmailStr
from typing import List, Optional, Dict
from datetime import datetime

class OnboardingQuestionnaire(BaseModel):
    user_category: str = Field(..., example="Software Developer") # Students, Aspirants, Developers, Professionals, Fitness, Entrepreneurs
    primary_goals: List[str] = Field(..., example=["Learn PyTorch", "System Design", "Consistency"])
    target_daily_hours: float = Field(default=6.0, ge=1.0, le=16.0)
    preferred_learning_style: str = Field(default="Hands-on coding")
    self_rated_discipline: int = Field(default=7, ge=1, le=10)
    focus_time_preference: str = Field(default="Morning")

class UserRegister(BaseModel):
    email: EmailStr
    username: str
    password: str
    onboarding: OnboardingQuestionnaire

class UserLogin(BaseModel):
    username_or_email: str
    password: str

class UserProfileResponse(BaseModel):
    id: str
    email: str
    username: str
    user_category: str
    cluster_id: int = 0
    cluster_name: str = "Unassigned"
    created_at: datetime = Field(default_factory=datetime.utcnow)
