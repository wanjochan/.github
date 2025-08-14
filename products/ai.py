#!/usr/bin/env python3
"""
AI MCP Service - Python and JavaScript execution capabilities
Extracted from mcp_tasker.py for focused code execution functionality

Installation and usage with Claude CLI:
----------------------------------------
1. Add MCP service to Claude:
   claude mcp add ai python3 /path/to/ai.py server

2. Allow tools (important for custom MCP services):
   claude --allowedTools "mcp__ai__*" -p "Your prompt"
   
   Or for specific tools:
   claude --allowedTools "mcp__ai__py,mcp__ai__js" -p "Your prompt"

3. Example usage:
   # Execute Python code
   claude --allowedTools "mcp__ai__py" -p "Calculate factorial of 10"
   
   # Execute JavaScript code  
   claude --allowedTools "mcp__ai__js" -p "Generate fibonacci sequence in JS"
   
   # Allow all ai tools
   claude --allowedTools "mcp__ai__*" -p "py(2**10)"

4. Check service status:
   claude mcp list

5. Remove service:
   claude mcp remove ai
"""

import sys
import io
import json
import asyncio
import traceback
import subprocess
import tempfile
import os
import contextlib
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Awaitable
from datetime import datetime
from abc import ABC, abstractmethod

# Windows terminal encoding handling
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


class BaseExecutor(ABC):
    """Abstract base class for code executors"""
    
    @abstractmethod
    async def execute(self, code: str) -> str:
        """Execute code and return results"""
        pass
    
    def _format_error(self, error: Exception) -> str:
        """Format error message consistently"""
        return f"Error: {str(error)}\n{traceback.format_exc()}"
    
    def _format_success(self, output: str) -> str:
        """Format successful execution output"""
        return output if output else "Code executed successfully (no output)"


class PythonExecutor(BaseExecutor):
    """Python code executor with persistent globals"""
    
    def __init__(self):
        """Initialize with persistent Python globals"""
        self.globals = {}
        self._setup_base_modules()
    
    def _setup_base_modules(self):
        """Setup commonly used Python modules"""
        import re
        self.base_modules = {
            '__builtins__': __builtins__,
            'json': json,
            'datetime': datetime,
            'Path': Path,
            're': re,
            'asyncio': asyncio,
            'sys': sys,
            'os': os,
        }
    
    async def execute(self, code: str) -> str:
        """Execute Python code with persistent state"""
        output_buffer = io.StringIO()
        
        try:
            # Prepare execution environment
            exec_env = dict(self.base_modules)
            exec_env.update(self.globals)
            
            with contextlib.redirect_stdout(output_buffer):
                try:
                    # Try to evaluate as expression first
                    result = eval(compile(code, '<ai_py>', 'eval'), exec_env)
                    if result is not None:
                        output_buffer.write(str(result))
                except SyntaxError:
                    # Execute as statement if not an expression
                    exec(compile(code, '<ai_py>', 'exec'), exec_env)
                
                # Update persistent globals (exclude base modules and privates)
                self.globals.update({
                    k: v for k, v in exec_env.items()
                    if k not in self.base_modules and not k.startswith('__')
                })
        
        except Exception as e:
            return self._format_error(e)
        
        return self._format_success(output_buffer.getvalue())


class JavaScriptExecutor(BaseExecutor):
    """JavaScript/Node.js code executor"""
    
    def __init__(self):
        """Initialize JavaScript executor"""
        self.node_available = self._check_node()
    
    def _check_node(self) -> bool:
        """Check if Node.js is available"""
        try:
            result = subprocess.run(
                ['node', '--version'],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False
    
    async def execute(self, code: str) -> str:
        """Execute JavaScript/Node.js code"""
        if not self.node_available:
            return "Error: Node.js is not installed or not in PATH"
        
        try:
            # Create temporary file for code
            with tempfile.NamedTemporaryFile(
                mode='w', 
                suffix='.js', 
                delete=False
            ) as temp_file:
                temp_file.write(code)
                temp_path = temp_file.name
            
            try:
                # Execute Node.js code
                result = subprocess.run(
                    ['node', temp_path],
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                
                # Combine output streams
                output = result.stdout
                if result.stderr:
                    output = f"{output}\n[stderr]: {result.stderr}" if output else f"[stderr]: {result.stderr}"
                
                if result.returncode != 0:
                    output = f"Exit code: {result.returncode}\n{output}"
                
                return self._format_success(output)
                
            finally:
                # Clean up temporary file
                try:
                    os.unlink(temp_path)
                except:
                    pass
                    
        except subprocess.TimeoutExpired:
            return "Error: JavaScript execution timed out (30 seconds)"
        except Exception as e:
            return self._format_error(e)


class MCPTool:
    """Represents an MCP tool definition"""
    
    def __init__(self, name: str, description: str, parameters: Dict[str, Any], 
                 handler: Callable[[Dict[str, Any]], Awaitable[str]]):
        self.name = name
        self.description = description
        self.parameters = parameters
        self.handler = handler
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to MCP tool definition format"""
        return {
            "name": self.name,
            "description": self.description,
            "inputSchema": {
                "type": "object",
                "properties": self.parameters,
                "required": list(self.parameters.keys())
            }
        }
    
    async def execute(self, arguments: Dict[str, Any]) -> str:
        """Execute the tool with given arguments"""
        return await self.handler(arguments)


class MCPServer:
    """MCP Server implementation with modular tool support"""
    
    def __init__(self):
        """Initialize MCP server with executors and tools"""
        self.server_info = {
            "name": "ai-mcp",
            "version": "2.0.0",
            "description": "AI code execution service for Python and JavaScript"
        }
        
        # Initialize executors
        self.python_executor = PythonExecutor()
        self.js_executor = JavaScriptExecutor()
        
        # Register tools
        self.tools = self._register_tools()
    
    def _register_tools(self) -> Dict[str, MCPTool]:
        """Register available tools"""
        return {
            "py": MCPTool(
                name="py",
                description="Execute Python code",
                parameters={
                    "code": {
                        "type": "string",
                        "description": "Python code to execute"
                    }
                },
                handler=lambda args: self.python_executor.execute(args.get("code", ""))
            ),
            "js": MCPTool(
                name="js",
                description="Execute JavaScript/Node.js code",
                parameters={
                    "code": {
                        "type": "string",
                        "description": "JavaScript/Node.js code to execute"
                    }
                },
                handler=lambda args: self.js_executor.execute(args.get("code", ""))
            )
        }
    
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
        print("AI MCP Service started (stdio mode)", file=sys.stderr)
        
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
                    print(json.dumps(response), flush=True)
                    
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Server error: {e}", file=sys.stderr)
                traceback.print_exc()


async def main():
    """Main entry point - run MCP server"""
    if len(sys.argv) > 1 and sys.argv[1] == 'server':
        server = MCPServer()
        await server.run_stdio()
    else:
        print("Usage: python ai.py server", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())