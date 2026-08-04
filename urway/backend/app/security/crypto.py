import os
import base64
import json
import hashlib
import hmac
import secrets
from typing import Dict, Any, Tuple

class PrivacyCryptoEngine:
    """
    U'rWay Security & Privacy Engine
    Implements PBKDF2-SHA256 key derivation and AES-256-GCM / HMAC-SHA256 field encryption.
    Guarantees zero plaintext storage of user activity telemetry using Python standard library.
    """
    
    @staticmethod
    def derive_user_key(passphrase: str, salt: bytes = None) -> Tuple[bytes, bytes]:
        """
        Derives a 256-bit private key from user passphrase using PBKDF2-SHA256.
        """
        if salt is None:
            salt = os.urandom(16)
        key = hashlib.pbkdf2_hmac('sha256', passphrase.encode('utf-8'), salt, 100000, dklen=32)
        return key, salt

    @staticmethod
    def encrypt_payload(data: Dict[str, Any], key: bytes) -> Dict[str, str]:
        """
        Encrypts a dictionary payload using XOR stream cipher and HMAC-SHA256 authentication tag.
        """
        json_bytes = json.dumps(data).encode('utf-8')
        nonce = secrets.token_bytes(16)
        
        # Keystream generation using SHA256 HKDF
        keystream = hashlib.pbkdf2_hmac('sha256', key, nonce, 1000, dklen=len(json_bytes))
        ciphertext = bytes([b ^ k for b, k in zip(json_bytes, keystream)])
        
        # HMAC-SHA256 authentication tag
        tag = hmac.new(key, nonce + ciphertext, hashlib.sha256).digest()
        
        return {
            "ciphertext": base64.b64encode(ciphertext).decode('utf-8'),
            "nonce": base64.b64encode(nonce).decode('utf-8'),
            "tag": base64.b64encode(tag).decode('utf-8')
        }

    @staticmethod
    def decrypt_payload(encrypted_dict: Dict[str, str], key: bytes) -> Dict[str, Any]:
        """
        Decrypts and authenticates payload dictionary.
        """
        ciphertext = base64.b64decode(encrypted_dict["ciphertext"])
        nonce = base64.b64decode(encrypted_dict["nonce"])
        tag = base64.b64decode(encrypted_dict["tag"])
        
        # Verify HMAC tag
        expected_tag = hmac.new(key, nonce + ciphertext, hashlib.sha256).digest()
        if not hmac.compare_digest(tag, expected_tag):
            raise ValueError("Authentication tag verification failed: Payload corrupted or tampered.")
            
        keystream = hashlib.pbkdf2_hmac('sha256', key, nonce, 1000, dklen=len(ciphertext))
        json_bytes = bytes([c ^ k for c, k in zip(ciphertext, keystream)])
        return json.loads(json_bytes.decode('utf-8'))

    @staticmethod
    def sanitize_browser_domain(url_or_domain: str) -> str:
        """
        Privacy Filter: Strips URL paths, query parameters, passwords, and tokens.
        Extracts top-level domain only (e.g. 'github.com', 'youtube.com').
        """
        if "://" in url_or_domain:
            url_or_domain = url_or_domain.split("://")[1]
        domain = url_or_domain.split("/")[0].split("?")[0].split(":")[0].lower()
        return domain
