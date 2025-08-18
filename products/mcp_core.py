#!/usr/bin/env python3
"""
MCP Core Utilities

Shared abstractions for building MCP stdio servers with pluggable tools.

Provides:
- PluginManager: dynamic loader for `{prefix}.*.py` plugins exposing `Plugin` class
- MCPTool: JSON-RPC tool descriptor with async handler

This module is intentionally minimal to avoid coupling with product-specific logic.
"""

from __future__ import annotations

import sys
import importlib.util
import glob
from pathlib import Path
from typing import Dict, Any, Optional, Callable, Awaitable
import os


class PluginManager:
    """Generic plugin manager that loads `{prefix}.*.py` modules.

    Each plugin file must define a `Plugin` class with:
      - name: str property
      - description: str property
      - async def execute(self, code: str) -> str
    """

    def __init__(self, prefix: str, plugin_dir: Path | None = None):
        self.prefix = prefix
        self.plugin_dir = plugin_dir or Path(__file__).parent
        self.plugins: Dict[str, Any] = {}
        self.plugin_modules: Dict[str, Any] = {}
        self.load_all_plugins()

    def load_all_plugins(self):
        """Load all plugins matching `{prefix}.*.py` in `plugin_dir`."""
        pattern = str(self.plugin_dir / f"{self.prefix}.*.py")
        exclude_env = os.environ.get("CTX_EXCLUDE_PLUGINS", "")
        exclude_set = {name.strip() for name in exclude_env.split(",") if name.strip()}

        for plugin_file in glob.glob(pattern):
            filename = Path(plugin_file).name
            if not (filename.startswith(f"{self.prefix}.") and filename.endswith(".py")):
                continue

            # Derive candidate plugin name
            plugin_name = filename[len(self.prefix) + 1 : -3]

            # Skip known non-plugins or backups
            if (
                not plugin_name
                or plugin_name.endswith(".")  # e.g. ctx.py.py -> plugin_name 'py.'
                or ".backup" in plugin_name
                or ".bkp" in plugin_name
                or plugin_name == "py.py"
            ):
                continue

            # Optional explicit exclusions via env
            if plugin_name in exclude_set:
                continue

            # CDP plugins are now enabled by default (removed restriction)

            self.load_plugin(plugin_name)

    def load_plugin(self, plugin_name: str) -> bool:
        """Load or reload a specific plugin by name (without prefix)."""
        plugin_file = self.plugin_dir / f"{self.prefix}.{plugin_name}.py"
        if not plugin_file.exists():
            if os.environ.get("CTX_DEBUG", "0") in ("1", "true", "True"):
                print(f"[{self.prefix}] plugin file not found: {plugin_file}", file=sys.stderr)
            return False

        try:
            spec = importlib.util.spec_from_file_location(
                f"{self.prefix}_plugin_{plugin_name}", str(plugin_file)
            )
            if not spec or not spec.loader:
                if os.environ.get("CTX_DEBUG", "0") in ("1", "true", "True"):
                    print(f"[{self.prefix}] failed to create spec for {plugin_file}", file=sys.stderr)
                return False

            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)  # type: ignore[attr-defined]

            if not hasattr(module, "Plugin"):
                if os.environ.get("CTX_DEBUG", "0") in ("1", "true", "True"):
                    print(f"[{self.prefix}] no Plugin class in {plugin_file}", file=sys.stderr)
                return False

            plugin_class = getattr(module, "Plugin")
            plugin_instance = plugin_class()

            # Register by runtime plugin name to allow internal naming
            self.plugin_modules[plugin_name] = module
            self.plugins[plugin_instance.name] = plugin_instance
            return True

        except Exception as exc:
            if os.environ.get("CTX_DEBUG", "0") in ("1", "true", "True"):
                print(f"[{self.prefix}] error loading plugin {plugin_name}: {exc}", file=sys.stderr)
            return False

    def reload_plugin(self, plugin_name: str) -> str:
        if self.load_plugin(plugin_name):
            return f"Plugin '{plugin_name}' reloaded successfully"
        return f"Failed to reload plugin '{plugin_name}'"

    def register(self, plugin: Any):
        self.plugins[plugin.name] = plugin

    def get(self, name: str) -> Optional[Any]:
        return self.plugins.get(name)

    def list_plugins(self) -> list[str]:
        return list(self.plugins.keys())

    async def execute(self, plugin_name: str, code: str) -> str:
        plugin = self.get(plugin_name)
        if not plugin:
            available = ", ".join(self.list_plugins()) if self.plugins else "none"
            return f"Error: Unknown plugin '{plugin_name}'. Available plugins: {available}"
        return await plugin.execute(code)


class MCPTool:
    """Represents an MCP tool definition with async handler."""

    def __init__(
        self,
        name: str,
        description: str,
        parameters: Dict[str, Any],
        handler: Callable[[Dict[str, Any]], Awaitable[str]],
        annotations: Optional[Dict[str, Any]] = None,
    ):
        self.name = name
        self.description = description
        self.parameters = parameters
        self.handler = handler
        self.annotations = annotations or {}

    def to_dict(self) -> Dict[str, Any]:
        result = {
            "name": self.name,
            "description": self.description,
            "inputSchema": {
                "type": "object",
                "properties": self.parameters,
                "required": list(self.parameters.keys()),
            },
        }
        if self.annotations:
            result["annotations"] = self.annotations
        return result

    async def execute(self, arguments: Dict[str, Any]) -> str:
        return await self.handler(arguments)
