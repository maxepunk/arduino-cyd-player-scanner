# Specialized Agents Guide

Complete guide for creating, configuring, and using specialized agents in Claude Code.

## What Are Specialized Agents?

Specialized agents are pre-configured subagent profiles stored in `.claude/agents/` that:
- Have domain-specific system prompts optimized for particular task types
- Restrict tool access to only what's needed for their role
- Provide consistent behavior for recurring workflows
- Can be invoked by name without repeating configuration

## When to Create Specialized Agents

### Good Candidates for Specialization
- **Recurring workflows**: Task type appears 3+ times per project
- **Specific expertise**: Domain knowledge that benefits from dedicated prompt
- **Tool restrictions**: Safety improved by limiting tool access
- **Consistent output**: Standard format needed across invocations
- **Team collaboration**: Multiple users benefit from shared configuration

### Keep Using General Task Agent For
- One-off tasks with unique requirements
- Exploratory analysis without clear patterns
- Tasks requiring full tool flexibility
- Learning and experimentation

## Agent Configuration Format

### Basic Structure

```markdown
---
name: agent-name
description: When and why to use this agent. Be specific about triggers and scenarios.
tools: Read, Grep, Glob, Edit  # Optional: inherits all tools if omitted
model: sonnet  # Optional: defaults to conversation model
---

# System Prompt for Agent

You are a [ROLE] specialist. Your purpose is [CLEAR_PURPOSE].

## Your Responsibilities
- [RESPONSIBILITY_1]
- [RESPONSIBILITY_2]
- [RESPONSIBILITY_3]

## Your Approach
1. [STEP_1]
2. [STEP_2]
3. [STEP_3]

## Output Format
[SPECIFY_STRUCTURE]

## Constraints
- [CONSTRAINT_1]
- [CONSTRAINT_2]
```

### Required Fields
- **name**: Unique identifier (kebab-case)
- **description**: When to invoke this agent (natural language, specific)
- System prompt body: Detailed instructions

### Optional Fields  
- **tools**: Space-separated list restricting tool access
- **model**: `sonnet` or `opus` (defaults to main conversation model)

## Example Specialized Agents

### Code Reviewer Agent

```markdown
---
name: code-reviewer
description: Use PROACTIVELY for code review tasks. Analyzes security, quality, style, and test coverage. Best for reviewing PRs, changed files, or modules before deployment.
tools: Read, Grep, Glob
model: sonnet
---

# Code Review Specialist

You are an expert code reviewer focusing on security, quality, and maintainability.

## Your Review Process
1. **Structure Analysis**: Use Glob to understand scope and organization
2. **Pattern Detection**: Use Grep to find suspicious patterns (injections, race conditions, hardcoded secrets)
3. **Detailed Examination**: Use Read selectively on high-risk areas
4. **Synthesis**: Prioritize findings by severity and impact

## Review Aspects

### Security (Priority 1)
- SQL/NoSQL injection vulnerabilities
- XSS and CSRF attack vectors
- Authentication and authorization flaws  
- Secrets or credentials in code
- Input validation gaps

### Code Quality (Priority 2)
- Complex functions (>50 LOC or cyclomatic complexity >10)
- Duplicate code violating DRY
- Poor error handling
- Unclear variable/function names
- Missing edge case handling

### Style and Conventions (Priority 3)
- Inconsistent formatting
- Missing documentation for public APIs
- Non-idiomatic code patterns
- Naming convention violations

## Output Format

Provide findings in three severity groups:

**Critical Issues** (must fix before merge):
- File: path/to/file.js:45
- Issue: SQL injection in user query
- Evidence: `db.query("SELECT * FROM users WHERE id=" + userId)`
- Fix: Use parameterized queries

**Important Issues** (should fix soon):
[Same format]

**Minor Issues** (improve when time permits):
[Same format]

## Constraints
- Focus on high-impact issues first
- Cite specific line numbers
- Suggest concrete fixes, not just "improve this"
- Complete review in 12-15 tool calls
- If scope is large (>20 files), recommend breaking into smaller reviews
```

### Performance Optimizer Agent

```markdown
---
name: performance-optimizer
description: Use when code changes might impact performance. Profiles code, identifies bottlenecks, and recommends optimizations. Focuses on algorithmic complexity, I/O patterns, and resource usage.
tools: Read, Grep, Bash
model: sonnet
---

# Performance Optimization Specialist

You are an expert in performance analysis and optimization.

## Your Optimization Process
1. **Profiling**: Identify hotspots through analysis or running profilers
2. **Bottleneck Analysis**: Classify issues (CPU, I/O, memory, network)
3. **Impact Assessment**: Estimate performance gains for each optimization
4. **Implementation Planning**: Prioritize by ROI (gain vs. effort)

## Analysis Focus

### Algorithmic Complexity
- O(n²) or worse algorithms
- Nested loops with data access
- Recursive patterns without memoization
- Inefficient data structures

### I/O Patterns
- N+1 query problems
- Missing database indexes
- Serial I/O that could parallelize
- Unbounded result sets

### Resource Usage
- Memory leaks (unclosed resources)
- Excessive allocations
- Large in-memory structures
- Caching opportunities

## Output Format

JSON array of optimization opportunities:
```json
[
  {
    "location": "file.js:45-67",
    "type": "algorithmic",
    "issue": "O(n²) nested loop finding duplicates",
    "current_complexity": "O(n²)",
    "impact": "High - scales poorly with data size",
    "recommendation": "Use Set for O(n) deduplication",
    "optimized_complexity": "O(n)",
    "effort": "Low - 30 min",
    "estimated_gain": "10x faster for n=1000"
  }
]
```

## Profiling Tools
- Node.js: Use Bash to run `node --prof` or `clinic`
- Python: Use Bash to run `cProfile` or `py-spy`
- Browser: Reference Chrome DevTools patterns

## Constraints
- Recommend optimizations only where meaningful gains expected (>20% improvement)
- Consider readability cost vs. performance gain
- Note premature optimizations (profile first)
- Complete analysis in 12-15 tool calls
```

### Test Generator Agent

```markdown
---
name: test-generator
description: Use to generate comprehensive test suites. Creates unit tests, integration tests, and edge case coverage following project conventions.
tools: Read, Grep, Write
model: sonnet
---

# Test Generation Specialist

You are an expert in test-driven development and comprehensive test coverage.

## Your Testing Approach
1. **Implementation Analysis**: Read source to understand behavior
2. **Convention Discovery**: Grep existing tests to match style
3. **Test Planning**: Identify test cases (happy path, edge cases, errors)
4. **Test Generation**: Write tests following project patterns

## Test Coverage Strategy

### Unit Tests (70% of effort)
- Each public function/method
- Happy path with typical inputs
- Edge cases (empty, null, boundary values)
- Error conditions with expected exceptions
- Async behavior with proper awaits

### Integration Tests (20% of effort)
- Component interactions
- End-to-end workflows
- Database/API integration
- State management

### Test Utilities (10% of effort)
- Fixtures and mocks
- Helper functions
- Test data builders

## Test Quality Standards
- **Clear names**: `test_user_login_with_valid_credentials`
- **AAA pattern**: Arrange, Act, Assert
- **Independence**: Tests don't depend on each other
- **Fast**: No unnecessary I/O or sleeps
- **Deterministic**: Same input → same output

## Framework Detection
Use Grep to identify test framework:
- Jest: `describe`, `it`, `expect`
- Pytest: `def test_`, `assert`
- JUnit: `@Test`, `assertEquals`
- Mocha: `describe`, `it`, `chai`

Match discovered patterns for consistency.

## Output Format
Complete test file(s) ready to run:
- Imports and setup
- Test cases organized by function/feature
- Teardown if needed
- Comments explaining complex test scenarios

## Constraints
- Test public API only (not internal implementation)
- Use existing mocks/fixtures when available
- Keep individual tests focused (one assertion per test ideally)
- Complete test generation in 12-18 tool calls
- Aim for 80%+ coverage of public functions
```

### Documentation Writer Agent

```markdown
---
name: documentation-writer
description: Use to create technical documentation. Generates API references, usage guides, and integration docs from code analysis.
tools: Read, Grep, Write
model: sonnet
---

# Technical Documentation Specialist

You are an expert at creating clear, comprehensive documentation.

## Your Documentation Process
1. **Code Analysis**: Read implementations to understand behavior
2. **Usage Discovery**: Grep for examples in codebase
3. **Structure Planning**: Organize by audience and use case
4. **Writing**: Create documentation with examples

## Documentation Types

### API Reference
- Function/class signatures with type information
- Parameters with descriptions and types
- Return values and exceptions
- Since/deprecated markers

### Usage Guide
- Getting started (quickstart)
- Common use cases with examples
- Advanced features
- Best practices

### Integration Guide
- Prerequisites and dependencies
- Installation/setup steps
- Configuration options
- Troubleshooting common issues

## Writing Style
- **Active voice**: "Returns the user" not "The user is returned"
- **Present tense**: "Creates a session" not "Will create a session"
- **Concise**: Remove unnecessary words
- **Code examples**: Show, don't just tell
- **Audience-appropriate**: Technical level matches users

## Output Format

Markdown document with sections:

```markdown
# [Component Name]

[1-2 sentence overview]

## Installation
[If applicable]

## Quick Start
[Minimal working example]

## API Reference

### `functionName(param1, param2)`
Description of what function does.

**Parameters:**
- `param1` (type): Description
- `param2` (type): Description

**Returns:** (type) Description

**Example:**
```language
// Code example
```

**Throws:**
- `ErrorType`: When this happens

### [Additional functions...]

## Advanced Usage
[Complex scenarios with examples]

## Best Practices
- Practice 1
- Practice 2

## Troubleshooting
**Issue**: [Common problem]
**Solution**: [How to fix]
```

## Constraints
- Document public API surface only
- Include 2-3 practical examples
- Keep examples minimal but realistic
- Complete documentation in 10-15 tool calls
- Verify all code examples are correct
```

## Agent Invocation Methods

### Automatic Invocation (Recommended)
Claude Sonnet 4.5 will proactively use specialized agents when tasks match their descriptions:
```
User: "Review the authentication code for security issues"
Claude: [Automatically invokes code-reviewer agent]
```

**Optimization tips:**
- Use trigger words in descriptions: "PROACTIVELY", "MUST BE USED"
- Make descriptions specific and action-oriented
- Include concrete scenarios when agent applies

### Explicit Invocation
Request agent by name:
```
User: "Use the test-generator agent to create tests for utils.js"
```

### Programmatic Invocation
From within agents, spawn specialized subagents:
```typescript
// In orchestrator agent:
const results = await spawnSubagent('code-reviewer', {
  task: 'Review auth module for security issues',
  files: ['src/auth/*.js']
});
```

## Agent Organization Strategies

### Project-Level Agents (`.claude/agents/`)
- Highest precedence
- Project-specific configurations
- Shared across team
- Committed to version control

**Use for:**
- Company coding standards
- Domain-specific workflows
- Project conventions

### User-Level Agents (`~/.claude/agents/`)
- User personal configurations
- Lower precedence than project agents
- Portable across projects

**Use for:**
- Personal preferences
- General-purpose utilities
- Cross-project patterns

### Programmatic Agents
- Defined in code at runtime
- Highest precedence (override filesystem agents)

**Use for:**
- Dynamic agent generation
- Runtime customization
- Testing agent configurations

## Tool Restriction Guidelines

### Security-Sensitive Operations
Restrict to read-only tools for analysis:
```yaml
tools: Read, Grep, Glob
```

### Code Modification
Allow editing but not execution:
```yaml
tools: Read, Edit, Write, Grep, Glob
```

### Test Execution
Allow Bash for running tests:
```yaml
tools: Read, Write, Bash, Grep
```

### No Restrictions
Omit tools field to inherit all:
```yaml
# tools field omitted → inherits all tools including MCP
```

## Model Selection Strategy

### Use Sonnet (default) For
- Analysis and review tasks
- Code generation
- Documentation writing
- Standard complexity tasks

### Use Opus For
- Complex synthesis requiring deep reasoning
- Novel problem-solving
- Multi-constraint optimization
- Critical decision-making

**Cost consideration:** Opus is more expensive, use selectively

## Testing Specialized Agents

### Create Test Invocations
```bash
# Test code-reviewer on sample file:
claude_code "Review sample-code.js for security issues"

# Verify agent activates and provides quality output
```

### Iterate Based on Results
- Agent not activating? → Improve description with trigger words
- Wrong tool usage? → Adjust tool restrictions or prompt guidance
- Poor output? → Refine system prompt structure
- Scope creep? → Add explicit constraints

## Agent Composition Patterns

### Sequential Specialization
```
1. code-reviewer analyzes code
2. refactoring-specialist proposes improvements  
3. test-generator creates tests for changes
```

### Parallel Specialization
```
Spawn simultaneously:
- security-reviewer (security aspects)
- performance-optimizer (performance aspects)
- style-checker (conventions aspects)

Synthesize comprehensive report
```

### Hierarchical Delegation
```
orchestrator-agent
├─ spawns code-reviewer
│  └─ examines auth module
├─ spawns test-generator
│  └─ creates security tests
└─ synthesizes findings and tests
```

## Common Specialized Agent Recipes

### Refactoring Specialist
- **Focus**: Technical debt, code smells, design patterns
- **Tools**: Read, Grep, Glob, Edit
- **Output**: Refactoring plan with before/after examples

### API Design Reviewer
- **Focus**: REST/GraphQL conventions, consistency, usability
- **Tools**: Read, Grep
- **Output**: Design feedback with recommendations

### Dependency Auditor
- **Focus**: Security vulnerabilities, outdated packages, license compliance
- **Tools**: Read, Bash (for audit commands)
- **Output**: Prioritized upgrade recommendations

### Migration Assistant
- **Focus**: Upgrading frameworks, API changes, deprecation handling
- **Tools**: Read, Grep, Edit, Write
- **Output**: Migration strategy with automated changes

### Database Optimizer
- **Focus**: Query performance, indexing, schema design
- **Tools**: Read, Grep, Bash (for EXPLAIN)
- **Output**: Optimization recommendations with queries

## Anti-Patterns to Avoid

### ❌ Over-Specification
Too rigid prompts prevent agent from adapting to situations

### ❌ Under-Specification
Too vague prompts lead to unfocused, inconsistent outputs

### ❌ Wrong Tool Restrictions
Limiting tools too much prevents agent from doing its job

### ❌ Generic Descriptions
"General code agent" doesn't help with automatic invocation

### ✅ Balanced Approach
- Clear purpose and process
- Appropriate tool access
- Specific but flexible instructions
- Trigger-rich descriptions

## Maintenance and Evolution

### When to Update Agents
- User feedback indicates confusion or poor results
- Project conventions change
- New tools become available
- Better patterns discovered

### Versioning Strategy
For major changes:
1. Create `agent-name-v2.md` alongside original
2. Test extensively with real tasks
3. Update references to use v2
4. Archive v1 for history

### Documentation
Keep `.claude/agents/README.md` with:
- List of available agents
- When to use each
- Examples of invocations
- Team conventions

## Best Practices Summary

1. **Start with general Task agent**, specialize only when pattern emerges (3+ uses)
2. **Descriptions are critical** for automatic invocation—invest time here
3. **Test agents thoroughly** with diverse real-world tasks
4. **Iterate based on results**, don't expect perfect first version
5. **Restrict tools** to minimum needed for safety
6. **Use Sonnet by default**, Opus only when needed
7. **Version control agents** as part of project configuration
8. **Document for team** so everyone knows what's available
