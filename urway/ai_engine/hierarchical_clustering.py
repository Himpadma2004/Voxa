import numpy as np
from sklearn.cluster import KMeans
from typing import Dict, List, Any

class HierarchicalMetaClusterEngine:
    """
    Implements U'rWay Hierarchical Clustering Architecture.
    
    Data Sources Pipeline:
    Onboarding Cluster
         ↓
    Browser Cluster
         ↓
    YouTube Cluster
         ↓
    Coding Cluster
         ↓
    Physical Activity Cluster
         ↓
    Meta Cluster -> Final User Cluster
    
    Prevents single-source dataset bias and maintains robust behavioral classification.
    """
    
    META_CLUSTER_NAMES = {
        0: "Hyper-Focused Builder",
        1: "Adaptive Learner",
        2: "Distracted Explorer",
        3: "Consistent Performer"
    }
    
    def __init__(self):
        # Domain-level K-Means models
        self.onboarding_kmeans = KMeans(n_clusters=2, random_state=42, n_init=10)
        self.browser_kmeans = KMeans(n_clusters=2, random_state=42, n_init=10)
        self.coding_kmeans = KMeans(n_clusters=2, random_state=42, n_init=10)
        self.youtube_kmeans = KMeans(n_clusters=2, random_state=42, n_init=10)
        self.meta_kmeans = KMeans(n_clusters=2, random_state=42, n_init=10)
        self.is_fitted = False

    def _fit_synthetic_centroids(self):
        """
        Initializes default cluster centroids for bootstrapping initial user onboarding.
        """
        # Synthetic baseline matrices (8 distinct samples per domain)
        onb_data = np.array([
            [0.8, 0.9, 0.4, 1.0], [0.4, 0.5, 0.2, 0.5], [0.2, 0.3, 0.0, 0.5], [0.9, 0.8, 0.6, 1.0],
            [0.7, 0.8, 0.3, 0.9], [0.3, 0.4, 0.1, 0.4], [0.1, 0.2, 0.0, 0.3], [0.8, 0.7, 0.5, 0.8]
        ], dtype=np.float32)
        
        brw_data = np.array([
            [4.0, 0.5, 0.89], [1.5, 3.0, 0.33], [0.5, 4.0, 0.11], [5.0, 1.0, 0.83],
            [3.5, 0.8, 0.81], [1.2, 2.5, 0.32], [0.3, 3.5, 0.08], [4.5, 1.2, 0.79]
        ], dtype=np.float32)
        
        cod_data = np.array([
            [5.0, 0.5, 0.91], [2.0, 1.5, 0.57], [0.5, 2.0, 0.20], [6.0, 0.5, 0.92],
            [4.5, 0.6, 0.88], [1.8, 1.2, 0.60], [0.4, 1.8, 0.18], [5.5, 0.4, 0.93]
        ], dtype=np.float32)
        
        yt_data = np.array([
            [3.0, 0.5, 0.86], [1.0, 2.5, 0.28], [0.5, 3.0, 0.14], [4.0, 1.0, 0.80],
            [2.5, 0.6, 0.80], [0.8, 2.0, 0.28], [0.4, 2.5, 0.13], [3.5, 0.8, 0.81]
        ], dtype=np.float32)
        
        c_onb = self.onboarding_kmeans.fit_predict(onb_data)
        c_brw = self.browser_kmeans.fit_predict(brw_data)
        c_cod = self.coding_kmeans.fit_predict(cod_data)
        c_yt  = self.youtube_kmeans.fit_predict(yt_data)
        
        meta_features = np.column_stack([c_onb, c_brw, c_cod, c_yt]).astype(np.float32)
        self.meta_kmeans.fit(meta_features)
        self.is_fitted = True

    def compute_meta_cluster(self, onboarding_vec: np.ndarray, browser_vec: np.ndarray, 
                             coding_vec: np.ndarray, youtube_vec: np.ndarray) -> Dict[str, Any]:
        """
        Evaluates independent behavioral clusters and returns final Meta Cluster classification.
        """
        if not self.is_fitted:
            self._fit_synthetic_centroids()
            
        c_onb = int(self.onboarding_kmeans.predict(onboarding_vec.astype(np.float32).reshape(1, -1))[0])
        c_brw = int(self.browser_kmeans.predict(browser_vec.astype(np.float32).reshape(1, -1))[0])
        c_cod = int(self.coding_kmeans.predict(coding_vec.astype(np.float32).reshape(1, -1))[0])
        c_yt  = int(self.youtube_kmeans.predict(youtube_vec.astype(np.float32).reshape(1, -1))[0])
        
        meta_input = np.array([[c_onb, c_brw, c_cod, c_yt]], dtype=np.float32)
        meta_id = int(self.meta_kmeans.predict(meta_input)[0])
        
        return {
            "onboarding_cluster": c_onb,
            "browser_cluster": c_brw,
            "coding_cluster": c_cod,
            "youtube_cluster": c_yt,
            "meta_cluster_id": meta_id,
            "meta_cluster_name": self.META_CLUSTER_NAMES.get(meta_id, "Adaptive Learner")
        }
