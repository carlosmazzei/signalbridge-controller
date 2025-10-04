# Cppcheck with MISRA C:2012 Setup

This document explains how to use cppcheck with MISRA C:2012 addon for static analysis of the Signalbridge Controller firmware.

## 🚀 Quick Start

### DevContainer (Recommended)
If using the provided DevContainer, cppcheck is pre-installed and configured:

```bash
# Run full analysis with MISRA rules
./scripts/run_cppcheck.sh

# Quick check with simplified rules
./scripts/cppcheck_simple.sh
```

### VS Code Tasks
Use `Ctrl+Shift+P` → "Tasks: Run Task" and select:
- **"Run Cppcheck (Full Analysis)"** - Complete analysis with MISRA addon
- **"Run Cppcheck (Quick Check)"** - Simplified analysis 
- **"Run Cppcheck (Single File)"** - Analyze currently open file

## 🔧 Configuration Files

### `.cppcheck_config`
Contains all preprocessor definitions and include paths required for proper analysis:
- ARM Cortex-M0+ architecture defines
- RP2040 platform definitions
- FreeRTOS configuration parameters
- Critical `portBYTE_ALIGNMENT=8` definition
- All necessary include paths

### `.cppcheck_suppressions.txt`
Manages suppression of known issues:
- External library suppressions (MISRA DEVIATION D5)
- Hardware abstraction layer exceptions
- FreeRTOS integration exceptions
- Documented rationale for each suppression

### `misra.json`
MISRA C:2012 addon configuration pointing to rule text file.

## 🚨 Fixed Issues

### Issue 1: FreeRTOS portBYTE_ALIGNMENT Error
**Problem:** 
```
error preprocessorErrorDirective: #error "Invalid portBYTE_ALIGNMENT definition"
```

**Root Cause:** Missing preprocessor definition for FreeRTOS port layer.

**Solution:** Added `-DportBYTE_ALIGNMENT=8` and other critical FreeRTOS defines in configuration files.

### Issue 2: Unmatched MISRA Suppressions
**Problem:**
```
information unmatchedSuppression: Unmatched suppression: misra-c2012-2.5
```

**Root Cause:** MISRA addon not properly loaded or suppressions too broad.

**Solution:** 
- Fixed MISRA addon path in `misra.json`
- Restructured suppressions to be more specific
- Added `unmatchedSuppression:*` to handle flexible matching

### Issue 3: Critical Errors Preventing Analysis
**Problem:** Critical preprocessor errors stopped analysis completely.

**Solution:** Comprehensive configuration with all required defines and force-include directives.

## 📋 MISRA C:2012 Compliance

### Suppression Categories

#### External Libraries (DEVIATION D5)
```
*:lib/pico-sdk/*:*          # Pico SDK
*:lib/FreeRTOS-Kernel/*:*   # FreeRTOS
*:include/FreeRTOSConfig.h:* # FreeRTOS configuration
*:include/tusb_config.h:*    # TinyUSB configuration
```

#### Hardware Abstraction (DEVIATION D1)
```
misra-c2012-11.4:src/inputs.c:*   # GPIO register access
misra-c2012-11.4:src/outputs.c:*  # SPI register access
misra-c2012-11.5:src/error_management.c:* # Hardware register casts
```

#### FreeRTOS Integration (DEVIATION D5)
```
misra-c2012-11.1:src/app_tasks.c:*  # Function pointer casts
misra-c2012-8.13:src/app_tasks.c:*  # Task parameter constraints
```

### Adding New Suppressions

When adding suppressions, always include:
1. **Specific file/function** where possible
2. **MISRA deviation category** (D1-D5)
3. **Clear rationale** in comments

Example:
```
misra-c2012-11.4:src/new_driver.c:setup_registers  # Hardware register access requires cast - DEVIATION (D1)
```

## 🛠 Troubleshooting

### Common Issues

#### "cppcheck not found"
- **DevContainer:** Rebuild container or check dependencies.json
- **Host system:** Install with `apt install cppcheck` or `brew install cppcheck`

#### "MISRA addon not found"
- Check addon path: `/usr/share/cppcheck/addons/misra.py`
- Verify `misra.txt` exists in project root
- Some cppcheck installations don't include MISRA addon

#### "Include path errors"
- Verify git submodules are initialized: `git submodule update --init --recursive`
- Check that Pico SDK and FreeRTOS paths exist
- Use `--force-include` for critical headers

#### "Too many false positives"
- Update suppressions file with specific rules
- Use file-specific suppressions rather than global
- Document rationale for each suppression

### Advanced Configuration

#### Custom Defines for New Features
Add to `.cppcheck_config`:
```
-DNEW_FEATURE_ENABLED=1
-DFEATURE_CONFIG_VALUE=42
```

#### Additional Include Paths
Add to `.cppcheck_config`:
```
-I./new_lib/include
-I./additional/path
```

#### New Suppression Rules
Add to `.cppcheck_suppressions.txt`:
```
misra-c2012-X.Y:src/specific_file.c:function_name  # Specific rationale
```

## 📊 Integration with CI/CD

### GitHub Actions Example
```yaml
- name: Run Static Analysis
  run: |
    ./scripts/run_cppcheck.sh
  continue-on-error: true  # Allow warnings but capture output
```

### Pre-commit Hook
```bash
#!/bin/bash
# Check modified C files
git diff --cached --name-only | grep -E '\.(c|h)$' | while read file; do
    cppcheck --config-file=.cppcheck_config "$file"
done
```

## 🎯 Best Practices

1. **Run analysis regularly** - Integrate into development workflow
2. **Fix real issues first** - Address actual bugs before style issues  
3. **Document suppressions** - Always explain why something is suppressed
4. **Use specific suppressions** - Avoid broad wildcards when possible
5. **Review new warnings** - Don't automatically suppress new issues

## 📈 Expected Results

After proper configuration, you should see:
- ✅ No critical preprocessor errors
- ✅ MISRA rules properly loaded and applied
- ✅ Only relevant warnings for project code
- ✅ External libraries properly suppressed
- ✅ Clear, actionable output with file/line references

The analysis will focus on actual code quality issues in your source files while properly handling the embedded systems environment and external dependencies.