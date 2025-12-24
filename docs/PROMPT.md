# README Enhancement Guidance for Embedded C Projects

## Purpose
Use this guidance to enhance and organize the README for embedded C projects while keeping it synchronized with the live codebase. The emphasis is on preserving accurate information, removing obsolete details, and aligning documentation with current implementations.

## Preservation and Synchronization
- Keep valid technical details, links, acknowledgments, and configuration notes that match the current code.
- Remove outdated sections tied to deprecated features, changed APIs, or removed protocols.
- Verify documented commands and interfaces against actual function signatures and source comments before publishing updates.
- Integrate insights from Doxygen comments where they improve clarity and traceability.

## Analysis Expectations
- Review source files to confirm that documented APIs and behaviors align with implementations.
- Compare existing documentation with command handlers and configuration headers to eliminate discrepancies.
- Inspect devcontainer settings so onboarding instructions highlight the simplest setup path.

## Conditional Sections
Include only sections that reflect implemented features. Avoid describing unimplemented functionality or speculative roadmaps. Focus on real usage patterns, supported protocols, and tested behaviors.

## Development Environment Detection and Setup
Prioritize container-based workflows when available. If a `.devcontainer` directory exists, highlight the DevContainer experience as the recommended path. Otherwise, provide concise manual setup guidance covering required tools, compilers, debuggers, documentation generators, and analysis utilities.

## Quick Start Recommendations
- Present the DevContainer workflow first when applicable, outlining prerequisites and the basic steps to open the project.
- For manual setups, summarize platform-specific requirements for Linux, macOS, and Windows without embedding command transcripts.

## Documentation Best Practices
- Preserve existing content that remains accurate, but restructure it into a logical flow covering introduction, repository layout, setup, features, and diagnostics.
- Favor concise descriptions over lengthy code examples. When examples are necessary, describe expected behavior rather than embedding source listings.
- Ensure links and references point to the latest file locations and configurations.
- Regularly audit the README against the codebase to keep protocols, commands, and configuration options synchronized.

## Final Checklist
- All instructions reflect current code and configuration files.
- Legacy or obsolete references have been removed.
- Onboarding steps are clear, starting with the simplest option.
- Sections are concise, accurate, and free from code snippets to keep the documentation focused on behavior and usage.
