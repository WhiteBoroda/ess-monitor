// OTA Firmware Update Page

const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OTA Firmware Update</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: #0f0f0f;
      color: #e0e0e0;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
    }
    .container {
      max-width: 600px;
      width: 100%;
    }
    .card {
      background: #1a1a1a;
      border: 1px solid #333;
      border-radius: 8px;
      padding: 30px;
    }
    h1 {
      color: #4CAF50;
      margin-bottom: 20px;
      font-size: 1.8em;
    }
    .info {
      background: #2a2a2a;
      padding: 15px;
      border-radius: 4px;
      margin-bottom: 20px;
      border-left: 4px solid #FF9800;
    }
    .info p {
      margin: 5px 0;
      color: #ccc;
    }
    .upload-form {
      margin-top: 20px;
    }
    .file-input {
      display: none;
    }
    .file-label {
      display: block;
      padding: 15px;
      background: #2a2a2a;
      border: 2px dashed #555;
      border-radius: 4px;
      text-align: center;
      cursor: pointer;
      transition: all 0.3s;
      margin-bottom: 20px;
    }
    .file-label:hover {
      border-color: #4CAF50;
      background: #333;
    }
    .file-name {
      color: #4CAF50;
      margin-top: 10px;
      font-weight: bold;
    }
    .btn {
      width: 100%;
      padding: 15px;
      border: none;
      border-radius: 4px;
      font-size: 1.1em;
      cursor: pointer;
      transition: all 0.3s;
      margin-bottom: 10px;
    }
    .btn-primary {
      background: #4CAF50;
      color: #000;
    }
    .btn-primary:hover {
      background: #45a049;
    }
    .btn-primary:disabled {
      background: #555;
      cursor: not-allowed;
    }
    .btn-secondary {
      background: #555;
      color: #e0e0e0;
    }
    .btn-secondary:hover {
      background: #666;
    }
    .progress {
      width: 100%;
      height: 30px;
      background: #2a2a2a;
      border-radius: 4px;
      overflow: hidden;
      margin-bottom: 20px;
      display: none;
    }
    .progress-bar {
      height: 100%;
      background: #4CAF50;
      width: 0%;
      transition: width 0.3s;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #000;
      font-weight: bold;
    }
    .message {
      padding: 15px;
      border-radius: 4px;
      margin-bottom: 20px;
      display: none;
    }
    .message.success {
      background: #4CAF50;
      color: #000;
    }
    .message.error {
      background: #F44336;
      color: #fff;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>🔄 OTA Firmware Update</h1>

      <div class="info">
        <p><strong>⚠️ Warning:</strong></p>
        <p>• Do not disconnect power during update</p>
        <p>• Only upload .bin files from trusted sources</p>
        <p>• Device will reboot after successful update</p>
      </div>

      <div id="message" class="message"></div>

      <form id="uploadForm" class="upload-form">
        <label for="firmware" class="file-label">
          📁 Click to select firmware (.bin file)
          <div id="fileName" class="file-name"></div>
        </label>
        <input type="file" id="firmware" name="firmware" accept=".bin" class="file-input" required>

        <div id="progress" class="progress">
          <div id="progressBar" class="progress-bar">0%</div>
        </div>

        <button type="submit" id="uploadBtn" class="btn btn-primary">📤 Upload Firmware</button>
        <button type="button" onclick="window.location.href='/'" class="btn btn-secondary">← Back to Dashboard</button>
      </form>
    </div>
  </div>

  <script>
    const fileInput = document.getElementById('firmware');
    const fileName = document.getElementById('fileName');
    const uploadForm = document.getElementById('uploadForm');
    const uploadBtn = document.getElementById('uploadBtn');
    const progress = document.getElementById('progress');
    const progressBar = document.getElementById('progressBar');
    const message = document.getElementById('message');

    fileInput.addEventListener('change', function(e) {
      if (e.target.files.length > 0) {
        fileName.textContent = '✓ ' + e.target.files[0].name;
      } else {
        fileName.textContent = '';
      }
    });

    uploadForm.addEventListener('submit', function(e) {
      e.preventDefault();

      const file = fileInput.files[0];
      if (!file) {
        showMessage('Please select a firmware file', 'error');
        return;
      }

      if (!file.name.endsWith('.bin')) {
        showMessage('Only .bin files are allowed', 'error');
        return;
      }

      uploadFirmware(file);
    });

    function uploadFirmware(file) {
      const xhr = new XMLHttpRequest();
      const formData = new FormData();
      formData.append('firmware', file);

      // Show progress bar
      progress.style.display = 'block';
      uploadBtn.disabled = true;
      message.style.display = 'none';

      // Upload progress
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressBar.style.width = percent + '%';
          progressBar.textContent = percent + '%';
        }
      });

      // Upload complete
      xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
          progressBar.style.width = '100%';
          progressBar.textContent = '100%';
          showMessage('✓ Update successful! Device is rebooting...', 'success');
          setTimeout(function() {
            window.location.href = '/';
          }, 5000);
        } else {
          showMessage('✗ Update failed: ' + xhr.responseText, 'error');
          uploadBtn.disabled = false;
        }
      });

      // Upload error
      xhr.addEventListener('error', function() {
        showMessage('✗ Upload failed. Please try again.', 'error');
        uploadBtn.disabled = false;
      });

      xhr.open('POST', '/ota_update', true);
      xhr.send(formData);
    }

    function showMessage(text, type) {
      message.textContent = text;
      message.className = 'message ' + type;
      message.style.display = 'block';
    }
  </script>
</body>
</html>
)rawliteral";
