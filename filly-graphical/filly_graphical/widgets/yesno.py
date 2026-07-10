from .base import BaseWindow

class YesNoWindow(BaseWindow):
    def run(self):
        vbox = self._create_window(400, 200)
        vbox.append(self._make_button_box(
            ("Yes", lambda b: self._answer(True), True),
            ("No", lambda b: self._answer(False), False),
        ))
        return super().run()

    def _answer(self, val):
        self.result = val
        self._quit()