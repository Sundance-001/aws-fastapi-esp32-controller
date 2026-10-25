Serverless ESP32 IoT Hub
An end-to-end serverless IoT system that allows you to remotely control a physical ESP32 microcontroller from anywhere in the world using AWS cloud services.
Architecture Overview
This project consists of three main components:
Frontend Dashboard: A static HTML/JS web page hosted on AWS Amplify. It fetches and updates the device state via REST API calls.
Serverless Backend: An Amazon API Gateway routing traffic to an AWS Lambda function. The backend logic is built with Python and FastAPI (wrapped with Mangum), which reads/writes data to an Amazon DynamoDB table.
Hardware (ESP32): A C++ Arduino sketch running on an ESP32. It uses a non-blocking `millis()` timer to poll the API Gateway every 3 seconds and updates local hardware (LEDs and an SSD1306 OLED screen) based on the cloud state.
 Features
Real-time Remote Control: Toggle LEDs (Red, Yellow, Green) and update an OLED display remotely from a web dashboard.
100% Serverless Cloud: Utilizing AWS Lambda and DynamoDB means zero idle server costs and instant scaling.
Non-Blocking Firmware: The ESP32 utilizes a non-blocking HTTP polling mechanism, ensuring background Wi-Fi tasks are not interrupted.
RESTful API: Clean REST API endpoints built with Python and FastAPI.
 Tech Stack
Cloud & Backend:
AWS Lambda (Serverless compute)
Amazon API Gateway (HTTP API routing)
Amazon DynamoDB (NoSQL state storage)
Python 3.11 + FastAPI + Mangum
Frontend:
HTML5, CSS3, Vanilla JavaScript (Fetch API)
AWS Amplify (Static web hosting)
Hardware / Embedded:
ESP32 NodeMCU
Arduino IDE / C++
Adafruit SSD1306 (OLED Display)
ArduinoJson (JSON deserialization)
 Hardware Requirements
1x ESP32 Development Board
1x SSD1306 I2C OLED Display (128x64)
3x LEDs (Red, Yellow, Green)
3x 330Ω Resistors (for LEDs)
1 47-100 microfarad capacitor
Breadboard and Jumper Wires

⚙️ Setup Instructions
1. Cloud Backend Deployment
Create a DynamoDB table named `DeviceState` with a partition key `device_id` (String).
Package the `main.py` FastAPI app alongside its dependencies (`fastapi`, `mangum`, `pydantic`).
Deploy the ZIP package to an AWS Lambda function and attach an IAM role with DynamoDB access.
Create an API Gateway (HTTP) and route `GET /state/{device_id}` and `POST /state/{device_id}` to your Lambda function. Note: Ensure CORS is configured properly.
2. Frontend Deployment
Update `API_URL` in `index.html` with your API Gateway endpoint.
Zip the `index.html` file.
Deploy the zip file manually to AWS Amplify for instant public HTTPS hosting.
3. ESP32 Firmware
Open `oled-and-led-ctrl.ino` in the Arduino IDE.
Install necessary libraries: `Adafruit GFX`, `Adafruit SSD1306`, and `ArduinoJson`.
Update the `ssid`, `password`, and `api_url` variables at the top of the sketch.
Flash to your ESP32.
📝 License
This project is licensed under the MIT License.
