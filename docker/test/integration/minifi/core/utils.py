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
    # encode('ascii') makes sure that we don't mess up unicode characters
    # (as it throws if there are any)
    return str.encode('ascii').decode('unicode_escape')
