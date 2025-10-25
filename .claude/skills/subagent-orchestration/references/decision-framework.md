# Subagent Decision Framework

This reference provides the detailed decision tree and criteria for determining when to use subagents versus working directly.

## Core Philosophy: Start Simple

**Default: Work directly unless subagents are clearly justified.**

Multi-agent systems consume 15× more tokens than single-agent work. Only deploy subagents when the value clearly exceeds this cost.

## Decision Tree

```
Query received
    ├─→ Is this simple and well-defined?
    │   └─→ YES → Work directly
    │
    ├─→ Is the answer stable and already known?
    │   └─→ YES → Work directly
    │
    ├─→ Does this require breadth-first exploration across multiple independent domains?
    │   └─→ YES → Consider subagents → Check value
    │       ├─→ High-value task → Use subagents
    │       └─→ Low-value task → Work directly
    │
    ├─→ Does information exceed single context window requiring compression?
    │   └─→ YES → Consider subagents → Check parallelization
    │       ├─→ Parallelizable → Use subagents
    │       └─→ Sequential with shared context → Work directly
    │
    ├─→ Is this open-ended with unpredictable steps?
    │   └─→ YES → Consider subagents → Check complexity
    │       ├─→ Highly complex + valuable → Use subagents
    │       └─→ Manageable → Single agent
    │
    └─→ Default → Work directly
```

## Characteristics Favoring Subagents

### 1. Breadth-First Queries

**Indicator:** Multiple independent exploration paths that can run in parallel

**Examples:**
- "Compare economic systems of Nordic countries" → Parallel country agents
- "Analyze semiconductor supply chain across manufacturing, logistics, geopolitics" → Parallel domain agents
- "Research top 10 ML frameworks and their trade-offs" → Parallel framework agents

**Why subagents:** Each path can compress its own information space independently

### 2. Information Exceeding Context

**Indicator:** Single context window cannot hold all relevant information

**Examples:**
- "Analyze all API endpoints in this 50-file codebase for security issues"
- "Research historical precedents for this legal question across 100+ cases"
- "Summarize key findings from 20 research papers"

**Why subagents:** Act as intelligent filters, returning only relevant findings

### 3. Heavy Parallelization Potential

**Indicator:** Independent subtasks with no interdependencies

**Examples:**
- Code review: style, security, performance, test coverage agents run simultaneously
- Content analysis: sentiment, topics, entities, factuality agents work in parallel
- Data validation: schema, completeness, quality, consistency agents operate independently

**Why subagents:** 90% time reduction through parallel execution

### 4. Open-Ended Problems

**Indicator:** Cannot predict required steps; flexible exploration needed

**Examples:**
- "Design a complete marketing strategy for this product"
- "Debug this complex distributed system issue"
- "Propose architectural improvements for this legacy codebase"

**Why subagents:** Autonomous agents can adapt approach as they discover information

## Characteristics Avoiding Subagents

### 1. Simple, Well-Defined Tasks

**Indicator:** Clear steps, predictable approach, single straightforward answer

**Examples:**
- "Format this JSON prettily"
- "Write a function to calculate Fibonacci numbers"
- "Explain how DNS works"

**Why not:** Token overhead not justified; simple prompts suffice

### 2. Shared Context Requirements

**Indicator:** All subtasks need the same information

**Examples:**
- "Refactor this function to use async/await" (needs same code context)
- "Add error handling throughout this module" (needs module understanding)
- "Update all references to old API in these files" (needs API knowledge)

**Why not:** Duplicating context across agents wastes tokens; single context more efficient

### 3. Tight Interdependencies

**Indicator:** Each step depends on previous step's output

**Examples:**
- "Design database schema, then implement migrations, then write ORM models"
- "Parse requirements, create design doc, implement solution"
- "Read file, transform data, write output"

**Why not:** Sequential dependencies prevent parallelization; coordination overhead exceeds benefits

### 4. Low-Value Outcomes

**Indicator:** Result doesn't justify 15× token cost

**Examples:**
- "What's the capital of France?"
- "Fix this typo"
- "Generate a sample config file"

**Why not:** Cost-benefit analysis fails; simple approach adequate

## Scaling Guidelines

### Task Complexity → Agent Count

**1-2 agents:** Simple parallelization
- Parallel data processing
- Basic domain split (frontend + backend)

**3-5 agents:** Moderate complexity
- Multi-domain research
- Code review with multiple aspects
- Comparative analysis

**5-10 agents:** High complexity
- Comprehensive research across many domains
- Large codebase analysis
- Multi-stage workflows

**10+ agents:** Maximum complexity
- Only for highest-value tasks
- Complete system analysis
- Extensive research projects

### Stopping Criteria

**Stop spawning when:**
- Diminishing returns observed (new agents provide little additional value)
- Information saturation reached (agents start repeating findings)
- Time constraints met (task needs completion now)
- Cost threshold approached (token budget nearly exhausted)

## Economic Calculation

**Formula:**
```
Subagent Value = (Task Value × Quality Improvement) - (Token Cost × 15)
```

**Use subagents when:** Subagent Value > 0

**Examples:**

✅ **High-value research project**
- Value: $1000 (client deliverable)
- Quality: +50% (much better insights)
- Cost: $5 in tokens × 15 = $75
- **Result:** $500 improvement - $75 cost = $425 net benefit → USE SUBAGENTS

❌ **Simple code formatting**
- Value: $5 (minor task)
- Quality: +10% (slightly better)
- Cost: $0.50 × 15 = $7.50
- **Result:** $0.50 improvement - $7.50 cost = -$7 net loss → DON'T USE

## Quick Assessment Checklist

Before spawning subagents, verify:

- [ ] Can this task be parallelized into independent subtasks?
- [ ] Is the information space too large for single context?
- [ ] Is the value high enough to justify 15× token cost?
- [ ] Would parallel execution provide significant speedup?
- [ ] Is this truly open-ended requiring flexible exploration?

**If 3+ checks: Strong subagent candidate**
**If 1-2 checks: Borderline—consider simpler approaches first**
**If 0 checks: Work directly**

## Context Engineering Principle

**Remember:** "Find the smallest possible set of high-signal tokens that maximize the likelihood of desired outcome."

Subagents enable this by:
- Compressing large information spaces through intelligent filtering
- Keeping main context clean and focused
- Scaling token usage only where it provides value

But only when used judiciously for appropriate tasks.
