/**
 * Projekt: EWSI - moduł bezprzewodowego sensora wilgotności i temperatury
 * Moduł: Strona WWW konfiguracji modułu
 * Autorzy: Jakub Kochańczyk, Maciej Bębenek
 * Data: 05.2026
 * Wersja: 1.1.0
 */

#ifndef WEB_PAGES_H
#define WEB_PAGES_H

const char CONFIG_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Sensor Konfiguracja</title>
    <style>
        body { font-family: sans-serif; margin: 20px; background: #f4f4f9; color: #333; }
        .container { max-width: 400px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        h1 { font-size: 1.5em; text-align: center; color: #008CBA; }
        label { font-weight: bold; display: block; margin-top: 10px; }
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin: 5px 0 15px 0; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
        .checkbox-group { display: flex; align-items: center; margin: 15px 0; }
        .checkbox-group input { width: auto; margin-right: 10px; }
        #static_fields { display: none; background: #f9f9f9; padding: 10px; border-radius: 4px; border: 1px solid #eee; }
        input[type="submit"] { width: 100%; background-color: #008CBA; color: white; border: none; padding: 12px; cursor: pointer; border-radius: 4px; font-size: 1em; margin-top: 10px; }
        input[type="submit"]:hover { background-color: #007ba7; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Konfiguracja Sensora</h1>
        <form action='/save' method='POST'>
            <label>SSID:</label>
            <input type='text' name='ssid' placeholder='Nazwa WiFi' required>
            
            <label>Hasło:</label>
            <input type='password' name='pass' placeholder='Hasło WiFi'>
            
            <label>IP Home Assistant:</label>
            <input type='text' name='ha_ip' placeholder='np. 192.168.1.101' required>

            <div class="checkbox-group">
                <input type="checkbox" id="use_static" name="use_static" value="1" onchange="toggleFields()">
                <label for="use_static">Użyj Static IP</label>
            </div>

            <div id="static_fields">
                <label>Adres IP ESP:</label>
                <input type='text' name='static_ip' placeholder='np. 192.168.1.185'>
                
                <label>Brama (Gateway):</label>
                <input type='text' name='gateway' placeholder='np. 192.168.1.1'>
                
                <label>Maska (Subnet):</label>
                <input type='text' name='subnet' placeholder='255.255.255.0'>
            </div>

            <input type='submit' value='Zapisz i Restart'>
        </form>
    </div>

    <script>
        function toggleFields() {
            var checkBox = document.getElementById("use_static");
            var fields = document.getElementById("static_fields");
            fields.style.display = checkBox.checked ? "block" : "none";
        }
    </script>
</body>
</html>
)=====";

#endif
