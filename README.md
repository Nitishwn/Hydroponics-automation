# Hydroponics Helper AI

## What's This All About?

Ever wish your hydroponics setup could give you a heads-up when things aren't quite right? That's the idea behind this project! It's a small web server, built with Python and Flask, that connects to the Gemini AI. You feed it sensor data (pH, TDS, temp, humidity) from your system, and it gives you back practical advice or answers your specific hydroponics questions, all thanks to the AI's knowledge.

Think of it as having a little plant and water expert on call!

## Cool Things It Does

* **AI-Powered Recommendations:** Instead of just raw numbers, you get actual recommendations from the AI based on your sensor readings. Is the pH too high? Is the temperature off? It'll try to give you concise, actionable steps.
* **Ask It Anything (Hydroponics-Related!):** Got a specific question, like "What are the signs of nutrient lockout?" You can ask the server, providing your current sensor readings for context, and the AI will give you a targeted answer.
* **Simple Server Setup:** It uses Flask, a popular and relatively easy-to-use Python framework for web stuff.
* **CORS Enabled:** CORS is enabled, meaning you can easily hook this up to a frontend dashboard or app running on a different address.
* **Clean Responses:** The AI can sometimes be a bit... verbose with formatting. This server cleans up the text (removing markdown like asterisks or bullet points) so you get straightforward advice.

## Requirements
* Python 3.x
* Flask (`pip install Flask`)
* Requests (`pip install requests`)
* Flask-CORS (`pip install Flask-Cors`)
* A Gemini API Key


## Installation

1.  **Clone the repository (or download the files).**
2.  **Install dependencies:**
    ```bash
    pip install Flask requests Flask-Cors
    ```
3.  **Configure API Key:**
    * Open `ai_server.py`.
    * Replace `"***************************"` with your actual Gemini API key.
    ```python
    # Gemini API configuration
    GEMINI_API_KEY = "YOUR_ACTUAL_GEMINI_API_KEY" # <-- Add your key here
    GEMINI_API_URL = "[https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent](https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent)"
    ```

## How to Use It

1.  **Start the Server:** In your terminal, navigate to where you saved the files and run:
    ```bash
    python ai_server.py
    ```
    You should see messages indicating it's running, likely on `http://localhost:5000`.

2.  **Talk to the Server:** Now you can send requests to it! You can use tools like `curl`, Postman, or build it into your own hydroponics dashboard.

## Talking to the API (Endpoints)

Here's how you communicate with the server:

* **Get Recommendations (`POST /analyze`)**
    * **What it does:** Sends your sensor data, gets general advice back.
    * **How to send data:** Send a `POST` request with JSON in the body, like this:
        ```json
        {
          "ph": 6.5,
          "tds": 850,
          "temperature": 22.5,
          "humidity": 60.0
        }
        ```
        *(Don't worry if you miss one, it uses default values)*
    * **What you get back (Success):**
        ```json
        {
          "recommendation": "Your pH is looking good, but the TDS is a bit high for lettuce. Consider diluting your nutrient solution slightly."
        }
        ```
    * **What you get back (If something's wrong):**
        ```json
        {
          "error": "Something went wrong, couldn't get advice."
        }
        ```
* **Ask a Specific Question (`GET /search`)**
    * **What it does:** Asks the AI about a specific term (like "root rot") using your current sensor readings as context.
    * **How to send data:** Send a `GET` request using query parameters in the URL. `term` is needed, the others ( `ph`, `tds`, `temp`, `hum`) are optional but helpful for context.
    * **Example URL:**
        `http://localhost:5000/search?term=root%20rot&ph=5.1&tds=900&temp=26&hum=70`
    * **What you get back (Success):**
        ```json
        {
          "result": "Root rot risk increases with warmer temperatures like 26°C and lower pH (5.1). Ensure good aeration in your reservoir..."
        }
        ```
    * **What you get back (If something's wrong):**
        ```json
        {
          "error": "Couldn't find info on that topic."
        }
        ```
* **Check if it's Alive (`GET /test`)**
    * **What it does:** Just confirms the server is up and running.
    * **Example URL:** `http://localhost:5000/test`
    * **What you get back:**
        ```json
        {
          "status": "Server is running",
          "message": "API is operational"
        }
        ```
       

## Quick Example: Using it from JavaScript

Here's a basic idea of how you might call the `/analyze` endpoint from some JavaScript code running in a browser or Node.js (using the example sensor data):

```javascript
const sensorData = {
  ph: 7.5,
  tds: 420,
  temperature: 32,
  humidity: 55
};

fetch("http://localhost:5000/analyze", { // Make sure this matches the running server endpoint!
  method: "POST",
  headers: {
    "Content-Type": "application/json"
  },
  body: JSON.stringify(sensorData) // Send the data!
})
.then(response => response.json())
.then(data => {
  if (data.recommendation) {
    console.log("AI Says:", data.recommendation);
    // You could display this on a webpage here!
    // e.g., document.getElementById('ai-advice').textContent = data.recommendation;
  } else {
    console.error("Error:", data.error);
  }
})
.catch(error => {
  console.error("Fetch Error:", error);
});