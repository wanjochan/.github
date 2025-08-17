#!/usr/bin/env python3
"""Python execution plugin for AI MCP service"""

import sys
import io
import contextlib
import traceback
from datetime import datetime
from pathlib import Path
import json
import asyncio
import os

class PythonPlugin:
    """Python code executor plugin with persistent globals"""
    
    @property
    def name(self) -> str:
        return "py"
    
    @property
    def description(self) -> str:
        return "Execute Python code with persistent state"
    
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
            
            # Redirect stdout to capture output
            with contextlib.redirect_stdout(output_buffer):
                # Try to compile and evaluate as an expression first
                try:
                    result = eval(code, exec_env, self.globals)
                    if result is not None:
                        print(result)
                except SyntaxError:
                    # If not an expression, execute as statements
                    exec(code, exec_env, self.globals)
                except Exception:
                    # If eval fails for other reasons, try exec
                    exec(code, exec_env, self.globals)
            
            # Update persistent globals (excluding base modules)
            for key, value in exec_env.items():
                if key not in self.base_modules:
                    self.globals[key] = value
            
            output = output_buffer.getvalue()
            return output if output else "Code executed successfully (no output)"
            
        except Exception as e:
            return f"Error: {str(e)}\n{traceback.format_exc()}"

# Export plugin class
Plugin = PythonPlugin