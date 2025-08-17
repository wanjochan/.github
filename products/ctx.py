#!/usr/bin/env python3
"""
Context Management MCP Service - Plugin-based architecture
Version 3.0 with proper MCP stdio server implementation

Installation and usage with Claude CLI:
----------------------------------------
1. Add MCP service to Claude:
   claude mcp add ctx python3 /path/to/ctx.py server

2. Allow tools (important for custom MCP services):
   claude --allowedTools "mcp__ctx__*" -p "Your prompt"

3. Example usage:
   # Unified ctx() method - specify plugin with 'plugin' parameter
   claude --allowedTools "mcp__ctx__ctx" -p "Use ctx(plugin='mgr', code='save project_x')"
   
   # Reload plugin for hot-reloading
   claude --allowedTools "mcp__ctx__reload" -p "Reload mgr plugin"
   
   # Allow all ctx tools
   claude --allowedTools "mcp__ctx__*" -p "Execute context command"

4. Available plugins (auto-loaded from ctx.*.py files):
   - mgr: AIX virtual company management views
   - aix: AI session communication
   - (extensible - add custom plugins by creating ctx.{name}.py files)

5. Check service status:
   claude mcp list

6. Remove service:
   claude mcp remove ctx
"""

import sys
import io
import json
import asyncio
import traceback
import os
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Awaitable
from datetime import datetime
try:
    from mcp_core import PluginManager, MCPTool
except ImportError:  # When imported as package module
    from products.mcp_core import PluginManager, MCPTool

# Windows terminal encoding handling
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


class MCPServer:
    """MCP Server implementation with plugin-based architecture"""
    
    def __init__(self):
        """Initialize MCP server with plugin manager"""
        self.server_info = {
            "name": "ctx-mcp",
            "version": "3.0.0",
            "description": "Context management service with plugin architecture"
        }
        
        # Initialize shared plugin manager
        self.plugin_manager = PluginManager(prefix="ctx")
        
        # Register tools
        self.tools = self._register_tools()
    
    def _register_tools(self) -> Dict[str, MCPTool]:
        """Register available tools"""
        tools = {
            # Unified ctx method
            "ctx": MCPTool(
                name="ctx",
                description="Execute context commands using specified plugin",
                parameters={
                    "plugin": {
                        "type": "string",
                        "description": "Plugin name (e.g., mgr)"
                    },
                    "code": {
                        "type": "string", 
                        "description": "Command to execute"
                    }
                },
                handler=lambda args: self.plugin_manager.execute(
                    args.get("plugin", ""),
                    args.get("code", "")
                ),
                annotations={
                    "title": "ctx",
                    "readOnlyHint": False,
                    "destructiveHint": False,
                    "openWorldHint": False
                }
            )
        }
        
        # Add reload tool for hot-reloading plugins
        tools["reload"] = MCPTool(
            name="reload",
            description="Reload a plugin to pick up code changes",
            parameters={
                "plugin": {
                    "type": "string",
                    "description": "Plugin name to reload"
                }
            },
            handler=lambda args: asyncio.create_task(
                self._handle_reload(args.get("plugin", ""))
            ),
            annotations={
                "title": "Reload Plugin",
                "readOnlyHint": False,
                "destructiveHint": False,
                "openWorldHint": False
            }
        )
        
        # Add list tool to show available plugins
        tools["list"] = MCPTool(
            name="list",
            description="List all available plugins",
            parameters={},
            handler=lambda args: asyncio.create_task(
                self._handle_list()
            ),
            annotations={
                "title": "List Plugins",
                "readOnlyHint": True,
                "destructiveHint": False,
                "openWorldHint": False
            }
        )
        
        return tools
    
    async def _handle_reload(self, plugin_name: str) -> str:
        """Handle plugin reload request"""
        return self.plugin_manager.reload_plugin(plugin_name)
    
    async def _handle_list(self) -> str:
        """Handle list plugins request"""
        plugins = self.plugin_manager.list_plugins()
        if not plugins:
            return "No plugins loaded"
        
        output = "Available Context Plugins:\n"
        for name in plugins:
            plugin = self.plugin_manager.get(name)
            if plugin:
                output += f"  - {name}: {plugin.description}\n"
        return output
    
    def _create_response(self, request_id: Any, result: Any) -> Dict[str, Any]:
        """Create a successful JSON-RPC response"""
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": result
        }
    
    def _create_error(self, request_id: Any, code: int, message: str) -> Dict[str, Any]:
        """Create an error JSON-RPC response"""
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {
                "code": code,
                "message": message
            }
        }
    
    async def handle_request(self, request: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Handle incoming MCP requests"""
        method = request.get("method")
        params = request.get("params", {})
        request_id = request.get("id")
        
        handlers = {
            "initialize": self._handle_initialize,
            "initialized": lambda *_: None,  # No response needed
            "tools/list": self._handle_list_tools,
            "tools/call": self._handle_tool_call
        }
        
        handler = handlers.get(method)
        if not handler:
            return self._create_error(request_id, -32601, f"Method not found: {method}")
        
        try:
            result = await handler(request_id, params) if asyncio.iscoroutinefunction(handler) else handler(request_id, params)
            return result
        except Exception as e:
            traceback.print_exc()
            return self._create_error(request_id, -32603, f"Internal error: {str(e)}")
    
    def _handle_initialize(self, request_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle initialization request"""
        return self._create_response(request_id, {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {},
                "resources": {}
            },
            "serverInfo": self.server_info
        })
    
    def _handle_list_tools(self, request_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle tools list request"""
        return self._create_response(request_id, {
            "tools": [tool.to_dict() for tool in self.tools.values()]
        })
    
    async def _handle_tool_call(self, request_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle tool execution request"""
        tool_name = params.get("name")
        arguments = params.get("arguments", {})
        
        tool = self.tools.get(tool_name)
        if not tool:
            return self._create_error(request_id, -32602, f"Unknown tool: {tool_name}")
        
        result = await tool.execute(arguments)
        return self._create_response(request_id, {
            "content": [{"type": "text", "text": result}]
        })
    
    async def run_stdio(self):
        """Run the MCP server in stdio mode"""
        print("Context MCP Service started (stdio mode)", file=sys.stderr)
        print(f"Plugin directory: {self.plugin_manager.plugin_dir}", file=sys.stderr)
        
        while True:
            try:
                line = sys.stdin.readline()
                if not line:
                    break
                
                # Parse and handle request
                try:
                    request = json.loads(line.strip())
                except json.JSONDecodeError:
                    continue
                
                response = await self.handle_request(request)
                if response is not None:
                    # Use ensure_ascii=False to avoid escaping non-ASCII characters
                    print(json.dumps(response, ensure_ascii=False), flush=True)
                    
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Server error: {e}", file=sys.stderr)
                traceback.print_exc()


def check_singleton():
    """Ensure only one ctx server instance is running"""
    import subprocess
    import os
    
    try:
        # Check for existing ctx.py server processes
        result = subprocess.run(
            ["ps", "aux"], 
            capture_output=True, text=True, timeout=5
        )
        
        lines = result.stdout.split('\n')
        ctx_processes = [line for line in lines if 'ctx.py server' in line and str(os.getpid()) not in line]
        
        if ctx_processes:
            print(f"CTX singleton check: Found {len(ctx_processes)} existing ctx server process(es)", file=sys.stderr)
            print("Existing processes:", file=sys.stderr)
            for proc in ctx_processes:
                print(f"  {proc}", file=sys.stderr)
            
            # Gracefully terminate existing processes
            for proc_line in ctx_processes:
                try:
                    pid = proc_line.split()[1]
                    print(f"Terminating existing ctx server (PID: {pid})", file=sys.stderr)
                    subprocess.run(["kill", "-TERM", pid], timeout=3)
                except (IndexError, subprocess.TimeoutExpired, subprocess.CalledProcessError):
                    pass
            
            # Wait a moment for graceful shutdown
            import time
            time.sleep(1)
            
            print("CTX singleton protection: Proceeding with new instance", file=sys.stderr)
        else:
            print("CTX singleton check: No existing instances found", file=sys.stderr)
            
    except Exception as e:
        print(f"CTX singleton check failed: {e}", file=sys.stderr)
        # Continue anyway - don't block startup on check failure


async def main():
    """Main entry point - run MCP server"""
    if len(sys.argv) > 1 and sys.argv[1] == 'server':
        # Enforce singleton pattern
        check_singleton()
        
        server = MCPServer()
        await server.run_stdio()
    else:
        print("Usage: python ctx.py server", file=sys.stderr)
        sys.exit(1)


# === Python API for direct imports ===
def ctx(plugin: str = "mgr", code: str = "") -> str:
    """
    Direct Python API for CTX system
    
    Usage:
        from ctx import ctx
        result = ctx("mgr", "status")
        result = ctx("mgr", "lesson add Never create documentation files")
    
    Args:
        plugin: Plugin name (default: "mgr")
        code: Command to execute
        
    Returns:
        String result from the plugin
    """
    try:
        # Initialize plugin manager
        pm = PluginManager(prefix="ctx")
        
        # Execute command
        async def run_async():
            return await pm.execute(plugin, code)
        
        # Create new event loop if needed
        try:
            loop = asyncio.get_event_loop()
            if loop.is_running():
                # If loop is already running (e.g., in Jupyter), create task
                import concurrent.futures
                with concurrent.futures.ThreadPoolExecutor() as executor:
                    future = executor.submit(asyncio.run, run_async())
                    result = future.result()
            else:
                result = loop.run_until_complete(run_async())
        except RuntimeError:
            # No event loop, create new one
            result = asyncio.run(run_async())
        
        return result
    except Exception as e:
        return f"Error: {str(e)}"


# Export for Python imports
__all__ = ['ctx', 'MCPServer', 'PluginManager']


if __name__ == "__main__":
    asyncio.run(main())