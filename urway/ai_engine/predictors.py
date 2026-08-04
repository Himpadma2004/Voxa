import numpy as np
from typing import Dict, Any

class BehavioralPredictorEngine:
    """
    Machine Learning Predictor Engine for U'rWay.
    Calculates Productivity Score, Consistency Index, and Goal Completion Likelihood.
    """
    
    @staticmethod
    def predict_scores(browser_vec: np.ndarray, coding_vec: np.ndarray, youtube_vec: np.ndarray) -> Dict[str, float]:
        """
        Computes weighted machine learning productivity and consistency forecast metrics.
        """
        # Feature extraction
        prod_ratio = browser_vec[2]      # [0, 1]
        coding_hours = coding_vec[0]     # hours
        focus_intensity = coding_vec[2]  # [0, 1]
        edu_ratio = youtube_vec[2]       # [0, 1]
        
        # Weighted productivity score [0 - 100]
        raw_productivity = (prod_ratio * 0.40 + focus_intensity * 0.40 + edu_ratio * 0.20) * 100.0
        productivity_score = float(np.clip(raw_productivity, 10.0, 99.0))
        
        # Weighted consistency index [0 - 100] based on volume & ratio stability
        coding_boost = min(30.0, coding_hours * 6.0)
        raw_consistency = (prod_ratio * 0.50 + edu_ratio * 0.20) * 70.0 + coding_boost
        consistency_score = float(np.clip(raw_consistency, 15.0, 98.0))
        
        # Goal completion forecast probability [0.0 - 1.0]
        completion_prob = float(np.clip((productivity_score * 0.6 + consistency_score * 0.4) / 100.0, 0.15, 0.95))
        
        return {
            "productivity_score": round(productivity_score, 1),
            "consistency_score": round(consistency_score, 1),
            "goal_completion_probability": round(completion_prob, 2)
        }
