# Agent Creation Patterns

This reference provides comprehensive patterns and best practices for creating effective subagent definitions.

## Agent Definition Structure

Every subagent requires:

```markdown
---
name: unique-identifier
description: Natural language purpose with trigger conditions
tools: [Read, Edit, Bash, Grep]  # Optional, inherits all if omitted
model: sonnet  # Optional, defaults to main model
---

System prompt defining role, approach, and boundaries
```

## Required Fields

### Name

**Purpose:** Unique identifier for the agent

**Guidelines:**
- Use kebab-case: `performance-optimizer`, `security-scanner`
- Be descriptive and specific
- Avoid generic names like `helper` or `agent1`
- Keep under 30 characters

**Examples:**
- ✅ `frontend-react-specialist`
- ✅ `api-security-auditor`
- ❌ `agent-1`
- ❌ `helper`

### Description

**Purpose:** Tells Claude when and why to use this agent

**Critical:** This is how Claude selects agents automatically. Make it action-oriented and specific.

**Formula:**
```
[ACTION VERB] [SPECIFIC DOMAIN] [TRIGGER CONDITIONS]
```

**Trigger Phrases:**
- "Use PROACTIVELY when..."
- "MUST BE USED for..."
- "Automatically invoke when..."
- "Required for..."

**Examples:**

✅ **Strong descriptions:**
```yaml
description: Use PROACTIVELY when analyzing React components for performance bottlenecks, render optimization, or bundle size issues

description: MUST BE USED for any security audit involving authentication, authorization, input validation, or cryptographic operations

description: Automatically invoke when writing or reviewing API endpoints to ensure proper error handling, rate limiting, and documentation
```

❌ **Weak descriptions:**
```yaml
description: Helps with frontend code

description: Security stuff

description: General purpose coding assistant
```

## System Prompt Design

### Single Responsibility Principle

**Each agent should have ONE clear purpose.**

**Example:**

✅ **Focused:**
```markdown
You are a performance optimization specialist focused exclusively on identifying 
and fixing performance bottlenecks in Python code. Do not handle security, 
style, or functionality—only performance.
```

❌ **Too broad:**
```markdown
You are a code quality specialist who handles performance, security, style, 
testing, documentation, and architecture.
```

### Structured Prompt Template

```markdown
You are [ROLE WITH EXPERTISE].

When invoked:
1. [FIRST STEP - usually analysis/assessment]
2. [SECOND STEP - usually identification/planning]
3. [THIRD STEP - usually implementation/action]
4. [FINAL STEP - usually verification/reporting]

[SPECIFIC CONSTRAINTS OR BOUNDARIES]

[EXPECTED OUTPUT FORMAT]

[TOOL USAGE GUIDANCE IF RELEVANT]
```

### Complete Example

```markdown
---
name: react-performance-optimizer
description: Use PROACTIVELY when React components show performance issues, unnecessary re-renders, or large bundle sizes
tools: [Read, Edit, Bash]
model: sonnet
---

You are a React performance optimization specialist with deep expertise in 
React rendering behavior, memoization strategies, and bundle optimization.

When invoked:
1. Profile the component(s) to identify performance characteristics:
   - Render frequency and triggers
   - Props and state usage patterns
   - Child component relationships
   - Bundle size contributions

2. Identify specific bottlenecks:
   - Unnecessary re-renders
   - Missing memoization opportunities
   - Inefficient data structures
   - Large dependency imports

3. Implement targeted optimizations:
   - Apply React.memo() where beneficial
   - Use useMemo() and useCallback() appropriately
   - Optimize data structures and algorithms
   - Split or lazy-load large components

4. Verify improvements:
   - Measure render counts before/after
   - Check bundle size impact
   - Ensure functionality unchanged

CONSTRAINTS:
- Only modify performance-related code
- Do not change component functionality or API
- Preserve all existing tests
- Add comments explaining optimization rationale

OUTPUT FORMAT:
- List of changes made
- Performance impact metrics
- Before/after comparison
- Recommendations for further optimization

TOOLS:
- Use Read to analyze component files
- Use Edit to apply optimizations
- Use Bash to run build analysis (webpack-bundle-analyzer)
```

## Tool Configuration

### Inheritance vs. Restriction

**Default:** Agents inherit ALL tools from parent (including MCP tools)

**Restrict when:**
- Security concerns (prevent file system access)
- Focus needs (only search, no editing)
- Cost control (prevent expensive operations)

**Examples:**

```yaml
# Full access (default)
tools: [Read, Edit, Bash, Grep, web_search, ...]

# Read-only analyst
tools: [Read, Grep]

# Editor only
tools: [Read, Edit]

# Researcher only
tools: [web_search, web_fetch]
```

## Model Selection

### Available Models

- `opus`: Most capable, highest cost, best for complex reasoning
- `sonnet`: Balanced capability/cost, recommended for most agents
- `haiku`: Fast and economical, good for simple tasks

### Selection Guidelines

**Use Opus when:**
- Complex reasoning required (architecture decisions, debugging)
- High accuracy critical (security audits, production code)
- Nuanced understanding needed (business logic, requirements)

**Use Sonnet when:**
- Standard coding tasks (implementation, refactoring)
- Analysis and research (code review, documentation)
- Most agent tasks (default choice)

**Use Haiku when:**
- Simple, repetitive tasks (formatting, basic validation)
- High-volume operations (batch processing)
- Cost optimization critical (many parallel agents)

**Example:**

```yaml
# Complex architectural analysis
model: opus

# Standard code review
model: sonnet

# Simple style checking
model: haiku
```

## Agent Role Patterns

### 1. Specialist Pattern

**Purpose:** Deep expertise in narrow domain

**When:** Task requires specialized knowledge

**Example:**
```yaml
name: kubernetes-networking-specialist
description: MUST BE USED for debugging Kubernetes networking issues involving DNS, services, ingress, or network policies
```

### 2. Analyst Pattern

**Purpose:** Research and synthesis without modification

**When:** Need information gathering and analysis

**Example:**
```yaml
name: codebase-dependency-analyst
description: Use PROACTIVELY when assessing dependency relationships, import patterns, or architectural boundaries
tools: [Read, Grep]
```

### 3. Implementer Pattern

**Purpose:** Execute specific implementation tasks

**When:** Clear implementation requirements defined

**Example:**
```yaml
name: test-suite-implementer
description: Automatically invoke when adding tests for existing functionality based on provided specifications
tools: [Read, Edit, Bash]
```

### 4. Auditor Pattern

**Purpose:** Verification and validation

**When:** Need quality assurance or compliance checking

**Example:**
```yaml
name: api-contract-auditor
description: MUST BE USED when verifying API implementations match OpenAPI specifications
tools: [Read, Bash]
```

### 5. Orchestrator Pattern

**Purpose:** Coordinate other agents

**When:** Complex multi-stage workflows

**Example:**
```yaml
name: full-stack-feature-orchestrator
description: Use for implementing complete features spanning frontend, backend, and database layers
```

## Context Boundary Setting

### Clear Scope Definition

**Define what the agent SHOULD and SHOULD NOT do:**

```markdown
SCOPE:
- DO: Analyze performance bottlenecks
- DO: Suggest optimization strategies
- DO: Implement approved optimizations
- DO NOT: Modify functionality
- DO NOT: Change APIs
- DO NOT: Touch test files (unless performance tests)
```

### Input/Output Contracts

**Specify expected inputs and outputs:**

```markdown
INPUTS:
- Component file path(s)
- Performance criteria (optional)
- Optimization constraints (optional)

OUTPUTS:
- Performance analysis report
- Recommended optimizations with rationale
- Implementation of approved changes
- Before/after metrics
```

## Progressive Disclosure in Agents

For agents needing extensive knowledge, use progressive disclosure:

```markdown
You are a Django REST Framework specialist.

For basic operations: Use your general Django knowledge.

For advanced serializer patterns: Read references/serializers.md
For authentication strategies: Read references/auth.md  
For performance optimization: Read references/performance.md
```

This keeps agent prompts lean while providing access to deep knowledge when needed.

## Testing Agent Definitions

### Validation Checklist

- [ ] Name is unique and descriptive
- [ ] Description includes trigger conditions
- [ ] System prompt has clear structure
- [ ] Single responsibility is maintained
- [ ] Tools are appropriate for role
- [ ] Model selection justified
- [ ] Scope boundaries are explicit
- [ ] Expected outputs defined

### Test Scenarios

Create test cases representing typical invocations:

```markdown
TEST SCENARIOS:

1. "Review this React component for performance issues"
   Expected: Agent invoked automatically
   Result: Performance analysis with recommendations

2. "Add a feature to this component"
   Expected: Agent NOT invoked (not performance-related)
   Result: Main agent handles directly

3. "Why is this component rendering so much?"
   Expected: Agent invoked automatically
   Result: Render analysis with optimization plan
```

## Common Pitfalls

### ❌ Too Broad

```yaml
name: code-helper
description: Helps with code
```

**Problem:** No clear triggers, overlaps with everything

### ❌ Too Narrow

```yaml
name: fix-import-statement-at-line-47-specialist
description: Fixes the import at line 47
```

**Problem:** Uselessly specific, won't reuse

### ❌ Missing Triggers

```yaml
description: Handles security audits
```

**Problem:** Claude won't know when to invoke automatically

### ❌ Vague Instructions

```markdown
You help with code. Do your best. Be helpful.
```

**Problem:** No structure, unclear approach

### ✅ Well-Designed

```yaml
name: api-rate-limiting-implementer
description: Use PROACTIVELY when implementing or reviewing API endpoints that need rate limiting, quota management, or throttling

---

You are an API rate limiting specialist focused on implementing production-ready 
rate limiting strategies.

When invoked:
1. Analyze endpoint requirements (expected traffic, user tiers, abuse vectors)
2. Design appropriate rate limiting strategy (fixed window, sliding window, token bucket)
3. Implement rate limiting with proper error responses and headers
4. Add monitoring and alerting hooks
5. Document rate limits for API consumers

CONSTRAINTS:
- Use Redis for distributed rate limiting
- Follow RFC 6585 for 429 responses
- Include Retry-After headers
- Preserve existing endpoint logic

TOOLS:
- Read to analyze endpoint code
- Edit to implement rate limiting
- Bash to verify Redis connectivity
```

## Agent Composition Patterns

### Independent Specialists

```
Task → Spawn multiple independent agents → Collect results → Synthesize

Example: Code review
├─→ style-checker (parallel)
├─→ security-scanner (parallel)  
├─→ performance-analyzer (parallel)
└─→ test-coverage-checker (parallel)
```

### Sequential Pipeline

```
Task → Agent 1 → Result 1 → Agent 2 → Result 2 → Agent 3 → Final

Example: Feature implementation
└─→ requirements-analyst
    └─→ architecture-designer
        └─→ implementation-engineer
            └─→ test-suite-creator
```

### Hierarchical Coordination

```
Task → Orchestrator Agent
       ├─→ Frontend Agent
       │   ├─→ React Specialist
       │   └─→ CSS Specialist
       └─→ Backend Agent
           ├─→ API Specialist
           └─→ Database Specialist
```

## Versioning and Evolution

### Iterative Improvement

1. **Start simple:** Basic role definition
2. **Deploy and observe:** See how agent performs
3. **Identify patterns:** Common issues or missed triggers
4. **Refine prompt:** Add constraints or guidance
5. **Expand capabilities:** Add tools or knowledge references
6. **Document lessons:** Update this guide

### Agent Metadata

Consider adding metadata for tracking:

```yaml
version: 2.1
created: 2025-01-15
updated: 2025-03-20
author: team-platform
tested_with: [sonnet-4, opus-4]
success_rate: 94%
avg_tokens: 12500
```

## Summary Principles

1. **Single responsibility** - One agent, one purpose
2. **Clear triggers** - Explicit description of when to use
3. **Structured prompts** - Step-by-step approach
4. **Appropriate tools** - Only what's needed
5. **Right model** - Match capability to task
6. **Explicit boundaries** - Define scope clearly
7. **Expected outputs** - Specify deliverables
8. **Progressive disclosure** - Link to detailed references when needed
