---
name: subagent-orchestration
description: Use PROACTIVELY for complex tasks requiring parallel exploration, context compression, or specialized expertise. Essential for breadth-first research, large-scale analysis, multi-domain problems, or when information exceeds single context window. Provides workflows for creating new agents, selecting existing agents, and orchestrating multiple agents efficiently.
---

# Subagent Orchestration for Claude Code

This skill enables sophisticated multi-agent orchestration in Claude Code, providing decision frameworks, creation patterns, and coordination strategies for deploying subagents effectively.

## Core Philosophy

**Start Simple. Scale Judiciously.**

Multi-agent systems consume 15× more tokens than single-agent work. Only use subagents when value clearly exceeds cost.

## Quick Decision Tree

```
Can you answer directly? → Work directly
Is this simple/well-defined? → Work directly  
Does this need breadth-first exploration? → Consider subagents
Does information exceed context window? → Consider subagents
Is this open-ended and complex? → Consider subagents
Is the value high enough? → Use subagents
```

**Rule of thumb:** If 3+ boxes checked above → strong subagent candidate

## Three Primary Workflows

### Workflow A: Discover and Select Existing Agents

**When:** Suitable specialized agents already exist

**Process:**
1. Discover available agents: `python scripts/discover_agents.py`
2. Match task to agent capabilities
3. Select optimal agents
4. Invoke with clear task descriptions
5. Synthesize results

**Example:**
```markdown
"Review this authentication module for security issues"

1. Discover agents → Find: security-scanner, auth-specialist
2. Match → Both relevant
3. Select → Use both in parallel
4. Invoke:
   Task: Use security-scanner on auth.py
   Task: Use auth-specialist on auth.py
5. Synthesize → Combine findings into report
```

### Workflow B: Create New Specialized Agents

**When:** No suitable agents exist, complex novel task requires specialized workers

**Process:**
1. Analyze task requirements
2. Decompose into specialized roles (single responsibility each)
3. Design agent definitions (read references/agent-creation-patterns.md)
4. Create agent files in .claude/agents/
5. Validate: `python scripts/validate_agent.py <agent-file>`
6. Invoke agents
7. Synthesize results

**Example:**
```markdown
"Build a complete e-commerce checkout flow"

1. Analyze → Need: frontend, backend, payments, security
2. Decompose → 4 specialized agents:
   - checkout-frontend-agent (React UI)
   - checkout-api-agent (REST endpoints)
   - payment-integration-agent (Stripe integration)
   - checkout-security-agent (PCI compliance)
3. Design each agent (see references/agent-creation-patterns.md)
4. Create .claude/agents/checkout-frontend-agent.md (etc.)
5. Validate all definitions
6. Invoke in coordinated sequence
7. Synthesize into complete feature
```

### Workflow C: Hybrid - Mix Existing + New

**When:** Some agents exist, gaps need filling

**Process:**
1. Discover existing agents
2. Identify gaps
3. Create missing agents
4. Orchestrate all agents (existing + new)
5. Synthesize results

## When to Use Subagents

### ✅ Use Subagents For:

**Breadth-first queries**
- Multiple independent exploration paths
- Example: "Compare React, Vue, Angular, Svelte"
- Pattern: Spawn parallel agents per domain

**Information exceeding context**
- Single context window insufficient
- Example: "Analyze security across 50-file codebase"
- Pattern: Agents act as intelligent filters

**Heavy parallelization**
- Independent tasks benefit from concurrency
- Example: Code review (style, security, performance, tests)
- Pattern: All agents run simultaneously

**Open-ended problems**
- Unpredictable steps, flexible exploration needed
- Example: "Design marketing strategy for product"
- Pattern: Autonomous agents adapt approach

### ❌ Don't Use Subagents For:

- Simple, well-defined tasks (token overhead not justified)
- Shared context requirements (duplication wastes tokens)
- Tight interdependencies (coordination overhead exceeds benefits)
- Low-value outcomes (cost-benefit analysis fails)

**Full details:** Read references/decision-framework.md

## Agent Creation Best Practices

### Required Agent Definition Structure

```markdown
---
name: kebab-case-identifier
description: Use PROACTIVELY when [specific conditions]. Action-oriented with trigger phrases.
tools: [Read, Edit, Bash]  # Optional, restricts access
model: sonnet  # Optional: opus/sonnet/haiku
---

You are [ROLE WITH EXPERTISE].

When invoked:
1. [First step - usually analysis]
2. [Second step - usually planning]
3. [Third step - usually implementation]
4. [Final step - usually verification]

CONSTRAINTS:
- [What agent should NOT do]
- [Scope boundaries]

OUTPUT FORMAT:
- [Expected deliverables]
```

### Key Principles

1. **Single responsibility** - One agent, one purpose
2. **Clear triggers** - Use "PROACTIVELY", "MUST BE USED" in description
3. **Structured prompts** - Step-by-step approach
4. **Appropriate tools** - Restrict when needed for focus/security
5. **Right model** - opus (complex), sonnet (standard), haiku (simple)
6. **Explicit boundaries** - Define scope clearly
7. **Expected outputs** - Specify deliverables

**Full patterns:** Read references/agent-creation-patterns.md

## Orchestration Patterns

### Pattern 1: Parallel Independent (fastest)

```
Task
├─→ Agent A (parallel)
├─→ Agent B (parallel)
├─→ Agent C (parallel)
└─→ Agent D (parallel)
     ↓
Synthesize results
```

**Use for:** Code review, research, validation, analysis

**Example:**
```markdown
Task: Run security-scanner on api/
Task: Run performance-analyzer on api/
Task: Run test-coverage on api/
→ All run in parallel, results merged
```

### Pattern 2: Sequential Pipeline

```
Task → Agent 1 → Result 1
                  ↓
              Agent 2 → Result 2
                         ↓
                     Agent 3 → Final
```

**Use for:** Dependencies between stages

**Example:**
```markdown
Task: requirements-analyst analyze request
→ Wait for requirements
Task: architect design based on [requirements]
→ Wait for design
Task: engineer implement [design]
```

### Pattern 3: Hierarchical Orchestration

```
Main Orchestrator
├─→ Frontend Coordinator
│   ├─→ React Specialist
│   └─→ State Specialist
└─→ Backend Coordinator
    ├─→ API Specialist
    └─→ Database Specialist
```

**Use for:** Complex multi-layer systems

### Pattern 4: Iterative Refinement

```
Task → Draft Agent
        ↓
    Review Agent → Feedback
                    ↓
                Draft Agent → Revision
                               ↓
                           Review → Approval
```

**Use for:** Quality improvement through iteration

**Full patterns:** Read references/orchestration-patterns.md

## Practical Implementation Examples

### Example 1: Create Specialized Agents for Novel Task

```markdown
Task: "Implement microservices architecture for order processing"

STEP 1: Decompose into specialized roles
- service-architect (design services and boundaries)
- api-gateway-implementer (implement gateway)
- order-service-implementer (implement order service)
- inventory-service-implementer (implement inventory service)
- message-queue-specialist (set up async messaging)

STEP 2: Create agent definitions

File: .claude/agents/service-architect.md
---
name: service-architect
description: Use PROACTIVELY when designing microservices architecture, service boundaries, or distributed system patterns
model: opus
---

You are a microservices architecture specialist with expertise in service decomposition, API design, and distributed systems patterns.

When invoked:
1. Analyze domain requirements and business capabilities
2. Identify service boundaries using domain-driven design
3. Define service contracts and communication patterns
4. Design data management strategies per service
5. Plan deployment and orchestration approach

CONSTRAINTS:
- Follow 12-factor app principles
- Each service must be independently deployable
- Avoid tight coupling between services

OUTPUT FORMAT:
- Service boundary diagram
- API contracts for each service
- Data flow documentation
- Deployment architecture

[Create similar files for other agents...]

STEP 3: Validate all agents
$ python scripts/validate_agent.py .claude/agents/*.md

STEP 4: Orchestrate implementation
Task: Use service-architect to design microservices architecture

[Wait for architecture design]

Task: Use api-gateway-implementer to create gateway based on [design]
Task: Use order-service-implementer to create order service based on [design]
Task: Use inventory-service-implementer to create inventory service based on [design]

[Parallel implementation of services]

Task: Use message-queue-specialist to set up messaging between [services]

STEP 5: Synthesize into complete system
```

### Example 2: Select from Existing Agents

```markdown
Task: "Comprehensive security audit of authentication system"

STEP 1: Discover available agents
$ python scripts/discover_agents.py

Output shows:
- security-scanner (general vulnerabilities)
- auth-specialist (authentication/authorization)
- crypto-auditor (cryptographic implementations)

STEP 2: Select relevant agents
All three are relevant → Use all in parallel

STEP 3: Invoke with clear scopes
Task: Use security-scanner on auth/ - check for common vulnerabilities (SQL injection, XSS, CSRF)
Task: Use auth-specialist on auth/ - audit authentication flows, session management, token handling
Task: Use crypto-auditor on auth/ - verify password hashing, encryption, key management

STEP 4: Collect and synthesize
[All agents run in parallel]
[Collect findings from each]
[Merge into prioritized security report]
```

### Example 3: Hybrid Approach

```markdown
Task: "Optimize and deploy ML model to production"

STEP 1: Discover existing agents
$ python scripts/discover_agents.py

Found:
- performance-optimizer (exists)
- deployment-specialist (exists)

Missing:
- ml-model-optimizer (need to create)
- inference-api-creator (need to create)

STEP 2: Create missing agents

File: .claude/agents/ml-model-optimizer.md
---
name: ml-model-optimizer
description: Use PROACTIVELY when optimizing ML models for production deployment, focusing on inference speed, model size, and accuracy trade-offs
tools: [Read, Edit, Bash]
model: opus
---

You are an ML model optimization specialist with expertise in quantization, pruning, knowledge distillation, and inference acceleration.

When invoked:
1. Profile model inference characteristics (latency, throughput, memory)
2. Identify optimization opportunities (quantization, pruning, distillation)
3. Implement optimizations while preserving accuracy
4. Benchmark optimized model vs baseline
5. Validate accuracy within acceptable bounds

[Full prompt...]

[Create inference-api-creator agent similarly...]

STEP 3: Orchestrate all agents (existing + new)
Task: Use ml-model-optimizer to optimize model for production
[Wait for optimized model]

Task: Use inference-api-creator to build FastAPI endpoint for [optimized model]
Task: Use performance-optimizer to optimize API code
[Parallel API development and optimization]

Task: Use deployment-specialist to deploy to production

STEP 4: Synthesize complete deployment
```

## Context Management

### Progressive Disclosure

For agents needing extensive knowledge:

```markdown
You are a Django specialist.

For basic CRUD: Use general Django knowledge
For advanced serializers: Read references/serializers.md
For authentication: Read references/auth.md
For performance: Read references/performance.md
```

### Result Compression

Each subagent should return **condensed findings**, not full exploration:

```markdown
❌ BAD: Return 50 pages of search results
✅ GOOD: Return 5 key findings with citations

❌ BAD: Include every file analyzed  
✅ GOOD: Summarize patterns found across files
```

## Scaling Guidelines

**Agent count by complexity:**
- 1-2 agents: Simple parallelization
- 3-5 agents: Moderate complexity  
- 5-10 agents: High complexity
- 10+ agents: Maximum complexity (only for highest-value tasks)

**Stop spawning when:**
- Diminishing returns observed
- Information saturation reached
- Cost threshold approached

## Validation and Quality

### Validate Agent Definitions

```bash
# Validate single agent
python scripts/validate_agent.py .claude/agents/my-agent.md

# Validate all agents
python scripts/validate_agent.py .claude/agents/*.md

# Strict mode (warnings as errors)
python scripts/validate_agent.py --strict .claude/agents/*.md
```

### Test Agent Behavior

Create test scenarios before deploying:

```markdown
TEST CASE 1: "Review this React component"
Expected: performance-optimizer invoked
Verify: Agent focuses only on performance

TEST CASE 2: "Add feature to component"  
Expected: Agent NOT invoked (not performance-related)
Verify: Main agent handles directly
```

## Error Handling

### Graceful Degradation

```markdown
TRY spawn security-scanner
IF fails:
  LOG failure
  TRY alternative approach (manual review)
  CONTINUE with partial results
  WARN user of incomplete analysis

Never fail entire task due to single agent failure
```

### Result Validation

Before synthesis:
- Verify all expected agents completed
- Check result format compliance
- Validate required fields present
- Flag anomalies for review

## Monitoring

Track key metrics:
- Agent spawn count
- Token consumption per agent
- Execution time per agent
- Success/failure rates
- Result quality scores

Log decision points:
```markdown
DECISION: Spawn subagents (breadth-first query detected)
SPAWNED: 5 agents [A, B, C, D, E]
TIMING: A=12s, B=15s, C=10s, D=18s, E=11s
RESULTS: 4 success, 1 partial
SYNTHESIS: 250 findings → 15 key insights
```

## Utility Scripts

### Discover Available Agents

```bash
# Text format (human-readable)
python scripts/discover_agents.py

# JSON format (for processing)
python scripts/discover_agents.py --format json

# Custom directories
python scripts/discover_agents.py --project-dir .claude/agents --user-dir ~/my-agents
```

### Validate Agent Definitions

```bash
# Basic validation
python scripts/validate_agent.py agent.md

# Multiple files
python scripts/validate_agent.py agent1.md agent2.md agent3.md

# Strict mode
python scripts/validate_agent.py --strict agent.md
```

## Summary Checklist

Before spawning subagents, verify:

- [ ] Can I answer directly? (If yes, don't use agents)
- [ ] Is this task parallelizable? (Independent subtasks)
- [ ] Is information space too large? (Exceeds single context)
- [ ] Is value high enough? (Justifies 15× token cost)
- [ ] Do suitable agents exist? (Discover first)
- [ ] If creating agents: Single responsibility per agent?
- [ ] If creating agents: Clear trigger conditions in descriptions?
- [ ] If creating agents: Structured prompts with explicit steps?
- [ ] Orchestration pattern chosen? (Parallel, sequential, hybrid)
- [ ] Result synthesis strategy defined? (How to combine findings)

## Advanced Topics

### Extended Thinking Mode

Use extended thinking in orchestrator for:
- Planning overall approach
- Assessing which tools fit
- Determining agent count
- Defining each agent's role

Subagents use interleaved thinking after tool results to:
- Evaluate quality
- Identify gaps
- Refine next queries

### Lifecycle Hooks

**SubagentStop hook:** Control when subagents complete
- Block with reason if output inadequate
- Allow completion if output meets criteria

### Agent Skills

Agents can leverage skills for specialized capabilities:
- PDF manipulation
- Excel generation
- PowerPoint creation
- Word formatting
- Custom domain skills

## Common Pitfalls to Avoid

### ❌ Over-orchestration
```markdown
Main → Sub1 → Sub2 → Sub3 → Worker
(Too many coordination layers)
```

### ❌ Context Duplication
```markdown
Agent A: Analyze entire codebase
Agent B: Analyze entire codebase
(Massive duplication)
```

### ❌ Sequential When Parallel Possible
```markdown
Analyze file A, wait
Analyze file B, wait
Analyze file C, wait
(3x slower than parallel)
```

### ✅ Efficient Orchestration
```markdown
Main → [Worker A, Worker B, Worker C] in parallel
(Flat, efficient structure)
```

## References

For detailed information, read:

- **references/decision-framework.md** - Complete decision tree and criteria
- **references/agent-creation-patterns.md** - Comprehensive agent design guide
- **references/orchestration-patterns.md** - All coordination patterns

## Support

For issues or questions about subagent orchestration:
1. Validate agent definitions with scripts/validate_agent.py
2. Review decision framework to ensure appropriate usage
3. Check orchestration patterns for coordination strategies
4. Consult reference docs for detailed guidance
