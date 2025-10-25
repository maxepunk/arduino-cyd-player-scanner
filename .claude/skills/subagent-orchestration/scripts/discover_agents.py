#!/usr/bin/env python3
"""
Discover and catalog available subagents.

This script scans project-level and user-level agent directories,
parses agent definitions, and creates a catalog of available agents
with their capabilities and use cases.
"""

import os
import sys
import yaml
import json
from pathlib import Path
from typing import Dict, List, Optional


def parse_frontmatter(content: str) -> Dict:
    """Extract YAML frontmatter from markdown content."""
    if not content.startswith('---'):
        return {}
    
    try:
        # Find the closing ---
        end_idx = content.find('---', 3)
        if end_idx == -1:
            return {}
        
        frontmatter = content[3:end_idx].strip()
        return yaml.safe_load(frontmatter) or {}
    except Exception as e:
        print(f"Warning: Failed to parse frontmatter: {e}", file=sys.stderr)
        return {}


def discover_agents_in_directory(directory: Path) -> List[Dict]:
    """Discover all agent definitions in a directory."""
    agents = []
    
    if not directory.exists():
        return agents
    
    # Check for agent definition files
    for agent_file in directory.glob('*.md'):
        try:
            content = agent_file.read_text(encoding='utf-8')
            frontmatter = parse_frontmatter(content)
            
            if not frontmatter:
                continue
            
            # Extract agent information
            agent_info = {
                'name': frontmatter.get('name', agent_file.stem),
                'description': frontmatter.get('description', 'No description provided'),
                'tools': frontmatter.get('tools', 'All tools (inherited)'),
                'model': frontmatter.get('model', 'Inherited from main'),
                'file_path': str(agent_file),
                'location': 'project' if '.claude/agents' in str(agent_file) else 'user',
                'system_prompt_preview': extract_prompt_preview(content, frontmatter)
            }
            
            agents.append(agent_info)
            
        except Exception as e:
            print(f"Warning: Failed to parse {agent_file}: {e}", file=sys.stderr)
    
    return agents


def extract_prompt_preview(content: str, frontmatter: Dict) -> str:
    """Extract first 200 chars of system prompt for preview."""
    # Find the end of frontmatter
    end_idx = content.find('---', 3)
    if end_idx == -1:
        return "No system prompt found"
    
    # Get content after frontmatter
    prompt = content[end_idx + 3:].strip()
    
    # Return first 200 characters
    if len(prompt) <= 200:
        return prompt
    return prompt[:197] + "..."


def categorize_agents(agents: List[Dict]) -> Dict[str, List[Dict]]:
    """Categorize agents by domain/purpose."""
    categories = {
        'code_analysis': [],
        'implementation': [],
        'testing': [],
        'security': [],
        'performance': [],
        'research': [],
        'documentation': [],
        'infrastructure': [],
        'data': [],
        'other': []
    }
    
    keywords = {
        'code_analysis': ['review', 'analyze', 'audit', 'inspect', 'examine'],
        'implementation': ['implement', 'create', 'build', 'develop', 'code', 'write'],
        'testing': ['test', 'qa', 'coverage', 'validation'],
        'security': ['security', 'vulnerability', 'auth', 'encrypt', 'compliance'],
        'performance': ['performance', 'optimize', 'profile', 'benchmark', 'speed'],
        'research': ['research', 'investigate', 'explore', 'discover', 'study'],
        'documentation': ['document', 'doc', 'readme', 'guide', 'explain'],
        'infrastructure': ['deploy', 'infrastructure', 'cloud', 'kubernetes', 'docker'],
        'data': ['data', 'database', 'sql', 'analytics', 'etl']
    }
    
    for agent in agents:
        desc_lower = (agent['description'] + ' ' + agent['name']).lower()
        categorized = False
        
        for category, keywords_list in keywords.items():
            if any(keyword in desc_lower for keyword in keywords_list):
                categories[category].append(agent)
                categorized = True
                break
        
        if not categorized:
            categories['other'].append(agent)
    
    # Remove empty categories
    return {k: v for k, v in categories.items() if v}


def print_agent_catalog(agents: List[Dict], format: str = 'text'):
    """Print agent catalog in specified format."""
    if format == 'json':
        print(json.dumps(agents, indent=2))
        return
    
    if not agents:
        print("No agents found.")
        return
    
    # Categorize agents
    categorized = categorize_agents(agents)
    
    print(f"\n{'='*80}")
    print(f"DISCOVERED {len(agents)} SUBAGENTS")
    print(f"{'='*80}\n")
    
    for category, category_agents in categorized.items():
        print(f"\n{category.upper().replace('_', ' ')} ({len(category_agents)} agents)")
        print("-" * 80)
        
        for agent in category_agents:
            print(f"\n📦 {agent['name']}")
            print(f"   Location: {agent['location']}")
            print(f"   Description: {agent['description']}")
            print(f"   Tools: {agent['tools']}")
            print(f"   Model: {agent['model']}")
            print(f"   Path: {agent['file_path']}")


def print_usage_summary(agents: List[Dict]):
    """Print summary of how to use discovered agents."""
    print(f"\n{'='*80}")
    print("USAGE SUMMARY")
    print(f"{'='*80}\n")
    
    print("To use an agent, either:")
    print("  1. Let Claude automatically invoke based on task context")
    print("  2. Explicitly request: 'Use the <agent-name> subagent to...'")
    print("  3. Via Task tool: Task(name='agent-name', prompt='...')\n")
    
    # Show agents by trigger strength
    proactive = [a for a in agents if 'PROACTIVE' in a['description'] or 'MUST BE USED' in a['description']]
    if proactive:
        print(f"Agents with strong automatic triggers ({len(proactive)}):")
        for agent in proactive:
            print(f"  • {agent['name']}")
    
    print()


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Discover and catalog available subagents'
    )
    parser.add_argument(
        '--format', 
        choices=['text', 'json'], 
        default='text',
        help='Output format (default: text)'
    )
    parser.add_argument(
        '--project-dir',
        type=str,
        default='.claude/agents',
        help='Project-level agents directory (default: .claude/agents)'
    )
    parser.add_argument(
        '--user-dir',
        type=str,
        default='~/.claude/agents',
        help='User-level agents directory (default: ~/.claude/agents)'
    )
    
    args = parser.parse_args()
    
    # Discover agents
    all_agents = []
    
    # Project-level agents
    project_dir = Path(args.project_dir).expanduser()
    project_agents = discover_agents_in_directory(project_dir)
    all_agents.extend(project_agents)
    
    # User-level agents
    user_dir = Path(args.user_dir).expanduser()
    user_agents = discover_agents_in_directory(user_dir)
    all_agents.extend(user_agents)
    
    # Print catalog
    print_agent_catalog(all_agents, args.format)
    
    if args.format == 'text' and all_agents:
        print_usage_summary(all_agents)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
