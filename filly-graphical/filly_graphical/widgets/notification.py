import subprocess
from .base import BaseWindow

class NotificationWindow(BaseWindow):
    def run(self):
        message = self.params.get("message", "")
        duration = self.params.get("duration", 3)
        title = self.title_text or "FILLY"

        # Try desktop notification first
        try:
            subprocess.run(
                ["notify-send", title, message,
                 "--expire-time", str(duration * 1000)],
                timeout=2
            )
        except Exception:
            pass

        # Always return immediately; notification is fire-and-forget
        return {"result": None, "cancelled": False}