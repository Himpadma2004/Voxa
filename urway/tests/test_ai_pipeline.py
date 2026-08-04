import unittest
import numpy as np
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from backend.app.security.crypto import PrivacyCryptoEngine
from ai_engine.feature_engineering import FeatureEngineeringPipeline
from ai_engine.hierarchical_clustering import HierarchicalMetaClusterEngine
from ai_engine.predictors import BehavioralPredictorEngine
from ai_engine.gnn_graph_builder import UserBehaviorGraphBuilder
from ai_engine.roadmap_generator import AdaptiveRoadmapGenerator

class TestUrWayAIPipeline(unittest.TestCase):
    
    def test_crypto_engine(self):
        key, salt = PrivacyCryptoEngine.derive_user_key("user_secret_passphrase")
        self.assertEqual(len(key), 32)
        
        sample_payload = {"domain": "github.com", "duration_seconds": 1800, "is_productive": True}
        encrypted = PrivacyCryptoEngine.encrypt_payload(sample_payload, key)
        self.assertIn("ciphertext", encrypted)
        self.assertIn("nonce", encrypted)
        self.assertIn("tag", encrypted)
        
        decrypted = PrivacyCryptoEngine.decrypt_payload(encrypted, key)
        self.assertEqual(decrypted["domain"], "github.com")
        self.assertEqual(decrypted["duration_seconds"], 1800)

    def test_domain_sanitizer(self):
        sanitized = PrivacyCryptoEngine.sanitize_browser_domain("https://github.com/user/repo?token=123")
        self.assertEqual(sanitized, "github.com")

    def test_feature_engineering(self):
        onb_vec = FeatureEngineeringPipeline.extract_onboarding_features({
            "user_category": "Developers",
            "target_daily_hours": 8.0,
            "self_rated_discipline": 8,
            "focus_time_preference": "Morning"
        })
        self.assertEqual(len(onb_vec), 4)

    def test_hierarchical_clustering(self):
        engine = HierarchicalMetaClusterEngine()
        onb_vec = np.array([0.5, 0.8, 0.4, 1.0], dtype=np.float32)
        brw_vec = np.array([4.0, 0.5, 0.89], dtype=np.float32)
        cod_vec = np.array([5.0, 0.5, 0.91], dtype=np.float32)
        yt_vec  = np.array([3.0, 0.5, 0.86], dtype=np.float32)
        
        cluster_info = engine.compute_meta_cluster(onb_vec, brw_vec, cod_vec, yt_vec)
        self.assertIn("meta_cluster_id", cluster_info)
        self.assertIn("meta_cluster_name", cluster_info)

    def test_behavioral_predictors(self):
        brw_vec = np.array([4.0, 0.5, 0.89], dtype=np.float32)
        cod_vec = np.array([5.0, 0.5, 0.91], dtype=np.float32)
        yt_vec  = np.array([3.0, 0.5, 0.86], dtype=np.float32)
        
        scores = BehavioralPredictorEngine.predict_scores(brw_vec, cod_vec, yt_vec)
        self.assertGreater(scores["productivity_score"], 50.0)
        self.assertGreater(scores["consistency_score"], 50.0)

    def test_gnn_builder(self):
        graph = UserBehaviorGraphBuilder.build_user_graph(
            user_id="test_user_1",
            user_category="Developer",
            goals=["PyTorch Mastery", "System Design"],
            top_domains=["github.com", "arxiv.org"],
            coding_languages=["python"],
            youtube_topics=["Technology"]
        )
        self.assertGreater(graph["node_count"], 4)
        self.assertGreater(graph["edge_count"], 3)

    def test_adaptive_roadmap_generator(self):
        roadmap = AdaptiveRoadmapGenerator.generate_roadmap(
            user_id="test_user_1",
            meta_cluster={"meta_cluster_name": "Hyper-Focused Builder"},
            scores={"productivity_score": 85.0, "consistency_score": 80.0},
            goals=["PyTorch Mastery"]
        )
        self.assertEqual(len(roadmap["daily_plan"]), 3)
        self.assertIn("Hyper-Focused Builder", roadmap["meta_cluster_name"])

if __name__ == "__main__":
    unittest.main()
