import time
import functools


def retry_check(max_tries=5, retry_interval=1):
    def retry_check_func(func):
        @functools.wraps(func)
        def retry_wrapper(*args, **kwargs):
            current_retry_count = 0
            while current_retry_count < max_tries:
                if not func(*args, **kwargs):
                    current_retry_count += 1
                    time.sleep(retry_interval)
                else:
                    return True
            return False
        return retry_wrapper
    return retry_check_func


def decode_escaped_str(str):
    special = {"n": "\n", "v": "\v", "t": "\t", "f": "\f", "r": "\r", "a": "\a", "\\": "\\"}
    escaped = False
    result = ""
    for ch in str:
        if escaped:
            if ch in special:
                result += special[ch]
            else:
                result += "\\" + ch
            escaped = False
        elif ch == "\\":
            escaped = True
        else:
            result += ch
    if escaped:
        result += "\\"
    return result
