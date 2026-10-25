
import boto3
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from mangum import Mangum

app = FastAPI()
dynamodb = boto3.client('dynamodb', region_name='us-east-1')
TABLE_NAME = 'DeviceState'

class StatePayload(BaseModel):
    oled_text: str
    led_red: bool
    led_yellow: bool
    led_green: bool

@app.get("/state/{device_id}")
def get_state(device_id: str):
    response = dynamodb.get_item(
        TableName=TABLE_NAME,
        Key={'device_id': {'S': device_id}}
    )
    if 'Item' not in response:
        raise HTTPException(status_code=404, detail="Device not found")
    return response['Item']

@app.post("/state/{device_id}")
def update_state(device_id: str, payload: StatePayload):
    dynamodb.put_item(
        TableName=TABLE_NAME,
        Item={
            'device_id': {'S': device_id},
            'oled_text': {'S': payload.oled_text},
            'led_red': {'BOOL': payload.led_red},
            'led_yellow': {'BOOL': payload.led_yellow},
            'led_green': {'BOOL': payload.led_green}
        }
    )
    return {"status": "success"}

# Wrap the FastAPI app for AWS Lambda execution
handler = Mangum(app)