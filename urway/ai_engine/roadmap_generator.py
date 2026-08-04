from typing import Dict, List, Any
from datetime import datetime, timedelta

class AdaptiveRoadmapGenerator:
    """
    LLM & Context-Aware Adaptive Roadmap Engine.
    Generates personalized daily plans, weekly milestones, and adaptive updates
    by synthesizing ML behavioral predictor scores, GNN graph topology, and meta-clusters.
    """
    
    @staticmethod
    def generate_roadmap(user_id: str, meta_cluster: Dict[str, Any], 
                         scores: Dict[str, float], goals: List[str]) -> Dict[str, Any]:
        """
        Synthesizes structured adaptive roadmap tailored to user behavior.
        """
        cluster_name = meta_cluster.get("meta_cluster_name", "Adaptive Learner")
        prod_score = scores.get("productivity_score", 75.0)
        cons_score = scores.get("consistency_score", 70.0)
        
        # Primary goal or default
        primary_goal = goals[0] if goals else "Software Engineering Mastery"
        
        # Adaptation reasoning logic based on behavioral predictors
        if prod_score >= 80.0:
            adaptation_msg = f"Accelerating pace due to High Productivity ({prod_score}%). Advanced milestones unlocked."
            task_duration = 90
        elif prod_score <= 50.0:
            adaptation_msg = f"Adjusting load to build consistency. Focus split into 30-minute micro-tasks."
            task_duration = 30
        else:
            adaptation_msg = f"Balanced adaptive plan aligned with {cluster_name} cluster profile."
            task_duration = 60

        daily_plan = [
            {
                "title": f"Core Focus: {primary_goal} Deep Work",
                "description": f"Dedicated focus block based on {cluster_name} profile.",
                "estimated_minutes": task_duration,
                "category": "Deep Work",
                "completed": False
            },
            {
                "title": "VS Code Hands-On Practice",
                "description": "Active coding session to reinforce skills.",
                "estimated_minutes": 45,
                "category": "Coding",
                "completed": False
            },
            {
                "title": "Curated Knowledge Consumption",
                "description": "Educational reading or targeted YouTube tech tutorials.",
                "estimated_minutes": 30,
                "category": "Learning",
                "completed": False
            }
        ]

        weekly_plan = [
            {
                "title": f"Milestone 1: {primary_goal} Architecture",
                "description": "Design core modules and complete practical implementation.",
                "estimated_minutes": 240,
                "category": "Milestone",
                "completed": False
            },
            {
                "title": "Behavioral Review & Roadmap Recalibration",
                "description": "Weekly reflection on productivity & consistency trends.",
                "estimated_minutes": 30,
                "category": "Review",
                "completed": False
            }
        ]

        target_date_str = (datetime.utcnow() + timedelta(days=14)).strftime("%Y-%m-%d")
        milestones = [
            {
                "title": f"Phase 1 Completion: {primary_goal}",
                "target_date": target_date_str,
                "deliverable": f"Working project prototype for {primary_goal}",
                "tasks": daily_plan
            }
        ]

        return {
            "user_id": user_id,
            "meta_cluster_name": cluster_name,
            "productivity_score": prod_score,
            "consistency_score": cons_score,
            "daily_plan": daily_plan,
            "weekly_plan": weekly_plan,
            "milestones": milestones,
            "adaptation_reason": adaptation_msg,
            "generated_at": datetime.utcnow().isoformat()
        }
