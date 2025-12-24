# Cppcheck with MISRA C:2012 Setup

This guide explains how to run static analysis with cppcheck and the MISRA C:2012 addon for the Signalbridge Controller firmware without embedding code snippets.

## Quick Start
- In the DevContainer, cppcheck is already installed and configured. Run the comprehensive analysis using the helper script in `scripts/run_cppcheck.sh` or the simplified sweep via `scripts/cppcheck_simple.sh`.
- In VS Code, open the command palette, choose **Tasks: Run Task**, and select the available cppcheck tasks for full, quick, or single-file analysis.

## Configuration Files
- **.cppcheck_config** lists the preprocessor definitions and include paths for the RP2040, FreeRTOS settings, and alignment requirements. It also declares the force-includes needed for correct parsing.
- **.cppcheck_suppressions** documents targeted suppressions for external libraries, hardware abstraction layers, and FreeRTOS integration so noise stays contained.
- **misra.json** points cppcheck to the MISRA addon and the accompanying `misra.txt` rule text.

## Fixed Issues
- Added `portBYTE_ALIGNMENT=8` and related FreeRTOS definitions to eliminate preprocessor errors that previously halted analysis.
- Corrected MISRA addon paths and refined suppressions to avoid unmatched entries while keeping third-party code filtered.
- Hardened the base configuration to prevent critical errors from stopping analysis before results are produced.

## MISRA Compliance Notes
Suppressions are grouped by rationale: external libraries, hardware register access, and FreeRTOS integrations. Keep suppressions specific to affected files or functions and include the deviation category so reviewers understand the context.

When adding new suppressions, document the exact location and the reason. Prefer focused patterns over broad wildcards to keep reports actionable.

## Troubleshooting
- If cppcheck is not found, rebuild the DevContainer or install the package on your host environment.
- When the MISRA addon cannot be located, verify the addon path, ensure `misra.txt` is present, and confirm your cppcheck installation includes MISRA support.
- For include path errors, make sure submodules are initialized and required headers are available; add force-includes when the parser needs specific configuration headers.
- Reduce false positives by refining suppressions and limiting them to the smallest necessary scope.

## Advanced Configuration
- Introduce new feature flags by extending `.cppcheck_config` with the additional definitions relevant to the feature.
- Add include paths for extra libraries in the same configuration file so cppcheck resolves dependencies accurately.
- Record any new suppression rules with a clear rationale to maintain auditability.

## CI/CD Integration
Static analysis can run in continuous integration by invoking the full cppcheck script as part of the build workflow. Allow the job to report warnings while still capturing output for review. Pre-commit hooks can also invoke cppcheck on staged C or header files to catch issues before they reach the repository.

## Best Practices
1. Run analysis regularly and prioritize real defects over stylistic warnings.
2. Document every suppression with a concise reason and deviation category.
3. Review new warnings instead of suppressing them automatically.
4. Keep configuration files under version control to ensure consistent results across environments.

## Expected Results
A healthy run should load MISRA rules correctly, avoid critical preprocessor errors, and limit warnings to actionable findings in the project code while suppressing noise from external dependencies.
