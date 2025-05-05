from flask import Flask, request, jsonify
import requests
import json
import re
import logging
from flask_cors import CORS

# Set up logging
logging.basicConfig(level=logging.DEBUG, 
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app)  

# Gemini API configuration
GEMINI_API_KEY = "***************************"
GEMINI_API_URL = "***********************"

def clean_text(text):
    """Remove markdown formatting and clean up the text."""
    if not text:
        return ""
    
    text = re.sub(r'\*+', '', text)
    
    text = re.sub(r'^\s*[-•]\s*', '', text, flags=re.MULTILINE)
    # Remove numbering
    text = re.sub(r'^\s*\d+\.\s*', '', text, flags=re.MULTILINE)
    # Remove hash marks (headers)
    text = re.sub(r'^\s*#+\s*', '', text, flags=re.MULTILINE)
    # Remove excessive newlines
    text = re.sub(r'\n{3,}', '\n\n', text)
    # Trim whitespace
    text = text.strip()
    return text

@app.route('/analyze', methods=['POST'])
def analyze():
    try:
        # Log received request
        logger.debug("Received analyze request")
        
        # Get request data
        sensor_data = request.get_json(silent=True)
        if not sensor_data:
            logger.error("No JSON data received or invalid JSON")
            return jsonify({"error": "No data received or invalid JSON format"}), 400
            
        logger.debug(f"Received data: {sensor_data}")
        
        # Handle missing fields with defaults
        ph = float(sensor_data.get('ph', 7.0))
        tds = float(sensor_data.get('tds', 0.0))
        temperature = float(sensor_data.get('temperature', 25.0)) 
        humidity = float(sensor_data.get('humidity', 50.0))
        
        # Create prompt for Gemini API
        prompt = f"As a water quality and plant health expert, provide a concise practical recommendation based on these sensor readings: "
        prompt += f"pH: {ph:.2f}, "
        prompt += f"TDS: {tds:.2f} ppm, "
        prompt += f"Temperature: {temperature:.2f}°C, "
        prompt += f"Humidity: {humidity:.2f}%. "
        prompt += "Focus on immediate actions needed for optimal water quality and plant health. Keep response under 150 words. Do not use any asterisks, bullet points, or markdown formatting in your response."
        
        logger.debug(f"Sending prompt to Gemini API: {prompt}")
        
        # Prepare request to Gemini API
        headers = {"Content-Type": "application/json"}
        payload = {
            "contents": [{
                "parts": [{"text": prompt}]
            }]
        }
        
        # Make request to Gemini API
        try:
            logger.debug("Making request to Gemini API...")
            response = requests.post(
                f"{GEMINI_API_URL}?key={GEMINI_API_KEY}",
                headers=headers,
                json=payload,
                timeout=15  # Add timeout to prevent hanging
            )
            response.raise_for_status()  # Raise exception for HTTP errors
            
            logger.debug(f"Received response from Gemini API: Status {response.status_code}")
            response_data = response.json()
            logger.debug(f"Response data: {json.dumps(response_data)[:500]}...")  # Log first 500 chars
            
            if 'candidates' in response_data and len(response_data['candidates']) > 0:
                if 'content' in response_data['candidates'][0]:
                    content = response_data['candidates'][0]['content']
                    if 'parts' in content and len(content['parts']) > 0:
                        recommendation = content['parts'][0]['text']
                        # Clean the recommendation text
                        recommendation = clean_text(recommendation)
                        logger.debug(f"Processed recommendation: {recommendation[:100]}...")  # Log first 100 chars
                        return jsonify({"recommendation": recommendation})
            
            logger.error("Unexpected API response format")
            return jsonify({"error": "No recommendation generated from API"}), 500
            
        except requests.exceptions.RequestException as e:
            logger.error(f"Request to Gemini API failed: {str(e)}")
            return jsonify({"error": f"API request error: {str(e)}"}), 500
            
    except Exception as e:
        logger.error(f"Unexpected error in analyze endpoint: {str(e)}", exc_info=True)
        return jsonify({"error": str(e)}), 500

@app.route('/search', methods=['GET'])
def search():
    try:
        # Log received request
        logger.debug("Received search request")
        
        search_term = request.args.get('term', '')
        logger.debug(f"Search term: {search_term}")
        
        if not search_term:
            return jsonify({"error": "No search term provided"}), 400
        
        # Get sensor values, defaulting if not provided
        try:
            ph = float(request.args.get('ph', 7.0))
            tds = float(request.args.get('tds', 0.0))
            temp = float(request.args.get('temp', 25.0))
            hum = float(request.args.get('hum', 50.0))
        except ValueError as e:
            logger.error(f"Invalid sensor value: {str(e)}")
            return jsonify({"error": f"Invalid sensor value: {str(e)}"}), 400
        
        # Create prompt for Gemini API
        prompt = f"As a water quality and plant health expert, provide specific information about '{search_term}' "
        prompt += f"in the context of current sensor readings: "
        prompt += f"pH: {ph:.2f}, "
        prompt += f"TDS: {tds:.2f} ppm, "
        prompt += f"Temperature: {temp:.2f}°C, "
        prompt += f"Humidity: {hum:.2f}%. "
        prompt += "Focus on practical advice related to the search term. Keep response under 200 words. Do not use any asterisks, bullet points, or markdown formatting in your response."
        
        logger.debug(f"Sending prompt to Gemini API: {prompt}")
        
        # Prepare request to Gemini API
        headers = {"Content-Type": "application/json"}
        payload = {
            "contents": [{
                "parts": [{"text": prompt}]
            }]
        }
        
        # Make request to Gemini API
        try:
            response = requests.post(
                f"{GEMINI_API_URL}?key={GEMINI_API_KEY}",
                headers=headers,
                json=payload,
                timeout=15  # Add timeout
            )
            response.raise_for_status()
            
            logger.debug(f"Received response from Gemini API: Status {response.status_code}")
            response_data = response.json()
            
            if 'candidates' in response_data and len(response_data['candidates']) > 0:
                if 'content' in response_data['candidates'][0]:
                    content = response_data['candidates'][0]['content']
                    if 'parts' in content and len(content['parts']) > 0:
                        result = content['parts'][0]['text']
                        # Clean the result text
                        result = clean_text(result)
                        return jsonify({"result": result})
            
            logger.error("No valid information found in API response")
            return jsonify({"error": f"No information found for '{search_term}'"}), 404
            
        except requests.exceptions.RequestException as e:
            logger.error(f"Request to Gemini API failed: {str(e)}")
            return jsonify({"error": f"API request error: {str(e)}"}), 500
            
    except Exception as e:
        logger.error(f"Unexpected error in search endpoint: {str(e)}", exc_info=True)
        return jsonify({"error": str(e)}), 500

# Add a simple test endpoint
@app.route('/test', methods=['GET'])
def test():
    return jsonify({"status": "Server is running", "message": "API is operational"})

if __name__ == '__main__':
    logger.info("Starting AI recommendation server on port 5000...")
    app.run(host='0.0.0.0', port=5000, debug=True)