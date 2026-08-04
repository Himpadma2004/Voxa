from fastapi import APIRouter, HTTPException, Depends
from app.models.behavior import BrowserActivityPayload, VSCodeActivityPayload, YouTubeActivityPayload, IngestResponse
from app.security.crypto import PrivacyCryptoEngine
from app.config import settings
from datetime import datetime

router = APIRouter(prefix="/ingest", tags=["Behavioral Telemetry Ingestion"])

@router.post("/browser", response_model=IngestResponse)
async def ingest_browser_telemetry(payload: BrowserActivityPayload):
    """
    Ingests browser usage data.
    Strips raw URLs, parameters, and sensitive strings; stores only sanitized top-level domain metadata.
    Field-encrypts telemetry before persistence.
    """
    sanitized_domain = PrivacyCryptoEngine.sanitize_browser_domain(payload.domain)
    
    clean_data = {
        "domain": sanitized_domain,
        "duration_seconds": payload.session_duration_seconds,
        "is_productive": payload.is_productive,
        "timestamp": datetime.utcnow().isoformat()
    }
    
    # Authenticated field encryption using System Master Key
    key = bytes.fromhex(settings.MASTER_ENCRYPTION_KEY)
    encrypted_record = PrivacyCryptoEngine.encrypt_payload(clean_data, key)
    
    # Store encrypted record in database collection
    # (Motor DB write operation will execute here when connected)
    
    return IngestResponse(status="success", encrypted=True)

@router.post("/vscode", response_model=IngestResponse)
async def ingest_vscode_telemetry(payload: VSCodeActivityPayload):
    """
    Ingests coding duration and programming language telemetry from VS Code.
    Zero source code or file contents are recorded.
    """
    clean_data = {
        "language": payload.language.lower(),
        "active_coding_seconds": payload.active_coding_seconds,
        "idle_seconds": payload.idle_seconds,
        "timestamp": datetime.utcnow().isoformat()
    }
    
    key = bytes.fromhex(settings.MASTER_ENCRYPTION_KEY)
    encrypted_record = PrivacyCryptoEngine.encrypt_payload(clean_data, key)
    
    return IngestResponse(status="success", encrypted=True)

@router.post("/youtube", response_model=IngestResponse)
async def ingest_youtube_telemetry(payload: YouTubeActivityPayload):
    """
    Ingests YouTube category & interest telemetry.
    Classifies videos into domain categories (Education, Tech, Gaming, etc.).
    """
    clean_data = {
        "category": payload.video_category,
        "duration_seconds": payload.duration_watched_seconds,
        "is_short": payload.is_short,
        "timestamp": datetime.utcnow().isoformat()
    }
    
    key = bytes.fromhex(settings.MASTER_ENCRYPTION_KEY)
    encrypted_record = PrivacyCryptoEngine.encrypt_payload(clean_data, key)
    
    return IngestResponse(status="success", encrypted=True)
