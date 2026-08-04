import networkx as nx
from typing import Dict, List, Any

class UserBehaviorGraphBuilder:
    """
    Constructs Graph Neural Network topology representation for U'rWay.
    
    Nodes:
    - User
    - Goals
    - Habits
    - Browser Behavior
    - Coding Activity
    - YouTube Interests
    - Skills
    
    Edges represent semantic & temporal relationships between behavioral entities.
    """
    
    @staticmethod
    def build_user_graph(user_id: str, user_category: str, goals: List[str], 
                         top_domains: List[str], coding_languages: List[str], 
                         youtube_topics: List[str]) -> Dict[str, Any]:
        """
        Constructs a NetworkX directed behavioral graph and returns serialized node/edge structure.
        """
        G = nx.DiGraph()
        
        # 1. Add Core User Node
        user_node_id = f"user_{user_id}"
        G.add_node(user_node_id, label="User", category=user_category)
        
        # 2. Add Goal & Skill Nodes
        for goal in goals:
            g_id = f"goal_{goal.lower().replace(' ', '_')}"
            G.add_node(g_id, label="Goal", name=goal)
            G.add_edge(user_node_id, g_id, relation="PURSUES")
            
        # 3. Add Coding Language & Activity Nodes
        for lang in coding_languages:
            l_id = f"skill_{lang.lower()}"
            G.add_node(l_id, label="Skill", name=lang)
            G.add_edge(user_node_id, l_id, relation="PRACTICES")
            
        # 4. Add Browser Domain Nodes
        for dom in top_domains:
            d_id = f"domain_{dom.replace('.', '_')}"
            G.add_node(d_id, label="BrowserActivity", domain=dom)
            G.add_edge(user_node_id, d_id, relation="VISITS")
            
        # 5. Add YouTube Interest Nodes
        for topic in youtube_topics:
            t_id = f"yt_{topic.lower()}"
            G.add_node(t_id, label="YouTubeInterest", topic=topic)
            G.add_edge(user_node_id, t_id, relation="CONSUMES")
            
        # Serialize node & edge counts for AI pipeline consumption
        nodes_data = [{"id": n, "data": G.nodes[n]} for n in G.nodes]
        edges_data = [{"source": u, "target": v, "relation": G.edges[u, v]["relation"]} for u, v in G.edges]
        
        return {
            "node_count": len(G.nodes),
            "edge_count": len(G.edges),
            "nodes": nodes_data,
            "edges": edges_data
        }
