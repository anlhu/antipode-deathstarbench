import json
import threading
import time
from typing import Dict, Any, Callable
from datetime import datetime, timedelta
import uuid

class CacheManager:
    _instance = None
    _lock = threading.Lock()
    
    def __new__(cls):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(CacheManager, cls).__new__(cls)
            return cls._instance
    
    def __init__(self):
        if not hasattr(self, 'initialized'):
            self.cache: Dict[Any, Dict[str, Any]] = {}
            self.initialized = True
            self._start_cleanup_thread()
    
    def _start_cleanup_thread(self):
        def cleanup():
            while True:
                with self._lock:
                    current_time = datetime.now()
                    expired_keys = [
                        key for key, value in self.cache.items()
                        if current_time > value['expiry']
                    ]
                    for key in expired_keys:
                        del self.cache[key]
                time.sleep(60)  # 每分钟检查一次
        
        cleanup_thread = threading.Thread(target=cleanup, daemon=True)
        cleanup_thread.start()
    
    def add(self, json_data: str, expiry_seconds: int = 3600):
        try:
            data = json.loads(json_data)
            if not isinstance(data, dict) or 'uuid' not in data or 'message' not in data:
                raise ValueError("JSON must contain 'uuid' and 'message' fields")
            
            uuid = data['uuid']
            message = data['message']
            
            with self._lock:
                self.cache[uuid] = {
                    'message': message,
                    'expiry': datetime.now() + timedelta(seconds=expiry_seconds)
                }
            print(f"Added to cache: {uuid} {message}")
            return True
        except json.JSONDecodeError:
            raise ValueError("Invalid JSON format")
    
    def get(self, uuid: Any) -> Any:
        with self._lock:
            if uuid in self.cache:
                cache_data = self.cache[uuid]
                if datetime.now() <= cache_data['expiry']:
                    return cache_data['message']
                else:
                    del self.cache[uuid]
            return None

def cache_decorator(expiry_seconds: int = 3600):
    def wrapper(func: Callable):
        def inner(*args, **kwargs):
            print(f"Calling function: {func.__name__}")
            cache_manager = CacheManager()
            
            # 不再使用 json，而是从参数列表中获取 message
            message = kwargs.get('message')
            
            if message is not None:
                new_uuid = str(uuid.uuid4())  # 将 UUID 转换为字符串
                
                # 执行原函数
                result = func(*args, **kwargs)
                
                # 将新的 uuid 与 message 加入缓存
                new_data = json.dumps({
                    "uuid": new_uuid,
                    "message": message
                })
                cache_manager.add(new_data, expiry_seconds)
                
                return result
            
            return func(*args, **kwargs)
        return inner
    return wrapper

# 使用示例
@cache_decorator(expiry_seconds=1800)  # 30分钟 = 1800秒
def process_message(message: Any) -> Any:
    # 模拟处理消息
    return f"Processed: {message}"

# 使用示例
if __name__ == "__main__":
    # 使用装饰器，显式传入 message 参数
    result1 = process_message(message="Hello World")
    result2 = process_message(message=42)
    result3 = process_message(message={"key": "value"})
    
    print(result1)
    print(result2)
    print(result3) 