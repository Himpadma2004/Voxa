from fastapi import APIRouter, HTTPException
from typing import List, Optional
from app.models.roadmap import AdaptiveRoadmapResponse
from app.models.user import OnboardingQuestionnaire
from ai_engine.feature_engineering import FeatureEngineeringPipeline
from ai_engine.hierarchical_clustering import HierarchicalMetaClusterEngine
from ai_engine.predictors import BehavioralPredictorEngine
from ai_engine.gnn_graph_builder import UserBehaviorGraphBuilder
from ai_engine.roadmap_generator import AdaptiveRoadmapGenerator

router = APIRouter(prefix="/roadmap", tags=["Adaptive Roadmap AI Engine"])

cluster_engine = HierarchicalMetaClusterEngine()

@router.post("/generate", response_model=AdaptiveRoadmapResponse)
async def generate_user_roadmap(user_id: str, onboarding: OnboardingQuestionnaire):
    """
    Executes the full U'rWay AI Pipeline:
    1. Feature Engineering (Onboarding, Browser, Coding, YouTube vectors)
    2. Hierarchical Meta-Clustering (Domain clusters -> Meta cluster)
    3. Behavioral ML Scoring (Productivity & Consistency)
    4. GNN Graph Topology Construction
    5. Adaptive LLM Roadmap Generation
    """
    # 1. Feature Engineering
    onb_dict = onboarding.model_dump()
    onb_vec = FeatureEngineeringPipeline.extract_onboarding_features(onb_dict)
    
    # Baseline telemetry vectors (until real extension logs accumulate)
    brw_vec = FeatureEngineeringPipeline.extract_browser_features([{"duration_seconds": 14400, "is_productive": True}])
    cod_vec = FeatureEngineeringPipeline.extract_coding_features([{"active_coding_seconds": 10800, "idle_seconds": 1200}])
    yt_vec = FeatureEngineeringPipeline.extract_youtube_features([{"duration_seconds": 3600, "category": "Technology"}])
    
    # 2. Hierarchical Meta-Clustering
    meta_cluster = cluster_engine.compute_meta_cluster(onb_vec, brw_vec, cod_vec, yt_vec)
    
    # 3. Behavioral Predictor Scores
    scores = BehavioralPredictorEngine.predict_scores(brw_vec, cod_vec, yt_vec)
    
    # 4. GNN Graph Topology Construction
    graph = UserBehaviorGraphBuilder.build_user_graph(
        user_id=user_id,
        user_category=onboarding.user_category,
        goals=onboarding.primary_goals,
        top_domains=["github.com", "leetcode.com", "arxiv.org"],
        coding_languages=["python", "typescript"],
        youtube_topics=["Technology", "Education"]
    )
    
    # 5. Context-Aware Adaptive Roadmap Generation
    roadmap = AdaptiveRoadmapGenerator.generate_roadmap(
        user_id=user_id,
        meta_cluster=meta_cluster,
        scores=scores,
        goals=onboarding.primary_goals
    )
    
    return roadmap
