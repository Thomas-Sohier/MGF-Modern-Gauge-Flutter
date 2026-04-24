# AGENTS.md

Behavioral guidelines for AI assistants working on this Flutter project.

## Before Coding

- Read `CLAUDE.md` for project structure and conventions
- Run `flutter analyze` before and after changes
- State assumptions if architecture decisions are unclear

## Code Changes

### Surgical Edits
- Touch only what's requested
- Match existing style (French comments, naming patterns)
- Don't refactor adjacent code
- Remove only imports/functions YOUR changes made unused

### Simplicity
- No speculative features or abstractions
- No error handling for impossible scenarios
- If 200 lines can be 50, rewrite

### Flutter-Specific
- Use `Selector<>` over `Consumer<>` for rebuilds
- Keep `build()` methods lightweight
- Use `const` constructors where possible
- Models: use `copyWith()` pattern

## Verification Loop

Transform tasks into verifiable goals:
```
1. [Change] → verify: flutter analyze passes
2. [Change] → verify: flutter test passes
3. [UI change] → verify: test in browser/device
```

## Don'ts

- Don't use `print()` — use `LogService`
- Don't add features beyond the request
- Don't "improve" unrelated code
- Don't create docs unless asked
- Don't skip `flutter analyze`

## Architecture Rules

- Views never import Services directly
- ViewModels (Providers) mediate all data flow
- Services emit Streams, Providers call `notifyListeners()`
- Local UI state stays in StatefulWidget, not Provider
