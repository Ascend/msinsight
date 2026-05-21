import functools
from mcp import types
from state import state


def require_events(*event_names):
    """
    装饰器：确保绑定的 C++ 后端事件已完成后，才允许执行该 Tool。
    否则向 LLM 返回一个未就绪的 Error Text，提示其稍后再试。
    """

    def decorator(func):
        @functools.wraps(func)
        async def wrapper(*args, **kwargs):
            # 检查缺失的事件
            missing = [evt for evt in event_names if not state.is_completed(evt)]
            if missing:
                missing_str = ", ".join(missing)
                return types.CallToolResult(
                    content=[
                        types.TextContent(
                            type="text",
                            text=f"TOOL EXECUTION BLOCKED: The required backend parsing is not yet completed. "
                            f"Missing events: [{missing_str}]. Please wait a moment and try again.",
                        )
                    ],
                    isError=True,
                )
            # 依赖满足，正常执行原始的 Tool 逻辑
            return await func(*args, **kwargs)

        return wrapper

    return decorator
