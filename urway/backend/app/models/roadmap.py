from pydantic import BaseModel, Field
from typing import List, Optional, Dict
from datetime import datetime

class TaskItem(BaseModel):
    title: str
    description: str
    estimated_minutes: int
    category: str = Field(default="Productivity")
    completed: bool = Field(default=False)

class Milestone(BaseModel):
    title: str
    target_date: str
    deliverable: str
    tasks: List[TaskItem]

class AdaptiveRoadmapResponse(BaseModel):
    user_id: str
    meta_cluster_name: str
    productivity_score: float
    consistency_score: float
    daily_plan: List[TaskItem]
    weekly_plan: List[TaskItem]
    milestones: List[Milestone]
    adaptation_reason: str
    generated_at: datetime = Field(default_factory=datetime.utcnow)
