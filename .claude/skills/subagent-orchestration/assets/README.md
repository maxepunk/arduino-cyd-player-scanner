# Example Specialized Agent Configurations

This directory contains example specialized agent configurations that you can copy and adapt for your projects.

## How to Use These Examples

1. **Copy to your project**:
   ```bash
   cp assets/code-reviewer-agent.md .claude/agents/
   ```

2. **Customize for your needs**:
   - Adjust tool restrictions
   - Modify output formats
   - Add domain-specific constraints
   - Update conventions to match your project

3. **Test the agent**:
   ```bash
   claude_code "Review my authentication code"
   ```

## Available Example Agents

### code-reviewer-agent.md
**Purpose**: Comprehensive code review covering security, quality, and style  
**Tools**: Read, Grep, Glob (read-only)  
**Best for**: PR reviews, pre-deployment checks, security audits

**Customization tips**:
- Add language-specific security patterns
- Include your team's coding standards
- Adjust severity thresholds

### test-generator-agent.md
**Purpose**: Generate comprehensive test suites with unit and integration coverage  
**Tools**: Read, Grep, Write  
**Best for**: Creating tests for new features, improving coverage

**Customization tips**:
- Specify your test framework in the system prompt
- Add project-specific test helpers
- Define coverage targets

### performance-optimizer-agent.md
**Purpose**: Identify and recommend performance optimizations  
**Tools**: Read, Grep, Bash  
**Best for**: Performance audits, bottleneck analysis, optimization planning

**Customization tips**:
- Add platform-specific profiling tools
- Define performance SLAs
- Include architecture-specific patterns

## Creating Your Own Agents

See `references/specialized-agents.md` for a complete guide to creating custom specialized agents.

### Quick Start Template

```markdown
---
name: your-agent-name
description: Clear description of when to use this agent with trigger keywords
tools: Read, Grep, Write  # Optional: omit to inherit all tools
model: sonnet  # Optional: sonnet or opus
---

# Your Agent Role

You are a [ROLE] specialist focused on [PURPOSE].

## Your Process
1. [STEP_1]
2. [STEP_2]
3. [STEP_3]

## Output Format
[SPECIFY_STRUCTURE]

## Constraints
- [CONSTRAINT_1]
- [CONSTRAINT_2]
```

## Combining Multiple Agents

You can use multiple specialized agents in sequence or parallel:

```bash
# Sequential: Review → Fix → Test
claude_code "Review auth.js, then fix issues, then generate tests"

# Parallel: Multiple aspects simultaneously
claude_code "Analyze this module for both security and performance"
```

## Best Practices

1. **Start with examples**, customize incrementally
2. **Test with real tasks** before committing to version control
3. **Document agents** in your project README
4. **Version control** agent configs so team shares patterns
5. **Iterate based on results** - agents improve with use

## Agent Priority

When multiple agents could apply:
1. Project-level (`.claude/agents/`) - highest priority
2. User-level (`~/.claude/agents/`) - medium priority  
3. Built-in agents - lowest priority

Name your agents clearly to avoid conflicts.
