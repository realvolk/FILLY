import ctypes
import os

_lib = None

def _load_lib():
    global _lib
    if _lib is not None:
        return
    paths = [
        os.path.join(os.path.dirname(__file__), '../../../libfilly.so'),
        'libfilly.so',
    ]
    for p in paths:
        try:
            _lib = ctypes.CDLL(p)
            break
        except OSError:
            continue
    if _lib is None:
        raise OSError("Cannot load libfilly.so")

class Client:
    def __init__(self, socket_path=None):
        _load_lib()
        if socket_path is None:
            xdg = os.environ.get('XDG_RUNTIME_DIR', '/tmp')
            socket_path = f'{xdg}/filly.sock'
        self._c = _lib.filly_client_connect(socket_path.encode())
        if not self._c:
            raise ConnectionError(f"Cannot connect to {socket_path}")

    def send(self, json_str):
        _lib.filly_client_send_request(self._c, json_str.encode())
        result = ctypes.c_void_p()
        cancelled = ctypes.c_bool()
        _lib.filly_client_get_response(self._c, ctypes.byref(result), ctypes.byref(cancelled))
        return result, cancelled.value

    def send_key(self, keycode, ch=''):
        _lib.filly_client_send_key(self._c, keycode, ord(ch) if ch else 0)

    def close(self):
        _lib.filly_client_disconnect(self._c)
        self._c = None