// Copy this file to a new file named arduino_secrets.h (same folder) and fill
// it in. Only the ESP8266 uses it.
//
// arduino_secrets.h holds your WiFi password, so keep it off GitHub: it's
// listed in .gitignore, but don't upload it through GitHub's website either.

#define SECRET_WIFI_NAME "your-wifi-name"
#define SECRET_WIFI_PASSWORD "your-wifi-password"

// Phone notifications use ntfy (https://ntfy.sh). Install the ntfy app and
// subscribe to a topic name that's hard to guess, because anyone who knows the
// name can read your messages. Leave it empty ("") to turn notifications off.
#define SECRET_NTFY_TOPIC ""

// Login for the Open/Close buttons on the status page. Leave the password
// empty ("") to let anyone on your WiFi use the buttons.
#define SECRET_WEB_USERNAME "coop"
#define SECRET_WEB_PASSWORD ""
