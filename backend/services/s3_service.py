import boto3
import os
import uuid
from dotenv import load_dotenv

load_dotenv()

AWS_ACCESS_KEY_ID = os.getenv(
    "AWS_ACCESS_KEY_ID"
)

AWS_SECRET_ACCESS_KEY = os.getenv(
    "AWS_SECRET_ACCESS_KEY"
)

AWS_BUCKET_NAME = os.getenv(
    "AWS_BUCKET_NAME"
)

AWS_REGION = os.getenv(
    "AWS_REGION"
)

s3_client = boto3.client(
    "s3",
    aws_access_key_id=AWS_ACCESS_KEY_ID,
    aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
    region_name=AWS_REGION
)


def upload_file(file_path):

    extension = os.path.splitext(
        file_path
    )[1]

    unique_name = (
        f"{uuid.uuid4()}{extension}"
    )

    s3_key = (
        f"audio/{unique_name}"
    )

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

    return {
        "s3_key": s3_key,
        "audio_url": audio_url
    }