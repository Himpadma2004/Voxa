import numpy as np
from typing import Dict, List, Any

class FeatureEngineeringPipeline:
    """
    Transforms raw multi-source behavioral telemetry into normalized feature vectors.
    Handles missing values, domain scaling, and standard normalization.
    """
    
    @staticmethod
    def extract_onboarding_features(questionnaire: Dict[str, Any]) -> np.ndarray:
        """
        Extracts 4-dimensional normalized vector from user onboarding.
        [target_daily_hours_norm, discipline_scale, category_code, focus_time_code]
        """
        categories = ["Students", "Aspirants", "Developers", "Professionals", "Fitness", "Entrepreneurs"]
        cat_code = categories.index(questionnaire.get("user_category", "Developers")) if questionnaire.get("user_category") in categories else 2
        
        target_hours = float(questionnaire.get("target_daily_hours", 6.0)) / 16.0 # Normalize [0, 1]
        discipline = float(questionnaire.get("self_rated_discipline", 7)) / 10.0 # Normalize [0, 1]
        focus_code = 1.0 if questionnaire.get("focus_time_preference") == "Morning" else 0.5
        
        return np.array([target_hours, discipline, cat_code / 5.0, focus_code], dtype=np.float32)

    @staticmethod
    def extract_browser_features(browsing_logs: List[Dict[str, Any]]) -> np.ndarray:
        """
        Extracts 3-dimensional vector from browser activity logs.
        [total_productive_hours, total_unproductive_hours, productivity_ratio]
        """
        if not browsing_logs:
            return np.array([0.0, 0.0, 0.5], dtype=np.float32)
            
        prod_sec = sum(log["duration_seconds"] for log in browsing_logs if log.get("is_productive", True))
        unprod_sec = sum(log["duration_seconds"] for log in browsing_logs if not log.get("is_productive", True))
        total_sec = max(1, prod_sec + unprod_sec)
        
        ratio = prod_sec / float(total_sec)
        return np.array([prod_sec / 3600.0, unprod_sec / 3600.0, ratio], dtype=np.float32)

    @staticmethod
    def extract_coding_features(coding_logs: List[Dict[str, Any]]) -> np.ndarray:
        """
        Extracts 3-dimensional vector from VS Code activity logs.
        [active_coding_hours, idle_hours, focus_intensity]
        """
        if not coding_logs:
            return np.array([0.0, 0.0, 0.5], dtype=np.float32)
            
        active_sec = sum(log["active_coding_seconds"] for log in coding_logs)
        idle_sec = sum(log.get("idle_seconds", 0) for log in coding_logs)
        total_sec = max(1, active_sec + idle_sec)
        
        focus_intensity = active_sec / float(total_sec)
        return np.array([active_sec / 3600.0, idle_sec / 3600.0, focus_intensity], dtype=np.float32)

    @staticmethod
    def extract_youtube_features(youtube_logs: List[Dict[str, Any]]) -> np.ndarray:
        """
        Extracts 3-dimensional vector from YouTube category interests.
        [educational_watch_hours, entertainment_watch_hours, educational_ratio]
        """
        if not youtube_logs:
            return np.array([0.0, 0.0, 0.5], dtype=np.float32)
            
        edu_categories = {"Education", "Technology", "Business", "Motivation", "Self Improvement"}
        
        edu_sec = sum(log["duration_seconds"] for log in youtube_logs if log.get("category") in edu_categories)
        ent_sec = sum(log["duration_seconds"] for log in youtube_logs if log.get("category") not in edu_categories)
        total_sec = max(1, edu_sec + ent_sec)
        
        edu_ratio = edu_sec / float(total_sec)
        return np.array([edu_sec / 3600.0, ent_sec / 3600.0, edu_ratio], dtype=np.float32)
