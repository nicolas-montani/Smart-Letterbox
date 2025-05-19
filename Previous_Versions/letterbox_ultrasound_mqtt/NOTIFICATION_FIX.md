# Smart Letterbox Notification Fix

This document explains the changes made to fix the issue with notifications not being sent to your phone when the MQTT gets updated.

## Problem

The original code had an issue in the `send_notification` function where it was trying to access the Flask `request` object outside of a request context. This happens because the MQTT callback runs in a separate thread, outside of any HTTP request.

```python
# Original problematic code
def send_notification(message):
    # ...
    payload = {
        # ...
        "link": f"http://{request.host}/",  # This line causes an error in MQTT context
        # ...
    }
    # ...
```

When the MQTT callback tried to send a notification, it would fail because `request.host` is not available in that context.

## Solution

The solution involved several changes:

1. **Modified the `send_notification` function** to accept an optional `host_url` parameter:
   ```python
   def send_notification(message, host_url=None):
       # ...
       # Add link only if host_url is provided
       if host_url:
           payload["link"] = f"http://{host_url}/"
       # ...
   ```

2. **Added detailed error logging** to help diagnose any issues with the notification service:
   ```python
   # Print debug info
   print(f"Sending notification with payload: {payload}")
   print(f"Using API key: {TECHULUS_API_KEY}")
   print(f"To URL: {NOTIFICATION_URL}")
   
   # More detailed response logging
   print(f"Response status: {response.status_code}")
   print(f"Response headers: {response.headers}")
   print(f"Response body: {response.text}")
   ```

3. **Added a test notification endpoint** to verify the notification system is working:
   ```python
   @app.route('/api/test-notification')
   def test_notification():
       """Route to test the notification system"""
       try:
           # Send a test notification with the current host URL
           send_notification("This is a test notification from your Smart Letterbox", request.host)
           return jsonify({"success": True, "message": "Test notification sent"})
       except Exception as e:
           return jsonify({"success": False, "message": f"Error sending notification: {str(e)}"})
   ```

4. **Added a "Test Notification" button** to the web interface to easily test if notifications are working.

5. **Added a notification setup guide** to the web interface to help with configuring the Techulus Push app.

## How to Test

1. Make sure you have the Techulus Push app installed on your phone and have set up an account.
2. Ensure your API key is correctly set in the `TECHULUS_API_KEY` variable in the `raspberry_pi_mqtt_server.py` file.
3. Restart the server to apply the changes.
4. Open the web interface and click the "Test Notification" button.
5. You should receive a notification on your phone.
6. Check the server logs for detailed information about the notification request and response.

## Troubleshooting

If you're still not receiving notifications:

1. **Check the API key**: Make sure your Techulus Push API key is correct. You can get this from the Techulus Push app.
2. **Check the server logs**: Look for any error messages in the server logs when a notification is sent.
3. **Verify the Techulus Push app**: Make sure the app is installed, you're logged in, and it has the necessary permissions.
4. **Check your internet connection**: Both the Raspberry Pi and your phone need internet access for notifications to work.
5. **Try a different notification service**: If Techulus Push doesn't work for you, you might want to consider alternatives like Pushover, Telegram, or email notifications.
