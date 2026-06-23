import boto3
import os
import uuid
import tempfile
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# AWS Configuration
AWS_ACCESS_KEY_ID = os.getenv("AWS_ACCESS_KEY_ID")
AWS_SECRET_ACCESS_KEY = os.getenv("AWS_SECRET_ACCESS_KEY")
AWS_BUCKET_NAME = os.getenv("AWS_BUCKET_NAME")
AWS_REGION = os.getenv("AWS_REGION")

# Create S3 Client
s3_client = boto3.client(
    "s3",
    aws_access_key_id=AWS_ACCESS_KEY_ID,
    aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
    region_name=AWS_REGION
)

def upload_file(file_path):
    """
    Upload a file to AWS S3 and return
    the S3 key and public URL.
    """
    extension = os.path.splitext(file_path)[1]
    unique_filename = f"{uuid.uuid4()}{extension}"
    s3_key = f"audio/{unique_filename}"

    try:
        s3_client.upload_file(
            file_path,
            AWS_BUCKET_NAME,
            s3_key
        )

        audio_url = (
            f"https://{AWS_BUCKET_NAME}"
            f".s3.{AWS_REGION}"
            f".amazonaws.com/{s3_key}"
        )

        print("✅ File uploaded to S3")
        return {
            "s3_key": s3_key,
            "audio_url": audio_url
        }

    except Exception as e:
        print(f"❌ S3 Upload Error: {e}")
        raise

def download_file(s3_key):
    """
    Download an audio file from S3
    into a temporary local file.
    """
    try:
        temp_dir = tempfile.gettempdir()
        local_path = os.path.join(
            temp_dir,
            f"{uuid.uuid4()}.wav"
        )

        s3_client.download_file(
            AWS_BUCKET_NAME,
            s3_key,
            local_path
        )

        print(f"📥 Downloaded from S3: {local_path}")
        return local_path

    except Exception as e:
        print(f"❌ S3 Download Error: {e}")
        raise

def delete_file(s3_key):
    """
    Optional utility to delete
    an object from S3.
    """
    try:
        s3_client.delete_object(
            Bucket=AWS_BUCKET_NAME,
            Key=s3_key
        )
        print(f"🗑️ Deleted S3 Object: {s3_key}")

    except Exception as e:
        print(f"❌ Delete Error: {e}")
        raise