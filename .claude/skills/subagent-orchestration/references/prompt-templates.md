# Prompt Templates for Subagent Tasks

Copy-paste templates for common subagent scenarios, optimized for context efficiency and quality output.

## Template Structure

Each template includes:
1. **Objective**: Clear, measurable goal
2. **Output Format**: Structured for easy integration
3. **Tool Guidance**: Efficient tool use patterns
4. **Scope Boundaries**: Explicit inclusions/exclusions

## Code Review Templates

### Security Analysis Template

```
Objective: Identify security vulnerabilities in [SPECIFIC_AREA] with severity rankings.

Output Format:
Markdown table with columns:
- File: Path relative to project root
- Line: Line number(s)
- Severity: Critical/High/Medium/Low
- Issue: Vulnerability type (e.g., SQL injection, XSS)
- Evidence: Code snippet showing issue (max 2 lines)
- Recommendation: Specific fix

Tool Guidance:
1. Use Grep to find patterns: user input handling, database queries, file operations, authentication
2. Use Read with view_range to examine suspicious sections (not entire files)
3. Focus on high-signal files first

Scope Boundaries:
- Include: [SPECIFIC_FILES_OR_DIRS]
- Exclude: [WHAT_TO_SKIP]
- Focus on: [VULNERABILITY_TYPES] (e.g., injection attacks, auth bypasses)

Tool Budget: 12-15 tool calls maximum
```

### Code Quality Review Template

```
Objective: Evaluate code quality in [AREA] for maintainability, readability, and adherence to best practices.

Output Format:
Grouped by category:
- **Critical Issues**: Must fix (breaks, technical debt)
- **Improvements**: Should fix (clarity, performance)  
- **Suggestions**: Nice to have (style, conventions)

Each issue: File, Line, Description, Proposed fix

Tool Guidance:
1. Use Glob to understand module structure
2. Use Grep to find: long functions, complex conditionals, duplicated code patterns
3. Use Read selectively on flagged areas

Scope Boundaries:
- Focus on: [MODULES_OR_FILES]
- Exclude: Test files, generated code, vendor dependencies
- Standards: [STYLE_GUIDE_OR_CONVENTIONS]

Tool Budget: 10-12 tool calls
```

### Test Coverage Analysis Template

```
Objective: Assess test coverage for [FEATURE_OR_MODULE] and identify critical gaps.

Output Format:
Two sections:
1. **Coverage Summary**:
   - Files/functions with tests
   - Files/functions without tests
   - Coverage percentage estimate

2. **Priority Gaps**:
   - Critical untested paths (auth, payments, data integrity)
   - Recommended test additions

Tool Guidance:
1. Use Glob to find test files matching [PATTERN]
2. Use Grep in source to find test descriptions
3. Use Read to examine complex functions needing tests

Scope Boundaries:
- Source: [SOURCE_DIRS]
- Tests: [TEST_DIRS]
- Focus on: Public APIs, critical business logic
- Exclude: Internal helpers, trivial getters/setters

Tool Budget: 10-15 tool calls
```

## Implementation Templates

### Feature Implementation Template

```
Objective: Implement [FEATURE_NAME] according to specifications: [1-2 SENTENCE SUMMARY].

Output Format:
1. **Implementation Plan**: Bullet points of changes needed
2. **Code Changes**: Create/modify files with full implementations
3. **Integration Points**: Where new code connects to existing

Tool Guidance:
1. Use Grep to find similar existing patterns
2. Use Read to understand relevant interfaces/contracts
3. Use Write to create new files, Edit to modify existing

Scope Boundaries:
- Implement ONLY the core feature logic
- Do NOT add tests (separate subagent)
- Do NOT add documentation (separate subagent)
- Focus on: [SPECIFIC_MODULES]

Tool Budget: 15-20 tool calls
```

### Test Generation Template

```
Objective: Generate comprehensive test suite for [FEATURE/MODULE] with unit and integration coverage.

Output Format:
Test file(s) with:
- Unit tests for each public function
- Integration tests for main workflows
- Edge case coverage
- Clear test descriptions

Tool Guidance:
1. Use Read to examine implementation
2. Use Grep to find existing test patterns
3. Use Write to create test files following project conventions

Scope Boundaries:
- Test ONLY: [SPECIFIC_FILES_OR_FUNCTIONS]
- Coverage target: [PERCENTAGE] of public API
- Exclude: Internal implementation details
- Follow: [TEST_FRAMEWORK] patterns

Tool Budget: 12-18 tool calls
```

### Documentation Template

```
Objective: Create technical documentation for [COMPONENT] covering API, usage examples, and integration.

Output Format:
Markdown documentation with sections:
1. Overview (1-2 paragraphs)
2. API Reference (functions/classes with signatures)
3. Usage Examples (2-3 practical examples)
4. Integration Guide (how to use in larger system)

Tool Guidance:
1. Use Read to examine implementation and extract signatures
2. Use Grep to find usage examples in codebase
3. Use Write to create documentation file

Scope Boundaries:
- Document: [PUBLIC_API_SURFACE]
- Exclude: Internal implementation, private methods
- Audience: [DEVELOPERS/USERS/OPERATORS]
- Style: [TECHNICAL_LEVEL]

Tool Budget: 8-12 tool calls
```

## Analysis Templates

### Performance Analysis Template

```
Objective: Identify performance bottlenecks in [SYSTEM/MODULE] and recommend optimizations.

Output Format:
JSON array of findings:
```json
[
  {
    "location": "file.js:45-67",
    "issue": "N+1 query pattern",
    "impact": "High - scales O(n²) with user count",
    "recommendation": "Use bulk query with JOIN",
    "effort": "Medium - 2-3 hours"
  }
]
```

Tool Guidance:
1. Use Grep to find: loops with queries, recursive patterns, large data structures
2. Use Read to examine suspicious algorithms
3. Use Bash to run profiling tools (if available)

Scope Boundaries:
- Analyze: [PERFORMANCE_CRITICAL_PATHS]
- Focus on: Algorithmic complexity, I/O patterns, memory usage
- Exclude: Premature optimizations, micro-optimizations

Tool Budget: 12-15 tool calls
```

### Debugging Investigation Template

```
Objective: Identify root cause of [BUG_DESCRIPTION] reported in [AREA].

Output Format:
1. **Root Cause**: File, line, explanation
2. **Call Stack**: Sequence of events leading to bug
3. **Proposed Fix**: Specific code changes
4. **Testing Plan**: How to verify fix

Tool Guidance:
1. Use Grep to find related error messages, function calls
2. Use Read to trace execution flow
3. Use Bash to reproduce (if commands provided)

Scope Boundaries:
- Suspected area: [FILES_OR_MODULES]
- Exclude: [KNOWN_GOOD_AREAS]
- Focus on: [SPECIFIC_FAILURE_SCENARIO]

Tool Budget: 10-15 tool calls
```

### Refactoring Analysis Template

```
Objective: Analyze [MODULE] for refactoring opportunities improving maintainability without changing behavior.

Output Format:
Prioritized list:
1. **High Priority**: Significant debt, blocking improvements
2. **Medium Priority**: Moderate improvements, good ROI
3. **Low Priority**: Nice to have, minor gains

Each item: Location, Issue, Proposed refactoring, Estimated effort

Tool Guidance:
1. Use Glob to understand module structure
2. Use Grep to find: duplicate code, long functions, complex conditionals
3. Use Read to assess refactoring complexity

Scope Boundaries:
- Analyze: [SPECIFIC_MODULES]
- Exclude: Working, simple code (don't over-engineer)
- Criteria: Complexity, duplication, testability

Tool Budget: 10-12 tool calls
```

## Specialized Analysis Templates

### Dependency Audit Template

```
Objective: Audit dependencies in [PROJECT] for security, maintenance, and compatibility issues.

Output Format:
Three sections:
1. **Security Issues**: CVEs, known vulnerabilities
2. **Maintenance Issues**: Unmaintained, deprecated packages
3. **Compatibility Issues**: Version conflicts, breaking changes

Tool Guidance:
1. Use Read to examine package files (package.json, requirements.txt, etc.)
2. Use Bash to run audit tools (npm audit, pip-audit, etc.)
3. Use Grep to find version constraints

Scope Boundaries:
- Audit: Direct dependencies (not dev dependencies unless requested)
- Focus on: Severity ≥ Medium
- Exclude: Warnings with no fix available

Tool Budget: 8-10 tool calls
```

### API Design Review Template

```
Objective: Review [API/INTERFACE] design for usability, consistency, and best practices.

Output Format:
Categorized feedback:
- **Breaking Issues**: Must fix before release
- **Inconsistencies**: Deviates from patterns/conventions
- **Usability**: Confusing, error-prone designs
- **Recommendations**: Improvements

Tool Guidance:
1. Use Read to examine interface definitions
2. Use Grep to find similar patterns in codebase
3. Compare against [STANDARD_OR_CONVENTION]

Scope Boundaries:
- Review: [SPECIFIC_API_SURFACE]
- Standards: [REST/GraphQL/RPC/etc. conventions]
- Focus on: Public API (not internal)

Tool Budget: 8-12 tool calls
```

## Context-Efficient Variations

### Minimal Context Template (for simple tasks)

```
Objective: [SINGLE_CLEAR_GOAL]
Output: [BRIEF_FORMAT_SPEC]
Tools: [1-2 PRIMARY_TOOLS]
Scope: [TIGHT_BOUNDARY]
Budget: 5-8 tool calls
```

### Exploration Template (for discovery tasks)

```
Objective: Explore [AREA] to understand [ASPECT] and identify [FINDINGS_TYPE].
Output: Summary with key discoveries and file references (not full details)
Tools: Start with Glob/Grep for breadth, use Read sparingly
Scope: Broad initial exploration, then narrow to high-signal areas
Budget: 12-15 tool calls
```

### Verification Template (for validation tasks)

```
Objective: Verify [CLAIM_OR_REQUIREMENT] holds true in [CODEBASE].
Output: Boolean (true/false) + supporting evidence (files, lines)
Tools: Grep for patterns, Read to confirm, list counterexamples if false
Scope: [VERIFICATION_AREA]
Budget: 6-10 tool calls
```

## Template Customization Guidelines

### Adapting Templates
1. Replace [BRACKETED_PLACEHOLDERS] with specific values
2. Adjust tool budgets based on scope (±30% of suggested)
3. Modify output format to match integration needs
4. Add domain-specific constraints in scope boundaries

### When to Write Custom Prompts
- Task doesn't match any template closely
- Novel analysis type not covered
- Specific output format requirements
- Integration with custom tooling

### Template Composition
Combine templates for multi-phase tasks:
```
Phase 1: Use Exploration Template to understand codebase
Phase 2: Use Feature Implementation Template for changes
Phase 3: Use Test Generation Template for coverage
```

## Anti-Patterns to Avoid

### ❌ Vague Objective
"Review the code" → What aspect? Security? Style? Bugs?

### ❌ No Output Format
"Find issues" → Returns unstructured wall of text

### ❌ No Tool Guidance
Subagent reads every file → Context overload

### ❌ Unlimited Scope
"Check everything" → Never finishes, unfocused output

### ✅ Well-Specified Prompt
Clear objective + structured output + tool guidance + bounded scope

## Quick Template Selection

**Security concerns** → Security Analysis Template  
**Code quality** → Code Quality Review Template  
**Missing tests** → Test Coverage Analysis Template  
**New feature** → Feature Implementation Template  
**Bug investigation** → Debugging Investigation Template  
**Slow performance** → Performance Analysis Template  
**Technical debt** → Refactoring Analysis Template  
**Dependencies** → Dependency Audit Template

Customize templates for specific context while maintaining core structure.
