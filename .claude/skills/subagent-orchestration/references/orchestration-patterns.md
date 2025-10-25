# Orchestration Patterns

This reference provides proven patterns for coordinating multiple subagents effectively.

## Core Orchestration Principles

### 1. Context Compression

**Key insight:** Subagents compress large information spaces into relevant findings

**Pattern:**
```
Orchestrator defines scope → Subagents explore deeply → Return condensed findings
```

**NOT this:**
```
Orchestrator defines scope → Subagents return everything → Orchestrator drowns in detail
```

### 2. Parallel Execution for Independence

**Rule:** Run subagents in parallel when their tasks are independent

**Benefit:** 90% time reduction on complex queries

**Pattern:**
```python
# Parallel pattern
parallel_tasks = [
    "security-scanner analyze auth.py",
    "performance-analyzer profile auth.py",
    "test-coverage check auth.py"
]
# All spawn simultaneously
```

### 3. Sequential Execution for Dependencies

**Rule:** Run subagents sequentially when each depends on previous output

**Pattern:**
```python
# Sequential pattern
requirements = requirements_analyst.analyze(request)
design = architect.design(requirements)
implementation = engineer.implement(design)
tests = test_engineer.create(implementation)
```

## Pattern Catalog

### Pattern 1: Parallel Independent Analysis

**Use when:** Multiple aspects need analysis with no interdependencies

**Structure:**
```
Main Agent
├─→ Subagent A (parallel)
├─→ Subagent B (parallel)
├─→ Subagent C (parallel)
└─→ Subagent D (parallel)
      ↓
Collect all results
      ↓
Synthesize insights
```

**Example: Code Review**
```
Code Review Request
├─→ style-checker (parallel)
├─→ security-scanner (parallel)
├─→ performance-analyzer (parallel)
└─→ test-coverage-checker (parallel)
      ↓
Combine findings
      ↓
Present unified report
```

**Invocation:**
```markdown
# Spawn all agents simultaneously
Task: Run style-checker on files/*.py - focus on PEP8 compliance
Task: Run security-scanner on files/*.py - check for vulnerabilities
Task: Run performance-analyzer on files/*.py - identify bottlenecks
Task: Run test-coverage-checker on files/*.py - measure coverage gaps
```

**Key traits:**
- Each agent works independently
- No shared mutable state
- Results merged at orchestrator level
- Maximum parallelization efficiency

### Pattern 2: Breadth-First Research

**Use when:** Multiple independent domains need exploration

**Structure:**
```
Research Query
├─→ Domain Expert A (parallel)
├─→ Domain Expert B (parallel)
├─→ Domain Expert C (parallel)
└─→ Domain Expert D (parallel)
      ↓
Each compresses their domain
      ↓
Synthesize cross-domain insights
```

**Example: Technology Comparison**
```
"Compare React, Vue, Angular, Svelte"
├─→ react-researcher (parallel)
├─→ vue-researcher (parallel)
├─→ angular-researcher (parallel)
└─→ svelte-researcher (parallel)
      ↓
Each finds: strengths, weaknesses, use cases
      ↓
Build comparison matrix
```

**Invocation:**
```markdown
Task: Research React ecosystem - focus on performance, developer experience, ecosystem maturity
Task: Research Vue ecosystem - focus on performance, developer experience, ecosystem maturity
Task: Research Angular ecosystem - focus on performance, developer experience, ecosystem maturity
Task: Research Svelte ecosystem - focus on performance, developer experience, ecosystem maturity
```

**Key traits:**
- Each domain explored independently
- Common evaluation criteria across agents
- Results structured for comparison
- Orchestrator synthesizes patterns

### Pattern 3: Sequential Pipeline

**Use when:** Output of one step feeds into next step

**Structure:**
```
Task
 ↓
Agent 1 → Result 1
          ↓
      Agent 2 → Result 2
                ↓
            Agent 3 → Result 3
                      ↓
                  Final Output
```

**Example: Feature Development**
```
Feature Request
 ↓
requirements-analyst → Requirements Doc
                       ↓
                   architect → Design Spec
                              ↓
                          engineer → Implementation
                                     ↓
                                 test-writer → Test Suite
```

**Invocation:**
```markdown
# Step 1: Analyze requirements
Task: Use requirements-analyst to extract and document requirements from user story

# Wait for Result 1, then Step 2: Design solution
Task: Use architect to design implementation based on [Result 1]

# Wait for Result 2, then Step 3: Implement
Task: Use engineer to implement features following [Result 2]

# Wait for Result 3, then Step 4: Test
Task: Use test-writer to create comprehensive tests for [Result 3]
```

**Key traits:**
- Strict ordering enforced
- Each step consumes previous output
- Quality gates between stages
- Linear workflow

### Pattern 4: Hierarchical Orchestration

**Use when:** Complex task with natural hierarchy

**Structure:**
```
Main Orchestrator
├─→ Sub-Orchestrator A
│   ├─→ Specialist A1
│   └─→ Specialist A2
└─→ Sub-Orchestrator B
    ├─→ Specialist B1
    └─→ Specialist B2
```

**Example: Full-Stack Feature**
```
Feature Orchestrator
├─→ Frontend Coordinator
│   ├─→ react-specialist
│   ├─→ css-specialist
│   └─→ state-management-specialist
└─→ Backend Coordinator
    ├─→ api-specialist
    ├─→ database-specialist
    └─→ auth-specialist
```

**Invocation:**
```markdown
# Top-level orchestration
Task: Use frontend-coordinator to implement user-facing components for [feature]
Task: Use backend-coordinator to implement API and data layer for [feature]

# Frontend coordinator then spawns:
Task: Use react-specialist for component implementation
Task: Use css-specialist for styling
Task: Use state-management-specialist for state logic

# Backend coordinator then spawns:
Task: Use api-specialist for endpoint creation
Task: Use database-specialist for schema and queries
Task: Use auth-specialist for permission checks
```

**Key traits:**
- Natural delegation hierarchy
- Coordinators manage complexity
- Specialists stay focused
- Recursive orchestration pattern

### Pattern 5: Iterative Refinement

**Use when:** Quality improves through multiple passes

**Structure:**
```
Initial Task
 ↓
Agent → Draft 1
        ↓
    Evaluator → Feedback 1
                ↓
            Agent → Draft 2
                    ↓
                Evaluator → Feedback 2
                            ↓
                        ... → Final
```

**Example: Document Creation**
```
Create Technical Doc
 ↓
writer-agent → Draft
               ↓
           reviewer-agent → Critique
                           ↓
                       writer-agent → Revision
                                     ↓
                                 reviewer-agent → Approval
```

**Invocation:**
```markdown
# Iteration loop
LOOP until quality threshold met:
  Task: Use writer-agent to draft/revise technical documentation
  Task: Use reviewer-agent to evaluate quality against criteria
  IF reviewer-agent approves → BREAK
  ELSE continue loop with feedback
```

**Key traits:**
- Continuous improvement
- Quality gates enforced
- Feedback loops explicit
- Convergence to standards

### Pattern 6: Validation Chain

**Use when:** Multiple validation criteria must be satisfied

**Structure:**
```
Implementation
 ↓
Validator 1 (parallel) ─┐
Validator 2 (parallel) ─┼→ All Pass? → Approve
Validator 3 (parallel) ─┤   Any Fail? → Reject
Validator 4 (parallel) ─┘
```

**Example: Production Deployment**
```
Code Changes
 ↓
security-validator (parallel) ─┐
test-validator (parallel) ─────┼→ All Green? → Deploy
performance-validator (parallel)┤   Any Red? → Block
compliance-validator (parallel) ┘
```

**Invocation:**
```markdown
# Parallel validation
Task: Run security-validator on changes - check for vulnerabilities
Task: Run test-validator on changes - ensure all tests pass
Task: Run performance-validator on changes - verify no degradation
Task: Run compliance-validator on changes - confirm policy adherence

# Collect results
IF all validators pass → proceed with deployment
ELSE → block and report failures
```

**Key traits:**
- Parallel execution for speed
- AND gate for all criteria
- Fail-fast on any blocker
- Comprehensive validation

### Pattern 7: Hybrid Parallel-Sequential

**Use when:** Some tasks parallel, some sequential

**Structure:**
```
Task
 ↓
Phase 1: Parallel Analysis
├─→ Agent A
├─→ Agent B
└─→ Agent C
     ↓
  Synthesis
     ↓
Phase 2: Sequential Implementation
Agent D → Agent E → Agent F
```

**Example: System Migration**
```
Migration Request
 ↓
Analysis Phase (parallel)
├─→ dependency-analyzer
├─→ compatibility-checker
└─→ risk-assessor
     ↓
  Create migration plan
     ↓
Execution Phase (sequential)
backup-agent → migration-agent → validation-agent
```

**Invocation:**
```markdown
# Phase 1: Parallel analysis
Task: Use dependency-analyzer to map all dependencies
Task: Use compatibility-checker to verify new system compatibility
Task: Use risk-assessor to identify migration risks

# Wait for Phase 1, synthesize findings
[Create migration plan from analysis results]

# Phase 2: Sequential execution
Task: Use backup-agent to create full system backup
[Wait for backup]
Task: Use migration-agent to execute migration plan
[Wait for migration]
Task: Use validation-agent to verify migration success
```

**Key traits:**
- Hybrid parallelization
- Natural phase boundaries
- Analysis before action
- Optimized resource usage

## Coordination Patterns

### Result Collection

**Pattern: Structured Output**

```markdown
Each subagent returns:
{
  "agent": "agent-name",
  "status": "success|failure",
  "findings": [...],
  "recommendations": [...],
  "confidence": 0.0-1.0
}
```

**Orchestrator synthesizes:**
```python
all_findings = aggregate(subagent_results)
prioritized = prioritize_by_confidence(all_findings)
deduped = remove_duplicates(prioritized)
final_report = synthesize_narrative(deduped)
```

### Conflict Resolution

**When subagents disagree:**

1. **Confidence-based:** Trust higher confidence findings
2. **Expertise-based:** Weight specialized agents more
3. **Voting-based:** Majority consensus for binary questions
4. **Escalation-based:** Flag conflicts for human review

**Example:**
```markdown
Security-agent: "This is vulnerable" (confidence: 0.9)
Performance-agent: "This is fine" (confidence: 0.6)

→ Resolution: Flag as security issue (higher confidence + security priority)
```

### Error Handling

**Pattern: Graceful Degradation**

```markdown
TRY spawn subagent A
IF fails:
  LOG failure
  TRY alternative approach
  IF still fails:
    CONTINUE with partial results
    WARN user of incomplete analysis

Never fail entire task due to single subagent failure
```

### Resource Management

**Pattern: Staged Rollout**

```markdown
# Start with small batch
Spawn agents 1-3
Evaluate results

IF results promising:
  Spawn agents 4-10
  
IF still valuable:
  Spawn agents 11-20

STOP when diminishing returns observed
```

## Anti-Patterns to Avoid

### ❌ Context Duplication

**Wrong:**
```markdown
# Both agents need same large codebase
Task: Agent A analyze entire codebase for security
Task: Agent B analyze entire codebase for performance
→ Massive context duplication
```

**Right:**
```markdown
# Split codebase analysis
Task: Agent A analyze security in auth/*.py
Task: Agent B analyze security in api/*.py
Task: Agent C analyze performance in critical_path/*.py
→ Focused, non-overlapping contexts
```

### ❌ Sequential When Parallel Possible

**Wrong:**
```markdown
Task: Analyze file A
Wait for result
Task: Analyze file B
Wait for result
Task: Analyze file C
→ 3x slower than necessary
```

**Right:**
```markdown
Task: Analyze file A
Task: Analyze file B
Task: Analyze file C
→ All run in parallel
```

### ❌ Over-Orchestration

**Wrong:**
```markdown
Main orchestrator
└─→ Sub-orchestrator 1
    └─→ Sub-orchestrator 2
        └─→ Sub-orchestrator 3
            └─→ Actual worker
→ Excessive coordination overhead
```

**Right:**
```markdown
Main orchestrator
├─→ Worker A (parallel)
├─→ Worker B (parallel)
└─→ Worker C (parallel)
→ Flat, efficient structure
```

### ❌ Unclear Responsibilities

**Wrong:**
```markdown
Task: Do whatever you think is needed
→ Vague, unpredictable behavior
```

**Right:**
```markdown
Task: Analyze authentication logic in auth.py for:
- SQL injection vulnerabilities
- Password handling issues
- Session management flaws
Output: JSON report with CVE mappings
→ Clear scope and expectations
```

## Synthesis Strategies

### Strategy 1: Aggregation

**When:** Combining similar findings from multiple sources

**Pattern:**
```markdown
Findings from agents: [A, B, C]
→ Merge common themes
→ Highlight unique insights
→ Provide integrated view
```

### Strategy 2: Comparison

**When:** Evaluating alternatives

**Pattern:**
```markdown
Research from agents: [React, Vue, Angular]
→ Build comparison matrix
→ Identify trade-offs
→ Recommend best fit for context
```

### Strategy 3: Prioritization

**When:** Too many findings to act on

**Pattern:**
```markdown
Issues from agents: [security, performance, style, tests]
→ Score by severity and impact
→ Group by theme
→ Present prioritized action plan
```

### Strategy 4: Narrative

**When:** Complex story needs telling

**Pattern:**
```markdown
Insights from agents: [historical, current, future]
→ Construct timeline
→ Connect cause and effect
→ Present coherent narrative
```

## Performance Optimization

### Minimize Agent Count

**Rule:** Use minimum number of agents that still achieves parallelization benefits

**Example:**
- 2 agents: 2x potential speedup
- 5 agents: 5x potential speedup  
- 20 agents: 20x potential speedup BUT 20x cost

**Sweet spot:** 3-7 agents for most tasks

### Batch Similar Operations

**Pattern:**
```markdown
# Instead of:
10 agents each analyzing 1 file → 10 agents

# Do this:
2 agents each analyzing 5 files → 2 agents
```

### Early Termination

**Pattern:**
```markdown
IF first 3 agents find critical blocker:
  STOP spawning remaining agents
  RETURN blocker immediately
→ Fail-fast optimization
```

## Monitoring and Observability

### Track Key Metrics

- Agent spawn count
- Token consumption per agent
- Execution time per agent
- Success/failure rates
- Result quality scores

### Log Decision Points

```markdown
DECISION: Spawn subagents (breadth-first query detected)
SPAWNED: 5 agents [agent-a, agent-b, agent-c, agent-d, agent-e]
TIMING: agent-a: 12s, agent-b: 15s, agent-c: 10s, agent-d: 18s, agent-e: 11s
RESULTS: 4 success, 1 partial (agent-d timeout)
SYNTHESIS: 250 findings → 15 key insights
```

### Quality Gates

```markdown
BEFORE synthesis:
- Verify all expected agents completed
- Check result format compliance
- Validate required fields present
- Flag anomalies for review
```

## Summary Principles

1. **Parallel when possible** - Maximize speed for independent tasks
2. **Sequential when necessary** - Respect dependencies
3. **Structured outputs** - Consistent result formats
4. **Clear boundaries** - Each agent knows its scope
5. **Graceful degradation** - Handle failures without cascading
6. **Synthesis quality** - Transform findings into insights
7. **Resource awareness** - Balance parallelization vs cost
8. **Observability** - Track and log for debugging

Choose orchestration pattern based on:
- Task structure (parallel vs sequential)
- Dependencies (independent vs coupled)
- Quality requirements (validation needs)
- Resource constraints (token budget)
- Time sensitivity (speed vs thoroughness)
