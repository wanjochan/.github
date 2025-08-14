#!/usr/bin/env python3
"""
Source validation utilities for Herbiverse.

- Loads trusted organizations and licenses from `config.json`
- Validates references list objects: [{title, org, year, url}]
- Provides CLI to validate a JSON file that contains a `references` array

Usage examples:
  python source_validator.py --file ../json/mint.json
  python source_validator.py --url https://powo.science.kew.org/taxon/urn:lsid:ipni.org:names:30000000-2
"""
from __future__ import annotations

import argparse
import json
import logging
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple
from urllib.parse import urlparse

CONFIG_PATH = Path(__file__).resolve().parents[1] / "config.json"


def get_logger(name: str) -> logging.Logger:
    logger = logging.getLogger(name)
    if not logger.handlers:
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            fmt="%(asctime)s | %(levelname)s | %(name)s | %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        logger.setLevel(logging.INFO)
    return logger


logger = get_logger("source_validator")


@dataclass(frozen=True)
class ValidationResult:
    is_valid: bool
    reason: Optional[str] = None


class SourceValidator:
    def __init__(self, config_path: Path = CONFIG_PATH) -> None:
        self.config = self._load_config(config_path)
        self.trusted_orgs = self._build_trusted_orgs_index(self.config)
        self.trusted_domains = self._build_trusted_domains_default()
        # Allow user-specified domains if present in config
        for key in ("trustedDomains", "trusted_domains"):
            extra = self.config.get(key)
            if isinstance(extra, list):
                for d in extra:
                    if isinstance(d, str):
                        self.trusted_domains.add(d.lower())

    @staticmethod
    def _load_config(config_path: Path) -> Dict:
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except FileNotFoundError:
            logger.warning("Config file not found at %s; using defaults", config_path)
            return {}
        except json.JSONDecodeError as e:
            logger.error("Failed to parse config.json: %s", e)
            return {}

    @staticmethod
    def _build_trusted_orgs_index(config: Dict) -> List[str]:
        trusted_sources = config.get("trustedSources", {})
        flat: List[str] = []
        for _, items in (trusted_sources.items() if isinstance(trusted_sources, dict) else []):
            for s in items:
                if isinstance(s, str):
                    flat.append(s.lower())
        # Deduplicate while keeping order
        seen = set()
        unique = []
        for s in flat:
            if s not in seen:
                seen.add(s)
                unique.append(s)
        return unique

    @staticmethod
    def _build_trusted_domains_default() -> set:
        # Curated list mapping known orgs to domains
        return set(d.lower() for d in [
            "www.rhs.org.uk",
            "powo.science.kew.org",
            "plants.usda.gov",
            "www.missouribotanicalgarden.org",
            "pubchem.ncbi.nlm.nih.gov",
            "www.chemspider.com",
            "www.who.int",
            "www.efsa.europa.eu",
            "www.fda.gov",
            "www.ncbi.nlm.nih.gov",
            "www.sciencedirect.com",
            "doi.org",
            "academic.oup.com",
            "link.springer.com",
            "www.frontiersin.org",
        ])

    @staticmethod
    def _extract_domain(url: str) -> Optional[str]:
        try:
            return urlparse(url).netloc.lower()
        except Exception:
            return None

    def is_trusted_org(self, org: Optional[str]) -> ValidationResult:
        if not org:
            return ValidationResult(False, "Missing org")
        org_l = org.lower()
        for trusted in self.trusted_orgs:
            if trusted in org_l:
                return ValidationResult(True)
        return ValidationResult(False, f"Org not in trusted list: {org}")

    def is_trusted_url(self, url: Optional[str]) -> ValidationResult:
        if not url:
            return ValidationResult(False, "Missing url")
        domain = self._extract_domain(url)
        if not domain:
            return ValidationResult(False, "Invalid URL")
        if domain in self.trusted_domains:
            return ValidationResult(True)
        # Also allow scholarly DOIs and NCBI subdomains
        if domain.endswith(".nih.gov") or domain.endswith(".gov"):
            return ValidationResult(True)
        return ValidationResult(False, f"Domain not trusted: {domain}")

    def validate_reference(self, ref: Dict) -> ValidationResult:
        # Expect keys: title, org, year, url
        title = ref.get("title")
        org = ref.get("org")
        url = ref.get("url")
        if not title or not isinstance(title, str):
            return ValidationResult(False, "Missing title")
        org_ok = self.is_trusted_org(org)
        url_ok = self.is_trusted_url(url)
        if org_ok.is_valid or url_ok.is_valid:
            return ValidationResult(True)
        return ValidationResult(False, f"Untrusted org/url: {org_ok.reason}; {url_ok.reason}")

    def validate_references(self, refs: Iterable[Dict]) -> Tuple[List[Dict], List[Tuple[Dict, str]]]:
        valid: List[Dict] = []
        invalid: List[Tuple[Dict, str]] = []
        for r in refs:
            result = self.validate_reference(r)
            if result.is_valid:
                valid.append(r)
            else:
                invalid.append((r, result.reason or "Invalid"))
        return valid, invalid

    def validate_json_file(self, json_path: Path) -> Tuple[int, int]:
        try:
            with open(json_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            logger.error("Failed to read %s: %s", json_path, e)
            return 0, 0
        refs = data.get("references") or []
        if not isinstance(refs, list):
            logger.error("`references` must be a list in %s", json_path)
            return 0, 0
        valid, invalid = self.validate_references(refs)
        for ref, reason in invalid:
            logger.warning("Invalid reference: %s | reason=%s", ref, reason)
        logger.info("Validated %d references: %d valid, %d invalid", len(refs), len(valid), len(invalid))
        return len(valid), len(invalid)


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate Herbiverse references against trusted sources")
    parser.add_argument("--file", type=str, help="Path to JSON file containing `references`")
    parser.add_argument("--url", type=str, nargs="*", help="One or more URLs to quickly check")
    args = parser.parse_args()

    validator = SourceValidator()

    if args.file:
        json_path = Path(args.file)
        validator.validate_json_file(json_path)

    if args.url:
        for u in args.url:
            res = validator.is_trusted_url(u)
            logger.info("%s => %s (%s)", u, "trusted" if res.is_valid else "untrusted", res.reason or "ok")


if __name__ == "__main__":
    main()
