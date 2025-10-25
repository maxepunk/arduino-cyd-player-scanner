#!/usr/bin/env python3
"""
Validate subagent definitions.

This script validates agent definition files to ensure they follow
best practices and contain all required fields.
"""

import os
import sys
import yaml
import re
from pathlib import Path
from typing import Dict, List, Tuple


class ValidationResult:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.suggestions = []
    
    def add_error(self, msg: str):
        self.errors.append(msg)
    
    def add_warning(self, msg: str):
        self.warnings.append(msg)
    
    def add_suggestion(self, msg: str):
        self.suggestions.append(msg)
    
    def is_valid(self) -> bool:
        return len(self.errors) == 0
    
    def print_report(self, agent_name: str):
        """Print validation report."""
        print(f"\n{'='*80}")
        print(f"VALIDATION REPORT: {agent_name}")
        print(f"{'='*80}\n")
        
        if self.is_valid():
            print("✅ PASSED - Agent definition is valid\n")
        else:
            print("❌ FAILED - Agent definition has errors\n")
        
        if self.errors:
            print(f"ERRORS ({len(self.errors)}):")
            for i, error in enumerate(self.errors, 1):
                print(f"  {i}. {error}")
            print()
        
        if self.warnings:
            print(f"WARNINGS ({len(self.warnings)}):")
            for i, warning in enumerate(self.warnings, 1):
                print(f"  {i}. {warning}")
            print()
        
        if self.suggestions:
            print(f"SUGGESTIONS ({len(self.suggestions)}):")
            for i, suggestion in enumerate(self.suggestions, 1):
                print(f"  {i}. {suggestion}")
            print()


def parse_agent_file(file_path: Path) -> Tuple[Dict, str]:
    """Parse agent file and return frontmatter and system prompt."""
    try:
        content = file_path.read_text(encoding='utf-8')
    except Exception as e:
        raise ValueError(f"Failed to read file: {e}")
    
    if not content.startswith('---'):
        raise ValueError("File must start with YAML frontmatter (---)")
    
    # Find the closing ---
    end_idx = content.find('---', 3)
    if end_idx == -1:
        raise ValueError("Unclosed YAML frontmatter (missing closing ---)")
    
    # Parse frontmatter
    frontmatter_str = content[3:end_idx].strip()
    try:
        frontmatter = yaml.safe_load(frontmatter_str) or {}
    except yaml.YAMLError as e:
        raise ValueError(f"Invalid YAML frontmatter: {e}")
    
    # Extract system prompt
    system_prompt = content[end_idx + 3:].strip()
    
    return frontmatter, system_prompt


def validate_name(name: str, result: ValidationResult):
    """Validate agent name."""
    if not name:
        result.add_error("Missing 'name' field in frontmatter")
        return
    
    # Check format (kebab-case)
    if not re.match(r'^[a-z][a-z0-9-]*[a-z0-9]$', name):
        result.add_error(
            f"Name '{name}' should be kebab-case (lowercase with hyphens): "
            "e.g., 'performance-optimizer', 'security-scanner'"
        )
    
    # Check length
    if len(name) > 50:
        result.add_warning(f"Name is quite long ({len(name)} chars). Consider shortening.")
    
    # Check for generic names
    generic_names = ['helper', 'agent', 'assistant', 'worker', 'handler']
    if name in generic_names or name.startswith('agent-'):
        result.add_warning(
            f"Name '{name}' is generic. Consider more specific name describing the agent's purpose."
        )


def validate_description(description: str, result: ValidationResult):
    """Validate agent description."""
    if not description:
        result.add_error("Missing 'description' field in frontmatter")
        return
    
    # Check length
    if len(description) < 20:
        result.add_warning(
            f"Description is very short ({len(description)} chars). "
            "Add more detail about when and how to use this agent."
        )
    
    if len(description) > 500:
        result.add_warning(
            f"Description is very long ({len(description)} chars). "
            "Consider being more concise while keeping key trigger conditions."
        )
    
    # Check for trigger phrases
    trigger_phrases = ['PROACTIVE', 'MUST BE USED', 'automatically invoke', 'required for']
    has_trigger = any(phrase.lower() in description.lower() for phrase in trigger_phrases)
    
    if not has_trigger:
        result.add_suggestion(
            "Consider adding explicit trigger conditions like 'Use PROACTIVELY when...' "
            "or 'MUST BE USED for...' to help Claude know when to invoke this agent."
        )
    
    # Check for generic descriptions
    generic_phrases = ['helps with', 'assists in', 'general purpose']
    if any(phrase.lower() in description.lower() for phrase in generic_phrases):
        result.add_warning(
            "Description contains generic phrases. Be more specific about capabilities and use cases."
        )


def validate_tools(tools, result: ValidationResult):
    """Validate tools configuration."""
    if tools is None:
        result.add_suggestion(
            "No 'tools' field specified - agent will inherit all tools. "
            "Consider restricting tools if agent has focused purpose."
        )
        return
    
    valid_tools = ['Read', 'Edit', 'Bash', 'Grep', 'Task']
    
    if isinstance(tools, str):
        result.add_error(f"'tools' should be a list, not a string: {tools}")
        return
    
    if not isinstance(tools, list):
        result.add_error(f"'tools' should be a list, got {type(tools).__name__}")
        return
    
    for tool in tools:
        if tool not in valid_tools:
            result.add_warning(
                f"Tool '{tool}' is not a standard tool. "
                f"Valid tools: {', '.join(valid_tools)} (plus MCP tools)"
            )


def validate_model(model: str, result: ValidationResult):
    """Validate model selection."""
    if not model:
        result.add_suggestion(
            "No 'model' field specified - agent will use main conversation model. "
            "Consider specifying 'opus', 'sonnet', or 'haiku' based on task complexity."
        )
        return
    
    valid_models = ['opus', 'sonnet', 'haiku']
    if model not in valid_models:
        result.add_warning(
            f"Model '{model}' is not standard. Valid models: {', '.join(valid_models)}"
        )


def validate_system_prompt(prompt: str, result: ValidationResult):
    """Validate system prompt structure and content."""
    if not prompt:
        result.add_error("Missing system prompt after frontmatter")
        return
    
    if len(prompt) < 50:
        result.add_error(
            f"System prompt is very short ({len(prompt)} chars). "
            "Provide detailed instructions for the agent's role and approach."
        )
    
    if len(prompt) > 5000:
        result.add_warning(
            f"System prompt is very long ({len(prompt)} chars). "
            "Consider moving detailed reference material to separate files."
        )
    
    # Check for structured approach
    numbered_steps = re.findall(r'^\d+\.', prompt, re.MULTILINE)
    if len(numbered_steps) == 0:
        result.add_suggestion(
            "Consider structuring the prompt with numbered steps: "
            "'When invoked: 1. First step 2. Second step ...'"
        )
    
    # Check for role definition
    role_patterns = ['You are', 'You\'re', 'Your role']
    has_role = any(pattern.lower() in prompt.lower() for pattern in role_patterns)
    if not has_role:
        result.add_suggestion(
            "Consider starting with explicit role definition: 'You are a [role] with expertise in...'"
        )
    
    # Check for constraints/boundaries
    boundary_keywords = ['constraint', 'boundary', 'scope', 'do not', 'avoid', 'only']
    has_boundaries = any(keyword.lower() in prompt.lower() for keyword in boundary_keywords)
    if not has_boundaries:
        result.add_suggestion(
            "Consider adding explicit constraints or scope boundaries: "
            "'CONSTRAINTS: Only modify...' or 'DO NOT: Change...'"
        )
    
    # Check for output format
    output_keywords = ['output', 'return', 'provide', 'format', 'structure']
    has_output = any(keyword.lower() in prompt.lower() for keyword in output_keywords)
    if not has_output:
        result.add_suggestion(
            "Consider specifying expected output format: 'OUTPUT FORMAT: ...'"
        )


def validate_agent(file_path: Path) -> ValidationResult:
    """Validate a single agent definition file."""
    result = ValidationResult()
    
    # Check file extension
    if file_path.suffix != '.md':
        result.add_error(f"Agent file must have .md extension, got {file_path.suffix}")
        return result
    
    # Parse file
    try:
        frontmatter, system_prompt = parse_agent_file(file_path)
    except ValueError as e:
        result.add_error(str(e))
        return result
    
    # Validate frontmatter fields
    validate_name(frontmatter.get('name', ''), result)
    validate_description(frontmatter.get('description', ''), result)
    validate_tools(frontmatter.get('tools'), result)
    validate_model(frontmatter.get('model', ''), result)
    
    # Validate system prompt
    validate_system_prompt(system_prompt, result)
    
    return result


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Validate subagent definition files'
    )
    parser.add_argument(
        'files',
        nargs='+',
        help='Agent definition file(s) to validate'
    )
    parser.add_argument(
        '--strict',
        action='store_true',
        help='Treat warnings as errors'
    )
    
    args = parser.parse_args()
    
    all_valid = True
    
    for file_path_str in args.files:
        file_path = Path(file_path_str)
        
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            all_valid = False
            continue
        
        result = validate_agent(file_path)
        result.print_report(file_path.name)
        
        if not result.is_valid():
            all_valid = False
        
        if args.strict and result.warnings:
            all_valid = False
    
    if all_valid:
        print(f"\n✅ All agent definitions are valid!\n")
        return 0
    else:
        print(f"\n❌ Some agent definitions have issues. Please fix and re-validate.\n")
        return 1


if __name__ == '__main__':
    sys.exit(main())
