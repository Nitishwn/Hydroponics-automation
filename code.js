const sensorData = {
    ph: 7.5,
    tds: 420,
    temperature: 32,
    humidity: 55
  };
  
  // Step 1: Format into a query string
  const queryString = `pH: ${sensorData.ph}, TDS: ${sensorData.tds} ppm, Temperature: ${sensorData.temperature}°C, Humidity: ${sensorData.humidity}%`;
  
  // Step 2: Send POST request with "query" key
  fetch("http://localhost:5000/recommend", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify({ query: queryString })
  })
  .then(response => response.json())
  .then(data => {
    console.log("AI Response:", data);
  
    // Show it on your dashboard
    document.getElementById("ai-output").innerText = data.recommendation || data.error;
  })
  .catch(error => {
    console.error("Error:", error);
  });
  